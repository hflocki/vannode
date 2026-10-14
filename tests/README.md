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

## Findings from the captures

- All CP Plus status frames (devices, heater, timer, clock, config) are decoded without checksum errors.
- The heater update frame we generate for "22 °C, fan high" is byte-identical in its payload to the one of the original iNet Box; only the message counter (and therefore the checksum) differs.
- Differences to the original iNet Box (all also present in the proven ESPHome implementation, so they are accepted by the CP Plus):
  - The original pads state-buffer answers to 41 bytes (transport length 0x29), we send the real length (e.g. 0x1D).
  - Read-by-identifier 0x00: the original answers variant `0x00`, we answer `0x01`.
  - Read-by-identifier 0x20 / 0x22: the original answers `02 02 00` / `03 07 00`, we answer `17 46 00` (shown as T23.70.0 in the CP Plus INDEX menu) / empty.
