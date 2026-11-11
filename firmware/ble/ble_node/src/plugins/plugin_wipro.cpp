// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
//
// Plugin "wipro": read-only status of a Thitronik WiPro III alarm module over BLE.
// Read-only - the plugin never arms, disarms or changes the alarm, it only reports status.
// Protocol and value mapping from the maintainer's own project hflocki/wipro2ha.
//
// The module sends an 8-byte status as a notification on the status characteristic
// (example 01 8C 00 21 01 05 00 00). Only subscribing and reading is done, so the phone
// app and other Bluetooth devices are unaffected.
//   byte 1  armed when (byte1 & 0x0C) == 0x0C
//   byte 2  alarm reason (0 = none; 1..31 = wireless contact number; see REASONS)
//   byte 6  bitmask of the open wireless contacts, one bit each (bit 0 = contact0 ...)
// Use "Bluetooth für App freigeben" to let the phone app connect to the module when needed.

#include "../core/plugin.h"

#if PLUGIN_WIPRO

#include "../core/ble_link.h"
#include "../core/devlog.h"
#include "../core/util.h"

// Default status characteristic ("WiPro Status"). Empty service = search all services for it.
#define WIPRO_SERVICE_UUID ""
#define WIPRO_STATUS_UUID  "57695072-6f20-5374-6174-757320202020"

#define WIPRO_ARM_MASK     0x0C   // Byte 1: armed when both bits set
#define WIPRO_MIN_LEN      7      // Need at least byte 6
#define WIPRO_CONTACTS     8      // Bits in byte 6 (contact0 .. contact7)

// Byte 2: alarm reason. Codes 1..31 are "wireless contact N"; these are the fixed ones.
struct WiproReason { uint8_t value; const char* name; };
static const WiproReason WIPRO_REASONS[] = {
    {0,   "Kein Alarm"},
    {32,  "Einbruch Tür (Innenlicht-Kontakt)"},
    {33,  "Einbruch Tür (CAN-Bus)"},
    {36,  "Gaswarner getriggert"},
    {37,  "Störsender erkannt"},
    {38,  "Funk-Kabelschleife unterbrochen"},
    {40,  "Funk-Wasserdetektor"},
    {43,  "Funk-Gasdetektor (CO)"},
    {224, "SMS-Alarm"},
    {225, "Panik-Alarm"},
};

struct WiproCfg {
    char mac[18];
    char pin[8];           // Optional BLE pairing PIN (digits), empty = no pairing (default)
    char names[96];        // Contact names for bit 0..7, comma separated (empty = "Kontakt N")
    char service[40];      // GATT service UUID, empty = search all services
    char notify[40];       // Status characteristic, empty = WiPro Status default
};

static const Field WIPRO_FIELDS[] = {
    FLD_MAC(WiproCfg, mac, "mac", "Alarm-MAC", "Über \"Suchen\" aus der Liste wählen"),
    FLD_TEXT(WiproCfg, pin, "pin", "Bluetooth-PIN", "Leer lassen - das Modul koppelt nicht. Nur ausfüllen, "
                                              "falls es beim Verbinden doch eine Kopplung verlangt."),
    FLD_TEXT(WiproCfg, names, "names", "Kontaktnamen", "Namen der Kontakte (Bit 0-7), mit Komma getrennt, "
                                              "z. B. Tür vorn,Tür hinten,Fenster. Leer = Kontakt 0, Kontakt 1 ..."),
    FLD_TEXT(WiproCfg, service, "service", "Service-UUID", "Leer = alle Services durchsuchen (Standard)"),
    FLD_TEXT(WiproCfg, notify, "notify", "Status-UUID", "Leer = Standard-Characteristic. * = jede, die senden kann"),
};

