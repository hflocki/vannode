#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
# GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
# See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
"""Check that docs/ARDUINO.md names the same board packages and library versions as the CI.

Usage: check_arduino.py

Reads every "https://github.com/<owner>/<repo>.git#<tag>" and the ESP32 / ESP8266 core
versions from .github/workflows/build.yml. docs/ARDUINO.md must have a table row with
`<owner>/<repo>` and the version (tag without a leading "v"), and must name both core
versions. Exit code 1 on any difference. Standard library only.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github" / "workflows" / "build.yml"
DOC = ROOT / "docs" / "ARDUINO.md"


def main():
    wf = WORKFLOW.read_text(encoding="utf-8")
    doc = DOC.read_text(encoding="utf-8")
    rows = [line for line in doc.splitlines() if line.startswith("|")]
    errors = []

    libs = sorted(set(re.findall(r"https://github\.com/([\w.-]+/[\w.-]+?)(?:\.git)?#v?([\w.]+)", wf)))
    for repo, ver in libs:
        hits = [r for r in rows if f"`{repo}`" in r]
        if not hits:
            errors.append(f"{repo}: missing in docs/ARDUINO.md (CI uses {ver})")
        elif not any(re.search(r"\|\s*v?" + re.escape(ver) + r"\s*\|", r) for r in hits):
            errors.append(f"{repo}: docs/ARDUINO.md has another version than the CI ({ver})")

    for name, var in (("esp32", "ESP32_CORE"), ("esp8266", "ESP8266_CORE")):
        m = re.search(var + r":\s*\S+@([\d.]+)", wf)
        if m and f"CI: {m.group(1)}" not in doc:
            errors.append(f"board package {name}: docs/ARDUINO.md should say \"CI: {m.group(1)}\"")

    for e in errors:
        print("docs/ARDUINO.md:", e)
    if errors:
        sys.exit(1)
    print(f"docs/ARDUINO.md matches build.yml ({len(libs)} libraries)")


if __name__ == "__main__":
    main()
