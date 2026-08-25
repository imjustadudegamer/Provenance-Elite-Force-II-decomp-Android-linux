/*
reproduces.  This is a retail port: an approximation is a divergence, so the
*/

#define GLES3_FUNCTABLE_SIZE 2048
#define GLES3_FUNCTABLE_MASK 2047

uniform sampler2D	u_WaveTable;	/* 2048 x 5 R32F, NEAREST, no mips */
uniform float		u_ShaderTime;	/* tess.shaderTime (tr_shade.c:1713) */

float GLES3_Wave(int func, float base, float amplitude, float phase, float frequency)
{
	int idx = int((phase + u_ShaderTime * frequency) * float(GLES3_FUNCTABLE_SIZE)) & GLES3_FUNCTABLE_MASK;
	int row = clamp(func - 1, 0, 4);
	return base + texelFetch(u_WaveTable, ivec2(idx, row), 0).r * amplitude;
}

float GLES3_WaveClamped(int func, float base, float amplitude, float phase, float frequency)
{
	float glow = GLES3_Wave(func, base, amplitude, phase, frequency);
	return clamp(glow, 0.0, 1.0);
}

vec4 GLES3_Byte(vec4 c)
{
	return floor(clamp(c, 0.0, 255.0)) / 255.0;
}

vec3 GLES3_Byte3(vec3 c)
{
	return floor(clamp(c, 0.0, 255.0)) / 255.0;
}

vec3 GLES3_RgbFromScalar(float d, float alphaMin, float alphaMax)
{
	float v = (d - alphaMin) * (255.0 / (alphaMax - alphaMin));
	return GLES3_Byte3(vec3(v));
}

float GLES3_AlphaFromScalar(float d, float alphaMin, float alphaMax)
{
	float v = (d - alphaMin) * (255.0 / (alphaMax - alphaMin));
	return floor(clamp(v, 0.0, 255.0)) / 255.0;
}

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
the per-arm arithmetic lives in tr_shade_calc.c and is cited in the vertex
*/

/*
The integer generator selects are retail's own enum ordinals -- no translation.
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

uniform int		u_FogMode;			/* 0 none, 1 acff vertex modulate, 2 GL_LINEAR fixed-function */
uniform float	u_FogStart;			/* backEnd.viewParms.farplane_bias   (tr_backend.c:1381) */
uniform float	u_FogEnd;			/* backEnd.viewParms.farplane_distance (tr_backend.c:1382) */
uniform vec4	u_FogFixColor;		/* farplane_color * tr.identityLight (tr_backend.c:1386-1389) */

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

float GLES3_FogRampFactor(vec3 pos)
{
	vec2 st = GLES3_FogST(pos);
	float factor = -(256.0 * (st.x * (st.y - 0.25)) - 1.0);
	return max(factor, 0.0);
}

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
a vertex behind the fog plane's S origin gives factor = 1 + 56*|S| / 1 + 184*|S|
(from the two T ends, tr_shade_calc.c:2250-2253), and ComputeColors @0x353F0
*/
vec4 GLES3_AcffByte(vec4 c)
{
	return mod(trunc(c), 256.0);
}

/*
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
depth = gl_Position.w.
*/
vec3 GLES3_FixedFog(vec3 rgb, float depth)
{
	float f = clamp((u_FogEnd - depth) / (u_FogEnd - u_FogStart), 0.0, 1.0);
	return mix(u_FogFixColor.rgb, rgb, f);
}
