// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Plugin "supersense": SuperSense tank level sensor (Pro, Solid, Laser) over a BLE connection.
// Read-only. After connecting, the init packet is written once and the sensor notifies
// 8-byte measurements on its own. Decoding in supersense_decode.h (shared with the tanklevel
// node, host test in firmware/ble/tests). Protocol source: see NOTICE.md.
//
// The unit of the level value is not confirmed, so the plugin converts with a factor
// (litres per raw unit, web UI) and also publishes the raw value.

#include "../core/plugin.h"

#if PLUGIN_SUPERSENSE

#include "../core/ble_link.h"
#include "../core/devlog.h"
#include "../core/util.h"
#include "../../supersense_decode.h"

struct SuperSenseCfg {
    char mac[18];
    float tank_size;   // litres
    float factor;      // litres per raw unit
    uint8_t medium;    // 0 water, 1 grey water, 2 black water, 3 other (HA icon / name only)
};

static const Field SS_FIELDS[] = {
    FLD_MAC(SuperSenseCfg, mac, "mac", "Sensor-MAC", "Über \"Suchen\" aus der Liste wählen"),
    FLD_SELECT(SuperSenseCfg, medium, "medium", "Tank", "Frischwasser|Grauwasser|Schwarzwasser|Sonstiges", nullptr),
    FLD_FLOAT(SuperSenseCfg, tank_size, "tank_size", "Tankgröße", "L", 1, 5000, 0.1f, nullptr),
    FLD_FLOAT(SuperSenseCfg, factor, "factor", "Liter je Rohwert-Einheit", "L", 0.00001f, 10, 0.00001f,
              "Laut Quelle ist der Rohwert in ml (0,001). Noch nicht am echten Sensor bestätigt: "
              "Rohwert bei bekanntem Inhalt ablesen und Faktor = Liter / Rohwert eintragen."),
};

static const Entity SS_ENTITIES[] = {
    {E_SENSOR, "percent",     "Füllstand",        "%",   nullptr,           "measurement", "mdi:water-percent", 0, nullptr},
    {E_SENSOR, "liters",      "Inhalt",           "L",   "volume_storage",  "measurement", "mdi:water", 0, nullptr},
    {E_SENSOR, "temperature", "Temperatur",       "°C",  "temperature",     "measurement", nullptr, 0, nullptr},
    {E_SENSOR, "raw",         "Rohwert",          nullptr, nullptr,         "measurement", "mdi:numeric", EF_DIAG, nullptr},
    {E_SENSOR, "pressure",    "Rohdruck",         nullptr, nullptr,         "measurement", "mdi:gauge", EF_DIAG | EF_HIDDEN, nullptr},
    {E_SENSOR, "sensor_status", "Sensorstatus",   nullptr, nullptr,         nullptr, "mdi:information-outline", EF_DIAG | EF_HIDDEN, nullptr},
    {E_SENSOR, "ble_rssi",    "Bluetooth Signal", "dBm", "signal_strength", "measurement", "mdi:bluetooth", EF_DIAG | EF_HIDDEN, nullptr},
    {E_SENSOR, "state",       "Status",           nullptr, nullptr,         nullptr, "mdi:bluetooth-connect", EF_DIAG | EF_ALWAYS, nullptr},
    {E_BINARY, "connected",   "Verbindung",       nullptr, "connectivity",  nullptr, "mdi:bluetooth-connect", EF_DIAG | EF_ALWAYS, nullptr},
    {E_SWITCH, "release",     "Bluetooth für App freigeben", nullptr, nullptr, nullptr, "mdi:bluetooth-off", EF_CONFIG | EF_ALWAYS, nullptr},
};

static const char* const SS_MEDIUM[] = {"Frischwasser", "Grauwasser", "Schwarzwasser", "Tank"};

class SuperSensePlugin : public Plugin {
public:
    explicit SuperSensePlugin(uint8_t index) : Plugin("supersense", "SuperSense Tanksensor", "Tank", index) {}

    size_t fields(const Field*& out) override {
        out = SS_FIELDS;
        return sizeof(SS_FIELDS) / sizeof(SS_FIELDS[0]);
    }
    void* cfg() override { return &c; }
    void setDefaults() override {
        memset(&c, 0, sizeof(c));
        c.tank_size = SUPERSENSE_DEFAULT_TANK;
        c.factor = SS_DEFAULT_FACTOR;
    }
    void onConfigChanged() override {
        if (c.tank_size < 1.0f) c.tank_size = SUPERSENSE_DEFAULT_TANK;
        if (c.factor <= 0.0f) c.factor = SS_DEFAULT_FACTOR;
        if (c.medium > 3) c.medium = 0;
        if (strcmp(c.mac, lastMac) != 0) {
            strlcpy(lastMac, c.mac, sizeof(lastMac));
            hasData = false;
        }
        updates++;
        if (begun) link.setTarget(c.mac);
    }

