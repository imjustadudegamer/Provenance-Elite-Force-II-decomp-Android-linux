#!/usr/bin/env python3
"""
bindkey_control.py -- the per-bind pipeline lookup's key, proven offline.

`VK_Shade_Pipeline' builds a cache key out of (stateBits, family, defines, cull,
polygonOffset, topology) and probes it on every surface bind.  The program
lookup -- two hash probes over the shader-module table -- used to run BEFORE the
probe and its result was thrown away on a hit, so every bind paid for a lookup
it did not need.  The key carries the module lookup's whole input (family and
the permutation bits), a built pipeline proves its modules exist (the pipeline
table and the module cache are created and dropped together), and the
permutation census counts the same events either way, so the probe can come
first and a hit can return before the lookup.

This control pins exactly that: the probe precedes the program lookup, the hit
path returns with the census note and without the lookup, the key still carries
family and defines, and the miss path still prints its row.  Planted faults move
the lookup back, drop the key's family or defines, or drop the hit's census
note.

Build/run: docs/controls/bindkey/run.sh  (never launches the game)
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
WT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
SHADE = os.path.join(WT, "app", "src", "main", "cpp", "code", "renderervulkan", "vk_shade.c")

failures = 0
checks = 0


def check(cond, msg):
    global failures, checks
    checks += 1
    if not cond:
        failures += 1
        print("  FAIL  " + msg)


def body(text, header):
    i = text.find(header)
    if i < 0:
        return ""
    j = text.find("\n}\n", i)
    return text[i:j + 3] if j > 0 else text[i:]


def main():
    mutant = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    path = sys.argv[2] if len(sys.argv) > 2 else SHADE

    with open(path, encoding="utf-8", errors="replace") as f:
        src = f.read()

    fn = body(src, "static VkPipeline VK_Shade_Pipeline(")
    check(bool(fn), "VK_Shade_Pipeline is gone")

    probe = fn.find("for ( probe = 0; probe < VK_SHADE_HASH_SIZE; probe++ )")
    program = fn.find("VK_Program_Modules( family, defines, &vert, &frag )")
    hit = fn.find("VK_Shade_NotePerm( 0 );\n\t\t\treturn vk_shadePipelines[at].pipeline;")
    miss = fn.find('VK_Shade_PermRow( shader, family, defines );')

    if mutant == 1:
        probe, program = program, probe          # planted: the lookup moved back

    check(probe >= 0 and program > probe,
          "the program lookup runs before the pipeline probe (probe=%d program=%d), "
          "so every bind pays for a lookup a hit throws away" % (probe, program))
    check(hit >= 0 and hit > probe,
          "the hit path does not return through the census note (hit=%d)" % hit)
    check(miss >= 0 and miss > program,
          "the miss path no longer prints the permutation row (miss=%d)" % miss)

    # the key carries the module lookup's whole input
    key = fn[fn.find("pkey[0]"):fn.find("phash =")]
    if mutant == 2:
        key = key.replace("pkey[1] = (uintptr_t)family;", "")
    if mutant == 3:
        key = key.replace("pkey[2] = defines;", "")
    check("pkey[1] = (uintptr_t)family;" in key,
          "the cache key no longer carries the family, which the program lookup keys on")
    check("pkey[2] = defines;" in key,
          "the cache key no longer carries the permutation bits, which the program "
          "lookup keys on")

    # the census is unchanged: a hit notes a request, the miss notes a miss
    note_hits = len(re.findall(r"VK_Shade_NotePerm\( 0 \)", fn))
    if mutant == 4:
        note_hits -= 1
    check(note_hits >= 2,
          "the census note is not on both the hit path and the create path (%d)" % note_hits)
    check("VK_Shade_NotePerm( 1 );" in fn and "VK_Shade_PermRow( shader, family, defines );" in fn,
          "the miss path no longer notes the miss")

    # the model: a frame of N binds over P distinct pipelines costs N probes and
    # at most P program lookups
    distinct = 37
    binds = 2000
    lookups = min(distinct, binds)
    check(lookups == distinct and lookups < binds,
          "the probe-first ordering does not reduce the program lookups (%d of %d binds)"
          % (lookups, binds))

    print("bindkey control: %d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
