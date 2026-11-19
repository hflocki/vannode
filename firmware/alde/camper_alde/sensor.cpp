// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Sensor part of the Alde node, two independent buses (each one optional):
//
// 1. Heater bus (panel <-> heater, 9600 8N2, config.lin_enabled): listened to passively. That UART is opened
//    WITHOUT a TX pin, so the firmware cannot write there even by mistake. The Alde panel
//    is the master of that bus. Decoding: alde_lin.cpp (based on alde-3020he-lin, MIT).
// 2. External panel bus ("CI-bus", 19200 8N1, optional, ci_mode > 0): the ESP is the LIN
//    master and polls the panel's info frame. With ci_mode 2 it also sends control frames,
//    with ci_mode 3 it only listens (TX not used) and collects the frames of other devices
//    (alde_ci_sniff.*), to find out who is LIN master on this bus.
//    always built from the panel's current state. Protocol: alde_ci.cpp (based on
//    alde_3030_plus, MIT).
//
// The ESP32-C3 has only one free UART: it runs either the heater bus or the panel bus.
#include "sensor.h"
#include "alde_lin.h"
#include "evlog.h"
#include "alde_ci_sniff.h"
#include "dbglog.h"
#include <ArduinoJson.h>

// Order = index into sensorData.values[]. Keys listed in BINARY_KEYS (mqtt_ha.cpp)
// become binary sensors in Home Assistant (payload 1/0).
const ValueDef VALUES[] = {
//   key                name                    unit   device_class   icon                          dec  ci
    // ---- heater bus (passive) ----
    {"room",            "Raumtemperatur",       "°C",  "temperature", "mdi:home-thermometer",       1, false},
    {"setpoint",        "Sollwert (Heizbus)",   "°C",  "temperature", "mdi:thermostat",             1, false},
    {"glycol",          "Glykol",               "°C",  "temperature", "mdi:coolant-temperature",    1, false},
    {"water",           "Warmwasser",           "°C",  "temperature", "mdi:water-thermometer",      1, false},
    {"hotwater_target", "Warmwasser Ziel",      "°C",  "temperature", "mdi:water-boiler",           0, false},
    {"pcb",             "Platine",              "°C",  "temperature", "mdi:chip",                   1, false},
    {"battery",         "Batterie",             "V",   "voltage",     "mdi:car-battery",            2, false},
    {"mains_current",   "Landstrom",            "A",   "current",     "mdi:power-plug",             2, false},
    {"el_power",        "Heizstab",             "W",   "power",       "mdi:flash",                  0, false},
    {"el_allowed",      "Heizstab erlaubt",     "W",   "power",       "mdi:flash-outline",          0, false},
    {"burner_fan",      "Brennergebläse",       "rpm", nullptr,       "mdi:fan",                    0, false},
    {"gas",             "Gas",                  "",    nullptr,       "mdi:fire",                   0, false},
    {"ac_active",       "Klimaanlage aktiv",    "",    nullptr,       "mdi:snowflake",              0, false},
    {"ac_light",        "Klimaanlage Licht",    "%",   nullptr,       "mdi:lightbulb",              0, false},
    {"bus_ok",          "LIN-Bus",              "",    nullptr,       "mdi:lan-connect",            0, false},
    // ---- external panel bus (ci_mode > 0) ----
    {"target",          "Sollwert",             "°C",  "temperature", "mdi:thermostat",             1, true},
    {"panel_room",      "Raum (Bedienteil)",    "°C",  "temperature", "mdi:home-thermometer-outline", 1, true},
    {"outdoor",         "Außentemperatur",      "°C",  "temperature", "mdi:thermometer",            1, true},
    {"gas_enabled",     "Gasbetrieb",           "",    nullptr,       "mdi:fire-circle",            0, true},
    {"el_level",        "Heizstab Stufe",       "kW",  nullptr,       "mdi:flash",                  0, true},
    {"water_mode",      "Warmwasser Modus",     "",    nullptr,       "mdi:water-boiler",           0, true},
    {"pump",            "Umwälzpumpe",          "",    nullptr,       "mdi:pump",                   0, true},
    {"panel_on",        "Heizung an",           "",    nullptr,       "mdi:power",                  0, true},
    {"error",           "Störung",              "",    nullptr,       "mdi:alert-circle",           0, true},
    {"ac_input",        "230 V",                "",    nullptr,       "mdi:power-plug-outline",     0, true},
    {"ci_ok",           "Bedienteil-Bus",       "",    nullptr,       "mdi:lan-connect",            0, true},
};
const uint8_t VALUE_COUNT = sizeof(VALUES) / sizeof(VALUES[0]);
static_assert(sizeof(VALUES) / sizeof(VALUES[0]) <= MAX_VALUES, "Too many values, raise MAX_VALUES");

