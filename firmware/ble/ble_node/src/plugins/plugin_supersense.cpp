// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Plugin "supersense": SuperSense receiver box ("SSBT…") with up to 4 tank sensors, over a BLE
// connection. Read-only. About 1 s after connecting the init packet is written, then the box
// notifies the levels and temperatures of all 4 channels. Each channel is set up as a tank
// (kind, size, taught raw values for empty / full, shown as content or free space). Decoding in
// supersense_decode.h (shared with the tanklevel node, host test in firmware/ble/tests).
//
// Update interval: the box notifies by itself. The levels are averaged over an interval and
// only then shown and published - a short one when standing, a long one while driving (water
// sloshes, the alarms on the display should not flap). Driving comes from registryDriving():
// OBD adapter of this node (engine running) or the departure check of the display over MQTT.

#include "../core/plugin.h"

#if PLUGIN_SUPERSENSE

#include "../core/ble_link.h"
#include "../core/devlog.h"
#include "../core/registry.h"
#include "../core/util.h"
#include "supersense_decode.h"

#define SS_KINDS "nicht benutzt|Frischwasser|Grauwasser|Schwarzwasser|Gas|Sonstiges"
static const char* const SS_KIND[] = {"Sensor", "Frischwasser", "Grauwasser", "Schwarzwasser", "Gas", "Tank"};
#define SS_NKIND (sizeof(SS_KIND) / sizeof(SS_KIND[0]))

#define SS_SHOWS "Inhalt|Freier Platz"

struct SuperSenseCfg {
    char mac[18];
    uint8_t kind[SS_CHANNELS];      // 0 = not used
    float size[SS_CHANNELS];        // litres
    int32_t empty[SS_CHANNELS];     // taught raw value at empty (0 = default scale)
    int32_t full[SS_CHANNELS];      // taught raw value at full  (0 = default scale)
    uint8_t show[SS_CHANNELS];      // web UI tile: 0 = content, 1 = free space
    int32_t poll_s;                 // update interval when standing (s)
    int32_t drive_s;                // update interval while driving (s)
};

