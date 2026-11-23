// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Shared source – edit only in firmware/_common, then run tools/common/sync_common.py
//
// Clock without internet: nodes that need the time of day (display, heater, BLE nodes) get it
// from NTP when there is internet (or from the router as NTP server). Without it the web page
// sets the clock: automatically from the browser (phone / laptop) when the node has no valid
// time, or by hand. Web: GET /api/system carries "clock" (json()), POST /api/time?t=<unix
// seconds>[&m=1] sets it (m=1: entered by hand). A later NTP sync replaces a set time.
// The ESP has no battery backed clock: the time survives a restart, not a power cut.
#ifndef CLOCKSET_H
#define CLOCKSET_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>

namespace clockset {

void begin();                          // after configTime / configTzTime: notices NTP syncs
bool valid();                          // time set at least once (after 2023)
bool set(int64_t unixSeconds, bool manual);   // false: value out of range
void json(JsonObject o);               // {"t": unix seconds, "ok": valid, "src": "ntp" | "browser" | "manual" | ""}

}  // namespace clockset

#endif  // CLOCKSET_H
