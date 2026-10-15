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

	if ( R_ProbeStatsOn() ) {
		VK_Guard_Row( "VK:smp", "born" );
	}

	vk_renderThreadFunction();

	/* The walk function returning is a thread exit, and the front end is parked
	   on the handoff it was about to make.  Naming it and dropping active is
	   what lets that park release instead of running forever. */
	if ( R_ProbeStatsOn() ) {
		VK_Guard_Row( "VK:smp", "exiting active-was=%d", vkRenderThreadActive ? 1 : 0 );
	}
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

	if ( R_ProbeStatsOn() ) {
		VK_Guard_Row( "VK:smp", "spawned frames-in-flight=%d", VK_FRAMES_IN_FLIGHT );
	}

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

/* ---- command pools: one pool per producer, one lock per pool ---------------

   Vulkan 1.4 s6.2, "Command Pools" (vkspec.txt:14741-14745): "Command pools are
   externally synchronized, meaning that a command pool must not be used
   concurrently in multiple threads.  That includes use via recording commands
   on any command buffers allocated from the pool, as well as operations that
   allocate, free, and reset command buffers or the pool itself."  The per-call
   Host Synchronization clauses repeat it for every pool operation
   (vkspec.txt:15000, :15067, :15160, :15277, :15340, :15440 and :18948-18953).

   One shared upload pool therefore cannot be guarded by a lock held only around
   one-shot transactions, because one of its command buffers -- the static
   arena's transfer buffer -- records ACROSS a frame, so its recording window is
   the frame.  Locking that window would serialise the walk behind every upload;
   leaving it out is the "VkCommandPool is simultaneously used in current thread
   ... and thread ..." the layer reported at vkAllocateCommandBuffers.  Two
   mutexes do not fix it either: they do not exclude each other.

   The port instead gives each thread its own pool.  Within a thread use is
   sequential by construction and across threads the pools are disjoint, so
   every pool has exactly one owner and the whole-lifetime rule is met with one
   lock per pool and no frame-long critical section. */

#define VK_POOL_OWNERS	16

typedef struct {
	unsigned long	owner;	/* the only thread that may touch it; 0 = the walk's frame pool */
	VkCommandPool	pool;
	SDL_mutex		*lock;
} vk_poolOwner_t;

static vk_poolOwner_t	s_poolOwners[VK_POOL_OWNERS];
static int				s_poolOwnerCount;
static SDL_mutex		*s_poolTableMutex;

/* Created once, from VK_Sync_Init on the main thread before any pool exists: a
   lazily created table mutex has a window in which both threads create one and
   only one of them guards. */
void VK_Pool_Init( void )
{
	if ( !s_poolTableMutex ) {
		s_poolTableMutex = SDL_CreateMutex();
	}
}

static vk_poolOwner_t *VK_Pool_Find( VkCommandPool pool )
{
	int i;

	for ( i = 0; i < s_poolOwnerCount; i++ ) {
		if ( s_poolOwners[i].pool == pool ) {
			return &s_poolOwners[i];
		}
	}

	return NULL;
}

/* Register a pool together with the one lock that is its whole lifetime.
   owner 0 marks a pool the engine may only record from a single thread at a
   time (the walk's frame pool); every other pool belongs to its creating
   thread and refuses a different one. */
qboolean VK_Pool_Register( VkCommandPool pool, unsigned long owner )
{
	vk_poolOwner_t *slot;

	if ( pool == VK_NULL_HANDLE ) {
		return qfalse;
	}

	VK_Pool_Init();

	SDL_LockMutex( s_poolTableMutex );

	if ( VK_Pool_Find( pool ) != NULL ) {
		SDL_UnlockMutex( s_poolTableMutex );
		return qtrue;
	}
	if ( s_poolOwnerCount >= VK_POOL_OWNERS ) {
		SDL_UnlockMutex( s_poolTableMutex );
		VK_Guard_Hit( VK_GSITE_SYNC, "command pool table full", VK_ERROR_TOO_MANY_OBJECTS );
		return qfalse;
	}

	slot = &s_poolOwners[s_poolOwnerCount];
	slot->pool = pool;
	slot->owner = owner;
	slot->lock = SDL_CreateMutex();
	s_poolOwnerCount++;

	SDL_UnlockMutex( s_poolTableMutex );

	return ( slot->lock != NULL ) ? qtrue : qfalse;
}

