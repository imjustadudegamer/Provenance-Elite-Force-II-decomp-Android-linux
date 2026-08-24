#!/usr/bin/env python3
"""EF2 retail BSP lump dumper / CM-model oracle.

Structures are the retail SDK's (docs/evidence/ef2-sdk/ef2-sdk/Shared/qcommon/qfiles.h,
LF-normalised).  Lump indices are LUMP_* from that same header (0..29,
HEADER_LUMPS = 30); sizeof(dheader_t) = 12 + 30*8 = 252, matching retail
CM_LoadMap's `FS_Read(v61, 252, v56)` (engine_decomp/CM_LoadMap.c:112) and its
`if ( v15 != 20 ) Com_Error(...)` version gate (:129).
"""
import os, struct, sys, json

SDK_LUMPS = [
    ("LUMP_SHADERS", 0), ("LUMP_PLANES", 1), ("LUMP_LIGHTMAPS", 2),
    ("LUMP_BASELIGHTMAPS", 3), ("LUMP_CONTLIGHTMAPS", 4), ("LUMP_SURFACES", 5),
    ("LUMP_DRAWVERTS", 6), ("LUMP_DRAWINDEXES", 7), ("LUMP_LEAFBRUSHES", 8),
    ("LUMP_LEAFSURFACES", 9), ("LUMP_LEAFS", 10), ("LUMP_NODES", 11),
    ("LUMP_BRUSHSIDES", 12), ("LUMP_BRUSHES", 13), ("LUMP_FOGS", 14),
    ("LUMP_MODELS", 15), ("LUMP_ENTITIES", 16), ("LUMP_VISIBILITY", 17),
    ("LUMP_LIGHTGRID", 18), ("LUMP_ENTLIGHTS", 19), ("LUMP_ENTLIGHTSVIS", 20),
    ("LUMP_LIGHTDEFS", 21), ("LUMP_BASELIGHTINGVERTS", 22),
    ("LUMP_CONTLIGHTINGVERTS", 23), ("LUMP_BASELIGHTINGSURFS", 24),
    ("LUMP_LIGHTINGSURFS", 25), ("LUMP_LIGHTINGVERTSURFS", 26),
    ("LUMP_LIGHTINGGROUPS", 27), ("LUMP_STATIC_LOD_MODELS", 28),
    ("LUMP_BSPINFO", 29),
]
HEADER_LUMPS = 30

# record strides, straight out of the SDK struct definitions
STRIDES = {
    "LUMP_SHADERS": 76,        # dshader_t   {char[64], int, int, int}
    "LUMP_PLANES": 16,         # dplane_t    {float[3], float}  (no `type`)
    "LUMP_SURFACES": 132,      # dsurface_t
    "LUMP_DRAWVERTS": 48,      # drawVert_t  {xyz,st,normal,color,lodExtra,union}
    "LUMP_DRAWINDEXES": 4,
    "LUMP_LEAFBRUSHES": 4,
    "LUMP_LEAFSURFACES": 4,
    "LUMP_LEAFS": 48,          # dleaf_t     {cluster,area,mins,maxs,nLS,fLS,fLB,nLB}
    "LUMP_NODES": 36,          # dnode_t     {planeNum,children[2],mins[3],maxs[3]}
    "LUMP_BRUSHSIDES": 8,      # dbrushside_t{shaderNum,planeNum}
    "LUMP_BRUSHES": 12,        # dbrush_t    {numSides,firstSide,shaderNum}
    "LUMP_FOGS": 72,           # dfog_t      {char[64],brushNum,visibleSide}
    "LUMP_MODELS": 40,         # dmodel_t    {mins[3],maxs[3],4 ints}
    "LUMP_LIGHTINGGROUPS": 64, # lightingGroup_t {char name[64]}
    "LUMP_BASELIGHTINGVERTS": 3,   # lightingVert_t   {byte color[3]} -- packed, no padding
    "LUMP_BASELIGHTINGSURFS": 44,  # baseLightingSurf_t -- 11 ints, not 10
    "LUMP_LIGHTINGSURFS": 20,      # lightingSurf_t -- 4 ints + byte[3], padded 19 -> 20
    "LUMP_LIGHTINGVERTSURFS": 12,  # lightingVertSurf_t {int, int, byte[3]} padded 9 -> 12
}

# the lumps retail's CM_LoadMap actually consumes, in its call order
# (engine_decomp/CM_LoadMap.c:141..455)
CM_LUMPS = [
    "LUMP_SHADERS",        # CMod_LoadShaders
    "LUMP_PLANES",         # CMod_LoadPlanes
    "LUMP_SURFACES",       # CMod_LoadPatches
    "LUMP_DRAWVERTS",      # CMod_LoadTerrain
    "LUMP_DRAWINDEXES",    # raw int array -> cm.drawIndexes
    "LUMP_LEAFBRUSHES",    # raw int array -> cm.leafbrushes
    "LUMP_LEAFS",          # CMod_LoadLeafs
    "LUMP_NODES",          # CMod_LoadNodes
    "LUMP_BRUSHSIDES",     # CMod_LoadBrushSides
    "LUMP_BRUSHES",        # CMod_LoadBrushes
    "LUMP_MODELS",         # CMod_LoadSubmodels
    "LUMP_ENTITIES",       # raw copy      -> cm.entitystring
    "LUMP_VISIBILITY",     # raw copy      -> cm.visibility (+numClusters/rowSize)
    "LUMP_LIGHTINGGROUPS", # 64-byte strcpy loop -> cm lighting groups
]


