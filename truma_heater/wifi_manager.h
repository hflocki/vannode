// WiFi station with optional static IP, AP fallback + captive portal for first setup.
#pragma once

#include <Arduino.h>

namespace wifi {

void begin();
void loop();
bool connected();
bool ap_active();
String ip();
int rssi();

}  // namespace wifi
