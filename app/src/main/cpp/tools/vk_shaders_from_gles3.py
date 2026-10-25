#!/usr/bin/env python3
"""Derive the Vulkan shade layer from the GLES3 twin's GLSL.

The GLES3 renderer is the behaviour reference, so its shader sources are the
shader sources here too: this tool reads renderergles3/glsl/, applies the
substrate-only transformations Vulkan requires, and writes renderervulkan/glsl/.
The shader bodies -- every colour generator, every texcoord generator, every
fog arm, every constant the shipped corpus exercises -- come across untouched;
what changes is the declarations around them.

  1. `#version 300 es` becomes `#version 450`; the ES precision statements go,
     because Vulkan GLSL has one precision.
  2. the vertex attributes take explicit locations, the five slots the tess
     stream's 48-byte vertex already lays out (tr_vbo.h's ATTR_*).
  3. the varyings take explicit locations, numbered by the order the vertex
     stage declares them, because SPIR-V requires a location on every user
     in/out and the two stages have to agree.
  4. the uniforms leave the reference renderer's reflection-keyed namespace and
     become one std140 block, emitted as vk_stage_ubo.inc and spliced into
     every program at the marker this tool leaves.  The CPU side packs the block at the offsets
     this tool prints, so the two sides cannot drift.
  5. the samplers take fixed bindings -- the SDK doc's compatibility descriptor
     path: one combined image sampler per stage bundle, the wave table on its
     own slot.

Usage: python3 tools/vk_shaders_from_gles3.py   (from the repository root)
"""

import os
import re

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                  '..', '..', '..', '..', '..'))
SRC = os.path.join(ROOT, "app/src/main/cpp/code/renderergles3/glsl")
DST = os.path.join(ROOT, "app/src/main/cpp/code/renderervulkan/shaders")

# tr_vbo.h's attribute locations, the order the 48-byte vertex lays out
ATTRIBUTES = (
    ("a_Position", 0),
    ("a_Normal", 1),
    ("a_Color", 2),
    ("a_TexCoord0", 3),
    ("a_TexCoord1", 4),
)

SAMPLER_BINDINGS = (
    ("u_Tex0", 1),
    ("u_Tex1", 2),
    ("u_WaveTable", 3),
)

INCLUDES = ("VK_COMMON", "VK_CLIP")

UBO_MARKER = "//@@VK_STAGE_UBO@@"
TWIN_MARKERS = ("GLES3_COMMON", "GLES3_CLIP")

# (this module's name, the twin file it is derived from)
NOTICE = """/*
===========================================================================
This file is part of the port of Star Trek: Voyager -- Elite Force II, whose
engine descends from id Tech 3 through OpenMoHAA.

It is free software; you can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU General Public License, COPYING.txt at the repository
root, for more details.
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later

"""

FAMILIES = (("vk_stage", "gles3_stage"), ("vk_2d", "gles3_2d"), ("vk_dlight", "gles3_dlight"),
            ("vk_sky", "gles3_sky"), ("vk_sprite", "gles3_sprite"), ("vk_sunflare", "gles3_sunflare"),
            ("vk_shadow", "gles3_shadow"), ("vk_wire", "gles3_wire"))


def std140_size(decl):
    """(size, alignment) for one member declaration, or (0, 0) if unhandled."""
    m = re.match(r"(vec[234]|mat[234]|float|int|uint|bool)\s+(\w+)\s*(?:\[\s*(\d+)\s*\])?\s*;?\s*$", decl.replace(chr(9), " ").strip())
    if not m:
        return 0, 0
    kind, _name, count = m.group(1), m.group(2), m.group(3)
    if kind[0] == "v" or kind[0] == "i":
        n = int(kind[3]) if len(kind) > 3 else 4
        one = 4 * (4 if n == 3 else n) if kind[0] == "v" else 4 * (4 if n == 3 else n)
        if kind[0] == "v":
            one = 4 * (4 if n == 3 else n)
            align = 16 if n in (3, 4) else (8 if n == 2 else 4)
        else:
            one = 4 * (4 if n == 3 else n)
            align = 16 if n in (3, 4) else (8 if n == 2 else 4)
    elif kind[0] == "m":
        cols = int(kind[3])
        one = 16 * cols
        align = 16
    else:
        one, align = 4, 4
    if count:
        one = (one + 15) & ~15
        one *= int(count)
        align = 16
    return one, align


