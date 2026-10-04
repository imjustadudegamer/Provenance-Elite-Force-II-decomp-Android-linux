#!/usr/bin/env python3
"""The world-set census's static invariant, read off the SHIPPING tree.

An iterator path that cannot reach the census is a blind spot, and a census
outside the master gate is an ungated emitter.  Both are properties of the
source text, so they are checked here without a run:

  1. VK_Shade_BatchBegin is called from RB_BeginSurface, and its census gate
     still carries R_ProbeStatsOn() and no longer reads backEnd.currentEntity;
  2. VK_Shade_BatchEnd is called from RB_EndSurface after the stage iterator,
     and its guard returns on !s_batchCensus before any row;
  3. every function tr_shader.c assigns to optimalStageIteratorFunc reaches the
     pass accounting (VK_DrawStage or VK_Shade_NoteSubmitted), directly or
     through the functions it calls;
  4. a disagreement branch precedes the sampling branch in BatchEnd;
  5. every census row goes through VK_Guard_Row;
  6. the alpha test the fragment stage discards on is derived from the STAGE
     whose uniforms are being set, never from the latch the last state block
     left: an iterator that publishes a state word of its own (the
     lightmapped-multitexture pass forces depth on and carries no atest bits)
     otherwise draws an alpha-cut surface with alphaFunc GT0 switched off.

  usage: staticinvariant.py <code-dir> [--mutate N]
"""
import os
import re
import sys

FILES = {
    "vk_shade": "renderervulkan/vk_shade.c",
    "tr_shade": "renderervulkan/tr_shade.c",
    "tr_shader": "renderervulkan/tr_shader.c",
    "tr_sky": "renderervulkan/tr_sky.c",
}

MUTANTS = {
    # 1: the batch-close call is dropped: no world batch is ever censused
    1: ("tr_shade", "\tVK_Shade_BatchEnd();\n", ""),
    # 2: the master gate is dropped from the census window
    2: ("vk_shade", "R_ProbeStatsOn() && !backEnd.in2D && shader", "1 && shader"),
    # 3: a fast iterator stops accounting its pass
    3: ("tr_shade", "\t\t\tVK_Shade_NoteSubmitted( 1 );\n", ""),
    # 4: the census is gated on the stale entity again
    4: ("vk_shade", "R_ProbeStatsOn() && !backEnd.in2D && shader",
        "R_ProbeStatsOn() && backEnd.currentEntity == &tr.worldEntity && shader"),
    # 5: the alpha test goes back to the latch, so the lightmapped-multitexture
    #    pass draws alpha-cut art with its alphaFunc switched off
    5: ("vk_shade", "VK_AlphaTestForState( (unsigned long)pStage->stateBits ) );",
        "VK_CurrentAlphaTest() );"),
}

DEF = re.compile(r'^([A-Za-z_][\w \t\*]*?)\b([A-Za-z_]\w*)\s*\(([^;{}]*)\)\s*(\{)?[ \t]*$', re.M)
CALL = re.compile(r'\b([A-Za-z_]\w*)\s*\(')

FAILS = []


def brace_body(text, start):
    depth = 0
    for k in range(start, len(text)):
        if text[k] == "{":
            depth += 1
        elif text[k] == "}":
            depth -= 1
            if depth == 0:
                return text[start:k + 1]
    return None


def definitions(text, into):
    for m in DEF.finditer(text):
        name, brace = m.group(2), m.group(4)
        if not brace:
            nxt = text[m.end():m.end() + 200].lstrip()
            if not nxt.startswith("{"):
                continue
            start = m.end() + (len(text[m.end():m.end() + 200]) - len(nxt))
        else:
            start = m.end() - 1
        b = brace_body(text, start)
        if b:
            into[name] = b


def reaches(bodies, name, sinks, depth=0, seen=None):
    if seen is None:
        seen = set()
    if name in seen or depth > 6:
        return False
    seen.add(name)
    b = bodies.get(name)
    if not b:
        return False
    if any(s + "(" in b for s in sinks):
        return True
    for callee in set(CALL.findall(b)):
        if callee in bodies and reaches(bodies, callee, sinks, depth + 1, seen):
            return True
    return False


def check(cond, what):
    if cond:
        print("  ok    %s" % what)
    else:
        print("  FAIL  %s" % what)
        FAILS.append(what)
    return cond


