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
 * The front end records a frame's surfaces into `tess` exactly as the GLES3
 * twin's front end does; this module's back end walks them onto the frame's
 * command buffer.  One surface is one stage pass: the stage's state tuple
 * becomes a pipeline (vk_draw.c), the stage's bundle becomes a descriptor
 * bind, the tess arrays become a slice of the frame's vertex ring, and the
 * indexes become an indexed draw.  Nothing here touches a GL object -- the
 * GLES3 twin's glDepthMask/glBlendFunc/glBindTexture all become fields of the
 * pipeline and the descriptor bind that already carry them.
 *
 * The 3D stage iterators (sky, lightmapped multitexture, vertex-lit, the GL
 * fog pass) are the next milestone's surface; the 2D/HUD iterator below is
 * the one the menu paints through and it is the same walk the generic
 * iterator performs minus the lighting terms M3's shaders add.
 */

#include "tr_local.h"

#include <string.h>

shaderCommands_t	tess;
backEndState_t		backEnd;
trGlobals_t			tr;
int					r_sequencenumber = 1;
glconfig_t			glConfig;

/* The slot the walk is recording into, or -1 when no frame is walking.  The
   frame layer writes it: it is the one thing that knows when a command buffer
   is open. */
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

/* ---- the tessellation store ---------------------------------------------- */

void RB_CheckOverflow( int verts, int indexes )
{
	if ( tess.numVertexes + verts >= SHADER_MAX_VERTEXES
		 || tess.numIndexes + indexes >= SHADER_MAX_INDEXES ) {
		RB_EndSurface();
		RB_BeginSurface( tess.shader, tess.fogNum );
	}
}

void RB_BeginSurface( shader_t *shader, int fogNum )
{
	shader_t	*state;

	/* Opening a surface resets the tess, which would silently discard anything
	   the 2D batch still holds.  Drawing it first is the one rule that covers
	   every caller of this function. */
	RB_2D_Flush();

	state = shader->remappedShader ? shader->remappedShader : shader;

	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.shader = state;
	tess.dlightBits = 0;
	tess.vertexColorValid = qfalse;
	tess.entityFade = 1.0f;
	tess.shaderTime = 1.0f;
	tess.no_global_fog = qfalse;

	if ( fogNum < 0 ) {
		fogNum = 0;
	}
	tess.fogNum = fogNum;

	tess.xstages = state->unfoggedStages;
	tess.numPasses = state->numUnfoggedPasses;

	/* A surface inside the 2D window takes the 2D walk.  The shader compiler
	   hands a font or HUD shader the vertex-lit fast path (a font shader is
	   created with LIGHTMAP_NONE and the fast path keys on that), and the
	   vertex-lit walk is 3D world geometry: its lighting terms are the M3
	   surface.  In the 2D window the generic walk is the correct one, and it
	   is the one the texture, glyph and box fixtures are proven with. */
	if ( backEnd.in2D ) {
		tess.currentStageIteratorFunc = RB_StageIteratorGeneric;
	} else {
		tess.currentStageIteratorFunc = state->optimalStageIteratorFunc;
	}
}

/* ---- the stage pass: one stage, one pipeline, one bind, one indexed draw.
 * The state tuple is the whole of the fixed-function state the stage asked
 * for: blend from the GLS_SRCBLEND/GLS_DSTBLEND fields, depth from
 * GLS_DEPTHMASK_TRUE and GLS_DEPTHTEST_DISABLE, the cull from the shader's
 * own cullType, and the alpha test from GLS_ATEST_*. */

static qboolean RB_StagePass( shaderStage_t *stage )
{
	vkPipelineKey_t key;
	VkPipeline pipeline;
	VkImageView view;
	VkSampler sampler;
	vkTessVertex_t *ringVerts;
	glIndex_t *ringIndexes;
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
	/* The 2D window is two-sided, as the GLES3 twin's RB_2D_SetWindow is. */
	key.cull = ( backEnd.in2D || tess.shader->lightmapIndex == LIGHTMAP_2D )
			   ? CT_TWO_SIDED : tess.shader->cullType;
	key.depthTest = ( stage->stateBits & GLS_DEPTHTEST_DISABLE ) ? qfalse : qtrue;
	key.depthWrite = ( stage->stateBits & GLS_DEPTHMASK_TRUE ) ? qtrue : qfalse;

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

	memcpy( ringIndexes, tess.indexes, (size_t)tess.numIndexes * sizeof( glIndex_t ) );



	/* The ring is bound per surface: the demo triangle binds its own buffer
	   inside this pass, and a bind is cheaper than reasoning about which one
	   is current. */
	VK_Draw_BindRing( vk.commandBuffers[s_rbSlot] );
	VK_Draw_DrawIndexed( vk.commandBuffers[s_rbSlot], tess.numIndexes, indexBase, vertexBase );

	vk_ring_note( vertexBase + tess.numVertexes, indexBase + tess.numIndexes );

	return qtrue;
}

/* The generic stage iterator: one pass per active stage, which is the whole of
   the 2D/HUD walk.  A shader that needs a 3D term -- spherical lighting, a fog
   stage, a vertex deform -- is the 3D surface's work and is named by its guard
   rather than drawn wrong. */
void RB_StageIteratorGeneric( void )
{
	int stage;

	/* The 3D terms -- spherical lighting, a fog stage, a vertex deform -- are
	   the 3D surface's work.  In the 2D window they do not apply: a font or
	   HUD shader carries the 2D generators R_LoadFontShader gave it, its fog
	   number is zero by construction, and the window's own colour is per
	   vertex. */
	if ( !backEnd.in2D
		 && ( tess.shader->needsLSpherical || tess.fogNum > 0 || tess.shader->numDeforms > 0 ) ) {
		VK_M3_Touched( "3D stage terms (lighting/fog/deform)" );
		return;
	}

	for ( stage = 0; stage < tess.numPasses; stage++ ) {
		if ( !tess.xstages[stage] || !tess.xstages[stage]->active ) {
			continue;
		}
		RB_StagePass( tess.xstages[stage] );
	}
}

