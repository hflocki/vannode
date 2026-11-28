// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#pragma once

// Optional personal overrides, not in git (see config_local.h.example). Everything defined there
// wins over the defaults in this file, e.g. own passwords or WiFi / MQTT for a freshly flashed device.
#if __has_include("config_local.h")
#include "config_local.h"
#endif
// =====================================================================
//  Display Node - Guition ESP32-4848S040 (4", 480x480, ESP32-S3)
//              or ESP32-8048S070C (7", 800x480, ESP32-S3, capacitive touch)
//  Wall panel + web page, both fed live from the MQTT broker
// =====================================================================

// ---- Board ---------------------------------------------------------------
// 1 = Guition ESP32-4848S040 (4", 480 x 480, default)
// 2 = ESP32-8048S070C (7", 800 x 480 landscape, GT911); the release build "esp32s3-7" sets it
#define BOARD_4848S040          1
#define BOARD_8048S070          2
#ifndef DISPLAY_BOARD
#define DISPLAY_BOARD           BOARD_4848S040
#endif
#if DISPLAY_BOARD != BOARD_4848S040 && DISPLAY_BOARD != BOARD_8048S070
#error "DISPLAY_BOARD: 1 = ESP32-4848S040, 2 = ESP32-8048S070C"
#endif
// Hardware name of the release file (display-node-<FW_HW>-v<version>.bin), see fw_config.h
#if DISPLAY_BOARD == BOARD_8048S070 && !defined(FW_HW)
#define FW_HW                   "esp32s3-7"
#endif

#define FW_NAME                 "display-node"
// Language of the web page: "de" (default) or "en". Set it in config_local.h before the
// first flash; the browser then shows the page in that language (vannode-i18n.js).
// The panel itself stays German.
#ifndef UI_LANG
#define UI_LANG                 "de"
#endif
#define FW_VERSION              "2.28.0"
#define FW_DEVICE               "display"   // device id in the firmware marker (fwcheck.h)
#ifndef WIFI_TRIAL_MS
#define WIFI_TRIAL_MS           50000   // a changed WiFi must connect this fast, else the old one returns
#endif

// ---- Device / access point ------------------------------------------
#ifndef STATIC_IP_FALLBACK_MS
#define STATIC_IP_FALLBACK_MS   180000  // static IP, WiFi ok but no MQTT this long -> DHCP until restart
#endif
#ifndef DEV_TILE_COLS
// sensor tiles per row on Sensoren / Energie / Extra (4": 3 .. 5, 5 = names wrap to two short lines; 7": up to 8)
#if DISPLAY_BOARD == BOARD_8048S070
#define DEV_TILE_COLS           6
#else
#define DEV_TILE_COLS           4
#endif
#endif
#ifndef DEFAULT_WEB_TILES
// home of the web page: fixed slots per group (Tanks 4, Klima 4, Temperatur 2, Energie 2, Fahrzeug 4), "none" = empty
#define DEFAULT_WEB_TILES       "water,grey,gas,none,heater,fan,heki,weather,temp,none,bat,none,level,actors,lights,switches"
#endif
// Home grid of the panel: columns x rows (4" panel: 3 x 3; 7" panel: 5 x 3, same tile size)
#ifndef OV_COLS
#if DISPLAY_BOARD == BOARD_8048S070
#define OV_COLS                 5
#else
#define OV_COLS                 3
#endif
#endif
#ifndef OV_ROWS
#define OV_ROWS                 3
#endif
#define OV_SLOTS                (OV_COLS * OV_ROWS)

