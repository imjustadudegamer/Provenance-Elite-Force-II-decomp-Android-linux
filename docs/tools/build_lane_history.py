#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
build_lane_history.py -- consolidate the per-workstream process archaeology into one file.

The port accumulated one report .md per workstream (and one dispatch brief per workstream).  Those
are process artifacts: their behavioural findings are already folded into
docs/DIVERGENCE_LEDGER_0904.md and docs/agent_reports/w30final_0912_master.tsv, and
the full text stays in git history.  This tool emits a compact digest so the tree keeps a
readable record of *which workstream found what* without carrying the whole pile.

Inputs : docs/agent_reports/*.md, docs/notes/W*_DISPATCH.md, docs/notes/SESSION_*.md,
         docs/notes/W*_STATUS.md, docs/notes/W*_FOLLOWON_WAVES.md
Output : docs/LANE_HISTORY.md

NOTE: this digest is generated BEFORE the files it describes are deleted; running it after a
consolidation only sees the survivors.  The originals stay in git history, so a fuller digest
can always be rebuilt from `git show <commit>:<path>` for the files the consolidation removed.
"""
import os
import re
import glob

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + ""
NOTES = os.path.join(ROOT, "docs/notes")
REPORTS = os.path.join(ROOT, "docs/agent_reports")
OUT = os.path.join(NOTES, "LANE_HISTORY.md")

COMMIT_RE = re.compile(r"\b([0-9a-f]{7,10})\b")
SKIP_NAMES = {"LANE_HISTORY.md", "INDEX.md"}


def digest(path, limit=6, width=240):
    """First heading + the first few substantive lines, one line each."""
    try:
        text = open(path, encoding="utf-8", errors="replace").read()
    except OSError:
        return [], set()
    lines = [l.rstrip() for l in text.split("\n")]
    head = ""
    body = []
    for l in lines:
        s = l.strip()
        if not s:
            continue
        if not head and s.startswith("#"):
            head = s.lstrip("# ").strip()
            continue
        if head is None:
            continue
        if s.startswith("```") or s.startswith("|") or s.startswith("---"):
            continue
        body.append(s)
        if len(body) >= limit:
            break
    commits = set(COMMIT_RE.findall(text))
    return ([head] if head else []) + body, commits


def main():
    groups = [
        ("Workstream reports (docs/agent_reports)", sorted(glob.glob(os.path.join(REPORTS, "*.md")))),
        ("Dispatch briefs (docs/notes)", sorted(glob.glob(os.path.join(NOTES, "*_DISPATCH.md")))),
        ("Session logs (docs/notes)", sorted(glob.glob(os.path.join(NOTES, "SESSION_*.md")))),
        ("Status snapshots (docs/notes)",
         sorted(glob.glob(os.path.join(NOTES, "*_STATUS.md"))) +
         sorted(glob.glob(os.path.join(NOTES, "*_FOLLOWON_WAVES.md")))),
    ]

    out = []
    out.append("# EF2 port - workstream history (consolidated digest)")
    out.append("")
    out.append("Every per-workstream report, dispatch brief, session log and status snapshot the port")
    out.append("produced, reduced to its headline lines.  The full text of each entry is in git")
    out.append("history at the commit that introduced it; the behavioural findings themselves live")
    out.append("in `DIVERGENCE_LEDGER_0904.md` (the narrative record) and")
    out.append("`docs/agent_reports/w30final_0912_master.tsv` (the machine register).")
    out.append("")
    out.append("Regenerate with `python3 docs/tools/build_lane_history.py`.")
    out.append("")

    total = 0
    for title, files in groups:
        files = [f for f in files if os.path.basename(f) not in SKIP_NAMES]
        if not files:
            continue
        out.append("---")
        out.append("")
        out.append("## " + title)
        out.append("")
        for f in files:
            base = os.path.basename(f)
            body, commits = digest(f)
            if not body:
                body = ["(no extractable text)"]
            out.append("### " + base)
            for b in body:
                b = b if len(b) <= 240 else b[:237] + "..."
                out.append("- " + b)
            if commits:
                cs = sorted(commits)
                out.append("- commits mentioned: " + ", ".join(cs[:12]) + (" ..." if len(cs) > 12 else ""))
            out.append("")
            total += 1

    open(OUT, "w", encoding="utf-8").write("\n".join(out) + "\n")
    print("wrote", OUT, "entries:", total)


if __name__ == "__main__":
    main()
