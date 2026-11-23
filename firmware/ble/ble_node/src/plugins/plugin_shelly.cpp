// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Plugin "shelly": one switch channel of a Shelly Gen2+ device (Plus / Pro / Gen3 / Gen4,
// e.g. Plus 1PM, Mini 1PM Gen3) over Bluetooth, without WiFi on the Shelly.
// Protocol: Shelly's published BLE RPC (JSON-RPC 2.0 over GATT), see the Shelly knowledge
// base article "Communicating with Shelly devices via Bluetooth using RPC":
//   service  5f6d4f53-5f52-5043-5f53-56435f49445f
//   data     5f6d4f53-5f52-5043-5f64-6174615f5f5f  request / response bytes
//   tx_ctl   5f6d4f53-5f52-5043-5f74-785f63746c5f  length of the request (4 bytes, big endian)
//   rx_ctl   5f6d4f53-5f52-5043-5f72-785f63746c5f  length of the response (4 bytes, big endian)
// Request: length to tx_ctl, then the JSON to data (in MTU sized pieces). Response: length
// from rx_ctl (read until it is not 0), then data read until that many bytes are there.
// Used methods: Switch.GetStatus {id} (state, power, voltage, current, energy, temperature)
// and Switch.Set {id, on}. Shellies with a password (digest auth) are not supported.

#include "../core/plugin.h"

#if PLUGIN_SHELLY

#include "../core/ble_link.h"
#include "../core/devlog.h"
#include "../core/util.h"

#define SHELLY_SVC     "5f6d4f53-5f52-5043-5f53-56435f49445f"
#define SHELLY_DATA    "5f6d4f53-5f52-5043-5f64-6174615f5f5f"
#define SHELLY_TX_CTL  "5f6d4f53-5f52-5043-5f74-785f63746c5f"
#define SHELLY_RX_CTL  "5f6d4f53-5f52-5043-5f72-785f63746c5f"

#define SHELLY_RESP_MAX     1536     // Switch.GetStatus is ~300 bytes
#define SHELLY_ANSWER_MS    5000     // time for an answer
#define SHELLY_POLL_STEP_MS 150      // rx_ctl is read this often while waiting

struct ShellyCfg {
    char mac[18];
    int32_t channel;       // switch id 0..3 (Plus 2PM: 0 and 1)
    int32_t interval;      // seconds between status requests
};

static const Field SHELLY_FIELDS[] = {
    FLD_MAC(ShellyCfg, mac, "mac", "Shelly-MAC", "Über \"Suchen\" aus der Liste wählen (Name beginnt mit \"Shelly\")"),
    FLD_INT(ShellyCfg, channel, "channel", "Kanal", nullptr, 0, 3, "0 bei Geräten mit einem Ausgang, beim 2PM 0 oder 1"),
    FLD_INT(ShellyCfg, interval, "interval", "Abfrage alle", "s", 5, 600, "Leistung und Zustand werden so oft abgefragt"),
};

static const Entity SHELLY_ENTITIES[] = {
    {E_SWITCH, "output",      "Schalter",         nullptr, nullptr,           nullptr,            "mdi:power-socket-eu", 0, nullptr},
    {E_SENSOR, "power",       "Leistung",         "W",     "power",           "measurement",      nullptr, 0, nullptr},
    {E_SENSOR, "voltage",     "Spannung",         "V",     "voltage",         "measurement",      nullptr, 0, nullptr},
    {E_SENSOR, "current",     "Strom",            "A",     "current",         "measurement",      nullptr, 0, nullptr},
    {E_SENSOR, "energy",      "Energie",          "kWh",   "energy",          "total_increasing", nullptr, 0, nullptr},
    {E_SENSOR, "temperature", "Gerätetemperatur", "°C",    "temperature",     "measurement",      nullptr, EF_DIAG, nullptr},
    {E_SENSOR, "ble_rssi",    "Bluetooth Signal", "dBm",   "signal_strength", "measurement",      "mdi:bluetooth", EF_DIAG | EF_HIDDEN, nullptr},
    {E_SENSOR, "state",       "Status",           nullptr, nullptr,           nullptr,            "mdi:bluetooth-connect", EF_DIAG | EF_ALWAYS, nullptr},
    {E_BINARY, "connected",   "Verbindung",       nullptr, "connectivity",    nullptr,            "mdi:bluetooth-connect", EF_DIAG | EF_ALWAYS, nullptr},
};

class ShellyPlugin : public Plugin {
public:
    explicit ShellyPlugin(uint8_t index) : Plugin("shelly", "Shelly (Bluetooth)", "Shelly", index) {}

