// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

/*
 * Battery Node - batteries of the camper over Bluetooth, on an M5Stack AtomS3 Lite
 * ==============================================================
 * Battery types (config.h, PLUGIN_xxx 1/0), each one a device in the web UI:
 *   - Daly smart BMS (BLE connection, polled)
 *   - Ective BM X battery monitor (BLE connection, polled)
 *   - BM2 battery monitor, e.g. on the starter battery (broadcast, connection as fallback)
 * All batteries are published one by one (<prefix>/<id>/...) and as one JSON list
 * <prefix>/batteries for the displays; status LED of the AtomS3 (config.h).
 *
 * Core (setup/loop, WiFi, MQTT, web server, BLE) in src/core and the web page in web/ are
 * copies of firmware/_common/ble, shared with the ble-node (tools/common/sync_common.py);
 * the battery plugins live in src/plugins.
 * Shared modules of all VanNode devices (sketch root, copies of firmware/_common):
 * fwcheck (upload checks), evlog (event log), safeguard (rollback, WiFi fallback), devinfo.
 */

#include "config.h"   // setup() and loop(): src/core/main.cpp
