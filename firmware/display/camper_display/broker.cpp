// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
#include "broker.h"
#include "evlog.h"
#include <AsyncTCP.h>
#include <esp_random.h>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace broker {
namespace {

// ---- PSRAM strings: retained messages and queues would fill the internal heap ----------
template <class T>
struct PsAlloc {
  using value_type = T;
  PsAlloc() = default;
  template <class U> PsAlloc(const PsAlloc<U>&) {}
  T* allocate(size_t n) {
    void* p = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = malloc(n * sizeof(T));
    if (!p) abort();
    return (T*)p;
  }
  void deallocate(T* p, size_t) { heap_caps_free(p); }
  template <class U> bool operator==(const PsAlloc<U>&) const { return true; }
  template <class U> bool operator!=(const PsAlloc<U>&) const { return false; }
};
using PStr = std::basic_string<char, std::char_traits<char>, PsAlloc<char>>;

const size_t MAX_CLIENTS = 16;
const size_t MAX_PACKET = 64 * 1024;         // Zigbee2MQTT bridge/devices can be large
const size_t MAX_QUEUE = 160 * 1024;         // per client; a client that does not read is dropped
const size_t MAX_RETAINED = 2000;
const size_t MAX_RETAINED_BYTES = 1024 * 1024;
const uint32_t CONNECT_TIMEOUT_MS = 10000;   // time for the CONNECT packet

struct Client {
  AsyncClient* tcp = nullptr;  // nullptr once the connection is gone
  AsyncClient* sock = nullptr; // the object itself, deleted with the client (see trash)
  bool gone = false;
  PStr rx;
  std::deque<PStr, PsAlloc<PStr>> out;
  size_t outOff = 0, outBytes = 0;
  bool connected = false;      // CONNECT accepted
  bool cleanClose = false;     // DISCONNECT received: no will
  bool dead = false;           // to be closed (by reap(), never in the middle of routing)
  String id;
  uint16_t keepAlive = 0;
  uint32_t lastRx = 0, since = 0;
  std::vector<PStr, PsAlloc<PStr>> subs;
  bool hasWill = false, willRetain = false;
  PStr willTopic, willMsg;
};

AsyncServer* server = nullptr;
std::vector<Client*> clients;
// Gone clients are deleted later from another client's callback: AsyncTCP still touches the
// AsyncClient after the callback in which it was closed (and may call it again for the rest
// of the received data).
std::vector<Client*> trash;
std::map<PStr, PStr, std::less<PStr>, PsAlloc<std::pair<const PStr, PStr>>> retained;
size_t retainedBytes = 0;
String bUser, bPass;
uint16_t bPort = 0;
uint32_t msgCount = 0;

// Closing runs the disconnect callback right away (which deletes the client), so clients are only
// marked here and closed by reap() at the end of each AsyncTCP callback.
void kill(Client* c) { c->dead = true; }

// ---- helpers --------------------------------------------------------------------------
void putLen(PStr& s, size_t n) {   // remaining length
  do {
    uint8_t b = n % 128;
    n /= 128;
    if (n) b |= 0x80;
    s.push_back((char)b);
  } while (n);
}

void putStr(PStr& s, const char* p, size_t n) {
  s.push_back((char)(n >> 8));
  s.push_back((char)(n & 0xFF));
  s.append(p, n);
}

PStr frame(uint8_t head, const PStr& body) {
  PStr f;
  f.reserve(body.size() + 5);
  f.push_back((char)head);
  putLen(f, body.size());
  f += body;
  return f;
}

PStr publishFrame(const PStr& topic, const PStr& payload, bool retain) {
  PStr body;
  body.reserve(topic.size() + payload.size() + 2);
  putStr(body, topic.data(), topic.size());
  body += payload;
  return frame(retain ? 0x31 : 0x30, body);
}

// MQTT topic filter: "+" one level, "#" the rest; "$..." topics only match explicit filters
bool matches(const PStr& filter, const PStr& topic) {
  if (!topic.empty() && topic[0] == '$' && !filter.empty() && (filter[0] == '+' || filter[0] == '#')) return false;
  size_t f = 0, t = 0;
  while (true) {
    size_t fe = filter.find('/', f), te = topic.find('/', t);
    PStr fl = filter.substr(f, fe == PStr::npos ? PStr::npos : fe - f);
    if (fl == "#") return true;
    PStr tl = topic.substr(t, te == PStr::npos ? PStr::npos : te - t);
    if (fl != "+" && fl != tl) return false;
    if (fe == PStr::npos && te == PStr::npos) return true;
    if (fe == PStr::npos) return false;                 // topic has more levels
    if (te == PStr::npos) {                             // filter has more levels: only "x/#" matches "x"
      return filter.compare(fe + 1, PStr::npos, "#") == 0;
    }
    f = fe + 1;
    t = te + 1;
  }
}

void flush(Client* c) {
  AsyncClient* tcp = c->tcp;
  if (!tcp || !tcp->connected()) return;
  bool any = false;
  while (!c->out.empty() && tcp->space() > 0) {
    PStr& f = c->out.front();
    size_t n = min(tcp->space(), f.size() - c->outOff);
    size_t added = tcp->add(f.data() + c->outOff, n);
    if (!added) break;
    any = true;
    c->outOff += added;
    if (c->outOff >= f.size()) {
      c->outBytes -= f.size();
      c->out.pop_front();
      c->outOff = 0;
    }
  }
  if (any) tcp->send();
}

void enqueue(Client* c, const PStr& f) {
  if (!c->tcp) return;
  if (c->dead && c->connected) return;   // a refused login still gets its CONNACK
  if (c->outBytes + f.size() > MAX_QUEUE) {
    Serial.printf("[broker] %s does not read, dropped\n", c->id.c_str());
    kill(c);
    return;
  }
  c->out.push_back(f);
  c->outBytes += f.size();
  flush(c);
}

void route(const PStr& topic, const PStr& payload, bool retain) {
  msgCount++;
  if (retain) {
    auto it = retained.find(topic);
    if (it != retained.end()) { retainedBytes -= it->first.size() + it->second.size(); retained.erase(it); }
    if (!payload.empty()) {
      if (retained.size() < MAX_RETAINED && retainedBytes + topic.size() + payload.size() <= MAX_RETAINED_BYTES) {
        retained.emplace(topic, payload);
        retainedBytes += topic.size() + payload.size();
      } else {
        static uint32_t warned = 0;
        if (!warned || millis() - warned > 600000) { warned = millis(); evlog::add("Broker: Speicher für gespeicherte Nachrichten voll"); }
      }
    }
  }
  PStr f;   // built once, only when someone wants it
  for (Client* c : clients) {
    if (!c->connected || c->dead) continue;
    for (auto& s : c->subs) {
      if (!matches(s, topic)) continue;
      if (f.empty()) f = publishFrame(topic, payload, false);
      enqueue(c, f);
      break;   // once per client
    }
  }
}

void dropClient(Client* c) {
  for (size_t i = 0; i < clients.size(); i++)
    if (clients[i] == c) { clients.erase(clients.begin() + i); break; }
  c->gone = true;
  if (c->connected && c->hasWill && !c->cleanClose) route(c->willTopic, c->willMsg, c->willRetain);
  trash.push_back(c);
}

void emptyTrash(Client* self) {
  for (size_t i = 0; i < trash.size();) {
    if (trash[i] == self) { i++; continue; }
    delete trash[i]->sock;
    delete trash[i];
    trash.erase(trash.begin() + i);
  }
}

void reap() {
  for (int guard = 0; guard < 4; guard++) {
    std::vector<Client*> d;
    for (Client* c : clients) if (c->dead && c->tcp) d.push_back(c);
    if (d.empty()) return;
    for (Client* c : d) if (c->tcp) c->tcp->close();   // -> onDisconnect -> dropClient (may kill others)
  }
}

// ---- packets ----------------------------------------------------------------------------
struct Reader {
  const uint8_t* p;
  size_t n, pos = 0;
  bool ok = true;
  Reader(const uint8_t* d, size_t l) : p(d), n(l) {}
  uint8_t u8() { if (pos + 1 > n) { ok = false; return 0; } return p[pos++]; }
  uint16_t u16() { uint16_t hi = u8(); return (hi << 8) | u8(); }
  PStr str() {
    uint16_t l = u16();
    if (!ok || pos + l > n) { ok = false; return PStr(); }
    PStr s((const char*)p + pos, l);
    pos += l;
    return s;
  }
  PStr rest() { PStr s((const char*)p + pos, n - pos); pos = n; return s; }
};

void ack(Client* c, uint8_t head, uint16_t id) {
  PStr b;
  b.push_back((char)(id >> 8));
  b.push_back((char)(id & 0xFF));
  enqueue(c, frame(head, b));
}

void onConnect(Client* c, Reader& r) {
  PStr proto = r.str();
  uint8_t level = r.u8(), flags = r.u8();
  c->keepAlive = r.u16();
  PStr id = r.str();
  PStr wTopic, wMsg, user, pass;
  if (flags & 0x04) { wTopic = r.str(); wMsg = r.str(); }
  if (flags & 0x80) user = r.str();
  if (flags & 0x40) pass = r.str();
  if (!r.ok) { kill(c); return; }

  uint8_t rc = 0;
  if (!((proto == "MQTT" && level == 4) || (proto == "MQIsdp" && level == 3))) rc = 1;
  else if (bUser.length() && (user != bUser.c_str() || pass != bPass.c_str())) rc = 5;
  PStr b;
  b.push_back(0);                // no stored session
  b.push_back((char)rc);
  enqueue(c, frame(0x20, b));
  if (rc) {
    Serial.printf("[broker] login refused (%u)\n", rc);
    kill(c);
    return;
  }
  if (id.empty()) id = ("auto-" + String(esp_random(), HEX)).c_str();
  // same id again (reconnect after a lost connection): the old one goes without its will
  for (Client* o : clients)
    if (o != c && o->connected && o->id == id.c_str()) { o->cleanClose = true; kill(o); }
  c->id = id.c_str();
  c->hasWill = flags & 0x04;
  c->willRetain = flags & 0x20;
  c->willTopic = wTopic;
  c->willMsg = wMsg;
  c->connected = true;
}

void onPublish(Client* c, uint8_t head, Reader& r) {
  uint8_t qos = (head >> 1) & 3;
  bool retain = head & 1;
  PStr topic = r.str();
  uint16_t id = qos ? r.u16() : 0;
  if (!r.ok || topic.empty() || topic.find_first_of("+#") != PStr::npos) { kill(c); return; }
  PStr payload = r.rest();
  if (qos == 1) ack(c, 0x40, id);         // PUBACK
  else if (qos == 2) ack(c, 0x50, id);    // PUBREC (delivered right away, PUBREL is answered below)
  route(topic, payload, retain);
}

void onSubscribe(Client* c, Reader& r) {
  uint16_t id = r.u16();
  PStr codes;
  std::vector<PStr, PsAlloc<PStr>> added;
  while (r.ok && r.pos < r.n) {
    PStr f = r.str();
    r.u8();   // requested QoS, everything goes out with QoS 0
    if (!r.ok) break;
    bool bad = f.empty();
    size_t h = f.find('#');
    if (h != PStr::npos && (h != f.size() - 1 || (h > 0 && f[h - 1] != '/'))) bad = true;
    codes.push_back(bad ? (char)0x80 : 0);
    if (bad) continue;
    bool have = false;
    for (auto& s : c->subs) if (s == f) have = true;
    if (!have) c->subs.push_back(f);
    added.push_back(f);
  }
  PStr b;
  b.push_back((char)(id >> 8));
  b.push_back((char)(id & 0xFF));
  b += codes;
  enqueue(c, frame(0x90, b));
  // retained messages for the new filters (each topic once)
  for (auto& kv : retained) {
    for (auto& f : added) {
      if (!matches(f, kv.first)) continue;
      enqueue(c, publishFrame(kv.first, kv.second, true));
      break;
    }
    if (c->dead) return;   // dropped (queue full)
  }
}

void onUnsubscribe(Client* c, Reader& r) {
  uint16_t id = r.u16();
  while (r.ok && r.pos < r.n) {
    PStr f = r.str();
    for (size_t i = 0; i < c->subs.size(); i++)
      if (c->subs[i] == f) { c->subs.erase(c->subs.begin() + i); break; }
  }
  ack(c, 0xB0, id);
}

void handle(Client* c, uint8_t head, const uint8_t* body, size_t len) {
  Reader r(body, len);
  uint8_t type = head >> 4;
  if (!c->connected && type != 1) { kill(c); return; }
  switch (type) {
    case 1:  if (c->connected) kill(c); else onConnect(c, r); break;
    case 3:  onPublish(c, head, r); break;
    case 6:  ack(c, 0x70, r.u16()); break;          // PUBREL -> PUBCOMP
    case 4: case 5: case 7: break;                   // acks for QoS we never send
    case 8:  onSubscribe(c, r); break;
    case 10: onUnsubscribe(c, r); break;
    case 12: enqueue(c, frame(0xD0, PStr())); break; // PINGREQ -> PINGRESP
    case 14: c->cleanClose = true; kill(c); break;
    default: kill(c); break;
  }
}

void onData(Client* c, const uint8_t* data, size_t len) {
  if (c->dead) return;
  c->lastRx = millis();
  c->rx.append((const char*)data, len);
  while (c->rx.size() >= 2) {
    size_t n = 0, mul = 1, i = 1;
    bool done = false;
    while (i < c->rx.size() && i <= 4) {
      uint8_t b = (uint8_t)c->rx[i++];
      n += (b & 0x7F) * mul;
      mul *= 128;
      if (!(b & 0x80)) { done = true; break; }
    }
    if (!done) {
      if (i > 4) { kill(c); return; }   // malformed length
      return;                                        // wait for more
    }
    if (n > MAX_PACKET) {
      Serial.printf("[broker] packet of %u bytes too big\n", (unsigned)n);
      kill(c);
      return;
    }
    if (c->rx.size() < i + n) return;
    uint8_t head = (uint8_t)c->rx[0];
    PStr body = c->rx.substr(i, n);
    c->rx.erase(0, i + n);
    handle(c, head, (const uint8_t*)body.data(), body.size());
    if (c->dead) return;
  }
}

void onPoll(Client* c) {
  if (c->dead) return;
  uint32_t now = millis();
  if (!c->connected && now - c->since > CONNECT_TIMEOUT_MS) { kill(c); return; }
  if (c->connected && c->keepAlive && now - c->lastRx > c->keepAlive * 1500UL) {
    Serial.printf("[broker] %s timed out\n", c->id.c_str());
    kill(c);
    return;
  }
  flush(c);
}

void onClient(void*, AsyncClient* tcp) {
  emptyTrash(nullptr);
  if (clients.size() >= MAX_CLIENTS) {
    Serial.println("[broker] too many clients");
    tcp->close();
    delete tcp;
    return;
  }
  Client* c = new Client();
  c->tcp = c->sock = tcp;
  c->since = c->lastRx = millis();
  clients.push_back(c);
  tcp->setNoDelay(true);
  tcp->onData([c](void*, AsyncClient*, void* d, size_t l) {
    if (c->gone) return;
    emptyTrash(c);
    onData(c, (const uint8_t*)d, l);
    reap();
  });
  tcp->onAck([c](void*, AsyncClient*, size_t, uint32_t) { if (!c->gone) flush(c); });
  tcp->onPoll([c](void*, AsyncClient*) {
    if (c->gone) return;
    emptyTrash(c);
    onPoll(c);
    reap();
  });
  tcp->onError([](void*, AsyncClient*, int8_t) {});
  tcp->onDisconnect([c](void*, AsyncClient*) {
    if (c->gone) return;
    c->tcp = nullptr;
    dropClient(c);
  });
}

}  // namespace

void begin(uint16_t port, const String& user, const String& pass) {
  if (server) return;
  bUser = user;
  bPass = pass;
  bPort = port;
  server = new AsyncServer(port);
  server->onClient(onClient, nullptr);
  server->setNoDelay(true);
  server->begin();
  Serial.printf("[broker] listening on port %u%s\n", port, user.length() ? " (login)" : "");
  evlog::add("Eigener Broker an (Port %u)", port);
}

bool running() { return server != nullptr; }

void json(JsonObject o) {
  o["on"] = running();
  if (!running()) return;
  size_t n = 0;
  for (Client* c : clients) if (c->connected) n++;
  o["port"] = bPort;
  o["clients"] = n;
  o["retained"] = retained.size();
  o["kb"] = (uint32_t)(retainedBytes / 1024);
  o["msgs"] = msgCount;
}

String summary() {
  if (!running()) return "";
  size_t n = 0;
  for (Client* c : clients) if (c->connected) n++;
  return String(n) + " verbunden, " + String(retained.size()) + " gespeichert";
}

}  // namespace broker
