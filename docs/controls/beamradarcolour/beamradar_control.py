#!/usr/bin/env python3
"""The beam/radar colour control, over the SHIPPED bytes and the shipping source.

The overhead-light volumetric beam and the HUD radar fov cone are the two
additive surfaces whose colour reached the screen wrong.  Everything above the
sampler is a data fact or a source constant, so an offline reading settles them:

  1. the shipped DDS colour blocks, decoded with this lane's own DXT3 algorithm
     (renderervulkan/tr_hwtex.c SMP_DXTDecodeBlock) -- the colour the sampler
     holds is a function of these bytes and nothing else;
  2. the shipped shader text that authors each surface's combine and blend;
  3. the shipping source's generator and blend-state constants, and the two
     vertex-shader arms those constants select.

  usage: beamradar_control.py [retail-root] [--mutate N]
"""
import os
import re
import struct
import sys
import zipfile

ROOT = os.environ.get("EF2_RETAIL_ROOT", os.path.expanduser("~/Star Trek Elite Force II"))
LUMP_SHADERS_TEX = 0                 # not used here; the shader text comes from the pak
HEADER_LUMPS = 30

FAILS = []


def check(cond, what):
    if cond:
        print("  ok    %s" % what)
    else:
        print("  FAIL  %s" % what)
        FAILS.append(what)
    return cond


# ---- the lane's own block decoder, renderervulkan/tr_hwtex.c:33-105 ----------

def expand565(c):
    r, g, b = (c >> 11) & 0x1F, (c >> 5) & 0x3F, c & 0x1F
    return [(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2), 255]


