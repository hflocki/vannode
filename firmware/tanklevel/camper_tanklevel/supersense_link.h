// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#ifndef SUPERSENSE_LINK_H
#define SUPERSENSE_LINK_H

#include <Arduino.h>
#include "config.h"

// SuperSense receiver box (up to 4 tank sensors) as source of the fresh and / or grey water
// tank (tank type TANK_TYPE_SUPERSENSE, sensor number 1..4 per tank). One BLE connection per
// box, shared when both tanks use the same box; set up without blocking loop(). Readings land
// in sensorData.water / sensorData.grey (ss_* fields); water_sensor.cpp turns them into
// percent and liters. Protocol: supersense_decode.h.

// A switched-on tank uses a SuperSense sensor (Bluetooth must be started for it)
bool ssUsed();
// Call after gasInit() (Bluetooth is initialized there)
void ssInit();
void ssLoop();
// "Release for the app": stays disconnected so the phone app can connect (tank 0 fresh, 1 grey)
void ssRelease(uint8_t tank, bool on);
bool ssReleased(uint8_t tank);

#endif // SUPERSENSE_LINK_H
