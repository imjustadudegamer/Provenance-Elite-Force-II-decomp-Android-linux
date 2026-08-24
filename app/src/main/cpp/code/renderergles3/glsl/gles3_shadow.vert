#version 300 es
/*
 * RB_ShadowTessEnd (tr_shadows.c) -- the stencil shadow VOLUME.  It draws
   depth-only geometry with colour writes off (glColorMask) and the stencil
   op set by state; the vertex transform is the plain MVP.
RB_ShadowFinish (tr_backend.c) darkens the stencil buffer with a full-screen
*/
in vec3 a_Position;
uniform mat4 u_MVP;
uniform mat4 u_ModelMatrix;	/* backEnd.ori.modelMatrix: eye-space clip test */

//@@GLES3_CLIP@@
out highp float v_ClipDist;
void main()
{
	gl_Position = u_MVP * vec4(a_Position, 1.0);
	v_ClipDist = dot(u_ClipPlane, u_ModelMatrix * vec4(a_Position, 1.0));
}
