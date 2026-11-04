#!/usr/bin/env python3
# tag-root lane: decode retail's greensparkle cloud geometry from the golden oracle
# refent dumps (reType=0x2 hModel=0x9 = greensparkle.spr), and fit the velocity cone.
import struct, sys, math, collections

path = "/home/adam/Desktop/PORTS/EF2_Android_WIP/personal/oracle/golden/multimatch-menu-return/20261103-153210/retail.C"

def f(h):
    v = int(h, 16)
    s = 1 if v & 0x80000000 == 0 else -1
    if (v & 0x7F800000) == 0x7F800000:
        return float('nan')
    e = ((v >> 23) & 0xFF) - 127
    if (v & 0x7F800000) == 0:
        e = -126
    m = (v & 0x7FFFFF) / 8388608.0
    return s * (1.0 + m) * (2.0 ** e)

rows = []
with open(path, 'r', errors='replace') as fh:
    for line in fh:
        if not line.startswith('# retail refent'):
            continue
        if 'hModel=0x00000009' not in line:
            continue
        d = {}
        for tok in line.split():
            if '=' in tok:
                k, v = tok.split('=', 1)
                d[k] = v
        org = d.get('origin', '')
        p = org.split(',')
        if len(p) != 3:
            continue
        rows.append((int(d.get('f', -1)), f(p[0]), f(p[1]), f(p[2]),
                     d.get('rgba', ''), d.get('scale', '')))
print("sprite adds=%d frames=%d" % (len(rows), len(set(r[0] for r in rows))))

# cluster the spawn origins: sprites spawn at the tag + a small offset
# use the earliest frame each appears near a tight cluster
xs = sorted(r[1] for r in rows)
ys = sorted(r[2] for r in rows)
zs = sorted(r[3] for r in rows)
n = len(rows)
print("x[%.1f%%]=%.2f med=%.2f" % (0, xs[0], xs[n//2]))
print("median origin = (%.2f, %.2f, %.2f)" % (xs[n//2], ys[n//2], zs[n//2]))
print("range x %.2f..%.2f  y %.2f..%.2f  z %.2f..%.2f" % (xs[0], xs[-1], ys[0], ys[-1], zs[0], zs[-1]))
