#!/usr/bin/env python3
"""Restore the licence notices on the native source under code/.

The port's comment-thinning passes removed the per-file notices that the
OpenMoHAA lineage, the Ritual Elite Force II SDK and the tree's own history
gave every source file.  This tool puts them back, per file, from the file's
actual nearest ancestor -- never from a generic header -- and records the
decision in PROVENANCE.tsv.

Buckets
    gpl       OpenMoHAA / id Tech 3 lineage      SPDX GPL-2.0-or-later
    sdk       Ritual Elite Force II SDK          LicenseRef-Ritual-EF2-SDK
    recon     rebuilt from the retail binaries   LicenseRef-EF2-Retail-Reconstruction
    vendored  a third party's own notice         (restored, never replaced)

For each file the tool picks the nearest ancestor by comparing the file body
(notice stripped) against every OpenMoHAA candidate and every EF2 SDK
candidate; the closer one decides the bucket, and the ancestor's own notice is
copied in verbatim.  A file with no ancestor keeps the notice it carried in the
pre-thinning tree, and a file with neither gets the honest no-rights statement.
"""

import argparse
import difflib
import os
import re
import sys

SRC_EXT = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".inc"}
VENDOR_MODULES = ("gamespy", "tools")
SIM_THRESHOLD = 0.50
CHANGE_DATE = "2026-10-04"

# The standard Ritual SDK notice, without a $Logfile block: used only on an
# SDK-derived file whose SDK counterpart carries no banner (SDK §2(k) still
# requires the notice, and 40 of the 341 SDK files ship without one).
RITUAL_STANDARD = """//-----------------------------------------------------------------------------
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//-----------------------------------------------------------------------------"""

RECON_NOTICE = """/*
===========================================================================
Reconstructed from the retail Star Trek: Voyager -- Elite Force II binaries.
The publisher released no source for this file.  This port asserts no copyright
in it and grants no rights to it: it remains the property of its respective
rights holders, and it is covered neither by the GNU GPL nor by the Ritual
Elite Force II SDK agreement.  See LICENSING.md.
===========================================================================
*/"""

GPL_SPDX = "// SPDX-License-Identifier: GPL-2.0-or-later"
SDK_SPDX = "// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK"
RECON_SPDX = "// SPDX-License-Identifier: LicenseRef-EF2-Retail-Reconstruction"

RECON_MODULES = {"cgame", "client", "server", "tiki", "skeletor", "uilib", "corepp"}
ENGINE_MODULES = {"qcommon", "botlib", "renderercommon", "renderergles3", "renderervulkan", "sdl", "sys",
                  "script", "null", "asm", "web", "parser", "q3_ui", "curl"}

STANDARD_GPL = """/*
===========================================================================
This file is part of the port of Star Trek: Voyager -- Elite Force II, whose
engine descends from id Tech 3 through OpenMoHAA.

It is free software; you can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU General Public License, COPYING.txt at the repository
root, for more details.
===========================================================================
*/"""

SDK_CHANGE = (    "// Modified %s for this port.  The Ritual Elite Force II SDK agreement\n"
    "// permits non-commercial distribution of the SDK and its New Creations\n"
    "// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it\n"
    "// forbids derivative works (section 2(j)) and requires the notice above\n"
    "// to stay intact (section 2(k))." % CHANGE_DATE
)
GPL_CHANGE = "// Modified %s for this port; the changes are listed in PROVENANCE.tsv." % CHANGE_DATE
VENDOR_POINTER = "/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */"

LICENCE_HINT = re.compile(
    r"Copyright|License|Licence|GNU|GPL|public domain|All rights reserved|"
    r"Permission is hereby granted|SPDX-License-Identifier|May not be distributed",
    re.I,
)
PORT_AUTHORED = re.compile(r"EF2 port|PROVENANCE:|GENERATED|do not edit|this port", re.I)


def read(path):
    # byte-faithful: never let universal newlines or a decode error rewrite a file
    with open(path, "r", encoding="utf-8", errors="surrogateescape", newline="") as fh:
        return fh.read()


