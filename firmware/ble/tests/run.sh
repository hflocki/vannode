#!/usr/bin/env bash
# Host tests of the BLE node (no ESP32 needed)
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
${CXX:-g++} -std=gnu++17 -Wall -Wextra -o build/blesensor_test blesensor_test.cpp
./build/blesensor_test
