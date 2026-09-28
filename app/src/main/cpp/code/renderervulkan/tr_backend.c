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

	view = VK_Texture_View( (int)image->texnum );
	if ( view == VK_NULL_HANDLE ) {
		VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "image without a Vulkan view", VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	/* The 2D binding row: the quad's shader, the view the slot resolves to, and
	   the rect the geometry carries.  A tile sampling the wrong art names itself
	   here as shader X with image Y. */
	if ( backEnd.in2D ) {
		static int s_rows;

		float x0 = tess.xyz[0][0], y0 = tess.xyz[0][1];
		float x1 = x0, y1 = y0;
		int   v;

		for ( v = 1; v < tess.numVertexes; v++ ) {
			if ( tess.xyz[v][0] < x0 ) x0 = tess.xyz[v][0];
			if ( tess.xyz[v][0] > x1 ) x1 = tess.xyz[v][0];
			if ( tess.xyz[v][1] < y0 ) y0 = tess.xyz[v][1];
			if ( tess.xyz[v][1] > y1 ) y1 = tess.xyz[v][1];
		}

		/* the order transcript: the rate-limited row above cannot answer a
		   z-order question, because after the fortieth draw the sampling
		   throws the sequence away.  A widget's depth is the position it draws
		   at, so the ordered names are the answer; this collects them and
		   VK_Shade_NewFrame emits the frame's list. */
		VK_2DOrder_Note( tess.shader ? tess.shader->name : "?" );

		s_rows++;
		if ( s_rows <= 40 || ( s_rows % 64 ) == 0 ) {
			VK_Guard_Row( "VK:2ddraw",
						  "n=%d shader='%s' img='%s' tex=%d verts=%d idx=%d "
						  "rect=(%.1f,%.1f)-(%.1f,%.1f) st=(%.3f,%.3f)-(%.3f,%.3f) "
						  "sb=0x%x col=%d,%d,%d,%d vert=%d,%d,%d,%d rgbGen=%d "
						  "const=%d,%d,%d,%d alphaGen=%d "
						  "win=(%d,%d,%d,%d) scn=(%d,%d,%d,%d) wrap=%d,%d clamp=%d",
						  s_rows,
						  tess.shader ? tess.shader->name : "?",
						  image->imgName,
						  (int)image->texnum,
						  tess.numVertexes, tess.numIndexes,
						  x0, y0, x1, y1,
						  tess.texCoords[0][0][0], tess.texCoords[0][0][1],
						  tess.texCoords[tess.numVertexes - 1][0][0],
						  tess.texCoords[tess.numVertexes - 1][0][1],
						  stage->stateBits,
						  backEnd.color2D[0], backEnd.color2D[1],
						  backEnd.color2D[2], backEnd.color2D[3],
						  tess.vertexColors[0][0], tess.vertexColors[0][1],
						  tess.vertexColors[0][2], tess.vertexColors[0][3],
						  (int)stage->rgbGen,
						  stage->colorConst[0], stage->colorConst[1],
						  stage->colorConst[2], stage->colorConst[3],
						  (int)stage->alphaGen,
						  vk_2dWindow[0], vk_2dWindow[1], vk_2dWindow[2], vk_2dWindow[3],
						  vk_2dScissor[0], vk_2dScissor[1], vk_2dScissor[2], vk_2dScissor[3],
						  (int)image->wrapClampModeX, (int)image->wrapClampModeY,
						  ( image->flags & IMGFLAG_CLAMPTOEDGE
							|| image->wrapClampModeX != VK_TWRAP_REPEAT
							|| image->wrapClampModeY != VK_TWRAP_REPEAT ) ? 1 : 0 );
		}
	}

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

	pipeline = VK_Draw_Pipeline( &key );
	if ( pipeline == VK_NULL_HANDLE ) {
		return qfalse;
	}

	VK_Draw_BindPipeline( vk.commandBuffers[s_rbSlot], pipeline );
	VK_Draw_SetAlphaTest( vk.commandBuffers[s_rbSlot], stage->stateBits );

	/* wrapClampModeX/Y hold the legacy wrap enum: REPEAT is 10497, not 0, and the
	   loader sets them from `clampmap' -- the GLES3 twin's whole 2D pass samples
	   by exactly this mode.  Testing only IMGFLAG_CLAMPTOEDGE, a flag the font
	   atlas alone carries, left every 2D clampmap stage on REPEAT, so a
	   linear-sampled panel bled its opposite edge into the border and left the
	   seam between neighbours.  clampTime is not a wrap: it is a tcMod clock and
	   no shipped shader sets it. */
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
		VK_Draw_DrawIndexed( vk.commandBuffers[s_rbSlot], tess.numIndexes, indexBase, vertexBase );
		VK_Shade_NoteDraw( qfalse );
		return qtrue;
	}

	ringVerts = VK_VertexRing_Reserve( tess.numVertexes, &vertexBase );
	if ( !ringVerts ) {
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

	ringIndexes = VK_VertexRing_ReserveIndexes( tess.numIndexes, &indexBase );
	if ( !ringIndexes ) {
		return qfalse;
	}

	memcpy( ringIndexes, tess.indexes, (size_t)tess.numIndexes * sizeof( vkIndex_t ) );

	if ( stage ) {
		VK_VertexRing_StreamStore( tess.numVertexes, tess.numIndexes,
								   tess.texCoords[0][0], tess.texCoords[0][0], 0,
								   colors, NULL, vertexBase, indexBase );
	}

	/* The ring is bound per surface: a bind is cheaper than tracking which. */
	VK_Draw_BindRing( vk.commandBuffers[s_rbSlot] );
	VK_Draw_DrawIndexed( vk.commandBuffers[s_rbSlot], tess.numIndexes, indexBase, vertexBase );
	VK_Shade_NoteDraw( qfalse );

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
	s_rbSlot = -1;

	/* No 2D window/scissor invalidation here.  That cache belongs to the FRONT
	   END's recording stream, and this runs on the walk thread for a frame the
	   front end recorded earlier: dropping it from here raced the front end's
	   recording of the NEXT frame, which is what made a frame's first 2D window
	   be elided on some frames and recorded on others.  The front end drops it
	   at its own frame boundary (RE_EndFrame) and after every 3D command. */
}

