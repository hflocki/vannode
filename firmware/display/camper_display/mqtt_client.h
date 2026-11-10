// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#pragma once
#include <Arduino.h>
#include <vector>

namespace mqttc {
  void begin();
  void loop();
  bool connected();
  void resubscribe();            // after tile / prefix changes
  void publishSelfState();
  bool publishTo(const String& topic, const String& payload);   // e.g. for automation rules
  bool publishRetained(const String& topic, const String& payload);   // "" deletes a retained topic

  // Topic finder (helps to find zigbee2mqtt and other topics)
  struct Found { String topic; String payload; };
  void finderStart(const String& filter);
  void finderStop();
  bool finderActive();
  const String& finderFilter();
  std::vector<Found> finderTake();       // new / changed topics since the last call
  std::vector<Found> finderAll();
}

// Commands shared by the touch UI and the web page. Only known commands are
// accepted; everything else is rejected (returns false).
namespace cmd {
  bool heater(const String& key, const String& value);
  bool fan(const String& key, const String& value = "");
  // roof hatch on the ble-node: position (0..100), open, close, or a preset auf / zu / regen / lueften
  bool heki(const String& key, const String& value = "");
  bool level(const String& key);
  bool tile(size_t index, const String& action);   // on / off / toggle / press / <number>
  bool light(size_t device, const String& key, const String& value);   // own Zigbee light, see sensors::lightCommand
}
