// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#pragma once
#include <Arduino.h>

// Optional reed contact on the lid (frame + magnet), wired between a free ESP GPIO and GND.
// The MaxxFan itself reports nothing, so this is the only real feedback about the lid.
namespace lid {
  void begin();
  void loop();                   // call from the main loop

  bool    enabled();             // a pin is configured
  int8_t  state();               // -1 unknown / not installed, 0 closed, 1 open
  const char* text();            // "open" / "closed" / "" (MQTT payload)

  // Called after a close request: logs a warning if the lid is still open after the timeout.
  void expectClosed();

  bool validPin(int pin);        // free, non-strapping GPIO of the ESP32-C3
}
