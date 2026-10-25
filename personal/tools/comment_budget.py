#!/usr/bin/env python3
"""The comment budget.

The tree keeps its editorial comments small.  Only files this project authors or
maintains are measured; vendored trees are excluded by scope, and the licence
notice a file must carry is excluded by definition and reported separately,
because the licences require it and it is not editorial.

SCOPE (the only files counted)
    app/src/main/cpp/code/**            minus code/thirdparty/,
                                              code/tools/ommap/,
                                              code/tools/lcc/
    app/src/main/java/com/provenanceef2/**
    app/make_zpak.py
Generated output that is not tracked (`.build/`, `__pycache__/`, `.git/`) is
never counted, so the figure is the same from a fresh checkout as from a working
tree.  The same scope and the same exclusions are used by every tool that
reasons about comments.

COMMENT LINE
    One lexer is used for every file in scope, whatever its extension: a line
    whose first non-space characters are `//`, or a line that opens or lies
    inside a `/* ... */` block.  One lexer means two tools cannot disagree.

CLASSIFICATION
    * licence notice  the leading comment paragraphs that carry a licence
                      marker (SPDX, Copyright, License, GNU/GPL, Ritual, public
                      domain, ...).  Exempt: the licences require it.
    * design header   the remaining leading comment paragraphs -- the file's
                      own top-of-file design prose.  In budget, and untouchable:
                      the budget is met by shortening body comments, never by
                      deleting a file's header.
    * body comment    any comment line after the leading comment region.  This
                      is the pool the budget is cut from.

The ceiling is 3,000 (operator order 2026-10-10, the Vulkan campaign), and brevity is still
the rule for anything added.  A change that needs more than the headroom reports
the count it needs rather than deleting a file's header.

Exit: 0 within budget, 1 over, 2 usage.
"""

import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_TREE = os.path.abspath(os.path.join(HERE, ".."))

sys.path.insert(0, HERE)
# One licence-marker definition for the whole suite: the budget's "licence
# notice" and the lint's notice region cannot disagree about what a notice is.
from license_lint import LICENCE_MARK  # noqa: E402

# The scope, relative to the tree root.
SCOPE = (
    "app/src/main/cpp/code",
    "app/src/main/java/com/provenanceef2",
    "app/make_zpak.py",
)
# A path is skipped when any of these components appears in it: vendored or
# out-of-scope tool trees, and generated output that is not tracked.
SKIP_COMPONENTS = ("thirdparty", "ommap", "lcc", ".build", "__pycache__", ".git")

# Generated sources are not editorial: the shader sources under
# renderervulkan/shaders/ are written by tools/vk_shaders_from_gles3.py, and the
# only comment lines they carry are the licence notice (exempt by definition)
# and the splicing directives their build reads.  Counting them would measure
# the generator, not the prose the ceiling is about.
GENERATED = (
    "app/src/main/cpp/code/renderervulkan/shaders",
    "app/src/main/cpp/code/renderervulkan/vk_shaders_embedded.h",
    "app/src/main/cpp/code/renderervulkan/vk_shaders_embedded3d.h",
)

TREE = DEFAULT_TREE

# The editorial ceiling: body comments plus design headers, licence notices
# exempt.  2,000 was the old ceiling; it was lifted to 2,200 and this constant is
# the single source of truth, read back with --print-budget rather than carried
# as a copy elsewhere.  It is a chosen headroom, not a measured quantity: it
# records how much design prose the tree is allowed.
EDITORIAL_BUDGET = 3000


def comment_indexes(text):
    """The set of line indexes that are comment lines, one lexer for all files."""
    out = set()
    in_block = False
    for i, line in enumerate(text.splitlines()):
        s = line.strip()
        if in_block:
            out.add(i)
            if "*/" in s:
                in_block = False
            continue
        if s.startswith("//"):
            out.add(i)
        elif s.startswith("/*"):
            out.add(i)
            if "*/" not in s:
                in_block = True
    return out


def leading_end(text):
    """One past the leading run of blank and comment lines (0 when the file
    opens with code)."""
    lines = text.splitlines()
    j = 0
    in_block = False
    started = False
    while j < len(lines):
        s = lines[j].strip()
        if in_block:
            started = True
            if "*/" in s:
                in_block = False
            j += 1
            continue
        if s == "":
            j += 1
            continue
        if s.startswith("//") or s.startswith("#"):
            started = True
            j += 1
            continue
        if s.startswith("/*"):
            started = True
            if "*/" not in s:
                in_block = True
            j += 1
            continue
        break
    return j if started else 0


def classify(text, indexes):
    """(licence lines, header lines, body lines)."""
    end = leading_end(text)
    lines = text.splitlines()
    licence = header = 0
    para = []
    for i in range(0, end):
        if i in indexes:
            para.append(i)
            continue
        if lines[i].strip() == "" and para:
            if any(LICENCE_MARK.search(lines[k]) for k in para):
                licence += len(para)
            else:
                header += len(para)
            para = []
    if para:
        if any(LICENCE_MARK.search(lines[k]) for k in para):
            licence += len(para)
        else:
            header += len(para)
    body = len([i for i in indexes if i >= end])
    return licence, header, body


def walk(rel):
    path = os.path.join(TREE, rel)
    if os.path.isfile(path):
        yield path
        return
    for dirpath, dirnames, filenames in os.walk(path):
        dirnames[:] = [d for d in dirnames if d not in SKIP_COMPONENTS]
        parts = dirpath.replace(os.sep, "/").split("/")
        if any(p in SKIP_COMPONENTS for p in parts):
            continue
        for fn in sorted(filenames):
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, TREE).replace(os.sep, "/")
            if rel in GENERATED or any(rel.startswith(g + "/") for g in GENERATED):
                continue
            yield full


def measure():
    files = licence = header = body = 0
    for rel in SCOPE:
        for path in walk(rel):
            try:
                data = open(path, "rb").read()
            except OSError:
                continue
            if b"\x00" in data[:8192]:
                continue
            text = data.decode("utf-8", "surrogateescape")
            indexes = comment_indexes(text)
            if not indexes:
                continue
            files += 1
            lic, hdr, bdy = classify(text, indexes)
            licence += lic
            header += hdr
            body += bdy
    return {"files": files, "licence": licence, "header": header, "body": body,
            "editorial": header + body}


def main():
    global TREE
    ap = argparse.ArgumentParser()
    ap.add_argument("--tree", default=DEFAULT_TREE, help="the project root")
    ap.add_argument("--limit", type=int, default=EDITORIAL_BUDGET)
    ap.add_argument("--print-budget", action="store_true",
                    help="print the editorial ceiling and exit")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    if args.print_budget:
        print(EDITORIAL_BUDGET)
        return 0

    TREE = os.path.abspath(args.tree)

    m = measure()
    print("comment_budget: %d files in scope" % m["files"])
    print("  design headers          : %d (in budget, never deleted)" % m["header"])
    print("  body comments           : %d (in budget, the cut pool)" % m["body"])
    print("  editorial total         : %d (budget %d)" % (m["editorial"], args.limit))
    print("  licence notices         : %d (exempt, reported separately)" % m["licence"])
    if m["editorial"] > args.limit:
        print("comment_budget: RED -- editorial comments over budget")
        return 1
    print("comment_budget: GREEN")
    return 0


if __name__ == "__main__":
    sys.exit(main())
