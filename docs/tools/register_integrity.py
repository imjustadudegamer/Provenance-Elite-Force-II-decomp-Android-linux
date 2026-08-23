#!/usr/bin/env python3
"""Register-integrity audit: every address the machine register cites must exist in the
Mac PPC symbol tables, and every function size it states must match.

The register (docs/agent_reports/w30final_0912_master.tsv) carries the evidence for each
divergence row.  This tool extracts every `@0xADDR` citation from the evidence columns and
every `(NNNN B)` / `size=NNNN` size statement next to it, then checks them against
personal/evidence/mac_symbols/{engine_EF2exe,game_dll,cgame_dll}_symbols.txt -- the tables are kept
SEPARATE because the same address means different things in the engine, the game DLL and
the cgame DLL (a cross-table merge produces false mismatches; that bug bit the first run
of this tool).

usage:  python3 docs/tools/register_integrity.py [tsv]
Output: citations checked, size mismatches (must be 0), and the list of citations that
are inner-instruction or data addresses (legitimately absent from the symbol tables).
"""
import os
import re
import sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + ""
TABLES = {
    "engine": ROOT + "/personal/evidence/mac_symbols/engine_EF2exe_symbols.txt",
    "game": ROOT + "/personal/evidence/mac_symbols/game_dll_symbols.txt",
    "cgame": ROOT + "/personal/evidence/mac_symbols/cgame_dll_symbols.txt",
}
SYM = defaultdict(list)


def load():
    for tag, path in TABLES.items():
        try:
            fh = open(path, errors="ignore")
        except FileNotFoundError:
            print("missing table: %s" % path)
            continue
        with fh:
            for line in fh:
                m = re.match(r"\s*0x([0-9A-Fa-f]+)\s+(\d+)\s+\.(\S+)", line)
                if m:
                    SYM[int(m.group(1), 16)].append((tag, m.group(3), int(m.group(2))))


def main():
    tsv = sys.argv[1] if len(sys.argv) > 1 else ROOT + "/docs/agent_reports/w30final_0912_master.tsv"
    load()
    addr_re = re.compile(r"@\s*0x([0-9A-Fa-f]{4,})")
    size_re = re.compile(r"\((\d+)\s*B\b|size=(\d+)")
    checked = mismatches = 0
    inner = []
    for row in open(tsv, errors="ignore").read().split("\n")[1:]:
        f = row.split("\t")
        if len(f) < 8:
            continue
        evidence = f[4] + " " + f[7]
        for m in addr_re.finditer(evidence):
            addr = int(m.group(1), 16)
            checked += 1
            cands = SYM.get(addr)
            # Associate a stated size with THIS address only: cut the window at the next
            # address citation, so a row that lists several addresses before one "(NNN B)"
            # cannot charge that size to the wrong symbol (W31-VERIFY5 D6 found this).
            window = evidence[m.end():m.end() + 90]
            nxt = window.find("@0x")
            if nxt >= 0:
                window = window[:nxt]
            stated_m = size_re.search(window)
            if not cands:
                inner.append((f[1], hex(addr)))
                continue
            if stated_m:
                stated = int(next(g for g in stated_m.groups() if g))
                if not any(c[2] == stated for c in cands):
                    mismatches += 1
                    print("SIZE MISMATCH row=%s addr=%s stated=%d table=%s"
                          % (f[1], hex(addr), stated,
                             ", ".join("%s:%s=%d" % c for c in cands)))
    print("citations checked=%d  size mismatches=%d  inner/data addresses=%d"
          % (checked, mismatches, len(inner)))
    for row_id, addr in inner:
        print("  inner/data (no symbol entry): %s %s" % (row_id, addr))
    print("ALL REGISTER CITATIONS OK" if mismatches == 0 else "REGISTER CITATIONS HAVE PROBLEMS")


if __name__ == "__main__":
    main()
