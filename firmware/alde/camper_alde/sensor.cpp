// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Sensor part of the Alde node, two independent buses:
//
// 1. Heater bus (panel <-> heater, 9600 8N2): listened to passively. That UART is opened
//    WITHOUT a TX pin, so the firmware cannot write there even by mistake. The Alde panel
//    is the master of that bus. Decoding: alde_lin.cpp (based on alde-3020he-lin, MIT).
// 2. External panel bus ("CI-bus", 19200 8N1, optional, ci_mode > 0): the ESP is the LIN
//    master and polls the panel's info frame. With ci_mode 2 it also sends control frames,
//    always built from the panel's current state. Protocol: alde_ci.cpp (based on
//    alde_3030_plus, MIT).
#include "sensor.h"
#include "alde_lin.h"
#include "evlog.h"

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
    {"panel_on",        "Bedienteil an",        "",    nullptr,       "mdi:power",                  0, true},
    {"error",           "Störung",              "",    nullptr,       "mdi:alert-circle",           0, true},
    {"ac_input",        "230 V",                "",    nullptr,       "mdi:power-plug-outline",     0, true},
    {"ci_ok",           "Bedienteil-Bus",       "",    nullptr,       "mdi:lan-connect",            0, true},
};
const uint8_t VALUE_COUNT = sizeof(VALUES) / sizeof(VALUES[0]);
static_assert(sizeof(VALUES) / sizeof(VALUES[0]) <= MAX_VALUES, "Too many values, raise MAX_VALUES");

// UART2 on the classic ESP32 / S3; the C3 only has UART0 and UART1 (no CI bus there)
#if SOC_UART_NUM > 2
static HardwareSerial& linSerial = Serial2;
static HardwareSerial& ciSerial = Serial1;
#define CI_SUPPORTED 1
#else
static HardwareSerial& linSerial = Serial1;
#define CI_SUPPORTED 0
#endif

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
static AldeCiCommand pendingCmd;
static volatile bool cmdPending = false;

static void logSignal(uint8_t pid, const uint8_t* d, int n) {
    if (!sigLog) return;
    char hex[3 * 8 + 1];
    int p = 0;
    for (int k = 0; k < n && k < 8; k++) p += snprintf(hex + p, sizeof(hex) - p, "%02X ", d[k]);
    if (p > 0) hex[p - 1] = 0; else hex[0] = 0;
    Serial.printf("[SIG] %02X (len=%d): %s\n", pid, n, hex);
}

bool sensorCiOk() {
    return ciActiveMode > 0 && ciLastOk != 0 && millis() - ciLastOk < CI_TIMEOUT_MS;
}

// ============================================================
// External panel bus (LIN master, 19200 8N1)
// ============================================================
#if CI_SUPPORTED
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
    uint8_t echo[16];
    ciRead(echo, sizeof(echo), 10);   // single-wire bus: we hear ourselves, throw it away
}

static void ciRegister() {
    uint8_t d[8];
    aldeCiDiagPayload(config.ci_function, d);
    ciSendFrame(ALDE_CI_ID_DIAG, d);
    ciLastRegister = millis();
    Serial.printf("[CI] Registration sent (function id 0x%04X)\n", config.ci_function);
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
        return false;
    }
    ciPollsOk++;
    if (!ciState.valid || memcmp(ciState.raw, s.raw, 8) != 0) ciChanged = true;
    ciState = s;
    return true;
}

static void ciStart() {
#if CI_SLP_PIN >= 0
    pinMode(CI_SLP_PIN, OUTPUT);
    digitalWrite(CI_SLP_PIN, HIGH);
#endif
    ciSerial.begin(CI_BAUD, SERIAL_8N1, CI_RX_PIN, CI_TX_PIN);
    ciActiveMode = config.ci_mode;
    ciLastOk = 0;
    ciFails = 0;
    ciNextPoll = millis() + 200;
    ciRegister();
    evlog::add("Bedienteil-Bus an (%s)", config.ci_mode == 2 ? "lesen + steuern" : "nur lesen");
}

