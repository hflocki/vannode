#!/usr/bin/env bash
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
${CXX:-g++} -std=gnu++17 -Wall -Wextra -o build/bms_test bms_test.cpp ../ble_node/src/plugins/bms_proto.cpp
./build/bms_test
