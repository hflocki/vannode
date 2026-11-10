// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>
#include "config.h"

void webserverInit();     // mounts LittleFS, loads the config, registers the routes
void webserverStart();    // after WiFi is up
void webserverLoop();     // deferred work (flash writes, restart) + live updates
void loadConfig();
void saveConfig();

#endif // WEBSERVER_H
