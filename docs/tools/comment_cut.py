#!/usr/bin/env python3
"""Comment cutter + token-diff prover for the port's own C/C++ sources.

Modes:
  count    report comment lines / code lines per root
  analyze  histogram of comment block sizes and keep-predicate survivors
  cut      remove comments, keeping only load-bearing ones (<= --cap lines)
  tokens   print the token stream (string literals + non-comment, non-space bytes)
           of one file, for the comments-only proof

Nothing but comment bytes is ever removed; a whole-line comment takes its own
indentation and newline with it.
"""
import argparse, os, re, sys

C_EXT = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".hh", ".inl", ".inc"}

KEEP_RE = re.compile(
    r"(?i)("
    r"\bMUST\b|\bMUST NOT\b|\bNEVER\b|\bINVARIANT\b|\bTRAP\b|"
    r"\bDELIBERATE(?:LY)?\b|\bSANCTIONED\b|\bPORT-ONLY\b|\bDO NOT\b|"
    r"\bONE WRITER\b|\bSINGLE WRITER\b|\bOWNERSHIP\b|\bOWNED BY\b|"
    r"\bORDER MATTERS\b|\bORDERING\b|\bMUST RUN\b|\bLATCH\b|\bSENTINEL\b|"
    r"\bRACE\b|\bDEADLOCK\b|\bBY CONSTRUCTION\b|\bDUPLICATE CALL\b|"
    r"\bNOT [A-Z_]{3,}\b"
    r")"
)
MAX_KEEP_BLOCK = 3   # a surviving comment is at most this many lines


def lex_comments(data):
    i, n = 0, len(data)
    out = []
    while i < n:
        c = data[i:i + 1]
        two = data[i:i + 2]
        if two == b"//":
            s = i
            i += 2
            while i < n:
                if data[i:i + 1] == b"\n":
                    break
                if data[i:i + 1] == b"\\" and data[i + 1:i + 2] in (b"\n", b"\r"):
                    i += 1
                    if data[i:i + 1] == b"\r":
                        i += 1
                    if data[i:i + 1] == b"\n":
                        i += 1
                    continue
                i += 1
            out.append((s, i, "line"))
        elif two == b"/*":
            s = i
            i += 2
            while i < n and data[i:i + 2] != b"*/":
                i += 1
            i = min(n, i + 2)
            out.append((s, i, "block"))
        elif c in (b'"', b"'"):
            q = c
            i += 1
            while i < n:
                if data[i:i + 1] == b"\\":
                    i += 2
                    continue
                if data[i:i + 1] == q:
                    i += 1
                    break
                i += 1
        elif c == b"R" and data[i + 1:i + 2] == b'"':
            j = i + 2
            d = b""
            while j < n and data[j:j + 1] != b"(" and len(d) < 16:
                d += data[j:j + 1]
                j += 1
            if data[j:j + 1] == b"(":
                close = b")" + d + b'"'
                k = data.find(close, j + 1)
                i = n if k < 0 else k + len(close)
            else:
                i += 1
        else:
            i += 1
    return out


def line_bounds(data, s, e):
    ls = data.rfind(b"\n", 0, s) + 1
    le = data.find(b"\n", e)
    le = len(data) if le < 0 else le + 1
    return ls, le


def whole_line(data, s, e):
    ls, le = line_bounds(data, s, e)
    return data[ls:s].strip() == b"" and data[e:le].strip() == b""


def spanned_lines(data, s, e):
    ls, le = line_bounds(data, s, e)
    return data[ls:le].count(b"\n") or 1


def collect(root, skip_thirdparty=True):
    files = []
    for dp, dn, fn in os.walk(root):
        dn[:] = [d for d in dn if d not in ("__pycache__", ".git")]
        if skip_thirdparty:
            dn[:] = [d for d in dn if d not in ("thirdparty", "gamespy")]
        for f in fn:
            if os.path.splitext(f)[1].lower() in C_EXT:
                files.append(os.path.join(dp, f))
    return sorted(files)


def code_lines(data, cs):
    stripped = bytearray(data)
    for (s, e, k) in cs:
        for j in range(s, e):
            if stripped[j] not in (10, 13):
                stripped[j] = 32
    return sum(1 for ln in bytes(stripped).split(b"\n") if ln.strip())


def count_mode(roots):
    for root in roots:
        tf = cl = kl = wl = 0
        for p in collect(root):
            data = open(p, "rb").read()
            cs = lex_comments(data)
            tf += 1
            kl += code_lines(data, cs)
            for (s, e, k) in cs:
                cl += spanned_lines(data, s, e)
                if whole_line(data, s, e):
                    wl += spanned_lines(data, s, e)
        print("%-48s files=%5d commentLines=%6d wholeLine=%6d codeLines=%7d" %
              (root, tf, cl, wl, kl))


