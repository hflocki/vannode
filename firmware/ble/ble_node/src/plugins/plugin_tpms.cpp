// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Plugin "tpms": SuperSense TireCheck tire pressure sensors, passive (advertisements only,
// no connection). One plugin instance covers up to TPMS_SLOTS wheels: each slot has the
// sensor MAC and a wheel position. Sensors that are seen but not assigned are listed in the
// web UI so their MAC can be copied. Warning limits per axle (the position decides the axle),
// a global limit as fallback. Decoding in tpms_decode.h (host test in firmware/ble/tests).
// Both sensor formats are understood (service data 0xFAF0 / 0xEEFD and the older manufacturer
// data format); the sensor id may be entered in either byte order.

#include "../core/plugin.h"

#if PLUGIN_TPMS

#include <NimBLEDevice.h>
#include "../core/devlog.h"
#include "../core/util.h"
#include "tpms_decode.h"

#define TPMS_SLOTS    8
#define TPMS_UNKNOWN  6   // Unassigned sensors remembered for the web UI

#define TPMS_POSITIONS "Vorne links|Vorne rechts|Mitte links|Mitte rechts|Hinten links|Hinten rechts|" \
                       "Hinten links innen|Hinten rechts innen|Anhänger vorne links|Anhänger vorne rechts|" \
                       "Anhänger hinten links|Anhänger hinten rechts|Reserverad"
static const char* const TPMS_POS[] = {
    "Vorne links", "Vorne rechts", "Mitte links", "Mitte rechts", "Hinten links", "Hinten rechts",
    "Hinten links innen", "Hinten rechts innen", "Anhänger vorne links", "Anhänger vorne rechts",
    "Anhänger hinten links", "Anhänger hinten rechts", "Reserverad"};
#define TPMS_NPOS (sizeof(TPMS_POS) / sizeof(TPMS_POS[0]))

// Axle of each position (index into TPMS_AXLE)
static const uint8_t TPMS_POS_AXLE[TPMS_NPOS] = {0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 4, 4, 5};
static const char* const TPMS_AXLE[] = {"Vorderachse", "Mittelachse", "Hinterachse",
                                        "Anhänger vorne", "Anhänger hinten", "Reserverad"};
#define TPMS_NAXLE 6

struct TpmsCfg {
    char mac[TPMS_SLOTS][18];
    uint8_t pos[TPMS_SLOTS];
    float p_min;    // bar, 0 = no warning (all axles without their own limit)
    float p_max;    // bar, 0 = no warning
    float ax_min[TPMS_NAXLE];   // bar per axle, 0 = use p_min
    float ax_max[TPMS_NAXLE];   // bar per axle, 0 = use p_max
};

