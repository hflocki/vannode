// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#ifndef CT_MQTT_HA_H
#define CT_MQTT_HA_H

#include <Arduino.h>
#include "../../config.h"
#include "plugin.h"

void mqttInit();
void mqttLoop();
bool mqttIsConnected();

// Re-send the discovery on the next loop (e.g. after a plugin setting changed entities)
void mqttRequestDiscovery();

// Device removed in the web UI: delete its Home Assistant entities and retained values
void mqttForget(Plugin* p);
// Home Assistant node ID derived from the MQTT prefix ("camper/tool" -> "camper_tool")
const char* mqttNodeId();

#endif // CT_MQTT_HA_H
