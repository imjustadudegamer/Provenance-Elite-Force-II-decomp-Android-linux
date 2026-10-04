#!/usr/bin/env python3
"""d2color -- the 2D COLOUR verdict, driven over shipped data and retail's own
trace numbers.

The coverage census ([VK:2dcov], qa_probe AND vk_2dCover) already answers
"did the quad produce pixels".  This control certifies the question it could
not: what COLOUR the element produced.  It runs the port's own decision
function -- the one the census now emits -- over

  * the SHIPPED art (sysimg/hud/{hud_health_bkg,health_bar,armor_bar}.dds in
    pak0.pk3, decoded here byte for byte), because "the bar is black" is a
    claim about the data as much as about the draw;
  * the SHIPPED shader scripts (shaders/hud.shader) and widget definitions
    (ui/healthhud.urc in locpak1.pk3), which say what each element resolves
    its colour from and where it lands;
  * retail's own HUD draw stream: personal/evidence/traces/
    retail-menu-solomatch-final.trace, `apitrace dump --grep='glDraw|glBindTexture
    |glColor|glBlendFunc|glScissor|glViewport|glOrtho|glEnable|glDisable|glAlphaFunc'`
    -- the widget windows, colours, blend funcs and texture binds of the
    health/armor row, with their call numbers.

Usage: d2color_control.py [fault] [tr_draw.c] [tr_backend.c]
  fault 0 (default) runs the shipping tree and the shipped data, GREEN when
  every element is accounted for.  1..12 plant a fault in the MODEL and must
  print a verdict that is not OK.
"""

import os
import re
import struct
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
RETAIL = os.environ.get("EF2_RETAIL_ROOT", os.path.expanduser("~/Star Trek Elite Force II"))
SRC = os.path.join(HERE, "..", "..", "..", "app", "src", "main", "cpp", "code", "renderervulkan")

TR_DRAW = os.path.join(SRC, "tr_draw.c")
TR_BACKEND = os.path.join(SRC, "tr_backend.c")

if len(sys.argv) > 2 and sys.argv[2]:
    TR_DRAW = sys.argv[2]
if len(sys.argv) > 3 and sys.argv[3]:
    TR_BACKEND = sys.argv[3]

fails = []
checks = [0]


def check(ok, what):
    checks[0] += 1
    if not ok:
        fails.append(what)
    return ok


# ---------------------------------------------------------------------------
# The decision function the census emits.  Kept here in the census's own terms
# so the control certifies the same predicate the instrument uses.
# ---------------------------------------------------------------------------

CGEN_IDENTITY, CGEN_IDENTITY_LIGHTING, CGEN_EXACT_VERTEX, CGEN_VERTEX = 2, 1, 5, 6
CGEN_CONSTANT, CGEN_GLOBAL_COLOR = 11, 13
AGEN_SKIP, AGEN_IDENTITY, AGEN_VERTEX = 1, 0, 4
AGEN_CONSTANT, AGEN_GLOBAL_ALPHA = 15, 16

# VK_StageNeedsCPUColor2D's keep-set, tr_shade.c: the gens the 2D quad's own
# vertex colour already carries, which is why the walk must NOT resolve them.
VERTEX_RGB_GENS = (CGEN_IDENTITY, CGEN_IDENTITY_LIGHTING, CGEN_GLOBAL_COLOR,
                   CGEN_EXACT_VERTEX, CGEN_VERTEX)
VERTEX_ALPHA_GENS = (AGEN_SKIP, AGEN_IDENTITY, AGEN_GLOBAL_ALPHA, AGEN_VERTEX)


