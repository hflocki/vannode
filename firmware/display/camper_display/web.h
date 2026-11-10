// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#pragma once
#include <Arduino.h>

// Web server: serves the page, a WebSocket for live values / commands and
// the settings API. The page shows the same pages and tiles as the panel.
namespace web {
  void begin();
  void loop();                 // dispatch queued commands, push value changes
  void configChanged();        // tell connected browsers to reload the page layout
}
