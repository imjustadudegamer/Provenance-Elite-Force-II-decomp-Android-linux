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
 * tr_cmds.c -- the front end's recorded-command buffer.
 *
 * The reference renderer records every frame into a byte list the render thread walks;
 * this module records into the same shape and walks it into the frame's Vulkan
 * command buffer.  A frame's list is RC_VK_BEGIN (open the pass and clear),
 * the 2D commands the UI recorded while the render thread was parked,
 * RC_VK_PRESENT (close, submit, present), RC_END_OF_LIST -- so a threaded frame
 * and a serial frame run the same walk, and the 2D draw order the UI produced
 * is the order the walk performs.
 *
 * One list per frame slot: with two frames in flight the front end fills slot
 * N while the render thread is still walking slot N-1, which is the same
 * reason the reference renderer keeps backEndData[SMP_FRAMES].
 */

#include "tr_local.h"

#include <string.h>

typedef struct {
	byte	cmds[MAX_RENDER_COMMANDS];
	int		used;
} vkCommandList_t;

typedef struct {
	vkCommandList_t	*list;
	int smpFrame;
	int frameCount;
	int boneArena;
	int backEndMsec;
} rbFrame_t;

static vkCommandList_t	*rb_commandLists[SMP_FRAMES];
static rbFrame_t		rb_frames[SMP_FRAMES];
static rbFrame_t		*rb_activeFrame = NULL;
static int				rb_frameCount = 0;
static int				rb_issueRows = 0;

cvar_t *r_smp;

static vkCommandList_t *RB_CommandList( void )
{
	return rb_commandLists[tr.smpFrame];
}

int RB_FrameSmpFrame( void )
{
	return rb_activeFrame ? rb_activeFrame->smpFrame : tr.smpFrame;
}

int RB_FrameCount( void )
{
	return rb_activeFrame ? rb_activeFrame->frameCount : rb_frameCount;
}

int RB_FrameBoneArena( void )
{
	return rb_activeFrame ? rb_activeFrame->boneArena : tr.smpFrame;
}

qboolean R_SmpActive( void )
{
	return vk.smpActive;
}

qboolean R_PendingRenderCommands( void )
{
	vkCommandList_t *list = RB_CommandList();

	return ( list && list->used > 0 ) ? qtrue : qfalse;
}

qboolean vk_frameOpen = qfalse;

/* The list goes to the walk whole, from RE_EndFrame: a frame is one pass from
   RC_VK_BEGIN to RC_VK_PRESENT, so a mid-frame flush is held until it ends.
   The GL twin replays whatever a load-time flush finds, because GL draws outside
   begin/end just land on the bound framebuffer.  Here a between-frames fragment
   has no pass to land in and the slot's own submission may still be in flight,
   so walking it would record into a live command buffer -- the sublevel hang.
   It is dropped and named instead; the next frame redraws it. */
void R_FlushRenderCommands( void )
{
	vkCommandList_t *list = RB_CommandList();
	static int droppedRows;

	if ( !tr.registered || vk_frameOpen || !R_PendingRenderCommands() ) {
		return;
	}

	if ( droppedRows < 8 ) {
		/* The opcode head names the recorder that drew outside the window, so the
		   boot log identifies the leaker without a backtrace. */
		int op0 = list->used >= (int)sizeof( int ) ? *( const int * )list->cmds : -1;
		int op1 = list->used >= 16 ? *( const int * )( list->cmds + 8 ) : -1;
		VK_Guard_Row( "VK:frame", "drop-fragment slot=%d bytes=%d f=%d op=%d,%d",
					  RB_FrameSmpFrame(), list->used, RB_FrameCount(), op0, op1 );
	}
	droppedRows++;

	list->used = 0;
}

void R_IssuePendingRenderCommands( void )
{
	R_SyncRenderThread();
	R_FlushRenderCommands();
}

void R_SyncRenderThread( void )
{
	if ( !vk.smpActive || VKimp_IsRenderThread() ) {
		return;
	}

	VKimp_FrontEndSleep();
}

