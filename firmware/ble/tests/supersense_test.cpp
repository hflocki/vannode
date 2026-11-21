// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Host test of the SuperSense box decoding (BLE node plugin "supersense" and tanklevel node): ./run.sh
#include <cmath>
#include <cstdio>
#include "../../_common/supersense_decode.h"

static int fails = 0;
static void check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}
static bool near(float a, float b, float tol = 0.01f) { return std::fabs(a - b) < tol; }

int main() {
    int32_t v[SS_CHANNELS];
    // Channel 1 = 1237 (BCD 12 37), 2 = 0, 3 and 4 = no sensor
    const uint8_t a[] = {0x12, 0x37, 0x00, 0x00, 0x99, 0x99, 0x99, 0x99};
    check(ssDecode(a, sizeof(a), v), "8 bytes accepted");
    check(v[0] == 1237 && v[1] == 0 && v[2] == SS_NONE && v[3] == SS_NONE, "4 channels, 9999 = none");

    // Default scale: 2475 = 100 %, 1237 ~ 50 %
    check(near(ssPercent(2475, 0, 0), 100.0f) && near(ssPercent(1237, 0, 0), 49.98f), "default 0..2475");
    check(near(ssPercent(3000, 0, 0), 100.0f) && near(ssPercent(0, 0, 0), 0.0f), "clamped");
    check(ssPercent(SS_NONE, 0, 0) < 0, "no sensor -> -1");
    // Taught: empty 200, full 2200 -> 1200 = 50 %
    check(near(ssPercent(1200, 200, 2200), 50.0f), "taught empty / full");
    check(near(ssPercent(100, 200, 2200), 0.0f), "below empty -> 0");

    // Temperature: raw 143 -> 143 * 0.48876 - 50 = 19.89 °C
    float t = 0;
    check(ssTemperature(143, t) && near(t, 19.89f), "temperature 143 -> 19.9 °C");
    check(!ssTemperature(SS_NONE, t), "temperature none");

    // A non-BCD digit invalidates the whole packet
    const uint8_t bad[] = {0x12, 0x37, 0x0A, 0x00, 0x99, 0x99, 0x99, 0x99};
    check(!ssDecode(bad, sizeof(bad), v), "non-BCD packet rejected");
    check(!ssDecode(a, 7, v) && !ssDecode(nullptr, 8, v), "wrong length / null rejected");
    check(ssBcd(0x98, 0x76) == 9876, "bcd 9876");

    if (fails) {
        printf("%d test(s) failed\n", fails);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
