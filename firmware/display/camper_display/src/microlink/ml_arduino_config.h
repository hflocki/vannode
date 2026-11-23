// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Build settings of microlink for the Arduino build of the display.
// With ESP-IDF these come from microlink's Kconfig menu (sdkconfig.h). The Arduino core ships
// a prebuilt sdkconfig without them, so every microlink C file includes this header first.
// Boolean options follow sdkconfig.h: defined = on, not defined = off (most are tested with #ifdef).
// No credentials here: auth key and device name come from the display settings at run time (tailscale.cpp).
#pragma once

#define CONFIG_ML_MAX_PEERS 16              // simultaneous WireGuard peers (a camper tailnet is small)
#define CONFIG_ML_NVS_MAX_PEERS 64          // peers cached in NVS for a quick start after a reboot
#define CONFIG_ML_DERP_VERIFY_CERT 1        // check the TLS certificate of the DERP relays (core certificate bundle)
#define CONFIG_ML_DERP_REGION 4             // home DERP relay: 4 = Frankfurt (microlink default 9 = Dallas)
#define CONFIG_ML_PRIORITY_PEER_IP ""
#define CONFIG_ML_H2_BUFFER_SIZE_KB 256     // in PSRAM; enough for a tailnet of a few dozen devices
#define CONFIG_ML_JSON_BUFFER_SIZE_KB 256   // in PSRAM
#define CONFIG_ML_PACKET_FILTER_ENFORCE 1   // follow the access rules (ACL) of the tailnet
#define CONFIG_ML_FILTER_MAX_ENTRIES 4096
#define CONFIG_ML_UDP_RX_TASK_PRIO 7
#define CONFIG_ML_STOP_TIMEOUT_MS 30000
#define CONFIG_ML_WIFI_SSID ""              // WiFi is run by the display itself
#define CONFIG_ML_WIFI_PASSWORD ""
#define CONFIG_ML_TAILSCALE_AUTH_KEY ""
#define CONFIG_ML_DEVICE_NAME ""
#define CONFIG_ML_CTRL_HOST "controlplane.tailscale.com"
#define CONFIG_ML_CTRL_NOISE_PUBKEY_HEX ""

// Stack sizes of microlink's tasks in bytes (internal RAM). microlink 2.3.0 uses 8 / 14 / 12 / 8 kB.
// Lowered to the measured use plus about 2 kB (display 2.27.3, tailnet with 12 devices:
// used 2.9 / 4.2 / 6.9 / 3.2 kB). The Tailscale card shows the reserve; keep it above about 1 kB.
#define ML_TASK_NET_IO_STACK   (5 * 1024)
#define ML_TASK_DERP_TX_STACK  (7 * 1024 + 512)
#define ML_TASK_COORD_STACK    (9 * 1024 + 512)
#define ML_TASK_WG_MGR_STACK   (5 * 1024 + 512)

// Left off (not defined): zero-copy receive, subnet router / exit node, IPv6 forwarding (NAT66),
// cellular modem, WiFi/cellular switching, microlink's own web UI, OTA rollback,
// control plane over TLS, streaming map fetch.
