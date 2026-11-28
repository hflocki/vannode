// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Truma iNet X panel over BLE, see inetx.h.
//
// GATT (service fc310002-f3b2-11e8-8eb2-f2801f1b9fd1, encrypted link required):
//   CMD    fc314001  transport control, write with response + notifications
//   DATA_W fc314002  outgoing messages, write without response
//   DATA_R fc314003  incoming messages, notifications
//
// Transport, one message at a time:
//   out:  CMD <- 01 len_lo len_hi, CMD -> 81 (ready), DATA_W <- message, CMD -> F0 01 (taken; F0 00 = refused)
//   in:   CMD -> 83 len_lo len_hi, CMD <- 03 00, DATA_R -> message in ATT-sized parts, CMD <- F0 01
//
// Session: register (answer = our bus address), subscribe to the topics, send an identity,
// ask every device for its parameters, then keep the link alive and send commands.
#include "inetx.h"
#include "config.h"
#include "dbglog.h"
#include "evlog.h"
#include <mutex>
#include <time.h>

// Library NimBLE-Arduino 2.x (h2zero) is needed for every build of this node
#include <NimBLEDevice.h>

#ifndef INETX_PRIVACY
#define INETX_PRIVACY 1        // own resolvable address: lets the controller resolve the panel's rotating address
#endif

static const char* SVC_UUID    = "fc310002-f3b2-11e8-8eb2-f2801f1b9fd1";
static const char* CMD_UUID    = "fc314001-f3b2-11e8-8eb2-f2801f1b9fd1";
static const char* DATA_W_UUID = "fc314002-f3b2-11e8-8eb2-f2801f1b9fd1";
static const char* DATA_R_UUID = "fc314003-f3b2-11e8-8eb2-f2801f1b9fd1";

// Topics we read (the panel relays only subscribed topics)
static const char* const TOPICS[] = {"AirHeating", "EnergySrc", "WaterHeating", "RoomClimate", "ErrorReset",
                                     "Identify", "Temperature", "System", "MobileIdentity", "PowerMgmt"};

static const unsigned long PAIR_WINDOW_MS   = 180000;   // bonding allowed after the web button
static const unsigned long KEEPALIVE_MS     = 45000;    // the panel only sends changes; ask something small
static const unsigned long STALL_MS         = 150000;   // nothing at all received: link is dead
static const unsigned long REGISTER_MS      = 20000;
static const unsigned long TRANSPORT_MS     = 2500;

// ---- shared with the other tasks ----
static std::mutex mtx;
static InetxModel model;              // values, written by the task
static bool modelValid = false;
static volatile bool changed = false;
static InetxCommand pending;
static volatile bool cmdPending = false;
static volatile bool ready = false;
static volatile bool pairReq = false, forgetReq = false;
static volatile unsigned long pairUntil = 0;
static char stateText[64] = "Startet";
static char peer[20] = "";
static volatile int rssi = 0;
static uint32_t rxFrames = 0, txFrames = 0, txRefused = 0, connects = 0, cmdsSent = 0;   // written by the task only
static volatile unsigned long lastRx = 0, lastConnect = 0;
static volatile uint8_t bonds = 0;
static bool begun = false;

static void setState(const char* fmt, ...) {
    char buf[sizeof(stateText)];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    {
        std::lock_guard<std::mutex> g(mtx);
        if (!strcmp(buf, stateText)) return;
        strlcpy(stateText, buf, sizeof(stateText));
    }
    dbglogf("[INETX] %s", buf);
}

// ---- events from the BLE host task ----
struct Ev {
    uint8_t ch;          // 0 = CMD, 1 = DATA_R
    uint16_t len;
    uint8_t d[248];
};
static QueueHandle_t evq = nullptr;
static uint32_t evDropped = 0;   // written by the BLE host task only

static NimBLEClient* client = nullptr;
static NimBLERemoteCharacteristic* chCmd = nullptr;
static NimBLERemoteCharacteristic* chW = nullptr;
static NimBLERemoteCharacteristic* chR = nullptr;

