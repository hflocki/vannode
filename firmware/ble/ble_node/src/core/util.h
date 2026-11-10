// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#ifndef CT_UTIL_H
#define CT_UTIL_H

#include <Arduino.h>

// Normalizes "aa-bb-cc-dd-ee-ff" / "aabbccddeeff" / "AA:BB:..." to "AA:BB:CC:DD:EE:FF".
// Empty input is valid (= no device). Returns false on invalid input.
bool normalizeMac(const char* in, char* out, size_t size);

// "01 AB FF" (for the raw data views)
void bytesToHex(const uint8_t* d, size_t n, char* out, size_t size);

// Printable copy of an ASCII message (control characters as '.')
void printableCopy(const uint8_t* d, size_t n, char* out, size_t size);

// Case-insensitive substring search (e.g. a 16 bit UUID inside "0000fcd2-0000-...")
bool containsIgnoreCase(const char* haystack, const char* needle);

#endif // CT_UTIL_H
