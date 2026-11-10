// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#pragma once
#include <Arduino.h>

namespace wifimgr {
  void   begin();
  void   loop();
  bool   apActive();
  bool   staConnected();
  String apSsid();
  String macSuffix();
  String ipString();
  int    rssi();
  String netString();   // static IP settings as one string (WiFi trial, safeguard.h)
}
