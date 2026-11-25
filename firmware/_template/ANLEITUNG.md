# Vorlage für ein neues VanNode-Gerät

`firmware/_template` ist ein fertiges, lauffähiges Muster-Gerät („Example Node“). Es enthält alles, was alle Geräte im Projekt gemeinsam haben: WLAN mit Setup-WLAN, Web-Oberfläche, Einstellungen, MQTT mit Home-Assistant-Discovery und Updates ohne USB. Für ein neues Gerät kopierst du es mit einem Befehl und füllst nur noch den Sensor-Teil.

Das Muster misst als Beispiel eine Spannung an einem Analog-Pin. So kannst du es direkt flashen und ausprobieren, bevor du etwas änderst.

## 1. Neues Gerät anlegen

Im Repo-Ordner, in einer virtuellen Umgebung (das Skript braucht nur die Python-Standardbibliothek):

```bash
cd ~/projekte/github/vannode
python3 -m venv .venv && source .venv/bin/activate
pip install -r firmware/_template/tools/requirements.txt

python firmware/_template/tools/new_device.py waste "Abwasser"
```

Das legt `firmware/waste/` an und ersetzt dabei überall die Platzhalter:

| In der Vorlage | Wird zu (Beispiel) | Wo |
|---|---|---|
| `camper_example` | `camper_waste` | Sketch-Ordner, `.ino`, HA-Geräte-ID |
| `example-node` | `waste-node` | Hostname, `waste-node.local`, OTA |
| `camper/example` | `camper/waste` | MQTT-Prefix |
| `Example-Node` | `Abwasser-Node` | Setup-WLAN `Abwasser-Node-XXXXXX` |
| `Example Node` | `Abwasser Node` | Titel der Web-Oberfläche, Gerät in HA |

Gibt es den Ordner schon, bricht das Skript ab und ändert nichts.

## 2. Mit Leben füllen

Alle Stellen, an denen du etwas tun musst, sind im Code mit **`TODO`** markiert (`grep -rn TODO firmware/waste`).

| Datei | Was du änderst |
|---|---|
| `sensor.cpp` | **Das Wichtigste.** `VALUES[]` = Liste der Werte (Key, Name, Einheit, HA-Geräteklasse, Icon, Nachkommastellen). `sensorInit()` = Hardware einrichten. `measure()` = messen und `sensorData.values[i]` in derselben Reihenfolge wie `VALUES[]` füllen. MQTT-Topics, HA-Entitäten und Kacheln der Web-Oberfläche entstehen daraus von selbst. |
| `config.h` | Pins und Standardwerte (`SENSOR_PIN`, `DEFAULT_INTERVAL_S` …), eigene Einstellungen im `struct AppConfig` |
| `webserver.cpp` | Eigene Einstellungen an 5 Stellen ergänzen: `setDefaults()`, `handleApiGetConfig()`, Prüfen und Übernehmen in `handleApiSaveConfig()`, `loadConfig()`, `saveConfig()`. Eigene Aktionen wie `/api/measure` |
| `mqtt_ha.cpp` | Eigene Befehle in `onMessage()` (`<prefix>/cmd/...`) und passende HA-Entitäten (Taste, Schalter) in `mqttPublishDiscovery()` |
| `web/index.html` | Tab **Einstellungen**: Eingabefelder `cfg-…`, dazu `loadSetup()` und `saveSettings()`. Die Live-Kacheln brauchen keine Änderung. |
| `README.md` | TODO-Stellen: Beschreibung, Hardware, zusätzliche Bibliotheken, Basis-Topic. Allgemeines zur Arduino IDE steht in `docs/ARDUINO.md`, die Topics in `docs/TOPICS.md` |
| `docs/TOPICS.md` | eigenen Abschnitt mit allen Topics und Befehlen anlegen (Vorlage: Abschnitt *Muster-Gerät*) und in der *Übersicht* eintragen |
| `README.md` | Setup-WLAN und Passwort ab Werk in die Tabelle *Setup-WLAN und Passwörter ab Werk* eintragen |
| `docs/ARDUINO.md` | zusätzliche Bibliothek in die Tabelle *Zusätzlich je Gerät* eintragen (mit Quelle `owner/repo` und Version wie in `build.yml`, sonst schlägt die CI-Prüfung fehl) |
| `CHANGELOG.md` | Titel auf das neue Gerät ändern, alte Zeilen löschen, erste Version eintragen. Bei jeder neuen `FW_VERSION` eine Zeile oben ergänzen – sie wird beim Tag zum Text des GitHub-Release. |

