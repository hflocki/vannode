// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Shared source of the BLE nodes – edit only in firmware/_common/ble, then run
// tools/common/sync_common.py (copies it into firmware/ble and firmware/battery).

#ifndef CT_WEBSERVER_H
#define CT_WEBSERVER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../../config.h"

void webserverInit();
void webserverStart();
void webserverLoop();

// Live data of the core and all enabled plugins (web UI, serial "read")
void buildLiveJson(JsonDocument& doc);

#endif // CT_WEBSERVER_H