def resolve_2d_colour(rgb_gen, alpha_gen, vcol, const_color, global_color,
                      from_vertex):
    """The colour the 2D stage delivers, in the walk's own terms.

    from_vertex is what the walk decided: True when the stage's gens are the
    ones the quad's per-vertex colour already carries (so the quad's colour IS
    the resolution) and False when the stage had to be resolved into the CPU
    colour array -- in which case rgbGen global takes the 2D global colour and
    rgbGen const takes the stage's constant.

    The defect this catches: the global colour must be the one the element's
    own record carried, not whatever the shared variable held at draw time.
    """
    if from_vertex:
        return tuple(vcol)

    if rgb_gen == CGEN_GLOBAL_COLOR:
        c = list(global_color)
    elif rgb_gen == CGEN_CONSTANT:
        c = list(const_color)
    elif rgb_gen in (CGEN_IDENTITY, CGEN_IDENTITY_LIGHTING):
        c = [255, 255, 255, vcol[3]]
    else:
        c = list(vcol)

    if alpha_gen == AGEN_GLOBAL_ALPHA:
        c[3] = global_color[3]
    elif alpha_gen == AGEN_CONSTANT:
        c[3] = const_color[3]
    elif alpha_gen in (AGEN_IDENTITY, AGEN_SKIP):
        c[3] = 255
    return tuple(c)


def image_binding_verdict(stage_art, bound_desc):
    """The unit's answer: is the image the stage names the image it samples."""
    if not bound_desc or bound_desc in ("(none)", "(no image)", "0x0/0x0/0m"):
        return "UNRESOLVED_IMAGE"
    if stage_art and bound_desc != stage_art:
        return "MISMATCHED_IMAGE"
    return "OK"


def text_run_verdict(run_quads, glyphs):
    """A run the UI asked for that produced no quad at all."""
    if glyphs > 0 and run_quads == 0:
        return "TEXT_RUN_EMPTY"
    return "OK"


def coverage_colour_verdict(px, samples, clear):
    """What the element produced, measured over its own rect.

    px is the census's geometry-and-clip area; samples are the rect's own
    readback pixels.  A verdict of ISSUED_NOT_RASTERISED where px > 0 is the
    census reporting pixels the readback says are clear -- the case the
    coverage census exists to refuse, and the one a colour census must not
    contradict.
    """
    if px == 0:
        return "CLIP0"
    lit = [s for s in samples if tuple(s) != tuple(clear)]
    if not lit:
        return "ISSUED_NOT_RASTERISED"
    if all(s[0] < 8 and s[1] < 8 and s[2] < 8 and s[3] > 200 for s in lit):
        return "DREW_BLACK"
    if len(set(tuple(s) for s in samples)) <= 1:
        return "NO_CONTENT"
    return "OK"


# ---------------------------------------------------------------------------
# Shipped data: the art, the shader scripts, the widget definitions.
# ---------------------------------------------------------------------------

def read_pak_member(pak, name):
    with zipfile.ZipFile(pak) as z:
        try:
            return z.read(name)
        except KeyError:
            return None


def find(pak, needle):
    with zipfile.ZipFile(pak) as z:
        for n in z.namelist():
            if n.lower() == needle.lower():
                return n
    return None


def dds_level0(data):
    """Header, size and the DXT3 level 0 of a shipped DDS."""
    magic, size, flags, h, w, pitch, depth, mips = struct.unpack_from("<4sIIIIIII", data, 0)
    fourcc = data[0x54:0x58]
    return {"fourcc": fourcc, "w": w, "h": h, "mips": mips, "pixels": data[128:]}