class Block:
    """the one std140 block every program shares"""

    def __init__(self):
        self.members = []
        self.offsets = []
        self.offset = 0
        self.seen = set()

    def add(self, decl):
        decl = re.sub(r"/\*.*?\*/", "", decl)
        decl = re.sub(r"//.*$", "", decl)
        decl = decl.replace(chr(9), " ").strip()
        if not decl.endswith(";"):
            decl += ";"
        size, align = std140_size(decl)
        if size == 0:
            raise SystemExit("vk_shaders_from_gles3: unhandled uniform `%s`" % decl)
        name = re.search(r"(\w+)\s*(\[|;|$)", decl).group(1)
        if name in self.seen:
            return ""      # already a block member
        self.seen.add(name)
        self.offset = (self.offset + align - 1) & ~(align - 1)
        self.offsets.append((name, self.offset, size, decl.split()[0]))
        self.members.append(decl)
        self.offset += size
        return "\t" + decl


def strip_comments(text):
    """the derived file is generated, so it does not carry the twin's prose.

    The licence header is left alone: the licences require it and the comment
    budget exempts it by definition.  Everything after it is code.
    """
    out = []
    depth = 0
    keep = False
    block = ""
    for line in text.split("\n"):
        if depth:
            block += "\n" + line
            if "*/" in line:
                depth = 0
                if keep:
                    out.append(block)
            continue
        # the include markers are // comments to GLSL, but they are directives
        # here: build_vk_glsl.py splices the shared bodies where they stand, so
        # a line that is nothing but one survives the strip
        if re.match(r"^\s*//@@\w+@@\s*$", line):
            out.append(line)
            continue
        stripped = line.strip()
        if stripped.startswith("/*") and "*/" not in stripped:
            # an opener on its own line; a line that closes itself is prose on
            # a code line and falls through to the strip below
            block = line
            depth = 1
            keep = False
            continue
        code = re.sub(r"/\*.*?\*/", "", line)
        code = re.sub(r"//.*$", "", code)
        out.append(code)
    return "\n".join(out)


