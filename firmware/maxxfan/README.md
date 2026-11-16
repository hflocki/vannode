# MaxxFan Node – VanTurtle-Board per MQTT

> **Achtung:** Ohne Gewähr, Nutzung auf eigene Gefahr – siehe [Haftungsausschluss](../../README.md#haftungsausschluss--nutzung-auf-eigene-gefahr). Der Lüfter wird aus der Ferne geschaltet – vorher sicherstellen, dass niemand gefährdet wird.

Firmware für einen **ESP32-C3** mit dem **VanTurtle**-Board (TCA9555-Port-Expander), die eine MaxxFan-Dachhaube per **MQTT** steuert – reines **Arduino**, ohne ESPHome. Aufbau wie [tanklevel-node](../tanklevel) und [heater-node](../heater):

- Tastendrücke der MaxxFan-Bedienung (Auto, Richtung, Ein/Aus, Schneller, Langsamer, Piep) für **Port 0 (RJ11)** und **Port 1 (RJ45)**
- Eingang **Auto Hold** und **Controller-LED** je Port
- **Stufe setzen** (0–10): 1–10 wie früher: 10× Langsamer bis zur kleinsten Stufe, dann (Stufe−1)× Schneller. **Stufe 0 = Haube zu** (wie „Schließen“)
- **Schließen:** 11× Langsamer, schließt die Haube aus jeder Stufe. „Ein/Aus“ öffnet die Haube nur
- **Sync auf AUS** (Auto drücken → auf Auto Hold warten → Ein/Aus), wie das bisherige ESPHome-Script
- Home-Assistant-Auto-Discovery, Web-Oberfläche mit Bedienung, Einstellungen und Firmware-Update

## Ordner

```
camper_maxxfan/        Sketch für die Arduino IDE
camper_maxxfan/web/    Quelle der Web-Oberfläche (index.html)
camper_maxxfan/data/   LittleFS-Inhalt: index.html.gz (komprimiert, wird hochgeladen)
```

| Datei | Inhalt |
| --- | --- |
| `config.h` | Pins, I2C-Adresse, Polarität, Standardwerte, `FW_VERSION` |
| `tca9555.*` | Treiber für den Port-Expander (keine Library nötig) |
| `fan_control.*` | Tastendrücke (nicht blockierend), Auto-Hold-Eingang, LED, Sync-Ablauf |
| `mqtt_ha.*` | MQTT, Home-Assistant-Discovery |
| `webserver.*` | Web-Oberfläche aus LittleFS, REST-API, Firmware-Update |
| `wifi_manager.*` | WLAN, feste IP, Setup-AP |
| `fwcheck.*`, `evlog.*` | Prüfung beim Firmware-Upload, Ereignis-Log (Kopien aus `firmware/_common`) |
| `safeguard.*` | Rückfall nach Update, WLAN-Änderung mit Rückweg (Kopie aus `firmware/_common`) |
| `settings.*` | Einstellungen im NVS |

## Hardware

Der ESP32-C3 sitzt zusammen mit dem VanTurtle-Board direkt im Lüftergehäuse und wird aus der Lüfterversorgung (9–14 V) gespeist. Laut VanTurtle eine Sicherung zwischen VCC und Batterie vorsehen; das Board wird etwas warm, also nicht dicht einpacken. Wird ein 5-V-Wandler für den ESP genutzt, muss er 14 V Eingang abkönnen.


| Signal | ESP32-C3 | `config.h` |
| --- | --- | --- |
| I2C SDA | GPIO5 | `I2C_SDA_PIN` |
| I2C SCL | GPIO6 | `I2C_SCL_PIN` |
| TCA9555 | Adresse 0x27 | `TCA9555_ADDR` |

Belegung des TCA9555 (wie in der VanTurtle-Vorlage), je Port `n` = 0 oder 1, Pin = `n*8 + x`:

| x | Funktion | Richtung |
| --- | --- | --- |
| 0 | Auto Hold | Eingang |
| 1 | Controller-LED | Ausgang |
| 2 | Auto | Taste |
| 3 | Richtung | Taste |
| 4 | Ein/Aus | Taste |
| 5 | Schneller | Taste |
| 6 | Langsamer | Taste |
| 7 | Piep | Taste |

Tasten werden als Impuls ausgegeben (Standard 150 ms, im Web-UI einstellbar), mit gleich langer Pause bis zum nächsten Druck am selben Port. Ausgänge sind wie bei ESPHome aktiv HIGH (`OUTPUT_ACTIVE_HIGH`), der Auto-Hold-Eingang aktiv HIGH (`HOLD_ACTIVE_HIGH`).

## Arduino IDE

Board-Paket, gemeinsame Bibliotheken, Board-Einstellungen, erstes Flashen, LittleFS-Plugin und Hilfe bei Upload-Problemen: **[docs/ARDUINO.md](../../docs/ARDUINO.md)**. Hier nur, was für dieses Gerät gilt.

| Einstellung | Wert |
| --- | --- |
| Board | ESP32-C3 (VanTurtle-Board), Flash Size 4MB |
| Zusätzliche Bibliotheken | keine |
| Sketch | `camper_maxxfan/camper_maxxfan.ino` |
| Web-Oberfläche | als LittleFS-Image (Plugin siehe docs/ARDUINO.md). Die Einstellungen liegen im NVS, ein LittleFS-Upload löscht sie nicht |

Vor dem ersten Flashen in `config_local.h` `DEFAULT_AP_PASSWORD` und `DEFAULT_OTA_PASSWORD` setzen. Ohne LittleFS-Image zeigt das Gerät eine Notseite mit Hinweis und Firmware-Update.

**Updates ohne USB** (das Gerät sitzt im Lüftergehäuse): im Web-UI unter **WiFi → Firmware- & Web-Update**. Welche Datei gemeint ist, erkennt die Firmware am Namen:

| Datei | Wirkung |
| --- | --- |
| `….ino.bin` (*Sketch → Kompilierte Binärdatei exportieren*) | neue Firmware, danach Neustart |
| `data/index.html.gz` | nur die Web-Oberfläche wird ersetzt, ohne Neustart |
| LittleFS-Image (Dateiname enthält `littlefs` oder `spiffs`) | ganzes Dateisystem, danach Neustart |

Alternativ per Arduino-OTA (`maxxfan-node` unter Netzwerk-Ports); auch ein Dateisystem-Upload über `espota -s` wird angenommen.

## Web-Oberfläche

Gleicher Aufbau und gleiche Optik wie bei tanklevel-node, temp-node und heater-node: Kopfzeile mit Statuspunkten (Board, WiFi, MQTT), Reiter mit Symbolen.

- **Live:** Ein/Öffnen, Schließen, Auto, Richtung, Langsamer, Schneller, Stufe 1–10 setzen, Auto-Hold-Anzeige, Sync auf AUS
- **MaxxFan:** Bedienteil je Port (Controller-LED, Piep als Verbindungstest), Ports ein/aus, Tastendruck-Dauer, Sync-Port
- **MQTT:** Broker, Topic-Prefix, Home-Assistant-Discovery, Web-Login
- **WiFi:** Verbindungsstatus, WLAN, feste IP, Access Point, Firmware-Update, Neustart, Werkseinstellungen

Zum Ausprobieren ohne Gerät: [`docs/mockups/maxxfan.html`](../../docs/mockups/maxxfan.html) im Repo-Hauptordner (siehe [Mockups](../../README.md#mockups)).

## Sicheres Firmware-Update und Ereignisse

Seit 1.7.0 trägt jede Firmware eine Kennung (`CAMPER-FW:maxxfan:<version>:<chip>;`, `fwcheck.*` aus `firmware/_common`). Im Web-UI unter **WiFi → Firmware- & Web-Update** steht die installierte Version. Eine gewählte Datei wird schon im Browser geprüft und beim Hochladen im Gerät noch einmal, bevor etwas aktiviert wird:

| Datei | Ergebnis |
| --- | --- |
| `.bin` dieses Geräts | Gerät, Version und Vergleich mit der installierten (neuer / gleich / älter, bei gleich oder älter mit Rückfrage) |
| `.merged.bin` aus der Arduino IDE | abgelehnt: das ist das komplette Flash-Abbild für USB, fürs Web-Update die `.bin` ohne „merged“ nehmen |
| `.bin` eines anderen Geräts oder Chips | abgelehnt, z. B. „Firmware für „tanklevel“, dieses Gerät ist „maxxfan““ |
| `.bin` ohne Kennung (ältere Firmware, fremde Datei) | gesperrt, mit „Ohne Kennung hochladen“ trotzdem möglich (für ein Downgrade auf eine Version vor 1.7.0) |
| `data/index.html.gz` | Web-Oberfläche, ohne Neustart |

Schlägt etwas fehl, nennt die Meldung den Grund statt nur „Update failed“. Die Karte **Ereignisse** darunter zeigt die letzten 24 Ereignisse: Start mit Grund (Einschalten, Absturz, Watchdog …), WLAN und MQTT verbunden/getrennt, Uploads mit Ergebnis, Rückfall und gespeicherte Einstellungen (`evlog.*`). Das Log bleibt bei Neustart, Absturz und Update erhalten, bei Stromausfall ist es leer. Updates über den Netzwerkport der Arduino IDE werden nicht geprüft, aber ebenfalls protokolliert.

## Absicherung gegen Aussperren

Damit das Gerät erreichbar bleibt, ohne dass jemand hinmuss (`safeguard.*` aus `firmware/_common`):

- **Rückfall nach Update:** Nach einem Firmware-Update über das Web-UI oder den IDE-Netzwerkport läuft die neue Version auf Probe. Hat sie nach 5 Minuten kein WLAN oder startet sie mehr als dreimal neu, startet der ESP wieder die vorherige Firmware (sie liegt noch im zweiten App-Bereich). Ab 1 Minute stabilem WLAN gilt die neue Version als gut. Flashen per USB wird nie zurückgerollt.
- **WLAN-Änderung mit Rückweg:** Werden SSID, Passwort oder feste IP geändert, während das Gerät verbunden ist, merkt es sich die funktionierenden Werte. Klappt die Verbindung mit den neuen nach dem Neustart nicht (etwa 1 Minute), setzt es die alten wieder ein und startet neu. Ist das Gerät beim Ändern gar nicht verbunden (Einrichtung über den Setup-AP), gelten die neuen Werte sofort.
- Was zuletzt passiert ist, steht im Web-UI unter **WiFi → Verbindungsstatus** in der Zeile **Absicherung**.

Der Schutz greift erst bei Updates, die von dieser Version **aus** gemacht werden. Das Update auf diese Version selbst ist noch nicht abgesichert.

## Erstbenutzung

1. Beim ersten Start läuft nur der Access Point **`MaxxFan-Node-XXXXXX`** (Passwort aus `config.h`, `XXXXXX` = MAC-Endung).
2. Verbinden, `http://192.168.4.1` öffnen (öffnet sich meist automatisch).
3. **Einstellungen**: WLAN (optional feste IP), MQTT-Broker, aktive Ports → Speichern & Neustart.
4. Ab Werk ist nur **Port 1 (RJ45)** aktiv. Wer RJ11 nutzt, schaltet Port 0 ein.

Ist das WLAN länger als eine Minute weg, öffnet sich der Setup-AP wieder; der ESP versucht parallel weiter, sich zu verbinden.

## MQTT

Basis-Topic `camper/maxxfan` (in der Web-UI änderbar). Je Anschluss `p<n>` gibt es Tasten-Befehle (`…/cmd/p1/faster` …), die Stufe (`…/cmd/p1/speed/set`) und „Haube schließen“. Alle Topics und Befehle: **[docs/TOPICS.md](../../docs/TOPICS.md#dachlüfter-campermaxxfan)**.

Beispiel:

```
mosquitto_pub -h <broker> -t camper/maxxfan/cmd/p1/faster -m PRESS
mosquitto_sub -h <broker> -t 'camper/maxxfan/#' -v
```

### Home Assistant

Mit Discovery erscheint ein Gerät **„MaxxFan Node“** mit Tasten-Entitäten je aktivem Port (Piep und LED unter *Konfiguration*), Stufen-Regler (0–10, 0 = zu), Taste „Close Lid“, LED-Schalter, Auto-Hold-Sensor, „Sync to OFF“, Neustart und Diagnose (WLAN, Port-Expander). Deaktivierte Ports werden aus HA entfernt.

**Umstieg von ESPHome:** Das alte ESPHome-Gerät in HA unter *Einstellungen → Geräte & Dienste → ESPHome* löschen. Die Entity-IDs ändern sich, Automationen und Dashboards (z. B. Display-Panels) müssen auf die neuen Entitäten oder direkt auf die MQTT-Topics umgestellt werden.

**Hinweis:** Die MaxxFan meldet ihren Zustand (an/aus, Stufe) nicht zurück – wie bei ESPHome werden nur Tastendrücke gesendet. „Sync auf AUS“ bringt sie in einen bekannten Zustand.

## Versionen

Die laufende Version steht in der Web-Oberfläche des Geräts. Alle Änderungen je Version: [CHANGELOG.md](CHANGELOG.md) (steht auch im jeweiligen GitHub-Release).

## Fertige Firmware

Wird im Haupt-Repo per GitHub Actions gebaut, siehe [Fertige Firmware](../../README.md#fertige-firmware).

## Lizenz

GNU GPL Version 3 mit zusätzlichen Bedingungen (Urhebervermerk bleibt erhalten, veränderte Fassungen sind zu kennzeichnen), siehe [LICENSE](LICENSE) und [NOTICE.md](NOTICE.md). Name und Logo „VanNode“ sind nicht Teil der Lizenz: [NAME-UND-LOGO.md](../../NAME-UND-LOGO.md).

Ohne Gewähr, Nutzung auf eigene Gefahr; die Haftung ist ausgeschlossen, soweit gesetzlich zulässig (siehe [NOTICE.md](NOTICE.md)). Produkt- und Firmennamen sind Marken ihrer Inhaber, VanNode hat keine Verbindung zu ihnen.