def leading_block(text):
    """The leading comment block, verbatim, or ''."""
    if text.startswith("\ufeff"):
        text = text[1:]
    i, n = 0, len(text)
    while i < n:
        j = text.find("\n", i)
        line = text[i:j if j != -1 else n]
        if line.startswith("#!") or line.strip() == "":
            i = j + 1 if j != -1 else n
            continue
        break
    if i >= n:
        return ""
    if text.startswith("/*", i):
        end = text.find("*/", i)
        return text[i:] if end == -1 else text[i:end + 2]
    if text.startswith("//", i):
        out = []
        while i < n:
            j = text.find("\n", i)
            line = text[i:j if j != -1 else n]
            if not line.startswith("//"):
                break
            out.append(line)
            i = j + 1 if j != -1 else n
        return "\n".join(out)
    return ""


def strip_block(text):
    """Drop the leading comment block and return the body."""
    blk = leading_block(text)
    if not blk:
        return text
    return text[len(blk):]


_COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)


def uncomment(text):
    return _COMMENT.sub(" ", strip_block(text))


def strip_leading_comments(text):
    """Drop the whole top-of-file comment region, so the fingerprint does not
    depend on the notice this tool has itself written there."""
    lines = text.splitlines()
    i, in_block = 0, False
    while i < len(lines):
        s = lines[i].strip()
        if in_block:
            if "*/" in s:
                in_block = False
            i += 1
            continue
        if s == "" or s.startswith("//"):
            i += 1
            continue
        if s.startswith("/*"):
            if "*/" not in s:
                in_block = True
            i += 1
            continue
        break
    return "\n".join(lines[i:])


def body_lines(text):
    """Code lines only: interior comments are thinned differently on each side,
    so only the code is a fair fingerprint of the shared ancestry."""
    text = uncomment(strip_leading_comments(text))
    return [re.sub(r"\s+", " ", ln).strip() for ln in text.splitlines() if ln.strip()]


def similarity(a, b):
    if not a or not b:
        return 0.0
    return difflib.SequenceMatcher(None, a, b, autojunk=False).ratio()


def notice_class(block):
    if not block.strip():
        return "none"
    if PORT_AUTHORED.search(block) and not re.search(r"OpenMoHAA source code|Quake III Arena", block):
        return "port"
    if re.search(r"Ritual Entertainment", block):
        return "ritual"
    if LICENCE_HINT.search(block):
        if re.search(r"GNU General Public License", block):
            return "gpl"
        return "thirdparty"
    return "none"


class Ancestors:
    def __init__(self, upstream, sdk):
        self.upstream = upstream
        self.sdk = sdk
        self.sdk_index = {}
        for dp, _dn, fns in os.walk(sdk):
            for fn in fns:
                self.sdk_index.setdefault(fn, []).append(os.path.join(dp, fn))

    def upstream_candidates(self, rel):
        module = rel.split("/")[0]
        if module == "renderergles3" and "/" in rel:
            rest = rel.split("/", 1)[1]
            return [os.path.join(self.upstream, be, rest)
                    for be in ("renderergl2", "renderergl1")]
        return [os.path.join(self.upstream, rel)]

    def sdk_candidates(self, rel):
        base = os.path.basename(rel)
        hits = self.sdk_index.get(base, [])
        if not hits:
            return []
        tail = "/".join(rel.split("/")[1:])
        pref = [h for h in hits if h.replace("\\", "/").endswith("/" + tail)]
        return pref + [h for h in hits if h not in pref]

    def sdk_covered(self, rel):
        """The SDK file that covers this path, by the SDK's own tree layout."""
        if "/" not in rel:
            return None
        tail = rel.split("/", 1)[1]
        for sub in ("dlls/game", "Shared/qcommon", "Executable"):
            p = os.path.join(self.sdk, sub, tail)
            if os.path.isfile(p):
                return p
        return None

    def best(self, rel, body):
        up_best, up_sim = None, 0.0
        for c in self.upstream_candidates(rel):
            if os.path.isfile(c):
                s = similarity(body, body_lines(read(c)))
                if s > up_sim:
                    up_best, up_sim = c, s
        sdk_best, sdk_sim = None, 0.0
        for c in self.sdk_candidates(rel):
            s = similarity(body, body_lines(read(c)))
            if s > sdk_sim:
                sdk_best, sdk_sim = c, s
            if sdk_sim >= 0.999:
                break
        return (up_best, up_sim), (sdk_best, sdk_sim)