#ifndef DEFAULT_OV_TILES
#if DISPLAY_BOARD == BOARD_8048S070
#define DEFAULT_OV_TILES        "water,grey,gas,heater,level,fan,temp,bat,weather,status"   // 7": two full rows of 5
#else
#define DEFAULT_OV_TILES        "water,grey,gas,heater,level,fan,temp,bat,status"   // overview tiles in order
#endif
#endif
#ifndef MAX_LINKS
#define MAX_LINKS               16      // device links on the web page (name + URL)
#endif
#ifndef DEFAULT_DISPLAY_NAME
#define DEFAULT_DISPLAY_NAME    ""                  // name shown in the header (web UI: Einstellungen)
#endif
#ifndef DEFAULT_HOSTNAME
#define DEFAULT_HOSTNAME        "display-node"
#endif
#ifndef AP_SSID_PREFIX
#define AP_SSID_PREFIX          "Display-Node-"   // + last 6 hex digits of the MAC
#endif
#ifndef DEFAULT_AP_PASSWORD
#define DEFAULT_AP_PASSWORD     "display1234"       // min. 8 characters - CHANGE ME
#endif
#ifndef DEFAULT_OTA_PASSWORD
#define DEFAULT_OTA_PASSWORD    "display1234"       // Arduino IDE network port - CHANGE ME
#endif
#ifndef WIFI_AP_FALLBACK_MS
#define WIFI_AP_FALLBACK_MS     60000               // open setup AP after 1 min without WiFi
#endif

// ---- MQTT ------------------------------------------------------------
#ifndef DEFAULT_MQTT_PORT
#define DEFAULT_MQTT_PORT       1883
#endif
// Stacks of the own tasks in bytes (internal RAM), measured use plus about 3 kB reserve
// (the Tailscale card shows the reserve). Weather: HTTPS + JSON, was 12 kB; Pushover: HTTPS, was 8 kB.
#ifndef WX_TASK_STACK
#define WX_TASK_STACK           (9 * 1024)
#endif
#ifndef PUSH_TASK_STACK
#define PUSH_TASK_STACK         (6 * 1024 + 512)
#endif
// Tailscale (tailscale.h): switched on in the web UI, off by default.
// 0 leaves microlink out of the firmware (saves flash; the settings card then says so).
#ifndef WITH_TAILSCALE
#define WITH_TAILSCALE          1
#endif
#ifndef DEFAULT_PREFIX_SELF
#define DEFAULT_PREFIX_SELF     "camper/display"
#endif
#ifndef DEFAULT_PREFIX_TANK
#define DEFAULT_PREFIX_TANK     "camper/tanklevel"
#endif
#ifndef DEFAULT_PREFIX_LEVEL
#define DEFAULT_PREFIX_LEVEL    "camper/level"
#endif
#ifndef DEFAULT_PREFIX_HEATER
#define DEFAULT_PREFIX_HEATER   "camper/heater"
#endif
#ifndef DEFAULT_PREFIX_FAN
#define DEFAULT_PREFIX_FAN      "camper/maxxfan"
#endif
#ifndef DEFAULT_PREFIX_BLE
#define DEFAULT_PREFIX_BLE  "camper/ble"   // BLE node (roof hatch: <prefix>/rvtech/...); ble2 / ble3: web UI
#endif
#ifndef DEFAULT_PREFIX_TEMP
#define DEFAULT_PREFIX_TEMP     "camper/temp"
#endif
#ifndef DEFAULT_PREFIX_ALDE
#define DEFAULT_PREFIX_ALDE     "camper/alde"
#endif
#ifndef DEFAULT_PREFIX_IO
#define DEFAULT_PREFIX_IO       "camper/io"
#endif
#ifndef DEFAULT_PREFIX_BATTERY
#define DEFAULT_PREFIX_BATTERY  "camper/battery"
#endif

// ---- Battery warnings (battery-node) ------------------------------------
#ifndef BAT_WARN_SOC
#define BAT_WARN_SOC            20      // warn below this state of charge in % (monitors with current sensor)
#endif

