#!/usr/bin/env python3
"""Syntax-only compile check for one source file, in ANY tree.

Usage:  python3 syncheck.py <path/to/file.cpp> [more.cpp ...]

Pulls the real compile command for that file out of the MAIN tree's
build/compile_commands.json, rewrites every path so it points at the
tree the file actually lives in, and runs it with -fsyntax-only.
Costs ~2 s per file and never writes an object file, so it is safe to
run concurrently from many trees.
"""
import json
import os
import re
import shlex
import subprocess
import sys

MAIN = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/app/src/main/cpp"
DB = os.path.join(MAIN, "build", "compile_commands.json")


def tree_root_of(path):
    """Walk up until we find the checkout root (has a code/ dir)."""
    p = os.path.abspath(path)
    while p != "/":
        p = os.path.dirname(p)
        if os.path.isdir(os.path.join(p, "code")) and os.path.exists(os.path.join(p, ".git")):
            return p
    return MAIN


def main(argv):
    if not os.path.exists(DB):
        print("no compile_commands.json in the main tree; run "
              "`cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` there first")
        return 2
    db = json.load(open(DB))
    by_suffix = {}
    for e in db:
        by_suffix[os.path.relpath(e["file"], MAIN)] = e

    rc = 0
    for target in argv:
        target = os.path.abspath(target)
        root = tree_root_of(target)
        rel = os.path.relpath(target, root)
        entry = by_suffix.get(rel)
        if entry is None:
            print(f"SKIP  {rel}: not in compile_commands.json (header, or not built)")
            continue
        cmd = entry["command"].replace(MAIN, root)
        # strip output/LTO flags, add syntax-only
        parts = shlex.split(cmd)
        out = []
        skip = False
        for i, a in enumerate(parts):
            if skip:
                skip = False
                continue
            if a == "-o":
                skip = True
                continue
            if a in ("-c",) or a.startswith("-flto") or a == "-fno-fat-lto-objects":
                continue
            out.append(a)
        out.append("-fsyntax-only")
        out.append(target)
        r = subprocess.run(out, capture_output=True, text=True, cwd=root)
        lines = r.stderr.splitlines()
        # Only real diagnostics; this tree emits a large, pre-existing warning wall
        # that would otherwise bury them.
        errs = [l for l in lines if re.search(r":\d+:\d+: (fatal )?error:", l)]
        if r.returncode != 0 or errs:
            rc = 1
            print(f"FAIL  {rel}  ({len(errs)} error lines)")
            if errs:
                print("\n".join(errs[:80]))
            else:
                # non-zero exit with no parsed error line: show the tail verbatim
                print("\n".join(lines[-40:]))
        else:
            print(f"OK    {rel}")
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
