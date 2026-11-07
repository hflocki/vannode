#ifndef CT_MQTT_HA_H
#define CT_MQTT_HA_H

#include <Arduino.h>
#include "../../config.h"
#include "plugin.h"

void mqttInit();
void mqttLoop();
bool mqttIsConnected();

// Re-send the discovery on the next loop (e.g. after a plugin setting changed entities)
void mqttRequestDiscovery();

// Device removed in the web UI: delete its Home Assistant entities and retained values
void mqttForget(Plugin* p);
// Home Assistant node ID derived from the MQTT prefix ("camper/ble" -> "camper_ble")
const char* mqttNodeId();

#endif // CT_MQTT_HA_H