// Classic ESP32 / S3: one UART per bus. ESP32-C3: only UART1 is free, so the panel bus
// can only run while the heater bus is switched off.
#if SOC_UART_NUM > 2
static HardwareSerial& linSerial = Serial2;
static HardwareSerial& ciSerial = Serial1;
#define CI_SHARED_UART 0
#else
static HardwareSerial& linSerial = Serial1;
static HardwareSerial& ciSerial = Serial1;
#define CI_SHARED_UART 1
#endif

static bool linActive = false;            // heater bus opened at boot (config.lin_enabled)

static AldeLin lin;
static unsigned long lastPublish = 0;
static volatile bool publishRequested = true;
static bool sigLog = false;

// ---- external panel bus state ----
static const unsigned long CI_TIMEOUT_MS = 5000;   // no valid answer for 5 s = connection lost
static AldeCiState ciState = {};
static unsigned long ciLastOk = 0;
static unsigned long ciNextPoll = 0;
static unsigned long ciLastRegister = 0;
static uint8_t ciFails = 0;
static uint8_t ciFastPolls = 0;
static uint8_t ciActiveMode = 0;          // mode the UART was set up for
static bool ciChanged = false;
static uint32_t ciPollsOk = 0, ciPollsBad = 0, ciCmdsSent = 0;
static portMUX_TYPE cmdMux = portMUX_INITIALIZER_UNLOCKED;
static AldeCiCommand pendingCmd;          // written by the web / MQTT task
static volatile bool cmdPending = false;
static AldeCiCommand activeCmd;           // command being worked on in ciLoop()
static AldeSniffer sniffer;               // ci_mode 3: passive listener
static volatile bool sniffResetReq = false;
static volatile bool dumpReq = false;
static uint8_t cmdTries = 0;
static bool cmdRegistered = false;

// Detailed log (signal changes, frames): serial command "sig" or the debug setting
static bool logDetail() {
    return sigLog || config.debug;
}

static void hexLine(char* out, size_t size, const uint8_t* d, int n) {
    int p = 0;
    out[0] = 0;
    for (int k = 0; k < n && p + 4 < (int)size; k++) p += snprintf(out + p, size - p, "%02X ", d[k]);
    if (p > 0) out[p - 1] = 0;
}

static void logSignal(uint8_t pid, const uint8_t* d, int n) {
    if (!logDetail()) return;
    char hex[3 * 8 + 1];
    int p = 0;
    for (int k = 0; k < n && k < 8; k++) p += snprintf(hex + p, sizeof(hex) - p, "%02X ", d[k]);
    if (p > 0) hex[p - 1] = 0; else hex[0] = 0;
    dbglogf("[SIG] %02X (len=%d): %s\n", pid, n, hex);
}

bool sensorCiOk() {
    return ciActiveMode > 0 && ciActiveMode != 3 && ciLastOk != 0 && millis() - ciLastOk < CI_TIMEOUT_MS;
}

const AldeSniffer& sensorSniffer() {
    return sniffer;
}

bool sensorSniffActive() {
    return ciActiveMode == 3;
}

void sensorSniffReset() {
    sniffResetReq = true;                 // done in the loop task, which owns the sniffer
}

static void sniffNewId(uint8_t id) {
    evlog::add("Mithören: ID %02X zum ersten Mal", id);
    dbglogf("[CI-SNIFF] New id 0x%02X (PID 0x%02X)\n", id, aldeLinPid(id));
}

