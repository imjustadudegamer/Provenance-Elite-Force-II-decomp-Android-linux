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

cvar_t* r_ignore;
cvar_t* r_jobs;
cvar_t* r_debuglines_depthmask;
cvar_t* r_farplane;
cvar_t* r_farplane_color;
cvar_t* r_farplane_nocull;
cvar_t* r_farplane_nofog;
cvar_t* r_light_ambientEmphasize;
cvar_t* r_light_ambientFudge;
cvar_t* r_lightcoronasize;
cvar_t* r_light_emphasize;
cvar_t* r_light_emphasizePercent;
cvar_t* r_light_lines;
cvar_t* r_light_nolight;
cvar_t* r_light_simple;
cvar_t* r_light_simple_dist;
cvar_t* r_light_sun_line;
cvar_t* r_numdebuglines;
cvar_t* r_skyportal;
cvar_t* r_skyportal_origin;
cvar_t* r_stipplelines;
cvar_t* r_useglfog;

/* the 3D cvars the mirrored front end reads; the reference defines them in its
   own tr_init.c and this module carries the same list */
cvar_t	*r_allowGeometryDeforms;
cvar_t	*r_debugSort;
cvar_t	*r_debugSurface;
cvar_t	*r_detailSurfaces;
cvar_t	*r_drawcurves;
cvar_t	*r_drawentities;
cvar_t	*r_drawsky;
cvar_t	*r_drawsprites;
cvar_t	*r_drawterrain;
cvar_t	*r_drawviewmodel;
cvar_t	*r_drawworld;
cvar_t	*r_dynamiclight;
cvar_t	*r_facePlaneCull;
cvar_t	*r_fadeDistMod;
cvar_t	*r_fastsky;
cvar_t	*r_flares;
cvar_t	*r_flaresMaxDist;
cvar_t	*r_fullbright;
cvar_t	*r_lerpmodels;
cvar_t	*r_lightmap;
cvar_t	*r_lockpvs;
cvar_t	*r_lodbias;
cvar_t	*r_lodCurveError;
cvar_t	*r_lodscale;
cvar_t	*r_lodScaleDistance;
cvar_t	*r_logFile;
cvar_t	*r_nocull;
cvar_t	*r_nocurves;
cvar_t	*r_noportals;
cvar_t	*r_norefresh;
cvar_t	*r_novis;
cvar_t	*r_offsetFactor;
cvar_t	*r_offsetUnits;
cvar_t	*r_portalOnly;
cvar_t	*r_shadows;
cvar_t	*r_showcluster;
cvar_t	*r_showlod;
cvar_t	*r_shownormals;
cvar_t	*r_showsky;
cvar_t	*r_showtris;
cvar_t	*r_singleShader;
cvar_t	*r_subdivisions;
cvar_t	*r_terrain_global;
cvar_t	*r_terrain_global_min;
cvar_t	*r_znear;

#include "vk_firstlight.h"

#include <stdio.h>
#include <string.h>

typedef struct {
	char description[64];
	int width, height;
	int pixelAspect;
} vidmode_t;

static const vidmode_t r_vidModes[] = {
	{ "Mode  0: 320x240",		320,	240,	1 },
	{ "Mode  1: 400x300",		400,	300,	1 },
	{ "Mode  2: 512x384",		512,	384,	1 },
	{ "Mode  3: 640x480",		640,	480,	1 },
	{ "Mode  4: 800x600",		800,	600,	1 },
	{ "Mode  5: 960x720",		960,	720,	1 },
	{ "Mode  6: 1024x768",		1024,	768,	1 },
	{ "Mode  7: 1152x864",		1152,	864,	1 },
	{ "Mode  8: 1280x1024",		1280,	1024,	1 },
	{ "Mode  9: 1600x1200",		1600,	1200,	1 },
	{ "Mode 10: 2048x1536",		2048,	1536,	1 },
	{ "Mode 11: 856x480 (wide)",856,	480,	1 }
};

static int s_numVidModes = ( int )( sizeof( r_vidModes ) / sizeof( r_vidModes[0] ) );

static char s_graphicsInfo[2048];
static cvar_t *r_vkTrace;

cvar_t *r_picmip;
cvar_t *r_roundImagesDown;
cvar_t *r_colorMipLevels;
cvar_t *r_texturebits;
cvar_t *r_colorbits;
cvar_t *r_draw2d;
cvar_t *r_vertexLight;
cvar_t *r_ignoreFastPath;
cvar_t *r_developer;
cvar_t *r_printShaders;
cvar_t *r_overBrightBits;
cvar_t *r_mapOverBrightBits;
cvar_t *r_intensity;
cvar_t *r_gamma;
cvar_t *r_detailtextures;















