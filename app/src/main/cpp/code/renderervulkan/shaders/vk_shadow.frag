#version 450
precision highp float;	

//@@VK_CLIP@@
layout(location = 1) in float v_ClipDist;
layout(location = 0) out vec4 fragColor;
void main()
{
	if (u_ClipPlaneEnable != 0 && v_ClipDist < 0.0) discard;
	fragColor = vec4(0.0);
}
