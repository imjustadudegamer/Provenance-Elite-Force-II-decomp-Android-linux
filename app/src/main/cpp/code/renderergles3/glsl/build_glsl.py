#!/usr/bin/env python3
"""
GLSL ES 3.00 build + validation for the gles3-gles3-shade change (G2).

WHAT IT DOES
------------
1. Substitutes the shared library (glsl/gles3_common.inc.glsl) into every
   program source at the `//@@GLES3_COMMON@@` marker, producing
   glsl/.built/<name>.  GLSL ES 3.00 has no #include, so the substitution is
   done here and at RUNTIME by tr_program.c's loader (which uses the embedded
   header emitted in step 3), exactly as idTech renderergl2 concatenates its
   preamble in tr_glsl.c.
2. Validates every built source with glslangValidator, once per distinct
   GLES3_PROGRAM_KEY defines bitmask in tr_program_table.inc, so a permutation
   that does not compile fails the build rather than a device run.
3. Emits glsl/gles3_glsl_embedded.h -- a C string table of the built sources
   keyed by program name, so the renderer never reads a file at run time.

Re-running this script refreshes the emitted
header, so the embedded sources cannot drift from
the .glsl sources.
"""

import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
BUILT = os.path.join(HERE, '.built')

GLP_NAMES = [
    (0x00000001, 'GLP_RGBAGEN'),
    (0x00000002, 'GLP_TCGEN0'),
    (0x00000004, 'GLP_TCGEN1'),
    (0x00000008, 'GLP_TCMOD0'),
    (0x00000010, 'GLP_TCMOD1'),
    (0x00000020, 'GLP_FOG'),
    (0x00000040, 'GLP_LIGHTMAP'),
    (0x00000080, 'GLP_VERTEXLIT'),
    (0x00000100, 'GLP_LIGHTMAP_MT'),
    (0x00000200, 'GLP_DLIGHT'),
    (0x00000400, 'GLP_2D'),
    (0x00000800, 'GLP_SKY'),
    (0x00001000, 'GLP_ALPHATEST'),
    (0x00002000, 'GLP_SKIP_COLOR'),
    (0x00004000, 'GLP_NORMAL'),
    (0x00008000, 'GLP_WAVE'),
    (0x00010000, 'GLP_SPECULAR'),
    (0x00020000, 'GLP_ENTITY_SPACE'),
    (0x00040000, 'GLP_DEFORM'),
    (0x00080000, 'GLP_SHADOW'),
    (0x00100000, 'GLP_HEIGHTFOG'),
    # texenv / fog-mode sub-permutations used by the stage fragment shader
    (0x00200000, 'GLP_TEXENV_ADD'),
    (0x00400000, 'GLP_TEXENV_INTERPOLATE'),
    (0x00800000, 'GLP_TEXENV_COMBINE4'),
]

RE_KEY = re.compile(r'GLES3_PROGRAM_KEY\([^)]*?(0x[0-9a-fA-F]+|\d+)\s*,\s*"([^"]*)"\s*,')

# ---------------------------------------------------------------------------
# STAGE-MATCHED PRECISION PRELUDE (gles3 0.2.79 -- the white-screen root fix).
#
# GLSL ES 3.00 gives int a DIFFERENT default precision per stage: highp in the
# vertex language, mediump in the fragment language (OpenGL ES 3.0 Programming
# Guide, "Precision qualifiers", pp. 119-120; GLSL ES 3.00 spec section 4.5.3).
# A uniform or varying declared in BOTH stages without an explicit precision is
# therefore a LINK error, not a compile error -- which is exactly why the
# per-source compile validation passed while a device failed.  Measured on the
# Pixel 8's Mali-G715:
#     ERROR: GLES3 program 'gles3_stage' (defines 0x02000b) failed to link:
#     L0001 The fragment integer variable u_Acff does not match the vertex
#           variable u_Acff.  The precision does not match.
#     ... u_FogDensityArm, u_FogViewOutside, u_FogNum likewise
# and with the stage program unlinked every stage draw rendered WHITE.
#
# The fix is the one the reference GLES renderers use: emit the SAME explicit
# prelude into every source (GZDoom's GLES backend builds it in
# common/rendering/gles/gles_shader.cpp GetGLSLPrecision():
# "precision highp int;\nprecision highp float;").  Both stages then default to
# highp, so every shared interface matches -- present and future.
#
# MALI_FIXES.md note: its entry #2 (never emit gl_ClipDistance on Mali) is why
# the portal clip is a varying + fragment discard in gles3_clip.inc.glsl rather
# than gl_ClipDistance; that choice already matches the documented guidance.
PRECISION_PRELUDE = (
    '/* gles3: stage-matched default precision (see build_glsl.py); int and\n'
    '   sampler2D defaults differ per stage in ES 3.00, and a shared uniform or\n'
    '   varying must match or the pair is a LINK error on a strict driver.\n'
    '   sampler2D is pinned highp in BOTH stages on purpose: u_WaveTable is an\n'
    '   R32F sin/table texture read with texelFetch and its value precision is\n'
    '   the sampler qualifier, so lowp would truncate the oracle-exact table. */\n'
    'precision highp int;\n'
    'precision highp float;\n'
    'precision highp sampler2D;\n'
)


