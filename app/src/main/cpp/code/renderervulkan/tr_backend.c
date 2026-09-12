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
 * command buffer.  One surface is one stage pass: the stage's state tuple
 * becomes a pipeline (vk_draw.c), the stage's bundle becomes a descriptor
 * bind, the tess arrays become a slice of the frame's vertex ring, and the
 * indexes become an indexed draw.  Nothing here touches a driver object -- the
 * reference renderer's per-state calls all become fields of the
 * pipeline and the descriptor bind that already carry them.
 *
 * The 3D stage iterators (sky, lightmapped multitexture, vertex-lit, the legacy
 * fog pass) are the next milestone's surface; the 2D/HUD iterator below is
 * the one the menu paints through and it is the same walk the generic
 * iterator performs minus the lighting terms M3's shaders add.
 */

#include "tr_local.h"
#include "tr_vbo.h"

#include <string.h>

/* defined by the mirrored 3D front end */
backEndState_t		backEnd;
/* trGlobals_t tr is defined by the mirrored 3D front end */
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

/* the tessellation store lives in tr_shade.c now, as it does in the reference renderer */


/* ---- the stage pass: one stage, one pipeline, one bind, one indexed draw.
 * The state tuple carries all of it: blend, depth, cull, alpha test. */

static qboolean RB_StagePass( shaderStage_t *stage )
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

	memset( &key, 0, sizeof( key ) );
	key.topology = VK_IMM_TRIANGLES;
	key.stateBits = stage->stateBits;
	/* The 2D window is two-sided, as the reference renderer's RB_2D_SetWindow is. */
	key.cull = ( backEnd.in2D || tess.shader->lightmapIndex == LIGHTMAP_2D )
			   ? CT_TWO_SIDED : tess.shader->cullType;
	key.depthTest = ( stage->stateBits & VKS_DEPTHTEST_DISABLE ) ? qfalse : qtrue;
	key.depthWrite = ( stage->stateBits & VKS_DEPTHMASK_TRUE ) ? qtrue : qfalse;

	pipeline = VK_Draw_Pipeline( &key );
	if ( pipeline == VK_NULL_HANDLE ) {
		return qfalse;
	}

	qvk.CmdBindPipeline( vk.commandBuffers[s_rbSlot], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
	VK_Draw_SetAlphaTest( vk.commandBuffers[s_rbSlot], stage->stateBits );

	clamp = ( tess.shader->clampTime != 0.0f )
			|| ( image->flags & IMGFLAG_CLAMPTOEDGE ) ? qtrue : qfalse;
	mipmap = ( image->numMipmaps > 1 ) ? qtrue : qfalse;

	if ( !VK_Texture_Sampler( clamp, mipmap, qtrue, &sampler ) ) {
		return qfalse;
	}

	if ( !VK_Draw_BindTexture( vk.commandBuffers[s_rbSlot], view, sampler ) ) {
		return qfalse;
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

		if ( tess.vertexColorValid ) {
			ringVerts[i].color[0] = tess.vertexColors[i][0];
			ringVerts[i].color[1] = tess.vertexColors[i][1];
			ringVerts[i].color[2] = tess.vertexColors[i][2];
			ringVerts[i].color[3] = tess.vertexColors[i][3];
		} else {
			ringVerts[i].color[0] = 254;
			ringVerts[i].color[1] = 254;
			ringVerts[i].color[2] = 254;
			ringVerts[i].color[3] = 254;
		}
	}

	ringIndexes = VK_VertexRing_ReserveIndexes( tess.numIndexes, &indexBase );
	if ( !ringIndexes ) {
		return qfalse;
	}

	memcpy( ringIndexes, tess.indexes, (size_t)tess.numIndexes * sizeof( vkIndex_t ) );



	/* The ring is bound per surface: a bind is cheaper than tracking which. */
	VK_Draw_BindRing( vk.commandBuffers[s_rbSlot] );
	VK_Draw_DrawIndexed( vk.commandBuffers[s_rbSlot], tess.numIndexes, indexBase, vertexBase );

	vk_ring_note( vertexBase + tess.numVertexes, indexBase + tess.numIndexes );

	return qtrue;
}

/* The generic iterator: one pass per active stage.  A shader needing a 3D
   term is the 3D surface's work and is named by its guard. */


/* RB_EndSurface is tr_shade.c's, as in the reference renderer. */


void rb_set_slot( int slot )
{
	s_rbSlot = slot;
}

