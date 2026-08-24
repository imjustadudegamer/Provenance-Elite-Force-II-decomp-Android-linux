#!/usr/bin/env python3
"""Byte-stream divergence: compare the ordered sequence of archive TYPES only
(field names do not affect the emitted bytes)."""
import os
import re, glob, os
SDK  = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/docs/evidence/ef2-sdk/ef2-sdk/dlls/game"
PORT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/app/src/main/cpp/code/fgame"

def strip_comments(s):
    s = re.sub(r'//[^\n]*', '', s)           # LINE comments first
    return re.sub(r'/\*.*?\*/', '', s, flags=re.S)

DEF = re.compile(r'\bvoid\s+([A-Za-z_]\w*)\s*::\s*Archive\s*\(\s*Archiver\s*&\s*\w*\s*\)\s*\{', re.S)
def bodies(root):
    out = {}
    for path in glob.glob(root+"/*.cpp")+glob.glob(root+"/*.h"):
        src = strip_comments(open(path, encoding='latin1').read())
        for m in DEF.finditer(src):
            i = m.end()-1; depth=0; j=i
            while j < len(src):
                if src[j]=='{': depth+=1
                elif src[j]=='}':
                    depth-=1
                    if depth==0: break
                j+=1
            out[m.group(1)] = (os.path.basename(path), src[i:j+1])
    return out

STEP = re.compile(r'\b(?:arc\s*\.\s*(Archive\w+)\s*\(|([A-Za-z_]\w*)\s*::\s*Archive\s*\(\s*\w+\s*\)\s*;)', re.S)
def kinds(body):
    out=[]
    for m in STEP.finditer(body):
        out.append(m.group(1) if m.group(1) else "^"+m.group(2))
    return out

sdk, port = bodies(SDK), bodies(PORT)
common = sorted(set(sdk)&set(port))
same, diff = [], []
for c in common:
    a,b = kinds(sdk[c][1]), kinds(port[c][1])
    (same if a==b else diff).append((c,a,b))

print("common classes: %d   BYTE-STREAM IDENTICAL: %d   DIVERGENT: %d" % (len(common), len(same), len(diff)))
print("\n=== DIVERGENT (first mismatch position / sdk len / port len) ===")
for c,a,b in sorted(diff, key=lambda x:-abs(len(x[1])-len(x[2]))):
    k=0
    while k<len(a) and k<len(b) and a[k]==b[k]: k+=1
    print("  %-26s @%-4d sdk=%-4d port=%-4d   sdk[%d]=%-22s port[%d]=%s" % (
        c, k, len(a), len(b), k, (a[k] if k<len(a) else "<end>"), k, (b[k] if k<len(b) else "<end>")))
