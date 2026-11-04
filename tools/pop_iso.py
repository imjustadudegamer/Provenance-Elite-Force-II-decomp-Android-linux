#!/usr/bin/env python3
# tag-root lane: isolate retail's greensparkles02 by radius (greensparkles01 is
# radius<=24.7 across every height; anything wider is 02) and fit its cone.
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

wide = [p for p in pts if math.hypot(p[0], p[1]) > 26.0]
print("greensparkles02 (r>26): n=%d" % len(wide))
rs = sorted(math.hypot(p[0], p[1]) for p in wide)
dz = sorted(p[2] for p in wide)
els = sorted(math.degrees(math.atan2(p[2], math.hypot(p[0], p[1]))) for p in wide)
n = len(els)
print("  r   min=%.1f p10=%.1f med=%.1f p90=%.1f max=%.1f" % (rs[0], rs[n//10], rs[n//2], rs[9*n//10], rs[-1]))
print("  dz  min=%.1f med=%.1f max=%.1f" % (dz[0], dz[n//2], dz[-1]))
print("  el  min=%.1f p10=%.1f p25=%.1f med=%.1f p75=%.1f p90=%.1f max=%.1f"
      % (els[0], els[n//10], els[n//4], els[n//2], els[3*n//4], els[9*n//10], els[-1]))
h = collections.Counter(int(e // 5) * 5 for e in els)
mx = max(h.values())
for k in sorted(h):
    print("   el %4d..%4d : %5d %s" % (k, k+5, h[k], '#' * min(56, h[k]*56//mx)))
ha = collections.Counter(int(math.degrees(math.atan2(p[1], p[0])) // 15) * 15 for p in wide)
mx = max(ha.values())
print("  azimuth:")
for k in sorted(ha):
    print("   az %5d..%5d : %5d %s" % (k, k+15, ha[k], '#' * min(56, ha[k]*56//mx)))
# dz vs r: is the cone a straight line (constant elevation)?
print("  dz/r ratio: ", end="")
rat = sorted((p[2] / math.hypot(p[0], p[1])) for p in wide)
print("p10=%.2f med=%.2f p90=%.2f  (44.9deg = %.3f)" % (rat[n//10], rat[n//2], rat[9*n//10],
                                                         math.tan(math.radians(44.9))))
