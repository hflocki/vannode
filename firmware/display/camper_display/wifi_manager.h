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
  String ipString();       // STA IP if connected, otherwise AP IP
  int    rssi();
  bool   timeValid();
  // Static IP check: ip, gateway and subnet valid and the gateway inside the subnet. err = reason
  bool   staticIpValid(const String& ip, const String& gw, const String& sn, String& err);
  bool   dhcpFallback();   // static IP did not work, running with DHCP until the next restart
}
