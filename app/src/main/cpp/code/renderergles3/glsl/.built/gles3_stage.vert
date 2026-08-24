#version 300 es
/* r200: stage-matched default precision (see build_glsl.py); int and
   sampler2D defaults differ per stage in ES 3.00, and a shared uniform or
   varying must match or the pair is a LINK error on a strict driver.
   sampler2D is pinned highp in BOTH stages on purpose: u_WaveTable is an
   R32F sin/table texture read with texelFetch and its value precision is
   the sampler qualifier, so lowp would truncate the oracle-exact table. */
precision highp int;
precision highp float;
precision highp sampler2D;

/*
===========================================================================
ProvenanceEF2 -- OpenGL ES 3.0 shade layer (G2 r200-gles3-shade)
gles3_stage.vert -- the generic-stage vertex program.

This ONE source is compiled once per distinct GLP_* bitmask
(tr_program_table.inc), which is how "every combination the shipped corpus
can produce" is covered without a combinatorial source explosion: the coarse
axis (bundle count, tcGen family, tcMod presence, lightmap, atest, wave,
normal, entity-space) is a #define, and the per-stage generator arm is an
integer uniform carrying retail's OWN enum ordinal
(tr_local.h colorGen_t:483 / alphaGen_t:433 / texCoordGen_t:537), so
`u_ColorGen = pStage->rgbGen` with no translation.  renderergl2 does exactly
this (glsl/generic_vp.glsl:56-59 `uniform int u_ColorGen; u_AlphaGen;` and
tr_local.h:656-657 UNIFORM_COLORGEN / UNIFORM_ALPHAGEN).

Every arm cites the the deleted oracle line it mirrors.  the deleted oracle is the
behaviour oracle for this port; an approximation here is a divergence.
===========================================================================
*/
#define GLES3_VERTEX 1

in vec3 a_Position;
#ifdef GLP_NORMAL
in vec3 a_Normal;
#endif
in vec4 a_Color;			/* tess.vertexColors, normalized bytes */
in vec2 a_TexCoord0;		/* tess.texCoords[i][0] */
in vec2 a_TexCoord1;		/* tess.texCoords[i][1]; always present in the
							   vertex layout, read by TCGEN_LIGHTMAP even on a
							   one-bundle stage (tr_shade.c:1478). */

out vec4 v_Color;
out vec2 v_TexCoord0;
out float v_FogDepth;		/* gl_Position.w (= -ze) for GL_LINEAR fog */
out highp float v_ClipDist;	/* portal clip plane, eye space (gles3_clip.inc.glsl) */
#ifdef GLP_LIGHTMAP
out vec2 v_TexCoord1;
#endif

/*
===========================================================================
ProvenanceEF2 -- OpenGL ES 3.0 shade layer (G2 r200-gles3-shade)
gles3_common.inc.glsl -- shared GLSL ES 3.00 library, concatenated at
//@@GLES3_COMMON@@ by glsl/build_glsl.py.

Every function here mirrors a the deleted oracle (behaviour oracle) function.  The
citation next to each is the the deleted oracle source line range whose arithmetic it
reproduces.  This is a retail port: an approximation is a divergence, so the
expression order, the truncation points and the 0..255 byte quantisation are
part of the contract, not an implementation detail.

FUNCTABLE: the deleted oracle keeps the wave tables in tr.sinTable[] etc., filled by
tr_init.c:1844-1866 and indexed as
    table[ myftol( (phase + shaderTime*frequency) * FUNCTABLE_SIZE ) & FUNCTABLE_MASK ]
(tr_shade_calc.c:27, WAVEVALUE).  The tables are uploaded here as a 2048x5
GL_R32F texture (row = genFunc_t - 1) and read with texelFetch so the value is
bit-identical to the CPU table; a direct sin() would differ by ULPs and is
exactly the kind of approximation this port exists to remove.
===========================================================================
*/

#define GLES3_FUNCTABLE_SIZE 2048
#define GLES3_FUNCTABLE_MASK 2047

uniform sampler2D	u_WaveTable;	/* 2048 x 5 R32F, NEAREST, no mips */
uniform float		u_ShaderTime;	/* tess.shaderTime (tr_shade.c:1713) */

/*
GLES3_Wave -- tr_shade_calc.c:57 EvalWaveForm / :27 WAVEVALUE.
  func: genFunc_t (tr_local.h:369) GF_NONE 0, GF_SIN 1, GF_SQUARE 2,
        GF_TRIANGLE 3, GF_SAWTOOTH 4, GF_INVERSE_SAWTOOTH 5, GF_NOISE 6.
  GF_NONE/GF_NOISE: TableForFunc() raises ERR_DROP (tr_shade_calc.c:48), so no
  shipped content can reach them (labelled DEAD in tr_program_table.inc); row
  is clamped so a malformed shader cannot read out of range.
  The `& 2047` on a negative int matches C: GLSL ES 3.00 int is 32-bit two's
  complement and `&` is bitwise, so (-1)&2047 == 2047 as in WAVEVALUE.
*/
float GLES3_Wave(int func, float base, float amplitude, float phase, float frequency)
{
	int idx = int((phase + u_ShaderTime * frequency) * float(GLES3_FUNCTABLE_SIZE)) & GLES3_FUNCTABLE_MASK;
	int row = clamp(func - 1, 0, 4);
	return base + texelFetch(u_WaveTable, ivec2(idx, row), 0).r * amplitude;
}

/*
GLES3_WaveClamped -- tr_shade_calc.c:110 EvalWaveFormClamped: clamp to [0,1].
*/
float GLES3_WaveClamped(int func, float base, float amplitude, float phase, float frequency)
{
	float glow = GLES3_Wave(func, base, amplitude, phase, frequency);
	return clamp(glow, 0.0, 1.0);
}

/*
GLES3_Byte -- the C store `dstColors[i*4+j] = (float expression)` truncates
toward zero into an unsigned byte (myftol is `#define myftol(x) ((int)(x))`,
tr_local.h:66).  Colour generators in tr_shade_calc.c therefore quantise to
1/255 steps; the fixed-function pipeline then divides by 255.  Reproduce both
halves: floor() (int cast) then clamp to [0,255], then /255.
*/
vec4 GLES3_Byte(vec4 c)
{
	return floor(clamp(c, 0.0, 255.0)) / 255.0;
}

vec3 GLES3_Byte3(vec3 c)
{
	return floor(clamp(c, 0.0, 255.0)) / 255.0;
}

/*
GLES3_RgbFromScalar -- the shared body of RB_CalcRGBFromDot /
RB_CalcRGBFromOneMinusDot / RB_CalcRGBFromTexCoords
(tr_shade_calc.c:976 / :998 / :1133): a scalar in [alphaMin,alphaMax] mapped to
0..255 on all three channels, alpha untouched.
*/
vec3 GLES3_RgbFromScalar(float d, float alphaMin, float alphaMax)
{
	float v = (d - alphaMin) * (255.0 / (alphaMax - alphaMin));
	return GLES3_Byte3(vec3(v));
}

/*
GLES3_AlphaFromScalar -- RB_CalcAlphaFromDot / OneMinusDot / TexCoords / View
(tr_shade_calc.c:1032 / :1083 / :1114 / :1918 / :1946).
*/
float GLES3_AlphaFromScalar(float d, float alphaMin, float alphaMax)
{
	float v = (d - alphaMin) * (255.0 / (alphaMax - alphaMin));
	return floor(clamp(v, 0.0, 255.0)) / 255.0;
}

