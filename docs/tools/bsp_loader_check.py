#!/usr/bin/env python3
"""Run the live CM loader's validation over every shipped EF2 .bsp.

This is the check docs/tools/bsp_dump.py could not make: bsp_dump.py hard-codes its
record strides, so its "84/84 OK" proved something about the *data* and nothing about
the *loader*.  The loader broke exactly there -- its dshader_t was 268 bytes because
the port's MAX_QPATH is 256 instead of the SDK's 64, so CMod_LoadShaders'
`l->length % sizeof( dshader_t )` rejected every shipped map and the TUTORIAL entry
bounced back to the main menu with "CM_LoadMap: funny lump size in ".

So the strides used here are not hard-coded: this script compiles a probe that
includes the port's own code/qcommon/qfiles.h and prints sizeof() for every record
struct, then replays CM_LoadMap's checks in its call order (cm_load.c) against those
compiled sizes:

  version != 20                 -> "wrong version number"   (CM_LoadMap@0x000BB400)
  length % sizeof(record)       -> "funny lump size"         (every CMod_Load*)
  count < 1                     -> "Map with no shaders/planes/leafs/nodes/models"
  brushside/brush shaderNum     -> "bad shaderNum"           (cm_load.c:463, :287)
  lump ofs/len outside the file -> CM_LoadLump's FS_Seek failure

Cross-lump references that retail indexes without checking (node->planeNum,
leaf->firstLeafBrush/firstLeafSurface, patch->shaderNum) are reported separately as
"oob-ref": they are not Com_Error sites, but an out-of-range value means the loader
mis-parsed the lump, which is the failure mode this whole regression was.
"""
import glob
import os
import struct
import subprocess
import sys
import tempfile
import zipfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WIP = os.path.dirname(REPO)                       # .../EF2_Android_WIP
OPENMOHAA = os.path.join(REPO, "openmohaa")
BASE = os.path.join(WIP, "scratchpad", "airun", "base")

HEADER_LUMPS = 30
BSP_VERSION = 20

# code/qcommon/qfiles.h LUMP_*, in file order
LUMP = {
    "SHADERS": 0, "PLANES": 1, "LIGHTMAPS": 2, "BASELIGHTMAPS": 3,
    "CONTLIGHTMAPS": 4, "SURFACES": 5, "DRAWVERTS": 6, "DRAWINDEXES": 7,
    "LEAFBRUSHES": 8, "LEAFSURFACES": 9, "LEAFS": 10, "NODES": 11,
    "BRUSHSIDES": 12, "BRUSHES": 13, "FOGS": 14, "MODELS": 15, "ENTITIES": 16,
    "VISIBILITY": 17, "LIGHTGRID": 18, "ENTLIGHTS": 19, "ENTLIGHTSVIS": 20,
    "LIGHTDEFS": 21, "BASELIGHTINGVERTS": 22, "CONTLIGHTINGVERTS": 23,
    "BASELIGHTINGSURFS": 24, "LIGHTINGSURFS": 25, "LIGHTINGVERTSURFS": 26,
    "LIGHTINGGROUPS": 27, "STATIC_LOD_MODELS": 28, "BSPINFO": 29,
}

PROBE_C = r"""
#include <stdio.h>
#include "q_shared.h"
#include "qfiles.h"

#define P(S) printf(#S " %zu\n", sizeof(S))

int main(void) {
    P(dheader_t); P(dshader_t); P(dplane_t); P(dnode_t); P(dleaf_t);
    P(dbrushside_t); P(dbrush_t); P(dfog_t); P(dmodel_t); P(drawVert_t);
    P(dsurface_t); P(lightingGroup_t); P(lightingVert_t);
    return 0;
}
"""


def compiled_strides():
    """sizeof() straight out of the port's own qfiles.h -- nothing mirrored here."""
    with tempfile.TemporaryDirectory() as td:
        c = os.path.join(td, "probe.c")
        exe = os.path.join(td, "probe")
        with open(c, "w") as f:
            f.write(PROBE_C)
        cmd = ["gcc", "-o", exe, c, "-I", os.path.join(OPENMOHAA, "code", "qcommon")]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode:
            raise SystemExit("stride probe failed to compile:\n%s\n%s"
                             % (r.stdout, r.stderr))
        r = subprocess.run([exe], capture_output=True, text=True)
        if r.returncode:
            raise SystemExit("stride probe failed to run:\n%s" % r.stderr)
        out = {}
        for line in r.stdout.strip().splitlines():
            name, val = line.split()
            out[name] = int(val)
        return out


def bsp_files():
    """Every .bsp in the shipped paks (pak10 holds 84; four more live in the ctf/hm paks)."""
    for pk3 in sorted(glob.glob(os.path.join(BASE, "*.pk3"))):
        with zipfile.ZipFile(pk3) as z:
            for name in z.namelist():
                if name.lower().endswith(".bsp"):
                    yield pk3, name, z.read(name)


