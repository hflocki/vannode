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
// >>> PLUGINS - device types in the firmware <<<
// ------------------------------------------------------------
// 1 = type is compiled in, 0 = left out (smaller firmware). Devices are not set here:
// they are added in the web UI (System -> Plugins, "+ Gerät hinzufügen"), any number of
// the same type, up to MAX_PLUGINS devices in total.
// Bluetooth limits of one ESP32:
//   - passive sensors (gas, own BLE sensor) only listen, any number works
//   - connecting devices (pump, roof hatch, fridge) keep a BLE connection each:
//     at most MAX_BLE_LINKS (3) of them switched on at the same time
// More devices: a second BLE node with its own topic (e.g. camper/tool2).
// ============================================================
#ifndef PLUGIN_DIMES
#define PLUGIN_DIMES          1      // Gas: Rotarex DIMES / SRG WAVE (passive BLE)
#endif
#ifndef PLUGIN_MOPEKA_PRO
#define PLUGIN_MOPEKA_PRO     1      // Gas: Mopeka Pro family (Pro Check, Pro Plus, Universal ...)
#endif
#ifndef PLUGIN_MOPEKA_STD
#define PLUGIN_MOPEKA_STD     1      // Gas: Mopeka Standard / Std Check (older model)
#endif
#ifndef PLUGIN_BLESENSOR
#define PLUGIN_BLESENSOR      1      // Own BLE sensor: values at freely set positions of the advertisement
#endif
#ifndef PLUGIN_GOVEE
#define PLUGIN_GOVEE          1      // Thermometer: Govee H5072/H5074/H5075/H5101/H5102/H5104/H5174/H5177/H5179 (passive BLE)
#endif
#ifndef PLUGIN_BTHOME
#define PLUGIN_BTHOME         1      // BTHome v2 sensors without encryption (ATC/pvvx, Shelly BLU ...), passive BLE
#endif
#ifndef PLUGIN_RVTECH
#define PLUGIN_RVTECH         1      // Roof hatch: RV Tech skylight motor control, BLE connection
#endif
#ifndef PLUGIN_CARAWATER
#define PLUGIN_CARAWATER      1      // Water pump: CaraWater (e.g. ISP 15), BLE connection
#endif
#ifndef PLUGIN_WCS_FRIDGE
#define PLUGIN_WCS_FRIDGE     1      // Fridge: WCS compressor fridge, BLE connection
#endif
#ifndef PLUGIN_WIPRO
#define PLUGIN_WIPRO          1      // Alarm: Thitronik WiPro III status (read-only), BLE connection
#endif
#ifndef PLUGIN_DALY
#define PLUGIN_DALY           1      // Battery: Daly smart BMS (BLE name "DL-..."), BLE connection
#endif
#ifndef PLUGIN_ECTIVE
#define PLUGIN_ECTIVE         1      // Battery: Ective BM X battery monitor (LiPower), BLE connection
#endif
#ifndef PLUGIN_BM2
#define PLUGIN_BM2            1      // Battery: BM2 battery monitor (broadcast, connection as fallback)
#endif

// At most this many connecting devices may be switched on at the same time
// (NimBLE default CONFIG_BT_NIMBLE_MAX_CONNECTIONS = 3). Checked in the web UI, when saving
// and at boot. Passive devices only listen and have no limit of their own.
#ifndef MAX_BLE_LINKS
#define MAX_BLE_LINKS         3
#endif
// Devices in total (RAM: every device is an object with its own buffers)
#ifndef MAX_PLUGINS
#define MAX_PLUGINS           12
#endif

// Derived switches (do not edit): which code parts are compiled
#define CT_BLE_PASSIVE  (PLUGIN_DIMES + PLUGIN_MOPEKA_PRO + PLUGIN_MOPEKA_STD + PLUGIN_BLESENSOR + PLUGIN_GOVEE + PLUGIN_BTHOME)
#define CT_GAS          (PLUGIN_DIMES + PLUGIN_MOPEKA_PRO + PLUGIN_MOPEKA_STD)
#define CT_BMS          (PLUGIN_DALY + PLUGIN_ECTIVE + PLUGIN_BM2)
#define CT_BLE_LINKS    (PLUGIN_CARAWATER + PLUGIN_RVTECH + PLUGIN_WCS_FRIDGE + PLUGIN_WIPRO + CT_BMS)
#define CT_USE_BLE      ((CT_BLE_PASSIVE + CT_BLE_LINKS) > 0)
#define CT_PLUGIN_COUNT (CT_BLE_PASSIVE + CT_BLE_LINKS)

#if CT_PLUGIN_COUNT == 0
#warning "No plugin selected in config.h - the device only offers WiFi, MQTT and the web UI"
#endif