static void sniffFrame(uint8_t id, const uint8_t* d, int n, bool ok) {
    if (!logDetail()) return;
    char hex[3 * 12 + 1];
    int p = 0;
    for (int k = 0; k < n && k < 12; k++) p += snprintf(hex + p, sizeof(hex) - p, "%02X ", d[k]);
    if (p > 0) hex[p - 1] = 0; else hex[0] = 0;
    dbglogf("[CI-SNIFF] %02X %s: %s\n", id, ok ? "ok" : (n ? "bad" : "no answer"), hex);
}

bool sensorLinActive() {
    return linActive;
}

bool sensorCiPossible() {
    return !CI_SHARED_UART || !linActive;
}

// ============================================================
// External panel bus (LIN master, 19200 8N1)
// ============================================================
static void ciDrain() {
    while (ciSerial.available()) ciSerial.read();
}

// LIN break: one 0x00 at 1200 baud keeps the bus dominant for ~7.5 ms (> 13 bits at 19200)
static void ciBreak() {
    ciSerial.flush();
    ciSerial.updateBaudRate(1200);
    ciSerial.write((uint8_t)0x00);
    ciSerial.flush();
    delay(2);
    ciSerial.updateBaudRate(CI_BAUD);
}

// Read until max bytes or no byte for 20 ms (first byte within timeoutMs)
static int ciRead(uint8_t* buf, int max, unsigned long timeoutMs) {
    int n = 0;
    unsigned long deadline = millis() + timeoutMs;
    while (n < max && (long)(deadline - millis()) > 0) {
        if (ciSerial.available()) {
            buf[n++] = (uint8_t)ciSerial.read();
            deadline = millis() + 20;
        } else {
            delay(1);
        }
    }
    return n;
}

static void ciSendFrame(uint8_t id, const uint8_t* d) {
    ciDrain();
    ciBreak();
    uint8_t frame[11] = {0x55, id};
    memcpy(frame + 2, d, 8);
    frame[10] = aldeCiChecksum(id, d, 8);
    ciSerial.write(frame, sizeof(frame));
    ciSerial.flush();
    if (logDetail()) {
        char hex[3 * 11 + 1];
        hexLine(hex, sizeof(hex), frame, sizeof(frame));
        dbglogf("[CI] TX %s", hex);
    }
    uint8_t echo[16];
    ciRead(echo, sizeof(echo), 10);   // single-wire bus: we hear ourselves, throw it away
}

static void ciRegister() {
    uint8_t d[8];
    aldeCiDiagPayload(config.ci_function, d);
    ciSendFrame(ALDE_CI_ID_DIAG, d);
    ciLastRegister = millis();
    dbglogf("[CI] Registration sent (function id 0x%04X)\n", config.ci_function);
}

// One info poll. Returns true and updates ciState on a valid answer.
static bool ciPoll() {
    ciDrain();
    ciBreak();
    const uint8_t hdr[2] = {0x55, ALDE_CI_ID_INFO};
    ciSerial.write(hdr, 2);
    ciSerial.flush();
    uint8_t buf[24];
    int n = ciRead(buf, sizeof(buf), 60);
    AldeCiState s;
    if (!aldeCiParseInfo(buf, n, &s)) {
        ciPollsBad++;
        if (logDetail()) {
            char hex[3 * 24 + 1];
            hexLine(hex, sizeof(hex), buf, n);
            dbglogf("[CI] Poll 0x1B: no valid answer (%d bytes) %s", n, hex);
        }
        return false;
    }
    ciPollsOk++;
    // Only changes, otherwise one line per second would flood the log
    if (logDetail() && (!ciState.valid || memcmp(ciState.raw, s.raw, 8) != 0)) {
        char hex[3 * 8 + 1];
        hexLine(hex, sizeof(hex), s.raw, 8);
        dbglogf("[CI] Info 0x1B changed: %s", hex);
    }
    if (!ciState.valid || memcmp(ciState.raw, s.raw, 8) != 0) ciChanged = true;
    ciState = s;
    return true;
}

