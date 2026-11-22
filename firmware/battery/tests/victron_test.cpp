// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Host test of victron_decode.h. Reference advertisements, keys and expected values from the
// test suite of keshavdv/victron-ble (Unlicense).
#include <cmath>
#include <cstdio>
#include "../battery_node/src/plugins/victron_decode.h"
#include "aes128_ref.h"

static int fails = 0;
static void check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}
static bool near(float a, float b) { return std::fabs(a - b) < 0.006f; }
static size_t hex(const char* s, uint8_t* out) {
    size_t n = 0;
    for (; s[0] && s[1]; s += 2) {
        unsigned v;
        sscanf(s, "%2x", &v);
        out[n++] = (uint8_t)v;
    }
    return n;
}
static bool decode(const char* adv, const char* keyHex, VicReading& r) {
    uint8_t m[40], key[16], plain[VIC_MAX_DATA];
    size_t n = hex(adv, m), pl = 0;
    uint16_t model;
    uint8_t rec;
    bool mismatch;
    if (!vicParseKey(keyHex, key) || !vicHeader(m, n, model, rec)) return false;
    if (!vicDecrypt(m, n, key, aes128EncryptBlock, plain, pl, mismatch)) return false;
    return vicParse(rec, model, plain, pl, r);
}

int main() {
    // AES-128 reference (FIPS-197 appendix C.1)
    {
        uint8_t k[16], p[16], c[16], e[16];
        hex("000102030405060708090a0b0c0d0e0f", k);
        hex("00112233445566778899aabbccddeeff", p);
        hex("69c4e0d86a7b0430d8cdb78070b4c55a", e);
        aes128EncryptBlock(k, p, c);
        check(!memcmp(c, e, 16), "AES-128 reference block");
    }
    VicReading r;
    // Solar charger: absorption, 13.88 V, 1.4 A, 30 Wh today, 19 W, load 0 A (BlueSolar MPPT 75/15)
    bool ok = decode("100242a0016207adceb37b605d7e0ee21b24df5c", "adeccb947395801a4dd45a2eaa44bf17", r);
    check(ok && r.rec == VIC_REC_SOLAR && r.state == 4 && near(r.voltage, 13.88f) && near(r.current, 1.4f) &&
          near(r.yield_wh, 30) && near(r.power, 19) && near(r.load_a, 0), "Solar charger");
    // Battery monitor: 12.53 V, 0 A, 50 %, -50 Ah, no remaining time, no alarm (SmartShunt 500A)
    ok = decode("100289a302b040af925d09a4d89aa0128bdef48c6298a9", "aff4d0995b7d1e176c0c33ecb9e70dcd", r);
    check(ok && r.rec == VIC_REC_BATTMON && near(r.voltage, 12.53f) && near(r.current, 0) && near(r.soc, 50) &&
          near(r.consumed_ah, -50) && std::isnan(r.remaining_min) && r.alarm == 0, "Battery monitor");
    // DC-DC: off, no error, input 13.15 V, output not available, off reason engine shutdown
    ok = decode("1000c0a304121d64ca8d442b90bbdf6a8cba", "64ba49f1a8562e45197a8e1fe50d7658", r);
    check(ok && r.rec == VIC_REC_DCDC && r.state == 0 && r.error == 0 && near(r.input_v, 13.15f) &&
          std::isnan(r.voltage) && r.off_reason == 0x80, "DC-DC converter");
    check(!strcmp(vicOffReasonName(r.off_reason), "Motor aus"), "DC-DC off reason text");
    // wrong key: key check byte does not match
    {
        uint8_t m[40], key[16], plain[VIC_MAX_DATA];
        size_t n = hex("100289a302b040af925d09a4d89aa0128bdef48c6298a9", m), pl;
        bool mismatch;
        vicParseKey("ffffffffffffffffffffffffffffffff", key);
        check(!vicDecrypt(m, n, key, aes128EncryptBlock, plain, pl, mismatch) && mismatch, "Wrong key detected");
        check(!vicParseKey("12345", key) && !vicParseKey("zz112233445566778899aabbccddeeff", key), "Invalid key text");
    }
    if (fails) { printf("%d test(s) failed\n", fails); return 1; }
    printf("all tests passed\n");
    return 0;
}
