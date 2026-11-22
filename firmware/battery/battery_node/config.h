// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Optional personal overrides, not in git (see config_local.h.example). Everything defined there
// wins over the defaults in this file, e.g. own passwords, WiFi / MQTT or a different plugin set.
#if __has_include("config_local.h")
#include "config_local.h"
#endif

// ============================================================
// >>> PLUGINS - battery types in the firmware <<<
// ------------------------------------------------------------
// 1 = type is compiled in, 0 = left out. Batteries are not set here: they are added in
// the web UI (Geräte, "+ Gerät hinzufügen"), up to MAX_PLUGINS devices in total.
// Every battery keeps a BLE connection (the BM2 only as fallback), so at most
// MAX_BLE_LINKS (3) of them can be switched on at the same time. Victron only listens
// and needs no connection slot.
// The other Bluetooth devices (roof hatch, pump, fridge, sensors) run on the ble-node
// (firmware/ble); both share the core in firmware/_common/ble.
// ============================================================
#ifndef PLUGIN_DALY
#define PLUGIN_DALY           1      // Battery: Daly smart BMS (BLE name "DL-..."), BLE connection
#endif
#ifndef PLUGIN_ECTIVE
#define PLUGIN_ECTIVE         1      // Battery: Ective BM X battery monitor (LiPower), BLE connection
#endif
#ifndef PLUGIN_BM2
#define PLUGIN_BM2            1      // Battery: BM2 battery monitor (broadcast, connection as fallback)
#endif
#ifndef PLUGIN_JBD
#define PLUGIN_JBD            1      // Battery: JBD / Xiaoxiang smart BMS (service FF00), BLE connection
#endif
#ifndef PLUGIN_JK
#define PLUGIN_JK             1      // Battery: JK-BMS (JK02 protocol), BLE connection
#endif
#ifndef PLUGIN_LITIME
#define PLUGIN_LITIME         1      // Battery: LiTime / Redodo / PowerQueen, BLE connection
#endif
#ifndef PLUGIN_VICTRON
#define PLUGIN_VICTRON        1      // Victron Instant Readout: battery monitor (SmartShunt, BMV), solar charger,
#endif                               // DC-DC charger - passive BLE with the key from the VictronConnect app

// At most this many connecting devices may be switched on at the same time
// (NimBLE default CONFIG_BT_NIMBLE_MAX_CONNECTIONS = 3). Checked in the web UI, when saving
// and at boot.
#ifndef MAX_BLE_LINKS
#define MAX_BLE_LINKS         3
#endif
// Devices in total (RAM: every device is an object with its own buffers)
#ifndef MAX_PLUGINS
#define MAX_PLUGINS           6
#endif

// Derived switches (do not edit): which code parts of the shared core are compiled.
// Plugins of the ble-node are not part of this firmware.
#define CT_BLE_PASSIVE  PLUGIN_VICTRON   // listen only, no connection slot
#define CT_GAS          0
#define CT_BMS          (PLUGIN_DALY + PLUGIN_ECTIVE + PLUGIN_BM2 + PLUGIN_JBD + PLUGIN_JK + PLUGIN_LITIME)
#define CT_BLE_LINKS    CT_BMS
#define CT_USE_BLE      ((CT_BLE_PASSIVE + CT_BLE_LINKS) > 0)
#define CT_PLUGIN_COUNT (CT_BLE_PASSIVE + CT_BLE_LINKS)

#if CT_PLUGIN_COUNT == 0
#warning "No battery type selected in config.h - the device only offers WiFi, MQTT and the web UI"
#endif

// ============================================================
// GENERAL CONSTANTS
// ============================================================
#define FW_VERSION            "2.6.2"
#define FW_DEVICE             "battery"   // device id in the firmware marker (fwcheck.h)
#define NODE_TITLE            "Battery Node" // web page without index.html.gz, Home Assistant model, serial
#define NODE_DIR              "battery"   // repo folder (about card)

// Network upload from the Arduino IDE (port in the IDE, password DEFAULT_OTA_PASSWORD).
// 0 = off, updates then only through the web UI (with login, if set).
#ifndef ENABLE_ARDUINO_OTA
#define ENABLE_ARDUINO_OTA    1
#endif