// ============================================================
// GENERAL CONSTANTS
// ============================================================
#define FW_VERSION            "2.1.12"
#define FW_DEVICE             "toolbox"   // device id in the firmware marker (fwcheck.h); stays "toolbox"
                                          // so devices with the former Toolbox firmware accept updates

// Network upload from the Arduino IDE (port in the IDE, password DEFAULT_OTA_PASSWORD).
// 0 = off, updates then only through the web UI (with login, if set).
#ifndef ENABLE_ARDUINO_OTA
#define ENABLE_ARDUINO_OTA    1
#endif

#ifndef DEFAULT_NODE_NAME
#define DEFAULT_NODE_NAME     "tool-node"         // Node name (web UI, MQTT tab): host name, Home Assistant
#endif                                            // device and web UI title, e.g. "battery-node"
#ifndef DEFAULT_AP_SSID
#define DEFAULT_AP_SSID       "BLE-Node"          // Base name; "-XXXXXX" (last 3 MAC bytes) is appended
#endif
#ifndef DEFAULT_AP_PASSWORD
#define DEFAULT_AP_PASSWORD   "blenode1234"   // Test placeholder (min. 8 characters) - change in the web UI
#endif
#ifndef DEFAULT_OTA_PASSWORD
#define DEFAULT_OTA_PASSWORD  "blenode1234"   // Test placeholder - change before the first flash
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
// from it ("camper/tool" -> "camper_tool"), so two BLE nodes need two prefixes.
#ifndef DEFAULT_MQTT_PREFIX
#define DEFAULT_MQTT_PREFIX   "camper/tool"
#endif

// WiFi TX power in 0.25 dBm steps: 70 = 17.5 dBm (factory default, selectable in the web UI).
// Full power (19.5 dBm) tends to be unstable on ESP32-C3 Super Mini boards; if the
// connection keeps dropping, try 8.5 dBm (34) first.
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
#ifndef BLE_SCAN_INTERVAL
#define BLE_SCAN_INTERVAL     160    // 100 ms
#endif
#ifndef BLE_SCAN_WINDOW
#define BLE_SCAN_WINDOW       80     // 50 ms (50 % duty cycle)
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
// Connecting plugins (pump, roof hatch, fridge)
#ifndef BLE_CONNECT_TIMEOUT
#define BLE_CONNECT_TIMEOUT   4000   // Connection attempt timeout in ms (non-blocking)
#endif
#ifndef BLE_RETRY_MS
#define BLE_RETRY_MS          10000  // Wait between connection attempts
#endif
// Clock (NTP over WiFi), used e.g. for the time in the pump commands
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
// PLUGIN: OWN BLE SENSOR (values at freely set positions, set in the web UI)
// ============================================================
#ifndef BLESENSOR_VALUES
#define BLESENSOR_VALUES      3      // Values per sensor (each with name, position, conversion)
#endif
#ifndef BLESENSOR_TIMEOUT_MS
#define BLESENSOR_TIMEOUT_MS  (10UL * 60UL * 1000UL) // No advertisement for this long: "offline"
#endif
#ifndef MQTT_BLESENSOR_MS
#define MQTT_BLESENSOR_MS     30000  // Periodic publish (also published immediately on changes)
#endif

// ============================================================
// PLUGIN: GOVEE THERMOMETER
// ============================================================
#ifndef GOVEE_TIMEOUT_MS
#define GOVEE_TIMEOUT_MS      (10UL * 60UL * 1000UL) // No advertisement for this long: "offline"
#endif
#ifndef MQTT_GOVEE_MS
#define MQTT_GOVEE_MS         60000  // Periodic publish (also published immediately on changes)
#endif

// ============================================================
// PLUGIN: BTHOME
// ============================================================
#ifndef BTHOME_VALUES
#define BTHOME_VALUES         8      // Values per sensor (recognized from the packet)
#endif
#ifndef BTHOME_TIMEOUT_MS
#define BTHOME_TIMEOUT_MS     (10UL * 60UL * 1000UL) // No advertisement for this long: "offline"
#endif
#ifndef MQTT_BTHOME_MS
#define MQTT_BTHOME_MS        60000  // Periodic publish (also published immediately on changes)
#endif

