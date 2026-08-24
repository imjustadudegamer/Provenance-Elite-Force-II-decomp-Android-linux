/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef __TR_PUBLIC_H
#define __TR_PUBLIC_H

#include "tr_types.h"

#include <stddef.h>	 

#ifdef __cplusplus
extern "C" {
#endif

#define	REF_API_VERSION		14

typedef struct dtiki_s dtiki_t;
typedef struct skelAnimFrame_s skelAnimFrame_t;


// (files owned by a different change) are moved onto retail's paths.
typedef struct {
	void	(*Shutdown)( qboolean destroyWindow );						 

	void	(*BeginRegistration)( glconfig_t *config );					 
	qhandle_t (*RegisterModel)( const char *name );						 
	qhandle_t (*ForceRegisterModel)( const char *name );				 
	qhandle_t (*RegisterSkin)( const char *name );						 
	qhandle_t (*RegisterShader)( const char *name );					 
	qhandle_t (*RegisterShaderNoMip)( const char *name );				 
	qhandle_t (*RefreshShaderNoMip)(const char* name);					 

	void	(*EndRegistration)( void );									 

	int		(*GetRegistrationSequence)( void );							 
	qhandle_t (*ReplaceShader)( const char *name, const char *text );	 
	void	(*SetWorldVisData)( const byte *vis );						 
	void	(*LoadWorld)( const char *name );							 

	void	(*ClearScene)( void );										 
	void	(*ClearFlares)( void );										 
	void (*SetDynamicLights)(const byte *defaultIntensities, const byte *intensities);	 
	void (*SetWindDirection)(vec3_t direction);							 
	void (*SetWindIntensity)(float intensity);							 
	void (*SetTimeScale)(float timeScale);								 

	void	(*AddRefEntityToScene)( const refEntity_t *re );	 
	void (*AddRefSpriteToScene)(const refEntity_t* ent);				 
	qboolean(*AddPolyToScene)( qhandle_t hShader , int numVerts, const polyVert_t *verts, int num );	 
	void	(*AddLightToScene)( const vec3_t org, float intensity, float r, float g, float b, int type );	 
	// R_LensflareNameToNum @0x00047F30).  `key` is compared, never dereferenced.
	void	(*AddLensflareToScene)( void *key, const vec3_t org, int lensflareNum, float r, float g, float b, float scale, qhandle_t replacementShader );	 
	int		(*LensflareNameToNum)( const char *name );					 
	void	(*RenderScene)( const refdef_t *fd );						 
	refEntity_t* (*GetRenderEntity)(int entityNumber);					 
	void    (*SavePerformanceCounters)(void);							 

	void	(*SetColor)( const float *rgba );	 
	void    (*Set2DWindow)(int x, int y, int w, int h, float left, float right, float bottom, float top, float n, float f);	 
	void	(*DrawStretchPic) ( float x, float y, float w, float h,
		float s1, float t1, float s2, float t2, qhandle_t hShader );	 
	void    (*DrawTilePic)(float x, float y, float w, float h, qhandle_t hShader);		 
	void    (*DrawTilePicOffset)(float x, float y, float w, float h, qhandle_t hShader, int offsetX, int offsetY);	 
	void	(*DrawStretchRaw) (int x, int y, int w, int h, int cols, int rows, int components, const byte* data, qboolean dirty);	 
	void	(*UploadCinematic) (int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty);	 
	void    (*DebugLine)(const vec3_t start, const vec3_t end, float r, float g, float b, float alpha);	 
	void    (*DrawBox)(float x, float y, float w, float h);				 
	void    (*AddBox)(float x, float y, float w, float h);				 

	void	(*BeginFrame)( stereoFrame_t stereoFrame );					 
	void    (*Scissor)(int x, int y, int width, int height);			 
	void    (*DrawLineLoop)(const vec2_t* points, int count, int stippleFactor, int stippleMask);	 
	void	(*EndFrame)( int *frontEndMsec, int *backEndMsec );			 
	int		(*MarkFragments)( int numPoints, const vec3_t *points, const vec3_t projection,
				   int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer, float fRadiusSquared );	 
	int		(*LerpTag)( orientation_t *tag,  qhandle_t model, int startFrame, int endFrame,
					 float frac, const char *tagName );				 
	void	(*ModelBounds)( qhandle_t model, vec3_t mins, vec3_t maxs );	 
	float   (*ModelRadius)(qhandle_t handle);							 

	dtiki_t*        (*R_Model_GetHandle)(qhandle_t handle);				 
	void            (*TIKI_FlushAll)(void);								 
	void            (*DrawString)(fontheader_t* font, const char* text, float x, float y, int maxLen, const float *pvVirtualScreen);	 
	float           (*GetFontHeight)(const fontheader_t* font);			 
	float           (*GetFontStringWidth)(const fontheader_t* font, const char* string);	 
	fontheader_t*   (*LoadFont)(const char* name);						 

	void            (*SwipeBegin)(float thisTime, float life, qhandle_t hShader);	 
	void            (*SwipePoint)(vec3_t point1, vec3_t point2, float time);		 
	void            (*SwipeEnd)(void);									 
	void            (*SetRenderTime)(int t);							 
	float           (*Noise)(float x, float y, float z, double t);		 
	qboolean        (*SetMode)(int mode, glconfig_t* glConfig);			 
	void            (*SetFullscreen)(qboolean fullScreen, glconfig_t* glConfig);	 

	int             (*GetShaderWidth)(qhandle_t hShader);				 
	int             (*GetShaderHeight)(qhandle_t hShader);				 
	const char*     (*GetGraphicsInfo)(void);							 

	void	(*LightForPoint)( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );
	void	(*AddAdditiveLightToScene)( const vec3_t org, float intensity, float r, float g, float b );
#ifdef __USEA3D
	void    (*A3D_RenderGeometry) (void *pVoidA3D, void *pVoidGeom, void *pVoidMat, void *pVoidGeomStatus);
#endif
	void	(*RegisterFont)(const char *fontName, int pointSize, fontInfo_t *font);
	void	(*RemapShader)(const char *oldShader, const char *newShader);
	qboolean (*GetEntityToken)( char *buffer, int size );
	qboolean (*inPVS)( const vec3_t p1, const vec3_t p2 );
	void (*TakeVideoFrame)( int h, int w, byte* captureBuffer, byte *encodeBuffer, qboolean motionJpeg );
	qhandle_t(*SpawnEffectModel)(const char* name, vec3_t pos, vec3_t axis[3]);
	qhandle_t(*RegisterServerModel)(const char* name);
	void (*UnregisterServerModel)(qhandle_t model);
	void (*PrintBSPFileSizes)(void);
	int (*MapVersion)(void);
	void (*AddTerrainMarkToScene)(int terrainIndex, qhandle_t hShader, int numVerts, const polyVert_t* verts, int renderfx);
	void	(*DrawStretchPic2)(float x, float y, float w, float h, float s1, float t1, float s2, float t2, float sx, float sy, qhandle_t hShader);
	void    (*DrawTrianglePic)(const vec2_t vPoints[3], const vec2_t vTexCoords[3], qhandle_t hShader);
	void    (*DrawBackground)(int cols, int rows, int bgr, uint8_t* data);
	int     (*MarkFragmentsForInlineModel)(clipHandle_t bmodel, const vec3_t vAngles, const vec3_t vOrigin, int numPoints,
                const vec3_t* points, const vec3_t projection, int maxPoints, vec3_t pointBuffer,
                int maxFragments, markFragment_t* fragmentBuffer);
	void    (*GetInlineModelBounds)(int index, vec3_t mins, vec3_t maxs);
	void    (*GetLightingForDecal)(vec3_t light, const vec3_t facing, const vec3_t origin);
	void    (*GetLightingForSmoke)(vec3_t light, const vec3_t origin);
	int     (*R_GatherLightSources)(const vec3_t pos, vec3_t* lightPos, vec3_t* lightIntensity, int maxLights);
	const char*		(*GetShaderName)(qhandle_t hShader);
	const char*		(*GetModelName)(qhandle_t hModel);
	void            (*ForceUpdatePose)(refEntity_t* model);
	orientation_t   (*TIKI_Orientation)(refEntity_t* model, int tagNum);
	qboolean        (*TIKI_IsOnGround)(refEntity_t* model, int tagNum, float threshold);
	void            (*SetFrameNumber)(int frameNumber);
	qboolean (*ImageExists)(const char* name);
	int (*CountTextureMemory)(void);
	qboolean (*LoadRawImage)(const char *name, byte **pic, int *width, int *height);
	void (*FreeRawImage)(byte *pic);
	void (*Set2DInitialShaderTime)(float startTime);
} refexport_t;

#define REFEXPORT_RETAIL_SLOTS	62
typedef char refexport_retail_slot_count_check[
	( offsetof( refexport_t, LightForPoint ) == REFEXPORT_RETAIL_SLOTS * sizeof( void * ) ) ? 1 : -1 ];
typedef char refexport_replace_shader_slot_check[
	( offsetof( refexport_t, ReplaceShader ) == 10 * sizeof( void * ) ) ? 1 : -1 ];
typedef char refexport_shutdown_slot_check[
	( offsetof( refexport_t, Shutdown ) == 0 ) ? 1 : -1 ];
typedef char refexport_getgraphicsinfo_slot_check[
	( offsetof( refexport_t, GetGraphicsInfo ) == 61 * sizeof( void * ) ) ? 1 : -1 ];

typedef struct {
	void	(QDECL *Printf)( int printLevel, const char *fmt, ...) Q_PRINTF_FUNC(2, 3);

	void	(QDECL *Error)( int errorLevel, const char *fmt, ...) Q_NO_RETURN Q_PRINTF_FUNC(2, 3);

	int		(*Milliseconds)( void );

#ifdef HUNK_DEBUG
	void	*(*Hunk_AllocDebug)( int size, ha_pref pref, char *label, char *file, int line );
#else
	void	*(*Hunk_Alloc)( int size, ha_pref pref );
#endif
	void	*(*Hunk_AllocateTempMemory)( int size );
	void	(*Hunk_FreeTempMemory)( void *block );

	void	*(*Malloc)( int bytes );
	void	(*Free)( void *buf );

	cvar_t	*(*Cvar_Get)( const char *name, const char *value, int flags );
	void	(*Cvar_Set)( const char *name, const char *value );
	void	(*Cvar_SetValue) (const char *name, float value);
	void	(*Cvar_CheckRange)( cvar_t *cv, float minVal, float maxVal, qboolean shouldBeIntegral );
	void	(*Cvar_SetDescription)( cvar_t *cv, const char *description );

	int		(*Cvar_VariableIntegerValue) (const char *var_name);

	void	(*Cmd_AddCommand)( const char *name, void(*cmd)(void) );
	void	(*Cmd_RemoveCommand)( const char *name );

	int		(*Cmd_Argc) (void);
	char	*(*Cmd_Argv) (int i);

	void	(*Cmd_ExecuteText) (int exec_when, const char *text);

	byte	*(*CM_ClusterPVS)(int cluster);

	void	(*CM_DrawDebugSurface)( void (*drawPoly)(int color, int numPoints, float *points) );

	int		(*FS_FileIsInPAK)( const char *name, int *pCheckSum );
	long		(*FS_ReadFile)( const char *name, void **buf );
	void	(*FS_FreeFile)( void *buf );
	char **	(*FS_ListFiles)( const char *name, const char *extension, int *numfilesfound );
	void	(*FS_FreeFileList)( char **filelist );
	void	(*FS_WriteFile)( const char *qpath, const void *buffer, int size );
	qboolean (*FS_FileExists)( const char *file );

	void	(*CIN_UploadCinematic)(int handle);
	int		(*CIN_PlayCinematic)( const char *arg0, int xpos, int ypos, int width, int height, int bits);
	e_status (*CIN_RunCinematic) (int handle);


	void	(*IN_Init)( void *windowData );
	void	(*IN_Shutdown)( void );
	void	(*IN_Restart)( void );

	long    (*ftol)(float f);

	void	(*Sys_SetEnv)( const char *name, const char *value );
	void	(*Sys_GLimpSafeInit)( void );
	void	(*Sys_GLimpInit)( void );
	qboolean (*Sys_LowPhysicalMemory)( void );

    void (*Hunk_Clear)(void);
    void (*Clear)(void);
    void (*Cvar_SetDefault)(cvar_t* var, const char* varValue);

    long    (*FS_OpenFile)(const char* qpath, fileHandle_t *file, qboolean uniqueFILE, qboolean quiet);
	fileHandle_t (*FS_OpenFileWrite)(const char* filename);
    size_t  (*FS_Read)(void* buffer, size_t len, fileHandle_t fileHandle);
	size_t (*FS_Write)(const void* buffer, size_t len, fileHandle_t h);
    void    (*FS_CloseFile)(fileHandle_t fileHandle);
    int     (*FS_Seek)(fileHandle_t fileHandle, long offset, int origin);
    long     (*FS_ReadFileEx)(const char* qpath, void** buffer, qboolean quiet);
	void	(*FS_CanonicalFilename)(char* filename);

    void        (*CM_BoxTrace)(trace_t* results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, int model, int brushMask, int cylinder);
    int         (*CM_TerrainSquareType)(int terrainPatch, int i, int j);
    char*       (*CM_EntityString)(void);
	const char* (*CM_MapTime)(void);
    int         (*CG_PermanentMark)(vec3_t origin, vec3_t dir, float orientation, float sScale, float tScale, float red, float green, float blue, float alpha, qboolean doLighting, float sCenter, float tCenter, markFragment_t* markFragments, void* polyVerts);
    void        (*CG_ProcessInitCommands)(dtiki_t* tiki, refEntity_t* ent);
    void        (*CG_EndTiki)(dtiki_t* tiki);
    void        (*SetPerformanceCounters)(int totalTris, int totalVerts, int totalTexels, int worldTris, int worldVerts, int characterLights);

    debugline_t** DebugLines;
    int* numDebugLines;
    debugstring_t** DebugStrings;
    int* numDebugStrings;

    orientation_t (*TIKI_OrientationInternal)(dtiki_t *tiki, int entNum, int tagNum, float scale);
    qboolean (*TIKI_IsOnGroundInternal)(dtiki_t *tiki, int entNum, int tagNum, float thresHold);
    void (*TIKI_SetPoseInternal)(
        void *skeletor, const frameInfo_t *frameInfo, const int *boneTag, const vec4_t *boneQuat, float actionWeight
    );
    void (*TIKI_SetPoseChannelsInternal)(
        void *skeletor,
        const frameInfo_t *frameInfo,
        const short int *bodyParts,
        const int *boneTag,
        const vec4_t *boneQuat,
        float actionWeight
    );
    void *(*TIKI_Alloc)(size_t size);
    float (*GetRadiusInternal)(dtiki_t *tiki, int entNum, float scale);
    float (*GetCentroidRadiusInternal)(dtiki_t *tiki, int entNum, float scale, vec3_t centroid);
    void (*GetFrameInternal)(dtiki_t *tiki, int entNum, skelAnimFrame_t *newFrame);

    void (*UI_LoadResource)(const char *name);

    int (*CM_PointLeafnum)(const vec3_t p);
    int (*CM_LeafCluster)(int leafnum);

    void (*TIKI_CalcLodConsts)(lodControl_t *LOD);
    void (*TIKI_CalculateBounds)(dtiki_t *pmdl, float scale, vec3_t mins, vec3_t maxs);
    dtiki_t *(*TIKI_FindTiki)(const char *path);
    dtiki_t *(*TIKI_RegisterTikiFlags)(const char *path, qboolean use);
    void *(*TIKI_GetSkeletor)(dtiki_t *tiki, int entnum);
    skelHeaderGame_t *(*TIKI_GetSkel)(int index);
    int *(*TIKI_GetBoneMapping)(int handle);
    int *(*TIKI_GetMorphMapping)(int handle);
    void (*TIKI_GetSkelAnimFrame)(dtiki_t *tiki, skelBoneCache_t *bones, float *radius, vec3_t *mins, vec3_t *maxes);
    float (*TIKI_GlobalRadius)(dtiki_t *pmdl);
    skelcache_t *(*TIKI_FindSkelByHeader)(skelHeaderGame_t *skelmodel);
    int (*TIKI_GetNumChannels)(dtiki_t *tiki);
    int (*TIKI_GetLocalChannel)(dtiki_t *tiki, int channel);
    int (*TIKI_GetLocalFromGlobal)(dtiki_t *tiki, int channel);
    int (*TIKI_Anim_NumFrames)(dtiki_t *tiki, int animnum);
    float (*TIKI_Anim_FrameTime)(dtiki_t *tiki, int animnum, int framenum);
    struct tikiFrame_s *(*TIKI_Anim_TanFrames)(dtiki_t *tiki, int animnum, int *pNumFrames);
    struct skelAnimHeader_s *(*TIKI_Skan_FindByAnim)(dtiki_t *tiki, int animnum);
    int (*TIKI_SkelBonesFromRecords)(
        dtiki_t      *tiki,
        int           anim,
        int           frame,
        int           oldanim,
        int           oldframe,
        float         framelerp,
        int           crossblend_anim,
        int           crossblend_frame,
        float         crossblend_lerp,
        int           torso_anim,
        int           torso_frame,
        int           torso_oldanim,
        int           torso_oldframe,
        float         torso_framelerp,
        int           torso_crossblend_anim,
        int           torso_crossblend_frame,
        float         torso_crossblend_lerp,
        qboolean      uselegs,
        qboolean      usetorso,
        const int    *bone_tag,
        const vec4_t *bone_quat,
        const vec3_t  localOrigin,
        skelBoneCache_t *bones,
        int           maxBones,
        float        *radius,
        vec3_t       *mins,
        vec3_t       *maxs
    );

    int (*SKEL_GetMorphWeightFrame)(void *skeletor, int index, float time, int *data);
    void (*SKEL_SetExternalMorphControllers)(void *skeletor, const morph_controller_t *controllers, int count);
    int (*SKEL_GetBoneParent)(void *skeletor, int boneIndex);

    void (*NoteBlockedMsec)( int msec );

    // PORT-ONLY (retail had no on-disk cache to evict), appended after the budget
    void (*FS_RemoveHomeData)( const char *qpath );
} refimport_t;


#ifdef USE_RENDERER_DLOPEN
typedef	refexport_t* (QDECL *GetRefAPI_t) (int apiVersion, refimport_t * rimp);
#else
refexport_t*GetRefAPI( int apiVersion, refimport_t *rimp );
#endif

qboolean R_ImageExists(const char* name);

#ifdef __cplusplus
}
#endif

#endif	
