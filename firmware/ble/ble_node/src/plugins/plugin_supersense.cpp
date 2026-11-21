// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Plugin "supersense": SuperSense receiver box ("SSBT…") with up to 4 tank sensors, over a BLE
// connection. Read-only. About 1 s after connecting the init packet is written, then the box
// notifies the levels and temperatures of all 4 channels. Each channel is set up as a tank
// (kind, size, taught raw values for empty / full). Decoding in supersense_decode.h (shared
// with the tanklevel node, host test in firmware/ble/tests).

#include "../core/plugin.h"

#if PLUGIN_SUPERSENSE

#include "../core/ble_link.h"
#include "../core/devlog.h"
#include "../core/util.h"
#include "supersense_decode.h"

#define SS_KINDS "nicht benutzt|Frischwasser|Grauwasser|Schwarzwasser|Gas|Sonstiges"
static const char* const SS_KIND[] = {"Sensor", "Frischwasser", "Grauwasser", "Schwarzwasser", "Gas", "Tank"};
#define SS_NKIND (sizeof(SS_KIND) / sizeof(SS_KIND[0]))

struct SuperSenseCfg {
    char mac[18];
    uint8_t kind[SS_CHANNELS];      // 0 = not used
    float size[SS_CHANNELS];        // litres
    int32_t empty[SS_CHANNELS];     // taught raw value at empty (0 = default scale)
    int32_t full[SS_CHANNELS];      // taught raw value at full  (0 = default scale)
};

// Channel 1 keeps the keys of 2.2 ("medium", "tank_size") so existing settings carry over
#define SS_CH_FIELDS(i, kk, ks, ke, kf, n)                                                                  \
    FLD_SELECT(SuperSenseCfg, kind[i], kk, "Sensor " n ": Tank", SS_KINDS, nullptr),                       \
    FLD_FLOAT(SuperSenseCfg, size[i], ks, "Sensor " n ": Tankgröße", "L", 0, 5000, 0.1f, nullptr),         \
    FLD_INT(SuperSenseCfg, empty[i], ke, "Sensor " n ": Rohwert leer", nullptr, 0, 9998,                    \
            i == 0 ? "Leer und voll = 0: Standard (2475 = 100 %). Sonst den Rohwert aus der Live-Ansicht bei leerem bzw. vollem Tank eintragen" : nullptr), \
    FLD_INT(SuperSenseCfg, full[i], kf, "Sensor " n ": Rohwert voll", nullptr, 0, 9998, nullptr)

static const Field SS_FIELDS[] = {
    FLD_MAC(SuperSenseCfg, mac, "mac", "MAC der Box", "SSBT… – über \"Suchen\" aus der Liste wählen"),
    SS_CH_FIELDS(0, "medium", "tank_size", "empty1", "full1", "1"),
    SS_CH_FIELDS(1, "kind2", "size2", "empty2", "full2", "2"),
    SS_CH_FIELDS(2, "kind3", "size3", "empty3", "full3", "3"),
    SS_CH_FIELDS(3, "kind4", "size4", "empty4", "full4", "4"),
};

