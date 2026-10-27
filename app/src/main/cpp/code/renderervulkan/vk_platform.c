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
#include "tr_local.h"
#include "vk_firstlight.h"

#ifdef USE_INTERNAL_SDL_HEADERS
#	include "SDL.h"
#	include "SDL_vulkan.h"
#	include "SDL_syswm.h"
#else
#	include <SDL.h>
#	include <SDL_vulkan.h>
#	include <SDL_syswm.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#undef CLIENT_WINDOW_TITLE
#define CLIENT_WINDOW_TITLE "Provenance EF2"

SDL_Window *SDL_window = NULL;

/* the headless surface's planned extent; the swapchain does not exist yet */
static int vk_headlessWidth;
static int vk_headlessHeight;

cvar_t *r_fullscreen;
cvar_t *r_mode;
cvar_t *r_customwidth;
cvar_t *r_customheight;
cvar_t *r_customaspect;
cvar_t *r_swapInterval;

static const char *VK_HomePath_ri( void )
{
	cvar_t *cv = ri.Cvar_Get( "fs_homepath", "", 0 );

	return cv ? cv->string : NULL;
}

static void VK_LogRow( const char *line )
{
	ri.Printf( PRINT_ALL, "%s", line );
}

static int VK_TraceCvar( const char *name, int defaultValue )
{
	char fallback[16];
	cvar_t *cv;

	snprintf( fallback, sizeof( fallback ), "%d", defaultValue );
	cv = ri.Cvar_Get( name, fallback, 0 );

	return cv ? cv->integer : defaultValue;
}

static void VK_FullPath( char *out, size_t len, const char *path )
{
	const char *home = VK_HomePath();

	if ( home && home[0] ) {
		snprintf( out, len, "%s/%s", home, path );
	} else {
		snprintf( out, len, "%s", path );
	}
}

static qboolean VK_SentinelExists( const char *path )
{
	char full[1024];

	VK_FullPath( full, sizeof( full ), path );

	if ( !ri.FS_FileExists( full ) ) {
		return qfalse;
	}

	return qtrue;
}

static qboolean VK_SentinelWrite( const char *path, const void *data, int size )
{
	char full[1024];

	VK_FullPath( full, sizeof( full ), path );

	ri.FS_WriteFile( full, data, size );

	return ri.FS_FileExists( full ) ? qtrue : qfalse;
}

static qboolean VK_SentinelRemove( const char *path )
{
	char full[1024];

	VK_FullPath( full, sizeof( full ), path );

	if ( !ri.FS_FileExists( full ) ) {
		return qfalse;
	}

	ri.FS_RemoveHomeData( path );

	return qtrue;
}

void VKimp_AdapterInit( void )
{
	VK_Guard_SetLogger( VK_LogRow );
	VK_Guard_SetTraceProvider( VK_TraceCvar );
	VK_SetHomePathFn( VK_HomePath_ri );
	VK_Sentinel_SetIO( VK_SentinelExists, VK_SentinelWrite, VK_SentinelRemove );
}