void *R_GetCommandBufferReserved( int bytes, int reservedBytes )
{
	vkCommandList_t *list = RB_CommandList();
	byte *buf;

	if ( !list ) {
		return NULL;
	}

	bytes = PAD( bytes, sizeof( void * ) );

	if ( list->used + bytes + reservedBytes + (int)sizeof( int ) > MAX_RENDER_COMMANDS ) {
		if ( bytes > MAX_RENDER_COMMANDS - (int)sizeof( int ) ) {
			ri.Error( ERR_FATAL, "R_GetCommandBuffer: bad size %i", bytes );
		}
		ri.Printf( PRINT_WARNING, "Failed to allocate render command of size %d\n", bytes );
		return NULL;
	}

	buf = list->cmds + list->used;
	list->used += bytes;

	return buf;
}

void *R_GetCommandBuffer( int bytes )
{
	return R_GetCommandBufferReserved( bytes, sizeof( int ) );
}

void RB_ExecuteRenderCommands( const void *data )
{
	const byte *cmds = ( const byte * )data;
	int slot = rb_activeFrame ? rb_activeFrame->smpFrame : 0;
	qboolean passOpen = qfalse;
	qboolean inFrame = qfalse;

	while ( 1 ) {
		/* A draw that leaked out of the frame window sits ahead of the frame
		   marker: RE_BeginFrame appends RC_VK_BEGIN after it.  Recording it would
		   write into a command buffer nothing has begun, so the walk refuses the
		   list rather than hand the driver an unbegun buffer.  An empty list just
		   ends here. */
		if ( !inFrame && *( const int * )cmds != RC_VK_BEGIN ) {
			if ( *( const int * )cmds == RC_END_OF_LIST ) {
				return;
			}
			VK_Guard_Row( "VK:frame", "walk-refuse head-op=%d slot=%d f=%d",
						  *( const int * )cmds, slot, RB_FrameCount() );
			goto abandoned;
		}
		if ( *( const int * )cmds == RC_VK_BEGIN ) {
			inFrame = qtrue;
		}

		switch ( *( const int * )cmds ) {
		case RC_VK_BEGIN:
			cmds += PAD( sizeof( int ), sizeof( void * ) );
			if ( !VK_Frame_Begin( slot ) ) {
				goto abandoned;
			}
			if ( !VK_Frame_Acquire( slot ) ) {
				goto abandoned;
			}
			RB_FrameOpen( slot );
			VK_Frame_RecordBegin( slot );
			passOpen = qtrue;
			break;

		case RC_2D_SET_WINDOW:
		{
			setWindow2DCommand_t *cmd = ( setWindow2DCommand_t * )cmds;

			/* Pending 2D runs close first, or their quads draw under the next window's viewport and ortho. */
			RB_2D_Flush();

			VK_Draw_Set2D( vk.commandBuffers[slot], cmd->x, cmd->y, cmd->w, cmd->h,
						   cmd->left, cmd->right, cmd->bottom, cmd->top, cmd->n, cmd->f );
			cmds += PAD( sizeof( *cmd ), sizeof( void * ) );
			break;
		}

		case RC_2D_SCISSOR:
		{
			scissor2DCommand_t *cmd = ( scissor2DCommand_t * )cmds;

			RB_2D_Flush();

			VK_Draw_SetScissor( vk.commandBuffers[slot], cmd->x, cmd->y, cmd->width, cmd->height );
			cmds += PAD( sizeof( *cmd ), sizeof( void * ) );
			break;
		}

		case RC_2D_COLOR:
		{
			color2DCommand_t *cmd = ( color2DCommand_t * )cmds;

			RB_2D_SetColor( cmd->color );
			cmds += PAD( sizeof( *cmd ), sizeof( void * ) );
			break;
		}

		case RC_2D_STRETCH_PIC:
		{
			stretchPic2DCommand_t *cmd = ( stretchPic2DCommand_t * )cmds;

			RB_2D_StretchPic( cmd->shader, cmd->x, cmd->y, cmd->w, cmd->h,
							  cmd->s1, cmd->t1, cmd->s2, cmd->t2 );
			cmds += PAD( sizeof( *cmd ), sizeof( void * ) );
			break;
		}

		case RC_2D_STRETCH_PIC2:
		{
			stretchPic2Command_t *cmd = ( stretchPic2Command_t * )cmds;

			RB_2D_StretchPic2( cmd->shader, cmd->x, cmd->y, cmd->w, cmd->h,
							   cmd->s1, cmd->t1, cmd->s2, cmd->t2, cmd->sx, cmd->sy );
			cmds += PAD( sizeof( *cmd ), sizeof( void * ) );
			break;
		}

		case RC_2D_TILE_PIC:
		{
			tilePic2DCommand_t *cmd = ( tilePic2DCommand_t * )cmds;

			RB_2D_TilePic( cmd->shader, cmd->x, cmd->y, cmd->w, cmd->h, cmd->picw, cmd->pich );
			cmds += PAD( sizeof( *cmd ), sizeof( void * ) );
			break;
		}

		case RC_2D_TILE_PIC_OFFSET:
		{
			tilePic2DCommand_t *cmd = ( tilePic2DCommand_t * )cmds;

			RB_2D_TilePicOffset( cmd->shader, cmd->x, cmd->y, cmd->w, cmd->h,
								 cmd->picw, cmd->pich, cmd->offsetX, cmd->offsetY );
			cmds += PAD( sizeof( *cmd ), sizeof( void * ) );
			break;
		}

		case RC_2D_TRIANGLE_PIC:
		{
			trianglePic2DCommand_t *cmd = ( trianglePic2DCommand_t * )cmds;

			RB_2D_TrianglePic( cmd->shader, ( const vec2_t * )cmd->points,
							   ( const vec2_t * )cmd->texCoords );
			cmds += PAD( sizeof( *cmd ), sizeof( void * ) );
			break;
		}

		case RC_2D_BOX:
		{
			box2DCommand_t *cmd = ( box2DCommand_t * )cmds;

			RB_2D_Box( cmd->x, cmd->y, cmd->w, cmd->h, cmd->blend ? qtrue : qfalse );
			cmds += PAD( sizeof( *cmd ), sizeof( void * ) );
			break;
		}

		case RC_2D_LINE_LOOP:
		{
			lineLoop2DCommand_t *cmd = ( lineLoop2DCommand_t * )cmds;

			RB_2D_LineLoop( ( const vec2_t * )( cmd + 1 ), cmd->count,
							cmd->stipple_factor, cmd->stipple_mask );
			cmds += PAD( (int)sizeof( *cmd ) + cmd->count * (int)sizeof( vec2_t ), sizeof( void * ) );
			break;
		}

		case RC_2D_SHADER_TIME:
		{
			shaderTime2DCommand_t *cmd = ( shaderTime2DCommand_t * )cmds;

			RB_2D_ShaderTime( cmd->startTime );
			cmds += PAD( sizeof( *cmd ), sizeof( void * ) );
			break;
		}

		case RC_2D_STRETCH_RAW:
		{
			stretchRaw2DCommand_t *cmd = ( stretchRaw2DCommand_t * )cmds;

			RB_StretchRawDraw( cmd );
			cmds += PAD( sizeof( *cmd ), sizeof( void * ) );
			break;
		}

		case RC_2D_FONT_STRING:
		{
			fontString2DCommand_t *cmd = ( fontString2DCommand_t * )cmds;

			RB_FontRunDraw( cmd, ( fontGlyph_t * )( cmd + 1 ) );
			cmds += PAD( sizeof( *cmd ) + cmd->numGlyphs * (int)sizeof( fontGlyph_t ), sizeof( void * ) );
			break;
		}

		case RC_VK_PRESENT:
			cmds += PAD( sizeof( int ), sizeof( void * ) );
			RB_2D_Flush();
			VK_Frame_RecordEnd( slot );
			passOpen = qfalse;
			if ( !VK_Frame_Submit( slot ) ) {
				goto abandoned;
			}
			if ( !VK_Frame_Present( slot ) ) {
				goto abandoned;
			}
			/* the presented image is the composited result: sampling it here is
			   what separates "the dome drew" from "the dome is on screen" */
			VK_Shade_ReadbackFrame( vk.acquiredImage );
			/* Anything after the present belongs to the next pass, not this one. */
			inFrame = qfalse;
			break;

		case RC_DRAW_SURFS:
			cmds = RB_DrawSurfs( cmds );
			break;

		case RC_SPRITE_SURFS:
			cmds = RB_SpriteSurfs( cmds );
			break;

		case RC_END_OF_LIST:
		default:
			goto abandoned;
		}
	}

	/* RE_BeginFrame opens the pass and RE_EndFrame's PRESENT closes it.  Anything
	   that ends the walk in between -- including a fragment a flush was handed --
	   must close it here, or the fence stays reset and the next Begin on the slot
	   waits forever. */
abandoned:
	if ( passOpen ) {
		VK_Frame_Abandon( slot );
	}
}