static void ciStop() {
    ciSerial.end();
    ciActiveMode = 0;
    ciState.valid = false;
    ciLastOk = 0;
    ciChanged = true;
    evlog::add("Bedienteil-Bus aus");
}

static void ciLoop(unsigned long now) {
    // Follow changes of the setting at runtime (0 <-> on restarts the UART, 1 <-> 2 does not)
    if ((config.ci_mode > 0) != (ciActiveMode > 0)) {
        if (ciActiveMode > 0) ciStop();
        else ciStart();
    }
    if (ciActiveMode == 0) return;
    ciActiveMode = config.ci_mode;

    // A queued command: read the current state first, then change only what was asked
    if (cmdPending) {
        AldeCiCommand cmd;
        portENTER_CRITICAL(&cmdMux);
        cmd = pendingCmd;
        aldeCiCommandClear(pendingCmd);
        cmdPending = false;
        portEXIT_CRITICAL(&cmdMux);

        if (config.ci_mode == 2 && !aldeCiCommandEmpty(cmd) && ciPoll()) {
            ciLastOk = millis();
            uint8_t d[8];
            aldeCiBuildControl(ciState, cmd, d);
            ciSendFrame(ALDE_CI_ID_CONTROL, d);
            ciCmdsSent++;
            evlog::add("Befehl an Bedienteil: %02X %02X %02X", d[3], d[4], d[5]);
            Serial.printf("[CI] Control sent: 00 00 00 %02X %02X %02X FF FF\n", d[3], d[4], d[5]);
            ciFastPolls = 5;                  // confirm quickly
            ciNextPoll = millis() + 300;
            lastPublish = 0;                  // publish the confirmation right away
        } else if (!aldeCiCommandEmpty(cmd)) {
            evlog::add("Befehl verworfen: Bedienteil antwortet nicht");
        }
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
#endif // CI_SUPPORTED

const char* sensorControl(const AldeCiCommand& cmd) {
#if !CI_SUPPORTED
    (void)cmd;
    return "Steuerung auf diesem Chip nicht möglich (zweiter UART fehlt)";
#else
    if (config.ci_mode != 2) return "Steuerung ist aus (Einstellungen → Bedienteil-Bus)";
    if (!sensorCiOk()) return "Keine Verbindung zum Bedienteil";
    if (aldeCiCommandEmpty(cmd)) return "Kein Befehl";
    if (cmd.hasTarget && !(cmd.target >= 5.0f && cmd.target <= 30.0f)) return "Sollwert 5-30 °C";
    if (cmd.gas > 1) return "Gas: 0 oder 1";
    if (cmd.el_level > 3) return "Heizstab: 0-3 kW";
    if (cmd.water > 2) return "Warmwasser: 0 = aus, 1 = an, 2 = Boost";
    portENTER_CRITICAL(&cmdMux);
    if (cmd.hasTarget) { pendingCmd.hasTarget = true; pendingCmd.target = cmd.target; }
    if (cmd.gas >= 0) pendingCmd.gas = cmd.gas;
    if (cmd.el_level >= 0) pendingCmd.el_level = cmd.el_level;
    if (cmd.water >= 0) pendingCmd.water = cmd.water;
    cmdPending = true;
    portEXIT_CRITICAL(&cmdMux);
    return nullptr;
#endif
}

// ============================================================
// Setup / loop
// ============================================================
void sensorInit() {
    for (uint8_t i = 0; i < MAX_VALUES; i++) sensorData.values[i] = NAN;
    aldeCiCommandClear(pendingCmd);

#if LIN_SLP_PIN >= 0
    // TJA1020 and similar: SLP/NSLP high = normal mode (receiver on). TX stays unused.
    pinMode(LIN_SLP_PIN, OUTPUT);
    digitalWrite(LIN_SLP_PIN, HIGH);
#endif
    linSerial.setRxBufferSize(1024);
    // 9600 baud 8N2, RX only (TX pin -1): receive-only by construction
    linSerial.begin(LIN_BAUD, SERIAL_8N2, LIN_RX_PIN, -1);
    lin.onSignalChange = logSignal;
    Serial.printf("[LIN] Alde reader on GPIO%d (RX only, %d baud 8N2)\n", LIN_RX_PIN, LIN_BAUD);
#if CI_SUPPORTED
    Serial.printf("[CI] External panel bus on RX GPIO%d / TX GPIO%d, mode %u\n", CI_RX_PIN, CI_TX_PIN, config.ci_mode);
#endif
}

static void copyReadings(unsigned long now) {
    const AldeReadings& r = lin.readings();
    bool ci = sensorCiOk() && ciState.valid;
    const float N = NAN;
    const AldeCiState& c = ciState;
    const float src[] = {
        r.room, r.setpoint, r.glycol, r.water, r.hotwater_target, r.pcb, r.battery,
        r.mains_current, r.el_power, r.el_allowed, r.burner_fan, r.gas, r.ac_active, r.ac_light,
        lin.busOk(now) ? 1.0f : 0.0f,
        ci ? c.setpoint : N, ci ? c.room : N, ci ? c.outdoor : N,
        ci ? (float)c.gas : N, ci ? (float)c.el_level : N, ci ? (float)c.water : N,
        ci ? (float)c.pump : N, ci ? (float)c.panel_on : N, ci ? (float)c.error : N,
        ci ? (float)c.ac_input : N,
        ciActiveMode > 0 ? (sensorCiOk() ? 1.0f : 0.0f) : N,
    };
    static_assert(sizeof(src) / sizeof(src[0]) == sizeof(VALUES) / sizeof(VALUES[0]), "VALUES[] and src[] differ");
    for (uint8_t i = 0; i < VALUE_COUNT; i++) sensorData.values[i] = src[i];
}

void sensorLoop() {
    unsigned long now = millis();

    uint8_t chunk[128];
    int avail;
    while ((avail = linSerial.available()) > 0) {
        int n = linSerial.readBytes(chunk, avail < (int)sizeof(chunk) ? avail : (int)sizeof(chunk));
        if (n <= 0) break;
        lin.feed(chunk, n, now);
    }
    lin.tick(now);

#if CI_SUPPORTED
    static bool lastCiOk = false;
    ciLoop(now);
    now = millis();
    bool ciOk = sensorCiOk();
    if (ciOk != lastCiOk) { lastCiOk = ciOk; ciChanged = true; }
#endif

    bool busOk = lin.busOk(now);
    bool busChanged = busOk != sensorData.ok;
    sensorData.ok = busOk;
    if (busOk) sensorData.last_ok = lin.stats().last_frame_ms;
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
    Serial.printf("[LIN] ok=%u bad=%u empty=%u bus=%s ac_seen=%lus ago\n", (unsigned)s.frames_ok,
                  (unsigned)s.frames_bad, (unsigned)s.headers_empty, lin.busOk(millis()) ? "OK" : "---",
                  s.last_ac_ms ? (unsigned long)((millis() - s.last_ac_ms) / 1000) : 0UL);
    Serial.printf("[CI] mode=%u polls ok=%u bad=%u commands=%u link=%s raw=%02X %02X %02X %02X %02X %02X\n",
                  config.ci_mode, (unsigned)ciPollsOk, (unsigned)ciPollsBad, (unsigned)ciCmdsSent,
                  sensorCiOk() ? "OK" : "---", ciState.raw[0], ciState.raw[1], ciState.raw[2],
                  ciState.raw[3], ciState.raw[4], ciState.raw[5]);
    for (uint8_t i = 0; i < VALUE_COUNT; i++) {
        Serial.printf("[READ] %-20s %10.*f %s -> %s/%s\n", VALUES[i].name, VALUES[i].decimals,
                      sensorData.values[i], VALUES[i].unit, config.mqtt_prefix, VALUES[i].key);
    }
}

void sensorToggleSigLog() {
    sigLog = !sigLog;
    Serial.printf("[SIG] Change log %s\n", sigLog ? "on" : "off");
}
