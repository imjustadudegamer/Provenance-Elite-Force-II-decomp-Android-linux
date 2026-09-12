#version 450
precision highp float;	
//@@VK_CLIP@@
layout(location = 2) in float v_ClipDist;

layout(location = 1) in vec4 v_Color;
layout(location = 0) out vec4 fragColor;
void main()
{
	if (u_ClipPlaneEnable != 0 && v_ClipDist < 0.0) discard;
	fragColor = v_Color; }
