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

#include <stdio.h>
#include <string.h>

refimport_t ri;

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

/* ---- the cvars the renderer's data-model layer reads ---------------------
 * The same names and defaults the GLES3 twin registers, so a config written
 * for one renderer means the same thing to the other.  The 3D-only tail
 * (lod/flare/sky/lighting) arrives with the 3D stage iterators; these are the
 * ones the shader, image, font and 2D layers read today. */
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

static void RE_StubRow( const char *what )
{
	static int rows = 0;

	if ( !VK_TraceOn( VK_TRACE_CVAR, 1 ) ) {
		return;
	}

	if ( rows >= 8 && ( rows % 600 ) != 0 ) {
		rows++;
		return;
	}

	VK_Guard_Row( "VK:stub", "call=%s n=%d", what, rows );
	rows++;
}

void R_Init( void )
{
	memset( &vk, 0, sizeof( vk ) );

	RB_InitBackend();

	/* The identity light the 2D colour path scales by: no overbright on the
	   Vulkan leg yet, so the byte scale is the full 255. */
	tr.identityLight = 1.0f;
	tr.identityLightByte = 255;
	tr.overbrightBits = 0;
	glConfig.textureCompression = TC_S3TC;

	r_vkTrace = ri.Cvar_Get( VK_TRACE_CVAR, "1", 0 );
	ri.Cvar_Get( VK_GUARD_TRACE_CVAR, "1", 0 );

	R_RegisterCvars();

	vk.demoDraw = ri.Cvar_Get( "r_vkDemo", "1", CVAR_ARCHIVE )->integer ? qtrue : qfalse;

	VKimp_AdapterInit();
	R_InitCommandBuffers();

	VK_Guard_Row( "VK:init", "renderer=vulkan api=14 milestone=M2 frames-in-flight=%d", VK_FRAMES_IN_FLIGHT );
}

void RE_Shutdown( qboolean destroyWindow )
{
	R_ShutdownCommandBuffers();
	VK_BringDown();
	VKimp_Shutdown();

	VK_Guard_Row( "VK:init", "shutdown destroyWindow=%d", destroyWindow ? 1 : 0 );
}

static void RE_BeginRegistration( glconfig_t *config )
{
	cvar_t *generation;
	char value[16];

	if ( ri.UI_LoadResource ) {
		ri.UI_LoadResource( "*123" );
	}

	R_IssuePendingRenderCommands();

	*config = glConfig;

	/* The registration sequence the font's shader cache keys on: it grows by
	   exactly one per registration, exactly as the GLES3 twin's does. */
	generation = ri.Cvar_Get( "r_renderergeneration", "0", CVAR_NORESTART );

	r_sequencenumber = generation->integer + 1;
	if ( r_sequencenumber <= 0 ) {
		r_sequencenumber = 1;
	}

	Com_sprintf( value, sizeof( value ), "%d", r_sequencenumber );
	ri.Cvar_Set( "r_renderergeneration", value );

	tr.registrating = qtrue;

	if ( !tr.registered ) {
		R_InitImages();
		R_StartupShaders();
	}

	tr.registered = qtrue;

	if ( ri.UI_LoadResource ) {
		ri.UI_LoadResource( "*124" );
	}
}

static qhandle_t RE_RegisterModel( const char *name )
{
	RE_StubRow( "RegisterModel" );
	return 0;
}

static qhandle_t RE_ForceRegisterModel( const char *name )
{
	return 0;
}





static void RE_EndRegistration( void )
{
	tr.registrating = qfalse;
	R_IssuePendingRenderCommands();
	R_FreeUnusedImages();
}

static int RE_GetRegistrationSequence( void )
{
	return r_sequencenumber;
}

static qhandle_t RE_ReplaceShader( const char *name, const char *text )
{
	return 0;
}

static void RE_SetWorldVisData( const byte *vis )
{
}

static void RE_LoadWorldMap( const char *name )
{
	RE_StubRow( "LoadWorld" );
}

static void RE_ClearScene( void )
{
}

static void R_ClearLensflareStates( void )
{
}

static void RE_SetDynamicLights( const byte *defaultIntensities, const byte *intensities )
{
}

static vec3_t s_windDirection;

static void RE_SetWindDirection( vec3_t direction )
{
	VectorCopy( direction, s_windDirection );
}

static void RE_SetWindIntensity( float intensity )
{
}

static void RE_SetTimeScale( float timeScale )
{
}

static void RE_AddRefEntityToScene( const refEntity_t *re )
{
}

static void RE_AddRefSpriteToScene( const refEntity_t *ent )
{
}

static qboolean RE_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts, int num )
{
	return qfalse;
}

static void RE_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b, int type )
{
}

static void RE_AddLensflareToScene( void *key, const vec3_t org, int lensflareNum, float r, float g, float b,
									float scale, qhandle_t replacementShader )
{
}

static int R_LensflareNameToNum( const char *name )
{
	return -1;
}

static void RE_RenderScene( const refdef_t *fd )
{
	RE_StubRow( "RenderScene" );
}

static refEntity_t *RE_GetRenderEntity( int entityNumber )
{
	return NULL;
}

static void R_SavePerformanceCounters( void )
{
}






static void RE_StretchRaw( int x, int y, int w, int h, int cols, int rows, int components, const byte *data,
						   qboolean dirty )
{
}

static void RE_UploadCinematic( int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty )
{
}

