#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
inventory.py -- full markdown + code inventory for the EF2 Android port.

Outputs (under docs/notes/):
    inventory_md.tsv        every .md outside the inner engine repo
    inventory_code.tsv      every source/header under app/src/main/cpp/code
    INVENTORY.md            human-readable summary (both sides, one file)

Code "compiled" is derived from app/src/main/cpp/build/build.ninja (the port's
actual build graph); everything else under code/ is listed as not-compiled so a
workstream can decide whether it is dead residue or deliberately excluded.
"""
import os
import re
import subprocess
import sys
import time
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + ""
INNER = os.path.join(ROOT, "app/src/main/cpp")
CODE = os.path.join(INNER, "code")
NOTES = os.path.join(ROOT, "docs/notes")
BUILD_NINJA = os.path.join(INNER, "build/build.ninja")

MD_SKIP_DIRS = {".git", "openmohaa", "node_modules", ".gradle"}
SRC_EXT = {".c", ".cpp", ".cc", ".cxx"}
HDR_EXT = {".h", ".hpp", ".inl"}


def walk(root, skip_dirs):
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in skip_dirs]
        for fn in filenames:
            yield os.path.join(dirpath, fn)


def rel(p):
    return os.path.relpath(p, ROOT)


def git_tracked(paths):
    """Return the set of repo-relative paths tracked by git (outer repo)."""
    try:
        out = subprocess.run(
            ["git", "-C", ROOT, "ls-files", "-z"],
            capture_output=True, text=True, timeout=120, check=False).stdout
        return set(x for x in out.split("\0") if x)
    except Exception:
        return set()


def categorize_md(relpath):
    p = relpath
    if p.startswith("docs/notes/") and "SESSION_" in p:
        return "session-record"
    if "/golden/" in p or "/oracle/" in p or p.startswith("personal/oracle/"):
        return "oracle"
    if p.startswith("docs/agent_reports/"):
        return "workstream-report"
    if p.startswith("docs/notes/"):
        return "reference-or-plan"
    if p.startswith("docs/"):
        return "work-doc"
    return "root-doc"


def main():
    os.makedirs(NOTES, exist_ok=True)
    ts = time.strftime("%Y-%m-%d %H:%M")

    # ---------------- markdown ----------------
    tracked = git_tracked(None)
    md_rows = []
    for path in walk(ROOT, MD_SKIP_DIRS):
        if not path.lower().endswith(".md"):
            continue
        r = rel(path)
        st = os.stat(path)
        md_rows.append({
            "path": r,
            "bytes": st.st_size,
            "mtime": time.strftime("%Y-%m-%d", time.localtime(st.st_mtime)),
            "git": "tracked" if r in tracked else "untracked/gitignored",
            "category": categorize_md(r),
        })
    md_rows.sort(key=lambda x: x["path"])

    with open(os.path.join(NOTES, "inventory_md.tsv"), "w") as f:
        f.write("path\tbytes\tmtime\tgit\tcategory\n")
        for r in md_rows:
            f.write("%s\t%d\t%s\t%s\t%s\n" % (r["path"], r["bytes"], r["mtime"], r["git"], r["category"]))

    # ---------------- code ----------------
    ninja_src = set()
    if os.path.exists(BUILD_NINJA):
        with open(BUILD_NINJA, errors="replace") as f:
            txt = f.read()
        # NOTE: the extension alternation must list the long forms first, or ".cpp"
        # matches the single-letter ".c" branch and every source looks like a .c.
        for m in re.finditer(r"(code/[^\s:]+\.(?:cpp|cc|cxx|c))(?![A-Za-z0-9])", txt):
            ninja_src.add(m.group(1))

    # include-name set from EVERY source/header under code/ (for orphan-header detection);
    # using only the compiled set would mislabel headers included by other headers.
    included = set()
    inc_re = re.compile(r'#\s*include\s*[<"]([^">]+)[">]')
    for base, _dirs, files in os.walk(CODE):
        for fn in files:
            if os.path.splitext(fn)[1].lower() not in (SRC_EXT | HDR_EXT):
                continue
            try:
                with open(os.path.join(base, fn), errors="replace") as f:
                    for line in f:
                        mm = inc_re.search(line)
                        if mm:
                            included.add(os.path.basename(mm.group(1)))
            except Exception:
                pass

    code_rows = []
    for path in walk(CODE, set()):
        ext = os.path.splitext(path)[1].lower()
        if ext not in SRC_EXT and ext not in HDR_EXT:
            continue
        r = rel(path)
        inner_rel = os.path.relpath(path, INNER)
        st = os.stat(path)
        if ext in SRC_EXT:
            compiled = "yes" if inner_rel in ninja_src else "NO"
            kind = "source"
            orphan = ""
        else:
            compiled = "n/a"
            kind = "header"
            orphan = "ORPHAN" if os.path.basename(path) not in included else ""
        code_rows.append({
            "path": r,
            "ext": ext,
            "kind": kind,
            "compiled": compiled,
            "orphan": orphan,
            "bytes": st.st_size,
            "dir": os.path.dirname(r),
        })
    code_rows.sort(key=lambda x: x["path"])

    with open(os.path.join(NOTES, "inventory_code.tsv"), "w") as f:
        f.write("path\text\tkind\tcompiled\torphan_header\tbytes\n")
        for r in code_rows:
            f.write("%s\t%s\t%s\t%s\t%s\t%d\n" % (r["path"], r["ext"], r["kind"], r["compiled"], r["orphan"], r["bytes"]))

    # ---------------- summaries ----------------
    by_cat = Counter(r["category"] for r in md_rows)
    by_git = Counter(r["git"] for r in md_rows)

    out = []
    if True:
        f = out
        f.append("# INVENTORY (generated %s)\n\n" % ts)
        f.append("Two censuses in one file.  MD: every `.md` outside `app/src/main/cpp/` (the inner\n")
        f.append("engine repo).  CODE: every source and\n")
        f.append("header under `app/src/main/cpp/code/`.  Machine-readable: `inventory_md.tsv` and\n")
        f.append("`inventory_code.tsv`; role index: `docs/WORK_INDEX.md`.\n\n")
        f.append("## MD files\n\n")
        f.append("All `.md` outside `app/src/main/cpp/`.  Total: **%d** markdown files.\n\n" % len(md_rows))
        f.append("| category | count |\n|---|---|\n")
        for k, v in sorted(by_cat.items(), key=lambda x: -x[1]):
            f.append("| %s | %d |\n" % (k, v))
        f.append("\n| git state | count |\n|---|---|\n")
        for k, v in sorted(by_git.items(), key=lambda x: -x[1]):
            f.append("| %s | %d |\n" % (k, v))
        f.append("\n## Files\n\n| path | bytes | mtime | git | category |\n|---|---|---|---|---|\n")
        for r in md_rows:
            f.append("| `%s` | %d | %s | %s | %s |\n" % (r["path"], r["bytes"], r["mtime"], r["git"], r["category"]))

    srcs = [r for r in code_rows if r["kind"] == "source"]
    hdrs = [r for r in code_rows if r["kind"] == "header"]
    uncompiled = [r for r in srcs if r["compiled"] == "NO"]
    orphans = [r for r in hdrs if r["orphan"] == "ORPHAN"]
    by_dir = Counter(r["dir"] for r in srcs)
    base_dupes = defaultdict(list)
    for r in code_rows:
        base_dupes[os.path.basename(r["path"])].append(r["path"])
    dupes = {k: v for k, v in base_dupes.items() if len(v) > 1}

    if True:
        f = out
        f.append("\n## CODE FILES\n")
        f.append("# CODE FILE INVENTORY (generated %s)\n" % ts)
        f.append("Every `.c/.cpp/.cc/.cxx/.h/.hpp/.inl` under `app/src/main/cpp/code/` "
                "(the port + the OpenMoHAA base). Machine-readable: `inventory_code.tsv`. "
                "`compiled=yes` is derived from the live `build/build.ninja` graph.\n")
        f.append("| metric | count |\n|---|---|\n")
        f.append("| source files (`compiled=yes`) | %d |\n" % (len(srcs) - len(uncompiled)))
        f.append("| source files (`NO` -- not in the build graph) | %d |\n" % len(uncompiled))
        f.append("| headers | %d |\n" % len(hdrs))
        f.append("| orphan headers (basename included nowhere) | %d |\n" % len(orphans))
        f.append("| duplicate basenames | %d |\n" % len(dupes))
        f.append("| total | %d |\n" % len(code_rows))
        f.append("\n## Sources by directory\n\n| directory | files |\n|---|---|\n")
        for k, v in sorted(by_dir.items(), key=lambda x: -x[1]):
            f.append("| `%s` | %d |\n" % (k, v))
        f.append("\n## Sources NOT in the build graph (%d)\n\n" % len(uncompiled))
        for r in uncompiled:
            f.append("- `%s`\n" % r["path"])
        f.append("\n## Orphan headers (%d)\n\n" % len(orphans))
        for r in orphans:
            f.append("- `%s`\n" % r["path"])
        f.append("\n## Duplicate basenames (%d)\n\n" % len(dupes))
        for k, v in sorted(dupes.items()):
            f.append("- `%s`: %s\n" % (k, ", ".join("`%s`" % x for x in sorted(v))))

    with open(os.path.join(NOTES, "INVENTORY.md"), "w") as fh:
        fh.write("".join(out))

    print("md files: %d" % len(md_rows))
    print("code files: %d (sources %d, compiled %d, uncompiled %d; headers %d, orphan %d; dup basenames %d)"
          % (len(code_rows), len(srcs), len(srcs) - len(uncompiled), len(uncompiled), len(hdrs), len(orphans), len(dupes)))


if __name__ == "__main__":
    sys.exit(main())
