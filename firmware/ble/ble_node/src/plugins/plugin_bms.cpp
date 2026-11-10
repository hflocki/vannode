// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Plugins "daly", "ective", "bm2": batteries (BMS / battery monitors) over Bluetooth.
// Formerly the separate battery-node firmware (firmware/battery); protocol decoding in
// bms_proto.cpp (ported from aiobmsble, Apache-2.0).
//
//   daly    Daly smart BMS: MODBUS read over FFF0/FFF1/FFF2, protocol D2 or 0x81 (detected)
//   ective  Ective BM X / LiPower: MODBUS read over FFE0/FFE1 (or AF30/FFE1), device id detected
//   bm2     BM2 battery monitor: AES encrypted broadcast; a connection (notify FFF4) only
//           while no broadcast arrives (older firmware)
//
// Values: <prefix>/<id>/soc, voltage, current ... and, for all batteries of the node, the
// JSON list <prefix>/batteries for the displays (bmsBatteryList(), published by mqtt_ha).

#include "../core/plugin.h"
#include "../core/devlog.h"

#if CT_BMS

#include "../core/ble_link.h"
#include "../core/registry.h"
#include "../core/mqtt_ha.h"
#include "bms_plugin.h"
#include "bms_proto.h"
#include "mbedtls/aes.h"

volatile uint32_t bmsListChanges = 0;

struct BmsCfg {
    char mac[18];
    int32_t interval;   // poll interval in s
};

static const Field BMS_FIELDS[] = {
    FLD_MAC(BmsCfg, mac, "mac", "Batterie-MAC", "Über \"Suchen\" aus der Liste wählen"),
    FLD_INT(BmsCfg, interval, "interval", "Abfrageintervall", "s", 5, 300, "Wie oft die Werte gelesen werden"),
};
static const Field BM2_FIELDS[] = {
    FLD_MAC(BmsCfg, mac, "mac", "BM2-MAC", "Über \"Suchen\" aus der Liste wählen (Name \"Battery Monitor\" / \"BM2\")"),
};

// One table for all three types; entityActive() switches off what a type does not deliver
static const char* const CELL_KEYS[BMS_MAX_CELLS] = {"cell_1", "cell_2", "cell_3", "cell_4", "cell_5", "cell_6",
                                                     "cell_7", "cell_8", "cell_9", "cell_10", "cell_11", "cell_12",
                                                     "cell_13", "cell_14", "cell_15", "cell_16"};
static const char* const CELL_NAMES[BMS_MAX_CELLS] = {"Zelle 1", "Zelle 2", "Zelle 3", "Zelle 4", "Zelle 5", "Zelle 6",
                                                      "Zelle 7", "Zelle 8", "Zelle 9", "Zelle 10", "Zelle 11", "Zelle 12",
                                                      "Zelle 13", "Zelle 14", "Zelle 15", "Zelle 16"};
