#version 300 es
/* ProvenanceEF2 GLES3 shade layer -- 2D/HUD/font fragment program.
   GL_MODULATE (tr_shade.c:1665 GL_State leaves TEXTURE_ENV MODULATE) plus the
   three GLS_ATEST arms for the font/UI alpha-tested bitmaps
   (tr_backend.c:490-523).  The 2D path is never fogged (tr_shade.c:430
   `!backEnd.in2D` is the global-fog gate), so there is no fog here. */
precision highp float;
precision highp sampler2D;

in vec4 v_Color;
in vec2 v_TexCoord0;

uniform sampler2D u_Tex0;
uniform int u_TexEnv0;		/* 1 modulate, 3 replace */
uniform int u_AlphaTest;	/* 0,1 GT0, 2 LT_80, 4 GE_80 */

out vec4 fragColor;

void main()
{
	vec4 t0 = texture(u_Tex0, v_TexCoord0);
	vec4 c = (u_TexEnv0 == 3) ? t0 : t0 * v_Color;
	if (u_AlphaTest == 1)      { if (!(c.a > 0.0)) discard; }
	else if (u_AlphaTest == 2) { if (!(c.a < 0.5)) discard; }
	else if (u_AlphaTest == 4) { if (!(c.a >= 0.5)) discard; }
	fragColor = c;
}
