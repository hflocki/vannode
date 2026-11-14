// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
#include "config.h"

// One published value. The list VALUES[] in sensor.cpp drives everything else:
// MQTT topics, Home Assistant entities and the tiles of the web UI.
struct ValueDef {
    const char* key;       // MQTT sub topic and id: <prefix>/<key>  (a-z, 0-9, _)
    const char* name;      // Name in Home Assistant and in the web UI
    const char* unit;      // "" = none
    const char* devClass;  // Home Assistant device_class, nullptr = none
    const char* icon;      // mdi icon
    uint8_t decimals;      // Digits after the decimal point (MQTT, web UI)
};

extern const ValueDef VALUES[];
extern const uint8_t VALUE_COUNT;

void sensorInit();
void sensorLoop();

// Publish the current readings right away (web UI button, MQTT command). Safe to call from the web task.
void sensorRequestMeasure();

// Serial command "sig": log every changed LIN signal frame
void sensorToggleSigLog();

// Serial command "read"
void sensorPrint();

#endif // SENSOR_H
