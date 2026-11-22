// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#ifndef CONFIG_H
#define CONFIG_H

// =====================================================================
//  Alde Node - LIN reader and optional control for Alde Compact 3020 / 3030 / 3030 Plus
//  Created from firmware/_template.
// =====================================================================

#include <Arduino.h>

// Optional personal overrides, not in git (see config_local.h.example). Everything defined there
// wins over the defaults in this file, e.g. own passwords or WiFi / MQTT for a freshly flashed device.
#if __has_include("config_local.h")
#include "config_local.h"
#endif

// ============================================================
// GENERAL CONSTANTS
// ============================================================
#define FW_VERSION            "0.6.3"
#define FW_DEVICE            "alde"   // device id in the firmware marker (fwcheck.h)
#define DEVICE_NAME           "Alde Node"   // web UI title, HA device name, serial banner

#ifndef DEFAULT_AP_SSID
#define DEFAULT_AP_SSID       "Alde-Node"   // Base name; "-XXXXXX" (last 3 MAC bytes) is appended
#endif
#ifndef DEFAULT_AP_PASSWORD
#define DEFAULT_AP_PASSWORD   "alde1234"      // Test placeholder (min. 8 characters) - change in config_local.h
#endif
#ifndef DEFAULT_OTA_PASSWORD
#define DEFAULT_OTA_PASSWORD  "alde1234"      // Test placeholder - change in config_local.h
#endif
#ifndef DEFAULT_MQTT_SERVER
#define DEFAULT_MQTT_SERVER   ""                 // Set in the web UI
#endif
#ifndef DEFAULT_MQTT_PORT
#define DEFAULT_MQTT_PORT     1883
#endif

// ---- Factory defaults for WiFi / MQTT / web login -----------------------
// Used only while nothing is stored on the device yet: first boot, after "factory
// reset" or after flashing with "Erase All Flash". Put your own values into
// config_local.h, then a freshly flashed device connects right away.
#ifndef DEFAULT_WIFI_SSID
#define DEFAULT_WIFI_SSID       ""
#endif
#ifndef DEFAULT_WIFI_PASSWORD
#define DEFAULT_WIFI_PASSWORD   ""
#endif
#ifndef DEFAULT_MQTT_USER
#define DEFAULT_MQTT_USER       ""
#endif
#ifndef DEFAULT_MQTT_PASSWORD
#define DEFAULT_MQTT_PASSWORD   ""
#endif
#ifndef DEFAULT_WEB_USER
#define DEFAULT_WEB_USER        ""      // empty = no login
#endif
#ifndef DEFAULT_WEB_PASSWORD
#define DEFAULT_WEB_PASSWORD    ""
#endif
#ifndef DEFAULT_MQTT_PREFIX
#define DEFAULT_MQTT_PREFIX   "camper/alde"   // Topics: <prefix>/<value key>, <prefix>/status, <prefix>/rssi
#endif

// WiFi TX power in 0.25 dBm steps: 70 = 17.5 dBm (factory default, selectable in the web UI)
#ifndef DEFAULT_TX_POWER
#define DEFAULT_TX_POWER      70
#endif

// Own access point while connected to the WiFi (web UI, tab "WiFi"):
//   true  = AP always on (reachable at 192.168.4.1 at any time)
//   false = AP only as fallback when the WiFi is not reachable (more stable STA connection)
#ifndef DEFAULT_AP_ALWAYS
#define DEFAULT_AP_ALWAYS     false
#endif
#ifndef AP_FALLBACK_MS
#define AP_FALLBACK_MS        60000  // AP-only-as-fallback: start the AP after 60 s without WiFi
#endif

#ifndef MQTT_REPUBLISH_MS
#define MQTT_REPUBLISH_MS     60000  // Re-send all values at least this often (also sent after every reading)
#endif
#ifndef WS_UPDATE_MS
#define WS_UPDATE_MS          1000   // Web UI live update interval
#endif

#ifndef CONFIG_FILE
#define CONFIG_FILE           "/config.json"
#endif
#ifndef OTA_HOSTNAME
#define OTA_HOSTNAME          "alde-node"   // also the mDNS name: alde-node.local
#endif

// ============================================================
// PINS (defaults per chip, override in config_local.h)
//   ESP32 (classic)  heater bus RX 16, panel bus RX 26 / TX 25
//   ESP32-S3         heater bus RX 16, panel bus RX 4 / TX 5 (no GPIO 25, GPIO 26 is flash)
//   ESP32-C3         only ONE bus at a time (two UARTs): heater bus RX 3 or panel bus RX 4 / TX 5
// ============================================================
#if CONFIG_IDF_TARGET_ESP32
#define ALDE_DEF_LIN_RX 16
#define ALDE_DEF_CI_RX  26
#define ALDE_DEF_CI_TX  25
#elif CONFIG_IDF_TARGET_ESP32C3
#define ALDE_DEF_LIN_RX 3
#define ALDE_DEF_CI_RX  4
#define ALDE_DEF_CI_TX  5
#else
#define ALDE_DEF_LIN_RX 16
#define ALDE_DEF_CI_RX  4
#define ALDE_DEF_CI_TX  5
#endif

