
#include "tr_local.h"

#ifdef USE_INTERNAL_SDL_HEADERS
#	include "SDL.h"
#else
#	include <SDL.h>
#endif

volatile qboolean	renderThreadActive = qfalse;

static SDL_Thread	*glimp_renderThread = NULL;
static SDL_mutex	*glimp_smpMutex = NULL;
static SDL_cond		*glimp_renderCommandsEvent = NULL;
static SDL_cond		*glimp_renderCompletedEvent = NULL;
static void			(*glimp_renderThreadFunction)(void) = NULL;

static void			*glimp_smpData = NULL;

static volatile int	glimp_smpShutdown = 0;
static SDL_threadID	glimp_renderThreadId = 0;

enum { GLIMP_GL_NONE = 0, GLIMP_GL_MAIN = 1, GLIMP_GL_RENDER = 2 };
static int			glimp_glOwner = GLIMP_GL_NONE;

static int			glimp_handshakeViolations = 0;
static int			glimp_wakeRows = 0;
static int			glimp_sleepRows = 0;

#define SMP_MCORE_PROBE_HEAD  8
#define SMP_MCORE_PROBE_EVERY 600

extern qboolean	GLimp_GLContextMakeCurrent( qboolean makeCurrent );

static SDL_threadID	glimp_glCurrentThread = 0;

static qboolean GLimp_GLThreadClaim( qboolean makeCurrent )
{
	SDL_threadID self = SDL_ThreadID();

	if ( makeCurrent ) {
		if ( glimp_glCurrentThread != 0 && glimp_glCurrentThread != self ) {
			return qfalse;		
		}
		glimp_glCurrentThread = self;
	} else {
		if ( glimp_glCurrentThread != 0 && glimp_glCurrentThread != self ) {
			return qfalse;		
		}
		glimp_glCurrentThread = 0;
	}
	return qtrue;
}

static void GLimp_GLSetOwner( int owner )
{
	qboolean current = ( owner != GLIMP_GL_NONE ) ? qtrue : qfalse;
	qboolean ok;

	ok = GLimp_GLThreadClaim( current );
	ok = GLimp_GLContextMakeCurrent( current ) && ok;
	if ( !ok ) {
		glimp_handshakeViolations++;
	}
	glimp_glOwner = owner;
}

qboolean GLimp_IsRenderThread( void )
{
	return ( glimp_renderThread != NULL && SDL_ThreadID() == glimp_renderThreadId ) ? qtrue : qfalse;
}

static int SDLCALL GLimp_RenderThreadWrapper( void *arg )
{
	(void)arg;

	glimp_renderThreadId = SDL_ThreadID();
	glimp_renderThreadFunction();
	glimp_renderThreadId = 0;

	return 0;
}

qboolean GLimp_SpawnRenderThread( void (*function)( void ) )
{
	glimp_smpShutdown = qfalse;
	glimp_smpData = (void *)1;

	if ( glimp_smpMutex == NULL ) {
		glimp_smpMutex = SDL_CreateMutex();
	}
	if ( glimp_renderCommandsEvent == NULL ) {
		glimp_renderCommandsEvent = SDL_CreateCond();
	}
	if ( glimp_renderCompletedEvent == NULL ) {
		glimp_renderCompletedEvent = SDL_CreateCond();
	}
	if ( !glimp_smpMutex || !glimp_renderCommandsEvent || !glimp_renderCompletedEvent ) {
		glimp_smpData = NULL;
		return qfalse;
	}

	glimp_glOwner = GLIMP_GL_MAIN;

	glimp_renderThreadFunction = function;

	glimp_renderThread = SDL_CreateThread( GLimp_RenderThreadWrapper, "gles3-render", NULL );
	if ( !glimp_renderThread ) {
		ri.Printf( PRINT_WARNING, "r_smp: SDL_CreateThread() returned %s\n", SDL_GetError() );
		glimp_smpData = NULL;
		return qfalse;
	}

	renderThreadActive = qtrue;

	GLimp_FrontEndSleep();

	return qtrue;
}

