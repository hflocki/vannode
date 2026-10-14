#!/usr/bin/env bash
# Host-side tests for the LIN / iNet Box protocol code (no ESP32 needed).
#   1. protocol_test: synthetic CP Plus session (identify, assign NAD, init, status, command)
#   2. replay:        real CP Plus / Combi D6 E captures (WomoLIN project) fed through our parser,
#                     our answers compared with the original iNet Box answers
set -euo pipefail
cd "$(dirname "$0")"
SRC=../truma_heater
OUT=build
mkdir -p "$OUT"
CXX=${CXX:-g++}
$CXX -std=gnu++17 -Wall -I stub -I "$SRC" -o "$OUT/protocol_test" protocol_test.cpp "$SRC/inetbox.cpp"
$CXX -std=gnu++17 -Wall -I stub -I "$SRC" -o "$OUT/replay" replay.cpp "$SRC/inetbox.cpp"

for f in captures/*.log; do
  python3 log2replay.py "$f" "$OUT/$(basename "$f" .log).txt" > /dev/null
done

echo "== protocol_test"
"$OUT/protocol_test" | tail -2

echo "== replay: init with Combi 6"
"$OUT/replay" "$OUT/cp_plus-init-combi_6.txt" | tail -3

echo "== replay: Combi D6 E session, then fan HIGH / 22 C command (compare with original iNet Box)"
"$OUT/replay" "$OUT/cp_plus-combi_6d_e-inet_box.txt" cmd:22,A "$OUT/truma_app-set_fan_high.txt" | grep -E "pid 3D|slots|NAD=" 
