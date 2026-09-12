#version 450
layout(location = 0) in vec3 a_Position;



//@@VK_CLIP@@
layout(location = 1) out float v_ClipDist;
void main()
{
	gl_Position = u_MVP * vec4(a_Position, 1.0);
	v_ClipDist = dot(u_ClipPlane, u_ModelMatrix * vec4(a_Position, 1.0));
}