    uint8_t bleUse() const override { return BLE_LINK; }
    const char* bleAddress() const override { return c.mac; }
    bool bleCandidate(int32_t company, const char* uuid, const char* name) const override {
        return strstr(uuid, "a0eeb190") || containsIgnoreCase(name, "supersense");
    }
    const char* manufacturer() const override { return "SuperSense"; }

    void begin() override {
        link.begin(id, SS_SERVICE_UUID, SS_NOTIFY_UUID, SS_WRITE_UUID);
        link.setSilenceTimeout(SS_SILENCE_MS);   // Connected but silent: reconnect (sends init again)
        begun = true;
        link.setTarget(c.mac);
    }

    void loop() override {
        link.loop();
        // Fresh connection: send the init packet once, then the sensor notifies on its own
        bool up = link.connected();
        if (up && !wasUp) {
            bool ok = link.write(SS_INIT_PACKET, sizeof(SS_INIT_PACKET), true);
            devlogf(id, "Init packet: %s", ok ? "OK" : "FAILED");
            link.logNextRx(3);
        }
        wasUp = up;
        if (link.changes != lastLinkChanges) {
            lastLinkChanges = link.changes;
            updates++;
        }
        BlePacket pkt;
        while (link.receive(pkt)) parse(pkt.data, pkt.len);

        bool avail = available();
        if (avail != lastAvailable) {
            lastAvailable = avail;
            updates++;
        }
    }

    bool available() override {
        return link.connected() && hasData && millis() - lastMsg <= SS_SILENCE_MS;
    }

    void live(JsonObject o) override {
        o["state"] = stateText();
        o["ok"] = available();
        if (hasData) {
            JsonObject g = o["gauge"].to<JsonObject>();
            g["v"] = roundf(percent());
            g["unit"] = "%";
            g["label"] = SS_MEDIUM[c.medium];
            g["max"] = 100;
            liveRowNum(o, "Inhalt", liters(), 1, "L");
            liveRowNum(o, "Rohwert", (float)cur.level, 0, nullptr);
            liveRowNum(o, "Temperatur", cur.tempValid ? cur.temperature : NAN, 1, "°C");
            liveRowNum(o, "Rohdruck", cur.pressure >= 0 ? (float)cur.pressure : NAN, 0, nullptr);
            liveRowNum(o, "Sensorstatus", cur.status >= 0 ? (float)cur.status : NAN, 0, nullptr);
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
        if (!hasData) return;
        out.num("percent", percent(), 0);
        out.num("liters", liters(), 1);
        out.integer("raw", cur.level);
        out.num("temperature", cur.tempValid ? cur.temperature : NAN, 1);
        if (cur.pressure >= 0) out.integer("pressure", cur.pressure);
        if (cur.status >= 0) out.integer("sensor_status", cur.status);
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
    char lastMac[18] = "";
    uint32_t lastLinkChanges = 0;
    bool lastAvailable = false;

    bool hasData = false;
    SuperSenseReading cur = {};
    char rawHex[26] = "";
    unsigned long lastMsg = 0;

    float liters() const { return ssLiters(cur.level, c.factor); }
    float percent() const {
        float p = c.tank_size > 0 ? liters() / c.tank_size * 100.0f : 0.0f;
        return p < 0 ? 0 : (p > 100 ? 100 : p);
    }

    const char* stateText() {
        if (link.connected() && !hasData) return "Warte auf Daten";
        if (link.connected() && millis() - lastMsg > SS_SILENCE_MS) return "Keine Daten";
        return link.state();
    }

    void parse(const uint8_t* d, size_t n) {
        size_t h = 0;
        for (size_t i = 0; i < n && i < 8 && h + 3 < sizeof(rawHex); i++)
            h += snprintf(rawHex + h, sizeof(rawHex) - h, "%02X ", d[i]);
        if (h) rawHex[h - 1] = '\0';

        SuperSenseReading r;
        if (!ssDecode(d, n, r)) {
            devlogf(id, "Packet with %u bytes ignored", (unsigned)n);
            return;
        }
        if (!r.levelValid) {
            devlogf(id, "Invalid level (%s)", rawHex);
            return;
        }
        bool changed = !hasData || r.level != cur.level || r.status != cur.status;
        cur = r;
        hasData = true;
        lastMsg = millis();
        if (changed) {
            updates++;
            devlogf(id, "Level raw %ld -> %.1f L (%.0f %%), %s", (long)r.level, liters(), percent(), rawHex);
        }
    }
};

Plugin* createSuperSense(uint8_t index) {
    return new SuperSensePlugin(index);
}

#endif // PLUGIN_SUPERSENSE
