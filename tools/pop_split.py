#!/usr/bin/env python3
# tag-root lane: split retail's greensparkle cloud into its two emitter populations
# and measure each one's velocity cone against the two candidate mechanisms.
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
            pts.append((f(p[0]) - TAG[0], f(p[1]) - TAG[1], f(p[2]) - TAG[2],
                        d.get('rgba', ''), int(d.get('f', 0))))

def stat(sel, label):
    s = [p for p in pts if sel(p)]
    if not s:
        print("%s: EMPTY" % label)
        return
    rs = [math.hypot(p[0], p[1]) for p in s]
    dzs = [p[2] for p in s]
    els = [math.degrees(math.atan2(dz, r)) for r, dz in zip(rs, dzs) if r > 1e-4 or dz > 1e-4]
    els.sort()
    n = len(els)
    rs.sort(); dzs.sort()
    print("%s: n=%d  r max=%.1f  dz min=%.3f max=%.1f  |dz|<=0.5 count=%d"
          % (label, len(s), rs[-1], dzs[0], dzs[-1], sum(1 for z in dzs if abs(z) <= 0.5)))
    if n:
        print("   elevation: p05=%.1f p25=%.1f med=%.1f p75=%.1f p95=%.1f"
              % (els[n//20], els[n//4], els[n//2], els[3*n//4], els[19*n//20]))
    h = collections.Counter(int(e // 15) * 15 for e in els)
    for k in sorted(h):
        print("     el %4d..%4d : %6d %s" % (k, k+15, h[k], '#' * min(50, h[k]*50//max(h.values()))))

print("total adds=%d" % len(pts))
stat(lambda p: abs(p[2]) <= 0.02, "EXACT-FLAT (|dz|<=0.02)")
print()
stat(lambda p: 0.02 < abs(p[2]) <= 0.6, "NEAR-FLAT")
print()
stat(lambda p: abs(p[2]) > 0.6, "RAISING")
print()
# the flat population's horizontal radius histogram
s = [p for p in pts if abs(p[2]) <= 0.02]
rs = sorted(math.hypot(p[0], p[1]) for p in s)
if rs:
    print("EXACT-FLAT r: min=%.2f med=%.2f max=%.2f" % (rs[0], rs[len(rs)//2], rs[-1]))
    h = collections.Counter(int(r // 10) * 10 for r in rs)
    for k in sorted(h):
        print("     r %4d..%4d : %6d %s" % (k, k+10, h[k], '#' * min(50, h[k]*50//max(h.values()))))