/* The one lock for this pool.  A pool registered to another thread names the
   illegal use here, once per call, so a run shows the thread that stepped out
   of its own pool instead of only the API call that tripped the layer. */
static SDL_mutex *VK_Pool_LockOf( VkCommandPool pool )
{
	vk_poolOwner_t	*slot;
	SDL_mutex		*lock = NULL;

	if ( !s_poolTableMutex ) {
		return NULL;
	}

	SDL_LockMutex( s_poolTableMutex );

	slot = VK_Pool_Find( pool );
	if ( slot ) {
		if ( slot->owner != 0 && slot->owner != (unsigned long)SDL_ThreadID() ) {
			if ( R_ProbeStatsOn() ) {
				VK_Guard_Row( "VK:pool", "cross-thread pool use thr=%lu owner=%lu",
							  (unsigned long)SDL_ThreadID(), slot->owner );
			}
		}
		lock = slot->lock;
	}

	SDL_UnlockMutex( s_poolTableMutex );

	return lock;
}

/* The calling thread's own one-shot pool, allocated, recorded, submitted and
   freed entirely from this thread.  Two threads that race here create two
   pools, each keyed to its own thread, which is the intended state. */
VkCommandPool VK_Upload_Pool( void )
{
	VkCommandPoolCreateInfo	poolInfo;
	VkCommandPool			created = VK_NULL_HANDLE;
	VkCommandPool			pool = VK_NULL_HANDLE;
	unsigned long			me = (unsigned long)SDL_ThreadID();
	int						i;

	VK_Pool_Init();

	SDL_LockMutex( s_poolTableMutex );
	for ( i = 0; i < s_poolOwnerCount; i++ ) {
		if ( s_poolOwners[i].owner == me ) {
			pool = s_poolOwners[i].pool;
			break;
		}
	}
	SDL_UnlockMutex( s_poolTableMutex );

	if ( pool != VK_NULL_HANDLE ) {
		return pool;
	}

	if ( vk.device == VK_NULL_HANDLE ) {
		return VK_NULL_HANDLE;
	}

	memset( &poolInfo, 0, sizeof( poolInfo ) );
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = vk.graphicsFamily;

	if ( !VK_Guard_Result( VK_GSITE_SYNC,
						   qvk.CreateCommandPool( vk.device, &poolInfo, NULL, &created ),
						   "vkCreateCommandPool(one-shot owner)" ) ) {
		return VK_NULL_HANDLE;
	}

	if ( !VK_Pool_Register( created, me ) ) {
		qvk.DestroyCommandPool( vk.device, created, NULL );
		return VK_NULL_HANDLE;
	}

	if ( R_ProbeStatsOn() ) {
		VK_Guard_Row( "VK:pool", "one-shot pool thr=%lu total=%d", me, VK_Pool_Count() );
	}

	return created;
}

qboolean VK_Upload_Ready( void )
{
	return ( vk.device != VK_NULL_HANDLE && vk.frameReady ) ? qtrue : qfalse;
}

int VK_Pool_Count( void )
{
	return s_poolOwnerCount;
}

/* No render thread may be alive here: RE_Shutdown joins it before VK_BringDown,
   so no buffer can outlive the pool it came from. */
