#version 300 es
/*
ProvenanceEF2 GLES3 shade layer -- sun-flare billboard (tr_sun_flare.cpp).

The flare is a camera-facing quad whose four corners and texcoords the CPU
builds from the view axis and the flare's size, exactly as GL1 does; the
program transforms and passes the flare colour (a per-vertex colour from the
flare's occlusion ramp).  Blend is ONE/ONE additive with depth-test on and
depth-write off (GL1's flare state).
*/
in vec3 a_Position;
in vec2 a_TexCoord0;
in vec4 a_Color;
uniform mat4 u_MVP;
uniform mat4 u_ModelMatrix;	/* backEnd.ori.modelMatrix: eye-space clip test */

//@@GLES3_CLIP@@
out vec4 v_Color;
out vec2 v_TexCoord0;
out highp float v_ClipDist;
void main()
{
	v_Color = a_Color;
	v_TexCoord0 = a_TexCoord0;
	gl_Position = u_MVP * vec4(a_Position, 1.0);
	v_ClipDist = dot(u_ClipPlane, u_ModelMatrix * vec4(a_Position, 1.0));
}
