#!/usr/bin/env python3
"""The registration boundary: what the renderer arena purge destroys, what has
to outlive it, and every table that must stop indexing it.

`RE_BeginRegistration` calls `ri.Hunk_Clear` (= `CL_RefStaticClear`), which is
`Z_FreeTags( TAG_STATIC_RENDERER )` -- the arena every `ri.Hunk_Alloc` returns
from.  The shipping Vulkan lane keeps objects the boundary must NOT destroy
inside that arena, and indexes the destroyed ones from tables it never resets.
Both are checkable against the shipping sources:

  1. the boundary fully parks the walk thread (`VKimp_FrontEndWaitIdle`)
     BEFORE the free -- `R_IssuePendingRenderCommands`/`VK_Frame_WaitIdle`
     stop at a claimed frame and the GPU, not at `vk_walkingSlot == -1`;
  2. the render-command buffers (`rb_commandLists`) come from the renderer pool
     (`ri.Malloc`, TAG_RENDERER), which nothing purges while they are needed,
     not from the arena the boundary frees;
  3. every table that indexes the arena is reset or rebuilt at the boundary:
     `tr.numSkins`/`tr.skins[]`, `tr.shaders[]`/`tr.sortedShaders[]`,
     `s_worldData`;
  4. the whole class: every `ri.Hunk_Alloc` site in the shipping renderer is in
     one of the files whose allocations the ledger says are destroyed and
     rebuilt (world, curves, sprites, shaders, skins), and the count is the
     ledger's;
  5. the free sites are enumerated: TAG_STATIC_RENDERER is freed in exactly one
     function, TAG_RENDERER in exactly one, and the renderer never calls
     `ri.Clear`, so a TAG_RENDERER object outlives every registration;
  6. the client-side caches that hold renderer handles are revalidated after
     the boundary, in `CL_BeginRegistration`.

Modes:
  0                  the shipping tree must be green
  1..N               a planted fault must be detected
  <files...>         the same checks against mutated copies (source faults)
Never launches the game.
"""

import os
import re
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
CODE = os.path.normpath(os.path.join(HERE, "..", "..", "..",
                                     "app", "src", "main", "cpp", "code"))

RK = "renderervulkan"
TR_INIT = os.path.join(RK, "tr_init.c")
TR_CMDS = os.path.join(RK, "tr_cmds.c")
TR_SHADER = os.path.join(RK, "tr_shader.c")
TR_BSP = os.path.join(RK, "tr_bsp.c")
CL_MAIN = os.path.join("client", "cl_main.cpp")
CL_UI = os.path.join("client", "cl_ui.cpp")

FILES = (TR_INIT, TR_CMDS, TR_SHADER, TR_BSP, CL_MAIN, CL_UI)

# The ledger.  An arena allocation may live only in these files, and only this
# many of them: the boundary frees the arena and each of these is rebuilt by
# the load path (world/submodels/lightmaps/curves/sprites) or by R_SetupShaders
# (shader_t, shaderStage_t, texMods) or lazily under a table the boundary resets
# (skin_t).  A new site anywhere else -- and in particular any site in the
# frame plumbing -- is a lifetime that has not been accounted for.
ARENA_FILES = {
    os.path.join(RK, "tr_bsp.c"),
    os.path.join(RK, "tr_curve.c"),
    os.path.join(RK, "tr_image.c"),
    os.path.join(RK, "tr_shader.c"),
    os.path.join(RK, "tr_sprite.c"),
}
ARENA_SITE_COUNT = 34


def read(root, rel):
    with open(os.path.join(root, rel), encoding="utf-8") as fh:
        return fh.read()


def function_body(src, name):
    """The text of `name(...) { ... }`, brace-matched from its definition."""
    m = re.search(r"\b" + re.escape(name) + r"\s*\([^;{]*\)\s*\{", src)
    if not m:
        return None
    i = m.end() - 1
    depth = 0
    while i < len(src):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                return src[m.start():i + 1]
        i += 1
    return None


def arena_sites(root):
    """(file, count) for every renderer file with a ri.Hunk_Alloc site."""
    found = {}
    d = os.path.join(root, RK)
    for f in sorted(os.listdir(d)):
        if not (f.endswith(".c") or f.endswith(".cpp")):
            continue
        rel = os.path.join(RK, f)
        n = read(root, rel).count("ri.Hunk_Alloc(")
        if n:
            found[rel] = n
    return found