// Transport state (task only)
static bool gotReady = false, gotAck = false;
static uint8_t ackVal = 0;
static uint8_t rxBuf[4096];
static size_t rxLen = 0, rxWant = 0;
static bool rxAnnounced = false, rxOverflow = false;
static uint16_t ourAddr = INETX_APP;
static uint8_t frameBuf[INETX_MAX_FRAME];
static uint16_t seen[16];
static uint8_t seenCount = 0;

static void onNotify(uint8_t ch, const uint8_t* d, size_t n) {
    Ev e;
    e.ch = ch;
    e.len = (uint16_t)(n < sizeof(e.d) ? n : sizeof(e.d));
    memcpy(e.d, d, e.len);
    if (xQueueSend(evq, &e, 0) != pdTRUE) evDropped++;
}

static void hexDump(const char* what, const uint8_t* d, size_t n) {
    if (!config.debug) return;
    char hex[3 * 48 + 8];
    size_t p = 0, k = 0;
    for (; k < n && k < 48; k++) p += snprintf(hex + p, sizeof(hex) - p, "%02X ", d[k]);
    if (k < n) snprintf(hex + p, sizeof(hex) - p, "... (%u)", (unsigned)n);
    else if (p) hex[p - 1] = 0;
    dbglogf("[INETX] %s %s", what, hex);
}

static bool cmdWrite(const uint8_t* d, size_t n) {
    if (!client || !client->isConnected() || !chCmd) return false;
    return chCmd->writeValue(d, n, true);
}

static void noteSender(uint16_t a) {
    if (a == INETX_BROADCAST || a == INETX_BROKER || a == ourAddr) return;
    for (uint8_t k = 0; k < seenCount; k++) if (seen[k] == a) return;
    if (seenCount < sizeof(seen) / sizeof(seen[0])) seen[seenCount++] = a;
}

static void takeFrame(const uint8_t* d, size_t n) {
    rxFrames++;
    lastRx = millis();
    hexDump("RX", d, n);
    uint16_t src = 0;
    int ch;
    {
        std::lock_guard<std::mutex> g(mtx);
        ch = inetxModelFeed(model, d, n, &src);
        if (model.assigned) ourAddr = model.assigned;
        if (ch) modelValid = true;
    }
    noteSender(src);
    if (ch) changed = true;
}

// Handles one event; transport replies set the flags send() waits for
static void handleEvent(const Ev& e) {
    if (e.ch == 0) {
        if (e.len >= 3 && e.d[0] == 0x83) {               // a message is announced
            rxWant = e.d[1] | (e.d[2] << 8);
            rxLen = 0;
            rxAnnounced = true;
            rxOverflow = rxWant > sizeof(rxBuf);
            const uint8_t ok[2] = {0x03, 0x00};
            cmdWrite(ok, 2);
        } else if (e.len >= 1 && e.d[0] == 0x81) {
            gotReady = true;
        } else if (e.len >= 2 && e.d[0] == 0xF0) {
            gotAck = true;
            ackVal = e.d[1];
        }
        return;
    }
    // DATA_R
    if (!rxAnnounced) {                                   // no announcement: take it as one message
        const uint8_t ack[2] = {0xF0, 0x01};
        cmdWrite(ack, 2);
        takeFrame(e.d, e.len);
        return;
    }
    if (!rxOverflow) memcpy(rxBuf + rxLen, e.d, (rxLen + e.len <= sizeof(rxBuf)) ? e.len : 0);
    rxLen += e.len;
    if (rxLen < rxWant) return;
    rxAnnounced = false;
    const uint8_t ack[2] = {0xF0, 0x01};
    cmdWrite(ack, 2);
    if (rxOverflow || rxLen != rxWant) {
        dbglogf("[INETX] Incoming message dropped (%u of %u bytes%s)", (unsigned)rxLen, (unsigned)rxWant,
                rxOverflow ? ", too large" : "");
        return;
    }
    takeFrame(rxBuf, rxLen);
}

