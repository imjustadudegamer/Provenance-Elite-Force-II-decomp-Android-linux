#!/usr/bin/env python3
"""Attribute self-time of target symbols to their nearest own-code ancestor frame.

Reads perf script default output (callee->caller order per sample).
A sample block = first line starting with a comm (not indented, ends with ':'),
followed by indented frame lines.
"""
import sys, re, collections

PATH = sys.argv[1] if len(sys.argv) > 1 else "script.txt"
TARGETS = set(sys.argv[2:]) if len(sys.argv) > 2 else set()

OWN = ("provenanceef2", "renderer_vulkan.so", "cgame.so", "game.so", "libopenal")

hdr = re.compile(r'^(\S+)\s+(\d+)\s+([\d.]+):\s+(\d+)\s+(\S+):\s*$')

def clean(fr):
    # "7943c19250bd __ioctl+0x3d (inlined)" -> ("__ioctl", "inlined")
    m = re.match(r'^([0-9a-f]+)\s+(.*)$', fr)
    if not m:
        return None
    rest = m.group(2)
    pm = re.match(r'^(.*?)\s*\((.*)\)$', rest)
    if not pm:
        return None
    sym, dso = pm.group(1), pm.group(2)
    sym = sym.split('+')[0]
    return sym, dso

samples = []
cur_event = None
cur_frames = []
nblocks = 0
with open(PATH, errors='replace') as f:
    for line in f:
        if not line.startswith((' ', '\t')):
            # header line
            if cur_frames:
                samples.append((cur_event, cur_frames))
            m = hdr.match(line.rstrip('\n'))
            if m:
                cur_event = m.group(5)
                cur_frames = []
                nblocks += 1
            else:
                cur_event = None
                cur_frames = []
            continue
        s = line.strip()
        if not s:
            continue
        c = clean(s)
        if c:
            cur_frames.append(c)
if cur_frames:
    samples.append((cur_event, cur_frames))

print("# blocks: %d, with frames: %d" % (nblocks, len(samples)), file=sys.stderr)

# leaf self-time totals per (event, leafsym)
leaf_total = collections.Counter()
# ancestor attribution: leafsym -> ancestor(sym in own dso) -> weight
attr = collections.defaultdict(collections.Counter)
# full own-code chain, joined
chain = collections.defaultdict(collections.Counter)
# thread attribution
tidc = collections.defaultdict(collections.Counter)

def is_own(dso):
    return any(dso == o or dso.endswith('/' + o) or dso.startswith(o) for o in OWN)

for ev, frames in samples:
    if not frames:
        continue
    leafsym, leafdso = frames[0]
    key = (ev or '?', leafsym)
    leaf_total[key] += 1
    # walk up for first own-code frame (skip index 0 = leaf itself)
    anc = None
    ownchain = []
    for sym, dso in frames[1:]:
        if is_own(dso):
            if anc is None:
                anc = sym
            ownchain.append(sym)
            if len(ownchain) >= 4:
                break
    if anc is not None:
        attr[(ev or '?', leafsym)][anc] += 1
        chain[(ev or '?', leafsym)]['<-'.join(ownchain)] += 1

# Print
def evname(e):
    if 'cpu_core' in e: return 'CORE'
    if 'cpu_atom' in e: return 'ATOM'
    return e

for key in sorted(leaf_total, key=lambda k: -leaf_total[k]):
    tot = leaf_total[key]
    print("\n=== LEAF %-55s [%s] samples=%d" % (key[1][:55], evname(key[0]), tot))
    a = attr.get(key)
    if a:
        print("   -- nearest own-code ancestor:")
        for s, n in a.most_common(12):
            print("      %6.1f%%  %s" % (100.0 * n / tot, s))
    c = chain.get(key)
    if c:
        print("   -- own-code chains (top 8):")
        for s, n in c.most_common(8):
            print("      %6.1f%%  %s" % (100.0 * n / tot, s))
