// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#ifndef CT_BTHOME_CRYPT_H
#define CT_BTHOME_CRYPT_H

// Decryption of encrypted BTHome v2 advertisements (https://bthome.io/encryption/), no
// Arduino types: tested on a PC (firmware/ble/tests/bthome_test.cpp).
//
// Service data: [device info, bit 0 set][ciphertext][counter, 4 bytes LE][MIC, 4 bytes]
// AES-128-CCM (RFC 3610), 4-byte MIC, no associated data, 13-byte nonce:
//   MAC (6 bytes as written, AA first) | D2 FC | device info | counter (4 bytes as sent)
// The AES block function is passed in (mbedtls on the ESP32, a small AES in the test).

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef void (*BthAesFn)(void* ctx, const uint8_t in[16], uint8_t out[16]);

// "AA:BB:CC:DD:EE:FF" -> 6 bytes; false if malformed
static inline bool bthMacBytes(const char* s, uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) {
        unsigned v = 0;
        for (int k = 0; k < 2; k++) {
            char c = s[i * 3 + k];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else return false;
        }
        if (i < 5 && s[i * 3 + 2] != ':') return false;
        mac[i] = (uint8_t)v;
    }
    return true;
}

// 32 hex digits -> 16 bytes; false if malformed (spaces are not allowed)
static inline bool bthKeyBytes(const char* s, uint8_t key[16]) {
    if (strlen(s) != 32) return false;
    for (int i = 0; i < 16; i++) {
        unsigned v = 0;
        for (int k = 0; k < 2; k++) {
            char c = s[i * 2 + k];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else return false;
        }
        key[i] = (uint8_t)v;
    }
    return true;
}

// Decrypts d (service data incl. device info byte). On success out holds a plain BTHome
// packet (device info with bit 0 cleared, then the objects) for bthParse(), outLen its
// length and counter the packet counter. false = too short or wrong key / MIC.
static inline bool bthDecrypt(BthAesFn aes, void* ctx, const uint8_t mac[6], const uint8_t* d, size_t n,
                              uint8_t* out, size_t outMax, size_t& outLen, uint32_t& counter) {
    outLen = 0;
    if (n < 1 + 4 + 4) return false;
    size_t m = n - 9;                       // ciphertext length
    if (m > 0xFFFF || m + 1 > outMax) return false;
    const uint8_t* ct = d + 1;
    const uint8_t* cnt = d + 1 + m;
    const uint8_t* mic = d + 1 + m + 4;
    counter = (uint32_t)cnt[0] | ((uint32_t)cnt[1] << 8) | ((uint32_t)cnt[2] << 16) | ((uint32_t)cnt[3] << 24);

    uint8_t nonce[13];
    memcpy(nonce, mac, 6);
    nonce[6] = 0xD2;
    nonce[7] = 0xFC;
    nonce[8] = d[0];
    memcpy(nonce + 9, cnt, 4);

    // Counter blocks A_i = flags (L - 1 = 1) | nonce | i (2 bytes BE)
    uint8_t a[16], s[16], s0[16];
    a[0] = 0x01;
    memcpy(a + 1, nonce, 13);
    a[14] = 0;
    a[15] = 0;
    aes(ctx, a, s0);
    for (size_t off = 0, i = 1; off < m; off += 16, i++) {
        a[14] = (uint8_t)(i >> 8);
        a[15] = (uint8_t)i;
        aes(ctx, a, s);
        for (size_t k = 0; k < 16 && off + k < m; k++) out[1 + off + k] = ct[off + k] ^ s[k];
    }

    // CBC-MAC: B_0 = flags (M = 4: (4-2)/2 << 3, L - 1 = 1) | nonce | length, then the plaintext
    uint8_t x[16], b[16];
    b[0] = 0x09;
    memcpy(b + 1, nonce, 13);
    b[14] = (uint8_t)(m >> 8);
    b[15] = (uint8_t)m;
    aes(ctx, b, x);
    for (size_t off = 0; off < m; off += 16) {
        for (size_t k = 0; k < 16; k++) b[k] = x[k] ^ (off + k < m ? out[1 + off + k] : 0);
        aes(ctx, b, x);
    }
    uint8_t diff = 0;
    for (int k = 0; k < 4; k++) diff |= (uint8_t)((x[k] ^ s0[k]) ^ mic[k]);
    if (diff) {
        memset(out, 0, m + 1);
        return false;
    }
    out[0] = d[0] & (uint8_t)~0x01;
    outLen = m + 1;
    return true;
}

#endif // CT_BTHOME_CRYPT_H
