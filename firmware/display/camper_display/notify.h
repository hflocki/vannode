// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#pragma once
#include <Arduino.h>

// Push notifications via Pushover (https://pushover.net) for the warnings shown on the
// overview (temperature alarms, heater errors). The HTTPS request runs in its own task,
// so the display never stalls while a message is sent.
namespace notify {
  void begin();
  void loop();                 // compares the current warnings with the ones already sent
  void requestTest();          // send a test message (from the web page)
  String status();             // last result for the web page, e.g. "OK 12:34" or "Fehler 400"
}
