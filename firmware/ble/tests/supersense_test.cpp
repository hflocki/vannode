// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Host test of the SuperSense decoding (BLE node plugin "supersense" and tanklevel node): ./run.sh
#include <cmath>
#include <cstdio>
#include "../../_common/supersense_decode.h"

static int fails = 0;
static void check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}
static bool near(float a, float b) { return std::fabs(a - b) < 0.001f; }

int main() {
    SuperSenseReading r;
    // 04 27 -> 427 (BCD, not 0x0427 = 1063), 02 35 -> 23.5 °C, 01 50 -> 150, 00 01 -> 1
    const uint8_t a[] = {0x04, 0x27, 0x02, 0x35, 0x01, 0x50, 0x00, 0x01};
    check(ssDecode(a, sizeof(a), r) && r.levelValid && r.level == 427, "level 0427 -> 427");
    check(r.tempValid && near(r.temperature, 23.5f), "temperature 0235 -> 23.5 °C");
    check(r.pressure == 150 && r.status == 1, "pressure and status");
    check(near(ssLiters(r.level, 0.001f), 0.427f), "litres with default factor");
    check(near(ssLiters(r.level, 0.1f), 42.7f), "litres with factor 0.1");
    check(near(ssLiters(1000, 0.0f), 1.0f), "factor 0 = default");

    // 9999 = invalid level, 0 temperature = none
    const uint8_t inv[] = {0x99, 0x99, 0x00, 0x00, 0x99, 0x99, 0x99, 0x99};
    check(ssDecode(inv, sizeof(inv), r) && !r.levelValid && !r.tempValid, "9999 / 0 -> invalid");
    check(r.pressure == -1 && r.status == -1, "9999 pressure / status -> -1");

    // Nibble above 9 is not BCD
    const uint8_t bad[] = {0x0A, 0x27, 0x02, 0x35, 0x00, 0x00, 0x00, 0x00};
    check(ssDecode(bad, sizeof(bad), r) && !r.levelValid, "non-BCD level rejected");

    // Wrong length
    check(!ssDecode(a, 7, r), "7 bytes rejected");
    check(!ssDecode(nullptr, 8, r), "null rejected");
    check(ssBcd(0x98, 0x76) == 9876, "bcd 9876");

    if (fails) {
        printf("%d test(s) failed\n", fails);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
