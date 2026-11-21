// SPDX-License-Identifier: GPL-3.0-only AND MIT
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
// Portions: service UUID and init packet from github.com/adahmen/supersense-homeassistant (MIT, see NOTICE.md)

#ifndef SUPERSENSE_DECODE_H
#define SUPERSENSE_DECODE_H

// Pure decoding of the SuperSense receiver box ("SSBT…", up to 4 tank sensors). No Arduino
// types, so it can be tested on a PC (firmware/ble/tests/supersense_test.cpp). Shared by the
// BLE node (plugin "supersense") and the tanklevel node (tank type SuperSense).
//
// GATT connection, no pairing. Service SS_SERVICE_UUID with three characteristics:
//   SS_WRITE_UUID  write: SS_INIT_PACKET, about 1 s after connecting
//   SS_LEVEL_UUID  read / notify: fill levels of sensor 1..4
//   SS_TEMP_UUID   read / notify: temperatures of sensor 1..4
// Both value characteristics carry 8 bytes = 4 values of 2 bytes, BCD coded (04 27 -> 427),
// one value per sensor channel. 9999 = no sensor on this channel. A packet with any
// non-BCD digit is invalid as a whole.
//   Level: raw 0..2475 = 0..100 % without calibration; with calibration the raw values of
//          "empty" and "full" are taught in (linear in between).
//   Temperature: °C = raw * 0.48876 - 50 (10 bit ADC, 5 V reference, 10 mV/K, 0.5 V at 0 °C).

#include <stddef.h>
#include <stdint.h>

#define SS_SERVICE_UUID  "a0eeb190-6b2a-45fb-966e-d8e49323691f"
#define SS_LEVEL_UUID    "8215dd0f-cc20-4d2a-8b64-73907ec4bd16"
#define SS_TEMP_UUID     "0bd4c5d4-62e3-4946-82ea-d7643834d368"
#define SS_WRITE_UUID    "8d81b2f7-e56f-44bf-94ee-94083f5ea6d7"
// First 16 bits of the two value characteristics (to tell the notifications apart)
#define SS_LEVEL_SRC     0x8215
#define SS_TEMP_SRC      0x0BD4
#define SS_PAYLOAD_LEN   8
#define SS_CHANNELS      4
#define SS_INVALID       9999
#define SS_NONE          (-1)      // channel without sensor (9999 in the packet)
#define SS_RAW_FULL      2475      // raw value for 100 % without calibration
#define SS_INIT_DELAY_MS 1000UL    // wait after connecting before sending the init packet
#define SS_SILENCE_MS    70000UL   // connected but nothing received for this long: reconnect

static const uint8_t SS_INIT_PACKET[7] = {0x0E, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

// Two bytes as 4 BCD digits. Returns -2 for a nibble above 9, SS_NONE for 9999.
static inline int32_t ssBcd(uint8_t hi, uint8_t lo) {
    const uint8_t n[4] = {(uint8_t)(hi >> 4), (uint8_t)(hi & 0x0F), (uint8_t)(lo >> 4), (uint8_t)(lo & 0x0F)};
    int32_t v = 0;
    for (uint8_t d : n) {
        if (d > 9) return -2;
        v = v * 10 + d;
    }
    return v == SS_INVALID ? SS_NONE : v;
}

// Decodes one notification (levels or temperatures) into 4 channel values (SS_NONE = no
// sensor). false = wrong length or a non-BCD digit (packet ignored as a whole).
static inline bool ssDecode(const uint8_t* d, size_t n, int32_t out[SS_CHANNELS]) {
    if (!d || n != SS_PAYLOAD_LEN) return false;
    int32_t v[SS_CHANNELS];
    for (int i = 0; i < SS_CHANNELS; i++) {
        v[i] = ssBcd(d[2 * i], d[2 * i + 1]);
        if (v[i] == -2) return false;
    }
    for (int i = 0; i < SS_CHANNELS; i++) out[i] = v[i];
    return true;
}

// Fill level in % from the raw value; empty / full = taught raw values (full <= empty:
// default 0 / SS_RAW_FULL). Returns -1 for SS_NONE.
static inline float ssPercent(int32_t raw, int32_t empty, int32_t full) {
    if (raw < 0) return -1.0f;
    if (full <= empty) {
        empty = 0;
        full = SS_RAW_FULL;
    }
    float p = (float)(raw - empty) * 100.0f / (float)(full - empty);
    return p < 0.0f ? 0.0f : (p > 100.0f ? 100.0f : p);
}

// Temperature in °C from the raw value; false for SS_NONE
static inline bool ssTemperature(int32_t raw, float& c) {
    if (raw < 0) return false;
    c = raw * 0.4887585532746823f - 50.0f;
    return true;
}

#endif // SUPERSENSE_DECODE_H
