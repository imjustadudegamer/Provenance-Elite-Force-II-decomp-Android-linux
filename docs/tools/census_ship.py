#!/usr/bin/env python3
"""Shipped-content census read straight out of the retail PK3s (no extraction).

Retail paks are zip containers, so every reachability claim of the form
"0 shipped <ext> does X" can be settled in one pass over the paks instead of a
partial scratch extraction.  This tool is the settling instrument for the
ship-census rows of DIVERGENCE_LEDGER_0904 / w30final_0912_master.tsv.

usage:
  census_ship.py                          # extension histogram
  census_ship.py --list tik               # every shipped .tik path
  census_ship.py --grep ischaracter:tik   # files where a token appears
  census_ship.py --grep a.tik,b.tan       # several tokens/exts at once
  census_ship.py --dir <pakdir>           # non-default pak directory

Grep is a case-insensitive byte search inside the archive member, so it also
covers binary lumps (BSP entity text) and works on deflated members.
"""
import os
import argparse, glob, os, sys, zipfile
from collections import Counter

DEFAULT_DIR = os.path.expanduser("~/Star Trek Elite Force II/base")


def paks(pakdir):
    out = sorted(glob.glob(os.path.join(pakdir, "pak*.pk3")))
    if not out:
        sys.exit("no pak*.pk3 under " + pakdir)
    return out


def members(pakdir):
    """Yield (pak, name, bytes) for every archive member."""
    for p in paks(pakdir):
        with zipfile.ZipFile(p) as z:
            for info in z.infolist():
                if info.is_dir():
                    continue
                yield os.path.basename(p), info.filename, z.read(info)


def ext_of(name):
    return os.path.splitext(name)[1].lower().lstrip(".")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default=DEFAULT_DIR)
    ap.add_argument("--list", metavar="EXT")
    ap.add_argument("--grep", metavar="TOKEN:EXT[,TOKEN:EXT...]")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()

    if a.list:
        want = a.list.lower().lstrip(".")
        n = 0
        for _pak, name, _b in members(a.dir):
            if ext_of(name) == want:
                print(name)
                n += 1
        print("total .%s = %d" % (want, n))
        return

    if a.grep:
        specs = []
        for s in a.grep.split(","):
            tok, _, ext = s.rpartition(":")
            if not tok or not ext:
                sys.exit("bad --grep spec: " + s)
            specs.append((tok.encode().lower(), ext.lower().lstrip(".")))
        hits = Counter()
        for _pak, name, b in members(a.dir):
            e = ext_of(name)
            low = b.lower()
            for tok, ext in specs:
                if e == ext and tok in low:
                    hits[(tok.decode(), ext)] += 1
                    if not a.quiet:
                        print("%s\t%s" % (name, tok.decode()))
        print("--- grep totals (files scanned = all members)")
        for (tok, ext), c in sorted(hits.items()):
            print("%-28s .%-6s %d" % (tok, ext, c))
        return

    c = Counter()
    for _pak, name, _b in members(a.dir):
        c[ext_of(name)] += 1
    for ext, n in c.most_common():
        print("%-8s %d" % (ext, n))
    print("total members %d" % sum(c.values()))


if __name__ == "__main__":
    main()
