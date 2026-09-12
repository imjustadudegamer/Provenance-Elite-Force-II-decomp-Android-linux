#version 450
layout(location = 0) in vec3 a_Position;
layout(location = 3) in vec2 a_TexCoord0;
layout(location = 2) in vec4 a_Color;



//@@VK_CLIP@@




layout(location = 1) out vec4 v_Color;
layout(location = 2) out vec2 v_TexCoord0;
layout(location = 3) out float v_ClipDist;
void main()
{
	vec4 c = a_Color * u_BaseColor;
	if (u_SkyAlphaMode == 19)      c.a = u_SkyAlpha;
	else if (u_SkyAlphaMode == 20) c.a = 1.0 - u_SkyAlpha;
	v_Color = c;
	v_TexCoord0 = a_TexCoord0 + u_SkyScroll;
	gl_Position = u_MVP * vec4(a_Position, 1.0);
	v_ClipDist = dot(u_ClipPlane, u_ModelMatrix * vec4(a_Position, 1.0));
}
