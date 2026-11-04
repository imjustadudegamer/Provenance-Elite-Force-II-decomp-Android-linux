#!/usr/bin/env python3
# tag-root lane: recover retail's greensparkles02 velocity direction per sprite.
# alpha is the age proxy (fadein 1.0 -> alpha = 255*age for age<1.0), so
# v = (origin - spawn) / age recovers the direction the emitter threw it.
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

rec = []
with open(path, 'r', errors='replace') as fh:
    for line in fh:
        if line.startswith('# retail refent') and 'hModel=0x00000009' in line:
            d = dict(t.split('=', 1) for t in line.split() if '=' in t)
            p = d['origin'].split(',')
            a = d.get('rgba', '0,0,0,0').split(',')
            rec.append((f(p[0]) - TAG[0], f(p[1]) - TAG[1], f(p[2]) - TAG[2], int(a[0])))

print("adds=%d alpha range=%d..%d" % (len(rec), min(r[3] for r in rec), max(r[3] for r in rec)))

# speed-magnitude distribution implied by displacement/age, and the elevation of it
els = []
spd = []
for dx, dy, dz, al in rec:
    r = math.hypot(dx, dy)
    if r < 26.0:
        continue                       # greensparkles01 / fresh spawns
    if al <= 4:
        continue                       # age proxy unusable
    age = al / 255.0
    vx, vy, vz = dx / age, dy / age, dz / age
    vh = math.hypot(vx, vy)
    els.append(math.degrees(math.atan2(vz, vh)))
    spd.append(math.sqrt(vx * vx + vy * vy + vz * vz))

els.sort(); spd.sort()
n = len(els)
print("greensparkles02 velocity fits n=%d" % n)
print("  implied speed  p10=%.1f med=%.1f p90=%.1f max=%.1f"
      % (spd[n//10], spd[n//2], spd[9*n//10], spd[-1]))
print("  implied elev   min=%.1f p10=%.1f p25=%.1f med=%.1f p75=%.1f p90=%.1f max=%.1f"
      % (els[0], els[n//10], els[n//4], els[n//2], els[3*n//4], els[9*n//10], els[-1]))
h = collections.Counter(int(e // 5) * 5 for e in els)
mx = max(h.values())
for k in sorted(h):
    print("   el %4d..%4d : %5d %s" % (k, k+5, h[k], '#' * min(56, h[k]*56//mx)))
