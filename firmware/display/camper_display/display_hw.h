// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#pragma once
#include <Arduino.h>

// Panel, backlight, GT911 touch and the LVGL driver glue.
namespace disp {
  bool begin();
  void loop();                    // runs LVGL timers + screen timeout
  void setBrightness(uint8_t pct);
  void wake();
  void sleep();                   // dim / off according to settings
  bool isAwake();
  bool touchOk();
}
