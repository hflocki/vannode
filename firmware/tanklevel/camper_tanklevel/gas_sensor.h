// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#ifndef GAS_SENSOR_H
#define GAS_SENSOR_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"

// Gas level: passive BLE advertising sensor (default: Rotarex DIMES / SRG WAVE)

bool gasInit();
void gasLoop();

// Call after the sensor source / MAC / mapping was changed in the web UI
void gasReset();

// Call after tank size or fill stop changed (keeps the last reading)
void gasRecalculate();

// Nearby BLE devices for the web UI (address, name, rssi, company ID, service data UUID)
void gasFillSeenList(JsonArray arr);
void gasPrintSeenList();

#endif // GAS_SENSOR_H
