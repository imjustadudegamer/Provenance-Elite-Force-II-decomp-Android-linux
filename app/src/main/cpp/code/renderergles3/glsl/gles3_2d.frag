#version 300 es
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
