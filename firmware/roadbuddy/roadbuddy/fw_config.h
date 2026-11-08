// Shared source – edit only in firmware/_common, then run tools/common/sync_common.py
//
// FW_VERSION and FW_DEVICE (plus optional GUARD_* overrides) for the shared modules:
// from fw_id.h if the sketch has one (because its config.h holds personal data and
// variables, like the Road Buddy), otherwise from config.h.
#pragma once
#if __has_include("fw_id.h")
#include "fw_id.h"
#else
#include "config.h"
#endif

// FW_HW: hardware name as in the release files <device>-node-<FW_HW>-v<version>.bin
// (board for M5Stack devices, otherwise the chip). Reported in <prefix>/info and on the
// update page, so the right file can be picked (e.g. ble-node on a C3 or an AtomS3).
#ifndef FW_HW
#if defined(ARDUINO_M5STACK_ATOMS3)
#define FW_HW "atoms3"
#elif defined(ARDUINO_M5STACK_CORE2)
#define FW_HW "core2"
#elif defined(ESP8266)
#define FW_HW "esp8266"
#elif defined(ESP32)
#include <sdkconfig.h>
#define FW_HW CONFIG_IDF_TARGET   // "esp32", "esp32c3", "esp32s3" ...
#else
#define FW_HW "unknown"
#endif
#endif
