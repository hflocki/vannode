# Alde Node

> **Achtung:** Ohne Gewähr, Nutzung auf eigene Gefahr – siehe [Haftungsausschluss](../../README.md#haftungsausschluss--nutzung-auf-eigene-gefahr).

> ⚠️ **IN ENTWICKLUNG** – noch nicht an einer echten Alde-Heizung getestet. Das Mitlesen ist aus [alde-3020he-lin](https://github.com/Mellowgroup-dk/alde-3020he-lin) übernommen und dort am Bedienteil geprüft, die Steuerung aus [alde_3030_plus](https://github.com/daviddootson/alde_3030_plus) (dort an einer 3030 Plus geprüft). Diese Arduino-Umsetzung ist bisher nur mit künstlichen Bus-Daten getestet (`tests/run.sh`), die Steuerung an einer **Compact 3020 oder 3030 noch von niemandem**.

Liest die Werte einer **Alde Compact 3020 HE, 3030 oder 3030 Plus** (Warmwasser-Heizung) und stellt sie per **MQTT** bereit – für das Display, die Pocket-Node und optional Home Assistant. Optional steuert er die Heizung auch.

Der Node arbeitet an **zwei getrennten Bussen**, beide sind einzeln abschaltbar. **Zum Steuern reicht ein Transceiver am Bedienteil-Bus**, der Heizbus bringt nur zusätzliche Messwerte:

| Bus | Anschluss | Rolle des ESP | Wozu |
|---|---|---|---|
| **Heizbus** (9600 Baud) | Buchse „Control panel“ an der Heizung, per Splitter | hört nur zu, UART **ohne TX-Pin** | Glykol, Warmwasser, Heizstab-Leistung, Gebläse, Batterie, Landstrom … |
| **Bedienteil-Bus** (CI-Bus, 19200 Baud, optional) | Anschluss für ein externes Bedienteil am Alde-Bedienteil | **LIN-Master**: fragt ab, sendet nur bei „Lesen und steuern“ Befehle | Sollwert, Außentemperatur, Gas, Heizstab-Stufe, Warmwasser, Pumpe, Störung – **und Steuerung** |

| Ausbau | Werte | Steuern |
|---|---|---|
| Nur Bedienteil-Bus (1 Transceiver) | Raum, außen, Sollwert, Gas, Heizstab-Stufe, Warmwasser-Modus, Pumpe, 230 V, Störung | ja |
| Nur Heizbus (1 Transceiver) | Raum, Glykol, Warmwasser-Temperatur, Batterie, Landstrom, Heizstab in W, Gebläse, Klimaanlage | nein |
| Beide (2 Transceiver) | alles | ja |

Der Heizbus ist bisher nur an der Compact 3020 HE dekodiert; an der 3030 kann er andere oder keine Werte liefern.

Auf dem Heizbus ist das Bedienteil Master, dort lässt sich nichts steuern (ausführlich: [CONTROL-NOT-SOLVED.md](https://github.com/Mellowgroup-dk/alde-3020he-lin/blob/main/docs/CONTROL-NOT-SOLVED.md)). Am externen Anschluss ist das Bedienteil dagegen Slave und nimmt Befehle an – so wie von einem Fremd-Bedienteil. Der Bedienteil-Bus ist ab Werk **aus**.

Teil des Verbunds [VanNode](../../README.md): eigene **Web-Oberfläche**, Home-Assistant-Discovery, Updates ohne USB.

## Funktionen

- 13 Werte vom Bus plus „Klimaanlage aktiv“ und „LIN-Bus“ unter `camper/alde/<key>`, Home Assistant legt die Entitäten automatisch an (Gas, Klimaanlage aktiv und LIN-Bus als Binärsensoren)
- Werte gehen bei Änderung raus, höchstens alle *Sendeabstand* Sekunden (Standard 5 s), unverändert spätestens jede Minute
- Ungültige Werte (Platzhalter `0xFFFF`/`0x7FFF`, unbekannte Gas-Codes, falsche Prüfsumme) werden verworfen, der letzte gültige Wert bleibt stehen
- Bedienteil-Bus erkennt die Prüfsumme des Bedienteils selbst (erweitert oder klassisch, wie in [esphome-alde-3020](https://github.com/holdestmade/esphome-alde-3020)) und sendet Befehle mit derselben
- Optional Bedienteil-Bus: 11 weitere Werte und – nur im Modus „Lesen und steuern“ – Heizung an/aus, Sollwert (5–30 °C), Gas, Heizstab (bis zur eingestellten Höchststufe) und Warmwasser (aus/an/Boost) per MQTT, Web-UI und Home Assistant
- Vor jedem Befehl meldet sich der Node neu am Bedienteil an und liest den aktuellen Stand. Gesendet wird nur, was sich wirklich ändert; am Bedienteil gemachte Einstellungen werden nicht überschrieben
- Kein Befehl, solange das Bedienteil „beschäftigt“ oder eine Störung meldet: Der Node versucht es bis zu fünfmal im Abstand von 0,3 s und verwirft den Befehl dann (Ereignis-Log)
- **Heizstab-Höchststufe** (Werkseinstellung 1 kW): Höhere Stufen lehnt der Node ab, damit eine schwache Landstrom-Absicherung nicht auslöst. Am Bedienteil selbst bleibt alles einstellbar
- Unbekannter Warmwasser-Modus (Bitwert 3) wird nicht gemeldet statt als „an“
- Web-Oberfläche: Live-Werte, Bus-Zustand, Steuerung, Einstellungen, MQTT, WLAN, Login (ab Werk `admin` / `admin`)
- Setup-WLAN `Alde-Node-XXXXXX` (192.168.4.1) beim ersten Start und wenn das WLAN 60 s fehlt
- Firmware (`.bin`) und Web-Oberfläche (`index.html.gz`) im Web-UI hochladbar, dazu Arduino-OTA

## Hardware

| Teil | Hinweis |
|---|---|
| ESP32 | siehe Tabelle unten; der Chip bestimmt, ob beide Busse gleichzeitig gehen |
| LIN-Modul, z. B. TJA1020 / TJA1021 | je Bus eins |
| Pegelwandler 3,3 V ↔ 5 V (z. B. BSS138, 4 Kanäle) | **bei den meisten fertigen LIN-Modulen nötig**, siehe *Verdrahtung* |
| RJ12-Splitter (nur Heizbus) | an der Buchse **„Control panel“** der Heizung, das Original-Bedienteil bleibt dran |
| Step-down 12 V → 5 V | versorgt den ESP aus dem Bordnetz (5V-Pin) |

### Welcher ESP

| Chip | Heizbus und Bedienteil-Bus | Release-Datei | Hinweis |
|---|---|---|---|
| **ESP32 DevKit (WROOM-32)** – Empfehlung | beide gleichzeitig | ✓ `alde-node-esp32` | Standard-Pins passen, kein eigenes Kompilieren nötig |
| ESP32-S3 | beide gleichzeitig | ✓ `alde-node-esp32s3` | GPIO 25 gibt es nicht, GPIO 26–37 je nach Modul für Flash/PSRAM belegt |
| ESP32-C3 | nur **einer** von beiden (ein freier UART) | – selbst kompilieren | für die Steuerung den Heizbus ausschalten (Web-UI → Einstellungen → Heizbus) |

### Verdrahtung

**Pegelwandler ja oder nein?** Der TJA1020-Chip selbst kommt mit 3,3 V aus, aber die meisten fertigen LIN-Module haben einen eigenen 5-V-Regler und ziehen **RX auf 5 V** hoch – das verträgt der ESP32 nicht auf Dauer. Prüfen: Modul an 12 V, ESP nicht angeschlossen, RX gegen GND messen.

| Messung an RX | Lösung |
|---|---|
| ca. 5 V | Pegelwandler (einfachste Lösung, alle Leitungen darüber) oder nur an RX einen Spannungsteiler (10 kΩ vom Modul, 20 kΩ nach GND) |
| höchstens 3,3 V | direkt anschließen |

TX und SLP (ESP → Modul) brauchen eigentlich keinen Wandler, der TJA1020 schaltet ab ca. 2 V. Sie über denselben Wandler zu führen, schadet aber nicht; ein BSS138-Wandler schafft 19200 Baud problemlos.

**Pins je ESP** (GPIO-Nummern):

| Leitung | ESP32 DevKit | ESP32-S3 | ESP32-C3 |
|---|---|---|---|
| 5 V (vom Step-down) | VIN / 5V | 5V (5V0) | 5V |
| 3,3 V | 3V3 | 3V3 | 3V3 |
| Bedienteil-Bus RX | 26 | 4 | 4 |
| Bedienteil-Bus TX | 25 | 5 | 5 |
| Bedienteil-Bus SLP | – (SLP fest an HV) | 6 | 6 |
| Heizbus RX | 16 | 16 | 3 (nur ohne Bedienteil-Bus) |
| Heizbus SLP | – (SLP fest an HV) | – (SLP fest an HV) | – (SLP fest an HV) |

**Pegelwandler** (4 Kanäle; LV-Seite = ESP mit 3,3 V, HV-Seite = LIN-Modul mit 5 V):

| Pegelwandler | ESP-Seite (LV) | Modul-Seite (HV) |
|---|---|---|
| LV / HV | ESP 3V3 | ESP 5V |
| GND | ESP GND | LIN-Modul GND |
| Kanal 1 | Bedienteil-Bus RX | LIN-Modul (Bedienteil) RX |
| Kanal 2 | Bedienteil-Bus TX | LIN-Modul (Bedienteil) TX |
| Kanal 3 | Bedienteil-Bus SLP (S3, C3) | LIN-Modul (Bedienteil) SLP |
| Kanal 4 | Heizbus RX | LIN-Modul (Heizbus) RX |

Nur Bedienteil-Bus (ein LIN-Modul): Kanal 4 bleibt frei. Am ESP32 DevKit bleibt Kanal 3 frei und SLP des Moduls kommt direkt an HV. INH am Modul bleibt frei.

| Weitere Leitungen | Nach |
|---|---|
| 12 V Bordnetz | LIN-Modul 12V, über den Step-down 12 V → 5 V auch ESP 5V |
| gelbe RJ12-Buchse: Pin 4 LIN, Pin 2 Masse | LIN-Modul (Bedienteil) LIN / GND |
| rote RJ12-Buchse per Splitter: Pin 4 LIN, Pin 2 Masse | LIN-Modul (Heizbus) LIN / GND |

Der 5V-Pin führt nur 5 V, wenn der ESP über USB oder einen Step-down versorgt wird.

**`config_local.h` je ESP** (nur beim Selbst-Kompilieren; mit den Release-Dateien für ESP32 DevKit und ESP32-S3 nicht nötig – dort den Heizbus im Web-UI ausschalten, wenn nur der Bedienteil-Bus angeschlossen ist):

| ESP | Einträge |
|---|---|
| ESP32 DevKit | keine (Standard-Pins); nur Bedienteil-Bus: `DEFAULT_LIN_ENABLED false` |
| ESP32-S3 | keine (Standard-Pins); nur Bedienteil-Bus: `DEFAULT_LIN_ENABLED false` |
| ESP32-C3 | `DEFAULT_LIN_ENABLED false` (für die Steuerung) |

Beispiel, nur Bedienteil-Bus:

```cpp
#define DEFAULT_LIN_ENABLED  false   // only the panel bus transceiver is fitted
```

Andere Pins gehen ebenfalls (`LIN_RX_PIN`, `CI_RX_PIN`, `CI_TX_PIN`, `LIN_SLP_PIN`, `CI_SLP_PIN`), etwa die Belegung RX 16 / TX 17 / SLP 25 an einem ESP32 DevKit – dann aber `LIN_RX_PIN` von 16 weg legen.

Allgemein:

- **12 V** an das LIN-Modul, **Masse** gemeinsam mit Heizung/Bedienteil und ESP.
- **SLP/NSLP** auf High legen oder an einen GPIO (`*_SLP_PIN`), sonst ist der Transceiver aus.
- **Heizbus:** nur RX anschließen, TX bleibt frei. **Kein Master-Widerstand** – Master ist das Alde-Bedienteil. Das Bedienteil muss angeschlossen bleiben, ohne es ist auf dem Bus Ruhe. LIN, Masse und 12 V kommen über das RJ12-Kabel.
- **Bedienteil-Bus:** RX und TX anschließen. Hier ist der ESP Master, also **Master-Widerstand 1 kΩ mit Diode von 12 V auf LIN** (bei vielen TJA1020-Modulen eine Lötbrücke oder ein Jumper).

Bedienteil-Bus: An der 3030 Plus ist das die **gelbe RJ12-Buchse** (Pin 4 LIN, Pin 2 Masse), der Heizbus liegt dort an der roten Buchse (Pin 4 LIN, Pin 5 12 V, Pin 2 Masse). Im Service-Menü muss nichts freigeschaltet werden: Die Option „Remote Control“ unter *System Configuration* kann aus bleiben. Ist sie an, meldet das Bedienteil eventuell „Remote control missing“ – das ist nur kosmetisch.

**3030:** Von einem Nutzer bestätigt: ESP mit **einem** LIN-Modul an der gelben Buchse, ESP als Master, läuft seit Tagen stabil. Das Alde-Bedienteil bleibt angeschlossen und lässt sich weiter ganz normal bedienen. Angemeldet wird dort mit Funktions-ID **3** (also Heizung „3030 Plus“) – ob ID 2 an der 3030 auch geht, ist nicht geprüft.

**Compact 3020:** Ein anderes Projekt (ESPHome) meldet, dass das Steuern über denselben Bus an einer Compact 3020 mit einer Zone funktioniert – unabhängig bestätigt ist das nicht. Möglicherweise unterstützen ältere 3020-Bedienteile den Bus noch nicht. **Wer das an einer 3020 ausprobiert, bitte melden** – mit Baujahr und Software-Version des Bedienteils und ob die Steuerung angenommen wurde.

## Arduino IDE

Board-Paket, gemeinsame Bibliotheken, Board-Einstellungen, erstes Flashen, LittleFS-Plugin und Hilfe bei Upload-Problemen: **[docs/ARDUINO.md](../../docs/ARDUINO.md)**. Hier nur, was für dieses Gerät gilt.

| Einstellung | Wert |
|---|---|
| Board | ESP32 DevKit (Release-Datei `alde-node-esp32`) oder ESP32-S3 (`alde-node-esp32s3`, Board *ESP32S3 Dev Module*, USB CDC On Boot an); ESP32-C3 geht auch (siehe *Welcher ESP*) |
| Zusätzliche Bibliotheken | keine |
| Web-Oberfläche | einmalig über das Upload-Formular (`http://<ip>/` → `data/index.html.gz`) |

## Updates ohne USB

| Was | Wie |
|---|---|
| Firmware | Arduino IDE → *Sketch → Kompilierte Binärdatei exportieren*, die `.bin` im Web-UI unter **WiFi → Firmware- & Web-Update** hochladen (Neustart), oder per Netzwerkport in der IDE (OTA-Passwort) |
| Web-Oberfläche | `data/index.html.gz` im Web-UI hochladen – ersetzt nur die Seite, ohne Neustart, Einstellungen bleiben |
| Komplettes LittleFS-Image | Nur per USB oder IDE-Netzwerkport; löscht dabei auch die Einstellungen (`config.json`) |

## MQTT / Home Assistant

Basis-Topic `camper/alde`: Werte vom Heizbus (Raum, Glykol, Warmwasser, Gas, Heizstab …), mit Bedienteil-Bus zusätzlich Sollwert, Außentemperatur und Status, Befehle unter `…/cmd/…` (nur im Modus „Lesen und steuern“). Alle Topics mit Quelle (LIN-Frame und Byte): **[docs/TOPICS.md](../../docs/TOPICS.md#alde-heizung-camperalde)**.

Unbekannte Werte werden verworfen (sie werden nie zu „aus“). Home Assistant bekommt für die Steuerung eine Zahl (Sollwert), zwei Schalter (Heizung an/aus, Gas) und zwei Auswahlen (Heizstab, Warmwasser); sie verschwinden wieder, wenn die Steuerung ausgeschaltet wird. Alle Werte retained.

## Bedienteil-Bus einschalten

Web-UI → **Einstellungen → Bedienteil-Bus (Steuerung)**:

| Modus | Was passiert |
|---|---|
| Aus (Werkseinstellung) | Der zweite UART ist aus, auf dem Anschluss passiert nichts |
| Nur lesen | Der ESP meldet sich einmal an (`0x3C`, Hersteller `0x41DE`, Funktions-ID der Heizung) und fragt jede Sekunde den Info-Frame `0x1B` ab. Er sendet keine Befehle |
| Lesen und steuern | zusätzlich Befehle per Control-Frame `0x1A`. Vor jedem Befehl wird der Stand neu gelesen, danach fünfmal schnell nachgefragt, damit die Bestätigung gleich da ist. Jeder Befehl steht im Ereignis-Log |
| Nur mithören (Diagnose) | Der ESP sendet **gar nichts** (TX-Pin vom UART getrennt und auf High), er sammelt nur, welche Frames andere Geräte schicken: je LIN-ID Kopfzeilen, Antworten, Länge, Prüfsumme (LIN 1/2) und die letzten Daten. Anzeige im Reiter **Debug**, abrufbar unter `/api/sniff`, neue IDs im Ereignis-Log. Keine Werte per MQTT |

Dazu die **Heizung** wählen (Funktions-ID: Compact 3020 = 1, 3030 = 2, 3030 Plus = 3; auch der Modellname in Home Assistant). An einer 3030 funktioniert nachweislich „3030 Plus“ (ID 3) – bei einer 3030 also am besten damit anfangen und die **Heizstab-Höchststufe** (0–3 kW, Werkseinstellung 1 kW; Home Assistant und Web-UI bieten nur die erlaubten Stufen an).

Ist nur der Bedienteil-Bus verdrahtet: **Einstellungen → Heizbus** ausschalten. Das Gerät startet neu, die Heizbus-Werte verschwinden aus Web-UI und Home Assistant, und am C3 wird der UART für den Bedienteil-Bus frei.

### Web-Seite: Reiter

| Reiter | Inhalt |
|---|---|
| Live | Werte, Heizung steuern (bei „Lesen und steuern“) |
| Alde | Heizbus und Bedienteil-Bus: Modus, Heizung, Heizstab-Höchststufe |
| MQTT, WiFi | Verbindungen |
| Einstellungen | Firmware-Update, Über VanNode, Ereignisse, Schalter **Debug**, Neustart |
| Debug | nur mit Debug oder im Modus „Nur mithören“: Bus-Statistik, Mithör-Tabelle, Log-Fenster |

**Debug** (Reiter Einstellungen) schreibt zusätzlich jede Signaländerung am Heizbus und die Frames am Bedienteil-Bus ins Log (dasselbe wie der serielle Befehl `sig`). Das Log liegt im RAM (16 kB, älteste Zeilen fallen raus) und ist nach einem Neustart leer. **Herunterladen** speichert es als `.log`-Datei, vorne mit Firmware-Version, Einstellungen (ohne WLAN-, MQTT- und Zugangsdaten) und Bus-Statistik – so bitte an die Entwicklung schicken. **Zustand ins Log** schreibt alle aktuellen Werte dazu (wie der serielle Befehl `read`).

### Wer ist Master auf dem Bedienteil-Bus? (Mithören)

Auf der gelben Buchse ist der ESP der Master, das Bedienteil antwortet nur – so beschreibt es alde_3030_plus, und an einer 3030 ist es in der Praxis bestätigt (Bedienteil bleibt bedienbar). Bei einer Heizung, an der das nicht klappt, klärt der Modus **„Nur mithören (Diagnose)“**:

1. **iNet-Box abstecken**, ESP an die gelbe Buchse, Modus „Nur mithören“, ein paar Minuten laufen lassen und das Bedienteil normal bedienen.
   - Liste bleibt leer, „völlige Ruhe auf dem Bus“ → das Bedienteil wartet auf einen Master: „Nur lesen“ / „Lesen und steuern“ passen.
   - Es erscheinen Kopfzeilen → das Bedienteil ist auf dieser Buchse selbst Master. Dann bitte melden – der Node bräuchte einen Slave-Modus.
2. Wer eine **iNet-Box** hat: dasselbe mit angeschlossener iNet-Box (ESP parallel über einen Splitter). Die Liste zeigt dann, welche IDs die beiden austauschen. Den Master-Widerstand am ESP-Modul dafür möglichst abklemmen, die iNet-Box bringt ihren eigenen mit.

Ergebnis bitte als Bildschirmfoto der Tabelle schicken, mit Heizungsmodell sowie Baujahr und Software-Version des Bedienteils.

Erst „Nur lesen“ ausprobieren: Kommen plausible Werte (Sollwert und Raum wie am Bedienteil), dann erst steuern – und dabei an der Heizung bleiben.

## Serieller Monitor (115200 Baud)

| Befehl | Wirkung |
|---|---|
| `read` | Statistik beider Busse (Heizbus gültig/verworfen/leer, Bedienteil-Bus Abfragen/Befehle und Rohbytes) und alle Werte |
| `measure` | Werte sofort per MQTT senden |
| `sig` | Mitschnitt an/aus: jeder Signal-Frame, dessen Inhalt sich ändert, als Hex (`[SIG] 47 (len=8): …`) – zum Vergleichen mit dem Bedienteil |
| `noauth` | Web-Login auf Werks-Login `admin` / `admin` zurücksetzen |
| `help` | Befehle anzeigen |

## Tests ohne Heizung

`tests/run.sh` übersetzt beide Protokoll-Teile für den PC:

- `alde_test`: künstlicher Heizbus-Zyklus mit den Rohwerten aus der Doku von alde-3020he-lin (Byte für Byte, Platzhalter, falsche Prüfsumme, Warmwasser aus, Klimaanlage wach, Bus-Timeout)
- `ci_test`: Bedienteil-Bus – Anmeldung, Info-Frame mit Echo, das Beispiel-Frame aus der Doku von alde_3030_plus, Befehle ändern nur ihr Feld, Grenzen, Sonderwerte, unveränderte Werte werden nicht gesendet, Sperre bei „beschäftigt“/Störung, Warmwasser-Modus 3
- `sniff_test`: Mithören – Paritätsbits, gemischter Verkehr (LIN 1 und 2, Kopfzeile ohne Antwort, falsche Prüfsumme), über zwei Lesevorgänge verteilte Frames, Störbytes

Läuft auch in der GitHub Action.

```bash
bash firmware/alde/tests/run.sh
```

## Sicheres Update, Ereignisse und Rückfall

Jede Firmware hat eine Kennung (`CAMPER-FW:alde:<version>:<chip>;`) und dieselbe Absicherung wie alle VanNode-Geräte (`fwcheck.*`, `evlog.*`, `safeguard.*` aus `firmware/_common`):

- **Prüfung beim Update** (Web-UI unter **WiFi → Firmware- & Web-Update**): Die installierte Version steht dabei. Eine gewählte Datei wird schon im Browser und beim Hochladen im Gerät noch einmal geprüft, bevor etwas aktiviert wird. Abgelehnt werden eine `.merged.bin` (komplettes Flash-Abbild für USB), Firmware für ein anderes Gerät oder einen anderen Chip und zu große Dateien. Fehler stehen im Klartext da statt „Update failed“.
- **Ereignisse:** Die letzten Ereignisse (Start mit Grund, WLAN und MQTT verbunden/getrennt, Uploads mit Ergebnis, Rückfall, gespeicherte Einstellungen) stehen in der Web-Oberfläche. Sie bleiben bei Neustart, Absturz und Update erhalten, nach einem Stromausfall ist das Log leer.
- **Rückfall nach Update:** Hat eine neue Firmware nach 5 Minuten kein WLAN oder startet sie mehr als dreimal neu, startet wieder die vorherige. Kam das Update über den Setup-AP (ohne WLAN), wird nur auf Neustart-Schleifen geachtet.
- **WLAN-Änderung mit Rückweg:** Werden die WLAN-Daten geändert, während das Gerät verbunden ist, merkt es sich die funktionierenden. Klappt die Verbindung mit den neuen nach dem Neustart etwa 1 Minute lang nicht, kommen die alten zurück.

## Versionen

Die laufende Version steht in der Web-Oberfläche des Geräts. Alle Änderungen je Version: [CHANGELOG.md](CHANGELOG.md) (steht auch im jeweiligen GitHub-Release).

## Lizenz

GNU GPL Version 3 mit zusätzlichen Bedingungen (Urhebervermerk bleibt erhalten, veränderte Fassungen sind zu kennzeichnen), siehe [LICENSE](LICENSE) und [NOTICE.md](../../NOTICE.md). Name und Logo „VanNode“ sind nicht Teil der Lizenz: [NAME-UND-LOGO.md](../../NAME-UND-LOGO.md).

Ohne Gewähr, Nutzung auf eigene Gefahr; die Haftung ist ausgeschlossen, soweit gesetzlich zulässig (siehe [NOTICE.md](../../NOTICE.md)). Fremde Teile: Die Heizbus-Dekodierung stammt aus [Mellowgroup-dk/alde-3020he-lin](https://github.com/Mellowgroup-dk/alde-3020he-lin) (Code MIT, siehe [LICENSE-alde-MIT](LICENSE-alde-MIT), Dokumentation CC BY 4.0), das Protokoll des Bedienteil-Busses aus [daviddootson/alde_3030_plus](https://github.com/daviddootson/alde_3030_plus) (MIT, siehe [LICENSE-alde3030-MIT](LICENSE-alde3030-MIT)). Produkt- und Firmennamen sind Marken ihrer Inhaber, VanNode hat keine Verbindung zu ihnen.