def check(data, st, report):
    """Replay CM_LoadMap's checks, in its call order."""
    if len(data) < st["dheader_t"]:
        return "file shorter than dheader_t (%d)" % st["dheader_t"]

    version = struct.unpack_from("<i", data, 4)[0]
    if version != BSP_VERSION:
        return "wrong version number (%i should be %i)" % (version, BSP_VERSION)

    lumps = {}
    for i in range(HEADER_LUMPS):
        ofs, ln = struct.unpack_from("<ii", data, 12 + 8 * i)
        lumps[i] = (ofs, ln)
        # CM_LoadLump: FS_Seek(handle, fileofs) + FS_Read(filelen) must stay in the file
        if ofs < 0 or ofs + ln > len(data):
            report.append("lump %d: ofs %d len %d outside file (%d)"
                          % (i, ofs, ln, len(data)))

    def stride(lump, rec):
        return st[rec] if lumps[lump][1] else 0

    def count(lump, rec):
        ofs, ln = lumps[lump]
        s = stride(lump, rec)
        return (ln // s) if s else 0

    def funny(lump, rec):
        s = stride(lump, rec)
        if s and lumps[lump][1] % s:
            report.append("funny lump size (lump %d, %d %% %d)"
                          % (lump, lumps[lump][1], s))

    # CMod_LoadShaders
    funny(LUMP["SHADERS"], "dshader_t")
    if count(LUMP["SHADERS"], "dshader_t") < 1:
        report.append("Map with no shaders")
    num_shaders = count(LUMP["SHADERS"], "dshader_t")

    # CMod_LoadPlanes
    funny(LUMP["PLANES"], "dplane_t")
    if count(LUMP["PLANES"], "dplane_t") < 1:
        report.append("Map with no planes")
    num_planes = count(LUMP["PLANES"], "dplane_t")

    # CMod_LoadPatches / CMod_LoadTerrainEF2 (both read LUMP_SURFACES + LUMP_DRAWVERTS)
    funny(LUMP["SURFACES"], "dsurface_t")
    funny(LUMP["DRAWVERTS"], "drawVert_t")
    sofs, slen = lumps[LUMP["SURFACES"]]
    for i in range(count(LUMP["SURFACES"], "dsurface_t")):
        base = sofs + i * st["dsurface_t"]
        shader_num = struct.unpack_from("<i", data, base)[0]          # dsurface_t.shaderNum
        stype = struct.unpack_from("<i", data, base + 8)[0]           # dsurface_t.surfaceType
        if stype == 2:  # MST_PATCH, cm.shaders[shaderNum] is read
            if not 0 <= shader_num < num_shaders:
                report.append("patch shaderNum %d / %d shaders" % (shader_num, num_shaders))
        if stype == 5:  # MST_TERRAIN, same shader read in CMod_LoadTerrainEF2
            if not 0 <= shader_num < num_shaders:
                report.append("terrain shaderNum %d / %d shaders" % (shader_num, num_shaders))

    # CMod_LoadLeafBrushes / CMod_LoadLeafSurfaces
    funny(LUMP["LEAFBRUSHES"], "int4")
    funny(LUMP["LEAFSURFACES"], "int4")
    num_leafbrushes = count(LUMP["LEAFBRUSHES"], "int4")
    num_leafsurfaces = count(LUMP["LEAFSURFACES"], "int4")

    # CMod_LoadLeafs
    funny(LUMP["LEAFS"], "dleaf_t")
    if count(LUMP["LEAFS"], "dleaf_t") < 1:
        report.append("Map with no leafs")
    lofs, llen = lumps[LUMP["LEAFS"]]
    for i in range(count(LUMP["LEAFS"], "dleaf_t")):
        base = lofs + i * st["dleaf_t"]
        # dleaf_t tail: numLeafSurfaces, firstLeafSurface, firstLeafBrush, numLeafBrushes
        _n_ls, first_ls, first_lb, _n_lb = struct.unpack_from("<iiii", data, base + 32)
        if _n_lb and not 0 <= first_lb < num_leafbrushes:
            report.append("leaf %d firstLeafBrush %d / %d" % (i, first_lb, num_leafbrushes))
        if _n_ls and not 0 <= first_ls < num_leafsurfaces:
            report.append("leaf %d firstLeafSurface %d / %d" % (i, first_ls, num_leafsurfaces))

    # CMod_LoadNodes
    funny(LUMP["NODES"], "dnode_t")
    if count(LUMP["NODES"], "dnode_t") < 1:
        report.append("Map has no nodes")
    nofs, nlen = lumps[LUMP["NODES"]]
    for i in range(count(LUMP["NODES"], "dnode_t")):
        plane_num = struct.unpack_from("<i", data, nofs + i * st["dnode_t"])[0]
        if not 0 <= plane_num < num_planes:
            report.append("node %d planeNum %d / %d planes" % (i, plane_num, num_planes))

    # CMod_LoadBrushSides
    funny(LUMP["BRUSHSIDES"], "dbrushside_t")
    bofs, blen = lumps[LUMP["BRUSHSIDES"]]
    for i in range(count(LUMP["BRUSHSIDES"], "dbrushside_t")):
        base = bofs + i * st["dbrushside_t"]
        # retail CMod_LoadBrushSides@0x000BADA0: in->[0] is the bounds-checked shaderNum,
        # in->[1] is the planeNum (`*v14 = v8[21] + 20 * LittleLong( v11[1] )`)
        shader_num, plane_num = struct.unpack_from("<ii", data, base)
        if not 0 <= shader_num < num_shaders:
            report.append("CMod_LoadBrushSides: bad shaderNum: %i" % shader_num)
        if not 0 <= plane_num < num_planes:
            report.append("brushside %d planeNum %d / %d planes" % (i, plane_num, num_planes))

    # CMod_LoadBrushes
    funny(LUMP["BRUSHES"], "dbrush_t")
    br_ofs, br_len = lumps[LUMP["BRUSHES"]]
    for i in range(count(LUMP["BRUSHES"], "dbrush_t")):
        base = br_ofs + i * st["dbrush_t"]
        num_sides, first_side, shader_num = struct.unpack_from("<iii", data, base)
        if not 0 <= shader_num < num_shaders:
            report.append("CMod_LoadBrushes: bad shaderNum: %i" % shader_num)
        if num_sides and not 0 <= first_side < count(LUMP["BRUSHSIDES"], "dbrushside_t"):
            report.append("brush %d firstSide %d / %d brushsides"
                          % (i, first_side, count(LUMP["BRUSHSIDES"], "dbrushside_t")))

    # CMod_LoadSubmodels
    funny(LUMP["MODELS"], "dmodel_t")
    if count(LUMP["MODELS"], "dmodel_t") < 1:
        report.append("Map with no models")

    # CMod_LoadVisibility reads [0]=numClusters and [1]=iRowSize
    if lumps[LUMP["VISIBILITY"]][1] and lumps[LUMP["VISIBILITY"]][1] < 8:
        report.append("visibility lump %d < 8" % lumps[LUMP["VISIBILITY"]][1])

    # CMod_LoadLightingGroups: retail only Com_WDPrintf's on len & 0x3F, count stays 0
    if lumps[LUMP["LIGHTINGGROUPS"]][1] % 64:
        report.append("WARN lightinggroups %d %% 64" % lumps[LUMP["LIGHTINGGROUPS"]][1])

    # FOGS is renderer-side (R_LoadFogs) but the same dfog_t record
    funny(LUMP["FOGS"], "dfog_t")
    return None


def main():
    st = compiled_strides()
    st["int4"] = 4
    print("== strides compiled out of the port's own qfiles.h ==")
    for k in sorted(st):
        print("   %-16s %d" % (k, st[k]))
    print()

    fatal_maps = 0
    oob_maps = 0
    total = 0
    rows = []
    for pk3, name, data in bsp_files():
        total += 1
        errors = []
        oob = []
        # split Com_Error-mirroring failures from the unchecked-index diagnostics
        captured = []
        err = check(data, st, captured)
        if err:
            errors.append(err)
        for c in captured:
            (oob if ("planeNum" in c or "firstLeaf" in c or "firstSide" in c
                     or "shaderNum %d /" in c) else errors).append(c)
        if err or errors:
            fatal_maps += 1
            shown = (errors + oob)[:4]
            more = "" if len(errors) + len(oob) <= 4 else " (+%d more)" % (len(errors) + len(oob) - 4)
            print("FAIL %-34s %s%s" % (name.split("/")[-1], "; ".join(shown), more))
        elif oob:
            oob_maps += 1
            print("OOB  %-34s %s" % (name.split("/")[-1], "; ".join(oob[:4])))
        else:
            rows.append((pk3.split("/")[-1], name, len(data)))
            print("ok   %-34s %9d bytes  %4d shaders" % (
                name.split("/")[-1], len(data), count(data, st, "dshader_t")))

    print("\n== %d maps, %d with a Com_Error-mirroring failure, %d with out-of-range refs, "
          "%d clean ==" % (total, fatal_maps, oob_maps, len(rows)))
    return 1 if fatal_maps else 0


def count(data, st, rec):
    n = struct.unpack_from("<i", data, 12 + 8 * LUMP["SHADERS"] + 4)[0]
    return (n // st[rec]) if n else 0


if __name__ == "__main__":
    sys.exit(main())