    size_t fields(const Field*& out) override {
        out = SHELLY_FIELDS;
        return sizeof(SHELLY_FIELDS) / sizeof(SHELLY_FIELDS[0]);
    }
    void* cfg() override { return &c; }
    void setDefaults() override {
        memset(&c, 0, sizeof(c));
        c.interval = 30;
    }
    void onConfigChanged() override {
        if (c.interval < 5) c.interval = 5;
        if (strcmp(c.mac, lastMac) != 0 || c.channel != lastChannel) {
            strlcpy(lastMac, c.mac, sizeof(lastMac));
            lastChannel = c.channel;
            hasData = false;
            updates++;
        }
        if (begun) link.setTarget(c.mac);
        nextPoll = 0;
    }

    uint8_t bleUse() const override { return BLE_LINK; }
    const char* bleAddress() const override { return c.mac; }
    bool bleCandidate(int32_t, const char* uuid, const char* name) const override {
        return (name && containsIgnoreCase(name, "shelly")) || (uuid && containsIgnoreCase(uuid, "5f6d4f53"));
    }
    const char* manufacturer() const override { return "Shelly"; }
    const char* model() const override { return model_[0] ? model_ : "Shelly (Bluetooth)"; }

    void begin() override {
        // rx_ctl notifies when an answer is ready; data is written (requests) and read (answers)
        link.begin(id, SHELLY_SVC, SHELLY_RX_CTL, SHELLY_DATA);
        begun = true;
        link.setTarget(c.mac);
    }

    void loop() override {
        link.loop();
        BlePacket pkt;
        while (link.receive(pkt)) {}   // rx_ctl notifications: the answer is read below anyway

        if (link.changes != lastLinkChanges) {
            lastLinkChanges = link.changes;
            if (!link.connected()) phase = IDLE;
            else nextPoll = 0;   // just connected: ask right away
            updates++;
        }
        if (!link.connected()) return;

        unsigned long now = millis();
        switch (phase) {
            case IDLE:
                if (pendingSet >= 0) {
                    char req[96];
                    snprintf(req, sizeof(req), "{\"id\":%lu,\"src\":\"vannode\",\"method\":\"Switch.Set\",\"params\":{\"id\":%ld,\"on\":%s}}",
                             (unsigned long)++reqId, (long)c.channel, pendingSet ? "true" : "false");
                    pendingSet = -1;
                    send(req, true);
                } else if (!model_[0] && !infoAsked) {
                    infoAsked = true;
                    char req[96];
                    snprintf(req, sizeof(req), "{\"id\":%lu,\"src\":\"vannode\",\"method\":\"Shelly.GetDeviceInfo\"}",
                             (unsigned long)++reqId);
                    send(req, false);
                } else if (now >= nextPoll) {
                    nextPoll = now + (unsigned long)c.interval * 1000UL;
                    char req[96];
                    snprintf(req, sizeof(req), "{\"id\":%lu,\"src\":\"vannode\",\"method\":\"Switch.GetStatus\",\"params\":{\"id\":%ld}}",
                             (unsigned long)++reqId, (long)c.channel);
                    send(req, false);
                }
                break;
            case WAIT: {
                if (now - phaseAt > SHELLY_ANSWER_MS) { fail("Keine Antwort"); break; }
                if (now - lastStep < SHELLY_POLL_STEP_MS) break;
                lastStep = now;
                uint8_t b[4];
                int n = link.read(SHELLY_RX_CTL, b, sizeof(b));
                if (n != 4) break;
                uint32_t len = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
                if (!len) break;
                if (len > SHELLY_RESP_MAX) { fail("Antwort zu groß"); break; }
                want = len;
                got = 0;
                phase = READ;
                break;
            }
            case READ: {
                if (now - phaseAt > SHELLY_ANSWER_MS) { fail("Antwort unvollständig"); break; }
                int n = link.read(SHELLY_DATA, (uint8_t*)resp + got, want - got);
                if (n <= 0) break;
                got += n;
                if (got >= want) {
                    resp[want] = '\0';
                    handleAnswer();
                    phase = IDLE;
                }
                break;
            }
        }
    }

    bool available() override { return link.connected() && hasData; }

    void live(JsonObject o) override {
        o["state"] = stateText();
        o["ok"] = available();
        if (hasData) {
            JsonObject g = o["gauge"].to<JsonObject>();
            g["v"] = isnan(power) ? 0 : roundf(power);
            g["unit"] = "W";
            g["label"] = output ? "Leistung - an" : "Leistung - aus";
            g["max"] = 2000;
            liveRowNum(o, "Spannung", voltage, 1, "V");
            liveRowNum(o, "Strom", current, 2, "A");
            liveRowNum(o, "Energie", energy, 2, "kWh");
            liveRowNum(o, "Gerätetemperatur", temperature, 0, "°C");
            liveCtrl(o, "switch", "output", "Schalter")["v"] = output;
        }
        liveRowNum(o, "Bluetooth", link.connected() ? (float)link.rssi() : NAN, 0, "dBm");
        if (model_[0]) liveRowText(o, "Gerät", model_);
        if (lastErr[0]) liveRowText(o, "Letzter Fehler", lastErr);
        if (link.lastError()) liveRowText(o, "Letzter BT-Fehler", link.lastErrorText());
    }

