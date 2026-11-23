# microlink (Tailscale-kompatibler Client) – fremder Code

Quelle: [CamM2325/microlink](https://github.com/CamM2325/microlink), Komponente `components/microlink`,
Version 2.3.0 (Commit `1cb8d1c` vom 18.11.2026).

| Teil | Urheber | Lizenz |
| --- | --- | --- |
| microlink (`microlink*`, `ml_*`, `nacl_box*`) | Cameron Malone | MIT, [`../../../LICENSE-microlink-MIT`](../../../LICENSE-microlink-MIT) |
| wireguard-lwip (`wireguard*`, `crypto.*`, `chacha20*`, `lwip_compat.h`) | Daniel Hope | BSD-3-Clause, [`../../../LICENSE-wireguard-lwip-BSD-3-Clause`](../../../LICENSE-wireguard-lwip-BSD-3-Clause) |
| X25519 (`x25519.*`) | Cryptography Research, Inc. | MIT, [`../../../LICENSE-x25519-MIT`](../../../LICENSE-x25519-MIT) |
| BLAKE2s (`blake2s.*`) | nach RFC 7693 | wie wireguard-lwip |
| Poly1305 (`poly1305-donna*`) | Andrew Moon (floodyberry/poly1305-donna) | Public Domain oder MIT |

Nicht von Tailscale Inc.; microlink ist eine unabhängige Umsetzung und hat keine Verbindung zu Tailscale Inc.

## Anpassungen für den Arduino-Build

Sonst unverändert. Geändert wurde nur, was der Arduino-Build braucht:

- **Flach in einem Ordner:** Die Arduino IDE kennt keine eigenen Include-Pfade. Deshalb liegen alle Dateien
  hier nebeneinander, und die Includes `"crypto/refc/…"` und `"../../crypto.h"` zeigen direkt auf die Datei
  daneben.
- **Einstellungen statt Kconfig:** Jede `.c`-Datei bindet als erste Zeile `ml_arduino_config.h` ein
  (eigene Datei, GPL-3.0). Darin stehen die Werte, die sonst aus dem Kconfig-Menü von ESP-IDF kommen.
  Keine Zugangsdaten – Auth-Key und Gerätename kommen zur Laufzeit aus den Einstellungen des Displays.
- **Weggelassen:** Mobilfunk (`ml_cellular.c`, `ml_at_socket.c`, `ml_at_validate.c`), Netzwechsel
  (`ml_net_switch.c`) und die eigene Einstellungsseite (`ml_config_httpd.c`, `ml_config_html.h`). Ihre Header
  bleiben, weil sie bei ausgeschalteter Funktion leere Ersatzfunktionen liefern.

Angebunden wird microlink in `../../tailscale.cpp`. Bei einem Update von microlink: Dateien aus
`components/microlink/src`, `include` und `components/wireguard_lwip/src` (samt `crypto/refc`) hierher kopieren,
die beiden Include-Pfade anpassen, die erste Zeile mit `ml_arduino_config.h` wieder einfügen und neue
`CONFIG_ML_*`-Werte in `ml_arduino_config.h` ergänzen.
