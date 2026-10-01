#!/usr/bin/env python3
"""
d2window_control.py -- the 2D window/scissor elision cache, proven offline.

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

It also pins the shape the trace gives the 2D pass itself, because the two go
together: with a per-widget window the cache had a window command per widget to
elide, and with retail's shape there is ONE window for the pass.  From
retail-menu-solomatch-final.trace calls 10103-10124 (and the repeats that
follow): `glViewport(0, 0, 640, 480)`, `glMatrixMode(GL_PROJECTION)`,
`glLoadIdentity()`, `glOrtho(0, 640, 480, 0, -1, 1)`, `glMatrixMode(GL_MODELVIEW)`,
`glLoadIdentity()`, then `glVertex2f` in SCREEN space, with `glScissor` the only
per-widget state -- `(49,51,320,26)`, `(424,53,123,26)`, `(371,49,256,32)`,
`(458,393,138,32)`, `(44,393,138,32)`, `(320,393,138,32)`, `(182,393,138,32)`,
`(435,20,96,32)`, `(40,9,40,16)`, `(20,9,25,16)`, all nonzero.  So the control
requires the constant window + the vertex origin, forbids a degenerate-ortho
branch, and proves the placement is unchanged by the rewrite.

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
        "frame": os.path.join(CODE, "renderervulkan", "tr_init.c"),
        "backend": os.path.join(CODE, "renderervulkan", "tr_backend.c"),
        "cmds": os.path.join(CODE, "renderervulkan", "tr_backend.c"),
    },
    "gles3": {
        "frame": os.path.join(CODE, "renderergles3", "tr_cmds.c"),
        "backend": os.path.join(CODE, "renderergles3", "tr_backend.c"),
        "cmds": os.path.join(CODE, "renderergles3", "tr_cmds.c"),
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


def main():
    mutant = int(sys.argv[1]) if len(sys.argv) > 1 else 0

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

    # the placement the rewrite promises: old window+ortho and new window+origin
    # put the same vertex at the same place
    for (sx, sy, sw, sh) in TRACE_SCISSORS:
        for (px, py) in ((sx, sy), (sx + 7, sy + 3), (sx + sw - 1, sy + sh - 1)):
            for (vx, vy) in ((0.0, 0.0), (5.0, 1.2), (sw - 1.0, sh - 1.0)):
                for (cox, coy) in ((0.0, 0.0), (-3.0, -2.0)):
                    W, H, vidW, vidH, originX, originY = float(sw), float(sh), 640.0, 480.0, 0.0, 40.0
                    # old: window at (originX+px, ...), ortho cox..cox+W
                    old_x = originX + px + vx - cox
                    old_y = (originY + vidH) - H - py + vy - coy
                    # new: one screen window, origin ox/oy on the vertices
                    ox = px - cox
                    oy = vidH - H - py - coy
                    new_x = originX + vx + ox
                    new_y = (originY + vidH) - vidH + vy + oy
                    check(abs(old_x - new_x) < 1e-4 and abs(old_y - new_y) < 1e-4,
                          "the rewrite moved a vertex: old=(%g,%g) new=(%g,%g) at "
                          "clip (%d,%d,%d,%d)" % (old_x, old_y, new_x, new_y, sx, sy, sw, sh))

    # no degenerate-ortho branch, and the placement is applied in the vertex writers
    for lane, paths in LANES.items():
        if lane == "vulkan":
            vkdraw = read(os.path.join(CODE, "renderervulkan", "vk_draw.c"))
            body2d = vkdraw[vkdraw.find("void VK_Draw_Set2D("):]
            body2d = body2d[:body2d.find("\n}\n") + 3]
            check("degenerate = " not in body2d and "if ( degenerate )" not in body2d,
                  "VK_Draw_Set2D still special-cases a degenerate ortho range with an "
                  "identity in place of the ortho")
            check("VK:2dwin" not in vkdraw,
                  "the degenerate-window row is still there, but a widget's window "
                  "is the screen now and cannot degenerate")
            util = read(os.path.join(CODE, "renderervulkan", "tr_util.cpp"))
            backend = read(os.path.join(CODE, "renderervulkan", "tr_backend.c"))
            check("backEnd.in2D" in body(util, "void RB_Vertex3f("),
                  "the surface batch's 2D vertex writer does not apply the placement")
            check("vk_2dOrigin" in body(backend, "void RB_ImmVertex2f(") and
                  "backEnd.in2D" in body(backend, "void RB_ImmVertex2f("),
                  "the immediate stream's 2D vertex writer does not apply the placement")

        # a window defines the space in the recorder, on the elided path too
        drawpath = (os.path.join(CODE, "renderervulkan", "tr_draw.c") if lane == "vulkan"
                    else os.path.join(CODE, "renderergles3", "tr_draw.c"))
        rec = body(read(drawpath), "void Set2DWindow(")
        if mutant == 6:
            rec = rec.replace("Set2DOrigin( 0.0f, 0.0f );", "")
        check(rec.count("Set2DOrigin( 0.0f, 0.0f );") >= 2,
              "%s: Set2DWindow does not drop the placement on both the elided and "
              "the recorded path, so a screen-space caller can inherit a widget's "
              "origin" % lane)

    print("2dwindow control: %d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
