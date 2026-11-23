// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
#pragma once
// Built-in emergency MQTT broker (off by default, display only).
//
// For a camper without a router that has a broker (or while testing at home): the display
// listens on the MQTT port itself and its own client connects through 127.0.0.1. The other
// nodes then use the display's IP address as their broker.
//
// MQTT 3.1.1 subset, enough for VanNode, Zigbee2MQTT and OpenMQTTGateway:
//   CONNECT with user/password (the display's MQTT login; empty = no login), keep alive,
//   last will (also retained), PUBLISH QoS 0/1/2 in (acknowledged), always QoS 0 out,
//   retained messages (kept in PSRAM, lost on restart), SUBSCRIBE / UNSUBSCRIBE with + and #,
//   PINGREQ, DISCONNECT. No persistent sessions, no queue for clients that are away.
// Runs entirely in the AsyncTCP task (event driven, no own task, no polling in loop()).
#include <Arduino.h>
#include <ArduinoJson.h>

namespace broker {

void begin(uint16_t port, const String& user, const String& pass);   // only when switched on
bool running();
void json(JsonObject o);    // {on, port, clients, retained, kb, msgs}
String summary();           // one line for the system page, "" = off

}  // namespace broker
