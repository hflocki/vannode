// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
//
// Plugin "obd": vehicle values from an OBD2 adapter with ELM327 command set over BLE
// (ELM327 BLE adapters, STN based adapters such as the vLinker MC+).
//
// Read only: the plugin sends standard requests (SAE J1979 mode 01 "current data" and
// mode 03 "stored trouble codes") and never clears codes or changes anything in the vehicle.
//
//   1. After connecting: ATZ, ATE0, ATL0, ATS0, ATH0, ATAT1, ATSP0 (automatic protocol; the
//      vehicle profile may set a fixed one), STI
//   2. ATRV (adapter measures the board voltage) - works with the ignition off
//   3. Engine running (voltage >= "Motor läuft ab"), engine detection "Drehzahl" (control unit
//      asked every OBD_PROBE_MS) or mode "immer": 0100/0120/... = which PIDs the vehicle
//      supports (kept in NVS, so Home Assistant only gets those), ATDPN, 03
//   4. Poll the supported PIDs of the table below in turn, every "Abfrage alle" seconds. PIDs
//      with a route of the vehicle profile are sent with a different header / receive filter
//   5. Engine off for OBD_STOP_MS: stop polling; after OBD_IDLE_MS the connection is paused
//      for "Ruhepause" minutes, so the adapter can go to sleep and the battery is spared
// Vehicle profiles hold what was confirmed in real vehicles (protocol, which control unit has
// to be asked). Further values can be tried with the MQTT command "raw".

#include "../core/plugin.h"

#if PLUGIN_OBD

#include <Preferences.h>
#include "../core/ble_link.h"
#include "../core/devlog.h"
#include "../core/mqtt_ha.h"
#include "../core/util.h"
#include "obd_decode.h"

// BLE layouts of common adapters: service, notify, write. Index = setting "gatt".
struct ObdGatt { const char* service; const char* notify; const char* write; };
static const ObdGatt OBD_GATT[] = {
    {"", "", "*"},               // 0 automatic: every notifying characteristic, write = first writable one
    {"fff0", "fff1", "fff2"},    // 1 many ELM327 BLE clones
    {"ffe0", "ffe1", "ffe1"},    // 2 HM-10 style serial module
    {"18f0", "2af0", "2af1"},    // 3 Vgate / Viecar
    {"e7810a71-73ae-499d-8c15-faa9aef0c3f2", "bef8d6c9-9c21-4c9e-b632-bd58c1009f9f",
     "bef8d6c9-9c21-4c9e-b632-bd58c1009f9f"},   // 4 Vgate iCar Pro BLE / vLinker (BLE mode)
    {nullptr, nullptr, nullptr}, // 5 own UUIDs from the settings
};
#define OBD_GATT_OWN 5

// Standard values (mode 01). Same keys as OBD_ENTITIES below.
struct ObdValue {
    uint8_t pid;
    uint8_t formula;
    uint8_t decimals;
    const char* key;
    const char* label;   // web UI
    const char* unit;
};
static const ObdValue OBD_VALUES[] = {
    {0x0C, OBD_F_RPM,       0, "rpm",          "Drehzahl",              "U/min"},
    {0x0D, OBD_F_A,         0, "speed",        "Geschwindigkeit",       "km/h"},
    {0x05, OBD_F_A_M40,     0, "coolant",      "Kühlwasser",            "°C"},
    {0x5C, OBD_F_A_M40,     0, "oil_temp",     "Öltemperatur",          "°C"},
    {0x2F, OBD_F_PCT,       0, "fuel",         "Tankfüllstand",         "%"},
    {0x5E, OBD_F_AB_D20,    1, "fuel_rate",    "Verbrauch",             "l/h"},
    {0x9D, OBD_F_FUEL_GS,   1, "fuel_rate",    "Verbrauch",             "l/h"},   // alternative to 5E, see valueUsed()
    {0x85, OBD_F_REAGENT_LVL, 0, "adblue",     "AdBlue",                "%"},
    {0x9B, OBD_F_DEF_TEMP,  0, "adblue_temp",  "AdBlue-Temperatur",     "°C"},
    {0x9B, OBD_F_DEF_CONC,  1, "adblue_conc",  "AdBlue-Konzentration",  "%"},     // same answer as the line above
    {0x04, OBD_F_PCT,       0, "load",         "Motorlast",             "%"},
    {0x46, OBD_F_A_M40,     0, "ambient",      "Außentemperatur",       "°C"},
    {0x0F, OBD_F_A_M40,     0, "intake_temp",  "Ansaugluft",            "°C"},
    {0x0B, OBD_F_A,         0, "map",          "Saugrohrdruck",         "kPa"},
    {0x33, OBD_F_A,         0, "baro",         "Luftdruck",             "kPa"},
    {0x10, OBD_F_AB_D100,   1, "maf",          "Luftmasse",             "g/s"},
    {0x11, OBD_F_PCT,       0, "throttle",     "Drosselklappe",         "%"},
    {0x42, OBD_F_AB_D1000,  1, "ecu_voltage",  "Spannung Steuergerät",  "V"},
    {0x1F, OBD_F_AB,        0, "runtime",      "Laufzeit seit Start",   "s"},
    {0x31, OBD_F_AB,        0, "dist_clear",   "Strecke seit Fehler löschen", "km"},
    {0xA6, OBD_F_ABCD_D10,  1, "odometer",     "Kilometerstand",        "km"},
    {0x01, OBD_F_DTC_COUNT, 0, "dtc_count",    "Anzahl Fehlercodes",    nullptr},
};
#define OBD_VALUE_COUNT (sizeof(OBD_VALUES) / sizeof(OBD_VALUES[0]))
#define OBD_MASK_WORDS  6   // PIDs 01..C0 (0x00, 0x20 ... 0xA0)
#define OBD_DTC_MAX     8
#define OBD_WRITE_CHUNK 20  // Payload of one BLE packet with the default MTU

