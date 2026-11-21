#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
# GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
# See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
"""Collect the firmware files listed in .github/bundle.txt into one folder for the bundle release.

Usage: bundle.py <artifact dir> <output dir> [<name>]
       bundle.py --version <device>      prints FW_VERSION of the device (used by the build)

For every listed device it copies <device>-node-<target>-v<version>.bin from the build artifacts,
adds the packed web page (data/index.html.gz) as <device>-node-web.html.gz where the
device has one, and writes NOTES.md with the firmware versions. With <name> (the bundle tag,
e.g. bundle-v2026.11.21) it also packs everything into vannode-<version>.zip, together with
LICENSE and NOTICE.md. Standard library only.
"""
import re
import shutil
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LIST = ROOT / ".github" / "bundle.txt"
FOLDER = {"template": "_template"}   # matrix device name -> folder under firmware/


def entries():
    for line in LIST.read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            device, target = line.split()
            yield device, target


def version(device):
    base = ROOT / "firmware" / FOLDER.get(device, device)
    for name in ("config.h", "fw_id.h"):
        for f in sorted(base.glob(f"*/{name}")):
            m = re.search(r'#define\s+FW_VERSION\s+"([^"]+)"', f.read_text(encoding="utf-8"))
            if m:
                return m.group(1)
    return "?"


def main():
    if len(sys.argv) == 3 and sys.argv[1] == "--version":
        print(version(sys.argv[2]))
        return
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    src, out = Path(sys.argv[1]), Path(sys.argv[2])
    tag = sys.argv[3] if len(sys.argv) == 4 else ""
    out.mkdir(parents=True, exist_ok=True)
    rows, missing = [], []
    for device, target in entries():
        name = f"{device}-node-{target}-v{version(device)}.bin"
        found = next(src.rglob(name), None)
        if not found:
            missing.append(name)
            continue
        shutil.copy2(found, out / name)
        web = sorted((ROOT / "firmware" / FOLDER.get(device, device)).glob("*/data/index.html.gz"))
        web_name = ""
        if web:
            web_name = f"{device}-node-web.html.gz"
            shutil.copy2(web[0], out / web_name)
        rows.append(f"| {device} | {version(device)} | `{name}` | {f'`{web_name}`' if web_name else '–'} |")
    if missing:
        sys.exit("missing build artifacts: " + ", ".join(missing))
    notes = [
        "All firmware files in one place, built from the same commit.",
        "",
        "| Device | Version | Firmware | Web page |",
        "| --- | --- | --- | --- |",
        *rows,
        "",
        "Install the `.bin` through the device's web page (Pocket-Node: its update page). "
        "Where a web page is listed, upload the `.html.gz` afterwards as well.",
    ]
    (out / "NOTES.md").write_text("\n".join(notes) + "\n", encoding="utf-8")
    print("\n".join(rows))
    if tag:
        make_zip(out, tag)


def make_zip(out, tag):
    """All files of the bundle in one zip; the folder inside carries the same name."""
    ver = tag.split("-v", 1)[1] if "-v" in tag else tag
    base = f"vannode-{ver}"
    zpath = out / f"{base}.zip"
    files = sorted(p for p in out.iterdir() if p.suffix in (".bin", ".gz") or p.name == "NOTES.md")
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        for p in files:
            z.write(p, f"{base}/{p.name}")
        # The license travels with the binaries (GPL); the source is the tagged commit
        for extra in ("LICENSE", "NOTICE.md"):
            z.write(ROOT / extra, f"{base}/{extra}")
    print(f"zip: {zpath.name} ({len(files) + 2} files)")


if __name__ == "__main__":
    main()
