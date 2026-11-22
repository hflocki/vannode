// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
//
// Plugin "trumalc": Truma LevelControl gas bottle sensor. The sensor only measures when asked,
// so the plugin connects every "interval" seconds, starts a measurement, reads it and
// disconnects again (the BLE connection slot is free in between, the sensor battery is spared).
// Protocol and decoding: truma_lc_decode.h (firmware/_common).

#include "../core/plugin.h"

#if PLUGIN_TRUMALC

#include "../core/ble_link.h"
#include "../core/devlog.h"
#include "../core/util.h"
#include "truma_lc_decode.h"

struct TrumaLcCfg {
    char mac[18];
    int32_t interval;      // s between two measurements
};

static const Field TLC_FIELDS[] = {
    FLD_MAC(TrumaLcCfg, mac, "mac", "Sensor-MAC", "Über \"Suchen\" aus der Liste wählen (Name beginnt mit \"T\", z. B. \"T…Alugas 11kg\")"),
    FLD_INT(TrumaLcCfg, interval, "interval", "Messintervall", "s", 60, 3600,
            "Wie oft gemessen wird. Jede Messung kostet Sensorbatterie, Standard 600 s"),
};

static const Entity TLC_ENTITIES[] = {
    {E_SENSOR, "level",       "Füllstand",        "%",   nullptr,           "measurement", "mdi:gas-cylinder", 0, nullptr},
    {E_SENSOR, "kg",          "Gasmenge",         "kg",  "weight",          "measurement", "mdi:gas-cylinder", 0, nullptr},
    {E_SENSOR, "battery",     "Batterie",         "%",   "battery",         "measurement", nullptr, EF_DIAG, nullptr},
    {E_SENSOR, "temperature", "Temperatur",       "°C",  "temperature",     "measurement", nullptr, EF_DIAG, nullptr},
    {E_SENSOR, "quality",     "Messqualität",     nullptr, nullptr,         "measurement", "mdi:signal", EF_DIAG, nullptr},
    {E_SENSOR, "state",       "Status",           nullptr, nullptr,         nullptr,       "mdi:bluetooth-connect", EF_DIAG | EF_ALWAYS, nullptr},
    {E_SENSOR, "ble_rssi",    "Bluetooth Signal", "dBm", "signal_strength", "measurement", "mdi:bluetooth", EF_DIAG | EF_HIDDEN, nullptr},
    {E_BUTTON, "measure",     "Jetzt messen",     nullptr, nullptr,         nullptr,       "mdi:refresh", EF_ALWAYS, nullptr},
    {E_SWITCH, "release",     "Bluetooth für App freigeben", nullptr, nullptr, nullptr,   "mdi:bluetooth-off", EF_CONFIG | EF_ALWAYS, nullptr},
};

#define TLC_CONNECT_TIMEOUT_MS  45000   // no connection within this time: give up this round
#define TLC_RETRY_MS            60000   // failed round: next try after 1 min
#define TLC_TRIGGER_GAP_MS      400     // between the two trigger bytes

class TrumaLcPlugin : public Plugin {
public:
    explicit TrumaLcPlugin(uint8_t index) : Plugin("trumalc", "Truma LevelControl", "Gasflasche", index) {}

    size_t fields(const Field*& out) override {
        out = TLC_FIELDS;
        return sizeof(TLC_FIELDS) / sizeof(TLC_FIELDS[0]);
    }
    void* cfg() override { return &c; }
    void setDefaults() override {
        memset(&c, 0, sizeof(c));
        c.interval = 600;
    }
    void onConfigChanged() override {
        if (strcmp(c.mac, lastMac) != 0) {
            strlcpy(lastMac, c.mac, sizeof(lastMac));
            hasData = false;
            nextAt = 0;
            finish(false);
            updates++;
        }
    }

