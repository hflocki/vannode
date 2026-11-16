# Alde Node

> **Achtung:** Ohne Gewähr, Nutzung auf eigene Gefahr – siehe [Haftungsausschluss](../../README.de.md#haftungsausschluss--nutzung-auf-eigene-gefahr).

> ⚠️ **IN ENTWICKLUNG** – noch nicht an einer echten Alde-Heizung getestet. Das Mitlesen ist aus [alde-3020he-lin](https://github.com/Mellowgroup-dk/alde-3020he-lin) übernommen und dort am Bedienteil geprüft, die Steuerung aus [alde_3030_plus](https://github.com/daviddootson/alde_3030_plus) (dort an einer 3030 Plus geprüft). Diese Arduino-Umsetzung ist bisher nur mit künstlichen Bus-Daten getestet (`tests/run.sh`), die Steuerung an einer **Compact 3020 noch von niemandem**.

Liest die Werte einer **Alde Compact 3020 HE** (Warmwasser-Heizung) und stellt sie per **MQTT** bereit – für das Display, die Pocket-Node und optional Home Assistant. Optional steuert er die Heizung auch.

Der Node arbeitet an **zwei getrennten Bussen**:

| Bus | Anschluss | Rolle des ESP | Wozu |
|---|---|---|---|
| **Heizbus** (9600 Baud) | Buchse „Control panel“ an der Heizung, per Splitter | hört nur zu, UART **ohne TX-Pin** | Glykol, Warmwasser, Heizstab-Leistung, Gebläse, Batterie, Landstrom … |
| **Bedienteil-Bus** (CI-Bus, 19200 Baud, optional) | Anschluss für ein externes Bedienteil am Alde-Bedienteil | **LIN-Master**: fragt ab, sendet nur bei „Lesen und steuern“ Befehle | Sollwert, Außentemperatur, Gas, Heizstab-Stufe, Warmwasser, Pumpe, Störung – **und Steuerung** |

Auf dem Heizbus ist das Bedienteil Master, dort lässt sich nichts steuern (ausführlich: [CONTROL-NOT-SOLVED.md](https://github.com/Mellowgroup-dk/alde-3020he-lin/blob/main/docs/CONTROL-NOT-SOLVED.md)). Am externen Anschluss ist das Bedienteil dagegen Slave und nimmt Befehle an – so wie von einem Fremd-Bedienteil. Der Bedienteil-Bus ist ab Werk **aus**.

Teil des Verbunds [VanNode](../../README.md): eigene **Web-Oberfläche**, Home-Assistant-Discovery, Updates ohne USB.

## Funktionen

- 13 Werte vom Bus plus „Klimaanlage aktiv“ und „LIN-Bus“ unter `camper/alde/<key>`, Home Assistant legt die Entitäten automatisch an (Gas, Klimaanlage aktiv und LIN-Bus als Binärsensoren)
- Werte gehen bei Änderung raus, höchstens alle *Sendeabstand* Sekunden (Standard 5 s), unverändert spätestens jede Minute
- Ungültige Werte (Platzhalter `0xFFFF`/`0x7FFF`, unbekannte Gas-Codes, falsche Prüfsumme) werden verworfen, der letzte gültige Wert bleibt stehen
- Optional Bedienteil-Bus: 11 weitere Werte und – nur im Modus „Lesen und steuern“ – Sollwert (5–30 °C), Gas, Heizstab (0–3 kW) und Warmwasser (aus/an/Boost) per MQTT, Web-UI und Home Assistant
- Jeder Befehl liest zuerst den aktuellen Stand des Bedienteils und ändert nur den gewählten Wert; am Bedienteil gemachte Einstellungen werden nicht überschrieben
- Web-Oberfläche: Live-Werte, Bus-Zustand, Steuerung, Einstellungen, MQTT, WLAN, optionaler Login
- Setup-WLAN `Alde-Node-XXXXXX` (192.168.4.1) beim ersten Start und wenn das WLAN 60 s fehlt
- Firmware (`.bin`) und Web-Oberfläche (`index.html.gz`) im Web-UI hochladbar, dazu Arduino-OTA

## Hardware

| Teil | Hinweis |
|---|---|
| ESP32 (z. B. ESP32-WROOM-32 DevKit) | braucht drei UARTs für beide Busse. Ein ESP32-C3 kann nur den Heizbus (nutzt dann UART1) |
| LIN-Transceiver 1, z. B. TJA1020-Modul (Heizbus) | **RXD → GPIO16** (`LIN_RX_PIN`), TXD bleibt frei. SLP/NSLP auf High legen oder an einen GPIO (`LIN_SLP_PIN`), sonst ist der Empfänger aus |
| RJ12-Splitter | an der Buchse **„Control panel“** der Heizung, das Original-Bedienteil bleibt dran |
| LIN-Transceiver 2 (Bedienteil-Bus, optional) | **RXD → GPIO26** (`CI_RX_PIN`), **TXD ← GPIO25** (`CI_TX_PIN`), SLP/NSLP auf High (`CI_SLP_PIN`). LIN-Leitung an den Anschluss für ein externes Bedienteil, gemeinsame Masse |

Heizbus: LIN, Masse und 12 V kommen über das RJ12-Kabel. Das Bedienteil muss angeschlossen bleiben: Es ist der Bus-Master, ohne es ist auf dem Bus komplett Ruhe.

Bedienteil-Bus: An der 3030 Plus ist das die **gelbe RJ12-Buchse** (Pin 4 LIN, Pin 2 Masse). Wo der Anschluss an der 3020 sitzt und ob dafür im Service-Menü „third party panel“ eingeschaltet werden muss, ist noch offen – das Bedienteil meldet „third party panel C fail“, solange es dort niemanden findet. **Wer das an einer 3020 ausprobiert, bitte melden.**

## Arduino IDE

Board-Paket, gemeinsame Bibliotheken, Board-Einstellungen, erstes Flashen, LittleFS-Plugin und Hilfe bei Upload-Problemen: **[docs/ARDUINO.md](../../docs/ARDUINO.md)**. Hier nur, was für dieses Gerät gilt.

| Einstellung | Wert |
|---|---|
| Board | ESP32 DevKit (Release-Datei `alde-node-esp32`), ESP32-C3 geht auch |
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

Unbekannte Werte werden verworfen (sie werden nie zu „aus“). Home Assistant bekommt für die Steuerung eine Zahl (Sollwert), einen Schalter (Gas) und zwei Auswahlen (Heizstab, Warmwasser); sie verschwinden wieder, wenn die Steuerung ausgeschaltet wird. Alle Werte retained.

## Bedienteil-Bus einschalten

Web-UI → **Einstellungen → Bedienteil-Bus (Steuerung)**:

| Modus | Was passiert |
|---|---|
| Aus (Werkseinstellung) | Der zweite UART ist aus, auf dem Anschluss passiert nichts |
| Nur lesen | Der ESP meldet sich einmal an (`0x3C`, Hersteller `0x41DE`, Funktions-ID der Heizung) und fragt jede Sekunde den Info-Frame `0x1B` ab. Er sendet keine Befehle |
| Lesen und steuern | zusätzlich Befehle per Control-Frame `0x1A`. Vor jedem Befehl wird der Stand neu gelesen, danach fünfmal schnell nachgefragt, damit die Bestätigung gleich da ist. Jeder Befehl steht im Ereignis-Log |

Dazu die **Heizung** wählen (Funktions-ID: Compact 3020 = 1, 3030 = 2, 3030 Plus = 3). Erst „Nur lesen“ ausprobieren: Kommen plausible Werte (Sollwert und Raum wie am Bedienteil), dann erst steuern – und dabei an der Heizung bleiben.

## Serieller Monitor (115200 Baud)

| Befehl | Wirkung |
|---|---|
| `read` | Statistik beider Busse (Heizbus gültig/verworfen/leer, Bedienteil-Bus Abfragen/Befehle und Rohbytes) und alle Werte |
| `measure` | Werte sofort per MQTT senden |
| `sig` | Mitschnitt an/aus: jeder Signal-Frame, dessen Inhalt sich ändert, als Hex (`[SIG] 47 (len=8): …`) – zum Vergleichen mit dem Bedienteil |
| `noauth` | Web-Login löschen |
| `help` | Befehle anzeigen |

## Tests ohne Heizung

`tests/run.sh` übersetzt beide Protokoll-Teile für den PC:

- `alde_test`: künstlicher Heizbus-Zyklus mit den Rohwerten aus der Doku von alde-3020he-lin (Byte für Byte, Platzhalter, falsche Prüfsumme, Warmwasser aus, Klimaanlage wach, Bus-Timeout)
- `ci_test`: Bedienteil-Bus – Anmeldung, Info-Frame mit Echo, das Beispiel-Frame aus der Doku von alde_3030_plus, Befehle ändern nur ihr Feld, Grenzen, Sonderwerte

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

GNU GPL Version 3 mit zusätzlichen Bedingungen (Urhebervermerk bleibt erhalten, veränderte Fassungen sind zu kennzeichnen), siehe [LICENSE](LICENSE) und [NOTICE.md](NOTICE.md). Name und Logo „VanNode“ sind nicht Teil der Lizenz: [NAME-UND-LOGO.md](../../NAME-UND-LOGO.md).

Ohne Gewähr, Nutzung auf eigene Gefahr; die Haftung ist ausgeschlossen, soweit gesetzlich zulässig (siehe [NOTICE.md](NOTICE.md)). Fremde Teile: Die Heizbus-Dekodierung stammt aus [Mellowgroup-dk/alde-3020he-lin](https://github.com/Mellowgroup-dk/alde-3020he-lin) (Code MIT, siehe [LICENSE-alde-MIT](LICENSE-alde-MIT), Dokumentation CC BY 4.0), das Protokoll des Bedienteil-Busses aus [daviddootson/alde_3030_plus](https://github.com/daviddootson/alde_3030_plus) (MIT, siehe [LICENSE-alde3030-MIT](LICENSE-alde3030-MIT)). Produkt- und Firmennamen sind Marken ihrer Inhaber, VanNode hat keine Verbindung zu ihnen.

