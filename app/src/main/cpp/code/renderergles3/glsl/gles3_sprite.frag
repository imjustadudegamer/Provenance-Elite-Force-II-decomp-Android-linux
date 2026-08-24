#version 300 es
precision highp float;
precision highp sampler2D;
//@@GLES3_CLIP@@
in highp float v_ClipDist;

in vec4 v_Color;
in vec2 v_TexCoord0;
uniform sampler2D u_Tex0;
uniform int u_AlphaTest;
out vec4 fragColor;
void main()
{
	if (u_ClipPlaneEnable != 0 && v_ClipDist < 0.0) discard;
	vec4 c = texture(u_Tex0, v_TexCoord0) * v_Color;
	if (u_AlphaTest == 1)      { if (!(c.a > 0.0)) discard; }
	else if (u_AlphaTest == 2) { if (!(c.a < 0.5)) discard; }
	else if (u_AlphaTest == 4) { if (!(c.a >= 0.5)) discard; }
	fragColor = c;
}