def with_precision_prelude(src):
    """Insert the prelude immediately after `#version`, in every source."""
    lines = src.split('\n')
    for i, ln in enumerate(lines):
        if ln.strip().startswith('#version'):
            return '\n'.join(lines[:i + 1] + [PRECISION_PRELUDE] + lines[i + 1:])
    return src


def defines_lines(mask):
    return ['#define %s 1' % n for bit, n in GLP_NAMES if mask & bit]


def substitute(src):
    """Expand the two shared source chunks.

    `//@@GLES3_COMMON@@` -- the generator/fog/wave library (gles3_common.inc.glsl).
    `//@@GLES3_CLIP@@`   -- the portal clip plane (gles3_clip.inc.glsl), carried
                          by every program that can be bound while the plane is
                          enabled; see the chunk's header for the adjudication.
    Two separate chunks so the deliberately small UI/sprite/flare programs do
    not have to carry the generator library (old-Mali shader-compiler faults,
    MALI_FIXES.md)."""
    common = open(os.path.join(HERE, 'gles3_common.inc.glsl'), 'r', encoding='utf-8').read()
    clip = open(os.path.join(HERE, 'gles3_clip.inc.glsl'), 'r', encoding='utf-8').read()
    src = src.replace('//@@GLES3_COMMON@@', common)
    src = src.replace('//@@GLES3_CLIP@@', clip)
    return src


def build(verbose=True):
    os.makedirs(BUILT, exist_ok=True)
    sources = []
    for f in sorted(os.listdir(HERE)):
        if f.endswith('.inc.glsl'):
            continue		# shared chunks: substituted, never compiled
        if f.endswith('.vert') or f.endswith('.frag'):
            sources.append(f)
    embedded = {}
    for f in sources:
        raw = open(os.path.join(HERE, f), 'r', encoding='utf-8').read()
        out = with_precision_prelude(substitute(raw))
        path = os.path.join(BUILT, f)
        with open(path, 'w', encoding='utf-8') as fh:
            fh.write(out)
        embedded[f] = out
        if verbose:
            print('built %s (%d lines)' % (f, out.count('\n') + 1))
    return embedded


