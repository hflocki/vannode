// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Host test of the BTHome v2 decoding (plugin "bthome", no ESP32 needed): ./run.sh
#include <cmath>
#include <cstdio>
#include <cstring>
#include "../ble_node/src/plugins/bthome_decode.h"
#include "../ble_node/src/plugins/bthome_crypt.h"

static int fails = 0;
static void check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}
static bool near(float a, float b) { return std::fabs(a - b) < 0.001f; }

// Small AES-128 (encryption only) for the CCM test; on the ESP32 mbedtls does this
static const uint8_t SBOX[256] = {
  0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
  0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
  0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
  0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
  0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
  0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
  0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
  0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16};
struct Aes { uint8_t rk[176]; };
static void aesInit(Aes& a, const uint8_t key[16]) {
    static const uint8_t rcon[10] = {1, 2, 4, 8, 16, 32, 64, 128, 0x1b, 0x36};
    memcpy(a.rk, key, 16);
    for (int i = 4; i < 44; i++) {
        uint8_t t[4];
        memcpy(t, a.rk + (i - 1) * 4, 4);
        if (i % 4 == 0) {
            uint8_t x = t[0];
            t[0] = SBOX[t[1]] ^ rcon[i / 4 - 1]; t[1] = SBOX[t[2]]; t[2] = SBOX[t[3]]; t[3] = SBOX[x];
        }
        for (int k = 0; k < 4; k++) a.rk[i * 4 + k] = a.rk[(i - 4) * 4 + k] ^ t[k];
    }
}
static uint8_t xt(uint8_t x) { return (uint8_t)((x << 1) ^ ((x & 0x80) ? 0x1b : 0)); }
static void aesBlock(void* ctx, const uint8_t in[16], uint8_t out[16]) {
    const Aes& a = *(const Aes*)ctx;
    uint8_t s[16];
    for (int i = 0; i < 16; i++) s[i] = in[i] ^ a.rk[i];
    for (int r = 1; r <= 10; r++) {
        uint8_t t[16];
        for (int i = 0; i < 16; i++) t[i] = SBOX[s[i]];
        for (int c = 0; c < 4; c++)   // shift rows
            for (int row = 0; row < 4; row++) s[c * 4 + row] = t[((c + row) % 4) * 4 + row];
        if (r < 10)
            for (int c = 0; c < 4; c++) {   // mix columns
                uint8_t* p = s + c * 4;
                uint8_t a0 = p[0], a1 = p[1], a2 = p[2], a3 = p[3], all = a0 ^ a1 ^ a2 ^ a3;
                p[0] ^= all ^ xt(a0 ^ a1); p[1] ^= all ^ xt(a1 ^ a2); p[2] ^= all ^ xt(a2 ^ a3); p[3] ^= all ^ xt(a3 ^ a0);
            }
        for (int i = 0; i < 16; i++) s[i] ^= a.rk[r * 16 + i];
    }
    memcpy(out, s, 16);
}