static const Entity OBD_ENTITIES[] = {
    {E_SENSOR, "rpm",         "Drehzahl",              "rpm",  nullptr,         "measurement", "mdi:engine", 0, nullptr},
    {E_SENSOR, "speed",       "Geschwindigkeit",       "km/h", "speed",         "measurement", nullptr, 0, nullptr},
    {E_SENSOR, "coolant",     "Kühlwasser",            "°C",   "temperature",   "measurement", "mdi:coolant-temperature", 0, nullptr},
    {E_SENSOR, "oil_temp",    "Öltemperatur",          "°C",   "temperature",   "measurement", "mdi:oil-temperature", 0, nullptr},
    {E_SENSOR, "fuel",        "Tankfüllstand",         "%",    nullptr,         "measurement", "mdi:gas-station", 0, nullptr},
    {E_SENSOR, "fuel_rate",   "Verbrauch",             "L/h",  nullptr,         "measurement", "mdi:fuel", 0, nullptr},
    {E_SENSOR, "adblue",      "AdBlue",                "%",    nullptr,         "measurement", "mdi:water-outline", 0, nullptr},
    {E_SENSOR, "adblue_temp", "AdBlue-Temperatur",     "°C",   "temperature",   "measurement", nullptr, EF_HIDDEN, nullptr},
    {E_SENSOR, "adblue_conc", "AdBlue-Konzentration",  "%",    nullptr,         "measurement", "mdi:test-tube", EF_DIAG | EF_HIDDEN, nullptr},
    {E_SENSOR, "load",        "Motorlast",             "%",    nullptr,         "measurement", "mdi:engine", EF_HIDDEN, nullptr},
    {E_SENSOR, "ambient",     "Außentemperatur",       "°C",   "temperature",   "measurement", nullptr, 0, nullptr},
    {E_SENSOR, "intake_temp", "Ansaugluft",            "°C",   "temperature",   "measurement", nullptr, EF_HIDDEN, nullptr},
    {E_SENSOR, "map",         "Saugrohrdruck",         "kPa",  "pressure",      "measurement", nullptr, EF_HIDDEN, nullptr},
    {E_SENSOR, "baro",        "Luftdruck",             "kPa",  "atmospheric_pressure", "measurement", nullptr, EF_HIDDEN, nullptr},
    {E_SENSOR, "maf",         "Luftmasse",             "g/s",  nullptr,         "measurement", "mdi:weather-windy", EF_HIDDEN, nullptr},
    {E_SENSOR, "throttle",    "Drosselklappe",         "%",    nullptr,         "measurement", "mdi:valve", EF_HIDDEN, nullptr},
    {E_SENSOR, "ecu_voltage", "Spannung Steuergerät",  "V",    "voltage",       "measurement", nullptr, EF_DIAG | EF_HIDDEN, nullptr},
    {E_SENSOR, "runtime",     "Laufzeit seit Start",   "s",    "duration",      "measurement", nullptr, EF_HIDDEN, nullptr},
    {E_SENSOR, "dist_clear",  "Strecke seit Fehler löschen", "km", "distance",  nullptr,       nullptr, EF_DIAG | EF_HIDDEN, nullptr},
    {E_SENSOR, "odometer",    "Kilometerstand",        "km",   "distance",      "total_increasing", "mdi:counter", 0, nullptr},
    {E_SENSOR, "dtc_count",   "Anzahl Fehlercodes",    nullptr, nullptr,        "measurement", "mdi:alert-circle-outline", 0, nullptr},
    // Not from the PID table
    {E_SENSOR, "voltage",     "Bordspannung",          "V",    "voltage",       "measurement", nullptr, 0, nullptr},
    {E_BINARY, "running",     "Motor läuft",           nullptr, "running",      nullptr,       "mdi:engine", EF_ALWAYS, nullptr},
    {E_BINARY, "mil",         "Motorkontrollleuchte",  nullptr, "problem",      nullptr,       "mdi:engine-off", 0, nullptr},
    {E_SENSOR, "dtc",         "Fehlercodes",           nullptr, nullptr,        nullptr,       "mdi:alert-circle", 0, nullptr},
    {E_BUTTON, "read_dtc",    "Fehlercodes lesen",     nullptr, nullptr,        nullptr,       "mdi:magnify", EF_ALWAYS, nullptr},
    {E_SENSOR, "protocol",    "Protokoll",             nullptr, nullptr,        nullptr,       "mdi:car-connected", EF_DIAG, nullptr},
    {E_SENSOR, "adapter",     "Adapter",               nullptr, nullptr,        nullptr,       "mdi:chip", EF_DIAG | EF_ALWAYS, nullptr},
    {E_BINARY, "connected",   "Verbindung",            nullptr, "connectivity", nullptr,       nullptr, EF_DIAG | EF_ALWAYS, nullptr},
    {E_SENSOR, "state",       "Status",                nullptr, nullptr,        nullptr,       "mdi:bluetooth-connect", EF_DIAG | EF_ALWAYS, nullptr},
    {E_SENSOR, "ble_rssi",    "Bluetooth Signal",      "dBm",  "signal_strength", "measurement", "mdi:bluetooth", EF_DIAG | EF_HIDDEN, nullptr},
    {E_SENSOR, "raw",         "Letzte Testantwort",    nullptr, nullptr,        nullptr,       "mdi:console", EF_DIAG | EF_HIDDEN | EF_ALWAYS, nullptr},
    {E_SWITCH, "release",     "Bluetooth für App freigeben", nullptr, nullptr,  nullptr,       "mdi:bluetooth-off", EF_CONFIG | EF_ALWAYS, nullptr},
};

// ---- Vehicle profiles ----
// A route sends one PID with another header or receive filter: "pre" before the request,
// "post" afterwards (back to the functional request to all control units).
struct ObdRoute { uint8_t pid; const char* pre; const char* post; };
struct ObdProfile {
    const char* protocol;        // ATSP command at init
    uint8_t fuelPid;             // PID used for "fuel_rate" if both 5E and 9D are supported
    const ObdRoute* routes;
    uint8_t routeCount;
};

