#!/usr/bin/env python3
"""Regenerate and check PROVENANCE.tsv.

PROVENANCE.tsv records, for every native source file, the bucket its notice
declares and the nearest ancestor the decision was taken from.  The corpora it
reads are retail-derived or vendor source and are never published, so they live
on disk only:

  upstream  personal/upstream/openmohaa-main/code          (OpenMoHAA)
  sdk       personal/evidence/ef2-sdk/ef2-sdk              (Ritual EF2 SDK)
  recovered personal/scratch/recovered-precut              (pre-thinning tree)

Each may be overridden with EF2_UPSTREAM, EF2_SDK and EF2_RECOVERED.  A missing
corpus degrades LOUDLY and is not a failure: the gate it feeds is the release
attestation, and there it fails because the corpus is required for a release.

Exit: 0 in agreement (or a loud skip), 1 a disagreement, 2 usage.
"""

import argparse
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
GENERATOR = os.path.join(HERE, "license_notices.py")

DEFAULTS = {
    "upstream": os.path.join(ROOT, "personal", "upstream", "openmohaa-main", "code"),
    "sdk": os.path.join(ROOT, "personal", "evidence", "ef2-sdk", "ef2-sdk"),
    "recovered": os.path.join(ROOT, "personal", "scratch", "recovered-precut", "app", "src", "main", "cpp", "code"),
}


def resolve(name):
    env = os.environ.get("EF2_" + name.upper())
    return env or DEFAULTS[name]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="compare, do not rewrite")
    ap.add_argument("--ledger", default=os.path.join(ROOT, "PROVENANCE.tsv"))
    ap.add_argument("--code", default=os.path.join(ROOT, "app", "src", "main", "cpp", "code"))
    args = ap.parse_args()

    paths = {k: resolve(k) for k in DEFAULTS}
    missing = [k for k, v in paths.items() if not os.path.isdir(v)]
    if missing:
        for k in missing:
            print("provenance_ledger: SKIP -- the %s corpus is absent (%s); "
                  "set EF2_%s to check the ledger" % (k, paths[k], k.upper()))
        return 0

    out = args.ledger
    tmp = None
    if args.check:
        fd, out = tempfile.mkstemp(suffix=".tsv")
        os.close(fd)
        tmp = out
    cmd = [sys.executable, GENERATOR, "--code", args.code, "--upstream", paths["upstream"],
           "--sdk", paths["sdk"], "--recovered", paths["recovered"], "--ledger", out]
    subprocess.run(cmd, check=True)

    if not args.check:
        print("provenance_ledger: wrote %s" % args.ledger)
        return 0

    with open(args.ledger, "r", encoding="utf-8", errors="surrogateescape") as fh:
        have = {p[0]: p[1] for p in (l.rstrip("\n").split("\t") for l in fh
                                     if not l.startswith("#") and l.strip()) if len(p) >= 2}
    with open(tmp, "r", encoding="utf-8", errors="surrogateescape") as fh:
        want = {p[0]: p[1] for p in (l.rstrip("\n").split("\t") for l in fh
                                     if not l.startswith("#") and l.strip()) if len(p) >= 2}
    os.unlink(tmp)
    # The bucket is the claim the ledger makes; the basis text and the similarity
    # figures are supporting detail, and the codemod's own notice must not be able
    # to change the bucket a re-run reports.
    diff = {k: (have.get(k), want.get(k)) for k in set(have) | set(want) if have.get(k) != want.get(k)}
    if not diff:
        print("provenance_ledger: OK -- PROVENANCE.tsv agrees with the corpora (%d files)" % len(have))
        return 0
    print("provenance_ledger: RED -- %d file(s) disagree with the corpora:" % len(diff))
    for k in sorted(diff)[:20]:
        print("    %s: ledger %s, corpora %s" % (k, diff[k][0], diff[k][1]))
    return 1


if __name__ == "__main__":
    sys.exit(main())
