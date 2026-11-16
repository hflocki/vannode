#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
# GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
# See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
"""Check everything before a release of one device and print the git commands for it.

Usage: python3 tools/release/release.py <device>      e.g. ble, display, pocket, template

Runs the checks of the CI (shared sources, packed web pages, Arduino docs, bundle list),
checks the tag "<device>-v<FW_VERSION>" (version, CHANGELOG row, tag not used yet) and the
git state (nothing uncommitted, no config_local.h in git). Nothing is changed, tagged or
pushed: at the end the script prints the commands to run yourself. Standard library only.
"""
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True   # no __pycache__ in the repo
sys.path.insert(0, str(Path(__file__).resolve().parent))
from bundle import ROOT, version  # noqa: E402
from check import check_tag, matrix  # noqa: E402

CHECKS = [
    ["tools/common/sync_common.py", "--check"],
    ["tools/web/pack_web.py", "--check"],
    ["tools/docs/check_arduino.py"],
    ["firmware/heater/tools/embed_web.py", "--check"],
    ["tools/release/check.py"],
]


def git(*args):
    r = subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True)
    return r.returncode, r.stdout.strip()


def main():
    if len(sys.argv) != 2 or sys.argv[1].startswith("-"):
        sys.exit(__doc__)
    device = sys.argv[1]
    devices = sorted({d for d, _ in matrix()})
    if device not in devices:
        sys.exit(f"Unbekanntes Gerät „{device}“. Möglich: {', '.join(devices)}")
    ver = version(device)
    tag = f"{device}-v{ver}"
    errors = []

    print(f"Release {tag}\n")
    for cmd in CHECKS:
        r = subprocess.run([sys.executable, "-B", *cmd], cwd=ROOT, capture_output=True, text=True)
        ok = r.returncode == 0
        print(f"  {'ok    ' if ok else 'FEHLER'} {' '.join(cmd)}")
        if not ok:
            errors.append(f"{cmd[0]}:\n{(r.stdout + r.stderr).strip()}")

    for e in check_tag(tag):
        errors.append(e)
    print(f"  {'ok    ' if not check_tag(tag) else 'FEHLER'} Version {ver} und CHANGELOG-Zeile")

    _, tracked = git("ls-files")
    leaked = [f for f in tracked.splitlines() if f.endswith("config_local.h")]
    if leaked:
        errors.append("config_local.h ist im Git – Zugangsdaten! Mit `git rm --cached <datei>` entfernen: "
                      + ", ".join(leaked))
    _, status = git("status", "--porcelain")
    if status:
        errors.append("Nicht committete Änderungen – erst committen (oder verwerfen):\n" + status)
    print(f"  {'ok    ' if not leaked and not status else 'FEHLER'} Git-Stand (alles committet, keine config_local.h)")

    _, local = git("tag", "-l", tag)
    rc, remote = git("ls-remote", "--tags", "origin", f"refs/tags/{tag}")
    if local or remote:
        errors.append(f"Tag {tag} gibt es schon – FW_VERSION erhöhen und neue CHANGELOG-Zeile schreiben")
    if rc != 0:
        print("  ?      GitHub nicht erreichbar, Tag nur lokal geprüft")
    print(f"  {'ok    ' if not (local or remote) else 'FEHLER'} Tag {tag} ist noch frei")

    _, ahead = git("rev-list", "--count", "@{upstream}..HEAD")
    if errors:
        print("\nNicht bereit:\n")
        for e in errors:
            print(f"- {e}\n")
        sys.exit(1)

    print("\nAlles bereit. Jetzt selbst ausführen:\n")
    if ahead not in ("", "0"):
        print("git push")
    print(f"git tag {tag} && git push origin {tag}")


if __name__ == "__main__":
    main()