/*
GLES3_TexModApply -- one tcMod arm, tr_shade_calc.c the RB_Calc*TexCoords
family.  gl2's generic_vp.glsl:150 ModTexCoords is the architectural precedent;
the arms here are the deleted oracle's, one for one, and each is cited at its case in
gles3_stage.vert.  u_TexMod is a flattened {type, p0, p1, p2, p3} record per
tcMod (see GLES3_TCMOD in tr_program.c).
*/
#define GLES3_TMOD_NONE       0
#define GLES3_TMOD_TRANSFORM  1
#define GLES3_TMOD_TURBULENT  2
#define GLES3_TMOD_SCROLL     3
#define GLES3_TMOD_SCALE      4
#define GLES3_TMOD_STRETCH    5
#define GLES3_TMOD_ROTATE     6
#define GLES3_TMOD_ENTITYTR   7
#define GLES3_TMOD_NOISE      8
#define GLES3_TMOD_OFFSET     9
#define GLES3_TMOD_PARALLAX  10
#define GLES3_TMOD_MACRO     11
#define GLES3_TMOD_WAVETRANS 12
#define GLES3_TMOD_WAVETRANT 13
#define GLES3_TMOD_BULGETRANS 14

/*
===========================================================================
RGB GENERATORS  (the deleted oracle tr_shade.c ComputeColors, lines 806-1081;
the per-arm arithmetic lives in tr_shade_calc.c and is cited in the vertex
shader where the arm is emitted)
===========================================================================
*/

/*
===========================================================================
UNIFORM BLOCK
---------------------------------------------------------------------------
One block for every program; unused uniforms are optimised out by the
compiler and GLES3_SetUniform* tolerates a -1 location (tr_program.c, mirrors
renderergl2 tr_glsl.c:694-740 `if (uniforms[uniformNum] == -1) return;`).
The integer generator selects are retail's own enum ordinals -- no translation.
===========================================================================
*/
uniform mat4	u_MVP;				/* GL1 gl_ModelViewProjectionMatrix */
uniform mat4	u_ModelMatrix;		/* backEnd.ori.modelMatrix (tr_shade_calc.c:2291) */
uniform vec3	u_ViewOrigin;		/* backEnd.ori.viewOrigin */
uniform vec3	u_ViewAxis0;		/* tr.refdef.viewaxis[0] (AGEN_DOT_VIEW) */
uniform vec3	u_EntityOrigin;		/* backEnd.currentEntity->e.origin */
uniform mat3	u_EntityAxis;		/* backEnd.currentEntity->e.axis */
uniform vec4	u_EntityColor;		/* e.shaderRGBA / 255 */
uniform vec4	u_BaseColor;		/* colorConst / 255, or backEnd.color2D / 255 */
uniform vec4	u_VertColor;		/* gl2's per-vertex tint (identity here) */
uniform float	u_IdentityLight;	/* tr.identityLight */
uniform float	u_IdentityLightByte;/* tr.identityLightByte (int, 0..255) */
uniform float	u_EntityFade;		/* tess.entityFade */
uniform float	u_PortalRange;		/* tess.shader->portalRange */
uniform vec3	u_SpecOrigin;		/* pStage->specOrigin */
uniform vec3	u_SunDirection;		/* tr.sunDirection */
uniform float	u_SkyAlpha;			/* tr.refdef.sky_alpha */
uniform vec4	u_EntityTexCoord;	/* e.shaderTexCoord (TMOD_ENTITY_TRANSLATE) */

uniform int		u_ColorGen;			/* colorGen_t  (tr_local.h:483) */
uniform int		u_AlphaGen;			/* alphaGen_t  (tr_local.h:433) */
uniform int		u_TCGen0;			/* texCoordGen_t per bundle */
uniform int		u_TCGen1;
uniform vec3	u_TCGen0Vec0, u_TCGen0Vec1;
uniform vec3	u_TCGen1Vec0, u_TCGen1Vec1;

uniform float	u_AlphaMin;			/* pStage->alphaMin   */
uniform float	u_AlphaMax;			/* pStage->alphaMax   */
uniform float	u_AlphaConst;		/* pStage->alphaConst / 255 */
uniform float	u_AlphaConstMin;	/* pStage->alphaConstMin / 255 */
uniform vec4	u_WaveRGB;			/* (func, base, amplitude, phase) */
uniform vec4	u_WaveAlpha;
uniform float	u_FreqRGB;			/* rgbWave.frequency   */
uniform float	u_FreqAlpha;		/* alphaWave.frequency */

uniform int		u_NumTexMod0;		/* bundle 0 tcMod count (<= TR_MAX_TEXMODS) */
uniform int		u_NumTexMod1;
uniform vec4	u_TexMod0[16];		/* 4 vec4 per tcMod, TR_MAX_TEXMODS = 4 */
uniform vec4	u_TexMod1[16];

uniform int		u_AlphaTest;		/* 0, GLS_ATEST_GT_0>>28 etc. */
uniform int		u_TexEnv0;			/* 1 modulate, 2 add, 3 replace, 4 interpolate */
uniform int		u_TexEnv1;
uniform sampler2D	u_Tex0;
uniform sampler2D	u_Tex1;
uniform vec4	u_Tex1Color;		/* gl2's u_Tex1Color equivalent (identity) */

/* ---- fog (tr_shade_calc.c:2205-2562) --------------------------------- */
uniform int		u_FogNum;			/* tess.fogNum: 0 none, 1..N volume, 9999 global */
uniform float	u_FogTcScale;		/* fog->tcScale                        (RB_CalcFogTexCoords) */
uniform float	u_FogSConstant;
uniform vec3	u_FogSVector;
uniform float	u_FogTConstant;
uniform vec3	u_FogTVector;
uniform int		u_FogViewOutside;	/* tConstant < 0 */
uniform vec4	u_FogColor;			/* fog_t::colorInt unpacked / 255 (CGEN_FOG) */
uniform int		u_FogDensityArm;	/* 0 global, 1 volume-inside, 2 volume-outside */
uniform float	u_FogOolen;			/* tr.viewParams.fog.oolen */
uniform float	u_FogPlaneConst;	/* RB_CalcAlphaFogDensities: planeConst */
uniform float	u_FogDView;
uniform vec3	u_FogDensityTVector;
uniform int		u_Acff;				/* acff_t (tr_local.h:553) */

/* ---- fixed-function fog (the RB_GLFogStageIteratorFunc arm) ----------- */
uniform int		u_FogMode;			/* 0 none, 1 acff vertex modulate, 2 GL_LINEAR fixed-function */
uniform float	u_FogStart;			/* backEnd.viewParms.farplane_bias   (tr_backend.c:1381) */
uniform float	u_FogEnd;			/* backEnd.viewParms.farplane_distance (tr_backend.c:1382) */
uniform vec4	u_FogFixColor;		/* farplane_color * tr.identityLight (tr_backend.c:1386-1389) */

/*
GLES3_FogST -- RB_CalcFogTexCoords tr_shade_calc.c:2228-2260, the exact
association order `(x*v0 + y*v1) + z*v2` and the two-end ramp.
*/
vec2 GLES3_FogST(vec3 pos)
{
	float s = (u_FogSConstant + 0.001953125)
		+ ((pos.x * u_FogSVector.x + pos.y * u_FogSVector.y) + pos.z * u_FogSVector.z);
	float tRaw = u_FogTConstant
		+ ((pos.x * u_FogTVector.x + pos.y * u_FogTVector.y) + pos.z * u_FogTVector.z);
	float t;
	if (u_FogViewOutside != 0) {
		if (tRaw >= 1.0) t = 0.03125 + (0.9375 * tRaw) / (tRaw - u_FogTConstant);
		else             t = 0.03125;
	} else if (tRaw >= 0.0) {
		t = 0.96875;
	} else {
		t = 0.03125;
	}
	return vec2(s, t);
}

