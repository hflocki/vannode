// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
// Portions: GATT layout, key derivation and value layout from
// github.com/cyclemat/HAOS-Truma-Levelcontrol-Integration (GPL-3.0, see NOTICE.md)

#ifndef TRUMA_LC_DECODE_H
#define TRUMA_LC_DECODE_H

// Pure decoding of the Truma LevelControl gas bottle sensor. No Arduino types, so it can be
// tested on a PC (firmware/ble/tests/trumalc_test.cpp).
//
// GATT connection, no pairing. One measurement:
//   1. read TLC_PASSWORD_UUID -> 5 bytes; RC4 key = these bytes XOR "TRUMA"
//   2. write 0x03, then 0x02 (about 0.4 s apart) to TLC_TRIGGER_UUID: start a fresh measurement
//   3. wait about 3 s, read TLC_MEASURE_UUID and decrypt it with RC4
// Decrypted bytes: [0] level in %, [1..2] gas in 100 g (little endian), [3] battery in %,
// [4] temperature in °C, [6] bit 6-7 measurement quality 0..3.
// The sensor advertises the 16-bit service 0x0A00 (older ones manufacturer id 0x5000).

#include <stddef.h>
#include <stdint.h>

#define TLC_SERVICE_UUID   "22bfa601-6afe-4cc5-b3ff-3e140baedeb6"
#define TLC_MEASURE_UUID   "22bfa610-6afe-4cc5-b3ff-3e140baedeb6"   // read / notify
#define TLC_TRIGGER_UUID   "22bfa617-6afe-4cc5-b3ff-3e140baedeb6"   // write
#define TLC_PASSWORD_UUID  "22bfa619-6afe-4cc5-b3ff-3e140baedeb6"   // read, 5 bytes
#define TLC_TRIGGER_INIT   0x03
#define TLC_TRIGGER_GO     0x02
#define TLC_COMPANY_OLD    0x5000
#define TLC_MEASURE_WAIT_MS 3000

struct TlcReading {
    uint8_t level;        // %
    float kg;             // gas left
    uint8_t battery;      // %
    int8_t temperature;   // °C
    int8_t quality;       // 0..3, -1 = not reported
};

// RC4 en-/decryption in place (the same operation both ways)
static inline void tlcRc4(const uint8_t* key, size_t keyLen, uint8_t* data, size_t n) {
    uint8_t s[256];
    for (int i = 0; i < 256; i++) s[i] = (uint8_t)i;
    uint8_t j = 0;
    for (int i = 0; i < 256; i++) {
        j = (uint8_t)(j + s[i] + key[i % keyLen]);
        uint8_t t = s[i]; s[i] = s[j]; s[j] = t;
    }
    uint8_t a = 0, b = 0;
    for (size_t k = 0; k < n; k++) {
        a = (uint8_t)(a + 1);
        b = (uint8_t)(b + s[a]);
        uint8_t t = s[a]; s[a] = s[b]; s[b] = t;
        data[k] ^= s[(uint8_t)(s[a] + s[b])];
    }
}

// RC4 key from the 5-byte password read from the sensor
static inline bool tlcKey(const uint8_t* pw, size_t n, uint8_t key[5]) {
    if (n != 5) return false;
    static const char M[5] = {'T', 'R', 'U', 'M', 'A'};
    for (int i = 0; i < 5; i++) key[i] = pw[i] ^ (uint8_t)M[i];
    return true;
}

// Decrypts the measurement (in place) and decodes it. false = too short or implausible.
static inline bool tlcDecode(const uint8_t key[5], uint8_t* data, size_t n, TlcReading& r) {
    if (n < 5) return false;
    tlcRc4(key, 5, data, n);
    if (data[0] > 100 || data[3] > 100) return false;   // wrong key or no measurement yet
    r.level = data[0];
    r.kg = (uint16_t)(data[1] | (data[2] << 8)) * 0.1f;
    r.battery = data[3];
    r.temperature = (int8_t)data[4];
    r.quality = n > 6 ? (int8_t)((data[6] >> 6) & 3) : -1;
    return true;
}

#endif // TRUMA_LC_DECODE_H
