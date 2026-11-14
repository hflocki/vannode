// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Sensor part: listens passively on the LIN bus of an Alde Compact 3020 HE.
//
// The UART is opened WITHOUT a TX pin, so this firmware cannot write to the bus
// even by mistake. The Alde panel stays the LIN master and keeps working.
// Decoding: alde_lin.cpp (based on Mellowgroup-dk/alde-3020he-lin, MIT).
#include "sensor.h"
#include "alde_lin.h"

// Order = index into sensorData.values[]. Keys listed in BINARY_KEYS (mqtt_ha.cpp)
// become binary sensors in Home Assistant (payload 1/0).
const ValueDef VALUES[] = {
//   key                name                    unit   device_class   icon                       decimals
    {"room",            "Raumtemperatur",       "°C",  "temperature", "mdi:home-thermometer",    1},
    {"setpoint",        "Sollwert",             "°C",  "temperature", "mdi:thermostat",          1},
    {"glycol",          "Glykol",               "°C",  "temperature", "mdi:coolant-temperature", 1},
    {"water",           "Warmwasser",           "°C",  "temperature", "mdi:water-thermometer",   1},
    {"hotwater_target", "Warmwasser Ziel",      "°C",  "temperature", "mdi:water-boiler",        0},
    {"pcb",             "Platine",              "°C",  "temperature", "mdi:chip",                1},
    {"battery",         "Batterie",             "V",   "voltage",     "mdi:car-battery",         2},
    {"mains_current",   "Landstrom",            "A",   "current",     "mdi:power-plug",          2},
    {"el_power",        "Heizstab",             "W",   "power",       "mdi:flash",               0},
    {"el_allowed",      "Heizstab erlaubt",     "W",   "power",       "mdi:flash-outline",       0},
    {"burner_fan",      "Brennergebläse",       "rpm", nullptr,       "mdi:fan",                 0},
    {"gas",             "Gas",                  "",    nullptr,       "mdi:fire",                0},
    {"ac_active",       "Klimaanlage aktiv",    "",    nullptr,       "mdi:snowflake",           0},
    {"ac_light",        "Klimaanlage Licht",    "%",   nullptr,       "mdi:lightbulb",           0},
    {"bus_ok",          "LIN-Bus",              "",    nullptr,       "mdi:lan-connect",         0},
};
const uint8_t VALUE_COUNT = sizeof(VALUES) / sizeof(VALUES[0]);
static_assert(sizeof(VALUES) / sizeof(VALUES[0]) <= MAX_VALUES, "Too many values, raise MAX_VALUES");

// UART2 on the classic ESP32 / S3; the C3 only has UART0 and UART1
#if SOC_UART_NUM > 2
static HardwareSerial& linSerial = Serial2;
#else
static HardwareSerial& linSerial = Serial1;
#endif
static AldeLin lin;
static unsigned long lastPublish = 0;
static volatile bool publishRequested = true;
static bool sigLog = false;

static void logSignal(uint8_t pid, const uint8_t* d, int n) {
    if (!sigLog) return;
    char hex[3 * 8 + 1];
    int p = 0;
    for (int k = 0; k < n && k < 8; k++) p += snprintf(hex + p, sizeof(hex) - p, "%02X ", d[k]);
    if (p > 0) hex[p - 1] = 0; else hex[0] = 0;
    Serial.printf("[SIG] %02X (len=%d): %s\n", pid, n, hex);
}

void sensorInit() {
    for (uint8_t i = 0; i < MAX_VALUES; i++) sensorData.values[i] = NAN;

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
}

static void copyReadings(unsigned long now) {
    const AldeReadings& r = lin.readings();
    const float src[] = {
        r.room, r.setpoint, r.glycol, r.water, r.hotwater_target, r.pcb, r.battery,
        r.mains_current, r.el_power, r.el_allowed, r.burner_fan, r.gas, r.ac_active, r.ac_light,
        lin.busOk(now) ? 1.0f : 0.0f,
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

    bool busOk = lin.busOk(now);
    bool busChanged = busOk != sensorData.ok;
    sensorData.ok = busOk;
    if (busOk) sensorData.last_ok = lin.stats().last_frame_ms;
    sensorData.errors = lin.stats().frames_bad;

    // Publish on change, but at most every interval_s; bus up/down right away
    bool changed = lin.takeChanged();
    if (changed) publishRequested = true;
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
    for (uint8_t i = 0; i < VALUE_COUNT; i++) {
        Serial.printf("[READ] %-20s %10.*f %s -> %s/%s\n", VALUES[i].name, VALUES[i].decimals,
                      sensorData.values[i], VALUES[i].unit, config.mqtt_prefix, VALUES[i].key);
    }
}

void sensorToggleSigLog() {
    sigLog = !sigLog;
    Serial.printf("[SIG] Change log %s\n", sigLog ? "on" : "off");
}
