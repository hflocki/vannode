// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
#include "config.h"

bool sensorInit();
void sensorLoop();

// Tare = current position becomes "level". All of them save the config.
void sensorSetTare();
void sensorResetTare();
void sensorSetTareValues(float pitch, float roll);

// Soft reset of the BNO085 (e.g. if it hangs); no calibration data is lost
void sensorRestart();

// Recalculate the wedges after the vehicle size or tolerance changed
void sensorRecalc();

const char* mountName(MountOrientation m);
bool mountFromName(const String& name, MountOrientation& out);

#endif // SENSOR_H
