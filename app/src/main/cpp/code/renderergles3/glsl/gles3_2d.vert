#version 300 es
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
