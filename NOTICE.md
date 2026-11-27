# VanNode – copyright, license and third-party components

Copyright © 2026 hflocki – <https://github.com/hflocki/vannode>

VanNode is free software: you can redistribute it and/or modify it under the terms of the
**GNU General Public License version 3** as published by the Free Software Foundation
(file [`LICENSE`](LICENSE)), together with the additional terms below. It is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
details.

This applies to every file in this repository unless a file or the table below says otherwise.
Third-party parts keep their own license and copyright; they are listed below, sorted by node, and
their license texts are kept in the license files of the folders.

## Additional terms (GNU GPL v3, section 7)

As permitted by section 7 of the GNU GPL v3, the following additional terms apply to this program
and to every work based on it:

1. **Author attribution (section 7 b).** The author attribution "Copyright © 2026 hflocki", the link
   to the original project <https://github.com/hflocki/vannode> and the license information that
   the program shows must be preserved in all copies and modified versions as part of their
   Appropriate Legal Notices. In VanNode these are: the card "Über VanNode" on the web page of
   every device (including the Pocket-Node update page), *Einstellungen → Info* on the web page of
   the display and the logo card on the system page of the wall panel. They may be moved
   or restyled, but not removed, shortened or made unreadable.
2. **No misrepresentation of origin (section 7 c).** Modified versions must be marked in a
   reasonable way as different from the original version (for example in their name, version and
   the notices above). Nobody may present this program or a modified version of it as their own
   original work.
3. **Name and logo (section 7 e).** The license grants no rights to the name "VanNode" or the
   VanNode logo. A modified version needs its own name and logo and may only state truthfully that
   it is based on VanNode. Details (in German): [`NAME-UND-LOGO.md`](NAME-UND-LOGO.md).

These terms do not forbid selling the program or devices that run it. Whoever distributes it must
follow the GNU GPL v3 (including passing on the complete source code) and the terms above.

## No warranty, no liability – use at your own risk

VanNode is a private hobby project. It is given away free of charge and without any business
activity. It monitors and switches real equipment in a vehicle (heater, gas and water tanks, water
pump, roof hatch and roof fan, batteries). Faults in the software, the settings, the hardware or the
installation can damage devices, the vehicle or other property and can put people at risk.

- Building, installing and using VanNode is entirely at your own risk and responsibility.
- VanNode is not a safety device. It does not replace the controls and protective functions of the
  device manufacturers, gas, CO or smoke detectors, the frost protection valve or regular checks.
  Values and warnings shown by VanNode can be wrong, late or missing – do not rely on them.
- Work on gas installations, 230 V and the vehicle's electrical system must only be done by
  qualified persons, following the applicable regulations and the manufacturers' instructions.
  Connecting VanNode to third-party devices may void their warranty, guarantee or approval.
- To the extent permitted by applicable law, the author accepts no liability for damage of any kind
  resulting from the use of VanNode, its documentation or devices built with it, or from the fact
  that they cannot be used (see also sections 15 and 16 of the GNU GPL v3). Only liability that
  cannot be excluded by law remains, in particular for intent and gross negligence, for injury to
  life, body or health and under mandatory product liability law, where it applies.

## Trademarks

Product and company names used in this project – for example Truma, Combi, CP Plus, CaraWater,
Shelly, Daly, Ective, JBD, JK-BMS, LiTime, Redodo, PowerQueen, Victron, Senso4s, Govee, Mopeka, SuperSense, MaxxFan, Heki, Votronic, Xiaomi, M5Stack, Espressif, Guition, SMLIGHT,
Teltonika, Tailscale, WireGuard and Home Assistant – are trademarks or registered trademarks of their respective owners.
They are used only to name compatible devices. VanNode is not affiliated with, endorsed, supported
or certified by any of these owners. Protocols are taken from public open-source projects or were
determined only to make the devices work together (interoperability).

## Third-party components