static const Entity WIPRO_ENTITIES[] = {
    {E_BINARY, "armed",     "Scharf",          nullptr, "safety",       nullptr, "mdi:shield-home", 0, nullptr},
    {E_SENSOR, "reason",    "Alarmgrund",      nullptr, nullptr,        nullptr, "mdi:shield-alert", 0, nullptr},
    {E_SENSOR, "open",      "Offene Kontakte", nullptr, nullptr,        nullptr, "mdi:door-open", 0, nullptr},
    {E_BINARY, "contact0",  "Kontakt 0",       nullptr, "safety",       nullptr, "mdi:motion-sensor", 0, nullptr},
    {E_BINARY, "contact1",  "Kontakt 1",       nullptr, "safety",       nullptr, "mdi:motion-sensor", 0, nullptr},
    {E_BINARY, "contact2",  "Kontakt 2",       nullptr, "safety",       nullptr, "mdi:motion-sensor", 0, nullptr},
    {E_BINARY, "contact3",  "Kontakt 3",       nullptr, "safety",       nullptr, "mdi:motion-sensor", 0, nullptr},
    {E_BINARY, "contact4",  "Kontakt 4",       nullptr, "safety",       nullptr, "mdi:motion-sensor", EF_HIDDEN, nullptr},
    {E_BINARY, "contact5",  "Kontakt 5",       nullptr, "safety",       nullptr, "mdi:motion-sensor", EF_HIDDEN, nullptr},
    {E_BINARY, "contact6",  "Kontakt 6",       nullptr, "safety",       nullptr, "mdi:motion-sensor", EF_HIDDEN, nullptr},
    {E_BINARY, "contact7",  "Kontakt 7",       nullptr, "safety",       nullptr, "mdi:motion-sensor", EF_HIDDEN, nullptr},
    {E_BINARY, "connected", "Verbindung",      nullptr, "connectivity", nullptr, nullptr, EF_DIAG | EF_ALWAYS, nullptr},
    {E_SENSOR, "state",     "Status",          nullptr, nullptr,        nullptr, "mdi:bluetooth-connect", EF_DIAG | EF_ALWAYS, nullptr},
    {E_SENSOR, "ble_rssi",  "Bluetooth Signal", "dBm", "signal_strength", "measurement", "mdi:bluetooth", EF_DIAG | EF_HIDDEN, nullptr},
    {E_SENSOR, "raw",       "Rohdaten",        nullptr, nullptr,        nullptr, "mdi:hexadecimal", EF_DIAG | EF_HIDDEN, nullptr},
    {E_SWITCH, "release",   "Bluetooth für App freigeben", nullptr, nullptr, nullptr, "mdi:bluetooth-off", EF_CONFIG | EF_ALWAYS, nullptr},
};

class WiproPlugin : public Plugin {
public:
    explicit WiproPlugin(uint8_t index) : Plugin("wipro", "WiPro Alarm", "Alarm", index) {}

    size_t fields(const Field*& out) override {
        out = WIPRO_FIELDS;
        return sizeof(WIPRO_FIELDS) / sizeof(WIPRO_FIELDS[0]);
    }
    void* cfg() override { return &c; }
    void setDefaults() override { memset(&c, 0, sizeof(c)); }
    void onConfigChanged() override {
        pinValue = blePinParse(c.pin);
        if (strcmp(c.mac, lastMac) != 0) {
            strlcpy(lastMac, c.mac, sizeof(lastMac));
            hasData = false;
            updates++;
        }
        if (begun) {
            link.setUuids(effUuid(c.service, WIPRO_SERVICE_UUID), effUuid(c.notify, WIPRO_STATUS_UUID), "");
            link.setTarget(c.mac, pinValue);
        }
    }

    uint8_t bleUse() const override { return BLE_LINK; }
    const char* bleAddress() const override { return c.mac; }
    bool bleCandidate(int32_t company, const char* uuid, const char* name) const override {
        return containsIgnoreCase(name, "WiPro") || containsIgnoreCase(name, "Thitronik");
    }
    const char* manufacturer() const override { return "Thitronik"; }

    void begin() override {
        // Read only: no write characteristic
        link.begin(id, effUuid(c.service, WIPRO_SERVICE_UUID), effUuid(c.notify, WIPRO_STATUS_UUID), "");
        link.setSilenceTimeout(WIPRO_OFFLINE_MS);   // Connected but silent: reconnect
        begun = true;
        link.setTarget(c.mac, pinValue);
    }

    void loop() override {
        link.loop();
        if (link.changes != lastLinkChanges) {
            lastLinkChanges = link.changes;
            updates++;
        }
        BlePacket pkt;
        while (link.receive(pkt)) parseStatus(pkt.data, pkt.len);

        bool avail = available();
        if (avail != lastAvailable) {
            lastAvailable = avail;
            updates++;
        }
    }

    bool available() override {
        return link.connected() && hasData && millis() - lastMsg <= WIPRO_OFFLINE_MS;
    }

    void live(JsonObject o) override {
        o["state"] = stateText();
        o["ok"] = available();
        if (hasData) {
            liveRowText(o, "Scharf", armed ? "Ja" : "Nein");
            liveRowText(o, "Alarmgrund", reasonText(reasonCode));
            liveRowText(o, "Offen", openText());
            char name[20];
            for (uint8_t i = 0; i < WIPRO_CONTACTS; i++)
                liveRowText(o, contactName(i, name, sizeof(name)), contactBit(i) ? "offen" : "zu");
        }
        liveRowNum(o, "Bluetooth", link.connected() ? (float)link.rssi() : NAN, 0, "dBm");
        if (link.lastError()) liveRowText(o, "Letzter BT-Fehler", link.lastErrorText());
        liveCtrl(o, "switch", "release", "Bluetooth für App freigeben")["v"] = link.released();
        if (hasData) o["raw"] = (const char*)rawHex;
    }

    size_t entities(const Entity*& out) override {
        out = WIPRO_ENTITIES;
        return sizeof(WIPRO_ENTITIES) / sizeof(WIPRO_ENTITIES[0]);
    }
    uint32_t publishIntervalMs() const override { return MQTT_WIPRO_MS; }

