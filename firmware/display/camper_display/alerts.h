// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#pragma once
#include <Arduino.h>
#include <vector>

// Built-in limit warnings (fresh water, gas, frost) and the hysteresis helper used by all
// threshold warnings, so a value hovering around a limit does not switch a warning on and off.
// Limits and hysteresis are in the settings (web page: Einstellungen > Dienste > Warnungen).
namespace alerts {

// Hysteresis per warning key: switches on when `on` is true and off again only when `off`
// is true. Returns the current state. Thread safe.
bool latch(const String& key, bool on, bool off);

// Same for "value below limit": on below `limit`, off at or above `limit + hyst`.
// A limit of 0 (off) or NAN value clears the key and returns false.
bool below(const String& key, float value, float limit, float hyst);
// "value above limit": on above `limit`, off at or below `limit - hyst`
bool above(const String& key, float value, float limit, float hyst);

// One line per active warning, "Frischwasser niedrig: 8 %" ...; cats (optional) gets the
// warning kind (WarnCat) of each line
String warnings(std::vector<uint8_t>* cats = nullptr);

// Heater fault as text like the CP Plus: "E 122 H: Kraftstoffmangel (...)", "" = none or heater
// offline. Codes without a known kind: "Fehler 212: ..." ("Warnung" while the heater reports
// operating status "warning").
String heaterFault();

// Awning: a switch on the display / web page / MQTT, or an own contact sensor (settings.awnSrc)
bool   awningIsSwitch();      // true = switch, false = contact sensor
int8_t awning();              // -1 unknown, 0 in, 1 out
void   setAwning(bool out);   // switch mode only: stores the state and publishes it

}  // namespace alerts
