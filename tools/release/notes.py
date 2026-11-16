#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
# GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
# See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
"""Print the release notes of one firmware version from firmware/<device>/CHANGELOG.md.

Usage: notes.py <device> [<version>]     version defaults to FW_VERSION of the device

The changelog is a Markdown table "| Version | Änderungen |"; the row of the version
becomes the text of the GitHub release. Without a row the script exits with code 1,
so the release step fails and no release without changelog text is published. Standard library only.
"""
import re
import sys
from pathlib import Path

sys.dont_write_bytecode = True   # no __pycache__ in the repo
sys.path.insert(0, str(Path(__file__).resolve().parent))
from bundle import FOLDER, ROOT, version  # noqa: E402


def notes(device, ver):
    log = ROOT / "firmware" / FOLDER.get(device, device) / "CHANGELOG.md"
    if log.exists():
        for line in log.read_text(encoding="utf-8").splitlines():
            m = re.match(r"^\|\s*" + re.escape(ver) + r"\s*\|(.*)\|\s*$", line)
            if m:
                return m.group(1).strip().replace("\\|", "|")
    return None


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    device = sys.argv[1]
    ver = sys.argv[2] if len(sys.argv) == 3 else version(device)
    text = notes(device, ver)
    if text is None:
        # No release without its changelog row: add "| <version> | ... |" to CHANGELOG.md
        print(f"::error::No entry for {device} {ver} in firmware/{FOLDER.get(device, device)}/CHANGELOG.md",
              file=sys.stderr)
        sys.exit(1)
    print(f"**{device}-node {ver}**\n\n{text}\n\n"
          f"Installieren: `.bin` über die Web-Oberfläche des Geräts hochladen (Road Buddy: Update-Seite); "
          f"wo eine `.html.gz` dabei ist, diese danach ebenfalls. Alle Versionen: "
          f"[CHANGELOG.md](https://github.com/hflocki/vannode/blob/main/firmware/{FOLDER.get(device, device)}/CHANGELOG.md)")


if __name__ == "__main__":
    main()