qboolean RB_RecordSurfaces( VkCommandBuffer cmd )
{
	return qtrue;
}

void RB_FramePrologue( void )
{
}

/* ---- the 2D immediate stream: a run is one pipeline and one ring slice -- */

typedef struct {
	qboolean	open;
	int			topology;
	int			verts;
	int			base;
	qboolean	pending;
	image_t		*image;
	VkPipeline	pipeline;
} vkImmRun_t;

static vkImmRun_t	s_imm;

static float		s_immCurSt[2];	/* the stream's current st, stamped per vertex */
static byte			s_immCurColor[4] = { 255, 255, 255, 255 };

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

/* The run's pipeline state is the caller's, not a field of the run. */
void RB_ImmBeginWithImage( int topology, image_t *image, unsigned long stateBits )
{
	vkPipelineKey_t key;
	vkImmRun_t *run = &s_imm;

	if ( run->open && ( run->topology != topology || run->image != image ) ) {
		RB_ImmEnd();
	}

	memset( &key, 0, sizeof( key ) );
	key.topology = topology;
	key.stateBits = stateBits;
	key.cull = CT_TWO_SIDED;
	key.depthTest = ( stateBits & VKS_DEPTHTEST_DISABLE ) ? qfalse : qtrue;
	key.depthWrite = ( stateBits & VKS_DEPTHMASK_TRUE ) ? qtrue : qfalse;

	/* The ortho is recorded on every run, at function scope: a run that trusted
	   the previous run's push drew with a stale ortho, and a 2D quad under a
	   stale ortho lands as a garbage sheet across the frame.  Which ortho it
	   carries depends on the space the run draws in -- inside 2D it is the
	   window's own, the same value the window setup pushed, and the 3D getters
	   do not hold it -- but the recording itself is unconditional. */
	struct { float ortho[16]; float alphaTest[4]; } push;	/* vk_draw.c's layout */
	const float *window, *proj, *view;
	int r, c, k;

	memset( &push, 0, sizeof( push ) );
	window = backEnd.in2D ? VK_Draw_CurrentOrtho() : NULL;
	if ( window ) {
		memcpy( push.ortho, window, sizeof( push.ortho ) );
	} else {
		proj = RB_GetProjectionMatrix();
		view = RB_GetModelViewMatrix();
		for ( r = 0; r < 4; r++ ) {
			for ( c = 0; c < 4; c++ ) {
				float sum = 0.0f;

				for ( k = 0; k < 4; k++ ) {
					sum += proj[r * 4 + k] * view[k * 4 + c];
				}
				push.ortho[c * 4 + r] = sum;
			}
		}
		/* the push holds the product transposed, keeping the flat offsets of
		   the negated row: 1, 5, 9, 13 */
		VK_Mat4_ClipToWindow( push.ortho );
	}
	push.alphaTest[0] = 0.0f;
	push.alphaTest[1] = 0.0f;

	qvk.CmdPushConstants( vk.commandBuffers[s_rbSlot], VK_Draw_Layout(),
						  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
						  0, sizeof( push ), &push );

	run->pipeline = VK_Draw_Pipeline( &key );
	if ( run->pipeline == VK_NULL_HANDLE ) {
		return;
	}

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

/* A run reaches the ring whole: the setters rewrite the slice, so an early
   commit would miss vertices.  A commit consumes exactly the slice it drew,
   so a second commit of the same run draws what came after the first, not the
   run again.  Re-issuing the run put every earlier quad of a run back on the
   screen at the last commit's position in the command buffer -- after any 3D
   batch that ran in between -- which is the menu fill over its own star dome
   and the flashing the operator reported. */
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

	{
		static int s_dbg;
		if ( s_dbg < 40 ) {
			VK_Guard_Row( "VK:dbg", "imm-commit topo=%d verts=%d base=%d in2d=%d",
						  run->topology, run->verts, run->base, backEnd.in2D );
			s_dbg++;
		}
	}
	VK_Draw_Draw( cmd, run->verts, run->base );

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

/* ---- the timing instruments: the seam the mirrored front end calls ---- */

static int s_beRootArmed[BE_FAM_COUNT];

void BE_Bump( int family, int counter, int n )
{
	(void)family; (void)counter; (void)n;
}

void BE_RootBegin( int family )
{
	if ( family >= 0 && family < BE_FAM_COUNT ) {
		s_beRootArmed[family]++;
	}
}

void BE_RootEnd( int family )
{
	if ( family >= 0 && family < BE_FAM_COUNT && s_beRootArmed[family] > 0 ) {
		s_beRootArmed[family]--;
	}
}

void BE_Sub( int family, int sub, int usec )
{
	(void)family; (void)sub; (void)usec;
}

void BE_Zone( int family, int zone, int usec )
{
	(void)family; (void)zone; (void)usec;
}

void BE_Name( int family, const char *name, int usec )
{
	(void)family; (void)name; (void)usec;
}

int BE_RootUsec( int family )
{
	(void)family;
	return 0;
}

/* ---- the data pool, the state block and the recorders ---- */

backEndData_t	*backEndData;

backEndData_t	*backEndDataBuf[SMP_FRAMES];

vkstate_t	vkState;

int		max_polys;

int		max_polyverts;

void	R_AddDrawSurfCmd( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	drawSurfsCommand_t	*cmd;

	R_Invalidate2DWindowCache();

	cmd = R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_DRAW_SURFS;

	cmd->drawSurfs = drawSurfs;
	cmd->numDrawSurfs = numDrawSurfs;

	cmd->refdef = tr.refdef;
	cmd->viewParms = tr.viewParms;
}

void	R_AddSpriteSurfCmd(drawSurf_t* drawSurfs, int numDrawSurfs) {
    drawSurfsCommand_t* cmd;

    R_Invalidate2DWindowCache();

    cmd = R_GetCommandBuffer(sizeof(*cmd));
    if (!cmd) {
        return;
    }
    cmd->commandId = RC_SPRITE_SURFS;

    cmd->drawSurfs = drawSurfs;
    cmd->numDrawSurfs = numDrawSurfs;

    cmd->refdef = tr.refdef;
    cmd->viewParms = tr.viewParms;
}

/* ---- the 3D view and the draw-surf walk ---------------------------------- */

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

	/* A rect cached here would survive the 3D pass and skip re-issuing a
	   scissor that pass overwrote: drop the pair. */
	R_Invalidate2DWindowCache();

	SetViewportAndScissor();

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
	}

	VK_DepthFuncApply( VK_LEQUAL );	/* pinned per view for the A/B cvar */

	/* Retail's traced menu frame clears DEPTH and nothing else: the view builds no volume, and colour
	   joins only for a world view's fastsky or fog. Colour on the UI's view erased the 2D painted
	   ahead of it; no depth left a later view testing against the first one's depth. */
	{
		VkClearAttachment attachments[2];
		VkClearRect rect;
		int count = 0;

		memset( attachments, 0, sizeof( attachments ) );
		attachments[count].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		attachments[count].clearValue.depthStencil.depth = 1.0f;
		attachments[count].clearValue.depthStencil.stencil = 0;
		count++;

		if ( ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) == 0 )
		{
			qboolean colour = qfalse;

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

		if ( rect.rect.extent.width && rect.rect.extent.height ) {
			qvk.CmdClearAttachments( VK_Shade_CommandBuffer(), count, attachments, 1, &rect );
		}
	}

	vkState.faceCulling = -1;

	backEnd.skyRenderedThisView = qfalse;

	if ( backEnd.viewParms.isPortal ) {
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
static long long VK_Shade_PartCharge( int part, long long t0, long long nested0 )
{
	long long usec;

	if ( t0 < 0 ) {
		return 0;
	}

	usec = BE_Microseconds64() - t0 - ( VK_WalkStat_Nested() - nested0 );
	if ( usec < 0 ) {
		usec = 0;
	}

	VK_WalkStat_ShadeAdd( part, usec );

	return usec;
}

/* Close the shade window: charge the residual to OTHER so the parts sum to the
   window, then charge the window to the walk bucket. */
static void VK_Shade_WindowClose( long long shadeT0, long long nested0, long long parts )
{
	long long usec, other;

	if ( shadeT0 < 0 ) {
		return;
	}

	usec = BE_Microseconds64() - shadeT0 - ( VK_WalkStat_Nested() - nested0 );
	other = usec - parts;

	VK_WalkStat_ShadeAdd( VK_WS_OTHER, other > 0 ? other : 0 );
	VK_WalkStat_Add( VK_WP_SHADE, usec );
}

void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	shader_t		*shader, *oldShader;
	int				entityNum, oldEntityNum;
	int				dlightMap, oldDlightMap;
	int				fogNum, oldFogNum;
	qboolean		depthRange, oldDepthRange;
	qboolean		bStaticModel, oldbStaticModel;
	int				i;
	drawSurf_t		*drawSurf;
	int				oldSort;

	RB_BeginDrawingView ();

	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldDepthRange = qfalse;
	oldFogNum = -1;
	oldDlightMap = 0;
	oldSort = -1;
	oldbStaticModel = qfalse;
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
			long long walkT0 = VK_WALK_T0();
			long long walkNest0 = VK_WALK_NEST0();

			/* the static arena's arm: a resident world surface advances the
			   counters its generator would have, without generating */
			if ( !RB_VboStaticTrySurface( drawSurf->surface, *drawSurf->surface ) ) {
				rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			}
			VK_WALK_ADD_NET( walkT0, walkNest0, VK_WP_GEN );
			continue;
		}
		oldSort = (int)drawSurf->sort;

		/* The per-surface bookkeeping between the surface generators: the sort
		   decompose, the entity rotation and the shader-change flush.  Each
		   named part is charged net of the binds it flushes; OTHER is the
		   residual, so the parts sum to the window. */
		long long shadeT0 = VK_WALK_T0();
		long long shadeNested0 = VK_WalkStat_Nested();
		long long shadeParts = 0;
		long long partT0, partNested0;

		partT0 = VK_WALK_T0();
		partNested0 = VK_WalkStat_Nested();

		if (*drawSurf->surface != SF_SPRITE) {
			R_DecomposeSort(drawSurf->sort, &entityNum, &shader, &fogNum, &dlightMap);
			bStaticModel = qfalse;
		} else {
			shader = tr.sortedShaders[((refSprite_t*)drawSurf->surface)->shaderNum];
			entityNum = ENTITYNUM_WORLD;
			fogNum = ((refSprite_t*)drawSurf->surface)->fogNum;
			dlightMap = 0;
			bStaticModel = qfalse;
		}
		shadeParts += VK_Shade_PartCharge( VK_WS_DECOMPOSE, partT0, partNested0 );

		if ( shader == NULL ) {
			/* the guard named the key; the window still closes or its parts
			   would sit outside every shade total */
			VK_Shade_WindowClose( shadeT0, shadeNested0, shadeParts );
			continue;
		}

		if (shader != oldShader || fogNum != oldFogNum || dlightMap != oldDlightMap
			|| (shader->noMerge & 1)
			|| ( entityNum != oldEntityNum && !shader->entityMergable )
			|| ( bStaticModel != oldbStaticModel && !shader->entityMergable )) {
			partT0 = VK_WALK_T0();
			partNested0 = VK_WalkStat_Nested();
			if (oldShader != NULL) {
				RB_EndSurface();
			}
			RB_BeginSurface( shader, fogNum );
			oldShader = shader;
			oldFogNum = fogNum;
			oldDlightMap = dlightMap;
			shadeParts += VK_Shade_PartCharge( VK_WS_SURFACE, partT0, partNested0 );
		}

		if ( entityNum != oldEntityNum || bStaticModel != oldbStaticModel ) {
			depthRange = qfalse;

			backEnd.sphere.TessFunction = NULL;

			partT0 = VK_WALK_T0();
			partNested0 = VK_WalkStat_Nested();

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
			shadeParts += VK_Shade_PartCharge( VK_WS_ROTATE, partT0, partNested0 );

			partT0 = VK_WALK_T0();
			partNested0 = VK_WalkStat_Nested();
			RB_LoadModelViewMatrix(  backEnd.ori.modelMatrix );
			shadeParts += VK_Shade_PartCharge( VK_WS_MODELVIEW, partT0, partNested0 );

			if ( oldDepthRange != depthRange ) {
				if ( depthRange ) {
					vkDepthRange (0, 0.3);
				} else {
					vkDepthRange (0, 1.0);
				}
				oldDepthRange = depthRange;
			}

			oldEntityNum = entityNum;
			oldbStaticModel = bStaticModel;
		}

		if (bStaticModel)
		{
			partT0 = VK_WALK_T0();
			partNested0 = VK_WalkStat_Nested();
			if (!r_fullbright->integer) {
				RB_Static_BuildDLights();
			}

			if (!backEnd.currentStaticModel->bStaticLightingCalculated) {
				RB_Static_SetupModel();
			}
			shadeParts += VK_Shade_PartCharge( VK_WS_STATICDLIGHTS, partT0, partNested0 );
		}
		else if (backEnd.currentEntity->e.tiki && shader->needsLSpherical)
		{
			partT0 = VK_WALK_T0();
			partNested0 = VK_WalkStat_Nested();
			R_Sphere_SetupEntity();
			shadeParts += VK_Shade_PartCharge( VK_WS_SPHERE, partT0, partNested0 );
		}

		if (*drawSurf->surface == SF_SPRITE) {
			backEnd.shaderStartTime = ((refSprite_t*)drawSurf->surface)->shaderTime;
		}

		VK_Shade_WindowClose( shadeT0, shadeNested0, shadeParts );

		{
			long long walkT0 = VK_WALK_T0();
			long long walkNest0 = VK_WALK_NEST0();

			if ( !RB_VboStaticTrySurface( drawSurf->surface, *drawSurf->surface ) ) {
				rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			}
			VK_WALK_ADD_NET( walkT0, walkNest0, VK_WP_GEN );
		}
	}

	if (oldShader != NULL) {
		RB_EndSurface();
	}

	RB_LoadModelViewMatrix(  backEnd.viewParms.world.modelMatrix );
	if ( oldDepthRange ) {
		vkDepthRange (0, 1.0);
	}

	if (!(backEnd.refdef.rdflags & RDF_FULLBRIGHT)) {
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
	qboolean	depthRange;
	qboolean	oldDepthRange;
	int			i;
	drawSurf_t	*drawSurf;

    backEnd.currentEntity = &tr.worldEntity;
    backEnd.currentStaticModel = 0;

	r_spriteListOpen = 1;

	backEnd.pc.c_surfaces += numDrawSurfs;
	backEnd.ori = backEnd.viewParms.world;

	oldShader = NULL;
    depthRange = qfalse;
    oldDepthRange = qfalse;

    for (i = 0, drawSurf = drawSurfs; i < numDrawSurfs; i++, drawSurf++) {
        if (!drawSurf->surface) continue;
        {
			refSprite_t *spr = (refSprite_t*)drawSurf->surface;

			if (spr->shaderNum < 0 || spr->shaderNum >= tr.numShaders) continue;
			shader = tr.sortedShaders[spr->shaderNum];
			if ( shader == NULL ) {
				continue;	/* the guard named the key */
			}
			depthRange = (spr->renderfx & RF_DEPTHHACK) != 0;

			if ((shader != oldShader || (shader->noMerge & 1)) && !shader->entityMergable)
			{
				if (oldShader) {
					RB_EndSurface();
				}

				RB_BeginSurface(shader, spr->fogNum);
				oldShader = shader;
			}

			RB_LoadModelViewMatrix( backEnd.ori.modelMatrix);

			if (oldDepthRange != depthRange)
			{
				vkDepthRange(0.0, depthRange ? 0.3 : 1.0);
				oldDepthRange = depthRange;
			}

			backEnd.shaderStartTime = ((refSprite_t*)drawSurf->surface)->shaderTime;

			rb_surfaceTable[*drawSurf->surface](drawSurf->surface);
		}
	}

	if (oldShader) {
		RB_EndSurface();
	}

	r_spriteListOpen = 0;

    RB_LoadModelViewMatrix( backEnd.viewParms.world.modelMatrix);
	if (oldDepthRange) {
		vkDepthRange(0.0, 1.0);
	}
}