void VKimp_FillConfig( glconfig_t *glconfig, int width, int height, float aspect, qboolean fullscreen )
{
	memset( glconfig, 0, sizeof( *glconfig ) );

	snprintf( glconfig->renderer_string, sizeof( glconfig->renderer_string ), "%s",
			  vk.deviceProps.deviceName );
	snprintf( glconfig->vendor_string, sizeof( glconfig->vendor_string ), "Vulkan vendor 0x%04x",
			  vk.deviceProps.vendorID );
	snprintf( glconfig->version_string, sizeof( glconfig->version_string ),
			  "Vulkan %u.%u.%u driver 0x%x",
			  VK_API_VERSION_MAJOR( vk.deviceApiVersion ),
			  VK_API_VERSION_MINOR( vk.deviceApiVersion ),
			  VK_API_VERSION_PATCH( vk.deviceApiVersion ),
			  vk.deviceProps.driverVersion );

	glconfig->maxTextureSize = (int)vk.deviceProps.limits.maxImageDimension2D;
	glconfig->numTextureUnits = 8;
	glconfig->colorBits = 32;
	glconfig->depthBits = 24;
	/* no stencil attachment is created, so the guards that arm the shadow
	   overlay have to see none until one does exist */
	glconfig->stencilBits = 0;
	glconfig->driverType = GLDRV_ICD;
	glconfig->hardwareType = GLHW_GENERIC;
	glconfig->deviceSupportsGamma = qfalse;
	glconfig->textureCompression = TC_S3TC;	/* the BCn block formats are device-probed, not assumed */
	glconfig->textureEnvAddAvailable = qtrue;
	glconfig->textureEnvCombineAvailable = qtrue;
	glconfig->vidWidth = width;
	glconfig->vidHeight = height;
	glconfig->windowAspect = aspect;
	glconfig->displayFrequency = 60;
	glconfig->isFullscreen = fullscreen;
	glconfig->stereoEnabled = qfalse;
	glconfig->smpActive = vk.smpActive;
}

#ifdef __ANDROID__
/* Android hands the app a DIFFERENT ANativeWindow after every suspend (SDL releases the old one and fetches a new one, recreating the EGL surface
   for GL and nothing for Vulkan), so the VkSurface built at first light is dead after the first sleep and every acquire/present on it fails forever.
   SDLActivity only signals resume once mIsSurfaceReady, so by the time the engine sees the foreground the new window exists and this rebind is legal. */
qboolean VK_Surface_Refresh( void )
{
	/* Also the recovery path for a surface that a previous refresh already lost:
	   returning early on NULL would leave Recreate failing against "no surface"
	   forever, so the NULL case is a create like any other. */
	if ( vk.surface != VK_NULL_HANDLE ) {
		VK_Surface_Destroy();
	}

	return VK_Surface_Create();
}

/* The window identity the surface was built from, tracked so the walk can tell
   "the window came back as a different handle" from "the swapchain merely went
   stale" -- the second is repaired by the existing swapchain recreate, the first
   needs the surface itself rebuilt.  NULL means the window is gone (backgrounded),
   which is a change like any other. */
static void *s_surfaceWindow = NULL;

qboolean VK_Surface_WindowChanged( void )
{
	SDL_SysWMinfo info;
	void *current;

	if ( !SDL_window ) {
		return qfalse;
	}

	SDL_VERSION( &info.version );
	if ( !SDL_GetWindowWMInfo( SDL_window, &info ) || info.subsystem != SDL_SYSWM_ANDROID ) {
		return qfalse;
	}

	current = info.info.android.window;
	/* A NULL window is the backgrounded state itself: there is nothing to bind
	   yet, and creating a surface against NULL would only fail.  The walk skips
	   on its own while the window is away, and the rebind waits for the real
	   handle the UI thread installs in onNativeSurfaceCreated. */
	if ( current == NULL ) {
		return qfalse;
	}
	if ( s_surfaceWindow == NULL ) {
		return qtrue;
	}

	return current != s_surfaceWindow;
}

void VK_Surface_WindowTrack( void )
{
	SDL_SysWMinfo info;

	s_surfaceWindow = NULL;
	if ( !SDL_window ) {
		return;
	}

	SDL_VERSION( &info.version );
	if ( !SDL_GetWindowWMInfo( SDL_window, &info ) || info.subsystem != SDL_SYSWM_ANDROID ) {
		return;
	}

	s_surfaceWindow = info.info.android.window;
}
#endif