/*
GLES3_FogRampFactor -- the non-_Alpha fog modulator, tr_shade_calc.c:2577/2605/
2630:  factor = max(1 - 256*s*(t-0.25), 0).
*/
float GLES3_FogRampFactor(vec3 pos)
{
	vec2 st = GLES3_FogST(pos);
	float factor = -(256.0 * (st.x * (st.y - 0.25)) - 1.0);
	return max(factor, 0.0);
}

/*
GLES3_FogDensity -- RB_CalcAlphaFogDensities tr_shade_calc.c:2383-2460.
u_FogDensityArm: 1 = global (fogNum 9999), 2 = volume camera-inside, 3 =
volume camera-outside.  Arm 0 is reserved for "the ramp arm, not the density
arm" and is handled by GLES3_ApplyAcff -- it never reaches here.
The arm selector is the SAME branch the oracle's caller takes: arm 1 from
`tess.fogNum == 9999' (tr_shade_calc.c:2470) and arms 2/3 from
`( dView >= 0.0f ) ? 1 : 2' (tr_shade_calc.c:2524).
The _Alpha modulator arm (stage->fogDensityArm) is selected by
u_FogDensityArm != 0 (tr_shade.c ComputeColors:1429-1448).
*/
float GLES3_FogDensity(vec3 pos)
{
	if (u_FogDensityArm == 1) {
		vec3 delta = pos - u_ViewOrigin;
		float dist2 = delta.z * delta.z + (delta.x * delta.x + delta.y * delta.y);
		return min(u_FogOolen * sqrt(dist2), 1.0);
	}
	float dVertex = ((pos.x * u_FogDensityTVector.x + pos.y * u_FogDensityTVector.y)
					 + pos.z * u_FogDensityTVector.z) - u_FogPlaneConst;
	if (dVertex < 0.0) return 0.0;
	vec3 delta = u_ViewOrigin - pos;
	float dist2 = delta.z * delta.z + (delta.x * delta.x + delta.y * delta.y);
	float dist = sqrt(dist2);
	if (u_FogDensityArm == 2) {
		return min((0.5 * dist) * (u_FogDView + dVertex), 1.0);
	}
	return min(((dist * dVertex) * (0.5 * (dVertex - u_FogDView) + 0.0)) / (dVertex - u_FogDView), 1.0);
}

/*
GLES3_AcffByte -- the acff modulators' own store, which is NOT GLES3_Byte's.

The oracle's RB_CalcModulateColorsByFog (tr_shade_calc.c:2592) and its _Alpha /
RGBA / Alphas siblings end in

    dstColors[i * 4 + j] = (int)( (float)(int)dstColors[i*4+j] * factor );

with `dstColors` an `unsigned char *`: the `(int)` truncates toward zero and the
byte store then wraps modulo 256.  GLES3_Byte() clamps instead, so wherever the
ramp factor exceeds 1.0 the two disagree -- the clamp saturates to 255 (opaque
white) exactly where the oracle wraps to a small byte.  The ramp factor is
routinely > 1: RB_CalcFogTexCoords (tr_shade_calc.c:2238) leaves S unclamped, so
a vertex behind the fog plane's S origin gives factor = 1 + 56*|S| / 1 + 184*|S|
(from the two T ends, tr_shade_calc.c:2250-2253), and ComputeColors @0x353F0
applies these modulators whenever the stage's acff is nonzero with NO fogNum test
(the acff tail of ComputeColors.c:335-375 reads only stage+208).

GLSL's `mod' is `x - y*floor(x/y)' -- the same unsigned wrap for a negative
value as C's `(int)' + byte store -- and `trunc' is that `(int)' cast.  Returns
byte space (0..255) so the caller's GLES3_Byte() still finishes the job.
*/
vec4 GLES3_AcffByte(vec4 c)
{
	return mod(trunc(c), 256.0);
}

/*
GLES3_ApplyAcff -- tr_shade.c ComputeColors:1424-1449.  c is in 0..255 byte
space.  acff 1 RGB, 2 RGBA, 3 ALPHA.  u_FogDensityArm == 0 selects the ramp
modulators (RB_CalcModulate*ByFog, tr_shade_calc.c:2577); 1/2/3 select the
density modulators (RB_CalcModulate*ByFog_Alpha, tr_shade_calc.c:2696), whose
value comes from GLES3_FogDensity with the arm's own formula.

The arm selector is pStage->acff ALONE.  The oracle's ComputeColors tail is
`v45 = *(stage + 208); if (v45) { ... }' (ComputeColors.c:335-375) with no
tess.fogNum read anywhere -- the ramp arm's RB_CalcFogTexCoords simply indexes
`tr.world->fogs[tess.fogNum]' (tr_shade_calc.c:2272), and index 0 is R_LoadFogs'
reserved ZEROED record, so a stage on a map with no fog volume still gets the
deterministic factor GLES3_SetupFogUniforms publishes for fogNum == 0.  This arm
used to short-circuit on `u_FogNum == 0', which every shader-computed stage on
such a map took: the GLES3 blob shadow was then 1.57x the retail strength
(vertex colour 255*0.647 instead of 255*0.647*0.640625).
*/
vec4 GLES3_ApplyAcff(vec4 c, vec3 pos)
{
	if (u_Acff == 0) return c;
	float f = (u_FogDensityArm != 0) ? (1.0 - GLES3_FogDensity(pos)) : GLES3_FogRampFactor(pos);
	if (u_Acff == 1) return vec4(GLES3_AcffByte(vec4(c.rgb, 0.0)).rgb, c.a);
	if (u_Acff == 2) return GLES3_AcffByte(c);
	if (u_Acff == 3) return vec4(c.rgb, GLES3_AcffByte(vec4(c.a, 0.0, 0.0, 0.0)).x);
	return c;
}

/*
GLES3_FixedFog -- the RB_GLFogStageIteratorFunc pass: GL_LINEAR fog
(tr_init.c:1007 qglFogi(GL_FOG_MODE, GL_LINEAR)) with START=farplane_bias and
END=farplane_distance (tr_backend.c:1381-1382) and the far-plane colour
(tr_backend.c:1386-1389).  GL_LINEAR factor = (end - |ze|) / (end - start);
with a standard perspective matrix gl_Position.w == -ze, so the caller passes
depth = gl_Position.w.
*/
vec3 GLES3_FixedFog(vec3 rgb, float depth)
{
	float f = clamp((u_FogEnd - depth) / (u_FogEnd - u_FogStart), 0.0, 1.0);
	return mix(u_FogFixColor.rgb, rgb, f);
}


