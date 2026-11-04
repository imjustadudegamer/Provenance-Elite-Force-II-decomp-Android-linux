#!/usr/bin/env python3
# tag-root lane: split retail's greensparkle cloud BY HEIGHT, per the operator's
# retail screenshot reading + the r3 decode, and measure the low band's cone.
import math, collections

path = "/home/adam/Desktop/PORTS/EF2_Android_WIP/personal/oracle/golden/multimatch-menu-return/20261103-153210/retail.C"
TAG = (-864.0, 6080.0, -1463.0)

def f(h):
    v = int(h, 16)
    if v == 0:
        return 0.0
    s = 1 if v & 0x80000000 == 0 else -1
    e = ((v >> 23) & 0xFF) - 127
    if (v & 0x7F800000) == 0:
        e = -126
    return s * (1.0 + (v & 0x7FFFFF) / 8388608.0) * (2.0 ** e)

pts = []
with open(path, 'r', errors='replace') as fh:
    for line in fh:
        if line.startswith('# retail refent') and 'hModel=0x00000009' in line:
            d = dict(t.split('=', 1) for t in line.split() if '=' in t)
            p = d['origin'].split(',')
            pts.append((f(p[0]) - TAG[0], f(p[1]) - TAG[1], f(p[2]) - TAG[2]))

def band(lo, hi, label):
    s = [p for p in pts if lo <= p[2] < hi]
    if not s:
        print("%s: EMPTY" % label); return
    rs = sorted(math.hypot(p[0], p[1]) for p in s)
    els = sorted(math.degrees(math.atan2(p[2], math.hypot(p[0], p[1])))
                 for p in s if math.hypot(p[0], p[1]) > 1e-4)
    n = len(rs); m = len(els)
    print("%s: n=%d  r min=%.1f p10=%.1f med=%.1f p90=%.1f max=%.1f | elev med=%.1f p10=%.1f p90=%.1f"
          % (label, n, rs[0], rs[n//10], rs[n//2], rs[9*n//10], rs[-1],
             els[m//2] if m else -1, els[m//10] if m else -1, els[9*m//10] if m else -1))
    h = collections.Counter(int(e // 10) * 10 for e in els)
    tot = max(h.values()) if h else 1
    line = " ".join("%d:%d" % (k, h[k]) for k in sorted(h))
    print("     el/10: %s" % line)
    ha = collections.Counter(int(math.degrees(math.atan2(p[1], p[0])) // 20) * 20
                             for p in s if math.hypot(p[0], p[1]) > 1e-4)
    tot = max(ha.values()) if ha else 1
    print("     az/20: %s" % " ".join("%d:%d" % (k, ha[k]) for k in sorted(ha)))

print("total adds=%d" % len(pts))
for lo, hi in [(0, 20), (20, 40), (40, 60), (60, 80), (80, 100), (100, 120),
               (120, 140), (140, 160), (160, 200)]:
    band(lo, hi, "dz %3d..%3d" % (lo, hi))
    print()