def analyze(root):
    hist = {}
    surv_lines = 0
    surv_n = 0
    total_comments = 0
    for p in collect(root):
        data = open(p, "rb").read()
        for (s, e, k) in lex_comments(data):
            n = spanned_lines(data, s, e)
            hist[n] = hist.get(n, 0) + 1
            total_comments += 1
            if KEEP_RE.search(data[s:e].decode("latin1")) and n <= MAX_KEEP_BLOCK:
                surv_n += 1
                surv_lines += n
    print("files:", len(collect(root)), "comments:", total_comments)
    print("survivors (marker + <=%d lines): %d comments / %d lines" %
          (MAX_KEEP_BLOCK, surv_n, surv_lines))
    for k in sorted(hist):
        print("  block %2d lines: %6d comments" % (k, hist[k]))


def tokens(data):
    ct = bytearray(data)
    for (s, e, k) in lex_comments(data):
        for j in range(s, e):
            ct[j] = 32
    out = []
    i, n = 0, len(ct)
    while i < n:
        c = ct[i:i + 1]
        if c in (b'"', b"'"):
            q = c
            j = i + 1
            while j < n:
                if ct[j:j + 1] == b"\\":
                    j += 2
                    continue
                if ct[j:j + 1] == q:
                    j += 1
                    break
                j += 1
            out.append(bytes(ct[i:j]))
            i = j
        elif c.isspace():
            i += 1
        else:
            out.append(c)
            i += 1
    return out


def cut_mode(root, cap):
    """Remove comments from every file; keep load-bearing ones up to cap lines."""
    kept_total = 0
    cand = []
    for p in collect(root):
        data = open(p, "rb").read()
        for (s, e, k) in lex_comments(data):
            text = data[s:e]
            n = spanned_lines(data, s, e)
            if KEEP_RE.search(text.decode("latin1")) and n <= MAX_KEEP_BLOCK:
                cand.append((p, s, e, n, text))
    # cap: keep the shortest, most-specific survivors first
    cand.sort(key=lambda c: (c[3], len(c[4])))
    keepset = set()
    for (p, s, e, n, text) in cand:
        if kept_total + n > cap:
            continue
        keepset.add((p, s, e))
        kept_total += n
    changed = 0
    for p in collect(root):
        data = open(p, "rb").read()
        comments = lex_comments(data)
        events = []
        for (s, e, k) in comments:
            events.append((s, e, (p, s, e) in keepset))
        out = bytearray()
        pos = 0
        for (s, e, keepit) in events:
            if keepit:
                continue
            if whole_line(data, s, e):
                ls, le = line_bounds(data, s, e)
                out += data[pos:ls]
                pos = le
            else:
                out += data[pos:s]
                out += b" "
                pos = e
        out += data[pos:]
        if bytes(out) != data:
            open(p, "wb").write(bytes(out))
            changed += 1
    print("kept %d comment lines; rewrote %d files" % (kept_total, changed))


def archive_mode(root, out_path):
    import json
    n = 0
    with open(out_path, "wb") as fh:
        for p in collect(root):
            data = open(p, "rb").read()
            for (s, e, k) in lex_comments(data):
                ls, le = line_bounds(data, s, e)
                text = data[s:e]
                rec = json.dumps([os.path.relpath(p), data[:s].count(b"\n") + 1,
                                  data[:e].count(b"\n") + 1,
                                  text.decode("latin1")], ensure_ascii=False)
                fh.write(rec.encode("utf-8") + b"\n")
                n += 1
    print("archived %d comments -> %s" % (n, out_path))


def prove_mode(root, rev):
    import subprocess
    bad = []
    files = collect(root)
    for p in files:
        rel = os.path.relpath(p)
        try:
            old = subprocess.check_output(["git", "show", "%s:%s" % (rev, rel)])
        except subprocess.CalledProcessError:
            continue
        new = open(p, "rb").read()
        if tokens(old) != tokens(new):
            bad.append(rel)
    print("proved %d files; token mismatches: %d" % (len(files), len(bad)))
    for b in bad[:40]:
        print("  MISMATCH", b)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=("count", "analyze", "cut", "tokens", "archive", "prove"))
    ap.add_argument("paths", nargs="+")
    ap.add_argument("--cap", type=int, default=2000)
    ap.add_argument("--out")
    ap.add_argument("--rev", default="HEAD")
    a = ap.parse_args()
    if a.mode == "count":
        count_mode(a.paths)
    elif a.mode == "analyze":
        for p in a.paths:
            analyze(p)
    elif a.mode == "cut":
        for p in a.paths:
            cut_mode(p, a.cap)
    elif a.mode == "archive":
        for p in a.paths:
            archive_mode(p, a.out)
    elif a.mode == "prove":
        for p in a.paths:
            prove_mode(p, a.rev)
    elif a.mode == "tokens":
        sys.stdout.buffer.write(b"\x00".join(tokens(open(a.paths[0], "rb").read())))
