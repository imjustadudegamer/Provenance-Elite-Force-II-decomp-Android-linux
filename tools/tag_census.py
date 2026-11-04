#!/usr/bin/env python3
# tag-root lane: map retail oracle model handles -> names via every register-model
# shaped row, then decode the tag axis per tiki.
import sys, math, collections, re

path = "/home/adam/Desktop/PORTS/EF2_Android_WIP/personal/oracle/golden/multimatch-menu-return/20261103-153210/retail.C"

def f(h):
    v = int(h, 16)
    if v == 0:
        return 0.0
    s = 1 if v & 0x80000000 == 0 else -1
    e = ((v >> 23) & 0xFF) - 127
    if (v & 0x7F800000) == 0:
        e = -126
    return s * (1.0 + (v & 0x7FFFFF) / 8388608.0) * (2.0 ** e)

names = {}
tagrows = []
pat = re.compile(r'^\S+\t\S+\t\S+\t\S+\t\S+\t(\S+)\t(\S+)\t(\S+)\t(.*)$')
with open(path, 'r', errors='replace') as fh:
    for line in fh:
        if line.startswith('# retail tag '):
            tagrows.append(dict(t.split('=', 1) for t in line.split() if '=' in t))
            continue
        if 'models/' not in line:
            continue
        toks = line.rstrip('\n').split('\t')
        nm = None
        for t in toks:
            i = t.find('<str>"')
            if i >= 0:
                j = t.find('"', i + 6)
                if j > i:
                    nm = t[i + 6:j]
        if not nm:
            continue
        h = None
        for t in reversed(toks):
            if re.fullmatch(r'0x[0-9a-fA-F]+', t):
                h = int(t, 16)
                break
        if h is not None and 0 < h < 8192:
            names.setdefault(h, nm)

print("handles mapped=%d" % len(names))
cnt = collections.Counter(d.get('tiki') for d in tagrows)
for k, v in cnt.most_common(20):
    print("  tiki=%-6s rows=%-6d name=%s" % (k, v, names.get(int(k), '?')))
print()
tn = collections.Counter((d.get('tiki'), d.get('tagnum')) for d in tagrows)
for k, v in tn.most_common(20):
    print("  tiki=%s tagnum=%s rows=%d name=%s" % (k[0], k[1], v, names.get(int(k[0]), '?')))
print()
print("--- every tiki mentioned anywhere in the oracle, with handle ---")
for h in sorted(names):
    print("  0x%04x %-60s" % (h, names[h]))