// Fiat Ducato (Euro 6, ISO 15765-4 CAN 29 bit 500 kbit/s). Answer IDs 18DAF110 (engine),
// 18DAF101 (AdBlue values, stays awake longest), 18DAF118 (gearbox). PID 85 is a multi-frame
// answer sent by two control units, so it is asked physically at 01. Findings as documented
// by github.com/Moormerlander/ESPHome-BLE-OBD (checked in a vehicle with 9-speed automatic);
// the 0131 filter is not yet confirmed there.
static const ObdRoute OBD_ROUTES_DUCATO[] = {
    {0x85, "ATSHDA01F1", "ATSHDB33F1"},
    {0x9B, "ATSHDA01F1", "ATSHDB33F1"},
    {0x31, "ATCRA18DAF110", "ATAR"},     // km since codes cleared from the engine, not the gearbox
};
static const ObdProfile OBD_PROFILES[] = {
    {"ATSP0", 0x5E, nullptr, 0},                                    // 0 standard, automatic protocol
    {"ATSP7", 0x9D, OBD_ROUTES_DUCATO,
     sizeof(OBD_ROUTES_DUCATO) / sizeof(OBD_ROUTES_DUCATO[0])},     // 1 Fiat Ducato
};
#define OBD_PROFILE_COUNT (sizeof(OBD_PROFILES) / sizeof(OBD_PROFILES[0]))

struct ObdCfg {
    char mac[18];
    char pin[8];
    uint8_t gatt;          // index into OBD_GATT
    char service[40];      // own UUIDs (gatt = OBD_GATT_OWN)
    char notify[40];
    char write[40];
    uint8_t profile;       // index into OBD_PROFILES
    uint8_t mode;          // 0 = only while the engine runs, 1 = whenever the control unit answers
    uint8_t detect;        // engine running from: 0 = board voltage, 1 = engine speed (ask the ECU)
    float run_v;           // voltage from which the engine counts as running
    int32_t poll_s;        // seconds between two rounds of requests
    int32_t rest_min;      // engine off: pause the connection this long (0 = keep it)
};

static const Field OBD_FIELDS[] = {
    FLD_MAC(ObdCfg, mac, "mac", "Adapter-MAC", "Über \"Suchen\" aus der Liste wählen (Name z. B. vLinker, OBDII, OBDBLE)"),
    FLD_TEXT(ObdCfg, pin, "pin", "Bluetooth-PIN", "Meist leer. Nur ausfüllen, wenn der Adapter beim Verbinden eine PIN verlangt"),
    FLD_SELECT(ObdCfg, gatt, "gatt", "Bluetooth-Kanäle",
               "Automatisch|FFF0 (FFF1 / FFF2)|FFE0 (FFE1)|18F0 (2AF0 / 2AF1)|Vgate / vLinker (E7810A71)|Eigene UUIDs",
               "Automatisch passt für die meisten Adapter. Im Log steht nach dem Verbinden, welche Kanäle es gibt"),
    FLD_TEXT(ObdCfg, service, "service", "Service-UUID", "Nur bei \"Eigene UUIDs\""),
    FLD_TEXT(ObdCfg, notify, "notify", "Notify-UUID", "Nur bei \"Eigene UUIDs\", leer = jede, die senden kann"),
    FLD_TEXT(ObdCfg, write, "write", "Write-UUID", "Nur bei \"Eigene UUIDs\""),
    FLD_SELECT(ObdCfg, profile, "profile", "Fahrzeug",
               "Standard (alle Fahrzeuge)|Fiat Ducato (Euro 6)",
               "Fiat Ducato: festes Protokoll CAN 29 Bit, AdBlue direkt vom zuständigen Steuergerät, "
               "Verbrauch aus der Kraftstoffmasse"),
    FLD_SELECT(ObdCfg, mode, "mode", "Abfragen",
               "Nur bei laufendem Motor|Immer, wenn das Steuergerät antwortet",
               "\"Nur bei laufendem Motor\" schont die Starterbatterie: ohne laufenden Motor fragt der Node das "
               "Fahrzeug nicht ab und lässt den Adapter schlafen"),
    FLD_SELECT(ObdCfg, detect, "detect", "Motor erkennen an",
               "Bordspannung|Drehzahl (Steuergerät jede Minute fragen)",
               "Bordspannung: der Node fragt das Fahrzeug erst, wenn die Lichtmaschine lädt. Fahrzeuge mit "
               "intelligenter Lichtmaschine (z. B. Fiat Ducato, Leerlauf nur 12,3-12,6 V) brauchen \"Drehzahl\""),
    FLD_FLOAT(ObdCfg, run_v, "run_v", "Motor läuft ab", "V", 12.0f, 15.0f, 0.1f,
              "Nur bei \"Motor erkennen an: Bordspannung\": Spannung, ab der der Motor als laufend gilt. Typisch 13,2 V"),
    FLD_INT(ObdCfg, poll_s, "poll_s", "Abfrage alle", "s", 1, 60, "Pause zwischen zwei Abfragerunden aller Werte"),
    FLD_INT(ObdCfg, rest_min, "rest_min", "Ruhepause", "min", 0, 60,
            "Motor aus: so lange trennt der Node die Verbindung, damit der Adapter schlafen kann. 0 = verbunden bleiben"),
};

class ObdPlugin : public Plugin {
public:
    explicit ObdPlugin(uint8_t index) : Plugin("obd", "OBD2-Adapter", "Fahrzeug", index) {
        for (size_t i = 0; i < OBD_VALUE_COUNT; i++) vals[i] = NAN;
    }