void RB_SetupFog(void) {
	if (!backEnd.viewParms.farplane_distance) {
		vkState.externalSetState &= ~VKS_FOG;
	} else if (r_farplane_nofog->integer) {
		vkState.externalSetState &= ~VKS_FOG;
	} else if (!backEnd.viewParms.farplane_fog) {
		vkState.externalSetState &= ~VKS_FOG;
	} else {
		vec4_t vFogColor;

        vFogColor[0] = backEnd.viewParms.farplane_color[0] * tr.identityLight;
        vFogColor[1] = backEnd.viewParms.farplane_color[1] * tr.identityLight;
        vFogColor[2] = backEnd.viewParms.farplane_color[2] * tr.identityLight;
        vFogColor[3] = 1.0;
        VK_SetFogColor(vFogColor);

		vkState.externalSetState |= VKS_FOG;
	}
}

const void	*RB_DrawSurfs( const void *data ) {
	const drawSurfsCommand_t	*cmd;

	BE_RootBegin( BE_FAM_DS );

	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	cmd = (const drawSurfsCommand_t *)data;

	/* The frame-carry law: the walk reads the record's copy of the frame, not
	   the live tr.* the front end may be refilling. */
	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	backEnd.pc.c_surfaces += 0;

	VK_Shade_NoteSurfCmds( cmd->numDrawSurfs );
	RB_SetupFog();
	RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );

	BE_RootEnd( BE_FAM_DS );

	return (const void *)(cmd + 1);
}

