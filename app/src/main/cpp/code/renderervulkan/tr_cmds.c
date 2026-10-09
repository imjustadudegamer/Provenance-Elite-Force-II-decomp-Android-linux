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

	list->cmds     = cmds;
	list->capacity = newBytes;

	/* a grow is the expected capacity event, not a failure; the refused grow has
	   its own PRINT_WARNING at the caller */
	if ( R_ProbeStatsOn() ) {
		VK_Guard_Row( "VK:cmdbuf", "slot=%d old=%d new=%d demand=%d f=%d",
					  RB_FrameSmpFrame(), oldBytes, newBytes, demand, RB_FrameCount() );
	}

	return qtrue;
}
static rbFrame_t		rb_frames[SMP_FRAMES];
static rbFrame_t		*rb_activeFrame = NULL;
static int				rb_frameCount = 0;
static int				rb_issueRows = 0;
static qboolean			rb_inlineFallback;

/* Walk time, accumulated by whichever thread walked.  backEnd.backEndMsec was
   only ever zeroed and read, so the fps row's be= was always 0 and a decline
   could not be attributed to the back end at all.  Written on the walk thread
   and drained by the front end once per row, which is a benign race for a
   diagnostic that reports a five second window.  Microseconds, because the
   phase split is microseconds: an integer-millisecond walk total left a
   quantisation error of up to a millisecond per frame in the row's accounting
   residual, and a residual that is the ruler's own error cannot show a bucket
   that does not sum. */
static volatile long long	rb_walkUsec;

cvar_t *r_smp;

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

/* The walk's own cost attribution.  be= is the whole walk and it contains a
   blocking frame-fence wait the device owns, so a saturated walk does not say
   whether to cut CPU work or device work -- on the full-resolution operator
   boot that wait was 8.9 ms of a ~20 ms frame.  These phases split the walk:
   the surface generation (skinning and tessellation), the vertex pack into the
   ring, the shader/descriptor bind, the command recording, and the fence wait.
   Gated with the r_perfFps row that prints them; the counters are written on
   the walk thread and drained by the front end, the same benign race rb_walkUsec
   already carries. */
static long long	s_walkPhaseUsec[VK_WP_PHASES];
static long long	s_walkShadeUsec[VK_WS_PARTS];
static long long	s_srfT0 = -1;
static long long	s_walkVerts;
static long long	s_walkSurfs;
static long long	s_walkFrames;
static long long	s_walkElisions[VK_ELIDE_HITS];

qboolean VK_WalkStatOn( void )
{
	static cvar_t *cv;

	if ( !cv ) {
		cv = ri.Cvar_Get( "r_perfFps", "1", CVAR_ARCHIVE );
	}

	/* r_perfFps is the family switch; qa_probe is the master gate and the
	   timing itself is the cost, so a probe-off boot pays neither. */
	return ( cv && cv->integer && R_ProbeStatsOn() ) ? qtrue : qfalse;
}

void VK_WalkStat_Add( int phase, long long usec )
{
	if ( phase >= 0 && phase < VK_WP_PHASES ) {
		s_walkPhaseUsec[phase] += usec;
	}
}

/* One named part of the shade window, charged and drained with the window. */
void VK_WalkStat_ShadeAdd( int part, long long usec )
{
	if ( part >= 0 && part < VK_WS_PARTS ) {
		s_walkShadeUsec[part] += usec;
	}
}

void VK_WalkStat_TakeShade( long long *usecOut )
{
	int i;

	for ( i = 0; i < VK_WS_PARTS; i++ ) {
		usecOut[i] = s_walkShadeUsec[i];
		s_walkShadeUsec[i] = 0;
	}
}

/* The srf window's split.  srf= names the RB_EndSurface/RB_BeginSurface pair
   the shader-change branch issues, and it is the largest shade part at about
   3us per stage draw; these sub-parts name where it goes, the same way dec/
   rot/mv already do for their own window.  The charge sites live inside the
   two functions, which the sprite and world walks also call, so the window
   arms around the pair and every site is one predictable test when it is not
   armed.  OTHER is the residual, so the parts sum to srf=. */
static long long	s_walkSrfUsec[VK_WSS_PARTS];
static long long	s_walkSrfTotal;
static long long	s_srfArmT0;
static long long	s_srfArmNested;
static int			s_srfArmed;