void RB_RenderThread( void )
{
	const void *data;

	data = VKimp_RendererSleep();

	while ( data != NULL ) {
		rbFrame_t *frame = (rbFrame_t *)data;

		rb_activeFrame = frame;
		if ( frame->list ) {
			RB_ExecuteRenderCommands( frame->list->cmds );
			/* The inline path clears the list it walked; the threaded path has to
			   as well, or the front end appends after frame N's bytes and the next
			   walk of this list starts at offset 0 on frame N's stale commands. */
			frame->list->used = 0;
		}
		frame->backEndMsec = 0;
		rb_activeFrame = NULL;

		if ( VK_TraceOn( VK_TRACE_CVAR, 1 ) && ( rb_issueRows < 8 || ( rb_issueRows % 600 ) == 0 ) ) {
			VK_Guard_Row( "VK:smp", "side=render slot=%d arena=%d violations=%d frames=%d",
						  frame->smpFrame, frame->boneArena,
						  VKimp_HandshakeViolations(), rb_frameCount );
		}

		rb_issueRows++;

		data = VKimp_RendererSleep();
	}
}

void R_IssueRenderCommands( void )
{
	rbFrame_t *frame;
	vkCommandList_t *list = RB_CommandList();

	if ( list ) {
		int *end = ( int * )R_GetCommandBufferReserved( 0, 0 );

		if ( end ) {
			*end = RC_END_OF_LIST;
		}
	}

	/* Every slot on the frame record is tr.smpFrame, as the twin's is: the
	   front end writes the palette into TIKI_Skel_Bones_Arena[tr.smpFrame], so
	   a walk keyed off another counter read a palette nobody wrote. */
	frame = &rb_frames[tr.smpFrame];
	frame->list = list;
	frame->smpFrame = tr.smpFrame;
	frame->frameCount = ++rb_frameCount;
	frame->boneArena = tr.smpFrame;
	frame->backEndMsec = 0;

	if ( vk.smpActive && !VKimp_IsRenderThread() ) {
		VKimp_WakeRenderer( frame );
		vk.frameSlot = ( vk.frameSlot + 1 ) % SMP_FRAMES;
		return;
	}

	rb_activeFrame = frame;
	if ( list ) {
		RB_ExecuteRenderCommands( list->cmds );
		list->used = 0;
	}
	rb_activeFrame = NULL;

	if ( VK_TraceOn( VK_TRACE_CVAR, 1 ) && ( rb_issueRows < 8 || ( rb_issueRows % 600 ) == 0 ) ) {
		VK_Guard_Row( "VK:smp", "side=inline slot=%d frames=%d", frame->smpFrame, rb_frameCount );
		rb_issueRows++;
	}

	vk.frameSlot = ( vk.frameSlot + 1 ) % SMP_FRAMES;
}