    size_t entities(const Entity*& out) override {
        out = SHELLY_ENTITIES;
        return sizeof(SHELLY_ENTITIES) / sizeof(SHELLY_ENTITIES[0]);
    }

    void publish(MqttOut& out) override {
        out.text("state", stateText());
        out.onoff("connected", link.connected());
        if (!hasData) return;
        out.onoff("output", output);
        out.num("power", power, 1);
        out.num("voltage", voltage, 1);
        out.num("current", current, 3);
        out.num("energy", energy, 3);
        out.num("temperature", temperature, 1);
        if (link.connected()) out.integer("ble_rssi", link.rssi());
    }

    void command(const char* key, const char* value) override {
        if (strcmp(key, "output") != 0) return;
        pendingSet = payloadOn(value) ? 1 : 0;
        output = pendingSet;   // shown right away, the status after the set confirms it
        updates++;
    }

private:
    enum Phase : uint8_t { IDLE, WAIT, READ };

    ShellyCfg c;
    BleLink link;
    bool begun = false;
    char lastMac[18] = "";
    int32_t lastChannel = -1;
    uint32_t lastLinkChanges = 0;

    Phase phase = IDLE;
    unsigned long phaseAt = 0, lastStep = 0, nextPoll = 0;
    uint32_t reqId = 0;
    bool afterSet = false;     // ask for the status right after a Switch.Set
    int8_t pendingSet = -1;
    bool infoAsked = false;
    char resp[SHELLY_RESP_MAX + 1];
    size_t want = 0, got = 0;

    bool hasData = false;
    bool output = false;
    float power = NAN, voltage = NAN, current = NAN, energy = NAN, temperature = NAN;
    char model_[32] = "";
    char lastErr[48] = "";

    const char* stateText() {
        if (!c.mac[0]) return "Kein Gerät";
        if (!link.connected()) return link.state();
        if (!hasData) return lastErr[0] ? lastErr : "Verbunden, warte auf Daten";
        return output ? "An" : "Aus";
    }

    void send(const char* req, bool isSet) {
        size_t n = strlen(req);
        uint8_t len[4] = {(uint8_t)(n >> 24), (uint8_t)(n >> 16), (uint8_t)(n >> 8), (uint8_t)n};
        if (!link.writeTo(SHELLY_TX_CTL, len, 4, true)) { fail("Schreiben fehlgeschlagen"); return; }
        size_t chunk = link.mtu() > 23 ? link.mtu() - 3 : 20;
        for (size_t off = 0; off < n; off += chunk) {
            size_t k = n - off < chunk ? n - off : chunk;
            if (!link.writeTo(SHELLY_DATA, (const uint8_t*)req + off, k, true)) { fail("Schreiben fehlgeschlagen"); return; }
        }
        afterSet = isSet;
        phase = WAIT;
        phaseAt = lastStep = millis();
    }

    void fail(const char* why) {
        devlogf(id, "RPC: %s", why);
        if (strcmp(lastErr, why) != 0) {
            strlcpy(lastErr, why, sizeof(lastErr));
            updates++;
        }
        phase = IDLE;
    }

    void handleAnswer() {
        JsonDocument d;
        if (deserializeJson(d, resp, want)) { fail("Antwort kein JSON"); return; }
        if (d["error"].is<JsonObject>()) {
            int code = d["error"]["code"] | 0;
            fail(code == 401 ? "Shelly hat ein Passwort (nicht unterstützt)" : "Shelly meldet Fehler");
            return;
        }
        JsonObject r = d["result"];
        if (r.isNull()) return;
        if (r["model"].is<const char*>() || r["app"].is<const char*>()) {   // Shelly.GetDeviceInfo
            const char* m = r["app"] | (r["model"] | "");
            strlcpy(model_, m, sizeof(model_));
            updates++;
            return;
        }
        if (afterSet) {   // Switch.Set answers {was_on}: fetch the new status
            afterSet = false;
            nextPoll = 0;
            return;
        }
        if (r["output"].is<bool>()) {   // Switch.GetStatus
            bool o = r["output"];
            float p = r["apower"] | NAN, u = r["voltage"] | NAN, i = r["current"] | NAN;
            float e = r["aenergy"]["total"] | NAN;   // Wh
            float t = r["temperature"]["tC"] | NAN;
            bool changed = !hasData || o != output || !(fabsf(p - power) < 1.0f);
            output = o;
            power = p;
            voltage = u;
            current = i;
            energy = isnan(e) ? NAN : e / 1000.0f;
            temperature = t;
            hasData = true;
            lastErr[0] = '\0';
            if (changed) updates++;
        }
    }
};

Plugin* createShelly(uint8_t index) {
    return new ShellyPlugin(index);
}

#endif // PLUGIN_SHELLY
