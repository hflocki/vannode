> [!WARNING]
> **IN ENTWICKLUNG.**

# Battery Node

> **Achtung:** Ohne Gewähr, Nutzung auf eigene Gefahr – siehe [Haftungsausschluss](../../README.md#haftungsausschluss--nutzung-auf-eigene-gefahr). Batteriewerte sind kein Schutz für die Batterie und ersetzen nicht die Schutzfunktionen des BMS.

Ein **M5Stack AtomS3 Lite** liest die Batterien im Camper per Bluetooth und stellt die Werte über eine Web-Oberfläche und MQTT (mit Home-Assistant-Discovery) bereit. Es wird nichts verdrahtet, nur der AtomS3 mit Strom, in Bluetooth-Reichweite der Batterien.

Bis ble 2.4.0 lief der battery-node mit der Firmware des [ble-node](../ble). Seit 2.5.0 hat er eine eigene Firmware mit eigenen Releases (`battery-v…`); Kern und Web-Oberfläche teilen sich beide weiter aus [`firmware/_common/ble`](../_common/ble), siehe [Aufbau](#aufbau). Ein neues Plugin im ble-node braucht damit kein neues Release des battery-node und umgekehrt.

## Batterien

| Typ | Bluetooth | Einstellungen in der Web-UI |
| --- | --- | --- |
| Batterie: Daly BMS (BLE-Name `DL-…`, Protokoll D2 oder 0x81 wird erkannt) | Verbinden | MAC, Abfrageintervall |
| Batterie: Ective BM X (LiPower, Geräte-ID wird erkannt) | Verbinden | MAC, Abfrageintervall |
| Batterie: BM2 Batteriewächter (z. B. Starterbatterie, nur Spannung und Ladestand) | Verbinden (nur falls kein Funk) | MAC |

Ab Werk ist keine Batterie angelegt: unter **Geräte** einen Typ wählen und **+ Gerät hinzufügen**, MAC über **Suchen** übernehmen, danach **Jetzt neu starten**. Vom selben Typ gehen mehrere (`daly`, `daly2` …).

- **Daly / Ective:** feste Verbindung, Abfrage alle *Abfrageintervall* Sekunden (5–300, Standard 10). Ladestand, Spannung, Strom, Leistung, Restkapazität, Energie, Restlaufzeit; bei Daly zusätzlich Zellspannungen, Zelldifferenz, Temperatur, Zyklen und Fehler als Text.
- **BM2:** neuere Geräte senden ihre Werte per Funk, dann bleibt der Verbindungsplatz frei („OK (Funk)“). Ältere ohne Funkwerte werden verbunden. Zustand aus der Spannung (Normal, Schwach, Kritisch, Lädt, Erhaltungsladung).
- Höchstens **3 Verbindungen gleichzeitig** (Grenze von NimBLE), insgesamt höchstens 6 Geräte (`MAX_PLUGINS`).
- Solange der ESP verbunden ist, kommt die Hersteller-App meist nicht dran: **Bluetooth für App freigeben** am Gerät (endet nach 30 min von selbst).
- Für die Displays steht die Liste aller Batterien unter `camper/battery/batteries`, Einzelwerte unter `camper/battery/<geräte-id>/…`, siehe [TOPICS](../../docs/TOPICS.md#batterien-camperbattery).

Die Protokolle stammen aus [aiobmsble](https://github.com/patman15/aiobmsble) von Patrick Loschmidt (patman15), der Bibliothek hinter der Home-Assistant-Integration *BLE Battery Management* (Apache-2.0, `src/plugins/bms_proto.*`, [LICENSE-Apache-2.0](LICENSE-Apache-2.0)). Host-Test: `tests/run.sh`.

Wie die Web-Oberfläche aussieht, zeigt das klickbare Mockup [`docs/mockups/battery.html`](../../docs/mockups/battery.html) (herunterladen und im Browser öffnen).

## Hardware

**M5Stack AtomS3 Lite** (ESP32-S3, 8 MB Flash) – die CI baut `battery-node-atoms3-v<version>.bin`. Andere Boards gibt es dafür nicht als Release. Der AtomS3 sitzt am besten in der Nähe der Batterien; Batteriekästen aus Metall dämpfen stark, die Signalstärke jeder Batterie steht in der Web-Oberfläche.

**Status-LED** (RGB-LED des AtomS3, Helligkeit `STATUS_LED_BRIGHTNESS`):

| LED | Bedeutung |
| --- | --- |
| grün | alle Batterien liefern Werte |
| gelb | mindestens eine fehlt oder antwortet nicht |
| rot, blinkend | kein WLAN oder keine MQTT-Verbindung |
| blau | noch keine Batterie angelegt |

## Arduino IDE

Board-Paket, gemeinsame Bibliotheken, Board-Einstellungen (Spalte *M5Stack AtomS3 Lite*), erstes Flashen und Hilfe bei Upload-Problemen: **[docs/ARDUINO.md](../../docs/ARDUINO.md)**. Hier nur, was für dieses Gerät gilt.

| Einstellung | Wert |
| --- | --- |
| Board | M5AtomS3, USB CDC On Boot **Enabled**, Partition **8M with spiffs (3MB APP/1.5MB SPIFFS)** |
| Release-Datei | `battery-node-atoms3-v<version>.bin` |
| Zusätzliche Bibliothek | NimBLE-Arduino 2.5.1 |
| Sketch | `battery_node/battery_node.ino` |
| Web-Oberfläche | einmalig über das Upload-Formular (`http://<ip>/` → `data/index.html.gz`), danach Updates unter **System → Firmware- & Web-Update** |

Eigene Passwörter, WLAN und Broker: `config_local.h.example` nach `config_local.h` kopieren (geht nicht nach GitHub).

Ohne eingetragenes WLAN öffnet der AtomS3 das Setup-WLAN `Battery-Node-XXXXXX` (Passwort `battery1234`), die Seite liegt dann unter `http://192.168.4.1`. Ab Werk heißt der Node `battery-node` mit Topic `camper/battery`.

## Umstieg vom ble-node (bis ble 2.4.0)

Die Update-Seite der ble-Firmware nimmt die battery-Datei nicht an (andere Gerätekennung `toolbox` statt `battery`). Deshalb **einmal** über die Arduino IDE flashen – per USB mit *Erase All Flash Before Sketch Upload: Disabled* oder über den Netzwerk-Port (OTA-Passwort wie bisher). Die Einstellungen bleiben erhalten: WLAN, MQTT (Gerätename `battery-node`, Topic `camper/battery`), Login und alle Batterien mit MAC und Abfrageintervall; Topics, Home-Assistant-Einträge und die Anzeige auf Wandpanel und Pocket-Node ändern sich nicht. Danach `data/index.html.gz` hochladen (gleiche Seite wie bisher, nur neu gepackt). Ab dann gehen Updates wieder über die Web-Oberfläche mit `battery-node-atoms3-…`.

Geräte anderer Typen (z. B. eine Dachhaube), die auf dem AtomS3 angelegt waren, werden übersprungen und fallen beim nächsten Speichern aus der Konfiguration – die gehören auf den ble-node.

Wer noch die alte battery-Firmware 1.x hat: Beim ersten Start wird ihre Batterieliste übernommen (wie bisher seit ble 2.0), einmal per USB flashen.

## Aufbau

| Pfad | Inhalt |
| --- | --- |
| `battery_node/config.h` | Batterie-Typen (`PLUGIN_DALY`, `PLUGIN_ECTIVE`, `PLUGIN_BM2`), Gerätename, Topic, Zeiten, Status-LED |
| `battery_node/src/plugins/` | Batterie-Plugins (`plugin_bms.cpp`) und Protokolle (`bms_proto.*`) |
| `battery_node/src/core/`, `battery_node/web/` | Kopie des gemeinsamen BLE-Kerns aus `firmware/_common/ble` (WLAN, MQTT/HA, Webserver, Bluetooth, `main.cpp` mit `setup()`/`loop()`) und der Web-Oberfläche – **nicht hier ändern**, sondern dort, dann `python3 tools/common/sync_common.py` und `python3 tools/web/pack_web.py` |
| `tests/` | Host-Test der Protokolle (`bash tests/run.sh`) |

Neue Batterie-Typen kommen als Plugin wie beim ble-node dazu (siehe [Neues Plugin schreiben](../ble/README.md#neues-plugin-schreiben)), Schalter in `config.h` hier.

## Serieller Monitor

115200 Baud. Befehle: `read` (alle Werte), `scan` (Bluetooth-Geräte in der Nähe), `noauth` (vergessenen Web-Login löschen), `help`. Jede Batterie hat unter *Geräte* eine Karte **Log** mit Download, der Reiter **Debug** (*System → Debug*) zeigt alle Logs auf einmal.

## Sicheres Update, Ereignisse und Rückfall

Kennung `CAMPER-FW:battery:<version>:atoms3;` und dieselbe Absicherung wie alle VanNode-Geräte (`fwcheck.*`, `evlog.*`, `safeguard.*` aus `firmware/_common`): Prüfung der Datei vor dem Update, Ereignisliste, Rückfall auf die vorherige Firmware, WLAN-Änderung mit Rückweg. Ein Update ersetzt nur das Programm, alle Einstellungen bleiben.

## Versionen

Die laufende Version steht in der Web-Oberfläche des Geräts. Alle Änderungen je Version: [CHANGELOG.md](CHANGELOG.md) (steht auch im jeweiligen GitHub-Release).

## Fertige Firmware

Wird im Haupt-Repo per GitHub Actions gebaut, Tag `battery-v<version>`, Datei `battery-node-atoms3-v<version>.bin` plus `battery-node-web-v<version>.html.gz`. Siehe [Fertige Firmware](../../README.md#fertige-firmware).

## Lizenz

GNU GPL Version 3 mit zusätzlichen Bedingungen (Urhebervermerk bleibt erhalten, veränderte Fassungen sind zu kennzeichnen), siehe [LICENSE](LICENSE) und [NOTICE.md](../../NOTICE.md). Name und Logo „VanNode“ sind nicht Teil der Lizenz: [NAME-UND-LOGO.md](../../NAME-UND-LOGO.md). Fremde Teile: Batterie-Protokolle `bms_proto.*` aus [aiobmsble](https://github.com/patman15/aiobmsble) (Apache-2.0, [LICENSE-Apache-2.0](LICENSE-Apache-2.0)).

Ohne Gewähr, Nutzung auf eigene Gefahr; die Haftung ist ausgeschlossen, soweit gesetzlich zulässig (siehe [NOTICE.md](../../NOTICE.md)). Produkt- und Firmennamen sind Marken ihrer Inhaber, VanNode hat keine Verbindung zu ihnen.