// ============================================================
// PLUGIN: GAS (DIMES, Mopeka Pro, Mopeka Standard)
// ============================================================
#ifndef DEFAULT_CAPACITY
#define DEFAULT_CAPACITY      22.0f  // Gross bottle volume in liters (web UI)
#endif
#ifndef DEFAULT_FILL_STOP_ON
#define DEFAULT_FILL_STOP_ON  true   // Bottle has a mechanical fill stop (web UI)
#endif
#ifndef DEFAULT_FILL_STOP
#define DEFAULT_FILL_STOP     80.0f  // Fill stop in % (web UI)
#endif
#ifndef GAS_DATA_TIMEOUT_MS
#define GAS_DATA_TIMEOUT_MS   (30UL * 60UL * 1000UL) // Gas values older than this are "no data"
#endif
#ifndef MQTT_GAS_MS
#define MQTT_GAS_MS           30000  // Periodic publish (also published immediately on new data)
#endif

// ---- DIMES / SRG WAVE: value mapping -------------------------------------
// Each value is decoded as:
//   value = unsigned integer (LEN bytes at OFFSET, little or big endian) * FACTOR + ADD
// LEVEL must give the raw fill level in % (0-100), BATTERY the battery in %.
// Offsets refer to:
//   SRC_MANUFACTURER : manufacturer data WITHOUT the 2 company ID bytes
//                      (same view as nRF Connect / Home Assistant)
//   SRC_SERVICE_DATA : service data of DIMES_UUID
// The web UI shows the received bytes with their offsets, so other passive BLE
// level sensors can be added by changing this block (e.g. in config_local.h).
#define SRC_MANUFACTURER      0
#define SRC_SERVICE_DATA      1
#define COMPANY_ANY           -1

#ifndef DIMES_MODEL
#define DIMES_MODEL           "DIMES / SRG WAVE"
#endif
#ifndef DIMES_SOURCE
#define DIMES_SOURCE          SRC_MANUFACTURER
#endif
#ifndef DIMES_COMPANY_ID
#define DIMES_COMPANY_ID      0xFFFF   // SRC_MANUFACTURER only (COMPANY_ANY = any)
#endif
#ifndef DIMES_UUID
#define DIMES_UUID            ""       // SRC_SERVICE_DATA only: service data UUID
#endif
#ifndef DIMES_LEVEL_OFFSET
#define DIMES_LEVEL_OFFSET    2
#endif
#ifndef DIMES_LEVEL_LEN
#define DIMES_LEVEL_LEN       1        // 1, 2 or 4 bytes
#endif
#ifndef DIMES_LEVEL_BIG_ENDIAN
#define DIMES_LEVEL_BIG_ENDIAN false
#endif
#ifndef DIMES_LEVEL_FACTOR
#define DIMES_LEVEL_FACTOR    1.0f
#endif
#ifndef DIMES_LEVEL_ADD
#define DIMES_LEVEL_ADD       0.0f
#endif
#ifndef DIMES_BATTERY_ENABLED
#define DIMES_BATTERY_ENABLED true     // false if the sensor does not report a battery level
#endif
#ifndef DIMES_BATTERY_OFFSET
#define DIMES_BATTERY_OFFSET  1
#endif
#ifndef DIMES_BATTERY_LEN
#define DIMES_BATTERY_LEN     1
#endif
#ifndef DIMES_BATTERY_BIG_ENDIAN
#define DIMES_BATTERY_BIG_ENDIAN false
#endif
#ifndef DIMES_BATTERY_FACTOR
#define DIMES_BATTERY_FACTOR  1.0f
#endif
#ifndef DIMES_BATTERY_ADD
#define DIMES_BATTERY_ADD     0.0f
#endif

// ---- Mopeka ---------------------------------------------------------------
// The sensor measures the liquid height in mm. Level % = (height - empty) / (full - empty).
// Typical values: Europe 6 kg 38/336, 11 kg 38/366, 14 kg 38/467, US 20 lb 38/254 mm.
#ifndef DEFAULT_MOPEKA_EMPTY_MM
#define DEFAULT_MOPEKA_EMPTY_MM  38     // Europe 11 kg bottle
#endif
#ifndef DEFAULT_MOPEKA_FULL_MM
#define DEFAULT_MOPEKA_FULL_MM   366
#endif
// Readings below this quality are ignored (0 = none, 1 = low, 2 = medium, 3 = high)
#ifndef MOPEKA_MIN_QUALITY
#define MOPEKA_MIN_QUALITY       2
#endif
#define MOPEKA_COMPANY_ID        0x0059 // Nordic Semiconductor (used by all Mopeka Pro sensors)
#define MOPEKA_STD_COMPANY_ID    0x000D // Texas Instruments (Mopeka Standard)
// Mopeka Standard: the speed of sound depends on the gas mix (web UI, % propane, rest butane)
#ifndef DEFAULT_LPG_PROPANE
#define DEFAULT_LPG_PROPANE      100
#endif

