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
