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

/* The frame's recorded commands, per slot.  Capacity grows: a widget-heavy
   screen's record demand is not knowable statically, and a full list silently
   dropped whatever the caller queued -- a colour, a scissor, a text run. */
typedef struct {
	byte	*cmds;
	int		used;
	int		capacity;
	int		highWater;
} vkCommandList_t;

/* the bound the growth stops at */
#define VK_COMMAND_MAX_BYTES	( 8 * 1024 * 1024 )

typedef struct {
	vkCommandList_t	*list;
	int smpFrame;
	int frameCount;
	int boneArena;
	int backEndMsec;
} rbFrame_t;

static vkCommandList_t	*rb_commandLists[SMP_FRAMES];

/* Grow one slot's list: 2x, capped, named.  The old block stays in the
   renderer pool until shutdown, so the bound is what keeps the cost finite. */
static qboolean RB_CommandListGrow( vkCommandList_t *list, int demand )
{
	int		oldBytes = list->capacity;
	int		newBytes = oldBytes;
	byte	*cmds;

	if ( !list || demand <= list->capacity || list->capacity >= VK_COMMAND_MAX_BYTES ) {
		return qfalse;
	}

	while ( newBytes < demand && newBytes < VK_COMMAND_MAX_BYTES ) {
		newBytes *= 2;
	}
	if ( newBytes > VK_COMMAND_MAX_BYTES ) {
		newBytes = VK_COMMAND_MAX_BYTES;
	}

	cmds = ( byte * )ri.Malloc( newBytes );
	if ( !cmds ) {
		return qfalse;
	}

	/* The grow runs inside R_GetCommandBuffer, i.e. mid-record, so the commands
	   already in this slot are the ones the render thread is about to walk: the
	   bytes have to move with the list.  Swapping in a fresh block and keeping
	   `used` read uninitialised memory for every earlier command of the frame --
	   the 2D screen went out that way while the 3D scene after it still drew. */
	if ( list->used > 0 && list->cmds ) {
		Com_Memcpy( cmds, list->cmds, list->used );
	}

	list->cmds     = cmds;
	list->capacity = newBytes;

	/* a grow is the expected capacity event, not a failure; the refused grow has
	   its own PRINT_WARNING at the caller */
	return qtrue;
}
static rbFrame_t		rb_frames[SMP_FRAMES];
static rbFrame_t		*rb_activeFrame = NULL;
static int				rb_frameCount = 0;
static int				rb_issueRows = 0;
static qboolean			rb_inlineFallback;

/* walk time, accumulated by whichever thread walked: backEnd.backEndMsec was only ever zeroed and read, so the fps row's be= was
   always 0.  Written on the walk thread and drained once per row (a benign race for a five second window); microseconds, because an
   integer-millisecond total left a quantisation error a residual cannot be read through. */
static volatile long long	rb_walkUsec;
/* the same total, frame-scoped: RE_EndFrame hands it to Com_Frame's [SMP:frame]
   row, whose renderBack= read backEnd.backEndMsec and so was always 0.  Draining
   it here keeps the fps row's own window total intact.  Mirrors the GLES3 twin's
   RB_FrameBackEndMsec/RB_FrameBackEndReset, so both back ends report the same
   quantity and a backend comparison reads two live numbers, not one live and
   one dead. */
static volatile long long	rb_walkFrameUsec;

cvar_t *r_smp;

static void RB_WalkUsecAdd( long long usec )
{
	rb_walkUsec     += usec;
	rb_walkFrameUsec += usec;
}

int RB_WalkMsec( void )
{
	long long usec = rb_walkUsec;

	rb_walkUsec = 0;

	return (int)( usec / 1000 );
}

long long RB_WalkUsec64( void )
{
	long long usec = rb_walkUsec;

	rb_walkUsec = 0;

	return usec;
}

/* the frame's own walk total, read and cleared at the frame boundary */
long long RB_WalkFrameUsec64( void )
{
	long long usec = rb_walkFrameUsec;

	rb_walkFrameUsec = 0;

	return usec;
}

static vkCommandList_t *RB_CommandList( void )
{
	return rb_commandLists[tr.smpFrame];
}

/* The front end's write position on the slot it is about to fill.  Only the
   front end calls this, and only once it owns the slot, so the two writers of
   used never overlap. */
