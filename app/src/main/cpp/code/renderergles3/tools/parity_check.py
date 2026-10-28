#!/usr/bin/env python3
"""The twin's compile-time parity gate.

Three things must agree by construction, and only a build-side check holds
them:

  1. the twin's state word and generator ordinals are the Vulkan renderer's --
     both renderers parse the same shader text and must fold the same bits, or
     the same shader draws differently per renderer;
  2. the GLSL dispatch literals are the enums -- gles3_stage.vert switches on
     `u_AlphaGen == 18' and friends, so an enum reorder would silently move a
     generator onto another arm;
  3. the derived Vulkan shaders still carry the twin's dispatch literal sets --
     tools/vk_shaders_from_gles3.py reads renderergles3/glsl/, so the twin's
     generator arithmetic is the shipped Vulkan renderer's too.

Exit: 0 green, 1 a check failed.
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CODE = os.path.abspath(os.path.join(HERE, "..", ".."))
TWIN = os.path.join(CODE, "renderergles3")
VK = os.path.join(CODE, "renderervulkan")


def read(path):
    with open(path, "r", encoding="utf-8", errors="surrogateescape") as fh:
        return fh.read()


def enum_ordinals(text, name):
    """(member -> ordinal) for `typedef enum { ... } name;'."""
    end = text.find("} %s;" % name)
    if end < 0:
        return None
    start = text.rfind("typedef enum {", 0, end)
    if start < 0:
        return None
    ordinals = {}
    value = 0
    for raw in text[start + len("typedef enum {"):end].splitlines():
        line = raw.split("//")[0].split("/*")[0].strip().rstrip(",")
        if not line:
            continue
        if "=" in line:
            member, _, rhs = line.partition("=")
            member = member.strip()
            try:
                value = int(rhs.strip(), 0)
            except ValueError:
                continue    # an expression; its use sites pin it instead
        else:
            member = line
        ordinals[member] = value
        value += 1
    return ordinals


def defines(text, prefix):
    out = {}
    for m in re.finditer(r"^#define\s+(%s[A-Za-z0-9_]*)\s+(0x[0-9a-fA-F]+|\d+)"
                         % prefix, text, re.M):
        out[m.group(1)] = int(m.group(2), 0)
    return out


def glsl_literals(source, uniform):
    """u_X == N literals in GLSL text, as {N: count}."""
    out = {}
    for m in re.finditer(r"%s\s*==\s*(0x[0-9a-fA-F]+|\d+)" % uniform, source):
        n = int(m.group(1), 0)
        out[n] = out.get(n, 0) + 1
    return out


def main():
    twin_local = read(os.path.join(TWIN, "tr_local.h"))
    vk_local = read(os.path.join(VK, "tr_local.h"))
    fails = []

    # ---- 1. the state word, bit for bit ------------------------------------
    twin_bits = defines(twin_local, "GLS_")
    vk_bits = {k.replace("VKS_", "GLS_", 1): v
               for k, v in defines(vk_local, "VKS_").items()}
    if twin_bits != vk_bits:
        fails.append("state word: twin-only=%s vulkan-only=%s differing=%s"
                     % (sorted(set(twin_bits) - set(vk_bits)),
                        sorted(set(vk_bits) - set(twin_bits)),
                        sorted(k for k in set(twin_bits) & set(vk_bits)
                               if twin_bits[k] != vk_bits[k])))

    # ---- 2. the generator ordinals, member for member ----------------------
    for enum in ("alphaGen_t", "colorGen_t", "texCoordGen_t"):
        a = enum_ordinals(twin_local, enum)
        b = enum_ordinals(vk_local, enum)
        if a is None or b is None:
            fails.append("%s: not found in both renderers" % enum)
            continue
        if a != b:
            fails.append("%s: twin-only=%s vulkan-only=%s differing=%s"
                         % (enum, sorted(set(a) - set(b)),
                            sorted(set(b) - set(a)),
                            sorted(k for k in set(a) & set(b) if a[k] != b[k])))

    # ---- 3. the GLSL dispatch literals are the enums -----------------------
    agen = enum_ordinals(twin_local, "alphaGen_t")
    cgen = enum_ordinals(twin_local, "colorGen_t")
    stage_v = read(os.path.join(TWIN, "glsl", "gles3_stage.vert"))
    stage_f = read(os.path.join(TWIN, "glsl", "gles3_stage.frag"))
    for uniform, table, source, name in (("u_AlphaGen", agen, stage_v, "gles3_stage.vert"),
                                         ("u_ColorGen", cgen, stage_v, "gles3_stage.vert"),
                                         ("u_AlphaGen", agen, stage_f, "gles3_stage.frag"),
                                         ("u_ColorGen", cgen, stage_f, "gles3_stage.frag")):
        if table is None:
            continue
        names = {}
        for member, value in table.items():
            names.setdefault(value, []).append(member)
        for value in sorted(glsl_literals(source, uniform)):
            if value not in names:
                fails.append("%s: %s == %d names no %s member"
                             % (name, uniform, value,
                                "alphaGen_t" if "Alpha" in uniform else "colorGen_t"))

    # ---- 4. the derived Vulkan shaders carry the same dispatch -------------
    for name in ("vk_stage.vert", "vk_stage.frag"):
        path = os.path.join(VK, "shaders", name)
        if not os.path.exists(path):
            fails.append("renderervulkan/shaders/%s: missing" % name)
            continue
        theirs = read(path)
        mine = stage_v if name.endswith(".vert") else stage_f
        for uniform in ("u_AlphaGen", "u_ColorGen"):
            a = glsl_literals(mine, uniform)
            b = glsl_literals(theirs, uniform)
            if a != b:
                fails.append("%s vs gles3: %s literals diverge "
                             "(twin-only=%s vulkan-only=%s)"
                             % (name, uniform,
                                sorted(set(a) - set(b)), sorted(set(b) - set(a))))

    if fails:
        print("parity_check: RED -- %d check(s) failed" % len(fails))
        for f in fails:
            print("  " + f)
        return 1
    print("parity_check: GREEN -- the state word and generator ordinals agree "
          "across both renderers, and the GLSL dispatch literals name the enums "
          "in both the twin and the derived Vulkan shaders")
    return 0


if __name__ == "__main__":
    sys.exit(main())
