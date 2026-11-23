// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Shared source – edit only in firmware/_common, then run tools/common/sync_common.py
//
// Fallback MQTT broker ("Ersatz-Broker"). Normally the node uses its main broker. When that
// cannot be reached for MQTTFB_SWITCH_MS, the node connects to the fallback broker instead
// (usually the display with its built-in broker switched on). While on the fallback it checks
// every MQTTFB_PROBE_MS with a plain TCP connect whether the main broker is back, and then
// returns to it. Empty fallback = off, behaviour as before.
//
// Use in the MQTT reconnect code (PubSubClient or any client with setServer(host, port)):
//   mqttfb::begin(mainHost, mainPort, fallback);   // after the settings are loaded, again after changes
//   if (!client.connected()) {                    // every connect attempt:
//       client.setServer(mqttfb::host(), mqttfb::port());
//       bool ok = client.connect(...);
//       mqttfb::result(ok);
//   } else if (mqttfb::backDue()) client.disconnect();   // main broker is back
//   mqttfb::json(d["mqttfb"].to<JsonObject>());   // GET /api/system, shown by vannode-system.js
//
// fallback: "host" or "host:port" (without port the port of the main broker is used).
#ifndef MQTTFB_H
#define MQTTFB_H

#include <Arduino.h>
#include <ArduinoJson.h>

#ifndef MQTTFB_SWITCH_MS
#define MQTTFB_SWITCH_MS  120000UL   // main broker unreachable this long -> fallback
#endif
#ifndef MQTTFB_PROBE_MS
#define MQTTFB_PROBE_MS   300000UL   // on the fallback: check the main broker this often
#endif

namespace mqttfb {

void        begin(const char* mainHost, uint16_t mainPort, const char* fallback);
const char* host();          // broker for the next connect attempt
uint16_t    port();
void        result(bool connected);   // after every connect attempt
bool        backDue();       // call while connected: true = main broker reachable again, disconnect now
bool        active();        // currently on the fallback broker
bool        configured();    // a fallback broker is set
void        json(JsonObject o);   // {fb, on, since} for the web page

}  // namespace mqttfb

#endif
