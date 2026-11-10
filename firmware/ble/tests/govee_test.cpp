// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Host test of the Govee decoding (plugin "govee", no ESP32 needed): ./run.sh
#include <cmath>
#include <cstdio>
#include "../ble_node/src/plugins/govee_decode.h"

static int fails = 0;
static void check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}
static bool near(float a, float b) { return std::fabs(a - b) < 0.01f; }

int main() {
    GoveeReading r;
    // H5075: 0x034A7C = 215676 -> 21.5 °C, 67.6 %, battery 100 %
    const uint8_t h5075[] = {0x00, 0x03, 0x4A, 0x7C, 0x64, 0x00};
    check(goveeDecode(0xEC88, h5075, sizeof(h5075), r) && near(r.temperature, 21.5f) && near(r.humidity, 67.6f) &&
              r.battery == 100, "H5075 21.5 °C / 67.6 %");
    // H5075 below zero: bit 23 + 52345 -> -5.2 °C, 34.5 %
    const uint8_t h5075neg[] = {0x00, 0x80, 0xCC, 0x79, 0x37, 0x00};
    check(goveeDecode(0xEC88, h5075neg, sizeof(h5075neg), r) && near(r.temperature, -5.2f) && near(r.humidity, 34.5f) &&
              r.battery == 55, "H5075 -5.2 °C / 34.5 %");
    // H5101: [2..4] = 0x01EB4E = 125774 -> 12.5 °C, 77.4 %, battery 0x5A = 90 %
    const uint8_t h5101[] = {0x01, 0x01, 0x01, 0xEB, 0x4E, 0x5A};
    check(goveeDecode(0x0001, h5101, sizeof(h5101), r) && near(r.temperature, 12.5f) && near(r.humidity, 77.4f) &&
              r.battery == 90, "H5101 12.5 °C / 77.4 %");
    // H5074: temp 0xFF38 = -200 -> -2.00 °C, hum 0x1388 = 5000 -> 50.00 %, battery 80
    const uint8_t h5074[] = {0x00, 0x38, 0xFF, 0x88, 0x13, 0x50, 0x02};
    check(goveeDecode(0xEC88, h5074, sizeof(h5074), r) && near(r.temperature, -2.0f) && near(r.humidity, 50.0f) &&
              r.battery == 80, "H5074 -2.00 °C / 50.00 %");
    // H5179: temp 0x0866 = 2150 -> 21.50 °C, hum 0x1770 = 6000 -> 60.00 %, battery 99
    const uint8_t h5179[] = {0x01, 0x02, 0x03, 0x04, 0x66, 0x08, 0x70, 0x17, 0x63};
    check(goveeDecode(0x8801, h5179, sizeof(h5179), r) && near(r.temperature, 21.5f) && near(r.humidity, 60.0f) &&
              r.battery == 99, "H5179 21.50 °C / 60.00 %");
    // Rejected: wrong length, other company, implausible humidity (value % 1000 > 1000 impossible,
    // but a garbage H5074 packet with 120 % is)
    check(!goveeDecode(0xEC88, h5075, 5, r), "short packet rejected");
    check(!goveeDecode(0x004C, h5075, sizeof(h5075), r), "Apple beacon rejected");
    const uint8_t bad[] = {0x00, 0x00, 0x01, 0xE0, 0x2E, 0x50, 0x00};   // hum 0x2EE0 = 120.00 %
    check(!goveeDecode(0xEC88, bad, sizeof(bad), r), "humidity 120 % rejected");
    check(goveeCompany(0xEC88) && goveeCompany(0x0001) && !goveeCompany(0x0059), "company ids");

    if (fails) {
        printf("%d test(s) failed\n", fails);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