qboolean VK_Surface_Create( void )
{
	int surfaceWidth = 0;
	int surfaceHeight = 0;

	if ( vk.headless ) {
		VkHeadlessSurfaceCreateInfoEXT info;

		if ( !qvk.CreateHeadlessSurfaceEXT ) {
			VK_Guard_Hit( VK_GSITE_SURFACE, "VK_EXT_headless_surface absent", VK_ERROR_EXTENSION_NOT_PRESENT );
			return qfalse;
		}

		memset( &info, 0, sizeof( info ) );
		info.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;
		info.flags = 0;

		if ( qvk.CreateHeadlessSurfaceEXT( vk.instance, &info, NULL, &vk.surface ) != VK_SUCCESS ) {
			VK_Guard_Hit( VK_GSITE_SURFACE, "vkCreateHeadlessSurfaceEXT", VK_ERROR_INITIALIZATION_FAILED );
			return qfalse;
		}

		if ( R_ProbeStatsOn() ) {
			VK_Guard_Row( "VK:surface", "created headless=%dx%d", vk_headlessWidth, vk_headlessHeight );
		}

		return qtrue;
	}

	if ( !SDL_window ) {
		return qtrue;
	}

	/* Tracked before the result is decided: a failed bind must still record the
	   window it failed against, or the next compare would re-arm forever. */
#ifdef __ANDROID__
	VK_Surface_WindowTrack();
#endif

	if ( !SDL_Vulkan_CreateSurface( SDL_window, vk.instance, &vk.surface ) ) {
		VK_Guard_Hit( VK_GSITE_SURFACE, "SDL_Vulkan_CreateSurface", VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	SDL_GetWindowSize( SDL_window, &surfaceWidth, &surfaceHeight );
	if ( R_ProbeStatsOn() ) {
		VK_Guard_Row( "VK:surface", "created window=%dx%d", surfaceWidth, surfaceHeight );
	}

	return qtrue;
}

void VK_Surface_Destroy( void )
{
	if ( vk.surface != VK_NULL_HANDLE && qvk.DestroySurfaceKHR ) {
		qvk.DestroySurfaceKHR( vk.instance, vk.surface, NULL );
	}
	vk.surface = VK_NULL_HANDLE;
}


static void VK_Platform_ModeSize( int mode, int *width, int *height, float *aspect )
{
	/* mode -2 is the desktop/native arm and GLimp owns it in the reference: the
	   window is created at the display's own size and the frame fills it.  With
	   SDL not yet initialised the display mode is unknown, so the same 640x480
	   the reference assumes is the floor. */
	if ( mode == -2 ) {
		SDL_DisplayMode desktop;

		memset( &desktop, 0, sizeof( desktop ) );

		if ( SDL_GetDesktopDisplayMode( 0, &desktop ) == 0 && desktop.w > 0 && desktop.h > 0 ) {
			*width = desktop.w;
			*height = desktop.h;
		} else {
			*width = 640;
			*height = 480;
			VK_Guard_Row( "VK:mode", "desktop display mode unavailable, assuming 640x480" );
		}

		*aspect = (float)( *width ) / (float)( *height );
		return;
	}

	if ( !R_GetModeInfo( width, height, aspect, mode ) ) {
		R_GetModeInfo( width, height, aspect, VK_PLATFORM_FALLBACK_MODE );
	}
}

static cvar_t *r_vkValidate;
static cvar_t *r_vkHeadless;

static const char *const vk_validationLayer = "VK_LAYER_KHRONOS_validation";
static const char *const vk_debugExtension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

/* Whether the walk's driver calls are being validated.  The layer wraps every
   vkCmd* call, so a frame cost measured with it armed is not the shipping cost;
   the perf row carries this so a measurement states its own conditions. */
int VK_Platform_Validation( void )
{
	return ( r_vkValidate && r_vkValidate->integer ) ? 1 : 0;
}

qboolean VKimp_WindowCreate( int width, int height, int fullscreen, qboolean hidden )
{
	Uint32 flags = SDL_WINDOW_VULKAN;

	if ( hidden ) {
		flags |= SDL_WINDOW_HIDDEN;
	} else {
		flags |= SDL_WINDOW_SHOWN;
	}

	if ( fullscreen ) {
		flags |= SDL_WINDOW_FULLSCREEN;
	}

	SDL_window = SDL_CreateWindow( CLIENT_WINDOW_TITLE,
								   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
								   width, height, flags );
	if ( !SDL_window ) {
		VK_Guard_Hit( VK_GSITE_SURFACE, "SDL_CreateWindow(SDL_WINDOW_VULKAN)", VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	return qtrue;
}

unsigned int VKimp_InstanceExtensions( const char **out, unsigned int max )
{
	unsigned int count = 0;

	if ( !SDL_Vulkan_GetInstanceExtensions( SDL_window, &count, NULL ) || count == 0 ) {
		return 0;
	}

	if ( count > max ) {
		count = max;
	}

	if ( SDL_Vulkan_GetInstanceExtensions( SDL_window, &count, out ) == SDL_FALSE ) {
		VK_Guard_Hit( VK_GSITE_SURFACE, "SDL_Vulkan_GetInstanceExtensions", VK_ERROR_INITIALIZATION_FAILED );
		return 0;
	}

	return count;
}

/* SDL's Vulkan loader needs the video subsystem open: without it,
   SDL_Vulkan_LoadLibrary only sets "Video subsystem has not been initialized". */
static qboolean VKimp_VideoInit( void )
{
	if ( SDL_WasInit( SDL_INIT_VIDEO ) ) {
		return qtrue;
	}

	SDL_SetHint( SDL_HINT_APP_NAME, PRODUCT_NAME );

	if ( SDL_Init( SDL_INIT_VIDEO ) != 0 ) {
		ri.Printf( PRINT_ALL, "SDL_Init( SDL_INIT_VIDEO ) FAILED (%s)\n", SDL_GetError() );
		return qfalse;
	}

	ri.Printf( PRINT_ALL, "SDL using driver \"%s\"\n", SDL_GetCurrentVideoDriver() );

	if ( ri.Cvar_Set ) {
		ri.Cvar_Set( "r_sdlDriver", SDL_GetCurrentVideoDriver() );
	}

	return qtrue;
}

qboolean R_SetMode( int mode, glconfig_t *vkConfigOut )
{
	int width = 640, height = 480;
	float aspect = 4.0f / 3.0f;
	unsigned int extensionCount = 0;
	const char **extensionNames = NULL;
	int fullscreen;
	int validate;
	int headless;
	char modeCvar[16];

	r_fullscreen = ri.Cvar_Get( "r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH );
	{
		char modeDefault[16];
		snprintf( modeDefault, sizeof( modeDefault ), "%d", mode );
		r_mode = ri.Cvar_Get( "r_mode", modeDefault, CVAR_ARCHIVE | CVAR_LATCH );
	}
	r_customwidth = ri.Cvar_Get( "r_customwidth", "1280", CVAR_ARCHIVE | CVAR_LATCH );
	r_customheight = ri.Cvar_Get( "r_customheight", "720", CVAR_ARCHIVE | CVAR_LATCH );
	r_customaspect = ri.Cvar_Get( "r_customaspect", "1", CVAR_ARCHIVE | CVAR_LATCH );
	/* Desktop reads this to pick the present rung; Android does not (vk_swapchain.c),
	   where the engine's com_maxfps budget is the only pace a player sets. */
	r_swapInterval = ri.Cvar_Get( "r_swapInterval", "1", CVAR_ARCHIVE );
	/* A diagnostic is not a setting: an archived one leaks into config.cfg. */

	r_vkValidate = ri.Cvar_Get( "r_vkValidate", "0", 0 );
	r_vkHeadless = ri.Cvar_Get( "r_vkHeadless", "0", CVAR_LATCH );

	fullscreen = r_fullscreen->integer;
	headless = r_vkHeadless->integer ? qtrue : qfalse;
	vk.headless = headless;

	/* the module owns its config; a parameter of that name would shadow it */
	memset( &vkConfig, 0, sizeof( vkConfig ) );

	if ( !headless ) {
		if ( !VKimp_VideoInit() ) {
			VK_Guard_Hit( VK_GSITE_LOADER, "SDL_Init(SDL_INIT_VIDEO)", VK_ERROR_INITIALIZATION_FAILED );
			return qfalse;
		}

		if ( SDL_Vulkan_LoadLibrary( NULL ) != 0 ) {
			static char loaderDetail[128];

			/* SDL's own error names the cause; carry it in the row */
			Com_sprintf( loaderDetail, sizeof( loaderDetail ),
						 "SDL_Vulkan_LoadLibrary: %s", SDL_GetError() );
			VK_Guard_Hit( VK_GSITE_LOADER, loaderDetail, VK_ERROR_INITIALIZATION_FAILED );
			return qfalse;
		}

		if ( SDL_window ) {
			SDL_DestroyWindow( SDL_window );
			SDL_window = NULL;
		}
	}

	/* After the video subsystem, because mode -2 resolves through the desktop
	   display mode and SDL is the only source for it. */
	VK_Platform_ModeSize( r_mode->integer, &width, &height, &aspect );
	vk_headlessWidth = width;
	vk_headlessHeight = height;

	if ( !headless ) {
		if ( !VKimp_WindowCreate( width, height, fullscreen, qfalse ) ) {
			return qfalse;
		}
	}

	extensionNames = (const char **)malloc( sizeof( char * ) * 16 );
	if ( extensionNames ) {
		if ( headless ) {
			/* No window and no display server: the surface is
			   VK_EXT_headless_surface, so the probes render offscreen. */
			extensionNames[extensionCount++] = VK_KHR_SURFACE_EXTENSION_NAME;
			extensionNames[extensionCount++] = VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME;
		} else {
			extensionCount = VKimp_InstanceExtensions( extensionNames, 14 );
		}

		validate = r_vkValidate->integer;
		if ( validate && VK_Instance_LayerAvailable( vk_validationLayer ) ) {
			VK_Instance_SetLayers( &vk_validationLayer, 1 );
			extensionNames[extensionCount++] = vk_debugExtension;
		} else if ( validate ) {
			VK_Guard_Row( "VK:guard", "site=instance detail=\"validation requested but VK_LAYER_KHRONOS_validation is absent\" result=success(0) n=1" );
		}
	}

	if ( !VK_BringUp( width, height, extensionNames, (int)extensionCount ) ) {
		free( extensionNames );
		if ( !headless && SDL_window ) {
			SDL_DestroyWindow( SDL_window );
			SDL_window = NULL;
		}
		return qfalse;
	}

	free( extensionNames );

	VKimp_FillConfig( &vkConfig, width, height, aspect, fullscreen ? qtrue : qfalse );

	/* the twin hands the window to the input layer here; it has no other caller */
	if ( !headless && ri.IN_Init ) {
		ri.IN_Init( SDL_window );
	}

	if ( vkConfigOut ) {
		memcpy( vkConfigOut, &vkConfig, sizeof( *vkConfigOut ) );
	}

	snprintf( modeCvar, sizeof( modeCvar ), "%d", mode );
	if ( ri.Cvar_Set ) {
		ri.Cvar_Set( "r_mode", modeCvar );
	}

	{
		int windowWidth = 0;
		int windowHeight = 0;

		if ( SDL_window ) {
			SDL_GetWindowSize( SDL_window, &windowWidth, &windowHeight );
		}

		/* surface= is the window the swapchain is created against: the mode is
		   right only when the two agree, so the row carries both. */
		if ( R_ProbeStatsOn() ) {
			VK_Guard_Row( "VK:mode", "mode=%d %dx%d fullscreen=%d surface=%dx%d maxTextureSize=%d",
						  mode, width, height, fullscreen, windowWidth, windowHeight, vkConfig.maxTextureSize );
		}
	}

	return qtrue;
}

void R_SetFullscreen( qboolean fullScreen, glconfig_t *vkConfigOut )
{
	int width;
	int height;
	float aspect;

	if ( !SDL_window ) {
		return;
	}

	width = vkConfig.vidWidth;
	height = vkConfig.vidHeight;
	aspect = vkConfig.windowAspect;

	if ( SDL_SetWindowFullscreen( SDL_window, fullScreen ? SDL_WINDOW_FULLSCREEN : 0 ) != 0 ) {
		VK_Guard_Hit( VK_GSITE_SURFACE, "SDL_SetWindowFullscreen", VK_ERROR_INITIALIZATION_FAILED );
	}

	VK_Platform_ModeSize( r_mode->integer, &width, &height, &aspect );
	VKimp_FillConfig( &vkConfig, width, height, aspect, fullScreen );

	if ( vkConfigOut ) {
		memcpy( vkConfigOut, &vkConfig, sizeof( *vkConfigOut ) );
	}

	if ( vk.swapchain != VK_NULL_HANDLE ) {
		/* This runs on the main thread, and the walk thread may be holding an
		   acquired image from the swapchain about to be destroyed.  Park it and
		   let the device drain before the recreate, or the destroy races an
		   acquire and a present that are already in flight. */
		if ( vk.smpActive && !VKimp_IsRenderThread() ) {
			VKimp_FrontEndWaitIdle();
		}
		VK_Frame_WaitIdle();
		VK_Swapchain_Recreate( width, height );
	}
}

void VKimp_Shutdown( void )
{
	if ( SDL_window ) {
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
	}

	if ( !vk.headless ) {
		SDL_Vulkan_UnloadLibrary();
	}
}

#define SMP_RLOAD_WINDOW_MS	1000
#define SMP_RLOAD_MIN_MS		30	
#define SMP_RLOAD_MAX_ROWS		720

cvar_t	*smp_rloadStats = NULL;
int		smp_rloadDepth = 0;

static int	smp_rloadRows;
static int	smp_rloadStopped;
static int	smp_rloadWindowStart;
static unsigned int	smp_rloadWindowFrames;
static int	smp_rloadSums[SMP_RLZ_COUNT], smp_rloadMaxs[SMP_RLZ_COUNT];
static int	smp_rloadWorldBrackets;	
static int	smp_rloadRegBrackets;

int SMP_RLoadNow( void ) {
	if ( !smp_rloadStats ) {
		smp_rloadStats = ri.Cvar_Get( "smp_loadStats", "1", 0 );
	}
	return (int)SDL_GetTicks();
}

static void SMP_RLoadFlush( int window ) {
	static const char *zoneName[SMP_RLZ_COUNT] = {
		"world", "bspread", "shaders", "lightmap", "lods",
		"reginit", "imginit", "setup", "findimg", "decode",
		"transcode", "upload", "modelreg",
		"shparse", "findsync", "texcreate"
	};
	char	line[1024], part[64];
	int		i, mean10, frames, unacctW;

	if ( smp_rloadStopped || !SMP_RLoadArmed() ) {
		return;
	}

	unacctW = smp_rloadSums[SMP_RLZ_world]
			- smp_rloadSums[SMP_RLZ_bspread]
			- smp_rloadSums[SMP_RLZ_shaders]
			- smp_rloadSums[SMP_RLZ_lightmap]
			- smp_rloadSums[SMP_RLZ_lods];
	if ( unacctW < 0 ) unacctW = 0;

	frames = (int)smp_rloadWindowFrames;
	if ( frames < 1 ) frames = 1;

	line[0] = '\0';
	for ( i = 0; i < SMP_RLZ_COUNT; i++ ) {
		mean10 = (int)( ( (long long)smp_rloadSums[i] * 10 + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d",
					 zoneName[i], mean10 / 10, mean10 % 10, smp_rloadMaxs[i] );
		Q_strcat( line, sizeof( line ), part );
	}
	Com_sprintf( part, sizeof( part ), " unacctWsum=%d", unacctW );
	Q_strcat( line, sizeof( line ), part );

	ri.Printf( PRINT_ALL, "[SMP:rload] window=%dms worldN=%d regN=%d%s\n",
			   window, smp_rloadWorldBrackets, smp_rloadRegBrackets, line );

	smp_rloadRows++;
	if ( smp_rloadRows >= SMP_RLOAD_MAX_ROWS ) {
		smp_rloadStopped = 1;
		ri.Printf( PRINT_ALL, "[SMP:rload] capped at %d rows; no further [SMP:rload] lines\n",
				   SMP_RLOAD_MAX_ROWS );
	}

	for ( i = 0; i < SMP_RLZ_COUNT; i++ ) {
		smp_rloadSums[i] = smp_rloadMaxs[i] = 0;
	}
	smp_rloadWindowFrames = 0;
	smp_rloadWorldBrackets = 0;
	smp_rloadRegBrackets = 0;
	smp_rloadWindowStart = SMP_RLoadNow();
}

void SMP_RLoadAdd( int zone, int msec, int depth ) {
	int	now;

	if ( zone < 0 || zone >= SMP_RLZ_COUNT ) {
		return;
	}
	if ( msec < 0 ) {
		msec = 0;
	}

	if ( !smp_rloadWindowStart ) {
		smp_rloadWindowStart = SMP_RLoadNow();
	}
	smp_rloadSums[zone] += msec;
	if ( msec > smp_rloadMaxs[zone] ) {
		smp_rloadMaxs[zone] = msec;
	}

	if ( depth != 0 ) {
		return;
	}

	smp_rloadWindowFrames++;
	if ( zone == SMP_RLZ_world ) {
		smp_rloadWorldBrackets++;
	} else if ( zone == SMP_RLZ_reginit ) {
		smp_rloadRegBrackets++;
	}

	now = SMP_RLoadNow();
	if ( msec >= SMP_RLOAD_MIN_MS || ( now - smp_rloadWindowStart ) >= SMP_RLOAD_WINDOW_MS ) {
		SMP_RLoadFlush( now - smp_rloadWindowStart );
	}
}

/* one bring-up, two call sites; the guard is the device handle, not a flag */
qboolean VK_EnsureBroughtUp( const char *where )
{
	int mode;
	cvar_t *rmode;

	/* an "already asked" flag would leave the second site blind after the first */
	if ( vk.device != VK_NULL_HANDLE ) {
		return qfalse;
	}

	/* harness hook: holds the R_Init site down so BeginRegistration brings the device up alone */
	{
		cvar_t *noInitUp = ri.Cvar_Get( "r_vkNoInitUp", "0", CVAR_CHEAT );

		if ( noInitUp && noInitUp->integer && !strcmp( where, "R_Init" ) ) {
			VK_Guard_Hit( VK_GSITE_QUIRK, "r_vkNoInitUp: R_Init site held down", VK_INCOMPLETE );
			return qfalse;
		}
	}

	/* the twin's own default: -2 is the desktop arm, the display's own size, and the
	   value the compiled seed ships; retail's 2003 mode 3 is nobody's screen now */
	rmode = ri.Cvar_Get( "r_mode", "-2", CVAR_ARCHIVE | CVAR_LATCH );
	mode = rmode ? rmode->integer : VK_PLATFORM_FALLBACK_MODE;

	/* the entry row is informational, not a guard hit: the guard ledger answers "did a Vulkan call fail" and VK_INCOMPLETE here reads as a failure
	   result without one.  The real failure is the row below, which stays a hit. */
	/* informational, and a quiet boot carries no informational rows: the failure below is the loud arm (guard hit + ri.Error) */
	if ( R_ProbeStatsOn() ) {
		VK_Guard_Row( "VK:bringup", "site=%s entry", where );
	}

	if ( !R_SetMode( mode, &vkConfig ) ) {
		VK_Guard_Hit( VK_GSITE_DEVICE, "bring-up failed", VK_ERROR_INITIALIZATION_FAILED );

		/* a named failure, not a silent one: a null device is the ip=0x0 the
		   boot's first image faulted on */
		ri.Error( ERR_FATAL, "VK_EnsureBroughtUp(%s): Vulkan bring-up failed; see the [VK:guard] rows", where );
		return qtrue;
	}

	return qtrue;
}