#define BMS_FIXED_ENTITIES 17
static Entity BMS_ENTITIES[BMS_FIXED_ENTITIES + BMS_MAX_CELLS] = {
    {E_SENSOR, "soc",          "Ladestand",      "%",   "battery",        "measurement", "mdi:battery", 0, nullptr},
    {E_SENSOR, "voltage",      "Spannung",       "V",   "voltage",        "measurement", "mdi:sine-wave", 0, nullptr},
    {E_SENSOR, "current",      "Strom",          "A",   "current",        "measurement", "mdi:current-dc", 0, nullptr},
    {E_SENSOR, "power",        "Leistung",       "W",   "power",          "measurement", "mdi:flash", 0, nullptr},
    {E_SENSOR, "charge",       "Restkapazität",  "Ah",  nullptr,          "measurement", "mdi:battery-plus", 0, nullptr},
    {E_SENSOR, "energy",       "Energie",        "Wh",  "energy_storage", "measurement", "mdi:battery-charging", 0, nullptr},
    {E_SENSOR, "runtime",      "Restlaufzeit",   "min", "duration",       "measurement", "mdi:timer-sand", 0, nullptr},
    {E_SENSOR, "temperature",  "Temperatur",     "°C",  "temperature",    "measurement", "mdi:thermometer", 0, nullptr},
    {E_SENSOR, "cycles",       "Zyklen",         nullptr, nullptr,        "total_increasing", "mdi:battery-sync", EF_DIAG, nullptr},
    {E_SENSOR, "delta",        "Zelldifferenz",  "V",   "voltage",        "measurement", "mdi:battery-heart-variant", 0, nullptr},
    {E_SENSOR, "state",        "Batteriezustand", nullptr, nullptr,       nullptr,       "mdi:car-battery", 0, nullptr},
    {E_BINARY, "charging",     "Lädt",           nullptr, "battery_charging", nullptr,  nullptr, 0, nullptr},
    {E_BINARY, "problem",      "Problem",        nullptr, "problem",      nullptr,       nullptr, 0, nullptr},
    {E_SENSOR, "problem_text", "Problem (Text)", nullptr, nullptr,        nullptr,       "mdi:alert-circle-outline", EF_DIAG, nullptr},
    {E_SENSOR, "ble_rssi",     "Bluetooth Signal", "dBm", "signal_strength", "measurement", "mdi:bluetooth", EF_DIAG | EF_HIDDEN, nullptr},
    {E_BINARY, "connected",    "Verbindung",     nullptr, "connectivity", nullptr,       "mdi:bluetooth-connect", EF_DIAG | EF_ALWAYS, nullptr},
    {E_SWITCH, "release",      "Bluetooth für App freigeben", nullptr, nullptr, nullptr, "mdi:bluetooth-off", EF_CONFIG | EF_ALWAYS, nullptr},
};
static bool cellEntitiesBuilt = false;
static void buildCellEntities() {
    if (cellEntitiesBuilt) return;
    for (uint8_t i = 0; i < BMS_MAX_CELLS; i++)
        BMS_ENTITIES[BMS_FIXED_ENTITIES + i] = {E_SENSOR, CELL_KEYS[i], CELL_NAMES[i], "V", "voltage", "measurement",
                                                "mdi:battery-outline", EF_DIAG, nullptr};
    cellEntitiesBuilt = true;
}

static bool bm2AesDecrypt(const uint8_t in[16], uint8_t out[16]) {
    uint8_t iv[16] = {0};
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, BM2_AES_KEY, 128);
    int rc = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, 16, iv, in, out);
    mbedtls_aes_free(&aes);
    return rc == 0;
}

// GATT layouts per type (tried in order when the service is missing)
struct BmsUuids {
    const char* service;
    const char* notify;
    const char* write;
};
static const BmsUuids DALY_LAYOUTS[] = {
    {"0000fff0-0000-1000-8000-00805f9b34fb", "0000fff1-0000-1000-8000-00805f9b34fb", "0000fff2-0000-1000-8000-00805f9b34fb"},
};
static const BmsUuids ECTIVE_LAYOUTS[] = {
    {"0000ffe0-0000-1000-8000-00805f9b34fb", "0000ffe1-0000-1000-8000-00805f9b34fb", "0000ffe1-0000-1000-8000-00805f9b34fb"},
    {"0000af30-0000-1000-8000-00805f9b34fb", "0000ffe1-0000-1000-8000-00805f9b34fb", "0000ffe1-0000-1000-8000-00805f9b34fb"},
};
static const BmsUuids BM2_LAYOUTS[] = {
    {"0000fff0-0000-1000-8000-00805f9b34fb", "0000fff4-0000-1000-8000-00805f9b34fb", ""},
};

class BmsPlugin : public Plugin {
public:
    BmsPlugin(BmsType kind, const char* type, const char* typeName, const char* title, uint8_t index)
        : Plugin(type, typeName, title, index), kind(kind) {
        portMUX_INITIALIZE(&mux);
        buildCellEntities();
        bmsClear(sample);
        setDefaults();
    }

    BmsType bmsKind() const { return kind; }

