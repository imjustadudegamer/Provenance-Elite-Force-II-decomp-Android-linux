/*
marker by glsl/build_glsl.py, so the uniform names cannot drift from
GL_CLIP_PLANE0 is an EYE-SPACE half-space: a vertex survives iff
dot(plane2.xyz, eyePosition) + plane2.w >= 0, with eyePosition the vertex after
and the vertex shader exports it as v_ClipDist.
Adjudication under STANDING_RULES section 1 (retail accuracy) and section 2
*/

uniform vec4	u_ClipPlane;		/* eye space: (A,B,C,D), RB_ClipPlane_Get */
uniform int		u_ClipPlaneEnable;	/* 0 = no active portal plane */
