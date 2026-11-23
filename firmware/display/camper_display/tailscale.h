// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
#pragma once
// Tailscale on the display (beta, off by default).
//
// Puts the display into a tailnet with microlink (src/microlink, MIT, an independent
// Tailscale-compatible client for ESP32). The web page, the WebSocket and the emergency
// broker are then also reachable under the display's 100.x.y.z address from anywhere.
// Only the display itself, no subnet router: the other nodes stay reachable through the
// router / an own subnet router as before.
//
// Settings (settings.tsOn / tsName / tsKey, NVS) take effect after a restart, so microlink
// is started once per boot and never stopped at run time. Sign-in either with an auth key
// (stored on the device only) or interactively: the status then carries the login link.
// microlink keeps the device keys in its own NVS namespaces ("microlink", "ml_peers");
// forget() erases them on the next start (sign out, factory reset).
#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"   // WITH_TAILSCALE (0 in config_local.h leaves microlink out of the firmware)

namespace ts {

void begin();             // in setup(), after the settings are loaded
void loop();              // in loop(): starts microlink once WiFi is up
void json(JsonObject o);  // {built, on, code, ip, peers, url, name, nodekey, heap, block, stk}; code -1 = not running
String ip();              // tailnet address, "" = not connected
String name();            // device name in the tailnet (setting or hostname)
bool needsLogin();        // waiting for the interactive sign-in
void forget();            // erase the device keys on the next start (sign out)

}  // namespace ts
