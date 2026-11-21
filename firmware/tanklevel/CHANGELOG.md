# Änderungen – tanklevel-node

Neueste Version oben. Der Abschnitt einer Version erscheint beim Tag `tanklevel-v<version>` automatisch als Text im GitHub-Release.

Die laufende Version steht in der Web-UI im Tab **WiFi** und in Home Assistant in der Geräteinfo. Bei Änderungen `FW_VERSION` in `config.h` hochzählen.

| Version | Änderungen |
| --- | --- |
| 1.10.1 | Über-Karte: keine eigene Liste fremder Teile mehr, „Hinweise und fremde Teile“ verlinkt die zentrale NOTICE.md des Projekts |
| 1.10.0 | **SuperSense** als Gebertyp für Frisch- und Grauwasser (Pro, Solid, Laser): Füllstand über eine Bluetooth-Verbindung statt Votronic-Geber, Auswahl im Tab Wasser / Grau unter **Gebertyp**, Gerätesuche, Faktor „Liter je Rohwert-Einheit“ (Einheit noch nicht bestätigt, Standard 0,001), Rohwert, Temperatur und Status in der Web-UI, „Für App freigeben“ (30 min). MQTT statt `voltage`: `ss_raw`, `temperature`, `ble_rssi`, `ble_state`. Bluetooth startet jetzt auch ohne Gassensor, wenn ein Tank SuperSense nutzt |
| 1.9.6 | Haftungsausschluss: Über-Karte zeigt „Ohne Gewähr – Nutzung auf eigene Gefahr“ und den Markenhinweis; Lizenzköpfe (SPDX) in allen Quelldateien |
| 1.9.5 | Lizenz jetzt GNU GPL v3 mit Zusatzbedingungen (Urhebervermerk bleibt, Forks kennzeichnen, Name/Logo nicht lizenziert); Über-Karte nennt Lizenz und fremde Teile |
| 1.9.4 | Webseite: Karte **Über VanNode** unter dem Update (Logo, Version, Copyright, Lizenz, Link zum Projekt) |
| 1.9.3 | Meldet in `<prefix>/info` die Hardware (`hw`, z. B. `esp32c3`, `atoms3`), das Display zeigt sie in der Geräteliste. Die Update-Seite nennt die passende Release-Datei (`…-node-<hw>-v….bin`), auch in der Fehlermeldung bei falschem Chip |
| 1.9.2 | Gewollte Neustarts (Einstellungen speichern, Neustart-Knopf, WLAN-Rückfall) zählen in der Update-Probezeit nicht mehr als Absturz – vorher konnten drei davon ein frisches Update auf die alte Version zurückrollen |
| 1.9.1 | VanNode-Symbol links neben der Überschrift der Webseite und als Favicon |
| 1.9.0 | Neue Namen: Hostname `tanklevel-node`, Setup-WLAN, Gerätename und Release-Datei `tanklevel-node-….bin`. Topics `camper/…` und Home-Assistant-IDs bleiben |
| 1.8.1 | Update-Prüfung vergleicht auch den Chip in der Kennung (gleich wie im Browser); ein Update über den Setup-AP löst keinen WLAN-Rückfall mehr aus |
| 1.8.0 | Sichere Updates: Firmware-Kennung, Prüfung der Datei im Browser und im Gerät (falsches Gerät, falscher Chip, .merged.bin, zu groß), Fehlergrund im Klartext, installierte Version und Ereignis-Log im Web-UI |
| 1.7.0 | Absicherung gegen Aussperren: Rückfall auf die vorherige Firmware, wenn ein Update kein WLAN bekommt; geänderte WLAN-Daten werden zurückgesetzt, wenn sie nicht funktionieren |
| 1.6.0 | Firmware (.bin) und Web-Oberfläche (index.html.gz) im Web-UI unter WiFi hochladbar, ohne USB; Arduino-OTA-Dateisystem-Upload hängt LittleFS vorher aus |
| 1.5.0 | Mopeka Standard (Std Check) als weiterer Sensortyp, Propan-Anteil einstellbar |
| 1.4.0 | Mopeka-Pro-Sensoren als Gassensor (Sensortyp im Tab Gas), Füllhöhe, Temperatur und Messqualität per MQTT |
| 1.3.0 | Optionaler Login für die Web-Oberfläche (Tab MQTT), Last Will mit QoS 1 |
| 1.2.2 | ESP32-S3-Unterstützung (Pins automatisch), Sendeleistung und „AP nur als Notfall“ im Tab WiFi, Neustartgrund, Laufzeit und WLAN-Abbruchgrund in der Web-UI |
| 1.2.1 | Web-UI gzip-komprimiert (Quelle jetzt in `web/`), kompakte Tab-Leiste (Symbol über Text, passt bis 320 px) |
| 1.2.0 | Optionaler Grauwasser-Sensor an GPIO1: Votronic Tankelektrode oder Tank-Sensor FL (mit Leer/Voll-Einlernen), eigene Tankgröße, Matrix und Filter |
| 1.1.0 | Sensoren einzeln abschaltbar (`config.h` und Web-UI), AP-Name mit MAC-Endung, Live-Seite nur mit Anzeigen, Filter im Tab Wasser |
| 1.0.0 | Erste kombinierte Version aus camper-waterlevel und camper-gaslevel |