void RB_CommandListReset( void )
{
	vkCommandList_t *list = RB_CommandList();

	if ( list ) {
		/* The slot's last demand sizes the next frame: growing here costs one
		   copy-free swap before anything is recorded into the block. */
		if ( list->highWater * 4 >= list->capacity * 3 ) {
			RB_CommandListGrow( list, list->highWater + ( int )sizeof( int ) * 8 );
		}
		list->used      = 0;
		list->highWater = 0;
	}
}

int RB_FrameSmpFrame( void )
{
	return rb_activeFrame ? rb_activeFrame->smpFrame : tr.smpFrame;
}

int RB_FrameCount( void )
{
	return rb_activeFrame ? rb_activeFrame->frameCount : rb_frameCount;
}

int RB_FrameSlot( const void *frame )
{
	return frame ? ( ( const rbFrame_t * )frame )->smpFrame : -1;
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

/* the list goes to the walk whole, from RE_EndFrame: a between-frames fragment has no pass to land in and the slot's own submission may
   still be in flight, so walking it would record into a live command buffer -- the sublevel hang.  It is dropped and named instead. */
void R_FlushRenderCommands( void )
{
	vkCommandList_t *list = RB_CommandList();

	if ( !tr.registered || vk_frameOpen || !R_PendingRenderCommands() ) {
		return;
	}

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

	if ( bytes > VK_COMMAND_MAX_BYTES - (int)sizeof( int ) ) {
		ri.Error( ERR_FATAL, "R_GetCommandBuffer: bad size %i", bytes );
	}

	if ( list->used + bytes + reservedBytes + (int)sizeof( int ) > list->capacity ) {
		/* A record that cannot land is a record the caller never sees -- the
		   colour, scissor or text run a later widget's state then replaced.
		   The list grows instead; the warning below is the assertion that
		   this never has to refuse under the bound. */
		if ( !RB_CommandListGrow( list, list->used + bytes + reservedBytes + (int)sizeof( int ) ) ) {
			ri.Printf( PRINT_WARNING, "Failed to allocate render command of size %d (used %d of %d)\n",
					   bytes, list->used, list->capacity );
			return NULL;
		}
	}

	buf = list->cmds + list->used;
	list->used += bytes;
	if ( list->used > list->highWater ) {
		list->highWater = list->used;
	}

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

	/* A registration whose warm budget ran out finishes here, a slice a frame.
	   This is also the ONLY drain of the mid-pass park table VK_Shade_Pipeline
	   fills: a pipeline first asked for inside an open pass (the appended fog
	   stages above all, and any effect permutation registered after the load's
	   warm walk) is parked there instead of being built, and its draw is refused
	   until the table drains.  With this call removed the drain never ran and
	   those draws were refused on every frame -- fog and blended effects gone on
	   Vulkan while the pre-warmed opaque world still drew.  It must run before
	   RC_VK_BEGIN opens the next pass (HEAD tr_cmds.c:616). */
	VK_Shade_WarmFrameStep( 2 );

	while ( 1 ) {
		/* A draw that leaked out of the frame window sits ahead of the frame
		   marker: RE_BeginFrame appends RC_VK_BEGIN after it.  Recording it would
		   write into a command buffer nothing has begun, so the walk refuses the
		   list rather than hand the driver an unbegun buffer.  An empty list just
		   ends here. */
		int op = *( const int * )cmds;

		if ( !inFrame && op != RC_VK_BEGIN ) {
			if ( op == RC_END_OF_LIST ) {
				return;
			}

			goto abandoned;
		}
		if ( op == RC_VK_BEGIN ) {
			inFrame = qtrue;
		}

		/* the 2D batch is recorded when it flushes, not where its commands sit: the front end paints a widget's background and then asks for
		   that widget's 3D model, so the CONTROLS stardome landed under its own full-screen fill.  Flushing here restores the recorded order. */
		if ( op == RC_DRAW_SURFS || op == RC_SPRITE_SURFS ) {

			RB_2D_Flush();

		}

		/* One GPU timestamp per class change, written before the case runs: the
		   interval that follows a mark is the work that mark's class records, so
		   a mark after the case would charge the draws to the previous class.
		   The frame's single pass is split into clear, world, sprites and the 2D
		   batch; the terminal mark before pass end closes the last one. */

		/* The 2D family is the UI cost; the surface cases time their own ring
		   phases.  The case body is wrapped so whatever a 2D case does that no
		   inner phase claims is still charged to UI, and the walk's per-case
		   overhead is measured rather than dropped. */
		{

		switch ( op ) {
		case RC_VK_BEGIN:
			cmds += PAD( sizeof( int ), sizeof( void * ) );
			/* Begin owns the fence wait and the acquire, each timed at its own
			   site; timing it here as well charged both to open and made the
			   row's accounting error negative -- the split only sums if no
			   bucket contains another. */
			if ( !VK_Frame_Begin( slot ) ) {
				goto abandoned;
			}
			{

				if ( !VK_Frame_Acquire( slot ) ) {
					/* Begin succeeded, so the slot owns a reset fence and a
					   recording buffer the abandon label below will not close:
					   Retire only runs for a pass that opened.  Hand it back. */
					VK_Frame_Abandon( slot );
					goto abandoned;
				}

				RB_FrameOpen( slot );
				if ( !VK_Frame_RecordBegin( slot ) ) {
					/* same prepared slot, and the pass still never opened */
					VK_Frame_Abandon( slot );
					goto abandoned;
				}
			}
			passOpen = qtrue;
			break;

		case RC_2D_SET_WINDOW:
		{
			setWindow2DCommand_t *cmd = ( setWindow2DCommand_t * )cmds;

			/* the queued quads were authored under the window in force, so they close as a span and the new state opens as the next one -- the
			   batch does NOT end here.  Only the immediate run has to close, since its vertices carry no span; an unrepresentable split falls back to the flush. */
			RB_2D_FlushImmediate();
			if ( !RB_2D_SpanBreak() ) {
				RB_2D_Flush();
			}

			VK_Draw_Set2D( vk.commandBuffers[slot], cmd->x, cmd->y, cmd->w, cmd->h,
						   cmd->left, cmd->right, cmd->bottom, cmd->top, cmd->n, cmd->f );

			/* The state just applied is the one the next quads belong to.  A qfalse
			   cannot happen here -- RB_2D_SpanBreak reserved the room and the state
			   was just named -- and closes the batch if it ever does. */
			if ( !RB_2D_SpanNote() ) {
				RB_2D_Flush();
			}
			cmds += PAD( sizeof( *cmd ), sizeof( void * ) );
			break;
		}

		case RC_2D_SCISSOR:
		{
			scissor2DCommand_t *cmd = ( scissor2DCommand_t * )cmds;

			RB_2D_FlushImmediate();
			if ( !RB_2D_SpanBreak() ) {
				RB_2D_Flush();
			}

			VK_Draw_SetScissor( vk.commandBuffers[slot], cmd->x, cmd->y, cmd->width, cmd->height );

			if ( !RB_2D_SpanNote() ) {
				RB_2D_Flush();
			}
			cmds += PAD( sizeof( *cmd ), sizeof( void * ) );
			break;
		}

		case RC_2D_ORIGIN:
		{
			origin2DCommand_t *cmd = ( origin2DCommand_t * )cmds;

			VK_Draw_Set2DOrigin( cmd->x, cmd->y );
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

			RB_FontRunDraw( vk.commandBuffers[slot], cmd, ( fontGlyph_t * )( cmd + 1 ) );
			cmds += PAD( sizeof( *cmd ) + cmd->numGlyphs * (int)sizeof( fontGlyph_t ), sizeof( void * ) );
			break;
		}

		case RC_VK_PRESENT:
			cmds += PAD( sizeof( int ), sizeof( void * ) );
			{

				RB_2D_Flush();


				{

					VK_Frame_RecordEnd( slot );
				}
				passOpen = qfalse;
				{

					if ( !VK_Frame_Submit( slot ) ) {
						goto abandoned;
					}
				}
				{

					/* the thumbnail is read while this frame is still ours: past
					   the present the image belongs to the presentation engine,
					   and a read there answers with whatever it left behind */
					if ( R_Screenshot_Pending() ) {

						R_Screenshot_Flush( vk.acquiredImage );
					}

					if ( !VK_Frame_Present( slot ) ) {
						goto abandoned;
					}
				}

				/* the presented image is the composited result: sampling it here is
				   what separates "the dome drew" from "the dome is on screen" */
			}
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
	}

	/* RE_BeginFrame opens the pass and RE_EndFrame's PRESENT closes it.  Anything
	   that ends the walk in between -- including a fragment a flush was handed --
	   retires here: the pass is closed and the acquired image is submitted and
	   presented back, so no walk path can hold a swapchain image. */
abandoned:
	if ( passOpen ) {
		VK_Frame_Retire( slot );
	}
}

void RB_RenderThread( void )
{
	const void *data;

	data = VKimp_RendererSleep();

	while ( data != NULL ) {
		rbFrame_t *frame = (rbFrame_t *)data;
		long long	walkStart;

		/* The walk thread's own milestones.  A boot that stops with the front end
		   parked on the handoff and no rows here says the thread never walked;
		   these say how far it got before it stopped. */
		rb_activeFrame = frame;
		/* the handoff claimed the slot (vk_renderthread.c RendererSleep);
		   re-writing it here cost two mutex ops a frame */
		walkStart = BE_Microseconds64();
		if ( frame->list ) {
			RB_ExecuteRenderCommands( frame->list->cmds );
			frame->list->used = 0;
		}
		if ( walkStart >= 0 ) {
			RB_WalkUsecAdd( BE_Microseconds64() - walkStart );
		}
		{
			/* the demand the frame just walked: slots are vertices, and the
			   census counts the surfaces the ring stored */
			int used, bytes, indexes, drops, surfs;

			VK_VertexRing_FrameDemand( &used, &bytes, &indexes, &drops, &surfs );
		}
		frame->backEndMsec = 0;
		rb_activeFrame = NULL;
		VKimp_WalkEnd( frame->smpFrame );

		rb_issueRows++;

		data = VKimp_RendererSleep();
	}
}

void R_IssueRenderCommands( void )
{
	rbFrame_t *frame;
	long long	walkStart;
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
		if ( !VKimp_RenderThreadAlive() ) {
			/* The walk thread is gone.  Handing off would park on a handoff
			   nothing will ever drain, so this frame walks inline and the boot
			   names the fallback once. */
			if ( !rb_inlineFallback ) {
				rb_inlineFallback = qtrue;

			}
			rb_activeFrame = frame;
			walkStart = BE_Microseconds64();
			if ( list ) {
				RB_ExecuteRenderCommands( list->cmds );
				list->used = 0;
			}
			if ( walkStart >= 0 ) {
				RB_WalkUsecAdd( BE_Microseconds64() - walkStart );
			}
			rb_activeFrame = NULL;
			vk.frameSlot = ( vk.frameSlot + 1 ) % SMP_FRAMES;
			return;
		}

		VKimp_WakeRenderer( frame );
		vk.frameSlot = ( vk.frameSlot + 1 ) % SMP_FRAMES;
		return;
	}

	rb_activeFrame = frame;
	walkStart = BE_Microseconds64();
	if ( list ) {
		RB_ExecuteRenderCommands( list->cmds );
		list->used = 0;
	}
	if ( walkStart >= 0 ) {
		RB_WalkUsecAdd( BE_Microseconds64() - walkStart );
	}
	rb_activeFrame = NULL;

	vk.frameSlot = ( vk.frameSlot + 1 ) % SMP_FRAMES;
}

void R_InitCommandBuffers( void )
{
	int i;

	/* forced before the read-only flag: a +set r_smp 0 creates the cvar first */
	r_smp = ri.Cvar_Get( "r_smp", "1", 0 );
	ri.Cvar_Set( "r_smp", "1" );
	r_smp->flags |= CVAR_ROM;
	vk.smpActive = r_smp->integer ? qtrue : qfalse;
	vk.frameSlot = 0;

	memset( rb_frames, 0, sizeof( rb_frames ) );

	for ( i = 0; i < SMP_FRAMES; i++ ) {
		if ( !rb_commandLists[i] ) {
			/* Recorded into every frame and read by the walk, and
			   ri.Hunk_Clear frees the arena at every map change: the list
			   must come from the renderer pool, not the arena.  h_low is not
			   a lifetime here -- CL_RefStaticMalloc ignores the preference. */
			rb_commandLists[i] = ( vkCommandList_t * )ri.Malloc( sizeof( vkCommandList_t ) );
			memset( rb_commandLists[i], 0, sizeof( *rb_commandLists[i] ) );
			rb_commandLists[i]->capacity = MAX_RENDER_COMMANDS;
			rb_commandLists[i]->cmds = ( byte * )ri.Malloc( MAX_RENDER_COMMANDS );
		}
		rb_commandLists[i]->used = 0;
		rb_commandLists[i]->highWater = 0;
	}

	if ( vk.smpActive && !vkRenderThreadActive ) {
		if ( !VKimp_SpawnRenderThread( RB_RenderThread ) ) {

			vk.smpActive = qfalse;
		}
	}

}

void R_ShutdownCommandBuffers( void )
{
	VKimp_ShutdownRenderThread();
	vk.smpActive = qfalse;
}
