#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
# GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
# See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
# Host tests of the BLE node (no ESP32 needed)
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
${CXX:-g++} -std=gnu++17 -Wall -Wextra -o build/blesensor_test blesensor_test.cpp
./build/blesensor_test
${CXX:-g++} -std=gnu++17 -Wall -Wextra -o build/govee_test govee_test.cpp
./build/govee_test
${CXX:-g++} -std=gnu++17 -Wall -Wextra -o build/bthome_test bthome_test.cpp
./build/bthome_test
${CXX:-g++} -std=gnu++17 -Wall -Wextra -o build/obd_test obd_test.cpp
./build/obd_test
${CXX:-g++} -std=gnu++17 -Wall -Wextra -o build/supersense_test supersense_test.cpp
./build/supersense_test
${CXX:-g++} -std=gnu++17 -Wall -Wextra -o build/tpms_test tpms_test.cpp
./build/tpms_test
${CXX:-g++} -std=gnu++17 -Wall -Wextra -o build/trumalc_test trumalc_test.cpp
./build/trumalc_test
${CXX:-g++} -std=gnu++17 -Wall -Wextra -o build/senso4s_test senso4s_test.cpp
./build/senso4s_test
