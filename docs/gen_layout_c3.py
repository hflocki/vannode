#!/usr/bin/env python3
"""Generate the perfboard layout for the ESP32-C3 Super Mini build (Bestückungs-/Verdrahtungsplan) as SVG + PNG."""
import sys
import cairosvg

out = sys.argv[1]
P = 24                 # pixel per 2.54 mm
X0, Y0 = 280, 150      # origin: hole (0,0)
COLS, ROWS = 32, 22
W, H = 1820, 1060
LIST_X, LIST_H = 1140, 880
s = []
a = s.append

C12, C5, C33, CG, CL, CTX, CRX = '#d62728', '#ff7f0e', '#c2185b', '#111', '#1f5fbf', '#2ca02c', '#17becf'


def hx(x):
    return X0 + x * P


def hy(y):
    return Y0 + y * P


def txt(x, y, t, size=13, anchor='start', col='#111', bold=False):
    b = ' font-weight="bold"' if bold else ''
    a(f'<text x="{x}" y="{y}" font-size="{size}" text-anchor="{anchor}" fill="{col}"{b}>{t}</text>')


def tbg(x, y, t, size=12, anchor='start', col='#111', bold=True):
    w = len(t) * size * 0.62
    x0 = x if anchor == 'start' else (x - w / 2 if anchor == 'middle' else x - w)
    a(f'<rect x="{x0-3}" y="{y-size}" width="{w+6}" height="{size+5}" rx="3" fill="#fff" fill-opacity="0.88"/>')
    txt(x, y, t, size, anchor, col, bold)


def wire(pts, col, w=4, dash=None):
    """Insulated wire along hole coordinates (grid units)."""
    p = ' '.join(f'{hx(x)},{hy(y)}' for x, y in pts)
    d = f' stroke-dasharray="{dash}"' if dash else ''
    a(f'<polyline points="{p}" fill="none" stroke="{col}" stroke-width="{w}" stroke-linejoin="round" '
      f'stroke-linecap="round" opacity="0.9"{d}/>')


def flying(p0, p1, col, bend=(0, 0)):
    """Flying wire to an off-board part, drawn as a curve. Points in pixels."""
    (x0, y0), (x1, y1) = p0, p1
    cx, cy = (x0 + x1) / 2 + bend[0], (y0 + y1) / 2 + bend[1]
    a(f'<path d="M{x0},{y0} Q{cx},{cy} {x1},{y1}" fill="none" stroke="{col}" stroke-width="4" '
      f'stroke-linecap="round" opacity="0.9"/>')


def solder(x, y, col='#111'):
    a(f'<circle cx="{hx(x)}" cy="{hy(y)}" r="6" fill="{col}" stroke="#fff" stroke-width="1.5"/>')


def label_hole(x, y, t, dx=10, dy=-8, col='#111', size=12, anchor='start'):
    tbg(hx(x) + dx, hy(y) + dy, t, size, anchor, col, True)


# ===================== ESP32-C3 Super Mini variant =====================
a(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" '
  'font-family="DejaVu Sans, Arial, sans-serif">')
a(f'<rect width="{W}" height="{H}" fill="#fff"/>')
txt(40, 46, 'Aufbau auf Lochraster mit ESP32-C3 Super Mini (Punktraster 2,54 mm, ca. 8,1 × 5,6 cm)', 24, bold=True)
txt(40, 74, 'Draufsicht Bestückungsseite. Koordinate (Spalte|Reihe). Farbige Linien = isolierte Drahtbrücken, '
    'Punkt = Lötstelle. Kreuzungen ohne Punkt sind nicht verbunden.', 14, col='#555')

# ---------------- board ----------------
a(f'<rect x="{hx(0.3)}" y="{hy(0.3)}" width="{(COLS+0.4)*P}" height="{(ROWS+0.4)*P}" rx="10" '
  'fill="#f3e3b8" stroke="#b08a3c" stroke-width="2"/>')
