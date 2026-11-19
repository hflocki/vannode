# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
# GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
# See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

import sys, re
from lin_transport import ESPHOME
# Converts a LIN analyzer log, an ESPHome truma_inetbox verbose log or a heater-node log with the
# sniffer in "all frames" mode ("SNIFF PID 07: 01 00 FF ... t=12345") into replay events.
# Slave-response slots (0x3D, 0x18) become "S pid <real answer or ->", everything else "M pid data".
# Sniffer logs in the default mode only hold changed frames - they decode, but are no complete session.
SNIFF = re.compile(r'SNIFF PID ([0-9A-F]{2}):((?: [0-9A-F]{2})+)')
def parse(path):
    for line in open(path, errors='replace'):
        e = ESPHOME.search(line)
        if e:
            d = e.group(5).split('.')
            if len(d) == 9: yield int(e.group(4), 16), d[:8]
            continue
        s = SNIFF.search(line)
        if s:
            yield int(s.group(1), 16) & 0x3F, s.group(2).split()
            continue
        m = re.match(r'\s*([\d,\.]+)\s+([0-9A-F]{2})\s+((?:[0-9A-F]{2} ){1,8}[0-9A-F]{2}|[0-9A-F]{2})?\s+(\d{4})\s*(.*)$', line.strip())
        if not m: continue
        t, pid, data, baud, err = m.groups()
        yield int(pid,16) & 0x3F, (data.split() if data and not err.strip() else None)
out = []
for pid, d in parse(sys.argv[1]):
    if pid in (0x3D, 0x18):
        out.append(f"S {pid:02X} " + (' '.join(d) if d else '-'))
    elif d and len(d) == 8:
        out.append(f"M {pid:02X} " + ' '.join(d))
open(sys.argv[2],'w').write('\n'.join(out)+'\n')
print(len(out), 'events')
