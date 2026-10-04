#!/usr/bin/env python3
"""Cross-mode render state: what a map change must destroy, and the census that
names what survives it.

The port has two renderer lanes over one front end.  renderergles3 is the
reference twin; renderervulkan is the shipping lane.  RE_BeginRegistration is
the port's map-change boundary in both, so the invalidation set the twin
performs there is the contract the shipping lane has to meet -- retail's own
R_BeginRegistration @0x00028D50 resets tr.viewCluster (the decompile's
`v2[1039] = -1`) and calls RE_ClearScene(), and the twin adds
R_FreeNonTikiModels()/ri.Hunk_Clear()/R_ClearWorld().

Checks, all against the shipping sources:
  1. the shipping lane performs every invalidation the twin performs there;
  2. the census is emitted at both ends of a registration and is gated by the
     master probe gate;
  3. the census names the state a map change must drop (world, worldMapLoaded,
     viewCluster, images, models, clear colour);
  4. the front end's own vk.clearColor is re-initialised at the boundary.

Modes:
  0                  the shipping tree must be green
  1..N               a planted fault in the fault table must be detected
  <files...>         the same checks against mutated copies (source faults)
Never launches the game.
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CODE = os.path.normpath(os.path.join(HERE, "..", "..", "..",
                                     "app", "src", "main", "cpp", "code"))
VK_INIT = os.path.join(CODE, "renderervulkan", "tr_init.c")
GL_INIT = os.path.join(CODE, "renderergles3", "tr_init.c")
VK_HDR = os.path.join(CODE, "renderervulkan", "vk_local.h")

# The invalidation set the reference twin performs in RE_BeginRegistration.
# Each row: (label, regex that must match the shipping lane's function body).
CONTRACT = [
    ("the visibility cluster lock is dropped",
     r"tr\.viewCluster\s*=\s*-1\s*;"),
    ("the previous scene's refdef is cleared",
     r"\bRE_ClearScene\s*\(\s*\)\s*;"),
    ("the previous map's non-tiki models are freed",
     r"\bR_FreeNonTikiModels\s*\(\s*\)\s*;"),
    ("the previous map's world is destroyed",
     r"\bR_ClearWorld\s*\(\s*\)\s*;"),
]

# The state the census has to name: a handle that crosses a map change.
CENSUS_FIELDS = ["world=%d", "worldMapLoaded=%d", "viewCluster=%d", "images=%d",
                 "numModels=%d", "swImages=%d", "fb=%d", "pass=%d", "clear=%.3f"]


def uncomment(src):
    """Drop // and /* */ comments, so a call that was commented out is gone."""
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    src = re.sub(r"//[^\n]*", " ", src)
    return src


def read(path):
    with open(path, "r", errors="ignore") as fh:
        return fh.read()


def function_body(src, name):
    """The text of a top-level function, brace-matched from its opening brace."""
    m = re.search(r"\n[A-Za-z_][A-Za-z0-9_ \t\*]*\b" + re.escape(name)
                  + r"\s*\([^;{]*\)\s*\{", src)
    if not m:
        return None
    i = src.index("{", m.end() - 1)
    depth = 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[i:j]
    return None