// ---- Weather (Open-Meteo, no API key) ----------------------------------------
// Factory defaults, changeable on the web page (Einstellungen -> Wetter). A position switches the weather on.
#ifndef DEFAULT_WX_LAT
#define DEFAULT_WX_LAT          ""      // e.g. "52.52"
#endif
#ifndef DEFAULT_WX_LON
#define DEFAULT_WX_LON          ""      // e.g. "13.40"
#endif
#ifndef DEFAULT_WX_NAME
#define DEFAULT_WX_NAME         ""
#endif
#ifndef DEFAULT_RT_TOPIC
#define DEFAULT_RT_TOPIC        "camper/router/mobile"   // mobile signal of the router (RUT956 Data to Server, GSM)
#endif
#ifndef DEFAULT_WX_TOPIC
#define DEFAULT_WX_TOPIC        ""      // MQTT topic with the GPS position, e.g. from the router
#endif
// Built-in warnings (web page: Einstellungen > Dienste > Warnungen), 0 = off
#ifndef DEFAULT_WARN_WATER
#define DEFAULT_WARN_WATER      10      // fresh water below this level in %
#endif
#ifndef DEFAULT_WARN_GAS
#define DEFAULT_WARN_GAS        15      // gas bottle below this level in %
#endif
#ifndef DEFAULT_WARN_FROST
#define DEFAULT_WARN_FROST      3       // frost warning below this outside temperature in °C (off by default)
#endif

#ifndef DEFAULT_AWNING_GUST
#define DEFAULT_AWNING_GUST     40      // awning out: warn from these gusts (km/h), 0 = off
#endif

// Departure check (web page: Einstellungen > Dienste > Warnungen > Abfahrtscheck)
#ifndef DEFAULT_DEPART_TOPIC
#define DEFAULT_DEPART_TOPIC    "camper/rut"    // router JSON with GPS.speed (km/h) and GPS.fix_status, "" = off
#endif
#ifndef DEFAULT_DOOR_TOPIC
#define DEFAULT_DOOR_TOPIC      "camper/door"   // door: RUT956 digital input JSON or plain open/closed, "" = none
#endif
#ifndef DEFAULT_DEPART_SPEED
#define DEFAULT_DEPART_SPEED    5       // driving above this speed in km/h ...
#endif
#ifndef DEFAULT_DEPART_SECS
#define DEFAULT_DEPART_SECS     10      // ... for this many seconds
#endif

#ifndef DEFAULT_WX_GUST
#define DEFAULT_WX_GUST         60      // warn from these gusts (km/h), 0 = off
#endif