// ============================================================
// HEATER BUS (passive, receive only, optional)
// LIN transceiver (e.g. TJA1020) on the "Control panel" RJ12 socket of the heater via
// a splitter, so the original Alde panel keeps working. NO master resistor on this side.
// Can be switched off in the web UI if only the panel bus transceiver is fitted.
// ============================================================
#ifndef LIN_RX_PIN
#define LIN_RX_PIN            ALDE_DEF_LIN_RX   // RXD of the LIN transceiver
#endif
#ifndef LIN_SLP_PIN
#define LIN_SLP_PIN           -1     // SLP/NSLP of the transceiver if wired to the ESP (driven HIGH), -1 = not wired
#endif
#ifndef DEFAULT_LIN_ENABLED
#define DEFAULT_LIN_ENABLED   true   // false = only the panel bus transceiver is fitted
#endif
#ifndef LIN_BAUD
#define LIN_BAUD              9600   // 8N2
#endif
#ifndef DEFAULT_INTERVAL_S
#define DEFAULT_INTERVAL_S    5      // Minimum gap between two MQTT updates in seconds (web UI, 1-600)
#endif
#ifndef MAX_VALUES
#define MAX_VALUES            32     // Values the device publishes (see VALUES[] in sensor.cpp)
#endif

// ============================================================
// EXTERNAL PANEL BUS ("CI-bus", optional)
// LIN transceiver on the external panel port of the Alde panel. Here the ESP is the LIN
// master (1 kOhm + diode master pull-up to 12 V on this transceiver): it polls the panel
// for its state and - only if allowed in the web UI - sends control frames. 19200 baud
// 8N1. Off by default (ci_mode 0). On the ESP32-C3 only with the heater bus switched off.
// ============================================================
#ifndef CI_RX_PIN
#define CI_RX_PIN             ALDE_DEF_CI_RX    // RXD of the panel bus transceiver
#endif
#ifndef CI_TX_PIN
#define CI_TX_PIN             ALDE_DEF_CI_TX    // TXD of the panel bus transceiver
#endif
#ifndef CI_SLP_PIN
#define CI_SLP_PIN            -1     // SLP/NSLP of the 2nd transceiver if wired to the ESP (driven HIGH), -1 = not wired
#endif
#ifndef CI_BAUD
#define CI_BAUD               19200
#endif
#ifndef CI_POLL_MS
#define CI_POLL_MS            1000   // Info poll interval
#endif
#ifndef DEFAULT_CI_MODE
#define DEFAULT_CI_MODE       0      // 0 = off, 1 = read only (ESP polls as master), 2 = read and control
#endif
#ifndef DEFAULT_CI_FUNCTION
#define DEFAULT_CI_FUNCTION   1      // Function id in the registration: 1 = Compact 3020, 2 = 3030, 3 = 3030 Plus
#endif
#ifndef DEFAULT_EL_MAX
#define DEFAULT_EL_MAX        1      // Highest heating element step (kW) a command may set, 0-3 (web UI)
#endif
#ifndef CI_CMD_TRIES
#define CI_CMD_TRIES          5      // Polls a command waits for a free panel (not busy, no error) before it is dropped
#endif

// ============================================================
// CONFIGURATION STRUCT (stored in /config.json)
// ============================================================
struct AppConfig {
    char wifi_ssid[64];
    char wifi_pass[64];
    char ap_ssid[32];
    char ap_pass[32];
    int8_t wifi_tx_power;  // TX power in 0.25 dBm steps (see TX_POWER_OPTIONS)
    bool ap_always;        // true = own AP stays on while connected, false = fallback only

    char mqtt_server[64];
    uint16_t mqtt_port;
    char mqtt_user[32];
    char mqtt_pass[32];
    char mqtt_prefix[32];

    char web_user[32];     // Web UI login, empty = no login
    char web_pass[32];

    // ---- Device settings (load/save/validate in webserver.cpp) ----
    uint16_t interval_s;   // Minimum gap between MQTT updates in seconds
    uint8_t ci_mode;       // External panel bus: 0 = off, 1 = read, 2 = read + control, 3 = listen only (diagnosis)
    uint8_t ci_function;   // Function id for the registration (1/2/3)
    bool lin_enabled;      // Heater bus transceiver fitted (change needs a restart)
    uint8_t el_max;        // Highest heating element step a command may set (kW)
    bool debug;            // Debug tab and detailed log (frames, signal changes)
};

// ============================================================
// LIVE DATA
// ============================================================
struct SensorData {
    float values[MAX_VALUES];  // Current value per entry of VALUES[] (sensor.cpp), NAN = unknown
    bool ok;                   // A bus delivers valid data (heater bus, or panel bus without it)
    unsigned long last_ok;     // millis() of the last valid reading
    uint32_t errors;           // Failed readings since boot
    uint32_t readCount;        // Completed measurement cycles (used to detect new data)
    int rssi;                  // WiFi signal strength (written by wifi_manager)
    uint32_t wifi_drops;       // WiFi disconnects since boot
    char wifi_reason[32];      // Reason of the last WiFi disconnect
};

extern AppConfig config;
extern SensorData sensorData;

#endif // CONFIG_H