static void R_DebugLine( const vec3_t start, const vec3_t end, float r, float g, float b, float alpha )
{
}



static void RE_BeginFrame( stereoFrame_t stereoFrame )
{
	int *cmd;

	(void)stereoFrame;

	/* The frame's list opens with the pass marker: the walk acquires, opens
	   the render pass and clears, then performs whatever the frame recorded. */
	RB_RecordFramePrologue();

	cmd = ( int * )R_GetCommandBuffer( sizeof( *cmd ) );
	if ( cmd ) {
		*cmd = RC_VK_BEGIN;
	}
}



static void RE_EndFrame( int *frontEndMsec, int *backEndMsec )
{
	int *cmd;

	/* The 2D batch the UI left open belongs to this frame; closing it before
	   the present marker is what keeps the last quad in the right frame. */
	RB_2D_Flush();

	cmd = ( int * )R_GetCommandBuffer( sizeof( *cmd ) );
	if ( cmd ) {
		*cmd = RC_VK_PRESENT;
	}

	R_IssueRenderCommands();

	if ( frontEndMsec ) {
		*frontEndMsec = tr.frontEndMsec;
	}
	if ( backEndMsec ) {
		*backEndMsec = backEnd.backEndMsec;
	}

	tr.frontEndMsec = 0;
	backEnd.backEndMsec = 0;
}

static int R_MarkFragments( int numPoints, const vec3_t *points, const vec3_t projection,
							int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer,
							float fRadiusSquared )
{
	return 0;
}

static int R_LerpTag( orientation_t *tag, qhandle_t model, int startFrame, int endFrame,
					  float frac, const char *tagName )
{
	return 0;
}

static void R_ModelBounds( qhandle_t model, vec3_t mins, vec3_t maxs )
{
	VectorClear( mins );
	VectorClear( maxs );
}

static float R_ModelRadius( qhandle_t handle )
{
	return 0.0f;
}

static dtiki_t *R_Model_GetHandle( qhandle_t handle )
{
	return NULL;
}

static void TIKI_FlushAll( void )
{
}





static void RE_SwipeBegin( float thisTime, float life, qhandle_t hShader )
{
}

static void RE_SwipePoint( vec3_t point1, vec3_t point2, float time )
{
}

static void RE_SwipeEnd( void )
{
}

static void RE_SetRenderTime( int t )
{
	backEnd.refdefFloatTime = ( float )t / 1000.0f;
}




static const char *RE_GetGraphicsInfo( void )
{
	return s_graphicsInfo;
}

static void R_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir )
{
}

static void RE_AddAdditiveLightToScene( const vec3_t org, float intensity, float r, float g, float b )
{
}

static qhandle_t RE_SpawnEffectModel( const char *name, vec3_t pos, vec3_t axis[3] )
{
	return 0;
}

static qhandle_t RE_RegisterServerModel( const char *name )
{
	return 0;
}

static void RE_UnregisterServerModel( qhandle_t model )
{
}

static void RE_PrintBSPFileSizes( void )
{
}

static int RE_MapVersion( void )
{
	return 0;
}

static void RE_AddTerrainMarkToScene( int terrainIndex, qhandle_t hShader, int numVerts,
									  const polyVert_t *verts, int renderfx )
{
}



static void RE_DrawBackground( int cols, int rows, int bgr, uint8_t *data )
{
}

static int R_MarkFragmentsForInlineModel( clipHandle_t bmodel, const vec3_t vAngles, const vec3_t vOrigin,
										  int numPoints, const vec3_t *points, const vec3_t projection,
										  int maxPoints, vec3_t pointBuffer, int maxFragments,
										  markFragment_t *fragmentBuffer )
{
	return 0;
}

static void R_GetInlineModelBounds( int index, vec3_t mins, vec3_t maxs )
{
	VectorClear( mins );
	VectorClear( maxs );
}

static void R_GetLightingForDecal( vec3_t light, const vec3_t facing, const vec3_t origin )
{
	VectorClear( light );
}

static void R_GetLightingForSmoke( vec3_t light, const vec3_t origin )
{
	VectorClear( light );
}

static int R_GatherLightSources( const vec3_t pos, vec3_t *lightPos, vec3_t *lightIntensity, int maxLights )
{
	return 0;
}

static const char *RE_GetShaderName( qhandle_t hShader )
{
	return "";
}

static const char *RE_GetModelName( qhandle_t hModel )
{
	return "";
}

static void RE_ForceUpdatePose( refEntity_t *model )
{
}

static orientation_t RE_TIKI_Orientation( refEntity_t *model, int tagNum )
{
	orientation_t orient;

	memset( &orient, 0, sizeof( orient ) );

	return orient;
}

static qboolean RE_TIKI_IsOnGround( refEntity_t *model, int tagNum, float threshold )
{
	return qfalse;
}

static void RE_SetFrameNumber( int frameNumber )
{
}







static void R_RemapShader( const char *oldShader, const char *newShader )
{
}

static qboolean R_GetEntityToken( char *buffer, int size )
{
	if ( buffer && size > 0 ) {
		buffer[0] = '\0';
	}

	return qfalse;
}

static qboolean R_inPVS( const vec3_t p1, const vec3_t p2 )
{
	return qtrue;
}

static void RE_TakeVideoFrame( int h, int w, byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg )
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

	re.LightForPoint = R_LightForPoint;
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
