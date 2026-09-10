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

	r_vkTrace = ri.Cvar_Get( VK_TRACE_CVAR, "1", 0 );
	ri.Cvar_Get( VK_GUARD_TRACE_CVAR, "1", 0 );

	vk.demoDraw = ri.Cvar_Get( "r_vkDemo", "1", CVAR_ARCHIVE )->integer ? qtrue : qfalse;

	VKimp_AdapterInit();
	R_InitCommandBuffers();

	VK_Guard_Row( "VK:init", "renderer=vulkan api=14 milestone=M1 frames-in-flight=%d", VK_FRAMES_IN_FLIGHT );
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
	*config = vk.glConfig;
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

static qhandle_t RE_RegisterSkin( const char *name )
{
	return 0;
}

static qhandle_t RE_RegisterShader( const char *name )
{
	RE_StubRow( "RegisterShader" );
	return 0;
}

qhandle_t RE_RegisterShaderNoMip( const char *name )
{
	return 0;
}

static qhandle_t RE_RefreshShaderNoMip( const char *name )
{
	return 0;
}

static void RE_EndRegistration( void )
{
	R_SyncRenderThread();
}

static int RE_GetRegistrationSequence( void )
{
	return 0;
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

static void Draw_SetColor( const float *rgba )
{
}

static void Set2DWindow( int x, int y, int w, int h, float left, float right, float bottom, float top,
						 float n, float f )
{
}

static void Draw_StretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2,
							 qhandle_t hShader )
{
	RE_StubRow( "DrawStretchPic" );
}

static void Draw_TilePic( float x, float y, float w, float h, qhandle_t hShader )
{
}

static void Draw_TilePicOffset( float x, float y, float w, float h, qhandle_t hShader, int offsetX, int offsetY )
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

static void DrawBox( float x, float y, float w, float h )
{
}

static void AddBox( float x, float y, float w, float h )
{
}

static void RE_BeginFrame( stereoFrame_t stereoFrame )
{
}

static void RE_Scissor( int x, int y, int width, int height )
{
}

static void DrawLineLoop( const vec2_t *points, int count, int stippleFactor, int stippleMask )
{
}

static void RE_EndFrame( int *frontEndMsec, int *backEndMsec )
{
	R_IssueRenderCommands();

	if ( frontEndMsec ) {
		*frontEndMsec = 0;
	}
	if ( backEndMsec ) {
		*backEndMsec = 0;
	}
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

static void R_DrawString( fontheader_t *font, const char *text, float x, float y, int maxLen,
						  const float *pvVirtualScreen )
{
}

static float R_GetFontHeight( const fontheader_t *font )
{
	return 0.0f;
}

static float R_GetFontStringWidth( const fontheader_t *font, const char *string )
{
	return 0.0f;
}

static fontheader_t *R_LoadFont( const char *name )
{
	return NULL;
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
}

static float R_NoiseGet4f( float x, float y, float z, double t )
{
	return 0.0f;
}

static int RE_GetShaderWidth( qhandle_t hShader )
{
	return 0;
}

static int RE_GetShaderHeight( qhandle_t hShader )
{
	return 0;
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

static void Draw_StretchPic2( float x, float y, float w, float h, float s1, float t1, float s2, float t2,
							  float sx, float sy, qhandle_t hShader )
{
}

static void Draw_TrianglePic( const vec2_t vPoints[3], const vec2_t vTexCoords[3], qhandle_t hShader )
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

qboolean R_ImageExists( const char *name )
{
	return qfalse;
}

static int R_CountTextureMemory( void )
{
	return 0;
}

static qboolean R_LoadRawImage( const char *name, byte **pic, int *width, int *height )
{
	*pic = NULL;
	*width = 0;
	*height = 0;

	return qfalse;
}

static void R_FreeRawImage( byte *pic )
{
}

static void Set2DInitialShaderTime( float startTime )
{
}

static void RE_RegisterFont( const char *fontName, int pointSize, fontInfo_t *font )
{
	memset( font, 0, sizeof( *font ) );
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
