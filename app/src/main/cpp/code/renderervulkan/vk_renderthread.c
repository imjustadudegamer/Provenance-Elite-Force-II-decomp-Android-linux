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
#include "vk_local.h"

#ifdef USE_INTERNAL_SDL_HEADERS
#	include "SDL.h"
#else
#	include <SDL.h>
#endif

volatile qboolean	vkRenderThreadActive = qfalse;

static SDL_Thread	*vk_renderThread = NULL;
static SDL_mutex	*vk_smpMutex = NULL;
static SDL_cond		*vk_renderCommandsEvent = NULL;
static SDL_cond		*vk_renderCompletedEvent = NULL;
static void			( *vk_renderThreadFunction )( void ) = NULL;
static void			*vk_smpData = NULL;
static volatile int	vk_smpShutdown = 0;
static SDL_threadID	vk_renderThreadId = 0;
static int			vk_handshakeViolations = 0;
static volatile int	vk_walkingSlot = -1;
/* Time the front end spent parked waiting for the handoff slot.  This is the
   overlap measurement: a front end that never parks is not being held up by the
   walk thread at all, and one that parks on every frame is running serially
   behind it no matter what the frame rate says. */
static volatile int	vk_parkMsec = 0;

static SDL_threadID VKimp_MainThreadId( void )
{
	static SDL_threadID mainThread = 0;

	if ( !mainThread ) {
		mainThread = SDL_ThreadID();
	}

	return mainThread;
}

qboolean VKimp_IsRenderThread( void )
{
	return ( vk_renderThread && SDL_ThreadID() == vk_renderThreadId ) ? qtrue : qfalse;
}

unsigned long VKimp_ThreadID( void )
{
	return (unsigned long)SDL_ThreadID();
}

qboolean VKimp_MainOwnsFrame( void )
{
	return ( !vk_renderThread || SDL_ThreadID() == VKimp_MainThreadId() ) ? qtrue : qfalse;
}

static int SDLCALL VKimp_RenderThreadWrapper( void *arg )
{
	vkRenderThreadActive = qtrue;
	vk_renderThreadId = SDL_ThreadID();

	VK_Guard_Row( "VK:smp", "born" );

	vk_renderThreadFunction();

	/* The walk function returning is a thread exit, and the front end is parked
	   on the handoff it was about to make.  Naming it and dropping active is
	   what lets that park release instead of running forever. */
	VK_Guard_Row( "VK:smp", "exiting active-was=%d", vkRenderThreadActive ? 1 : 0 );
	vkRenderThreadActive = qfalse;

	SDL_LockMutex( vk_smpMutex );
	vk_smpShutdown = -1;
	SDL_CondSignal( vk_renderCompletedEvent );
	SDL_UnlockMutex( vk_smpMutex );

	return 0;
}

