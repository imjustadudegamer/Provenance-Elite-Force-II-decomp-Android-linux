#version 300 es
/* gles3: stage-matched default precision (see build_glsl.py); int and
   sampler2D defaults differ per stage in ES 3.00, and a shared uniform or
   varying must match or the pair is a LINK error on a strict driver.
   sampler2D is pinned highp in BOTH stages on purpose: u_WaveTable is an
   R32F sin/table texture read with texelFetch and its value precision is
   the sampler qualifier, so a lowp sampler would truncate the table. */
precision highp int;
precision highp float;
precision highp sampler2D;


#define GLES3_FRAGMENT 1
precision highp float;
precision highp sampler2D;


#define GLES3_FUNCTABLE_SIZE 2048
#define GLES3_FUNCTABLE_MASK 2047

uniform sampler2D	u_WaveTable;	
uniform float		u_ShaderTime;	

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


uniform mat4	u_MVP;				
uniform mat4	u_ModelMatrix;		
uniform vec3	u_ViewOrigin;		
uniform vec3	u_ViewAxis0;		
uniform vec3	u_EntityOrigin;		
uniform mat3	u_EntityAxis;		
uniform vec4	u_EntityColor;		
uniform vec4	u_BaseColor;		
uniform vec4	u_VertColor;		
uniform float	u_IdentityLight;	
uniform float	u_IdentityLightByte;
uniform float	u_EntityFade;		
uniform float	u_PortalRange;		
uniform vec3	u_SpecOrigin;		
uniform vec3	u_SunDirection;		
uniform float	u_SkyAlpha;			
uniform vec4	u_EntityTexCoord;	

uniform int		u_ColorGen;			
uniform int		u_AlphaGen;			
uniform int		u_TCGen0;			
uniform int		u_TCGen1;
uniform vec3	u_TCGen0Vec0, u_TCGen0Vec1;
uniform vec3	u_TCGen1Vec0, u_TCGen1Vec1;

uniform float	u_AlphaMin;			
uniform float	u_AlphaMax;			
uniform float	u_AlphaConst;		
uniform float	u_AlphaConstMin;	
uniform vec4	u_WaveRGB;			
uniform vec4	u_WaveAlpha;
uniform float	u_FreqRGB;			
uniform float	u_FreqAlpha;		

uniform int		u_NumTexMod0;		
uniform int		u_NumTexMod1;
uniform vec4	u_TexMod0[16];		
uniform vec4	u_TexMod1[16];

uniform int		u_AlphaTest;		
uniform int		u_TexEnv0;			
uniform int		u_TexEnv1;
uniform sampler2D	u_Tex0;
uniform sampler2D	u_Tex1;
uniform vec4	u_Tex1Color;		

uniform int		u_FogNum;			
uniform float	u_FogTcScale;		
uniform float	u_FogSConstant;
uniform vec3	u_FogSVector;
uniform float	u_FogTConstant;
uniform vec3	u_FogTVector;
uniform int		u_FogViewOutside;	
uniform vec4	u_FogColor;			
uniform int		u_FogDensityArm;	
uniform float	u_FogOolen;			
uniform float	u_FogPlaneConst;	
uniform float	u_FogDView;
uniform vec3	u_FogDensityTVector;
uniform int		u_Acff;				

uniform int		u_FogMode;			
uniform float	u_FogStart;			
uniform float	u_FogEnd;			
uniform vec4	u_FogFixColor;		

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


vec4 GLES3_AcffByte(vec4 c)
{
	return mod(trunc(c), 256.0);
}


vec4 GLES3_ApplyAcff(vec4 c, vec3 pos)
{
	if (u_Acff == 0) return c;
	float f = (u_FogDensityArm != 0) ? (1.0 - GLES3_FogDensity(pos)) : GLES3_FogRampFactor(pos);
	if (u_Acff == 1) return vec4(GLES3_AcffByte(vec4(c.rgb, 0.0)).rgb, c.a);
	if (u_Acff == 2) return GLES3_AcffByte(c);
	if (u_Acff == 3) return vec4(c.rgb, GLES3_AcffByte(vec4(c.a, 0.0, 0.0, 0.0)).x);
	return c;
}


vec3 GLES3_FixedFog(vec3 rgb, float depth)
{
	float f = clamp((u_FogEnd - depth) / (u_FogEnd - u_FogStart), 0.0, 1.0);
	return mix(u_FogFixColor.rgb, rgb, f);
}


uniform vec4	u_ClipPlane;		
uniform int		u_ClipPlaneEnable;	

in highp float v_ClipDist;

in vec4 v_Color;
in vec2 v_TexCoord0;
in float v_FogDepth;
#ifdef GLP_LIGHTMAP
in vec2 v_TexCoord1;
#endif

out vec4 fragColor;

void main()
{
	if (u_ClipPlaneEnable != 0 && v_ClipDist < 0.0) discard;
	vec4 t0 = texture(u_Tex0, v_TexCoord0);
	vec4 c = t0 * v_Color;					

#ifdef GLP_LIGHTMAP
	vec4 t1 = texture(u_Tex1, v_TexCoord1);
	#if defined(GLP_TEXENV_ADD)
		
		c = t0 * v_Color + t1;
	#elif defined(GLP_TEXENV_INTERPOLATE)
		
		c.rgb = t1.rgb * v_Color.a + t0.rgb * (1.0 - v_Color.a);
		c.a   = t0.a * t1.a;
	#elif defined(GLP_TEXENV_COMBINE4)
		
		c.rgb = (1.0 - t0.rgb) * (1.0 - t1.rgb) + t1.rgb * (1.0 - t1.rgb) + t0.rgb * t1.rgb;
	#else
		
		c = t0 * t1 * v_Color;
	#endif
#endif


	if (u_AlphaTest == 1) {
		if (!(c.a > 0.0)) discard;			
	} else if (u_AlphaTest == 2) {
		if (!(c.a < 0.5)) discard;			
	} else if (u_AlphaTest == 4) {
		if (!(c.a >= 0.5)) discard;			
	}

	
	if (u_FogMode == 2) {
		c.rgb = GLES3_FixedFog(c.rgb, v_FogDepth);
	}

	fragColor = c;
}