    uint8_t bleUse() const override { return BLE_LINK; }
    const char* bleAddress() const override { return c.mac; }
    bool bleCandidate(int32_t company, const char* uuid, const char* name) const override {
        return company == TLC_COMPANY_OLD || strstr(uuid, "0a00") != nullptr ||
               containsIgnoreCase(name, "LevelControl");
    }
    const char* manufacturer() const override { return "Truma"; }

    void begin() override {
        link.begin(id, TLC_SERVICE_UUID, TLC_MEASURE_UUID, TLC_TRIGGER_UUID);
        begun = true;
        nextAt = millis() + 5000;   // first measurement shortly after the start
    }

    void loop() override {
        link.loop();
        unsigned long now = millis();
        if (link.changes != lastLinkChanges) {
            lastLinkChanges = link.changes;
            updates++;
        }
        BlePacket pkt;
        while (link.receive(pkt)) {}   // the value is read, notifications are not needed

        switch (phase) {
            case P_IDLE:
                if (c.mac[0] && !link.released() && (long)(now - nextAt) >= 0) {
                    phase = P_CONNECT;
                    phaseAt = now;
                    link.setTarget(c.mac);
                    updates++;
                }
                break;
            case P_CONNECT:
                if (link.connected()) {
                    uint8_t pw[8];
                    int n = link.read(TLC_PASSWORD_UUID, pw, sizeof(pw));
                    if (!tlcKey(pw, n < 0 ? 0 : (size_t)n, key)) {
                        devlogf(id, "Password not readable (%d bytes)", n);
                        finish(false);
                        break;
                    }
                    uint8_t b = TLC_TRIGGER_INIT;
                    link.writeAuto(&b, 1);
                    phase = P_TRIGGER;
                    phaseAt = now;
                } else if (now - phaseAt > TLC_CONNECT_TIMEOUT_MS) {
                    devlogf(id, "No connection");
                    finish(false);
                }
                break;
            case P_TRIGGER:
                if (!link.connected()) { finish(false); break; }
                if (now - phaseAt >= TLC_TRIGGER_GAP_MS) {
                    uint8_t b = TLC_TRIGGER_GO;
                    link.writeAuto(&b, 1);
                    phase = P_WAIT;
                    phaseAt = now;
                }
                break;
            case P_WAIT:
                if (!link.connected()) { finish(false); break; }
                if (now - phaseAt >= TLC_MEASURE_WAIT_MS) readMeasurement();
                break;
        }

        bool avail = available();
        if (avail != lastAvail) {
            lastAvail = avail;
            updates++;
        }
    }

    bool available() override {
        unsigned long maxAge = (unsigned long)c.interval * 3000UL + 120000UL;   // three rounds missed
        return hasData && millis() - lastOk <= maxAge;
    }

    void live(JsonObject o) override {
        o["state"] = stateText();
        o["ok"] = available();
        JsonObject g = o["gauge"].to<JsonObject>();
        if (hasData) g["v"] = r.level;
        else g["v"] = nullptr;
        g["unit"] = "%";
        g["label"] = "Füllstand";
        liveRowNum(o, "Gasmenge", hasData ? r.kg : NAN, 1, "kg");
        liveRowNum(o, "Batterie", hasData ? (float)r.battery : NAN, 0, "%");
        liveRowNum(o, "Temperatur", hasData ? (float)r.temperature : NAN, 0, "°C");
        liveRowNum(o, "Messqualität", hasData && r.quality >= 0 ? (float)r.quality : NAN, 0, "/ 3");
        liveRowNum(o, "Bluetooth", rssi ? (float)rssi : NAN, 0, "dBm");
        liveRowNum(o, "Alter", hasData ? (millis() - lastOk) / 1000.0f : NAN, 0, "s");
        if (link.lastError()) liveRowText(o, "Letzter BT-Fehler", link.lastErrorText());
        JsonObject b = liveCtrl(o, "buttons", "action", nullptr);
        JsonArray bl = b["b"].to<JsonArray>();
        JsonArray m = bl.add<JsonArray>();
        m.add("measure");
        m.add("Jetzt messen");
        liveCtrl(o, "switch", "release", "Bluetooth für App freigeben")["v"] = link.released();
        if (rawHex[0]) o["raw"] = (const char*)rawHex;
    }

