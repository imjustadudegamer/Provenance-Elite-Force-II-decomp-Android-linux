#!/usr/bin/env python3
"""Audit the OpenMoHAA port's fgame classes against the retail EF2 SDK.

Single-pass identifier counter; then per-class lookups are O(1).
"""
import os, re, sys, collections

PORT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/app/src/main/cpp/code"
FGAME = os.path.join(PORT, "fgame")
SDK = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/personal/evidence/ef2-sdk/ef2-sdk/dlls/game"

IDENT = re.compile(r"\b[A-Za-z_]\w*")

def walk(path):
    for root, dirs, files in os.walk(path):
        for f in files:
            if f.endswith((".h", ".cpp", ".c", ".hpp")):
                yield os.path.join(root, f)

def counter(path):
    c = collections.Counter()
    files = {}
    for p in walk(path):
        try:
            with open(p, "r", errors="replace") as fh:
                t = fh.read()
            files[p] = t
            for m in IDENT.finditer(t):
                c[m.group(0)] += 1
        except Exception as e:
            print(f"ERR reading {p}: {e}", file=sys.stderr)
    return c, files

sdk_counter, _ = counter(SDK)
port_counter, port_files = counter(PORT)
fgame_files = [p for p in port_files if p.startswith(FGAME)]

def sdk_hits(name): return sdk_counter.get(name, 0)
def port_hits(name): return port_counter.get(name, 0)

# ---------------- extract classes ----------------
class_decl = collections.OrderedDict()
for p in fgame_files:
    for m in re.finditer(r"CLASS_DECLARATION\s*\(\s*([A-Za-z_]\w*)\s*,\s*([A-Za-z_]\w*)", port_files[p]):
        name = m.group(2)
        if name not in class_decl:
            class_decl[name] = (p, port_files[p][:m.start()].count("\n") + 1)
for p in fgame_files:
    for m in re.finditer(r"\bclass\s+([A-Za-z_]\w*)\s*[:{]", port_files[p]):
        name = m.group(1)
        if name not in class_decl:
            class_decl[name] = (p, port_files[p][:m.start()].count("\n") + 1)

results = []
for name, (declfile, declline) in class_decl.items():
    sdk = sdk_hits(name)
    total = port_hits(name)
    outside = total - port_files[declfile].count(name)
    results.append((name, declfile, declline, sdk, total, outside))

results.sort(key=lambda r: (r[3] > 0, r[0]))
print(f"{'CLASS':<32} {'SDKhits':>7} {'portTotal':>9} {'outsideDecl':>11}  declared at")
for name, df, dl, sdk, tot, out in results:
    rel = os.path.relpath(df, FGAME)
    print(f"{name:<32} {sdk:>7} {tot:>9} {out:>11}  {rel}:{dl}")
