// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#ifndef TPMS_DECODE_H
#define TPMS_DECODE_H

// Pure decoding of the SuperSense TireCheck tire sensors (passive advertisements). No Arduino
// types, so it can be tested on a PC (firmware/ble/tests/tpms_test.cpp).
//
// Service data, 16-bit UUID 0xFAF0, 11 bytes:
//   [0..5]  sensor id = its MAC in reverse byte order
//   [6..7]  pressure, little endian, 0.01 bar
//   [8..9]  battery voltage in mV, little endian
//   [10]    temperature + 50 °C

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define TPMS_SVC_UUID16   0xFAF0
#define TPMS_PAYLOAD_LEN  11
#define TPMS_TEMP_OFFSET  50

struct TpmsReading {
    char id[18];        // "00:15:88:07:67:8D" (sensor MAC, upper case)
    float bar;          // pressure in bar (gauge pressure as sent by the sensor)
    float temperature;  // °C
    uint16_t mv;        // battery voltage in mV
};

// Decodes the 11 service data bytes (without the UUID). false = wrong length.
static inline bool tpmsDecode(const uint8_t* d, size_t n, TpmsReading& r) {
    if (!d || n != TPMS_PAYLOAD_LEN) return false;
    snprintf(r.id, sizeof(r.id), "%02X:%02X:%02X:%02X:%02X:%02X", d[5], d[4], d[3], d[2], d[1], d[0]);
    r.bar = (uint16_t)(d[6] | (d[7] << 8)) / 100.0f;
    r.mv = (uint16_t)(d[8] | (d[9] << 8));
    r.temperature = (float)((int)d[10] - TPMS_TEMP_OFFSET);
    return true;
}

// Battery in % from the voltage (rough: 2.6 V empty, 3.1 V full, coin cell)
static inline int tpmsBatteryPercent(uint16_t mv) {
    if (mv >= 3100) return 100;
    if (mv <= 2600) return 0;
    return (int)((mv - 2600) * 100 / 500);
}

#endif // TPMS_DECODE_H