    size_t fields(const Field*& out) override {
        out = OBD_FIELDS;
        return sizeof(OBD_FIELDS) / sizeof(OBD_FIELDS[0]);
    }
    void* cfg() override { return &c; }
    void setDefaults() override {
        memset(&c, 0, sizeof(c));
        c.gatt = 0;
        c.profile = 0;
        c.mode = 0;
        c.detect = 0;
        c.run_v = 13.2f;
        c.poll_s = 2;
        c.rest_min = 2;
    }
    void onConfigChanged() override {
        pinValue = blePinParse(c.pin);
        if (strcmp(c.mac, lastMac) != 0) {
            strlcpy(lastMac, c.mac, sizeof(lastMac));
            resetValues();
            updates++;
        }
        if (c.profile >= OBD_PROFILE_COUNT) c.profile = 0;
        if (c.profile != lastProfile) {
            // Other protocol and routes: start the adapter again with the new init
            bool first = lastProfile == 0xFF;
            lastProfile = c.profile;
            if (!first) {
                resetValues();
                updates++;
                if (link.connected()) link.disconnect();
            }
        }
        if (begun) {
            applyUuids();
            if (!resting) link.setTarget(c.mac, pinValue);
        }
    }

    uint8_t bleUse() const override { return BLE_LINK; }
    const char* bleAddress() const override { return c.mac; }
    bool bleCandidate(int32_t company, const char* uuid, const char* name) const override {
        return containsIgnoreCase(name, "OBD") || containsIgnoreCase(name, "vLink") ||
               containsIgnoreCase(name, "ELM327") || containsIgnoreCase(name, "V-LINK");
    }
    const char* manufacturer() const override { return "ELM327 / STN"; }

    void begin() override {
        loadMask();
        const ObdGatt g = gatt();
        link.begin(id, g.service, g.notify, g.write);
        begun = true;
        link.setTarget(c.mac, pinValue);
    }

    void loop() override {
        unsigned long now = millis();
        link.loop();
        if (link.changes != lastLinkChanges) {
            lastLinkChanges = link.changes;
            updates++;
        }

        // Rest: connection paused while the engine is off
        if (resting) {
            if ((long)(now - restUntil) >= 0) {
                resting = false;
                devlogf(id, "Rest over, connecting again");
                link.setTarget(c.mac, pinValue);
                updates++;
            }
            trackAvailable();
            return;
        }

        if (!link.connected()) {
            if (phase != P_DOWN) linkDown();
            trackAvailable();
            return;
        }
        if (phase == P_DOWN && link.canWrite()) {
            phase = P_INIT;
            initStep = 0;
            busy = false;
            rxLen = 0;
            timeouts = 0;
            devlogf(id, "Connected, initialising the adapter");
        }

        BlePacket pkt;
        while (link.receive(pkt)) receive(pkt.data, pkt.len);

        if (busy && now - sentAt > timeoutMs) onTimeout();
        if (!busy && phase != P_DOWN) next(now);
        trackAvailable();
    }

    bool available() override {
        return link.connected() && ecuOk && millis() - lastEcu <= OBD_STALE_MS;
    }

    void live(JsonObject o) override {
        o["state"] = stateText();
        o["ok"] = available();
        liveRowText(o, "Motor", running ? "läuft" : "aus");
        liveRowNum(o, "Bordspannung", voltage, 1, "V");
        for (size_t i = 0; i < OBD_VALUE_COUNT; i++) {
            if (isnan(vals[i]) || !valueUsed(i)) continue;
            if (OBD_VALUES[i].formula == OBD_F_DTC_COUNT) continue;   // shown with the codes
            liveRowNum(o, OBD_VALUES[i].label, vals[i], OBD_VALUES[i].decimals, OBD_VALUES[i].unit);
        }
        if (dtcKnown) {
            liveRowText(o, "Motorkontrollleuchte", mil ? "an" : "aus");
            liveRowText(o, "Fehlercodes", dtcText);
        }
        if (protocol) liveRowText(o, "Protokoll", elmProtocolName(protocol));
        if (adapter[0]) liveRowText(o, "Adapter", adapter);
        liveRowNum(o, "Bluetooth", link.connected() ? (float)link.rssi() : NAN, 0, "dBm");
        if (link.lastError()) liveRowText(o, "Letzter BT-Fehler", link.lastErrorText());
        if (link.connected()) {
            JsonObject b = liveCtrl(o, "buttons", "action", nullptr);
            JsonArray arr = b["b"].to<JsonArray>();
            JsonArray x = arr.add<JsonArray>();
            x.add("read_dtc");
            x.add("Fehlercodes lesen");
        }
        liveCtrl(o, "switch", "release", "Bluetooth für App freigeben")["v"] = link.released();
        if (rawResp[0]) o["raw"] = (const char*)rawResp;
    }

    size_t entities(const Entity*& out) override {
        out = OBD_ENTITIES;
        return sizeof(OBD_ENTITIES) / sizeof(OBD_ENTITIES[0]);
    }
    // Values of the PID table only if the vehicle supports them (known after the first contact)
    bool entityActive(const Entity& e) override {
        bool inTable = false;
        for (size_t i = 0; i < OBD_VALUE_COUNT; i++) {
            if (strcmp(e.key, OBD_VALUES[i].key)) continue;
            inTable = true;
            if (maskKnown && valueUsed(i)) return true;
        }
        return !inTable;
    }
    uint32_t publishIntervalMs() const override { return MQTT_OBD_MS; }

    void publish(MqttOut& out) override {
        out.text("state", stateText());
        out.onoff("connected", link.connected());
        out.onoff("release", link.released());
        out.onoff("running", running);
        if (adapter[0]) out.text("adapter", adapter);
        if (rawResp[0]) out.text("raw", rawResp);
        if (link.connected()) out.integer("ble_rssi", link.rssi());
        out.num("voltage", voltage, 1);
        if (protocol) out.text("protocol", elmProtocolName(protocol));
        if (dtcKnown) {
            out.onoff("mil", mil);
            out.text("dtc", dtcText);
        }
        for (size_t i = 0; i < OBD_VALUE_COUNT; i++) {
            if (valueUsed(i)) out.num(OBD_VALUES[i].key, vals[i], OBD_VALUES[i].decimals);
        }
    }

