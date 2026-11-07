> [!WARNING]
> **IN ENTWICKLUNG.** Die Plugins sind teils mangels Hardware noch nicht am echten Gerät getestet, auch die Dachhaube (RV Tech) noch nicht.

# BLE Node

Ein ESP32 als **Bluetooth-Gateway mit Plugins**: Er liest Bluetooth-Geräte im Camper aus bzw. steuert sie und stellt alles über eine Web-Oberfläche und MQTT (mit Home-Assistant-Discovery) bereit. Es wird nichts verdrahtet, nur ein ESP32 mit Strom.

Jedes Gerät ist ein **Plugin**. Welche in die Firmware kommen, legst du in `config.h` bzw. `config_local.h` fest. Neue Bluetooth-Geräte kommen als weiteres Plugin dazu und nutzen den fertigen Rahmen (WLAN, MQTT, Web-Oberfläche, Bluetooth-Verbindung, Updates).

Früher „Toolbox“ und davor ein eigenes Repo (`hflocki/camper_toolbox`), jetzt Teil von VanNode mit derselben Update-Absicherung wie die übrigen Geräte. Ein ESP32 für alle Bluetooth-Geräte im Camper, jedes als Plugin.

Wie die Web-Oberfläche aussieht, zeigt das klickbare Mockup [`docs/mockups/ble.html`](../../docs/mockups/ble.html) (herunterladen und im Browser öffnen).

## Geräte und Plugins

Alle Plugin-Typen sind in der Release-`.bin` enthalten. Ab Werk ist kein Gerät angelegt – in der Web-Oberfläche unter **Geräte** wählst du einen Typ und tippst **+ Gerät hinzufügen**. Das neue Gerät öffnet sich gleich mit seinen Einstellungen; es läuft nach **Jetzt neu starten** (mehrere neue Geräte brauchen nur einen Neustart). **Einstellen** neben jedem Gerät öffnet dessen Einstellungen, **← Alle Geräte** führt zurück zur Liste. Vom selben Typ gehen beliebig viele, sie heißen dann z. B. „Dachhaube“ und „Dachhaube 2“ (IDs `rvtech`, `rvtech2`). **✕** entfernt ein Gerät samt Einstellungen und Einträgen in Home Assistant. Mit dem Schalter lässt sich ein Gerät auch nur vorübergehend ausschalten, die Einstellungen bleiben.

| Typ | Rubrik | Einstellungen in der Web-UI |
| --- | --- | --- |
| Dachhaube RV Tech (Heki-Motorsteuerung) | Verbinden | MAC, PIN |
| Wasserpumpe CaraWater (ISP 15; GATT FF00, FFF0 oder FFE0 wird selbst erkannt) | Verbinden | MAC |
| Kühlschrank WCS (Kompressor) | Verbinden | MAC, PIN, optional UUIDs (Standard FFE0/FFE1) / Abfrage |
| Gas: Rotarex DIMES / SRG WAVE | Mithören | MAC, Flaschengröße, Füllstopp |
| Gas: Mopeka Pro (Pro Check, Universal …) | Mithören | MAC, Leer-/Voll-Höhe |
| Gas: Mopeka Standard / Std Check | Mithören | MAC, Leer-/Voll-Höhe, Propan-Anteil |
| Thermometer Govee (H5072, H5074, H5075, H5101, H5102, H5104, H5174, H5177, H5179; Modell wird erkannt) | Mithören | MAC, Korrektur Temperatur/Feuchte |
| **BTHome-Sensor** (ATC/pvvx-Firmware, Shelly BLU H&T / Door / Button / Motion …, BTHome v2 ohne Verschlüsselung) | Mithören | MAC, die Werte werden erkannt (siehe unten) |
| **Eigener BLE-Sensor** | Mithören | MAC, Datenquelle, bis zu 3 Werte mit Stelle und Umrechnung (siehe unten) |

