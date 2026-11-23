// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Shared source – edit only in firmware/_common, then run tools/common/sync_common.py

#include "clockset.h"
#include "evlog.h"

#include <sys/time.h>
#include <esp_sntp.h>
#include <esp_attr.h>

namespace clockset {

enum Source : uint8_t { SRC_NONE, SRC_NTP, SRC_BROWSER, SRC_MANUAL };
// Kept over a restart like the time itself (RTC memory)
RTC_NOINIT_ATTR uint32_t srcMagic;
RTC_NOINIT_ATTR uint8_t src;
static const uint32_t MAGIC = 0x434C4B31;   // "CLK1"

static void onNtpSync(struct timeval*) {
    srcMagic = MAGIC;
    src = SRC_NTP;
}

void begin() {
    if (srcMagic != MAGIC || !valid()) {
        srcMagic = MAGIC;
        src = SRC_NONE;
    }
    sntp_set_time_sync_notification_cb(onNtpSync);
}

bool valid() {
    return time(nullptr) > 1700000000;   // after 2023
}

bool set(int64_t unixSeconds, bool manual) {
    if (unixSeconds < 1700000000LL || unixSeconds > 4102444800LL) return false;   // 2023 .. 2100
    struct timeval tv;
    tv.tv_sec = (time_t)unixSeconds;
    tv.tv_usec = 0;
    if (settimeofday(&tv, nullptr) != 0) return false;
    srcMagic = MAGIC;
    src = manual ? SRC_MANUAL : SRC_BROWSER;
    evlog::add(manual ? "Uhrzeit von Hand gesetzt" : "Uhrzeit vom Browser übernommen");
    return true;
}

void json(JsonObject o) {
    static const char* const NAMES[] = {"", "ntp", "browser", "manual"};
    o["t"] = (long long)time(nullptr);
    o["ok"] = valid();
    o["src"] = NAMES[src < 4 ? src : 0];
}

}  // namespace clockset
