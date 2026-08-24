#!/usr/bin/env python3
"""Per-file word-boundary reference counts for the zero-SDK class list."""
import os, re, sys, collections

PORT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/app/src/main/cpp/code"
FGAME = os.path.join(PORT, "fgame")
IDENT = re.compile(r"\b[A-Za-z_]\w*")

classes = sys.argv[1:]
# classes plus everything after optional file
names = [c for c in classes if re.fullmatch(r"[A-Za-z_]\w*", c)]
paths = [c for c in classes if not re.fullmatch(r"[A-Za-z_]\w*", c)]
assert not paths, f"unexpected args: {paths}"

def walk(path):
    for root, dirs, files in os.walk(path):
        for f in files:
            if f.endswith((".h", ".cpp", ".c", ".hpp")):
                yield os.path.join(root, f)

files = {}
for p in walk(PORT):
    try:
        with open(p, "r", errors="replace") as fh:
            files[p] = fh.read()
    except Exception:
        pass

for name in names:
    print(f"\n===== {name} =====")
    rows = []
    for p, t in files.items():
        c = len(IDENT.findall(t))  # noqa
        n = len(re.findall(r"\b" + re.escape(name) + r"\b", t))
        if n:
            rel = os.path.relpath(p, PORT)
            rows.append((n, rel))
    rows.sort(reverse=True)
    for n, rel in rows:
        print(f"  {n:>4}  {rel}")
    total = sum(n for n, _ in rows)
    print(f"  TOTAL {total} in {len(rows)} files")