// Process events for up to ms (or until *flag gets true)
static void pump(unsigned long ms, const bool* flag = nullptr) {
    unsigned long t0 = millis();
    for (;;) {
        if (flag && *flag) return;
        unsigned long el = millis() - t0;
        if (el >= ms) return;
        unsigned long left = ms - el;
        Ev e;
        if (xQueueReceive(evq, &e, pdMS_TO_TICKS(left < 50 ? left : 50)) == pdTRUE) handleEvent(e);
        if (!client || !client->isConnected()) return;
    }
}

// One message out. probe = silence is an expected answer (address without a device)
static bool sendFrame(const uint8_t* f, size_t n, bool probe = false) {
    if (n == 0 || !client || !client->isConnected()) return false;
    hexDump("TX", f, n);
    gotReady = false;
    const uint8_t init[3] = {0x01, (uint8_t)(n & 0xFF), (uint8_t)(n >> 8)};
    if (!cmdWrite(init, 3)) return false;
    pump(TRANSPORT_MS, &gotReady);
    if (!gotReady) {
        if (!probe) dbglogf("[INETX] No ready for a %u byte message", (unsigned)n);
        return false;
    }
    gotAck = false;
    size_t chunk = client->getMTU() > 3 ? client->getMTU() - 3 : 20;
    for (size_t off = 0; off < n; off += chunk) {
        size_t k = (n - off < chunk) ? n - off : chunk;
        if (!chW->writeValue(f + off, k, false)) return false;
    }
    pump(TRANSPORT_MS, &gotAck);
    if (!gotAck) {
        if (!probe) dbglogf("[INETX] No acknowledgement for a %u byte message", (unsigned)n);
        return false;
    }
    txFrames++;
    if (ackVal != 0x01) {
        txRefused++;
        dbglogf("[INETX] Message refused by the panel (F0 %02X)", ackVal);
        return false;
    }
    return true;
}

static void dropLink() {
    ready = false;
    if (client) {
        if (client->isConnected()) client->disconnect();
        NimBLEDevice::deleteClient(client);
        client = nullptr;
    }
    chCmd = chW = chR = nullptr;
    xQueueReset(evq);
    rxAnnounced = false;
}

// Stable identity from the chip id (no storage needed): the panel lists us under this name
static void identity(char* muid, char* uuid) {
    uint64_t mac = ESP.getEfuseMac();
    uint32_t a = (uint32_t)(mac >> 16), b = (uint32_t)mac;
    snprintf(muid, 40, "%08lX-%04X-4%03X-A%03X-%012llX", (unsigned long)(a ^ 0x5A17E000UL), (unsigned)(b & 0xFFFF),
             (unsigned)((a >> 4) & 0xFFF), (unsigned)((b >> 8) & 0xFFF), (unsigned long long)(mac & 0xFFFFFFFFFFFFULL));
    snprintf(uuid, 40, "%08lx-%04x-4%03x-b%03x-%012llx", (unsigned long)(b ^ 0x1D3A0000UL), (unsigned)(a & 0xFFFF),
             (unsigned)((b >> 4) & 0xFFF), (unsigned)((a >> 8) & 0xFFF), (unsigned long long)((mac * 2654435761ULL) & 0xFFFFFFFFFFFFULL));
}

