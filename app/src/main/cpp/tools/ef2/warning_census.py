#!/usr/bin/env python3
"""Complete, normalised compiler-warning census for the EF2 port.

Replaces the ad-hoc grep that produced re_specs_0731/unique_warnings.txt, which
had two proven defects:

  1. It matched only warnings carrying a "[-Wflag]" tag.  GCC emits plenty of
     untagged ones ("#pragma once in main file", "'struct x' declared inside
     parameter list"), and those were invisible -- 0 of its 3,284 lines lacked a
     tag, which is not plausible for a tree this size.
  2. It did not normalise ".." in paths, so one physical site appeared under
     several spellings (code/gamespy/qr2/../common/gsMemory.h vs
     code/gamespy/common/gsMemory.h) and inflated per-directory totals, while
     genuinely foreign sites were attributed to whichever directory reached them.

Usage:
    python3 warning_census.py <build.log>          # census an existing log
    python3 warning_census.py --rebuild            # clean rebuild, then census

Output: a per-directory and per-flag table, plus a normalised unique-site list
on stdout (one "path:line:col: text" per line) suitable for diffing between runs.
"""
import collections
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/app/src/main/cpp"
# "<path>:<line>:<col>: warning: <text>"  -- tag is optional on purpose
# Column is OPTIONAL: preprocessor warnings ("X redefined") carry no column, and
# requiring one hid a real MAX_STACK_DEPTH collision on an earlier run.
WARN = re.compile(
    r"^(?P<path>[^\s:][^:]*):(?P<line>\d+):(?:(?P<col>\d+):)?\s+warning:\s+(?P<text>.*)$")
# NOT anchored to end-of-line: GCC does not always put the flag last, and anchoring
# it silently mis-files those as "(untagged)" -- which inflated the untagged count
# from 1 to 47 on the first run of this tool.
TAG = re.compile(r"\[(-W[a-z0-9+=.-]+)\]")


def normalise(path):
    """Collapse .. and make repo-relative, so one site has exactly one name."""
    p = os.path.normpath(path)
    if p.startswith(REPO + os.sep):
        p = os.path.relpath(p, REPO)
    return p


def bucket(relpath):
    parts = relpath.split(os.sep)
    if parts and parts[0] == "code" and len(parts) > 1:
        return parts[1]
    return parts[0] if parts else "?"


def census(logpath):
    sites = {}
    for raw in open(logpath, errors="replace"):
        m = WARN.match(raw.strip())
        if not m:
            continue
        p = normalise(m.group("path"))
        # System headers are REPORTED, not dropped: a fortified-memset warning is
        # emitted against /usr/include/... but its inlining context is our code, and
        # silently discarding those hid 11 real sites on an earlier run.
        if p.startswith("/") or p.startswith(".."):
            p = "<system> " + p
        text = m.group("text")
        t = TAG.search(text)
        key = (p, int(m.group("line")), int(m.group("col") or 0), text)
        sites[key] = t.group(1) if t else "(untagged)"
    return sites


def main(argv):
    if not argv:
        print(__doc__)
        return 2

    if argv[0] == "--rebuild":
        log = "/tmp/ef2_warning_census.log"
        subprocess.run(["ninja", "-C", os.path.join(REPO, "build"), "-t", "clean"],
                       capture_output=True)
        with open(log, "w") as f:
            r = subprocess.run(["ninja", "-C", os.path.join(REPO, "build")],
                               stdout=f, stderr=subprocess.STDOUT)
        print("rebuild exit=%d, log=%s" % (r.returncode, log))
    else:
        log = argv[0]

    sites = census(log)
    by_dir = collections.Counter(bucket(k[0]) for k in sites)
    by_flag = collections.Counter(sites.values())

    print("UNIQUE WARNING SITES (normalised): %d" % len(sites))
    print()
    print("by directory:")
    for d, n in by_dir.most_common():
        print("  %-18s %5d" % (d, n))
    print()
    print("by flag:")
    for f, n in by_flag.most_common():
        print("  %-34s %5d" % (f, n))

    out = os.environ.get("EF2_CENSUS_OUT")
    if out:
        with open(out, "w") as f:
            for (p, ln, col, text) in sorted(sites):
                f.write("%s:%d:%d: %s\n" % (p, ln, col, text))
        print("\nwrote %s" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
