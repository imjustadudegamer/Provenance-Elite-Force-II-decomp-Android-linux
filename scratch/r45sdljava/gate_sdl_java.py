#!/usr/bin/env python3
"""Census and gate the vendored SDL2 Java android.util.Log emitters.

Every live `Log.<v|d|i|w|e>(...)` statement is wrapped in `if (LogGate.on()) { ... }`
without touching its arguments or its tag.  Comments and string literals are
lexed, so commented-out vendor rows are never counted and never edited.
"""
import argparse
import os
import re
import sys

VENDOR = "app/src/main/java/org/libsdl/app"
EMITTER = re.compile(r"\bLog\.([vdi])\s*\(")  # routine rows only: warn/error stay live
GUARD = "LogGate.on()"


def code_mask(text):
    """Return text with comment/string/char contents blanked (newlines kept)."""
    out = list(text)
    i, n = 0, len(text)
    state = None
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if state is None:
            if c == "/" and nxt == "/":
                state = "line"
                out[i] = out[i + 1] = " "
                i += 2
                continue
            if c == "/" and nxt == "*":
                state = "block"
                out[i] = out[i + 1] = " "
                i += 2
                continue
            if c == '"':
                state = "str"
                out[i] = " "
                i += 1
                continue
            if c == "'":
                state = "chr"
                out[i] = " "
                i += 1
                continue
            i += 1
            continue
        if c == "\n":
            out[i] = "\n"
            if state == "line":
                state = None
            i += 1
            continue
        if state == "block" and c == "*" and nxt == "/":
            out[i] = out[i + 1] = " "
            state = None
            i += 2
            continue
        if state in ("str", "chr") and c == "\\":
            out[i] = " "
            if i + 1 < n:
                out[i + 1] = " "
            i += 2
            continue
        if state == "str" and c == '"':
            out[i] = " "
            state = None
            i += 1
            continue
        if state == "chr" and c == "'":
            out[i] = " "
            state = None
            i += 1
            continue
        out[i] = " "
        i += 1
    assert state is None, "unterminated comment/string"
    return "".join(out)


def statement_span(masked, start):
    """[start_of_Log, end_after_semicolon] for one emitter call, or None."""
    i = masked.index("(", start)
    depth = 0
    j = i
    while j < len(masked):
        if masked[j] == "(":
            depth += 1
        elif masked[j] == ")":
            depth -= 1
            if depth == 0:
                break
        j += 1
    else:
        return None
    if j >= len(masked):
        return None
    k = j + 1
    while k < len(masked) and masked[k] in " \t\r\n":
        k += 1
    if k >= len(masked) or masked[k] != ";":
        return None
    return (start, k + 1)


def line_start(text, idx):
    return text.rindex("\n", 0, idx) + 1


def brace_style(text, idx):
    """K&R ('kr') or Allman ('allman'), read off the nearest enclosing opener."""
    ls = line_start(text, idx)
    for line in reversed(text[:ls].split("\n")[:-1]):
        s = line.rstrip()
        if not s:
            continue
        if s.strip() == "{":
            return "allman"
        if s.endswith("{"):
            return "kr"
    return "kr"


def wrap(text, span, level, tag_unused=None):
    start, end = span
    ls = line_start(text, start)
    indent = re.match(r"[ \t]*", text[ls:start]).group(0)
    stmt = text[start:end]
    body_indent = indent + "    " if "\n" not in stmt else indent
    if brace_style(text, start) == "allman":
        guard = "%sif (%s)\n%s{\n%s%s\n%s}" % (
            indent, GUARD, indent, body_indent, stmt, indent)
    else:
        guard = "%sif (%s) {\n%s%s\n%s}" % (
            indent, GUARD, body_indent, stmt, indent)
    return text[:ls] + guard + text[end:]


def census(root):
    rows = []
    for name in sorted(os.listdir(os.path.join(root, VENDOR))):
        if not name.endswith(".java"):
            continue
        path = os.path.join(root, VENDOR, name)
        text = open(path, encoding="utf-8").read()
        masked = code_mask(text)
        counts = {k: 0 for k in "vdiew"}
        spans = []
        for m in EMITTER.finditer(masked):
            counts[m.group(1)] += 1
            spans.append((m.start(), m.group(1)))
        rows.append((name, counts, spans, text))
    return rows


def unguarded(root):
    """Live emitters not already behind the guard (post-apply check)."""
    bad = []
    for name, counts, spans, text in census(root):
        masked = code_mask(text)
        for pos, level in spans:
            ls = line_start(text, pos)
            head = text[ls:pos]
            prev = text[max(0, ls - 200):ls]
            if GUARD in prev or "isLoggable" in head:
                continue
            bad.append("%s:%d" % (name, text[:pos].count("\n") + 1))
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("root")
    ap.add_argument("mode", choices=["census", "apply", "check"])
    args = ap.parse_args()
    rows = census(args.root)
    total = 0
    if args.mode in ("census", "apply"):
        for name, counts, spans, text in rows:
            n = sum(counts.values())
            total += n
            mix = " ".join("%s=%d" % (k, counts[k]) for k in "vdiew" if counts[k])
            print("%-36s %3d  %s" % (name, n, mix))
        print("TOTAL %d" % total)
    if args.mode == "apply":
        for name, counts, spans, text in rows:
            if not spans:
                continue
            path = os.path.join(args.root, VENDOR, name)
            masked = code_mask(text)
            spans2 = [statement_span(masked, pos) for pos, _ in spans]
            assert all(spans2), name
            for span in sorted(spans2, key=lambda s: -s[0]):
                text = wrap(text, span, "v")
            open(path, "w", encoding="utf-8").write(text)
            print("wrote %s (%d sites)" % (name, len(spans2)))
    if args.mode == "check":
        bad = unguarded(args.root)
        print("unguarded live emitters: %d" % len(bad))
        for b in bad:
            print("  " + b)
        return 1 if bad else 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