// ---- History (24 h curves on the web page) ---------------------------
#ifndef HISTORY_STEP_S
#define HISTORY_STEP_S          300     // one sample every 5 minutes
#endif
#ifndef HISTORY_POINTS
#define HISTORY_POINTS          288     // 288 x 5 min = 24 h
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
// Static IP (optional): set DEFAULT_STATIC_IP to use it, "" = DHCP.
// Simpler: keep DHCP and reserve the address for the MAC in the router.
#ifndef DEFAULT_STATIC_IP
#define DEFAULT_STATIC_IP       ""
#endif
#ifndef DEFAULT_GATEWAY
#define DEFAULT_GATEWAY         ""
#endif
#ifndef DEFAULT_SUBNET
#define DEFAULT_SUBNET          "255.255.255.0"
#endif
#ifndef DEFAULT_DNS
#define DEFAULT_DNS             ""                  // "" = gateway
#endif
#ifndef DEFAULT_MQTT_SERVER
#define DEFAULT_MQTT_SERVER     ""
#endif
#ifndef DEFAULT_MQTT_USER
#define DEFAULT_MQTT_USER       ""
#endif
#ifndef DEFAULT_MQTT_PASSWORD
#define DEFAULT_MQTT_PASSWORD   ""
#endif
#ifndef DEFAULT_WEB_USER
#define DEFAULT_WEB_USER        "admin" // login is always on; empty input keeps the stored value
#endif
#ifndef DEFAULT_WEB_PASSWORD
#define DEFAULT_WEB_PASSWORD    "admin"
#endif
#ifndef DEFAULT_FAN_PORT
#define DEFAULT_FAN_PORT        1                   // VanTurtle port 1 = RJ45
#endif
#ifndef DEFAULT_FINDER_FILTER
#define DEFAULT_FINDER_FILTER   "zigbee2mqtt/#"
#endif
// ---- Pushover (push messages for warnings) --------------------------
// Token of your Pushover application and your user key. Can also be entered in the
// web page (Einstellungen -> Pushover); these are only the factory defaults.
#ifndef DEFAULT_PUSHOVER_TOKEN
#define DEFAULT_PUSHOVER_TOKEN  ""
#endif
#ifndef DEFAULT_PUSHOVER_USER
#define DEFAULT_PUSHOVER_USER   ""
#endif
#ifndef PUSHOVER_URL
#define PUSHOVER_URL            "https://api.pushover.net/1/messages.json"
#endif
#ifndef PUSHOVER_CANCEL_URL
#define PUSHOVER_CANCEL_URL     "https://api.pushover.net/1/receipts/cancel_by_tag/"   // + tag + ".json"
#endif
#ifndef PUSHOVER_HOST
#define PUSHOVER_HOST           "api.pushover.net"   // must match PUSHOVER_URL
#endif
#ifndef PUSHOVER_STABLE_MS
#define PUSHOVER_STABLE_MS      30000   // a warning must be active this long before it is sent
#endif
#ifndef PUSHOVER_RETRIES
#define PUSHOVER_RETRIES        5       // attempts per message (no WiFi / server error)
#endif
#ifndef PUSHOVER_RETRY_MS
#define PUSHOVER_RETRY_MS       60000
#endif
#ifndef PUSHOVER_QUEUE
#define PUSHOVER_QUEUE          8
#endif

#ifndef MQTT_BUFFER_SIZE
#define MQTT_BUFFER_SIZE        4096                // larger messages are dropped by PubSubClient
#endif
#ifndef MQTT_RECONNECT_MS
#define MQTT_RECONNECT_MS       5000
#endif
#ifndef SELF_PUBLISH_MS
#define SELF_PUBLISH_MS         60000
#endif

// ---- Time --------------------------------------------------------------
#ifndef DEFAULT_NTP_SERVER
#define DEFAULT_NTP_SERVER      "pool.ntp.org"
#endif
#ifndef DEFAULT_TZ
#define DEFAULT_TZ              "CET-1CEST,M3.5.0,M10.5.0/3"   // Europe/Berlin
#endif

// ---- Pages / tiles (user extensible) ----------------------------------
#ifndef MAX_PAGES
#define MAX_PAGES               8
#endif
#ifndef MAX_TILES
#define MAX_TILES               40
#endif

