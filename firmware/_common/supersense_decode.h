// SPDX-License-Identifier: GPL-3.0-only AND MIT
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
// Portions: protocol description from github.com/adahmen/supersense-homeassistant (MIT, see NOTICE.md)

#ifndef SUPERSENSE_DECODE_H
#define SUPERSENSE_DECODE_H

// Pure decoding of the SuperSense tank level sensors (Pro, Solid, Laser). No Arduino types,
// so it can be tested on a PC (firmware/ble/tests/supersense_test.cpp). Shared by the
// BLE node (plugin "supersense") and the tanklevel node (tank type SuperSense).
//
// GATT connection, no pairing:
//   1. connect, write SS_INIT_PACKET to SS_WRITE_UUID
//   2. subscribe SS_NOTIFY_UUID, the sensor then notifies 8 bytes on its own
// Payload: 4 values of 2 bytes each, BCD coded (bytes 04 27 -> 427):
//   [0..1] level (unit not confirmed: millilitres according to the source, see below)
//   [2..3] temperature in 0.1 °C
//   [4..5] raw pressure (internal unit)
//   [6..7] status
// 9999 marks an invalid value.
//
// The level unit is not confirmed with a real sensor: 4 BCD digits end at 9999, i.e. only
// 10 L if the unit were millilitres. Both users of this file therefore convert with a
// factor that is set in the web UI (litres per raw unit) and show the raw value.

#include <stddef.h>
#include <stdint.h>

#define SS_SERVICE_UUID  "a0eeb190-6b2a-45fb-966e-d8e49323691f"
#define SS_NOTIFY_UUID   "0bd4c5d4-62e3-4946-82ea-d7643834d368"
#define SS_WRITE_UUID    "8215dd0f-cc20-4d2a-8b64-73907ec4bd16"
#define SS_PAYLOAD_LEN   8
#define SS_INVALID       9999
#define SS_SILENCE_MS    70000UL   // Connected but nothing received for this long: reconnect
#define SS_DEFAULT_FACTOR 0.001f   // Litres per raw unit (source: millilitres)

static const uint8_t SS_INIT_PACKET[7] = {0x0E, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

struct SuperSenseReading {
    bool levelValid;     // false: 9999 or no BCD
    int32_t level;       // raw level value (0..9998)
    bool tempValid;
    float temperature;   // °C
    int32_t pressure;    // raw pressure, -1 = invalid
    int32_t status;      // status value, -1 = invalid
};

// Two bytes as 4 BCD digits. Returns -1 for a nibble above 9 or for the 9999 marker.
static inline int32_t ssBcd(uint8_t hi, uint8_t lo) {
    const uint8_t n[4] = {(uint8_t)(hi >> 4), (uint8_t)(hi & 0x0F), (uint8_t)(lo >> 4), (uint8_t)(lo & 0x0F)};
    int32_t v = 0;
    for (uint8_t d : n) {
        if (d > 9) return -1;
        v = v * 10 + d;
    }
    return v == SS_INVALID ? -1 : v;
}

// Decodes one notification. false = wrong length (not a measurement).
static inline bool ssDecode(const uint8_t* d, size_t n, SuperSenseReading& r) {
    if (!d || n != SS_PAYLOAD_LEN) return false;
    int32_t lv = ssBcd(d[0], d[1]);
    int32_t t = ssBcd(d[2], d[3]);
    r.levelValid = lv >= 0;
    r.level = lv >= 0 ? lv : 0;
    // 0 = no temperature (as in the source); unsigned only, so no frost values below 0 °C
    r.tempValid = t > 0;
    r.temperature = t > 0 ? t / 10.0f : 0.0f;
    r.pressure = ssBcd(d[4], d[5]);
    r.status = ssBcd(d[6], d[7]);
    return true;
}

// Litres from the raw level; factor = litres per raw unit (<= 0 = default)
static inline float ssLiters(int32_t raw, float factor) {
    if (factor <= 0.0f) factor = SS_DEFAULT_FACTOR;
    return raw * factor;
}

#endif // SUPERSENSE_DECODE_H
