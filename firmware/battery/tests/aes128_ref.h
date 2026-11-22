// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Plain AES-128 block encryption (FIPS-197) for the host tests only. The ESP32 uses mbedtls.
#ifndef AES128_REF_H
#define AES128_REF_H
#include <stdint.h>
#include <string.h>

static uint8_t aesXtime(uint8_t x) { return (uint8_t)((x << 1) ^ ((x & 0x80) ? 0x1B : 0)); }

static uint8_t aesSbox(uint8_t x) {
    // multiplicative inverse in GF(2^8), then the affine transform
    uint8_t inv = 0;
    if (x) {
        for (int c = 1; c < 256; c++) {
            uint8_t a = x, b = (uint8_t)c, p = 0;
            while (b) { if (b & 1) p ^= a; a = aesXtime(a); b >>= 1; }
            if (p == 1) { inv = (uint8_t)c; break; }
        }
    }
    uint8_t s = inv;
    for (int i = 1; i < 5; i++) s ^= (uint8_t)((inv << i) | (inv >> (8 - i)));
    return s ^ 0x63;
}

static bool aes128EncryptBlock(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]) {
    uint8_t sbox[256];
    for (int i = 0; i < 256; i++) sbox[i] = aesSbox((uint8_t)i);
    uint8_t rk[176];
    memcpy(rk, key, 16);
    uint8_t rcon = 1;
    for (int i = 16; i < 176; i += 4) {
        uint8_t t[4] = {rk[i - 4], rk[i - 3], rk[i - 2], rk[i - 1]};
        if (i % 16 == 0) {
            uint8_t u = t[0];
            t[0] = (uint8_t)(sbox[t[1]] ^ rcon); t[1] = sbox[t[2]]; t[2] = sbox[t[3]]; t[3] = sbox[u];
            rcon = aesXtime(rcon);
        }
        for (int k = 0; k < 4; k++) rk[i + k] = rk[i - 16 + k] ^ t[k];
    }
    uint8_t s[16];
    for (int i = 0; i < 16; i++) s[i] = in[i] ^ rk[i];
    for (int round = 1; round <= 10; round++) {
        uint8_t t[16];
        for (int i = 0; i < 16; i++) t[i] = sbox[s[i]];
        for (int c = 0; c < 4; c++)                       // shift rows (column-major state)
            for (int r = 0; r < 4; r++) s[c * 4 + r] = t[((c + r) % 4) * 4 + r];
        if (round < 10) {
            for (int c = 0; c < 4; c++) {                 // mix columns
                uint8_t* a = s + c * 4;
                uint8_t x = a[0] ^ a[1] ^ a[2] ^ a[3], a0 = a[0];
                a[0] ^= x ^ aesXtime(a[0] ^ a[1]);
                a[1] ^= x ^ aesXtime(a[1] ^ a[2]);
                a[2] ^= x ^ aesXtime(a[2] ^ a[3]);
                a[3] ^= x ^ aesXtime(a[3] ^ a0);
            }
        }
        for (int i = 0; i < 16; i++) s[i] ^= rk[round * 16 + i];
    }
    memcpy(out, s, 16);
    return true;
}
#endif