/*
===========================================================================
ProvenanceEF2 -- OpenGL ES 3.0 shade layer (G2 r200-gles3-shade)
gles3_clip.inc.glsl -- the PORTAL CLIP PLANE, shared by every program that
can be bound while the plane is enabled.  Substituted at the `//@@GLES3_CLIP@@`
marker by glsl/build_glsl.py, so the uniform names cannot drift from
GLES3_U_CLIPPLANE / GLES3_U_CLIPPLANEENABLE in tr_program.h.

WHAT IT REPLACES -- the deleted oracle tr_backend.c RB_BeginDrawingView
----------------------------------------------------------------
GL1 clips a portal view with the fixed-function plane:

    plane[0..3] = backEnd.viewParms.portalPlane.{normal,dist}
    plane2[0]   = DotProduct(viewParms.ori.axis[0], plane)     tr_backend.c:718
    plane2[1]   = DotProduct(viewParms.ori.axis[1], plane)     :719
    plane2[2]   = DotProduct(viewParms.ori.axis[2], plane)     :720
    plane2[3]   = DotProduct(plane, viewParms.ori.origin)
                  - plane[3]                                   :721
    qglLoadMatrixf ( s_flipMatrix );                           :723
    qglClipPlane ( GL_CLIP_PLANE0, plane2 );                   :724
    qglEnable ( GL_CLIP_PLANE0 );                              :725

and clears it again in RB_ShadowFinish (tr_shadows.c:322) and
RB_Set2DWindow (tr_draw.c:653).  G1 publishes exactly those four floats
through tr_vbo.c's RB_ClipPlane_Set; G3's RB_ClipPlane_Get returns them and
G2 uploads them here.

CLIP DISTANCE
-------------
GL_CLIP_PLANE0 is an EYE-SPACE half-space: a vertex survives iff
dot(plane2.xyz, eyePosition) + plane2.w >= 0, with eyePosition the vertex after
the model-view transform.  In this fork that transform is u_ModelMatrix
(= RB_GetModelViewMatrix(), the same backEnd.ori.modelMatrix GL1 loads before
each batch), so the exact quantity is

    dot(u_ClipPlane, u_ModelMatrix * vec4(a_Position, 1.0))

and the vertex shader exports it as v_ClipDist.

SANCTIONED DELTA -- per-fragment discard instead of per-primitive clipping
--------------------------------------------------------------------------
Core OpenGL ES 3.0 has NO gl_ClipDistance and no user clip planes: clip
distances are ES 3.2 core / EXT_clip_cull_distance (`EXT_clip_cull_distance`
is not guaranteed on any device this port targets -- see MALI_FIXES.md's rule
against hard-requiring extensions).  So the plane cannot be applied at
primitive assembly; it is applied in the fragment stage:

    if (u_ClipPlaneEnable != 0 && v_ClipDist < 0.0) discard;

Adjudication under STANDING_RULES section 1 (retail accuracy) and section 2
(sanctioned deltas) -- this is the *renderer-substrate* class: the fixed
function it replaces does not exist in the target API.

  * Retail-visible result reproduced: the plane's half-space is the same
    half-space, and the test is evaluated at the same fragments with the same
    sign.  v_ClipDist is a linear function of eye position and is interpolated
    perspective-correctly, so the fragment-stage value equals the clip-plane
    distance AT THAT FRAGMENT exactly (no approximation in the sign test).
    Geometry on the removed side is not drawn -- a fully-removed triangle
    contributes no fragments, and a straddling triangle keeps exactly its
    positive side.
  * The only difference is granularity: fixed-function clipping rewrites the
    primitive at the plane before rasterisation, so its cut edge is exact,
    while a discard cuts on the fragment grid.  The delta is bounded by one
    fragment along the portal seam (sub-pixel; the renderer has no MSAA and
    no line/polygon smoothing, so nothing else observes it).
  * No shipped content is changed outside portal views: RB_ClipPlane_Get
    returns qfalse for every non-portal view and for 2D (RB_Set2DWindow
    disables it), and the enable flag is uploaded with the rest of the view
    state, so a disabled plane never discards (u_ClipPlaneEnable == 0 makes
    the condition short-circuit).

MEASURED CONSUMERS: the clip contract asserts that
RB_ClipPlane_Get is called from GLES3_SetupViewUniforms, that every program
family that can be bound while the plane is enabled carries this chunk and the
discard, and that the 2D program (which is only ever bound after
RB_Set2DWindow's disable) does not.
===========================================================================
*/

uniform vec4	u_ClipPlane;		/* eye space: (A,B,C,D), RB_ClipPlane_Get */
uniform int		u_ClipPlaneEnable;	/* 0 = no active portal plane */


/* Extra stage-only uniforms (not in the shared library).
   u_LocalViewOrigin : tr.refdef.vieworg, the value
     RB_CalcParallaxTexCoords (tr_shade_calc.c:1651) reads -- distinct from
     u_ViewOrigin, which is backEnd.ori.viewOrigin (the env-map family).
   u_ForceAlpha : EF2 RF_FORCE_ALPHA.  When >= 0 it overrides the generated
     alpha with tess.entityFade-independent entity shaderRGBA[3], exactly the
     C loop `svars.colors[v][3] = e.shaderRGBA[3]` in
     RB_IterateStagesGeneric (tr_shade.c:1638) / RB_StageIteratorVertexLit-
     TextureUnfogged (tr_shade.c:1899).  -1 = inactive. */
uniform vec3	u_LocalViewOrigin;
uniform float	u_ForceAlpha;

/* ===================================================================
 * TexCoord generators -- the deleted oracle tr_shade.c ComputeTexCoords:1457
 * =================================================================== */