#define TPMS_SLOT_FIELDS(i, n)                                                                       \
    FLD_MAC(TpmsCfg, mac[i], "mac" #n, "Rad " #n ": Sensor-MAC", i == 0 ? "Nicht zugeordnete Sensoren stehen in der Live-Ansicht, MAC von dort übernehmen. Leer = Rad nicht belegt" : nullptr), \
    FLD_SELECT(TpmsCfg, pos[i], "pos" #n, "Rad " #n ": Position", TPMS_POSITIONS, nullptr)

#define TPMS_AXLE_FIELDS(i, k, n)                                                                         \
    FLD_FLOAT(TpmsCfg, ax_min[i], k "_min", n ": Warnung unter", "bar", 0, 15, 0.05f, i == 0 ? "0 = Standard von oben" : nullptr), \
    FLD_FLOAT(TpmsCfg, ax_max[i], k "_max", n ": Warnung über", "bar", 0, 15, 0.05f, nullptr)

static const Field TPMS_FIELDS[] = {
    TPMS_SLOT_FIELDS(0, 1), TPMS_SLOT_FIELDS(1, 2), TPMS_SLOT_FIELDS(2, 3), TPMS_SLOT_FIELDS(3, 4),
    TPMS_SLOT_FIELDS(4, 5), TPMS_SLOT_FIELDS(5, 6), TPMS_SLOT_FIELDS(6, 7), TPMS_SLOT_FIELDS(7, 8),
    FLD_FLOAT(TpmsCfg, p_min, "p_min", "Standard: Warnung unter", "bar", 0, 15, 0.05f,
              "Gilt für alle Achsen ohne eigene Grenze, 0 = keine Warnung"),
    FLD_FLOAT(TpmsCfg, p_max, "p_max", "Standard: Warnung über", "bar", 0, 15, 0.05f, "0 = keine Warnung"),
    TPMS_AXLE_FIELDS(0, "va", "Vorderachse"), TPMS_AXLE_FIELDS(1, "ma", "Mittelachse"),
    TPMS_AXLE_FIELDS(2, "ha", "Hinterachse"), TPMS_AXLE_FIELDS(3, "av", "Anhänger vorne"),
    TPMS_AXLE_FIELDS(4, "ah", "Anhänger hinten"), TPMS_AXLE_FIELDS(5, "rr", "Reserverad"),
};

// Per wheel: pressure, temperature, battery, signal. Names come from entityName() (position).
#define TPMS_SLOT_ENTITIES(n)                                                                                     \
    {E_SENSOR, "r" #n "_pressure",    "Druck",      "bar", "pressure",        "measurement", "mdi:car-tire-alert", 0, nullptr}, \
    {E_SENSOR, "r" #n "_temperature", "Temperatur", "°C",  "temperature",     "measurement", nullptr, 0, nullptr},              \
    {E_SENSOR, "r" #n "_battery",     "Batterie",   "%",   "battery",         "measurement", nullptr, EF_DIAG, nullptr},        \
    {E_SENSOR, "r" #n "_rssi",        "Signal",     "dBm", "signal_strength", "measurement", "mdi:bluetooth", EF_DIAG | EF_HIDDEN, nullptr}, \
    {E_BINARY, "r" #n "_alarm",       "Druckwarnung", nullptr, "problem",     nullptr, "mdi:car-tire-alert", 0, nullptr}

static const Entity TPMS_ENTITIES[] = {
    TPMS_SLOT_ENTITIES(1), TPMS_SLOT_ENTITIES(2), TPMS_SLOT_ENTITIES(3), TPMS_SLOT_ENTITIES(4),
    TPMS_SLOT_ENTITIES(5), TPMS_SLOT_ENTITIES(6), TPMS_SLOT_ENTITIES(7), TPMS_SLOT_ENTITIES(8),
    {E_BINARY, "warning", "Reifendruck Warnung", nullptr, "problem", nullptr, "mdi:car-tire-alert", 0, nullptr},
    {E_SENSOR, "warning_text", "Reifendruck Hinweis", nullptr, nullptr, nullptr, "mdi:car-tire-alert", 0, nullptr},
};

struct TpmsWheel {
    bool has;
    float bar, temp;
    uint16_t mv;
    int8_t battery;
    int rssi;
    unsigned long last;
};

struct TpmsUnknown {
    char id[18];
    float bar;
    int rssi;
    unsigned long last;
};

class TpmsPlugin : public Plugin {
public:
    explicit TpmsPlugin(uint8_t index) : Plugin("tpms", "Reifendruck", "Reifen", index) {
        mux = portMUX_INITIALIZER_UNLOCKED;
    }

    size_t fields(const Field*& out) override {
        out = TPMS_FIELDS;
        return sizeof(TPMS_FIELDS) / sizeof(TPMS_FIELDS[0]);
    }
    void* cfg() override { return &c; }
    void setDefaults() override {
        memset(&c, 0, sizeof(c));
        for (uint8_t i = 0; i < TPMS_SLOTS; i++) c.pos[i] = i;
    }
    void onConfigChanged() override {
        for (uint8_t i = 0; i < TPMS_SLOTS; i++) {
            if (c.pos[i] >= TPMS_NPOS) c.pos[i] = 0;
            if (strcmp(c.mac[i], lastMac[i]) != 0) {
                strlcpy(lastMac[i], c.mac[i], sizeof(lastMac[i]));
                portENTER_CRITICAL(&mux);
                w[i] = TpmsWheel{};
                portEXIT_CRITICAL(&mux);
            }
        }
        if (c.p_min < 0) c.p_min = 0;
        if (c.p_max < 0) c.p_max = 0;
        for (uint8_t a = 0; a < TPMS_NAXLE; a++) {
            if (c.ax_min[a] < 0) c.ax_min[a] = 0;
            if (c.ax_max[a] < 0) c.ax_max[a] = 0;
        }

        updates++;
    }

    uint8_t bleUse() const override { return BLE_PASSIVE; }
    const char* bleAddress() const override { return c.mac[0]; }
    // Every assigned wheel is labelled in the "nearby" list
    const char* bleLabel(const char* addr, char* buf, size_t size) const override {
        for (uint8_t i = 0; i < TPMS_SLOTS; i++) {
            if (c.mac[i][0] && strcmp(c.mac[i], addr) == 0) {
                snprintf(buf, size, "%s: %s", name, TPMS_POS[c.pos[i] < TPMS_NPOS ? c.pos[i] : 0]);
                return buf;
            }
        }
        return nullptr;
    }
    bool bleCandidate(int32_t company, const char* uuid, const char* name) const override {
        return strstr(uuid, "faf0") || strstr(uuid, "eefd") || company == TPMS_MFG_ID;
    }
    const char* manufacturer() const override { return "SuperSense"; }
    const char* model() const override { return "TireCheck"; }

    // NimBLE host task: decode (cheap, no allocation) and store per wheel
    void onAdvertisement(const NimBLEAdvertisedDevice* dev, const char* addr) override {
        static const NimBLEUUID uuidA((uint16_t)TPMS_SVC_UUID16);
        static const NimBLEUUID uuidB((uint16_t)TPMS_SVC_UUID16_B);
        TpmsReading r;
        bool ok = false;
        if (dev->haveServiceData()) {
            std::string sd = dev->getServiceData(uuidA);
            if (sd.empty()) sd = dev->getServiceData(uuidB);
            ok = tpmsDecode((const uint8_t*)sd.data(), sd.size(), r);
        }
        if (!ok) {
            for (uint8_t k = 0; k < dev->getManufacturerDataCount() && !ok; k++) {
                std::string m = dev->getManufacturerData(k);   // includes the 2-byte company id
                if (m.size() == TPMS_MFG_LEN + 2 && (uint8_t)m[0] == (TPMS_MFG_ID & 0xFF) && (uint8_t)m[1] == (TPMS_MFG_ID >> 8))
                    ok = tpmsDecodeMfg((const uint8_t*)m.data() + 2, TPMS_MFG_LEN, r);
            }
        }
        if (!ok) return;
        char rev[18];   // the id in the other byte order (both are accepted in the settings)
        snprintf(rev, sizeof(rev), "%.2s:%.2s:%.2s:%.2s:%.2s:%.2s", r.id + 15, r.id + 12, r.id + 9, r.id + 6, r.id + 3, r.id);
        int rssi = dev->getRSSI();
        unsigned long now = millis();

        portENTER_CRITICAL(&mux);
        bool known = false;
        for (uint8_t i = 0; i < TPMS_SLOTS; i++) {
            const char* m = c.mac[i];
            if (!m[0] || (strcmp(m, r.id) != 0 && strcmp(m, rev) != 0 && strcmp(m, addr) != 0)) continue;
            w[i].has = true;
            w[i].bar = r.bar;
            w[i].temp = r.temperature;
            w[i].mv = r.mv;
            w[i].battery = r.battery;
            w[i].rssi = rssi;
            w[i].last = now;
            known = true;
            seq++;
            break;
        }
        if (!known) rememberUnknown(r, rssi, now);
        portEXIT_CRITICAL(&mux);
    }

    void loop() override {
        uint32_t s;
        portENTER_CRITICAL(&mux);
        s = seq;
        portEXIT_CRITICAL(&mux);
        // Publish soon after new readings, but not for every advertisement
        if (s != lastSeq && millis() - lastChange > 2000) {
            lastSeq = s;
            lastChange = millis();
            updates++;
        }
        bool warn = warning();
        if (warn != lastWarn) {
            lastWarn = warn;
            if (warn) devlogf(id, "Warning: %s", warnText);
            updates++;
        }
        bool avail = available();
        if (avail != lastAvail) {
            lastAvail = avail;
            updates++;
        }
    }

    bool available() override {
        for (uint8_t i = 0; i < TPMS_SLOTS; i++)
            if (fresh(i)) return true;
        return false;
    }

    bool entityActive(const Entity& e) override {
        int s = slotOf(e.key);
        return s < 0 || c.mac[s][0] != '\0';
    }
    const char* entityName(const Entity& e, char* buf, size_t size) override {
        int s = slotOf(e.key);
        if (s < 0) return e.name;
        snprintf(buf, size, "%s %s", TPMS_POS[c.pos[s]], e.name);
        return buf;
    }

    void live(JsonObject o) override {
        uint8_t used = 0, seen = 0;
        for (uint8_t i = 0; i < TPMS_SLOTS; i++) {
            if (!c.mac[i][0]) continue;
            used++;
            if (fresh(i)) seen++;
        }
        bool warn = warning();
        char st[48];
        if (!used) strlcpy(st, "Keine Räder zugeordnet", sizeof(st));
        else if (warn) strlcpy(st, warnText, sizeof(st));
        else snprintf(st, sizeof(st), "%u von %u Rädern aktuell", seen, used);
        o["state"] = st;
        o["ok"] = used && seen && !warn;

        TpmsWheel copy[TPMS_SLOTS];
        TpmsUnknown unk[TPMS_UNKNOWN];
        portENTER_CRITICAL(&mux);
        memcpy(copy, w, sizeof(copy));
        memcpy(unk, unknown, sizeof(unk));
        portEXIT_CRITICAL(&mux);

        char v[64];
        for (uint8_t i = 0; i < TPMS_SLOTS; i++) {
            if (!c.mac[i][0]) continue;
            const TpmsWheel& x = copy[i];
            if (!x.has) {
                liveRowText(o, TPMS_POS[c.pos[i]], "noch kein Signal");
                continue;
            }
            const char* alarm = wheelAlarm(i, x) ? " · ⚠" : "";
            char tt[12];
            if (isnan(x.temp)) strlcpy(tt, "– °C", sizeof(tt));
            else snprintf(tt, sizeof(tt), "%.0f °C", x.temp);
            snprintf(v, sizeof(v), "%.2f bar · %s · %d %% · %lu s%s", x.bar, tt, x.battery,
                     (millis() - x.last) / 1000, alarm);
            liveRowText(o, TPMS_POS[c.pos[i]], v);
        }
        // Unassigned sensors: MAC for the settings
        for (uint8_t k = 0; k < TPMS_UNKNOWN; k++) {
            if (!unk[k].id[0] || millis() - unk[k].last > TPMS_TIMEOUT_MS) continue;
            snprintf(v, sizeof(v), "%.2f bar · %d dBm · nicht zugeordnet", unk[k].bar, unk[k].rssi);
            liveRowText(o, unk[k].id, v);
        }
    }

    size_t entities(const Entity*& out) override {
        out = TPMS_ENTITIES;
        return sizeof(TPMS_ENTITIES) / sizeof(TPMS_ENTITIES[0]);
    }
    uint32_t publishIntervalMs() const override { return MQTT_TPMS_MS; }

    void publish(MqttOut& out) override {
        TpmsWheel copy[TPMS_SLOTS];
        portENTER_CRITICAL(&mux);
        memcpy(copy, w, sizeof(copy));
        portEXIT_CRITICAL(&mux);
        char k[24];
        for (uint8_t i = 0; i < TPMS_SLOTS; i++) {
            if (!c.mac[i][0] || !copy[i].has) continue;
            snprintf(k, sizeof(k), "r%u_pressure", i + 1);
            out.num(k, copy[i].bar, 2);
            snprintf(k, sizeof(k), "r%u_temperature", i + 1);
            out.num(k, copy[i].temp, 0);
            snprintf(k, sizeof(k), "r%u_battery", i + 1);
            out.integer(k, copy[i].battery);
            snprintf(k, sizeof(k), "r%u_rssi", i + 1);
            out.integer(k, copy[i].rssi);
            snprintf(k, sizeof(k), "r%u_alarm", i + 1);
            out.onoff(k, fresh(i) && wheelAlarm(i, copy[i]));
        }
        bool warn = warning();
        out.onoff("warning", warn);
        out.text("warning_text", warn ? warnText : "OK");
    }

private:
    TpmsCfg c;
    char lastMac[TPMS_SLOTS][18] = {};
    portMUX_TYPE mux;
    TpmsWheel w[TPMS_SLOTS] = {};
    TpmsUnknown unknown[TPMS_UNKNOWN] = {};
    uint32_t seq = 0, lastSeq = 0;
    unsigned long lastChange = 0;
    bool lastWarn = false, lastAvail = false;
    char warnText[48] = "";

    static int slotOf(const char* key) {
        if (key[0] != 'r' || key[1] < '1' || key[1] > '0' + TPMS_SLOTS || key[2] != '_') return -1;
        return key[1] - '1';
    }

    bool fresh(uint8_t i) {
        return c.mac[i][0] && w[i].has && millis() - w[i].last <= TPMS_TIMEOUT_MS;
    }

    // Limits of the wheel's axle; 0 on the axle = the standard limit
    float limitMin(uint8_t i) const {
        float v = c.ax_min[TPMS_POS_AXLE[c.pos[i]]];
        return v > 0 ? v : c.p_min;
    }
    float limitMax(uint8_t i) const {
        float v = c.ax_max[TPMS_POS_AXLE[c.pos[i]]];
        return v > 0 ? v : c.p_max;
    }
    // -1 too low, 1 too high, 0 ok
    int wheelState(uint8_t i, const TpmsWheel& x) const {
        float lo = limitMin(i), hi = limitMax(i);
        if (lo > 0 && x.bar < lo) return -1;
        if (hi > 0 && x.bar > hi) return 1;
        return 0;
    }
    bool wheelAlarm(uint8_t i, const TpmsWheel& x) const { return wheelState(i, x) != 0; }

    // Any assigned wheel outside the limits of its axle (only current readings count)
    bool warning() {
        for (uint8_t i = 0; i < TPMS_SLOTS; i++) {
            if (!fresh(i)) continue;
            int st = wheelState(i, w[i]);
            if (st == 0) continue;
            snprintf(warnText, sizeof(warnText), "%s: %.2f bar zu %s", TPMS_POS[c.pos[i]], w[i].bar,
                     st < 0 ? "niedrig" : "hoch");
            return true;
        }
        warnText[0] = '\0';
        return false;
    }

    // Called with mux held
    void rememberUnknown(const TpmsReading& r, int rssi, unsigned long now) {
        int slot = -1, oldest = 0;
        for (int k = 0; k < TPMS_UNKNOWN; k++) {
            if (strcmp(unknown[k].id, r.id) == 0) { slot = k; break; }
            if (unknown[k].last < unknown[oldest].last) oldest = k;
        }
        if (slot < 0) slot = oldest;
        strlcpy(unknown[slot].id, r.id, sizeof(unknown[slot].id));
        unknown[slot].bar = r.bar;
        unknown[slot].rssi = rssi;
        unknown[slot].last = now;
    }
};

Plugin* createTpms(uint8_t index) {
    return new TpmsPlugin(index);
}

#endif // PLUGIN_TPMS
