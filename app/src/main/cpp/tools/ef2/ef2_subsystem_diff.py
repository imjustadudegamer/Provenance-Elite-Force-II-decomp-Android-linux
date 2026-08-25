#!/usr/bin/env python3
"""EF2 subsystem differential: per-SDK-file function diff vs port + retail-binary data refs."""
import os
import re, os, sys, difflib, struct
WORK = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/work"
SDK  = WORK + "/ef2-sdk/ef2-sdk/dlls/game"
PORT = WORK + "/openmohaa/code/fgame"
DECOMP = WORK + "/mac_symbols/game_decomp"
SYM   = WORK + "/mac_symbols/game_dll_symbols.txt"
DATA_BIN = "/tmp/retail_data.bin"
DATA_BASE = 0x7A33E

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

def normalize(body):
    b=re.sub(r'\s+',' ',body)
    b=re.sub(r'0x[0-9a-fA-F]+','N',b)
    b=re.sub(r'\b\d+\.\d+f?\b','F',b); b=re.sub(r'\b\d+f\b','F',b)
    b=re.sub(r'\b\d+\b','N',b); b=re.sub(r'"[^"]*"','S',b)
    return b

def name_of(body):
    sig=body[:body.find('{')]
    m=re.search(r'([\w:]+)::(\w+)\s*\(',sig)
    if m: return f"{m.group(1)}::{m.group(2)}"
    m2=re.search(r'([A-Za-z_]\w*)\s*\(',sig)
    return m2.group(1) if m2 else "?"

def load(path):
    out={}
    with open(path,errors='ignore') as f: src=f.read()
    for b in split_functions(src):
        nm=name_of(b)
        if nm and nm!="CLASS_DECLARATION":
            out.setdefault(nm,[]).append(normalize(b))
    return out

DATA=open(DATA_BIN,'rb').read()

def decode_ref(kind, addr):
    """Decode a retail data global to a readable constant."""
    def rd(a, n):
        off = a - DATA_BASE
        if off < 0 or off + n > len(DATA): return None
        return DATA[off:off+n]
    if kind == 'off':
        w = rd(addr, 4)
        if not w: return None
        target = struct.unpack('>I', w)[0]
        t = rd(target, 4)
        if t: return f"->0x{target:X} float {struct.unpack('>f',t)[0]:g}"
        return f"->0x{target:X}"
    if kind == 'flt':
        t = rd(addr, 4)
        return f"{struct.unpack('>f',t)[0]:g}" if t else None
    if kind == 'dword':
        t = rd(addr, 4)
        return f"{struct.unpack('>I',t)[0]}" if t else None
    if kind == 'byte':
        t = rd(addr, 1)
        return f"{t[0]}" if t else None
    return None

def read_word(addr):
    off=addr-DATA_BASE
    if off<0 or off+4>len(DATA): return None
    return struct.unpack('>I',DATA[off:off+4])[0]

# mangle -> demangled-base map (from symbol table last column)
mangle_map={}   # demangled-base -> [mangles]
with open(SYM) as f:
    for line in f:
        m=re.match(r'0x[0-9a-f]+\s+\d+\s+\.(\S+)\s+(.*)$', line.strip())
        if m:
            base=re.sub(r'\(.*','',m.group(2)).strip()
            mangle_map.setdefault(base,[]).append(m.group(1))

rows=[]; unmatched=[]
for f in sorted(os.listdir(SDK)):
    if not f.endswith(".cpp"): continue
    p=os.path.join(PORT,f)
    if not os.path.exists(p): continue
    sdk,port=load(os.path.join(SDK,f)),load(p)
    for nm,sb in sdk.items():
        if nm in port:
            r=max(difflib.SequenceMatcher(None,x,y).ratio() for x in sb for y in port[nm])
            rows.append((f,nm,r))
        else:
            unmatched.append((f,nm))

from collections import defaultdict
sub=defaultdict(lambda:[0,0,0,0.0])
for f,nm,r in rows:
    sub[f][0]+=1; sub[f][3]+=r
    if r>=0.9: sub[f][1]+=1
    elif r<0.4: sub[f][2]+=1

gpat=re.compile(r'\b(off|dword|flt|byte|unk|word)_([0-9A-Fa-f]{4,8})\b')
out=[]
out.append("# EF2 subsystem differential + retail-binary data verification\n")
out.append(f"subsystems {len(sub)} | matched {len(rows)} | divergent<0.4 {sum(1 for r in rows if r[2]<0.4)} | unmatched {len(unmatched)}\n")
out.append("## Per-subsystem aggregate\n| file | matched | aligned | divergent | avg-ratio |")
out.append("|---|---|---|---|---|")
for f,(m,al,dv,rs) in sorted(sub.items(),key=lambda kv:-kv[1][2]):
    out.append(f"| {f} | {m} | {al} | {dv} | {rs/m:.2f} |")
out.append("")
out.append("## Divergent functions + retail-binary data they reference\n")
for f,nm,r in sorted([x for x in rows if x[2]<0.4],key=lambda x:x[2]):
    out.append(f"### {r:.3f} {nm} [{f}]")
    mangles=mangle_map.get(nm,[])
    if not mangles:
        out.append("  (no symbol-table mangle)")
        continue
    refs=set()
    for mg in mangles:
        dp=os.path.join(DECOMP,mg+".c")
        if os.path.exists(dp):
            body=open(dp,errors='ignore').read()
            for g in gpat.findall(body):
                addr=int(g[1],16)
                w=read_word(addr)
                dec = decode_ref(g[0], addr)
                refs.add(f"{g[0]}_{addr:X}" + (f" word=0x{w:08X}" if w is not None else " <oor>") + (f" [{dec}]" if dec else ""))
    for r in sorted(refs):
        out.append(f"  {r}")
    if not refs:
        out.append("  (no decompile data refs)")

with open(WORK+"/EF2_SUBSYSTEM_DIFF.md","w") as fh:
    fh.write("\n".join(out)+"\n")
print("subsystems",len(sub),"matched",len(rows),"divergent",sum(1 for r in rows if r[2]<0.4),"unmatched",len(unmatched))
