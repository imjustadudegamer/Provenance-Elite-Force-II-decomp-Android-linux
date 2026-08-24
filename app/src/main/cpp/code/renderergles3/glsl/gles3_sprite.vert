#version 300 es
/*
ProvenanceEF2 GLES3 shade layer -- sprite program (tr_sprite.c).

GL1 RB_SurfaceSprite builds the billboard quad on the CPU from the sprite's
origin, radius, rotation and the view axis for each SPRITE_* arm
(tr_local.h:764-768: SPRITE_PARALLEL, SPRITE_PARALLEL_ORIENTED,
SPRITE_ORIENTED, SPRITE_PARALLEL_UPRIGHT).  The axis selection stays on the CPU
(tr_sprite.c is G3's file); this program transforms the resulting quad and
modulates the texture by the sprite's colour (rgbGen entity/constant).
*/
in vec3 a_Position;
in vec2 a_TexCoord0;
in vec4 a_Color;
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
