#!/usr/bin/env python3
"""
Operation-sequence divergence verifier (foreground, mechanical).

For each divergent SDK↔port function pair, extract the ordered "operation fingerprint":
method calls, member accesses, keywords, and constants. Real behavioral divergence shows
as missing/different core operations in the same flow position; benign restructure shows
retail's operations with extra guard-checks inserted.

Output: docs/EF2_VERIFY_OPS.md
"""
import os
import re, os, sys, difflib
WORK = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/work"
SDK  = WORK + "/ef2-sdk/ef2-sdk/dlls/game"
PORT = WORK + "/openmohaa/code/fgame"

def strip_comments(src):
    src = re.sub(r'/\*.*?\*/', ' ', src, flags=re.S)
    return re.sub(r'//[^\n]*', '', src)

def split_functions(src):
    src = strip_comments(src); funcs=[]; i=0; n=len(src)
    pat = re.compile(r'([A-Za-z_][\w:&*\s]*?)([A-Za-z_]\w*)\s*\(([^;{]*?)\)\s*\{')
    while i < n:
        m = pat.search(src, i)
        if not m: break
        name=m.group(2); before=m.group(1); depth=0; j=m.end()-1
        while j < n:
            if src[j]=='{': depth+=1
            elif src[j]=='}':
                depth-=1
                if depth==0: break
            j+=1
        body=src[m.start():j+1]
        if '{' in body and body.strip().endswith('}') and name not in ('if','for','while','switch'):
            sig=body[:body.find('{')+1]
            if '::' in sig or (before.strip() and len(before.strip())<80):
                funcs.append(body)
        i=j+1
    return funcs

def name_of(body):
    sig=body[:body.find('{')]
    m=re.search(r'([\w:]+)::(\w+)\s*\(',sig)
    if m: return f"{m.group(1)}::{m.group(2)}"
    m2=re.search(r'([A-Za-z_]\w*)\s*\(',sig)
    return m2.group(1) if m2 else "?"

KEYWORDS = {"if","else","while","for","return","break","continue","switch","case","default","do","goto","new","delete","true","false","null","NULL"}
def ops(body):
    """Extract the operation fingerprint: calls, member names, keywords, constants."""
    b = body[body.find('{')+1:-1]
    # tokenize (keep parens to detect calls)
    toks = re.findall(r'[A-Za-z_]\w*|0x[0-9a-fA-F]+|\d+\.\d+f?|\d+f|\d+|"[^"]*"|\.|->|\(', b)
    out = []
    i = 0
    while i < len(toks):
        t = toks[i]
        if t in ('.','->'):
            # member access: take the name after
            if i+1 < len(toks) and re.match(r'[A-Za-z_]\w*$', toks[i+1]):
                out.append("." + toks[i+1]); i += 2; continue
            out.append(t); i += 1; continue
        if re.match(r'[A-Za-z_]\w*$', t):
            if t in KEYWORDS:
                out.append("kw:" + t)
            elif i+1 < len(toks) and toks[i+1] == '(':
                out.append("CALL:" + t)
            else:
                out.append(t)
            i += 1; continue
        if t == '(':
            i += 1; continue
        if re.match(r'0x[0-9a-fA-F]+$', t):
            out.append("N"); i += 1; continue
        if re.match(r'\d', t):
            out.append("NUM"); i += 1; continue
        if t.startswith('"'):
            out.append("STR"); i += 1; continue
        i += 1
    return out

def load_f(path):
    out = {}
    with open(path, errors='ignore') as f: src = f.read()
    for b in split_functions(src):
        nm = name_of(b)
        if nm and nm != "CLASS_DECLARATION":
            out.setdefault(nm, []).append(b)
    return out

divergent = []
for line in open(WORK + "/tools/divergent_list.txt"):
    p = line.rstrip("\n").split("\t")
    if len(p)==3: divergent.append((float(p[0]), p[1], p[2]))

# load all SDK + port files once
sdk_all, port_all = {}, {}
for f in set(d[2] for d in divergent):
    sp, pp = os.path.join(SDK, f), os.path.join(PORT, f)
    if os.path.exists(sp): sdk_all[f] = load_f(sp)
    if os.path.exists(pp): port_all[f] = load_f(pp)

rows = []
for ratio, name, f in divergent:
    sdk_bodies = sdk_all.get(f, {}).get(name, [])
    port_bodies = port_all.get(f, {}).get(name, [])
    if not sdk_bodies or not port_bodies:
        rows.append((ratio, name, f, "NO-BODY", [], []))
        continue
    so, po = ops(sdk_bodies[0]), ops(port_bodies[0])
    # LCS-based analysis: what retail ops are missing in the port, what port ops are extra
    sm = difflib.SequenceMatcher(None, so, po)
    missing = []
    extra = []
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == 'delete':  # in retail, not in port
            missing.extend(so[i1:i2])
        elif tag == 'insert':  # in port, not retail
            extra.extend(po[j1:j2])
    rows.append((ratio, name, f, "OK", missing, extra))

# classify: real if missing core ops (calls/keywords), benign if only extra guards
with open(WORK + "/EF2_VERIFY_OPS.md", "w") as out:
    out.write("# EF2 operation-sequence divergence verification\n\n")
    out.write("MISSING = retail operations absent from the port (REAL divergence candidate).\n")
    out.write("EXTRA = port operations not in retail (usually inserted guards = BENIGN).\n\n")
    real = benign = 0
    for ratio, name, f, status, missing, extra in sorted(rows, key=lambda r: r[0]):
        if status == "NO-BODY":
            out.write(f"## {ratio:.3f} {name} [{f}] — NO-BODY (one side missing)\n"); continue
        core_missing = [m for m in missing if m.startswith('CALL:') or m.startswith('kw:')]
        if core_missing:
            real += 1
            out.write(f"## 🔴 REAL-candidate {ratio:.3f} {name} [{f}]\n")
            out.write(f"  MISSING core ops: {core_missing[:15]}\n")
            out.write(f"  extra ops (guards?): {extra[:12]}\n")
        else:
            benign += 1
            out.write(f"## ✓ BENIGN {ratio:.3f} {name} [{f}]\n")
            if extra:
                out.write(f"  extra ops only: {extra[:12]}\n")
            if missing:
                out.write(f"  missing non-core ops: {missing[:12]}\n")
    out.write(f"\n## SUMMARY: real-candidates {real} | benign {benign}\n")

print("done")
print("real-candidates + benign:", sum(1 for r in rows if r[3]=='OK'))
print(open(WORK+"/EF2_VERIFY_OPS.md").read().split("## SUMMARY")[-1])