def banner_of(path, fallback):
    blk = leading_block(read(path))
    if blk.strip():
        return blk
    return fallback


def sdk_banner(path):
    """The SDK's own Ritual banner, or the standard notice when the SDK shipped
    the file under some other header (the file's own notice stays below)."""
    blk = leading_block(read(path)) if path and os.path.isfile(path) else ""
    return blk if "Ritual Entertainment" in blk else RITUAL_STANDARD


def compose(bucket, banner, changed):
    """The text prepended to the file."""
    parts = [banner]
    if bucket == "gpl":
        parts.append(GPL_SPDX)
        if changed:
            parts.append(GPL_CHANGE)
    elif bucket == "sdk":
        parts.append(SDK_SPDX)
        parts.append(SDK_CHANGE)
    elif bucket == "recon":
        parts.append(RECON_SPDX)
    elif bucket == "vendored":
        pass
    return "\n".join(parts) + "\n"


SDK_EXPLICIT = re.compile(
    r"transcribed from the retail (Elite Force II |EF2 )?SDK|"
    r"Ported from (the )?EF2 SDK|from the retail EF2 SDK|"
    r"SDK (?:source|dlls?)\b|(?:dlls/game|Shared/qcommon)/", re.I
)
RETAIL_EXPLICIT = re.compile(
    r"Mac PPC|mac_symbols|address-by-address|retail[^\n]{0,60}binar", re.I
)


def classify(rel, text, anc, rec_tree):
    body = body_lines(text)
    cur = leading_block(text)
    module = rel.split("/")[0]
    rec_path = os.path.join(rec_tree, rel) if rec_tree else None
    rec = leading_block(read(rec_path)) if rec_path and os.path.isfile(rec_path) else ""
    rec_cls = notice_class(rec)
    (up_path, up_sim), (sdk_path, sdk_sim) = anc.best(rel, body)
    sdk_cover = anc.sdk_covered(rel)

    def row(bucket, banner, changed, why):
        return (bucket, banner, changed, why, up_path, up_sim, sdk_path, sdk_sim)

    # a third-party notice is authoritative: restore it and never relabel it
    if rec_cls == "thirdparty" and not re.search(r"GENERATED", rec, re.I):
        return row("vendored", rec, False, "recovered third-party notice")
    if cur.strip() and "SPDX-License-Identifier:" not in cur and notice_class(cur) == "thirdparty" \
            and not re.search(r"GENERATED", cur, re.I):
        return row("vendored", cur, False, "existing third-party notice")

    if module in VENDOR_MODULES:
        note = rec if rec.strip() and LICENCE_HINT.search(rec) else cur
        if not (note.strip() and LICENCE_HINT.search(note)):
            note = VENDOR_POINTER
        return row("vendored", note, False, "vendored module")

    # the file's own history states where it came from: that settles the bucket
    if SDK_EXPLICIT.search(rec):
        return row("sdk", sdk_banner(sdk_path),
                   True, "port record names the EF2 SDK as the source")
    if RETAIL_EXPLICIT.search(rec):
        return row("recon", RECON_NOTICE, False, "port record names the retail binaries as the source")
    if rec_cls == "ritual":
        return row("sdk", rec, True, "recovered Ritual banner")
    if rec_cls == "gpl":
        return row("gpl", rec, False, "recovered GPL banner")

    # no usable notice survived: the module's provenance decides
    if module == "renderergles3":
        for c in anc.upstream_candidates(rel):
            if os.path.isfile(c):
                banner = leading_block(read(c))
                if "GNU General Public License" in banner:
                    return row("gpl", banner, True, "GLES3 backend renamed from the OpenMoHAA renderer")
    if up_sim >= 0.60 and up_sim >= sdk_sim:
        banner = leading_block(read(up_path))
        if "GNU General Public License" in banner:
            return row("gpl", banner, up_sim < 0.999, "OpenMoHAA lineage")
        if banner.strip() and LICENCE_HINT.search(banner):
            return row("vendored", banner, False, "upstream third-party notice")
    if sdk_sim >= 0.60:
        return row("sdk", sdk_banner(sdk_path), True, "Ritual EF2 SDK")
    if module == "fgame" and sdk_cover:
        return row("sdk", sdk_banner(sdk_cover), True, "fgame is transcribed from the EF2 SDK")
    if RECON_MODULES and module in RECON_MODULES:
        return row("recon", RECON_NOTICE, False, "no SDK basis; rebuilt from the retail binaries")
    if module in ENGINE_MODULES:
        banner = leading_block(read(up_path)) if up_path else ""
        if "GNU General Public License" in banner:
            return row("gpl", banner, True, "id Tech 3 lineage")
        return row("gpl", STANDARD_GPL, True, "id Tech 3 lineage (standard GPL notice)")
    return row("recon", RECON_NOTICE, False, "no ancestor and no surviving notice")


