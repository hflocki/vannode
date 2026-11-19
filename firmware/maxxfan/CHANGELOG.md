# Änderungen – maxxfan-node

Neueste Version oben. Der Abschnitt einer Version erscheint beim Tag `maxxfan-v<version>` automatisch als Text im GitHub-Release.

| Version | Änderungen |
| --- | --- |
| 1.9.0 | Optionaler Haubenkontakt: Reedkontakt an einem freien GPIO (in der Web-UI wählbar, Standard aus) meldet `…/lid` = `open`/`closed`, Anzeige im Reiter Live, HA-Fenster-Sensor „Lid“; Eintrag im Ereignis-Log, wenn die Haube 90 s nach „Schließen“ noch offen ist |
| 1.8.6 | Haftungsausschluss: Über-Karte zeigt „Ohne Gewähr – Nutzung auf eigene Gefahr“ und den Markenhinweis; Lizenzköpfe (SPDX) in allen Quelldateien |
| 1.8.5 | Lizenz jetzt GNU GPL v3 mit Zusatzbedingungen (Urhebervermerk bleibt, Forks kennzeichnen, Name/Logo nicht lizenziert); Über-Karte nennt Lizenz und fremde Teile |
| 1.8.4 | Webseite: Karte **Über VanNode** unter dem Update (Logo, Version, Copyright, Lizenz, Link zum Projekt) |
| 1.8.3 | Meldet in `<prefix>/info` die Hardware (`hw`, z. B. `esp32c3`, `atoms3`), das Display zeigt sie in der Geräteliste. Die Update-Seite nennt die passende Release-Datei (`…-node-<hw>-v….bin`), auch in der Fehlermeldung bei falschem Chip |
| 1.8.2 | Gewollte Neustarts (Einstellungen speichern, Neustart-Knopf, WLAN-Rückfall) zählen in der Update-Probezeit nicht mehr als Absturz – vorher konnten drei davon ein frisches Update auf die alte Version zurückrollen |
| 1.8.1 | VanNode-Symbol links neben der Überschrift der Webseite und als Favicon |
| 1.8.0 | Neue Namen: Standard-Hostname `maxxfan-node` (ein gespeicherter Hostname bleibt), Setup-WLAN, Gerätename und Release-Datei `maxxfan-node-….bin`. Topics `camper/…` und Home-Assistant-IDs bleiben |
| 1.7.1 | Update-Prüfung vergleicht auch den Chip in der Kennung (gleich wie im Browser); ein Update über den Setup-AP löst keinen WLAN-Rückfall mehr aus |
| 1.7.0 | Sichere Updates: Firmware-Kennung, Prüfung der Datei im Browser und im Gerät (falsches Gerät, falscher Chip, .merged.bin, zu groß), Fehlergrund im Klartext, installierte Version und Ereignis-Log im Web-UI |
| 1.6.0 | Absicherung gegen Aussperren: Rückfall auf die vorherige Firmware, wenn ein Update kein WLAN bekommt; geänderte WLAN-Daten werden zurückgesetzt, wenn sie nicht funktionieren |
| 1.5.0 | Stufe setzen wieder wie in 1.3.0 (von unten), Stufe 0 schließt die Haube (Web-UI, MQTT, HA-Regler) |
| 1.4.0 | „Schließen“ (Web-UI, MQTT `cmd/p<n>/close`, HA-Taste); Stufe setzen von oben (10× Schneller), damit die Haube dabei nicht zugeht; Web-Oberfläche und LittleFS-Image per Web-UI oder OTA hochladbar (kein USB nötig) |
| 1.3.0 | Web-Oberfläche aus LittleFS (`data/index.html.gz`) wie bei tanklevel und temp, Notseite mit Firmware-Update, falls das Image fehlt |
| 1.2.0 | Web-Oberfläche im gleichen Stil wie die anderen Camper-Geräte (Statuspunkte, Reiter Live / MaxxFan / MQTT / WiFi) |
| 1.1.0 | Stufe 1–10 setzen (MQTT, HA-Regler, Web-UI) |
| 1.0.0 | Erste Arduino/MQTT-Version, ersetzt die ESPHome-Konfiguration |
