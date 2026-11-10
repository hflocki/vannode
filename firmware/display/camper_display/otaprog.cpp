// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#include "otaprog.h"

namespace otaprog {
namespace {
volatile int pct = -1;
}

void set(int p) { pct = p; }
int get() { return pct; }

}  // namespace otaprog
