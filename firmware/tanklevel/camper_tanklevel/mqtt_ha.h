// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#ifndef MQTT_HA_H
#define MQTT_HA_H

#include <Arduino.h>
#include "config.h"

void mqttInit();
void mqttLoop();
bool mqttIsConnected();
void mqttPublishDiscovery();

// Re-send the discovery on the next loop
void mqttRequestDiscovery();

#endif // MQTT_HA_H
