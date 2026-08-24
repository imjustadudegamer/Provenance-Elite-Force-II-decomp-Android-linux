#version 300 es
/* ProvenanceEF2 GLES3 shade layer -- shadow fragment program.
   The stencil-volume pass writes no colour (GL1 leaves colour writes off and
   only the depth/stencil test matters), so the fragment shader is empty; it
   exists because GLES 3.0 core has no fixed-function pipeline and a program
   must be bound. */
precision highp float;	/* r200: must match the vertex stage's default (see build_glsl.py) */

//@@GLES3_CLIP@@
in highp float v_ClipDist;
out vec4 fragColor;
void main()
{
	if (u_ClipPlaneEnable != 0 && v_ClipDist < 0.0) discard;
	fragColor = vec4(0.0);
}
