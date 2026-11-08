#pragma once
#include <Arduino.h>
#include <vector>

// LVGL user interface on the panel
namespace ui {
  void splash(const char* step);        // start screen with logo; call again to change the step text
  void begin();
  void loop();                          // refresh values, header, pending commands
  void requestRebuild();                // pages / tiles / builtin pages changed
  void showPageByName(const String& nameOrIndex);
  String warningText();                 // all active warnings, one per line ("" = none)
  String warningText(std::vector<uint8_t>& cats);   // same, cats = kind (WarnCat) of each line
}
