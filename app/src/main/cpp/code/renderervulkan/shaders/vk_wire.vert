#version 450
layout(location = 0) in vec3 a_Position;
layout(location = 2) in vec4 a_Color;



//@@VK_CLIP@@
layout(location = 1) out vec4 v_Color;
layout(location = 2) out float v_ClipDist;
void main()
{
	v_Color = a_Color;
	gl_Position = u_MVP * vec4(a_Position, 1.0);
	v_ClipDist = dot(u_ClipPlane, u_ModelMatrix * vec4(a_Position, 1.0));
}
