// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#ifndef CONFIG_H
#define CONFIG_H

// =====================================================================
//  IO Node - relays, inputs, DS18B20 and analog values, all set up in the web UI
//  (ESP32-C3 Super Mini; channels and bus pins in /io.json, see io.h)
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
#define FW_VERSION            "1.0.0"
#define FW_DEVICE            "io"   // device id in the firmware marker (fwcheck.h)
#define DEVICE_NAME           "IO Node"   // web UI title, HA device name, serial banner

#ifndef DEFAULT_AP_SSID
#define DEFAULT_AP_SSID       "IO-Node"   // Base name; "-XXXXXX" (last 3 MAC bytes) is appended
#endif
#ifndef DEFAULT_AP_PASSWORD
#define DEFAULT_AP_PASSWORD   "ionode1234"  // Test placeholder (min. 8 characters) - change in config_local.h
#endif
#ifndef DEFAULT_OTA_PASSWORD
#define DEFAULT_OTA_PASSWORD  "ionode1234"  // Test placeholder - change in config_local.h
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
#define DEFAULT_WEB_USER        "admin"      // login is always on; empty input keeps the stored value
#endif
#ifndef DEFAULT_WEB_PASSWORD
#define DEFAULT_WEB_PASSWORD    "admin"
#endif
#ifndef DEFAULT_MQTT_PREFIX
#define DEFAULT_MQTT_PREFIX   "camper/io"   // Topics: <prefix>/<channel id>, <prefix>/cmd/<id>, <prefix>/status
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
#define MQTT_REPUBLISH_MS     60000  // Re-send all states at least this often (changes go out right away)
#endif
#ifndef WS_UPDATE_MS
#define WS_UPDATE_MS          1000   // Web UI live update interval
#endif

#ifndef CONFIG_FILE
#define CONFIG_FILE           "/config.json"
#endif
#ifndef OTA_HOSTNAME
#define OTA_HOSTNAME          "io-node"   // also the mDNS name: io-node.local
#endif

// ============================================================
// CHANNELS (io.h) - everything else is set in the web UI
// ============================================================
#ifndef IO_MAX_CH
#define IO_MAX_CH             48     // channels (relays, inputs, temperatures, analog values)
#endif
#ifndef IO_SR_MAX
#define IO_SR_MAX             4      // 74HC595 in one chain (8 outputs each)
#endif
#ifndef IO_EXP_MAX
#define IO_EXP_MAX            8      // I2C expanders MCP23017 / PCF8574 (addresses one after the other)
#endif
#ifndef IO_FILE
#define IO_FILE               "/io.json"
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
    char mqtt_fb[72];        // fallback broker "host[:port]", empty = off (mqttfb.h)
    char mqtt_user[32];
    char mqtt_pass[32];
    char mqtt_prefix[32];

    char web_user[32];     // Web UI login, always on (factory login admin / admin)
    char web_pass[32];
};

// ============================================================
// LIVE DATA
// ============================================================
struct SensorData {
    int rssi;                  // WiFi signal strength (written by wifi_manager)
    uint32_t wifi_drops;       // WiFi disconnects since boot
    char wifi_reason[32];      // Reason of the last WiFi disconnect
};

extern AppConfig config;
extern SensorData sensorData;

#endif // CONFIG_H
