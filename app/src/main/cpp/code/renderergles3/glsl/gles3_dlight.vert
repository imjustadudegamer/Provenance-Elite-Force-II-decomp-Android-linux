#version 300 es
/*
texture coordinates and the per-vertex modulate colour on the CPU, byte for
*/
in vec3 a_Position;
in vec2 a_TexCoord0;		/* texCoordsArray[i] from ProjectDlightTexture */
in vec4 a_Color;			/* colorArray[i] (byte colour / 255) */

uniform mat4 u_MVP;
uniform mat4 u_ModelMatrix;	/* backEnd.ori.modelMatrix: eye-space clip test */


out vec4 v_Color;
out vec2 v_TexCoord0;
out highp float v_ClipDist;

void main()
{
	v_Color = a_Color;
	v_TexCoord0 = a_TexCoord0;
	gl_Position = u_MVP * vec4(a_Position, 1.0);
	v_ClipDist = dot(u_ClipPlane, u_ModelMatrix * vec4(a_Position, 1.0));
}
