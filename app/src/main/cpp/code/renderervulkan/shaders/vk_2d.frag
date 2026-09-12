#version 450


layout(location = 1) in vec4 v_Color;
layout(location = 2) in vec2 v_TexCoord0;

layout(set = 0, binding = 1) uniform sampler2D u_Tex0;



layout(location = 0) out vec4 fragColor;

void main()
{
	vec4 t0 = texture(u_Tex0, v_TexCoord0);
	vec4 c = (u_TexEnv0 == 3) ? t0 : t0 * v_Color;
	if (u_AlphaTest == 1)      { if (!(c.a > 0.0)) discard; }
	else if (u_AlphaTest == 2) { if (!(c.a < 0.5)) discard; }
	else if (u_AlphaTest == 4) { if (!(c.a >= 0.5)) discard; }
	fragColor = c;
}