    size_t fields(const Field*& out) override {
        if (kind == BMS_BM2) {
            out = BM2_FIELDS;
            return sizeof(BM2_FIELDS) / sizeof(BM2_FIELDS[0]);
        }
        out = BMS_FIELDS;
        return sizeof(BMS_FIELDS) / sizeof(BMS_FIELDS[0]);
    }
    void* cfg() override { return &c; }
    void setDefaults() override {
        memset(&c, 0, sizeof(c));
        c.interval = BMS_DEFAULT_INTERVAL_S;
    }
    void onConfigChanged() override {
        if (strcmp(c.mac, lastMac) != 0) {
            strlcpy(lastMac, c.mac, sizeof(lastMac));
            hasData = false;
            dalyProto = 0;
            ectiveId = -1;
            advAt = 0;
            bmsClear(sample);
            bmsListChanges = bmsListChanges + 1;
            updates++;
        }
        lastPoll = 0;
        if (begun) applyTarget(true);
    }

    uint8_t bleUse() const override { return BLE_LINK; }
    const char* bleAddress() const override { return c.mac; }
    bool bleCandidate(int32_t company, const char* uuid, const char* name) const override {
        if (kind == BMS_DALY)
            return (name && !strncmp(name, "DL-", 3)) ||
                   (strstr(uuid, "fff0") && (company == 0x102 || company == 0x104 || company == 0x302 ||
                                             company == 0x303 || company == 0x402));
        if (kind == BMS_ECTIVE) return strstr(uuid, "af30") != nullptr;
        return name && (!strcmp(name, "Battery Monitor") || !strncmp(name, "BM2", 3));
    }
    const char* manufacturer() const override {
        return kind == BMS_DALY ? "Daly" : kind == BMS_ECTIVE ? "Ective" : "BM2";
    }
    const char* model() const override { return typeName; }

    void begin() override {
        layout = 0;
        const BmsUuids& u = layouts()[0];
        link.begin(id, u.service, u.notify, u.write);
        begun = true;
        startedAt = millis();
        applyTarget(true);
    }

    // BM2 broadcast (NimBLE host task): decrypt and hand the reading to loop()
    void onAdvertisement(const NimBLEAdvertisedDevice* dev, const char* addr) override {
        if (kind != BMS_BM2 || c.mac[0] == '\0' || strcmp(addr, c.mac) != 0) return;
        for (uint8_t k = 0; k < dev->getManufacturerDataCount(); k++) {
            std::string m = dev->getManufacturerData(k);   // includes the 2-byte company id
            if (m.size() != 16) continue;                   // 23-byte iBeacon frame = old format, no voltage
            uint8_t plain[16];
            BmsSample s;
            if (!bm2AesDecrypt((const uint8_t*)m.data(), plain) || !bm2DecodeAdv(plain, s)) continue;
            portENTER_CRITICAL(&mux);
            advSample = s;
            advRssi = dev->getRSSI();
            advPending = true;
            portEXIT_CRITICAL(&mux);
            return;
        }
    }

    void loop() override {
        link.loop();
        unsigned long now = millis();

        if (kind == BMS_BM2) {
            loopBm2(now);
        } else {
            BlePacket pkt;
            while (link.receive(pkt)) collect(pkt.data, pkt.len);
            if (link.connected()) poll(now);
            else step = S_IDLE;
        }

        // Service missing: try the next GATT layout of the type
        if (link.changes != lastLinkChanges) {
            lastLinkChanges = link.changes;
            if (!strcmp(link.state(), "Falsches Gerät?") && layoutCount() > 1) {
                layout = (layout + 1) % layoutCount();
                const BmsUuids& u = layouts()[layout];
                devlogf(id, "Trying GATT layout %s", u.service);
                link.setUuids(u.service, u.notify, u.write);
            }
            updates++;
        }

        bool avail = available();
        if (avail != lastAvail) {
            lastAvail = avail;
            bmsListChanges = bmsListChanges + 1;
            updates++;
        }
    }

    bool available() override {
        return hasData && millis() - lastOk <= BMS_STALE_MS;
    }

