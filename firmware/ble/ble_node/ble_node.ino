// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

/*
 * BLE Node - one ESP32, several Bluetooth devices of the camper as plugins
 * ==============================================================
 * Which plugin types are built in is chosen in config.h (PLUGIN_xxx 1/0):
 *   - Gas (optional): Rotarex DIMES / SRG WAVE, Mopeka Pro, Mopeka Standard (passive BLE)
 *   - Water pump: CaraWater (e.g. ISP 15), BLE connection (status + control)
 *   - Roof hatch: RV Tech skylight motor, BLE connection (position, presets)
 *   - Fridge: WCS compressor fridge, BLE connection (temperatures, voltage)
 *   - Alarm WiPro III, OBD2 adapter, SuperSense tanks, tire pressure (TireCheck)
 *   - Thermometers: Govee, BTHome (ATC/pvvx, Shelly BLU), own BLE sensor (passive BLE)
 * Batteries (Daly, Ective, BM2) have their own node: firmware/battery.
 *
 * Values are published via MQTT (Home Assistant discovery) and shown in the web UI.
 * Core (setup/loop, WiFi, MQTT, web server, BLE) in src/core and the web page in web/ are
 * copies of firmware/_common/ble (shared with the battery-node, tools/common/sync_common.py);
 * the plugins of this node live in src/plugins.
 * Shared modules of all VanNode devices (sketch root, copies of firmware/_common):
 * fwcheck (upload checks), evlog (event log), safeguard (rollback, WiFi fallback), devinfo.
 */

#include "config.h"   // setup() and loop(): src/core/main.cpp