for x in range(1, COLS + 1):
    for y in range(1, ROWS + 1):
        a(f'<circle cx="{hx(x)}" cy="{hy(y)}" r="5" fill="#d9a441" stroke="#a9772a" stroke-width="1"/>'
          f'<circle cx="{hx(x)}" cy="{hy(y)}" r="2" fill="#fff"/>')
for x in range(1, COLS + 1):
    if x == 1 or x % 5 == 0:
        txt(hx(x), hy(0.3) - 8, str(x), 11, 'middle', '#777')
for y in range(1, ROWS + 1):
    if y == 1 or y % 5 == 0:
        txt(hx(0.3) - 8, hy(y) + 4, str(y), 11, 'end', '#777')

# ---------------- ESP32-C3 Super Mini (USB up) ----------------
LX_, RX_, Y1 = 23, 29, 2
left = ['5V', 'GND', '3V3', 'GPIO4', 'GPIO3', 'GPIO2', 'GPIO1', 'GPIO0']
right = ['GPIO5', 'GPIO6', 'GPIO7', 'GPIO8', 'GPIO9', 'GPIO10', 'GPIO20', 'GPIO21']
a(f'<rect x="{hx(LX_-0.7)}" y="{hy(Y1-1.6)}" width="{(RX_-LX_+1.4)*P}" height="{9.2*P}" rx="8" '
  'fill="#2b5fae" fill-opacity="0.12" stroke="#2b5fae" stroke-width="2" stroke-dasharray="8,5"/>')
a(f'<rect x="{(hx(LX_)+hx(RX_))/2-26}" y="{hy(Y1-2.3)}" width="52" height="30" rx="5" fill="#bbb" stroke="#777"/>')
txt((hx(LX_) + hx(RX_)) / 2, hy(Y1 - 1.5), 'USB-C', 11, 'middle', '#333', True)
tbg((hx(LX_) + hx(RX_)) / 2, hy(Y1 + 8.4), 'ESP32-C3 Super Mini', 13, 'middle', '#2b5fae', True)
tbg((hx(LX_) + hx(RX_)) / 2, hy(Y1 + 9.2), 'auf Buchsenleisten', 11, 'middle', '#2b5fae', False)
for i in range(8):
    for xx in (LX_, RX_):
        a(f'<rect x="{hx(xx)-8}" y="{hy(Y1+i)-8}" width="16" height="16" fill="{"#333" if xx == LX_ else "#999"}" rx="2"/>')
    txt(hx(RX_) + 14, hy(Y1 + i) + 4, right[i], 10, 'start', '#888')
    txt(hx(LX_) + 14, hy(Y1 + i) + 4, left[i], 10, 'start',
        '#000' if left[i] in ('5V', 'GND', '3V3', 'GPIO4', 'GPIO3') else '#888',
        left[i] in ('5V', 'GND', '3V3', 'GPIO4', 'GPIO3'))

# ---------------- GND ----------------
wire([(23, 3), (24, 3), (24, 21)], CG, 5)                # under the module between the headers
wire([(2, 14), (2, 21), (24, 21)], CG, 5)                # GND bus row 21
# ---------------- JST 12 V pass-through ----------------
for (yy, name) in ((13, 'JST IN (Kabelbaum)'), (17, 'JST OUT (CP Plus)')):
    a(f'<rect x="{hx(1)-62}" y="{hy(yy)-14}" width="52" height="{P+28}" rx="4" fill="#ddd" stroke="#888"/>')
    tbg(hx(1) - 36, hy(yy + 1) + 34, name, 12, 'middle', '#333', True)
    txt(hx(1) - 20, hy(yy) + 5, '+', 14, 'middle', C12, True)
    txt(hx(1) - 20, hy(yy + 1) + 5, '−', 14, 'middle', CG, True)