def load(path):
    with open(path, "rb") as f:
        return f.read()


def parse(path):
    b = load(path)
    out = {"path": path, "size": len(b)}
    ident, version, checksum = struct.unpack_from("<iii", b, 0)
    out["ident"] = ident
    out["ident_str"] = struct.unpack_from("<4s", b, 0)[0].decode("latin1")
    out["version"] = version
    out["checksum"] = checksum
    if version != 20:
        out["error"] = "version %d != 20" % version
        return out
    lumps = []
    for i in range(HEADER_LUMPS):
        o, l = struct.unpack_from("<ii", b, 12 + i * 8)
        lumps.append((o, l))
    out["lumps"] = lumps

    ents = {}
    for name, idx in SDK_LUMPS:
        o, l = lumps[idx]
        st = STRIDES.get(name)
        e = {"fileofs": o, "filelen": l}
        if st:
            e["stride"] = st
            e["count"] = (l // st) if st else 0
            e["rem"] = l % st
        ents[name] = e
    out["ent"] = ents

    # retail CM-observable quantities
    cm = {}
    for name in CM_LUMPS:
        cm[name] = ents[name].get("count", ents[name]["filelen"] // STRIDES.get(name, 1))
    # LUMP_VISIBILITY: retail reads [0]=numClusters [1]=iRowSize then (len-8) bytes
    vo, vl = lumps[17]
    if vl >= 8:
        cm["vis_numClusters"] = struct.unpack_from("<i", b, vo)[0]
        cm["vis_rowSize"] = struct.unpack_from("<i", b, vo + 4)[0]
        cm["vis_payload"] = vl - 8
    # retail CM portal/area count comes from the leafs' `area` field
    lo, ll = lumps[10]
    areas = 0
    clusters = 0
    for i in range(ll // 48):
        cluster, area = struct.unpack_from("<ii", b, lo + i * 48)
        areas = max(areas, area + 1)
        if cluster + 1 > clusters:
            clusters = cluster + 1
    cm["numAreas_from_leafs"] = areas
    cm["maxCluster_from_leafs"] = clusters
    out["cm"] = cm

    # sanity: contents/surface flag bit census over the shader lump
    so, sl = lumps[0]
    cbits, sbits, shaders = 0, 0, []
    for i in range(sl // 76):
        nm = b[so + i * 76: so + i * 76 + 64].split(b"\x00")[0].decode("latin1")
        sf, cf, sub = struct.unpack_from("<iii", b, so + i * 76 + 64)
        sbits |= sf
        cbits |= cf
        shaders.append((nm, sf, cf, sub))
    out["shader_cbits"] = cbits
    out["shader_sbits"] = sbits
    out["n_shaders"] = len(shaders)
    return out


def main():
    files = sys.argv[1:]
    if not files:
        files = sorted(
            os.path.join(d, f)
            for d, _, fs in os.walk("/tmp/ef2docs/bsp")
            for f in fs if f.endswith(".bsp")
        )
    bad = 0
    rows = []
    for p in files:
        try:
            r = parse(p)
        except Exception as e:
            print("FAIL %s: %s" % (p, e))
            bad += 1
            continue
        if "error" in r:
            print("BAD  %-40s %s (ident %r)" % (os.path.basename(p), r["error"], r["ident_str"]))
            bad += 1
            continue
        rem = {k: v["rem"] for k, v in r["ent"].items() if v.get("rem")}
        if rem:
            print("REM  %-40s %s" % (os.path.basename(p), rem))
            bad += 1
        rows.append(r)
    print("\n== %d parsed, %d bad ==" % (len(rows), bad))

    if rows:
        keys = ["n_shaders"]
        for n in CM_LUMPS:
            keys.append(n)
        print("%-24s %8s" % ("map", "shaders"), "".join("%9s" % k.replace("LUMP_", "")[:8] for k in keys[1:]))
        for r in rows[:0]:
            pass
        for r in rows:
            line = "%-24s %8d" % (os.path.basename(r["path"])[:23], r["n_shaders"])
            for k in keys[1:]:
                line += "%9d" % r["cm"][k]
            print(line)
        print("\n-- visibility / areas --")
        print("%-24s %10s %10s %10s %8s" % ("map", "clusters", "rowSize", "payload", "areas"))
        for r in rows:
            c = r["cm"]
            print("%-24s %10d %10d %10d %8d" % (
                os.path.basename(r["path"])[:23], c["vis_numClusters"],
                c["vis_rowSize"], c["vis_payload"], c["numAreas_from_leafs"]))
        print("\n-- shader flag bits seen across all maps --")
        cb = 0
        sb = 0
        for r in rows:
            cb |= r["shader_cbits"]
            sb |= r["shader_sbits"]
        print("contents bits: %s" % " ".join(str(i) for i in range(32) if cb >> i & 1))
        print("surface  bits: %s" % " ".join(str(i) for i in range(32) if sb >> i & 1))
        with open("/tmp/ef2docs/bsp_dump.json", "w") as f:
            json.dump(rows, f, indent=1)


if __name__ == "__main__":
    main()