void VK_SrfPart_Arm( void )
{
	if ( VK_WalkStatOn() ) {
		s_srfArmed = 1;
		s_srfArmT0 = BE_Microseconds64();
		s_srfArmNested = VK_WalkStat_Nested();
	}
}

void VK_SrfPart_Disarm( void )
{
	/* the whole window, net of what its own flushes charged elsewhere; OTHER is
	   the difference between this and the named sub-parts, so the split sums to
	   srf= however much the two functions skip */
	if ( s_srfArmed && s_srfArmT0 >= 0 ) {
		long long usec = BE_Microseconds64() - s_srfArmT0
						 - VK_WalkStat_NestedSince( s_srfArmNested );

		s_walkSrfTotal += usec > 0 ? usec : 0;
	}
	s_srfArmed = 0;
	s_srfArmT0 = -1;
}

void VK_SrfPart_Open( void )
{
	if ( s_srfArmed ) {
		s_srfT0 = BE_Microseconds64();
	}
}

void VK_SrfPart_Close( int part )
{
	long long usec;

	if ( !s_srfArmed || s_srfT0 < 0 ) {
		return;
	}

	usec = BE_Microseconds64() - s_srfT0;
	s_srfT0 = -1;

	if ( usec < 0 ) {
		usec = 0;
	}
	if ( part >= 0 && part < VK_WSS_PARTS ) {
		s_walkSrfUsec[part] += usec;
	}
}

void VK_WalkStat_TakeSrf( long long *usecOut )
{
	int i;

	for ( i = 0; i < VK_WSS_PARTS; i++ ) {
		usecOut[i] = s_walkSrfUsec[i];
		s_walkSrfUsec[i] = 0;
	}
	usecOut[VK_WSS_OTHER] = 0;
}

long long VK_WalkStat_TakeSrfTotal( void )
{
	long long t = s_walkSrfTotal;

	s_walkSrfTotal = 0;
	return t;
}

/* The present seam's children, drained with the phase row that prints them. */
static long long	s_walkPresentUsec[VK_WSUB_SLOTS];

void VK_WalkStat_PresentAdd( int which, long long usec )
{
	if ( which >= 0 && which < VK_WSUB_SLOTS ) {
		s_walkPresentUsec[which] += usec;
	}
}

void VK_WalkStat_TakePresent( long long *usecOut )
{
	int i;

	for ( i = 0; i < VK_WSUB_SLOTS; i++ ) {
		usecOut[i] = s_walkPresentUsec[i];
		s_walkPresentUsec[i] = 0;
	}
}

/* The binds a surface loop's own window contains.  The per-surface bookkeeping
   flushes the previous batch inside its window, so an outer wrap that simply
   added its elapsed time would charge those binds twice. */
long long VK_WalkStat_Nested( void )
{
	return s_walkPhaseUsec[VK_WP_BIND] + s_walkPhaseUsec[VK_WP_REC] + s_walkPhaseUsec[VK_WP_PACK];
}

/* The same reading as a window's own contribution.  The three counters are
   cumulative since the last drain and the front end drains them at a 1 s
   cadence, so `Nested() - n0` is negative whenever a drain landed between the
   window's open and its close -- and a negative "nested" makes the window's
   subtraction turn into an addition of the stale count, which is a bucket
   inflated by an amount that never happened (the residual is then large and
   negative, which the row reads as an accounting error).  After a drain the
   counter holds exactly what was added since it, so in that case it IS the
   window's contribution. */
long long VK_WalkStat_NestedSince( long long n0 )
{
	long long n = VK_WalkStat_Nested();

	return ( n < n0 ) ? n : ( n - n0 );
}

void VK_WalkStat_Frame( int verts, int surfs )
{
	s_walkVerts += verts;
	s_walkSurfs += surfs;
	s_walkFrames++;
}

void VK_WalkStat_Take( long long *usecOut, long long *vertsOut, long long *surfsOut,
					   long long *framesOut )
{
	int i;

	for ( i = 0; i < VK_WP_PHASES; i++ ) {
		usecOut[i] = s_walkPhaseUsec[i];
		s_walkPhaseUsec[i] = 0;
	}
	*vertsOut = s_walkVerts;
	*surfsOut = s_walkSurfs;
	*framesOut = s_walkFrames;
	s_walkVerts = 0;
	s_walkSurfs = 0;
	s_walkFrames = 0;
}