    void live(JsonObject o) override {
        o["state"] = stateText();
        o["ok"] = available();
        const BmsSample& s = sample;
        if (hasData) {
            if (!isnan(s.soc)) {
                JsonObject g = o["gauge"].to<JsonObject>();
                g["v"] = roundf(s.soc);
                g["unit"] = "%";
                g["label"] = "Ladestand";
                g["max"] = 100;
            }
            liveRowNum(o, "Spannung", s.voltage, 2, "V");
            if (!isnan(s.current)) {
                liveRowNum(o, "Strom", s.current, 2, "A");
                liveRowNum(o, "Leistung", s.power, 0, "W");
            }
            if (!isnan(s.charge_ah)) liveRowNum(o, "Restkapazität", s.charge_ah, 1, "Ah");
            if (!isnan(s.energy_wh)) liveRowNum(o, "Energie", s.energy_wh, 0, "Wh");
            if (!isnan(s.runtime_s)) liveRowNum(o, "Restlaufzeit", s.runtime_s / 3600.0f, 1, "h");
            if (!isnan(s.temperature)) liveRowNum(o, "Temperatur", s.temperature, 1, "°C");
            if (s.cell_count) {
                float lo = s.cell_v[0], hi = s.cell_v[0];
                for (uint8_t i = 1; i < s.cell_count; i++) {
                    if (s.cell_v[i] < lo) lo = s.cell_v[i];
                    if (s.cell_v[i] > hi) hi = s.cell_v[i];
                }
                char buf[40];
                snprintf(buf, sizeof(buf), "%u × %.3f … %.3f V", (unsigned)s.cell_count, lo, hi);
                liveRowText(o, "Zellen", buf);
                liveRowNum(o, "Zelldifferenz", s.delta_v * 1000.0f, 0, "mV");
            }
            if (s.cycles >= 0) liveRowNum(o, "Zyklen", (float)s.cycles, 0, "");
            const char* st = batStateName(s.bat_state);
            if (st) liveRowText(o, "Zustand", st);
            if (s.problem) {
                char pt[160];
                bmsProblemText(s, pt, sizeof(pt));
                liveRowText(o, "Problem", pt);
            }
            liveRowNum(o, "Alter", (millis() - lastOk) / 1000.0f, 0, "s");
        }
        liveRowNum(o, "Bluetooth", rssi ? (float)rssi : NAN, 0, "dBm");
        if (link.lastError()) liveRowText(o, "Letzter BT-Fehler", link.lastErrorText());
        liveCtrl(o, "switch", "release", "Bluetooth für App freigeben")["v"] = link.released();
    }

    size_t entities(const Entity*& out) override {
        out = BMS_ENTITIES;
        return BMS_FIXED_ENTITIES + BMS_MAX_CELLS;
    }
    bool entityActive(const Entity& e) override {
        const char* k = e.key;
        bool bm2 = kind == BMS_BM2;
        if (!strncmp(k, "cell_", 5)) return atoi(k + 5) <= announcedCells;
        if (!strcmp(k, "state")) return bm2;
        if (!strcmp(k, "current") || !strcmp(k, "power") || !strcmp(k, "charge") || !strcmp(k, "energy") ||
            !strcmp(k, "runtime"))
            return !bm2;
        if (!strcmp(k, "temperature") || !strcmp(k, "cycles") || !strcmp(k, "delta")) return kind == BMS_DALY;
        if (!strcmp(k, "problem") || !strcmp(k, "problem_text")) return kind == BMS_DALY;
        return true;
    }
    uint32_t publishIntervalMs() const override { return 60000; }

    void publish(MqttOut& out) override {
        out.onoff("connected", link.connected());
        out.onoff("release", link.released());
        if (rssi) out.integer("ble_rssi", rssi);
        if (!available()) return;   // HA keeps the last values and shows them as unavailable
        const BmsSample& s = sample;
        out.num("voltage", s.voltage, 2);
        out.num("current", s.current, 2);
        out.num("power", s.power, 0);
        out.num("soc", s.soc, 0);
        out.num("charge", s.charge_ah, 1);
        out.num("energy", s.energy_wh, 0);
        // "None" = unknown in HA (not discharging); an empty payload would keep the old value
        if (kind != BMS_BM2) {
            if (isnan(s.runtime_s)) out.text("runtime", "None");
            else out.num("runtime", s.runtime_s / 60.0f, 0);
        }
        const char* st = batStateName(s.bat_state);
        if (st) out.text("state", st);
        out.num("temperature", s.temperature, 1);
        if (s.cycles >= 0) out.integer("cycles", s.cycles);
        out.num("delta", s.delta_v, 3);
        for (uint8_t i = 0; i < s.cell_count && i < BMS_MAX_CELLS; i++) out.num(CELL_KEYS[i], s.cell_v[i], 3);
        out.onoff("charging", charging());
        if (kind == BMS_DALY) {
            out.onoff("problem", s.problem);
            char pt[160];
            bmsProblemText(s, pt, sizeof(pt));
            out.text("problem_text", pt);   // "" when there is no problem
        }
    }