wire([(1, 13), (3, 13), (3, 17), (1, 17)], C12, 4)
wire([(1, 14), (2, 14)], CG, 4)
wire([(1, 18), (2, 18)], CG, 4)
# ---------------- fuse, D1 bridge, 12 V line ----------------
wire([(3, 13), (5, 13)], C12, 4)
wire([(7, 13), (10, 13)], C12, 4, '7,5')
wire([(10, 13), (17, 13)], C12, 5)
fx, fy = hx(6), hy(11.6)
flying((hx(5), hy(13)), (fx - 34, fy), C12, (-10, 0))
flying((hx(7), hy(13)), (fx + 34, fy), C12, (10, 0))
a(f'<rect x="{fx-34}" y="{fy-14}" width="68" height="28" rx="6" fill="#222"/>')
txt(fx, fy + 5, 'F1 1 A', 12, 'middle', '#fff', True)
# D2 (ring at 12|13) -> 12|17 -> GND
a(f'<rect x="{hx(12)-8}" y="{hy(13.6)}" width="16" height="{2.8*P}" rx="4" fill="#222"/>')
a(f'<rect x="{hx(12)-8}" y="{hy(13.6)}" width="16" height="6" fill="#ddd"/>')
wire([(12, 17), (12, 21)], CG, 4)
# C1 (+ 14|14, - 14|15)
wire([(14, 13), (14, 14)], C12, 4)
a(f'<circle cx="{hx(14)}" cy="{(hy(14)+hy(15))/2}" r="17" fill="#2a2a6a" stroke="#000"/>')
txt(hx(14) - 2, hy(14) + 2, '+', 12, 'middle', '#fff', True)
wire([(14, 15), (14, 21)], CG, 4)

# ---------------- ESP side ----------------
wire([(21, 2), (23, 2)], C5, 5)                          # 5V
wire([(19, 4), (23, 4)], C33, 4)                         # 3V3 node
wire([(21, 5), (23, 5)], CTX, 4)                         # TX node -> GPIO4
wire([(21, 6), (23, 6)], CRX, 4)                         # RX node -> GPIO3
a(f'<rect x="{hx(20)-7}" y="{hy(4.4)}" width="14" height="{3.2*P}" rx="4" fill="#c9b48a" stroke="#7a5f2a"/>')
wire([(21, 6), (21, 8), (20, 8)], CRX, 4)                # RX node -> R1 bottom

# ---------------- LIN module ----------------
mx0, my0, mx1, my1 = 4.6, 1.2, 15.4, 9.6
a(f'<rect x="{hx(mx0)}" y="{hy(my0)}" width="{(mx1-mx0)*P}" height="{(my1-my0)*P}" rx="8" '
  'fill="#2e9e55" fill-opacity="0.85" stroke="#1d6b39" stroke-width="2"/>')
txt((hx(mx0) + hx(mx1)) / 2, hy(5.6), 'LIN-Modul T151', 15, 'middle', '#fff', True)
txt((hx(mx0) + hx(mx1)) / 2, hy(6.5), 'Abstandshalter / Heißkleber', 11, 'middle', '#eafff0')
top = {'GND': 6, 'RX': 8.7, 'SLP': 11.3, 'TX': 14}
bot = {'GND': 6, 'LIN': 8.7, 'INH': 11.3, '12V': 14}
for n, x in top.items():
    a(f'<circle cx="{hx(x)}" cy="{hy(my0+0.6)}" r="9" fill="#9be3a9" stroke="#1d6b39" stroke-width="2"/>')
    txt(hx(x), hy(my0 + 1.6), n, 12, 'middle', '#fff', True)
for n, x in bot.items():
    a(f'<circle cx="{hx(x)}" cy="{hy(my1-0.6)}" r="9" fill="#9be3a9" stroke="#1d6b39" stroke-width="2"/>')
    txt(hx(x), hy(my1 - 1.2), n, 12, 'middle', '#fff', True)
tp = lambda n: (hx(top[n]), hy(my0 + 0.6))
bp = lambda n: (hx(bot[n]), hy(my1 - 0.6))
flying(tp('TX'), (hx(21), hy(5)), CTX, (80, -60))
flying(tp('RX'), (hx(21), hy(6)), CRX, (120, -150))
flying(tp('SLP'), (hx(19), hy(4)), C33, (60, -90))
flying(bp('12V'), (hx(17), hy(13)), C12, (40, 0))
flying(bp('GND'), (hx(7), hy(21)), CG, (-80, 60))