    size_t entities(const Entity*& out) override {
        out = TLC_ENTITIES;
        return sizeof(TLC_ENTITIES) / sizeof(TLC_ENTITIES[0]);
    }
    uint32_t publishIntervalMs() const override { return MQTT_GAS_MS; }

    void publish(MqttOut& out) override {
        out.text("state", stateText());
        out.onoff("release", link.released());
        if (rssi) out.integer("ble_rssi", rssi);
        if (!hasData) return;
        out.integer("level", r.level);
        out.num("kg", r.kg, 1);
        out.integer("battery", r.battery);
        out.integer("temperature", r.temperature);
        if (r.quality >= 0) out.integer("quality", r.quality);
    }

    void command(const char* key, const char* value) override {
        if (!strcmp(key, "release")) {
            bool rel = payloadOn(value);
            if (rel) finish(false);
            link.setReleased(rel);
            updates++;
        } else if (!strcmp(key, "measure") || (!strcmp(key, "action") && !strcmp(value, "measure"))) {
            if (phase == P_IDLE) nextAt = millis();
        }
    }

private:
    enum Phase : uint8_t { P_IDLE, P_CONNECT, P_TRIGGER, P_WAIT };

    TrumaLcCfg c;
    BleLink link;
    bool begun = false;
    char lastMac[18] = "";
    uint32_t lastLinkChanges = 0;
    Phase phase = P_IDLE;
    unsigned long phaseAt = 0;
    unsigned long nextAt = 0;
    uint8_t key[5] = {0};

    TlcReading r = {};
    bool hasData = false;
    bool lastAvail = false;
    bool lastFailed = false;
    unsigned long lastOk = 0;
    int rssi = 0;
    char rawHex[3 * 12 + 1] = "";

    void readMeasurement() {
        uint8_t d[16];
        int n = link.read(TLC_MEASURE_UUID, d, sizeof(d));
        rssi = link.rssi();
        size_t h = 0;
        for (int i = 0; i < n && i < 12 && h + 3 < sizeof(rawHex); i++)
            h += snprintf(rawHex + h, sizeof(rawHex) - h, "%02X ", d[i]);
        if (h) rawHex[h - 1] = '\0';
        TlcReading m;
        if (n > 0 && tlcDecode(key, d, (size_t)n, m)) {
            r = m;
            hasData = true;
            lastOk = millis();
            devlogf(id, "Level %u %% | %.1f kg | battery %u %% | %d C | quality %d",
                    r.level, r.kg, r.battery, r.temperature, r.quality);
            finish(true);
        } else {
            devlogf(id, "No valid measurement (%d bytes)", n);
            finish(false);
        }
    }

    // End of a round: disconnect and plan the next one
    void finish(bool ok) {
        if (begun) link.setTarget("");
        lastFailed = !ok && phase != P_IDLE;
        phase = P_IDLE;
        unsigned long now = millis();
        nextAt = now + (ok ? (unsigned long)c.interval * 1000UL : TLC_RETRY_MS);
        updates++;
    }

    const char* stateText() {
        if (c.mac[0] == '\0') return "Kein Sensor";
        if (link.released()) return "Für App freigegeben";
        if (phase == P_CONNECT) return "Verbinde";
        if (phase != P_IDLE) return "Misst";
        if (!hasData) return lastFailed ? "Keine Antwort" : "Warte auf Messung";
        if (!available()) return "Keine Daten";
        return lastFailed ? "OK (letzte Messung fehlgeschlagen)" : "OK";
    }
};

Plugin* createTrumaLc(uint8_t index) {
    return new TrumaLcPlugin(index);
}

#endif // PLUGIN_TRUMALC
