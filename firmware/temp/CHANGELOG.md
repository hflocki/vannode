# Änderungen – temp-node

Neueste Version oben. Der Abschnitt einer Version erscheint beim Tag `temp-v<version>` automatisch als Text im GitHub-Release.

Die laufende Version steht in der Web-UI im Tab **WiFi** und in Home Assistant in der Geräteinfo. Bei Änderungen `FW_VERSION` in `config.h` hochzählen.

| Version | Änderungen |
|---|---|
| 1.4.9 | Neue Karte **Einstellungen sichern** unter dem Firmware-Update (gemeinsamer Baustein `firmware/_common/web/vannode-backup.js`): alle Einstellungen als Datei sichern und wieder einspielen. Passwörter und die Anmeldung sind nicht enthalten und bleiben beim Einspielen erhalten; WLAN, IP, Gerätename und MQTT-Server nur mit Häkchen. Läuft nur in der Web-Seite über die vorhandene `/api/config`, die Firmware selbst ist unverändert |
| 1.4.8 | Über-Karte kennt jetzt auch den Pocket-Node (gemeinsame Karte auf allen Geräte-Webseiten); keine sichtbare Änderung |
| 1.4.7 | Über-Karte: keine eigene Liste fremder Teile mehr, „Hinweise und fremde Teile“ verlinkt die zentrale NOTICE.md des Projekts |
| 1.4.6 | Haftungsausschluss: Über-Karte zeigt „Ohne Gewähr – Nutzung auf eigene Gefahr“ und den Markenhinweis; Lizenzköpfe (SPDX) in allen Quelldateien |
| 1.4.5 | Lizenz jetzt GNU GPL v3 mit Zusatzbedingungen (Urhebervermerk bleibt, Forks kennzeichnen, Name/Logo nicht lizenziert); Über-Karte nennt Lizenz und fremde Teile |
| 1.4.4 | Webseite: Karte **Über VanNode** unter dem Update (Logo, Version, Copyright, Lizenz, Link zum Projekt) |
| 1.4.3 | Meldet in `<prefix>/info` die Hardware (`hw`, z. B. `esp32c3`, `atoms3`), das Display zeigt sie in der Geräteliste. Die Update-Seite nennt die passende Release-Datei (`…-node-<hw>-v….bin`), auch in der Fehlermeldung bei falschem Chip |
| 1.4.2 | Gewollte Neustarts (Einstellungen speichern, Neustart-Knopf, WLAN-Rückfall) zählen in der Update-Probezeit nicht mehr als Absturz – vorher konnten drei davon ein frisches Update auf die alte Version zurückrollen |
| 1.4.1 | VanNode-Symbol links neben der Überschrift der Webseite und als Favicon |
| 1.4.0 | Neue Namen: Hostname `temp-node`, Setup-WLAN, Gerätename und Release-Datei `temp-node-….bin`. Topics `camper/…` und Home-Assistant-IDs bleiben |
| 1.3.0 | Sicheres Update (Kennung, Prüfung im Browser und im Gerät, Fehler im Klartext), Ereignis-Log, Rückfall auf die vorherige Firmware und WLAN-Änderung mit Rückweg; auf dem ESP8266 nur Prüfung und Log |
| 1.2.0 | Firmware (.bin) und Web-Oberfläche (index.html.gz) im Web-UI unter WiFi hochladbar, ohne USB (ESP32-C3 und ESP8266); Arduino-OTA-Dateisystem-Upload hängt LittleFS vorher aus |
| 1.0.0 | Erste Version: bis zu 8 DS18B20, automatische Erkennung, Web-UI, MQTT mit HA-Discovery |
| 1.1.0 | Alarm-Grenzwerte je Sensor mit Verzögerung (`<id>/alarm`, `alarm`, `alarm_text`, HA-Binärsensoren), JSON-Liste `<prefix>/sensors` für Displays, Web-Login, Last Will mit QoS 1 |