#define SS_CH_ENTITIES(n)                                                                                       \
    {E_SENSOR, "s" #n "_percent",     "Füllstand",  "%",  nullptr,          "measurement", "mdi:water-percent", 0, nullptr},    \
    {E_SENSOR, "s" #n "_liters",      "Inhalt",     "L",  "volume_storage", "measurement", "mdi:water", 0, nullptr},            \
    {E_SENSOR, "s" #n "_temperature", "Temperatur", "°C", "temperature",    "measurement", nullptr, 0, nullptr},                \
    {E_SENSOR, "s" #n "_raw",         "Rohwert",    nullptr, nullptr,       "measurement", "mdi:numeric", EF_DIAG, nullptr}

static const Entity SS_ENTITIES[] = {
    SS_CH_ENTITIES(1), SS_CH_ENTITIES(2), SS_CH_ENTITIES(3), SS_CH_ENTITIES(4),
    {E_SENSOR, "ble_rssi",  "Bluetooth Signal", "dBm", "signal_strength", "measurement", "mdi:bluetooth", EF_DIAG | EF_HIDDEN, nullptr},
    {E_SENSOR, "state",     "Status",           nullptr, nullptr,         nullptr, "mdi:bluetooth-connect", EF_DIAG | EF_ALWAYS, nullptr},
    {E_BINARY, "connected", "Verbindung",       nullptr, "connectivity",  nullptr, "mdi:bluetooth-connect", EF_DIAG | EF_ALWAYS, nullptr},
    {E_SWITCH, "release",   "Bluetooth für App freigeben", nullptr, nullptr, nullptr, "mdi:bluetooth-off", EF_CONFIG | EF_ALWAYS, nullptr},
};

class SuperSensePlugin : public Plugin {
public:
    explicit SuperSensePlugin(uint8_t index) : Plugin("supersense", "SuperSense Tanks", "Tanks", index) {}

    size_t fields(const Field*& out) override {
        out = SS_FIELDS;
        return sizeof(SS_FIELDS) / sizeof(SS_FIELDS[0]);
    }
    void* cfg() override { return &c; }
    void setDefaults() override {
        memset(&c, 0, sizeof(c));
        c.kind[0] = 1;   // sensor 1 = fresh water
        for (uint8_t i = 0; i < SS_CHANNELS; i++) c.size[i] = SUPERSENSE_DEFAULT_TANK;
    }
    void onConfigChanged() override {
        for (uint8_t i = 0; i < SS_CHANNELS; i++) {
            if (c.kind[i] >= SS_NKIND) c.kind[i] = 0;
            if (c.size[i] < 0) c.size[i] = 0;
            if (c.empty[i] < 0) c.empty[i] = 0;
            if (c.full[i] < 0) c.full[i] = 0;
        }
        if (strcmp(c.mac, lastMac) != 0) {
            strlcpy(lastMac, c.mac, sizeof(lastMac));
            hasLevels = hasTemps = false;
        }
        updates++;
        if (begun) link.setTarget(c.mac);
    }

    uint8_t bleUse() const override { return BLE_LINK; }
    const char* bleAddress() const override { return c.mac; }
    bool bleCandidate(int32_t company, const char* uuid, const char* name) const override {
        return strstr(uuid, "a0eeb190") || strncmp(name, "SSBT", 4) == 0 || containsIgnoreCase(name, "supersense");
    }
    const char* manufacturer() const override { return "SuperSense"; }
    const char* model() const override { return "Empfängerbox"; }

    void begin() override {
        // Empty notify UUID: subscribe every notifying characteristic of the service; the
        // packets are told apart by BlePacket::src (levels / temperatures)
        link.begin(id, SS_SERVICE_UUID, "", SS_WRITE_UUID);
        link.setSilenceTimeout(SS_SILENCE_MS);   // connected but silent: reconnect (sends init again)
        begun = true;
        link.setTarget(c.mac);
    }

    void loop() override {
        link.loop();
        bool up = link.connected();
        if (up && !wasUp) {
            connectedAt = millis();
            initSent = false;
        }
        wasUp = up;
        // Init packet about 1 s after connecting (as the box expects), then it notifies on its own
        if (up && !initSent && millis() - connectedAt >= SS_INIT_DELAY_MS) {
            initSent = true;
            bool ok = link.write(SS_INIT_PACKET, sizeof(SS_INIT_PACKET), true);
            devlogf(id, "Init packet: %s", ok ? "OK" : "FAILED");
            link.logNextRx(4);
        }
        if (link.changes != lastLinkChanges) {
            lastLinkChanges = link.changes;
            updates++;
        }
        BlePacket pkt;
        while (link.receive(pkt)) parse(pkt);

        bool avail = available();
        if (avail != lastAvailable) {
            lastAvailable = avail;
            updates++;
        }
    }

    bool available() override {
        return link.connected() && hasLevels && millis() - lastMsg <= SS_SILENCE_MS;
    }

    bool entityActive(const Entity& e) override {
        int ch = channelOf(e.key);
        return ch < 0 || c.kind[ch] != 0;
    }
    const char* entityName(const Entity& e, char* buf, size_t size) override {
        int ch = channelOf(e.key);
        if (ch < 0) return e.name;
        snprintf(buf, size, "%s %s", SS_KIND[c.kind[ch]], e.name);
        return buf;
    }

    void live(JsonObject o) override {
        o["state"] = stateText();
        o["ok"] = available();
        char label[40], v[64];
        // Gauge for the first used channel with a sensor
        bool gauge = false;
        for (uint8_t i = 0; i < SS_CHANNELS; i++) {
            float p = percent(i);
            if (c.kind[i] == 0 || p < 0) continue;
            if (!gauge) {
                JsonObject g = o["gauge"].to<JsonObject>();
                g["v"] = roundf(p);
                g["unit"] = "%";
                g["label"] = SS_KIND[c.kind[i]];
                g["max"] = 100;
                gauge = true;
            }
        }
        for (uint8_t i = 0; i < SS_CHANNELS; i++) {
            if (!hasLevels) break;
            int32_t raw = level[i];
            if (raw == SS_NONE && c.kind[i] == 0) continue;   // nothing connected, nothing set up
            snprintf(label, sizeof(label), "Sensor %u%s%s", i + 1, c.kind[i] ? ": " : "", c.kind[i] ? SS_KIND[c.kind[i]] : "");
            if (raw == SS_NONE) {
                strlcpy(v, "kein Sensor", sizeof(v));
            } else {
                float p = percent(i), t;
                size_t n = snprintf(v, sizeof(v), "%.0f %% · %.1f L · Rohwert %ld", p, liters(i), (long)raw);
                if (temperature(i, t)) snprintf(v + n, sizeof(v) - n, " · %.1f °C", t);
                if (c.kind[i] == 0) strlcat(v, " · nicht zugeordnet", sizeof(v));
            }
            liveRowText(o, label, v);
        }
        liveRowNum(o, "Bluetooth", link.connected() ? (float)link.rssi() : NAN, 0, "dBm");
        if (link.lastError()) liveRowText(o, "Letzter BT-Fehler", link.lastErrorText());
        liveCtrl(o, "switch", "release", "Bluetooth für App freigeben")["v"] = link.released();
        if (rawHex[0]) o["raw"] = (const char*)rawHex;
    }

    size_t entities(const Entity*& out) override {
        out = SS_ENTITIES;
        return sizeof(SS_ENTITIES) / sizeof(SS_ENTITIES[0]);
    }
    uint32_t publishIntervalMs() const override { return MQTT_SUPERSENSE_MS; }

    void publish(MqttOut& out) override {
        out.text("state", stateText());
        out.onoff("connected", link.connected());
        out.onoff("release", link.released());
        if (link.connected()) out.integer("ble_rssi", link.rssi());
        char k[20];
        for (uint8_t i = 0; i < SS_CHANNELS; i++) {
            if (c.kind[i] == 0 || !hasLevels) continue;
            float p = percent(i), t;
            snprintf(k, sizeof(k), "s%u_percent", i + 1);
            out.num(k, p < 0 ? NAN : p, 0);
            snprintf(k, sizeof(k), "s%u_liters", i + 1);
            out.num(k, p < 0 ? NAN : liters(i), 1);
            snprintf(k, sizeof(k), "s%u_raw", i + 1);
            if (level[i] >= 0) out.integer(k, level[i]);
            snprintf(k, sizeof(k), "s%u_temperature", i + 1);
            out.num(k, temperature(i, t) ? t : NAN, 1);
        }
    }

    void command(const char* key, const char* value) override {
        if (!strcmp(key, "release")) {
            link.setReleased(payloadOn(value));
            updates++;
        }
    }

private:
    SuperSenseCfg c;
    BleLink link;
    bool begun = false;
    bool wasUp = false;
    bool initSent = false;
    unsigned long connectedAt = 0;
    char lastMac[18] = "";
    uint32_t lastLinkChanges = 0;
    bool lastAvailable = false;

    bool hasLevels = false, hasTemps = false;
    int32_t level[SS_CHANNELS] = {SS_NONE, SS_NONE, SS_NONE, SS_NONE};
    int32_t temp[SS_CHANNELS] = {SS_NONE, SS_NONE, SS_NONE, SS_NONE};
    char rawHex[26] = "";
    unsigned long lastMsg = 0;

    static int channelOf(const char* key) {
        if (key[0] != 's' || key[1] < '1' || key[1] > '0' + SS_CHANNELS || key[2] != '_') return -1;
        return key[1] - '1';
    }
    float percent(uint8_t i) const { return hasLevels ? ssPercent(level[i], c.empty[i], c.full[i]) : -1.0f; }
    float liters(uint8_t i) const {
        float p = percent(i);
        return p < 0 ? 0.0f : p / 100.0f * c.size[i];
    }
    bool temperature(uint8_t i, float& t) const { return hasTemps && ssTemperature(temp[i], t); }

    const char* stateText() {
        if (link.connected() && !hasLevels) return "Warte auf Daten";
        if (link.connected() && millis() - lastMsg > SS_SILENCE_MS) return "Keine Daten";
        return link.state();
    }

    void parse(const BlePacket& p) {
        if (p.src != SS_LEVEL_SRC && p.src != SS_TEMP_SRC) return;   // the write characteristic also notifies
        int32_t v[SS_CHANNELS];
        if (!ssDecode(p.data, p.len, v)) {
            devlogf(id, "Invalid packet from %04X (%u bytes)", p.src, (unsigned)p.len);
            return;
        }
        lastMsg = millis();
        if (p.src == SS_TEMP_SRC) {
            memcpy(temp, v, sizeof(temp));
            if (!hasTemps) updates++;
            hasTemps = true;
            return;
        }
        size_t h = 0;
        for (size_t i = 0; i < p.len && h + 3 < sizeof(rawHex); i++) h += snprintf(rawHex + h, sizeof(rawHex) - h, "%02X ", p.data[i]);
        if (h) rawHex[h - 1] = '\0';
        bool changed = !hasLevels || memcmp(level, v, sizeof(level)) != 0;
        memcpy(level, v, sizeof(level));
        hasLevels = true;
        if (changed) {
            updates++;
            devlogf(id, "Levels %ld / %ld / %ld / %ld (%s)", (long)v[0], (long)v[1], (long)v[2], (long)v[3], rawHex);
        }
    }
};

Plugin* createSuperSense(uint8_t index) {
    return new SuperSensePlugin(index);
}

#endif // PLUGIN_SUPERSENSE
