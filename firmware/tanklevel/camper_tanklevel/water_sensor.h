// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#ifndef WATER_SENSOR_H
#define WATER_SENSOR_H

#include <Arduino.h>
#include "config.h"

// Fresh and grey water level: Votronic analog tank senders on ADC_PIN / GREY_ADC_PIN.
// Only the tanks enabled in the config are read.
bool waterInit();
void waterLoop();

#endif // WATER_SENSOR_H
