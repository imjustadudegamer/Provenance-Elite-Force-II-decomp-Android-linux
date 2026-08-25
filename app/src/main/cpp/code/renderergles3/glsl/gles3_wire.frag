#version 300 es
precision highp float;	/* gles3: v_Color is a shared varying; the vertex default is highp */
in highp float v_ClipDist;

in vec4 v_Color;
out vec4 fragColor;
void main()
{
	if (u_ClipPlaneEnable != 0 && v_ClipDist < 0.0) discard;
	fragColor = v_Color; }
