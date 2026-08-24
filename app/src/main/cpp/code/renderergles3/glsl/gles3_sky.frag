#version 300 es
precision highp float;
precision highp sampler2D;
//@@GLES3_CLIP@@
in highp float v_ClipDist;

in vec4 v_Color;
in vec2 v_TexCoord0;
uniform sampler2D u_Tex0;
out vec4 fragColor;
void main()
{
	if (u_ClipPlaneEnable != 0 && v_ClipDist < 0.0) discard;
	fragColor = texture(u_Tex0, v_TexCoord0) * v_Color; }
