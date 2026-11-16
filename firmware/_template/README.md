# Example Node

> **Achtung:** Ohne Gewähr, Nutzung auf eigene Gefahr – siehe [Haftungsausschluss](../../README.de.md#haftungsausschluss--nutzung-auf-eigene-gefahr).

> TODO: Ein Satz, was das Gerät misst oder steuert.

ESP32-Firmware für das Wohnmobil im Verbund [VanNode](../../README.md): Werte per **MQTT** mit Home-Assistant-Discovery, eigene **Web-Oberfläche** zum Einrichten, Updates ohne USB.

## Funktionen

- Werte per MQTT unter `camper/example/<key>`, Home Assistant legt die Entitäten automatisch an
- Web-Oberfläche: Live-Werte, Einstellungen, MQTT, WLAN, optionaler Login
- Setup-WLAN `Example-Node-XXXXXX` (192.168.4.1) beim ersten Start und wenn das WLAN 60 s fehlt
- Firmware (`.bin`) und Web-Oberfläche (`index.html.gz`) im Web-UI hochladbar, dazu Arduino-OTA
- TODO: gerätespezifische Funktionen

## Hardware

| Teil | Hinweis |
|---|---|
| ESP32-C3 Super Mini oder ESP32-S3 | |
| TODO: Sensor | Anschluss an GPIO `SENSOR_PIN` (`config.h`) |

TODO: Anschlussplan / Foto.

## Arduino IDE

Board-Paket, gemeinsame Bibliotheken, Board-Einstellungen, erstes Flashen, LittleFS-Plugin und Hilfe bei Upload-Problemen: **[docs/ARDUINO.md](../../docs/ARDUINO.md)**. Hier nur, was für dieses Gerät gilt.

| Einstellung | Wert |
|---|---|
| Board | ESP32-C3 Super Mini (bzw. ESP32-S3), Einstellungen siehe docs/ARDUINO.md |
| Zusätzliche Bibliotheken | TODO: Sensor-Bibliothek (auch in `docs/ARDUINO.md` und `build.yml` eintragen) |
| Web-Oberfläche | einmalig über das Upload-Formular (`http://<ip>/` → `data/index.html.gz`) |

## Updates ohne USB

| Was | Wie |
|---|---|
| Firmware | Arduino IDE → *Sketch → Kompilierte Binärdatei exportieren*, die `.bin` im Web-UI unter **WiFi → Firmware- & Web-Update** hochladen (Neustart), oder per Netzwerkport in der IDE (OTA-Passwort) |
| Web-Oberfläche | `data/index.html.gz` im Web-UI hochladen – ersetzt nur die Seite, ohne Neustart, Einstellungen bleiben |
| Komplettes LittleFS-Image | Nur per USB oder IDE-Netzwerkport; löscht dabei auch die Einstellungen (`config.json`) |

## MQTT / Home Assistant

Basis-Topic `camper/example`: ein Topic pro Wert (`VALUES[]` in `sensor.cpp`), dazu `status`, `info`, `rssi` und `…/cmd/measure`. Alle Topics: **[docs/TOPICS.md](../../docs/TOPICS.md#muster-gerät-camperexample)**.

TODO: eigene Werte und Befehle in `docs/TOPICS.md` unter einem eigenen Abschnitt eintragen und hier nur das Basis-Topic nennen.

## Serieller Monitor (115200 Baud)

`read` (Werte anzeigen), `measure` (sofort messen), `noauth` (Web-Login löschen), `help`.

## Sicheres Update, Ereignisse und Rückfall

Seit 1.1.0 hat jede Firmware eine Kennung (`CAMPER-FW:example:<version>:<chip>;`) und dieselbe Absicherung wie alle VanNode-Geräte (`fwcheck.*`, `evlog.*`, `safeguard.*` aus `firmware/_common`):

- **Prüfung beim Update** (Web-UI unter **WiFi → Firmware- & Web-Update**): Die installierte Version steht dabei. Eine gewählte Datei wird schon im Browser und beim Hochladen im Gerät noch einmal geprüft, bevor etwas aktiviert wird. Abgelehnt werden eine `.merged.bin` (komplettes Flash-Abbild für USB), Firmware für ein anderes Gerät oder einen anderen Chip und zu große Dateien. Eine Datei ohne Kennung (Version vor 1.1.0) geht nur mit „Ohne Kennung hochladen“. Fehler stehen im Klartext da statt „Update failed“.
- **Ereignisse:** Die letzten Ereignisse (Start mit Grund, WLAN und MQTT verbunden/getrennt, Uploads mit Ergebnis, Rückfall, gespeicherte Einstellungen) stehen in der Web-Oberfläche. Sie bleiben bei Neustart, Absturz und Update erhalten, nach einem Stromausfall ist das Log leer.
- **Rückfall nach Update:** Hat eine neue Firmware nach 5 Minuten kein WLAN oder startet sie mehr als dreimal neu, startet wieder die vorherige. Kam das Update über den Setup-AP (ohne WLAN), wird nur auf Neustart-Schleifen geachtet.
- **WLAN-Änderung mit Rückweg:** Werden die WLAN-Daten geändert, während das Gerät verbunden ist, merkt es sich die funktionierenden. Klappt die Verbindung mit den neuen nach dem Neustart etwa 1 Minute lang nicht, kommen die alten zurück.

## Versionen

Die laufende Version steht in der Web-Oberfläche des Geräts. Alle Änderungen je Version: [CHANGELOG.md](CHANGELOG.md) (steht auch im jeweiligen GitHub-Release).

## Lizenz

GNU GPL Version 3 mit zusätzlichen Bedingungen (Urhebervermerk bleibt erhalten, veränderte Fassungen sind zu kennzeichnen), siehe [LICENSE](LICENSE) und [NOTICE.md](NOTICE.md). Name und Logo „VanNode“ sind nicht Teil der Lizenz: [NAME-UND-LOGO.md](../../NAME-UND-LOGO.md).

Ohne Gewähr, Nutzung auf eigene Gefahr; die Haftung ist ausgeschlossen, soweit gesetzlich zulässig (siehe [NOTICE.md](NOTICE.md)). Produkt- und Firmennamen sind Marken ihrer Inhaber, VanNode hat keine Verbindung zu ihnen.

