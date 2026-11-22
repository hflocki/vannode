#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
# GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
# See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
# Host tests of the battery node (no ESP32 needed)
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
${CXX:-g++} -std=gnu++17 -Wall -Wextra -o build/bms_test bms_test.cpp ../battery_node/src/plugins/bms_proto.cpp
./build/bms_test
${CXX:-g++} -std=gnu++17 -Wall -Wextra -o build/victron_test victron_test.cpp
./build/victron_test
