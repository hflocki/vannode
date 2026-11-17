> [!WARNING]
> **IN ENTWICKLUNG.**

# BLE Node / Battery Node

> **Achtung:** Ohne Gewähr, Nutzung auf eigene Gefahr – siehe [Haftungsausschluss](../../README.md#haftungsausschluss--nutzung-auf-eigene-gefahr). Pumpe und Dachhaube werden aus der Ferne geschaltet – vorher sicherstellen, dass niemand gefährdet wird; Batteriewerte sind kein Schutz für die Batterie.

Ein ESP32 als **Bluetooth-Gateway mit Plugins**: Er liest Bluetooth-Geräte im Camper aus bzw. steuert sie und stellt alles über eine Web-Oberfläche und MQTT (mit Home-Assistant-Discovery) bereit. Es wird nichts verdrahtet, nur ein ESP32 mit Strom.

Jedes Gerät ist ein **Plugin**. Welche in die Firmware kommen, legst du in `config.h` bzw. `config_local.h` fest. Neue Bluetooth-Geräte kommen als weiteres Plugin dazu und nutzen den fertigen Rahmen (WLAN, MQTT, Web-Oberfläche, Bluetooth-Verbindung, Updates).

Früher „Toolbox“ und davor ein eigenes Repo (`hflocki/camper_toolbox`), jetzt Teil von VanNode mit derselben Update-Absicherung wie die übrigen Geräte. Ein ESP32 für alle Bluetooth-Geräte im Camper, jedes als Plugin.

**Eine Firmware, zwei Rollen.** Seit 2.0 läuft dieselbe Firmware auch auf dem **battery-node** (vorher eigene Firmware `firmware/battery`). Welche Rolle ein Gerät hat, steht nur in seinen Einstellungen (Reiter **MQTT**):

| Rolle | Gerätename | Topic | Geräte darauf |
| --- | --- | --- | --- |
| battery-node | `battery-node` | `camper/battery` | Daly BMS, Ective BM X, BM2 |
| tool-node | `tool-node` | `camper/tool` | Dachhaube, Pumpe, Kühlschrank, Alarm (nur lesen), OBD2-Adapter, Thermometer, Gas, eigene Sensoren |

Die Batterien bekommen damit eigene Verbindungsplätze und laufen weiter, auch wenn am anderen Node etwas hängt. Technisch kann jeder Node jeden Typ – für einen dritten Node reicht ein weiterer ESP mit eigenem Namen und Topic (z. B. `tool2-node`, `camper/tool2`).

Wie die Web-Oberfläche aussieht, zeigt das klickbare Mockup [`docs/mockups/ble.html`](../../docs/mockups/ble.html) (herunterladen und im Browser öffnen).

## Geräte und Plugins

Alle Plugin-Typen sind in der Release-`.bin` enthalten. Ab Werk ist kein Gerät angelegt – in der Web-Oberfläche unter **Geräte** wählst du einen Typ und tippst **+ Gerät hinzufügen**. Das neue Gerät öffnet sich gleich mit seinen Einstellungen; es läuft nach **Jetzt neu starten** (mehrere neue Geräte brauchen nur einen Neustart). **Einstellen** neben jedem Gerät öffnet dessen Einstellungen, **← Alle Geräte** führt zurück zur Liste. Vom selben Typ gehen beliebig viele, sie heißen dann z. B. „Dachhaube“ und „Dachhaube 2“ (IDs `rvtech`, `rvtech2`). **✕** entfernt ein Gerät samt Einstellungen und Einträgen in Home Assistant. Mit dem Schalter lässt sich ein Gerät auch nur vorübergehend ausschalten, die Einstellungen bleiben.

| Typ | Rubrik | Einstellungen in der Web-UI |
| --- | --- | --- |
| Dachhaube RV Tech (Heki-Motorsteuerung; Auf, Zu, Regen, Lüften, Favorit, Favorit speichern, Stopp) | Verbinden | MAC, PIN, Stellungen Regen / Lüften / Favorit in % (je Haube) |
| Wasserpumpe CaraWater (ISP 10/15/20/51, an/aus und Druckstufe; GATT FF00, FFF0 oder FFE0 wird selbst erkannt) | Verbinden | MAC |
| Kühlschrank WCS (Kompressor; Ein/Aus, Zonen einzeln ein/aus, Umluft, Nachtmodus, Soll-Temperatur je Zone, Batterieschutz, Fehlercodes mit Verlauf, Schnellkühlen je Zone, Türheizung) | Verbinden + App-Passwort | MAC, App-Passwort der WCS-App (Werkseinstellung `1234`, leer = `1234`), Namen der Zonen (Standard Gefrierfach / Kühlfach), optional UUIDs (Standard Service E530, Write E531, Notify alle; `*` = alle Services) / Abfrage. Keine Bluetooth-Kopplung |
| Alarm Thitronik WiPro III (**nur lesen**) | Verbinden | MAC, PIN normalerweise leer, Kontaktnamen (mit Komma), optional Status-UUID. Reiner Statusmelder: schaltet die Anlage nie scharf/unscharf |
| Fahrzeug: OBD2-Adapter ELM327 / vLinker (**nur lesen**; Standardwerte des Motorsteuergeräts, Fehlercodes lesen) | Verbinden | MAC, Bluetooth-Kanäle (Automatisch), Abfragen nur bei laufendem Motor oder immer, Spannung „Motor läuft ab“, Abfrageintervall, Ruhepause (siehe unten) |
| Batterie: Daly BMS (BLE-Name `DL-…`, Protokoll D2 oder 0x81 wird erkannt) | Verbinden | MAC, Abfrageintervall |
| Batterie: Ective BM X (LiPower, Geräte-ID wird erkannt) | Verbinden | MAC, Abfrageintervall |
| Batterie: BM2 Batteriewächter (z. B. Starterbatterie, nur Spannung und Ladestand) | Verbinden (nur falls kein Funk) | MAC |
| Gas: Rotarex DIMES / SRG WAVE | Mithören | MAC, Flaschengröße, Füllstopp |
| Gas: Mopeka Pro (Pro Check, Universal …) | Mithören | MAC, Leer-/Voll-Höhe |
| Gas: Mopeka Standard / Std Check | Mithören | MAC, Leer-/Voll-Höhe, Propan-Anteil |
| Thermometer Govee (H5072, H5074, H5075, H5101, H5102, H5104, H5174, H5177, H5179; Modell wird erkannt) | Mithören | MAC, Korrektur Temperatur/Feuchte |
| **BTHome-Sensor** (ATC/pvvx-Firmware, Shelly BLU H&T / Door / Button / Motion …, BTHome v2 ohne Verschlüsselung) | Mithören | MAC, die Werte werden erkannt (siehe unten) |
| **Eigener BLE-Sensor** | Mithören | MAC, Datenquelle, bis zu 3 Werte mit Stelle und Umrechnung (siehe unten) |

- **Mithören:** Sensoren, die nur senden. Beliebig viele gleichzeitig.
- **Verbinden:** Geräte mit fester Bluetooth-Verbindung. Höchstens **3 gleichzeitig** eingeschaltet (Grenze von NimBLE). Die Web-Oberfläche sperrt weitere, das Gerät prüft es beim Speichern und beim Start noch einmal.
- Insgesamt höchstens **12 Geräte** (Arbeitsspeicher, `MAX_PLUGINS`).
- **Mehr Geräte?** Einfach einen zweiten BLE-Node nehmen und ihm in der Web-Oberfläche (Reiter **MQTT**) ein eigenes Topic geben, z. B. `camper/tool2`. Home Assistant führt ihn dann als eigenes Gerät (`camper_tool2`).
- **Gas ist optional:** Im VanNode-System liest [`tanklevel`](../tanklevel) Gas und Tanks, Display und Pocket-Node nehmen die Werte von dort. Die Gas-Plugins hier sind für alle, die nur Bluetooth-Geräte haben und mit einem einzigen ESP auskommen wollen. Ihre Werte stehen unter `camper/tool/<gas>/…` und erscheinen in Home Assistant, nicht am Display.
- **Update von 1.3 / 1.4:** Eingeschaltete Plugins (z. B. die Dachhaube) werden beim ersten Start als Geräte übernommen, mit allen Einstellungen. Die leeren Plätze aus 1.4 verschwinden.
- Einzelne Typen weglassen (kleinere Firmware): in `config_local.h` z. B. `#define PLUGIN_MOPEKA_STD 0`.

### Batterien (battery-node)

Die Batterie-Plugins sind die frühere battery-node-Firmware. Die Protokolle stammen aus [aiobmsble](https://github.com/patman15/aiobmsble) von Patrick Loschmidt (patman15), der Bibliothek hinter der Home-Assistant-Integration *BLE Battery Management* (Apache-2.0, `src/plugins/bms_proto.*`, [LICENSE-Apache-2.0](LICENSE-Apache-2.0)).

- **Daly / Ective:** feste Verbindung, Abfrage alle *Abfrageintervall* Sekunden (5–300, Standard 10). Ladestand, Spannung, Strom, Leistung, Restkapazität, Energie, Restlaufzeit; bei Daly zusätzlich Zellspannungen, Zelldifferenz, Temperatur, Zyklen und Fehler als Text.
- **BM2:** neuere Geräte senden ihre Werte per Funk, dann bleibt der Verbindungsplatz frei („OK (Funk)“). Ältere ohne Funkwerte werden verbunden. Zustand aus der Spannung (Normal, Schwach, Kritisch, Lädt, Erhaltungsladung).
- Solange der ESP verbunden ist, kommt die Hersteller-App meist nicht dran: **Bluetooth für App freigeben** am Gerät (endet nach 30 min von selbst).
- Für die Displays steht die Liste aller Batterien unter `<prefix>/batteries` (wie bisher), Einzelwerte unter `<prefix>/<geräte-id>/…` (`daly`, `ective`, `bm2` …), siehe [TOPICS](../../docs/TOPICS.md#batterien-camperbattery).
- **Status-LED** (AtomS3 Lite, sonst `STATUS_LED_PIN`): grün = alle Geräte liefern, gelb = eins fehlt, rot blinkend = kein WLAN/MQTT, blau = noch kein Gerät.

**Umstieg vom alten battery-node (1.x):** Die Update-Seite der alten Firmware nimmt die neue `.bin` nicht an (anderes Gerät), deshalb **einmal per USB flashen** – mit *Erase All Flash Before Sketch Upload: Disabled*, dann bleiben die Einstellungen erhalten. Beim ersten Start werden WLAN, MQTT (Topic `camper/battery`) und Login übernommen, jede eingetragene Batterie wird ein Gerät (`daly`, `ective`, `bm2` …) mit Name, MAC und Abfrageintervall, und der Gerätename wird `battery-node`. Danach `data/index.html.gz` hochladen. Geändert: Die Einzel-Topics heißen jetzt nach der Geräte-ID statt der frei gewählten ID (`camper/battery/daly/soc` statt z. B. `camper/battery/house/soc`); Display und Road Buddy lesen nur die Liste `batteries` und merken davon nichts. In Home Assistant erscheinen die Batterien neu (alte Einträge von Hand löschen).

### BTHome-Sensor

[BTHome](https://bthome.io) ist ein offenes Funkformat: jeder Wert im Paket trägt eine Kennung (Temperatur, Feuchte, Batterie, Tür offen, Taster …). Das Plugin zerlegt das Paket selbst, eingestellt wird nur die MAC.

1. Gerät **BTHome-Sensor** hinzufügen, **Suchen** und den Sensor wählen (passende Geräte senden den Dienst `FCD2` und sind mit „✔ passt“ markiert).
2. Nach dem Neustart erscheinen die Werte, sobald das erste Paket kommt. Sie stehen dann unter **Erkannte Werte** (z. B. `01,02,03,0C`), damit Home Assistant sie auch nach einem Neustart gleich kennt. Zum Neu-Erkennen das Feld leeren und speichern.
3. Werte unter `camper/tool/<id>/<wert>`, z. B. `…/bthome/temperature`, `humidity`, `battery`, `voltage`; Tür, Fenster, Bewegung … als `ON`/`OFF`; Taster als Text (`press`, `double_press`, `long_press` …).

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

Die Stellen eines Sensors findest du mit nRF Connect (Werte am Sensor ändern und schauen, welche Bytes sich bewegen) oder in Projekten wie Theengs Decoder und ESPHome. **Verschlüsselte Sensoren** (z. B. Victron, Xiaomi mit Original-Firmware) lassen sich so nicht lesen. Werte erscheinen unter `camper/tool/<id>/v1` … `v3`, Home Assistant bekommt je Wert eine Entität mit dem eingetragenen Namen.

### OBD2-Adapter (Fahrzeug)

Liest Werte des Basisfahrzeugs über einen OBD2-Adapter mit **Bluetooth LE** und ELM327-Befehlssatz: günstige ELM327-BLE-Adapter oder Adapter mit STN-Chip wie der **vLinker MC+** (im BLE-Modus). Adapter nur mit klassischem Bluetooth (SPP) gehen nicht, der ESP32-C3 kann nur BLE.

- **Nur lesen:** Das Plugin fragt Werte und Fehlercodes ab (OBD2 Mode 01 und 03). Es löscht keine Fehler und ändert nichts am Fahrzeug.
- **Werte:** Alles, was das Fahrzeug als Standardwert meldet: Drehzahl, Geschwindigkeit, Kühlwasser, Öl, Tankfüllstand, Verbrauch, AdBlue (Füllstand, Temperatur, Konzentration), Motorlast, Außen- und Ansaugluft, Saugrohr- und Luftdruck, Luftmasse, Drosselklappe, Spannung, Laufzeit, Kilometerstand, Motorkontrollleuchte und Fehlercodes. Dazu die Bordspannung, die der Adapter selbst misst. Welche Werte ein Fahrzeug liefert, fragt das Plugin beim ersten Kontakt ab; nur diese erscheinen in Home Assistant.
- **Fahrzeug:** *Standard* passt für alle Fahrzeuge (Protokoll wird gesucht). *Fiat Ducato (Euro 6)* stellt fest CAN 29 Bit / 500 kBit/s ein (`ATSP7`), fragt AdBlue (`0185`, `019B`) direkt beim Steuergerät 01, das diese Werte liefert (sonst antworten zwei Steuergeräte gleichzeitig und die Antwort ist unbrauchbar), nimmt „km seit Fehler löschen“ (`0131`) nur vom Motorsteuergerät und berechnet den Verbrauch aus der Kraftstoffmasse (`019D`). Die Ducato-Werte stammen aus den Erkenntnissen von [Moormerlander/ESPHome-BLE-OBD](https://github.com/Moormerlander/ESPHome-BLE-OBD) (an einem Ducato mit 9-Gang-Automatik geprüft, ohne SGW-Bypass); ältere Baujahre sind ungeprüft. Ob ein Ducato AdBlue meldet, zeigt sich nach dem ersten Kontakt (Entität `adblue`).
- **Weitere Herstellerwerte** (DPF, Diesel bei Fahrzeugen ohne Standardwert) brauchen eigene Abfragen je Fahrzeug. Sie kommen als Fahrzeugprofile dazu, sobald sie an einem echten Fahrzeug bestätigt sind. Zum Ausprobieren gibt es den Testbefehl (unten).
- **Starterbatterie schonen:** Ab Werk fragt das Plugin das Fahrzeug nur bei laufendem Motor ab (Bordspannung ab „Motor läuft ab“, Standard 13,2 V, bzw. Drehzahl). Fahrzeuge mit **intelligenter Lichtmaschine** kommen im Leerlauf nicht über 13 V (Ducato: 12,3–12,6 V); dort *Motor erkennen an: Drehzahl* wählen. Dann fragt der Node das Steuergerät jede Minute nach der Drehzahl; zwischendurch gilt die Ruhepause wie unten. Ist der Motor 30 s aus, hört es auf; nach 2 Minuten trennt es die Bluetooth-Verbindung für die **Ruhepause** (Standard 2 min), damit der Adapter schlafen kann, und schaut danach wieder nach. *Abfragen: Immer, wenn das Steuergerät antwortet* liest auch bei Zündung an und stehendem Motor, z. B. den Tankfüllstand.
- **Bluetooth-Kanäle:** *Automatisch* nimmt die Kanäle, die der Adapter anbietet; das Log zeigt nach dem Verbinden alle Dienste. Klappt das nicht, einen der bekannten Aufbauten wählen (FFF0, FFE0, 18F0, Vgate / vLinker) oder *Eigene UUIDs* eintragen.
- **Fahrzeug scannen** (Knopf in der Karte, in Home Assistant unter Diagnose, oder `camper/tool/cmd/obd/scan/set` mit `PRESS`): Bei laufendem Motor (oder Zündung an) fragt der Node mit eingeschalteten Headern, welche Steuergeräte antworten, welche Werte jedes einzelne meldet und welches Steuergerät Diesel, Verbrauch, AdBlue und Kilometer liefert. Dauert etwa 10–30 s. Der Bericht steht im Geräte-Log und unter `camper/tool/obd/scan`; wer ein neues Fahrzeugprofil möchte, schickt diesen Text mit Fahrzeug, Baujahr und Euro-Norm. Der Scan liest nur und enthält keine Fahrgestellnummer.
- **Testbefehl** (nur per MQTT): `camper/tool/cmd/obd/raw/set` mit einem Adapter-Befehl, z. B. `ATRV` oder `0105`. Die Antwort steht im Geräte-Log und unter `camper/tool/obd/raw`. `04` (Fehler löschen) wird abgelehnt. Nach Versuchen mit `ATSH…` (anderes Steuergerät) mit `ATSH7DF` (11 Bit) bzw. `ATSHDB33F1` (29 Bit, Ducato) zurückstellen oder den Adapter neu verbinden.

## Hardware

**ESP32-C3 Super Mini**, **ESP32-S3** (z. B. DevKitC-1) oder **M5Stack AtomS3 Lite** – alle drei baut die CI: `ble-node-esp32c3-v<version>.bin`, `ble-node-esp32s3-v<version>.bin`, `ble-node-atoms3-v<version>.bin`. Der S3 hat mehr Arbeitsspeicher als der C3 und ist die bessere Wahl, wenn viele Bluetooth-Geräte gleichzeitig verbunden sind. Auf ein ESP32-S3-Board nicht die AtomS3-Datei spielen: Sie schaltet eine LED an GPIO35, der auf Boards mit 8 MB PSRAM (N16R8) zum Speicher gehört. Klassischer ESP32 geht ebenso (selbst bauen). Es wird nichts angeschlossen. Der ESP sollte in Bluetooth-Reichweite der Geräte sitzen, für die Dachhaube also oben im Fahrzeug.

## Arduino IDE

Board-Paket, gemeinsame Bibliotheken, Board-Einstellungen, erstes Flashen, LittleFS-Plugin und Hilfe bei Upload-Problemen: **[docs/ARDUINO.md](../../docs/ARDUINO.md)**. Hier nur, was für dieses Gerät gilt.

| Einstellung | Wert |
| --- | --- |
| Boards | ESP32-C3 Super Mini, ESP32-S3 (mehr Speicher für mehrere Bluetooth-Geräte), M5Stack AtomS3 Lite (mit Status-LED) |
| Release-Dateien | `ble-node-esp32c3-…`, `ble-node-esp32s3-…`, `ble-node-atoms3-…` – passend zum Board nehmen |
| Zusätzliche Bibliothek | NimBLE-Arduino 2.5.1 |
| Sketch | `ble_node/ble_node.ino` |
| Web-Oberfläche | einmalig über das Upload-Formular (`http://<ip>/` → `data/index.html.gz`), danach Updates unter **System → Firmware- & Web-Update** |

Ohne eingetragenes WLAN öffnet der ESP das Setup-WLAN `BLE-Node-XXXXXX` (Passwort `blenode1234`), die Seite liegt dann unter `http://192.168.4.1`. Danach unter **MQTT** Gerätename und Topic für die Rolle setzen (z. B. `battery-node` / `camper/battery`).

## Bluetooth-Geräte einrichten

**MAC-Adresse und PIN werden in der Web-UI eingetragen**, in den Einstellungen des jeweiligen Plugins, nicht in `config_local.h`.

1. Plugin aktivieren (Standard bzw. `config_local.h`) und flashen.
2. Web-UI → Plugin → **„Suchen“** → Gerät aus der Liste wählen (übernimmt die MAC).
3. Hat das Gerät eine **PIN** (Dachhaube): eintragen (1–6 Ziffern, auch `000000`), dieselbe wie in der Hersteller-App.
   Der **Kühlschrank** braucht keine Bluetooth-Kopplung, aber das **App-Passwort** der WCS-App (Werkseinstellung `1234`, Feld leer = `1234`). Ohne richtiges Passwort sendet er keine Werte; bei falschem Passwort zeigt der Status „Passwort falsch“, und das Plugin wartet, bis das Passwort in den Einstellungen geändert wird.
4. **Handy-App trennen** (App schließen oder Bluetooth am Handy aus): Die Geräte erlauben nur eine Verbindung. Später kann das Handy über **„Bluetooth für App freigeben“** wieder ran (endet nach 30 min von selbst).

| Status | Bedeutung / was tun |
| --- | --- |
| `Nicht erreichbar` | Gerät nicht in Reichweite oder noch mit dem Handy verbunden |
| `Kopplung fehlgeschlagen` | PIN falsch oder Gerät noch an ein Handy gekoppelt. Mit PIN probiert die Firmware nacheinander fünf Kopplungsarten (Secure Connections, Legacy, Legacy nach der Dienstsuche, ohne Anfrage, wie das ESP32-+-HM-10-Beispiel); klappt keine, wartet sie 5 Minuten, damit die anderen Geräte weiterlaufen |
| `PIN nötig?` | Gerät verlangt eine Kopplung, aber keine (oder eine falsche) PIN eingetragen |
| `Falsches Gerät?` | Erwartete Bluetooth-Dienste nicht gefunden – andere MAC gewählt? |
| `Warte auf Daten` | Verbunden, aber noch keine Werte. Kühlschrank: im Log nachsehen, ob „Password ok“ kommt |
| `Verbunden` | Alles in Ordnung |

## MQTT / Home Assistant

Basis-Topic `camper/tool` (in der Web-UI änderbar). Jedes Plugin hat eigene Topics unter `camper/tool/<plugin>/…`, Befehle unter `camper/tool/cmd/<plugin>/<key>/set`. Dazu `…/status` (Last Will) und `…/info` (Diagnose) wie bei allen Geräten. Alle Topics und Befehle der Plugins (Dachhaube, Pumpe, Kühlschrank, WiPro, OBD2, Thermometer …): **[docs/TOPICS.md](../../docs/TOPICS.md#ble-node-tool-campertool)**.

**Dachhaube:** Die Stellungen für Regen, Lüften und Favorit stellst du je Haube in den Einstellungen ein (zweite Haube `rvtech2` mit eigenen Werten und Topics `…/cmd/rvtech2/…`): **Stellung Regen** (Standard 12 %), **Stellung Lüften** (0 = Lüften-Funktion der Haube, jedes Drücken verlängert die Lüftzeit; sonst fester Wert in %), **Stellung Favorit** (0 = in der Haube gespeicherter Favorit, dann gibt es „Favorit speichern“; sonst fester Wert in %, „Favorit speichern“ entfällt). Die Knöpfe in der Karte zeigen den eingestellten Wert.

**Kühlschrank:** Die Topics heißen nach der Zone (`…1` = Zone 1, `…2` = Zone 2); welches Fach das ist, legen die Felder *Name Zone 1/2* fest (leer = Gefrierfach / Kühlfach, bei einer Zone Innenraum). Die Namen erscheinen in der Karte und in den Home-Assistant-Namen. Die letzten 5 Fehler stehen in der Karte unter „Letzte Fehler“ und als JSON in `…/fridge/errors`.

**Alarm WiPro III** (nur lesen): Die Namen der Kontakte stellst du im Feld **„Kontaktnamen“** ein (mit Komma getrennt); leer bleibt es bei „Kontakt 0“ usw. Das Plugin schaltet die Anlage nie scharf oder unscharf. Es hält die Bluetooth-Verbindung zum Alarm-Modul; mit „Bluetooth für App freigeben“ lässt es die Thitronik-App ran. Dein iPhone und die Apple Watch sind davon nicht betroffen, deren Bluetooth läuft unabhängig.

**OBD2-Adapter** (nur lesen): Es kommen nur die Werte, die das Fahrzeug meldet. Fehlercodes neu lesen mit `…/cmd/obd/read_dtc/set`.

In Home Assistant erscheint ein Gerät „BLE Node“ (WLAN, Status) und je Plugin ein eigenes Gerät, die Dachhaube als Abdeckung (`cover`).

> **Umstieg von der Toolbox (bis 1.2.x):** Das Gerät stellt ein gespeichertes `camper/toolbox` beim ersten Start selbst auf `camper/ble` um, das Display (ab 2.16.2) ebenso. Danach die alten retained Topics und die alten HA-Entitäten löschen: `python3 tools/mqtt/clean.py --host <broker> camper/toolbox homeassistant/sensor/camper_toolbox …` bzw. die Geräte „Toolbox Node“ in Home Assistant entfernen.

> Wer die Toolbox schon aus dem alten Repo nutzt: Das Basis-Topic war `camper/tool`, der Hostname `camper-tool`. Ein Gerät mit gespeicherten Einstellungen behält sein altes Topic, bis es in der Web-UI geändert wird. Ändert man es, legt Home Assistant die Entitäten neu an.

### Dachhaube am Display

Ab display-node 2.10.3 hat das Display eine eigene Seite **Dachhaube** und eine kleine Kachel für die Übersicht (beide ab Werk aus, einschalten unter *Einstellungen → Anzeige*). Sie lesen `camper/tool/rvtech/…` und senden die Voreinstellungen und die Position an `camper/tool/cmd/rvtech/…`. Eigene Kacheln dafür braucht es nicht mehr; wer sie angelegt hat, kann sie löschen.

- Basis-Topic am Display: *Einstellungen → Topics der Geräte → BLE-Node Tool* (Standard `camper/tool`). Weitere BLE-Nodes trägst du bei *Tool 2* und *Tool 3* ein; die Dachhaube wird auf jedem davon gefunden.
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

**Ohne USB-Kabel:** Jedes verbindende Gerät (Dachhaube, Pumpe, Kühlschrank, Batterien per Verbindung) hat auf seiner Seite unter *Geräte* eine Karte **Log** mit „Log herunterladen“ und „Log leeren“. Darin stehen dieselben Zeilen wie im seriellen Monitor (Verbindung, Kopplung, GATT-Tabelle, empfangene Daten), bis 8 kB je Gerät, seit dem letzten Neustart. Bei Problemen: Log leeren, Fehler abwarten, herunterladen und schicken.

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

Die laufende Version steht in der Web-Oberfläche des Geräts. Alle Änderungen je Version: [CHANGELOG.md](CHANGELOG.md) (steht auch im jeweiligen GitHub-Release).

## Fertige Firmware

Wird im Haupt-Repo per GitHub Actions gebaut, Tag `ble-v<version>`, Dateien `ble-node-esp32c3.bin` und `ble-node-atoms3.bin` (battery-node auf dem AtomS3). Siehe [Fertige Firmware](../../README.md#fertige-firmware).

## Lizenz

GNU GPL Version 3 mit zusätzlichen Bedingungen (Urhebervermerk bleibt erhalten, veränderte Fassungen sind zu kennzeichnen), siehe [LICENSE](LICENSE) und [NOTICE.md](NOTICE.md). Name und Logo „VanNode“ sind nicht Teil der Lizenz: [NAME-UND-LOGO.md](../../NAME-UND-LOGO.md). Fremde Teile: Batterie-Protokolle `bms_proto.*` aus [aiobmsble](https://github.com/patman15/aiobmsble) (Apache-2.0, [LICENSE-Apache-2.0](LICENSE-Apache-2.0)), Dachhauben-Protokoll aus [rvtech-hacs](https://github.com/C64Axel/rvtech-hacs) (MIT, [LICENSE-rvtech-MIT](LICENSE-rvtech-MIT)), Pumpen-Protokoll (eigene Umsetzung, kein Code übernommen) nach [toms2k84/Blackforest-Vanlife-SmartCamper](https://github.com/toms2k84/Blackforest-Vanlife-SmartCamper) (Ordner `carawater-isp-ble`, MIT: Schaltbefehl `AA 39`, Statusrahmen `AA 45`), [geierwally1978/carawater-isp15-ble](https://github.com/geierwally1978/carawater-isp15-ble) (keine Lizenzangabe: Telemetrie `AA 93`, Status-Bits, Prüfsumme, Umrechnung) und der ersten HA-Integration carawater_isp15 eines Community-Mitglieds.

Ohne Gewähr, Nutzung auf eigene Gefahr; die Haftung ist ausgeschlossen, soweit gesetzlich zulässig (siehe [NOTICE.md](NOTICE.md)). Produkt- und Firmennamen sind Marken ihrer Inhaber, VanNode hat keine Verbindung zu ihnen.