// ============================================================
// PLUGIN: CARAWATER PUMP
// ------------------------------------------------------------
// Protocol taken from the CaraWater ISP 15. Other CaraWater pumps using the same
// app (e.g. ISP 20) probably work too, but are untested. The pump accepts only ONE
// connection at a time - use "Bluetooth für App freigeben" to let the app connect.
// ============================================================
#define PUMP_SERVICE_UUID     "0000ff00-0000-1000-8000-00805f9b34fb"
#define PUMP_NOTIFY_UUID      "0000ff01-0000-1000-8000-00805f9b34fb"
#define PUMP_WRITE_UUID       "0000ff02-0000-1000-8000-00805f9b34fb"
// Alternative layout (other Seaflo based pumps), tried if the one above is not found
#define PUMP_ALT_SERVICE_UUID "0000fff0-0000-1000-8000-00805f9b34fb"
#define PUMP_ALT_NOTIFY_UUID  "0000fff1-0000-1000-8000-00805f9b34fb"
#define PUMP_ALT_WRITE_UUID   "0000fff2-0000-1000-8000-00805f9b34fb"
// Third layout (BLE UART bridge FFE0/FFE1), tried after the two above
#define PUMP_UART_SERVICE_UUID "0000ffe0-0000-1000-8000-00805f9b34fb"
#define PUMP_UART_CHAR_UUID    "0000ffe1-0000-1000-8000-00805f9b34fb"
#ifndef PUMP_OFFLINE_MS
#define PUMP_OFFLINE_MS       10000  // No status frame for 10 s = pump offline
#endif
#ifndef MQTT_PUMP_MS
#define MQTT_PUMP_MS          10000  // Periodic publish (also published on changes)
#endif

// ============================================================
// PLUGIN: RV TECH ROOF HATCH
// ------------------------------------------------------------
// ASCII telegrams over a BLE UART bridge (characteristic FFE1):
//   send "P_d=NNN\r\n" (target position), receive "P_a=NNN\r\n" (reported position)
// Positions 0-100, presets: open 100, closed 0, rain 12, vent 102.
// MAC address and PIN are set in the web UI (not in config_local.h).
// Ported from the Home Assistant integration rvtech-hacs.
// ============================================================
#define RVTECH_SERVICE_UUID   "0000ffe0-0000-1000-8000-00805f9b34fb"
#define RVTECH_CHAR_UUID      "0000ffe1-0000-1000-8000-00805f9b34fb"
#ifndef RVTECH_TARGET_TIMEOUT_MS
#define RVTECH_TARGET_TIMEOUT_MS 60000 // A sent target counts as "moving" for at most 60 s
#endif
#ifndef RVTECH_SEND_WAIT_MS
#define RVTECH_SEND_WAIT_MS   10000  // A command waits this long for a (re)connection
#endif
#ifndef MQTT_RVTECH_MS
#define MQTT_RVTECH_MS        30000
#endif

// ============================================================
// PLUGINS: BATTERIES (Daly, Ective BM X, BM2)
// ------------------------------------------------------------
// Protocols ported from aiobmsble (Apache-2.0), see src/plugins/bms_proto.h.
// Every battery keeps a BLE connection and is polled every "Abfrageintervall" seconds
// (web UI); the BM2 is read from its broadcast while that works and only connects if not.
// All batteries of the node are also published as one JSON list <prefix>/batteries
// for the displays (wall panel, Road Buddy).
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
// PLUGIN: WCS FRIDGE
// ------------------------------------------------------------
// The fridge sends ASCII status messages "/SC0/t0,t1,t2,<temp zone 1>,<temp zone 2>,
// <set zone 1>,<set zone 2>,t7,<voltage x10>,t9,<battery %>,...".
// GATT: own service E530 with the write characteristic E531; every characteristic of the
// service that can notify is subscribed (see plugin_wcs_fridge.cpp). Other UUIDs and an
// optional poll command are set in the web UI; the fridge uses no Bluetooth pairing.
// ============================================================
#ifndef WCS_OFFLINE_MS
#define WCS_OFFLINE_MS        120000 // No status message for 2 min = fridge offline
#endif
#ifndef MQTT_WCS_MS
#define MQTT_WCS_MS           30000
#endif
#ifndef WIPRO_OFFLINE_MS
#define WIPRO_OFFLINE_MS      120000 // No status for 2 min = alarm module offline
#endif
#ifndef MQTT_WIPRO_MS
#define MQTT_WIPRO_MS         30000
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

    char mqtt_server[64];
    uint16_t mqtt_port;
    char mqtt_user[32];
    char mqtt_pass[32];
    char mqtt_prefix[32];

    char web_user[32];     // Web UI login, empty = no login
    char web_pass[32];

    char node_name[24];    // "ble-node", "battery-node" ...: host name, HA device, web UI title
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
