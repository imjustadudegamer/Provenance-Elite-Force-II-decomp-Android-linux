#version 300 es
/*
state machine as an ordinary stage.  The vertex data is tess.xyz plus the sky
shader's time (tr_sky.c's cloud pass).  GLES 3.0 has no fixed-function matrices
*/
in vec3 a_Position;
in vec2 a_TexCoord0;
in vec4 a_Color;
uniform mat4 u_MVP;
uniform mat4 u_ModelMatrix;	/* backEnd.ori.modelMatrix: eye-space clip test */

//@@GLES3_CLIP@@
uniform vec4 u_BaseColor;	/* sky stage colour (rgbGen identity = white) */
uniform float u_SkyAlpha;	/* tr.refdef.sky_alpha */
uniform vec2 u_SkyScroll;	/* cloud-layer texcoord scroll (tr_sky.c) */
uniform int  u_SkyAlphaMode;/* 0 identity, 19 skyAlpha, 20 oneMinusSkyAlpha */
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