**Einstellung hinzufügen, Beispiel „Grenzwert“:**
1. `config.h`: `float limit;` in `AppConfig`, dazu `#define DEFAULT_LIMIT 50.0f`.
2. `webserver.cpp`: in `setDefaults()` `config.limit = DEFAULT_LIMIT;`, in `handleApiGetConfig()` `doc["limit"] = config.limit;`, in `handleApiSaveConfig()` prüfen und `config.limit = …` setzen, in `loadConfig()`/`saveConfig()` lesen und schreiben.
3. `web/index.html`: Eingabefeld `cfg-limit`, in `loadSetup()` füllen, in `saveSettings()` mitschicken.

**Regeln, die für alle Geräte gelten:**
- Code-Kommentare auf Englisch, Texte der Web-Oberfläche auf Deutsch.
- Nichts Blockierendes in Web-Handlern: nur ein Flag setzen, die Arbeit in `loop()` machen (wie `sensorRequestMeasure()`).
- Zugangsdaten nur in `config_local.h` (wird von git ignoriert) oder im Web-UI.
- Nach jeder Änderung an `web/index.html`: `python3 tools/web/pack_web.py <id>` im Repo-Hauptordner (packt nach `data/index.html.gz`; die GitHub Action prüft, ob beide zusammenpassen).

## 3. Bauen und flashen

1. `config_local.h.example` → `config_local.h` kopieren, WLAN, MQTT (`192.168.8.2`) und Passwörter eintragen.
2. Arduino IDE: Board **ESP32C3 Dev Module** (oder S3), USB CDC On Boot **Enabled**, Partition **Minimal SPIFFS**.
3. Per USB hochladen. Danach `http://<ip>/` öffnen. Solange die Web-Oberfläche fehlt, steht dort ein Upload-Formular: `data/index.html.gz` hochladen.
4. Ab dann Updates nur noch über das Web-UI (*WiFi → Firmware- & Web-Update*).

## 4. Einbinden

- **CI** (`.github/workflows/build.yml`), damit GitHub das Gerät mitbaut und bei einem Tag `waste-v1.0.0` eine `.bin` veröffentlicht. Unter `matrix: include:` ergänzen:
  ```yaml
  - device: waste
    target: esp32c3
    fqbn: esp32:esp32:esp32c3:CDCOnBoot=cdc,PartitionScheme=min_spiffs
    sketch: firmware/waste/camper_waste
    libs: ''        # zusätzliche Bibliotheken als git-URLs mit #Version
  ```
- **Haupt-README:** Zeile in der Gerätetabelle und bei den Lizenzen.
- **Router:** feste IP (Static Lease) für die MAC im RUT956.
- **Display / Pocket-Node:** Werte erscheinen über eine eigene Kachel (Display: *Seiten & Kacheln*, Topic `camper/waste/<key>`) ohne Firmware-Änderung. Eine feste Seite oder Warnungen brauchen eine Anpassung der Display-Firmware.

## Aufbau der Vorlage

```
firmware/_template/
├── ANLEITUNG.md                 diese Anleitung (wird nicht mitkopiert)
├── README.md                    README des neuen Geräts (mit TODOs)
├── CHANGELOG.md                 Änderungen je Version (Text der Releases)
├── LICENSE                      GNU GPL v3 (Zusatzbedingungen in NOTICE.md)
├── tools/new_device.py          Kopier-Skript (wird nicht mitkopiert)
└── camper_example/
    ├── camper_example.ino       setup()/loop(), OTA, serielle Befehle
    ├── config.h                 Konstanten, AppConfig, SensorData
    ├── config_local.h.example   persönliche Werte (Kopie bleibt außerhalb von git)
    ├── sensor.cpp / .h          ← dein Teil: Werteliste, Hardware, Messung
    ├── mqtt_ha.cpp / .h         MQTT, Befehle, Home-Assistant-Discovery
    ├── webserver.cpp / .h       Web-UI, REST-API, Upload, config.json
    ├── wifi_manager.cpp / .h    WLAN, Setup-WLAN, Wiederverbindung (wie bei allen Geräten)
    ├── web/index.html           Web-Oberfläche (lesbar)
    └── data/index.html.gz       dieselbe, komprimiert – das liegt auf dem Gerät
```

Die Vorlage selbst wird von der CI mitgebaut, damit sie immer kompiliert.