- **Mithören:** Sensoren, die nur senden. Beliebig viele gleichzeitig.
- **Verbinden:** Geräte mit fester Bluetooth-Verbindung. Höchstens **3 gleichzeitig** eingeschaltet (Grenze von NimBLE). Die Web-Oberfläche sperrt weitere, das Gerät prüft es beim Speichern und beim Start noch einmal.
- Insgesamt höchstens **12 Geräte** (Arbeitsspeicher, `MAX_PLUGINS`).
- **Mehr Geräte?** Einfach einen zweiten BLE-Node nehmen und ihm in der Web-Oberfläche (Reiter **MQTT**) ein eigenes Topic geben, z. B. `camper/ble2`. Home Assistant führt ihn dann als eigenes Gerät (`camper_ble2`).
- **Gas ist optional:** Im VanNode-System liest [`tanklevel`](../tanklevel) Gas und Tanks, Display und Road Buddy nehmen die Werte von dort. Die Gas-Plugins hier sind für alle, die nur Bluetooth-Geräte haben und mit einem einzigen ESP auskommen wollen. Ihre Werte stehen unter `camper/ble/<gas>/…` und erscheinen in Home Assistant, nicht am Display.
- **Update von 1.3 / 1.4:** Eingeschaltete Plugins (z. B. die Dachhaube) werden beim ersten Start als Geräte übernommen, mit allen Einstellungen. Die leeren Plätze aus 1.4 verschwinden.
- Einzelne Typen weglassen (kleinere Firmware): in `config_local.h` z. B. `#define PLUGIN_MOPEKA_STD 0`.

### BTHome-Sensor

[BTHome](https://bthome.io) ist ein offenes Funkformat: jeder Wert im Paket trägt eine Kennung (Temperatur, Feuchte, Batterie, Tür offen, Taster …). Das Plugin zerlegt das Paket selbst, eingestellt wird nur die MAC.

1. Gerät **BTHome-Sensor** hinzufügen, **Suchen** und den Sensor wählen (passende Geräte senden den Dienst `FCD2` und sind mit „✔ passt“ markiert).
2. Nach dem Neustart erscheinen die Werte, sobald das erste Paket kommt. Sie stehen dann unter **Erkannte Werte** (z. B. `01,02,03,0C`), damit Home Assistant sie auch nach einem Neustart gleich kennt. Zum Neu-Erkennen das Feld leeren und speichern.
3. Werte unter `camper/ble/<id>/<wert>`, z. B. `…/bthome/temperature`, `humidity`, `battery`, `voltage`; Tür, Fenster, Bewegung … als `ON`/`OFF`; Taster als Text (`press`, `double_press`, `long_press` …).

- **ATC/pvvx** (Xiaomi LYWSD03MMC u. a.): im Telink-Flasher *Advertising type* **BTHome** wählen (Werkseinstellung neuerer Versionen).
- **Verschlüsselte** Sensoren (Shelly BLU mit gesetztem Schlüssel) werden nicht gelesen, die Live-Seite zeigt dann „Verschlüsselt“.
- Höchstens 8 Werte je Sensor (`BTHOME_VALUES`).

### Eigener BLE-Sensor

Viele einfache Sensoren (Thermometer, Füllstand, Kontakte …) verbinden sich nicht, sondern funken alle paar Sekunden ein kurzes Datenpaket mit ihren Messwerten an festen Stellen. Mit diesem Plugin trägst du die Stellen selbst ein – ganz ohne neue Firmware:

1. Gerät **Eigener BLE-Sensor** hinzufügen, **Suchen** und den Sensor in der Liste wählen.
2. **Datenquelle:** *Herstellerdaten* (die Rohdaten beginnen nach den 2 Bytes der Hersteller-ID, wie in nRF Connect) oder *Service-Daten* einer UUID, z. B. `181A`.
3. Auf der Live-Seite unter **Rohdaten** stehen die empfangenen Bytes mit ihren Positionen (`0: 56 34 12 38  4: C1 A4 5C 08 …`).
4. Je Wert eintragen: **Name**, **Stelle** (0 = erstes Byte), **Länge** (1, 2 oder 4 Byte), **mit Vorzeichen** (für Werte unter null), **höchstes Byte zuerst** (selten, meist aus), **Faktor** und **Offset** (Wert = Rohzahl × Faktor + Offset), **Nachkommastellen**, **Einheit** und **Art** (Temperatur, Luftfeuchte … – für Symbol und Statistik in Home Assistant).

Beispiel: Xiaomi-Thermometer LYWSD03MMC mit der Firmware von pvvx (Service-Daten `181A`):

| Wert | Stelle | Länge | Vorzeichen | Faktor | Einheit | Art |
| --- | --- | --- | --- | --- | --- | --- |
| Temperatur | 6 | 2 Byte | ja | 0,01 | °C | Temperatur |
| Luftfeuchte | 8 | 2 Byte | nein | 0,01 | % | Luftfeuchte |
| Batterie | 12 | 1 Byte | nein | 1 | % | Batterie |

Die Stellen eines Sensors findest du mit nRF Connect (Werte am Sensor ändern und schauen, welche Bytes sich bewegen) oder in Projekten wie Theengs Decoder und ESPHome. **Verschlüsselte Sensoren** (z. B. Victron, Xiaomi mit Original-Firmware) lassen sich so nicht lesen. Werte erscheinen unter `camper/ble/<id>/v1` … `v3`, Home Assistant bekommt je Wert eine Entität mit dem eingetragenen Namen.

## Hardware

**ESP32-C3 Super Mini** (so baut die CI), ESP32-S3 und klassischer ESP32 gehen ebenso. Es wird nichts angeschlossen. Der ESP sollte in Bluetooth-Reichweite der Geräte sitzen, für die Dachhaube also oben im Fahrzeug.

## Arduino IDE

**Board-Einstellungen**

| Einstellung | Wert |
| --- | --- |
| Board | ESP32C3 Dev Module |
| USB CDC On Boot | **Enabled** |
| Partition Scheme | **Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)** – WLAN und Bluetooth brauchen gut 1,4 MB |