#ifndef DEFAULT_NODE_NAME
#define DEFAULT_NODE_NAME     "battery-node"      // Node name (web UI, MQTT tab): host name, Home Assistant
#endif                                            // device and web UI title
#ifndef DEFAULT_AP_SSID
#define DEFAULT_AP_SSID       "Battery-Node"          // Base name; "-XXXXXX" (last 3 MAC bytes) is appended
#endif
#ifndef DEFAULT_AP_PASSWORD
#define DEFAULT_AP_PASSWORD   "battery1234"   // Test placeholder (min. 8 characters) - change in the web UI
#endif
#ifndef DEFAULT_OTA_PASSWORD
#define DEFAULT_OTA_PASSWORD  "battery1234"   // Test placeholder - change before the first flash
#endif
#ifndef DEFAULT_MQTT_SERVER
#define DEFAULT_MQTT_SERVER   ""                 // Set in the web UI
#endif
#ifndef DEFAULT_MQTT_PORT
#define DEFAULT_MQTT_PORT     1883
#endif

// ---- Factory defaults for WiFi / MQTT / web login -----------------------
// Used only while nothing is stored on the device yet: first boot, after "factory
// reset" in the web UI or after flashing with "Erase All Flash". Put your own values
// into config_local.h, then a freshly flashed device connects right away.
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
// Topics: <prefix>/<plugin>/<value>, commands <prefix>/cmd/<plugin>/<value>/set,
// <prefix>/status, <prefix>/info (docs/TOPICS.md). The Home Assistant node ID is derived
// from it ("camper/battery" -> "camper_battery"), so two nodes need two prefixes.
#ifndef DEFAULT_MQTT_PREFIX
#define DEFAULT_MQTT_PREFIX   "camper/battery"
#endif

// WiFi TX power in 0.25 dBm steps: 70 = 17.5 dBm (factory default, selectable in the web UI).
// If the connection keeps dropping, try 8.5 dBm (34) first.
#ifndef DEFAULT_TX_POWER
#define DEFAULT_TX_POWER      70
#endif

// Own access point while connected to the WiFi (web UI, tab "WLAN"):
//   true  = AP always on (reachable at 192.168.4.1 at any time)
//   false = AP only as fallback when the WiFi is not reachable (more stable STA connection)
#ifndef DEFAULT_AP_ALWAYS
#define DEFAULT_AP_ALWAYS     true
#endif
#ifndef AP_FALLBACK_MS
#define AP_FALLBACK_MS        60000  // AP-only-as-fallback: start the AP after 60 s without WiFi
#endif

#ifndef MQTT_PUB_CACHE
#define MQTT_PUB_CACHE        256    // Remembered plugin topics (skip unchanged values between periodic publishes)
#endif
#ifndef MQTT_CORE_MS
#define MQTT_CORE_MS          30000  // Device status / WiFi RSSI publish interval
#endif
#ifndef WS_UPDATE_MS
#define WS_UPDATE_MS          1000   // Web UI live update interval
#endif

#ifndef CONFIG_FILE
#define CONFIG_FILE           "/config.json"
#endif

// ============================================================
// BLUETOOTH (shared by all BLE plugins)
// ============================================================
// Passive scan duty cycle (units of 0.625 ms). Window < interval leaves airtime for WiFi.
// Safe start: this many power-on / crash starts in a row, each ending before the
// window is over, start the node with all devices switched off (web UI stays usable)
#ifndef SAFE_START_BOOTS
#define SAFE_START_BOOTS      3
#endif
#ifndef SAFE_START_WINDOW_MS
#define SAFE_START_WINDOW_MS  30000
#endif
// Event log entry when the largest free heap block drops below this (bytes)
#ifndef HEAP_LOW_WARN
#define HEAP_LOW_WARN         16000
#endif

#ifndef BLE_SCAN_INTERVAL
#define BLE_SCAN_INTERVAL     160    // 100 ms
#endif
#ifndef BLE_SCAN_WINDOW
#define BLE_SCAN_WINDOW       80     // 50 ms (50 % duty cycle)
#endif
#ifndef BLE_SCAN_WINDOW_LINKED
#define BLE_SCAN_WINDOW_LINKED 32    // 20 ms (20 %) while a device connection is up
#endif
#ifndef BLE_MAX_SEEN
#define BLE_MAX_SEEN          30     // Devices kept in the "nearby" list of the web UI
#endif
#ifndef BLE_SEEN_TIMEOUT_MS
#define BLE_SEEN_TIMEOUT_MS   120000 // Drop devices from that list after 2 min silence
#endif
#ifndef BLE_RAW_MAX
#define BLE_RAW_MAX           32     // Max. bytes kept for the raw data view
#endif
// Connecting plugins (batteries)
#ifndef BLE_CONNECT_TIMEOUT
#define BLE_CONNECT_TIMEOUT   4000   // Connection attempt timeout in ms (non-blocking)
#endif
#ifndef BLE_RETRY_MS
#define BLE_RETRY_MS          10000  // Wait between connection attempts
#endif
// Clock (NTP over WiFi)
#ifndef NTP_SERVER
#define NTP_SERVER            "pool.ntp.org"
#endif
#ifndef TIME_ZONE
#define TIME_ZONE             "CET-1CEST,M3.5.0,M10.5.0/3"   // POSIX TZ: Central Europe
#endif
#ifndef BLE_PAIR_PAUSE_MS
#define BLE_PAIR_PAUSE_MS     300000 // Pause after all pairing variants failed (other devices keep running)
#endif
#ifndef BLE_RETRY_MAX_MS
#define BLE_RETRY_MAX_MS      60000  // Back-off limit while the device is out of range
#endif
#ifndef BLE_RELEASE_MS
#define BLE_RELEASE_MS        (30UL * 60UL * 1000UL) // "Release for the app" ends after 30 min (0 = never)
#endif

