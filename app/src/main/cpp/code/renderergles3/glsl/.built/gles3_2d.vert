#version 300 es
/* gles3: stage-matched default precision (see build_glsl.py); int and
   sampler2D defaults differ per stage in ES 3.00, and a shared uniform or
   varying must match or the pair is a LINK error on a strict driver.
   sampler2D is pinned highp in BOTH stages on purpose: u_WaveTable is an
   R32F sin/table texture read with texelFetch and its value precision is
   the sampler qualifier, so lowp would truncate the oracle-exact table. */
precision highp int;
precision highp float;
precision highp sampler2D;

/*
ProvenanceEF2 GLES3 shade layer (G2 gles3-gles3-shade)
gles3_2d.vert -- the 2D / HUD / font program.

the deleted oracle drives 2D through the SAME stage iterator with backEnd.in2D set:
RB_SetGL2D (tr_backend.c:1135) calls Set2DWindow (tr_draw.c) to install an
ortho projection, and RB_IterateStagesGeneric adds GLS_DEPTHTEST_DISABLE when
backEnd.in2D (tr_shade.c:1661-1663).  GL_State's depthtest-disable arm is a
render-state, not a program, so this program only has to reproduce the colour
path: rgbGen constant/global (Draw_SetColor -> tr_draw.c), alphaGen constant/
global, tcGen texture.  No normals, no lighting, no wave LUT -- the UI path is
kept small deliberately (old-Mali shader compiler faults, MALI_FIXES.md).
*/
in vec3 a_Position;
in vec2 a_TexCoord0;
in vec4 a_Color;

uniform mat4 u_MVP;			/* Set2DWindow ortho projection */
uniform vec4 u_BaseColor;	/* Draw_SetColor / backEnd.color2D, /255 */

out vec4 v_Color;
out vec2 v_TexCoord0;

void main()
{
	/* Draw_SetColor / CGEN_CONSTANT (tr_shade.c:1027) modulate the vertex
	   colour: the 2D quad attributes carry colour 255 (or the per-vertex
	   colour of a coloured font glyph). */
	v_Color = a_Color * u_BaseColor;
	v_TexCoord0 = a_TexCoord0;
	gl_Position = u_MVP * vec4(a_Position, 1.0);
}