#ifdef GLP_TCGEN0
vec2 GLES3_TexGen0(vec3 pos, vec3 nrm)
{
	if (u_TCGen0 == 2)	return a_TexCoord1;			/* TCGEN_LIGHTMAP  :1478 */
	if (u_TCGen0 == 3)	return a_TexCoord0;			/* TCGEN_TEXTURE   :1472 */
	if (u_TCGen0 == 1)	return vec2(0.0);			/* TCGEN_IDENTITY  :1469 */
	if (u_TCGen0 == 5)								/* TCGEN_VECTOR    :1487 */
		return vec2(dot(pos, u_TCGen0Vec0), dot(pos, u_TCGen0Vec1));
	if (u_TCGen0 == 4) {							/* ENVIRONMENT_MAPPED :1484 */
		/* RB_CalcEnvironmentTexCoords tr_shade_calc.c:1299-1321 */
		vec3 viewer = normalize(u_ViewOrigin - pos);
		float d = dot(nrm, viewer);
		vec3 refl = nrm * (2.0 * d) - viewer;
		return vec2(0.5 + refl.y * 0.5, 0.5 - refl.z * 0.5);
	}
	if (u_TCGen0 == 7) {							/* ANGLE_BASED_ENVIRONMENT :1496 */
		/* tr_shade_calc.c:1370-1389; the viewer is axis[0] normalised ONCE */
		vec3 viewer = normalize(u_EntityAxis[0]);
		float d = dot(nrm, viewer);
		vec3 refl = nrm * (2.0 * d) - viewer;
		return vec2(0.5 + refl.y * 0.5, 0.5 - refl.z * 0.5);
	}
	if (u_TCGen0 == 6) {							/* ENVIRONMENT_MAPPED2 :1493 */
		/* tr_shade_calc.c:1418-1451.  The worldReflected terms are computed by
		   retail and never used (st is built from `reflected'); reproduced
		   verbatim so the arithmetic order matches. */
		vec3 viewer = normalize(u_ViewOrigin - pos);
		float d = dot(nrm, viewer);
		vec3 refl;
		if (d > 0.0) refl = viewer;
		else         refl = nrm * (d * -2.0) + viewer;
		vec3 worldReflected;
		worldReflected.x = refl.x * u_EntityAxis[0][0];
		worldReflected.z = refl.x * u_EntityAxis[0][2];
		worldReflected.x = u_EntityAxis[1][0] * refl.y + worldReflected.x;
		worldReflected.z = u_EntityAxis[1][2] * refl.y + worldReflected.z;
		worldReflected.x = u_EntityAxis[2][0] * refl.z + worldReflected.x;
		worldReflected.z = u_EntityAxis[2][2] * refl.z + worldReflected.z;
		return vec2(0.5 + refl.y * 0.5, 0.5 - refl.z * 0.5);
	}
	if (u_TCGen0 == 8) {							/* SUN_REFLECTION :1501 */
		/* tr_shade_calc.c:1485-1537 */
		vec3 viewer = normalize(pos - u_ViewOrigin);
		float d = dot(viewer, nrm);
		vec3 refl;
		if (d > 0.0) refl = viewer;
		else         refl = viewer + nrm * (d * -2.0);
		vec3 sun1 = normalize(vec3(u_SunDirection.y, -u_SunDirection.x, 0.0));
		vec3 sun2 = cross(u_SunDirection, sun1);
		vec3 sr = u_SunDirection * refl.x;
		sr += sun1 * refl.z;
		sr += sun2 * (-refl.y);
		return vec2(sr.y + 0.5, sr.z + 0.5);
	}
	if (u_TCGen0 == 9) {							/* TCGEN_FOG :1504 */
		/* RB_CalcFogTexCoords tr_shade_calc.c:2228-2260 */
		float s = (u_FogSConstant + 0.001953125)
			+ ((pos.x * u_FogSVector.x + pos.y * u_FogSVector.y) + pos.z * u_FogSVector.z);
		float tRaw = u_FogTConstant
			+ ((pos.x * u_FogTVector.x + pos.y * u_FogTVector.y) + pos.z * u_FogTVector.z);
		float t;
		if (u_FogViewOutside != 0) {
			if (tRaw >= 1.0) t = 0.03125 + (0.9375 * tRaw) / (tRaw - u_FogTConstant);
			else             t = 0.03125;
		} else if (tRaw >= 0.0) {
			t = 0.96875;
		} else {
			t = 0.03125;
		}
		return vec2(s, t);
	}
	return a_TexCoord0;
}
#endif

#ifdef GLP_TCGEN1
vec2 GLES3_TexGen1(vec3 pos, vec3 nrm)
{
	if (u_TCGen1 == 2)	return a_TexCoord1;
	if (u_TCGen1 == 3)	return a_TexCoord0;
	if (u_TCGen1 == 1)	return vec2(0.0);
	if (u_TCGen1 == 5)
		return vec2(dot(pos, u_TCGen1Vec0), dot(pos, u_TCGen1Vec1));
	if (u_TCGen1 == 4) {
		vec3 viewer = normalize(u_ViewOrigin - pos);
		float d = dot(nrm, viewer);
		vec3 refl = nrm * (2.0 * d) - viewer;
		return vec2(0.5 + refl.y * 0.5, 0.5 - refl.z * 0.5);
	}
	if (u_TCGen1 == 9) {
		float s = (u_FogSConstant + 0.001953125)
			+ ((pos.x * u_FogSVector.x + pos.y * u_FogSVector.y) + pos.z * u_FogSVector.z);
		return vec2(s, 0.03125);
	}
	return a_TexCoord0;
}
#endif

/* ===================================================================
 * tcMod chain -- the deleted oracle tr_shade.c ComputeTexCoords:1523-1595 and
 * tr_shade_calc.c RB_Calc*TexCoords.  Up to TR_MAX_TEXMODS (4) per bundle;
 * TMOD_NONE breaks out of the loop (:1526).  u_TexMod[4i+0..3] packs the arm.
 * =================================================================== */
