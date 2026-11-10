// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#pragma once
#include <Arduino.h>

struct Settings {
  String   hostname;

  String   wifiSsid;
  String   wifiPass;
  bool     staticIp = false;
  String   ip, gateway, subnet, dns;
  String   apPass;

  String   mqttHost;
  uint16_t mqttPort = 1883;
  String   mqttUser;
  String   mqttPass;
  String   mqttPrefix;
  bool     discovery = true;

  String   webUser;          // empty = no login
  String   webPass;

  bool     portEnabled[2] = {false, true};
  uint16_t pulseMs = 150;
  uint8_t  syncPort = 1;
};

extern Settings settings;

void settingsLoad();
void settingsSave();
void settingsFactoryReset();