Board-Paket: esp32 von Espressif Systems, 3.3.x (CI: 3.3.2).

**Bibliotheken**

| Bibliothek | Autor | Version |
| --- | --- | --- |
| PubSubClient | Nick O'Leary | 2.8 |
| ArduinoJson | Benoit Blanchon | 7.4.2 |
| ESP Async WebServer | ESP32Async | 3.12.1 |
| Async TCP | ESP32Async | 3.5.0 |
| NimBLE-Arduino | h2zero | 2.5.1 |

**Flashen**

1. `config_local.h.example` nach `config_local.h` kopieren und WLAN, MQTT und Passwörter eintragen (die Datei geht nicht nach GitHub).
2. `ble_node/ble_node.ino` öffnen, Board-Einstellungen prüfen, per USB hochladen.
3. `http://<ip>/` öffnen. Solange die Web-Oberfläche fehlt, steht dort ein Upload-Formular: `data/index.html.gz` hochladen.
4. Ab dann Updates nur noch über die Web-UI (Tab **System → Firmware- & Web-Update**).

Ohne eingetragenes WLAN öffnet der ESP das Setup-WLAN `BLE-Node-XXXXXX` (Passwort `blenode1234`), die Seite liegt dann unter `http://192.168.4.1`.

Nach einer Änderung an `web/index.html` im Repo-Hauptordner `python3 tools/web/pack_web.py` ausführen (packt nach `data/index.html.gz`).

## Bluetooth-Geräte einrichten

**MAC-Adresse und PIN werden in der Web-UI eingetragen**, in den Einstellungen des jeweiligen Plugins, nicht in `config_local.h`.

1. Plugin aktivieren (Standard bzw. `config_local.h`) und flashen.
2. Web-UI → Plugin → **„Suchen“** → Gerät aus der Liste wählen (übernimmt die MAC).
3. Hat das Gerät eine **PIN** (Dachhaube, Kühlschrank): eintragen (1–6 Ziffern, auch `000000`), dieselbe wie in der Hersteller-App.
4. **Handy-App trennen** (App schließen oder Bluetooth am Handy aus): Die Geräte erlauben nur eine Verbindung. Später kann das Handy über **„Bluetooth für App freigeben“** wieder ran (endet nach 30 min von selbst).

| Status | Bedeutung / was tun |
| --- | --- |
| `Nicht erreichbar` | Gerät nicht in Reichweite oder noch mit dem Handy verbunden |
| `Kopplung fehlgeschlagen` | PIN falsch oder Gerät noch an ein Handy gekoppelt |
| `PIN nötig?` | Gerät verlangt eine Kopplung, aber keine (oder eine falsche) PIN eingetragen |
| `Falsches Gerät?` | Erwartete Bluetooth-Dienste nicht gefunden – andere MAC gewählt? |
| `Warte auf Daten` | Verbunden, aber noch keine Werte |
| `Verbunden` | Alles in Ordnung |

## MQTT / Home Assistant