# ---------------- RJ12 ----------------
for (yy, name) in ((1.4, 'RJ12 IN (Heizung)'), (9.0, 'RJ12 OUT (CP Plus)')):
    rx, ry = 40, hy(yy)
    a(f'<rect x="{rx}" y="{ry}" width="96" height="120" rx="6" fill="#3a3a3a"/>')
    txt(rx, ry - 8, name, 12, 'start', '#333', True)
    for k in range(6):
        a(f'<circle cx="{rx+100}" cy="{ry+16+k*18}" r="4" fill="#bbb"/>')
        txt(rx + 88, ry + 20 + k * 18, str(k + 1), 11, 'end', '#fff')
for k in range(6):
    y1, y2 = hy(1.4) + 16 + k * 18, hy(9.0) + 16 + k * 18
    col = CL if k == 2 else (CG if k == 4 else '#888')
    flying((144, y1), (144, y2), col, (30 + k * 10, 0))
flying((144, hy(1.4) + 16 + 2 * 18), bp('LIN'), CL, (-40, 140))
flying((144, hy(1.4) + 16 + 4 * 18), (hx(2), hy(16)), CG, (-60, 120))

# ---------------- DC/DC converter ----------------
cx, cy = hx(13), hy(25.0)
a(f'<rect x="{cx-110}" y="{cy-26}" width="220" height="52" rx="6" fill="#111"/>')
txt(cx, cy - 4, 'DC/DC Bauer 8–32 V → 5 V', 13, 'middle', '#fff', True)
txt(cx, cy + 14, '(vergossen, neben der Platine)', 11, 'middle', '#ccc')
flying((cx - 80, cy - 26), (hx(16), hy(13)), C12, (-150, 0))
flying((cx - 40, cy - 26), (hx(16), hy(21)), CG)
flying((cx + 40, cy - 26), (hx(19), hy(21)), CG)
flying((cx + 80, cy - 26), (hx(21), hy(2)), C5, (520, 40))
txt(cx - 80, cy + 44, 'rot', 11, 'middle'); txt(cx - 40, cy + 44, 'schw.', 11, 'middle')
txt(cx + 40, cy + 44, 'schw.', 11, 'middle'); txt(cx + 80, cy + 44, 'gelb', 11, 'middle')

# ---------------- solder points ----------------
for x, y, c in [(1, 13, C12), (1, 14, CG), (1, 17, C12), (1, 18, CG), (3, 13, C12), (5, 13, C12), (7, 13, C12),
                (10, 13, C12), (12, 13, C12), (14, 13, C12), (16, 13, C12), (17, 13, C12),
                (12, 17, CG), (14, 14, C12), (14, 15, CG), (2, 14, CG), (2, 16, CG), (2, 18, CG), (2, 21, CG),
                (7, 21, CG), (12, 21, CG), (14, 21, CG), (16, 21, CG), (19, 21, CG), (24, 21, CG),
                (23, 3, CG), (24, 3, CG), (21, 2, C5), (23, 2, C5), (19, 4, C33), (20, 4, C33), (23, 4, C33),
                (21, 5, CTX), (23, 5, CTX), (21, 6, CRX), (23, 6, CRX), (21, 8, CRX), (20, 8, CRX)]:
    solder(x, y, c)

label_hole(10, 13, '12 V abgesichert', -50, -12, C12, 12)
label_hole(4, 15.6, 'D1: Brücke oder 1N400x (Ring →)', 0, 0, '#555', 11)
label_hole(12, 17, 'D2 P6KE24A, Ring oben', 12, 10, '#333', 11)
label_hole(14, 15, 'C1 22 µF/35 V, + oben', 22, 4, '#333', 11)
label_hole(20, 6, 'R1 10 kΩ', -14, 4, '#333', 11, 'end')
label_hole(8, 22, 'Masse-Bus (Reihe 21)', 0, 5, CG, 12)
label_hole(24, 14, 'GND unter dem Modul', 10, 4, CG, 11)

