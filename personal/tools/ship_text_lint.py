#!/usr/bin/env python3
"""The shipped-text gate: the project's own text stays impersonal and portable.

  A  tracked markdown keeps one impersonal voice (no we/us/our/ours/ourselves);
     vendored third-party documents are exempt.
  B  no file in the project's own trees names a user home path, and none names
     a .cfg outside the engine's own set -- a machine-specific path or config
     name is a binary, not ship material.

Exit: 0 green, 1 red, 2 usage.
"""

import argparse
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))

FIRST_PERSON = re.compile(r"\b(we|us|our|ours|ourselves)\b", re.I)

SCAN_ROOTS = ("app/src/main/cpp/code", "app/src/main/java/com/provenanceef2",
              "docs", "tools")
SCAN_EXT = (".c", ".h", ".cpp", ".hpp", ".java", ".py", ".sh", ".md",
            ".cmake", ".txt", ".kts", ".xml", ".inc", ".glsl")
SKIP_COMPONENTS = ("thirdparty", ".git", ".build", "__pycache__", "build",
                   "build-linux-gles3", "node_modules",
                   # the gitignored worktree area: workstream scratch and its
                   # copied launchers carry absolute paths by construction, and
                   # none of it is shipped text
                   "wt")
HOME_PATH = re.compile(r"(?:/home/|/Users/|\\\\Users\\\\)([A-Za-z0-9_.<>-]+)")
HOME_PLACEHOLDERS = {"<user>", "<name>", "user", "name", "username", "you"}
CFG_LITERAL = re.compile(r'"([A-Za-z0-9_./-]+\.cfg)"')
# The engine's own config names; a .cfg outside this set is not ship material.
CFG_ALLOW = {
    "config.cfg", "config_server.cfg", "autoexec.cfg", "android_defaults.cfg",
    "default.cfg", "defaultcontrols.cfg", "defaultoptions.cfg", "fast.cfg",
    "game.cfg", "localized.cfg", "medium.cfg", "memorylow.cfg", "memorymed.cfg",
    "memoryhigh.cfg", "menu.cfg", "mp_ctf.cfg", "mp_dm.cfg", "mp_team.cfg",
    "mp_PointsPerWeapon.cfg", "slow.cfg",
}


def read(path):
    with open(path, "r", encoding="utf-8", errors="surrogateescape", newline="") as fh:
        return fh.read()


def tracked_markdown(root):
    p = subprocess.run(["git", "-C", root, "ls-files", "-z", "*.md"],
                       capture_output=True, text=True)
    if p.returncode != 0 or not p.stdout:
        return []
    return [os.path.join(root, f) for f in p.stdout.split("\0") if f]


def voice_scan(root):
    hits = []
    for path in tracked_markdown(root):
        rel = os.path.relpath(path, root).replace(os.sep, "/")
        if "thirdparty/" in rel or not os.path.isfile(path):
            continue
        for i, line in enumerate(read(path).splitlines(), 1):
            if FIRST_PERSON.search(line):
                hits.append((rel, i, line.strip()))
    return hits


def path_scan(root):
    hits = []
    for top in SCAN_ROOTS:
        base = os.path.join(root, top)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [d for d in dirnames if d not in SKIP_COMPONENTS]
            for fn in sorted(filenames):
                if not fn.endswith(SCAN_EXT):
                    continue
                path = os.path.join(dirpath, fn)
                rel = os.path.relpath(path, root).replace(os.sep, "/")
                for i, line in enumerate(read(path).splitlines(), 1):
                    for m in HOME_PATH.finditer(line):
                        if m.group(1).lower() not in HOME_PLACEHOLDERS:
                            hits.append((rel, i, "home path: " + line.strip()[:70]))
                    for m in CFG_LITERAL.finditer(line):
                        name = m.group(1).rsplit("/", 1)[-1]
                        if name not in CFG_ALLOW:
                            hits.append((rel, i, "config file: " + line.strip()[:70]))
    return hits


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=ROOT)
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    failures = []

    vhits = voice_scan(args.root)
    if not args.quiet:
        for rel, i, line in vhits[:10]:
            print("  %s:%d  %s" % (rel, i, line[:80]))
    if vhits:
        failures.append("shipped markdown uses we/us/our/ours/ourselves (%d hit(s))" % len(vhits))

    phits = path_scan(args.root)
    if not args.quiet:
        for rel, i, line in phits[:10]:
            print("  %s:%d  %s" % (rel, i, line))
    if phits:
        failures.append("shipped text names a user home or an unknown .cfg (%d hit(s))" % len(phits))

    for f in failures:
        print("ship_text_lint: FAIL  %s" % f)
    if failures:
        print("ship_text_lint: RED -- %d check(s) failed" % len(failures))
        return 1
    if not args.quiet:
        print("ship_text_lint: GREEN -- one voice, no machine path, no foreign .cfg")
    return 0


if __name__ == "__main__":
    sys.exit(main())
