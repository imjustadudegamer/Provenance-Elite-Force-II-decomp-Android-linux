#!/usr/bin/env python3
"""Delete provably-dead port-only class members.

A member is deleted only when
  (a) it is absent from the retail SDK header (member_census.py's job), and
  (b) its name occurs EXACTLY ONCE in the whole tree -- i.e. the declaration is
      the only textual reference, so nothing can read, write, archive or
      initialise it.

Only the declaration line is touched.  A constructor initialiser, an Archive
call or a script binding would all raise the occurrence count above 1 and so
exclude the member automatically.
"""
import os
import json, os, re, sys

PORT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + "/app/src/main/cpp/code/fgame"
SCAN = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + "/app/src/main/cpp/code"


def occurrence_paths(name):
    pat = re.compile(r"\b%s\b" % re.escape(name))
    hits = []
    for root, dirs, files in os.walk(SCAN):
        dirs[:] = [d for d in dirs if d not in ("tools", "botlib", "gamespy", "renderergl2")]
        for f in files:
            if not f.endswith((".c", ".cpp", ".h", ".hpp")):
                continue
            p = os.path.join(root, f)
            try:
                txt = open(p, encoding="latin1", errors="replace").read()
            except Exception:
                continue
            n = len(pat.findall(txt))
            if n:
                hits.append((p, n, txt))
    return hits


def drop_decl(txt, name):
    """Remove the single declaration line for `name`, returning (new, removed)."""
    pat = re.compile(r"^[^\n]*\b%s\b[^\n]*\n" % re.escape(name), re.M)
    ms = list(pat.finditer(txt))
    assert len(ms) == 1, "expected exactly 1 declaration line, got %d" % len(ms)
    return txt[:ms[0].start()] + txt[ms[0].end():], ms[0].group(0).rstrip("\n")


def main():
    data = json.load(open("/tmp/ef2docs/members.json"))
    targets = sys.argv[1:] or sorted(data)
    bycls = {c: sorted(m["name"] for m in data[c]["port_only"] if m["refs"] == 1)
             for c in targets if c in data}
    total = 0
    for cls, names in bycls.items():
        if not names:
            continue
        # the port header for this class holds every declaration
        hp = os.path.join(PORT, cls + ".h")
        txt = open(hp, encoding="latin1").read()
        removed = []
        for n in names:
            hits = occurrence_paths(n)
            if len(hits) != 1 or hits[0][0] != hp:
                print("  skip %-30s (%d hits, first %s)" % (n, len(hits), hits[0][0] if hits else "-"))
                continue
            txt, line = drop_decl(txt, n)
            removed.append(line)
            total += 1
        if removed:
            open(hp, "w", encoding="latin1").write(txt)
            print("%-12s removed %d" % (cls, len(removed)))
            for r in removed:
                print("      %s" % r.strip())
    print("\ntotal removed: %d" % total)


if __name__ == "__main__":
    main()
