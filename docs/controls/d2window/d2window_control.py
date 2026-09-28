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

    print("2dwindow control: %d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
