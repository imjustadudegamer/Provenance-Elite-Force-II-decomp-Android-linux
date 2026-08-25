/*
===========================================================================
ProvenanceEF2 -- OpenGL ES 3.0 shade layer (G2 gles3-gles3-shade)
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
