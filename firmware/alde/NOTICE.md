# Alde Node – copyright, license and third-party parts

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

Product and company names used in this project – for example Alde, Truma, Combi, CP Plus, CaraWater,
Daly, Ective, Govee, Mopeka, MaxxFan, Heki, Votronic, Xiaomi, M5Stack, Espressif, Guition, SMLIGHT,
Teltonika and Home Assistant – are trademarks or registered trademarks of their respective owners.
They are used only to name compatible devices. VanNode is not affiliated with, endorsed, supported
or certified by any of these owners. Protocols are taken from public open-source projects or were
determined only to make the devices work together (interoperability).

## Third-party parts in this folder

| Part | Used for | Source | License |
| --- | --- | --- | --- |
| `camper_alde/alde_lin.*` – frame layout, field offsets, encodings and plausibility gates of the Alde Compact 3020 HE LIN bus | ported from the ESPHome reader (receive-only parser, signal frames) | [Mellowgroup-dk/alde-3020he-lin](https://github.com/Mellowgroup-dk/alde-3020he-lin), Copyright (c) 2026 Mellowgroup | code MIT ([`LICENSE-alde-MIT`](LICENSE-alde-MIT)), protocol documentation CC BY 4.0 |
| `camper_alde/alde_ci.*` – frames, encodings, registration and command sequence of the Alde external panel bus (CI-bus) | ported from the Python bridge (protocol part) | [daviddootson/alde_3030_plus](https://github.com/daviddootson/alde_3030_plus), Copyright (c) 2026 daviddootson | MIT ([`LICENSE-alde3030-MIT`](LICENSE-alde3030-MIT)) |

Besides that only the libraries the Arduino IDE downloads when building (see the repository `NOTICE.md`).