    void command(const char* key, const char* value) override {
        if (!strcmp(key, "release")) {
            link.setReleased(payloadOn(value));
            updates++;
        }
    }

    // Entry of the list <prefix>/batteries (same fields as battery-node 1.x)
    void listEntry(JsonObject o) {
        o["id"] = (const char*)id;
        o["name"] = (const char*)name;
        bool ok = available();
        o["ok"] = ok;
        o["t"] = (int)kind;
        if (!ok) return;
        const BmsSample& s = sample;
        if (!isnan(s.soc)) o["soc"] = roundf(s.soc);
        if (!isnan(s.voltage)) o["v"] = roundf(s.voltage * 100) / 100;
        if (!isnan(s.current)) o["a"] = roundf(s.current * 100) / 100;
        if (!isnan(s.power)) o["w"] = roundf(s.power);
        const char* st = batStateName(s.bat_state);
        if (st) o["st"] = st;
        if (!isnan(s.runtime_s)) o["rt"] = (long)lroundf(s.runtime_s / 60.0f);
        if (s.problem) {
            o["p"] = true;
            char pt[100];   // short: the whole list has to fit into one MQTT message
            bmsProblemText(s, pt, sizeof(pt));
            o["pt"] = (const char*)pt;
        }
    }

private:
    enum Step : uint8_t { S_IDLE, S_WAIT };

    BmsType kind;
    BmsCfg c;
    BleLink link;
    bool begun = false;
    uint8_t layout = 0;
    uint32_t lastLinkChanges = 0;
    char lastMac[18] = "";
    unsigned long startedAt = 0;

    // Reading
    BmsSample sample;
    bool hasData = false;
    bool lastAvail = false;
    unsigned long lastOk = 0;
    int rssi = 0;
    uint8_t announcedCells = 0;

    // MODBUS polling (Daly, Ective)
    Step step = S_IDLE;
    unsigned long lastPoll = 0;
    unsigned long reqAt = 0;
    uint8_t head = 0;          // expected first byte of the answer
    uint8_t buf[200];
    size_t len = 0;
    bool frameDone = false;
    uint8_t dalyProto = 0;     // 0 = not detected yet
    uint8_t tryProto = 0;      // protocol / device id of the running request
    uint8_t reqIdx = 0;        // Daly 0x81: request 0 or 1
    uint8_t f1[200];
    size_t n1 = 0;
    int16_t ectiveId = -1;     // -1 = not detected yet
    uint8_t tryIdx = 0;        // index into the protocol / id list being tried

    // BM2 broadcast
    portMUX_TYPE mux;
    BmsSample advSample;
    int advRssi = 0;
    bool advPending = false;
    unsigned long advAt = 0;
    bool bm2Linked = false;

    const BmsUuids* layouts() const {
        return kind == BMS_DALY ? DALY_LAYOUTS : kind == BMS_ECTIVE ? ECTIVE_LAYOUTS : BM2_LAYOUTS;
    }
    uint8_t layoutCount() const {
        return kind == BMS_DALY ? sizeof(DALY_LAYOUTS) / sizeof(DALY_LAYOUTS[0])
             : kind == BMS_ECTIVE ? sizeof(ECTIVE_LAYOUTS) / sizeof(ECTIVE_LAYOUTS[0])
                                  : sizeof(BM2_LAYOUTS) / sizeof(BM2_LAYOUTS[0]);
    }

