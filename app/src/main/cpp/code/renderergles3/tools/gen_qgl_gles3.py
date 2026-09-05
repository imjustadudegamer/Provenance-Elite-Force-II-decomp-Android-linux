#!/usr/bin/env python3
# ===========================================================================
# gen_qgl_gles3.py -- emit renderergles3/qgl_gles3_procs.h
#
# WHY THIS EXISTS.  The G1 loader must cover EVERY GL entry point the renderer
# can reach, and "we think we got them all" is not a proof.  So the symbol
# set is DERIVED, twice, from machine-readable sources and the derived file is
# what the build compiles:
#
#   * the shared lists in renderercommon/qgl.h  (the inherited desktop set that
#     the GL1 renderer's tr_local.h already declared -- GL1 was the fork base);
#   * the Khronos GLES 3.0 core header <GLES3/gl3.h> (what a GLES3 renderer is
#     actually allowed to call), minus the names the shared lists already
#     carry so no symbol is declared or defined twice;
#   * the Khronos GLES 3.2 core header <GLES3/gl32.h>, for the names that became
#     core in a later ES than the 3.0 set and that the renderer's own sources call.
#     A 3.2-core name is resolved like the rest and trapped, not required.
#
# The generated macro QGL_GLES3_CORE_PROCS is expanded three times:
#   1. qgl_gles3.h  -- typedefs (name##proc) and `extern ... * qgl##name`;
#   2. qgl_gles3.c  -- the pointer DEFINITIONS;
#   3. qgl_gles3.c  -- the loader table (name string -> &qgl##name).
# QGL_GLES3_32_PROCS is expanded the same four ways beside it.  One source of
# truth, so "declared but never defined" cannot happen.
#
# RE-RUN CHECK (armed): re-running this
# script and diffs the result against the committed file.  If a Khronos header
# on the machine disagrees with the committed header, the diff shows it.
#
# Usage:
#   python3 tools/gen_qgl_gles3.py [path-to-gl3.h] [path-to-gl32.h] > qgl_gles3_procs.h
# Default inputs: $EF2_GLES3_HEADER then /usr/include/GLES3/gl3.h,
#                 $EF2_GLES32_HEADER then /usr/include/GLES3/gl32.h
# ===========================================================================
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MODULE = os.path.dirname(HERE)
CODE = os.path.dirname(MODULE)
SHARED_QGL = os.path.join(CODE, "renderercommon", "qgl.h")
RENDERER = os.path.join(CODE, "renderergles3")


def shared_names():
    """Every GLE(ret, Name, ...) name carried by renderercommon/qgl.h, plus the
    five names that header declares by hand."""
    with open(SHARED_QGL, "r", errors="ignore") as fh:
        txt = fh.read()
    names = set(m.group(1) for m in re.finditer(r"GLE\(\s*[^,]+,\s*([A-Za-z0-9_]+)\s*,", txt))
    names |= {
        "ActiveTextureARB",
        "ClientActiveTextureARB",
        "MultiTexCoord2fARB",
        "LockArraysEXT",
        "UnlockArraysEXT",
    }
    return names


def gl3_prototypes(path):
    """(ret, name, args) for every GL_APICALL prototype in a Khronos header.

    The character classes are deliberately [^;] rather than . with DOTALL: a
    Khronos prototype is "GL_APICALL <ret> GL_APIENTRY glFoo (args);" on one
    logical line, and a non-greedy .*? with DOTALL happily spans the ';' and
    swallows the NEXT prototype.  Rejecting ';' keeps every match inside one
    statement.  The separator before GL_APIENTRY is \\s* and not \\s+, because a
    pointer return closes on the calling convention with no blank:
    "void *GL_APIENTRY glMapBufferRange(...)" -- the header writes the star
    against GL_APIENTRY, so a required blank drops that prototype silently.
    """
    with open(path, "r", errors="ignore") as fh:
        txt = fh.read()
    out = []
    for m in re.finditer(
        r"GL_APICALL\s+([^;]*?)\s*GL_APIENTRY\s+(gl\w+)\s*\(([^;]*?)\);", txt
    ):
        ret = " ".join(m.group(1).split())
        name = m.group(2)[2:]
        arg = " ".join(m.group(3).split())
        out.append((ret, name, arg))
    return out