void *GLimp_RendererSleep( void )
{
	void *data;

	SDL_LockMutex( glimp_smpMutex );
	{
		if ( glimp_glOwner == GLIMP_GL_RENDER ) {
			GLimp_GLSetOwner( GLIMP_GL_NONE );
		}

		glimp_smpData = NULL;                          
		SDL_CondSignal( glimp_renderCompletedEvent );  

		while ( glimp_smpData == NULL && !glimp_smpShutdown ) {
			SDL_CondWait( glimp_renderCommandsEvent, glimp_smpMutex );
		}

		data = glimp_smpShutdown ? NULL : glimp_smpData;

		if ( data != NULL ) {
			GLimp_GLSetOwner( GLIMP_GL_RENDER );

			if ( glimp_sleepRows < SMP_MCORE_PROBE_HEAD || ( glimp_sleepRows % SMP_MCORE_PROBE_EVERY ) == 0 ) {
				ri.Printf( PRINT_ALL,
						   "[SMP:glhandoff] side=render wake=%d owner=%d violations=%d\n",
						   glimp_sleepRows, glimp_glOwner, glimp_handshakeViolations );
			}
			glimp_sleepRows++;
		}
	}
	SDL_UnlockMutex( glimp_smpMutex );

	return data;
}

void GLimp_FrontEndSleep( void )
{
	SDL_LockMutex( glimp_smpMutex );
	{
		while ( glimp_smpData != NULL ) {
			SDL_CondWait( glimp_renderCompletedEvent, glimp_smpMutex );
		}

		if ( !GLimp_IsRenderThread() && glimp_renderThread != NULL ) {
			GLimp_GLSetOwner( GLIMP_GL_MAIN );
		}
	}
	SDL_UnlockMutex( glimp_smpMutex );
}

void GLimp_WakeRenderer( void *data )
{
	SDL_LockMutex( glimp_smpMutex );
	{
		while ( glimp_smpData != NULL ) {
			SDL_CondWait( glimp_renderCompletedEvent, glimp_smpMutex );
		}

		if ( !GLimp_IsRenderThread() && glimp_glOwner == GLIMP_GL_MAIN ) {
			GLimp_GLSetOwner( GLIMP_GL_NONE );
		}

		glimp_smpData = data;
		SDL_CondSignal( glimp_renderCommandsEvent );

		if ( glimp_wakeRows < SMP_MCORE_PROBE_HEAD || ( glimp_wakeRows % SMP_MCORE_PROBE_EVERY ) == 0 ) {
			ri.Printf( PRINT_ALL,
					   "[SMP:glhandoff] side=main hand=%d owner=%d violations=%d\n",
					   glimp_wakeRows, glimp_glOwner, glimp_handshakeViolations );
		}
		glimp_wakeRows++;
	}
	SDL_UnlockMutex( glimp_smpMutex );
}

void GLimp_ShutdownRenderThread( void )
{
	if ( glimp_renderThread ) {
		GLimp_FrontEndSleep();           

		SDL_LockMutex( glimp_smpMutex );
		{
			glimp_smpShutdown = qtrue;
			SDL_CondSignal( glimp_renderCommandsEvent );
		}
		SDL_UnlockMutex( glimp_smpMutex );

		SDL_WaitThread( glimp_renderThread, NULL );
		glimp_renderThread = NULL;
	}

	renderThreadActive = qfalse;

	if ( glimp_renderCommandsEvent ) { SDL_DestroyCond( glimp_renderCommandsEvent ); glimp_renderCommandsEvent = NULL; }
	if ( glimp_renderCompletedEvent ) { SDL_DestroyCond( glimp_renderCompletedEvent ); glimp_renderCompletedEvent = NULL; }
	if ( glimp_smpMutex ) { SDL_DestroyMutex( glimp_smpMutex ); glimp_smpMutex = NULL; }

	glimp_smpData = NULL;
	glimp_smpShutdown = qfalse;
	glimp_glOwner = GLIMP_GL_NONE;
}

int GLimp_HandshakeViolations( void )
{
	return glimp_handshakeViolations;
}

int GLimp_DeviceCoreCount( void )
{
	int n = SDL_GetCPUCount();
	return ( n > 0 ) ? n : 1;
}