    void command(const char* key, const char* value) override {
        if (!strcmp(key, "release")) {
            link.setReleased(payloadOn(value));
            updates++;
        } else if (!strcmp(key, "read_dtc") || (!strcmp(key, "action") && !strcmp(value, "read_dtc"))) {
            dtcRequested = true;
            devlogf(id, "Reading trouble codes requested");
        } else if (!strcmp(key, "raw")) {
            // Diagnosis: any adapter command, e.g. "ATRV" or "0105"; the answer goes to the log
            // and to the topic "raw". Read the codes only - the plugin never sends mode 04.
            char cmd[sizeof(rawCmd)];
            strlcpy(cmd, value, sizeof(cmd));
            size_t n = strlen(cmd);
            while (n && (cmd[n - 1] == '\r' || cmd[n - 1] == '\n' || cmd[n - 1] == ' ')) cmd[--n] = '\0';
            const char* p = cmd;
            while (*p == ' ') p++;
            if (!*p) return;
            if (p[0] == '0' && p[1] == '4' && p[2] == '\0') {
                devlogf(id, "Command 04 (clear codes) is not allowed");
                return;
            }
            strlcpy(rawCmd, p, sizeof(rawCmd));
            rawPending = true;
            devlogf(id, "Test command queued: %s", rawCmd);
        }
    }

private:
    enum Phase : uint8_t { P_DOWN, P_INIT, P_IDLE, P_PROBE, P_POLL };
    enum Kind : uint8_t { K_INIT, K_VOLT, K_SUPPORT, K_PROTO, K_PID, K_DTC, K_RAW, K_ROUTE };

    ObdCfg c;
    BleLink link;
    bool begun = false;
    char lastMac[18] = "";
    uint8_t lastProfile = 0xFF;
    bool routeSet = false;           // "pre" of the route of OBD_VALUES[pollIdx] was sent
    const char* routePost = nullptr; // "post" still to send after the routed request
    int32_t pinValue = BLE_NO_PIN;
    uint32_t lastLinkChanges = 0;
    bool lastAvailable = false;

    // Command in flight
    Phase phase = P_DOWN;
    bool busy = false;
    Kind kind = K_INIT;
    uint8_t arg = 0;
    char cur[28] = "";
    unsigned long sentAt = 0;
    uint32_t timeoutMs = OBD_CMD_TIMEOUT_MS;
    uint8_t timeouts = 0;
    char rx[512];
    size_t rxLen = 0;

    uint8_t initStep = 0;
    uint8_t probeBase = 0;       // next 0x00 / 0x20 ... request while probing
    bool probeDone = false;
    bool firstRequest = true;    // first request after ATSP0 may take long (protocol search)
    size_t pollIdx = 0;
    bool roundVoltage = false;   // voltage read in this round
    unsigned long roundStart = 0;
    unsigned long lastVolt = 0;
    unsigned long lastProbe = 0;
    unsigned long idleSince = 0;
    unsigned long lastRunning = 0;
    uint8_t noData = 0;

    // Rest (connection paused)
    bool resting = false;
    unsigned long restUntil = 0;

    // Vehicle state
    uint32_t mask[OBD_MASK_WORDS] = {0};
    bool maskKnown = false;
    bool ecuOk = false;
    unsigned long lastEcu = 0;
    bool running = false;
    float voltage = NAN;
    float vals[OBD_VALUE_COUNT];
    char protocol = '\0';
    char adapter[32] = "";
    bool dtcKnown = false;
    bool dtcRequested = false;
    bool mil = false;
    int dtcCount = -1;           // from PID 01, -1 = unknown
    char dtcText[OBD_DTC_MAX * 7 + 16] = "";
    char rawCmd[24] = "";
    bool rawPending = false;
    char rawResp[96] = "";

    // ---- Setup helpers ----
    ObdGatt gatt() const {
        if (c.gatt == OBD_GATT_OWN) return {c.service, c.notify, c.write[0] ? c.write : "*"};
        uint8_t i = c.gatt < OBD_GATT_OWN ? c.gatt : 0;
        return OBD_GATT[i];
    }
    void applyUuids() {
        const ObdGatt g = gatt();
        link.setUuids(g.service, g.notify, g.write);
    }

    void resetValues() {
        for (size_t i = 0; i < OBD_VALUE_COUNT; i++) vals[i] = NAN;
        voltage = NAN;
        running = false;
        ecuOk = false;
        dtcKnown = false;
        mil = false;
        dtcText[0] = '\0';
        protocol = '\0';
        adapter[0] = '\0';
    }

    bool pidSupported(uint8_t pid) const { return obdPidInMask(mask, OBD_MASK_WORDS, pid); }

    const ObdProfile& profile() const { return OBD_PROFILES[c.profile < OBD_PROFILE_COUNT ? c.profile : 0]; }

    const ObdRoute* route(uint8_t pid) const {
        const ObdProfile& p = profile();
        for (uint8_t i = 0; i < p.routeCount; i++) if (p.routes[i].pid == pid) return &p.routes[i];
        return nullptr;
    }

    // Value i is read from this vehicle: PID supported and, for "fuel_rate" (PID 5E or 9D),
    // only the source of the profile if the vehicle has both
    bool valueUsed(size_t i) const {
        uint8_t pid = OBD_VALUES[i].pid;
        if (!pidSupported(pid)) return false;
        if (pid == 0x5E || pid == 0x9D) {
            uint8_t other = pid == 0x5E ? 0x9D : 0x5E;
            if (pidSupported(other) && pid != profile().fuelPid) return false;
        }
        return true;
    }

    // Value i needs its own request (not already asked by an earlier line with the same PID)
    bool pollWanted(size_t i) const {
        if (!valueUsed(i)) return false;
        for (size_t k = 0; k < i; k++) if (OBD_VALUES[k].pid == OBD_VALUES[i].pid) return false;
        return true;
    }