qboolean VKimp_SpawnRenderThread( void ( *function )( void ) )
{
	vk_smpMutex = SDL_CreateMutex();
	vk_renderCommandsEvent = SDL_CreateCond();
	vk_renderCompletedEvent = SDL_CreateCond();

	if ( !vk_smpMutex || !vk_renderCommandsEvent || !vk_renderCompletedEvent ) {
		VK_Guard_Hit( VK_GSITE_SYNC, "thread primitives", VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	vk_renderThreadFunction = function;
	vk_smpShutdown = 0;

	vk_renderThread = SDL_CreateThread( VKimp_RenderThreadWrapper, "render thread", NULL );

	if ( vk_renderThread == NULL ) {
		VK_Guard_Hit( VK_GSITE_SYNC, "SDL_CreateThread", VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	VK_Guard_Row( "VK:smp", "spawned frames-in-flight=%d", VK_FRAMES_IN_FLIGHT );

	return qtrue;
}

void *VKimp_RendererSleep( void )
{
	void *data;
	qboolean got;

	SDL_LockMutex( vk_smpMutex );

	/* A frame already waiting is this call's answer: taking it and then testing
	   "is the slot empty" slept on a second wake that the front end was never
	   going to send, because it parks until the slot it just filled is drained. */
	got   = ( vk_smpData != NULL ) ? qtrue : qfalse;
	data  = vk_smpData;
	vk_smpData = NULL;

	/* Ownership of the slot is claimed here, under the mutex the front end's own
	   wait reads, so there is no window in which the slot looks free to a front
	   end coming back to it while the walk is between the handoff and the read.
	   Claiming it in the walk body would leave exactly that window. */
	if ( got && data != NULL ) {
		vk_walkingSlot = RB_FrameSlot( data );
	}

	SDL_CondSignal( vk_renderCompletedEvent );

	while ( !vk_smpShutdown && !got ) {
		SDL_CondWait( vk_renderCommandsEvent, vk_smpMutex );
		if ( vk_smpData != NULL ) {
			got        = qtrue;
			data       = vk_smpData;
			vk_smpData = NULL;
			vk_walkingSlot = RB_FrameSlot( data );
			SDL_CondSignal( vk_renderCompletedEvent );
		}
	}

	if ( vk_smpShutdown ) {
		data = NULL;
		vk_walkingSlot = -1;
	}

	SDL_UnlockMutex( vk_smpMutex );

	return data;
}

void VKimp_FrontEndSleep( void )
{
	SDL_LockMutex( vk_smpMutex );

	/* A dead walk thread never drains the slot, so the liveness test is what
	   keeps this park finite.  Shutdown is -1 once the thread has returned. */
	while ( vk_smpData && !vk_smpShutdown ) {
		SDL_CondWait( vk_renderCompletedEvent, vk_smpMutex );
	}

	SDL_UnlockMutex( vk_smpMutex );
}

qboolean VKimp_RenderThreadAlive( void )
{
	return ( vk_renderThread && vkRenderThreadActive && !vk_smpShutdown ) ? qtrue : qfalse;
}

int VKimp_ParkMsec( void )
{
	int msec = vk_parkMsec;

	vk_parkMsec = 0;

	return msec;
}

/* Handing a frame to the walk thread releases the front end, so the front end
   can come back to this slot while the walk is still reading it.  With two
   slots that is the very next frame, and the walk reads byte offsets the front
   end is appending to -- which is how a walk saw a frame body that continued
   into the next frame's prologue and then left its pass without a present.
   These three make "the walk thread owns this list" a testable state. */

void VKimp_WalkBegin( int slot )
{
	SDL_LockMutex( vk_smpMutex );
	vk_walkingSlot = slot;
	SDL_UnlockMutex( vk_smpMutex );
}

void VKimp_WalkEnd( int slot )
{
	SDL_LockMutex( vk_smpMutex );
	if ( vk_walkingSlot == slot ) {
		vk_walkingSlot = -1;
	}
	/* Nothing waits on this today that did not wait before: the front end's
	   frame boundary waits here instead of on the handoff, so it needs the same
	   signal the handoff already raises. */
	SDL_CondBroadcast( vk_renderCompletedEvent );
	SDL_UnlockMutex( vk_smpMutex );
}

qboolean VKimp_SlotBusy( int slot )
{
	return ( vk_walkingSlot == slot ) ? qtrue : qfalse;
}

void VKimp_FrontEndWaitSlot( int slot )
{
	if ( !vk_smpMutex ) {
		return;
	}

	SDL_LockMutex( vk_smpMutex );
	while ( vk_walkingSlot == slot && !vk_smpShutdown ) {
		SDL_CondWait( vk_renderCompletedEvent, vk_smpMutex );
	}
	SDL_UnlockMutex( vk_smpMutex );
}

/* Nothing handed off and nothing being walked: the walk thread is parked and
   holds no frame.  This is the state the swapchain's own external
   synchronization requirement asks for, and the front end needs it before it
   destroys one -- a fullscreen toggle on the main thread was destroying a
   swapchain the walk thread could still be acquiring from and presenting to. */
void VKimp_FrontEndWaitIdle( void )
{
	if ( !vk_smpMutex ) {
		return;
	}

	SDL_LockMutex( vk_smpMutex );
	while ( !vk_smpShutdown && ( vk_smpData != NULL || vk_walkingSlot != -1 ) ) {
		SDL_CondWait( vk_renderCompletedEvent, vk_smpMutex );
	}
	SDL_UnlockMutex( vk_smpMutex );
}

void VKimp_WakeRenderer( void *data )
{
	qboolean parking = qfalse;
	Uint32	parkStart = 0;

	SDL_LockMutex( vk_smpMutex );

	/* A frame the render thread has picked up but not finished is normal; a
	   frame it has not picked up yet is the only reason a wake would be
	   illegal, and turning the front end away is a deadlock by construction
	   whenever the render thread is slower than one frame -- the front end has
	   nothing else to do but offer the next frame and be refused forever.
	   Wait for the handoff slot instead, so a wake is always legal when the
	   render thread is free to take one. */
	while ( vk_smpData != NULL && !vk_smpShutdown ) {
		if ( !parking ) {
			parking = qtrue;
			parkStart = SDL_GetTicks();
		}
		SDL_CondWait( vk_renderCompletedEvent, vk_smpMutex );
	}

	if ( parking ) {
		vk_parkMsec += (int)( SDL_GetTicks() - parkStart );
	}

	vk_smpData = data;
	SDL_CondSignal( vk_renderCommandsEvent );

	SDL_UnlockMutex( vk_smpMutex );
}

void VKimp_ShutdownRenderThread( void )
{
	if ( vk_renderThread == NULL ) {
		return;
	}

	SDL_LockMutex( vk_smpMutex );
	vk_smpShutdown = 1;
	SDL_CondBroadcast( vk_renderCommandsEvent );
	SDL_UnlockMutex( vk_smpMutex );

	SDL_WaitThread( vk_renderThread, NULL );
	vk_renderThread = NULL;

	SDL_DestroyMutex( vk_smpMutex );
	vk_smpMutex = NULL;
	SDL_DestroyCond( vk_renderCommandsEvent );
	vk_renderCommandsEvent = NULL;
	SDL_DestroyCond( vk_renderCompletedEvent );
	vk_renderCompletedEvent = NULL;

	vkRenderThreadActive = qfalse;
	vk_smpShutdown = 0;
	vk_renderThreadId = 0;
	vk_walkingSlot = -1;
}

int VKimp_HandshakeViolations( void )
{
	return vk_handshakeViolations;
}

/* The upload pool is one pool, and a VkCommandPool is host-synchronised for its
   WHOLE lifetime -- not only for allocate and free: Vulkan 1.4 s5.3 ("Host
   Synchronization" for VkCommandPool) makes "use via recording commands on any
   command buffers allocated from the pool" part of the same external
   synchronization.  Locking vkAllocateCommandBuffers and vkFreeCommandBuffers
   alone therefore still let the walk thread record (vkCmdPipelineBarrier) out
   of the pool while the main thread allocated or freed from it, which is the
   "VkCommandPool is simultaneously used in current thread ... and thread ..."
   the layer reported for vkAllocateCommandBuffers, vkFreeCommandBuffers and
   vkCmdPipelineBarrier.  One lock per one-shot upload transaction -- taken
   before the allocate and released after the free -- is that whole lifetime. */
static SDL_mutex *vk_uploadMutex;

/* Created once, from the sync init below, before either thread can reach an
   upload: a lazily created mutex has a window in which both threads create one
   and only one of them guards. */
void VK_Upload_Init( void )
{
	if ( !vk_uploadMutex ) {
		vk_uploadMutex = SDL_CreateMutex();
	}
}

void VK_Upload_Begin( void )
{
	VK_Upload_Init();

	SDL_LockMutex( vk_uploadMutex );
}

void VK_Upload_End( void )
{
	SDL_UnlockMutex( vk_uploadMutex );
}

/* A layout transition is a write to the image, so an image the walk thread may
   still sample has to be quiesced first: the walk thread parked and holding no
   frame is the only state in which no command buffer that references the image
   can still be recorded-and-unsubmitted (Vulkan 1.4 s7.7/s7.9: a resource in
   use by a pending submission must not be written).  Called from the render
   thread it would wait on its own walk, so it is a no-op there. */
void VK_Upload_Quiesce( void )
{
	if ( !vk.smpActive || VKimp_IsRenderThread() ) {
		return;
	}

	VKimp_FrontEndWaitIdle();
}

/* The graphics queue is host-synchronised, and under r_smp two threads reach
   it: the walk thread submits frames while the main thread submits texture
   uploads, publishes and readbacks.  The validation layer named it -- "object
   of type VkQueue is simultaneously used in current thread ... and thread ...".
   These three are every way the port touches a queue, so the lock lives here
   and no call site has to remember it. */
static SDL_mutex *vk_queueMutex;

static SDL_mutex *VK_Queue_Mutex( void )
{
	if ( !vk_queueMutex ) {
		/* VK_Sync_Init is what makes this unreachable -- VK_Frame_Init calls it
		   before it creates a pool or touches a queue.  If an ordering change
		   ever gets here, name it: SDL_LockMutex(NULL) is a silent no-op, so
		   without the row the section would simply not exist. */
		VK_Guard_Row( "VK:sync", "queue mutex missing; VK_Sync_Init was not reached" );
	}

	return vk_queueMutex;
}

/* Both host-synchronised objects under r_smp -- the graphics queue and the
   upload command pool -- take their lock from here, once, on the main thread in
   VK_Frame_Init before either thread can reach a pool or a queue.  Creating
   either mutex on first use leaves a window in which both threads find it null
   and create one each, so only one of them guards. */
void VK_Sync_Init( void )
{
	if ( !vk_queueMutex ) {
		vk_queueMutex = SDL_CreateMutex();
	}

	VK_Upload_Init();
}

VkResult VK_Queue_Submit( VkQueue queue, uint32_t submitCount,
						  const VkSubmitInfo *submits, VkFence fence )
{
	VkResult result;

	SDL_LockMutex( VK_Queue_Mutex() );
	result = qvk.QueueSubmit( queue, submitCount, submits, fence );
	SDL_UnlockMutex( VK_Queue_Mutex() );

	return result;
}

VkResult VK_Queue_Present( VkQueue queue, const VkPresentInfoKHR *present )
{
	VkResult result;

	SDL_LockMutex( VK_Queue_Mutex() );
	result = qvk.QueuePresentKHR( queue, present );
	SDL_UnlockMutex( VK_Queue_Mutex() );

	return result;
}

VkResult VK_Queue_WaitIdle( VkQueue queue )
{
	VkResult result;

	SDL_LockMutex( VK_Queue_Mutex() );
	result = qvk.QueueWaitIdle( queue );
	SDL_UnlockMutex( VK_Queue_Mutex() );

	return result;
}

/* The upload pool is shared by design, not by accident: registration loads
   textures on the main thread, and a frame that reaches for art it does not
   have yet -- a font atlas the first 2D draw touches, a cinematic frame -- does
   the same work on the walk thread.  This boot did both, 15455 against 1, and
   the validation layer named the pool rather than the queue.  A command pool is
   host synchronised like the queue, so its allocate, free and reset take the
   same lock. */
VkResult VK_Pool_AllocBuffers( VkDevice device, const VkCommandBufferAllocateInfo *info,
							   VkCommandBuffer *buffers )
{
	VkResult result;

	SDL_LockMutex( VK_Queue_Mutex() );
	result = qvk.AllocateCommandBuffers( device, info, buffers );
	SDL_UnlockMutex( VK_Queue_Mutex() );

	return result;
}

void VK_Pool_FreeBuffers( VkDevice device, VkCommandPool pool, uint32_t count,
						  const VkCommandBuffer *buffers )
{
	SDL_LockMutex( VK_Queue_Mutex() );
	qvk.FreeCommandBuffers( device, pool, count, buffers );
	SDL_UnlockMutex( VK_Queue_Mutex() );
}
