#version 300 es
/* ProvenanceEF2 GLES3 shade layer -- dynamic-light fragment program.
   GL_Bind(tr.dlightImage) then R_DrawElements (tr_shade.c:786-795); the blend
   The texel is modulated by the per-vertex colour, which is what
   fixed-function GL_MODULATE did. */
precision highp float;
precision highp sampler2D;

in highp float v_ClipDist;

in vec4 v_Color;
in vec2 v_TexCoord0;

uniform sampler2D u_Tex0;

out vec4 fragColor;

void main()
{
	if (u_ClipPlaneEnable != 0 && v_ClipDist < 0.0) discard;
	fragColor = texture(u_Tex0, v_TexCoord0) * v_Color;
}
