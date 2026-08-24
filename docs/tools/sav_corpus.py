#!/usr/bin/env python3
"""Retail EF2 savegame corpus walker -- the ABI oracle for the savegame layer.

Corpus: <home>/Downloads/startrekeliteforce2_savegames_all.zip
        (116 .sav + 179 .sub, retail 2003)

What this checks today
----------------------
The fixed part of the retail archive header, which is written by fgame/archive.cpp
and read back by FileRead::Open:

    offset 0   char[4]  "EF2\0"          == ArchiveHeader  (*(int *)"EF2")
    offset 4   int32    2                == ArchiveVersion
    offset 8   int32    21               strlen(ArchiveInfo)
    offset 12  char[21] "EF2 Archive Version 2" == ArchiveInfo

Everything from offset 33 on is the Archiver's typed stream (fgame/archive.cpp's
Archive<T> family).  This script does NOT model that stream -- doing so means
reimplementing the object/safepointer fixup tables and the event serialiser in
Python, which is its own task.  What it does instead is check the three things a
savegame rewrite can regress without touching C:

  1. every file in the corpus carries the retail header byte for byte
     (so GAME_API_VERSION / header-string / version churn is caught);
  2. the header the port writes is identical to the header retail wrote
     (compares against fgame/archive.cpp's own defines);
  3. the corpus is uniform -- if any file deviates on any of the above, the
     ABI assumption behind "retail saves must load" is wrong and the file is
     named.  (Verified on the whole corpus: all 116 .sav + 179 .sub agree.)

Usage
-----
    python3 docs/tools/sav_corpus.py [ extracted-corpus-dir ]

With no argument it extracts the zip into a temp dir first.
"""
import os, re, struct, sys, zipfile, tempfile

ZIP = os.path.expanduser("~/Downloads/startrekeliteforce2_savegames_all.zip")

MAGIC = b"EF2\x00"
VERSION = 2
INFO = b"EF2 Archive Version 2"

PORT_ARCHIVE_CPP = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "openmohaa", "code", "fgame", "archive.cpp")


def port_defines():
    """Pull the defines straight out of fgame/archive.cpp so the two can never
    drift apart without this script noticing."""
    src = open(PORT_ARCHIVE_CPP, encoding="latin1").read()
    out = {}
    m = re.search(r'#define\s+ArchiveHeader\s+\(?\s*\*\s*\(\s*int\s*\*\s*\)\s*"(.*?)"', src)
    out["magic"] = m.group(1).encode("latin1") + b"\x00" if m else None
    m = re.search(r'#define\s+ArchiveVersion\s+(\d+)', src)
    out["version"] = int(m.group(1)) if m else None
    m = re.search(r'#define\s+ArchiveInfo\s+"([^"]*)"', src)
    out["info"] = m.group(1).encode("latin1") if m else None
    return out


def check(path):
    """Return (ok, detail)."""
    n = os.path.getsize(path)
    with open(path, "rb") as f:
        head = f.read(12 + len(INFO) + 1)
    if len(head) < 12 + len(INFO) + 1:
        return False, "truncated (%d bytes)" % n
    magic = head[0:4]
    ver, slen = struct.unpack_from("<ii", head, 4)
    info = head[12:12 + slen] if 0 <= slen <= 64 else b""
    first_tag = head[12 + len(INFO)]
    problems = []
    if magic != MAGIC:
        problems.append("magic %r != %r" % (magic, MAGIC))
    if ver != VERSION:
        problems.append("version %d != %d" % (ver, VERSION))
    if slen != len(INFO):
        problems.append("info len %d != %d" % (slen, len(INFO)))
    elif info != INFO:
        problems.append("info %r != %r" % (info, INFO))
    if problems:
        return False, "; ".join(problems)
    return True, "size=%d tag0=%d" % (n, first_tag)


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else None
    if not d:
        d = tempfile.mkdtemp(prefix="ef2sav_")
        with zipfile.ZipFile(ZIP) as z:
            z.extractall(d)
        print("extracted to", d)

    p = port_defines()
    print("port fgame/archive.cpp: magic=%r version=%r info=%r" %
          (p["magic"], p["version"], p["info"]))
    ok = True
    if p["magic"] != MAGIC:
        print("  !! port magic %r != retail %r" % (p["magic"], MAGIC)); ok = False
    if p["version"] != VERSION:
        print("  !! port version %r != retail %d" % (p["version"], VERSION)); ok = False
    if p["info"] != INFO:
        print("  !! port info %r != retail %r" % (p["info"], INFO)); ok = False

    savs, subs = [], []
    for root, _, files in os.walk(d):
        for f in files:
            if f.endswith(".sav"):
                savs.append(os.path.join(root, f))
            elif f.endswith(".sub"):
                subs.append(os.path.join(root, f))
    savs.sort(); subs.sort()

    bad = 0
    for label, group in ((".sav", savs), (".sub", subs)):
        print("\n== %s: %d files ==" % (label, len(group)))
        badhere = 0
        for path in group:
            good, detail = check(path)
            if not good:
                badhere += 1
                print("  BAD  %s\n       %s" % (os.path.relpath(path, d), detail))
        bad += badhere
        print("  %d/%d conform" % (len(group) - badhere, len(group)))

    print("\n%s  (%d failures)" % ("CORPUS OK" if ok and bad == 0 else "CORPUS FAILURES", bad + (0 if ok else 1)))
    return 0 if (ok and bad == 0) else 1


if __name__ == "__main__":
    sys.exit(main())