# ---------------- connection list ----------------
LX, LY = LIST_X, 120
a(f'<rect x="{LX-20}" y="{LY-30}" width="660" height="{LIST_H}" rx="10" fill="#fafafa" stroke="#ddd"/>')
txt(LX, LY, 'Verdrahtungsliste', 18, bold=True)
rows = [
    ('Versorgung 12 V', None),
    ('1', 'JST IN + (1|13) → JST OUT + (1|17) über (3|13)–(3|17)'),
    ('2', 'JST IN − (1|14) und OUT − (1|18) → Masse (2|14)…(2|21)'),
    ('3', '(3|13) → (5|13) → Sicherungshalter F1 1 A → (7|13)'),
    ('4', '(7|13) → (10|13): Drahtbrücke (oder 1N400x, Ring rechts)'),
    ('5', '12-V-Leitung (10|13) … (17|13)'),
    ('6', 'D2 P6KE24A: Ring an (12|13), anderes Ende (12|17) → Masse'),
    ('7', 'C1 22 µF: + (14|14) an 12 V, − (14|15) → Masse'),
    ('8', 'Wandler rot → (16|13), schwarz (Eingang) → (16|21)'),
    ('9', 'Wandler gelb → (21|2) → (23|2) = 5V, schwarz → (19|21)'),
    ('Masse', None),
    ('10', 'Masse-Bus Reihe 21: (2|21) … (24|21)'),
    ('11', 'ESP GND (23|3) → (24|3) → (24|21), unter dem Modul'),
    ('LIN-Modul', None),
    ('12', '12V-Klemme → (17|13)    GND unten → (7|21)'),
    ('13', 'LIN-Klemme → RJ12 IN Kontakt 3 (vorher messen)'),
    ('14', 'TX → (21|5) → (23|5) = GPIO4'),
    ('15', 'RX → (21|6) → (23|6) = GPIO3'),
    ('16', 'SLP → (19|4) → (23|4) = 3V3'),
    ('17', 'INH frei, GND oben frei (intern = GND unten, messen)'),
    ('Pull-up', None),
    ('18', 'R1 10 kΩ von (20|4) nach (20|8); (20|8)–(21|8)–(21|6)'),
    ('RJ12', None),
    ('19', 'RJ12 IN ↔ RJ12 OUT: alle 6 Kontakte 1:1 durch'),
    ('20', 'RJ12 Kontakt 5 (GND, messen) → Masse (2|16)'),
    ('Reihenfolge', None),
    ('a', 'Buchsenleisten auf den C3 stecken, so aufsetzen, löten'),
    ('b', 'Masse, 12-V-Leitung, D2, C1, R1, Brücken'),
    ('c', 'JST-Buchsen, Sicherungshalter, Wandler-Kabel'),
    ('d', 'LIN-Modul befestigen und verdrahten, RJ12 zuletzt'),
    ('Vor dem ersten Anschluss', None),
    ('✓', 'Kein Durchgang 12 V ↔ Masse, 5 V ↔ Masse, 3V3 ↔ Masse'),
    ('✓', 'Modul: Widerstand 12V↔LIN ≥ 30 kΩ (kein Master-R)'),
    ('✓', '12 V ohne C3: 5 V an (23|2), RX am Modul ≤ 3,3 V'),
    ('✓', 'Polarität am CP-Plus-Stecker messen, erst dann einstecken'),
]
y = LY + 34
for k, t in rows:
    if t is None:
        y += 6
        txt(LX, y, k, 14, bold=True, col='#2b5fae')
        y += 21
        continue
    txt(LX + 4, y, k, 13, bold=True)
    txt(LX + 36, y, t, 13)
    y += 22

a('</svg>')
svg = '\n'.join(s)
open(out + '.svg', 'w').write(svg)
cairosvg.svg2png(bytestring=svg.encode(), write_to=out + '.png')