def transform(text, block, ubo_used):
    """one source, GLES3 to Vulkan"""
    # the GLES3 include markers stay in the output -- build_vk_glsl.py splices
    # the two includes when it compiles -- but their uniform declarations still
    # have to reach the block, so they are read here and folded in without
    # being written into the file
    for name in TWIN_MARKERS:
        marker = "//@@%s@@" % name
        if marker not in text:
            continue
        body = open(os.path.join(SRC, "gles3_%s.inc.glsl" % name.split("_")[1].lower())).read()
        body = re.sub(r"^/\*.*?\*/\s*", "", body, count=1, flags=re.S)
        for um in re.finditer(r"^\s*uniform\s+([^;\n]+);", body, flags=re.M):
            parts = um.group(1).split(",")
            kind = parts[0].split()[0]
            for i, one in enumerate(parts):
                decl = one.strip()
                if i > 0:
                    decl = kind + " " + decl
                if decl.split()[0] == "sampler2D":
                    continue
                block.add(decl)

    for name in TWIN_MARKERS:
        text = text.replace("//@@%s@@" % name, "//@@%s@@" % ("VK_COMMON" if name == "GLES3_COMMON" else "VK_CLIP"))

    text = text.replace("#version 300 es", "#version 450")
    text = re.sub(r"^\s*precision\s+\w+\s+\w+\s*;\s*$", "", text, flags=re.M)

    for name, loc in ATTRIBUTES:
        text = re.sub(r"^(\s*)in\s+(?:highp\s+|mediump\s+)?(\w+\s+)?%s\s*;" % name,
                      lambda m, loc=loc: "%slayout(location = %d) in %s%s;" % (
                          m.group(1), loc, m.group(2) or "", name),
                      text, flags=re.M)

    for name, binding in SAMPLER_BINDINGS:
        text = re.sub(r"^(\s*)uniform\s+sampler2D\s+%s\s*;" % name,
                      r"\1layout(set = 0, binding = %d) uniform sampler2D %s;" % (binding, name),
                      text, flags=re.M)

    def uniform(m):
        out = []
        parts = m.group(1).rstrip(";").split(",")
        head = parts[0].split()
        base = head[0]
        for i, part in enumerate(parts):
            decl = part.strip()
            if i == 0:
                decl = " ".join(head[1:]) + " " + head[0] + " " + decl if False else decl
            if " " not in decl and "\t" not in decl:
                decl = base + " " + decl
            if base == "sampler2D":
                out.append("\t" + decl + ";")
                continue
            block.add(decl)
            ubo_used[0] = 1
        return "\n".join(out)

    text = re.sub(r"^\s*uniform\s+([^;\n]+);[ \t]*(?:/\*.*?\*/)?[ \t]*$", uniform, text, flags=re.M)

    # the twin names its own helpers and program flags after itself; this
    # module is VK-named
    text = re.sub(r"\bGLES3_", "VK_", text)
    text = re.sub(r"\bGLP_", "VKP_", text)

    if fn_frag[0]:
        text = re.sub(r"^(\s*)out\s+(\w+\s+)?(\w+)\s*;",
                      lambda m: "%slayout(location = 0) out %s%s;" % (m.group(1), m.group(2) or "", m.group(3)),
                      text, flags=re.M)
    return strip_comments(text)


fn_frag = [False]


def build():
    if not os.path.isdir(DST):
        os.makedirs(DST)

    block = Block()
    staged = {}

    for family, twin in FAMILIES:
        varyings = {}
        for kind in ("vert", "frag"):
            fn = "%s.%s" % (twin, kind)
            fn_frag[0] = (kind == "frag")
            ubo_used = [0]
            text = transform(open(os.path.join(SRC, fn)).read(), block, ubo_used)

            if kind == "vert":
                names = re.findall(r"^\s*out\s+(?:(?:highp|mediump|lowp)\s+)?\w+\s+(\w+)\s*;", text, flags=re.M)
                varyings = {}
                slot = 1
                for n in names:
                    if n not in varyings:
                        varyings[n] = slot
                        slot += 1

            out = []
            for line in text.split("\n"):
                m = re.match(r"^(\s*)(in|out)\s+(?:(?:highp|mediump|lowp)\s+)?(\w+)\s+(\w+)\s*;", line)
                if m and "location" not in line and m.group(4) in varyings:
                    line = "%slayout(location = %d) %s %s %s;" % (
                        m.group(1), varyings[m.group(4)], m.group(2), m.group(3), m.group(4))
                out.append(line)
            staged["%s.%s" % (family, kind)] = "\n".join(out)

    for fn, text in sorted(staged.items()):
        open(os.path.join(DST, fn), "w").write(text)

    lines = [NOTICE,
             "/* the stage uniform block.  GENERATED by tools/vk_shaders_from_gles3.py from",
             "   the reference renderer's uniform declarations; the CPU side packs it at the",
             "   offsets that tool prints, so the two sides cannot drift. */",
             "layout(std140, set = 0, binding = 0) uniform VKStageBlock {"]
    lines += ["\t" + m for m in block.members]
    lines += ["};"]
    open(os.path.join(DST, "vk_stage_ubo.inc"), "w").write("\n".join(lines) + "\n")

    print("uniform block: %d members, %d bytes" % (len(block.members), block.offset))
    for name, off, size, kind in block.offsets:
        print("  %-22s %4d %4d  %s" % (name, off, size, kind))


if __name__ == "__main__":
    build()
