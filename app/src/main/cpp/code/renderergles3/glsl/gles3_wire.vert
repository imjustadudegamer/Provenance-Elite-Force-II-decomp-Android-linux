#version 300 es
/*
ProvenanceEF2 GLES3 shade layer -- r_showtris / r_shownormals debug program.

GL1 DrawTris (tr_shade.c:249) and DrawNormals (tr_shade.c:333) draw GL_LINES
with a per-line colour and GLS_POLYMODE_LINE.  GLES 3.0 has NO glPolygonMode,
so the triangle list is expanded to line segments on the CPU
(code/renderergles3/tr_shade.c GLES3_DrawTris) and this program draws them with
the same per-vertex colour and the same MVP.
*/
in vec3 a_Position;
in vec4 a_Color;
uniform mat4 u_MVP;
uniform mat4 u_ModelMatrix;	/* backEnd.ori.modelMatrix: eye-space clip test */

//@@GLES3_CLIP@@
out vec4 v_Color;
out highp float v_ClipDist;
void main()
{
	v_Color = a_Color;
	gl_Position = u_MVP * vec4(a_Position, 1.0);
	v_ClipDist = dot(u_ClipPlane, u_ModelMatrix * vec4(a_Position, 1.0));
}
