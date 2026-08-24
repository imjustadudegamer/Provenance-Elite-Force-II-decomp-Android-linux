#!/usr/bin/env python3
"""Classify zero-SDK identifiers in a class body into DATA MEMBERS vs methods.
A zero-SDK identifier is a data-member if, on its declaration line, no '(' appears
between it and the end of statement (; or , or }).
"""
import os, re, sys, collections

PORT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/app/src/main/cpp/code"
SDK = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/docs/evidence/ef2-sdk/ef2-sdk"
IDENT = re.compile(r"\b[A-Za-z_]\w*")

def walk(path):
    for root, dirs, files in os.walk(path):
        for f in files:
            if f.endswith((".h", ".cpp", ".c", ".hpp")):
                yield os.path.join(root, f)

sdk = collections.Counter()
for p in walk(SDK):
    try:
        with open(p, "r", errors="replace") as fh:
            for m in IDENT.finditer(fh.read()):
                sdk[m.group(0)] += 1
    except Exception:
        pass

def class_body(text, classname):
    for m in re.finditer(r"\bclass\s+" + classname + r"\b[^;]*?\{", text, re.S):
        depth, i, start = 0, m.end() - 1, m.end() - 1
        while i < len(text):
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    return text[start:i + 1], text[:start].count("\n") + 1
            i += 1
    return None, None

file, classname = sys.argv[1].split("=")
p = os.path.join(PORT, "fgame", file) if not file.startswith("fgame/") else os.path.join(PORT, file)
text = open(p, errors="replace").read()
body, line0 = class_body(text, classname)
body = re.sub(r"/\*.*?\*/", " ", body, flags=re.S)
body = re.sub(r"//[^\n]*", " ", body)

# split into statement lines
lines = body.split("\n")
members, methods = {}, {}
seen = {}
for li, line in enumerate(lines, 1):
    for m in IDENT.finditer(line):
        n = m.group(0)
        if sdk.get(n, 0) > 0 or n in seen:
            continue
        after = line[m.end():]
        stmt = after.split(";", 1)[0]
        is_method = "(" in after and "(" in stmt
        key = "methods" if is_method else "members"
        seen[n] = (li + line0 - 1, line.strip())
        (methods if is_method else members)[n] = (li + line0 - 1, line.strip())

print(f"== DATA MEMBERS (zero-SDK), {len(members)} ==")
for n, (ln, src) in sorted(members.items(), key=lambda kv: kv[1][0]):
    print(f"  {ln:>6}  {n:<40} {src}")
print(f"\n== METHODS (zero-SDK), {len(methods)} ==")
for n, (ln, src) in sorted(methods.items(), key=lambda kv: kv[1][0]):
    print(f"  {ln:>6}  {n}")