// Channel 1 keeps the keys of 2.2 ("medium", "tank_size") so existing settings carry over
#define SS_CH_FIELDS(i, kk, ks, ke, kf, n)                                                                  \
    FLD_SELECT(SuperSenseCfg, kind[i], kk, "Sensor " n ": Tank", SS_KINDS, nullptr),                       \
    FLD_SELECT(SuperSenseCfg, show[i], "show" n, "Sensor " n ": Anzeige", SS_SHOWS,                         \
               i == 0 ? "Nur die Kachel in der Live-Ansicht: Inhalt oder noch freier Platz (z. B. beim Abwassertank)" : nullptr), \
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
    FLD_INT(SuperSenseCfg, poll_s, "poll_s", "Aktualisieren im Stand alle", "s", 1, 3600,
            "Füllstände werden über diese Zeit gemittelt, dann angezeigt und per MQTT gesendet"),
    FLD_INT(SuperSenseCfg, drive_s, "drive_s", "Aktualisieren während der Fahrt alle", "s", 1, 3600,
            "Länger als im Stand, weil das Wasser schwappt (Warnungen am Display springen sonst). Fahrt = OBD-Adapter an diesem Node meldet Motor läuft, oder der Abfahrtscheck des Displays"),
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
        c.poll_s = SUPERSENSE_POLL_S;
        c.drive_s = SUPERSENSE_DRIVE_S;
    }
    void onConfigChanged() override {
        for (uint8_t i = 0; i < SS_CHANNELS; i++) {
            if (c.kind[i] >= SS_NKIND) c.kind[i] = 0;
            if (c.size[i] < 0) c.size[i] = 0;
            if (c.empty[i] < 0) c.empty[i] = 0;
            if (c.full[i] < 0) c.full[i] = 0;
            if (c.show[i] > 1) c.show[i] = 0;
        }
        if (c.poll_s < 1) c.poll_s = SUPERSENSE_POLL_S;    // settings of 2.6.1 and older
        if (c.drive_s < 1) c.drive_s = SUPERSENSE_DRIVE_S;
        if (strcmp(c.mac, lastMac) != 0) {
            strlcpy(lastMac, c.mac, sizeof(lastMac));
            hasLevels = hasTemps = false;
            resetWindow();
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
        applyWindow();

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
        // One tile per channel, all four the same size; the raw value is the latest one (for
        // teaching empty / full), level and litres are the averaged, published ones
        char v[40];
        JsonArray tanks = o["tanks"].to<JsonArray>();
        for (uint8_t i = 0; i < SS_CHANNELS; i++) {
            JsonObject t = tanks.add<JsonObject>();
            bool used = c.kind[i] != 0;
            if (used) {
                t["l"] = SS_KIND[c.kind[i]];
            } else {
                snprintf(v, sizeof(v), "Sensor %u", i + 1);
                t["l"] = (const char*)v;   // stack buffer: copied
            }
            int32_t raw = hasLevels ? lastRaw[i] : SS_NONE;
            if (raw >= 0) t["r"] = raw;
            float p = percent(i);
            if (!used || p < 0) {
                t["off"] = true;
                t["s"] = !used ? (raw >= 0 ? "nicht zugeordnet" : "nicht benutzt") : (hasLevels ? "kein Sensor" : "–");
                continue;
            }
            bool free = c.show[i] == 1;
            float l = liters(i);
            t["p"] = roundf(p);
            t["v"] = roundf(free ? 100.0f - p : p);
            t["u"] = "%";
            if (free) snprintf(v, sizeof(v), "frei %.1f L", c.size[i] - l);
            else snprintf(v, sizeof(v), "%.1f L", l);
            float tc;
            if (temperature(i, tc)) snprintf(v + strlen(v), sizeof(v) - strlen(v), " · %.1f °C", tc);
            t["s"] = (const char*)v;
            if (free) t["free"] = true;
        }
        bool drive = registryDriving();
        snprintf(v, sizeof(v), "alle %ld s (%s)", (long)(drive ? c.drive_s : c.poll_s), drive ? "Fahrt" : "Stand");
        liveRowText(o, "Aktualisierung", v);
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
    int32_t level[SS_CHANNELS] = {SS_NONE, SS_NONE, SS_NONE, SS_NONE};     // averaged, shown / published
    int32_t lastRaw[SS_CHANNELS] = {SS_NONE, SS_NONE, SS_NONE, SS_NONE};   // latest packet
    // Averaging window: sum and count of valid raw values per channel since the last update
    int64_t winSum[SS_CHANNELS] = {0, 0, 0, 0};
    uint16_t winCount[SS_CHANNELS] = {0, 0, 0, 0};
    bool winData = false;            // at least one level packet in the window
    unsigned long winStart = 0;
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
        if (memcmp(lastRaw, v, sizeof(lastRaw)) != 0)
            devlogf(id, "Raw %ld / %ld / %ld / %ld (%s)", (long)v[0], (long)v[1], (long)v[2], (long)v[3], rawHex);
        memcpy(lastRaw, v, sizeof(lastRaw));
        for (uint8_t i = 0; i < SS_CHANNELS; i++) {
            if (v[i] < 0 || winCount[i] == UINT16_MAX) continue;
            winSum[i] += v[i];
            winCount[i]++;
        }
        winData = true;
        // First levels after connecting: show them right away, the interval starts from there
        if (!hasLevels) applyWindow(true);
    }

    void resetWindow() {
        memset(winSum, 0, sizeof(winSum));
        memset(winCount, 0, sizeof(winCount));
        winData = false;
        winStart = millis();
    }

    // End of the interval (standing / driving): averaged levels become the shown ones
    void applyWindow(bool now = false) {
        uint32_t ms = (uint32_t)(registryDriving() ? c.drive_s : c.poll_s) * 1000UL;
        if (!now && (!winData || millis() - winStart < ms)) return;
        if (!winData) return;
        int32_t nl[SS_CHANNELS];
        for (uint8_t i = 0; i < SS_CHANNELS; i++)
            nl[i] = winCount[i] ? (int32_t)((winSum[i] + winCount[i] / 2) / winCount[i]) : SS_NONE;
        bool changed = !hasLevels || memcmp(level, nl, sizeof(level)) != 0;
        memcpy(level, nl, sizeof(level));
        hasLevels = true;
        resetWindow();
        if (changed) {
            updates++;
            devlogf(id, "Levels %ld / %ld / %ld / %ld", (long)nl[0], (long)nl[1], (long)nl[2], (long)nl[3]);
        }
    }
};

Plugin* createSuperSense(uint8_t index) {
    return new SuperSensePlugin(index);
}

#endif // PLUGIN_SUPERSENSE
