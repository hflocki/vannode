# Heater-Node – copyright, license and third-party parts

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
| `firmware/heater` – LIN/iNet Box protocol logic | ported | [skrebber/esphome-truma_inetbox](https://github.com/skrebber/esphome-truma_inetbox), fork of [Fabian-Schmidt/esphome-truma_inetbox](https://github.com/Fabian-Schmidt/esphome-truma_inetbox) | GPL-3.0 |
| `firmware/heater` – setting the timer | ported | [havanti/esphome-truma](https://github.com/havanti/esphome-truma) | GPL-3.0 |
| `firmware/heater` – protocol groundwork | reference | [danielfett/inetbox.py](https://github.com/danielfett/inetbox.py), [muccc/WomoLIN](https://github.com/muccc/WomoLIN) | GPL-3.0 |
| `firmware/heater/tests/captures` – CP Plus / Combi D6 E recordings | test data | [muccc/WomoLIN](https://github.com/muccc/WomoLIN) | GPL-3.0 |
| `firmware/heater` – MQTT topic layout `control_status/…`, `set/…` | naming only | [mc0110/inetbox2mqtt](https://github.com/mc0110/inetbox2mqtt) | MIT |
| `firmware/_common/truma_errors.h` – Truma fault codes | facts, texts rephrased in German | Truma "Diagnostics catalogue Truma Combi" (04.02.2019); CP Plus code lists as published on [womo-teile.de/truma-fehlercodes](https://www.womo-teile.de/truma-fehlercodes) | – |