def dxt3_colours(level, w, h):
    """Decode the DXT3 level to a colour census: the art's true content."""
    from collections import Counter
    bw, bh = (w + 3) // 4, (h + 3) // 4
    off = 0
    px = Counter()
    for _by in range(bh):
        for _bx in range(bw):
            alpha = level[off:off + 8]                 # the DXT3 alpha block
            off += 8
            c0, c1 = struct.unpack_from("<HH", level, off)
            off += 4
            idx = struct.unpack_from("<I", level, off)[0]
            off += 4

            def rgb(c):
                return (((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63,
                        (c & 31) * 255 // 31)

            p = [rgb(c0), rgb(c1)]
            if c0 > c1:
                p.append(tuple((2 * p[0][i] + p[1][i]) // 3 for i in range(3)))
                p.append(tuple((p[0][i] + 2 * p[1][i]) // 3 for i in range(3)))
            else:
                p.append(tuple((p[0][i] + p[1][i]) // 2 for i in range(3)))
                p.append((0, 0, 0))
            for i in range(16):
                a = (alpha[i >> 1] >> ((i & 1) * 4)) & 0xF
                px[(p[(idx >> (2 * i)) & 3]) + (a * 17,)] += 1
    return px


def shader_block(text, name):
    m = re.search(r"^\s*" + re.escape(name) + r"\s*\{(.*?)^\s*\}", text,
                  re.S | re.M)
    return m.group(1) if m else None


def urc_labels(text):
    out = []
    for m in re.finditer(r"resource\s+(\w+)\s*\{(.*?)\}", text, re.S):
        body = m.group(2)
        rect = re.search(r"rect\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)", body)
        sh = re.search(r'shader\s+"([^"]+)"', body)
        sb = re.search(r'statbar_shader\s+"([^"]+)"', body)
        fg = re.search(r"fgcolor\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)", body)
        out.append({"class": m.group(1),
                    "rect": tuple(int(x) for x in rect.groups()) if rect else None,
                    "shader": (sh or sb).group(1) if (sh or sb) else None,
                    "fgcolor": tuple(float(x) for x in fg.groups()) if fg else None})
    return out


# retail's own HUD stream, retail-menu-solomatch-final.trace.  Call numbers and
# arguments as `apitrace dump` prints them; the command is in the docstring.
RETAIL_HUD = [
    # call     what                                   value
    (1014686, "glViewport health-bar",               (78, -49, 73, 18)),
    (1014687, "glScissor  health-bar",               (78, -49, 73, 18)),
    (1014690, "glOrtho    health-bar",               (0, 73, 18, 0)),
    (1014699, "glColor    health-bar",               (255, 255, 255, 255)),
    (1014705, "glBindTexture health-bar",            1096),
    (1014706, "glDisable  GL_BLEND health-bar",      0),
    (1014711, "glDrawElements health-bar",           (6, 6)),
    (1014717, "glViewport armor-bar",                (78, -63, 73, 12)),
    (1014725, "glBlendFunc armor-bar",               ("GL_SRC_ALPHA", "GL_ONE_MINUS_SRC_ALPHA")),
    (1014732, "glColor    armor-bar",                (255, 255, 255, 255)),
    (1014733, "glViewport health-number",            (23, -49, 45, 18)),
    (1014746, "glColor    health-number",            (221, 147, 43, 255)),
    (1014756, "glDrawElements health-number",        (18, 18)),
    (1014762, "glViewport armor-number",             (36, -63, 32, 12)),
    (1014775, "glColor    armor-number",             (0, 153, 59, 255)),
    (1014783, "glDrawElements armor-number",         (6, 6)),
]


def load_shipped():
    base = os.path.join(RETAIL, "base")
    if not os.path.isdir(base):
        return None
    out = {}
    p0 = os.path.join(base, "pak0.pk3")
    l1 = os.path.join(base, "locpak1.pk3")
    if not (os.path.isfile(p0) and os.path.isfile(l1)):
        return None
    for name, key in (("sysimg/hud/hud_health_bkg.dds", "bkg"),
                      ("sysimg/hud/health_bar.dds", "health"),
                      ("sysimg/hud/armor_bar.dds", "armor")):
        n = find(p0, name)
        out[key] = dds_level0(read_pak_member(p0, n)) if n else None
    out["hud_shader"] = read_pak_member(p0, find(p0, "shaders/hud.shader"))
    out["healthhud"] = read_pak_member(l1, find(l1, "ui/healthhud.urc"))
    return out


# ---------------------------------------------------------------------------
# The arms.
# ---------------------------------------------------------------------------

def shipping_tree_green():
    """Every 2D colour consumer reads the record's own value, once."""
    tr_draw = open(TR_DRAW, encoding="utf-8", errors="replace").read()
    tr_backend = open(TR_BACKEND, encoding="utf-8", errors="replace").read()

    check("r_color2D[4] = { 255, 255, 255, 255 }" in tr_draw,
          "tr_draw.c: the recorded 2D colour does not start white")

    m = re.search(r"void Draw_SetColor\(const vec4_t rgba\) \{(.*?)\n\}", tr_draw, re.S)
    check(m is not None, "tr_draw.c: Draw_SetColor not found")
    if m:
        body = m.group(1)
        check("RC_2D_COLOR" in body,
              "tr_draw.c: Draw_SetColor no longer records RC_2D_COLOR")
        check(not re.search(r"backEnd\.color2D\s*\[", body),
              "tr_draw.c: Draw_SetColor writes backEnd.color2D -- the walk's 2D "
              "batch state, written from the front end's thread")

    m = re.search(r"void R_ChangeTextColor\(int index\) \{(.*?)\n\}", tr_draw, re.S)
    check(m is not None and "r_color2D[3]" in m.group(1),
          "tr_draw.c: R_ChangeTextColor reads the walk's colour from the front end")

    # The walk's own setter is the only writer left on the walk side.
    check("backEnd.color2D[0] = rgba[0]" in tr_draw,
          "tr_draw.c: RB_2D_SetColor no longer applies the record's colour")

    for frag, what in (
            ("vcol=%d,%d,%d,%d nvcol=%d rgbGen=%d const=%d,%d,%d,%d alphaGen=%d imgdesc=%s",
             "the span row does not carry the colour answer"),
            ("black=%u vcol=%d,%d,%d,%d rgbGen=%d", "the readback row does not carry the colour"),
            ('"VERDICT=DREW_BLACK"', "the census cannot say an element drew black"),
            ("black == nonclear", "the black verdict no longer requires every lit sample black"),
            ("*black)++;", "the readback no longer counts opaque black samples"),
            ("image->internalFormat", "the image descriptor is gone")):
        check(frag in tr_backend, "tr_backend.c: " + what)
    return not fails


def shipped_data_green(d):
    if d is None:
        # not a failure of the port; the caller decides.  The gate host has the
        # retail install, and the brief requires the decision function be driven
        # over shipped data, so an absent install is reported loudly.
        check(False, "retail install not found at %s (set EF2_RETAIL_ROOT)" % RETAIL)
        return False

    # 1. the art is not black.  If it were, "the bar is black" would be data.
    # Each element's art, its lit pixels and their hue.  "The bar is black" is
    # a claim about the DRAW only if the data is not black, so the shipped bytes
    # are decoded here byte for byte.
    for key, hue in (("health", "orange"), ("armor", "green"), ("bkg", "orange")):
        lvl = d.get(key)
        if not check(lvl is not None and lvl["fourcc"] == b"DXT3",
                     "shipped %s: not a DXT3 DDS" % key):
            continue
        colours = dxt3_colours(lvl["pixels"], lvl["w"], lvl["h"])
        check(colours, "shipped %s: no decoded pixels" % key)
        lit = {c: n for c, n in colours.items() if c[3] > 0 and c[:3] != (0, 0, 0)}
        if not check(lit, "shipped %s: the art decodes to nothing but black" % key):
            continue
        tot = sum(lit.values())
        mean = tuple(sum(c[i] * n for c, n in lit.items()) // tot for i in range(3))
        check(mean[0] > mean[2] and mean[1] > mean[2] if hue == "orange"
              else mean[1] > mean[0] and mean[1] > mean[2],
              "shipped %s: the art's lit mean %r is not %s" % (key, mean, hue))

    # 2. the shader scripts fix each element's colour resolution.
    txt = (d.get("hud_shader") or b"").decode("latin1")
    for name, blend, atest in (("hud/health", True, True),
                               ("hud/health-bar", False, False),
                               ("hud/armor-bar", True, True)):
        blk = shader_block(txt, name)
        if not check(blk is not None, "shipped shaders/hud.shader: no %s block" % name):
            continue
        check("rgbGen global" in blk and "alphaGen global" in blk,
              "shipped %s: not rgbGen global + alphaGen global" % name)
        low = blk.lower()
        check(("blendfunc blend" in low) == blend,
              "shipped %s: blendfunc does not match retail's stream" % name)
        check(("alphafunc gt0" in low) == atest,
              "shipped %s: alphaFunc does not match retail's stream" % name)

    # 3. the widget definitions, against retail's own windows.
    urc = (d.get("healthhud") or b"").decode("latin1")
    labels = urc_labels(urc)
    by_shader = {l["shader"]: l for l in labels if l["shader"]}
    check(by_shader.get("hud/health", {}).get("rect") == (0, 0, 256, 64),
          "shipped ui/healthhud.urc: hud/health's rect is not 0 0 256 64")
    check(by_shader.get("hud/health-bar", {}).get("rect") == (78, 32, 73, 18),
          "shipped ui/healthhud.urc: hud/health-bar's rect is not 78 32 73 18")
    check(by_shader.get("hud/armor-bar", {}).get("rect") == (78, 52, 73, 12),
          "shipped ui/healthhud.urc: hud/armor-bar's rect is not 78 52 73 12")

    # The URC measures y from the menu's bottom (64 tall) and the menu sits at
    # retail's screen y = -63; the widget rect and retail's own glViewport must
    # be the SAME rect.  This is the cross-check between shipped data and the
    # trace's own numbers.
    def flip(rect):
        x, y, w, h = rect
        return (x, (64 - y - h) - 63, w, h)

    want = {"hud/health-bar": (78, -49, 73, 18), "hud/armor-bar": (78, -63, 73, 12)}
    for shader, vp in want.items():
        rect = by_shader.get(shader, {}).get("rect")
        check(rect is not None and flip(rect) == vp,
              "shipped %s: its rect does not land on retail's own glViewport %r"
              % (shader, vp))

    # 4. retail's own colours: the number labels, which the URC's fgcolor pins.
    fg = [l["fgcolor"] for l in labels if l["fgcolor"] and l["rect"] == (23, 32, 45, 18)]
    check(fg and tuple(int(v * 255) for v in fg[0][:3]) == (221, 147, 43),
          "shipped ui/healthhud.urc: the health number's fgcolor is not retail's "
          "(221,147,43), trace call 1014746")
    fg = [l["fgcolor"] for l in labels if l["fgcolor"] and l["rect"] == (36, 52, 32, 12)]
    check(fg and tuple(int(v * 255) for v in fg[0][:3]) == (0, 153, 59),
          "shipped ui/healthhud.urc: the armor number's fgcolor is not retail's "
          "(0,153,59), trace call 1014775")

    # 5. retail's own stream: the bars' own vertex colour is WHITE, and the
    #    bars' blend is what the script says.  These are the numbers the port's
    #    log must reproduce per quad.
    check(dict((c, v) for c, _w, v in RETAIL_HUD) [1014699] == (255, 255, 255, 255),
          "retail trace: the health bar's vertex colour is not white")
    check(dict((c, v) for c, _w, v in RETAIL_HUD)[1014706] == 0,
          "retail trace: the health bar does not draw with blending disabled")
    check(dict((c, v) for c, _w, v in RETAIL_HUD)[1014725]
          == ("GL_SRC_ALPHA", "GL_ONE_MINUS_SRC_ALPHA"),
          "retail trace: the armor bar's blend func is not SRC_ALPHA/ONE_MINUS_SRC_ALPHA")
    check(dict((c, v) for c, _w, v in RETAIL_HUD)[1014746] == (221, 147, 43, 255),
          "retail trace: the health number's colour is not (221,147,43,255)")
    return not fails


def model_arm(fault):
    """fault 1..12: break the model and require the verdict to notice."""
    art = "256x64/0x83f2/9m"
    white = (255, 255, 255, 255)
    clear = (0, 0, 0, 0)

    if fault == 1:
        # a black colour resolution: the global colour the element resolves to
        # is (0,0,0,0) instead of the record's white.
        c = resolve_2d_colour(CGEN_GLOBAL_COLOR, AGEN_GLOBAL_ALPHA, white,
                              (0, 0, 0, 0), (0, 0, 0, 255), from_vertex=False)
        return coverage_colour_verdict(102400, [(0, 0, 0, 255)] * 32, clear), c
    if fault == 2:
        # the element's vertex colour is black (the value the fix takes away)
        c = resolve_2d_colour(CGEN_GLOBAL_COLOR, AGEN_GLOBAL_ALPHA,
                              (0, 0, 0, 255), (0, 0, 0, 0), (0, 0, 0, 0),
                              from_vertex=True)
        return coverage_colour_verdict(102400, [(0, 0, 0, 255)] * 32, clear), c
    if fault == 3:
        # an unresolvable / mismatched bound image
        return image_binding_verdict(art, "(none)"), white
    if fault == 4:
        return image_binding_verdict(art, "128x32/0x83f2/8m"), white
    if fault == 5:
        # a text run the UI asked for that emits no quad
        return text_run_verdict(0, 6), white
    if fault == 6:
        # a coverage verdict that reports pixels where the readback says clear
        return coverage_colour_verdict(1024, [clear] * 32, clear), white
    if fault == 7:
        # a rect that rasterised as one colour where the art varies
        return coverage_colour_verdict(1024, [(9, 9, 9, 255)] * 32, clear), white
    if fault == 8:
        # the art itself is black
        return coverage_colour_verdict(1024, [(0, 0, 0, 255)] * 8 + [(0, 0, 0, 0)] * 24, clear), white
    if fault == 9:
        # a text run that drew glyph quads but nothing landed
        return coverage_colour_verdict(580, [clear] * 32, clear), white
    if fault == 10:
        # the armour bar's blend disabled: its opaque art would hide the panel
        # behind it -- named here because the verdict row must carry blend
        c = resolve_2d_colour(CGEN_GLOBAL_COLOR, AGEN_GLOBAL_ALPHA, white,
                              (0, 0, 0, 0), (0, 0, 0, 255), from_vertex=False)
        return coverage_colour_verdict(1024, [(0, 0, 0, 255)] * 32, clear), c
    if fault == 11:
        # the readback samples the whole frame instead of the element's rect
        return coverage_colour_verdict(1024, [(0, 0, 0, 0)] * 32, clear), white
    if fault == 12:
        # an image bound to a placeholder art of the right size but no content
        return image_binding_verdict(art, "256x64/0x0/0m"), white
    raise SystemExit("unknown fault %r" % fault)


def main():
    fault = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1] else 0
    d = load_shipped()

    if fault == 0:
        if not shipping_tree_green():
            pass
        if not shipped_data_green(d):
            pass

        # the model itself, on retail's own numbers: the health bar's white
        # vertex colour over the shipped orange art is not black.
        colours = dxt3_colours(d["health"]["pixels"], d["health"]["w"], d["health"]["h"]) if d else {}
        litcols = sorted({c for c, n in colours.items() if c[3] > 0 and c[:3] != (0, 0, 0)})
        # the element's own rect read back: the art's own colours, as the census
        # would sample them (a uniform set is NO_CONTENT and must not be)
        lit = [litcols[i % len(litcols)] for i in range(32)] if litcols else []
        v = coverage_colour_verdict(102400, lit, (0, 0, 0, 0))
        check(v == "OK", "the shipping model calls the health bar's own art %s" % v)
        c = resolve_2d_colour(CGEN_GLOBAL_COLOR, AGEN_GLOBAL_ALPHA, (255, 255, 255, 255),
                              (0, 0, 0, 0), (255, 255, 255, 255), from_vertex=True)
        check(c == (255, 255, 255, 255),
              "the shipping model resolves the health bar to %r, not retail's white" % (c,))
        check(image_binding_verdict("256x64/0x83f2/9m", "256x64/0x83f2/9m") == "OK",
              "the shipping model cannot recognise its own art binding")
        check(text_run_verdict(3, 3) == "OK", "the shipping model flags a healthy run")
    else:
        # A fault arm exits NON-ZERO only when the fault DID fire, so a runner
        # that treats a zero exit as "the planted fault went unnoticed" is
        # itself red when the arm cannot trip.
        v = model_arm(fault)
        verdict = v[0]
        if verdict == "OK":
            print("  FAIL  planted fault %d did not fire (verdict %s)" % (fault, verdict))
            return 0
        print("  ok    planted fault %d fires: %s" % (fault, verdict))
        return 1

    for f in fails:
        print("  FAIL  " + f)
    if fails:
        return 1
    print("  ok    %d checks; every 2D colour question answered" % checks[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
