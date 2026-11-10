// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#pragma once
// Device log: every log line of a BLE device is written to the serial monitor as before
// ("[tag] message") and also kept in a small ring buffer per device in RAM. The web UI
// downloads it as a file (plugin tab, "Log herunterladen"), so testers need no USB cable.
// The buffers are lost on restart.

#include <Arduino.h>

#ifndef DEVLOG_SIZE
#define DEVLOG_SIZE     8192   // Bytes per device; the oldest lines are dropped when full
#endif
#define DEVLOG_MAX_TAGS 6      // Devices with their own buffer (allocated on first use)

// Logs one line for a device (tag = plugin id), without trailing newline
void devlogf(const char* tag, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

// Appends the log of one device to out, oldest line first. false = no entries.
bool devlogGet(const char* tag, String& out);

// Deletes the log of one device
void devlogClear(const char* tag);
