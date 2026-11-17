#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
# GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
# See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
# Host tests for the passive Alde LIN decoder, the external panel bus protocol and
# the passive listener on that bus
# (no ESP32 needed). Also used by the CI.
set -euo pipefail
cd "$(dirname "$0")"
SRC=../camper_alde
mkdir -p build
${CXX:-g++} -std=gnu++17 -Wall -Wextra -I "$SRC" -o build/alde_test alde_test.cpp "$SRC/alde_lin.cpp"
${CXX:-g++} -std=gnu++17 -Wall -Wextra -I "$SRC" -o build/ci_test ci_test.cpp "$SRC/alde_ci.cpp"
${CXX:-g++} -std=gnu++17 -Wall -Wextra -I "$SRC" -o build/sniff_test sniff_test.cpp "$SRC/alde_ci_sniff.cpp" "$SRC/alde_ci.cpp"
./build/alde_test
./build/ci_test
./build/sniff_test
