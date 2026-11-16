/*
===========================================================================
This file is part of the port of Star Trek: Voyager -- Elite Force II, whose
engine descends from id Tech 3 through OpenMoHAA.

It is free software; you can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU General Public License, COPYING.txt at the repository
root, for more details.
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later
// Modified 2026-10-04 for this port; the changes are listed in PROVENANCE.tsv.
/*
===========================================================================
This file is part of the port of Star Trek: Voyager -- Elite Force II, whose
engine descends from id Tech 3 through OpenMoHAA.

It is free software; you can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU General Public License, COPYING.txt at the repository
root, for more details.
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * tr_backend.c -- the render-thread half of the frame.
 *
 * The front end records a frame's surfaces into `tess` exactly as the reference
 * twin's front end does; this module's back end walks them onto the frame's
 * command buffer.  One surface is one stage pass: the state tuple becomes a
 * pipeline, the bundle a descriptor bind, the tess arrays a slice of the
 * vertex ring, the indexes an indexed draw.  Nothing here touches a driver
 * object -- the reference renderer's per-state calls all become fields of the
 * pipeline and the bind that already carry them.
 *
 * The 3D stage iterators (sky, lightmapped multitexture, vertex-lit, the
 * legacy fog pass) are the next milestone's surface; the 2D/HUD iterator below
 * is the walk the menu paints through.
 */

#include "tr_local.h"
#include "tr_program.h"
#include "tr_vbo.h"

#include <string.h>
#include <math.h>

/* defined by the mirrored 3D front end */
backEndState_t		backEnd;
int					r_sequencenumber = 1;
glconfig_t			vkConfig;

/* The slot the walk records into, -1 when none: the frame layer writes it. */
int					vk_back_end_slot = -1;
static int			s_rbSlot = -1;

void RB_InitBackend( void )
{
	memset( &tess, 0, sizeof( tess ) );
	memset( &backEnd, 0, sizeof( backEnd ) );

	backEnd.color2D[0] = 255;
	backEnd.color2D[1] = 255;
	backEnd.color2D[2] = 255;
	backEnd.color2D[3] = 255;
}

/* the tessellation store lives in tr_shade.c, as in the reference renderer */


/* ---- the 2D coverage census ---------------------------------------------- */
/* "issued" and "rasterised" are different claims and the scissor is what makes the gap: the census reads the rect beside the draw and
   reports quad INTERSECT scissor INTERSECT render area (px=0 proves no raster) plus the three rects that say where the state came from.
   vk_2dCover 2 adds a readback cross-check over the presented frame.  Gate: vk_2dCover, default 0. */

/* Declared here rather than in tr_local.h: the accessors live in vk_draw.c and tr_backend.c is their only consumer. */
void	 VK_Draw_LastScissorRect( int out[4] );
qboolean VK_Draw_LastScissorValid( void );
qboolean VK_Draw_2DAppliedGet( int *win, int *scn );
int		 VK_Draw_LastSetIndex( void );
int		 VK_Draw_LastAlphaTest( void );
qboolean VK_ModelPixelAtSwap( int x, int y, byte out[4] );

/* A 2D batch that crossed a window or scissor change draws once per span, each
   draw preceded by the state its quads were authored under.  Under two spans
   this is the single draw the batch always made: nothing about a run that sets
   one window changes.  The vertices are packed once for the whole batch, so a
   span only moves the first index and re-pushes the ortho and the two rects. */
static void RB_StageDrawSpan( VkCommandBuffer cmd, int indexBase, int vertexBase, int numIndexes,
							  VkPipeline pipeline, const shaderStage_t *stage, const image_t *image )
{
	int	spans = RB_2D_SpanCount();
	int	i;

	/* The draw reads vertexBase[0..numVertexes); if the ring there is not the
	   tess the row predicted, the quads land wherever the stale slice says. */
	if ( spans < 2 ) {
		/* One draw for the whole batch, under whatever state the command buffer
		   holds: for a 2D batch that is the arm which cannot say what its quads
		   were authored under (tr_draw.c RB_2D_SpanCount reports a single span as
		   two, so a one-state batch never lands here). */
		VK_Draw_DrawIndexed( cmd, numIndexes, indexBase, vertexBase );
		return;
	}

	for ( i = 0; i < spans; i++ ) {
		float	ortho[16];
		int		win[4], scn[4];
		int		first = 0, count = 0;

		RB_2D_SpanGet( i, &first, &count, ortho, win, scn );
		if ( count <= 0 ) {
			continue;
		}

		VK_Draw_2DStateApply( cmd, ortho, win, scn );

		VK_Draw_DrawIndexed( cmd, count, indexBase + first, vertexBase );
		/* One entry per span: the order transcript keeps the granularity it had
		   when each window ended the surface. */
	}
}


/* ---- the stage pass: one stage, one pipeline, one bind, one indexed draw;
 * the state tuple carries blend, depth, cull and alpha test. */