// ============================================================
// PLUGINS: BATTERIES (Daly, Ective BM X, BM2)
// ------------------------------------------------------------
// Protocols ported from aiobmsble (Apache-2.0), see src/plugins/bms_proto.h.
// Every battery keeps a BLE connection and is polled every "Abfrageintervall" seconds
// (web UI); the BM2 is read from its broadcast while that works and only connects if not.
// All batteries of the node are also published as one JSON list <prefix>/batteries
// for the displays (wall panel, Pocket-Node).
// ============================================================
#ifndef BMS_DEFAULT_INTERVAL_S
#define BMS_DEFAULT_INTERVAL_S 10    // Poll interval (web UI, 5-300 s)
#endif
#ifndef BMS_RESPONSE_TIMEOUT_MS
#define BMS_RESPONSE_TIMEOUT_MS 3000 // Wait for an answer
#endif
#ifndef BMS_STALE_MS
#define BMS_STALE_MS          120000 // Values older than this count as unavailable
#endif
#ifndef BM2_WAIT_MS
#define BM2_WAIT_MS           4000   // BM2 notifies about once per second
#endif
#ifndef BM2_ADV_FRESH_MS
#define BM2_ADV_FRESH_MS      90000  // BM2 broadcast younger than this: no connection needed
#endif
#ifndef BM2_ADV_GRACE_MS
#define BM2_ADV_GRACE_MS      30000  // After start: wait this long for a broadcast before connecting
#endif

// ============================================================
// STATUS LED (optional RGB LED, WS2812)
// M5Stack AtomS3 Lite: switched on automatically (board "M5AtomS3", RGB LED on GPIO35).
// Other boards with an RGB LED: set STATUS_LED_PIN in config_local.h. -1 = no LED.
//   green  = every switched-on device delivers values
//   yellow = at least one device missing / no data
//   red    = no WiFi or no MQTT connection (blinking)
//   blue   = no device set up yet
// ============================================================
#ifndef STATUS_LED_PIN
#if defined(ARDUINO_M5STACK_ATOMS3) && defined(PIN_RGB_LED)
#define STATUS_LED_PIN        PIN_RGB_LED
#else
#define STATUS_LED_PIN        -1
#endif
#endif
#ifndef STATUS_LED_BRIGHTNESS
#define STATUS_LED_BRIGHTNESS 24     // 0-255, kept low so it does not light up the camper at night
#endif

// ============================================================
// CORE CONFIGURATION (stored in /config.json; plugin settings live in the plugins)
// ============================================================
struct AppConfig {
    char wifi_ssid[64];
    char wifi_pass[64];
    char ap_ssid[32];
    char ap_pass[32];
    int8_t wifi_tx_power;  // TX power in 0.25 dBm steps (see TX_POWER_OPTIONS)
    bool ap_always;        // true = own AP stays on while connected, false = fallback only
    bool debug;            // web UI shows the debug tab (node status, device logs)

    char mqtt_server[64];
    uint16_t mqtt_port;
    char mqtt_user[32];
    char mqtt_pass[32];
    char mqtt_prefix[32];

    char web_user[32];     // Web UI login, empty = no login
    char web_pass[32];

    char node_name[24];    // "battery-node" ...: host name, HA device, web UI title
};

// Device wide runtime values (WiFi diagnosis)
struct CoreData {
    int rssi;              // WiFi signal strength (written by wifi_manager)
    uint32_t wifi_drops;   // WiFi disconnects since boot
    char wifi_reason[32];  // Reason of the last WiFi disconnect
};

extern AppConfig config;
extern CoreData coreData;

#endif // CONFIG_H