def main():
    code = sys.argv[1]
    mutate = 0
    if "--mutate" in sys.argv:
        mutate = int(sys.argv[sys.argv.index("--mutate") + 1])

    text, bodies = {}, {}
    for key, rel in FILES.items():
        path = os.path.join(code, rel)
        if not os.path.exists(path):
            continue
        text[key] = open(path).read()
        definitions(text[key], bodies)
    if mutate:
        key, old, new = MUTANTS[mutate]
        if text[key].count(old) < 1:
            print("  FAIL  mutation %d does not apply" % mutate)
            return 1
        text[key] = text[key].replace(old, new, 1)
        bodies.clear()
        for t in text.values():
            definitions(t, bodies)

    begin = bodies.get("VK_Shade_BatchBegin")
    end = bodies.get("VK_Shade_BatchEnd")
    begincall = bodies.get("RB_BeginSurface")
    endcall = bodies.get("RB_EndSurface")

    check(begincall is not None and "VK_Shade_BatchBegin( state )" in begincall,
          "the census window opens with every batch (RB_BeginSurface)")
    gate = ""
    if begin:
        i = begin.find("s_batchCensus")
        gate = begin[i:begin.find(";", i)] if i >= 0 else ""
    check("R_ProbeStatsOn()" in gate, "the census window is inside the master gate")
    check("currentEntity" not in gate, "the census window is not gated on the stale entity")
    check(endcall is not None and "VK_Shade_BatchEnd();" in endcall
          and endcall.find("VK_Shade_BatchEnd();") > endcall.find("tess.currentStageIteratorFunc()"),
          "the census closes after the stage walk in RB_EndSurface")

    if end:
        guard = end.find("!s_batchCensus")
        firstrow = end.find('VK_Guard_Row( "VK:worldset"')
        check(guard >= 0 and firstrow > guard,
              "no census row is reachable while s_batchCensus is false")
        disagreement = end.find("s_batchStagesDrawn != expected")
        sample = end.find("VK_WorldsetSampleDue")
        check(disagreement >= 0 and sample > disagreement,
              "a disagreement branch precedes the sampling branch")

    rows = len(re.findall(r'VK_Guard_Row\(\s*"VK:worldset"', text["vk_shade"]))
    check(rows == text["vk_shade"].count('"VK:worldset"'),
          "every VK:worldset row is emitted through VK_Guard_Row")

    # the honesty r18 added, and the per-unit bind evidence, are in the rows
    check(end is not None and end.count("src=%d srcidx=%d") >= 3,
          "every census row names the submitted geometry (src=/srcidx=)")
    check(end is not None and end.count("binds=[%s]") >= 3,
          "every census row names the bound image per unit (binds=)")
    stages = bodies.get("VK_Shade_ExpectedStages") or ""
    check("%d/%d:0x%x:tc%d,%d" in stages and "%s','%s'%s" in stages,
          "the expected set names rgbGen/alphaGen/stateBits/tcGen and both bundles")

    setup = bodies.get("VK_SetupStageUniforms") or ""
    check("VK_AlphaTestForState( (unsigned long)pStage->stateBits )" in setup,
          "the stage uniform publishes the STAGE's own alpha test")
    check("VK_CurrentAlphaTest" not in setup,
          "the stage uniform does not publish the latched alpha test")
    sel = bodies.get("VK_AlphaTestForState") or ""
    check(sel.count("VKS_ATEST_GT_0") == 1 and sel.count("return 1;") == 1
          and sel.count("return 2;") == 1 and sel.count("return 4;") == 1,
          "the selector is one mapping (GT0=1, LT80=2, GE80=4, off=0)")
    check(text["vk_shade"].count("VK_CurrentAlphaTest()") <= 1,
          "the latch has no publisher left")

    assigned = set(re.findall(r"optimalStageIteratorFunc = (RB_[A-Za-z0-9_]+)",
                              text["tr_shader"]))
    assigned.add("RB_StageIteratorGeneric")
    check(len(assigned) >= 4, "the iterator set was found in tr_shader.c (%d)" % len(assigned))

    sinks = ("VK_DrawStage", "VK_Shade_NoteSubmitted")
    for name in sorted(assigned):
        check(reaches(bodies, name, sinks),
              "%s reaches the pass accounting" % name)

    if FAILS:
        print("staticinvariant: %d failures" % len(FAILS))
        return 1
    print("staticinvariant: green")
    return 0


if __name__ == "__main__":
    sys.exit(main())
