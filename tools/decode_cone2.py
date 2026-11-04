#!/usr/bin/env python3
# tag-root lane: elevation/azimuth histogram of retail's greensparkle cloud,
# measured from the tag anchor at the generator (m1l2c shield generator).
import math, collections

path = "/home/adam/Desktop/PORTS/EF2_Android_WIP/personal/oracle/golden/multimatch-menu-return/20261103-153210/retail.C"
TAG = (-864.0, 6080.0, -1463.0)   # host org + (0,0,76) along host z axis

def f(h):
    v = int(h, 16)
    s = 1 if v & 0x80000000 == 0 else -1
    if (v & 0x7F800000) == 0x7F800000:
        return float('nan')
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

print("adds=%d" % len(pts))
zs = [p[2] for p in pts]
print("dz range %.2f .. %.2f  (below-base count=%d)" % (min(zs), max(zs), sum(1 for z in zs if z < -0.5)))
print("flat (|dz|<=0.5) count=%d" % sum(1 for z in zs if abs(z) <= 0.5))

els = []
azs = []
for dx, dy, dz in pts:
    r = math.hypot(dx, dy)
    if r < 1e-3 and abs(dz) < 1e-3:
        continue
    els.append(math.degrees(math.atan2(dz, r)))
    if r > 1e-3:
        azs.append(math.degrees(math.atan2(dy, dx)))

els.sort()
n = len(els)
print("elevation: min=%.1f p05=%.1f p25=%.1f med=%.1f p75=%.1f p95=%.1f max=%.1f  n=%d"
      % (els[0], els[n//20], els[n//4], els[n//2], els[3*n//4], els[19*n//20], els[-1], n))
h = collections.Counter(int(e // 10) * 10 for e in els)
for k in sorted(h):
    print("  el %4d..%4d : %6d %s" % (k, k+10, h[k], '#' * min(60, h[k] * 60 // max(h.values()))))
azs.sort()
m = len(azs)
print("azimuth: min=%.1f p05=%.1f med=%.1f p95=%.1f max=%.1f n=%d"
      % (azs[0], azs[m//20], azs[m//2], azs[19*m//20], azs[-1], m))
ha = collections.Counter(int(a // 30) * 30 for a in azs)
for k in sorted(ha):
    print("  az %5d..%5d : %6d %s" % (k, k+30, ha[k], '#' * min(60, ha[k] * 60 // max(ha.values()))))
