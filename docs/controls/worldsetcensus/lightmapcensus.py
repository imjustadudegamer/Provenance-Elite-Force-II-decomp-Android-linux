#!/usr/bin/env python3
"""The lightmap-atlas census, over the SHIPPED BSPs.

The one thing an offline reading of this lane can settle about "some surfaces
are black" is whether a surface can be assigned a lightmap slot the build never
filled.  The answer is in the shipped bytes, not in a run: the draw-surface
lump's `lightmapNum` (dsurface_t +28, qfiles.h) against
`LUMP_LIGHTMAPS / (LIGHTMAP_WIDTH*LIGHTMAP_HEIGHT*3)` and, for a surface that
carries DYNAMIC_LIGHTMAP_BIT, against `LUMP_BASELIGHTMAPS` for the same size.

  usage: lightmapcensus.py <retail-root> [--mutate N]
"""
import glob
import os
import struct
import sys
import zipfile

LM_SIZE = 128 * 128 * 3          # LIGHTMAP_WIDTH*LIGHTMAP_HEIGHT*3 (qfiles.h:334-337)
DS_SIZE = 132                    # sizeof(dsurface_t), qfiles.h:544-574
DS_LIGHTMAPNUM = 28              # offsetof(dsurface_t, lightmapNum)
DYNAMIC_LIGHTMAP_BIT = 1 << 30   # qfiles.h:514
LUMP_LIGHTMAPS, LUMP_BASELIGHTMAPS, LUMP_CONTLIGHTMAPS, LUMP_SURFACES = 2, 3, 4, 5
HEADER_LUMPS = 30
LUMP_DIR = 12                    # dheader_t: ident+version+checksum

FAILS = []


def check(cond, what):
    if cond:
        print("  ok    %s" % what)
    else:
        print("  FAIL  %s" % what)
        FAILS.append(what)
    return cond


def census(data, lm_off_shift=0):
    lumps = [struct.unpack_from("<ii", data, LUMP_DIR + i * 8) for i in range(HEADER_LUMPS)]
    nl = lumps[LUMP_LIGHTMAPS][1] // LM_SIZE
    nbl = lumps[LUMP_BASELIGHTMAPS][1] // LM_SIZE
    ncl = lumps[LUMP_CONTLIGHTMAPS][1] // LM_SIZE
    soff, slen = lumps[LUMP_SURFACES]
    n = slen // DS_SIZE
    oob = []
    used = set()
    dyn = set()
    for i in range(n):
        lm = struct.unpack_from("<i", data, soff + i * DS_SIZE + DS_LIGHTMAPNUM +
                                lm_off_shift)[0]
        if lm < 0:
            continue
        if lm & DYNAMIC_LIGHTMAP_BIT:
            idx = lm & (DYNAMIC_LIGHTMAP_BIT - 1)
            dyn.add(idx)
            if idx >= nbl:
                oob.append((i, "dyn-base", idx))
        else:
            used.add(lm)
            if lm >= nl:
                oob.append((i, "lm", lm))
    return dict(nl=nl, nbl=nbl, ncl=ncl, surfaces=n, oob=oob,
                used=sorted(used), dyn=sorted(dyn))


def main():
    root = os.environ.get("EF2_RETAIL_ROOT",
                            os.path.expanduser("~/Star Trek Elite Force II"))
    if len(sys.argv) > 1 and not sys.argv[1].startswith("--"):
        root = sys.argv[1]
    shift = 0
    if "--mutate" in sys.argv:
        shift = int(sys.argv[sys.argv.index("--mutate") + 1])
    sh = {1: 4, 2: 4}.get(shift, 0)      # a planted mis-offset for lightmapNum

    base = os.path.join(root, "base")
    paks = sorted(glob.glob(os.path.join(base, "*.pk3")))
    maps = []
    for p in paks:
        try:
            with zipfile.ZipFile(p) as z:
                for n in z.namelist():
                    if n.lower().endswith(".bsp"):
                        maps.append((p, n))
        except Exception:
            continue
    if not check(maps, "the shipped paks carry BSPs"):
        print("  FAIL  no shipped BSP found under %s (set EF2_RETAIL_ROOT)" % base)
        FAILS.append("retail install")
        return 1

    totals = dict(maps=0, surfaces=0, oob=0, dyn=0, empty_lm=0)
    for p, n in maps:
        try:
            with zipfile.ZipFile(p) as z:
                data = z.read(n)
        except Exception:
            continue
        if len(data) < LUMP_DIR + HEADER_LUMPS * 8 or data[:4] not in (b"EF2!", b"IBSP"):
            continue
        c = census(data, sh)
        totals["maps"] += 1
        totals["surfaces"] += c["surfaces"]
        totals["oob"] += len(c["oob"])
        totals["dyn"] += len(c["dyn"])
        if c["nl"] == 0:
            totals["empty_lm"] += 1
        if c["oob"]:
            check(False, "%s: %d surfaces index a lightmap the build never filled (%r)"
                  % (n, len(c["oob"]), c["oob"][:4]))

    check(totals["maps"] > 0,
          "%d shipped BSPs parsed, %d draw surfaces" % (totals["maps"], totals["surfaces"]))
    check(totals["oob"] == 0,
          "no surface indexes past LUMP_LIGHTMAPS/LUMP_BASELIGHTMAPS (%d violations)"
          % totals["oob"])

    if FAILS:
        print("lightmapcensus: %d failures" % len(FAILS))
        return 1
    print("lightmapcensus: green -- %d surfaces, %d use a dynamic base index, "
          "%d maps with an empty lightmap lump"
          % (totals["surfaces"], totals["dyn"], totals["empty_lm"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
