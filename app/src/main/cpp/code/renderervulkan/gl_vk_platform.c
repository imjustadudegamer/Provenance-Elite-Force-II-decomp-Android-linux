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
#else
#	include <SDL.h>
#	include <SDL_vulkan.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLIENT_WINDOW_TITLE "Provenance EF2"
#define VK_PLATFORM_FALLBACK_MODE 3

SDL_Window *SDL_window = NULL;

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

void VKimp_FillConfig( glconfig_t *config, int width, int height, float aspect, qboolean fullscreen )
{
	memset( config, 0, sizeof( *config ) );

	snprintf( config->renderer_string, sizeof( config->renderer_string ), "%s",
			  vk.deviceProps.deviceName );
	snprintf( config->vendor_string, sizeof( config->vendor_string ), "Vulkan vendor 0x%04x",
			  vk.deviceProps.vendorID );
	snprintf( config->version_string, sizeof( config->version_string ),
			  "Vulkan %u.%u.%u driver 0x%x",
			  VK_API_VERSION_MAJOR( vk.deviceApiVersion ),
			  VK_API_VERSION_MINOR( vk.deviceApiVersion ),
			  VK_API_VERSION_PATCH( vk.deviceApiVersion ),
			  vk.deviceProps.driverVersion );

	config->maxTextureSize = (int)vk.deviceProps.limits.maxImageDimension2D;
	config->numTextureUnits = 8;
	config->colorBits = 32;
	config->depthBits = 24;
	config->stencilBits = 8;
	config->driverType = GLDRV_ICD;
	config->hardwareType = GLHW_GENERIC;
	config->deviceSupportsGamma = qfalse;
	config->textureCompression = TC_S3TC;	/* the BCn block formats are device-probed, not assumed */
	config->textureEnvAddAvailable = qtrue;
	config->textureEnvCombineAvailable = qtrue;
	config->vidWidth = width;
	config->vidHeight = height;
	config->windowAspect = aspect;
	config->displayFrequency = 60;
	config->isFullscreen = fullscreen;
	config->stereoEnabled = qfalse;
	config->smpActive = vk.smpActive;
}

qboolean VK_Surface_Create( void )
{
	int surfaceWidth = 0;
	int surfaceHeight = 0;

	if ( !SDL_window ) {
		return qtrue;
	}

	if ( !SDL_Vulkan_CreateSurface( SDL_window, vk.instance, &vk.surface ) ) {
		VK_Guard_Hit( VK_GSITE_SURFACE, "SDL_Vulkan_CreateSurface", VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	SDL_GetWindowSize( SDL_window, &surfaceWidth, &surfaceHeight );
	VK_Guard_Row( "VK:surface", "created window=%dx%d", surfaceWidth, surfaceHeight );

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
	if ( !R_GetModeInfo( width, height, aspect, mode ) ) {
		R_GetModeInfo( width, height, aspect, VK_PLATFORM_FALLBACK_MODE );
	}
}

static cvar_t *r_vkValidate;

static const char *const vk_validationLayer = "VK_LAYER_KHRONOS_validation";
static const char *const vk_debugExtension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

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

qboolean R_SetMode( int mode, glconfig_t *glConfigOut )
{
	int width = 640, height = 480;
	float aspect = 4.0f / 3.0f;
	unsigned int extensionCount = 0;
	const char **extensionNames = NULL;
	int fullscreen;
	int validate;
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
	r_swapInterval = ri.Cvar_Get( "r_swapInterval", "1", CVAR_ARCHIVE );
	r_vkValidate = ri.Cvar_Get( "r_vkValidate", "0", CVAR_ARCHIVE );

	fullscreen = r_fullscreen->integer;
	VK_Platform_ModeSize( r_mode->integer, &width, &height, &aspect );

	/* The module owns its glconfig: every size decision the image and 2D
	   layers make reads this one, and the caller's copy is filled from it at
	   the end (the GLES3 twin's shape).  A parameter named glConfig would
	   shadow it, and the data model would run on an all-zero config. */
	memset( &glConfig, 0, sizeof( glConfig ) );

	if ( SDL_Vulkan_LoadLibrary( NULL ) != 0 ) {
		VK_Guard_Hit( VK_GSITE_LOADER, "SDL_Vulkan_LoadLibrary", VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	if ( SDL_window ) {
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
	}

	if ( !VKimp_WindowCreate( width, height, fullscreen, qfalse ) ) {
		return qfalse;
	}

	extensionNames = (const char **)malloc( sizeof( char * ) * 16 );
	if ( extensionNames ) {
		extensionCount = VKimp_InstanceExtensions( extensionNames, 14 );

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
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
		return qfalse;
	}

	free( extensionNames );

	VKimp_FillConfig( &glConfig, width, height, aspect, fullscreen ? qtrue : qfalse );

	if ( glConfigOut ) {
		memcpy( glConfigOut, &glConfig, sizeof( *glConfigOut ) );
	}

	snprintf( modeCvar, sizeof( modeCvar ), "%d", mode );
	if ( ri.Cvar_Set ) {
		ri.Cvar_Set( "r_mode", modeCvar );
	}

	VK_Guard_Row( "VK:mode", "mode=%d %dx%d fullscreen=%d maxTextureSize=%d",
				  mode, width, height, fullscreen, glConfig.maxTextureSize );

	return qtrue;
}

void R_SetFullscreen( qboolean fullScreen, glconfig_t *glConfigOut )
{
	int width;
	int height;
	float aspect;

	if ( !SDL_window ) {
		return;
	}

	width = glConfig.vidWidth;
	height = glConfig.vidHeight;
	aspect = glConfig.windowAspect;

	if ( SDL_SetWindowFullscreen( SDL_window, fullScreen ? SDL_WINDOW_FULLSCREEN : 0 ) != 0 ) {
		VK_Guard_Hit( VK_GSITE_SURFACE, "SDL_SetWindowFullscreen", VK_ERROR_INITIALIZATION_FAILED );
	}

	VK_Platform_ModeSize( r_mode->integer, &width, &height, &aspect );
	VKimp_FillConfig( &glConfig, width, height, aspect, fullScreen );

	if ( glConfigOut ) {
		memcpy( glConfigOut, &glConfig, sizeof( *glConfigOut ) );
	}

	if ( vk.swapchain != VK_NULL_HANDLE ) {
		VK_Swapchain_Recreate( width, height );
	}
}

void VKimp_Shutdown( void )
{
	if ( SDL_window ) {
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
	}
	SDL_Vulkan_UnloadLibrary();
}