// Register, subscribe, identity, parameter discovery. false = session unusable.
static bool startSession() {
    {
        std::lock_guard<std::mutex> g(mtx);
        model.assigned = 0;
    }
    ourAddr = INETX_APP;
    seenCount = 0;
    setState("Anmelden am Panel");
    if (!sendFrame(frameBuf, inetxRegisterFrame(frameBuf, sizeof(frameBuf), INETX_APP))) {
        setState("Panel nimmt keine Nachricht an");
        return false;
    }
    unsigned long t0 = millis();
    uint16_t assigned = 0;
    while (millis() - t0 < REGISTER_MS && client && client->isConnected()) {
        pump(200);
        std::lock_guard<std::mutex> g(mtx);
        assigned = model.assigned;
        if (assigned) break;
    }
    if (!assigned) {
        setState("Panel vergibt keine Adresse");
        return false;
    }
    ourAddr = assigned;
    dbglogf("[INETX] Registered, our address 0x%04X", ourAddr);

    sendFrame(frameBuf, inetxSubscribeFrame(frameBuf, sizeof(frameBuf), ourAddr, TOPICS, sizeof(TOPICS) / sizeof(TOPICS[0])));
    pump(1500);

    // Identity: the panel's clock only with a real time (no NTP = leave the clock alone)
    time_t now = time(nullptr);
    if (now > 1700000000) {
        sendFrame(frameBuf, inetxWriteIntFrame(frameBuf, sizeof(frameBuf), ourAddr, INETX_PANEL, "SystemTime", "Time", (int64_t)now));
        pump(300);
        sendFrame(frameBuf, inetxWriteIntFrame(frameBuf, sizeof(frameBuf), ourAddr, INETX_PANEL, "SystemTime", "Lot", 0));
        pump(300);
    }
    char muid[40], uuid[40];
    identity(muid, uuid);
    sendFrame(frameBuf, inetxWriteTextFrame(frameBuf, sizeof(frameBuf), ourAddr, INETX_PANEL, "MobileIdentity", "UserName", "VanNode Alde"));
    pump(300);
    sendFrame(frameBuf, inetxWriteTextFrame(frameBuf, sizeof(frameBuf), ourAddr, INETX_PANEL, "MobileIdentity", "Muid", muid));
    pump(300);
    sendFrame(frameBuf, inetxWriteTextFrame(frameBuf, sizeof(frameBuf), ourAddr, INETX_PANEL, "MobileIdentity", "Uuid", uuid));
    pump(300);
    sendFrame(frameBuf, inetxLastMessageFrame(frameBuf, sizeof(frameBuf), ourAddr));
    pump(500);

    // Ask every device for its parameters: panel, Truma heater, power / climate class (Alde 0x0404)
    setState("Werte abfragen");
    sendFrame(frameBuf, inetxDiscoverFrame(frameBuf, sizeof(frameBuf), ourAddr, INETX_BROADCAST), true);
    pump(150);
    uint16_t targets[24];
    uint8_t nt = 0;
    targets[nt++] = INETX_PANEL;
    targets[nt++] = 0x0201;
    for (uint16_t i = 1; i <= 8; i++) targets[nt++] = 0x0400 | i;
    for (uint8_t k = 0; k < seenCount && nt < 24; k++) {
        bool dup = false;
        for (uint8_t j = 0; j < nt; j++) if (targets[j] == seen[k]) dup = true;
        if (!dup) targets[nt++] = seen[k];
    }
    uint8_t acked = 0;
    for (uint8_t k = 0; k < nt && client && client->isConnected(); k++) {
        if (sendFrame(frameBuf, inetxDiscoverFrame(frameBuf, sizeof(frameBuf), ourAddr, targets[k]), true)) acked++;
        pump(150);
    }
    pump(3000);
    if (acked == 0) {
        setState("Kein Gerät am Panel antwortet");
        return false;
    }
    dbglogf("[INETX] Discovery: %u of %u addresses acknowledged", acked, nt);
    return true;
}