    // Supported PIDs are kept over restarts, so Home Assistant keeps its entities
    void loadMask() {
        Preferences p;
        if (!p.begin("obd", true)) return;
        maskKnown = p.getBytes(id, mask, sizeof(mask)) == sizeof(mask);
        p.end();
    }
    void saveMask() {
        Preferences p;
        if (!p.begin("obd", false)) return;
        p.putBytes(id, mask, sizeof(mask));
        p.end();
    }

    void linkDown() {
        phase = P_DOWN;
        busy = false;
        rxLen = 0;
        ecuOk = false;
        firstRequest = true;
        probeDone = false;
        routeSet = false;
        routePost = nullptr;
        if (running) {
            running = false;
            devlogf(id, "Connection lost: engine state unknown, shown as off");
        }
        updates++;
    }

    void trackAvailable() {
        bool avail = available();
        if (avail != lastAvailable) {
            lastAvailable = avail;
            updates++;
        }
    }

    const char* stateText() {
        if (resting) return "Ruhepause (Motor aus)";
        if (!link.connected()) return link.state();
        switch (phase) {
        case P_DOWN:
        case P_INIT: return "Adapter startet";
        case P_PROBE: return "Suche Fahrzeug";
        case P_POLL: return ecuOk ? "Verbunden" : "Keine Antwort";
        case P_IDLE: break;
        }
        if (c.mode == 0 && (c.detect == 1 || (!isnan(voltage) && voltage < c.run_v))) return "Motor aus";
        return "Zündung aus";
    }

    // ---- Sending and receiving ----
    void send(const char* cmd, Kind k, uint8_t a, uint32_t timeout) {
        strlcpy(cur, cmd, sizeof(cur));
        char line[sizeof(cur) + 2];
        size_t n = strlcpy(line, cmd, sizeof(line) - 1);
        if (n > sizeof(line) - 2) n = sizeof(line) - 2;
        line[n++] = '\r';
        line[n] = '\0';
        kind = k;
        arg = a;
        rxLen = 0;
        rx[0] = '\0';
        // The adapter reads a byte stream: longer commands go out in pieces of 20 bytes
        // (smallest BLE packet), the command ends with '\r'
        for (size_t o = 0; o < n; o += OBD_WRITE_CHUNK) {
            size_t len = n - o < OBD_WRITE_CHUNK ? n - o : OBD_WRITE_CHUNK;
            if (!link.writeAuto((const uint8_t*)line + o, len)) {
                devlogf(id, "Write failed: %s - reconnecting", cmd);
                busy = false;
                link.disconnect();
                return;
            }
        }
        busy = true;
        sentAt = millis();
        timeoutMs = timeout;
    }

    // Mode 01 / 03 request; the first one after ATSP0 waits for the protocol search
    void sendRequest(const char* cmd, Kind k, uint8_t a) {
        send(cmd, k, a, firstRequest ? OBD_SEARCH_TIMEOUT_MS : OBD_CMD_TIMEOUT_MS);
    }

    void receive(const uint8_t* d, size_t n) {
        bool done = false;
        for (size_t i = 0; i < n; i++) {
            char ch = (char)d[i];
            if (ch == '\0') continue;
            if (ch == '>') { done = true; break; }
            if (rxLen < sizeof(rx) - 1) rx[rxLen++] = ch;
        }
        rx[rxLen] = '\0';
        if (done && busy) {
            busy = false;
            timeouts = 0;
            handle();
        }
    }

    void onTimeout() {
        busy = false;
        timeouts++;
        devlogf(id, "No answer to %s (%u)", cur, (unsigned)timeouts);
        if (timeouts >= 3) {
            devlogf(id, "Adapter does not answer - reconnecting");
            timeouts = 0;
            link.disconnect();
            return;
        }
        if (phase == P_PROBE || phase == P_POLL) {
            // Treat like NO DATA: the vehicle side did not answer
            if (kind == K_SUPPORT || kind == K_PID) vehicleSilent();
        }
    }

    // Engine running: from the engine speed if the vehicle reports it, else from the voltage
    void evalRunning(unsigned long now) {
        size_t rpmIdx = valueIndex(0x0C);
        bool rpmFresh = ecuOk && rpmIdx < OBD_VALUE_COUNT && !isnan(vals[rpmIdx]) && pidSupported(0x0C);
        // With detection by engine speed the voltage never counts (smart alternators stay below 13 V)
        bool r = rpmFresh ? vals[rpmIdx] > OBD_RUN_RPM : (c.detect == 0 && !isnan(voltage) && voltage >= c.run_v);
        if (r) lastRunning = now;
        if (r != running) {
            running = r;
            updates++;
            devlogf(id, "Engine %s", running ? "running" : "off");
        }
    }

    static size_t valueIndex(uint8_t pid) {
        for (size_t i = 0; i < OBD_VALUE_COUNT; i++) if (OBD_VALUES[i].pid == pid) return i;
        return OBD_VALUE_COUNT;
    }

    bool wantProbe(unsigned long now) const {
        bool byEcu = c.mode == 1 || c.detect == 1;   // ask the control unit itself every OBD_PROBE_MS
        if (lastProbe && now - lastProbe < (byEcu ? OBD_PROBE_MS : 10000UL)) return false;
        if (byEcu) return true;
        return !isnan(voltage) && voltage >= c.run_v;
    }

    void toIdle(unsigned long now) {
        // A failed probe of the engine speed detection does not restart the time until the
        // rest, otherwise the probe every minute would keep the adapter awake for good
        if (phase != P_PROBE || ecuOk || c.mode == 1) idleSince = now;
        phase = P_IDLE;
        lastProbe = now;
        if (ecuOk) {
            ecuOk = false;
            updates++;
        }
    }

    void enterRest(unsigned long now) {
        devlogf(id, "Engine off - pausing the connection for %ld min", (long)c.rest_min);
        resting = true;
        restUntil = now + (unsigned long)c.rest_min * 60000UL;
        link.setTarget("", BLE_NO_PIN);
        linkDown();
        lastProbe = 0;
    }