    void publish(MqttOut& out) override {
        out.text("state", stateText());
        out.onoff("connected", link.connected());
        out.onoff("release", link.released());
        if (link.connected()) out.integer("ble_rssi", link.rssi());
        if (!hasData) return;
        out.onoff("armed", armed);
        out.text("reason", reasonText(reasonCode));
        out.text("open", openText());
        out.text("raw", rawHex);
        char key[12];
        for (uint8_t i = 0; i < WIPRO_CONTACTS; i++) {
            snprintf(key, sizeof(key), "contact%u", i);
            out.onoff(key, contactBit(i));
        }
    }

    void command(const char* key, const char* value) override {
        if (!strcmp(key, "release")) {
            link.setReleased(payloadOn(value));
            updates++;
        }
        // Read-only plugin: no other commands (never arms or disarms the alarm)
    }

private:
    // Empty field = default, "*" = no filter (search everything), otherwise the value entered
    static const char* effUuid(const char* field, const char* def) {
        if (!field[0]) return def;
        if (!strcmp(field, "*")) return "";
        return field;
    }

    WiproCfg c;
    BleLink link;
    bool begun = false;
    char lastMac[18] = "";
    int32_t pinValue = BLE_NO_PIN;
    uint32_t lastLinkChanges = 0;
    bool lastAvailable = false;

    bool hasData = false;
    bool armed = false;
    uint8_t reasonCode = 0;    // byte 2
    uint8_t contacts = 0;      // byte 6 (bitmask)
    char rawHex[26] = "";      // up to 8 bytes as hex text
    unsigned long lastMsg = 0;

    bool contactBit(uint8_t i) const { return (contacts >> i) & 1; }

    // Name of contact i: the i-th entry of the names field, else "Kontakt i"
    const char* contactName(uint8_t i, char* buf, size_t size) const {
        const char* p = c.names;
        for (uint8_t n = 0; n < i && p; n++) {
            p = strchr(p, ',');
            if (p) p++;
        }
        if (p && *p && *p != ',') {
            const char* end = strchr(p, ',');
            size_t len = end ? (size_t)(end - p) : strlen(p);
            while (len && *p == ' ') { p++; len--; }        // trim leading spaces
            while (len && p[len - 1] == ' ') len--;         // trim trailing spaces
            if (len) {
                if (len >= size) len = size - 1;
                memcpy(buf, p, len);
                buf[len] = '\0';
                return buf;
            }
        }
        snprintf(buf, size, "Kontakt %u", i);
        return buf;
    }

    const char* stateText() {
        if (link.connected() && !hasData) return "Warte auf Daten";
        if (link.connected() && millis() - lastMsg > WIPRO_OFFLINE_MS) return "Keine Daten";
        return link.state();
    }

    // Comma-separated names of the open contacts, "keine" if none
    const char* openText() {
        static char buf[128];
        char name[20];
        buf[0] = '\0';
        for (uint8_t i = 0; i < WIPRO_CONTACTS; i++) {
            if (!contactBit(i)) continue;
            if (buf[0]) strlcat(buf, ", ", sizeof(buf));
            strlcat(buf, contactName(i, name, sizeof(name)), sizeof(buf));
        }
        if (!buf[0]) strlcpy(buf, "keine", sizeof(buf));
        return buf;
    }

    // Byte 2: 1..31 = wireless contact N, otherwise a named reason
    static const char* reasonText(uint8_t v) {
        if (v >= 1 && v <= 31) {
            static char b[24];
            snprintf(b, sizeof(b), "Funkkontakt %u", v);
            return b;
        }
        for (const WiproReason& w : WIPRO_REASONS) if (w.value == v) return w.name;
        return "Unbekannt";
    }

    void parseStatus(const uint8_t* d, size_t n) {
        if (n < WIPRO_MIN_LEN) {
            devlogf(id, "Status too short (%u bytes)", (unsigned)n);
            return;
        }
        bool newArmed = (d[1] & WIPRO_ARM_MASK) == WIPRO_ARM_MASK;
        uint8_t newReason = d[2];
        uint8_t newContacts = d[6];
        bool changed = !hasData || newArmed != armed || newReason != reasonCode || newContacts != contacts;
        armed = newArmed;
        reasonCode = newReason;
        contacts = newContacts;
        size_t h = 0;
        for (size_t i = 0; i < n && i < 8 && h + 3 < sizeof(rawHex); i++)
            h += snprintf(rawHex + h, sizeof(rawHex) - h, "%02X ", d[i]);
        if (h) rawHex[h - 1] = '\0';
        hasData = true;
        lastMsg = millis();
        if (changed) {
            updates++;
            devlogf(id, "Status: %s | Grund: %s | offen: %s | %s",
                    armed ? "scharf" : "unscharf", reasonText(reasonCode), openText(), rawHex);
        }
    }
};

Plugin* createWipro(uint8_t index) {
    return new WiproPlugin(index);
}

#endif // PLUGIN_WIPRO