#ifdef GLP_TCMOD0
/*
PACKING (written by GLES3_SetupStageUniforms, tr_program.c):
    p0 = (type, a, b, c)     p1 = (d, e, f, g)
    TRANSFORM      a=m[0][0] b=m[1][0] c=translate[0] | d=m[0][1] e=m[1][1] f=translate[1]
    TURBULENT      c=amplitude                        | d=phase     e=frequency
    SCROLL         a=scroll[0] b=scroll[1]            |
    SCALE          a=scale[0]  b=scale[1]             |
    STRETCH        a=func b=base c=amplitude          | d=phase     e=frequency
    ROTATE         a=degsPerSecond (sentinel resolved) b=coef c=start
    OFFSET         a=offsetS (sentinel resolved) b=offsetT
    PARALLAX       a=rate[0]   b=rate[1]              |
    WAVETRANS/T    a=func b=base c=amplitude          | d=phase     e=frequency
MACRO is absent by design: RB_CalcMacroTexCoords (tr_shade_calc.c:1675) needs
RB_TextureAxisFromPlane / RB_QuakeTextureVecs and the bound image's
uploadWidth/Height, so GLES3_StageNeedsCPUTexCoords() routes it through the
oracle and passes tess.svars.texcoords[] straight through instead.
*/
vec2 GLES3_ApplyTexMod0(vec2 st, vec3 pos)
{
	for (int tm = 0; tm < u_NumTexMod0; tm++) {
		int   ty = int(u_TexMod0[tm * 4 + 0].x);
		vec4  p0 = u_TexMod0[tm * 4 + 0];
		vec4  p1 = u_TexMod0[tm * 4 + 1];
		if (ty == GLES3_TMOD_NONE) break;					/* :1526 */
		else if (ty == GLES3_TMOD_SCROLL || ty == GLES3_TMOD_ENTITYTR) {
			/* :1540 / :1535 -> RB_CalcScrollTexCoords (tr_shade_calc.c:1721):
			   adjusted = speed * shaderTime; adjusted -= floor(adjusted). */
			vec2 sp = (ty == GLES3_TMOD_ENTITYTR) ? u_EntityTexCoord.xy : p0.yz;
			vec2 adj = sp * u_ShaderTime;
			adj -= floor(adj);
			st += adj;
		}
		else if (ty == GLES3_TMOD_SCALE) {					/* :1545 tr_shade_calc.c:1745 */
			st.x *= p0.y; st.y *= p0.z;
		}
		else if (ty == GLES3_TMOD_OFFSET) {					/* :1567 tr_shade_calc.c:1607 */
			st.x += p0.y; st.y += p0.z;
		}
		else if (ty == GLES3_TMOD_STRETCH) {				/* :1550 tr_shade_calc.c:130 */
			float now = GLES3_Wave(int(p0.y), p0.z, p0.w, p1.x, p1.y);
			float scale = 1.0 / now;
			st.x = st.x * scale - 0.5 * (scale - 1.0);
			st.y = st.y * scale - 0.5 * (scale - 1.0);
		}
		else if (ty == GLES3_TMOD_TURBULENT) {				/* :1530 tr_shade_calc.c:1585 */
			/* RB_CalcTurbulentTexCoordsRange: now = phase + shaderTime*frequency,
			   index = (int)(((x+z) * 1.0/128 * 0.125 + now) * 2048) & 2047,
			   value = tr.sinTable[index] * amplitude.  The `1.0/128 * 0.125`
			   association is C's left-to-right evaluation, reproduced here. */
			float now = p1.x + u_ShaderTime * p1.y;
			float off0 = (pos.x + pos.z) * (1.0 / 128.0) * 0.125 + now;
			float off1 = pos.y * (1.0 / 128.0) * 0.125 + now;
			st.x += texelFetch(u_WaveTable, ivec2(int(off0 * 2048.0) & 2047, 0), 0).r * p0.w;
			st.y += texelFetch(u_WaveTable, ivec2(int(off1 * 2048.0) & 2047, 0), 0).r * p0.w;
		}
		else if (ty == GLES3_TMOD_ROTATE) {					/* :1560 tr_shade_calc.c:1779 */
			/* RB_CalcRotateTexCoords: degs = -degsPerSecond*coef*shaderTime - start;
			   index = degs * (2048/360); sin = sinTable[index&2047];
			   cos = sinTable[(index + 512)&2047]; then RB_CalcTransformTexCoords
			   with the centring matrix (translate = 0.5 -/+ 0.5*cos/sin). */
			float degs = -p0.y * p0.z * u_ShaderTime - p0.w;
			int   index = int(degs * (2048.0 / 360.0));
			float sinValue = texelFetch(u_WaveTable, ivec2(index & 2047, 0), 0).r;
			float cosValue = texelFetch(u_WaveTable, ivec2((index + 512) & 2047, 0), 0).r;
			float tr0 = 0.5 - 0.5 * cosValue + 0.5 * sinValue;
			float tr1 = 0.5 - 0.5 * sinValue - 0.5 * cosValue;
			vec2 t;
			t.x = st.x * cosValue + st.y * (-sinValue) + tr0;
			t.y = st.x * sinValue + st.y * cosValue + tr1;
			st = t;
		}
		else if (ty == GLES3_TMOD_TRANSFORM) {				/* :1555 tr_shade_calc.c:1758 */
			vec2 t;
			t.x = st.x * p0.y + st.y * p0.z + p0.w;
			t.y = st.x * p1.x + st.y * p1.y + p1.z;
			st = t;
		}
		else if (ty == GLES3_TMOD_PARALLAX) {				/* :1571 tr_shade_calc.c:1651 */
			/* RB_CalcParallaxTexCoords reads tr.refdef.vieworg, NOT
			   backEnd.ori.viewOrigin -- a different value in an entity's own
			   frame, hence the separate u_LocalViewOrigin uniform. */
			st.x += u_LocalViewOrigin.x * p0.y;
			st.y += u_LocalViewOrigin.y * p0.z;
		}
		else if (ty == GLES3_TMOD_WAVETRANS) {				/* :1579 tr_shade_calc.c:154 */
			float now = GLES3_Wave(int(p0.y), p0.z, p0.w, p1.x, p1.y);
			st.x += now;
		}
		else if (ty == GLES3_TMOD_WAVETRANT) {				/* :1583 tr_shade_calc.c:171 */
			float now = GLES3_Wave(int(p0.y), p0.z, p0.w, p1.x, p1.y);
			st.y += now;
		}
		else if (ty == GLES3_TMOD_BULGETRANS) {				/* :1587 tr_shade_calc.c:188 */
			/* RB_CalcBulgeTexCoords computes `offset' per vertex and DISCARDS it
			   (`(void)offset; // BUG: ... this function is a no-op').  The
			   oracle is the contract, so this arm is deliberately a no-op. */
		}
	}
	return st;
}
#endif
#ifdef GLP_TCMOD1
vec2 GLES3_ApplyTexMod1(vec2 st, vec3 pos)
{
	for (int tm = 0; tm < u_NumTexMod1; tm++) {
		int   ty = int(u_TexMod1[tm * 4 + 0].x);
		vec4  p0 = u_TexMod1[tm * 4 + 0];
		vec4  p1 = u_TexMod1[tm * 4 + 1];
		if (ty == GLES3_TMOD_NONE) break;
		else if (ty == GLES3_TMOD_SCROLL || ty == GLES3_TMOD_ENTITYTR) {
			vec2 sp = (ty == GLES3_TMOD_ENTITYTR) ? u_EntityTexCoord.xy : p0.yz;
			vec2 adj = sp * u_ShaderTime;
			adj -= floor(adj);
			st += adj;
		}
		else if (ty == GLES3_TMOD_SCALE)  { st.x *= p0.y; st.y *= p0.z; }
		else if (ty == GLES3_TMOD_OFFSET) { st.x += p0.y; st.y += p0.z; }
		else if (ty == GLES3_TMOD_STRETCH) {
			float now = GLES3_Wave(int(p0.y), p0.z, p0.w, p1.x, p1.y);
			float scale = 1.0 / now;
			st.x = st.x * scale - 0.5 * (scale - 1.0);
			st.y = st.y * scale - 0.5 * (scale - 1.0);
		}
		else if (ty == GLES3_TMOD_TURBULENT) {
			float now = p1.x + u_ShaderTime * p1.y;
			float off0 = (pos.x + pos.z) * (1.0 / 128.0) * 0.125 + now;
			float off1 = pos.y * (1.0 / 128.0) * 0.125 + now;
			st.x += texelFetch(u_WaveTable, ivec2(int(off0 * 2048.0) & 2047, 0), 0).r * p0.w;
			st.y += texelFetch(u_WaveTable, ivec2(int(off1 * 2048.0) & 2047, 0), 0).r * p0.w;
		}
		else if (ty == GLES3_TMOD_ROTATE) {
			float degs = -p0.y * p0.z * u_ShaderTime - p0.w;
			int   index = int(degs * (2048.0 / 360.0));
			float sinValue = texelFetch(u_WaveTable, ivec2(index & 2047, 0), 0).r;
			float cosValue = texelFetch(u_WaveTable, ivec2((index + 512) & 2047, 0), 0).r;
			float tr0 = 0.5 - 0.5 * cosValue + 0.5 * sinValue;
			float tr1 = 0.5 - 0.5 * sinValue - 0.5 * cosValue;
			vec2 t;
			t.x = st.x * cosValue + st.y * (-sinValue) + tr0;
			t.y = st.x * sinValue + st.y * cosValue + tr1;
			st = t;
		}
		else if (ty == GLES3_TMOD_TRANSFORM) {
			vec2 t;
			t.x = st.x * p0.y + st.y * p0.z + p0.w;
			t.y = st.x * p1.x + st.y * p1.y + p1.z;
			st = t;
		}
		else if (ty == GLES3_TMOD_PARALLAX) {
			st.x += u_LocalViewOrigin.x * p0.y;
			st.y += u_LocalViewOrigin.y * p0.z;
		}
		else if (ty == GLES3_TMOD_WAVETRANS) { st.x += GLES3_Wave(int(p0.y), p0.z, p0.w, p1.x, p1.y); }
		else if (ty == GLES3_TMOD_WAVETRANT) { st.y += GLES3_Wave(int(p0.y), p0.z, p0.w, p1.x, p1.y); }
		else if (ty == GLES3_TMOD_BULGETRANS) { /* oracle no-op, see ApplyTexMod0 */ }
	}
	return st;
}
#endif

/* ===================================================================
 * Colour generator -- the deleted oracle tr_shade.c ComputeColors:806-1081
 * r,g,b are in 0..255 byte space, exactly like tess.svars.colors, so every
 * truncation the C code performs at the byte store is reproduced by
 * GLES3_Byte() at the end.
 * =================================================================== */
