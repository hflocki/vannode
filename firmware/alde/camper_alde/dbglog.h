// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
//
// Debug log in RAM (ring buffer) for the web UI: every line also goes to the serial port.
// Lost on restart; the event log (evlog) is the persistent one.
#pragma once

#include <Arduino.h>

#ifndef DBGLOG_SIZE
#define DBGLOG_SIZE 16384   // Bytes; the oldest lines are dropped when full
#endif

// printf-style line with uptime stamp; a trailing newline in fmt is ignored
void dbglogf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// Whole buffer, oldest line first (false = empty)
bool dbglogGet(String& out);

void dbglogClear();
