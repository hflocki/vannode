#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
# GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
# See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
"""Release checks shared by the CI and tools/release/release.py.

Usage: check.py                 bundle list and build matrix agree, every device has a version
       check.py --tag <tag>     additionally: the tag fits the firmware (CI on tag pushes)

- Every "<device> <target>" in .github/bundle.txt must exist in the build matrix of
  .github/workflows/build.yml (error). Matrix entries missing in bundle.txt are listed as a
  warning, unless bundle.txt names them in a "# skip: <device> <target>" line.
- Every matrix device must have a readable FW_VERSION.
- A device tag "<device>-v<version>" must name a device of the matrix, <version> must equal
  its FW_VERSION and its CHANGELOG.md must have a row for it. "bundle-v..." tags need nothing.
Exit code 1 on any error. Standard library only.
"""
import re
import sys
from pathlib import Path

sys.dont_write_bytecode = True   # no __pycache__ in the repo
sys.path.insert(0, str(Path(__file__).resolve().parent))
from bundle import ROOT, entries, version  # noqa: E402
from notes import notes  # noqa: E402

WORKFLOW = ROOT / ".github" / "workflows" / "build.yml"


def matrix():
    """(device, target) pairs of the build matrix, in workflow order."""
    text = WORKFLOW.read_text(encoding="utf-8")
    return re.findall(r"-\s+device:\s*(\S+)\s*\n\s+target:\s*(\S+)", text)


def check_lists():
    errors, warnings = [], []
    mx = matrix()
    if not mx:
        errors.append("no build matrix found in .github/workflows/build.yml")
    bundle = list(entries())
    skipped = re.findall(r"^#\s*skip:\s*(\S+)\s+(\S+)", (ROOT / ".github" / "bundle.txt").read_text(encoding="utf-8"), re.M)
    for device, target in bundle:
        if (device, target) not in mx:
            errors.append(f".github/bundle.txt: \"{device} {target}\" is not in the build matrix")
    for device, target in mx:
        if device != "template" and (device, target) not in bundle and (device, target) not in skipped:
            warnings.append(f"\"{device} {target}\" is built but not in .github/bundle.txt")
    for device in sorted({d for d, _ in mx}):
        if version(device) == "?":
            errors.append(f"{device}: no FW_VERSION found")
    return errors, warnings


def check_tag(tag):
    if tag.startswith("bundle-v"):
        return []
    m = re.fullmatch(r"(.+)-v(.+)", tag)
    if not m:
        return [f"tag {tag}: expected <device>-v<version> or bundle-v<version>"]
    device, ver = m.groups()
    if device not in {d for d, _ in matrix()}:
        return [f"tag {tag}: \"{device}\" is not a device of the build matrix"]
    errors = []
    fw = version(device)
    if fw != ver:
        errors.append(f"tag {tag}: FW_VERSION of {device} is {fw}, not {ver}")
    if notes(device, ver) is None:
        errors.append(f"tag {tag}: no row \"| {ver} | ... |\" in the CHANGELOG.md of {device}")
    return errors


def main():
    args = sys.argv[1:]
    if args not in ([],) and not (len(args) == 2 and args[0] == "--tag"):
        sys.exit(__doc__)
    errors, warnings = check_lists()
    if args:
        errors += check_tag(args[1])
    for w in warnings:
        print(f"::warning::{w}")
    for e in errors:
        print(f"::error::{e}")
    if errors:
        sys.exit(1)
    print("release checks ok" + (f" for {args[1]}" if args else ""))


if __name__ == "__main__":
    main()