void RB_RecordFramePrologue( void )
{
	VK_VertexRing_ResetFrame();
	s_rbSlot = -1;

	/* Every frame is a fresh command buffer, so the window and scissor must
	   be recorded again even when unchanged. */
	R_Invalidate2DWindowCache();
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

/* The stream's current colour and texcoord.  The reference renderer keeps one
   pair for the whole immediate store and stamps every vertex with it at the
   moment that vertex is given, so a colour or a texcoord named before a run is
   the one the run draws with.  Rewriting the vertices already emitted is a
   different semantic: it leaves every vertex carrying the previous call's
   texcoord and the last one the zero, which is the menu background drawn with
   its texel rows shifted by one vertex. */
static float		s_immCurSt[2];
static byte			s_immCurColor[4] = { 255, 255, 255, 255 };

/* A vertex is stamped once, where it is given: both the 2D and the 3D callers
   go through this, so one stream cannot disagree with the other. */
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

/* The run's pipeline state is the caller's: every caller states its bits with
   VK_State / VK_ApplyStateBits immediately before it opens a run, exactly as the
   reference renderer's GLES3_ApplyStateBits precedes its RB_ImmBegin.  Reading a
   field of the run instead drew every immediate surface through one fixed blend
   nobody had ever written -- the menu background added to what was under it. */
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

	run->pipeline = VK_Draw_Pipeline( &key );
	if ( run->pipeline == VK_NULL_HANDLE ) {
		return;
	}

	qvk.CmdBindPipeline( vk.commandBuffers[s_rbSlot], VK_PIPELINE_BIND_POINT_GRAPHICS, run->pipeline );

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
	/* tr.whiteImage is what the legacy back end would have had bound for the
	   boxes: painted, not textured.  The state is the caller's, as above. */
	RB_ImmBeginWithImage( topology, tr.whiteImage, vk_Shade_StateBits() );
}

void RB_ImmEndDeferred( void )
{
}

/* A run reaches the ring whole: the colour and texcoord setters rewrite the
   slice, so committing early would miss vertices already drawn. */
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
		/* Vulkan has no line-loop topology: close the strip with the first
		   vertex repeated, which is the same edge set. */
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

	if ( run->topology == VK_IMM_TRIANGLES ) {
		VK_Draw_Draw( cmd, run->verts, run->base );
	} else {
		VK_Draw_Draw( cmd, run->verts, run->base );
	}

	backEnd.pc.c_vertexes += run->verts;
	backEnd.pc.c_surfaces++;
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
	/* The stream's current colour: the vertices after it carry it, the ones
	   before it keep their own. */
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
   calls the whole reference set, so the 2D stream carries the rest -------------- */

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

/* ---- the timing instruments: the mirrored front end calls the reference renderer's names, so
   the seam is here and the rows are what is missing ---- */

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

	{
		VkViewport viewport;
		VkRect2D scissor;

		memset( &viewport, 0, sizeof( viewport ) );
		viewport.x = (float)backEnd.viewParms.viewportX;
		viewport.y = (float)( vkConfig.vidHeight - backEnd.viewParms.viewportY
							  - backEnd.viewParms.viewportHeight );
		viewport.width = (float)backEnd.viewParms.viewportWidth;
		viewport.height = (float)backEnd.viewParms.viewportHeight;
		viewport.maxDepth = 1.0f;

		memset( &scissor, 0, sizeof( scissor ) );
		scissor.offset.x = backEnd.viewParms.viewportX;
		scissor.offset.y = vkConfig.vidHeight - backEnd.viewParms.viewportY
						   - backEnd.viewParms.viewportHeight;
		scissor.extent.width = (uint32_t)( backEnd.viewParms.viewportWidth > 0
											? backEnd.viewParms.viewportWidth : 0 );
		scissor.extent.height = (uint32_t)( backEnd.viewParms.viewportHeight > 0
											 ? backEnd.viewParms.viewportHeight : 0 );

		VK_Shade_Viewport3D( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
							 backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
		qvk.CmdSetViewport( VK_Shade_CommandBuffer(), 0, 1, &viewport );
		qvk.CmdSetScissor( VK_Shade_CommandBuffer(), 0, 1, &scissor );
	}
}

static void RB_Hyperspace( void ) {
	float c;

	c = ( backEnd.refdef.time & 255 ) / 255.0f;

	/* The pass already cleared colour; hyperspace is the grey it clears to, so
	   the record carries it and the next view's clear paints it. */
	vk.clearColor[0] = c;
	vk.clearColor[1] = c;
	vk.clearColor[2] = c;
	vk.clearColor[3] = 1.0f;

	backEnd.isHyperspace = qtrue;
}

