// Shared source – edit only in firmware/_common, then run tools/common/sync_common.py
//
// Diagnostics of a device, published retained as <prefix>/info, e.g.
// {"version":"1.2.0","hw":"esp32c3","ip":"192.168.8.31","mac":"AA:BB:CC:DD:EE:FF","rssi":-61,
//  "uptime":3605,"reset":"power on","heap":182344}
// "hw" is the hardware name of the release file (FW_HW in fw_config.h).
#ifndef DEVINFO_H
#define DEVINFO_H

#include <Arduino.h>

#ifndef DEVINFO_INTERVAL_MS
#define DEVINFO_INTERVAL_MS 60000UL   // resend at least this often
#endif

// JSON payload for the info topic
String devinfoJson(const char* version);

// True right after (re)connecting and then every DEVINFO_INTERVAL_MS.
// Call once per loop with the current MQTT connection state.
bool devinfoDue(bool connected);

#endif // DEVINFO_H