This is the only list of third-party parts in VanNode; the devices link to it from their
"Über VanNode" card. It is sorted by node. Temp-, IO-, MaxxFan- and Pocket-Node and the template
contain no third-party parts besides the libraries listed at the end. New third-party code is
added here, under its node, and nowhere else.

### Heater-Node (`firmware/heater`)

| Part | Used for | Source | License |
| --- | --- | --- | --- |
| `firmware/heater` – LIN/iNet Box protocol logic | ported | [skrebber/esphome-truma_inetbox](https://github.com/skrebber/esphome-truma_inetbox), fork of [Fabian-Schmidt/esphome-truma_inetbox](https://github.com/Fabian-Schmidt/esphome-truma_inetbox) | GPL-3.0 |
| `firmware/heater` – setting the timer | ported | [havanti/esphome-truma](https://github.com/havanti/esphome-truma) | GPL-3.0 |
| `firmware/heater` – protocol groundwork | reference | [danielfett/inetbox.py](https://github.com/danielfett/inetbox.py), [muccc/WomoLIN](https://github.com/muccc/WomoLIN) | GPL-3.0 |
| `firmware/heater/tests/captures` – CP Plus / Combi D6 E recordings | test data | [muccc/WomoLIN](https://github.com/muccc/WomoLIN) | GPL-3.0 |
| `firmware/heater` – MQTT topic layout `control_status/…`, `set/…` | naming only | [mc0110/inetbox2mqtt](https://github.com/mc0110/inetbox2mqtt) | MIT |
| `firmware/_common/truma_errors.h` – Truma fault codes | facts, texts rephrased in German | Truma "Diagnostics catalogue Truma Combi" (04.02.2019); CP Plus code lists as published on [womo-teile.de/truma-fehlercodes](https://www.womo-teile.de/truma-fehlercodes) | – |

### Alde-Node (`firmware/alde`)

| Part | Used for | Source | License |
| --- | --- | --- | --- |
| `firmware/alde` – Alde external panel bus (CI-bus) protocol: frames, registration, command sequence | ported | [daviddootson/alde_3030_plus](https://github.com/daviddootson/alde_3030_plus), Copyright (c) 2026 daviddootson | MIT ([`firmware/alde/LICENSE-alde3030-MIT`](firmware/alde/LICENSE-alde3030-MIT)) |
| `firmware/alde` – Alde Compact 3020 HE LIN decoding (frame layout, offsets, encodings, plausibility gates) | ported, receive only | [Mellowgroup-dk/alde-3020he-lin](https://github.com/Mellowgroup-dk/alde-3020he-lin), Copyright (c) 2026 Mellowgroup | MIT ([`firmware/alde/LICENSE-alde-MIT`](firmware/alde/LICENSE-alde-MIT)), documentation CC BY 4.0 |
| `firmware/alde` – panel bus: classic checksum fallback, answer window | reference | [holdestmade/esphome-alde-3020](https://github.com/holdestmade/esphome-alde-3020) | MIT ([`firmware/alde/LICENSE-alde3020-MIT`](firmware/alde/LICENSE-alde3020-MIT)) |

### Battery-Node (`firmware/battery`)

| Part | Used for | Source | License |
| --- | --- | --- | --- |
| `firmware/battery` – battery protocols `bms_proto.*` (Daly, Ective, BM2, JBD, JK-BMS JK02, LiTime/Redodo) | ported |
| `firmware/battery/tests/bms_vectors.h` – reference frames (JBD, JK-BMS, LiTime/Redodo) | test data | [patman15/aiobmsble](https://github.com/patman15/aiobmsble) test suite | Apache-2.0 ([`firmware/battery/LICENSE-Apache-2.0`](firmware/battery/LICENSE-Apache-2.0)) | [patman15/aiobmsble](https://github.com/patman15/aiobmsble) | Apache-2.0 ([`firmware/battery/LICENSE-Apache-2.0`](firmware/battery/LICENSE-Apache-2.0)) |
| `firmware/battery` – `victron_decode.h`, plugin `victron` – Victron Instant Readout record layouts; test vectors in `tests/victron_test.cpp` | ported | [keshavdv/victron-ble](https://github.com/keshavdv/victron-ble) | Unlicense (public domain) |
| `firmware/battery` – BM2 battery monitor frame layout | reference | [DJBenson/bm2-esphome](https://github.com/DJBenson/bm2-esphome), [andystewart999/ha_bm2monitor](https://github.com/andystewart999/ha_bm2monitor) | MIT |

### BLE-Node (`firmware/ble`)

| Part | Used for | Source | License |
| --- | --- | --- | --- |
| `firmware/ble` – roof hatch protocol (RV Tech / Heki) | ported | [C64Axel/rvtech-hacs](https://github.com/C64Axel/rvtech-hacs), Copyright (c) 2026 C64Axel | MIT ([`firmware/ble/LICENSE-rvtech-MIT`](firmware/ble/LICENSE-rvtech-MIT)) |
| `firmware/ble` – CaraWater pump: command `AA 39`, status frame `AA 45` | protocol knowledge, own C++ implementation (no code copied) | [toms2k84/Blackforest-Vanlife-SmartCamper](https://github.com/toms2k84/Blackforest-Vanlife-SmartCamper), folder `carawater-isp-ble`, Copyright (c) 2026 Blackforest Vanlife (toms2k84) | MIT |
| `firmware/ble` – CaraWater pump: telemetry `AA 93`, status bits, checksum, conversion | protocol knowledge, own C++ implementation (no code copied) | [geierwally1978/carawater-isp15-ble](https://github.com/geierwally1978/carawater-isp15-ble) | – (no license stated) |
| `firmware/ble` – CaraWater pump, first version | protocol knowledge, own C++ implementation | Home Assistant integration "carawater_isp15" by a community member, shared in a user group; he did not answer whether and how he wants to be named and will be credited on request | – (no license stated) |
| `firmware/ble` – Govee and Mopeka formats | decoding as in these libraries, own implementation | [Bluetooth-Devices/govee-ble](https://github.com/Bluetooth-Devices/govee-ble), [Bluetooth-Devices/mopeka-iot-ble](https://github.com/Bluetooth-Devices/mopeka-iot-ble) | MIT |
| `firmware/ble` – SuperSense tank sensors (service UUID, BCD payload) | protocol knowledge, own C++ implementation (no code copied) | [adahmen/supersense-homeassistant](https://github.com/adahmen/supersense-homeassistant) | MIT |
| `firmware/_common/truma_lc_decode.h`, plugin `trumalc` – Truma LevelControl (GATT layout, key derivation, value layout) | ported | [cyclemat/HAOS-Truma-Levelcontrol-Integration](https://github.com/cyclemat/HAOS-Truma-Levelcontrol-Integration) | GPL-3.0 |
| `firmware/ble` – plugin `shelly` – Shelly Gen2+ RPC over Bluetooth (GATT UUIDs, framing) | protocol knowledge from the manufacturer's public documentation, own C++ implementation | Shelly knowledge base, "Communicating with Shelly devices via Bluetooth Low Energy using RPC" | – (public documentation) |
| `firmware/ble` – `bthome_crypt.h` – BTHome v2 encryption (AES-CCM nonce and layout) | own implementation of the public format | [bthome.io/encryption](https://bthome.io/encryption/) | – (public specification) |
| `firmware/ble` – `senso4s_decode.h`, plugin `senso4s` – Senso4s advertisement layout and status codes | ported | [ksanislo/senso4s_ble](https://github.com/ksanislo/senso4s_ble), Copyright 2026 Ken Sanislo | Apache-2.0 ([`firmware/ble/LICENSE-Apache-2.0`](firmware/ble/LICENSE-Apache-2.0)) |

### Tanklevel-Node (`firmware/tanklevel`)

| Part | Used for | Source | License |
| --- | --- | --- | --- |
| `firmware/tanklevel` – Mopeka Pro gas sensor decoding | decoding as in this library, own implementation | [Bluetooth-Devices/mopeka-iot-ble](https://github.com/Bluetooth-Devices/mopeka-iot-ble) | MIT |
| `firmware/tanklevel` – SuperSense tank sensors (service UUID, BCD payload) | protocol knowledge, own C++ implementation (no code copied) | [adahmen/supersense-homeassistant](https://github.com/adahmen/supersense-homeassistant) | MIT |

### Level-Node (`firmware/level`)

| Part | Used for | Source | License |
| --- | --- | --- | --- |
| `firmware/level` – BNO085 level sensor (sensor logic, mounting positions, spirit level page, HA card, images, 3D print files) | based on | [klehnst/ESP32-Adrafruit-BNO085-HomeAssistant-Caravan-Levelsensor-MQTT](https://github.com/klehnst/ESP32-Adrafruit-BNO085-HomeAssistant-Caravan-Levelsensor-MQTT), Copyright (c) 2026 klehnst | MIT ([`firmware/level/LICENSE-klehnst-MIT`](firmware/level/LICENSE-klehnst-MIT)) |

### Display-Node (`firmware/display`)

| Part | Used for | Source | License |
| --- | --- | --- | --- |
| `firmware/display` – fonts Montserrat, Font Awesome 5 Free (converted LVGL fonts) | included | [LVGL](https://github.com/lvgl/lvgl), [Montserrat](https://github.com/JulietaUla/Montserrat), [Font Awesome Free](https://fontawesome.com) | SIL OFL 1.1 (Font Awesome icons: CC BY 4.0) |
| `firmware/display/camper_display/src/microlink` – microlink 2.3.0, Tailscale-compatible client (Tailscale on the display) | included; flat include paths and one `#include` line per C file for the Arduino build, see the README in that folder | [CamM2325/microlink](https://github.com/CamM2325/microlink), Copyright (c) 2025-2026 Cameron Malone | MIT ([`firmware/display/LICENSE-microlink-MIT`](firmware/display/LICENSE-microlink-MIT)) |
| `firmware/display/camper_display/src/microlink` – wireguard-lwip (part of microlink) | included | [smartalock/wireguard-lwip](https://github.com/smartalock/wireguard-lwip), Copyright (c) 2021 Daniel Hope | BSD-3-Clause ([`firmware/display/LICENSE-wireguard-lwip-BSD-3-Clause`](firmware/display/LICENSE-wireguard-lwip-BSD-3-Clause)) |
| `firmware/display/camper_display/src/microlink` – X25519 (`x25519.*`), Poly1305 (`poly1305-donna*`) | included | Copyright (c) 2015-2016 Cryptography Research, Inc.; [floodyberry/poly1305-donna](https://github.com/floodyberry/poly1305-donna) | MIT ([`firmware/display/LICENSE-x25519-MIT`](firmware/display/LICENSE-x25519-MIT)); Poly1305: public domain or MIT |

### Bluetooth-Gateway (`firmware/omg`)

| Part | Used for | Source | License |
| --- | --- | --- | --- |
| `firmware/omg` – OpenMQTTGateway | downloaded at build time, not included | [1technophile/OpenMQTTGateway](https://github.com/1technophile/OpenMQTTGateway) | GPL-3.0 |

### Whole project

| Part | Used for | Source | License |
| --- | --- | --- | --- |
| logo wordmark – Poppins glyphs as outlines | drawn | [Poppins](https://github.com/itfoundry/Poppins) | SIL OFL 1.1 |

Libraries that the Arduino IDE (or the CI) downloads when building, not part of this repository,
each under its own license: Arduino-ESP32 and the ESP8266 core (LGPL-2.1), LVGL (MIT),
GFX Library for Arduino (BSD), M5GFX and M5Unified (MIT), NimBLE-Arduino (Apache-2.0),
ESP Async WebServer, AsyncTCP and ESPAsyncTCP (LGPL-3.0), PubSubClient (MIT), ArduinoJson (MIT),
OneWire (permissive license in its source), DallasTemperature (MIT), SparkFun BNO08x (MIT).
