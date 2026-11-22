# Änderungen – battery-node

Neueste Version oben. Der Abschnitt einer Version erscheint beim Tag `battery-v<version>` automatisch als Text im GitHub-Release.

| Version | Änderungen |
| --- | --- |
| 2.6.0 | **Neue Batterie-Typen:** JBD / Xiaoxiang Smart-BMS (viele Akku-Marken, Dienst FF00), JK-BMS (JK02, alle Firmware-Stände ab 10) und LiTime / Redodo / PowerQueen – mit Ladestand, Spannung, Strom, Restkapazität, Zellen, Temperaturen, Zyklen und Fehlern als Text. Protokolle aus aiobmsble (Apache-2.0), an dessen Mitschnitten getestet; an echter Hardware noch **ungetestet**. Daly, Ective und BM2 unverändert |
| 2.5.1 | Gemeinsamer Kern: die Umstellung alter Namen gilt nur noch für den ble-node (`NODE_RENAMED_FROM` in dessen `config.h`); Hinweistexte der Web-Oberfläche nennen `ble-node` / `camper/ble`. Für den battery-node keine Änderung der Funktion |
| 2.5.0 | **Eigene Firmware für den battery-node** (M5Stack AtomS3 Lite, Release-Datei `battery-node-atoms3-…`). Bis ble 2.4.0 lief er mit der ble-Firmware; Funktionen, Topics (`camper/battery/…`), Home-Assistant-Einträge und Einstellungen bleiben gleich. Enthalten sind nur noch die Batterie-Plugins Daly BMS, Ective BM X und BM2, höchstens 6 Geräte. Neue Gerätekennung `battery`: **einmal über die Arduino IDE flashen** (USB ohne „Erase All Flash“ oder Netzwerk-Port), danach `data/index.html.gz` hochladen, siehe README „Umstieg vom ble-node“. Setup-WLAN ab Werk `Battery-Node-XXXXXX` / `battery1234`. Kern und Web-Oberfläche teilt er sich weiter mit dem ble-node (`firmware/_common/ble`); die Über-Karte verlinkt auf `firmware/battery` |
