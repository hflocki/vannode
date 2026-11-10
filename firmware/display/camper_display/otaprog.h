// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#pragma once

// Progress of a firmware upload over the web page, for the update screen on the panel.
// Written by the web server task, read by the UI in the main loop.
namespace otaprog {

void set(int pct);   // 0..100, -1 = no update running
int get();           // -1 = none

}  // namespace otaprog
