// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#ifndef CONFIG_H
#define CONFIG_H

// Defaults of the Pocket-Node. Own values (WiFi, MQTT, login, sensors) go into config_local.h
// next to this file (template config_local.h.example). config_local.h is excluded by .gitignore;
// everything defined there replaces the default below.
// Defines variables: include only once (pocket.ino).
#if __has_include("config_local.h")
#include "config_local.h"
#endif

// WiFi credentials. Empty = placeholder build: the firmware then takes the values stored in flash
// by an earlier build with real values (see below).
#ifndef DEFAULT_WIFI_SSID
#define DEFAULT_WIFI_SSID ""
#endif
#ifndef DEFAULT_WIFI_PASSWORD
#define DEFAULT_WIFI_PASSWORD ""
#endif

// MQTT broker
#ifndef DEFAULT_MQTT_SERVER
#define DEFAULT_MQTT_SERVER ""
#endif
#ifndef DEFAULT_MQTT_PORT
#define DEFAULT_MQTT_PORT 1883
#endif
#ifndef DEFAULT_MQTT_USER
#define DEFAULT_MQTT_USER ""
#endif
#ifndef DEFAULT_MQTT_PASSWORD
#define DEFAULT_MQTT_PASSWORD ""
#endif

// Level sensor topic (pitch, roll, is_level, tolerance, status). Water and gas topics are defined at the top of pocket.ino
#ifndef LEVEL_TOPIC
#define LEVEL_TOPIC "camper/level/#"
#endif

// Mobile signal of the router for the signal bars in the header ("" = WiFi bars of this device only)
#ifndef ROUTER_TOPIC
#define ROUTER_TOPIC "camper/router/mobile"
#endif

// Login for the update page (http://pocket-node.local), always on. Empty = factory login
#ifndef DEFAULT_WEB_USER
#define DEFAULT_WEB_USER "admin"
#endif
#ifndef DEFAULT_WEB_PASSWORD
#define DEFAULT_WEB_PASSWORD "admin"
#endif
#define WEB_USER DEFAULT_WEB_USER
#define WEB_PASS DEFAULT_WEB_PASSWORD

// Note: a build with real WiFi values stores WiFi, MQTT and the web login in flash.
// Firmware from GitHub Actions (built without config_local.h) reads them back from there,
// so it can be installed over the update page. Flash once over USB with your own config_local.h first.

// ---------- Sensors ----------
// Choose which sensors the Pocket-Node shows (1 = enabled, 0 = disabled).
// A disabled sensor gets no page and no MQTT subscription. The system page is always shown.
#ifndef USE_LEVEL
#define USE_LEVEL 1   // pitch / roll level sensor
#endif
#ifndef USE_WATER
#define USE_WATER 1   // fresh water tank
#endif
#ifndef USE_GAS
#define USE_GAS   1   // gas bottle
#endif
#ifndef USE_TEMP
#define USE_TEMP  1   // temperatures from temp-node (sensors 1-4 on one page, 5-8 on a second)
#endif
#ifndef USE_BATTERY
#define USE_BATTERY 1 // batteries from battery-node (up to 3: charge, voltage, power, runtime)
#endif

// Variables used by pocket.ino
const char* ssid = DEFAULT_WIFI_SSID;
const char* password = DEFAULT_WIFI_PASSWORD;
const char* mqtt_server = DEFAULT_MQTT_SERVER;
const int mqtt_port = DEFAULT_MQTT_PORT;
const char* mqtt_user = DEFAULT_MQTT_USER;
const char* mqtt_pass = DEFAULT_MQTT_PASSWORD;
const char* mqtt_topic = LEVEL_TOPIC;

#endif