void VK_WalkStat_Count( int what )
{
	if ( what >= 0 && what < VK_ELIDE_HITS ) {
		s_walkElisions[what]++;
	}
}

void VK_WalkStat_TakeElisions( long long *out )
{
	int i;

	for ( i = 0; i < VK_ELIDE_HITS; i++ ) {
		out[i] = s_walkElisions[i];
		s_walkElisions[i] = 0;
	}
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

/* ---- the frame's paint order, as the walk meets it ----------------------- */

/* The readback says what colour a rect holds; only the recorded order says
   whether the fill that colour came from was walked before or after the 3D
   batch it hid.  Every 2D box -- the widget fills, and the menu background wash
   among them -- and every surface batch's own rect is noted here in walk order,
   so one boot answers a composition question a target read cannot.  Gated like
   the other rows and emitted when the sequence changes. */
#define VK_ORDER_MAX	28

static char		s_orderLine[1024];
static int		s_orderLen;
static int		s_orderCount;
static int		s_orderMore;
static unsigned	s_orderHash;
static unsigned	s_orderLast;
static int		s_orderFrames;
static int		s_orderHave;

static void VK_Order_Note( const char *text )
{
	int len = (int)strlen( text );

	if ( s_orderCount >= VK_ORDER_MAX || s_orderLen + len + 2 >= (int)sizeof( s_orderLine ) ) {
		s_orderMore++;
		return;
	}
	if ( s_orderCount ) {
		s_orderLine[s_orderLen++] = '|';
	}
	memcpy( s_orderLine + s_orderLen, text, len + 1 );
	s_orderLen += len;
	s_orderCount++;
}

/* Emitted at the present, the one point where the frame's sequence is whole. */
static void VK_Order_Frame( void )
{
	char	full[1152];

	if ( !s_orderCount ) {
		return;
	}

	if ( s_orderMore ) {
		Com_sprintf( full, sizeof( full ), "%s|+%d", s_orderLine, s_orderMore );
	} else {
		Q_strncpyz( full, s_orderLine, sizeof( full ) );
	}

	s_orderHash = 0;
	{
		const char *p = full;

		while ( *p ) {
			s_orderHash = ( s_orderHash ^ (unsigned char)*p++ ) * 16777619u;
		}
	}

	s_orderFrames++;
	/* the transcript is an instrument: a quiet boot resets it and prints none */
	if ( R_ProbeStatsOn()
		 && ( !s_orderHave || s_orderHash != s_orderLast || ( s_orderFrames % 600 ) == 0 ) ) {
		VK_Guard_Row( "VK:order", "%s", full );
		s_orderLast = s_orderHash;
		s_orderHave = qtrue;
	}

	s_orderLine[0] = 0;
	s_orderLen = 0;
	s_orderCount = 0;
	s_orderMore = 0;
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
	int gpuLastCat = -1;

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
			VK_Guard_Row( "VK:frame", "walk-refuse head-op=%d slot=%d f=%d",
						  op, slot, RB_FrameCount() );
			goto abandoned;
		}
		if ( op == RC_VK_BEGIN ) {
			inFrame = qtrue;
		}

		/* The 2D batch is recorded when it flushes, not where its commands sit,
		   so a 3D batch that follows one in the list would overtake it in the
		   command buffer.  The front end paints a widget's background and then
		   asks for that widget's 3D model, so the CONTROLS stardome landed
		   under its own full-screen fill.  Flushing here restores the recorded
		   order, and the flush is charged to the 2D class it belongs to. */
		if ( op == RC_DRAW_SURFS || op == RC_SPRITE_SURFS ) {
			const drawSurfsCommand_t *ocmd = ( const drawSurfsCommand_t * )cmds;
			long long flushT0 = VK_WALK_T0();
			long long flushNest0 = VK_WALK_NEST0();

			RB_2D_Flush();
			VK_WALK_ADD_NET( flushT0, flushNest0, VK_WP_UI );

			{
				char	note[80];

				Com_sprintf( note, sizeof( note ), "%s(%d,%d %dx%d n=%d)",
							 op == RC_DRAW_SURFS ? "surf" : "spr",
							 ocmd->refdef.x, ocmd->refdef.y, ocmd->refdef.width, ocmd->refdef.height,
							 ocmd->numDrawSurfs );
				VK_Order_Note( note );
			}
		}

		/* One GPU timestamp per class change, written before the case runs: the
		   interval that follows a mark is the work that mark's class records, so
		   a mark after the case would charge the draws to the previous class.
		   The frame's single pass is split into clear, world, sprites and the 2D
		   batch; the terminal mark before pass end closes the last one. */
		{
			int gpuCat = -1;

			if ( op == RC_DRAW_SURFS ) {
				gpuCat = VK_GPU_WORLD;
			} else if ( op == RC_SPRITE_SURFS ) {
				gpuCat = VK_GPU_SPRITE;
			} else if ( op >= RC_2D_COLOR && op <= RC_2D_STRETCH_RAW ) {
				gpuCat = VK_GPU_UI;
			}

			if ( gpuCat >= 0 && gpuCat != gpuLastCat ) {
				VK_PerfGpu_Mark( slot, gpuCat );
				gpuLastCat = gpuCat;
			}
		}

		/* The 2D family is the UI cost; the surface cases time their own ring
		   phases.  The case body is wrapped so whatever a 2D case does that no
		   inner phase claims is still charged to UI, and the walk's per-case
		   overhead is measured rather than dropped. */
		{
		long long walkT0 = VK_WALK_T0();
		long long caseNest0 = VK_WALK_NEST0();

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
				long long acqT0 = VK_WALK_T0();
				long long openT0;

				if ( !VK_Frame_Acquire( slot ) ) {
					VK_WALK_ADD( acqT0, VK_WP_ACQ );
					goto abandoned;
				}
				VK_WALK_ADD( acqT0, VK_WP_ACQ );

				openT0 = VK_WALK_T0();
				RB_FrameOpen( slot );
				if ( !VK_Frame_RecordBegin( slot ) ) {
					VK_WALK_ADD( openT0, VK_WP_OPEN );
					goto abandoned;
				}
				VK_WALK_ADD( openT0, VK_WP_OPEN );
			}
			passOpen = qtrue;
			break;

		case RC_2D_SET_WINDOW:
		{
			setWindow2DCommand_t *cmd = ( setWindow2DCommand_t * )cmds;

			/* The queued quads were authored under the window in force, so they
			   close as a span and the new state opens as the next one: the batch
			   does NOT end here.  Only the immediate run has to close, because
			   its vertices carry no span -- and only one of the two is ever open.
			   A split that cannot be represented falls back to the flush this
			   case always did. */
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

			/* The state just applied is the one the next quads belong to.  A qfalse
			   cannot happen here -- RB_2D_SpanBreak reserved the room and the state
			   was just named -- and closes the batch if it ever does. */
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

			{
				char	note[96];

				/* The colour as well as the alpha: an opaque full-screen imm box is
				   a flash or the menu's own background, and which one it is is the
				   colour.  backEnd.color2D is the colour in force here, which is
				   the one the box is drawn with (RB_ImmColor4f bakes it). */
				Com_sprintf( note, sizeof( note ), "box(%.0f,%.0f %.0fx%.0f a%d rgb=%d,%d,%d)",
							 (double)cmd->x, (double)cmd->y, (double)cmd->w, (double)cmd->h,
							 (int)backEnd.color2D[3],
							 (int)backEnd.color2D[0], (int)backEnd.color2D[1], (int)backEnd.color2D[2] );
				VK_Order_Note( note );
			}

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
				long long uiT0 = VK_WALK_T0();
				long long uiNest0 = VK_WALK_NEST0();
				long long outT0;

				RB_2D_Flush();
				VK_WALK_ADD_NET( uiT0, uiNest0, VK_WP_UI );

				VK_Order_Frame();

				outT0 = VK_WALK_T0();
				{
					long long seamT0 = VK_WALK_T0();

					VK_Frame_RecordEnd( slot );
					VK_WALK_ADD_SUB( seamT0, VK_WSUB_RECEND );
				}
				passOpen = qfalse;
				{
					long long seamT0 = VK_WALK_T0();

					if ( !VK_Frame_Submit( slot ) ) {
						VK_WALK_ADD_SUB( seamT0, VK_WSUB_SUBMIT );
						VK_WALK_ADD( outT0, VK_WP_OUT );
						goto abandoned;
					}
					VK_WALK_ADD_SUB( seamT0, VK_WSUB_SUBMIT );
				}
				{
					long long seamT0 = VK_WALK_T0();

					if ( !VK_Frame_Present( slot ) ) {
						VK_WALK_ADD_SUB( seamT0, VK_WSUB_PRESENT );
						VK_WALK_ADD( outT0, VK_WP_OUT );
						goto abandoned;
					}
					VK_WALK_ADD_SUB( seamT0, VK_WSUB_PRESENT );
				}
				VK_WALK_ADD( outT0, VK_WP_OUT );

				/* the presented image is the composited result: sampling it here is
				   what separates "the dome drew" from "the dome is on screen" */
				{
					long long pixT0 = VK_WALK_T0();

					VK_Shade_ReadbackFrame( vk.acquiredImage );
					VK_WALK_ADD( pixT0, VK_WP_PIX );
				}
					/* the save-game thumbnail rides the same seam: a swap image is only
					   readable once the frame that drew it has been presented */
					if ( R_Screenshot_Pending() ) {
						long long shotT0 = VK_WALK_T0();

						R_Screenshot_Flush( vk.acquiredImage );
						VK_WALK_ADD( shotT0, VK_WP_PIX );
					}
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

		if ( op >= RC_2D_COLOR && op <= RC_2D_STRETCH_RAW ) {
			VK_WALK_ADD_NET( walkT0, caseNest0, VK_WP_UI );
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
		if ( R_ProbeStatsOn()
			 && ( rb_issueRows < 8 || ( rb_issueRows % 600 ) == 0 ) ) {
			VK_Guard_Row( "VK:smp", "walk-start f=%d slot=%d", rb_frameCount + 1, frame->smpFrame );
		}

		rb_activeFrame = frame;
		/* Claimed in the handoff, so the front end cannot be writing this list
		   while it is read.  The clear stays inside the claim because used is
		   the front end's write position: clearing it after releasing the slot
		   would clobber a frame the front end had already started. */
		VKimp_WalkBegin( frame->smpFrame );
		walkStart = VK_WALK_T0();
		if ( frame->list ) {
			RB_ExecuteRenderCommands( frame->list->cmds );
			frame->list->used = 0;
		}
		if ( walkStart >= 0 ) {
			rb_walkUsec += BE_Microseconds64() - walkStart;
		}
		{
			/* the demand the frame just walked: slots are vertices, and the
			   census counts the surfaces the ring stored */
			int used, bytes, indexes, drops, surfs;

			VK_VertexRing_FrameDemand( &used, &bytes, &indexes, &drops, &surfs );
			VK_WalkStat_Frame( used, surfs );
		}
		frame->backEndMsec = 0;
		rb_activeFrame = NULL;
		VKimp_WalkEnd( frame->smpFrame );

		if ( R_ProbeStatsOn()
			 && ( rb_issueRows < 8 || ( rb_issueRows % 600 ) == 0 ) ) {
			VK_Guard_Row( "VK:smp", "walk-done f=%d slot=%d", rb_frameCount, frame->smpFrame );
		}

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
				VK_Guard_Row( "VK:smp", "walk-thread-gone frames=%d walking-inline", rb_frameCount );
			}
			rb_activeFrame = frame;
			walkStart = VK_WALK_T0();
			if ( list ) {
				RB_ExecuteRenderCommands( list->cmds );
				list->used = 0;
			}
			if ( walkStart >= 0 ) {
				rb_walkUsec += BE_Microseconds64() - walkStart;
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
	walkStart = VK_WALK_T0();
	if ( list ) {
		RB_ExecuteRenderCommands( list->cmds );
		list->used = 0;
	}
	if ( walkStart >= 0 ) {
		rb_walkUsec += BE_Microseconds64() - walkStart;
	}
	rb_activeFrame = NULL;

	if ( R_ProbeStatsOn()
		 && ( rb_issueRows < 8 || ( rb_issueRows % 600 ) == 0 ) ) {
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
			VK_Guard_Row( "VK:guard", "site=sync detail=\"render thread refused, running serial\" result=success(0) n=1" );
			vk.smpActive = qfalse;
		}
	}

	if ( R_ProbeStatsOn() ) {
		VK_Guard_Row( "VK:smp", "active=%d frames-in-flight=%d", vk.smpActive ? 1 : 0, VK_FRAMES_IN_FLIGHT );
	}
}

void R_ShutdownCommandBuffers( void )
{
	VKimp_ShutdownRenderThread();
	vk.smpActive = qfalse;
}