static qboolean RB_StagePass( shaderStage_t *stage, const color4ub_t *colors )
{
	vkPipelineKey_t key;
	VkPipeline pipeline;
	VkImageView view;
	VkSampler sampler;
	vkTessVertex_t *ringVerts;
	vkIndex_t *ringIndexes;
	image_t *image;
	int vertexBase = 0;
	int indexBase = 0;
	int i;
	qboolean clamp;
	qboolean mipmap;

	if ( !stage->active ) {
		return qtrue;
	}

	image = stage->bundle[0].image[0];
	if ( !image ) {
		VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "stage without an image", VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	/* 0 is "no texture" here too: the first slot is a real image */
	if ( image->texnum == 0 ) {
		return qfalse;
	}

	/* the 2D half of the stamp witness: the bind the world census cannot see and the path every menu material and font takes, anomaly-only */

	view = VK_Texture_View( (int)image->texnum );
	if ( view == VK_NULL_HANDLE ) {
		VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "image without a Vulkan view", VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	/* The 2D binding row: the quad's shader, the view the slot resolves to, and
	   the rect the geometry carries.  A tile sampling the wrong art names itself
	   here as shader X with image Y.  Everything in the block exists for the row
	   and for the order transcript, which gates itself, so a quiet boot pays
	   neither. */
	memset( &key, 0, sizeof( key ) );
	key.topology = VK_IMM_TRIANGLES;
	key.stateBits = stage->stateBits;
	/* The 2D window is two-sided, as the reference renderer's RB_2D_SetWindow is. */
	key.cull = ( backEnd.in2D || tess.shader->lightmapIndex == LIGHTMAP_2D )
			   ? CT_TWO_SIDED : tess.shader->cullType;
	/* The reference's RB_2D_SetWindow turns depth off for the whole UI pass,
	   whatever a UI shader's own state bits say. */
	key.depthTest = ( stage->stateBits & VKS_DEPTHTEST_DISABLE ) ? qfalse : qtrue;
	if ( backEnd.in2D && VK_DepthGate2D() ) {
		key.depthTest = qfalse;
	}
	key.depthWrite = ( stage->stateBits & VKS_DEPTHMASK_TRUE ) ? qtrue : qfalse;
	VK_Shade_ShadowState( &key.stencilTest, &key.stencilFunc, &key.stencilRef,
						  &key.stencilMask, &key.stencilOpFail, &key.stencilOpZFail,
						  &key.stencilOpPass, &key.cullFace, &key.colorMask );

	/* A 2D batch that loses its two-sided or depth-off state culls or z-fails
	   every fragment and reads as text that never rasterised. */
	pipeline = VK_Draw_Pipeline( &key );
	if ( pipeline == VK_NULL_HANDLE ) {
		return qfalse;
	}

	VK_Draw_BindPipeline( vk.commandBuffers[s_rbSlot], pipeline );
	VK_Draw_SetAlphaTest( vk.commandBuffers[s_rbSlot], stage->stateBits );

	/* wrapClampModeX/Y hold the legacy wrap enum (REPEAT is 10497, not 0) and come from `clampmap', which the GLES3 twin's whole 2D pass samples by;
	   testing only IMGFLAG_CLAMPTOEDGE left every 2D clampmap stage on REPEAT, so a linear-sampled panel bled its opposite edge into the border.
	   clampTime is not a wrap: it is a tcMod clock and no shipped shader sets it. */
	clamp = ( image->flags & IMGFLAG_CLAMPTOEDGE )
			|| ( image->wrapClampModeX != VK_TWRAP_REPEAT )
			|| ( image->wrapClampModeY != VK_TWRAP_REPEAT ) ? qtrue : qfalse;
	mipmap = ( image->numMipmaps > 1 ) ? qtrue : qfalse;

	if ( !VK_Texture_Sampler( clamp, mipmap, qtrue, &sampler ) ) {
		return qfalse;
	}

	if ( !VK_Draw_BindTexture( vk.commandBuffers[s_rbSlot], view, sampler ) ) {
		return qfalse;
	}

	/* xyz, st and colour are the surface's own; which colour array that is
	   depends on the stage's gens -- RB_StageIterator2D resolved the ones the
	   tess pipeline cannot express into svars.colors and hands that array in,
	   the vertex-driven ones keep the quad's own vertexColors. */
	if ( stage && VK_VertexRing_StreamMatch( tess.numVertexes, tess.numIndexes,
											 tess.texCoords[0][0], tess.texCoords[0][0], 0,
											 colors, NULL,
											 &vertexBase, &indexBase ) ) {
		VK_Draw_BindRing( vk.commandBuffers[s_rbSlot] );
		RB_StageDrawSpan( vk.commandBuffers[s_rbSlot], indexBase, vertexBase, tess.numIndexes,
						  pipeline, stage, image );
		return qtrue;
	}

	/* Both halves are reserved before a vertex is written: a surface whose index
	   half does not fit is skipped whole, and reserving the vertex half first
	   left its written vertices stranded in the ring on that path. */
	ringIndexes = VK_VertexRing_ReserveIndexes( tess.numIndexes, &indexBase );
	if ( !ringIndexes ) {
		return qfalse;
	}

	ringVerts = VK_VertexRing_Reserve( tess.numVertexes, &vertexBase );
	if ( !ringVerts ) {
		VK_VertexRing_RollBack( 0, tess.numIndexes );
		return qfalse;
	}

	for ( i = 0; i < tess.numVertexes; i++ ) {
		ringVerts[i].xyz[0] = tess.xyz[i][0];
		ringVerts[i].xyz[1] = tess.xyz[i][1];
		ringVerts[i].xyz[2] = tess.xyz[i][2];
		ringVerts[i].st[0] = tess.texCoords[i][0][0];
		ringVerts[i].st[1] = tess.texCoords[i][0][1];

		if ( colors == tess.vertexColors && !tess.vertexColorValid ) {
			ringVerts[i].color[0] = 254;
			ringVerts[i].color[1] = 254;
			ringVerts[i].color[2] = 254;
			ringVerts[i].color[3] = 254;
		} else {
			ringVerts[i].color[0] = colors[i][0];
			ringVerts[i].color[1] = colors[i][1];
			ringVerts[i].color[2] = colors[i][2];
			ringVerts[i].color[3] = colors[i][3];
		}
	}

	memcpy( ringIndexes, tess.indexes, (size_t)tess.numIndexes * sizeof( vkIndex_t ) );

	if ( stage ) {
		VK_VertexRing_StreamStore( tess.numVertexes, tess.numIndexes,
								   tess.texCoords[0][0], tess.texCoords[0][0], 0,
								   colors, NULL, vertexBase, indexBase );
	}

	/* The ring is bound per surface: a bind is cheaper than tracking which. */
	VK_Draw_BindRing( vk.commandBuffers[s_rbSlot] );
	RB_StageDrawSpan( vk.commandBuffers[s_rbSlot], indexBase, vertexBase, tess.numIndexes,
					  pipeline, stage, image );

	vk_ring_note( vertexBase + tess.numVertexes, indexBase + tess.numIndexes );

	return qtrue;
}

/* The generic iterator: one pass per active stage.  A 3D term is the 3D
   surface's work and is named by its guard. */


void rb_set_slot( int slot )
{
	s_rbSlot = slot;
}

/* The frame's back-end state is reset where the frame is walked, not where it
   is recorded.  RE_BeginFrame used to do this on the front end, which with
   r_smp 1 rewrote the ring, the shade census and the 2D window cache while the
   render thread was still walking the previous frame through them. */
void RB_FrameOpen( int slot )
{

	VK_VertexRing_SetFrame( slot );	/* the ring this frame fills */
	VK_VertexRing_ResetFrame();
	RB_VBO_BeginFrame();
	VK_Shade_NewFrame();
	RB_2D_SpanReset();
	s_rbSlot = -1;

	/* no 2D window/scissor invalidation here: that cache belongs to the front end's recording stream and this runs on the walk thread for a
	   frame the front end recorded earlier -- dropping it here raced the front end's recording of the NEXT frame.  The front end drops it at
	   its own frame boundary (RE_EndFrame) and after every 3D command. */
}

qboolean RB_RecordSurfaces( VkCommandBuffer cmd )
{
	return qtrue;
}

void RB_FramePrologue( void )
{
}

/* ---- the 2D immediate stream: a run is one pipeline and one ring slice -- */


/* the marker surface's own screen rect, in the caller view's window space; the
   answer is Vulkan's top-left space, so it compares with the scissor directly */
/* ---- the 2D immediate stream and the surf record ------------------------ */

/* ---- the 2D immediate stream: a run is one pipeline and one ring slice -- */

typedef struct {
	qboolean	open;
	int			topology;
	int			verts;
	int			base;
	qboolean	pending;
	image_t		*image;
	VkPipeline	pipeline;
	unsigned	shadowGen;	/* the stencil/cull/colour-mask state the run opened under */
	qboolean	shadow;		/* the run armed the stencil: a volume pass */
	qboolean	coverClaim;	/* this run's first quad covers the target opaquely */
} vkImmRun_t;

static vkImmRun_t	s_imm;

/* The volume's own commits: `imm` in the frame row counts the 2D batches only,
   so without these two numbers "the volume reached the command buffer" had no
   witness at all. */
static int	s_immShadowRuns;
static int	s_immShadowVerts;

/* the key the last immediate runs were opened with: the volume row reads the
   pipeline state that was actually built, not the state word it meant */
static vkPipelineKey_t	s_immKey[2];
static unsigned			s_immKeySeq;

qboolean VK_Imm_KeyGet( int back, vkPipelineKey_t *out ) {
	if ( !out || s_immKeySeq <= (unsigned)back ) {
		return qfalse;
	}
	*out = s_immKey[ ( s_immKeySeq - 1 - (unsigned)back ) & 1u ];
	return qtrue;
}

void VK_Imm_ShadowCommits( int *runs, int *verts ) {
	if ( runs ) {
		*runs = s_immShadowRuns;
	}
	if ( verts ) {
		*verts = s_immShadowVerts;
	}
}

/* the matrix the open run pushed: a row that reports where the run's geometry
   lands has to read the transform the draw actually carries */
static float		s_immPush[16];

static float		s_immCurSt[2];	/* the stream's current st, stamped per vertex */
static byte			s_immCurColor[4] = { 255, 255, 255, 255 };

const float *RB_ImmPushedMatrix( void )
{
	return s_immPush;
}

static vkTessVertex_t *RB_Imm_NewVertex( void )
{
	vkTessVertex_t	*v;

	v = VK_VertexRing_Reserve( 1, NULL );
	if ( !v ) {
		return NULL;
	}

	v->xyz[2]   = 0.0f;
	v->st[0]    = s_immCurSt[0];
	v->st[1]    = s_immCurSt[1];
	memcpy( v->color, s_immCurColor, 4 );

	s_imm.verts++;
	s_imm.pending = qtrue;

	return v;
}

int RB_ImmStreamVerts( void )
{
	return s_imm.open ? ( s_imm.base + s_imm.verts ) : VK_VertexRing_Used();
}

static qboolean RB_Imm_BindImage( image_t *image )
{
	VkImageView view;
	VkSampler sampler;

	if ( !image ) {
		VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "immediate run without an image",
					  VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	view = VK_Texture_View( (int)image->texnum );
	if ( view == VK_NULL_HANDLE ) {
		VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "immediate run image without a view",
					  VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	if ( !VK_Texture_Sampler( qtrue, image->numMipmaps > 1, qfalse, &sampler ) ) {
		return qfalse;
	}

	return VK_Draw_BindTexture( vk.commandBuffers[s_rbSlot], view, sampler );
}

/* A 2D writer whose first quad proves it covers the whole target, set for the
   run it is about to open: the pass colour clear is then owed to that quad,
   which writes every texel it would have cleared (RB_2D_Box).  Read where the
   run's pipeline is resolved, because that is where the run's own state -- the
   colour mask, the stencil, the blend -- is known. */
static qboolean s_immCoverClaim;

void RB_ImmCoverClaim( qboolean on ) {
	s_immCoverClaim = on;
}

/* The run's quads are in the ring or they are not: a vertex the ring could not
   take leaves the covering quad incomplete, and a clear claimed by it would
   leave the texels it never reached undefined. */
void RB_ImmCoverCancel( void ) {
	s_imm.coverClaim = qfalse;
}

/* The run's pipeline state is the caller's, not a field of the run. */
void RB_ImmBeginWithImage( int topology, image_t *image, unsigned long stateBits )
{
	vkPipelineKey_t key;
	vkImmRun_t *run = &s_imm;
	qboolean coverClaim = s_immCoverClaim;

	s_immCoverClaim = qfalse;

	if ( run->open && ( run->topology != topology || run->image != image
						|| run->shadowGen != VK_Shade_ShadowGen() ) ) {
		RB_ImmEnd();
	}

	memset( &key, 0, sizeof( key ) );
	key.topology = topology;
	key.stateBits = stateBits;
	key.cull = CT_TWO_SIDED;
	key.depthTest = ( stateBits & VKS_DEPTHTEST_DISABLE ) ? qfalse : qtrue;
	key.depthWrite = ( stateBits & VKS_DEPTHMASK_TRUE ) ? qtrue : qfalse;
	/* the shadow volume's arm: the stencil test, the ops it counts with, the
	   cull face it flips between its two volumes and the colour it never writes */
	VK_Shade_ShadowState( &key.stencilTest, &key.stencilFunc, &key.stencilRef,
						  &key.stencilMask, &key.stencilOpFail, &key.stencilOpZFail,
						  &key.stencilOpPass, &key.cullFace, &key.colorMask );

	/* the ortho is recorded on every run, at function scope: a run that trusted the previous run's push drew with a stale ortho, and a 2D quad
	   under a stale ortho lands as a garbage sheet across the frame.  Which ortho it carries depends on the space the run draws in; the recording itself is unconditional. */
	struct { float ortho[16]; float alphaTest[4]; } push;	/* vk_draw.c's layout */
	const float *window;

	memset( &push, 0, sizeof( push ) );
	window = backEnd.in2D ? VK_Draw_CurrentOrtho() : NULL;
	if ( window ) {
		memcpy( push.ortho, window, sizeof( push.ortho ) );
	} else {
		/* column-major proj*view, the same product the stage pass uploads as u_MVP: the shader reads this push as a mat4, so a transposed
		   product put every 3D immediate run -- the volume and this overlay's own quad -- in a space none of its vertices survive. */
		VK_Mat4Mul( push.ortho, RB_GetProjectionMatrix(), RB_GetModelViewMatrix() );
		/* the engine's matrices leave +y up in clip space; row 1 is flat 1, 5, 9, 13 */
		VK_Mat4_ClipToWindow( push.ortho );
	}
	memcpy( s_immPush, push.ortho, sizeof( s_immPush ) );
	push.alphaTest[0] = 0.0f;
	push.alphaTest[1] = 0.0f;

	qvk.CmdPushConstants( vk.commandBuffers[s_rbSlot], VK_Draw_Layout(),
						  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
						  0, sizeof( push ), &push );

	/* The claim only stands for a run that writes colour opaquely over its whole
	   quad: two-sided, no depth test, no stencil, every channel of the colour
	   mask, and SRC_ALPHA/ONE_MINUS_SRC_ALPHA, whose alpha-1 result is the source
	   colour exactly.  The claim rides the run and is taken at its commit, where
	   the draw is certain; anything else pays the clear the pass would have done,
	   recorded here, before the run's draw. */
	coverClaim = ( coverClaim
				   && key.colorMask == ( VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
										 | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT )
				   && !key.stencilTest && !key.depthTest
				   && ( key.stateBits & ( VKS_SRCBLEND_BITS | VKS_DSTBLEND_BITS ) )
					  == ( VKS_SRCBLEND_SRC_ALPHA | VKS_DSTBLEND_ONE_MINUS_SRC_ALPHA ) )
				 ? qtrue : qfalse;

	if ( !coverClaim ) {
		VK_DeferredClear_Flush( vk.commandBuffers[s_rbSlot] );
	}

	run->pipeline = VK_Draw_Pipeline( &key );
	if ( run->pipeline == VK_NULL_HANDLE ) {
		return;
	}

	s_immKey[s_immKeySeq & 1u] = key;
	s_immKeySeq++;
	run->shadow = key.stencilTest;

	VK_Draw_BindPipeline( vk.commandBuffers[s_rbSlot], run->pipeline );
	/* the run's own state, as the stage pass does: a leftover push would arm the
	   previous surface's test on this one */
	VK_Draw_SetAlphaTest( vk.commandBuffers[s_rbSlot], stateBits );

	if ( !RB_Imm_BindImage( image ) ) {
		return;
	}

	run->open = qtrue;
	run->topology = topology;
	run->image = image;
	run->shadowGen = VK_Shade_ShadowGen();
	run->coverClaim = coverClaim;
	run->verts = 0;
	run->base = VK_VertexRing_Used();
}

void RB_ImmBegin( int topology )
{
	RB_ImmBeginWithImage( topology, tr.whiteImage, vk_Shade_StateBits() );
}

void RB_ImmEndDeferred( void )
{
}

/* a run reaches the ring whole and a commit consumes exactly the slice it drew, so a second commit draws what came after the first, not the
   run again -- re-issuing the run put every earlier quad back on screen after any 3D batch that ran in between. */
void RB_ImmCommit( void )
{
	vkImmRun_t *run = &s_imm;
	VkCommandBuffer cmd;

	if ( !run->open || run->verts == 0 ) {
		return;
	}

	cmd = vk.commandBuffers[s_rbSlot];
	if ( cmd == VK_NULL_HANDLE ) {
		return;
	}

	if ( run->topology == VK_IMM_LINE_LOOP ) {
		/* No line-loop topology in Vulkan: repeat the first vertex to close it. */
		vkTessVertex_t *first = VK_VertexRing_Tail( run->verts );
		vkTessVertex_t *copy;

		if ( first ) {
			copy = VK_VertexRing_Reserve( 1, NULL );
			if ( copy ) {
				*copy = *first;
				run->verts++;
			}
		}
	}

	VK_Draw_BindRing( cmd );

	/* the run's first quad covers every texel the pass clear would have written,
	   so the clear it would have read is not owed at all (RB_2D_Box's proof) */
	if ( run->coverClaim ) {
		VK_DeferredClear_Claim();
		run->coverClaim = qfalse;
	}

	VK_Draw_Draw( cmd, run->verts, run->base );

	if ( run->shadow ) {
		s_immShadowRuns++;
		s_immShadowVerts += run->verts;
	}

	backEnd.pc.c_vertexes += run->verts;
	backEnd.pc.c_surfaces++;

	run->base += run->verts;
	run->verts = 0;
	run->pending = qfalse;
}

void RB_ImmEnd( void )
{
	if ( s_imm.pending ) {
		RB_ImmCommit();
	}

	s_imm.coverClaim = qfalse;
	s_imm.open = qfalse;
	s_imm.verts = 0;
	s_imm.pending = qfalse;
	s_imm.image = NULL;
}

void RB_ImmFlush( void )
{
	RB_ImmEnd();
}

void RB_ImmVertex2f( float x, float y )
{
	vkTessVertex_t *v = RB_Imm_NewVertex();

	if ( !v ) {
		return;
	}

	v->xyz[0] = x;
	v->xyz[1] = y;
	RB_2DPlaceVertex( v->xyz );
}

void RB_ImmColor4f( float r, float g, float b, float a )
{
	/* the stream's current colour: the vertices after it carry it */
	s_immCurColor[0] = (byte)( r * 255.0f + 0.5f );
	s_immCurColor[1] = (byte)( g * 255.0f + 0.5f );
	s_immCurColor[2] = (byte)( b * 255.0f + 0.5f );
	s_immCurColor[3] = (byte)( a * 255.0f + 0.5f );
}

void RB_ImmTexCoord2f( float s, float t )
{
	s_immCurSt[0] = s;
	s_immCurSt[1] = t;
}

void RB_ShowImages( qboolean quiet )
{
	( void )quiet;
}

qboolean RB_SmpActive( void )
{
	return vk.smpActive;
}

/* ---- the immediate stream's remaining surface: the mirrored 3D front end
   calls the whole reference set, so the 2D stream carries the rest ---------- */

void RB_ImmVertex3f( float x, float y, float z )
{
	vkTessVertex_t *v = RB_Imm_NewVertex();

	if ( !v ) {
		return;
	}

	v->xyz[0] = x;
	v->xyz[1] = y;
	v->xyz[2] = z;
	RB_2DPlaceVertex( v->xyz );
}

void RB_ImmVertex3fv( const float *pos )
{
	RB_ImmVertex3f( pos[0], pos[1], pos[2] );
}

void RB_ImmColor3f( float r, float g, float b )
{
	RB_ImmColor4f( r, g, b, 1.0f );
}

void RB_ImmTexCoord2fv( const float *st )
{
	s_immCurSt[0] = st[0];
	s_immCurSt[1] = st[1];
}

/* ---- the data pool, the state block and the recorders ---- */

backEndData_t	*backEndData;

backEndData_t	*backEndDataBuf[SMP_FRAMES];

vkstate_t	vkState;

int		max_polys;

int		max_polyverts;

/* ---- the surf record's own list ------------------------------------------ */

/* The record has to own its surfaces.  cmd->drawSurfs used to be a raw pointer
   into tr.refdef.drawSurfs, which the front end rewrites for a later frame while
   the walk reads this record; a walk that meets the refilled array meets it in
   add order, and there a caster's <stencil shadow> surf comes before the surfaces
   its volume has to be depth-tested against.  Both volume walls then pass, the
   counters cancel, and the stencil plane reads empty with every other reading in
   the frame healthy. */
static drawSurfsCommand_t *R_AllocSurfCmd( int headBytes, int numSurfs ) {
	int					listBytes = ( numSurfs > 0 ) ? numSurfs * (int)sizeof( drawSurf_t ) : 0;
	int					bytes = headBytes + listBytes + (int)sizeof( void * );
	drawSurfsCommand_t	*cmd = (drawSurfsCommand_t *)R_GetCommandBuffer( bytes );
	int					pad = (int)sizeof( void * );

	/* The list advances a command by its own size, and this record keeps its
	   surfaces behind the struct: without the padded total the walk read the
	   slice as commands and every record after it -- the whole 2D screen -- was
	   lost.  The allocator pads its request to pointer size, so the total does
	   too. */
	if ( cmd ) {
		cmd->totalBytes = ( bytes + pad - 1 ) & ~( pad - 1 );
	}
	return cmd;
}

static void R_CarryDrawSurfs( drawSurfsCommand_t *cmd, drawSurf_t *surfs, int numSurfs ) {
	unsigned long	addr, align, pad;
	byte			*base;

	if ( numSurfs <= 0 || surfs == NULL ) {
		cmd->drawSurfs = NULL;
		return;
	}

	base  = (byte *)cmd + sizeof( *cmd );
	addr  = (unsigned long)base;
	align = (unsigned long)sizeof( void * );
	pad   = ( align - ( addr % align ) ) % align;

	cmd->drawSurfs = (drawSurf_t *)( base + pad );
	Com_Memcpy( cmd->drawSurfs, surfs, numSurfs * sizeof( drawSurf_t ) );
}

void	R_AddDrawSurfCmd( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	drawSurfsCommand_t	*cmd;

	/* A 3D view that reached the command list with no surfaces renders nothing
	   but the pass clear, which is exactly "the whole 3D viewport flashed".  A
	   scene that carried entities but produced no surfaces is the anomaly worth
	   naming; an empty scene is a legitimate NOWORLDMODEL-free view. */

	R_Invalidate2DWindowCache();

	cmd = R_AllocSurfCmd( sizeof( *cmd ), numDrawSurfs );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_DRAW_SURFS;

	if ( r_surfOwn && r_surfOwn->integer ) {
		R_CarryDrawSurfs( cmd, drawSurfs, numDrawSurfs );
	} else {
		/* the record names the live array again, which is only ever safe when the
		   front end cannot refill it before the walk: the switch exists so the two
		   behaviours can be compared on one boot */
		cmd->drawSurfs = drawSurfs;
	}
	cmd->numDrawSurfs = numDrawSurfs;

	cmd->refdef = tr.refdef;
	/* the record's own list is the one the walk reads, so the refdef points at it */
	cmd->refdef.drawSurfs = cmd->drawSurfs;
	cmd->viewParms = tr.viewParms;

	/* the record owns a copy of THIS view's slice while the refdef it carries
	   counts the whole frame: a UI shader that never reaches the walk is either
	   absent from the copy or added after it, and only these numbers say which */

}

void	R_AddSpriteSurfCmd(drawSurf_t* drawSurfs, int numDrawSurfs) {
    drawSurfsCommand_t* cmd;

    R_Invalidate2DWindowCache();

    cmd = R_AllocSurfCmd( sizeof( *cmd ), numDrawSurfs );
    if (!cmd) {
        return;
    }
    cmd->commandId = RC_SPRITE_SURFS;

    if ( r_surfOwn && r_surfOwn->integer ) {
        R_CarryDrawSurfs( cmd, drawSurfs, numDrawSurfs );
    } else {
        cmd->drawSurfs = drawSurfs;
    }
    cmd->numDrawSurfs = numDrawSurfs;

    cmd->refdef = tr.refdef;
    cmd->refdef.spriteSurfs = cmd->drawSurfs;
    cmd->viewParms = tr.viewParms;
}

static void SetViewportAndScissor( void ) {
	RB_LoadProjectionMatrix( backEnd.viewParms.projectionMatrix );

	/* The view's own rect, through the same clamp the 2D window takes: a view
	   whose rect lands off the framebuffer is undefined rasterization in
	   Vulkan, where GL cut it for free. */
	{
		VkCommandBuffer drawCmd = VK_Shade_CommandBuffer();

		VK_Shade_Viewport3D( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
							 backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
		VK_Draw_View3D( drawCmd, backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
						backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
	}
}

static void RB_Hyperspace( void ) {
	float c;

	c = ( backEnd.refdef.time & 255 ) / 255.0f;

	/* hyperspace is the grey the next view's clear paints */
	vk.clearColor[0] = c;
	vk.clearColor[1] = c;
	vk.clearColor[2] = c;
	vk.clearColor[3] = 1.0f;

	backEnd.isHyperspace = qtrue;
}

void RB_BeginDrawingView (void) {

	RB_2D_Flush();

	backEnd.in2D = qfalse;

	/* No R_Invalidate2DWindowCache here: the cache is the recorder's, and the walk
	   writing it made a string re-open the screen window over its widget's own. */

	SetViewportAndScissor();

	/* the rect this view is given and the scissor its draws will run under, read
	   where the backend has just issued both */
	VK_DepthState( qtrue, qtrue );	/* the 3D pass opens its gate, as retail's does */
	VK_StencilState( qfalse, VK_ALWAYS, 0, 255 );	/* retail's traced menu issues no stencil call */
	VK_State( VKS_DEFAULT );

	/* the colour is this view's choice, and rides on the pass's clear */
	if ( !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		if ( !backEnd.viewParms.farplane_distance || tr.skyRendered || tr.portalRendered )
		{
			if ( r_fastsky->integer )
			{
				vk.clearColor[0] = 0.0f;
				vk.clearColor[1] = 0.0f;
				vk.clearColor[2] = 0.0f;
				vk.clearColor[3] = 1.0f;
			}
		}
		else
		{
			vk.clearColor[0] = vkState.fFogColor[0];
			vk.clearColor[1] = vkState.fFogColor[1];
			vk.clearColor[2] = vkState.fFogColor[2];
			vk.clearColor[3] = 0.0f;
		}

		/* r_deferredClear: while nothing has written colour this frame the pass
		   clear is still owed, and it has to be paid with THIS view's colour.  The
		   value captured at RC_VK_BEGIN is the previous frame's last view, so a
		   fogged mission flushed it as black even after vkState.fFogColor was
		   published.  A view that has already painted leaves the debt settled and
		   this copy unused. */
		if ( vk.colorClearOwed )
		{
			vk.deferredClearColor[0] = vk.clearColor[0];
			vk.deferredClearColor[1] = vk.clearColor[1];
			vk.deferredClearColor[2] = vk.clearColor[2];
			vk.deferredClearColor[3] = vk.clearColor[3];
		}
	}

	VK_DepthFuncApply( VK_LEQUAL );	/* pinned per view for the A/B cvar */

	/* Retail's traced menu frame clears DEPTH and nothing else: the view builds no volume, and colour
	   joins only for a world view's fastsky or fog. Colour on the UI's view erased the 2D painted
	   ahead of it; no depth left a later view testing against the first one's depth. */
	{
		VkClearAttachment attachments[2];
		VkClearRect rect;
		qboolean colour = qfalse;	/* this view clears the colour plane too */
		int count = 0;

		memset( attachments, 0, sizeof( attachments ) );
		attachments[count].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		/* RB_BeginDrawingView @0x1490 clears 1280 = DEPTH_BUFFER_BIT|STENCIL_BUFFER_BIT when the cvar at off_227CA0 is
		   nonzero OR the one at off_227C9C reads 2, the same pointer RB_ShadowFinish @0x40BD0 gates the overlay on, i.e.
		   the precise shadow tier.  The first term is r_measureOverdraw's readback, which sums the plane every swap.
		   Without the stencil half every view in a frame shared one plane, so a sub-view's caster marked the main
		   view's screen. */
		if ( ( r_measureOverdraw->integer || r_shadows->integer == 2 ) && vkConfig.stencilBits >= 4 ) {
			attachments[count].aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		attachments[count].clearValue.depthStencil.depth = 1.0f;
		attachments[count].clearValue.depthStencil.stencil = 0;
		count++;

		if ( ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) == 0 )
		{
			if ( !backEnd.viewParms.farplane_distance || tr.skyRendered || tr.portalRendered )
			{
				if ( r_fastsky->integer )
				{
					colour = qtrue;
				}
			}
			else
			{
				colour = qtrue;
			}

			if ( colour ) {
				attachments[count].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				attachments[count].clearValue.color.float32[0] = vk.clearColor[0];
				attachments[count].clearValue.color.float32[1] = vk.clearColor[1];
				attachments[count].clearValue.color.float32[2] = vk.clearColor[2];
				attachments[count].clearValue.color.float32[3] = vk.clearColor[3];
				count++;
			}
		}

		/* The pass's own LOAD_OP_CLEAR already wrote 1.0 into every depth texel
		   this frame, and nothing that can write depth has been recorded since:
		   re-clearing the same value over a rect the pass cleared is a no-op, so
		   the depth aspect is dropped.  Only the depth bit goes -- the stencil
		   plane is written by the volume pass, which nothing here counts. */
		if ( vk.depthWritten == qfalse ) {
			attachments[0].aspectMask &= (uint32_t)~(uint32_t)VK_IMAGE_ASPECT_DEPTH_BIT;

			if ( attachments[0].aspectMask == 0 ) {
				attachments[0] = attachments[1];
				count--;
			}
		}

		/* The clear rect is the view's own, flipped to the framebuffer's origin
		   and clipped to the render area: a rect past the pass is a violation. */
		{
			int rx = backEnd.viewParms.viewportX;
			int ry = vkConfig.vidHeight - backEnd.viewParms.viewportY
					 - backEnd.viewParms.viewportHeight;
			int rw = backEnd.viewParms.viewportWidth;
			int rh = backEnd.viewParms.viewportHeight;
			int ex = (int)vk.swapchainExtent.width;
			int ey = (int)vk.swapchainExtent.height;

			if ( rx < 0 ) { rw += rx; rx = 0; }
			if ( ry < 0 ) { rh += ry; ry = 0; }
			if ( rx + rw > ex ) { rw = ex - rx; }
			if ( ry + rh > ey ) { rh = ey - ry; }

			rect.rect.offset.x = rx;
			rect.rect.offset.y = ry;
			rect.rect.extent.width  = (uint32_t)( rw > 0 ? rw : 0 );
			rect.rect.extent.height = (uint32_t)( rh > 0 ? rh : 0 );
			rect.baseArrayLayer = 0;
			rect.layerCount = 1;
		}

		/* The pass's colour clear (r_deferredClear) is still owed while nothing
		   has written colour this frame.  A view clear that covers the pass's
		   whole render area writes every texel the pass clear would have, so it
		   settles the debt; any narrower write cannot, so the pass clear is
		   recorded first, in the value the pass would have written. */
		if ( vk.colorClearOwed ) {
			if ( colour
				 && rect.rect.offset.x == 0 && rect.rect.offset.y == 0
				 && rect.rect.extent.width == vk.swapchainExtent.width
				 && rect.rect.extent.height == vk.swapchainExtent.height ) {
				VK_DeferredClear_Claim();
			} else {
				VK_DeferredClear_Flush( VK_Shade_CommandBuffer() );
			}
		}

		if ( count > 0 && rect.rect.extent.width && rect.rect.extent.height ) {
			qvk.CmdClearAttachments( VK_Shade_CommandBuffer(), count, attachments, 1, &rect );
		}
	}

	vkState.faceCulling = -1;

	backEnd.skyRenderedThisView = qfalse;

	/* RB_BeginDrawingView @0x1490 gates the user plane on the dword 16 bytes before
	   portalPlane, which in this struct is isMirror -- not isPortal.  Arming it for a
	   non-mirror portal (every viewscreen row in the log reads mirror=0) hands the
	   fragment stage an eye-space plane that discards one half of the panel. */
	if ( backEnd.viewParms.isMirror ) {
		float	plane[4];
		float	plane2[4];

		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		plane2[0] = DotProduct (backEnd.viewParms.ori.axis[0], plane);
		plane2[1] = DotProduct (backEnd.viewParms.ori.axis[1], plane);
		plane2[2] = DotProduct (backEnd.viewParms.ori.axis[2], plane);
		plane2[3] = DotProduct (plane, backEnd.viewParms.ori.origin) - plane[3];

		RB_ClipPlane_Set( plane2 );
	} else {
		RB_ClipPlane_Disable();
	}

	if ( ( backEnd.refdef.rdflags & RDF_HYPERSPACE ) )
	{
		RB_Hyperspace();
	}
	else
	{
		backEnd.isHyperspace = qfalse;
	}
}

/* One shade part, net of the binds its own flush issued (RB_BeginSurface and
   RB_EndSurface reach the stage iterator); returned so OTHER is the remainder. */

/* Close the shade window: charge the residual to OTHER so the parts sum to the
   window, then charge the window to the walk bucket. */
/* the stage-0 image a draw will sample: a wrong bind names both the image and the slot's tenant */
/* the sprite record's own shader, as its model still holds it at draw time */
/* a vertex-colour stage whose stream is pure magenta is the rollback arm, not an authored fade */

/* retail resolves the index and drops nothing: a stale record takes the live slot */
shader_t *RB_ResolveStaleShader( const char *walk, int gen, int index )
{
	
	if ( index < 0 || index >= tr.numShaders ) {
		return NULL;
	}

	return tr.sortedShaders[ index ];
}

/* names a draw whose carried index no longer matches its shader's live sortedIndex */
/* ---- where in the frame the volume is drawn ------------------------------ */

/* A z-pass volume is only correct after its receivers, and "drawn after" is not
   something the sort word proves: it packs sortedIndex at add time.  The frame's
   own draw counter is the witness -- the first draws by name, the counter the
   first <stencil shadow> met, and whether any surface draw came after it. */
void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	shader_t		*shader, *oldShader;
	int				entityNum, oldEntityNum;
	int				dlightMap, oldDlightMap;
	int				fogNum, oldFogNum;
	shader_t		*fogShader;
	qboolean		depthRange;
	qboolean		bStaticModel, oldbStaticModel;
	int				i;
	drawSurf_t		*drawSurf;
	int				oldSort;
	int				sphereEntity;

	RB_BeginDrawingView ();

	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldFogNum = -1;
	oldDlightMap = 0;
	oldSort = -1;
	oldbStaticModel = qfalse;
	sphereEntity = -1;
	depthRange = qfalse;

	backEnd.pc.c_surfaces += numDrawSurfs;
	backEnd.currentSphere = &backEnd.sphere;

	for (i = 0, drawSurf = drawSurfs ; i < numDrawSurfs ; i++, drawSurf++) {
		/* a null surface pointer is the fault that died at
		   RB_RenderDrawSurfList+0xb4; the sprite walk already answers it */
		if ( drawSurf->surface == NULL ) {
			static int nullRows;

			if ( nullRows < 8 || ( nullRows % 500 ) == 0 ) {
				VK_Guard_Hit( VK_GSITE_QUIRK, va( "draw surf %d of %d carries no surface",
												  i, numDrawSurfs ), VK_INCOMPLETE );
			}
			nullRows++;
			continue;
		}
		/* the surface's first word picks the walker: a key past the table is a
		   call through garbage, a fault that names nothing */
		if ( *drawSurf->surface < 0 || *drawSurf->surface >= SF_NUM_SURFACE_TYPES ) {
			static int surfRows;

			if ( surfRows < 8 || ( surfRows % 500 ) == 0 ) {
				VK_Guard_Hit( VK_GSITE_QUIRK, va( "draw surf names surface type %d",
												  *drawSurf->surface ), VK_INCOMPLETE );
			}
			surfRows++;
			continue;
		}
		if ( drawSurf->sort == (unsigned)oldSort && *drawSurf->surface != SF_SPRITE ) {

			/* the static arena's arm: a resident world surface advances the
			   counters its generator would have, without generating */
			if ( !RB_VboStaticTrySurface( drawSurf->surface, *drawSurf->surface ) ) {
				rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			} else {
				RB_NoteFeedSurf( drawSurf->surface, 3 );
			}
			continue;
		}
		oldSort = (int)drawSurf->sort;

		/* The per-surface bookkeeping between the surface generators: the sort
		   decompose, the entity rotation and the shader-change flush.  Each
		   named part is charged net of the binds it flushes; OTHER is the
		   residual, so the parts sum to the window. */


		if (*drawSurf->surface != SF_SPRITE) {
			int carriedIndex;

			/* sort decomposes for entity/fog/dlight; the shader comes from the
			   pack-time pointer, not the mutable index. */
			R_DecomposeSort(drawSurf->sort, &entityNum, &fogShader, &fogNum, &dlightMap);
			carriedIndex = ( drawSurf->sort >> QSORT_SHADERNUM_SHIFT ) & QSORT_SHADERNUM_MASK;
			shader = ( drawSurf->shaderGen == tr.shaderGen )
				   ? drawSurf->shader
				   : RB_ResolveStaleShader( "drawsurf", drawSurf->shaderGen, carriedIndex );
			bStaticModel = qfalse;
		} else {
			refSprite_t *spr = (refSprite_t*)drawSurf->surface;

			shader = ( spr->shaderGen == tr.shaderGen )
				   ? spr->shader
				   : RB_ResolveStaleShader( "sprite", spr->shaderGen, spr->shaderNum );
			entityNum = ENTITYNUM_WORLD;
			fogNum = spr->fogNum;
			dlightMap = 0;
			bStaticModel = qfalse;
		}

		if ( shader == NULL ) {
			/* the guard named the key; the window still closes or its parts
			   would sit outside every shade total */
			continue;
		}

		if (shader != oldShader || fogNum != oldFogNum || dlightMap != oldDlightMap
			|| (shader->noMerge & 1)
			|| ( entityNum != oldEntityNum && !shader->entityMergable )
			|| ( bStaticModel != oldbStaticModel && !shader->entityMergable )) {
			/* the srf sub-parts are charged inside the pair, so the window arms
			   only across it: the sprite and world walks reach the same two
			   functions and must not add to this window's split */
			if (oldShader != NULL) {
				RB_EndSurface();
			}
			RB_BeginSurface( shader, fogNum );
			oldShader = shader;
			oldFogNum = fogNum;
			oldDlightMap = dlightMap;
		}

		if ( entityNum != oldEntityNum || bStaticModel != oldbStaticModel ) {
			depthRange = qfalse;

			backEnd.sphere.TessFunction = NULL;


			if (bStaticModel) {
				backEnd.currentEntity = 0;
				backEnd.currentStaticModel = &backEnd.refdef.staticModels[entityNum];
				R_RotateForStaticModel(backEnd.currentStaticModel, &backEnd.viewParms, &backEnd.ori );
			}
			else {
				backEnd.currentStaticModel = NULL;
				if (entityNum != ENTITYNUM_WORLD) {
					backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
					backEnd.shaderStartTime = backEnd.currentEntity->e.shaderTime;

					R_RotateForEntity(backEnd.currentEntity, &backEnd.viewParms, &backEnd.ori);

					if (backEnd.currentEntity->needDlights) {
						R_TransformDlights(backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.ori);
					}

					if (backEnd.currentEntity->e.renderfx & RF_DEPTHHACK) {
						depthRange = qtrue;
					}
				}
				else {
					backEnd.currentEntity = &tr.worldEntity;
					backEnd.ori = backEnd.viewParms.world;
					backEnd.shaderStartTime = 0.0;
					R_TransformDlights(backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.ori);
				}
			}

			RB_LoadModelViewMatrix(  backEnd.ori.modelMatrix );

			/* the range is a recorded command, not retail's sticky GL state, so every transition re-issues it */
			vkDepthRange( 0, depthRange ? 0.3 : 1.0 );

			oldEntityNum = entityNum;
			oldbStaticModel = bStaticModel;
		}

		if (bStaticModel)
		{
			if (!r_fullbright->integer) {
				RB_Static_BuildDLights();
			}

			if (!backEnd.currentStaticModel->bStaticLightingCalculated) {
				RB_Static_SetupModel();
			}
		}
		/* the setup depends on the entity and the view, not the shader: one pass
		   per entity instead of one per spherical draw surface */
		else if (backEnd.currentEntity->e.tiki && shader->needsLSpherical
			&& entityNum != sphereEntity)
		{
			R_Sphere_SetupEntity();
			sphereEntity = entityNum;
		}

		if (*drawSurf->surface == SF_SPRITE) {
			backEnd.shaderStartTime = ((refSprite_t*)drawSurf->surface)->shaderTime;
		}

		/* every family's first draw of a shader names itself: the poly transcript samples the list head only */
		{

			if ( !RB_VboStaticTrySurface( drawSurf->surface, *drawSurf->surface ) ) {
				rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			} else {
				RB_NoteFeedSurf( drawSurf->surface, 3 );
			}
		}

		if ( *drawSurf->surface == SF_POLY ) {
		}
	}

	if (oldShader != NULL) {
		RB_EndSurface();
	}

	/* the sub-view's own numbers, and the marker's rect once the main view has
	   walked it: the pair is the whole question this row exists to answer */
	RB_LoadModelViewMatrix(  backEnd.viewParms.world.modelMatrix );
	/* the walk hands the next list the full range whatever its own mirror said */
	vkDepthRange (0, 1.0);

	/* the volumes' place in this view's walk, named before the overlay paints it */
	/* retail R_RenderView @0x0001F190 draws the pass only for a flag-free world refdef */
	if (!backEnd.refdef.rdflags) {
		R_DrawLensFlares();
	}

	if (g_bInfoworldtris)
	{
		g_bInfoworldtris = 0;
		R_PrintInfoWorldtris();
	}

	RB_ShadowFinish();

}

void RB_RenderSpriteSurfList(drawSurf_t* drawSurfs, int numDrawSurfs) {
	shader_t	*shader;
	shader_t	*oldShader;
	int			i;
	drawSurf_t	*drawSurf;

    backEnd.currentEntity = &tr.worldEntity;
    backEnd.currentStaticModel = 0;

	r_spriteListOpen = 1;
	r_spriteWalkSurfs = numDrawSurfs;

	backEnd.pc.c_surfaces += numDrawSurfs;
	backEnd.ori = backEnd.viewParms.world;

	oldShader = NULL;
    vkDepthRange( 0.0, 1.0 );	/* no RB_BeginDrawingView here, so the walk anchors the range it inherits */

    for (i = 0, drawSurf = drawSurfs; i < numDrawSurfs; i++, drawSurf++) {
        if (!drawSurf->surface) continue;
        {
			refSprite_t *spr = (refSprite_t*)drawSurf->surface;

			if (spr->shaderNum < 0 || spr->shaderNum >= tr.numShaders) continue;
			shader = ( spr->shaderGen == tr.shaderGen )
				   ? spr->shader
				   : RB_ResolveStaleShader( "sprite", spr->shaderGen, spr->shaderNum );
			if ( shader == NULL ) {
				continue;	/* the record names a slot outside the live table */
			}
			/* a record whose index no longer names its own shader's slot.  The
			   draw above already took the pointer, so this is pure witness: it
			   names the frames where resolving the index would have worn another
			   shader's stages. */
			if ( shader->sortedIndex != spr->shaderNum ) {
			}
			if ((shader != oldShader || (shader->noMerge & 1)) && !shader->entityMergable)
			{
				if (oldShader) {
					RB_EndSurface();
				}

				RB_BeginSurface(shader, spr->fogNum);
				oldShader = shader;
			}

			RB_LoadModelViewMatrix( backEnd.ori.modelMatrix);

			/* retail's sprite walk carries no range of its own: RB_RenderSpriteSurfList
			   never calls glDepthRange, so an inherited renderfx bit must not shrink it */
			backEnd.shaderStartTime = ((refSprite_t*)drawSurf->surface)->shaderTime;

			rb_surfaceTable[*drawSurf->surface](drawSurf->surface);

		}
	}

	if (oldShader) {
		RB_EndSurface();
	}

	r_spriteListOpen = 0;
	r_spriteWalkSurfs = 0;

    RB_LoadModelViewMatrix( backEnd.viewParms.world.modelMatrix);
	vkDepthRange(0.0, 1.0);
}

/* Retail only *enables* fog in RB_GLFogStageIteratorFunc @0x00034830, around one surface's own passes; R_SetupFrustum just sets the colour and the end distance.  Arming
   it for the whole view fogged every blended sprite stage, whose fog pass FinishShader drops, and filled the quad's rectangle. */
void RB_SetupFog(void) {
	vec4_t	vFogColor;

	vkState.externalSetState &= ~VKS_FOG;

	if (!backEnd.viewParms.farplane_distance
		|| r_farplane_nofog->integer
		|| !backEnd.viewParms.farplane_fog) {
		return;
	}

	vFogColor[0] = backEnd.viewParms.farplane_color[0] * tr.identityLight;
	vFogColor[1] = backEnd.viewParms.farplane_color[1] * tr.identityLight;
	vFogColor[2] = backEnd.viewParms.farplane_color[2] * tr.identityLight;
	vFogColor[3] = 1.0;
	VK_SetFogColor(vFogColor);
}

const void	*RB_DrawSurfs( const void *data ) {
	const drawSurfsCommand_t	*cmd;


	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	cmd = (const drawSurfsCommand_t *)data;

	/* The frame-carry law: the walk reads the record's copy of the frame, not
	   the live tr.* the front end may be refilling. */
	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	/* front end ahead of the handed-over frame: nonzero means the live reads raced */
	backEnd.pc.c_surfaces += 0;

	RB_SetupFog();
	RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );


	return (const void *)((const char *)cmd + cmd->totalBytes);
}

const void* RB_SpriteSurfs(const void* data) {
    const drawSurfsCommand_t* cmd;

    if (tess.numIndexes) {
        RB_EndSurface();
    }

    cmd = (const drawSurfsCommand_t*)data;

    backEnd.refdef = cmd->refdef;
    backEnd.viewParms = cmd->viewParms;

	/* The sprite pass is a 3D view and in2D is the walk's flag: cleared here as
	   RC_DRAW_SURFS clears it in RB_BeginDrawingView.  A sprite list reached with
	   in2D still up takes RB_StageIterator2D at RB_BeginSurface. */
    backEnd.in2D = qfalse;

	RB_SetupFog();
    RB_RenderSpriteSurfList(cmd->drawSurfs, cmd->numDrawSurfs);

    return (const void*)((const char *)cmd + cmd->totalBytes);
}

int RB_ModelViewDepth( void )
{
	return VK_Shade_ModelViewDepth();
}

int RB_ProjectionDepth( void )
{
	return VK_Shade_ProjectionDepth();
}

/* the reference renderer's debug overlay; the row names the site */
void R_DrawDebugNumber( const vec3_t org, float number, float scale, float r, float g, float b, int precision )
{
	(void)org; (void)number; (void)scale; (void)r; (void)g; (void)b; (void)precision;
}


void VKimp_LogComment(char *comment)
{
	(void)comment;
}

unsigned long RF_ForceAlphaStateBits( unsigned long stateBits )
{
	/* retail never touches the blend for RF_FORCE_ALPHA: the fade rides the
	   entity's own alpha in the colour (R_AddSkelSurfaces), so the shader's
	   blendFunc survives -- an additive flash stays additive.  Rewriting the
	   factors here turned every forced fade into an alpha-blend box. */
	return stateBits;
}

/* the reference renderer drives this through the font's renderer; the row names the site */
void R_DrawFloatingString( fontheader_t *font, const char *text, const vec3_t org,
						   const vec4_t color, float scale, int maxlen )
{
	(void)font; (void)text; (void)org; (void)color; (void)scale; (void)maxlen;
}

/* ---- the 2D walk, which the M2 fixtures prove: one pass, pipeline, bind -- */
void RB_StageIterator2D( void )
{
	int stage;

	/* This backend's 2D dispatch is RB_StageIterator2D, which never reaches
	   RB_StageIteratorGeneric, so the clock row is taken here as well: a
	   scrolling 2D shader (a wall panel's screen, a HUD effect) animates off
	   the same tess.shaderTime. */
	tess.shaderTime = backEnd.refdef.floatTime;

	/* the quad's own ST, before any stage's chain lands in it */
	VK_Stage2DAuthorST();

	for ( stage = 0; stage < tess.numPasses; stage++ ) {
		shaderStage_t *pStage = tess.xstages[stage];
		const color4ub_t *colors;

		if ( !pStage || !pStage->active ) {
			continue;
		}

		/* A stage whose gens the tess pipeline cannot express is resolved into
		   the CPU colour array first; the vertex-driven ones keep the 2D quad's
		   own colour (see VK_StageResolve2DColor). */
		colors = VK_StageResolve2DColor( pStage ) ? tess.svars.colors : tess.vertexColors;

		/* the stage's tcMods land in the vertex ST: the tess shader has no tcMod arm */
		VK_Stage2DResolveTexCoords( pStage );

		/* the 2D walk was the one iterator that never reported into the frame's
		   stage census, so a menu-heavy frame under-counted its own stages */
		RB_StagePass( pStage, colors );
	}
}