static void ciStart() {
#if CI_SLP_PIN >= 0
    pinMode(CI_SLP_PIN, OUTPUT);
    digitalWrite(CI_SLP_PIN, HIGH);
#endif
    ciActiveMode = config.ci_mode;
    ciLastOk = 0;
    ciFails = 0;
    if (ciActiveMode == 3) {
        // Listen only: TX pin not attached to the UART at all, held high (= recessive) so the
        // transceiver can never pull the bus dominant
        ciSerial.setRxBufferSize(1024);
        ciSerial.begin(CI_BAUD, SERIAL_8N1, CI_RX_PIN, -1);
        pinMode(CI_TX_PIN, OUTPUT);
        digitalWrite(CI_TX_PIN, HIGH);
        sniffer.onNewId = sniffNewId;
        sniffer.onFrame = sniffFrame;
        aldeSnifferReset(sniffer);
        evlog::add("Bedienteil-Bus an (nur mithören)");
        dbglogf("[CI] Listening only on RX GPIO%d (TX unused)\n", CI_RX_PIN);
        return;
    }
    ciSerial.begin(CI_BAUD, SERIAL_8N1, CI_RX_PIN, CI_TX_PIN);
    ciNextPoll = millis() + 200;
    ciRegister();
    evlog::add("Bedienteil-Bus an (%s)", config.ci_mode == 2 ? "lesen + steuern" : "nur lesen");
}

// ci_mode 3: collect what others send
static void ciSniffLoop(unsigned long now) {
    if (sniffResetReq) {
        sniffResetReq = false;
        aldeSnifferReset(sniffer);
        while (ciSerial.available()) ciSerial.read();
    }
    uint8_t chunk[64];
    int avail;
    while ((avail = ciSerial.available()) > 0) {
        int n = ciSerial.readBytes(chunk, avail < (int)sizeof(chunk) ? avail : (int)sizeof(chunk));
        if (n <= 0) break;
        aldeSnifferFeed(sniffer, chunk, n, now);
    }
    aldeSnifferTick(sniffer, now);
}

static void ciStop() {
    ciSerial.end();
    ciActiveMode = 0;
    ciState.valid = false;
    ciLastOk = 0;
    ciChanged = true;
    aldeCiCommandClear(activeCmd);
    evlog::add("Bedienteil-Bus aus");
}

