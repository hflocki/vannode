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
    // id 11 22 07 88 15 00 -> 00:15:88:07:22:11, 0x0136 = 310 -> 3.10 bar, 0x0C44 = 3140 mV, 0x46 = 70 -> 20 °C
    const uint8_t a[] = {0x11, 0x22, 0x07, 0x88, 0x15, 0x00, 0x36, 0x01, 0x44, 0x0C, 0x46};
    check(tpmsDecode(a, sizeof(a), r), "11 bytes accepted");
    check(strcmp(r.id, "00:15:88:07:22:11") == 0, "id = MAC reversed");
    check(near(r.bar, 3.10f), "pressure 3.10 bar");
    check(r.mv == 3140, "battery 3140 mV");
    check(near(r.temperature, 20.0f), "temperature 20 °C");

    // Frost: byte 40 -> -10 °C, low battery
    const uint8_t b[] = {0x11, 0x22, 0x07, 0x88, 0x15, 0x00, 0x00, 0x00, 0x9D, 0x0A, 0x28};
    check(tpmsDecode(b, sizeof(b), r) && near(r.temperature, -10.0f) && near(r.bar, 0.0f), "-10 °C, 0 bar");
    check(r.mv == 2717, "battery 2717 mV");

    check(tpmsBatteryPercent(3140) == 100 && tpmsBatteryPercent(2500) == 0 && tpmsBatteryPercent(2850) == 50, "battery percent");
    check(!tpmsDecode(a, 10, r), "10 bytes rejected");
    check(!tpmsDecode(nullptr, 11, r), "null rejected");

    if (fails) {
        printf("%d test(s) failed\n", fails);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