def validate(masks):
    """glslangValidator each source under each defines mask it can consume.

    glslangValidator only accepts -D with -l, so the #defines are injected
    into a temp file immediately after the #version line (where the runtime
    loader also injects them, tr_program.c GLES3_BuildSource)."""
    import tempfile
    fails = []
    for f in sorted(os.listdir(BUILT)):
        path = os.path.join(BUILT, f)
        stage = 'vert' if f.endswith('.vert') else 'frag'
        src = open(path, 'r', encoding='utf-8').read()
        for mask in sorted(set(masks)):
            defs = '\n'.join(defines_lines(mask))
            lines = src.split('\n')
            # find the #version line and insert after it
            out = []
            injected = False
            for ln in lines:
                out.append(ln)
                if not injected and ln.strip().startswith('#version'):
                    out.append(defs)
                    injected = True
            body = '\n'.join(out)
            with tempfile.NamedTemporaryFile('w', suffix='.' + stage, delete=False) as tf:
                tf.write(body)
                tmp = tf.name
            r = subprocess.run(['glslangValidator', '-S', stage, tmp],
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            os.unlink(tmp)
            if r.returncode != 0:
                fails.append((f, hex(mask), r.stdout.decode('utf-8', 'replace')))
    return fails


def _inject_defines(src, mask):
    """Insert the mask's #defines right after #version (the runtime does the same)."""
    defs = '\n'.join(defines_lines(mask))
    out, injected = [], False
    for ln in src.split('\n'):
        out.append(ln)
        if not injected and ln.strip().startswith('#version'):
            out.append(defs)
            injected = True
    return '\n'.join(out)


def validate_links(masks):
    """LINK each (vertex, fragment) pair under each defines mask.

    Precision defaults differ per stage, so a pair can compile cleanly source by
    source and still fail to link on a strict driver.  This is the check that
    would have caught the Mali 'Precision qualifiers must match' failure that
    rendered the device white; glslangValidator's -l enforces it (measured)."""
    import shutil
    import tempfile
    fails = []
    fams = {}
    for f in sorted(os.listdir(BUILT)):
        base, ext = os.path.splitext(f)
        fams.setdefault(base, {})[ext] = os.path.join(BUILT, f)
    for base, st in sorted(fams.items()):
        if '.vert' not in st or '.frag' not in st:
            continue
        vsrc = open(st['.vert'], 'r', encoding='utf-8').read()
        fsrc = open(st['.frag'], 'r', encoding='utf-8').read()
        for mask in sorted(set(masks)):
            d = tempfile.mkdtemp(prefix='gles3link_')
            try:
                vp = os.path.join(d, base + '.vert')
                fp = os.path.join(d, base + '.frag')
                with open(vp, 'w', encoding='utf-8') as fh:
                    fh.write(_inject_defines(vsrc, mask))
                with open(fp, 'w', encoding='utf-8') as fh:
                    fh.write(_inject_defines(fsrc, mask))
                r = subprocess.run(['glslangValidator', '-l', vp, fp],
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            finally:
                shutil.rmtree(d, ignore_errors=True)
            if r.returncode != 0:
                fails.append(('%s.vert + %s.frag' % (base, base), hex(mask),
                              r.stdout.decode('utf-8', 'replace')))
    return fails


def emit_header(embedded, out_path):
    with open(out_path, 'w', encoding='utf-8') as fh:
        fh.write('/* GENERATED by glsl/build_glsl.py -- do not edit by hand.\n'
                 ' * Embedded GLSL ES 3.00 sources for code/renderergles3.\n'
                 ' * The .glsl files under glsl/ are the sources of truth;\n'
                 ' * build_glsl.py regenerates\n'
                 ' * header so it cannot drift. */\n')
        fh.write('#ifndef __GLES3_GLSL_EMBEDDED_H__\n#define __GLES3_GLSL_EMBEDDED_H__\n\n')
        for name in sorted(embedded):
            sym = re.sub(r'[^A-Za-z0-9]', '_', name)
            body = embedded[name].replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n"\n"')
            fh.write('static const char gles3_glsl_%s[] =\n"%s";\n\n' % (sym, body))
        fh.write('typedef struct { const char *name; const char *src; } gles3GlslSource_t;\n')
        fh.write('static const gles3GlslSource_t gles3GlslSources[] = {\n')
        for name in sorted(embedded):
            sym = re.sub(r'[^A-Za-z0-9]', '_', name)
            fh.write('\t{ "%s", gles3_glsl_%s },\n' % (name, sym))
        fh.write('};\n')
        fh.write('#define GLES3_NUM_GLSL_SOURCES %d\n\n' % len(embedded))
        fh.write('#endif\n')


def main():
    inc = os.path.abspath(os.path.join(HERE, '..', 'tr_program_table.inc'))
    masks = set()
    if os.path.exists(inc):
        txt = open(inc, 'r', encoding='utf-8').read()
        for m in re.finditer(r'GLES3_PROGRAM_KEY\(([^)]*)\)', txt):
            parts = [p.strip() for p in m.group(1).split(',')]
            try:
                masks.add(int(parts[8], 0))
            except (ValueError, IndexError):
                pass
    masks.add(0x00000000)
    # The texenv sub-permutations are NOT in the table's `defines' column (they
    # are recorded in the texEnv column and OR'd in by GLES3_TexEnvForStage), so
    # they have to be seeded here or gles3_stage.frag's GL_ADD / GL_COMBINE /
    # GL_COMBINE4_NV branches would never be compiled at build time.
    masks.update([
        0x00200000,                    # GLP_TEXENV_ADD
        0x00400040,                    # GLP_TEXENV_INTERPOLATE | GLP_LIGHTMAP
        0x00800040,                    # GLP_TEXENV_COMBINE4    | GLP_LIGHTMAP
        0x00200040,                    # GLP_TEXENV_ADD         | GLP_LIGHTMAP
        0x00100040,                    # GLP_ALPHATEST          | GLP_LIGHTMAP
    ])
    embedded = build()
    fails = validate(masks)
    lfails = validate_links(masks)
    out = os.path.join(HERE, 'gles3_glsl_embedded.h')
    emit_header(embedded, out)
    print('validated %d sources x %d define masks' % (
        len([f for f in os.listdir(BUILT)]), len(masks)))
    print('linked %d (vertex, fragment) pairs x %d define masks' % (
        len([b for b in set(os.path.splitext(f)[0] for f in os.listdir(BUILT))
             if os.path.exists(os.path.join(BUILT, b + '.vert'))
             and os.path.exists(os.path.join(BUILT, b + '.frag'))]), len(masks)))
    if lfails:
        print('LINK VALIDATION FAILED: %d (pair, mask) combinations' % len(lfails))
        for f, mask, log in lfails[:10]:
            print('--- %s %s ---' % (f, mask))
            print(log[:4000])
        return 1
    if fails:
        print('VALIDATION FAILED: %d (source, mask) pairs' % len(fails))
        for f, mask, log in fails[:10]:
            print('--- %s %s ---' % (f, mask))
            print(log[:4000])
        return 1
    print('GLSL OK: every source compiles AND every (vert, frag) pair links for every defines mask.')
    print('emitted %s' % out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