    bool charging() const {
        const BmsSample& s = sample;
        return (!isnan(s.current) && s.current > 0.05f) || s.bat_state == BAT_STATE_CHARGING ||
               s.bat_state == BAT_STATE_FLOATING;
    }

    const char* stateText() {
        if (c.mac[0] == '\0') return "Keine Batterie";
        if (kind == BMS_BM2 && !bm2Linked) return available() ? "OK (Funk)" : "Warte auf Funk";
        if (link.connected() && hasData && !available()) return "Keine Antwort";
        if (link.connected() && available()) return "OK";
        return link.state();
    }

    // BM2: broadcast while it works, otherwise a connection
    bool bm2Passive(unsigned long now) const {
        if (advAt && now - advAt < BM2_ADV_FRESH_MS) return true;
        return !advAt && now - startedAt < BM2_ADV_GRACE_MS;   // give the first broadcasts a chance
    }

    void applyTarget(bool force) {
        bool want = c.mac[0] && (kind != BMS_BM2 || !bm2Passive(millis()));
        if (!force && want == bm2Linked) return;
        bm2Linked = want;
        link.setTarget(want ? c.mac : "");
    }

    void accept(const BmsSample& s, int r) {
        sample = s;
        rssi = r;
        lastOk = millis();
        if (!hasData) updates++;
        hasData = true;
        if (s.cell_count > announcedCells) {   // cell count known only after the first reading
            announcedCells = s.cell_count;
            mqttRequestDiscovery();
        }
        bmsListChanges = bmsListChanges + 1;
        updates++;
    }

    void loopBm2(unsigned long now) {
        bool have = false;
        BmsSample s;
        int r = 0;
        portENTER_CRITICAL(&mux);
        if (advPending) {
            s = advSample;
            r = advRssi;
            advPending = false;
            have = true;
        }
        portEXIT_CRITICAL(&mux);
        if (have) {
            advAt = now ? now : 1;
            accept(s, r);
        }
        if (kind == BMS_BM2) applyTarget(false);

        // Connected (older firmware): every notification is one AES block
        BlePacket pkt;
        while (link.receive(pkt)) {
            if (pkt.len < 16) continue;
            uint8_t plain[16];
            BmsSample n;
            if (bm2AesDecrypt(pkt.data, plain) && bm2Decode(plain, n)) accept(n, link.rssi());
        }
    }

    // ---- MODBUS polling (non-blocking) ------------------------------------------

    void sendRead(uint8_t dev, uint8_t expectHead, uint16_t addr, uint16_t count) {
        uint8_t cmd[8];
        bmsModbusRead(dev, addr, count, cmd);
        head = expectHead;
        len = 0;
        frameDone = false;
        reqAt = millis();
        step = S_WAIT;
        if (!link.writeAuto(cmd, sizeof(cmd))) reqAt = 0;   // fails at once: counts as timeout
    }

    // Collects notification pieces into one frame starting with the expected head byte
    void collect(const uint8_t* data, size_t n) {
        if (step != S_WAIT || frameDone || n == 0) return;
        if (len == 0 && data[0] != head) return;
        if (len + n > sizeof(buf)) len = 0;
        memcpy(buf + len, data, n);
        len += n;
        int need = bmsModbusExpectedLen(buf, len, head);
        if (need < 0) {
            // Garbage in the buffer: start over if this piece begins a new frame
            if (data[0] != head) { len = 0; return; }
            memmove(buf, data, n);
            len = n;
            need = bmsModbusExpectedLen(buf, len, head);
            if (need < 0) { len = 0; return; }
        }
        if (need > 0 && len >= (size_t)need) {
            len = need;
            frameDone = true;
        }
    }

    void startPoll() {
        tryIdx = 0;
        reqIdx = 0;
        if (kind == BMS_DALY) startDaly();
        else startEctive();
    }

    void startDaly() {
        static const DalyProto PROTOS[2] = {DALY_D2, DALY_X81};
        while (tryIdx < 2 && dalyProto && dalyProto != PROTOS[tryIdx]) tryIdx++;
        if (tryIdx >= 2) { pollFailed(); return; }
        DalyProto p = PROTOS[tryIdx];
        tryProto = p;
        uint16_t addr, count;
        dalyRequest(p, reqIdx, addr, count);
        sendRead(p, dalyResponseHead(p), addr, count);
    }

