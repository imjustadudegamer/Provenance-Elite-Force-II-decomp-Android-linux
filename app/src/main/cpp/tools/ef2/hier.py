#!/usr/bin/env python3
"""Extract the class hierarchy from the EF2 SDK (real retail source) and from the
port, and report every class whose PARENT differs.  The archive stream is emitted
by walking the base chain, so a re-parented class changes the bytes."""
import os
import re, glob, os

SDK  = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/docs/evidence/ef2-sdk/ef2-sdk/dlls/game"
PORT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/app/src/main/cpp/code/fgame"

def strip_comments(s):
    s = re.sub(r'//[^\n]*', '', s)          # LINE comments FIRST
    return re.sub(r'/\*.*?\*/', '', s, flags=re.S)

CLS = re.compile(r'\bclass\s+(?:\w+_EXPORT\s+)?([A-Za-z_]\w*)\s*:\s*public\s+([A-Za-z_][\w:]*)')
def hierarchy(root):
    h = {}
    for p in glob.glob(root+"/*.h")+glob.glob(root+"/*.cpp"):
        src = strip_comments(open(p, encoding='latin1').read())
        for m in CLS.finditer(src):
            child, parent = m.group(1), m.group(2).split("::")[-1]
            h.setdefault(child, (parent, os.path.basename(p)))
    return h

sdk, port = hierarchy(SDK), hierarchy(PORT)
print("SDK classes: %d   PORT classes: %d" % (len(sdk), len(port)))
common = sorted(set(sdk) & set(port))
diff = [(c, sdk[c][0], port[c][0]) for c in common if sdk[c][0] != port[c][0]]
print("common: %d   SAME PARENT: %d   RE-PARENTED: %d\n" % (len(common), len(common)-len(diff), len(diff)))

# Which of these actually matter for archiving?  Load the archive-diff class list.
print("=== RE-PARENTED CLASSES (retail parent -> port parent) ===")
for c, s, p in sorted(diff):
    print("  %-30s %-22s -> %s" % (c, s, p))

def chain(h, c, limit=12):
    out=[c]
    while c in h and len(out)<limit:
        c = h[c][0]; out.append(c)
    return " -> ".join(out)

print("\n=== CORE CHAINS ===")
for c in ["Entity","Animate","Sentient","Actor","Player","Trigger","Mover","ScriptSlave","Projectile","Item","Weapon","World","Level","Game"]:
    print("  %-12s SDK : %s" % (c, chain(sdk, c) if c in sdk else "(absent)"))
    print("  %-12s PORT: %s" % ("",   chain(port, c) if c in port else "(absent)"))
