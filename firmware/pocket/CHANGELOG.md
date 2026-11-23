# Änderungen – pocket-node

Neueste Version oben. Der Abschnitt einer Version erscheint beim Tag `pocket-v<version>` (bis 1.5.8 `roadbuddy-v<version>`) automatisch als Text im GitHub-Release.

| Version | Änderungen |
| --- | --- |
| 1.6.2 | **Web-Login immer an:** ab Werk Benutzer `admin`, Passwort `vannode2026`. Geräte, die bisher ohne Login liefen, bekommen ihn mit diesem Update; leere Felder behalten den gespeicherten Wert, abschalten lässt sich die Anmeldung nicht mehr. Gilt für die Update-Seite; eigene Werte weiter über `config_local.h`. |
| 1.6.1 | Update-Seite mit der gemeinsamen Karte **Über VanNode** wie alle anderen Geräte (Logo, Urheber, Lizenz, Links auf Lizenztext und zentrale NOTICE.md) statt der eigenen Fußzeile |
| 1.6.0 | **Road Buddy heißt jetzt Pocket-Node:** Ordner `firmware/pocket`, Sketch `pocket.ino`, Firmware-Kennung `pocket`, Hostname `pocket-node.local`, eigenes Topic `camper/pocket`, Release-Dateien `pocket-node-core2-…`. Einmal per USB flashen (die alte Update-Seite lehnt die neue Kennung ab); gespeicherte WLAN-Daten bleiben erhalten. Alte retained Topics `camper/roadbuddy/status` und `…/info` löschen |
| 1.5.8 | Eigene Einstellungen jetzt in `config_local.h` wie bei allen anderen Geräten (Vorlage `config_local.h.example`). `config.h` ist eine normale Datei im Repo mit den Standardwerten; `config.example.h` entfällt. Neue Namen: `DEFAULT_WIFI_SSID`, `DEFAULT_WIFI_PASSWORD`, `DEFAULT_MQTT_SERVER`, `DEFAULT_MQTT_PORT`, `DEFAULT_MQTT_USER`, `DEFAULT_MQTT_PASSWORD`, `DEFAULT_WEB_USER`, `DEFAULT_WEB_PASSWORD`, `LEVEL_TOPIC`. Funktion unverändert |

Frühere Versionen sind hier nicht aufgeführt; ihre Änderungen stehen in der Git-Historie.
