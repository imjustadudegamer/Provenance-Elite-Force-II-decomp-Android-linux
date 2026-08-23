#!/usr/bin/env python3
"""Extract identifiers inside port class bodies (Actor/Sentient/Player/Entity/...)
and report which have ZERO word-boundary hits in the whole retail EF2 SDK tree.
"""
import os, re, sys, collections

PORT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/app/src/main/cpp/code"
SDK = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/personal/evidence/ef2-sdk/ef2-sdk"

IDENT = re.compile(r"\b[A-Za-z_]\w*")

def walk(path):
    for root, dirs, files in os.walk(path):
        for f in files:
            if f.endswith((".h", ".cpp", ".c", ".hpp")):
                yield os.path.join(root, f)

# SDK identifier counter
sdk = collections.Counter()
for p in walk(SDK):
    try:
        with open(p, "r", errors="replace") as fh:
            for m in IDENT.finditer(fh.read()):
                sdk[m.group(0)] += 1
    except Exception:
        pass

def class_body(text, classname):
    """Return (body_text, start_line) of first `class classname` with brace matching."""
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

if __name__ == "__main__":
    for arg in sys.argv[1:]:
        if "=" in arg:
            file, classname = arg.split("=")
        else:
            file, classname = arg, arg
        p = os.path.join(PORT, file)
        if not os.path.exists(p):
            p = os.path.join(PORT, "fgame", file)
        with open(p, "r", errors="replace") as fh:
            text = fh.read()
        body, line0 = class_body(text, classname)
        if body is None:
            print(f"### {classname}: NOT FOUND in {p}")
            continue
        print(f"\n### {classname} @ {p}:{line0} body {len(body)} chars")
        # strip comments from body
        body = re.sub(r"/\*.*?\*/", " ", body, flags=re.S)
        body = re.sub(r"//[^\n]*", " ", body)
        counts = collections.Counter()
        loc = {}
        for m in IDENT.finditer(body):
            name = m.group(0)
            counts[name] += 1
            loc.setdefault(name, body[:m.start()].count("\n") + 1 + line0 - 1)
        zero = sorted((n for n in counts if sdk.get(n, 0) == 0),
                      key=lambda n: (-counts[n], n))
        for n in zero:
            print(f"  ZERO-SDK  x{counts[n]:<3} first@{loc[n]}  {n}")
        print(f"  ({len(zero)} zero-SDK identifiers of {len(counts)} total)")