void RB_BeginDrawingView (void) {

/* the reference renderer's tr_backend.c carries this note in full */
	RB_2D_Flush();

	backEnd.in2D = qfalse;

	/* A 2D rect cached here survives the 3D pass, and the next 2D call would
	   then skip re-issuing a scissor that pass overwrote: drop the pair. */
	R_Invalidate2DWindowCache();

	SetViewportAndScissor();

	VK_DepthState( qtrue, qtrue );	/* the 3D pass opens its gate, as retail's does */
	VK_StencilState( qfalse, VK_ALWAYS, 0, 255 );	/* retail's traced menu issues no stencil call */
	VK_State( VKS_DEFAULT );

	/* The pass clears colour and depth; the colour is this view's choice and
	   rides on the pass's clear colour. */
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

	/* the reference renderer cleared the bits its view asked for, inside the view.  The pass
	   already opened with LOAD_OP_CLEAR, but that colour is whatever the
	   previous frame's view chose, so a 2D-only frame would open on a clear it
	   never asked for.  Clearing here makes this view's choice the one that
	   lands.  (Old-Adreno wants a draw-based clear below driver 512.762.12;
	   that is a device-matrix item.) */
	{
		VkClearAttachment attachment;
		VkClearRect rect;

		memset( &attachment, 0, sizeof( attachment ) );
		attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		attachment.clearValue.color.float32[0] = vk.clearColor[0];
		attachment.clearValue.color.float32[1] = vk.clearColor[1];
		attachment.clearValue.color.float32[2] = vk.clearColor[2];
		attachment.clearValue.color.float32[3] = vk.clearColor[3];

		memset( &rect, 0, sizeof( rect ) );
		rect.rect.offset.x = backEnd.viewParms.viewportX;
		rect.rect.offset.y = vkConfig.vidHeight - backEnd.viewParms.viewportY
							 - backEnd.viewParms.viewportHeight;
		rect.rect.extent.width = (uint32_t)( backEnd.viewParms.viewportWidth > 0
											  ? backEnd.viewParms.viewportWidth : 0 );
		rect.rect.extent.height = (uint32_t)( backEnd.viewParms.viewportHeight > 0
											   ? backEnd.viewParms.viewportHeight : 0 );
		rect.baseArrayLayer = 0;
		rect.layerCount = 1;

		qvk.CmdClearAttachments( VK_Shade_CommandBuffer(), 1, &attachment, 1, &rect );
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
		if ( drawSurf->sort == (unsigned)oldSort && *drawSurf->surface != SF_SPRITE ) {
			rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			continue;
		}
		oldSort = (int)drawSurf->sort;
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

		if (shader != oldShader || fogNum != oldFogNum || dlightMap != oldDlightMap
			|| (shader->noMerge & 1)
			|| ( entityNum != oldEntityNum && !shader->entityMergable )
			|| ( bStaticModel != oldbStaticModel && !shader->entityMergable )) {
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
			if (!r_fullbright->integer) {
				RB_Static_BuildDLights();
			}

			if (!backEnd.currentStaticModel->bStaticLightingCalculated) {
				RB_Static_SetupModel();
			}
		}
		else if (backEnd.currentEntity->e.tiki && shader->needsLSpherical)
		{
			R_Sphere_SetupEntity();
		}

		if (*drawSurf->surface == SF_SPRITE) {
			backEnd.shaderStartTime = ((refSprite_t*)drawSurf->surface)->shaderTime;
		}

		rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
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

	/* The frame-carry law: the walk reads the record's copy of the frame, never
	   the live tr.* the front end may already be refilling. */
	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	backEnd.pc.c_surfaces += 0;

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

/* R_DrawDebugNumber is the reference renderer's debug overlay; the str class and the wire path
   it draws through are not part of this milestone.  The row names the site. */
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

/* the debug floating string: the reference renderer drives it through the font's sgl renderer,
   which this module has not been asked for yet.  The row names the site. */
void R_DrawFloatingString( fontheader_t *font, const char *text, const vec3_t org,
						   const vec4_t color, float scale, int maxlen )
{
	(void)font; (void)text; (void)org; (void)color; (void)scale; (void)maxlen;
}

/* ---- the 2D walk, which the M2 fixtures prove: one pass, pipeline, bind -- */
void RB_StageIterator2D( void )
{
	int stage;

	for ( stage = 0; stage < tess.numPasses; stage++ ) {
		if ( !tess.xstages[stage] || !tess.xstages[stage]->active ) {
			continue;
		}
		RB_StagePass( tess.xstages[stage] );
	}
}