def analyze(root):
    bad = []

    # 1. the park precedes the free
    body = function_body(read(root, TR_INIT), "RE_BeginRegistration")
    if body is None:
        bad.append("RE_BeginRegistration not found")
    else:
        parks = [m.start() for m in re.finditer(
            r"VKimp_FrontEndWaitIdle\s*\(\s*\)|VK_Upload_Quiesce\s*\(\s*\)", body)]
        park = min(parks, default=-1)
        free = body.find("ri.Hunk_Clear")
        if park < 0:
            bad.append("the boundary does not park the walk thread before the "
                       "arena free (VKimp_FrontEndWaitIdle)")
        elif free < 0:
            bad.append("the boundary does not free the arena")
        elif park > free:
            bad.append("the walk-thread park is after the arena free")

        # 3a. the skin table the freed arena held
        if not re.search(r"tr\s*\.\s*numSkins\s*=\s*0", body):
            bad.append("the boundary does not reset tr.numSkins")
        if not re.search(r"Com_Memset\(\s*tr\s*\.\s*skins", body):
            bad.append("the boundary does not clear tr.skins[]")

    # 2. the command buffers must outlive the arena purge
    cmds = function_body(read(root, TR_CMDS), "R_InitCommandBuffers")
    if cmds is None:
        bad.append("R_InitCommandBuffers not found")
    else:
        m = re.search(r"rb_commandLists\[i\]\s*=\s*[^;]*;", cmds)
        if not m:
            bad.append("rb_commandLists is never allocated")
        else:
            stmt = m.group(0)
            if "ri.Hunk_Alloc" in stmt:
                bad.append("the render-command buffers are allocated from the "
                           "arena the boundary frees (ri.Hunk_Alloc)")
            if "ri.Malloc" not in stmt:
                bad.append("the render-command buffers do not come from the "
                           "renderer pool (ri.Malloc)")

    # 3b. the shader tables the freed arena held
    setup = function_body(read(root, TR_SHADER), "R_SetupShaders")
    if setup is None:
        bad.append("R_SetupShaders not found")
    else:
        if not re.search(r"Com_Memset\(\s*tr\s*\.\s*shaders\b", setup):
            bad.append("R_SetupShaders does not clear tr.shaders[]")
        if not re.search(r"Com_Memset\(\s*tr\s*\.\s*sortedShaders\b", setup):
            bad.append("R_SetupShaders does not clear tr.sortedShaders[]")

    # 3c. every pointer inside the world
    clear = function_body(read(root, TR_BSP), "R_ClearWorld")
    if clear is None:
        bad.append("R_ClearWorld not found")
    elif not re.search(r"Com_Memset\(\s*&\s*s_worldData\b", clear):
        bad.append("R_ClearWorld leaves the pointers inside s_worldData")

    # 4. the class ledger
    found = arena_sites(root)
    if set(found) != ARENA_FILES:
        bad.append("arena allocations outside the destroyed-and-rebuilt ledger: "
                   + ", ".join(sorted(set(found) ^ ARENA_FILES)))
    if sum(found.values()) != ARENA_SITE_COUNT:
        bad.append("arena site count %d, ledger says %d"
                   % (sum(found.values()), ARENA_SITE_COUNT))

    # 5. the free sites, and the pool nothing purges
    cl = read(root, CL_MAIN)
    if cl.count("Z_FreeTags( TAG_STATIC_RENDERER )") != 1:
        bad.append("TAG_STATIC_RENDERER is not freed in exactly one place")
    if cl.count("Z_FreeTags( TAG_RENDERER )") != 1:
        bad.append("TAG_RENDERER is not freed in exactly one place")
    for rel in (TR_INIT, TR_CMDS, TR_SHADER, TR_BSP):
        if "ri.Clear(" in read(root, rel):
            bad.append("%s calls ri.Clear, which purges the pool the render "
                       "command buffers live in" % rel)

    # 6. the client caches are revalidated after the boundary
    cli = function_body(read(root, CL_UI), "CL_BeginRegistration")
    if cli is None:
        bad.append("CL_BeginRegistration not found")
    else:
        beg = cli.find("re.BeginRegistration")
        clean = cli.find("CleanupShadersFromList")
        models = cli.find("RegisterModelsFromList")
        if beg < 0 or clean < 0 or models < 0:
            bad.append("CL_BeginRegistration does not revalidate the cached "
                       "renderer handles after the boundary")
        elif not (beg < clean and beg < models):
            bad.append("the client caches are revalidated before the boundary, "
                       "so they cache handles into the freed arena")

    return bad