    // Too many requests without an answer: ignition off or wrong protocol
    void vehicleSilent() {
        if (++noData < 3) return;
        noData = 0;
        devlogf(id, "Vehicle does not answer (ignition off?)");
        toIdle(millis());
    }

    void next(unsigned long now) {
        static const char* const INIT[] = {"ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATAT1", "ATSP0", "STI"};
        static const uint8_t INIT_COUNT = sizeof(INIT) / sizeof(INIT[0]);

        if (phase != P_POLL) routeSet = false;
        if (routePost && !routeSet && phase != P_DOWN) {
            // Back to the request to all control units before anything else is sent,
            // also if polling stopped right after a routed request
            const char* post = routePost;
            routePost = nullptr;
            send(post, K_ROUTE, 0, OBD_CMD_TIMEOUT_MS);
            return;
        }

        switch (phase) {
        case P_DOWN:
            return;

        case P_INIT:
            if (initStep < INIT_COUNT) {
                const char* cmd = INIT[initStep];
                if (!strcmp(cmd, "ATSP0")) cmd = profile().protocol;
                send(cmd, K_INIT, initStep, initStep == 0 ? OBD_RESET_TIMEOUT_MS : OBD_CMD_TIMEOUT_MS);
                initStep++;
            } else {
                phase = P_IDLE;
                idleSince = now;
                lastVolt = 0;
                updates++;
            }
            return;

        case P_IDLE:
            if (rawPending) { sendRaw(); return; }
            if (!lastVolt || now - lastVolt >= OBD_VOLT_IDLE_MS) {
                lastVolt = now;
                send("ATRV", K_VOLT, 0, OBD_CMD_TIMEOUT_MS);
                return;
            }
            evalRunning(now);
            if (wantProbe(now) || dtcRequested) {
                phase = P_PROBE;
                probeBase = 0;
                probeDone = false;
                noData = 0;
                updates++;
                return;
            }
            if (c.rest_min > 0 && now - idleSince >= OBD_IDLE_MS && !running) enterRest(now);
            return;

        case P_PROBE:
            if (!probeDone) {
                char cmd[8];
                snprintf(cmd, sizeof(cmd), "01%02X", probeBase);
                sendRequest(cmd, K_SUPPORT, probeBase);
                return;
            }
            if (!protocol) {
                send("ATDPN", K_PROTO, 0, OBD_CMD_TIMEOUT_MS);
                return;
            }
            if (!dtcKnown || dtcRequested) {
                dtcRequested = false;
                sendRequest("03", K_DTC, 0);
                return;
            }
            phase = P_POLL;
            pollIdx = 0;
            roundVoltage = false;
            roundStart = now;
            lastRunning = now;
            updates++;
            return;

        case P_POLL:
            if (!routeSet) {
                if (rawPending) { sendRaw(); return; }
                if (dtcRequested) {
                    dtcRequested = false;
                    sendRequest("03", K_DTC, 0);
                    return;
                }
                while (pollIdx < OBD_VALUE_COUNT && !pollWanted(pollIdx)) pollIdx++;
            }
            if (pollIdx < OBD_VALUE_COUNT) {
                const ObdRoute* r = route(OBD_VALUES[pollIdx].pid);
                if (r && !routeSet) {
                    routeSet = true;
                    routePost = r->post;   // sent right after the request (see above)
                    send(r->pre, K_ROUTE, 0, OBD_CMD_TIMEOUT_MS);
                    return;
                }
                routeSet = false;
                char cmd[8];
                snprintf(cmd, sizeof(cmd), "01%02X", OBD_VALUES[pollIdx].pid);
                sendRequest(cmd, K_PID, (uint8_t)pollIdx);
                pollIdx++;
                return;
            }
            if (!roundVoltage) {
                roundVoltage = true;
                lastVolt = now;
                send("ATRV", K_VOLT, 0, OBD_CMD_TIMEOUT_MS);
                return;
            }
            // Round complete
            evalRunning(now);
            if (c.mode == 0 && now - lastRunning >= OBD_STOP_MS) {
                devlogf(id, "Engine off for %lu s - stop polling", (unsigned long)(OBD_STOP_MS / 1000));
                toIdle(now);
                return;
            }
            if (now - roundStart >= (unsigned long)c.poll_s * 1000UL) {
                pollIdx = 0;
                roundVoltage = false;
                roundStart = now;
            }
            return;
        }
    }

    void sendRaw() {
        rawPending = false;
        send(rawCmd, K_RAW, 0, OBD_SEARCH_TIMEOUT_MS);
    }

    // Copy of the answer without the empty lines, lines joined with " | "
    static void compact(const char* in, char* out, size_t size) {
        size_t o = 0;
        bool sep = false;
        for (const char* p = in; *p && o + 4 < size; p++) {
            if (*p == '\r' || *p == '\n') { sep = o > 0; continue; }
            if (sep) {
                out[o++] = ' '; out[o++] = '|'; out[o++] = ' ';
                sep = false;
            }
            out[o++] = (*p >= 32 && *p < 127) ? *p : '.';
        }
        out[o] = '\0';
    }