def check(vk_init, gl_init, vk_hdr):
    """Returns (failures, checks)."""
    fails = []
    checks = 0

    body = function_body(uncomment(vk_init), "RE_BeginRegistration")
    checks += 1
    if body is None:
        return ["renderervulkan/tr_init.c has no RE_BeginRegistration"], checks

    for label, pat in CONTRACT:
        checks += 1
        if not re.search(pat, body):
            fails.append("the map-change boundary does not: " + label)

    twin = function_body(uncomment(gl_init), "RE_BeginRegistration")
    checks += 1
    if twin is None:
        fails.append("renderergles3/tr_init.c has no RE_BeginRegistration")
    else:
        # parity: everything the twin destroys at the boundary, this lane must too
        for label, pat in CONTRACT:
            checks += 1
            if re.search(pat, twin) and not re.search(pat, body):
                fails.append("lane parity: the twin does, this lane does not: " + label)

    # the census itself
    checks += 1
    if "VK_Life_Census" not in vk_init:
        fails.append("no lifetime census in the renderer")
    checks += 1
    if "VK_Life_NoteCreate" not in vk_hdr or "VK_Life_NoteDestroy" not in vk_hdr:
        fails.append("no per-object lifetime counters declared")

    census = function_body(uncomment(vk_init), "VK_Life_Census")
    checks += 1
    if census is None:
        fails.append("VK_Life_Census has no body")
    else:
        checks += 1
        if "R_ProbeStatsOn" not in census:
            fails.append("the census is not behind the master probe gate")
        for f in CENSUS_FIELDS:
            checks += 1
            if f not in census:
                fails.append("the census does not name '" + f + "'")

        # it has to be emitted at BOTH ends of a registration
        checks += 1
        if 'VK_Life_Census( "begin-registration" )' not in body:
            fails.append("the census is not emitted at the start of a registration")
        checks += 1
        shutdown = function_body(uncomment(vk_init), "RE_Shutdown")
        if shutdown is None or 'VK_Life_Census( "shutdown" )' not in shutdown:
            fails.append("the census is not emitted at shutdown")

    # the frame clear is a view value; the boundary has to re-initialise it
    checks += 1
    if not re.search(r"vk\.clearColor\[0\]\s*=\s*0\.0f\s*;", body):
        fails.append("the map-change boundary does not re-initialise vk.clearColor")

    return fails, checks


# ---- planted faults (mode 1..N): (name, old, new) applied to a lane copy ----
FAULTS = [
    ("1 the world survives the map change",
     "\tR_ClearWorld();\n", "\t/* R_ClearWorld(); */\n"),
    ("2 the visibility cluster lock survives",
     "\ttr.viewCluster = -1;\n", "\t/* tr.viewCluster = -1; */\n"),
    ("3 the previous map's models survive",
     "\tR_FreeNonTikiModels();\n", "\t/* R_FreeNonTikiModels(); */\n"),
    ("4 the previous scene survives",
     "\tRE_ClearScene();\n", "\t/* RE_ClearScene(); */\n"),
    ("5 the frame clear survives",
     "\tvk.clearColor[0] = 0.0f;\n", "\t/* vk.clearColor[0] = 0.0f; */\n"),
    ("6 the census loses its gate",
     "\tif ( !R_ProbeStatsOn() ) {\n\t\treturn;\n\t}\n", ""),
    ("7 the census is not emitted at the boundary",
     '\tVK_Life_Census( "begin-registration" );\n', ""),
    ("8 the census stops naming the world",
     '"site=%s gen=%d f=%d world=%d worldMapLoaded=%d viewCluster=%d "',
     '"site=%s gen=%d f=%d "'),
]


def main(argv):
    mode = argv[1] if len(argv) > 1 else "0"
    if mode == "0" and len(argv) > 2:
        # source-fault mode: the mutated copies are the arguments after 0
        files = argv[2:]

        def pick(base):
            for f in files:
                if os.path.basename(f) == os.path.basename(base):
                    return read(f)
            return read(base)

        fails, checks = check(pick(VK_INIT), pick(GL_INIT), pick(VK_HDR))
    elif mode == "0":
        fails, checks = check(read(VK_INIT), read(GL_INIT), read(VK_HDR))
    else:
        idx = int(mode) - 1
        if idx < 0 or idx >= len(FAULTS):
            print("no such fault: " + mode)
            return 2
        name, old, new = FAULTS[idx]
        src = read(VK_INIT)
        if old not in src:
            print("  fault %s does not fire: anchor absent" % mode)
            return 1
        fails, checks = check(src.replace(old, new, 1), read(GL_INIT), read(VK_HDR))
        if not fails:
            print("  fault %s (%s) not detected" % (mode, name))
            return 1
        print("  fault %s detected: %s" % (mode, fails[0]))
        return 0

    if fails:
        for f in fails:
            print("  FAIL  " + f)
        return 1
    print("  ok    registration invalidation complete, census gated and named "
          "(%d checks)" % checks)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