void main()
{
	vec3 pos = a_Position;
	vec4 vcol = a_Color;
	vec3 nrm = vec3(0.0, 0.0, 1.0);
#ifdef GLP_NORMAL
	nrm = a_Normal;
#endif

	vec2 st0 = a_TexCoord0;
	vec2 st1 = vec2(0.0);
#ifdef GLP_LIGHTMAP
	st1 = a_TexCoord1;
#endif

	vec4 color;

#ifdef GLP_RGBAGEN
	/* --- rgbGen, tr_shade.c:813-1081 ------------------------------------ */
	if (u_ColorGen == 1) {						/* CGEN_IDENTITY_LIGHTING :819 */
		float v = u_IdentityLightByte;
		color = vec4(v, v, v, 255.0);
	}
	else if (u_ColorGen == 2) {					/* CGEN_IDENTITY :815 */
		color = vec4(255.0, 255.0, 255.0, 255.0);
	}
	else if (u_ColorGen == 3) {					/* CGEN_ENTITY :822 tr_shade_calc.c:914 */
		color = floor(u_EntityColor * 255.0);
	}
	else if (u_ColorGen == 4) {					/* CGEN_ONE_MINUS_ENTITY :825 :934 */
		color = floor((vec4(1.0) - u_EntityColor) * 255.0);
	}
	else if (u_ColorGen == 5) {					/* CGEN_EXACT_VERTEX :828 :837 */
		color = floor(vcol * 255.0);
	}
	else if (u_ColorGen == 6) {					/* CGEN_VERTEX :839 tr_shade.c:862 */
		color = vec4(floor(vcol.rgb * u_IdentityLight * 255.0), floor(vcol.a * 255.0));
	}
	else if (u_ColorGen == 7) {					/* CGEN_ONE_MINUS_VERTEX :877 */
		color = vec4(floor((1.0 - vcol.rgb) * u_IdentityLight * 255.0), 255.0);
	}
	else if (u_ColorGen == 8) {					/* CGEN_WAVEFORM :897 tr_shade_calc.c:1188 */
		float glow = GLES3_Wave(int(u_WaveRGB.x), u_WaveRGB.y, u_WaveRGB.z, u_WaveRGB.w, u_FreqRGB) * u_IdentityLight;
		glow = clamp(glow, 0.0, 1.0);
		float v = floor(255.0 * glow);
		color = vec4(v, v, v, 255.0);
	}
	else if (u_ColorGen == 9) {					/* CGEN_MULTIPLY_BY_WAVEFORM :900 */
		float glow = GLES3_Wave(int(u_WaveRGB.x), u_WaveRGB.y, u_WaveRGB.z, u_WaveRGB.w, u_FreqRGB) * u_IdentityLight;
		glow = clamp(glow, 0.0, 1.0);
		color = vec4(floor(u_BaseColor.rgb * 255.0 * glow), 255.0);
	}
	else if (u_ColorGen == 10 || u_ColorGen == 14) {
		/* CGEN_LIGHTING_SPHERICAL / CGEN_STATIC :903.  The entity/sphere light
		   model (tr_sphere_shade.cpp, RB_CalcDiffuseColor tr_shade_calc.c:2044)
		   is evaluated on the CPU into a_Color by the tr_shade_calc.c fork, so
		   the shader reproduces retail's byte copy (:1015/:920) exactly. */
		color = floor(vcol * 255.0);
	}
	else if (u_ColorGen == 11) {				/* CGEN_CONSTANT :1027 :961 */
		color = floor(u_BaseColor * 255.0);
	}
	else if (u_ColorGen == 13) {				/* CGEN_GLOBAL_COLOR :1030 */
		color = floor(u_BaseColor * 255.0);
	}
	else if (u_ColorGen == 15 || u_ColorGen == 16) {
		/* CGEN_SCOORD / CGEN_TCOORD :1033/:1045 tr_shade_calc.c:1133 */
		float w = (u_ColorGen == 15) ? st0.x : st0.y;
		float f = w;									/* sWeight/tWeight folded */
		f = ((u_AlphaMax - u_AlphaMin) * f + u_AlphaMin) * 255.0;
		f = f - max(float(int(f - u_AlphaConst * 255.0)), 0.0);
		f = u_AlphaConstMin * 255.0 + max(float(int(f - u_AlphaConstMin * 255.0)), 0.0);
		color = vec4(f, f, f, 255.0);
	}
	else if (u_ColorGen == 17 || u_ColorGen == 18) {
		/* CGEN_DOT / CGEN_ONE_MINUS_DOT :1057/:1060 tr_shade_calc.c:976/:998 */
		vec3 viewer = normalize(u_ViewOrigin - pos);
		float f = dot(nrm, viewer);
		f = f * f;
		if (u_ColorGen == 18) f = 1.0 - f;
		f = ((u_AlphaMax - u_AlphaMin) * f + u_AlphaMin) * 255.0;
		f = f - max(float(int(f - 255.0)), 0.0);
		float v = max(f, 0.0);
		color = vec4(v, v, v, 255.0);
	}
	else if (u_ColorGen == 20) {				/* CGEN_FOG :1063 (FinishShader ramp) */
		color = floor(u_FogColor * 255.0);
	}
	else if (u_ColorGen == 21) {				/* CGEN_FOG_BLEND :1077 tr_shade_calc.c:2547 */
		color = floor(u_FogColor * 255.0);
	}
	else {										/* CGEN_BAD/NOISE :818 default */
		color = vec4(255.0, 255.0, 255.0, 255.0);
	}

	/* --- alphaGen, tr_shade.c:1086-1411 -------------------------------- */
	if (u_AlphaGen == 0) {						/* AGEN_IDENTITY :1090 */
		color.a = 255.0;
	}
	else if (u_AlphaGen == 2) {					/* AGEN_ENTITY :1101 tr_shade_calc.c:1152 */
		color.a = floor(u_EntityColor.a * 255.0);
	}
	else if (u_AlphaGen == 3) {					/* AGEN_ONE_MINUS_ENTITY :1121 :1170 */
		color.a = floor((1.0 - u_EntityColor.a) * 255.0);
	}
	else if (u_AlphaGen == 4) {					/* AGEN_VERTEX :1124 */
		color.a = floor(vcol.a * 255.0);
	}
	else if (u_AlphaGen == 5) {					/* AGEN_ONE_MINUS_VERTEX :1131 */
		color.a = floor((1.0 - vcol.a) * 255.0);
	}
	else if (u_AlphaGen == 6) {					/* AGEN_LIGHTING_SPECULAR :1137 :1859 */
		vec3 lightDir = normalize(u_SpecOrigin - pos);
		float d = dot(nrm, lightDir);
		vec3 refl = nrm * (2.0 * d) - lightDir;
		vec3 viewer = u_ViewOrigin - pos;
		float ilength = inversesqrt(dot(viewer, viewer));
		float l = dot(refl, viewer) * ilength;
		float b;
		if (l < 0.0) b = 0.0;
		else { l = l * l; l = l * l; b = min(l * 255.0, 255.0); }
		color.a = b;
	}
	else if (u_AlphaGen == 7) {					/* AGEN_WAVEFORM :1140 tr_shade_calc.c:1233 */
		float glow = GLES3_WaveClamped(int(u_WaveAlpha.x), u_WaveAlpha.y, u_WaveAlpha.z, u_WaveAlpha.w, u_FreqAlpha);
		color.a = floor(255.0 * glow);
	}
	else if (u_AlphaGen == 8) {					/* AGEN_PORTAL :1143 */
		float len = length(pos - u_ViewOrigin) / u_PortalRange;
		float a;
		if (len < 0.0) a = 0.0;
		else if (len > 1.0) a = 255.0;
		else a = len * 255.0;
		color.a = a;
	}
	else if (u_AlphaGen == 13) {				/* AGEN_DOT :1174 :1032 */
		vec3 viewer = normalize(u_ViewOrigin - pos);
		float d = abs(dot(nrm, viewer));
		float f = (d * (u_AlphaMax - u_AlphaMin) + u_AlphaMin) * 255.0;
		f = f - max(float(int(f - 255.0)), 0.0);
		color.a = max(f, 0.0);
	}
	else if (u_AlphaGen == 14) {				/* AGEN_ONE_MINUS_DOT :1177 :1083 */
		vec3 viewer = normalize(u_ViewOrigin - pos);
		float f = 1.0 - dot(nrm, viewer);
		f = f * f;
		f = (f * (u_AlphaMax - u_AlphaMin) + u_AlphaMin) * 255.0;
		f = f - max(float(int(f - 255.0)), 0.0);
		color.a = max(f, 0.0);
	}
	else if (u_AlphaGen == 15) {				/* AGEN_CONSTANT :1180 */
		/* The oracle's AGEN_CONSTANT arm (the deleted docs/oracle/tr_shade.c:1180-1181)
		   reads pStage->colorConst[3], NOT the stage's sCoord/tCoord
		   alphaConst byte: ParseStage's `alphaGen const` arm
		   (tr_shader.c:1683-1684) writes colorConst[3], and only
		   `rgbGen sCoord/tCoord` (tr_shader.c:1429/1730) ever writes
		   alphaConst.  GLES3_SetupStageUniforms has already selected the
		   right source into u_BaseColor.a (color2D[3] for AGEN_GLOBAL_ALPHA,
		   colorConst[3] otherwise), exactly as the two C arms select
		   between backEnd.color2D[3] and pStage->colorConst[3], so this arm
		   and the AGEN_GLOBAL_ALPHA arm differ only in the value the CPU
		   published.  Reading u_AlphaConst here made every shipped
		   `alphaGen const` stage emit alpha 0 (the byte is zero for all of
		   them), which blended the inventory strip's const-alpha shaders
		   away entirely. */
		color.a = floor(u_BaseColor.a * 255.0);
	}
	else if (u_AlphaGen == 16) {				/* AGEN_GLOBAL_ALPHA :1183 */
		color.a = floor(u_BaseColor.a * 255.0);
	}
	else if (u_AlphaGen == 19) {				/* AGEN_SKYALPHA :1186 */
		color.a = floor(u_SkyAlpha * 255.0);
	}
	else if (u_AlphaGen == 20) {				/* AGEN_ONE_MINUS_SKYALPHA :1189 */
		color.a = floor((1.0 - u_SkyAlpha) * 255.0);
	}
	else if (u_AlphaGen == 21) {				/* AGEN_FORCED_ALPHA :1104 */
		color.a = floor(255.0 * u_EntityFade);
	}
	else if (u_AlphaGen == 22 || u_AlphaGen == 23) {
		/* AGEN_DOT_VIEW / AGEN_ONE_MINUS_DOT_VIEW, the deleted docs/oracle/tr_shade_calc.c
		   RB_CalcAlphaFromDotView :2620-2655 and RB_CalcAlphaFromOneMinusDotView
		   :2660-2695:
		       VectorCopy(tr.refdef.viewaxis[0], viewInModel);
		       VectorNormalizeFast(viewInModel);
		       d = fabs(DotProduct(viewInModel, tess.normal[i]));
		       (one-minus: d = 1.0 - fabs(dot))
		   The axis is tr.refdef.viewaxis[0], published as u_ViewAxis0; it was a
		   hardcoded (1,0,0) here, which pinned the generator to one world
		   direction and produced the wrong alpha for all 26 shipped
		   viewDot/oneMinusViewDot stages (census rgbGen/alphaGen pairs
		   (2,22)/(2,23)/(11,22)). */
		vec3 viewInModel = normalize(u_ViewAxis0);
		float f = abs(dot(viewInModel, nrm));
		if (u_AlphaGen == 23) f = 1.0 - f;
		f = ((u_AlphaMax - u_AlphaMin) * f + u_AlphaMin) * 255.0;
		f = f - max(float(int(f - 255.0)), 0.0);
		color.a = max(f, 0.0);
	}
	else if (u_AlphaGen == 33 || u_AlphaGen == 34) {
		/* AGEN_SCOORD / AGEN_TCOORD :1192/:1204 :1114 */
		float w = (u_AlphaGen == 33) ? st0.x : st0.y;
		float f = ((u_AlphaMax - u_AlphaMin) * w + u_AlphaMin) * 255.0;
		f = f - max(float(int(f - u_AlphaConst * 255.0)), 0.0);
		f = u_AlphaConstMin * 255.0 + max(float(int(f - u_AlphaConstMin * 255.0)), 0.0);
		color.a = f;
	}
	else if (u_AlphaGen == 39) {				/* AGEN_HEIGHT_FADE :1406 :1065 */
		float dist = abs(u_ViewOrigin.z - pos.z);
		dist = clamp(dist, u_AlphaMin, u_AlphaMax);
		float alpha = (dist - u_AlphaMin) / (u_AlphaMax - u_AlphaMin);
		color.a = (1.0 - alpha) * 255.0;
	}
#endif /* GLP_RGBAGEN */

#ifndef GLP_RGBAGEN
	color = vec4(255.0, 255.0, 255.0, 255.0);
#endif

	/* Per-vertex fog modulation -- tr_shade.c ComputeColors:1424-1449.
	   Applied in byte space, exactly as the C code truncates at the byte
	   store.  When the stage's colour is CPU-computed (spherelight / static /
	   dist-fade) GLES3_SetupStageUniforms sets u_Acff = 0: the oracle has
	   already applied it and the value arrives through a_Color. */
	color = GLES3_ApplyAcff(color, pos);

	/* EF2 RF_FORCE_ALPHA -- the per-vertex alpha overwrite
	   (tr_shade.c:1638/1899).  Only the shader-computed arms need it; the
	   CPU-coloured arms get the byte written into svars.colors by the caller. */
	if (u_ForceAlpha >= 0.0) {
		color.a = floor(u_ForceAlpha * 255.0);
	}

	/* Tessellation-independent: finish the colour in byte space. */
	v_Color = GLES3_Byte(color);

	/* --- texture coordinates ------------------------------------------ */
	vec2 tc0 = a_TexCoord0;
#ifdef GLP_TCGEN0
	tc0 = GLES3_TexGen0(pos, nrm);
#endif
#ifdef GLP_TCMOD0
	tc0 = GLES3_ApplyTexMod0(tc0, pos);
#endif
	v_TexCoord0 = tc0;

#ifdef GLP_LIGHTMAP
	vec2 tc1 = a_TexCoord1;
#ifdef GLP_TCGEN1
	tc1 = GLES3_TexGen1(pos, nrm);
#endif
#ifdef GLP_TCMOD1
	tc1 = GLES3_ApplyTexMod1(tc1, pos);
#endif
	v_TexCoord1 = tc1;
#endif

	gl_Position = u_MVP * vec4(pos, 1.0);
	v_FogDepth = gl_Position.w;
	/* GL_CLIP_PLANE0 replaced: the same eye-space half-space, evaluated per
	   vertex and interpolated; the fragment stage discards the negative side. */
	v_ClipDist = dot(u_ClipPlane, u_ModelMatrix * vec4(a_Position, 1.0));
}
