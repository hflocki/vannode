// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#ifndef CT_BMS_PLUGIN_H
#define CT_BMS_PLUGIN_H

// Battery plugins (daly, ective, bm2, plugin_bms.cpp): the shared list for the displays
#include "../../config.h"

#if CT_BMS
#include <stddef.h>
#include <stdint.h>

extern volatile uint32_t bmsListChanges;   // incremented on every new reading / availability change
bool bmsAny();                              // at least one battery device switched on
// JSON list of all batteries for <prefix>/batteries (as battery-node 1.x):
// [{"id","name","ok","t","soc","v","a","w","st","rt","p","pt"}, ...]. Returns the length, 0 = too long.
size_t bmsBatteryList(char* out, size_t n);
#endif

#endif // CT_BMS_PLUGIN_H
