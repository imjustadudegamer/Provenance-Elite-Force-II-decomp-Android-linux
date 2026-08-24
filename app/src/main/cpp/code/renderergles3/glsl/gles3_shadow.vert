#version 300 es
/*
ProvenanceEF2 GLES3 shade layer -- shadow program family.

Two arms share this source, selected by GLP_SHADOW usage at the call site:
 * RB_ShadowTessEnd (tr_shadows.c) -- the stencil shadow VOLUME.  It draws
   depth-only geometry with colour writes off (glColorMask) and the stencil
   op set by state; the vertex transform is the plain MVP.
 * RB_ProjectionShadowDeform (tr_shade_calc.c:828) -- the projected blob.  The
   deform is applied on the CPU (it needs the shadow plane and the entity
   matrix, exactly as GL1 computes it) before upload, so this program sees the
   already-deformed positions and only the colour path differs.

RB_ShadowFinish (tr_backend.c) darkens the stencil buffer with a full-screen
quad whose colour is the projection colour -- that quad uses the 2D program.
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
