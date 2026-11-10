// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#pragma once
#include <Arduino.h>
#include <vector>

// LVGL user interface on the panel
namespace ui {
  void splash(const char* step);        // start screen with logo; call again to change the step text
  void begin();
  void loop();                          // refresh values, header, pending commands
  void requestRebuild();                // pages / tiles / builtin pages changed
  void showPageByName(const String& nameOrIndex);
  String warningText();                 // all active warnings, one per line ("" = none)
  String warningText(std::vector<uint8_t>& cats);   // same, cats = kind (WarnCat) of each line
}
