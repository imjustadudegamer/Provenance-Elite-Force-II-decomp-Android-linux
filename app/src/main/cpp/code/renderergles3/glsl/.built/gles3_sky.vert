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


in vec3 a_Position;
in vec2 a_TexCoord0;
in vec4 a_Color;
uniform mat4 u_MVP;
uniform mat4 u_ModelMatrix;	


uniform vec4	u_ClipPlane;		
uniform int		u_ClipPlaneEnable;	

uniform vec4 u_BaseColor;	
uniform float u_SkyAlpha;	
uniform vec2 u_SkyScroll;	
uniform int  u_SkyAlphaMode;
out vec4 v_Color;
out vec2 v_TexCoord0;
out highp float v_ClipDist;
void main()
{
	vec4 c = a_Color * u_BaseColor;
	if (u_SkyAlphaMode == 19)      c.a = u_SkyAlpha;
	else if (u_SkyAlphaMode == 20) c.a = 1.0 - u_SkyAlpha;
	v_Color = c;
	v_TexCoord0 = a_TexCoord0 + u_SkyScroll;
	gl_Position = u_MVP * vec4(a_Position, 1.0);
	v_ClipDist = dot(u_ClipPlane, u_ModelMatrix * vec4(a_Position, 1.0));
}