static void ciLoop(unsigned long now) {
    if (!sensorCiPossible()) return;      // C3 with heater bus: the only UART is taken
    // Follow changes of the setting at runtime: 0 <-> on and listen-only <-> master restart
    // the UART, 1 <-> 2 does not
    if ((config.ci_mode > 0) != (ciActiveMode > 0) ||
        (config.ci_mode == 3) != (ciActiveMode == 3)) {
        if (ciActiveMode > 0) ciStop();
        if (config.ci_mode > 0) ciStart();
    }
    if (ciActiveMode == 0) return;
    ciActiveMode = config.ci_mode;
    if (ciActiveMode == 3) {
        aldeCiCommandClear(activeCmd);
        ciSniffLoop(now);
        return;
    }

    // A new command from the web / MQTT task: merge it into the one being worked on
    if (cmdPending) {
        portENTER_CRITICAL(&cmdMux);
        if (pendingCmd.hasTarget) { activeCmd.hasTarget = true; activeCmd.target = pendingCmd.target; }
        if (pendingCmd.gas >= 0) activeCmd.gas = pendingCmd.gas;
        if (pendingCmd.el_level >= 0) activeCmd.el_level = pendingCmd.el_level;
        if (pendingCmd.water >= 0) activeCmd.water = pendingCmd.water;
        if (pendingCmd.power >= 0) activeCmd.power = pendingCmd.power;
        aldeCiCommandClear(pendingCmd);
        cmdPending = false;
        portEXIT_CRITICAL(&cmdMux);
        cmdTries = 0;
        cmdRegistered = false;
        ciNextPoll = now;
    }

    if (!aldeCiCommandEmpty(activeCmd)) {
        if (config.ci_mode != 2) {
            aldeCiCommandClear(activeCmd);
            evlog::add("Befehl verworfen: Steuerung aus");
            return;
        }
        if ((long)(now - ciNextPoll) < 0) return;
        // Register right before the command, so a panel that restarted knows us again
        if (!cmdRegistered) {
            ciRegister();
            delay(50);
            cmdRegistered = true;
        }
        // Read the current state; never send while the panel is busy or reports an error
        bool ok = ciPoll();
        if (ok) { ciLastOk = millis(); ciFails = 0; }
        if (!ok || aldeCiBlocked(ciState)) {
            if (++cmdTries >= CI_CMD_TRIES) {
                aldeCiCommandClear(activeCmd);
                const char* why = !ok ? "Bedienteil antwortet nicht" : ciState.error ? "Bedienteil meldet Störung" : "Bedienteil beschäftigt";
                evlog::add("Befehl verworfen: %s", why);
                dbglogf("[CI] Command dropped: %s\n", why);
            } else {
                ciNextPoll = millis() + 300;
            }
            return;
        }
        AldeCiCommand cmd = activeCmd;
        aldeCiCommandClear(activeCmd);
        // Only send what differs from the panel's state
        if (!aldeCiCommandReduce(ciState, cmd)) {
            evlog::add("Befehl: Wert schon eingestellt");
            dbglogf("[CI] Command skipped: panel already reports the value");
            ciNextPoll = millis() + CI_POLL_MS;
            return;
        }
        uint8_t d[8];
        aldeCiBuildControl(ciState, cmd, d);
        ciSendFrame(ALDE_CI_ID_CONTROL, d);
        ciCmdsSent++;
        evlog::add("Befehl an Bedienteil: %02X %02X %02X", d[3], d[4], d[5]);
        dbglogf("[CI] Control sent: 00 00 00 %02X %02X %02X FF FF\n", d[3], d[4], d[5]);
        ciFastPolls = 5;                  // confirm quickly
        ciNextPoll = millis() + 300;
        lastPublish = 0;                  // publish the confirmation right away
        return;
    }

    if ((long)(now - ciNextPoll) < 0) return;
    bool ok = ciPoll();
    now = millis();
    if (ok) {
        ciLastOk = now;
        ciFails = 0;
    } else if (++ciFails >= 5 && now - ciLastRegister > 10000) {
        ciRegister();   // panel restarted or lost us: register again
    }
    if (ciFastPolls > 0) {
        ciFastPolls--;
        ciNextPoll = now + 300;
    } else {
        ciNextPoll = now + CI_POLL_MS;
    }
}
const char* sensorControl(const AldeCiCommand& cmd) {
    static char msg[48];
    if (!sensorCiPossible()) return "ESP32-C3: Steuerung nur mit ausgeschaltetem Heizbus";
    if (config.ci_mode != 2) return "Steuerung ist aus (Einstellungen → Bedienteil-Bus)";
    if (!sensorCiOk()) return "Keine Verbindung zum Bedienteil";
    if (aldeCiCommandEmpty(cmd)) return "Kein Befehl";
    if (cmd.hasTarget && !(cmd.target >= 5.0f && cmd.target <= 30.0f)) return "Sollwert 5-30 °C";
    if (cmd.gas > 1) return "Gas: 0 oder 1";
    if (cmd.el_level > 3) return "Heizstab: 0-3 kW";
    if (cmd.el_level > (int8_t)config.el_max) {
        snprintf(msg, sizeof(msg), "Heizstab höchstens %u kW (Einstellungen)", (unsigned)config.el_max);
        return msg;
    }
    if (cmd.water > 2) return "Warmwasser: 0 = aus, 1 = an, 2 = Boost";
    if (cmd.power > 1) return "Heizung: 0 = aus, 1 = an";
    portENTER_CRITICAL(&cmdMux);
    if (cmd.hasTarget) { pendingCmd.hasTarget = true; pendingCmd.target = cmd.target; }
    if (cmd.gas >= 0) pendingCmd.gas = cmd.gas;
    if (cmd.el_level >= 0) pendingCmd.el_level = cmd.el_level;
    if (cmd.water >= 0) pendingCmd.water = cmd.water;
    if (cmd.power >= 0) pendingCmd.power = cmd.power;
    cmdPending = true;
    portEXIT_CRITICAL(&cmdMux);
    return nullptr;
}

