#!/usr/bin/env python3
"""
d2window_control.py -- the 2D window, the placement and the writer census.

`Set2DWindow' and `RE_Scissor' are front-end entry points that drop a command
when the rect asked for is the one the cache already describes.  That is only
sound if the cache describes the GPU state the walked command buffer will have
at that point, so the cache has to be owned by the thread that records the
commands -- the front end -- and dropped at exactly two places:

    * the frame boundary, because a fresh command buffer has no viewport or
      scissor at all, so a frame whose first window request is elided draws its
      2D with whatever the buffer happens to carry;
    * after every 3D command, because the 3D pass overwrites both.

The reference twin (renderergles3) does both.  The shipping Vulkan lane dropped
the cache from RB_FrameOpen instead -- the WALK thread, one frame earlier under
r_smp -- so whether the front end's first window request of a frame saw a valid
cache was a race: the window was recorded on some frames and elided on others,
which is the flashing 2D the operator kept reporting.

It also pins the shape the trace gives the 2D pass itself.  Retail opens the pass
on the screen window -- retail-menu-solomatch-final.trace 10103-10109:
`glViewport(0, 0, 640, 480)`, `glMatrixMode(GL_PROJECTION)`, `glLoadIdentity()`,
`glOrtho(0, 640, 480, 0, -1, 1)`, `glMatrixMode(GL_MODELVIEW)`,
`glLoadIdentity()` -- and a TEXT/BOX widget keeps it, with `glVertex2f` in SCREEN
space and `glScissor` its only per-widget state: `(49,51,320,26)`,
`(424,53,123,26)`, `(371,49,256,32)`, `(458,393,138,32)`, `(44,393,138,32)`,
`(320,393,138,32)`, `(182,393,138,32)`, `(435,20,96,32)`, `(40,9,40,16)`,
`(20,9,25,16)`.

A MATERIAL widget does NOT keep it.  retail-menu-jul04.trace 12614-12620 gives
the first LCARS tile its OWN window -- `glViewport(0, 224, 256, 256)` +
`glOrtho(0, 256, 256, 0)` + `glScissor(0, 224, 256, 256)` -- and 12645-12649 is
the same for the next tile at x=256, 12707-12719 for the tile at (0,-32) with the
state block's clamped `glScissor(0,0,256,224)`.  So the port's one screen window
plus the placement it publishes has to reproduce a PER-WIDGET window, and the
checks below prove it does, on the tiles' own numbers.  (The claim this file used
to carry -- that retail had one window for the whole pass -- was read off the
pass opener, calls 10103-10124, and is refuted by the tile pass.)  The control
also forbids a degenerate-ortho branch and proves the placement is unchanged by
the rewrite.

THE PLACEMENT (the r14-round-7 defect).  The rewrite put the widget's placement
on the vertices, in ONE choke point per lane (`RB_2DPlaceVertex`, tr_util.cpp),
which every 2D vertex writer must end with:

    vulkan: RB_Vertex3fv/RB_Vertex3f, RB_ImmVertex2f/RB_ImmVertex3f,
            R_FontRunWrite, RB_FontRunDraw, R_DrawString_sgl
    gles3:  the same, plus RB_2DFontString and the legacy RB_StretchPic

Two defects made the menu render partially, mirrored upward, with no text:

  * `UIWidget::set2D` published the widget's GL-viewport box y
    (`vidHeight - size.height - pos.y - clippedorigin.y`) instead of the vertex
    offset.  The constant window's ortho is y-down (retail's
    `glOrtho(0, 640, 480, 0)`), so a vertex y already counts down from the
    pass's own origin; publishing the bottom-left box value flips it a second
    time and mirrors every widget about the middle of the screen.  Both axes
    are `m_screenframe.pos - m_clippedorigin`.  This control EVALUATES the two
    expressions out of the shipping `set2D` and puts the numbers through the
    renderer's real window+ortho transform, so the old expression is RED.
  * the font/glyph writers bypassed the choke point, so glyph quads stayed in
    widget-local coordinates and fell outside the widget's own scissor -- the
    text was clipped away entirely.

The census table below lists every function in either lane that writes a tess
vertex, with a verdict: `placed` (it must end with the choke point), `3d`
(world-space vertices, never reachable with backEnd.in2D) or `guard` (the
RB_EndSurface sentinel slot).  A writer that is not in the table is RED, so the
class cannot silently grow a bypass again.

Build/run: docs/controls/d2window/run.sh
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
WT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
CODE = os.path.join(WT, "app", "src", "main", "cpp", "code")

LANES = {
    "vulkan": {
        "dir": "renderervulkan",
        "origin": "vk_2dOrigin",
        "frame": os.path.join(CODE, "renderervulkan", "tr_init.c"),
        "backend": os.path.join(CODE, "renderervulkan", "tr_backend.c"),
        "cmds": os.path.join(CODE, "renderervulkan", "tr_backend.c"),
        "util": os.path.join(CODE, "renderervulkan", "tr_util.cpp"),
        "font": os.path.join(CODE, "renderervulkan", "tr_font.cp" + "p"),
        "draw": os.path.join(CODE, "renderervulkan", "tr_draw.c"),
        "vkdraw": os.path.join(CODE, "renderervulkan", "vk_draw.c"),
    },
    "gles3": {
        "dir": "renderergles3",
        "origin": "gles3_2dOrigin",
        "frame": os.path.join(CODE, "renderergles3", "tr_cmds.c"),
        "backend": os.path.join(CODE, "renderergles3", "tr_backend.c"),
        "cmds": os.path.join(CODE, "renderergles3", "tr_cmds.c"),
        "util": os.path.join(CODE, "renderergles3", "tr_util.cpp"),
        "font": os.path.join(CODE, "renderergles3", "tr_font.cpp"),
        "draw": os.path.join(CODE, "renderergles3", "tr_draw.c"),
        "vkdraw": os.path.join(CODE, "renderergles3", "tr_draw.c"),
    },
}

failures = 0
checks = 0


def check(cond, msg):
    global failures, checks
    checks += 1
    if not cond:
        failures += 1
        print("  FAIL  " + msg)


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def body(text, header):
    """The source of one function, from its header to the next top-level }."""
    i = text.find(header)
    if i < 0:
        return None
    j = text.find("\n}\n", i)
    return text[i:j + 3] if j > 0 else text[i:]


def model(invalidate_at_frame_start, invalidate_on_3d):
    """A frame's request stream: window A, 2D, a 3D command, window A, 2D.

    Returns the window requests the front end actually recorded."""
    cache = "stale-from-last-frame"        # the cache as the frame starts
    recorded = []
    if invalidate_at_frame_start:
        cache = None
    for req in ("win:A", "2d", "3d", "win:A", "2d"):
        if req.startswith("win:"):
            rect = req.split(":")[1]
            if cache != rect:
                recorded.append(rect)
                cache = rect
        elif req == "3d" and invalidate_on_3d:
            cache = None
    return recorded


# ---- a tiny evaluator for the shipping expressions --------------------------
#
# The placement and the scissor are single expressions in uilib/uiwidget.cpp.
# Reading them out and evaluating them is what makes this control exercise the
# artifact rather than a paraphrase of it: change the expression and the model
# moves with it, so the pre-fix expression is RED and a future edit is measured
# instead of assumed.

class ExprError(Exception):
    pass


def strip_casts(expr):
    return re.sub(r"\((?:int|float|double)\)", "", expr)


def split_args(argtext):
    """The top-level comma-separated arguments of an argument list."""
    out, depth, cur = [], 0, ""
    for ch in argtext:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur)
    return [a.strip() for a in out]


def call_args(text, call):
    """The arguments of the first `call(...)` in the text, or None."""
    i = text.find(call)
    while i >= 0:
        j = i + len(call)
        if j < len(text) and text[j] == "(":
            break
        i = text.find(call, i + 1)
    if i < 0:
        return None
    depth, k = 0, j
    while k < len(text):
        if text[k] == "(":
            depth += 1
        elif text[k] == ")":
            depth -= 1
            if depth == 0:
                break
        k += 1
    return split_args(text[j + 1:k])


def eval_expr(expr, sym):
    """Evaluate a C arithmetic expression over a symbol table of floats."""
    toks = re.findall(r"\d+\.?\d*(?:[eE][-+]?\d+)?|[A-Za-z_][A-Za-z0-9_.]*|[()+\-*/]", expr)
    pos = [0]

    def peek():
        return toks[pos[0]] if pos[0] < len(toks) else None

    def eat(tok=None):
        x = peek()
        if tok is not None and x != tok:
            raise ExprError("expected %r, got %r, in %r" % (tok, x, expr))
        pos[0] += 1
        return x

    def primary():
        x = peek()
        if x is None:
            raise ExprError("unexpected end of %r" % expr)
        if x == "(":
            eat("(")
            v = sumexpr()
            eat(")")
            return v
        if x == "-":
            eat("-")
            return -primary()
        if x == "+":
            eat("+")
            return primary()
        if re.match(r"^\d", x):
            eat()
            return float(x)
        if x in sym:
            eat()
            return sym[x]
        raise ExprError("unknown symbol %r in %r" % (x, expr))

    def term():
        v = primary()
        while peek() in ("*", "/"):
            op = eat()
            r = primary()
            v = v * r if op == "*" else v / r
        return v

    def sumexpr():
        v = term()
        while peek() in ("+", "-"):
            op = eat()
            r = term()
            v = v + r if op == "+" else v - r
        return v

    v = sumexpr()
    if pos[0] != len(toks):
        raise ExprError("trailing tokens in %r" % expr)
    return v


def set2d_origin(set2d, sym):
    """(ox, oy) exactly as the shipping set2D publishes them."""
    a = call_args(set2d, "uii.Rend_Set2DOrigin")
    if not a or len(a) != 2:
        raise ExprError("no two-argument Rend_Set2DOrigin in set2D")
    return eval_expr(strip_casts(a[0]), sym), eval_expr(strip_casts(a[1]), sym)


def set2d_window(set2d, sym):
    """x, y, w, h, left, right, bottom, top as the shipping set2D passes them."""
    a = call_args(set2d, "uii.Rend_Set2D")
    if not a or len(a) < 8:
        raise ExprError("no window tuple in set2D")
    return [eval_expr(strip_casts(x), sym) for x in a[:8]]


def set2d_scissor(set2d, sym):
    a = call_args(set2d, "uii.Rend_Scissor")
    if not a or len(a) != 4:
        raise ExprError("no four-argument Rend_Scissor in set2D")
    return [eval_expr(strip_casts(x), sym) for x in a]


# ---- the renderer's own transform -------------------------------------------
#
# window = (x, y, w, h) is the GL bottom-left viewport box, ortho =
# (left, right, bottom, top) the projection range, and a vertex is in the
# window's own coordinates.  Both lanes compose to this: gles3 uses the standard
# glOrtho with the GL viewport; the Vulkan lane writes the ortho with its y rows
# negated (m[5] = -2/tb, m[13] = (top+bottom)/tb in VK_Draw_Set2D) because
# Vulkan's viewport is top-left -- the two negations cancel.  The result is
#   fb_x = win.x + (vx - left) / (right - left) * win.w
#   fb_y = (H - win.y - win.h) + (vy - top) / (bottom - top) * win.h
# with fb_y counting DOWN from the framebuffer top, which is the space the
# widget's own screenframe lives in.

def fb_point(window, ortho, vx, vy, H):
    x, y, w, h = window
    left, right, bottom, top = ortho
    fx = x + (vx - left) / (right - left) * w
    fy = (H - y - h) + (vy - top) / (bottom - top) * h
    return fx, fy


def main():
    # `report` prints the per-resolution evidence the checks otherwise only
    # assert: the pillarbox rect, the UI scale, and a worked widget's placed
    # rect and scissor at 800x600, 1280x720, 1920x1080 and 1920x1200.
    report = len(sys.argv) > 1 and sys.argv[1] == "report"
    mutant = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 0

    for lane, paths in LANES.items():
        frame = read(paths["frame"])
        backend = read(paths["backend"])
        cmds = read(paths["cmds"])

        # --- the front end's frame boundary -------------------------------
        site = None
        for fn in ("RE_EndFrame", "RE_BeginFrame", "RE_EndFrame"):
            b = body(frame, "void %s(" % fn)
            if b and "R_InitNextFrame" in b:
                site = b
                break
        has_frame_drop = bool(site) and "R_Invalidate2DWindowCache();" in site
        if mutant == 1:
            has_frame_drop = False          # planted: the boundary drop removed
        check(has_frame_drop,
              "%s: the frame boundary does not drop the 2D window cache after "
              "R_InitNextFrame, so a frame's first window request can be elided" % lane)

        # --- the walk must not touch it -----------------------------------
        fr = body(backend, "void RB_FrameOpen(") if "RB_FrameOpen" in backend else None
        if fr is None and lane == "gles3":
            fr = body(cmds, "void RB_FrameOpen(")
        walk_touches = bool(fr) and "R_Invalidate2DWindowCache();" in fr
        if mutant == 3:
            walk_touches = True             # planted: the racy walk-side drop back
        check(not walk_touches,
              "%s: RB_FrameOpen drops the 2D window cache from the walk thread, "
              "racing the front end's recording of the next frame" % lane)

        # --- every 3D command drops it -------------------------------------
        for fn in ("R_AddDrawSurfCmd", "R_AddSpriteSurfCmd"):
            b = body(cmds, "void\t%s(" % fn) or body(cmds, "void %s(" % fn)
            has = bool(b) and "R_Invalidate2DWindowCache();" in b
            if mutant == 2:
                has = False
            check(has, "%s: %s does not drop the 2D window cache" % (lane, fn))

        # --- the behavioural model ----------------------------------------
        frame_drop = has_frame_drop
        on3d = not (mutant == 2)
        got = model(frame_drop, on3d)
        check(len(got) >= 1,
              "%s: a frame's 2D request stream recorded %d window commands -- the "
              "2D draws with no viewport set" % (lane, len(got)))
        check(got.count("A") == 2 or frame_drop is False,
              "%s: the window after the 3D command was elided (%r)" % (lane, got))
        if frame_drop and on3d:
            check(got == ["A", "A"],
                  "%s: expected both window requests recorded, got %r" % (lane, got))

    # --- retail's 2D shape (trace 10103-10124) ----------------------------
    #
    # The scissor rects the trace sets, in the 640x480 virtual space the UI uses.
    TRACE_SCISSORS = (
        (49, 51, 320, 26), (424, 53, 123, 26), (371, 49, 256, 32),
        (458, 393, 138, 32), (44, 393, 138, 32), (320, 393, 138, 32),
        (182, 393, 138, 32), (435, 20, 96, 32), (40, 9, 40, 16), (20, 9, 25, 16),
    )

    uiw = read(os.path.join(CODE, "uilib", "uiwidget.cpp"))
    set2d = body(uiw, "void UIWidget::set2D(void)")

    # the constant screen window, then the widget's placement, then its scissor
    check("uid.originX," in set2d and "uid.vidWidth," in set2d,
          "UIWidget::set2D does not ask for the pass's screen window")
    if mutant == 4:
        set2d = set2d.replace("0, uid.vidWidth, uid.vidHeight, 0,",
                              "m_clippedorigin.x, m_clippedorigin.x + m_screenframe.size.width, "
                              "m_clippedorigin.y + m_screenframe.size.height, m_clippedorigin.y,")
    if mutant == 5:
        set2d = set2d.replace("uii.Rend_Set2DOrigin(", "uii.Rend_Set2DOriginDropped(")
    if mutant == 7:
        # planted: the r14-round-7 y, the widget's GL-viewport box instead of the
        # vertex offset
        set2d = set2d.replace("m_screenframe.pos.y - m_clippedorigin.y",
                              "(float)uid.vidHeight - m_screenframe.size.height "
                              "- m_screenframe.pos.y - m_clippedorigin.y")
    if mutant == 11:
        # planted: the placement loses the clipped origin on x
        set2d = set2d.replace("m_screenframe.pos.x - m_clippedorigin.x",
                              "m_screenframe.pos.x")
    check("0, uid.vidWidth, uid.vidHeight, 0," in set2d,
          "the 2D window's ortho is not the constant screen ortho (the trace's "
          "glOrtho(0, 640, 480, 0, -1, 1))")
    check("uii.Rend_Set2DOrigin(" in set2d,
          "the widget does not publish its placement, so every widget would draw "
          "at the screen origin")
    check("m_screenframe.pos.x - m_clippedorigin.x" in set2d,
          "the published placement is not the widget's screen position (the "
          "placement the per-widget window used to carry in its ortho)")
    check("uii.Rend_Scissor(" in set2d,
          "the widget does not clip itself, and the scissor is the only per-widget "
          "state retail has")
    check("m_clippedorigin.x,\n        m_clippedorigin.x + m_screenframe.size.width," not in set2d,
          "a widget still passes its own frame as the window's ortho")

    for (sx, sy, sw, sh) in TRACE_SCISSORS:
        check(sw > 0 and sh > 0,
              "the trace's scissor (%d,%d,%d,%d) is empty, so a draw would vanish" % (sx, sy, sw, sh))

    # --- the placement the rewrite promises, EVALUATED from the shipping text
    #
    # The pre-rewrite shape is the reference (4631f620^:uilib/uiwidget.cpp
    # set2D and the renderer it fed): the window's viewport is the widget's own
    # screen rect and the ortho is [clippedorigin, clippedorigin + size].  The
    # rewrite must put the same vertex at the same framebuffer pixel through the
    # pass's one screen window plus the published origin.  Every number on the
    # new side comes out of the shipping expressions.
    origin_ok = True
    try:
        for (sx, sy, sw, sh) in TRACE_SCISSORS:
            for (px, py) in ((sx, sy), (sx + 7, sy + 3), (sx + sw - 1, sy + sh - 1)):
                for (vx, vy) in ((0.0, 0.0), (5.0, 1.2), (sw - 1.0, sh - 1.0)):
                    for (cox, coy) in ((0.0, 0.0), (-3.0, -2.0)):
                        W, H = float(sw), float(sh)
                        vidW, vidH, originX, originY, surfaceH = 640.0, 480.0, 0.0, 40.0, 480.0
                        sframe = (float(px), float(py), W, H)
                        clip = (cox, coy)
                        sym = {
                            "m_screenframe.pos.x": px, "m_screenframe.pos.y": py,
                            "m_screenframe.size.width": W, "m_screenframe.size.height": H,
                            "m_clippedorigin.x": cox, "m_clippedorigin.y": coy,
                            "m_clippedframe.pos.x": float(px), "m_clippedframe.pos.y": float(py),
                            "m_clippedframe.size.width": W, "m_clippedframe.size.height": H,
                            "uid.vidWidth": vidW, "uid.vidHeight": vidH,
                            "uid.originX": originX, "uid.originY": originY,
                            "uid.surfaceHeight": surfaceH,
                        }
                        # the pre-rewrite window + ortho
                        old_win = (originX + sframe[0],
                                   surfaceH - originY - (H + sframe[1]), W, H)
                        old_ortho = (clip[0], clip[0] + W, clip[1] + H, clip[1])
                        old = fb_point(old_win, old_ortho, vx, vy, H)
                        # the shipped window + the shipped origin
                        ox, oy = set2d_origin(set2d, sym)
                        win = set2d_window(set2d, sym)
                        new = fb_point(tuple(win[0:4]), tuple(win[4:8]), vx + ox, vy + oy, H)
                        check(abs(old[0] - new[0]) < 1e-4 and abs(old[1] - new[1]) < 1e-4,
                              "the rewrite moved a vertex: old=(%g,%g) new=(%g,%g) at "
                              "clip (%d,%d,%d,%d) origin (%.0f,%.0f)"
                              % (old[0], old[1], new[0], new[1], sx, sy, sw, sh, cox, coy))

        # retail's MATERIAL widgets: each sets its OWN window, one per widget
        # (retail-menu-jul04.trace 12614-12620 for the first tile, 12645-12649 for
        # the second, 12707-12719 for the fourth, 12800-12812 for the logo).  The
        # port's one screen window plus the shipped placement must land the same
        # local vertex on the same framebuffer pixel as that per-widget window,
        # and these are the rects the operator's missing-LCARS frame is made of.
        RETAIL_WIDGET_WINDOWS = (
            ("bkg_01", (0, 224, 256, 256), (0, 224, 256, 256)),
            ("bkg_02", (256, 224, 256, 256), (256, 224, 256, 256)),
            ("bkg_03", (512, 224, 128, 256), (512, 224, 128, 256)),
            ("bkg_04", (0, -32, 256, 256), (0, 0, 256, 224)),
            ("bkg_05", (256, -32, 256, 256), (256, 0, 256, 224)),
            ("bkg_06", (512, -32, 128, 256), (512, 0, 128, 224)),
            ("logo", (174, 375, 288, 133), (174, 375, 288, 105)),
        )
        for (w_name, vp, wscn) in RETAIL_WIDGET_WINDOWS:
            vx0, vy0, vw, vh = vp
            check(vw > 0 and vh > 0 and wscn[2] > 0 and wscn[3] > 0,
                  "retail's %s window/scissor is degenerate" % w_name)
            vidW, vidH, originX, originY, surfaceH = 640.0, 480.0, 0.0, 0.0, 480.0
            sf = (float(vx0), surfaceH - float(vy0) - float(vh), float(vw), float(vh))
            sym = {
                "m_screenframe.pos.x": sf[0], "m_screenframe.pos.y": sf[1],
                "m_screenframe.size.width": sf[2], "m_screenframe.size.height": sf[3],
                "m_clippedorigin.x": 0.0, "m_clippedorigin.y": 0.0,
                "m_clippedframe.pos.x": sf[0], "m_clippedframe.pos.y": sf[1],
                "m_clippedframe.size.width": sf[2], "m_clippedframe.size.height": sf[3],
                "uid.vidWidth": vidW, "uid.vidHeight": vidH,
                "uid.originX": originX, "uid.originY": originY,
                "uid.surfaceHeight": surfaceH,
            }
            ox, oy = set2d_origin(set2d, sym)
            win = set2d_window(set2d, sym)
            for (vx, vy) in ((0.0, 0.0), (1.0, 1.0), (float(vw), float(vh))):
                # retail: the widget's own window, its local ortho, local vertex
                old = fb_point((float(vx0), float(vy0), float(vw), float(vh)),
                               (0.0, float(vw), float(vh), 0.0), vx, vy, float(surfaceH))
                # the port: the pass's screen window and the shipped placement
                new = fb_point(tuple(win[0:4]), tuple(win[4:8]),
                               vx + ox, vy + oy, float(surfaceH))
                check(abs(old[0] - new[0]) < 1e-4 and abs(old[1] - new[1]) < 1e-4,
                      "retail's %s per-widget window and the port's screen window + "
                      "placement disagree at local (%g,%g): retail=(%g,%g) "
                      "port=(%g,%g)" % (w_name, vx, vy, old[0], old[1], new[0], new[1]))

    # and the widget itself: its own local top-left (clippedorigin) must land
    # on the widget's screenframe, which is the mirror the pre-fix y broke.
        # Numbers from the operator's run-20261015-105154 log at 800x600: the
        # menu/main/credits button, authored rect (371,399,256,32) in the 640x480
        # space (locpak1.pk3 loc/Eng/ui/menu_main.urc:225) -> screenframe
        # (463.75,498.75,320,40), scissor (464,61,320,40) in the trace's own
        # bottom-left glScissor space (trace scissor (371,49,256,32) at 1.25).
        MENU = [("credits button", (463.75, 498.75, 320.0, 40.0), (0.0, 0.0)),
                ("inset label", (530.0, 501.25, 154.0, 32.5), (-20.0, -10.0)),
                ("fullscreen fill", (0.0, 0.0, 800.0, 600.0), (0.0, 0.0))]
        for (name, sf, clip) in MENU:
            sym = {
                "m_screenframe.pos.x": sf[0], "m_screenframe.pos.y": sf[1],
                "m_screenframe.size.width": sf[2], "m_screenframe.size.height": sf[3],
                "m_clippedorigin.x": clip[0], "m_clippedorigin.y": clip[1],
                "m_clippedframe.pos.x": sf[0], "m_clippedframe.pos.y": sf[1],
                "m_clippedframe.size.width": sf[2], "m_clippedframe.size.height": sf[3],
                "uid.vidWidth": 800.0, "uid.vidHeight": 600.0,
                "uid.originX": 0.0, "uid.originY": 0.0, "uid.surfaceHeight": 600.0,
            }
            ox, oy = set2d_origin(set2d, sym)
            win = set2d_window(set2d, sym)
            fx, fy = fb_point(tuple(win[0:4]), tuple(win[4:8]), clip[0] + ox, clip[1] + oy, 600.0)
            check(abs(fx - sf[0]) < 0.01 and abs(fy - sf[1]) < 0.01,
                  "%s: the widget's own top-left lands at (%.2f,%.2f), not on its "
                  "screenframe (%.2f,%.2f) -- the placement is mirrored"
                  % (name, fx, fy, sf[0], sf[1]))
    except ExprError as e:
        origin_ok = False
        check(False, "the shipping placement expression could not be evaluated: %s" % e)

    # the scissor expression must cover the widget's clipped frame, in the same
    # bottom-left space the trace's glScissor uses
    try:
        sym = {
            "m_screenframe.pos.x": 463.75, "m_screenframe.pos.y": 498.75,
            "m_screenframe.size.width": 320.0, "m_screenframe.size.height": 40.0,
            "m_clippedorigin.x": 0.0, "m_clippedorigin.y": 0.0,
            "m_clippedframe.pos.x": 463.75, "m_clippedframe.pos.y": 498.75,
            "m_clippedframe.size.width": 320.0, "m_clippedframe.size.height": 40.0,
            "uid.vidWidth": 800.0, "uid.vidHeight": 600.0,
            "uid.originX": 0.0, "uid.originY": 0.0, "uid.surfaceHeight": 600.0,
        }
        s = set2d_scissor(set2d, sym)
        check(abs(s[0] - 463.75) < 0.5 and abs(s[1] - 61.25) < 0.5
              and abs(s[2] - 320.0) < 0.5,
              "the scissor for the credits button is %r, not the trace's "
              "(371,49,256,32) scaled to 800x600 -- (464,61,320,40) in glScissor "
              "space" % (s,))
    except ExprError as e:
        check(False, "the shipping scissor expression could not be evaluated: %s" % e)

    # --- EVERY RESOLUTION: the pillarbox, the scale and the placement ------
    #
    # The UI lives in the 640x480 virtual space and the client pillarboxes it:
    # SCR_AndroidDisplayRect (client/cl_scrn.cpp:51) centres a 16:9 (mode 1) or
    # 4:3 (mode 2) rect inside the surface, and CL_FillUIDef
    # (client/cl_ui.cpp:2245) puts that rect into uid.originX/Y, uid.vidWidth/
    # vidHeight and uid.scaleRes = vid/640, vid/480.  A widget's authored frame
    # is scaled by scaleRes (UIWidget::ChangeResolution, uiwidget.cpp:3033-3036),
    # so the placement has to hold for every (dx, dy, dw, dh) the division can
    # produce.  The pillarbox numbers below are the shipped algorithm, pinned by
    # its source; the placement numbers are the shipped expressions.
    scrn = read(os.path.join(CODE, "client", "cl_scrn.cpp"))
    pbody = body(scrn, "void SCR_AndroidDisplayRect(")
    check(pbody is not None and "16.0f / 9.0f" in pbody and "4.0f / 3.0f" in pbody
          and "( surfaceW - w ) / 2" in pbody and "( surfaceH - h ) / 2" in pbody,
          "SCR_AndroidDisplayRect is no longer the centred pillarbox division the "
          "resolution model reproduces")
    change = body(uiw, "void UIWidget::ChangeResolution(")
    check(change is not None
          and "(float)(int)(0.5f + scalex * m_authoredFrame.pos.x)" in change
          and "(float)(int)(0.5f + scaley * m_authoredFrame.pos.y)" in change,
          "ChangeResolution no longer scales the authored frame by scaleRes, so the "
          "resolution model below is measuring a different widget")

    def pillarbox(W, H, mode):
        """SCR_AndroidDisplayRect, transcribed."""
        if mode == 1:
            target = 16.0 / 9.0
        elif mode == 2:
            target = 4.0 / 3.0
        else:
            return 0, 0, W, H
        if W <= 0 or H <= 0:
            return 0, 0, W, H
        if float(W) / float(H) > target:
            w = max(1, int(H * target + 0.5))
            return (W - w) // 2, 0, w, H
        h = max(1, int(W / target + 0.5))
        return 0, (H - h) // 2, W, h

    def change_resolution(ax, ay, aw, ah, sx, sy):
        """UIWidget::ChangeResolution:3033-3036, transcribed."""
        nx = float(int(0.5 + sx * ax))
        ny = float(int(0.5 + sy * ay))
        nw = float(int(0.5 + (sx * (aw + ax) - nx)))
        nh = float(int(0.5 + (sy * (ah + ay) - ny)))
        return nx, ny, nw, nh

    RESOLUTIONS = ((800, 600, 0), (800, 600, 2), (1280, 720, 1), (1920, 1080, 1),
                   (1920, 1080, 0), (1920, 1200, 1), (1920, 1200, 2), (1920, 1200, 0))
    if report:
        print("resolution   mode  pillarbox(dx,dy,dw,dh)  scale(vidw/640,vidh/480)  "
              "sample scissor (bottom-left)  placed rect (top-left)")
    for (W, H, mode) in RESOLUTIONS:
        dx, dy, dw, dh = pillarbox(W, H, mode)
        sxr, syr = float(dw) / 640.0, float(dh) / 480.0
        sample = None
        for (sx, sy, sw, sh) in TRACE_SCISSORS:
            # the trace's scissor is bottom-left in the 640x480 space
            ax, ay = float(sx), 480.0 - float(sy) - float(sh)
            fx, fy, fw, fh = change_resolution(ax, ay, float(sw), float(sh), sxr, syr)
            sym = {
                "m_screenframe.pos.x": fx, "m_screenframe.pos.y": fy,
                "m_screenframe.size.width": fw, "m_screenframe.size.height": fh,
                "m_clippedorigin.x": 0.0, "m_clippedorigin.y": 0.0,
                "m_clippedframe.pos.x": fx, "m_clippedframe.pos.y": fy,
                "m_clippedframe.size.width": fw, "m_clippedframe.size.height": fh,
                "uid.vidWidth": float(dw), "uid.vidHeight": float(dh),
                "uid.originX": float(dx), "uid.originY": float(dy),
                "uid.surfaceHeight": float(H),
            }
            try:
                ox, oy = set2d_origin(set2d, sym)
                win = set2d_window(set2d, sym)
                scn = set2d_scissor(set2d, sym)
            except ExprError as e:
                check(False, "the placement expression failed at %dx%d mode %d: %s"
                      % (W, H, mode, e))
                continue
            check(abs(win[0] - dx) < 0.01 and abs(win[1] - (H - dy - dh)) < 0.01
                  and abs(win[2] - dw) < 0.01 and abs(win[3] - dh) < 0.01,
                  "the pass window at %dx%d mode %d is %r, not the pillarbox rect "
                  "(%d,%d,%d,%d)" % (W, H, mode, win[0:4], dx, H - dy - dh, dw, dh))
            p0 = fb_point(tuple(win[0:4]), tuple(win[4:8]), 0.0 + ox, 0.0 + oy, float(H))
            p1 = fb_point(tuple(win[0:4]), tuple(win[4:8]), fw + ox, fh + oy, float(H))
            rect = (min(p0[0], p1[0]), min(p0[1], p1[1]), max(p0[0], p1[0]), max(p0[1], p1[1]))
            check(abs(rect[0] - (dx + fx)) < 0.01 and abs(rect[1] - (dy + fy)) < 0.01
                  and abs(rect[2] - (dx + fx + fw)) < 0.01
                  and abs(rect[3] - (dy + fy + fh)) < 0.01,
                  "at %dx%d mode %d a widget at (%g,%g) %gx%g lands at "
                  "(%.2f,%.2f)-(%.2f,%.2f), not on its own frame/scale"
                  % (W, H, mode, fx, fy, fw, fh, rect[0], rect[1], rect[2], rect[3]))
            check(abs(scn[0] - (dx + fx)) < 0.01 and abs(scn[1] - (H - dy - fh - fy)) < 0.01
                  and abs(scn[2] - fw) < 0.01 and abs(scn[3] - fh) < 0.01,
                  "at %dx%d mode %d the widget's scissor %r does not cover its own "
                  "frame" % (W, H, mode, scn))
            if report and (sx, sy, sw, sh) == TRACE_SCISSORS[0]:
                sample = (scn, rect)
        if report and sample:
            scn, rect = sample
            print("%4dx%-6d %-5d (%d,%d,%d,%d)%-8s %.3f,%.3f%-14s "
                  "(%g,%g,%g,%g)   (%.1f,%.1f)-(%.1f,%.1f)"
                  % (W, H, mode, dx, dy, dw, dh, "", sxr, syr, "",
                     scn[0], scn[1], scn[2], scn[3], rect[0], rect[1], rect[2], rect[3]))

    # --- ui_hudScale > 1: the placement is an offset, NOT a scaled quantity --
    #
    # A noresize widget is authored in screen pixels; ChangeResolution scales its
    # frame by s_hudScaleValue and the font is told the same factor
    # (uiwidget.cpp:3013-3024), while UIFont::Print divides the draw origin by
    # that factor because the renderer multiplies the glyph's own coordinates by
    # it (uifont.cpp:131-141).  So a glyph vertex is local*scale and the widget's
    # placement is added AFTER it: exactly the order the glyph writers now use.
    # A widget away from the origin is what makes the wrong order visible.
    for (s, W, H, mode) in ((1.2, 800, 600, 0), (1.5, 1280, 720, 1),
                            (2.0, 1920, 1080, 1), (2.0, 1920, 1200, 2)):
        dx, dy, dw, dh = pillarbox(W, H, mode)
        authored = (400.0, 300.0, 120.0, 40.0)
        fx, fy, fw, fh = change_resolution(authored[0], authored[1], authored[2],
                                           authored[3], s, s)
        sym = {
            "m_screenframe.pos.x": fx, "m_screenframe.pos.y": fy,
            "m_screenframe.size.width": fw, "m_screenframe.size.height": fh,
            "m_clippedorigin.x": 0.0, "m_clippedorigin.y": 0.0,
            "m_clippedframe.pos.x": fx, "m_clippedframe.pos.y": fy,
            "m_clippedframe.size.width": fw, "m_clippedframe.size.height": fh,
            "uid.vidWidth": float(dw), "uid.vidHeight": float(dh),
            "uid.originX": float(dx), "uid.originY": float(dy),
            "uid.surfaceHeight": float(H),
        }
        try:
            ox, oy = set2d_origin(set2d, sym)
            win = set2d_window(set2d, sym)
        except ExprError as e:
            check(False, "the placement expression failed at ui_hudScale %g: %s" % (s, e))
            continue
        # a glyph run inside the widget: its own coordinates are scaled by s, the
        # placement is not
        local = (0.25 * fw, 0.25 * fh, 0.75 * fw, 0.75 * fh)
        for (tag, add_x, add_y) in (("placed", ox, oy), ("pre-scale", 0.0, 0.0)):
            for mul in ((1.0,) if tag == "placed" else (1.0,)):
                q0 = fb_point(tuple(win[0:4]), tuple(win[4:8]),
                              local[0] * mul + add_x, local[1] * mul + add_y, float(H))
                q1 = fb_point(tuple(win[0:4]), tuple(win[4:8]),
                              local[2] * mul + add_x, local[3] * mul + add_y, float(H))
                inside = (min(q0[0], q1[0]) >= dx + fx - 0.01
                          and min(q0[1], q1[1]) >= dy + fy - 0.01
                          and max(q0[0], q1[0]) <= dx + fx + fw + 0.01
                          and max(q0[1], q1[1]) <= dy + fy + fh + 0.01)
                if tag == "placed":
                    check(inside,
                          "at ui_hudScale %g (%dx%d mode %d) a glyph run inside the "
                          "widget lands outside it: (%.1f,%.1f)-(%.1f,%.1f) vs "
                          "(%.1f,%.1f,%.1f,%.1f)"
                          % (s, W, H, mode, min(q0[0], q1[0]), min(q0[1], q1[1]),
                             max(q0[0], q1[0]), max(q0[1], q1[1]),
                             dx + fx, dy + fy, fw, fh))
                else:
                    # the wrong order: the placement is scaled with the glyphs,
                    # i.e. vertex = (local + placement) * scale
                    pre = fb_point(tuple(win[0:4]), tuple(win[4:8]),
                                   (local[0] + ox) * s, (local[1] + oy) * s, float(H))
                    pre_inside = (dx + fx - 0.01 <= pre[0] <= dx + fx + fw + 0.01
                                  and dy + fy - 0.01 <= pre[1] <= dy + fy + fh + 0.01)
                    check(not pre_inside,
                          "at ui_hudScale %g the pre-scale placement would still "
                          "land inside the widget, so this case cannot tell the two "
                          "orders apart" % s)

    # --- THE FONT PATH: a glyph quad must land inside its widget's clip ----
    #
    # Numbers from the operator's run-20261015-105154 game.log at 800x600 (with
    # the retail 640x480 space times the 800/640 pillarbox factor): the Credits
    # button is authored rect (371,399,256,32) in
    # locpak1.pk3 loc/Eng/ui/menu_main.urc:225, so its y-down screenframe is
    # (463.75,498.75,320,40) and its glScissor box is (464,61,320,40) -- the
    # trace's own (371,49,256,32).  Its label's glyph run reports the local rect
    # (41.2,5.0)-(113.8,28.8) and draws under that same scissor.  The placed run
    # must fall inside the clip; the unplaced (widget-local) run cannot, which is
    # why the missing placement was not "text in the wrong place" but NO text.
    # The run's effective placement is taken from the writer source, so a writer
    # that drops the choke point is RED here as well.
    GLYPH_LOCAL = (41.2, 5.0, 113.8, 28.8)
    BUTTON_CLIP = (464.0, 61.0, 320.0, 40.0)
    LABEL_FRAME = (530.0, 501.25, 154.0, 32.5)
    for lane, paths in LANES.items():
        font_text = read(paths["font"])
        headers = ["static void R_FontRunWrite("]
        if lane == "vulkan":
            headers.append("void RB_FontRunDraw(")
        else:
            headers.append("const void *RB_2DFontString(")
            font_text = font_text + read(paths["backend"])
        headers.append("void R_DrawString_sgl(")
        for header in headers:
            src = font_text if header != "const void *RB_2DFontString(" else read(paths["backend"])
            b = body(src, header)
            if not b:
                continue
            places = "RB_2DPlaceRange(" in b and "backEnd.in2D" in body(read(paths["util"]),
                                                                       "void RB_2DPlaceVertex(")
            if mutant == 8:
                places = False     # planted: the glyph writer bypasses the placement
            sym = {
                "m_screenframe.pos.x": LABEL_FRAME[0], "m_screenframe.pos.y": LABEL_FRAME[1],
                "m_screenframe.size.width": LABEL_FRAME[2],
                "m_screenframe.size.height": LABEL_FRAME[3],
                "m_clippedorigin.x": 0.0, "m_clippedorigin.y": 0.0,
                "m_clippedframe.pos.x": LABEL_FRAME[0], "m_clippedframe.pos.y": LABEL_FRAME[1],
                "m_clippedframe.size.width": LABEL_FRAME[2],
                "m_clippedframe.size.height": LABEL_FRAME[3],
                "uid.vidWidth": 800.0, "uid.vidHeight": 600.0,
                "uid.originX": 0.0, "uid.originY": 0.0, "uid.surfaceHeight": 600.0,
            }
            # the shipping placement, and the placement a bypassing writer would
            # have (none at all)
            ox, oy = set2d_origin(set2d, sym)
            if not places:
                ox, oy = 0.0, 0.0
            win = set2d_window(set2d, sym)
            fx0, fy0 = fb_point(tuple(win[0:4]), tuple(win[4:8]),
                                GLYPH_LOCAL[0] + ox, GLYPH_LOCAL[1] + oy, 600.0)
            fx1, fy1 = fb_point(tuple(win[0:4]), tuple(win[4:8]),
                                GLYPH_LOCAL[2] + ox, GLYPH_LOCAL[3] + oy, 600.0)
            # fb_point counts down; glScissor (and the trace) count up
            gx0, gy0 = min(fx0, fx1), 600.0 - max(fy0, fy1)
            gx1, gy1 = max(fx0, fx1), 600.0 - min(fy0, fy1)
            inside = (gx0 >= BUTTON_CLIP[0] and gy0 >= BUTTON_CLIP[1]
                      and gx1 <= BUTTON_CLIP[0] + BUTTON_CLIP[2]
                      and gy1 <= BUTTON_CLIP[1] + BUTTON_CLIP[3])
            check(inside,
                  "%s: %s places the glyph run at (%.1f,%.1f)-(%.1f,%.1f), outside "
                  "its clip (%.0f,%.0f,%.0f,%.0f) in glScissor space -- the run is "
                  "clipped away, which is the menu's missing text"
                  % (lane, header, gx0, gy0, gx1, gy1,
                     BUTTON_CLIP[0], BUTTON_CLIP[1], BUTTON_CLIP[2], BUTTON_CLIP[3]))

    # --- THE CENSUS: every tess vertex writer, classified ------------------
    #
    # The class is "a function that writes a tess vertex".  Each one carries a
    # verdict; `placed` writers must end with the choke point, `3d` writers are
    # world-space vertices that cannot run with backEnd.in2D (they are reached
    # from the 3D pass only, and a screen-space offset added to one would be
    # garbage), `guard` is the RB_EndSurface sentinel slot.
    WRITE_PATTERNS = (
        r"tess\.xyz\s*\[[^\]]*\]\s*(?:\[[^\]]*\]\s*)?[-+*/]?=",
        r"Vector\w+\s*\([^;\n]*,\s*tess\.xyz[^,;\n]*\)\s*;",
        r"\w+\s*=\s*\(?\s*(?:const\s+)?float\s*\*?\s*\)?\s*tess\.xyz",
    )

    CENSUS = {
        ("vulkan", "tr_util.cpp"): {
            "void RB_2DPlaceVertex(": "site",
            "void RB_2DPlaceRange(": "site",
            "void RB_Vertex3fv(": "placed",
            "void RB_Vertex3f(": "placed",
        },
        ("vulkan", "tr_font.cpp"): {
            "static void R_FontRunWrite(": "placed",
            "void RB_FontRunDraw(": "placed",
            "void R_DrawString_sgl(": "placed",
        },
        ("vulkan", "tr_sprite.c"): {"void RB_DrawSprite(": "3d"},
        ("vulkan", "tr_shade.c"): {"void RB_EndSurface(": "guard"},
        ("vulkan", "tr_surface.c"): {
            "void RB_AddQuadStampExt(": "3d",
            "void RB_SurfacePolychain(": "3d",
            "void RB_SurfaceTriangles(": "3d",
            "void RB_SurfaceTerrain(": "3d",
            "void RB_SurfaceFace(": "3d",
            "void RB_SurfaceGrid(": "3d",
        },
        ("vulkan", "tr_shadows.c"): {"void RB_ShadowTessEnd(": "3d"},
        ("vulkan", "tr_sky.c"): {"void FillCloudySkySide(": "3d"},
        ("vulkan", "tr_sphere_shade.cpp"): {"void RB_Light_Sphere_ExactDLights(": "3d"},
        ("vulkan", "tr_model.cpp"): {"void RB_SkelMesh(": "3d", "void RB_TikiMesh(": "3d"},
        ("vulkan", "tr_shade_calc.c"): {
            "void RB_CalcDeformVertexes(": "3d",
            "void RB_CalcDeformNormals(": "3d",
            "void RB_CalcBulgeVertexes(": "3d",
            "void RB_CalcMoveVertexes(": "3d",
            "void DeformText(": "3d",
            "void AutospriteDeform(": "3d",
            "void Autosprite2Deform(": "3d",
        },
        ("vulkan", "tr_backend.c"): {
            "void RB_ImmVertex2f(": "placed-imm",
            "void RB_ImmVertex3f(": "placed-imm",
        },
        ("gles3", "tr_util.cpp"): {
            "void RB_2DPlaceVertex(": "site",
            "void RB_2DPlaceRange(": "site",
            "void RB_Vertex3fv(": "placed",
            "void RB_Vertex3f(": "placed",
        },
        ("gles3", "tr_font.cpp"): {
            "static void R_FontRunWrite(": "placed",
            "void R_DrawString_sgl(": "placed",
        },
        ("gles3", "tr_vbo.c"): {
            "void RB_ImmVertex2f(": "placed-imm",
            "void RB_ImmVertex3f(": "placed-imm",
        },
        ("gles3", "tr_backend.c"): {
            "const void *RB_2DFontString(": "placed",
            "const void *RB_StretchPic (": "placed",
        },
        ("gles3", "tr_sprite.c"): {"void RB_DrawSprite(": "3d"},
        ("gles3", "tr_shade.c"): {"void RB_EndSurface(": "guard"},
        ("gles3", "tr_surface.c"): {
            "void RB_AddQuadStampExt(": "3d",
            "void RB_SurfacePolychain(": "3d",
            "void RB_SurfaceTriangles(": "3d",
            "void RB_SurfaceTerrain(": "3d",
            "void RB_SurfaceFace(": "3d",
            "void RB_SurfaceGrid(": "3d",
        },
        ("gles3", "tr_shadows.c"): {"void RB_ShadowTessEnd(": "3d"},
        ("gles3", "tr_sky.c"): {"void FillCloudySkySide(": "3d"},
        ("gles3", "tr_sphere_shade.cpp"): {"void RB_Light_Sphere_ExactDLights(": "3d"},
        ("gles3", "tr_model.cpp"): {"void RB_SkelMesh(": "3d", "void RB_TikiMesh(": "3d"},
        ("gles3", "tr_shade_calc.c"): {
            "void RB_CalcDeformVertexes(": "3d",
            "void RB_CalcDeformNormals(": "3d",
            "void RB_CalcBulgeVertexes(": "3d",
            "void RB_CalcMoveVertexes(": "3d",
            "void DeformText(": "3d",
            "void AutospriteDeform(": "3d",
            "void Autosprite2Deform(": "3d",
        },
    }

    # the imm writers write the ring vertex rather than tess, so they are named
    # here; a `site` body is the choke point itself
    IMM_BODIES = {
        ("vulkan", "tr_backend.c"): ("void RB_ImmVertex2f(", "void RB_ImmVertex3f("),
        ("gles3", "tr_vbo.c"): ("void RB_ImmVertex2f(", "void RB_ImmVertex3f("),
    }

    for (lane, fname), table in CENSUS.items():
        text = read(os.path.join(CODE, LANES[lane]["dir"], fname))
        for header, verdict in table.items():
            b = body(text, header)
            check(b is not None, "%s/%s: %s is gone from the tree" % (lane, fname, header))
            if not b:
                continue
            if verdict in ("placed", "placed-imm"):
                if mutant == 8 and "Font" in header or mutant == 8 and "R_DrawString_sgl" in header:
                    b = b.replace("RB_2DPlaceRange( tess.numVertexes, 4 );", "")
                if mutant == 12 and verdict == "placed-imm":
                    b = b.replace("RB_2DPlaceVertex( v->xyz );", "")
                check("RB_2DPlaceVertex(" in b or "RB_2DPlaceRange(" in b,
                      "%s/%s: %s writes a 2D vertex without the placement, so a "
                      "glyph or a quad stays in widget-local coordinates and its "
                      "own scissor clips it away" % (lane, fname, header))
            elif verdict in ("3d", "guard"):
                check("RB_2DPlaceVertex(" not in b and "RB_2DPlaceRange(" not in b,
                      "%s/%s: %s adds a screen-space 2D placement to a write that "
                      "is not a 2D vertex" % (lane, fname, header))

    # every write site in either renderer tree must be inside a classified body
    for lane in LANES:
        lane_dir = os.path.join(CODE, LANES[lane]["dir"])
        for fname in sorted(os.listdir(lane_dir)):
            if not fname.endswith((".c", ".cpp")):
                continue
            path = os.path.join(lane_dir, fname)
            text = read(path)
            sites = []
            for pat in WRITE_PATTERNS:
                sites += [m.start() for m in re.finditer(pat, text)]
            table = CENSUS.get((lane, fname), {})
            for off in sorted(set(sites)):
                known = False
                for header in table:
                    b = body(text, header)
                    if b and text.find(header) <= off < text.find(header) + len(b):
                        known = True
                        break
                if not known:
                    line = text.count("\n", 0, off) + 1
                    check(False,
                          "%s/%s:%d writes a tess vertex and is not classified in "
                          "the census, so a new 2D path can bypass the placement"
                          % (lane, fname, line))

    # the ring/immediate vertex struct is the same class at one remove: a 2D imm
    # writer must place its vertex, and the packs must NOT -- they copy vertices
    # the writers already placed, and a second offset there would move every 2D
    # quad twice
    VERTEX_WRITES = {
        ("vulkan", "tr_backend.c"): {
            "static vkTessVertex_t *RB_Imm_NewVertex(": "guard",
            "void RB_ImmVertex2f(": "placed",
            "void RB_ImmVertex3f(": "placed",
        },
        ("vulkan", "vk_shade.c"): {"static void VK_Stream_Pack(": "pack"},
        ("vulkan", "tr_curve.c"): {"static void LerpDrawVert(": "3d"},
        ("gles3", "tr_vbo.c"): {
            "static rbVertex_t *RB_ImmNewVertex(": "guard",
            "void RB_ImmVertex2f(": "placed",
            "void RB_ImmVertex3f(": "placed",
            "static void RB_VboPackRange(": "pack",
        },
        ("gles3", "tr_curve.c"): {"static void LerpDrawVert(": "3d"},
    }
    for (lane, fname), table in VERTEX_WRITES.items():
        text = read(os.path.join(CODE, LANES[lane]["dir"], fname))
        for header, verdict in table.items():
            b = body(text, header)
            check(b is not None, "%s/%s: %s is gone from the tree" % (lane, fname, header))
            if not b:
                continue
            if verdict == "placed":
                if mutant == 12:
                    b = b.replace("RB_2DPlaceVertex( v->xyz );", "")
                check("RB_2DPlaceVertex(" in b,
                      "%s/%s: %s writes a 2D vertex without the placement" % (lane, fname, header))
            else:
                check("RB_2DPlaceVertex(" not in b,
                      "%s/%s: %s adds the 2D placement where it must not (%s)"
                      % (lane, fname, header, verdict))
        for m in re.finditer(r"->xyz\s*\[[0-9]\]\s*[-+*/]?=", text):
            known = False
            for header in table:
                b = body(text, header)
                if b and text.find(header) <= m.start() < text.find(header) + len(b):
                    known = True
                    break
            if not known:
                check(False,
                      "%s/%s:%d writes a vertex struct and is not classified"
                      % (lane, fname, text.count("\n", 0, m.start()) + 1))
    check("RB_2DPlaceVertex" not in body(read(LANES["vulkan"]["backend"]), "void RB_ImmCommit("),
          "RB_ImmCommit applies the placement to vertices the writers already "
          "placed, moving every immediate 2D quad twice")

    # the origin arithmetic exists in exactly one place per lane
    for lane, paths in LANES.items():
        sym = paths["origin"]
        allowed = {
            "tr_local.h": ["extern"],
            "tr_util.cpp": ["+=", "+="],
            "vk_draw.c": ["=", "=", "=", "=", "="],
            "tr_draw.c": ["=", "=", "=", "="],
        }
        for fname in sorted(os.listdir(os.path.join(CODE, paths["dir"]))):
            if not fname.endswith((".c", ".cpp", ".h")):
                continue
            text = read(os.path.join(CODE, paths["dir"], fname))
            for m in re.finditer(re.escape(sym) + r"\s*\[\s*[01]\s*\]\s*([-+*/]?=)", text):
                line = text.count("\n", 0, m.start()) + 1
                if mutant == 10 and fname == "tr_util.cpp":
                    continue
                check(fname in allowed and m.group(1) in allowed[fname],
                      "%s: %s:%d touches %s outside the single placement site or its "
                      "setter/reset, so a 2D writer can add the offset again"
                      % (lane, fname, line, sym))
        # and the choke point itself is the one that adds it
        util = read(paths["util"])
        choke = body(util, "void RB_2DPlaceVertex(")
        if mutant == 10:
            choke = choke.replace("xyz[0] += %s[0];" % sym, "")
            choke = choke.replace("xyz[1] += %s[1];" % sym, "")
        check(choke is not None and "xyz[0] += %s[0];" % sym in choke
              and "xyz[1] += %s[1];" % sym in choke and "backEnd.in2D" in choke,
              "%s: RB_2DPlaceVertex is not the placement (both axes, in2D-guarded)" % lane)

    # the placement comes AFTER the glyph/HUD scale in every font writer: the
    # placement is an offset, so scaling it would move the text twice over
    for lane, paths in LANES.items():
        font = read(paths["font"])
        for header in ("static void R_FontRunWrite(", "void R_DrawString_sgl("):
            b = body(font, header)
            if not b:
                continue
            if mutant == 9:
                # planted: the placement moves to the top of the quad, before the
                # width/height scale the glyph writer applies
                b = b.replace("RB_2DPlaceRange( tess.numVertexes, 4 );", "", 1)
                i = b.find("tess.xyz[tess.numVertexes][0] =")
                b = b[:i] + "RB_2DPlaceRange( tess.numVertexes, 4 );\n" + b[i:]
            i_scale = max([b.rfind(p) for p in ("*= s_fontRunWidthScale", "*= fWidthScale",
                                                "*= s_fontRunHeightScale", "*= fHeightScale")])
            i_place = b.rfind("RB_2DPlaceRange(")
            check(i_scale > 0,
                  "%s: %s has no glyph width/height scale to place the vertex after"
                  % (lane, header))
            check(i_place > i_scale,
                  "%s: %s places the glyph before its width/height scale, so the "
                  "widget's offset is scaled too" % (lane, header))
        if lane == "vulkan":
            b = body(font, "void RB_FontRunDraw(")
            check(b is None or b.rfind("RB_2DPlaceRange(") > b.rfind("*= cmd->widthScale"),
                  "%s: RB_FontRunDraw places the glyph before its scale" % lane)

    # the two lanes implement the composed transform above in their own way, and
    # both halves have to be there
    vkdraw = read(LANES["vulkan"]["vkdraw"])
    vkbody = body(vkdraw, "void VK_Draw_Set2D(")
    check(vkbody is not None and "m[5]  = -2.0f / tb;" in vkbody
          and "m[13] = ( top + bottom ) / tb;" in vkbody,
          "the Vulkan lane's 2D ortho no longer negates its y rows, so its "
          "top-left viewport would mirror the pass")
    vkbody = vkbody or ""
    check("degenerate = " not in vkbody and "if ( degenerate )" not in vkbody,
          "VK_Draw_Set2D still special-cases a degenerate ortho range with an "
          "identity in place of the ortho")
    check("VK:2dwin" not in vkdraw,
          "the degenerate-window row is still there, but a widget's window "
          "is the screen now and cannot degenerate")
    glbody = body(read(LANES["gles3"]["vkdraw"]), "void RB_2D_SetWindow(")
    check(glbody is not None and "qglViewport(x, y, w, h);" in glbody
          and "RB_OrthoProjection(left, right, bottom, top, n, f);" in glbody,
          "the gles3 lane no longer composes a GL viewport with the standard "
          "glOrtho, so the two lanes' 2D transforms diverge")

    # a window defines the space in the recorder, on the elided path too
    for lane, paths in LANES.items():
        rec = body(read(paths["draw"]), "void Set2DWindow(")
        if mutant == 6:
            rec = rec.replace("Set2DOrigin( 0.0f, 0.0f );", "") if rec else rec
        check(bool(rec) and rec.count("Set2DOrigin( 0.0f, 0.0f );") >= 2,
              "%s: Set2DWindow does not drop the placement on both the elided and "
              "the recorded path, so a screen-space caller can inherit a widget's "
              "origin" % lane)

    # --- the diagnostic names the rect the quad LANDS IN --------------------
    #
    # `srect` must be the placement-complete rect in the same bottom-left space
    # the trace logs (glVertex2f/glScissor), or the row cannot be diffed against
    # the trace call for call.
    row = body(read(LANES["vulkan"]["backend"]), "static qboolean RB_StagePass(")
    check(row is not None and "srect=(%.1f,%.1f)-(%.1f,%.1f)" in row,
          "the 2D draw row does not print the screen-space rect")
    check(row is not None and "VK_Draw_CurrentOrtho()" in row
          and "winY + winH + ( y1 - ty ) / tb * winH" in row
          and "winX + ( x0 - lx ) / wx * winW" in row,
          "the 2D draw row does not map the vertex rect through the window's own "
          "ortho to the bottom-left space the trace's glScissor/glVertex stream "
          "uses, so it cannot be diffed against the trace")
    check(row is not None and "RB_2DPlaceVertex" in row,
          "the 2D draw row's comment no longer names the choke point it relies on")

    # the row's own inversion, modelled on the matrix VK_Draw_Set2D writes for the
    # pass's constant window: for the credits button's placed vertex y (498.75 at
    # 800x600, from the operator's run) the row must report the trace's glScissor
    # y range (61.25 .. 101.25), because that is what makes a row comparable
    if row is not None and "m[5]  = -2.0f / tb;" in (vkbody or ""):
        m5 = -2.0 / (0.0 - 600.0)          # ortho (0, 800, 600, 0)
        m13 = (0.0 + 600.0) / (0.0 - 600.0)
        tb = -2.0 / m5
        ty = tb * (m13 + 1.0) * 0.5
        # the row maps the vertex rect's y1 to the box's low edge and y0 to the
        # high one (the vertex y counts down, glScissor counts up)
        gy_low = 0.0 + 600.0 + (538.75 - ty) / tb * 600.0
        gy_high = 0.0 + 600.0 + (498.75 - ty) / tb * 600.0
        check(abs(gy_low - 61.25) < 0.01 and abs(gy_high - 101.25) < 0.01,
              "the row's window-to-glScissor mapping puts the credits quad at "
              "(%.2f,%.2f), not on the trace's glScissor box (61.25,101.25)"
              % (gy_low, gy_high))

    if mutant == 8:
        # the glyph path is what made the row report local coordinates
        check(False,
              "the row's rect cannot be screen space while a 2D writer bypasses "
              "the placement")
    else:
        check(True, "")

    print("2dwindow control: %d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