    void startEctive() {
        while (tryIdx < 4 && ectiveId >= 0 && ectiveId != ECTIVE_DEV_IDS[tryIdx]) tryIdx++;
        if (tryIdx >= 4) { pollFailed(); return; }
        uint8_t dev = ECTIVE_DEV_IDS[tryIdx];
        tryProto = dev;
        sendRead(dev, dev, ECTIVE_REG_ADDR, ECTIVE_REG_COUNT);
    }

    void pollFailed() {
        step = S_IDLE;
        devlogf(id, "No valid answer");
        updates++;
    }

    void poll(unsigned long now) {
        if (step == S_IDLE) {
            if (lastPoll == 0 || now - lastPoll >= (unsigned long)c.interval * 1000UL) {
                lastPoll = now ? now : 1;
                startPoll();
            }
            return;
        }
        // S_WAIT: answer complete, or timeout (reqAt 0 = the write failed)
        bool timeout = reqAt == 0 || now - reqAt >= BMS_RESPONSE_TIMEOUT_MS;
        if (!frameDone && !timeout) return;
        bool crcOk = frameDone && bmsModbusCrcOk(buf, len);
        BmsSample s;
        if (kind == BMS_DALY) {
            DalyProto p = (DalyProto)tryProto;
            if (crcOk && dalyRequestCount(p) > 1 && reqIdx == 0) {
                memcpy(f1, buf, len);   // first half of protocol 0x81, ask for the second
                n1 = len;
                reqIdx = 1;
                startDaly();
                return;
            }
            bool ok = false;
            if (crcOk) {
                if (dalyRequestCount(p) > 1) ok = dalyDecode(p, f1, n1, buf, len, s);
                else ok = dalyDecode(p, buf, len, nullptr, 0, s);
            }
            if (ok) {
                if (!dalyProto) devlogf(id, "Daly protocol 0x%02X", p);
                dalyProto = p;
                step = S_IDLE;
                accept(s, link.rssi());
                return;
            }
            reqIdx = 0;
            tryIdx++;
            startDaly();
        } else {
            if (crcOk && ectiveDecode(buf, len, s)) {
                if (ectiveId < 0) devlogf(id, "Ective device id 0x%02X", tryProto);
                ectiveId = tryProto;
                step = S_IDLE;
                accept(s, link.rssi());
                return;
            }
            tryIdx++;
            startEctive();
        }
    }
};

static Plugin* bmsAt(uint8_t i, BmsPlugin** out) {
    Plugin* p = pluginAt(i);
    if (!p || !p->enabled || p->removed) return nullptr;
    if (strcmp(p->type, "daly") && strcmp(p->type, "ective") && strcmp(p->type, "bm2")) return nullptr;
    *out = static_cast<BmsPlugin*>(p);
    return p;
}

bool bmsAny() {
    BmsPlugin* b;
    for (uint8_t i = 0; i < pluginCount(); i++)
        if (bmsAt(i, &b)) return true;
    return false;
}

size_t bmsBatteryList(char* out, size_t n) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    BmsPlugin* b;
    for (uint8_t i = 0; i < pluginCount(); i++)
        if (bmsAt(i, &b)) b->listEntry(arr.add<JsonObject>());
    size_t len = serializeJson(doc, out, n);
    return (len == 0 || len >= n) ? 0 : len;
}

#if PLUGIN_DALY
Plugin* createDaly(uint8_t index) { return new BmsPlugin(BMS_DALY, "daly", "Daly BMS", "Batterie", index); }
#endif
#if PLUGIN_ECTIVE
Plugin* createEctive(uint8_t index) { return new BmsPlugin(BMS_ECTIVE, "ective", "Ective BM X", "Batterie", index); }
#endif
#if PLUGIN_BM2
Plugin* createBm2(uint8_t index) { return new BmsPlugin(BMS_BM2, "bm2", "BM2 Batteriewächter", "Starterbatterie", index); }
#endif

#endif // CT_BMS