// Connect to the panel (scan, connect, encrypt, find the characteristics). false = try later.
static bool connectPanel(bool mayPair) {
    setState("Suche Panel");
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(80);
    NimBLEScanResults res = scan->getResults(6000, false);
    const NimBLEAdvertisedDevice* best = nullptr;
    for (int i = 0; i < res.getCount(); i++) {
        const NimBLEAdvertisedDevice* d = res.getDevice(i);
        if (!d->isAdvertisingService(NimBLEUUID(SVC_UUID))) continue;
        if (!best || d->getRSSI() > best->getRSSI()) best = d;
    }
    if (!best) {
        scan->clearResults();
        setState("Kein iNet-X-Panel in Reichweite");
        return false;
    }
    strlcpy(peer, best->getAddress().toString().c_str(), sizeof(peer));
    rssi = best->getRSSI();
    setState("Verbinde mit %s", peer);
    client = NimBLEDevice::createClient();
    client->setConnectTimeout(10000);
    lastConnect = millis();
    bool ok = client->connect(best, true, false, true);
    scan->clearResults();
    if (!ok) {
        setState("Verbindung fehlgeschlagen");
        dropLink();
        return false;
    }
    connects++;
    setState(mayPair ? "Koppeln (Panel im Kopplungsmodus?)" : "Verschlüsseln");
    if (!client->secureConnection()) {
        setState(mayPair ? "Kopplung abgelehnt – Panel im Kopplungsmodus?" : "Verschlüsselung abgelehnt – neu koppeln?");
        dropLink();
        return false;
    }
    bonds = (uint8_t)NimBLEDevice::getNumBonds();
    if (mayPair) {
        evlog::add("iNet X: gekoppelt");
        pairUntil = 0;
    }
    NimBLERemoteService* svc = client->getService(NimBLEUUID(SVC_UUID));
    if (svc) {
        chCmd = svc->getCharacteristic(NimBLEUUID(CMD_UUID));
        chW = svc->getCharacteristic(NimBLEUUID(DATA_W_UUID));
        chR = svc->getCharacteristic(NimBLEUUID(DATA_R_UUID));
    }
    if (!chCmd || !chW || !chR) {
        setState("Panel-Dienst nicht gefunden");
        dropLink();
        return false;
    }
    bool s1 = chCmd->subscribe(chCmd->canNotify(), [](NimBLERemoteCharacteristic*, uint8_t* d, size_t n, bool) { onNotify(0, d, n); });
    bool s2 = chR->subscribe(chR->canNotify(), [](NimBLERemoteCharacteristic*, uint8_t* d, size_t n, bool) { onNotify(1, d, n); });
    if (!s1 || !s2) {
        setState("Benachrichtigungen abgelehnt");
        dropLink();
        return false;
    }
    dbglogf("[INETX] Connected to %s, MTU %u, RSSI %d", peer, client->getMTU(), rssi);
    return true;
}

static void sendCommands() {
    if (!cmdPending) return;
    InetxCommand c;
    InetxModel m;
    {
        std::lock_guard<std::mutex> g(mtx);
        c = pending;
        inetxCommandClear(pending);
        cmdPending = false;
        m = model;
    }
    InetxWrite w[6];
    int n = inetxCommandWrites(m, c, w, 6);
    if (n == 0) {
        evlog::add("Befehl: Wert schon eingestellt");
        return;
    }
    for (int k = 0; k < n; k++) {
        bool ok = sendFrame(frameBuf, inetxWriteIntFrame(frameBuf, sizeof(frameBuf), ourAddr, w[k].dst, w[k].topic, w[k].param, w[k].value));
        if (ok) cmdsSent++;
        evlog::add("iNet X: %s.%s = %ld an %04X %s", w[k].topic, w[k].param, (long)w[k].value, w[k].dst, ok ? "gesendet" : "nicht angenommen");
        pump(200);
    }
}

