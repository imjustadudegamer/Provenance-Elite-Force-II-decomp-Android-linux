#version 300 es
/*
ProvenanceEF2 GLES3 shade layer -- dynamic-light pass program.

ProjectDlightTexture (tr_shade.c:642-799) computes BOTH the projected light
texture coordinates and the per-vertex modulate colour on the CPU, byte for
byte, and hands them to the fixed-function unit as client arrays.  The GLES3
fork keeps that CPU computation verbatim (code/renderergles3/tr_shade.c) and
uploads the two arrays as attributes, so this program is a pure passthrough:
that is the only way to be bit-exact, because the planetype selection, the
clip-bit triangle rejection and the `modulate` falloff are integer/float
arithmetic the CPU already reproduces with citations.
*/
in vec3 a_Position;
in vec2 a_TexCoord0;		/* texCoordsArray[i] from ProjectDlightTexture */
in vec4 a_Color;			/* colorArray[i] (byte colour / 255) */

uniform mat4 u_MVP;
uniform mat4 u_ModelMatrix;	/* backEnd.ori.modelMatrix: eye-space clip test */

//@@GLES3_CLIP@@

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
