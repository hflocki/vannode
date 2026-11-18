// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
//
// Debug log ring buffer, see dbglog.h (same scheme as devlog of the BLE node).
#include "dbglog.h"

#include <mutex>
#include <stdarg.h>

static char buf[DBGLOG_SIZE];
static size_t head = 0;   // Index of the oldest byte
static size_t len = 0;    // Bytes in use
static std::mutex lock;

static void append(const char* s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        buf[(head + len) % DBGLOG_SIZE] = s[i];
        if (len < DBGLOG_SIZE) len++;
        else head = (head + 1) % DBGLOG_SIZE;   // Full: overwrite the oldest byte
    }
}

void dbglogf(const char* fmt, ...) {
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n >= sizeof(msg)) n = sizeof(msg) - 1;
    while (n > 0 && (msg[n - 1] == '\n' || msg[n - 1] == '\r')) msg[--n] = '\0';

    Serial.println(msg);

    // Time stamp: uptime h:mm:ss.mmm (the ESP has no clock time)
    unsigned long ms = millis();
    char stamp[24];
    int sn = snprintf(stamp, sizeof(stamp), "%lu:%02lu:%02lu.%03lu ", ms / 3600000UL, (ms / 60000UL) % 60,
                      (ms / 1000UL) % 60, ms % 1000UL);

    std::lock_guard<std::mutex> guard(lock);
    append(stamp, sn);
    append(msg, n);
    append("\n", 1);
}

bool dbglogGet(String& out) {
    std::lock_guard<std::mutex> guard(lock);
    if (len == 0) return false;
    size_t start = head, n = len;
    if (n == DBGLOG_SIZE) {
        // Buffer has wrapped: skip the cut-off first line
        size_t skip = 0;
        while (skip < n && buf[(start + skip) % DBGLOG_SIZE] != '\n') skip++;
        if (skip < n) skip++;
        start = (start + skip) % DBGLOG_SIZE;
        n -= skip;
    }
    out.reserve(out.length() + n + 1);
    size_t first = n;
    if (start + first > DBGLOG_SIZE) first = DBGLOG_SIZE - start;   // Wraps around
    out.concat(buf + start, first);
    if (first < n) out.concat(buf, n - first);
    return true;
}

void dbglogClear() {
    std::lock_guard<std::mutex> guard(lock);
    head = 0;
    len = 0;
}