# Each planted fault is a regex mutation of the shipping text; the control must
# go RED for every one, and each is reported by the check it belongs to.
FAULTS = {
    1: [(TR_CMDS,
         r"rb_commandLists\[i\]\s*=\s*\( vkCommandList_t \* \)ri\.Malloc\( sizeof\( vkCommandList_t \) \);",
         "rb_commandLists[i] = ( vkCommandList_t * )ri.Hunk_Alloc( sizeof( vkCommandList_t ), h_low );")],
    2: [(TR_INIT, r"\n[ \t]*VKimp_FrontEndWaitIdle\(\);", "")],
    3: [(TR_INIT, r"\n[ \t]*tr\.numSkins = 0;", "")],
    4: [(TR_SHADER, r"\n[ \t]*Com_Memset\( tr\.shaders, 0, sizeof\( tr\.shaders \) \);", "")],
    5: [(TR_BSP, r"\n[ \t]*Com_Memset\( &s_worldData, 0, sizeof\( s_worldData \) \);", "",
         "R_ClearWorld")],
    6: [(CL_UI, r"\n[ \t]*uWinMan\.CleanupShadersFromList\(\);", "")],
    7: [(CL_MAIN, r"Z_FreeTags\( TAG_STATIC_RENDERER \)",
         "Z_FreeTags( TAG_RENDERER )")],
    8: [(TR_INIT, r"\n[ \t]*VKimp_FrontEndWaitIdle\(\);", ""),
        (TR_INIT, r"\n[ \t]*R_ClearWorld\(\);", "\n\tVKimp_FrontEndWaitIdle();\n\tR_ClearWorld();")],
}


def stage(dst, mutations):
    """A full copy of the shipping renderer plus the client files under test."""
    if os.path.exists(dst):
        shutil.rmtree(dst)
    shutil.copytree(os.path.join(CODE, RK), os.path.join(dst, RK))
    os.makedirs(os.path.join(dst, "client"), exist_ok=True)
    for rel in (CL_MAIN, CL_UI):
        shutil.copyfile(os.path.join(CODE, rel), os.path.join(dst, rel))
    for mut in mutations:
        rel, old, new = mut[0], mut[1], mut[2]
        scope = mut[3] if len(mut) > 3 else None
        p = os.path.join(dst, rel)
        with open(p, encoding="utf-8") as fh:
            src = fh.read()
        if scope:
            body = function_body(src, scope)
            if body is None:
                return "function %s absent in %s" % (scope, rel)
            body2, n = re.subn(old, new, body, count=1)
            if n != 1:
                return "anchor absent: %s in %s/%s" % (old[:50], rel, scope)
            src2 = src.replace(body, body2, 1)
        else:
            src2, n = re.subn(old, new, src, count=1)
            if n != 1:
                return "anchor absent: %s in %s" % (old[:50], rel)
        with open(p, "w", encoding="utf-8") as fh:
            fh.write(src2)
    return None


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "0"

    if mode == "0" and len(sys.argv) > 2:
        tmp = tempfile.mkdtemp()
        try:
            err = stage(tmp, [])
            if err:
                print("  " + err)
                return 2
            for src in sys.argv[2:]:
                base = os.path.basename(src)
                for rel in FILES:
                    if os.path.basename(rel) == base:
                        shutil.copyfile(src, os.path.join(tmp, rel))
            bad = analyze(tmp)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
        for b in bad:
            print("  RED  " + b)
        return 1 if bad else 0

    if mode == "0":
        bad = analyze(CODE)
        for b in bad:
            print("  RED  " + b)
        return 1 if bad else 0

    idx = int(mode)
    if idx not in FAULTS:
        print("  unknown fault %s" % mode)
        return 2

    tmp = tempfile.mkdtemp()
    try:
        err = stage(tmp, FAULTS[idx])
        if err:
            print("  arm %d did not fire: %s" % (idx, err))
            return 2
        bad = analyze(tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    if bad:
        print("  ok   fault %d detected: %s" % (idx, bad[0]))
        return 0
    print("  FAIL fault %d was not detected" % idx)
    return 1


if __name__ == "__main__":
    sys.exit(main())
