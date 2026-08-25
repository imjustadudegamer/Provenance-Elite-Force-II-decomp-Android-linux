/*
===========================================================================
ProvenanceEF2 -- OpenGL ES 3.0 shade layer (G2 gles3-gles3-shade)
gles3_clip.inc.glsl -- the PORTAL CLIP PLANE, shared by every program that
can be bound while the plane is enabled.  Substituted at the `//@@GLES3_CLIP@@`
marker by glsl/build_glsl.py, so the uniform names cannot drift from
GLES3_U_CLIPPLANE / GLES3_U_CLIPPLANEENABLE in tr_program.h.

WHAT IT REPLACES -- the deleted oracle tr_backend.c RB_BeginDrawingView
----------------------------------------------------------------
GL1 clips a portal view with the fixed-function plane:

    plane[0..3] = backEnd.viewParms.portalPlane.{normal,dist}
    plane2[0]   = DotProduct(viewParms.ori.axis[0], plane)     tr_backend.c:718
    plane2[1]   = DotProduct(viewParms.ori.axis[1], plane)     :719
    plane2[2]   = DotProduct(viewParms.ori.axis[2], plane)     :720
    plane2[3]   = DotProduct(plane, viewParms.ori.origin)
                  - plane[3]                                   :721
    qglLoadMatrixf ( s_flipMatrix );                           :723
    qglClipPlane ( GL_CLIP_PLANE0, plane2 );                   :724
    qglEnable ( GL_CLIP_PLANE0 );                              :725

and clears it again in RB_ShadowFinish (tr_shadows.c:322) and
RB_Set2DWindow (tr_draw.c:653).  G1 publishes exactly those four floats
through tr_vbo.c's RB_ClipPlane_Set; G3's RB_ClipPlane_Get returns them and
G2 uploads them here.

CLIP DISTANCE
-------------
GL_CLIP_PLANE0 is an EYE-SPACE half-space: a vertex survives iff
dot(plane2.xyz, eyePosition) + plane2.w >= 0, with eyePosition the vertex after
the model-view transform.  In this fork that transform is u_ModelMatrix
(= RB_GetModelViewMatrix(), the same backEnd.ori.modelMatrix GL1 loads before
each batch), so the exact quantity is

    dot(u_ClipPlane, u_ModelMatrix * vec4(a_Position, 1.0))

and the vertex shader exports it as v_ClipDist.

SANCTIONED DELTA -- per-fragment discard instead of per-primitive clipping
--------------------------------------------------------------------------
Core OpenGL ES 3.0 has NO gl_ClipDistance and no user clip planes: clip
distances are ES 3.2 core / EXT_clip_cull_distance (`EXT_clip_cull_distance`
is not guaranteed on any device this port targets -- see MALI_FIXES.md's rule
against hard-requiring extensions).  So the plane cannot be applied at
primitive assembly; it is applied in the fragment stage:

    if (u_ClipPlaneEnable != 0 && v_ClipDist < 0.0) discard;

Adjudication under STANDING_RULES section 1 (retail accuracy) and section 2
(sanctioned deltas) -- this is the *renderer-substrate* class: the fixed
function it replaces does not exist in the target API.

  * Retail-visible result reproduced: the plane's half-space is the same
    half-space, and the test is evaluated at the same fragments with the same
    sign.  v_ClipDist is a linear function of eye position and is interpolated
    perspective-correctly, so the fragment-stage value equals the clip-plane
    distance AT THAT FRAGMENT exactly (no approximation in the sign test).
    Geometry on the removed side is not drawn -- a fully-removed triangle
    contributes no fragments, and a straddling triangle keeps exactly its
    positive side.
  * The only difference is granularity: fixed-function clipping rewrites the
    primitive at the plane before rasterisation, so its cut edge is exact,
    while a discard cuts on the fragment grid.  The delta is bounded by one
    fragment along the portal seam (sub-pixel; the renderer has no MSAA and
    no line/polygon smoothing, so nothing else observes it).
  * No shipped content is changed outside portal views: RB_ClipPlane_Get
    returns qfalse for every non-portal view and for 2D (RB_Set2DWindow
    disables it), and the enable flag is uploaded with the rest of the view
    state, so a disabled plane never discards (u_ClipPlaneEnable == 0 makes
    the condition short-circuit).

MEASURED CONSUMERS: the clip contract asserts that
RB_ClipPlane_Get is called from GLES3_SetupViewUniforms, that every program
family that can be bound while the plane is enabled carries this chunk and the
discard, and that the 2D program (which is only ever bound after
RB_Set2DWindow's disable) does not.
===========================================================================
*/

uniform vec4	u_ClipPlane;		/* eye space: (A,B,C,D), RB_ClipPlane_Get */
uniform int		u_ClipPlaneEnable;	/* 0 = no active portal plane */
