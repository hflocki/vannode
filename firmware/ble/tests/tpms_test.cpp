// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Host test of the TireCheck tire sensor decoding (BLE node plugin "tpms"): ./run.sh
#include <cmath>
#include <cstdio>
#include <cstring>
#include "../ble_node/src/plugins/tpms_decode.h"

static int fails = 0;
static void check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}
static bool near(float a, float b) { return std::fabs(a - b) < 0.001f; }

int main() {
    TpmsReading r;
    // Format A: id 11 22 07 88 15 00 -> 00:15:88:07:22:11, 0x0136 = 310 kPa -> 3.10 bar,
    // 0x0C44 = 3140 mV, 0x46 = 70 -> 20 °C
    const uint8_t a[] = {0x11, 0x22, 0x07, 0x88, 0x15, 0x00, 0x36, 0x01, 0x44, 0x0C, 0x46};
    check(tpmsDecode(a, sizeof(a), r), "A: 11 bytes accepted");
    check(strcmp(r.id, "00:15:88:07:22:11") == 0, "A: id = MAC reversed");
    check(near(r.bar, 3.10f) && r.mv == 3140 && r.battery == 100, "A: 3.10 bar, 3140 mV, 100 %");
    check(near(r.temperature, 20.0f), "A: temperature 20 °C");

    // Frost: byte 40 -> -10 °C, low battery
    const uint8_t b[] = {0x11, 0x22, 0x07, 0x88, 0x15, 0x00, 0x00, 0x00, 0x9D, 0x0A, 0x28};
    check(tpmsDecode(b, sizeof(b), r) && near(r.temperature, -10.0f) && near(r.bar, 0.0f), "A: -10 °C, 0 bar");
    check(r.mv == 2717 && r.battery == 23, "A: 2717 mV -> 23 %");
    // 0xFF temperature = invalid, negative pressure rejected
    const uint8_t c[] = {0x11, 0x22, 0x07, 0x88, 0x15, 0x00, 0x36, 0x01, 0x44, 0x0C, 0xFF};
    check(tpmsDecode(c, sizeof(c), r) && std::isnan(r.temperature), "A: 0xFF temperature invalid");
    const uint8_t d[] = {0x11, 0x22, 0x07, 0x88, 0x15, 0x00, 0xFF, 0xFF, 0x44, 0x0C, 0x46};
    check(!tpmsDecode(d, sizeof(d), r), "A: negative pressure rejected");
    check(!tpmsDecode(a, 10, r) && !tpmsDecode(nullptr, 11, r), "A: wrong length / null");

    // Format B (16 bytes after the company id): id = bytes 0..5,
    // 350000 Pa -> 3.50 bar, 2150 -> 21.50 °C, battery 80
    const uint8_t m[] = {0x12, 0x34, 0xAA, 0xBB, 0xCC, 0xDD, 0x30, 0x57, 0x05, 0x00,
                         0x66, 0x08, 0x00, 0x00, 80, 0x00};
    check(tpmsDecodeMfg(m, sizeof(m), r), "B: 16 bytes accepted");
    check(strcmp(r.id, "12:34:AA:BB:CC:DD") == 0, "B: id in MAC order");
    check(near(r.bar, 3.5f) && near(r.temperature, 21.5f) && r.battery == 80, "B: 3.50 bar, 21.5 °C, 80 %");
    check(!tpmsDecodeMfg(m, 15, r), "B: wrong length");

    if (fails) {
        printf("%d test(s) failed\n", fails);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