// ---- Display hardware -----------------------------------------------------
#if DISPLAY_BOARD == BOARD_8048S070
// ESP32-8048S070C: 7" 800 x 480 RGB panel (no init sequence needed) + GT911 touch.
// Pins and timing as in the Arduino_GFX device list (ESP32_8048S070).
#ifndef LCD_WIDTH
#define LCD_WIDTH               800
#endif
#ifndef LCD_HEIGHT
#define LCD_HEIGHT              480
#endif
#ifndef LCD_BL_PIN
#define LCD_BL_PIN              2
#endif
#ifndef LCD_DE
#define LCD_DE                  41
#endif
#ifndef LCD_VSYNC
#define LCD_VSYNC               40
#endif
#ifndef LCD_HSYNC
#define LCD_HSYNC               39
#endif
#ifndef LCD_PCLK
#define LCD_PCLK                42
#endif
// Some board revisions have red and blue swapped: LCD_SWAP_RB 1 if red shows as blue
#ifndef LCD_SWAP_RB
#define LCD_SWAP_RB             0
#endif
#if LCD_SWAP_RB
#define LCD_RB_A0 15
#define LCD_RB_A1 7
#define LCD_RB_A2 6
#define LCD_RB_A3 5
#define LCD_RB_A4 4
#define LCD_RB_B0 14
#define LCD_RB_B1 21
#define LCD_RB_B2 47
#define LCD_RB_B3 48
#define LCD_RB_B4 45
#else
#define LCD_RB_A0 14
#define LCD_RB_A1 21
#define LCD_RB_A2 47
#define LCD_RB_A3 48
#define LCD_RB_A4 45
#define LCD_RB_B0 15
#define LCD_RB_B1 7
#define LCD_RB_B2 6
#define LCD_RB_B3 5
#define LCD_RB_B4 4
#endif
#ifndef LCD_R0
#define LCD_R0 LCD_RB_A0
#endif
#ifndef LCD_R1
#define LCD_R1 LCD_RB_A1
#endif
#ifndef LCD_R2
#define LCD_R2 LCD_RB_A2
#endif
#ifndef LCD_R3
#define LCD_R3 LCD_RB_A3
#endif
#ifndef LCD_R4
#define LCD_R4 LCD_RB_A4
#endif
#ifndef LCD_G0
#define LCD_G0 9
#endif
#ifndef LCD_G1
#define LCD_G1 46
#endif
#ifndef LCD_G2
#define LCD_G2 3
#endif
#ifndef LCD_G3
#define LCD_G3 8
#endif
#ifndef LCD_G4
#define LCD_G4 16
#endif
#ifndef LCD_G5
#define LCD_G5 1
#endif
#ifndef LCD_B0
#define LCD_B0 LCD_RB_B0
#endif
#ifndef LCD_B1
#define LCD_B1 LCD_RB_B1
#endif
#ifndef LCD_B2
#define LCD_B2 LCD_RB_B2
#endif
#ifndef LCD_B3
#define LCD_B3 LCD_RB_B3
#endif
#ifndef LCD_B4
#define LCD_B4 LCD_RB_B4
#endif
#ifndef LCD_PCLK_HZ
#define LCD_PCLK_HZ             12000000
#endif
// Sync timing: polarity, front porch, pulse width, back porch
#ifndef LCD_HSYNC_TIMING
#define LCD_HSYNC_TIMING        0, 180, 30, 16
#endif
#ifndef LCD_VSYNC_TIMING
#define LCD_VSYNC_TIMING        0, 12, 13, 10
#endif
#ifndef LCD_PCLK_NEG
#define LCD_PCLK_NEG            1
#endif
// Bounce buffer in internal RAM (pixels, 0 = off). Against flicker / shifted picture when WiFi
// and PSRAM are busy, e.g. LCD_WIDTH * 10; costs 2 x 2 bytes per pixel of internal RAM.
#ifndef LCD_BOUNCE_PX
#define LCD_BOUNCE_PX           0
#endif
#ifndef TOUCH_SDA
#define TOUCH_SDA               19
#endif
#ifndef TOUCH_SCL
#define TOUCH_SCL               20
#endif
#ifndef TOUCH_RST
#define TOUCH_RST               38      // -1 = not wired
#endif

