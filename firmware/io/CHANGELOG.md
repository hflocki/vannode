# Änderungen – io-node

Neueste Version oben. Der Abschnitt einer Version erscheint beim Tag `io-v<version>` automatisch als Text im GitHub-Release.

| Version | Änderungen |
|---|---|
| 1.0.0 | **Erste Version.** Relais, Eingänge, DS18B20-Temperaturen und Analogwerte auf einem ESP32-C3 Super Mini, alles in der Web-Oberfläche eingerichtet: Relais an GPIOs, an einer 74HC595-Kette (bis 4 Chips) oder an I2C-Expandern (MCP23017 / PCF8574, bis 8 Chips), *schaltet bei LOW / HIGH*, Zustand nach dem Start (aus, an, letzter) und Impuls; Eingänge mit *Umkehren*, Pull-up und Entprellen; DS18B20 an einem Pin mit Suche; Analog mit Zweipunkt-Umrechnung. Pin-Übersicht mit Warnung bei doppelt belegten Pins und Boot-Pins. MQTT `camper/io/<id>`, Befehle `camper/io/cmd/<id>` (`ON`, `OFF`, `TOGGLE`), Home Assistant: Relais als Schalter, Eingänge als Binärsensor. Ersatz-Broker, Englisch (`UI_LANG`) und alle gemeinsamen System-Karten wie bei den anderen Nodes. |
