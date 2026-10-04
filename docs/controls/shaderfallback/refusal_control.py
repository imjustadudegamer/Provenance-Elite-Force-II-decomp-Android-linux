#!/usr/bin/env python3
"""The loud-refusal control: every fallback site in the Vulkan lane either
refuses at the point of failure or names itself, and the content fallbacks the
port keeps because retail keeps them carry the decompile that says so.

Mode 0 is green on the shipping tree.  Each later mode plants one fault and
must go red, so the checks are proven to fire rather than merely present.
Never launches the game.
"""

import os
import re
import sys

RENDERER = os.path.join("app", "src", "main", "cpp", "code", "renderervulkan")


def read(rel):
    with open(os.path.join(RENDERER, rel), encoding="utf-8") as fh:
        return fh.read()


def check_guarded_row(text, tag, marker, label):
    """A row that names a fallback must sit behind the master gate."""
    idx = text.find(marker)
    if idx < 0:
        return "%s: no %s row" % (label, marker)
    window = text[max(0, idx - 900):idx]
    if "R_ProbeStatsOn()" not in window:
        return "%s: %s row is not behind the master gate" % (label, marker)
    return None


def mode_default(src):
    """Green tree: the arena sits out, its bake verifies its own rebase, and
    the disarm fallback names itself."""
    vbo = read("vk_vbo.c")
    shade = read("vk_shade.c")

    if '"r_vboStatic", "0"' not in vbo:
        return "the arena's compiled default is not off"
    if "if ( rel < 0 || rel >= r->numVerts ) {" not in vbo:
        return "the bake does not refuse a rebase that leaves its own span"
    if '"bake refused' not in vbo:
        return "the bake refusal names nothing"

    fault = check_guarded_row(shade, "VK:shade", '"unit disarmed tmu=', "disarm")
    if fault:
        return fault

    # The bake refusal must reclaim the region it reserved: the arena counters
    # advance only after a clean bake.
    bake = vbo[vbo.index("static qboolean VK_StaticBake"):vbo.index("void RB_VboStaticBakeFrame")]
    if bake.index("e->surf = NULL;") > bake.index("s_arenaVerts +="):
        return "the bake refusal does not reclaim its region"
    if 'if ( R_ProbeStatsOn() ) {' not in bake:
        return "the bake refusal rows are not behind the master gate"
    if bake.count('if ( R_ProbeStatsOn() ) {') < 2:
        return "the bake does not gate both its refusal rows"
    return None


def mode_arena_on(src):
    """Fault: the arena back on without accounting for itself."""
    _sub("vk_vbo.c", '"r_vboStatic", "0"', '"r_vboStatic", "1"', "arena-on")


def mode_bake_unchecked(src):
    """Fault: the bake writes an out-of-span rebase instead of refusing."""
    _sub("vk_vbo.c", "if ( rel < 0 || rel >= r->numVerts ) {",
         "if ( rel < 0 && rel >= r->numVerts ) {", "bake-unchecked")


def mode_bake_ungated(src):
    """Fault: the bake's rebase-refusal row fires with the master gate off.
    The bake carries two gated rows now, so the plant names its own."""
    _sub("vk_vbo.c",
         'if ( R_ProbeStatsOn() ) {\n\t\t\t\tVK_Guard_Row( "VK:vbo", "bake refused shader=\'%s\' idx=%d rel=%d verts=%d region=%d",',
         'if ( 1 ) {\n\t\t\t\tVK_Guard_Row( "VK:vbo", "bake refused shader=\'%s\' idx=%d rel=%d verts=%d region=%d",',
         "bake-ungated")


def mode_disarm_ungated(src):
    """Fault: the disarm row fires with the master gate off."""
    _sub("vk_shade.c", "if ( R_ProbeStatsOn() && s_disarmRows < 16 ) {",
         "if ( s_disarmRows < 16 ) {", "disarm-ungated")


FAULTS = {
    1: mode_arena_on,
    2: mode_bake_unchecked,
    3: mode_bake_ungated,
    4: mode_disarm_ungated,
}


def _write(rel, text):
    with open(os.path.join(RENDERER, rel), "w", encoding="utf-8") as fh:
        fh.write(text)


def _sub(rel, old, new, label):
    """Plant one fault, or say so: a fault that matches nothing is a control
    that passes for the wrong reason."""
    text = read(rel)
    if old not in text:
        raise SystemExit("  FAIL  %s: fault does not match the tree" % label)
    _write(rel, text.replace(old, new, 1))


def main():
    mode = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    src = sys.argv[2] if len(sys.argv) > 2 else os.getcwd()

    os.chdir(src)

    if mode in FAULTS:
        FAULTS[mode](src)
        # A fault is only proven when the green check goes red against it.
        fault = mode_default(src)
        if fault is None:
            print("  FAIL  planted fault %d went green" % mode)
            return 1
        print("  ok    fault %d red: %s" % (mode, fault))
        return 0

    fault = mode_default(src)
    if fault:
        print("  FAIL  %s" % fault)
        return 1
    print("  ok    arena off, bake verifies its rebase, disarm names itself")
    return 0


if __name__ == "__main__":
    sys.exit(main())