#else   // BOARD_4848S040
// Guition ESP32-4848S040: ST7701S RGB panel (init over 3-wire SPI) + GT911 touch
#ifndef LCD_WIDTH
#define LCD_WIDTH               480
#endif
#ifndef LCD_HEIGHT
#define LCD_HEIGHT              480
#endif
#ifndef LCD_BL_PIN
#define LCD_BL_PIN              38
#endif
#ifndef LCD_SPI_CS
#define LCD_SPI_CS              39
#endif
#ifndef LCD_SPI_SCK
#define LCD_SPI_SCK             48
#endif
#ifndef LCD_SPI_MOSI
#define LCD_SPI_MOSI            47
#endif
#ifndef LCD_DE
#define LCD_DE                  18
#endif
#ifndef LCD_VSYNC
#define LCD_VSYNC               17
#endif
#ifndef LCD_HSYNC
#define LCD_HSYNC               16
#endif
#ifndef LCD_PCLK
#define LCD_PCLK                21
#endif
#ifndef LCD_R0
#define LCD_R0 11
#endif
#ifndef LCD_R1
#define LCD_R1 12
#endif
#ifndef LCD_R2
#define LCD_R2 13
#endif
#ifndef LCD_R3
#define LCD_R3 14
#endif
#ifndef LCD_R4
#define LCD_R4 0
#endif
#ifndef LCD_G0
#define LCD_G0 8
#endif
#ifndef LCD_G1
#define LCD_G1 20
#endif
#ifndef LCD_G2
#define LCD_G2 3
#endif
#ifndef LCD_G3
#define LCD_G3 46
#endif
#ifndef LCD_G4
#define LCD_G4 9
#endif
#ifndef LCD_G5
#define LCD_G5 10
#endif
#ifndef LCD_B0
#define LCD_B0 4
#endif
#ifndef LCD_B1
#define LCD_B1 5
#endif
#ifndef LCD_B2
#define LCD_B2 6
#endif
#ifndef LCD_B3
#define LCD_B3 7
#endif
#ifndef LCD_B4
#define LCD_B4 15
#endif
#ifndef LCD_PCLK_HZ
#define LCD_PCLK_HZ             12000000
#endif
// ST7701 init sequence: 9 = current Guition panels, 1 = older batches.
// If the picture stays black / has wrong colours, try the other one.
#ifndef LCD_INIT_TYPE
#define LCD_INIT_TYPE           9
#endif

#ifndef LCD_HSYNC_TIMING
#define LCD_HSYNC_TIMING        1, 10, 8, 50
#endif
#ifndef LCD_VSYNC_TIMING
#define LCD_VSYNC_TIMING        1, 10, 8, 20
#endif
#ifndef LCD_PCLK_NEG
#define LCD_PCLK_NEG            0
#endif
#ifndef LCD_BOUNCE_PX
#define LCD_BOUNCE_PX           0
#endif
#ifndef TOUCH_SDA
#define TOUCH_SDA               19
#endif
#ifndef TOUCH_SCL
#define TOUCH_SCL               45
#endif
#ifndef TOUCH_RST
#define TOUCH_RST               -1
#endif
#endif  // DISPLAY_BOARD

// 0..3, rotates drawing and touch together (wall mount orientation).
// 7" panel: only 0 or 2 (the pages are laid out for landscape)
#ifndef LCD_ROTATION
#define LCD_ROTATION            0
#endif
#if DISPLAY_BOARD == BOARD_8048S070 && (LCD_ROTATION & 1)
#error "LCD_ROTATION: the 7\" panel only supports 0 or 2"
#endif

#ifndef TOUCH_I2C_HZ
#define TOUCH_I2C_HZ            400000
#endif
// Touch axis correction if taps land in the wrong place
#ifndef TOUCH_SWAP_XY
#define TOUCH_SWAP_XY           0
#endif
#ifndef TOUCH_INVERT_X
#define TOUCH_INVERT_X          0
#endif
#ifndef TOUCH_INVERT_Y
#define TOUCH_INVERT_Y          0
#endif

// ---- Display behaviour defaults (editable in the web UI) ---------------
#ifndef DEFAULT_BRIGHTNESS
#define DEFAULT_BRIGHTNESS      80      // %
#endif
#ifndef DEFAULT_DIM_BRIGHTNESS
#define DEFAULT_DIM_BRIGHTNESS  10      // % after timeout, 0 = screen off
#endif
#ifndef DEFAULT_SCREEN_TIMEOUT
#define DEFAULT_SCREEN_TIMEOUT  120     // s, 0 = never dim
#endif

// LVGL draw buffer: lines per partial buffer (two buffers)
#ifndef LV_BUF_LINES
#define LV_BUF_LINES            48
#endif
#ifndef LV_BUF_IN_PSRAM
#define LV_BUF_IN_PSRAM         1       // 0 = internal RAM (slightly faster drawing, but HTTPS / Pushover then lacks memory)
#endif