void R_RegisterCvars( void )
{
	r_picmip              = ri.Cvar_Get( "r_picmip", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_roundImagesDown     = ri.Cvar_Get( "r_roundImagesDown", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_colorMipLevels      = ri.Cvar_Get( "r_colorMipLevels", "0", CVAR_LATCH );
	r_texturebits         = ri.Cvar_Get( "r_texturebits", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_colorbits           = ri.Cvar_Get( "r_colorbits", "32", CVAR_ARCHIVE | CVAR_LATCH );
	r_draw2d              = ri.Cvar_Get( "r_draw2d", "1", 0 );
	r_vertexLight         = ri.Cvar_Get( "r_vertexLight", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_ignoreFastPath      = ri.Cvar_Get( "r_ignoreFastPath", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_developer           = ri.Cvar_Get( "r_developer", "0", 0 );
	r_printShaders        = ri.Cvar_Get( "r_printShaders", "0", 0 );
	r_overBrightBits      = ri.Cvar_Get( "r_overBrightBits", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_mapOverBrightBits   = ri.Cvar_Get( "r_mapOverBrightBits", "0", CVAR_LATCH );
	r_intensity           = ri.Cvar_Get( "r_intensity", "1", CVAR_LATCH );
	r_gamma               = ri.Cvar_Get( "r_gamma", "1.2", CVAR_ARCHIVE );
	r_detailtextures      = ri.Cvar_Get( "r_detailtextures", "1", 33 );

	/* A cvar the module reads but never registers is a null read, so the 3D
	   front end's set is registered here like the twin's. */
	r_detailSurfaces = ri.Cvar_Get( "r_detailSurfaces", "1", CVAR_ARCHIVE  );
	r_allowGeometryDeforms = ri.Cvar_Get( "r_allowGeometryDeforms", "1", CVAR_ARCHIVE  );
	r_subdivisions = ri.Cvar_Get( "r_subdivisions", "3", CVAR_ARCHIVE | CVAR_LATCH );
	r_fullbright = ri.Cvar_Get( "r_fullbright", "0", CVAR_LATCH|CVAR_CHEAT  );
	r_singleShader = ri.Cvar_Get( "r_singleShader", "0", CVAR_CHEAT | CVAR_LATCH  );
	r_lerpmodels = ri.Cvar_Get( "r_lerpmodels", "1", 0 );
	r_lodCurveError = ri.Cvar_Get( "r_lodCurveError", "250", CVAR_ARCHIVE|CVAR_CHEAT  );
	r_lodbias = ri.Cvar_Get( "r_lodbias", "1", CVAR_ARCHIVE );
	r_flares = ri.Cvar_Get( "r_flares", "1", CVAR_ARCHIVE  );
	r_flaresMaxDist = ri.Cvar_Get( "r_flaresMaxDist", "768", CVAR_ARCHIVE  );
	r_znear = ri.Cvar_Get( "r_znear", "4", CVAR_CHEAT  );
	r_fastsky = ri.Cvar_Get( "r_fastsky", "0", 0  );
	r_dynamiclight = ri.Cvar_Get( "r_dynamiclight", "1", CVAR_ARCHIVE  );
	r_facePlaneCull = ri.Cvar_Get( "r_facePlaneCull", "1", CVAR_ARCHIVE  );
	r_showlod = ri.Cvar_Get( "r_showlod", "0", CVAR_TEMP | CVAR_CHEAT );
	r_debugSort = ri.Cvar_Get( "r_debugSort", "0", CVAR_CHEAT  );
	r_nocurves = ri.Cvar_Get( "r_nocurves", "0", CVAR_CHEAT  );
	r_drawworld = ri.Cvar_Get( "r_drawworld", "1", CVAR_CHEAT  );
	r_lightmap = ri.Cvar_Get( "r_lightmap", "0", 0  );
	r_portalOnly = ri.Cvar_Get( "r_portalOnly", "0", CVAR_CHEAT  );
	r_norefresh = ri.Cvar_Get( "r_norefresh", "0", CVAR_CHEAT );
	r_drawentities = ri.Cvar_Get( "r_drawentities", "1", CVAR_CHEAT );
	r_drawcurves = ri.Cvar_Get( "r_drawcurves", "1", CVAR_CHEAT );
	r_drawviewmodel = ri.Cvar_Get( "r_drawviewmodel", "1", CVAR_CHEAT );
	r_drawterrain = ri.Cvar_Get( "r_drawterrain", "1", CVAR_CHEAT );
	r_drawsprites = ri.Cvar_Get( "r_drawsprites", "1", CVAR_CHEAT );
	r_fadeDistMod = ri.Cvar_Get( "r_fadeDistMod", "1", CVAR_ARCHIVE );
	r_lodscale = ri.Cvar_Get( "r_lodscale", "1", 0 );
	r_lodScaleDistance = ri.Cvar_Get( "r_lodScaleDistance", "1", CVAR_ARCHIVE );
	r_nocull = ri.Cvar_Get( "r_nocull", "0", CVAR_CHEAT );
	r_novis = ri.Cvar_Get( "r_novis", "0", CVAR_CHEAT );
	r_showcluster = ri.Cvar_Get( "r_showcluster", "0", CVAR_CHEAT );
	r_logFile = ri.Cvar_Get( "r_logFile", "0", CVAR_CHEAT  );
	r_debugSurface = ri.Cvar_Get( "r_debugSurface", "0", CVAR_CHEAT );
	r_showtris = ri.Cvar_Get( "r_showtris", "0", CVAR_CHEAT );
	r_showsky = ri.Cvar_Get( "r_showsky", "0", CVAR_CHEAT );
	r_drawsky = ri.Cvar_Get( "r_drawsky", "1", 0 );
	r_shownormals = ri.Cvar_Get( "r_shownormals", "0", CVAR_CHEAT );
	r_offsetFactor = ri.Cvar_Get( "r_offsetfactor", "-1", CVAR_CHEAT  );
	r_offsetUnits = ri.Cvar_Get( "r_offsetunits", "-2", CVAR_CHEAT  );
	r_lockpvs = ri.Cvar_Get( "r_lockpvs", "0", CVAR_CHEAT );
	r_noportals = ri.Cvar_Get( "r_noportals", "0", CVAR_CHEAT );
	r_shadows = ri.Cvar_Get( "cg_shadows", "1", 0  );
	r_terrain_global = ri.Cvar_Get( "r_terrain_global", "0", CVAR_CHEAT );
	r_terrain_global_min = ri.Cvar_Get( "r_terrain_global_min", va("%f", -65536.0f), CVAR_CHEAT );

	/* The rest of the 3D front end's cvars, with the twin's names, defaults
	   and flags; unregistered, any of them is a null read. */
	r_debuglines_depthmask = ri.Cvar_Get("r_debuglines_depthmask", "0", CVAR_ARCHIVE);
	r_farplane = ri.Cvar_Get("r_farplane", "0", CVAR_CHEAT);
	r_farplane_color = ri.Cvar_Get("r_farplane_color", ".5 .5 .5", CVAR_CHEAT);
	r_farplane_nocull = ri.Cvar_Get("r_farplane_nocull", "0", CVAR_CHEAT);
	r_farplane_nofog = ri.Cvar_Get("r_farplane_nofog", "0", CVAR_CHEAT);
	r_ignore = ri.Cvar_Get( "r_ignore", "1", CVAR_CHEAT );
	r_jobs = ri.Cvar_Get("r_jobs", "-1", CVAR_ARCHIVE | CVAR_LATCH);
	r_light_ambientEmphasize = ri.Cvar_Get("r_light_ambientEmphasize", "1.6", 0);
	r_light_ambientFudge = ri.Cvar_Get("r_light_ambientFudge", "0.7", 0);
	r_light_emphasize = ri.Cvar_Get("r_light_emphasize", "0", 0);
	r_light_emphasizePercent = ri.Cvar_Get("r_light_emphasizePercent", "0", 0);
	r_light_lines = ri.Cvar_Get("r_light_lines", "0", CVAR_CHEAT);
	r_light_nolight = ri.Cvar_Get("r_light_nolight", "0", CVAR_CHEAT | CVAR_ARCHIVE);
	r_light_simple = ri.Cvar_Get("r_light_simple", "0", 0);
	r_light_simple_dist = ri.Cvar_Get("r_light_simple_dist", "1000", 0);
	r_light_sun_line = ri.Cvar_Get("r_light_sun_line", "0", CVAR_CHEAT);
	r_lightcoronasize = ri.Cvar_Get("r_lightcoronasize", ".1", CVAR_ARCHIVE);
	r_numdebuglines = ri.Cvar_Get("g_numdebuglines", "4096", CVAR_LATCH);
	r_skyportal = ri.Cvar_Get("r_skyportal", "0", 0);
	r_skyportal_origin = ri.Cvar_Get("r_skyportal_origin", "0 0 0", 0);
	r_stipplelines = ri.Cvar_Get("r_stipplelines", "1", CVAR_ARCHIVE);
	r_useglfog = ri.Cvar_Get("r_useglfog", "1", CVAR_ARCHIVE);
}

static cvar_t *s_vkSafeMode;
static qboolean s_vkSafeModeArmed = qfalse;

/* SDK doc 8: a bring-up that dies inside the driver cannot be caught, so the
   next launch needs an on-disk fact.  The marker is armed before the compressed
   texture bring-up and cleared by the first present; present at launch means
   the last attempt died there, and this launch takes the decode-only path. */
#define VK_SAFE_MODE_MARKER ".vk_safe_mode"

qboolean VK_SafeModeMarkerPresent( void )
{
	return ( ri.FS_FileExists && ri.FS_FileExists( VK_SAFE_MODE_MARKER ) ) ? qtrue : qfalse;
}

void VK_SafeModeArm( void )
{
	fileHandle_t f;

	if ( !ri.FS_OpenFileWrite ) {
		return;
	}

	f = ri.FS_OpenFileWrite( VK_SAFE_MODE_MARKER );
	if ( f ) {
		ri.FS_Write( "1", 1, f );
		ri.FS_CloseFile( f );
	}
}

void VK_SafeModeClear( void )
{
	if ( ri.FS_RemoveHomeData ) {
		ri.FS_RemoveHomeData( VK_SAFE_MODE_MARKER );
	}
}

qboolean R_GetModeInfo( int *width, int *height, float *windowAspect, int mode )
{
	const vidmode_t *vm;

	if ( mode < 0 || mode >= s_numVidModes ) {
		return qfalse;
	}

	vm = &r_vidModes[mode];
	*width = vm->width;
	*height = vm->height;
	*windowAspect = ( float )vm->width / ( vm->height * vm->pixelAspect );

	return qtrue;
}

void R_InitExtensions(void) {
}

void R_Init( void )
{
	int err;
	int i;
	byte *ptr;

	memset( &vk, 0, sizeof( vk ) );

	RB_InitBackend();

	/* the twin's front-end initialisation; left out, these were zero tables */
	tr.identityLight = 1.0f;
	tr.identityLightByte = 255;
	tr.overbrightBits = 0;
	/* The compressed-source path is only taken when the device can hold the
	   blocks and the last bring-up did not die taking them.  A prior marker or
	   an explicit r_vkSafeMode 1 forces the decode-only path, which is the one
	   that cannot fault on a device without BC. */
	s_vkSafeMode = ri.Cvar_Get( "r_vkSafeMode", "0", CVAR_ARCHIVE );
	if ( s_vkSafeMode->integer || VK_SafeModeMarkerPresent() ) {
		vkConfig.textureCompression = TC_NONE;
		VK_Guard_Row( "VK:guard",
					  "site=safe-mode detail=\"decode-only texture path\" cause=prior-crash" );
	} else {
		vkConfig.textureCompression = TC_S3TC;
		VK_SafeModeArm();
		s_vkSafeModeArmed = qtrue;
	}

	/* The .so is never unloaded, so one registration covers the process. */
	if ( ri.Hunk_ClearNotify ) {
		ri.Hunk_ClearNotify( RB_2D_OwnersPurged );
	}

	memset( tess.constantColor255, 255, sizeof( tess.constantColor255 ) );

	tr.worldEntity.e.shaderRGBA[0] = -1;
	tr.worldEntity.e.shaderRGBA[1] = 0;
	tr.worldEntity.e.shaderRGBA[2] = -1;
	tr.worldEntity.e.shaderRGBA[3] = -1;

	for ( i = 0; i < FUNCTABLE_SIZE; i++ )
	{
		tr.sinTable[i]		= sin( DEG2RAD( i * 360.0f / ( ( float ) ( FUNCTABLE_SIZE - 1 ) ) ) );
		tr.squareTable[i]	= ( i < FUNCTABLE_SIZE/2 ) ? 1.0f : -1.0f;
		tr.sawToothTable[i] = (float)i / FUNCTABLE_SIZE;
		tr.inverseSawToothTable[i] = 1.0f - tr.sawToothTable[i];

		if ( i < FUNCTABLE_SIZE / 2 )
		{
			if ( i < FUNCTABLE_SIZE / 4 )
			{
				tr.triangleTable[i] = ( float ) i / ( FUNCTABLE_SIZE / 4 );
			}
			else
			{
				tr.triangleTable[i] = 1.0f - tr.triangleTable[i-FUNCTABLE_SIZE / 4];
			}
		}
		else
		{
			tr.triangleTable[i] = -tr.triangleTable[i-FUNCTABLE_SIZE/2];
		}
	}

	tr.rendererhandle = ri.Milliseconds();

	if (!tr.rendererhandle) {
		tr.rendererhandle = -1;
	}

	R_NoiseInit();

	R_Sky_Init();

	R_RegisterCvars();

	/* The demo triangle is scaffolding, off by default and not persisted: an
	   archived "1" drew it over a later session's frames. */
	vk.demoDraw = ri.Cvar_Get( "r_vkDemo", "0", 0 )->integer ? qtrue : qfalse;

	r_vkTrace = ri.Cvar_Get( VK_TRACE_CVAR, "1", 0 );
	ri.Cvar_Get( VK_GUARD_TRACE_CVAR, "1", 0 );

	max_polys = MAX_POLYS;
	max_polyverts = MAX_POLYVERTS;

	for ( i = 0; i < SMP_FRAMES; i++ ) {
		ptr = ri.Malloc(sizeof(backEndData_t) + sizeof(srfPoly_t) * max_polys + sizeof(polyVert_t) * max_polyverts);
		backEndDataBuf[i] = (backEndData_t*)ptr;
		backEndDataBuf[i]->polys = (srfPoly_t*)((char*)ptr + sizeof(backEndData_t));
		backEndDataBuf[i]->polyVerts = (polyVert_t*)((char*)ptr + sizeof(backEndData_t) + sizeof(srfPoly_t) * max_polys);
		backEndDataBuf[i]->staticModels = NULL;
		backEndDataBuf[i]->staticModelData = NULL;
		backEndDataBuf[i]->commands.used = 0;
	}
	backEndData = backEndDataBuf[0];
	R_InitNextFrame();

	/* the twin's context init runs here, so the device exists by registration */
	VK_EnsureBroughtUp( "R_Init" );

	VK_ProgramsInit();

	R_InitExtensions();

	R_ModelInit();

	tr.pFontDebugStrings = NULL;
	g_bInfoworldtris = qfalse;

	JobPool_InitCritical( "vkjob", r_jobs->integer );

	VKimp_AdapterInit();
	R_InitCommandBuffers();

	VK_Guard_Row( "VK:init", "renderer=vulkan api=14 milestone=M3 frames-in-flight=%d", VK_FRAMES_IN_FLIGHT );

	(void)err;
}

void RE_Shutdown( qboolean destroyWindow )
{
	R_ShutdownCommandBuffers();
	VK_BringDown();
	VKimp_Shutdown();

	VK_Guard_Row( "VK:init", "shutdown destroyWindow=%d", destroyWindow ? 1 : 0 );
}

void RE_BeginRegistration( glconfig_t *glconfig )
{
	cvar_t *generation;
	char value[16];

	/* the boot reaches here without ever calling re.SetMode */
	VK_EnsureBroughtUp( "BeginRegistration" );

	if ( ri.UI_LoadResource ) {
		ri.UI_LoadResource( "*123" );
	}

	R_IssuePendingRenderCommands();

	/* The registration that follows frees and re-hands out every image and
	   descriptor the in-flight frames are still reading.  GL got away with it by
	   deferring the frees; retire the frames first. */
	VK_Frame_WaitIdle();

	*glconfig = vkConfig;

	/* The registration sequence the font's shader cache keys on: it grows by
	   exactly one per registration, as the reference's does. */
	generation = ri.Cvar_Get( "r_renderergeneration", "0", CVAR_NORESTART );

	r_sequencenumber = generation->integer + 1;
	if ( r_sequencenumber <= 0 ) {
		r_sequencenumber = 1;
	}

	Com_sprintf( value, sizeof( value ), "%d", r_sequencenumber );
	ri.Cvar_Set( "r_renderergeneration", value );

	tr.registrating = qtrue;

	if ( !tr.registered ) {
		/* the image layer below reads the device the bring-up fills */
		if ( vk.device == VK_NULL_HANDLE ) {
			ri.Error( ERR_FATAL, "RE_BeginRegistration: no Vulkan device; images cannot be created" );
			return;
		}

		R_InitImages();
		R_StartupShaders();
	} else {
		/* Retail re-runs the shader setup on every registration, not only the
		   first.  R_SetupShaders clears shadertext_t::shader over the whole
		   table and resets tr.numShaders, so every shader re-parses in the same
		   order, takes the same index, and re-resolves its stage images with
		   the new r_sequencenumber.  Without it a shader parsed before the load
		   keeps image_t pointers into tr.images[] slots that R_FreeUnusedImages
		   is about to free, and the next R_CreateImage hands that slot to some
		   later texture: the shader then draws the wrong art and the row reads
		   img='' while the slot is still empty. */
		R_SetupShaders();
	}

	/* The reference inits the flares here, after the internal shaders exist: the
	   dlight corona keeps the shader it looks up now for the whole run. */
	R_InitLensFlare();

	tr.registered = qtrue;

	if ( ri.UI_LoadResource ) {
		ri.UI_LoadResource( "*124" );
	}
}









static void RE_EndRegistration( void )
{
	tr.registrating = qfalse;
	R_IssuePendingRenderCommands();
	R_FreeUnusedImages();
}

int RE_GetRegistrationSequence( void )
{
	return r_sequencenumber;
}









void RE_SetDynamicLights( const byte *defaultIntensities, const byte *intensities )
{
}

static vec3_t s_windDirection;

void RE_SetWindDirection( vec3_t direction )
{
	VectorCopy( direction, s_windDirection );
}

void RE_SetWindIntensity( float intensity )
{
}

void RE_SetTimeScale( float timeScale )
{
}

















void R_SavePerformanceCounters( void )
{
}











void RE_BeginFrame( stereoFrame_t stereoFrame )
{
	int *cmd;

	(void)stereoFrame;

	/* The front end is about to fill this slot's list, so it has to be the only
	   writer of it.  Two frames in flight means the front end comes back to a
	   slot while the walk thread may still be reading the frame before last, and
	   both the byte offsets and the prologue position follow from that.  Waiting
	   here is the ring's own back pressure, not a serialization: the walk is
	   already one frame behind, and the front end keeps a full frame of work
	   queued ahead of it. */
	if ( vk.smpActive && !VKimp_IsRenderThread() ) {
		VKimp_FrontEndWaitSlot( tr.smpFrame );
	}

	/* The slot is owned, so the write position starts at the front of the list
	   instead of after whatever the last walk of it left behind. */
	RB_CommandListReset();

	vk_frameOpen = qtrue;

	/* The frame's list opens with the pass marker: the walk acquires, opens the
	   pass and clears, then performs what the frame recorded.  The back end
	   resets its own state there -- see RB_FrameOpen. */
	cmd = ( int * )R_GetCommandBuffer( sizeof( *cmd ) );
	if ( cmd ) {
		*cmd = RC_VK_BEGIN;
	}
}



/* The rate the operator sees, and which side is spending it.  The reported
   decline -- 343 to 230 to 115 frames a second across one session -- was only
   readable by diffing the probe rows, and no row named the split.  One row a
   second gives the whole curve and names front end against back end, so the
   next boot measures the decline directly.  A five second window produced no
   row at all on a boot that only rendered 1.2 seconds of frames, which is why
   this matches the one second cadence the rest of the probe family already
   runs at.  r_perfFps 0 stops it. */
static cvar_t *s_perfFps;

void VK_PerfFpsRow( void )
{
	static int		 frames;
	static long long	 feUsec;
	static long long		 beUsec;
	static int		 t0;
	static int		 baseFps = -1;
	cvar_t			*cv;
	int			 now;
	int			 winFps;
	long long		 fe;
	long long		 be;

	cv = s_perfFps;
	if ( !cv ) {
		cv = s_perfFps = ri.Cvar_Get( "r_perfFps", "1", CVAR_ARCHIVE );
	}
	if ( !cv || !cv->integer ) {
		return;
	}

	frames++;
	feUsec += tr.frontEndMsec * 1000;
	/* The walk owns its own time and drains it here, so be= is real under both
	   r_smp values instead of always 0. */
	beUsec += RB_WalkMsec() * 1000;

	now = ri.Milliseconds();
	if ( !t0 ) {
		t0 = now;
		return;
	}

	if ( now - t0 < 1000 ) {
		return;
	}

	/* The first window is the baseline: every later row carries the drift off
	   it, so the decline is one number instead of a series to eyeball. */
	fe = feUsec / 1000;
	be = beUsec / 1000;
	winFps = ( int )( ( double )frames * 1000.0 / ( double )( now - t0 ) );
	if ( baseFps < 0 ) {
		baseFps = winFps;
	}

	ri.Printf( PRINT_ALL,
			   "[PERF:fps] t=%ds fps=%d drift=%+d%% base=%d fe=%lldms be=%lldms park=%dms frames=%d smp=%d\n",
			   ( int )( ( now - t0 ) / 1000 ), winFps,
			   baseFps ? ( ( winFps - baseFps ) * 100 / baseFps ) : 0, baseFps,
			   fe, be, VKimp_ParkMsec(), frames, vk.smpActive ? 1 : 0 );

	frames = 0;
	feUsec = 0;
	beUsec = 0;
	t0     = now;
}

void RE_EndFrame( int *frontEndMsec, int *backEndMsec )
{
	int *cmd;

	/* The back end owns the open 2D batch and closes it at the present marker,
	   in its own walk.  Flushing it here reached into tess and the batch state
	   from the front end while the render thread was walking them. */
	cmd = ( int * )R_GetCommandBuffer( sizeof( *cmd ) );
	if ( cmd ) {
		*cmd = RC_VK_PRESENT;
	}

	R_IssueRenderCommands();
	vk_frameOpen = qfalse;

	/* A completed frame is the bring-up succeeding, so the sentinel that
	   guards the next launch against a crash during it comes down.  It comes
	   down here, on the main thread: the present site runs on the walk thread
	   under r_smp, and the filesystem is not the walk's to touch. */
	if ( s_vkSafeModeArmed ) {
		s_vkSafeModeArmed = qfalse;
		VK_SafeModeClear();
	}

	/* The frame boundary: the scene pools, the back-end slot and tr.smpFrame are budgets the frame's
	   walk has consumed. The reference renderer runs it here; this module ran it only from R_Init, so
	   the pools drained once and stayed drained -- the entity pool hit its cap ~1.3s into the menu. */
	R_InitNextFrame();

	if ( frontEndMsec ) {
		*frontEndMsec = tr.frontEndMsec;
	}
	if ( backEndMsec ) {
		*backEndMsec = backEnd.backEndMsec;
	}

	VK_PerfFpsRow();

	tr.frontEndMsec = 0;
	backEnd.backEndMsec = 0;
}























static void RE_SetRenderTime( int t )
{
	backEnd.refdefFloatTime = ( float )t / 1000.0f;
}




static const char *RE_GetGraphicsInfo( void )
{
	return s_graphicsInfo;
}

/* the export table's slot returns void and R_LightForPoint returns int */
static void RE_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight,
							  vec3_t lightDir )
{
	(void)R_LightForPoint( point, ambientLight, directedLight, lightDir );
}


























const char *RE_GetModelName( qhandle_t hModel )
{
	return "";
}



















void RE_TakeVideoFrame( int h, int w, byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg )
{
}

#ifdef USE_RENDERER_DLOPEN
Q_EXPORT refexport_t *QDECL GetRefAPI( int apiVersion, refimport_t *rimp )
#else
refexport_t *GetRefAPI( int apiVersion, refimport_t *rimp )
#endif
{
	static refexport_t re;

	ri = *rimp;

	memset( &re, 0, sizeof( re ) );

	if ( apiVersion != REF_API_VERSION ) {
		ri.Printf( PRINT_ALL, "Mismatched REF_API_VERSION: expected %i, got %i\n",
				   REF_API_VERSION, apiVersion );
		return NULL;
	}

	R_Init();

	re.Shutdown = RE_Shutdown;

	re.BeginRegistration = RE_BeginRegistration;
	re.RegisterModel = RE_RegisterModel;
	re.ForceRegisterModel = RE_ForceRegisterModel;
	re.RegisterSkin = RE_RegisterSkin;
	re.RegisterShader = RE_RegisterShader;
	re.RegisterShaderNoMip = RE_RegisterShaderNoMip;
	re.RefreshShaderNoMip = RE_RefreshShaderNoMip;
	re.RegisterShaderNoMip = RE_RegisterShaderNoMip;

	re.EndRegistration = RE_EndRegistration;

	re.GetRegistrationSequence = RE_GetRegistrationSequence;
	re.ReplaceShader = RE_ReplaceShader;
	re.SetWorldVisData = RE_SetWorldVisData;
	re.LoadWorld = RE_LoadWorldMap;

	re.ClearScene = RE_ClearScene;
	re.ClearFlares = R_ClearLensflareStates;
	re.SetDynamicLights = RE_SetDynamicLights;
	re.SetWindDirection = RE_SetWindDirection;
	re.SetWindIntensity = RE_SetWindIntensity;
	re.SetTimeScale = RE_SetTimeScale;

	re.AddRefEntityToScene = RE_AddRefEntityToScene;
	re.AddRefSpriteToScene = RE_AddRefSpriteToScene;
	re.AddPolyToScene = RE_AddPolyToScene;
	re.AddLightToScene = RE_AddLightToScene;
	re.AddLensflareToScene = RE_AddLensflareToScene;
	re.LensflareNameToNum = R_LensflareNameToNum;
	re.RenderScene = RE_RenderScene;
	re.GetRenderEntity = RE_GetRenderEntity;
	re.SavePerformanceCounters = R_SavePerformanceCounters;

	re.SetColor = Draw_SetColor;
	re.Set2DWindow = Set2DWindow;
	re.DrawStretchPic = Draw_StretchPic;
	re.DrawTilePic = Draw_TilePic;
	re.DrawTilePicOffset = Draw_TilePicOffset;
	re.DrawStretchRaw = RE_StretchRaw;
	re.UploadCinematic = RE_UploadCinematic;
	re.DebugLine = R_DebugLine;
	re.DrawBox = DrawBox;
	re.AddBox = AddBox;

	re.BeginFrame = RE_BeginFrame;
	re.Scissor = RE_Scissor;
	re.DrawLineLoop = DrawLineLoop;
	re.EndFrame = RE_EndFrame;
	re.MarkFragments = R_MarkFragments;
	re.LerpTag = R_LerpTag;
	re.ModelBounds = R_ModelBounds;
	re.ModelRadius = R_ModelRadius;

	re.R_Model_GetHandle = R_Model_GetHandle;
	re.TIKI_FlushAll = TIKI_FlushAll;
	re.DrawString = R_DrawString;
	re.GetFontHeight = R_GetFontHeight;
	re.GetFontStringWidth = R_GetFontStringWidth;
	re.LoadFont = R_LoadFont;

	re.SwipeBegin = RE_SwipeBegin;
	re.SwipePoint = RE_SwipePoint;
	re.SwipeEnd = RE_SwipeEnd;
	re.SetRenderTime = RE_SetRenderTime;
	re.Noise = R_NoiseGet4f;
	re.SetMode = R_SetMode;
	re.SetFullscreen = R_SetFullscreen;

	re.GetShaderWidth = RE_GetShaderWidth;
	re.GetShaderHeight = RE_GetShaderHeight;
	re.GetGraphicsInfo = RE_GetGraphicsInfo;

	re.LightForPoint = RE_LightForPoint;
	re.AddAdditiveLightToScene = RE_AddAdditiveLightToScene;
	re.SpawnEffectModel = RE_SpawnEffectModel;
	re.RegisterServerModel = RE_RegisterServerModel;
	re.UnregisterServerModel = RE_UnregisterServerModel;
	re.PrintBSPFileSizes = RE_PrintBSPFileSizes;
	re.MapVersion = RE_MapVersion;
	re.AddTerrainMarkToScene = RE_AddTerrainMarkToScene;
	re.DrawStretchPic2 = Draw_StretchPic2;
	re.DrawTrianglePic = Draw_TrianglePic;
	re.MarkFragmentsForInlineModel = R_MarkFragmentsForInlineModel;
	re.GetInlineModelBounds = R_GetInlineModelBounds;
	re.GetLightingForDecal = R_GetLightingForDecal;
	re.GetLightingForSmoke = R_GetLightingForSmoke;
	re.R_GatherLightSources = R_GatherLightSources;
	re.GetShaderName = RE_GetShaderName;
	re.GetModelName = RE_GetModelName;
	re.ForceUpdatePose = RE_ForceUpdatePose;
	re.TIKI_Orientation = RE_TIKI_Orientation;
	re.TIKI_IsOnGround = RE_TIKI_IsOnGround;
	re.SetFrameNumber = RE_SetFrameNumber;
	re.ImageExists = R_ImageExists;
	re.CountTextureMemory = R_CountTextureMemory;
	re.LoadRawImage = R_LoadRawImage;
	re.FreeRawImage = R_FreeRawImage;
	re.Set2DInitialShaderTime = Set2DInitialShaderTime;
	re.RegisterFont = RE_RegisterFont;
	re.RemapShader = R_RemapShader;
	re.GetEntityToken = R_GetEntityToken;
	re.inPVS = R_inPVS;
	re.TakeVideoFrame = RE_TakeVideoFrame;

	(void)s_graphicsInfo;

	return &re;
}
