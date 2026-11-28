// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Connection to a Truma iNet X panel over Bluetooth LE (setting "Anschluss: BLE (Truma iNet X)").
// Instead of LIN wiring the ESP talks to the panel like the phone app: it bonds once ("just
// works", while the panel shows its add-device screen), registers on the panel's bus,
// subscribes to the Alde topics and writes parameters for commands. Runs in its own task;
// the message format is in inetx_proto.*.
//
// EXPERIMENTAL: written from the description of the protocol, not yet tested on a vehicle.
#ifndef INETX_H
#define INETX_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "inetx_proto.h"

// Starts BLE and the connection task (only in the iNet X mode, called once at boot)
void inetxBegin();

// Session with the panel is up (registered, values requested)
bool inetxReady();

// Copy of the values; false while nothing has been received yet
bool inetxSnapshot(InetxModel& out);

// True once after values changed (for publishing)
bool inetxTakeChanged();

// Queue a command (merged with one not yet sent). nullptr = accepted, otherwise a German error text.
const char* inetxControl(const InetxCommand& c);

// Web UI: allow bonding for the next 3 minutes (panel must show its add-device screen)
void inetxPairArm();
// Web UI: delete the stored bond (the panel keeps its side; remove it there too)
void inetxForget();

// State for the web UI and the debug tab
void inetxStatusJson(JsonObject o);

#endif // INETX_H