void R_InitCommandBuffers( void )
{
	int i;

	r_smp = ri.Cvar_Get( "r_smp", "1", CVAR_ARCHIVE | CVAR_LATCH );
	vk.smpActive = r_smp->integer ? qtrue : qfalse;
	vk.frameSlot = 0;

	memset( rb_frames, 0, sizeof( rb_frames ) );

	for ( i = 0; i < SMP_FRAMES; i++ ) {
		if ( !rb_commandLists[i] ) {
			rb_commandLists[i] = ( vkCommandList_t * )ri.Hunk_Alloc( sizeof( vkCommandList_t ), h_low );
		}
		memset( rb_commandLists[i], 0, sizeof( *rb_commandLists[i] ) );
	}

	if ( vk.smpActive && !vkRenderThreadActive ) {
		if ( !VKimp_SpawnRenderThread( RB_RenderThread ) ) {
			VK_Guard_Row( "VK:guard", "site=sync detail=\"render thread refused, running serial\" result=success(0) n=1" );
			vk.smpActive = qfalse;
		}
	}

	VK_Guard_Row( "VK:smp", "active=%d frames-in-flight=%d", vk.smpActive ? 1 : 0, VK_FRAMES_IN_FLIGHT );
}

void R_ShutdownCommandBuffers( void )
{
	VKimp_ShutdownRenderThread();
	vk.smpActive = qfalse;
}
