# Pocket-Node – kleines mobiles Display (M5Stack Core2)

> Bis Version 1.5.8 hieß das Gerät **Road Buddy** (`firmware/roadbuddy`, Topic `camper/roadbuddy`), siehe [Umstieg vom Road Buddy](#umstieg-vom-road-buddy).

> **Achtung:** Ohne Gewähr, Nutzung auf eigene Gefahr – siehe [Haftungsausschluss](../../README.md#haftungsausschluss--nutzung-auf-eigene-gefahr).

Kleines Touch-Display mit Akku für das Wohnmobil auf Basis des **M5Stack Core2**, zum Mitnehmen: mit Magnet hängt es im Cockpit, am Küchenschrank am Eingang oder beim Wasserfüllen am Einfüllstutzen, damit man sieht, wie viel schon im Tank ist. Gedacht für den schnellen Blick: Stehe ich gerade, reichen Wasser und Gas? Dazu zeigt es per MQTT Neigung (Wasserwaage), Frischwassertank, Gasflasche, Temperaturen und Batterien. Zum Akku-Sparen schaltet sich das Display automatisch aus, danach geht das Gerät in den Deep Sleep, WLAN und MQTT sind dann komplett aus.

Optik und Farben entsprechen dem großen Wandpanel [display-node](../display): dunkle Karten mit dünnem Rand, farbiger Kreis mit Symbol, Dezimalkomma, gleiche Zustandsfarben und dieselbe Segmentanzeige für Batterien.

---

## Flashen

Allgemeines zur Arduino IDE und Hilfe bei Upload-Problemen: **[docs/ARDUINO.md](../../docs/ARDUINO.md)**. Die Pocket-Node hat aber eigene Bibliotheken, deshalb hier vollständig.

Gebaut wird mit der Arduino IDE (1.x und 2.x). Der Quellcode liegt in `pocket/`. Deine WLAN- und MQTT-Zugangsdaten kommen wie bei allen VanNode-Geräten in `pocket/config_local.h` (Vorlage `config_local.h.example`). Die Datei ist per `.gitignore` vom Git ausgeschlossen; `config.h` enthält nur die Standardwerte und bleibt unverändert.

### Voraussetzungen

1. **Board-Paket esp32 von Espressif Systems, 3.3.x** (wie bei allen VanNode-Geräten, siehe [docs/ARDUINO.md](../../docs/ARDUINO.md)). **Nicht** das eigene Board-Paket „M5Stack“ von M5Stack nehmen: Es beruht auf einem älteren ESP-IDF und bricht mit `'ESP_RST_USB' was not declared in this scope` ab. Wer es installiert hat, deinstalliert es am besten, damit man es nicht aus Versehen auswählt.
2. **Bibliotheken** über den Bibliotheksverwalter:
   - **M5Unified** (zieht **M5GFX** automatisch mit)
   - **PubSubClient** von Nick O'Leary
   - **ArduinoJson** von Benoit Blanchon (für die Temperaturliste)
3. Falls vorhanden, die alte Bibliothek **M5Core2** entfernen (Ordner `Documents/Arduino/libraries/M5Core2` löschen oder verschieben). Sie ist mit neuen ESP32-Cores nicht mehr kompatibel und führt zum Fehler `rom/miniz.h: No such file or directory`.

### Schritte

1. Den Ordner [`pocket/`](pocket/) in der Arduino IDE öffnen (`pocket.ino`). Der Ordner muss genauso heißen wie die Hauptdatei, sonst findet die IDE den Sketch nicht.
2. In diesem Ordner `config_local.h.example` nach `config_local.h` kopieren und die eigenen Zugangsdaten eintragen:

   ```cpp
   #define DEFAULT_WIFI_SSID      "YOUR_WIFI_SSID"
   #define DEFAULT_WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"

   #define DEFAULT_MQTT_SERVER    "192.168.1.10"
   #define DEFAULT_MQTT_PORT      1883
   #define DEFAULT_MQTT_USER      "YOUR_MQTT_USER"
   #define DEFAULT_MQTT_PASSWORD  "YOUR_MQTT_PASSWORD"
   ```

   Dort lassen sich auch Topics (`LEVEL_TOPIC`, `ROUTER_TOPIC`, `SELF_TOPIC`), der Login der Update-Seite und die angezeigten Seiten (`USE_…`) ändern.

3. Unter *Werkzeuge → Board → **esp32*** den Eintrag **M5Core2** wählen (nicht unter „M5Stack“). Partition Scheme auf dem Standard lassen: „Default (2 x 6.5 MB app, 3.6 MB SPIFFS)“ hat zwei App-Bereiche und damit Updates über die Webseite.
4. Den Core2 per USB-C verbinden, unter *Werkzeuge → Port* den Anschluss wählen (auf dem Mac z. B. `/dev/cu.usbserial-…`).
5. Hochladen.

Der Serial Monitor (115200 Baud) gibt jedes eingehende MQTT-Topic mit Wert aus, das hilft bei der Fehlersuche.


### Fehlersuche

- **`rom/miniz.h: No such file or directory`:** Die alte `M5Core2`-Bibliothek ist noch installiert, siehe Voraussetzungen Punkt 3.
- **`'ESP_RST_USB' was not declared in this scope`:** Gebaut wird mit dem Board-Paket „M5Stack“ oder einem esp32-Paket älter als 3.x. Board unter *esp32 → M5Core2* wählen und esp32 von Espressif 3.3.x installieren (Voraussetzungen Punkt 1).
- **„No credentials“ im Serial Monitor:** `config_local.h` fehlt oder enthält kein WLAN, und im Flash ist noch nichts gespeichert. `config_local.h.example` nach `config_local.h` kopieren und ausfüllen.
- **„Multiple libraries were found for WiFi.h“:** Nur eine Warnung. Die WiFi-Bibliothek aus dem ESP32-Core wird verwendet. Eine zusätzliche `WiFi`-Bibliothek in `Documents/Arduino/libraries` kann gelöscht werden.
- **Display zeigt falsche Pitch-/Roll-Werte:** Im Serial Monitor prüfen, welche Topics wirklich ankommen.
- **Upload bricht ab oder Port fehlt:** Anderes USB-Kabel (Datenkabel) testen, bei Bedarf den Treiber für den USB-Seriell-Chip installieren.
- **Gerät wacht nicht auf:** Display kurz antippen. Hilft das nicht, den Power-Knopf links am Gehäuse drücken, um das Gerät neu zu starten.
- **Seite zeigt `--`:** Es wurde noch kein Wert empfangen. Prüfen, ob der Sensor mit `retain` sendet, sonst erscheint der Wert erst bei der nächsten Meldung.

---

## Seiten

Die Seiten werden mit den beiden äußeren Touch-Tasten (links/rechts) umgeschaltet. Die mittlere Taste schaltet das Display aus.

| Seite | Inhalt |
| ----- | ------ |
| Start | VanNode-Logo (Symbol über dem Schriftzug). Erste Seite nach dem Einschalten, rechts davon folgen Level und die übrigen Seiten |
| Level | Fahrzeug von oben mit „vorne“ oben: an jedem Rad die Keilhöhe in cm, Räder mit Keil orange. In der Mitte ein Zielring, der Punkt wandert zur tiefen Seite (grün, wenn eben). Rechts „Eben“ oder „Nicht eben“, darunter Li/Re („0,9° rechts tief“) und Vo/Hi („eben“) und der größte Keil. Gleiche Darstellung wie am Wandpanel |
| Wassertank | Füllstand groß in %, Liter darunter, Sensor-Status; rechts ein Tank mit animierter Oberfläche. Unter 25 % gelb, unter 10 % rot (Tank und Zahl) |
| Gas | Füllstand groß in %, Restmenge darunter, Status und Akku des Gassensors; rechts die Gasflasche. Unter 15 % rot |
| Temperaturen | Sensoren von temp-node mit Namen und Wert. 1–2 Sensoren als große Zeilen, 3–4 als 2×2-Kacheln. Bei 5–8 Sensoren kommt automatisch eine zweite Temperaturseite dazu. Alarm (zu warm/zu kalt) = roter Rahmen. Farbe: blau unter 10 °C, grün bis 28 °C, gelb bis 45 °C, rot darüber |
| Batterien | Bis zu 3 Batterien von battery-node: Name, Ladestand (rot unter 20 %, gelb unter 40 %), Spannung, Leistung (+ = lädt, z. B. Solar), Restlaufzeit beim Entladen mit Uhr-Symbol („3,2 Tage“, „5 h 20 min“). Darunter eine Segmentanzeige mit 13 Feldern von Rot nach Grün nach dem Ladezustand, ohne Ladezustand (z. B. BM2) nach der Spannung von 11,8 bis 12,8 V. Der BM2 an der Starterbatterie zeigt statt Leistung seinen Zustand (Normal, Schwach, Kritisch, Lädt, Erhaltungsladung). Fehler oder schwache Starterbatterie = roter Rahmen |
| System | WLAN, Mobilfunk des Routers, MQTT-Broker, Akku, Firmware-Version und Laufzeit, IP-Adresse für die Update-Seite. Solange diese Seite offen ist, bleibt der Pocket-Node bis zu 15 Minuten wach (Update-Modus) |

Welche Seiten es gibt, stellst du in `config_local.h` ein (`USE_LEVEL`, `USE_WATER`, `USE_GAS`, `USE_TEMP`, `USE_BATTERY`). Start- und System-Seite gibt es immer. Das Logo kommt aus `logo_img.h`, erzeugt mit `tools/logo/make_logo.py`.

In der Kopfzeile stehen links Signalbalken: mit Router-Daten die Mobilfunkstärke (5 Balken nach RSRP bzw. RSSI, dazu das Netz, z. B. „LTE“), sonst die WLAN-Stärke des Pocket-Node (4 Balken). Grün = WLAN und MQTT ok, gelb = MQTT fehlt, rotes Kreuz = kein WLAN. In der Mitte die Seitenanzeige (aktive Seite als Pille), rechts der Akku des Core2 (blau mit „+“ beim Laden).

Level-Werte, die länger als 30 Sekunden nicht aktualisiert wurden, behalten ihre Farben; rechts unten steht dann „Werte alt“. Meldet ein Sensor `offline`, wird seine Seite ausgegraut.

---

## Stromsparen

| Zeit ohne Berührung | Verhalten |
| ------------------- | --------- |
| 60 s | Display aus (WLAN und MQTT laufen weiter, ein Antippen zeigt sofort aktuelle Werte) |
| 5 min nach Display-aus | Deep Sleep: WLAN und MQTT aus, minimaler Verbrauch |

- **Aufwecken:** Berührung irgendwo auf dem Display. Das Gerät startet neu, verbindet WLAN und MQTT und zeigt die zuletzt geöffnete Seite. Die weckende Berührung löst keinen Seitenwechsel aus.
- Das Display kann auch manuell mit der mittleren Taste ausgeschaltet werden, der Schlaf-Timer läuft dann ab diesem Moment.
- Zusätzlich weckt ein Timer das Gerät alle 24 Stunden, es schläft sofort wieder ein (Sicherheitsnetz, kostet praktisch keinen Strom).

---

## MQTT-Topics

Das Display ist reiner Empfänger und fragt nichts ab, die Aktualisierungsrate bestimmen die Sensoren. Bei `retain`-Meldungen erscheinen die Werte direkt nach dem Aufwachen. Es liest Ausrichtung (`camper/level/…`), Wasser und Gas (`camper/tanklevel/…`), `camper/temp/sensors`, `camper/battery/batteries` und das Mobilfunk-Topic des Routers; selbst sendet es nur `camper/pocket/status` und `…/info`. Die genaue Liste: **[docs/TOPICS.md](../../docs/TOPICS.md#pocket-node-camperpocket)**.

Die Topics für Wasser und Gas stehen als Konstanten am Anfang des Sketches, Level-, Router- und eigenes Topic (`ROUTER_TOPIC`, `SELF_TOPIC`) in `config.h` und lassen sich in `config_local.h` ändern. Die früheren getrennten Geräte (`camper/waterlevel`, `camper/gaslevel`) werden nicht mehr unterstützt.

Die Sensoren dazu:

- Level: [level-node](../level) (ESP32 + Adafruit BNO085, Fork des Levelsensors von klehnst)
- Wasser und Gas: [tanklevel-node](../tanklevel) (ein ESP32-C3 für Frischwasser, Grauwasser und Gas)
- Temperaturen: [temp-node](../temp) (bis zu 8 DS18B20 mit Alarm-Grenzwerten)
- Batterien: [battery-node](../battery) (Daly, Ective BM X, BM2 über Bluetooth)

Mehr zeigt der Pocket-Node bewusst nicht: Heizung, Lüfter und Warnungen laufen auf dem großen Wandpanel [display-node](../display).

---

## Update über WLAN

Der Pocket-Node hat eine kleine Webseite für Firmware-Updates. Sie ist nur erreichbar, solange er wach ist.

1. Display antippen und zur **System-Seite** blättern. Dort stehen IP-Adresse und Version. Solange die Seite offen ist, bleibt der Pocket-Node bis zu 15 Minuten wach.
2. Im Browser `http://pocket-node.local` oder die angezeigte IP öffnen.
3. Die `.bin` aus den GitHub Actions bzw. dem Release (`pocket-node-core2-v<version>.bin`, bis 1.5.8 `roadbuddy-node-core2-…`) auswählen und hochladen.
4. Während des Uploads zeigt das Display den Fortschritt, danach startet er neu.

### Sicheres Update, Ereignisse und Rückfall

Seit 1.3.0 hat jede Firmware eine Kennung (`CAMPER-FW:pocket:<version>:<chip>;`, bis 1.5.8 `roadbuddy`) und dieselbe Absicherung wie alle VanNode-Geräte (`fwcheck.*`, `evlog.*`, `safeguard.*` aus `firmware/_common`):

- **Prüfung beim Update** (Update-Webseite): Die installierte Version steht dabei. Eine gewählte Datei wird schon im Browser und beim Hochladen im Gerät noch einmal geprüft, bevor etwas aktiviert wird. Abgelehnt werden eine `.merged.bin` (komplettes Flash-Abbild für USB), Firmware für ein anderes Gerät oder einen anderen Chip und zu große Dateien. Eine Datei ohne Kennung (Version vor 1.3.0) geht nur mit „Ohne Kennung hochladen“. Fehler stehen im Klartext da statt „Update failed“.
- **Ereignisse:** Die letzten Ereignisse (Start mit Grund, WLAN und MQTT verbunden/getrennt, Uploads mit Ergebnis, Rückfall, gespeicherte Einstellungen) stehen in der Web-Oberfläche. Sie bleiben bei Neustart, Absturz und Update erhalten, nach einem Stromausfall ist das Log leer.
- **Rückfall nach Update:** Hat eine neue Firmware nach 5 Minuten kein WLAN oder startet sie mehr als dreimal neu, startet wieder die vorherige. Kam das Update über den Setup-AP (ohne WLAN), wird nur auf Neustart-Schleifen geachtet.
- Aufwachen aus dem Deep Sleep zählt nicht als Neustart und wird nicht eingetragen. Eine neue Firmware gilt nach 15 s mit WLAN als gut. WLAN-Daten kommen nur über USB (`config_local.h`), deshalb gibt es keinen WLAN-Rückweg.

**Einmal per USB mit eigener `config_local.h` flashen:** Der Build aus GitHub Actions enthält keine Zugangsdaten (ohne `config_local.h` gebaut). Ab Version 1.1.0 speichert eine Firmware mit eigenen Zugangsdaten WLAN, MQTT und den Web-Login im Flash, die Actions-Firmware liest sie von dort. Deshalb das erste Mal (und nach einem Löschen des Flash) per USB mit eigener `config_local.h` flashen, danach reichen die Updates über die Webseite.

Die Update-Seite verlangt eine Anmeldung, ab Werk `admin` / `vannode2026`; eigene Werte mit `DEFAULT_WEB_USER` / `DEFAULT_WEB_PASSWORD` in `config_local.h` (leer = Werks-Login).

---

## Anpassungen

Alle Einstellungen stehen am Anfang von `pocket.ino`:

| Konstante | Wirkung |
| --------- | ------- |
| `DISPLAY_TIMEOUT_MS` | Zeit bis das Display ausgeht (Standard 60 s) |
| `SLEEP_DELAY_MS` | Zeit von Display-aus bis Deep Sleep (Standard 5 min) |
| `STALE_MS` | Ab wann Level-Werte als veraltet gelten (Standard 30 s) |
| `BUBBLE_SIGN_X` / `BUBBLE_SIGN_Y` | Auf `-1.0f` setzen, wenn der Punkt nicht zur tiefen Seite wandert |
| `BUBBLE_FULL_SCALE_DEG` | Neigung, bei der der Punkt den Rand des Fahrzeugs erreicht (Standard 3°) |
| `TOPIC_TANK_*` | MQTT-Topics für Wasser und Gas |
| `TOPIC_TEMP_*` | MQTT-Topics von temp-node |
| `UPDATE_AWAKE_MS` | Wie lange die System-Seite den Pocket-Node wach hält (Standard 15 min) |
| Farben `COL_*`, `SEG_COL` | Palette wie am Wandpanel (`firmware/display/.../ui.cpp`), beim Ändern dort mitziehen |

WLAN-Hostname und MQTT-Client-ID beginnen mit `pocket-node`.

---

## Projektstruktur

```
firmware/pocket/
├── README.md                    – diese Übersicht
├── .gitignore
└── pocket/
    ├── pocket.ino               – Sketch (Seiten, MQTT, Stromsparen)
    ├── logo_img.h               – Logo der Startseite (erzeugt, nicht von Hand ändern)
    ├── webota.cpp / .h          – Update-Webseite
    ├── config.h                 – Standardwerte (nicht ändern)
    └── config_local.h.example   – Vorlage für eigene WLAN- und MQTT-Daten (`config_local.h`)
```

---

## Hinweise

- Zugangsdaten gehören nur in `config_local.h`, diese Datei ist in `.gitignore` eingetragen und darf nie committet werden.
- **Umstieg von Version 1.5.7 oder älter:** Früher war `config.h` die eigene Datei. Die Werte aus der alten `config.h` in eine neue `config_local.h` übertragen (`ssid` → `DEFAULT_WIFI_SSID`, `password` → `DEFAULT_WIFI_PASSWORD`, `mqtt_server` → `DEFAULT_MQTT_SERVER`, `mqtt_port` → `DEFAULT_MQTT_PORT`, `mqtt_user` → `DEFAULT_MQTT_USER`, `mqtt_pass` → `DEFAULT_MQTT_PASSWORD`, `mqtt_topic` → `LEVEL_TOPIC`, `WEB_USER`/`WEB_PASS` → `DEFAULT_WEB_USER`/`DEFAULT_WEB_PASSWORD`).
- Der Prozentwert des Core2-Akkus ist nur eine Schätzung aus der Spannung.

---

## Umstieg vom Road Buddy

Ab 1.6.0 heißt der Road Buddy **Pocket-Node**: Ordner `firmware/pocket`, Sketch `pocket/pocket.ino`, Hostname `pocket-node.local`, eigenes Topic `camper/pocket`, Release-Dateien `pocket-node-core2-v….bin` und Tags `pocket-v…`. Die gelesenen Topics und die Bedienung bleiben gleich.

1. **Einmal per USB flashen.** Die alte Update-Seite nimmt die neue Firmware nicht an, weil die Kennung jetzt `pocket` statt `roadbuddy` lautet („Firmware für pocket, dieses Gerät ist roadbuddy“). Danach gehen Updates wieder über die Webseite.
2. **WLAN-Daten bleiben erhalten:** Die Firmware liest die gespeicherten Zugangsdaten unter dem alten Namen weiter.
3. **Alte retained Topics löschen** (sonst steht im Display ein „Road Buddy“ als offline):
   ```bash
   mosquitto_pub -h <broker> -t camper/roadbuddy/status -r -n
   mosquitto_pub -h <broker> -t camper/roadbuddy/info -r -n
   ```

## Versionen

Die laufende Version steht in der Web-Oberfläche des Geräts. Alle Änderungen je Version: [CHANGELOG.md](CHANGELOG.md) (steht auch im jeweiligen GitHub-Release).

## Fertige Firmware

Wird im Haupt-Repo per GitHub Actions gebaut, siehe [Fertige Firmware](../../README.md#fertige-firmware). Der CI-Build hat keine `config_local.h`, die `.bin` enthält also keine Zugangsdaten. Sie übernimmt die Zugangsdaten, die eine eigene Firmware vorher im Flash gespeichert hat (siehe [Update über WLAN](#update-über-wlan)).

## Lizenz

GNU GPL Version 3 mit zusätzlichen Bedingungen (Urhebervermerk bleibt erhalten, veränderte Fassungen sind zu kennzeichnen), siehe [LICENSE](LICENSE) und [NOTICE.md](../../NOTICE.md). Name und Logo „VanNode“ sind nicht Teil der Lizenz: [NAME-UND-LOGO.md](../../NAME-UND-LOGO.md).

Ohne Gewähr, Nutzung auf eigene Gefahr; die Haftung ist ausgeschlossen, soweit gesetzlich zulässig (siehe [NOTICE.md](../../NOTICE.md)). Produkt- und Firmennamen sind Marken ihrer Inhaber, VanNode hat keine Verbindung zu ihnen.

