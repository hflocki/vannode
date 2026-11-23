// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Node key of the display in the tailnet ("nodekey:<hex>"), read from microlink's internal state.
// Needed to sign the display by hand in a tailnet with Tailnet Lock (tailscale lock sign nodekey:...).
// Plain C on purpose: microlink_internal.h is a C header and is not meant for C++.
#include "microlink/ml_arduino_config.h"
#include "microlink/microlink_internal.h"
#include <stdio.h>

void vn_ts_nodekey(const microlink_t *ml, char *out, size_t len) {
  if (!out || !len) return;
  out[0] = '\0';
  if (!ml || len < 8 + 64 + 1) return;
  int n = snprintf(out, len, "nodekey:");
  for (int i = 0; i < 32; i++) n += snprintf(out + n, len - n, "%02x", ml->wg_public_key[i]);
}
