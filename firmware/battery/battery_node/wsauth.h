// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Shared source – edit only in firmware/_common, then run tools/common/sync_common.py
//
// Login for the WebSocket of the device page. Most browsers send the page's basic auth along with
// the WebSocket upgrade, Safari (iPhone, iPad, Mac) does not: the page stayed empty there. So the
// page first fetches a token from GET /api/wstoken (behind the login, wsauth::send) and opens
// /ws?t=<token>. The handshake takes the login or the token (wsauth::tokenOk). The token is random
// and stays the same until the next restart.
#ifndef WSAUTH_H
#define WSAUTH_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#if !defined(ESP8266)
#include <esp_random.h>
#endif

namespace wsauth {

inline const char* token() {
    static char t[17] = "";
    if (!t[0]) {
        for (int i = 0; i < 2; i++) {
#if defined(ESP8266)
            uint32_t r = ESP.random();
#else
            uint32_t r = esp_random();
#endif
            snprintf(t + 8 * i, 9, "%08lx", (unsigned long)r);
        }
    }
    return t;
}

// WebSocket handshake: token from /api/wstoken in the query (?t=...)
inline bool tokenOk(AsyncWebServerRequest* request) {
    return request->hasParam("t") && request->getParam("t")->value() == token();
}

// Answer of GET /api/wstoken; call it only after the login check
inline void send(AsyncWebServerRequest* request) {
    AsyncWebServerResponse* r = request->beginResponse(200, "application/json", String("{\"t\":\"") + token() + "\"}");
    r->addHeader("Cache-Control", "no-store");
    request->send(r);
}

}  // namespace wsauth

#endif  // WSAUTH_H