void VK_Pool_ShutdownAll( void )
{
	int i;

	if ( !s_poolTableMutex ) {
		return;
	}

	SDL_LockMutex( s_poolTableMutex );

	for ( i = 0; i < s_poolOwnerCount; i++ ) {
		if ( s_poolOwners[i].pool != VK_NULL_HANDLE && qvk.DestroyCommandPool ) {
			qvk.DestroyCommandPool( vk.device, s_poolOwners[i].pool, NULL );
		}
		s_poolOwners[i].pool = VK_NULL_HANDLE;
		s_poolOwners[i].owner = 0;
		if ( s_poolOwners[i].lock ) {
			SDL_DestroyMutex( s_poolOwners[i].lock );
			s_poolOwners[i].lock = NULL;
		}
	}
	s_poolOwnerCount = 0;

	SDL_UnlockMutex( s_poolTableMutex );
}

void VK_Upload_Init( void )
{
	VK_Pool_Init();
}

void VK_Upload_Begin( void )
{
	SDL_mutex *lock = VK_Pool_LockOf( VK_Upload_Pool() );

	if ( lock ) {
		SDL_LockMutex( lock );
	}
}

void VK_Upload_End( void )
{
	SDL_mutex *lock = VK_Pool_LockOf( VK_Upload_Pool() );

	if ( lock ) {
		SDL_UnlockMutex( lock );
	}
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

/* The same id VK_Pool_Register keys its pools by, so a per-texture owner and a
   pool owner answer the same question. */
unsigned long VK_Upload_ThreadID( void )
{
	return ( unsigned long )SDL_ThreadID();
}

/* The graphics queue is host-synchronised, and under r_smp two threads reach
   it: the walk thread submits frames while the main thread submits texture
   uploads, publishes and readbacks.  The validation layer named it -- "object
   of type VkQueue is simultaneously used in current thread ... and thread ...".
   These three are every way the port touches a queue, so the lock lives here
   and no call site has to remember it. */
static SDL_mutex *vk_queueMutex;

/* Fences are the same shape: the walk's frame fence and the readback fence are
   reset from two threads, and the layer names that too, so the resets take
   their own lock rather than each site remembering whose fence it is. */
static SDL_mutex *vk_fenceMutex;

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
   command-pool table -- take their lock from here, once, on the main thread in
   VK_Frame_Init before either thread can reach a pool or a queue.  Creating
   either mutex on first use leaves a window in which both threads find it null
   and create one each, so only one of them guards. */
void VK_Sync_Init( void )
{
	if ( !vk_queueMutex ) {
		vk_queueMutex = SDL_CreateMutex();
	}
	if ( !vk_fenceMutex ) {
		vk_fenceMutex = SDL_CreateMutex();
	}

	VK_Pool_Init();
}

VkResult VK_Fence_ResetResult( VkFence fence )
{
	VkResult result;

	SDL_LockMutex( vk_fenceMutex );
	result = qvk.ResetFences( vk.device, 1, &fence );
	SDL_UnlockMutex( vk_fenceMutex );

	return result;
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

/* The pool is the unit of host synchronization, so every allocate and free
   takes the pool's own lock -- the same one VK_Upload_Begin holds for a whole
   one-shot transaction.  SDL mutexes are recursive, so a transaction that calls
   through here does not deadlock on itself.  Called on the walk's frame pool the
   lock is simply uncontended: only one thread records a frame. */
VkResult VK_Pool_AllocBuffers( VkCommandPool pool, const VkCommandBufferAllocateInfo *info,
							   VkCommandBuffer *buffers )
{
	SDL_mutex	*lock = VK_Pool_LockOf( pool );
	VkResult	 result;

	if ( lock ) {
		SDL_LockMutex( lock );
	}

	result = qvk.AllocateCommandBuffers( vk.device, info, buffers );

	if ( lock ) {
		SDL_UnlockMutex( lock );
	}

	return result;
}

void VK_Pool_FreeBuffers( VkCommandPool pool, uint32_t count,
						  const VkCommandBuffer *buffers )
{
	SDL_mutex *lock = VK_Pool_LockOf( pool );

	if ( lock ) {
		SDL_LockMutex( lock );
	}

	qvk.FreeCommandBuffers( vk.device, pool, count, buffers );

	if ( lock ) {
		SDL_UnlockMutex( lock );
	}
}