def renderer_call_sites():
    """Every `qgl<Name>(' the renderer's own sources spell out.

    Every qgl name the renderer every qgl name the renderer
    references is declared by the loader, so the derivation here has to be the
    same walk over the same files: one regex, every .c/.cpp/.h under
    renderergles3, comments and strings included.
    """
    used = set()
    call = re.compile(r"\bqgl([A-Za-z0-9_]+)\s*\(")
    for dirpath, _, files in os.walk(RENDERER):
        for fn in files:
            if not fn.endswith((".c", ".cpp", ".h")):
                continue
            with open(os.path.join(dirpath, fn), "r", errors="ignore") as fh:
                for m in call.finditer(fh.read()):
                    used.add(m.group(1))
    return used


def main():
    hdr = sys.argv[1] if len(sys.argv) > 1 else os.environ.get(
        "EF2_GLES3_HEADER", "/usr/include/GLES3/gl3.h"
    )
    if not os.path.exists(hdr):
        sys.stderr.write("gen_qgl_gles3.py: cannot read %s\n" % hdr)
        return 2
    hdr32 = sys.argv[2] if len(sys.argv) > 2 else os.environ.get(
        "EF2_GLES32_HEADER", "/usr/include/GLES3/gl32.h"
    )
    if not os.path.exists(hdr32):
        sys.stderr.write("gen_qgl_gles3.py: cannot read %s\n" % hdr32)
        return 2
    listed = shared_names()
    core = [p for p in gl3_prototypes(hdr) if p[1] not in listed]
    core.sort(key=lambda p: p[1])
    if len(core) < 100:
        sys.stderr.write(
            "gen_qgl_gles3.py: only %d ES3-core names parsed -- wrong header?\n" % len(core)
        )
        return 3

    # names core in a later ES than the 3.0 set above, that the renderer calls,
    # and that neither the shared lists nor the 3.0 macro already carry
    core_names = set(p[1] for p in core) | listed
    have = gl3_prototypes(hdr32)
    later = [p for p in have
             if p[1] not in core_names and p[1] in renderer_call_sites()]
    later.sort(key=lambda p: p[1])

    print("/*")
    print("===========================================================================")
    print("This file is part of the port of Star Trek: Voyager -- Elite Force II, whose")
    print("engine descends from id Tech 3 through OpenMoHAA.")
    print("")
    print("It is free software; you can redistribute it and/or modify it under the terms of")
    print("the GNU General Public License as published by the Free Software Foundation;")
    print("either version 2 of the License, or (at your option) any later version.")
    print("")
    print("It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;")
    print("without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR")
    print("PURPOSE.  See the GNU General Public License, COPYING.txt at the repository")
    print("root, for more details.")
    print("===========================================================================")
    print("*/")
    print("// SPDX-License-Identifier: GPL-2.0-or-later")
    print("// Modified 2026-10-04 for this port; the changes are listed in PROVENANCE.tsv.")
    print("/*")
    print(" * qgl_gles3_procs.h -- GENERATED by tools/gen_qgl_gles3.py; do not edit.")
    print(" *")
    print(" * Input: %s" % hdr)
    print(" *        %s (core in a later ES than the set above)" % hdr32)
    print(" *        %s (for the names already carried there)" % os.path.relpath(SHARED_QGL, CODE))
    print(" * GLES 3.0 core entry points not already carried by the shared lists: %d" % len(core))
    print(" * entry points core only in a later ES that the renderer calls: %d" % len(later))
    print(" *")
    print(" * Every name here is loaded by qgl_gles3.c through SDL_GL_GetProcAddress")
    print(" * and, when the context does not provide it, replaced by a trap that")
    print(" * prints `[GLES3:glmissing] name=<name>` -- never a NULL call, never a")
    print(" * silent wrong render.  A later-ES name is trapped the same way and is NOT")
    print(" * counted as a missing core entry point, because a 3.0 context is still a")
    print(" * working context for the code that never calls it.")
    print(" */")
    print("#ifndef GLES3_QGL_GLES3_PROCS_H")
    print("#define GLES3_QGL_GLES3_PROCS_H")
    print()
    print("#define QGL_GLES3_CORE_PROCS \\")
    for ret, name, arg in core:
        print("\tGLE(%s, %s, %s) \\" % (ret, name, arg))
    # terminate the macro with a final line with no backslash
    print("\t/* end QGL_GLES3_CORE_PROCS */")
    print()
    print("#define QGL_GLES3_32_PROCS \\")
    for ret, name, arg in later:
        print("\tGLE(%s, %s, %s) \\" % (ret, name, arg))
    print("\t/* end QGL_GLES3_32_PROCS */")
    print()
    print("#endif /* GLES3_QGL_GLES3_PROCS_H */")
    return 0


if __name__ == "__main__":
    sys.exit(main())