    void handle() {
        unsigned long now = millis();
        switch (kind) {
        case K_INIT:
            if (arg == 0 || !strcmp(cur, "STI")) {
                // ATZ: "ELM327 v1.5", STI: "STN2120 v5.6.19" ("?" on plain ELM327 adapters)
                char t[64];
                compact(rx, t, sizeof(t));
                // ATZ still runs with echo on: "ATZ | ELM327 v1.5"
                const char* name = t;
                size_t el = strlen(cur);
                if (!strncmp(name, cur, el) && !strncmp(name + el, " | ", 3)) name += el + 3;
                if (name[0] && !strchr(name, '?') &&
                    (arg == 0 || strncmp(name, "STN", 3) == 0 || strncmp(name, "vLink", 5) == 0)) {
                    strlcpy(adapter, name, sizeof(adapter));
                    devlogf(id, "Adapter: %s", adapter);
                    updates++;
                }
            }
            return;

        case K_VOLT: {
            float v = elmVoltage(rx);
            if (!isnan(v)) voltage = v;
            evalRunning(now);
            return;
        }

        case K_SUPPORT: {
            uint32_t m = 0;
            if (elmSupported(rx, arg, m)) {
                firstRequest = false;
                noData = 0;
                ecuOk = true;
                lastEcu = now;
                uint32_t old = mask[arg / 32];
                mask[arg / 32] = m;
                bool more = (m & 1) && arg < 0xA0;   // bit 0 = the next range is supported
                if (more) {
                    probeBase = arg + 0x20;
                } else {
                    for (size_t w = arg / 32 + 1; w < OBD_MASK_WORDS; w++) mask[w] = 0;
                    probeDone = true;
                    finishMask();
                }
                if (old != m) devlogf(id, "PIDs %02X: %08lX", arg, (unsigned long)m);
                return;
            }
            ElmStatus st = elmStatus(rx);
            char t[64];
            compact(rx, t, sizeof(t));
            if (arg == 0) {
                devlogf(id, "Vehicle not reachable (%s): %s", st == ELM_NO_DATA ? "no data" : "no bus",
                        t[0] ? t : "(empty)");
                if (dtcRequested) {
                    dtcRequested = false;
                    devlogf(id, "Trouble codes: vehicle not reachable - switch the ignition on");
                }
                toIdle(now);
            } else {
                // Later ranges may be missing even if announced: keep what we have
                devlogf(id, "PIDs %02X: no answer (%s)", arg, t);
                probeDone = true;
                finishMask();
            }
            return;
        }

        case K_PROTO:
            protocol = elmProtocol(rx);
            devlogf(id, "Protocol: %s", elmProtocolName(protocol));
            if (!protocol) protocol = '0';   // unknown, do not ask again in this connection
            updates++;
            return;

        case K_DTC: {
            char codes[OBD_DTC_MAX][6];
            size_t total = 0;
            size_t n = obdParseDtcs(rx, elmProtocolIsCan(protocol), codes, OBD_DTC_MAX, &total);
            ElmStatus st = elmStatus(rx);
            bool any = n > 0 || st == ELM_NO_DATA || strstr(rx, "43") != nullptr;
            if (!any) {
                char t[64];
                compact(rx, t, sizeof(t));
                devlogf(id, "Trouble codes: no answer (%s)", t);
                dtcKnown = true;   // do not ask again until requested
                return;
            }
            dtcText[0] = '\0';
            for (size_t i = 0; i < n; i++) {
                if (i) strlcat(dtcText, ", ", sizeof(dtcText));
                strlcat(dtcText, codes[i], sizeof(dtcText));
            }
            if (total > n) {
                char more[16];
                snprintf(more, sizeof(more), " (+%u)", (unsigned)(total - n));
                strlcat(dtcText, more, sizeof(dtcText));
            }
            if (!n) strlcpy(dtcText, "keine", sizeof(dtcText));
            dtcKnown = true;
            updates++;
            devlogf(id, "Trouble codes: %s", dtcText);
            return;
        }

        case K_PID: {
            const ObdValue& v = OBD_VALUES[arg];
            uint8_t d[16];
            size_t n = 0;
            float f;
            bool lamp = false;
            bool got = elmMode01(rx, v.pid, d, sizeof(d), n);
            if (got) {
                // Further values from the same answer (PID 9B: AdBlue temperature and concentration)
                for (size_t k = arg + 1; k < OBD_VALUE_COUNT; k++) {
                    float g;
                    if (OBD_VALUES[k].pid == v.pid && obdDecode(OBD_VALUES[k].formula, d, n, g)) vals[k] = g;
                }
            }
            if (got && obdDecode(v.formula, d, n, f, &lamp)) {
                firstRequest = false;
                noData = 0;
                lastEcu = now;
                if (!ecuOk) {
                    ecuOk = true;
                    updates++;
                }
                vals[arg] = f;
                if (v.formula == OBD_F_DTC_COUNT) {
                    if (lamp != mil) updates++;
                    mil = lamp;
                    // Number of codes changed since the last read: read them again
                    int count = (int)f;
                    if (dtcCount >= 0 && count != dtcCount) dtcRequested = true;
                    dtcCount = count;
                }
                return;
            }
            if (got) return;   // answer came, value not valid (e.g. oil temperature FF of a cold engine)
            ElmStatus st = elmStatus(rx);
            // Routed values (other control unit) without answer do not mean "vehicle silent":
            // that control unit may sleep or lack the value while the engine still answers
            if ((st == ELM_NO_DATA || st == ELM_NO_CONNECT) && !route(v.pid)) vehicleSilent();
            return;
        }

        case K_ROUTE:
            if (elmStatus(rx) != ELM_OK) {
                char t[48];
                compact(rx, t, sizeof(t));
                devlogf(id, "%s: %s", cur, t[0] ? t : "(empty)");
            }
            return;

        case K_RAW:
            compact(rx, rawResp, sizeof(rawResp));
            devlogf(id, "Test %s -> %s", cur, rawResp[0] ? rawResp : "(empty)");
            updates++;
            return;
        }
    }

    // Probe done: store the supported PIDs and update Home Assistant if they changed
    void finishMask() {
        bool changed = !maskKnown;
        uint32_t saved[OBD_MASK_WORDS] = {0};
        Preferences p;
        if (p.begin("obd", true)) {
            if (p.getBytes(id, saved, sizeof(saved)) != sizeof(saved)) changed = true;
            p.end();
        }
        if (memcmp(saved, mask, sizeof(mask)) != 0) changed = true;
        maskKnown = true;
        if (changed) {
            saveMask();
            devlogf(id, "Supported values changed - updating Home Assistant");
            mqttRequestDiscovery();
        }
    }
};

Plugin* createObd(uint8_t index) {
    return new ObdPlugin(index);
}

#endif // PLUGIN_OBD