void RB_EndSurface( void )
{
	shaderCommands_t *input;

	input = &tess;

	if ( input->numIndexes == 0 ) {
		return;
	}

	if ( input->indexes[SHADER_MAX_INDEXES - 1] != 0 ) {
		ri.Error( ERR_DROP, "RB_EndSurface() - SHADER_MAX_INDEXES hit" );
	}
	if ( input->xyz[SHADER_MAX_VERTEXES - 1][0] != 0 ) {
		ri.Error( ERR_DROP, "RB_EndSurface() - SHADER_MAX_VERTEXES hit" );
	}

	s_rbSlot = vk_back_end_slot;

	if ( s_rbSlot < 0 || vk.commandBuffers[s_rbSlot] == VK_NULL_HANDLE ) {
		/* A surface closed with no frame recording is the front end tidying up
		   between frames; there is no command buffer to record into. */
		tess.numIndexes = 0;
		tess.numVertexes = 0;
		return;
	}

	if ( !tess.currentStageIteratorFunc ) {
		tess.currentStageIteratorFunc = RB_StageIteratorGeneric;
	}

	backEnd.pc.c_shaders++;
	backEnd.pc.c_vertexes += tess.numVertexes;
	backEnd.pc.c_indexes += tess.numIndexes;
	backEnd.pc.c_totalIndexes += tess.numIndexes * tess.numPasses;

	tess.currentStageIteratorFunc();

	tess.numIndexes = 0;
}

void rb_set_slot( int slot )
{
	s_rbSlot = slot;
}

void RB_RecordFramePrologue( void )
{
	VK_VertexRing_ResetFrame();
	s_rbSlot = -1;

	/* Every frame is a fresh command buffer: the window and scissor the last
	   one issued are not in force, so the frame must record them again even
	   when the values did not change. */
	R_Invalidate2DWindowCache();
}

qboolean RB_RecordSurfaces( VkCommandBuffer cmd )
{
	return qtrue;
}

void RB_FramePrologue( void )
{
}

/* ---- the 2D immediate stream --------------------------------------------
 * Boxes and line loops draw through the same ring.  The GLES3 twin keeps a
 * private GL stream with its own program setup; here a run is one pipeline
 * bound once and a slice of the ring filled per box. */

typedef struct {
	qboolean	open;
	int			topology;
	int			blend;
	int			verts;
	int			base;
	qboolean	pending;
	image_t		*image;
	VkPipeline	pipeline;
} vkImmRun_t;

static vkImmRun_t	s_imm;

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

void RB_ImmBeginWithImage( int topology, image_t *image )
{
	vkPipelineKey_t key;
	vkImmRun_t *run = &s_imm;

	if ( run->open && ( run->topology != topology || run->image != image ) ) {
		RB_ImmEnd();
	}

	memset( &key, 0, sizeof( key ) );
	key.topology = topology;
	key.stateBits = run->blend ? ( GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA
								   | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA )
							  : ( GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE );
	key.cull = CT_TWO_SIDED;
	key.depthTest = qfalse;
	key.depthWrite = qfalse;

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
	/* tr.whiteImage is what a GL back end would have had bound for the boxes:
	   painted, not textured. */
	RB_ImmBeginWithImage( topology, tr.whiteImage );
}

void RB_ImmEndDeferred( void )
{
}

/* A run's vertices only reach the ring when the whole run is in hand: the
   colour and texture-coordinate setters rewrite the slice, so committing
   early would let a later setter miss vertices that are already drawn. */
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
	vkTessVertex_t *v;

	v = VK_VertexRing_Reserve( 1, NULL );
	if ( !v ) {
		return;
	}

	v->xyz[0] = x;
	v->xyz[1] = y;
	v->xyz[2] = 0.0f;
	v->st[0] = 0.0f;
	v->st[1] = 0.0f;
	v->color[0] = backEnd.color2D[0];
	v->color[1] = backEnd.color2D[1];
	v->color[2] = backEnd.color2D[2];
	v->color[3] = backEnd.color2D[3];

	s_imm.verts++;
	s_imm.pending = qtrue;
}

void RB_ImmColor4f( float r, float g, float b, float a )
{
	/* The colour lands in the vertices the run is still filling: the ring's
	   tail is the run's own slice, so rewinding four bytes per channel is
	   what a GL immediate colour would have done. */
	vkTessVertex_t *v = VK_VertexRing_Tail( s_imm.verts );
	int i;

	if ( !v ) {
		return;
	}

	for ( i = 0; i < s_imm.verts; i++ ) {
		v[i].color[0] = (byte)( r * 255.0f + 0.5f );
		v[i].color[1] = (byte)( g * 255.0f + 0.5f );
		v[i].color[2] = (byte)( b * 255.0f + 0.5f );
		v[i].color[3] = (byte)( a * 255.0f + 0.5f );
	}
}

void RB_ImmTexCoord2f( float s, float t )
{
	vkTessVertex_t *v = VK_VertexRing_Tail( s_imm.verts );
	int i;

	if ( !v ) {
		return;
	}

	for ( i = 0; i < s_imm.verts; i++ ) {
		v[i].st[0] = s;
		v[i].st[1] = t;
	}
}

void RB_StreamBeginDrawSurf( void )
{
}

void RB_StreamEndDrawSurf( void )
{
}

void RB_ShowImages( qboolean quiet )
{
	( void )quiet;
}

qboolean RB_SmpActive( void )
{
	return vk.smpActive;
}