// ============================================================
// Setup / loop
// ============================================================
void sensorInit() {
    for (uint8_t i = 0; i < MAX_VALUES; i++) sensorData.values[i] = NAN;
    aldeCiCommandClear(pendingCmd);
    aldeCiCommandClear(activeCmd);

    linActive = config.lin_enabled;
    if (!linActive) {
        dbglogf("[LIN] Heater bus switched off (only the panel bus transceiver fitted)");
    } else {
#if LIN_SLP_PIN >= 0
        // TJA1020 and similar: SLP/NSLP high = normal mode (receiver on). TX stays unused.
        pinMode(LIN_SLP_PIN, OUTPUT);
        digitalWrite(LIN_SLP_PIN, HIGH);
#endif
        linSerial.setRxBufferSize(1024);
        // 9600 baud 8N2, RX only (TX pin -1): receive-only by construction
        linSerial.begin(LIN_BAUD, SERIAL_8N2, LIN_RX_PIN, -1);
        lin.onSignalChange = logSignal;
        dbglogf("[LIN] Alde reader on GPIO%d (RX only, %d baud 8N2)\n", LIN_RX_PIN, LIN_BAUD);
    }
    if (sensorCiPossible()) {
        dbglogf("[CI] External panel bus on RX GPIO%d / TX GPIO%d, mode %u\n", CI_RX_PIN, CI_TX_PIN, config.ci_mode);
    } else {
        dbglogf("[CI] External panel bus not available: the C3's only UART runs the heater bus");
    }
}

static void copyReadings(unsigned long now) {
    const AldeReadings& r = lin.readings();
    bool ci = sensorCiOk() && ciState.valid;
    const float N = NAN;
    const bool h = linActive;   // heater bus switched off: its values stay unknown
    const AldeCiState& c = ciState;
    const float src[] = {
        h ? r.room : N, h ? r.setpoint : N, h ? r.glycol : N, h ? r.water : N,
        h ? r.hotwater_target : N, h ? r.pcb : N, h ? r.battery : N, h ? r.mains_current : N,
        h ? r.el_power : N, h ? r.el_allowed : N, h ? r.burner_fan : N, h ? r.gas : N,
        h ? r.ac_active : N, h ? r.ac_light : N,
        h ? (lin.busOk(now) ? 1.0f : 0.0f) : N,
        ci ? c.setpoint : N, ci ? c.room : N, ci ? c.outdoor : N,
        ci ? (float)c.gas : N, ci ? (float)c.el_level : N,
        (ci && c.water <= 2) ? (float)c.water : N,   // mode 3 is unknown, never published
        ci ? (float)c.pump : N, ci ? (float)c.panel_on : N, ci ? (float)c.error : N,
        ci ? (float)c.ac_input : N,
        (ciActiveMode > 0 && ciActiveMode != 3) ? (sensorCiOk() ? 1.0f : 0.0f) : N,
    };
    static_assert(sizeof(src) / sizeof(src[0]) == sizeof(VALUES) / sizeof(VALUES[0]), "VALUES[] and src[] differ");
    for (uint8_t i = 0; i < VALUE_COUNT; i++) sensorData.values[i] = src[i];
}

void sensorLoop() {
    unsigned long now = millis();

    if (linActive) {
        uint8_t chunk[128];
        int avail;
        while ((avail = linSerial.available()) > 0) {
            int n = linSerial.readBytes(chunk, avail < (int)sizeof(chunk) ? avail : (int)sizeof(chunk));
            if (n <= 0) break;
            lin.feed(chunk, n, now);
        }
        lin.tick(now);
    }

    if (dumpReq) {
        dumpReq = false;
        sensorPrint();
    }

    static bool lastCiOk = false;
    ciLoop(now);
    now = millis();
    bool ciOk = sensorCiOk();
    if (ciOk != lastCiOk) { lastCiOk = ciOk; ciChanged = true; }

    // "Sensor OK" = heater bus runs; without heater bus the panel bus counts
    bool busOk = linActive ? lin.busOk(now) : ciOk;
    bool busChanged = busOk != sensorData.ok;
    sensorData.ok = busOk;
    if (busOk) sensorData.last_ok = linActive ? lin.stats().last_frame_ms : ciLastOk;
    sensorData.errors = lin.stats().frames_bad;

    // Publish on change, but at most every interval_s; bus up/down right away
    bool changed = lin.takeChanged();
    if (changed || ciChanged) publishRequested = true;
    ciChanged = false;
    if (busChanged || (publishRequested && now - lastPublish >= (unsigned long)config.interval_s * 1000UL)) {
        publishRequested = false;
        lastPublish = now;
        copyReadings(now);
        sensorData.readCount++;   // MQTT publishes after every cycle
    }
}

