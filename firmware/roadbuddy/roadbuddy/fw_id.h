// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Version and device id for the firmware marker and the shared modules (fwcheck, safeguard).
// A separate file because config.h holds the personal data and may only be included once.
#pragma once
#define FW_VERSION "1.5.7"
#define FW_DEVICE  "roadbuddy"
// Awake only briefly: a new firmware counts as good after 15 s with WiFi
#define GUARD_CONFIRM_MS 15000UL
