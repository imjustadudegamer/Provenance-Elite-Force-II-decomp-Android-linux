#version 300 es
/*
gles3_stage.vert -- the generic-stage vertex program.
integer uniform carrying retail's OWN enum ordinal
this (glsl/generic_vp.glsl:56-59 `uniform int u_ColorGen; u_AlphaGen;` and
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

//@@GLES3_COMMON@@

//@@GLES3_CLIP@@

uniform vec3	u_LocalViewOrigin;
uniform float	u_ForceAlpha;

#ifdef GLP_TCGEN0
vec2 GLES3_TexGen0(vec3 pos, vec3 nrm)
{
	if (u_TCGen0 == 2)	return a_TexCoord1;			/* TCGEN_LIGHTMAP  :1478 */
	if (u_TCGen0 == 3)	return a_TexCoord0;			/* TCGEN_TEXTURE   :1472 */
	if (u_TCGen0 == 1)	return vec2(0.0);			/* TCGEN_IDENTITY  :1469 */
	if (u_TCGen0 == 5)								/* TCGEN_VECTOR    :1487 */
		return vec2(dot(pos, u_TCGen0Vec0), dot(pos, u_TCGen0Vec1));
	if (u_TCGen0 == 4) {							/* ENVIRONMENT_MAPPED :1484 */
		vec3 viewer = normalize(u_ViewOrigin - pos);
		float d = dot(nrm, viewer);
		vec3 refl = nrm * (2.0 * d) - viewer;
		/* retail is K*(r+1) with K = 1: a [0,2] wrap, not a [0,1] sphere map.
		   retail's own menu capture pins the Intrepid base stage's st at
		   0.0029..1.9837 / 0.0092..1.9919 over 1034 vertices. */
		return vec2(1.0 + refl.y, 1.0 - refl.z);
	}
	if (u_TCGen0 == 7) {							/* ANGLE_BASED_ENVIRONMENT :1496 */
		vec3 viewer = normalize(u_EntityAxis[0]);
		float d = dot(nrm, viewer);
		vec3 refl = nrm * (2.0 * d) - viewer;
		return vec2(1.0 + refl.y, 1.0 - refl.z);
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

#ifdef GLP_TCMOD0
vec2 GLES3_ApplyTexMod0(vec2 st, vec3 pos)
{
	for (int tm = 0; tm < u_NumTexMod0; tm++) {
		int   ty = int(u_TexMod0[tm * 4 + 0].x);
		vec4  p0 = u_TexMod0[tm * 4 + 0];
		vec4  p1 = u_TexMod0[tm * 4 + 1];
		if (ty == GLES3_TMOD_NONE) break;					/* :1526 */
		else if (ty == GLES3_TMOD_SCROLL || ty == GLES3_TMOD_ENTITYTR) {
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
			float now = p1.x + u_ShaderTime * p1.y;
			float off0 = (pos.x + pos.z) * (1.0 / 128.0) * 0.125 + now;
			float off1 = pos.y * (1.0 / 128.0) * 0.125 + now;
			st.x += texelFetch(u_WaveTable, ivec2(int(off0 * 2048.0) & 2047, 0), 0).r * p0.w;
			st.y += texelFetch(u_WaveTable, ivec2(int(off1 * 2048.0) & 2047, 0), 0).r * p0.w;
		}
		else if (ty == GLES3_TMOD_ROTATE) {					/* :1560 tr_shade_calc.c:1779 */
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
			/* RB_CalcBulgeTexCoords computes `offset' per vertex and DISCARDS it,
			   so this arm is deliberately a no-op. */
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
		else if (ty == GLES3_TMOD_BULGETRANS) { /* no-op, see ApplyTexMod0 */ }
	}
	return st;
}
#endif

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
		float w = (u_ColorGen == 15) ? st0.x : st0.y;
		float f = w;									/* sWeight/tWeight folded */
		f = ((u_AlphaMax - u_AlphaMin) * f + u_AlphaMin) * 255.0;
		f = f - max(float(int(f - u_AlphaConst * 255.0)), 0.0);
		f = u_AlphaConstMin * 255.0 + max(float(int(f - u_AlphaConstMin * 255.0)), 0.0);
		color = vec4(f, f, f, 255.0);
	}
	else if (u_ColorGen == 17 || u_ColorGen == 18) {
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
		vec3 viewInModel = normalize(u_ViewAxis0);
		float f = abs(dot(viewInModel, nrm));
		if (u_AlphaGen == 23) f = 1.0 - f;
		f = ((u_AlphaMax - u_AlphaMin) * f + u_AlphaMin) * 255.0;
		f = f - max(float(int(f - 255.0)), 0.0);
		color.a = max(f, 0.0);
	}
	else if (u_AlphaGen == 33 || u_AlphaGen == 34) {
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
	   already applied it and the value arrives through a_Color. */
	color = GLES3_ApplyAcff(color, pos);

	/* EF2 RF_FORCE_ALPHA -- the per-vertex alpha overwrite
	   CPU-coloured arms get the byte written into svars.colors by the caller. */
	if (u_ForceAlpha >= 0.0) {
		color.a = floor(u_ForceAlpha * 255.0);
	}

	v_Color = GLES3_Byte(color);

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
