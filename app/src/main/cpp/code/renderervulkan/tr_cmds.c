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
#include "vk_local.h"

#include <string.h>

typedef struct {
	void *cmds;
	int smpFrame;
	int frameCount;
	int boneArena;
	int backEndMsec;
} rbFrame_t;

static rbFrame_t rb_frames[SMP_FRAMES];
static rbFrame_t *rb_activeFrame = NULL;
static volatile rbFrame_t *rb_lastExecuted = NULL;
static int rb_frameCount = 0;
static int rb_issueRows = 0;

static int vk_frameCmds[3] = { RC_VK_BEGIN, RC_VK_PRESENT, RC_END_OF_LIST };

static cvar_t *r_smp;

int RB_FrameSmpFrame( void )
{
	return rb_activeFrame ? rb_activeFrame->smpFrame : 0;
}

int RB_FrameCount( void )
{
	return rb_activeFrame ? rb_activeFrame->frameCount : rb_frameCount;
}

int RB_FrameBoneArena( void )
{
	return rb_activeFrame ? rb_activeFrame->boneArena : 0;
}

qboolean R_SmpActive( void )
{
	return vk.smpActive;
}

qboolean R_PendingRenderCommands( void )
{
	return qfalse;
}

void R_IssuePendingRenderCommands( void )
{
	R_SyncRenderThread();
}

void R_SyncRenderThread( void )
{
	if ( !vk.smpActive || VKimp_IsRenderThread() ) {
		return;
	}

	VKimp_FrontEndSleep();
}

void RB_ExecuteRenderCommands( const void *data )
{
	const int *cmds = (const int *)data;
	int slot = rb_activeFrame ? rb_activeFrame->smpFrame : 0;

	while ( 1 ) {
		switch ( *cmds ) {
		case RC_VK_BEGIN:
			if ( !VK_Frame_Begin( slot ) ) {
				return;
			}
			if ( !VK_Frame_Acquire( slot ) ) {
				return;
			}
			VK_Frame_RecordClear( slot, vk.clearColor );
			break;

		case RC_VK_PRESENT:
			if ( !VK_Frame_Submit( slot ) ) {
				return;
			}
			if ( !VK_Frame_Present( slot ) ) {
				return;
			}
			break;

		case RC_END_OF_LIST:
		default:
			rb_lastExecuted = rb_activeFrame;
			return;
		}

		cmds++;
	}
}

void RB_RenderThread( void )
{
	const void *data;

	data = VKimp_RendererSleep();

	while ( data != NULL ) {
		rbFrame_t *frame = (rbFrame_t *)data;

		rb_activeFrame = frame;
		RB_ExecuteRenderCommands( frame->cmds );
		frame->backEndMsec = 0;
		rb_activeFrame = NULL;
		rb_lastExecuted = frame;

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

	frame = &rb_frames[vk.frameSlot];
	frame->cmds = vk_frameCmds;
	frame->smpFrame = vk.frameSlot;
	frame->frameCount = ++rb_frameCount;
	frame->boneArena = vk.frameSlot;
	frame->backEndMsec = 0;

	if ( vk.smpActive && !VKimp_IsRenderThread() ) {
		VKimp_WakeRenderer( frame );
		vk.frameSlot = ( vk.frameSlot + 1 ) % SMP_FRAMES;
		return;
	}

	rb_activeFrame = frame;
	RB_ExecuteRenderCommands( frame->cmds );
	rb_activeFrame = NULL;

	if ( VK_TraceOn( VK_TRACE_CVAR, 1 ) && ( rb_issueRows < 8 || ( rb_issueRows % 600 ) == 0 ) ) {
		VK_Guard_Row( "VK:smp", "side=inline slot=%d frames=%d", frame->smpFrame, rb_frameCount );
		rb_issueRows++;
	}

	rb_lastExecuted = frame;
	vk.frameSlot = ( vk.frameSlot + 1 ) % SMP_FRAMES;
}

void R_InitCommandBuffers( void )
{
	r_smp = ri.Cvar_Get( "r_smp", "0", CVAR_ARCHIVE | CVAR_LATCH );
	vk.smpActive = r_smp->integer ? qtrue : qfalse;
	vk.frameSlot = 0;

	memset( rb_frames, 0, sizeof( rb_frames ) );

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