int main() {
    BthValue v[12];
    size_t n;
    // pvvx (ATC firmware) in BTHome mode: packet id, battery 100 %, 22.50 °C, 50.55 %, 2.854 V
    const uint8_t pvvx[] = {0x40, 0x00, 0xA5, 0x01, 0x64, 0x02, 0xCA, 0x08, 0x03, 0xBF, 0x13, 0x0C, 0x26, 0x0B};
    check(bthParse(pvvx, sizeof(pvvx), v, 12, n) == BTH_OK && n == 4, "pvvx: 4 values (packet id skipped)");
    check(v[0].id == 0x01 && near(v[0].v, 100), "battery 100 %");
    check(v[1].id == 0x02 && near(v[1].v, 22.5f), "temperature 22.50 °C");
    check(v[2].id == 0x03 && near(v[2].v, 50.55f), "humidity 50.55 %");
    check(v[3].id == 0x0C && near(v[3].v, 2.854f), "voltage 2.854 V");

    // Below zero (sint16): 0xFF38 = -200 -> -2.00 °C; 0x45 sint16 * 0.1: 0xFFF6 -> -1.0
    const uint8_t neg[] = {0x40, 0x02, 0x38, 0xFF, 0x45, 0xF6, 0xFF};
    check(bthParse(neg, sizeof(neg), v, 12, n) == BTH_OK && n == 2 && near(v[0].v, -2.0f) && near(v[1].v, -1.0f),
          "negative temperatures");

    // Shelly BLU Button (trigger based): battery, button press
    const uint8_t btn[] = {0x44, 0x00, 0x12, 0x01, 0x5A, 0x3A, 0x01};
    check(bthParse(btn, sizeof(btn), v, 12, n) == BTH_OK && n == 2 && v[1].id == 0x3A &&
              !strcmp(bthEventText(v[1].id, v[1].v), "press"), "button press");
    check(!strcmp(bthEventText(0x3A, 0x04), "long_press") && !strcmp(bthEventText(0x3A, 0), ""), "event texts");

    // Shelly BLU Door/Window: battery, illuminance (uint24 * 0.01), window open, rotation
    const uint8_t door[] = {0x44, 0x00, 0x01, 0x01, 0x64, 0x05, 0x10, 0x27, 0x00, 0x2D, 0x01, 0x3F, 0x84, 0x03};
    check(bthParse(door, sizeof(door), v, 12, n) == BTH_OK && n == 4 && near(v[1].v, 100.0f) && v[2].id == 0x2D &&
              near(v[2].v, 1) && near(v[3].v, 90.0f), "door/window: lux, open, rotation 90°");

    // Two temperatures in one packet
    const uint8_t two[] = {0x40, 0x02, 0xCA, 0x08, 0x02, 0x10, 0x0E};
    check(bthParse(two, sizeof(two), v, 12, n) == BTH_OK && n == 2 && near(v[1].v, 36.0f), "two temperatures");

    // Text object with length byte is skipped, parsing goes on
    const uint8_t txt[] = {0x40, 0x53, 0x03, 'a', 'b', 'c', 0x01, 0x32};
    check(bthParse(txt, sizeof(txt), v, 12, n) == BTH_OK && n == 1 && near(v[0].v, 50), "text skipped");

    // Errors
    const uint8_t enc[] = {0x41, 0x01, 0x64};
    check(bthParse(enc, sizeof(enc), v, 12, n) == BTH_ENCRYPTED, "encrypted rejected");
    const uint8_t v1[] = {0x02, 0x01, 0x64};
    check(bthParse(v1, sizeof(v1), v, 12, n) == BTH_WRONG_VERSION, "not version 2");
    const uint8_t unk[] = {0x40, 0x02, 0xCA, 0x08, 0xEE, 0x01};
    check(bthParse(unk, sizeof(unk), v, 12, n) == BTH_UNKNOWN_OBJECT && n == 1 && near(v[0].v, 22.5f),
          "unknown object: values before it kept");
    const uint8_t cut[] = {0x40, 0x02, 0xCA};
    check(bthParse(cut, sizeof(cut), v, 12, n) == BTH_TOO_SHORT && n == 0, "cut packet");
    check(bthParse(pvvx, sizeof(pvvx), v, 2, n) == BTH_OK && n == 2, "max respected");


    // Encrypted BTHome: example of bthome.io/encryption (key 231d39c1..., MAC 54:48:E6:8F:80:A5)
    {
        Aes ctx;
        uint8_t key[16], mac[6];
        check(bthKeyBytes("231d39c1d7cc1ab1aee224cd096db932", key) && bthMacBytes("54:48:E6:8F:80:A5", mac), "key / MAC parsing");
        uint8_t aesOut[16];
        const uint8_t fipsKey[16] = {0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
        const uint8_t fipsIn[16] = {0x32,0x43,0xf6,0xa8,0x88,0x5a,0x30,0x8d,0x31,0x31,0x98,0xa2,0xe0,0x37,0x07,0x34};
        const uint8_t fipsOut[16] = {0x39,0x25,0x84,0x1d,0x02,0xdc,0x09,0xfb,0xdc,0x11,0x85,0x97,0x19,0x6a,0x0b,0x32};
        aesInit(ctx, fipsKey);
        aesBlock(&ctx, fipsIn, aesOut);
        check(!memcmp(aesOut, fipsOut, 16), "test AES (FIPS-197 example)");
        aesInit(ctx, key);
        const uint8_t enc[] = {0x41, 0xe4, 0x45, 0xf3, 0xc9, 0x96, 0x2b, 0x33, 0x22, 0x11, 0x00, 0x6c, 0x7c, 0x45, 0x19};
        uint8_t plain[32];
        size_t pl = 0;
        uint32_t cnt = 0;
        bool ok = bthDecrypt(aesBlock, &ctx, mac, enc, sizeof(enc), plain, sizeof(plain), pl, cnt);
        check(ok && pl == 7 && cnt == 0x00112233, "decrypt: MIC ok, counter 0x00112233");
        check(bthParse(plain, pl, v, 12, n) == BTH_OK && n == 2 && near(v[0].v, 25.06f) && near(v[1].v, 50.55f),
              "decrypted values 25.06 °C / 50.55 %");
        uint8_t bad[sizeof(enc)];
        memcpy(bad, enc, sizeof(enc));
        bad[2] ^= 1;
        check(!bthDecrypt(aesBlock, &ctx, mac, bad, sizeof(bad), plain, sizeof(plain), pl, cnt), "changed byte: MIC wrong");
        uint8_t wrongKey[16];
        bthKeyBytes("231d39c1d7cc1ab1aee224cd096db933", wrongKey);
        aesInit(ctx, wrongKey);
        check(!bthDecrypt(aesBlock, &ctx, mac, enc, sizeof(enc), plain, sizeof(plain), pl, cnt), "wrong key: MIC wrong");
        check(!bthKeyBytes("231d39c1", key) && !bthKeyBytes("231d39c1d7cc1ab1aee224cd096db93g", key), "bad keys refused");
    }
    if (fails) {
        printf("%d test(s) failed\n", fails);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