Basis-Topic `camper/ble` (in der Web-UI änderbar). Jedes Plugin hat eigene Topics unter `camper/ble/<plugin>/…`, Befehle unter `camper/ble/cmd/<plugin>/<key>/set`. Dazu `…/status` (Last Will) und `…/info` (Diagnose) wie bei allen Geräten. Alle Topics: [`docs/TOPICS.md`](../../docs/TOPICS.md#toolbox-campertoolbox).

**Dachhaube** (`rvtech`):

| Topic | Inhalt |
| --- | --- |
| `camper/ble/rvtech/cover` | `open` / `closed` / `opening` / `closing` |
| `camper/ble/rvtech/cover_position` | 0 … 100 % |
| `camper/ble/cmd/rvtech/cover/set` | `OPEN` / `CLOSE` |
| `camper/ble/cmd/rvtech/cover_position/set` | `0` … `100` |
| `camper/ble/cmd/rvtech/regen/set` | Voreinstellung Regen (ebenso `auf`, `zu`, `lueften`) |

In Home Assistant erscheint ein Gerät „BLE Node“ (WLAN, Status) und je Plugin ein eigenes Gerät, die Dachhaube als Abdeckung (`cover`).

> **Umstieg von der Toolbox (bis 1.2.x):** Das Gerät stellt ein gespeichertes `camper/toolbox` beim ersten Start selbst auf `camper/ble` um, das Display (ab 2.16.2) ebenso. Danach die alten retained Topics und die alten HA-Entitäten löschen: `python3 tools/mqtt/clean.py --host <broker> camper/toolbox homeassistant/sensor/camper_toolbox …` bzw. die Geräte „Toolbox Node“ in Home Assistant entfernen.

> Wer die Toolbox schon aus dem alten Repo nutzt: Das Basis-Topic war `camper/tool`, der Hostname `camper-tool`. Ein Gerät mit gespeicherten Einstellungen behält sein altes Topic, bis es in der Web-UI geändert wird. Ändert man es, legt Home Assistant die Entitäten neu an.

### Dachhaube am Display

Ab display-node 2.10.3 hat das Display eine eigene Seite **Dachhaube** und eine kleine Kachel für die Übersicht (beide ab Werk aus, einschalten unter *Einstellungen → Anzeige*). Sie lesen `camper/ble/rvtech/…` und senden die Voreinstellungen und die Position an `camper/ble/cmd/rvtech/…`. Eigene Kacheln dafür braucht es nicht mehr; wer sie angelegt hat, kann sie löschen.

- Basis-Topic am Display: *Einstellungen → Topics der Geräte → BLE-Node* (Standard `camper/ble`; ein Gerät aus dem alten Repo sendet auf `camper/tool`).
- Eine zweite Haube (`rvtech2`) kennt das Display noch nicht, ebenso der Abfahrtscheck.

## Neues Plugin schreiben

1. Neue Datei `src/plugins/plugin_<name>.cpp`, Klasse von `Plugin` ableiten (`src/core/plugin.h`): Einstellungen `fields()`, HA-Entitäten `entities()`, MQTT-Werte `publish()`, Befehle `command()`, Web-Karte `live()`. Für Geräte mit Verbindung gibt es `BleLink` (`src/core/ble_link.h`), für passive Sensoren `onAdvertisement()`.
2. Fabrik `Plugin* create<Name>(uint8_t index)` am Ende der Datei.
3. `config.h`: Schalter `PLUGIN_<NAME> 1` und in `CT_BLE_PASSIVE` bzw. `CT_BLE_LINKS` aufnehmen.
4. `src/core/registry.cpp`: Fabrik deklarieren und eine Zeile in `TYPES` (Typ-ID, Name in der Web-UI, Bluetooth-Art). Danach steht der Typ unter „+ Gerät hinzufügen“ zur Auswahl.
5. Topics in `docs/TOPICS.md` eintragen, Plugin-Tabelle oben ergänzen.

Die Web-Oberfläche baut Karten, Knöpfe und Einstellungen aus `live()` und `fields()` selbst und braucht in der Regel keine Änderung.

## Serieller Monitor

115200 Baud. Befehle: `read` (alle Werte), `scan` (Bluetooth-Geräte in der Nähe), `noauth` (vergessenen Web-Login löschen), `help`.

Für eine Fehlermeldung den Monitor vom Einschalten an mitschneiden, bis das Problem auftritt (beim Kühlschrank mindestens 3 Minuten). Dort stehen u. a. Verbindungsaufbau und Trennungsgrund (`Reason 0x…`), das Ergebnis der Kopplung (`Security: …`), alle Dienste des Geräts (`GATT table:`) und die ersten empfangenen Pakete (`RX … bytes`). Den Log als Text schicken, MAC-Adressen dürfen geschwärzt werden.

## Sicheres Update, Ereignisse und Rückfall

Jede Firmware hat eine Kennung (`CAMPER-FW:toolbox:<version>:<chip>;`, bleibt `toolbox`, damit Geräte mit der Toolbox-Firmware das Update annehmen) und dieselbe Absicherung wie alle VanNode-Geräte (`fwcheck.*`, `evlog.*`, `safeguard.*` aus `firmware/_common`):

- **Prüfung beim Update** (Tab **System → Firmware- & Web-Update**): Die installierte Version steht dabei. Eine gewählte Datei wird schon im Browser und beim Hochladen im Gerät geprüft. Abgelehnt werden eine `.merged.bin`, Firmware für ein anderes Gerät oder einen anderen Chip und zu große Dateien. Firmware aus dem alten Repo (ohne Kennung) geht nur mit „Ohne Kennung hochladen“.
- **Ereignisse:** Start mit Grund, WLAN und MQTT, Uploads mit Ergebnis, Rückfall, gespeicherte Einstellungen. Bleibt bei Neustart, Absturz und Update erhalten, nicht bei Stromausfall.
- **Rückfall nach Update:** Hat eine neue Firmware nach 5 Minuten kein WLAN oder startet sie mehr als dreimal neu, startet wieder die vorherige.
- **WLAN-Änderung mit Rückweg:** Klappt ein geänderter WLAN-Zugang nach dem Neustart etwa 1 Minute lang nicht, kommen die alten Daten zurück.

Ein Update ersetzt nur das Programm: WLAN, MQTT, Login, Plugin-Einstellungen (MAC, PIN …) und Bluetooth-Kopplungen bleiben erhalten.

**Umstieg vom alten Repo:** Die alte Firmware (0.1.x) prüft nichts und meldet bei Fehlern nur „Update failed“. Die neue `.bin` (`ble-node-esp32c3.bin`) über das alte Update-Feld hochladen, danach `data/index.html.gz`. Die `-full.bin` bzw. `.merged.bin` nur per USB.

## Versionen

| Version | Änderungen |
| --- | --- |
| 1.5.4 | Neues Plugin **BTHome-Sensor** (Mithören): alle Sensoren mit BTHome v2 ohne Verschlüsselung, z. B. ATC/pvvx-Thermometer und Shelly BLU; Werte werden aus dem Paket erkannt und gemerkt, Home Assistant bekommt passende Sensoren und Binärsensoren. Host-Test der Dekodierung (`tests/bthome_test.cpp`) |
| 1.5.3 | Neues Plugin **Thermometer Govee** (Mithören): Temperatur (auch unter null), Luftfeuchte und Batterie von H5072, H5074, H5075, H5101, H5102, H5104, H5174, H5177 und H5179, nur MAC wählen; Korrektur für Temperatur und Feuchte. Host-Test der Dekodierung (`tests/govee_test.cpp`) |
| 1.5.2 | Neuer Reiter **Geräte** statt eines Reiters je Gerät: Liste mit An/Aus, Hinzufügen, Entfernen und Neustart, je Gerät **Einstellen** (Einstellungen jetzt auch für ausgeschaltete Geräte). Ein neues Gerät öffnet sich gleich mit seinen Einstellungen und dem Knopf „Jetzt neu starten“. Behoben: Mit mehreren Eigenen BLE-Sensoren wurde `/api/config` zu groß für den Speicher, die Seite zeigte „CFG.plugins is not iterable“ und keine Rückmeldung beim Hinzufügen – die Feldbeschreibung kommt jetzt nur einmal je Typ (`schemas`), die Antwort wird gestreamt, bei Speichermangel kommt eine klare Meldung. Knöpfe sind gesperrt, solange eine Änderung unterwegs ist (kein doppeltes Hinzufügen) |
| 1.5.1 | Änderungen unter System → Plugins (hinzufügen, entfernen, an/aus) werden gespeichert, ohne sofort neu zu starten; neue und entfernte Geräte sind markiert, darunter erscheint **Jetzt neu starten** – mehrere Änderungen brauchen nur einen Neustart. Neue Geräte lassen sich schon vor dem Neustart einstellen, entfernte bis dahin mit „Rückgängig“ behalten. Gewollte Neustarts (Einstellungen speichern, Neustart-Knopf, WLAN-Rückfall) zählen in der Update-Probezeit nicht mehr als Absturz – vorher konnten drei davon ein frisches Update auf die alte Version zurückrollen (so ging 1.5.0 nach dem Hinzufügen auf 1.3.0 zurück) |
| 1.5.0 | Geräte frei anlegen statt fester Plätze: „+ Gerät hinzufügen“ und „✕“ unter System → Plugins, beliebig viele vom selben Typ (insgesamt 12, höchstens 3 mit Verbindung). Eingeschaltete Plugins aus 1.3 / 1.4 werden übernommen. Neues Plugin **Eigener BLE-Sensor**: Werte an frei einstellbaren Stellen im Funkpaket, ohne neue Firmware. Rohdaten mit Positionen, Reiter-Leiste am Handy scrollbar. Behoben: Mehr als 8 Plugins wurden nicht alle an MQTT gesendet (in 1.4.0 der Kühlschrank). Host-Test der Umrechnung (`tests/run.sh`, auch in der CI) |
| 1.4.0 | Alle Plugins in der Release-`.bin` (Plätze: 2 × DIMES, 2 × Mopeka Pro, 1 × Mopeka Standard, 2 × Dachhaube, 1 × Pumpe, 1 × Kühlschrank), ab Werk aus. System → Plugins in zwei Rubriken *Mithören* und *Verbinden* (höchstens 3 gleichzeitig, wird beim Speichern und beim Start geprüft). Gas-Plugins mit eigenem Namen je Typ |
| 1.3.0 | Aus der Toolbox wird der **BLE Node**: Hostname `ble-node`, Setup-WLAN `BLE-Node-XXXXXX`, Topic `camper/ble` (ein gespeichertes `camper/toolbox` wird beim Start umgestellt), Release-Datei `ble-node-….bin`, Tag `ble-v…`, Ordner `firmware/ble/ble_node`. Gas-Plugins DIMES und Mopeka wieder als Option (ab Werk aus). Home Assistant legt die Entitäten neu an (`camper_ble`) |
| 1.2.2 | VanNode-Symbol links neben der Überschrift der Webseite und als Favicon |
| 1.2.1 | Pumpe: dritte GATT-Variante FFE0/FFE1, „läuft“ aus dem Statusbit, Druck aus psi × 10 umgerechnet, Statusrahmen anderer Pumpen werden angenommen. Dachhaube: robusteres Zusammensetzen der Telegramme. Kühlschrank: ohne eingetragene UUIDs FFE0/FFE1 statt aller Characteristics |
| 1.2.0 | Neue Namen: Hostname `toolbox-node`, Setup-WLAN, Gerätename und Release-Datei `toolbox-node-….bin`. Topics `camper/…` und Home-Assistant-IDs bleiben |
| 1.1.0 | Gas-Plugins entfernt (DIMES, Mopeka Pro, Mopeka Standard, gemeinsamer Gas-Teil): Gasflasche und Tanks liest camper-tanklevel. Übrig: Dachhaube, Wasserpumpe, Kühlschrank |
| 1.0.1 | Web-Oberfläche im Aussehen der übrigen VanNode-Geräte (Farben, Kopfzeile mit Statuspunkten, Tab-Leiste mit Symbolen, Werte als Kacheln, Schalter statt Häkchen) |
| 1.0.0 | Ins VanNode-Repo übernommen: sicheres Update (Kennung, Prüfung im Browser und im Gerät), Ereignis-Log, Rückfall und WLAN-Rückweg, Topic `info`; Basis-Topic `camper/toolbox`, Hostname `camper-toolbox`; ab Werk nur die Dachhaube (RV Tech) |
| 0.1.x | Eigenes Repo `camper_toolbox`: Plugins Gas (DIMES, Mopeka), Wasserpumpe, Dachhaube, Kühlschrank |

## Fertige Firmware

Wird im Haupt-Repo per GitHub Actions gebaut, Tag `ble-v<version>`, Datei `ble-node-esp32c3.bin`. Siehe [Fertige Firmware](../../README.md#fertige-firmware).

## Lizenz

MIT, siehe [LICENSE](LICENSE). Das Dachhauben-Protokoll ist aus der Home-Assistant-Integration rvtech-hacs portiert.