void sensorRequestMeasure() {
    publishRequested = true;
    lastPublish = 0;
}

void sensorPrint() {
    const AldeBusStats& s = lin.stats();
    if (!linActive) dbglogf("[LIN] heater bus switched off");
    dbglogf("[LIN] ok=%u bad=%u empty=%u bus=%s ac_seen=%lus ago\n", (unsigned)s.frames_ok,
                  (unsigned)s.frames_bad, (unsigned)s.headers_empty, lin.busOk(millis()) ? "OK" : "---",
                  s.last_ac_ms ? (unsigned long)((millis() - s.last_ac_ms) / 1000) : 0UL);
    if (!sensorCiPossible()) dbglogf("[CI] not available (C3: the only UART runs the heater bus)");
    dbglogf("[CI] mode=%u el_max=%u polls ok=%u bad=%u commands=%u link=%s raw=%02X %02X %02X %02X %02X %02X\n",
                  config.ci_mode, config.el_max, (unsigned)ciPollsOk, (unsigned)ciPollsBad, (unsigned)ciCmdsSent,
                  sensorCiOk() ? "OK" : "---", ciState.raw[0], ciState.raw[1], ciState.raw[2],
                  ciState.raw[3], ciState.raw[4], ciState.raw[5]);
    if (ciActiveMode == 3) {
        dbglogf("[CI-SNIFF] bytes=%u frames=%u junk=%u\n", (unsigned)sniffer.bytes,
                      (unsigned)sniffer.frames, (unsigned)sniffer.junk);
        for (uint8_t id = 0; id < 64; id++) {
            const AldeSniffId& e = sniffer.ids[id];
            if (!e.headers) continue;
            dbglogf("[CI-SNIFF] id %02X pid %02X headers=%u answers=%u bad=%u len=%u %s\n", id,
                          aldeLinPid(id), (unsigned)e.headers, (unsigned)e.responses, (unsigned)e.bad,
                          e.len, e.len ? (e.enhanced ? "LIN2" : "LIN1") : "");
        }
    }
    for (uint8_t i = 0; i < VALUE_COUNT; i++) {
        dbglogf("[READ] %-20s %10.*f %s -> %s/%s\n", VALUES[i].name, VALUES[i].decimals,
                      sensorData.values[i], VALUES[i].unit, config.mqtt_prefix, VALUES[i].key);
    }
}

void sensorRequestDump() {
    dumpReq = true;
}

void sensorDebugJson(JsonObject o) {
    unsigned long now = millis();
    const AldeBusStats& s = lin.stats();
    JsonObject h = o["lin"].to<JsonObject>();
    h["active"] = linActive;
    h["ok"] = linActive && lin.busOk(now);
    h["frames_ok"] = s.frames_ok;
    h["frames_bad"] = s.frames_bad;
    h["empty"] = s.headers_empty;
    h["age"] = (linActive && s.last_frame_ms) ? (long)((now - s.last_frame_ms) / 1000) : -1;
    JsonObject c = o["ci"].to<JsonObject>();
    c["mode"] = config.ci_mode;
    c["possible"] = sensorCiPossible();
    c["ok"] = sensorCiOk();
    c["polls_ok"] = ciPollsOk;
    c["polls_bad"] = ciPollsBad;
    c["commands"] = ciCmdsSent;
    c["registered"] = ciLastRegister ? (long)((now - ciLastRegister) / 1000) : -1;
    if (ciState.valid) {
        char hex[3 * 8 + 1];
        hexLine(hex, sizeof(hex), ciState.raw, 8);
        c["raw"] = hex;
        c["busy"] = ciState.busy;
        c["error"] = ciState.error;
    }
    o["detail"] = logDetail();
}

void sensorToggleSigLog() {
    sigLog = !sigLog;
    dbglogf("[SIG] Change log %s\n", sigLog ? "on" : "off");
}
