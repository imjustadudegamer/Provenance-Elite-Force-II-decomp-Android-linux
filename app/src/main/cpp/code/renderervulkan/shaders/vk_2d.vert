#version 450
layout(location = 0) in vec3 a_Position;
layout(location = 3) in vec2 a_TexCoord0;
layout(location = 2) in vec4 a_Color;



layout(location = 1) out vec4 v_Color;
layout(location = 2) out vec2 v_TexCoord0;

void main()
{
	v_Color = a_Color * u_BaseColor;
	v_TexCoord0 = a_TexCoord0;
	gl_Position = u_MVP * vec4(a_Position, 1.0);
}