static void task(void*) {
    unsigned long backoff = 2000;
    for (;;) {
        if (forgetReq) {
            forgetReq = false;
            dropLink();
            NimBLEDevice::deleteAllBonds();
            bonds = 0;
            evlog::add("iNet X: Kopplung gelöscht");
        }
        if (pairReq) {
            pairReq = false;
            pairUntil = millis() + PAIR_WINDOW_MS;
            backoff = 0;
        }
        bonds = (uint8_t)NimBLEDevice::getNumBonds();
        bool mayPair = pairUntil && (long)(pairUntil - millis()) > 0;
        if (!mayPair) pairUntil = 0;
        if (bonds == 0 && !mayPair) {
            setState("Nicht gekoppelt – Panel in den Kopplungsmodus, dann „Koppeln“");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        if (!connectPanel(bonds == 0 && mayPair)) {
            unsigned long wait = mayPair ? 2000 : backoff;
            for (unsigned long t = 0; t < wait && !pairReq && !forgetReq; t += 250) vTaskDelay(pdMS_TO_TICKS(250));
            backoff = backoff < 60000 ? (backoff ? backoff * 2 : 2000) : 60000;
            continue;
        }
        if (!startSession()) {
            dropLink();
            vTaskDelay(pdMS_TO_TICKS(backoff));
            backoff = backoff < 60000 ? backoff * 2 : 60000;
            continue;
        }
        backoff = 2000;
        ready = true;
        changed = true;
        setState("Verbunden");
        evlog::add("iNet X verbunden");
        unsigned long lastAlive = millis();
        uint8_t aliveMiss = 0;
        while (client && client->isConnected() && !forgetReq) {
            pump(200);
            sendCommands();
            if (millis() - lastAlive >= KEEPALIVE_MS) {
                lastAlive = millis();
                bool ok = sendFrame(frameBuf, inetxDiscoverFrame(frameBuf, sizeof(frameBuf), ourAddr, INETX_BLE_MGMT), true);
                aliveMiss = ok ? 0 : aliveMiss + 1;
                rssi = client->getRssi();
                if (aliveMiss >= 3) { dbglogf("[INETX] Keep-alive unanswered 3 times"); break; }
            }
            if (millis() - lastRx > STALL_MS) { dbglogf("[INETX] Nothing received for %lu s", STALL_MS / 1000); break; }
        }
        ready = false;
        changed = true;
        dropLink();
        evlog::add("iNet X getrennt");
        setState("Getrennt – verbinde neu");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void inetxBegin() {
    if (begun) return;
    begun = true;
    inetxModelClear(model);
    inetxCommandClear(pending);
    evq = xQueueCreate(24, sizeof(Ev));
    NimBLEDevice::init("");
#if INETX_PRIVACY
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RPA_PUBLIC_DEFAULT);
#endif
    NimBLEDevice::setMTU(247);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);   // "just works", as the panel bonds
    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
    NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
    bonds = (uint8_t)NimBLEDevice::getNumBonds();
    dbglogf("[INETX] BLE started, %u bond(s) stored", bonds);
    xTaskCreatePinnedToCore(task, "inetx", 8192, nullptr, 1, nullptr, 1);
}


bool inetxReady() {
    return ready;
}

bool inetxSnapshot(InetxModel& out) {
    std::lock_guard<std::mutex> g(mtx);
    out = model;
    return modelValid;
}

bool inetxTakeChanged() {
    if (!changed) return false;
    changed = false;
    return true;
}

const char* inetxControl(const InetxCommand& c) {
    if (!begun) return "iNet X ist nicht aktiv";
    if (!ready) return "Keine Verbindung zum iNet-X-Panel";
    std::lock_guard<std::mutex> g(mtx);
    if (c.hasTarget) { pending.hasTarget = true; pending.target = c.target; }
    if (c.gas >= 0) pending.gas = c.gas;
    if (c.el_level >= 0) pending.el_level = c.el_level;
    if (c.water >= 0) pending.water = c.water;
    if (c.power >= 0) pending.power = c.power;
    cmdPending = true;
    return nullptr;
}

void inetxPairArm() {
    pairReq = true;
    evlog::add("iNet X: Koppeln für 3 Minuten erlaubt");
}

void inetxForget() {
    forgetReq = true;
}

void inetxStatusJson(JsonObject o) {
    unsigned long now = millis();
    std::lock_guard<std::mutex> g(mtx);
    o["state"] = (const char*)stateText;
    o["ready"] = (bool)ready;
    o["bonds"] = bonds;
    o["pairing"] = pairUntil ? (long)((long)(pairUntil - now) / 1000) : 0;
    o["peer"] = (const char*)peer;
    o["rssi"] = (int)rssi;
    o["rx"] = rxFrames;
    o["tx"] = txFrames;
    o["refused"] = txRefused;
    o["commands"] = cmdsSent;
    o["connects"] = connects;
    o["last_rx"] = lastRx ? (long)((now - lastRx) / 1000) : -1;
    char a[8];
    snprintf(a, sizeof(a), "%04X", model.assigned);
    o["addr"] = a;
    snprintf(a, sizeof(a), "%04X", model.heaterAddr);
    o["heater"] = a;
    o["supplier"] = (const char*)model.supplier;
    o["dropped"] = evDropped;
}
