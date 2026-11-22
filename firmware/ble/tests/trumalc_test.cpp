// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Host test of truma_lc_decode.h (Truma LevelControl)
#include <cmath>
#include <cstdio>
#include <cstring>
#include "../ble_node/src/plugins/truma_lc_decode.h"

static int fails = 0;
static void check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

int main() {
    // RC4 reference: key "Key", plaintext "Plaintext" -> BB F3 16 E8 D9 40 AF 0A D3
    {
        uint8_t d[9];
        memcpy(d, "Plaintext", 9);
        tlcRc4((const uint8_t*)"Key", 3, d, 9);
        const uint8_t e[9] = {0xBB, 0xF3, 0x16, 0xE8, 0xD9, 0x40, 0xAF, 0x0A, 0xD3};
        check(!memcmp(d, e, 9), "RC4 reference");
    }
    // Key = password XOR "TRUMA"
    const uint8_t pw[5] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t key[5];
    check(tlcKey(pw, 5, key) && key[0] == ('T' ^ 1) && key[4] == ('A' ^ 5), "key from password");
    check(!tlcKey(pw, 4, key), "short password rejected");
    tlcKey(pw, 5, key);
    // 55 %, 110 x 100 g = 11.0 kg, battery 87 %, 18 C, quality 2
    uint8_t plain[7] = {55, 110, 0, 87, 18, 0, 0x80};
    uint8_t d[7];
    memcpy(d, plain, 7);
    tlcRc4(key, 5, d, 7);                       // what the sensor sends
    TlcReading r;
    check(tlcDecode(key, d, 7, r) && r.level == 55 && std::fabs(r.kg - 11.0f) < 0.01f && r.battery == 87 &&
          r.temperature == 18 && r.quality == 2, "measurement decoded");
    // negative temperature, no quality byte
    uint8_t p2[5] = {3, 0x2C, 0x01, 40, (uint8_t)-7};
    memcpy(d, p2, 5);
    tlcRc4(key, 5, d, 5);
    check(tlcDecode(key, d, 5, r) && std::fabs(r.kg - 30.0f) < 0.01f && r.temperature == -7 && r.quality == -1,
          "frost, 30 kg, no quality");
    // wrong key -> implausible values are rejected (at least most of the time)
    memcpy(d, plain, 7);
    tlcRc4(key, 5, d, 7);
    uint8_t wrong[5] = {0x10, 0x20, 0x30, 0x40, 0x50};
    check(!tlcDecode(wrong, d, 7, r), "wrong key rejected");
    check(!tlcDecode(key, d, 4, r), "short answer rejected");
    if (fails) { printf("%d test(s) failed\n", fails); return 1; }
    printf("all tests passed\n");
    return 0;
}
