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

qboolean VKimp_MainOwnsFrame( void )
{
	return ( !vk_renderThread || SDL_ThreadID() == VKimp_MainThreadId() ) ? qtrue : qfalse;
}

static int SDLCALL VKimp_RenderThreadWrapper( void *arg )
{
	vkRenderThreadActive = qtrue;
	vk_renderThreadId = SDL_ThreadID();

	vk_renderThreadFunction();

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

	SDL_LockMutex( vk_smpMutex );

	data = vk_smpData;
	vk_smpData = NULL;

	SDL_CondSignal( vk_renderCompletedEvent );

	while ( !vk_smpShutdown && vk_smpData == NULL ) {
		SDL_CondWait( vk_renderCommandsEvent, vk_smpMutex );
	}

	if ( vk_smpShutdown ) {
		data = NULL;
	}

	SDL_UnlockMutex( vk_smpMutex );

	return data;
}

void VKimp_FrontEndSleep( void )
{
	SDL_LockMutex( vk_smpMutex );

	while ( vk_smpData ) {
		SDL_CondWait( vk_renderCompletedEvent, vk_smpMutex );
	}

	SDL_UnlockMutex( vk_smpMutex );
}

void VKimp_WakeRenderer( void *data )
{
	SDL_LockMutex( vk_smpMutex );

	if ( vk_smpData ) {
		vk_handshakeViolations++;
		VK_Guard_Hit( VK_GSITE_SYNC, "wake while a frame is still in flight", VK_ERROR_TOO_MANY_OBJECTS );
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
}

int VKimp_HandshakeViolations( void )
{
	return vk_handshakeViolations;
}
