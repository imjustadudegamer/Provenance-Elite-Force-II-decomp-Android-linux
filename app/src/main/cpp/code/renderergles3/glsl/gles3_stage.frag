#version 300 es
/*
===========================================================================
ProvenanceEF2 -- OpenGL ES 3.0 shade layer (G2 gles3-gles3-shade)
gles3_stage.frag -- the generic-stage fragment program.

Replaces fixed-function texture env + alpha test + (per-vertex) fog output.

TEXTURE ENV -- the deleted oracle GL_TexEnv (tr_backend.c:191) and the multitexture
combine arms at tr_shade.c:582-606 / tr_backend.c:458-485:
  * one bundle, texEnv 0            -> the stage colour modulates the texel
                                       (GL_MODULATE; tr_shade.c:1665 GL_State
                                       leaves TEXTURE_ENV at MODULATE default)
  * one bundle, r_lightmap          -> GL_REPLACE (DrawMultitextured :586 for a
                                       lightmap bundle; the iterator path also
                                       uses REPLACE at :2018)
  * two bundles, texEnv MODULATE    -> t0 * t1        (:604 GL_TexEnv)
  * two bundles, texEnv ADD         -> t0 + t1        (GL_ADD)
  * two bundles, GLS_MULTITEXTURE   -> GL_COMBINE INTERPOLATE
                                       (tr_backend.c:468-479) i.e.
                                       t1.rgb*primary.a + t0.rgb*(1-primary.a)
  * GLS_MULTITEXTURE_ENV            -> GL_COMBINE4_NV (tr_shade.c:547-576):
                                       MULTITEXTURE_ENV=ADD means
                                       (1-t0)*(1-t1) + t1*(1-t1) + t0*t1... the
                                       retail NV arm is reproduced from the
                                       TexEnv call sequence below.

ALPHA TEST -- GL_State tr_backend.c:490-523: exactly three arms,
GLS_ATEST_GT_0 -> GL_GREATER 0.0, GLS_ATEST_LT_80 -> GL_LESS 0.5,
GLS_ATEST_GE_80 -> GL_GEQUAL 0.5; a mixed mask is a silent no-op.  GLES3 has
no glAlphaFunc, so it is a discard here.
===========================================================================
*/
#define GLES3_FRAGMENT 1
precision highp float;
precision highp sampler2D;

//@@GLES3_COMMON@@

//@@GLES3_CLIP@@
in highp float v_ClipDist;

in vec4 v_Color;
in vec2 v_TexCoord0;
in float v_FogDepth;
#ifdef GLP_LIGHTMAP
in vec2 v_TexCoord1;
#endif

out vec4 fragColor;

void main()
{
	if (u_ClipPlaneEnable != 0 && v_ClipDist < 0.0) discard;
	vec4 t0 = texture(u_Tex0, v_TexCoord0);
	vec4 c = t0 * v_Color;					/* GL_MODULATE (default) */

#ifdef GLP_LIGHTMAP
	vec4 t1 = texture(u_Tex1, v_TexCoord1);
	#if defined(GLP_TEXENV_ADD)
		/* GL_ADD: GL1 GL_TexEnv(GL_ADD), tr_shade.c:605; the alpha halves add
		   too, exactly as fixed-function GL_ADD does. */
		c = t0 * v_Color + t1;
	#elif defined(GLP_TEXENV_INTERPOLATE)
		/* GLS_MULTITEXTURE -> GL_COMBINE, tr_backend.c:468-479:
		   RGB = PRIMARY_COLOR*alpha + TEXTURE*(1-alpha) with
		   ALPHA = MODULATE. */
		c.rgb = t1.rgb * v_Color.a + t0.rgb * (1.0 - v_Color.a);
		c.a   = t0.a * t1.a;
	#elif defined(GLP_TEXENV_COMBINE4)
		/* GLS_MULTITEXTURE_ENV -> GL_COMBINE4_NV, tr_shade.c:552-570 for
		   pStage->multitextureEnv == GL_ADD:
		     RGB = (1-t0)*(1-t1) + t1*(1-t1) + t0*t1
		   The GL1 call sequence expresses this with SRC_COLOR /
		   ONE_MINUS_SRC_COLOR operands; the algebraic result is written out
		   here so the arithmetic is explicit. */
		c.rgb = (1.0 - t0.rgb) * (1.0 - t1.rgb) + t1.rgb * (1.0 - t1.rgb) + t0.rgb * t1.rgb;
	#else
		/* GL_MODULATE on both TMUs, tr_shade.c:605. */
		c = t0 * t1 * v_Color;
	#endif
#endif

	/* --- alpha test, tr_backend.c:490-523 ------------------------------- */
	/* u_AlphaTest: 0 off, 1 GLS_ATEST_GT_0>>28, 2 LT_80>>28, 4 GE_80>>28.
	   A mixed mask (3/5/6/7) is a silent no-op in retail -- the switch falls
	   through -- and is rejected by the census, so only the real arms appear. */
	if (u_AlphaTest == 1) {
		if (!(c.a > 0.0)) discard;			/* GL_GREATER 0.0 */
	} else if (u_AlphaTest == 2) {
		if (!(c.a < 0.5)) discard;			/* GL_LESS 0.5 */
	} else if (u_AlphaTest == 4) {
		if (!(c.a >= 0.5)) discard;			/* GL_GEQUAL 0.5 */
	}

	/* --- fixed-function fog arm, tr_backend.c:1370-1391 ----------------
	   GL_LINEAR with START=farplane_bias, END=farplane_distance and the
	   far-plane colour; only the RB_GLFogStageIteratorFunc pass sets
	   u_FogMode == 2 (tr_shade.c:501-509).  No fixed-function fog exists in
	   GLES 3.0, so the identical equation is evaluated here. */
	if (u_FogMode == 2) {
		c.rgb = GLES3_FixedFog(c.rgb, v_FogDepth);
	}

	fragColor = c;
}
