// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Runtime settings stored in NVS (Preferences).
#pragma once

#include <Arduino.h>

struct Settings {
  // WiFi
  String wifi_ssid;
  String wifi_pass;
  String hostname;
  String ap_pass;
  String ap_ssid;                // empty = <hostname>-XXXX
  bool ap_always = false;        // false = AP only without WiFi (fallback)
  uint8_t wifi_tx_power = 0;     // 0.25 dBm units, 0 = default of the board profile
  // Static IP, all empty = DHCP
  String static_ip;
  String gateway;
  String subnet;
  String dns;

  // MQTT
  String mqtt_host;
  uint16_t mqtt_port = 1883;
  String mqtt_user;
  String mqtt_pass;
  String mqtt_base;
  bool ha_discovery = true;
  String ha_prefix;

  // Time
  String ntp_server;
  String tz;

  // Web UI protection (empty = open)
  String web_user;
  String web_pass;

  // Heater
  String heater_model;           // key from heater_model.h, e.g. "combi_d"
  uint16_t decoke_minutes = 30;
  float room_setpoint = 20.0f;   // last used room temperature, used when switching on
  String fan_mode;               // last used fan mode when switching on (eco/high/boost, VarioHeat night/auto/boost)

  uint8_t log_level = 2;       // 0=error .. 3=debug
  uint8_t mqtt_log_level = 1;  // lines up to this level go to <base>/log, 4 = off
};

extern Settings g_settings;

void settings_load();
void settings_save();
// Only the heater preferences, cheap and called on user changes.
void settings_save_heater();
