# Level Node – Ausrichtung mit Keil-Empfehlung per MQTT

> **Achtung:** Ohne Gewähr, Nutzung auf eigene Gefahr – siehe [Haftungsausschluss](../../README.de.md#haftungsausschluss--nutzung-auf-eigene-gefahr).

Firmware für einen ESP32 (ESP32-S3, ESP32-C3 oder klassischer ESP32) mit einem **Adafruit BNO085**, die Längs- und Querneigung des Wohnmobils misst und für **jedes der vier Räder** anzeigt, wie hoch der Keil sein muss, damit das Fahrzeug waagerecht steht.

- Werte per **MQTT** mit Home-Assistant-Discovery, Topics `camper/level/…` wie bei den anderen Geräten in diesem Repo
- Web-Oberfläche mit Wasserwaage, Draufsicht mit den vier Keilhöhen, Tara, Fahrzeugmaßen, MQTT- und WLAN-Einstellungen
- Wird direkt auf dem [Wandpanel](../display) und der [Pocket-Node](../pocket) angezeigt, dazu eine Kachel für Home Assistant (`ha-card/`)

> **Ursprung:** Dieses Projekt basiert auf dem [ESP32 Adafruit BNO085 Caravan-Levelsensor](https://github.com/klehnst/ESP32-Adrafruit-BNO085-HomeAssistant-Caravan-Levelsensor-MQTT) von **klehnst** – vielen Dank für die Vorarbeit (BNO085-Anbindung, Einbaulagen, Wasserwaagen-Oberfläche, HA-Kachel, 3D-Druck-Gehäuse). Wer einen **Wohnwagen** mit Stützrad nivellieren will, ist dort richtig: Der Wohnwagen-Modus ist hier bewusst entfernt.

## Web-Oberfläche

| Live | Fahrzeug | Tara |
|---|---|---|
| ![](docs/web-live.png) | ![](docs/web-vehicle.png) | ![](docs/web-tare.png) |

## Was gegenüber dem Original anders ist

**Umbau auf Camper**
- Nur noch Wohnmobil mit vier Keilen; Wohnwagen-Modus (Stützrad, Achse–Stützrad, Keil links/rechts) entfernt
- Alles von `wowa` auf `camper` umbenannt: Sketch `camper_level.ino`, Topics `camper/level`, HA-Gerät „Camper Level“, Access Point `Camper-Level-XXXXXX`, Hostname `camper-level`
- Gleicher Aufbau wie die anderen Geräte: Web-Oberfläche im gemeinsamen Stil, Web-Login, Last Will mit QoS 1, `config_local.h` für eigene Passwörter und Pins, Reiter WiFi mit Sendeleistung und „AP nur als Notfall“

**Fehler behoben**
- **Keilhöhen waren nur halb so groß wie nötig.** Das Original hat die tiefen Räder nur auf die Höhe der Fahrzeugmitte angehoben. Da sich die hohe Seite nicht absenken lässt, muss jedes Rad auf das **höchste** Rad gebracht werden. Steht der Camper z. B. nur seitlich schief, braucht die tiefe Seite den vollen Höhenunterschied über die Spurweite, nicht den halben.
- **Die Konfiguration samt WLAN- und MQTT-Passwort war über `http://<ip>/config.json` für jeden im Netz abrufbar**, weil das ganze Dateisystem ausgeliefert wurde. Jetzt wird nur noch die Web-Oberfläche ausgeliefert.
- `/api/config` hat alle Passwörter im Klartext an den Browser geschickt – jetzt nie mehr.
- Der Glättungsfilter ließ sich schalten, wurde aber nirgends angewendet. Jetzt glättet er wirklich (gleitender Mittelwert).
- Die „Temperatur“ war ein Zufallswert aus einem unbeschalteten Analog-Pin – entfernt.
- Fehlte die Spurweite in der gespeicherten Konfiguration, wurde sie 0 und alle Keile auch.
- Neustart und Flash-Schreiben passieren nicht mehr im Web-Task (konnte den ESP aufhängen).
- Der BNO085 wird erneut gesucht, wenn er beim Start nicht gefunden wurde, statt bis zum nächsten Neustart auszufallen.
- Doppelte HA-Entitäten (Toleranz, Filter und MQTT-Tempo jeweils zweimal) zusammengefasst; Einstellungen werden nur bei Änderung und einmal pro Minute gesendet statt fünfmal pro Sekunde.
- „MQTT schnell“ (5 Hz) schaltet sich nach 10 Minuten selbst wieder ab.

## Hardware

- ESP32-S3, ESP32-C3 Super Mini oder ESP32 DevKit
- [Adafruit BNO085](https://www.adafruit.com/product/4754) (I2C, Adresse 0x4A)
- 3D-Druck-Gehäuse aus dem Original-Projekt unter [`3d-print/`](3d-print)

| BNO085 | ESP32 |
|---|---|
| VIN | 3V3 |
| GND | GND |
| SDA | `PIN_I2C_SDA` (Standard GPIO2) |
| SCL | `PIN_I2C_SCL` (Standard GPIO1) |

Die Standard-Pins gelten auch für die fertige `.bin` aus den Releases. Andere Pins (z. B. SDA GPIO1 / SCL GPIO2 oder ESP32-C3 GPIO4/GPIO10) in `config_local.h` eintragen und selbst bauen.

Den Sensor fest mit dem Fahrzeug verbinden. Die Einbaulage (flach oder an einer Wand, USB-Anschluss zeigt nach …) stellt man in der Web-Oberfläche ein.

## Arduino IDE

Board-Paket, gemeinsame Bibliotheken, Board-Einstellungen, erstes Flashen, LittleFS-Plugin und Hilfe bei Upload-Problemen: **[docs/ARDUINO.md](../../docs/ARDUINO.md)**. Hier nur, was für dieses Gerät gilt.

| Einstellung | Wert |
|---|---|
| Board | ESP32-S3 (Release-Datei `level-node-esp32s3`), ESP32-C3 und ESP32 DevKit gehen auch |
| Zusätzliche Bibliothek | SparkFun BNO08x Cortex Based IMU 1.0.6 |
| Sketch | `camper_level/camper_level.ino` |
| Web-Oberfläche | als LittleFS-Image aus `data/` (Plugin siehe docs/ARDUINO.md). Danach startet das Gerät mit leeren Einstellungen bzw. den Werten aus `config_local.h` |

## Erstbenutzung

1. Ohne WLAN-Daten startet der Access Point **`Level-Node-XXXXXX`** (Passwort aus `config.h` bzw. `config_local.h`). Verbinden, `http://192.168.4.1` öffnen.
2. Reiter **WiFi**: WLAN eintragen, speichern. Reiter **MQTT**: Broker eintragen.
3. Reiter **Fahrzeug**: Spurweite, Radstand und Einbaulage eintragen. Prüfen: Fahrzeug vorne anheben → Längsneigung wird positiv; links anheben → Querneigung wird positiv. Stimmt eine Richtung nicht, andere Einbaulage wählen.
4. Reiter **Tara**: Fahrzeug auf einem sicher ebenen Platz mit einer Wasserwaage ausrichten, **Tara setzen**.

## Keile ablesen

Die Live-Seite zeigt für jedes Rad die nötige Keilhöhe. Das höchste Rad bekommt 0 cm, alle anderen werden auf seine Höhe gebracht. Die Werte gelten für die gerade gemessene Neigung: nach dem Auffahren auf die Keile ändern sie sich, bis „Waagerecht“ erscheint. Zum Einweisen auf dem Handy **Schnell senden** einschalten (5 Hz statt alle 2 s).

Die Wasserwaage zeigt wie eine echte Libelle: Die Blase wandert zur **hohen** Seite.

## MQTT / Home Assistant

Basis-Topic `camper/level` (in der Web-UI änderbar): Neigung `…/pitch`, `…/roll`, Keilhöhen `…/wedge_fl` …, Befehle wie Tara unter `…/cmd/…`. Alle Topics und Befehle: **[docs/TOPICS.md](../../docs/TOPICS.md#levelsensor-camperlevel)**.

In Home Assistant erscheint ein Gerät „Level Node“. Beim ersten Verbinden löscht die Firmware die Einträge der Original-Firmware (`wowa_level`), damit kein zweites, totes Gerät stehen bleibt.

## Home-Assistant-Kachel

`ha-card/camper-level-card.js` zeigt Längs- und Querneigung mit kippenden Fahrzeugbildern, die vier Keile und optional einen Schnell-Modus-Knopf.

1. `camper-level-card.js` nach `/config/www/camper-level-card/` kopieren, die Bilder aus `ha-card/img/` nach `/config/www/camper-level-card/img/`.
2. *Einstellungen → Dashboards → ⋮ → Ressourcen*: `/local/camper-level-card/camper-level-card.js` als JavaScript-Modul hinzufügen.
3. Karte „Camper Ausrichtung“ hinzufügen und die Entitäten wählen: Längs- und Querneigung, die vier Keile, „Waagerecht“ als Level-Status, „MQTT schnell (5 Hz)“ als Boost-Schalter.

Bestehende Dashboards mit `custom:wowa-level-card` funktionieren weiter.

## Serieller Monitor

115200 Baud. Befehle: `read` (Neigung und Keile), `tare`, `notare`, `sensor` (BNO085 neu starten), `noauth` (Web-Login entfernen), `help`.

## Updates ohne USB

| Was | Wie |
|---|---|
| Firmware | Arduino IDE → *Sketch → Kompilierte Binärdatei exportieren*, die `.bin` im Web-UI unter **WiFi → Firmware- & Web-Update** hochladen (Neustart), oder per Netzwerkport in der IDE (OTA-Passwort) |
| Web-Oberfläche | `data/index.html.gz` im Web-UI hochladen – ersetzt nur die Seite, ohne Neustart, Einstellungen bleiben |
| Komplettes LittleFS-Image | Nur per USB oder IDE-Netzwerkport; löscht dabei auch die Einstellungen (`config.json`) |

## Sicheres Firmware-Update und Ereignisse

Seit 2.3.0 trägt jede Firmware eine Kennung (`CAMPER-FW:level:<version>:<chip>;`, `fwcheck.*` aus `firmware/_common`). Im Web-UI unter **WiFi → Firmware- & Web-Update** steht die installierte Version. Eine gewählte Datei wird schon im Browser geprüft und beim Hochladen im Gerät noch einmal, bevor etwas aktiviert wird:

| Datei | Ergebnis |
| --- | --- |
| `.bin` dieses Geräts | Gerät, Version und Vergleich mit der installierten (neuer / gleich / älter, bei gleich oder älter mit Rückfrage) |
| `.merged.bin` aus der Arduino IDE | abgelehnt: das ist das komplette Flash-Abbild für USB, fürs Web-Update die `.bin` ohne „merged“ nehmen |
| `.bin` eines anderen Geräts oder Chips | abgelehnt, z. B. „Firmware für „tanklevel“, dieses Gerät ist „level““ |
| `.bin` ohne Kennung (ältere Firmware, fremde Datei) | gesperrt, mit „Ohne Kennung hochladen“ trotzdem möglich (für ein Downgrade auf eine Version vor 2.3.0) |
| `data/index.html.gz` | Web-Oberfläche, ohne Neustart |

Schlägt etwas fehl, nennt die Meldung den Grund statt nur „Update failed“. Die Karte **Ereignisse** darunter zeigt die letzten 24 Ereignisse: Start mit Grund (Einschalten, Absturz, Watchdog …), WLAN und MQTT verbunden/getrennt, Uploads mit Ergebnis, Rückfall und gespeicherte Einstellungen (`evlog.*`). Das Log bleibt bei Neustart, Absturz und Update erhalten, bei Stromausfall ist es leer. Updates über den Netzwerkport der Arduino IDE werden nicht geprüft, aber ebenfalls protokolliert.

## Absicherung gegen Aussperren

Damit das Gerät erreichbar bleibt, ohne dass jemand hinmuss (`safeguard.*` aus `firmware/_common`):

- **Rückfall nach Update:** Nach einem Firmware-Update über das Web-UI oder den IDE-Netzwerkport läuft die neue Version auf Probe. Hat sie nach 5 Minuten kein WLAN oder startet sie mehr als dreimal neu, startet der ESP wieder die vorherige Firmware (sie liegt noch im zweiten App-Bereich). Ab 1 Minute stabilem WLAN gilt die neue Version als gut. Flashen per USB wird nie zurückgerollt.
- **WLAN-Änderung mit Rückweg:** Werden SSID, Passwort geändert, während das Gerät verbunden ist, merkt es sich die funktionierenden Werte. Klappt die Verbindung mit den neuen nach dem Neustart nicht (etwa 1 Minute), setzt es die alten wieder ein und startet neu. Ist das Gerät beim Ändern gar nicht verbunden (Einrichtung über den Setup-AP), gelten die neuen Werte sofort.
- Was zuletzt passiert ist, steht im Web-UI unter **WiFi → Verbindungsstatus** in der Zeile **Absicherung**.

Der Schutz greift erst bei Updates, die von dieser Version **aus** gemacht werden. Das Update auf diese Version selbst ist noch nicht abgesichert.

## Versionen

Die laufende Version steht in der Web-Oberfläche des Geräts. Alle Änderungen je Version: [CHANGELOG.md](CHANGELOG.md) (steht auch im jeweiligen GitHub-Release).

## Fertige Firmware

Wird im Haupt-Repo per GitHub Actions gebaut, siehe [Fertige Firmware](../../README.de.md#fertige-firmware).

## Lizenz

GNU GPL Version 3 mit zusätzlichen Bedingungen (Urhebervermerk bleibt erhalten, veränderte Fassungen sind zu kennzeichnen), siehe [LICENSE](LICENSE) und [NOTICE.md](NOTICE.md). Name und Logo „VanNode“ sind nicht Teil der Lizenz: [NAME-UND-LOGO.md](../../NAME-UND-LOGO.md). Das Original von [klehnst](https://github.com/klehnst/ESP32-Adrafruit-BNO085-HomeAssistant-Caravan-Levelsensor-MQTT) steht unter der MIT-Lizenz (Copyright (c) 2026 klehnst, siehe [LICENSE-klehnst-MIT](LICENSE-klehnst-MIT)); seine übernommenen Teile (Sensor-Grundlogik, Einbaulagen, HA-Kachel, Bilder, 3D-Druck-Dateien) bleiben auch unter dieser Lizenz nutzbar.

Ohne Gewähr, Nutzung auf eigene Gefahr; die Haftung ist ausgeschlossen, soweit gesetzlich zulässig (siehe [NOTICE.md](NOTICE.md)). Produkt- und Firmennamen sind Marken ihrer Inhaber, VanNode hat keine Verbindung zu ihnen.

