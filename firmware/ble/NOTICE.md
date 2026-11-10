# BLE-Node – copyright, license and third-party parts

Copyright © 2026 hflocki – part of VanNode, <https://github.com/hflocki/vannode>

This folder is free software under the **GNU General Public License version 3** (file
[`LICENSE`](LICENSE)) with the additional terms below. It comes WITHOUT ANY WARRANTY.
The complete list of third-party parts of VanNode is in the [`NOTICE.md`](../../NOTICE.md) of the
repository.

## Additional terms (GNU GPL v3, section 7)

As permitted by section 7 of the GNU GPL v3, the following additional terms apply to this program
and to every work based on it:

1. **Author attribution (section 7 b).** The author attribution "Copyright © 2026 hflocki", the link
   to the original project <https://github.com/hflocki/vannode> and the license information that
   the program shows must be preserved in all copies and modified versions as part of their
   Appropriate Legal Notices. In VanNode these are: the card "Über VanNode" on the web page of
   every device, *Einstellungen → Info* on the web page of the display, the logo card on the
   system page of the wall panel and the footer of the Road Buddy update page. They may be moved
   or restyled, but not removed, shortened or made unreadable.
2. **No misrepresentation of origin (section 7 c).** Modified versions must be marked in a
   reasonable way as different from the original version (for example in their name, version and
   the notices above). Nobody may present this program or a modified version of it as their own
   original work.
3. **Name and logo (section 7 e).** The license grants no rights to the name "VanNode" or the
   VanNode logo. A modified version needs its own name and logo and may only state truthfully that
   it is based on VanNode. Details (in German): [`NAME-UND-LOGO.md`](../../NAME-UND-LOGO.md).

These terms do not forbid selling the program or devices that run it. Whoever distributes it must
follow the GNU GPL v3 (including passing on the complete source code) and the terms above.

## Third-party parts in this folder

| Part | Used for | Source | License |
| --- | --- | --- | --- |
| `firmware/ble` – battery protocols `bms_proto.*` (Daly, Ective, BM2) | ported | [patman15/aiobmsble](https://github.com/patman15/aiobmsble) | Apache-2.0 ([`firmware/ble/LICENSE-Apache-2.0`](../../firmware/ble/LICENSE-Apache-2.0)) |
| `firmware/ble` – roof hatch protocol (RV Tech / Heki) | ported | [C64Axel/rvtech-hacs](https://github.com/C64Axel/rvtech-hacs), Copyright (c) 2026 C64Axel | MIT ([`firmware/ble/LICENSE-rvtech-MIT`](../../firmware/ble/LICENSE-rvtech-MIT)) |
| `firmware/ble` – CaraWater pump protocol | protocol knowledge | Home Assistant integration "carawater_isp15" | – |
| `firmware/ble` – Govee and Mopeka formats | decoding as in these libraries, own implementation | [Bluetooth-Devices/govee-ble](https://github.com/Bluetooth-Devices/govee-ble), [Bluetooth-Devices/mopeka-iot-ble](https://github.com/Bluetooth-Devices/mopeka-iot-ble) | MIT |
| `firmware/ble` – BM2 battery monitor frame layout | reference | [DJBenson/bm2-esphome](https://github.com/DJBenson/bm2-esphome), [andystewart999/ha_bm2monitor](https://github.com/andystewart999/ha_bm2monitor) | MIT |