const void* RB_SpriteSurfs(const void* data) {
    const drawSurfsCommand_t* cmd;

    if (tess.numIndexes) {
        RB_EndSurface();
    }

    cmd = (const drawSurfsCommand_t*)data;

    backEnd.refdef = cmd->refdef;
    backEnd.viewParms = cmd->viewParms;

	RB_SetupFog();
    RB_RenderSpriteSurfList(cmd->drawSurfs, cmd->numDrawSurfs);

    return (const void*)(cmd + 1);
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
	if ( backEnd.currentEntity && ( backEnd.currentEntity->e.renderfx & RF_FORCE_ALPHA )
		&& backEnd.currentEntity->e.shaderRGBA[3] < 255 )
	{
		return ( stateBits & ~( VKS_SRCBLEND_BITS | VKS_DSTBLEND_BITS | VKS_DEPTHMASK_TRUE ) )
		       | VKS_SRCBLEND_SRC_ALPHA | VKS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}

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

	/* This lane's 2D dispatch is RB_StageIterator2D, which never reaches
	   RB_StageIteratorGeneric, so the clock row is taken here as well: a
	   scrolling 2D shader (a wall panel's screen, a HUD effect) animates off
	   the same tess.shaderTime. */
	tess.shaderTime = backEnd.refdef.floatTime;
	VK_Shade_NoteShaderTime( "2d" );

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

		/* the 2D walk was the one iterator that never reported into the frame's
		   stage census, so a menu-heavy frame under-counted its own stages */
		VK_Shade_NoteStage( tess.shader );
		RB_StagePass( pStage, colors );
	}
}
