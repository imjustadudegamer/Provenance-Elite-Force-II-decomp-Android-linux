#version 450


//@@VK_CLIP@@
layout(location = 3) in float v_ClipDist;

layout(location = 1) in vec4 v_Color;
layout(location = 2) in vec2 v_TexCoord0;

layout(set = 0, binding = 1) uniform sampler2D u_Tex0;

layout(location = 0) out vec4 fragColor;

void main()
{
	if (u_ClipPlaneEnable != 0 && v_ClipDist < 0.0) discard;
	fragColor = texture(u_Tex0, v_TexCoord0) * v_Color;
}