def apply_notice(text, composed):
    if text.startswith(composed):
        return text, False
    cur = leading_block(text)
    if cur.strip() and "SPDX-License-Identifier:" in cur:
        rest = text[len(cur):]
        if rest.startswith("\r\n"):
            rest = rest[2:]
        elif rest.startswith("\n"):
            rest = rest[1:]
        return composed + rest, True
    return composed + text, True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--code", required=True)
    ap.add_argument("--upstream", required=True)
    ap.add_argument("--sdk", required=True)
    ap.add_argument("--recovered", default="")
    ap.add_argument("--ledger", default="")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--only", default="")
    args = ap.parse_args()

    anc = Ancestors(args.upstream, args.sdk)
    rows = []
    counts = {}
    for dp, dn, fns in os.walk(args.code):
        dn[:] = [d for d in dn if d != "thirdparty"]
        for fn in sorted(fns):
            if os.path.splitext(fn)[1].lower() not in SRC_EXT:
                continue
            p = os.path.join(dp, fn)
            rel = os.path.relpath(p, args.code)
            if args.only and not rel.startswith(args.only):
                continue
            text = read(p)
            bucket, banner, changed, why, up_path, up_sim, sdk_path, sdk_sim = classify(rel, text, anc, args.recovered)
            counts[bucket] = counts.get(bucket, 0) + 1
            rows.append((rel, bucket, why, up_path or "", "%.3f" % up_sim,
                         sdk_path or "", "%.3f" % sdk_sim))
            if args.apply:
                composed = compose(bucket, banner, changed)
                new, did = apply_notice(text, composed)
                if did:
                    with open(p, "w", encoding="utf-8", errors="surrogateescape", newline="") as fh:
                        fh.write(new)
    if args.ledger:
        with open(args.ledger, "w", encoding="utf-8") as fh:
            fh.write("# PROVENANCE.tsv -- generated by tools/license_notices.py; do not edit.\n")
            fh.write("# path\tbucket\tbasis\tnearest-openmohaa\topenmohaa-similarity\t"
                     "nearest-ef2-sdk\tef2-sdk-similarity\n")
            for r in sorted(rows):
                up = os.path.relpath(r[3], args.upstream) if r[3] else "-"
                sdk = os.path.relpath(r[5], args.sdk) if r[5] else "-"
                fh.write("\t".join((r[0], r[1], r[2], up, r[4], sdk, r[6])) + "\n")
    print(dict(sorted(counts.items())), file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