def decode_dxt3_block(block):
    c0 = block[8] | (block[9] << 8)
    c1 = block[10] | (block[11] << 8)
    pal = [expand565(c0), expand565(c1), [0, 0, 0, 255], [0, 0, 0, 255]]
    for k in range(3):
        pal[2][k] = (2 * pal[0][k] + pal[1][k]) // 3
        pal[3][k] = (pal[0][k] + 2 * pal[1][k]) // 3
    bits = block[12] | (block[13] << 8) | (block[14] << 16) | (block[15] << 24)
    out = []
    for i in range(16):
        a = (block[i >> 1] >> ((i & 1) * 4)) & 0xF
        p = pal[(bits >> (2 * i)) & 3]
        out.append((p[0], p[1], p[2], a * 255 // 15))
    return out


def dds_colour(data, mutate=0):
    """(w, h, mean rgb over a>0 texels, max alpha).  The mutate arm reads the
    colour half of the block as if it started at the alpha half -- the one
    off-by-eight a wrong BC2 layout would make."""
    h = struct.unpack_from("<I", data, 12)[0]
    w = struct.unpack_from("<I", data, 16)[0]
    bw, bh = (w + 3) // 4, (h + 3) // 4
    sr = sg = sb = 0
    n = 0
    amax = 0
    for by in range(bh):
        for bx in range(bw):
            o = 128 + (by * bw + bx) * 16
            blk = data[o:o + 16]
            if len(blk) < 16:
                continue
            if mutate:
                blk = blk[8:] + blk[:8]
            for px in decode_dxt3_block(blk):
                if px[3] > 0:
                    sr += px[0]
                    sg += px[1]
                    sb += px[2]
                    n += 1
                amax = max(amax, px[3])
    if not n:
        return (w, h, (0, 0, 0), amax, 0)
    return (w, h, (sr / n, sg / n, sb / n), amax, n)


# ---- the shipped shader text -------------------------------------------------

def stage_of(shader_text, name):
    m = re.search(r"(?m)^%s\s*$\n\{(.*?)\n\}" % re.escape(name), shader_text, re.S)
    return m.group(1) if m else None


def has(block, pattern):
    return re.search(pattern, block or "", re.I) is not None


# ---- the shipping source constants ------------------------------------------

def src_constant(src, path, enum_name, member):
    """The member's ordinal in its own enum, read out of the shipping header."""
    p = os.path.join(src, path)
    if not os.path.exists(p):
        return None
    body = open(p, encoding="latin1").read()
    end = body.find("} %s;" % enum_name)
    if end < 0:
        return None
    start = body.rfind("typedef enum", 0, end)
    if start < 0:
        return None
    open_b = body.find("{", start, end)
    if open_b < 0:
        return None
    inner = body[open_b + 1:end]
    value = 0
    for line in inner.splitlines():
        line = line.split("//")[0]
        d = re.match(r"\s*([A-Za-z_][A-Za-z0-9_]*)\s*(=\s*([^,]+))?\s*,?\s*$", line)
        if not d:
            continue
        if d.group(3) and re.fullmatch(r"[0-9]+|0[xX][0-9a-fA-F]+", d.group(3).strip()):
            value = int(d.group(3).strip(), 0)
        if d.group(1) == member:
            return value
        value += 1
    return None


def main():
    root = ROOT
    argv = [a for a in sys.argv[1:]]
    if argv and not argv[0].startswith("--"):
        root = argv.pop(0)
    mutate = 0
    if "--mutate" in argv:
        mutate = int(argv[argv.index("--mutate") + 1])
    src = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "..", "..", "..", "app", "src", "main", "cpp", "code")
    src = os.path.normpath(src)

    base = os.path.join(root, "base")
    paks = sorted(os.path.join(base, p) for p in os.listdir(base) if p.endswith(".pk3")) \
        if os.path.isdir(base) else []

    blobs = {}

    def read(name):
        if name in blobs:
            return blobs[name]
        for p in paks:
            try:
                with zipfile.ZipFile(p) as z:
                    if name in z.namelist():
                        blobs[name] = (p, z.read(name))
                        return blobs[name]
            except Exception:
                continue
        blobs[name] = (None, None)
        return blobs[name]

    if not check(paks, "the shipped paks are present under %s" % base):
        return 1

    # 1. the colour the sampler holds
    cases = [
        ("sysimg/hud/radar/fov.dds", "green", (0, 1, 0)),
        ("sysimg/hud/radar/arrow-arm.dds", "red", (1, 0, 0)),
        ("textures/lights/volume_yellow_dark.dds", "yellow", (1, 1, 0)),
        ("textures/lights/volume_sun1.dds", "yellow", (1, 1, 0)),
        ("textures/lights/volume_green_dark.dds", "green", (0, 1, 0)),
        ("textures/lights/volume_red.dds", "red", (1, 0, 0)),
    ]
    for name, want, sign in cases:
        pak, data = read(name)
        if not check(data is not None, "%s ships" % name):
            continue
        w, h, mean, amax, n = dds_colour(data, 1 if mutate == 1 else 0)
        ok = True
        for i in range(3):
            if sign[i] and mean[i] < 8:
                ok = False
            if not sign[i] and mean[i] > mean[ [j for j in range(3) if sign[j]][0] ]:
                ok = False
        check(ok, "%s decodes %s-dominant, mean=(%.0f,%.0f,%.0f) amax=%d [%s]"
              % (os.path.basename(name), want, mean[0], mean[1], mean[2], amax,
                 os.path.basename(pak)))

    # 2. the shipped shader text that authors each combine
    pak, hud = read("shaders/hud.shader")
    if check(hud is not None, "shaders/hud.shader ships"):
        text = hud.decode("latin1")
        b = stage_of(text, "hud/radar/fov")
        check(has(b, r"blendFunc\s+GL_SRC_ALPHA\s+GL_ONE")
              and has(b, r"rgbGen\s+const(ant)?\s+1\.0[0-9]* 1\.0[0-9]* 1\.0[0-9]*")
              and has(b, r"alphaFunc\s+GT0"), "hud/radar/fov is const-white additive, atest GT0")
    pak, lights = read("shaders/lights.shader")
    if check(lights is not None, "shaders/lights.shader ships"):
        text = lights.decode("latin1")
        for nm in ("textures/lights/volume_yellow_dark", "textures/lights/volume_sun1"):
            b = stage_of(text, nm)
            check(has(b, r"blendFunc\s+GL_SRC_ALPHA\s+GL_ONE")
                  and has(b, r"alphaGen\s+dot")
                  and has(b, r"rgbGen\s+constant\s+1\.0* 0\.9[0-9]* 0\.8[0-9]*"),
                  "%s is (1.0,0.9,0.8) additive alphaGen dot" % nm)

    # 3. the shipping source's own constants and the arms they select
    cgen = src_constant(src, "renderervulkan/tr_local.h", "colorGen_t", "CGEN_CONSTANT")
    agen_id = src_constant(src, "renderervulkan/tr_local.h", "alphaGen_t", "AGEN_IDENTITY")
    agen_dot = src_constant(src, "renderervulkan/tr_local.h", "alphaGen_t", "AGEN_DOT")
    sb = None
    tl = os.path.join(src, "renderervulkan", "tr_local.h")
    if os.path.exists(tl):
        m = re.search(r"#define\s+VKS_SRCBLEND_SRC_ALPHA\s+(0[xX][0-9a-fA-F]+|[0-9]+)", open(tl, encoding="latin1").read())
        sb = int(m.group(1), 16) if m and m.group(1).lower().startswith("0x") else (int(m.group(1)) if m else None)
    # mutants 3 and 4 are the two mis-reads a wrong lane would make: the constant
    # taken for the entity colour, and the additive blend taken for opaque
    check(cgen == (3 if mutate == 3 else 11),
          "CGEN_CONSTANT is %s (the stagebind row's rg=%s)" % (cgen, 3 if mutate == 3 else 11))
    check(agen_id == 0, "AGEN_IDENTITY is %s (the stagebind row's ag=0)" % agen_id)
    check(agen_dot == 13, "AGEN_DOT is %s" % agen_dot)
    check(sb == (2 if mutate == 4 else 5),
          "VKS_SRCBLEND_SRC_ALPHA is %s" % sb)

    vert = open(os.path.join(src, "renderervulkan", "shaders", "vk_stage.vert"),
                encoding="latin1").read()
    const_arm = re.search(r"u_ColorGen == %d\)\s*\{([^{}]*)\}" % (cgen or 11), vert)
    ident_arm = re.search(r"u_AlphaGen == %d\)\s*\{([^{}]*)\}" % (agen_id or 0), vert)
    ok = bool(const_arm) and "u_BaseColor" in (const_arm.group(0) if const_arm else "")
    check(ok, "the constant-colour arm reads u_BaseColor")
    ok = bool(ident_arm) and ("color.a = 255.0" in ident_arm.group(0)) == (mutate != 2)
    check(ok, "the identity-alpha arm forces an opaque vertex alpha")

    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
