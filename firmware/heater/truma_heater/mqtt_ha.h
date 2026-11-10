// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// MQTT client, topics in the inetbox2mqtt layout and Home Assistant auto discovery.
//
// State:   <base>/control_status/<key>   (retained)
// Command: <base>/set/<key>
// LWT:     <base>/status                 online / offline
#pragma once

#include <Arduino.h>

namespace mqtt {

void begin();
void loop();
bool connected();
// Force a full state publish (e.g. after settings change).
void request_publish();
// Send the Home Assistant discovery again (e.g. after the heater model changed).
void request_discovery();

}  // namespace mqtt
