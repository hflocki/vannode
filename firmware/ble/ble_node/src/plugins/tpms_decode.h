// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#ifndef TPMS_DECODE_H
#define TPMS_DECODE_H

// Pure decoding of the SuperSense TireCheck tire sensors (passive advertisements). No Arduino
// types, so it can be tested on a PC (firmware/ble/tests/tpms_test.cpp). Two formats:
//
// A) Service data, 16-bit UUID 0xFAF0 or 0xEEFD, 11 bytes:
//   [0..5]  sensor id (byte order reversed compared to the MAC)
//   [6..7]  pressure, little endian, kPa (0.01 bar); negative = invalid
//   [8..9]  battery voltage in mV, little endian
//   [10]    temperature + 50 °C (0xFF = invalid)
// B) Manufacturer data, company id 0x0100, 16 bytes after the company id (older sensors):
//   [0..5]  sensor id in MAC byte order
//   [6..9]  pressure, int32 little endian, Pa
//   [10..13] temperature, int32 little endian, 0.01 °C
//   [14]    battery level 0..100 (coded, converted to an approximate voltage)

#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>

#define TPMS_SVC_UUID16   0xFAF0
#define TPMS_SVC_UUID16_B 0xEEFD
#define TPMS_PAYLOAD_LEN  11
#define TPMS_MFG_ID       0x0100
#define TPMS_MFG_LEN      16
#define TPMS_TEMP_OFFSET  50

struct TpmsReading {
    char id[18];        // "00:15:88:07:67:8D" (sensor MAC, upper case)
    float bar;          // pressure in bar (gauge pressure as sent by the sensor)
    float temperature;  // °C
    uint16_t mv;        // battery voltage in mV
    int8_t battery;     // battery in %
};

// Battery in % from the voltage of format A (rough: 2.6 V empty, 3.1 V full, coin cell)
static inline int tpmsBatteryPercent(uint16_t mv) {
    if (mv >= 3100) return 100;
    if (mv <= 2600) return 0;
    return (int)((mv - 2600) * 100 / 500);
}

// Decodes the 11 service data bytes (format A, without the UUID). false = wrong length or
// invalid pressure. Temperature NAN if invalid.
static inline bool tpmsDecode(const uint8_t* d, size_t n, TpmsReading& r) {
    if (!d || n != TPMS_PAYLOAD_LEN) return false;
    int16_t kpa = (int16_t)(d[6] | (d[7] << 8));
    if (kpa < 0) return false;
    snprintf(r.id, sizeof(r.id), "%02X:%02X:%02X:%02X:%02X:%02X", d[5], d[4], d[3], d[2], d[1], d[0]);
    r.bar = kpa / 100.0f;
    r.mv = (uint16_t)(d[8] | (d[9] << 8));
    r.temperature = d[10] == 0xFF ? NAN : (float)((int)d[10] - TPMS_TEMP_OFFSET);
    r.battery = (int8_t)tpmsBatteryPercent(r.mv);
    return true;
}

static inline int32_t tpmsLe32(const uint8_t* p) {
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

// Format B: the 16 manufacturer data bytes WITHOUT the 2 company id bytes
static inline bool tpmsDecodeMfg(const uint8_t* f, size_t n, TpmsReading& r) {
    if (!f || n != TPMS_MFG_LEN) return false;
    int32_t pa = tpmsLe32(f + 6);
    if (pa < 0) return false;
    snprintf(r.id, sizeof(r.id), "%02X:%02X:%02X:%02X:%02X:%02X", f[0], f[1], f[2], f[3], f[4], f[5]);
    r.bar = pa / 100000.0f;
    r.temperature = tpmsLe32(f + 10) / 100.0f;
    // Battery level byte -> approximate voltage (piecewise, as the sensor codes it)
    uint8_t b = f[14];
    float x, base;
    if (b <= 4) { x = b * 224.0f / 4.0f; base = 1136.0f; }
    else if (b <= 28) { x = (b - 4) * 224.0f / 24.0f; base = 1360.0f; }
    else { x = (b - 28) * 121.0f / 72.0f; base = 1584.0f; }
    r.mv = (uint16_t)((x + base) / 2.0f / 1023.0f * 3.6f * 1000.0f + 0.5f);
    r.battery = (int8_t)(b > 100 ? 100 : b);
    return true;
}


#endif // TPMS_DECODE_H
