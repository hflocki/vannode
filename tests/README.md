# Tests

Host-side tests for the LIN / iNet Box protocol code. They run on the Mac or Linux with `g++` and `python3`, no ESP32 needed.

```bash
./tests/run.sh
```

| Test | What it does |
|---|---|
| `protocol_test.cpp` | Synthetic CP Plus session: read-by-identifier, NAD assignment, init request, device + heater status frames, room/fan command, checksums |
| `replay.cpp` | Feeds real LIN analyzer captures into `InetBox` byte by byte. Master frames are replayed, for every slave slot (0x18, 0x3D) our answer is compared with the answer of the original Truma iNet Box |

`replay` accepts several capture files and commands in between, e.g. `cmd:22,A` = room 22 °C, fan HIGH (`HeatingMode` value in hex).

## Captures

`captures/` contains LIN bus recordings (Microchip LIN analyzer, 9600 baud) of a Truma CP Plus with Combi 6 / Combi D6 E and an original iNet Box, from the WomoLIN project (muccc/WomoLIN, GPL-3.0).

`captures/community/` contains ESPHome `truma_inetbox` logs (observer mode, VERBOSE) from a community member: CP Plus with Combi heater, Aventa aircon and an original iNet Box at NAD 0x03. Only the LIN-related log lines are kept. `replay nad:03 <file>` replays them with our node address fixed to 0x03, because the captures contain no NAD assignment.

| Capture | Actions (in order) |
|---|---|
| `heater_vent-off_1_2_3_4_5` | Heater ventilation (CP Plus) off → level 1 … 5 |
| `heater_vent-5_6_7_8_9_10_off` | Ventilation level 5 → 6 … 10 → off |
| `aircon_auto-app_N-…` | Aircon AUTO via Truma app: on, 22 °C → 24 °C, off (app_1: → vent) |
| `aircon_auto-cpplus-…` | The same on the CP Plus |

## Findings from the captures

- All 29 captures (about 6,900 frames): all CP Plus status frames (devices, heater, timer, clock, config) are decoded without checksum errors.
- For all 14 Truma app commands in the captures (room 13–22 °C, fan eco/high, heating on/off) the heater update frame we generate is byte-identical in its payload to the one of the original iNet Box; only the message counter (and therefore the checksum) differs. `run.sh` checks this automatically.
- The captures contain no timer command, so the timer update frame (0x3C) is only checked against the ESPHome implementation, not against the original iNet Box.
- Community captures: all 8 logs (about 7,700 frames incl. aircon status 0x35) decode without checksum errors.
- **Heater ventilation (CP Plus „Lüften“, levels 1–10) is not visible in the heater status frame (0x33).** Room target, heating mode and water target stay 0 for every level; only the operating status changes (4 start → 5 running, back to 4 when switched off). The vent level is therefore neither readable nor settable via the iNet Box interface (esphome-truma_inetbox and inetbox.py have no such mode either; inetbox2mqtt only reads it from the CP Plus ↔ heater frame 0x20, which the iNet Box slot does not see). The firmware reports `hvac_action = fan` when the heater runs with room and water off (ventilation or fan after-run).
- Aircon AUTO: the CP Plus reports it with the *manual* aircon status frame 0x35 (operating mode 0x07, fan 0x77); the AUTO frame 0x37 never appears. Writes from the original iNet Box use frame 0x34 (length 0x0C): `mode, 00, fan, 01, target temp (u16, 0 = keep), …` – on: mode 0x04 → 0x07 with target 0, then target 22 °C; off: mode 0x00, fan 0x70. The firmware ignores aircon frames (heater only).
- Differences to the original iNet Box (all also present in the proven ESPHome implementation, so they are accepted by the CP Plus):
  - The original pads state-buffer answers to 41 bytes (transport length 0x29), we send the real length (e.g. 0x1D).
  - Read-by-identifier 0x00: the original answers variant `0x00`, we answer `0x01`.
  - Read-by-identifier 0x20 / 0x22: the original answers `02 02 00` / `03 07 00`, we answer `17 46 00` (shown as T23.70.0 in the CP Plus INDEX menu) / empty.
