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
#ifndef VK_TR_LOCAL_H
#define VK_TR_LOCAL_H

/*
 * The renderer's data model, mirrored from renderergles3/tr_local.h.
 *
 * Everything here is the API-neutral half of the reference renderer's own local
 * header: the shader/image/tessellation structures carry the same fields with
 * the same meanings, so the parsers that fill them (tr_shader.c, tr_image.c,
 * tr_font.cpp) are the same code in both modules.  What differs is only the
 * substrate the structures describe: `image_t::texnum` is an index into this
 * module's texture table rather than a driver texture name, and the
 * back-end state records the frame's Vulkan command buffer instead of a driver
 * context.  No legacy raster header is included and no gl*() symbol is named anywhere in
 * this module (the reference renderer's raster typedefs come in through its own header, which this
 * module does not have).
 */

#include "../qcommon/q_shared.h"
#include "../qcommon/qfiles.h"
#include "../qcommon/qcommon.h"
#include "../renderercommon/tr_public.h"
#include "../renderercommon/tr_common.h"
#include "../qcommon/q_jobs.h"
#include "tr_hwtex.h"
#include "tr_texcache.h"
#include "vk_local.h"

/* vk_local.h is included above the linkage block: its declarations are shared
   with the module's C and C++ translation units, so they keep the linkage they
   were declared with and are not re-captured by this file's extern "C". */
#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int vkIndex_t;

#define	SMP_FRAMES		2

#define	MAX_SHADERS				16384
#define MAX_STATES_PER_SHADER 32
#define MAX_STATE_NAME 32

#define	MAX_MOD_KNOWN	1024
/* 8192: the retail corpus names more distinct textures than 4096 slots hold, which is both the off
   line permutation census's truncation point and a hard ERR_DROP for a session that loads past it.
   The table is the only cost; the pixels are allocated when an image is actually uploaded. */
#define	MAX_DRAWIMAGES			8192
#define	MAX_SKINS				1024

/* the tessellation index type, and the reference renderer's float->int truncation */
#define	VK_INDEX_TYPE		unsigned int
#define	myftol(x)			((int)(x))
typedef struct skelSurfaceGame_s skelSurfaceGame_t;
typedef struct staticSurface_s staticSurface_t;

// @0x5080), not MOHAA's srfTerrain_t patch machinery.
typedef enum {
    SF_BAD = 0,			
    SF_SKIP = 1,		
    SF_FACE = 2,		
    SF_GRID = 3,		
    SF_TRIANGLES = 4,	
    SF_POLY = 5,		
    SF_MESH = 6,		 
    SF_ANIM = 7,		 
    SF_FLARE = 8,		
    SF_ENTITY = 9,		
    SF_DISPLAY_LIST = 10,	
    SF_TIKI_MESH = 11,		 
    SF_TIKI_SKEL = 12,		
    SF_SWIPE = 13,		
    SF_SPRITE = 14,		
    SF_TERRAIN = 15,	

    SF_NUM_SURFACE_TYPES = SF_TERRAIN + 1,	 

    SF_MAX = 0x7fffffff			
} surfaceType_t;

typedef struct {
    struct mnode_s* cntNode;
    struct msurface_s* skySurfs[32];
    int numSurfs;
    vec3_t offset;
    vec3_t mins;
    vec3_t maxs;
} portalsky_t;

typedef struct {
    vec3_t transformed;
    int index;
} sphere_dlight_t;

typedef enum {
    SPHEREL_POINT = 0,
    SPHEREL_AREA  = 1,
    SPHEREL_SPOT  = 2,
    SPHEREL_SUN   = 3
} spherel_type_t;

#define SPHERE_NUM_BASIS 6

typedef struct {
    vec3_t origin;       
    vec3_t worldOrigin;  
    float radius;        
    struct mnode_s* leaves[4];              // retail backEnd+0x5D8..0x5E4 -- FOUR, not eight
    void(*TessFunction) (unsigned char* dstColors);
    vec3_t pointcolor[SPHERE_NUM_BASIS];    
    vec3_t ambient;                         
    int numLights;                          
} sphereor_t;

typedef struct spherel_s {
    vec3_t origin;
    vec3_t color;
    struct mnode_s* leaf;
    int   needs_trace;
    int   lensflare;
    int   type;
    int   group_num;
    vec3_t targetNormal;
    float dotProduct_hotspot;
    float dotProduct_penumbra;
    float falloff_start_dist;
    float falloff_end_dist;
    float curveOffset;
    float spherical_ambient;
    float brightness;
    int   exclusiveLightNameCount;
    unsigned int exclusiveLightNameHashList[4];
    int   reference_count;
} spherel_t;

typedef struct dlight_s {
	vec3_t	origin;
	vec3_t	color;				
	float	radius;

	dlighttype_t type;
	vec3_t	transformed;		
} dlight_t;


// `key` is only ever compared, never dereferenced; layout is retail's 40 bytes.
typedef struct lensflare_s {
	void			*key;
	vec3_t			origin;
	vec3_t			color;
	int				num;
	float			scale;
	struct shader_s	*replacementShader;
} lensflare_t;

#define	MAX_LENSFLARES	128


typedef struct {
	refEntity_t	e;

	float		axisLength;		

	qboolean	needDlights;	
	float		lodpercentage[2];


	vec3_t		lightDir;		
	vec3_t		ambientLight;	
	int			ambientLightInt;	
	vec3_t		directedLight;
} trRefEntity_t;

typedef struct refSprite_s {
    surfaceType_t surftype;
    int hModel;
    int shaderNum;
    /* pack-time shader + the table generation it is valid in */
    struct shader_s *shader;
    int shaderGen;
    float origin[3];
    float scale;
    float axis[3][3];
    unsigned char shaderRGBA[4];
    int renderfx;
    float shaderTime;
    int fogNum;
} refSprite_t;

typedef struct {
	vec3_t		origin;			
	vec3_t		axis[3];		
	vec3_t		viewOrigin;		
	float		modelMatrix[16];
} orientationr_t;



typedef enum {
	SS_BAD,
	SS_PORTAL,			
	SS_PORTALSKY,
	SS_ENVIRONMENT,		
	SS_OPAQUE,			

	SS_DECAL,			
	SS_SEE_THROUGH,		
	SS_BANNER,

	SS_FOG,

	SS_UNDERWATER,		

	SS_BLEND0,			
	SS_BLEND1,			
	SS_BLEND2,
	SS_BLEND3,

	SS_BLEND6,
	SS_STENCIL_SHADOW,
	SS_ALMOST_NEAREST,	

	SS_NEAREST			
} shaderSort_t;


#define MAX_SHADER_STAGES 8

typedef enum {
	GF_NONE,

	GF_SIN,
	GF_SQUARE,
	GF_TRIANGLE,
	GF_SAWTOOTH, 
	GF_INVERSE_SAWTOOTH, 

	GF_NOISE

} genFunc_t;

typedef enum {
	DEFORM_NONE,
	DEFORM_WAVE,
	DEFORM_HEIGHTWAVE,
	DEFORM_INVERSEHEIGHTWAVE,
	DEFORM_NORMALS,
	DEFORM_BULGE,
	DEFORM_MOVE,
	DEFORM_PROJECTION_SHADOW,
	DEFORM_AUTOSPRITE,
	DEFORM_AUTOSPRITE2,
	DEFORM_TEXT0,
	DEFORM_TEXT1,
	DEFORM_TEXT2,
	DEFORM_TEXT3,
	DEFORM_TEXT4,
	DEFORM_TEXT5,
	DEFORM_TEXT6,
	DEFORM_TEXT7
} deform_t;

typedef enum {
	AGEN_IDENTITY,				
	AGEN_SKIP,					
	AGEN_ENTITY,				
	AGEN_ONE_MINUS_ENTITY,		
	AGEN_VERTEX,				
	AGEN_ONE_MINUS_VERTEX,		
	AGEN_LIGHTING_SPECULAR,		
	AGEN_WAVEFORM,				
	AGEN_PORTAL,				
	AGEN_UNUSED_09,				 
	AGEN_UNUSED_0A,				
	AGEN_UNUSED_0B,				
	AGEN_UNUSED_0C,				
	AGEN_DOT,					
	AGEN_ONE_MINUS_DOT,			
	AGEN_CONSTANT,				
	AGEN_GLOBAL_ALPHA,			
	AGEN_INERT_11,				 
	AGEN_FOG_GLOBAL,			
	AGEN_SKYALPHA,				
	AGEN_ONE_MINUS_SKYALPHA,	
	AGEN_FORCED_ALPHA,			
	AGEN_DOT_VIEW,				
	AGEN_ONE_MINUS_DOT_VIEW,	
	// ---- port-only generators, deliberately ABOVE retail's 0x00..0x17 range ----
	AGEN_NOISE = 0x20,
	AGEN_SCOORD,
	AGEN_TCOORD,
	AGEN_DIST_FADE,
	AGEN_ONE_MINUS_DIST_FADE,
	AGEN_TIKI_DIST_FADE,
	AGEN_ONE_MINUS_TIKI_DIST_FADE,
	AGEN_HEIGHT_FADE,
} alphaGen_t;

typedef enum {
	CGEN_BAD,
	CGEN_IDENTITY_LIGHTING,	
	CGEN_IDENTITY,			
	CGEN_ENTITY,			
	CGEN_ONE_MINUS_ENTITY,	
	CGEN_EXACT_VERTEX,		
	CGEN_VERTEX,			
	CGEN_ONE_MINUS_VERTEX,
	CGEN_WAVEFORM,			
	CGEN_MULTIPLY_BY_WAVEFORM,
	CGEN_LIGHTING_SPHERICAL,
	CGEN_CONSTANT,
	CGEN_NOISE,
	CGEN_GLOBAL_COLOR,
	CGEN_STATIC,
	CGEN_SCOORD,
	CGEN_TCOORD,
	CGEN_DOT,
	CGEN_ONE_MINUS_DOT,
	CGEN_DEFAULT,		 
	CGEN_FOG,			 
	CGEN_FOG_BLEND		 
} colorGen_t;

typedef enum {
	TCGEN_BAD,
	TCGEN_IDENTITY,			
	TCGEN_LIGHTMAP,
	TCGEN_TEXTURE,
	TCGEN_ENVIRONMENT_MAPPED,
	TCGEN_VECTOR,			
	TCGEN_ENVIRONMENT_MAPPED2,
	TCGEN_ANGLE_BASED_ENVIRONMENT,
	TCGEN_SUN_REFLECTION,
	TCGEN_FOG
} texCoordGen_t;

typedef enum {
	ACFF_NONE,
	ACFF_MODULATE_RGB,
	ACFF_MODULATE_RGBA,
	ACFF_MODULATE_ALPHA
} acff_t;

typedef struct {
	genFunc_t	func;

	float base;
	float amplitude;
	float phase;
	float frequency;

	int wind;
} waveForm_t;

#define TR_MAX_TEXMODS 4

typedef enum {
    TMOD_NONE,
    TMOD_TRANSFORM,
    TMOD_TURBULENT,
    TMOD_SCROLL,
    TMOD_SCALE,
    TMOD_STRETCH,
    TMOD_ROTATE,
    TMOD_ENTITY_TRANSLATE,
    TMOD_NOISE,
    TMOD_OFFSET,
    TMOD_PARALLAX,
    TMOD_MACRO,
    TMOD_WAVETRANS,
    TMOD_WAVETRANT,
    TMOD_BULGETRANS
} texMod_t;

#define	MAX_SHADER_DEFORMS	3
typedef struct {
	deform_t	deformation;			

	vec3_t		moveVector;
	waveForm_t	deformationWave;
	float		deformationSpread;

	float		bulgeWidth;
	float		bulgeHeight;
	float		bulgeSpeed;
} deformStage_t;


typedef struct {
	texMod_t		type;

	waveForm_t		wave;

	float			matrix[2][2];		
	float			translate[2];		

	float			scale[2];			

	float			rate[2];

	float			scroll[2];			

	float			rotateSpeed;
    float			rotateStart;
    float			rotateCoef;

} texModInfo_t;


#define	MAX_IMAGE_ANIMATIONS	64
#define BUNDLE_ANIMATE_ONCE		1

typedef struct {
	image_t			*image[MAX_IMAGE_ANIMATIONS];
	int				numImageAnimations;
	float			imageAnimationSpeed;
	float			imageAnimationPhase;

	texCoordGen_t	tcGen;
	vec3_t			tcGenVectors[2];

	int				numTexMods;
	texModInfo_t	*texMods;

	int				videoMapHandle;
	qboolean		isLightmap;
	qboolean		vertexLightmap;
	qboolean		isVideoMap;
	int				flags;
	/* the image stamp this bundle resolved at parse; another stamp under the same pointer names the recycled slot */
	int				imgStamp;
	/* the same stamp folded over every frame the parse resolved, so an anim frame drawn from its own list is not read as a recycle */
	int				imgAnimStamp;
} textureBundle_t;

#define NUM_TEXTURE_BUNDLES 2

typedef struct {
	qboolean		active;

	// Consequence, reproduced here deliberately: retail always takes the RAMP arm,
	qboolean		fogDensityArm;

	qboolean		hasNormalMap;
	
	textureBundle_t	bundle[NUM_TEXTURE_BUNDLES];
	image_t			*normalMap;
	int				multitextureEnv;		

	waveForm_t		rgbWave;
	colorGen_t		rgbGen;

	waveForm_t		alphaWave;
    alphaGen_t		alphaGen;

	acff_t			acff;

    unsigned		stateBits;					


	qboolean		noMipMaps;
    qboolean		noPicMip;
    qboolean		force32bit;

	float			alphaMin;
	float			alphaMax;
	vec3_t			specOrigin;

    byte			colorConst[4];			
	byte			alphaConst;
	byte			alphaConstMin;
} shaderStage_t;

struct shaderCommands_s;

#define LIGHTMAP_2D			-4		
#define LIGHTMAP_BY_VERTEX	-3		
#define LIGHTMAP_WHITEIMAGE	-2
#define	LIGHTMAP_NONE		-1

#define	LIGHTMAP_BASE_MASK	(DYNAMIC_LIGHTMAP_BIT - 1)

typedef enum {
	CT_FRONT_SIDED,
	CT_BACK_SIDED,
	CT_TWO_SIDED
} cullType_t;

typedef enum {
	FP_NONE,		
	FP_EQUAL,		
	FP_LE			
} fogPass_t;

typedef struct {
	float		cloudHeight;
	image_t		*outerbox[6], *innerbox[6];
} skyParms_t;

typedef enum {
	SPRITE_PARALLEL,
	SPRITE_PARALLEL_ORIENTED,
	SPRITE_ORIENTED,
	SPRITE_PARALLEL_UPRIGHT
} spriteType_t;

typedef struct {
  spriteType_t type;
  float scale;
} spriteParms_t;

typedef struct {
	vec3_t	color;
	float	depthForOpaque;
	waveForm_t	wave;
} fogParms_t;


typedef struct shader_s {
	char		name[MAX_QPATH];		
	int			lightmapIndex;			

	int			index;					
	int			sortedIndex;			

	float		sort;					

	qboolean	defaultShader;			

	qboolean	explicitlyDefined;		

	int			surfaceFlags;			
	int			contentFlags;

	qboolean	entityMergable;			

	qboolean	isSky;
	skyParms_t	sky;
	spriteParms_t sprite;
	qboolean	isPortalSky;
	float subdivisions;
	float fDistRange;
	float fDistNear;
	fogParms_t	fogParms;

	float		portalRange;			

	cullType_t	cullType;				
	qboolean	polygonOffset;			
	qboolean	noMipMaps;				
	qboolean	noPicMip;				

	fogPass_t	fogPass;				

	qboolean	fogonly;

	qboolean	needsNormal;			
	qboolean	needsST1;
	qboolean	needsST2;
	qboolean	needsColor;

	int			numDeforms;
	deformStage_t	deforms[MAX_SHADER_DEFORMS];

	int			numUnfoggedPasses;
	shaderStage_t	* unfoggedStages[MAX_SHADER_STAGES];

	int			numFogPasses;
	shaderStage_t	* fogStages[MAX_SHADER_STAGES];
	int			numGlobalFogPasses;
	shaderStage_t	* globalFogStages[MAX_SHADER_STAGES];


	int			needsLSpherical;		 
	int			usesGlobalFogIterator;
	int			fogFlags;
	// Retail's shader+0x21C bit 0.  It is a general "do not merge / start a fresh
	int			noMerge;				 
	void		(*optimalStageIteratorFunc)( void );	 
	struct shader_s *remappedShader;		 
	char		forcedAlphaShader[64];
	int			forcedAlphaKey;			 
	struct	shader_s	*next;			 

	// ---- port-only fields: retail EF2's shader_t has no counterpart ----------
	int			flags;

	// in the port-only tail rather than at retail's byte offset because this port is
	qboolean	detailShader;

  float clampTime;                                  
  float timeOffset;                                 

  int numStates;                                    
  struct shader_s *currentShader;                   
  struct shader_s *parentShader;                    
  int currentState;                                 
  long expireTime;                                  

  int shaderStates[MAX_STATES_PER_SHADER];          
} shader_t;

#define W31_SHADER_OFF(member) ((int)__builtin_offsetof(shader_t, member))
#define W31_STAGE_OFF(member)  ((int)__builtin_offsetof(shaderStage_t, member))
#if defined(__cplusplus)
#define W31_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define W31_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

W31_STATIC_ASSERT(sizeof(((shader_t *)0)->forcedAlphaShader) == 64,
	"retail shader_t::forcedAlphaShader is char[64] (R_FindShader strlen bound @0x3DD8C)");

W31_STATIC_ASSERT(W31_SHADER_OFF(usesGlobalFogIterator) == W31_SHADER_OFF(needsLSpherical) + 4,
	"retail +0x210 needsLSpherical then +0x214 usesGlobalFogIterator");
W31_STATIC_ASSERT(W31_SHADER_OFF(fogFlags) == W31_SHADER_OFF(usesGlobalFogIterator) + 4,
	"retail +0x214 usesGlobalFogIterator then +0x218 fogFlags");
W31_STATIC_ASSERT(W31_SHADER_OFF(noMerge) == W31_SHADER_OFF(fogFlags) + 4,
	"retail +0x218 fogFlags then +0x21C noMerge");
W31_STATIC_ASSERT(W31_SHADER_OFF(optimalStageIteratorFunc) == W31_SHADER_OFF(needsLSpherical) + 0x10,
	"retail +0x220 optimalStageIteratorFunc (0x220 - 0x210 == 0x10)");

W31_STATIC_ASSERT(W31_SHADER_OFF(remappedShader) > W31_SHADER_OFF(optimalStageIteratorFunc),
	"retail order: +0x220 iterator then +0x224 remappedShader");
W31_STATIC_ASSERT(W31_SHADER_OFF(forcedAlphaShader) > W31_SHADER_OFF(remappedShader),
	"retail order: +0x224 remappedShader then +0x228 forcedAlphaShader");
W31_STATIC_ASSERT(W31_SHADER_OFF(forcedAlphaKey) >= W31_SHADER_OFF(forcedAlphaShader) + 64,
	"retail order: +0x268 forcedAlphaKey follows the 64-byte forcedAlphaShader");
W31_STATIC_ASSERT(W31_SHADER_OFF(next) >= W31_SHADER_OFF(forcedAlphaKey) + 4,
	"retail order: +0x26C next follows +0x268 forcedAlphaKey");
W31_STATIC_ASSERT(W31_SHADER_OFF(flags) > W31_SHADER_OFF(next),
	"port-only shader_t::flags stays after retail's tail");

W31_STATIC_ASSERT(W31_SHADER_OFF(needsLSpherical) == 896, "port shader_t tail base moved");
W31_STATIC_ASSERT(W31_SHADER_OFF(usesGlobalFogIterator) == 900, "port usesGlobalFogIterator moved");
W31_STATIC_ASSERT(W31_SHADER_OFF(fogFlags) == 904, "port fogFlags moved");
W31_STATIC_ASSERT(W31_SHADER_OFF(noMerge) == 908, "port noMerge moved");
W31_STATIC_ASSERT(W31_SHADER_OFF(optimalStageIteratorFunc) == 912, "port optimalStageIteratorFunc moved");
W31_STATIC_ASSERT(W31_SHADER_OFF(remappedShader) == 920, "port remappedShader moved");
W31_STATIC_ASSERT(W31_SHADER_OFF(forcedAlphaShader) == 928, "port forcedAlphaShader moved");
W31_STATIC_ASSERT(W31_SHADER_OFF(forcedAlphaKey) == 992, "port forcedAlphaKey moved");
W31_STATIC_ASSERT(W31_SHADER_OFF(next) == 1000, "port shader_t::next moved");

typedef struct shaderState_s {
  char shaderName[MAX_QPATH];     
  char name[MAX_STATE_NAME];      
  char stateShader[MAX_QPATH];    
  int cycleTime;                  
  shader_t *shader;
} shaderState_t;


typedef struct {
	int			x, y, width, height;
	float		fov_x, fov_y;
	vec3_t		vieworg;
	vec3_t		viewaxis[3];		

	stereoFrame_t	stereoFrame;

	int			time;				
	int			rdflags;			

	byte		areamask[MAX_MAP_AREA_BYTES];
	qboolean	areamaskModified;	

	float		floatTime;			

	char		text[MAX_RENDER_STRINGS][MAX_RENDER_STRING_LENGTH];

	int			num_entities;
	trRefEntity_t	*entities;

    int			num_sprites;
    refSprite_t	*sprites;

	int			num_dlights;
	struct dlight_s	*dlights;

	int			num_lensflares;
	struct lensflare_s	*lensflares;

	int			numPolys;
	struct srfPoly_s	*polys;

	int			numDrawSurfs;
	struct drawSurf_s	*drawSurfs;

	int			numSpriteSurfs;
	struct drawSurf_s	*spriteSurfs;

    int			numStaticModels;
    struct cStaticModelUnpacked_s *staticModels;

    int			numStaticModelData;
    unsigned char		*staticModelData;
    qboolean sky_portal;
    float sky_alpha;
    vec3_t sky_origin;
    vec3_t sky_axis[3];

    /* the light-group set is client-written and walk-read, so it rides the frame */
    byte    lightGroupIntensity[MAX_LIGHTING_GROUPS];
    byte    lightGroupDefaultIntensity[MAX_LIGHTING_GROUPS];
    int     lightGroupFrame[MAX_LIGHTING_GROUPS];
} trRefdef_t;



typedef struct {
	char		name[MAX_QPATH];
	shader_t	*shader;
} skinSurface_t;

typedef struct skin_s {
	char		name[MAX_QPATH];		
	int			numSurfaces;
	skinSurface_t	*surfaces[MD3_MAX_SURFACES];
} skin_t;


typedef struct {
	int			originalBrushNumber;
	vec3_t		bounds[2];

	unsigned	colorInt;				
	float		tcScale;				
	fogParms_t	parms;

	qboolean	hasSurface;
	float		surface[4];
} fog_t;

typedef struct depthfog_s {
	float len;
	float oolen;
	int enabled;
	int extrafrustums;
} depthfog_t;

#define FARPLANE_CULL_NONE			0
#define FARPLANE_CULL_STANDARD		1
#define FARPLANE_CULL_PORTALSKY		2

typedef struct {
	orientationr_t	ori;
	orientationr_t	world;
	vec3_t		pvsOrigin;			
	qboolean	isPortal;			
	qboolean	isMirror;			
    qboolean	isPortalSky;		
	int			frameSceneNum;		
	int			frameCount;			
	cplane_t	portalPlane;		
	int			viewportX, viewportY, viewportWidth, viewportHeight;
	float		fovX, fovY;
	float		projectionMatrix[16];
	cplane_t	frustum[5];
	vec3_t		visBounds[2];
	float		zFar;
	depthfog_t	fog;
	int			fogColorInt;
    float		farplane_distance;
    float		farplane_bias; 
	float		farplane_color[3];
    int			farplane_cull;
    qboolean	farplane_fog;

    float		worldSurfaceFade;

    // assigns a fresh viewParms every view, so the latch is per-view exactly as retail's
    int			terrainGlobalRun;
    float		terrainGlobalMinZ;
} viewParms_t;



typedef struct drawSurf_s {
	unsigned			sort;
	surfaceType_t		*surface;
	/* pack-time shader + generation; the index in `sort' is mutable */
	struct shader_s		*shader;
	int					shaderGen;
} drawSurf_t;

#define	MAX_FACE_POINTS		64

#define	MAX_PATCH_SIZE		32			
#define	MAX_GRID_SIZE		65			

typedef struct srfPoly_s {
	surfaceType_t	surfaceType;
	qhandle_t		hShader;
	int				fogNum;
	int				numVerts;
	polyVert_t		*verts;
	int				renderfx;
} srfPoly_t;

typedef struct srfDisplayList_s {
	surfaceType_t	surfaceType;
	int				listNum;
} srfDisplayList_t;


typedef struct srfFlare_s {
	surfaceType_t	surfaceType;
	vec3_t			origin;
	vec3_t			normal;
	vec3_t			color;
} srfFlare_t;

typedef struct srfGridMesh_s {
	surfaceType_t	surfaceType;			

	int				dlightBits[SMP_FRAMES];	 

	vec3_t			meshBounds[2];			

	vec3_t			localOrigin;			
	float			meshRadius;				
	vec3_t			lodOrigin;				
	float			lodRadius;				
	int				lodFixed;				

	int				baseLightingSurface;	

	int				width, height;			
	float			*widthLodError;			
	float			*heightLodError;		
	drawVert_t		verts[1];				
} srfGridMesh_t;




#define	VERTEXSIZE	8
typedef struct {
	surfaceType_t	surfaceType;	
	cplane_t	plane;			

	int			dlightBits[SMP_FRAMES];	

	int			baseLightingSurface;	
	byte		*color;					

	int			numPoints;		
	int			numIndices;		
	int			ofsIndices;		
	float		points[1][VERTEXSIZE];	
} srfSurfaceFace_t;



typedef struct {
	surfaceType_t	surfaceType;	

	int				dlightBits[SMP_FRAMES];	

	vec3_t			bounds[2];		
	vec3_t			localOrigin;	
	float			radius;			

	float			shaderTime;		
	int				baseLightingSurface;	
	byte			*color;			

	int				numIndexes;		
	int				*indexes;		

	int				numVerts;		
	drawVert_t		*verts;			
} srfTriangles_t;


typedef union varnodeUnpacked_u {
    float fVariance;
    int flags;
} varnodeUnpacked_t;

typedef unsigned short terraInt;


typedef struct srfTerrain_s {
    surfaceType_t surfaceType;
    terraInt iVertHead;
    terraInt iTriHead;
    terraInt iTriTail;
    terraInt iMergeHead;
    int nVerts;
    int nTris;
    int lmapSize;
    int dlightBits[2];
    float lmapStep;
    int dlightMap[2];
    byte* lmData;
    float lmapX;
    float lmapY;
} srfTerrain_t;

typedef struct cTerraPatchUnpacked_s {
    srfTerrain_t drawinfo;
    int viewCount;
    int visCountCheck;
    int visCountDraw;
    int frameCount;
    unsigned int uiDistRecalc;
    float s;
    float t;
    vec2_t texCoord[2][2];
    float x0;
    float y0;
    float z0;
    float zmax;
    shader_t* shader;
    short int iNorth;
    short int iEast;
    short int iSouth;
    short int iWest;
    struct cTerraPatchUnpacked_s* pNextActive;
    varnodeUnpacked_t varTree[2][63];
    unsigned char heightmap[81];
    byte flags;
    byte byDirty;
} cTerraPatchUnpacked_t;

typedef struct srfStaticModel_s {
    surfaceType_t surfaceType;
    struct cStaticModelUnpacked_s* parent;
} srfStaticModel_t;

typedef struct cStaticModelUnpacked_s {
    qboolean useSpecialLighting;
    qboolean bStaticLightingCalculated;
    qboolean bRendered;
    char model[128];
    vec3_t origin;
    vec3_t angles;
    vec3_t axis[3];
    float scale;
    int firstVertexData;
    int numVertexData;
    int visCount;
    dtiki_t* tiki;
    sphere_dlight_t dlights[MAX_DLIGHTS];
    int numdlights;
    float radius;
    float cull_radius;
    int iStaticLighting;
    float lodpercentage[2];
} cStaticModelUnpacked_t;

extern	void (*rb_surfaceTable[SF_NUM_SURFACE_TYPES])(void *);




#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2

typedef struct msurface_s {
	int					viewCount;		
	struct shader_s		*shader;		
	int					fogIndex;		

	surfaceType_t		*data;			
} msurface_t;




#define	CONTENTS_NODE		-1
typedef struct mnode_s {
	int			contents;		
	int			visframe;		
	vec3_t		mins, maxs;		
	struct mnode_s	*parent;

	cplane_t	*plane;
	struct mnode_s	*children[2];	

	int			cluster;
    int			area;
    spherel_t**	lights;
    int			numlights;

	msurface_t	**firstmarksurface;
    int			nummarksurfaces;

    int			firstTerraPatch;
    int			numTerraPatches;
    int			firstStaticModel;
    int			numStaticModels;
    void**		pFirstMarkFragment;
    int			iNumMarkFragment;
} mnode_t;

typedef struct {
	vec3_t		bounds[2];		
	msurface_t	*firstSurface;
	int			numSurfaces;
    void** pFirstMarkFragment;
    int iNumMarkFragment;
    int frameCount;
    qboolean hasLightmap;
} bmodel_t;

typedef struct {
    float width;
    float height;
    float origin_x;
    float origin_y;
    float scale;
    shader_t* shader;
} sprite_t;

typedef struct {
	char		name[MAX_QPATH];		
	char		baseName[MAX_QPATH];	

	int			dataSize;

	int			numShaders;
	dshader_t	*shaders;

	int			numBmodels;
	bmodel_t	*bmodels;

	int			numplanes;
	cplane_t	*planes;

	int			numfogs;
	fog_t		*fogs;

	int			numnodes;		
	int			numDecisionNodes;
	mnode_t		*nodes;

	int			numsurfaces;
	msurface_t	*surfaces;

	int			nummarksurfaces;
	msurface_t	**marksurfaces;

	byte		lightGroupIntensity[MAX_LIGHTING_GROUPS];

	byte		*baseLightmapData;		
	byte		*contLightmapData;		
	byte		*baseLightmapImage;		
	int			*baseLightmapFrame;		
	byte		*baseLightingVerts;		
	byte		*baseLightingVertsWork;	
	byte		*contLightingVerts;		
	int			numBaseLightingSurfs;	
	baseLightingSurf_t	*baseLightingSurfs;		
	int			numLightingSurfs;		
	lightingSurf_t		*lightingSurfs;			
	int			numLightingVertSurfs;	
	lightingVertSurf_t	*lightingVertSurfs;		

	int			lightGroupFrame[MAX_LIGHTING_GROUPS];			

	byte		lightGroupDefaultIntensity[MAX_LIGHTING_GROUPS];	
	int			lightGroupDefaultFrame[MAX_LIGHTING_GROUPS];	

	vec3_t		windDirection;
	float		windIntensity;
	float		timeScale;

	int						numStaticLodModels;
	staticLodModel_t		*staticLodModels;

    int numTerraPatches;
    cTerraPatchUnpacked_t* terraPatches;
    cTerraPatchUnpacked_t* activeTerraPatches;

    int numVisTerraPatches;
    cTerraPatchUnpacked_t** visTerraPatches;

    int numStaticModelData;
    byte* staticModelData;

    int numStaticModels;
    cStaticModelUnpacked_t* staticModels;

    int numVisStaticModels;
    cStaticModelUnpacked_t** visStaticModels;

	int			numClusters;
	int			clusterBytes;
	const byte	*vis;			

	byte		*novis;			
	byte		*lighting;
} world_t;


typedef enum {
	MOD_BAD,
	MOD_BRUSH,
	MOD_TIKI,
	MOD_SPRITE
} modtype_t;

typedef struct model_s {
	char		name[MAX_QPATH];
	modtype_t	type;
	int			index;				

	float		fadeDistMod;

	int			r_sequence;

	/* generation the sprite's raw shader pointer was taken at */
	int			spriteShaderGen;

    qboolean serveronly;
    union {
        bmodel_t* bmodel;
        dtiki_t* tiki;
        sprite_t* sprite;
    } d;
} model_t;


#define	MAX_MOD_KNOWN	1024

void		R_ModelInit (void);
void		TIKI_FlushAll (void);
model_t		*R_GetModelByHandle( qhandle_t hModel );
int			R_LerpTag( orientation_t *tag, qhandle_t handle, int startFrame, int endFrame, 
					 float frac, const char *tagName );

void		R_Modellist_f (void);

extern	refimport_t		ri;

/* 8192: the retail corpus names more distinct textures than 4096 slots hold, which is both the off
   line permutation census's truncation point and a hard ERR_DROP for a session that loads past it.
   The table is the only cost; the pixels are allocated when an image is actually uploaded. */
#define	MAX_DRAWIMAGES			8192
#define	MAX_LIGHTMAPS			256
#define	MAX_SKINS				1024
#define MAX_MAP_SPHERE_LIGHTS_EF2	2532

#define	MAX_DRAWSURFS			0x10000
#define	MAX_SPRITESURFS			0x8000
#define	DRAWSURF_MASK			(MAX_DRAWSURFS-1)

#define MAX_SPRITE_DIST				16384.0f
#define MAX_SPRITE_DIST_SQUARED		(MAX_SPRITE_DIST * MAX_SPRITE_DIST)

#define	QSORT_SHADERNUM_SHIFT	21
#define	QSORT_SHADERNUM_MASK	( ( 1 << ( 32 - QSORT_SHADERNUM_SHIFT ) ) - 1 )
#define	QSORT_ENTITYNUM_SHIFT	11
#define	QSORT_ENTITYNUM_MASK	0x3FF
#define	QSORT_FOGNUM_SHIFT		3
#define	QSORT_FOGNUM_MASK		31
#define	QSORT_DLIGHTMAP_MASK	3

extern	int			vk_filter_min, vk_filter_max;

typedef struct {
	int		c_sphere_cull_patch_in, c_sphere_cull_patch_clip, c_sphere_cull_patch_out;
	int		c_box_cull_patch_in, c_box_cull_patch_clip, c_box_cull_patch_out;
	int		c_sphere_cull_md3_in, c_sphere_cull_md3_clip, c_sphere_cull_md3_out;
	int		c_box_cull_md3_in, c_box_cull_md3_clip, c_box_cull_md3_out;

	int		c_leafs;
	int		c_dlightSurfaces;
	int		c_dlightSurfacesCulled;
	int		c_dlightMaps;
	int		c_dlightTexels;
} frontEndCounters_t;

#define	FOG_TABLE_SIZE		256
#define FUNCTABLE_SIZE		2048
#define FUNCTABLE_SIZE2		11
#define FUNCTABLE_MASK		(FUNCTABLE_SIZE-1)


typedef struct {
	int			currenttextures[2];
	int			currenttmu;
	unsigned int	currentProgram;
	qboolean	finishCalled;
	int			texEnv[2];
	int			faceCulling;
	int			cntTexEnvExt;
	int			cntnvblendmode;
	long unsigned int vkStateBits;
	long unsigned int externalSetState;
	vec4_t fFogColor;
} vkstate_t;


typedef struct {
	int		c_surfaces, c_shaders, c_vertexes, c_indexes, c_indexBytes, c_totalIndexes, c_characterlights;
	float	c_overDraw;
	
	int		c_dlightVertexes;
	int		c_dlightIndexes;

	int		c_flareAdds;
	int		c_flareTests;
	int		c_flareRenders;

	int		msec;			
} backEndCounters_t;

typedef struct {
	int			smpFrame;
	trRefdef_t	refdef;
	viewParms_t	viewParms;
	orientationr_t	ori;
	backEndCounters_t	pc;
	qboolean	isHyperspace;
	trRefEntity_t	*currentEntity;
    qboolean skyRenderedThisView;	
    sphereor_t sphere;
    sphereor_t* currentSphere;
    cStaticModelUnpacked_t* currentStaticModel;
    qboolean bExactDLights;
    int numExactDLights;
    sphere_dlight_t exactDLights[MAX_DLIGHTS];
    int dsStreamVert;
	qboolean in2D;	
	byte color2D[4];
	qboolean vertexes2D;		
	trRefEntity_t entity2D;	
    int backEndMsec;
    float shaderStartTime;
    int			currentEntityNum;
    int			currentSpriteNum;
    int			shiftedEntityNum;
    int			refdefTime;
    float		refdefFloatTime;
} backEndState_t;

typedef struct {
	qboolean				registered;		

	qboolean				registrating;

	/* bumped by every R_SetupShaders; cacheable models hold raw shader_t*
	   pointers and compare against this to know their pointer pre-dates
	   the table they are about to be drawn through */
	int						shaderGen;

	int						visCount;
	int						frameCount;		
	int						sceneCount;		
	int						viewCount;		

	int						smpFrame;		

	int						frameSceneNum;	

	qboolean				worldMapLoaded;
	world_t					*world;

	const byte				*externalVisData;	

	image_t					*defaultImage;
	image_t					*scratchImage;
	image_t					*fogImage;
	image_t					*dlightImage;	
	image_t					*flareImage;
	image_t					*whiteImage;			
	image_t					*identityLightImage;	

	shader_t				*defaultShader;
	shader_t				*shadowShader;
	shader_t				*projectionShadowShader;

	shader_t				*flareShader;
	shader_t				*sunShader;

	int						numLightmaps;
	int						numBaseLightmaps;
	int						numContLightmaps;
	image_t					*lightmaps[MAX_LIGHTMAPS];
	image_t					*baseLightmaps[MAX_LIGHTMAPS];

	trRefEntity_t			*currentEntity;
	trRefEntity_t			worldEntity;		
	int						currentEntityNum;
	int						currentSpriteNum;
	int						shiftedEntityNum;	
	int                     shiftedIsStatic;
	model_t					*currentModel;

	viewParms_t				viewParms;

	float					identityLight;		
	int						identityLightByte;	
	int						overbrightBits;		
    int						overbrightShift;
    float					overbrightMult;
    int						needsLightScale;

	orientationr_t			ori;				

	portalsky_t				portalsky;
	qboolean				skyRendered;
	qboolean				portalRendered;
	trRefdef_t				refdef;

	int						viewCluster;

	vec3_t					sunLight;			
	vec3_t					sunDirection;

	frontEndCounters_t		pc;
	int						frontEndMsec;		

	model_t					models[MAX_MOD_KNOWN];
	int						numModels;

	int						numImages;
	image_t					images[MAX_DRAWIMAGES];

	int						numShaders;
	shader_t				*shaders[MAX_SHADERS];
	shader_t				*sortedShaders[MAX_SHADERS];

	int						numSkins;
	skin_t					*skins[MAX_SKINS];

	float					sinTable[FUNCTABLE_SIZE];
	float					squareTable[FUNCTABLE_SIZE];
	float					triangleTable[FUNCTABLE_SIZE];
	float					sawToothTable[FUNCTABLE_SIZE];
	float					inverseSawToothTable[FUNCTABLE_SIZE];

    spherel_t sLights[MAX_MAP_SPHERE_LIGHTS_EF2 + MAX_DLIGHTS];
    int numSLights;
    int rendererhandle;
    qboolean shadersParsed;
    int frame_skel_index;
    int skel_index[1024];
    fontheader_t* pFontDebugStrings;

    shader_t	**currentShader;
} trGlobals_t;

extern backEndState_t	backEnd;
extern trGlobals_t	tr;
extern glconfig_t	vkConfig;		

extern qboolean  textureFilterAnisotropic;
extern int       maxAnisotropy;

extern int       smp_texTranscode;		
extern qboolean  smp_hasAstcLdr;
extern qboolean  smp_hasAstcSliced3d;
extern qboolean  smp_hasEtc2Core;
extern float     displayAspect;
extern qboolean  haveClampToEdge;

extern vkstate_t	vkState;		
extern int r_sequencenumber;



extern cvar_t	*r_ignore;				
extern cvar_t	*r_verbose;				
extern cvar_t	*r_ignoreFastPath;		

extern cvar_t	*r_znear;				

extern cvar_t	*r_stencilbits;			
extern cvar_t	*r_depthbits;			
extern cvar_t	*r_colorbits;			
extern cvar_t	*r_stereo;				
extern cvar_t	*r_detailtextures;
extern cvar_t	*r_detailSurfaces;
extern cvar_t	*r_allowGeometryDeforms;
extern cvar_t	*r_texturebits;			

extern cvar_t	*r_measureOverdraw;		
extern cvar_t	*r_depthFuncAlways;		/* diagnostic: the depth comparator is VK_ALWAYS */
void		VK_DepthState( qboolean test, qboolean write );
void		VK_DepthStateReset( void );
void		VK_DepthFuncApply( int func );
#define VK_PLATFORM_FALLBACK_MODE 3

void		VK_Shade_Viewport3D( int x, int y, int w, int h );
void		VK_ProgramsInit( void );
void		VK_ProgramsShutdown( void );
qboolean	VK_DepthGate2D( void );
void		VK_DepthGate2DTake( void );
void		VK_DepthGate3DOpen( void );
void		VK_StencilState( qboolean test, int func, int ref, unsigned int mask );
void		VK_StencilStateReset( void );

extern cvar_t	*r_lodscale;

extern cvar_t	*r_lodbias;
extern cvar_t	*r_lodScaleDistance;

extern cvar_t	*r_primitives;			

extern cvar_t	*r_inGameVideo;				
extern cvar_t	*r_fastsky;				
extern cvar_t	*r_fastdlights;
extern cvar_t	*r_drawSun;				
extern cvar_t	*r_dlightBacks;			
extern cvar_t	*r_dynamiclightmaps;	
extern cvar_t	*r_dynamiclight;		 

void RE_SetDynamicLights( const byte *defaultIntensities, const byte *intensities );
void R_DetachLightGroups( void );
void R_InitLightGroups( void );
void R_LightConfigRow( void );
void RE_SetWindDirection( vec3_t direction );
void RE_SetWindIntensity( float intensity );
void RE_SetTimeScale( float timeScale );

extern	cvar_t	*r_norefresh;			
extern	cvar_t	*r_drawentities;		
extern	cvar_t	*r_drawterrain;
extern	cvar_t	*r_drawsprites;
extern	cvar_t	*r_fadeDistMod;			
extern	cvar_t	*r_drawworld;			
extern	cvar_t	*r_speeds;				
extern  cvar_t	*r_detailTextures;		
extern	cvar_t	*r_novis;				
extern	cvar_t	*r_nocull;
extern	cvar_t	*r_facePlaneCull;		
extern	cvar_t	*r_nocurves;
extern	cvar_t	*r_showcluster;

// The default MUST be non-zero: the shipped BSPs contain MST_PATCH surfaces
extern	cvar_t	*r_drawcurves;

extern	cvar_t	*r_drawviewmodel;

extern	cvar_t	*r_terrain_global;
extern	cvar_t	*r_terrain_global_min;

extern cvar_t	*r_mode;				
extern cvar_t	*r_fullscreen;
extern cvar_t	*r_gamma;
extern cvar_t	*r_displayRefresh;		
extern cvar_t	*r_ignorehwgamma;		

extern cvar_t	*r_allowExtensions;				
extern cvar_t	*r_ext_compress_textures;		 
extern cvar_t	*r_ext_gamma_control;
extern cvar_t	*r_ext_texenv_op;
extern cvar_t	*r_ext_multitexture;
extern cvar_t	*r_ext_compiled_vertex_array;
extern cvar_t	*r_ext_texture_env_add;
extern cvar_t	*r_ext_texture_env_combine;
extern cvar_t	*r_draw2d;

extern	cvar_t	*r_nobind;						
extern	cvar_t	*r_singleShader;				
extern	cvar_t	*r_lerpmodels;
extern	cvar_t	*r_roundImagesDown;
extern	cvar_t	*r_colorMipLevels;				
extern	cvar_t	*r_picmip;						
extern	cvar_t	*r_finish;
extern	cvar_t	*r_drawBuffer;
extern  cvar_t  *r_glDriver;
extern	cvar_t	*r_swapInterval;
extern	cvar_t	*r_textureMode;
extern	cvar_t	*r_offsetFactor;
extern	cvar_t	*r_offsetUnits;

extern	cvar_t	*r_fullbright;					
extern	cvar_t	*r_lightmap;					
extern	cvar_t	*r_vertexLight;					

extern	cvar_t	*r_logFile;						
extern	cvar_t	*r_showtris;
extern	cvar_t	*r_worldFlat;
extern	cvar_t	*r_showsky;
extern	cvar_t	*r_drawsky;						
extern	cvar_t	*r_shownormals;					
extern	cvar_t	*r_clear;						

extern	cvar_t	*r_shadows;						
extern	cvar_t	*r_flares;						
extern	cvar_t	*r_flaresMaxDist;				

extern	cvar_t	*r_intensity;

extern	cvar_t	*r_lockpvs;
extern	cvar_t	*r_noportals;
extern	cvar_t	*r_fastentlight;
extern	cvar_t	*r_portalOnly;

extern	cvar_t	*r_subdivisions;
extern	cvar_t	*r_lodCurveError;
extern	cvar_t	*r_skipBackEnd;

extern	cvar_t	*r_ignoreGLErrors;

extern	cvar_t	*r_overBrightBits;
extern	cvar_t	*r_mapOverBrightBits;

extern	cvar_t	*r_debugSurface;

extern	cvar_t	*r_showImages;
// pool 0x239E60+0x9fc).  The port-only `r_showstaticlod' alias is deleted.
extern	cvar_t	*r_showlod;
extern	cvar_t	*r_debugSort;

extern	cvar_t	*r_printShaders;
extern	cvar_t	*r_saveFontData;

extern	cvar_t* r_lodscale;


extern  cvar_t* r_numdebuglines;

extern	cvar_t* r_stipplelines;
extern	cvar_t* r_light_lines;
extern	cvar_t* r_light_sun_line;
extern	cvar_t* r_light_int_scale;
extern	cvar_t* r_light_emphasize;
extern	cvar_t* r_light_emphasizePercent;
extern	cvar_t* r_light_nolight;
extern	cvar_t* r_light_simple;
extern	cvar_t* r_light_simple_dist;
extern	cvar_t* r_light_ambientFudge;
extern	cvar_t* r_light_ambientEmphasize;
extern	cvar_t* r_skyportal;
extern	cvar_t* r_skyportal_origin;
extern	cvar_t* r_farplane;
extern	cvar_t* r_farplane_color;
extern	cvar_t* r_farplane_nocull;
extern	cvar_t* r_farplane_nofog;
extern	cvar_t* r_lightcoronasize;
extern	cvar_t* r_useglfog;
extern	cvar_t* r_debuglines_depthmask;
extern	cvar_t* ter_minMarkRadius;
extern	cvar_t* r_bumpmap;

extern  cvar_t* r_developer;
extern  cvar_t* r_fps;

extern  cvar_t* r_jobs;
extern  cvar_t* r_jobsStats;
void    RB_MtFrameBoundary( void );

extern  cvar_t* r_smp;


void  R_NoiseInit( void );

void R_RenderView( viewParms_t *parms );

qboolean SurfIsOffscreen(const srfSurfaceFace_t* surface, shader_t* shader, int entityNum);

void R_AddMD3Surfaces( trRefEntity_t *e );
void R_AddNullModelSurfaces( trRefEntity_t *e );
void R_AddBeamSurfaces( trRefEntity_t *e );
void R_AddRailSurfaces( trRefEntity_t *e, qboolean isUnderwater );
void R_AddLightningBoltSurfaces( trRefEntity_t *e );

void R_AddPolygonSurfaces( void );

void R_DecomposeSort(unsigned int sort, int* entityNum, shader_t** shader, int* fogNum, int* dlightMap);

void R_AddDrawSurf(surfaceType_t* surface, shader_t* shader, int fogNum, int dlightMap);

/* the scene's surface path, counted for the frame row */
void R_FrontEnd_NoteAdd( void );
const char *R_FrontEnd_ViewRow( int recorded, int entities );

int R_CalcFogNum(const vec3_t worldOrigin, float radius);

int R_SpriteFogNum(const vec3_t origin, float radius);


#define	CULL_IN		0		
#define	CULL_CLIP	1		
#define	CULL_OUT	2		
void R_LocalNormalToWorld (const vec3_t local, vec3_t world);
void R_LocalPointToWorld (const vec3_t local, vec3_t world);
int R_CullLocalBoxOffset(const vec3_t offset, vec3_t bounds[2]);
int R_CullLocalBox (vec3_t bounds[2]);
int R_CullPointAndRadius( vec3_t origin, float radius );
int R_CullLocalPointAndRadius( vec3_t origin, float radius );
int R_DistanceCullLocalPointAndRadius(float fDist, const vec3_t pt, float radius);
int R_DistanceCullPointAndRadius(float fDist, const vec3_t pt, float radius);

void R_RotateForEntity( const trRefEntity_t *ent, const viewParms_t *viewParms, orientationr_t *ori );
void R_RotateForStaticModel(cStaticModelUnpacked_t* SM, const viewParms_t* viewParms, orientationr_t* ori );
void R_RotateForViewer(void);
void R_SetupFrustum(void);

void	VK_SetFogColor(const vec4_t fColor);
void	VK_Bind( image_t *image );
void	VK_BindUnit0( image_t *image );
void	VK_SetDefaultState (void);
void	VK_SelectTexture( int unit );
void	VK_TextureMode( const char *string );
void	VK_CheckErrors( void );
void	VK_State( unsigned long stateVector );
unsigned long	vk_Shade_StateBits( void );
unsigned long RF_ForceAlphaStateBits( unsigned long stateVector );
void	VK_TexEnv( int env );
void	VK_Cull( int cullType );

#define VKS_SRCBLEND_ZERO						0x00000001
#define VKS_SRCBLEND_ONE						0x00000002
#define VKS_SRCBLEND_DST_COLOR					0x00000003
#define VKS_SRCBLEND_ONE_MINUS_DST_COLOR		0x00000004
#define VKS_SRCBLEND_SRC_ALPHA					0x00000005
#define VKS_SRCBLEND_ONE_MINUS_SRC_ALPHA		0x00000006
#define VKS_SRCBLEND_DST_ALPHA					0x00000007
#define VKS_SRCBLEND_ONE_MINUS_DST_ALPHA		0x00000008
#define VKS_SRCBLEND_ALPHA_SATURATE				0x00000009
#define		VKS_SRCBLEND_BITS					0x0000000f

#define VKS_DSTBLEND_ZERO						0x00000010
#define VKS_DSTBLEND_ONE						0x00000020
#define VKS_DSTBLEND_SRC_COLOR					0x00000030
#define VKS_DSTBLEND_ONE_MINUS_SRC_COLOR		0x00000040
#define VKS_DSTBLEND_SRC_ALPHA					0x00000050
#define VKS_DSTBLEND_ONE_MINUS_SRC_ALPHA		0x00000060
#define VKS_DSTBLEND_DST_ALPHA					0x00000070
#define VKS_DSTBLEND_ONE_MINUS_DST_ALPHA		0x00000080
#define		VKS_DSTBLEND_BITS					0x000000f0

#define VKS_DEPTHMASK_TRUE						0x00000100
#define VKS_POLYMODE_LINE						0x00000200
#define VKS_DEPTHTEST_DISABLE					0x00000400
#define VKS_DEPTHFUNC_EQUAL						0x00000800

#define VKS_CLAMP_EDGE							0x00001000
#define VKS_MULTITEXTURE						0x00002000
#define VKS_MULTITEXTURE_ENV					0x00004000
#define VKS_FOG									0x00008000

#define VKS_CUBEMAP								0x00010000

#define VKS_ATEST_GT_0							0x10000000
#define VKS_ATEST_LT_80							0x20000000
#define VKS_ATEST_GE_80							0x40000000
#define VKS_ATEST_BITS							0x70000000

#define VKS_DEFAULT			VKS_DEPTHMASK_TRUE

/* the engine's clip-space arithmetic was written for the OpenGL clip volume (-w <= z <= w) and Vulkan clips against 0 <= z <= w, so a
   vertex at the GL near plane is discarded -- a bubble about 2*zNear wide, the width of a corridor in the Borg maps.  The answer-key Vulkan
   renderer (renderervk/tr_main.c R_SetupProjectionZ) carries the same split, so the projection/ortho rows, the CPU clip tests and the clip->window depth all go through these two helpers. */
static inline float VK_ClipZToDepth( float clipZ, float clipW )
{
	return clipZ / clipW;
}

static inline qboolean VK_ClipZInside( float clipZ, float clipW )
{
	return ( clipZ >= 0.0f && clipZ <= clipW ) ? qtrue : qfalse;
}

void Draw_SetColor(const vec4_t rgba);
void R_ChangeTextColor(int index);
void Draw_StretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);
void Draw_StretchPic2(float x, float y, float w, float h, float s1, float t1, float s2, float t2, float sx, float sy, qhandle_t hShader);
void Draw_TilePic(float x, float y, float w, float h, qhandle_t hShader);
void Draw_TilePicOffset(float x, float y, float w, float h, qhandle_t hShader, int offsetX, int offsetY);
void Draw_TrianglePic(const vec2_t vPoints[3], const vec2_t vTexCoords[3], qhandle_t hShader);
void DrawBox(float x, float y, float w, float h);
void AddBox(float x, float y, float w, float h);
void Set2DWindow(int x, int y, int w, int h, float left, float right, float bottom, float top, float n, float f);

void Set2DInitialShaderTime(float startTime);

void RE_Scissor(int x, int y, int width, int height);
qboolean R_ProbeStatsOn( void );
void SMP_VpProbe( const char *src, const char *why, int x, int y, int w, int h, int clearBits, qboolean setsViewport );
void SMP_ShaderDefaultProbe( const char *kind, const char *requested, const char *source, int a, int b );
void DrawLineLoop(const vec2_t* points, int count, int stipple_factor, int stipple_mask);

/* one body per 2D entry point: immediate, or the same body under the render thread */
void RB_2D_SetColor( const byte *rgba );
void RB_2D_StretchPic( shader_t *shader, float x, float y, float w, float h, float s1, float t1, float s2, float t2 );
void RB_2D_StretchPic2( shader_t *shader, float x, float y, float w, float h, float s1, float t1, float s2, float t2, float sx, float sy );
void RB_2D_TilePic( shader_t *shader, float x, float y, float w, float h, float picw, float pich );
void RB_2D_TilePicOffset( shader_t *shader, float x, float y, float w, float h, float picw, float pich, int offsetX, int offsetY );
void RB_2D_TrianglePic( shader_t *shader, const vec2_t vPoints[3], const vec2_t vTexCoords[3] );
void RB_2D_Box( float x, float y, float w, float h, qboolean blend );
void RB_2D_LineLoop( const vec2_t *points, int count, int stipple_factor, int stipple_mask );
void RB_2D_ShaderTime( float startTime );

/* Drops the recorded window/scissor coalescing state. */
void R_Invalidate2DWindowCache( void );
void RB_2D_InvalidateWindow( void );
qboolean	RB_2D_WindowValid( void );
void RB_2D_WindowState( int *win, int *scn );
/* The widget's placement inside the pass's screen-space 2D window: the UI says
   where the widget is and the renderer adds it to the 2D vertices, the way
   retail's screen-space glVertex2f stream already carries it. */
void Set2DOrigin( float x, float y );
void R_2DQueuePlacement( float *ox, float *oy, qboolean *scnValid, int *scn, byte *color );
/* The recorder's own "a 2D window went out this frame"; see the [VK:2dlost] row. */
qboolean R_2D_WindowSeenThisFrame( void );
void R_2D_BeginFrame( void );
void R_2D_OriginState( float *ox, float *oy );

void RB_2D_Flush( void );
/* The immediate (box and line) run alone, which a window change still closes. */
void RB_2D_FlushImmediate( void );
/* The batch's state spans: a window or scissor change inside a batch is a span
   rather than a flush.  RB_2D_SpanBreak runs before the new state is applied,
   RB_2D_SpanNote after, and a qfalse from either means the caller flushes. */
void RB_2D_SpanReset( void );
int  RB_2D_SpanCount( void );
void RB_2D_SpanGet( int i, int *firstIndex, int *count, float *ortho, int *win, int *scn );
qboolean RB_2D_SpanBreak( void );
qboolean RB_2D_SpanNote( void );
void D2_StringBegin( int *t0 );
void D2_StringEnd( int t0 );
/* Draws the batch and drops the census before a purge frees the names they hold. */
void RB_2D_OwnersPurged( void );
qboolean R_PendingRenderCommands( void );

void RE_StretchRaw(int x, int y, int w, int h, int cols, int rows, int components, const byte* data, qboolean dirty);
void	RE_UploadCinematic (int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty);

void		RE_BeginFrame( stereoFrame_t stereoFrame );
void		RE_BeginRegistration( glconfig_t *glconfig );
void		RE_LoadWorldMap( const char *mapname );
void		RE_PrintBSPFileSizes(void);
int			RE_MapVersion(void);
void		RE_SetWorldVisData( const byte *vis );
qhandle_t	RE_RegisterModel( const char *name );
qhandle_t	RE_ForceRegisterModel( const char *name );
qhandle_t	RE_RegisterSkin( const char *name );
void		RE_Shutdown( qboolean destroyWindow );
void        R_ClearWorld(void);

qboolean	R_GetEntityToken( char *buffer, int size );

model_t		*R_AllocModel( void );

void    	R_Init( void );

image_t		*R_FindImageFileOld(const char* name, qboolean mipmap, qboolean allowPicmip, qboolean force32bit, int textureMode);
image_t		*R_RefreshImageFileOld(const char* name, qboolean mipmap, qboolean allowPicmip, qboolean force32bit, int textureMode);

image_t* R_CreateImageOld(
    const char* name,
    byte* pic,
    int width,
    int height,
    int numMipmaps,
    int iMipmapsAvailable,
    qboolean allowPicmip,
    qboolean force32bit,
    qboolean hasAlpha,
    int vkCompressMode,
    int textureMode
);

qboolean R_ImageExists(const char* name);
int R_CountTextureMemory(void);
qboolean R_LoadRawImage(const char *name, byte **pic, int *width, int *height);
void R_FreeRawImage(byte *pic);

qboolean	R_GetModeInfo( int *width, int *height, float *windowAspect, int mode );

void		R_SetColorMappings( void );
void		R_GammaCorrect( byte *buffer, int bufSize );

void	R_ImageList_f( void );
void	R_SkinList_f( void );
int		R_SumOfTextureMemory( int type, int usedOnly, int estimated );
void	R_TotalTextureMemInfo_f( void );
const void *RB_TakeScreenshotCmd( const void *data );
void	R_ScreenShot_f( void );
void	R_Register( void );
void	R_Screenshot_Request( const char *name, int width, int height );
qboolean	R_Screenshot_Pending( void );
void	R_Screenshot_Flush( unsigned imageIndex );
byte	*RB_ReadPixels( int x, int y, int width, int height, size_t *offset, int *padlen );

void	R_InitImages( void );
void	R_FreeImage(image_t* image);
image_t	*VK_Image_SlotTenant( int handle );
void	VK_Image_SlotTenant_Set( unsigned int handle, image_t *image );
void	R_DeleteTextures( void );
void    R_FreeUnusedImages( void );
int		R_SumOfUsedImages( void );
skin_t	*R_GetSkinByHandle( qhandle_t hSkin );

const void *RB_TakeVideoFrameCmd( const void *data );

qhandle_t		 RE_RegisterShader( const char *name );
qhandle_t		 RE_RegisterShaderNoMip( const char *name );
qhandle_t		RE_RefreshShaderNoMip(const char* name);
qhandle_t RE_RegisterShaderFromImage(const char *name, int lightmapIndex, image_t *image, qboolean mipRawImage);

shader_t* R_FindShader(const char* name, int lightmapIndex, qboolean mipRawImage, qboolean picmip, qboolean wrapx, qboolean wrapy);
shader_t* R_FindShaderForceAlpha(const char* name);
shader_t* R_FindShaderForceAlphaLM(const char* name, int lightmapIndex);
shader_t	*R_GetShaderByHandle( qhandle_t hShader );
shader_t	*R_GetShaderByState( int index, long *cycleTime );
void R_StartupShaders(void);
void R_ShutdownShaders(void);
void R_SetupShaders(void);
shader_t *R_FindShaderByName( const char *name );
void		R_ShaderList_f( void );
void    R_RemapShader(const char *oldShader, const char *newShader);


void		GLimp_Init( qboolean fixedFunction );
void		GLimp_Shutdown( void );
void		GLimp_EndFrame( void );

qboolean	GLimp_SpawnRenderThread( void (*function)( void ) );
void		*GLimp_RendererSleep( void );
void		GLimp_FrontEndSleep( void );
void		GLimp_WakeRenderer( void *data );
void		GLimp_ShutdownRenderThread( void );
qboolean	GLimp_IsRenderThread( void );

void		VK_ClaimContextForResource( void );	/* parks the render thread */
int			GLimp_HandshakeViolations( void );
int			GLimp_DeviceCoreCount( void );

void		VKimp_LogComment( char *comment );

void		GLimp_SetGamma( unsigned char red[256], 
						    unsigned char green[256],
							unsigned char blue[256] );


typedef byte color4ub_t[4];

typedef struct stageVars
{
	color4ub_t	colors[SHADER_MAX_VERTEXES];
	vec2_t		texcoords[NUM_TEXTURE_BUNDLES][SHADER_MAX_VERTEXES];
} stageVars_t;

typedef struct shaderCommands_s 
{
	vkIndex_t	indexes[SHADER_MAX_INDEXES] Q_ALIGN(16);
	vec4_t		xyz[SHADER_MAX_VERTEXES] Q_ALIGN(16);
	vec4_t		normal[SHADER_MAX_VERTEXES] Q_ALIGN(16);
	vec2_t		texCoords[SHADER_MAX_VERTEXES][2] Q_ALIGN(16);
	color4ub_t	vertexColors[SHADER_MAX_VERTEXES] Q_ALIGN(16);
	int			vertexDlightBits[SHADER_MAX_VERTEXES] Q_ALIGN(16);

	stageVars_t	svars Q_ALIGN(16);

	color4ub_t	constantColor255[SHADER_MAX_VERTEXES] Q_ALIGN(16);

	float		fogDensities[SHADER_MAX_VERTEXES] Q_ALIGN(16);

	shader_t	*shader;
	float		shaderTime;
	float		entityFade;
	int			fogNum;

	int			dlightBits;	
	int			dlightMap;

	int			numIndexes;
	int			numVertexes;

	int			numPasses;
	void		(*currentStageIteratorFunc)( void );
	shaderStage_t	**xstages;
    qboolean no_global_fog;
    qboolean vertexColorValid;
} shaderCommands_t;

extern	shaderCommands_t	tess;

void RB_BeginSurface(shader_t *shader, int fogNum );
void RB_NoteFeedSurf( const void *surf, int kind );
const void *RB_FeedSurf( int *kind );
void RB_ClearFeedSurf( void );
void RB_EndSurface(void);
extern qboolean vk_frameOpen;
void RB_BeginDrawingView( void );
void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs );
void RB_RenderSpriteSurfList( drawSurf_t *drawSurfs, int numDrawSurfs );
void RB_NoteIndexDesync( const char *walk, shader_t *shader, int carriedIndex );
shader_t *RB_ResolveStaleShader( const char *walk, int gen, int index );
/* why the last R_AddSkelSurfaces produced nothing, or NULL when it drew */
extern const char *r_skelDropWhy;
void R_NoteEntitySurfStart( void );
void R_NoteEntitySurfEnd( const trRefEntity_t *ent );
void RB_SetupFog( void );
const void *RB_DrawSurfs( const void *data );
const void *RB_SpriteSurfs( const void *data );
void RB_GLFogStageIteratorFunc( void );
void RB_CheckOverflow( int verts, int indexes );
#define RB_CHECKOVERFLOW(v,i) if (tess.numVertexes + (v) >= SHADER_MAX_VERTEXES || tess.numIndexes + (i) >= SHADER_MAX_INDEXES ) {RB_CheckOverflow(v,i);}

void RB_StageIteratorGeneric( void );
void RB_StageIterator2D( void );
qboolean VK_StageResolve2DColor( shaderStage_t *pStage );
void VK_Stage2DAuthorST( void );
qboolean VK_Stage2DResolveTexCoords( shaderStage_t *pStage );
void RB_StageIteratorVertexLitTextureUnfogged_2D( void );
void RB_StageIteratorSky( void );
void RB_StageIteratorVertexLitTextureUnfogged( void );
void RB_StageIteratorLightmappedMultitextureUnfogged( void );

void RB_AddQuadStamp( vec3_t origin, vec3_t left, vec3_t up, byte *color );
void RB_AddQuadStampExt( vec3_t origin, vec3_t left, vec3_t up, byte *color, float s1, float t1, float s2, float t2 );

void RB_ShowImages( qboolean quiet );




void R_AddBrushModelSurfaces( trRefEntity_t *e );
void R_GetInlineModelBounds(int iIndex, vec3_t vMins, vec3_t vMaxs);
int R_SphereInLeafs(const vec3_t p, float r, mnode_t** nodes, int nMaxNodes);
mnode_t* R_PointInLeaf(const vec3_t p);
int R_DlightTerrain(cTerraPatchUnpacked_t* surf, int dlightBits);
int R_CheckDlightTerrain(cTerraPatchUnpacked_t* surf, int dlightBits);
void R_AddWorldSurfaces( void );
qboolean R_inPVS( const vec3_t p1, const vec3_t p2 );






void R_DlightBmodel( bmodel_t *bmodel );
void R_GetLightingAtPoint(const vec3_t vPos, vec3_t vLight, const byte *groupIntensity);
void R_GetLightingForDecal(vec3_t vLight, const vec3_t vFacing, const vec3_t vOrigin);
void R_GetLightingForSmoke(vec3_t vLight, const vec3_t vOrigin);
void R_SetupEntityLighting( const trRefdef_t *refdef, trRefEntity_t *ent );
void RB_SetupStaticModelLighting(trRefdef_t* refdef, cStaticModelUnpacked_t* ent, const vec3_t lightOrigin);
// MOHAA's R_RealDlightFace / R_RealDlightPatch are NOT retail functions (neither name is
void R_CalcSurfLighting( msurface_t *surf );
void R_ClearDynamicLightmaps( void );
void R_UploadDynamicLightmaps( void );
void R_TransformDlights( int count, dlight_t *dl, orientationr_t *ori );
void RB_DLightFeedCensus( void );
void RB_Light_Sphere(unsigned char* colors);
void RB_Light_Sphere_ExactDLights(unsigned char* colors);
void RB_Light_FullbrightSphere(unsigned char* colors);
void R_Sphere_BuildDLights(spherel_t** pLightList);
void R_Sphere_SetupEntity(void);
void RB_Static_SetupModel(void);
void R_Sphere_InitLights(void);
int R_GatherLightSources(const vec3_t vPos, vec3_t* pvLightPos, vec3_t* pvLightIntensity, int iMaxLights);

int R_LightForPoint(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir);


void RB_ShadowTessEnd( void );
void RB_ShadowFinish( void );
void RB_ProjectionShadowDeform( void );


void R_BuildCloudData( shaderCommands_t *shader );
void R_InitSkyTexCoords( float cloudLayerHeight );
void R_DrawSkyBox( shaderCommands_t *shader );
void RB_ClipSkyPolygons( shaderCommands_t *shader );

void R_ShutdownShadows( void );


srfGridMesh_t *R_SubdividePatchToGrid( int width, int height, float subdivide,
								drawVert_t points[MAX_PATCH_SIZE*MAX_PATCH_SIZE] );


int R_MarkFragments( int numPoints, const vec3_t *points, const vec3_t projection,
				   int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer, float fPushDist);

int R_MarkFragmentsForInlineModel(clipHandle_t bmodel, const vec3_t vAngles, const vec3_t vOrigin, int numPoints,
	const vec3_t* points, const vec3_t projection, int maxPoints, vec3_t pointBuffer,
	int maxFragments, markFragment_t* fragmentBuffer);



void R_InitNextFrame( void );

void RE_ClearScene( void );
void RE_AddRefEntityToScene( const refEntity_t *ent );
void RE_AddRefSpriteToScene(const refEntity_t* ent);
void RE_AddTerrainMarkToScene(int iTerrainIndex, qhandle_t hShader, int numVerts, const polyVert_t* verts, int renderfx);
refEntity_t* RE_GetRenderEntity(int entityNumber);
qboolean RE_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t* verts, int renderfx);
void RE_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b, int type );
void RE_AddAdditiveLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void RE_AddLensflareToScene( void *key, const vec3_t org, int lensflareNum, float r, float g, float b, float scale, qhandle_t replacementShader );
void RE_RenderScene( const refdef_t *fd );


void R_MakeAnimModel( model_t *model );
void R_AddAnimSurfaces(trRefEntity_t* ent);

void R_ShutdownFont(void);
fontheader_t* R_LoadFont(const char* name);
void R_LoadFontShader(fontheader_sgl_t* font);
void R_DrawString(fontheader_t* font, const char* text, float x, float y, int maxlen, const float *pvVirtualScreen);
void R_DrawFloatingString(fontheader_t* font, const char* text, const vec3_t org, const vec4_t color, float scale, int maxlen);
float R_GetFontHeight(const fontheader_t* font);
float R_GetFontStringWidth(const fontheader_t* font, const char* s);


void R_UpdateGhostTextures(void);
void R_SetGhostImage(const char* name, image_t* image);
void LoadGHOST(const char* name, byte** pic, int* width, int* height);

void R_Sky_Init(void);
void R_Sky_Reset(void);
void R_Sky_AddSurf(msurface_t* surf);
void R_Sky_Render(void);

sprite_t* SPR_RegisterSprite(const char* name);
void RB_DrawSprite(const refSprite_t* spr);


void R_InitLensFlare(void);
void R_DrawLensFlares(void);

void R_LoadLensflares( void );
void R_ShutdownLensflares( void );
void R_RegisterLensflareShaders( void );
int  R_LensflareNameToNum( const char *name );
void R_ClearLensflareStates( void );

void RB_DrawSwipeSurface(surfaceType_t* pswipe);
void RE_SwipeBegin(float thistime, float life, qhandle_t shader);
void RE_SwipePoint(vec3_t point1, vec3_t point2, float time);
void RE_SwipeEnd(void);
void R_AddSwipeSurfaces(void);

void R_AddTerrainSurfaces(void);
void R_ShutdownTerrain(void);
void R_TerrainFree(void);
void R_TerrainPrepareFrame(void);
qboolean R_TerrainHeightForPoly(cTerraPatchUnpacked_t* pPatch, polyVert_t* pVerts, int nVerts);

void R_TerrainCrater_f(void);

struct skelHeaderGame_s;
struct skelAnimFrame_s;

void R_InitStaticModels(void);
void R_FreeNonTikiModels(void);
void R_FreeModels(void);
qhandle_t RE_SpawnEffectModel(const char* szModel, vec3_t vPos, vec3_t* axis);
qhandle_t RE_RegisterServerModel(const char* name);
void RE_UnregisterServerModel(qhandle_t hModel);
orientation_t RE_TIKI_Orientation(refEntity_t* model, int tagnum);
qboolean RE_TIKI_IsOnGround(refEntity_t* model, int tagnum, float threshold);
float R_ModelRadius(qhandle_t handle);
void R_ModelBounds(qhandle_t handle, vec3_t mins, vec3_t maxs);
dtiki_t* R_Model_GetHandle(qhandle_t handle);

float R_GetRadius(refEntity_t* model);
void R_GetFrame(refEntity_t* model, struct skelAnimFrame_s* newFrame);
void RE_ForceUpdatePose(refEntity_t* model);
void RE_SetFrameNumber(int frameNumber);
int  R_UpdatePoseInternal(refEntity_t* model);
void RB_SkelMesh(skelSurfaceGame_t* sf);
void RB_TikiMesh(skelSurfaceGame_t* sf);	 
void RB_SurfaceTerrain(srfTriangles_t* srf);	 
void RB_Static_BuildDLights(void);
void R_AddSkelSurfaces(trRefEntity_t* ent);
void R_AddTikiSurfaces(trRefEntity_t* ent);
void R_CountTikiLodTris(dtiki_t* tiki, float lodpercentage, int* render_tris, int* total_tris);
float R_TikiCalcLodDist(const vec3_t org, const dtiki_t* tiki);
int GetLodCutoff(const struct skelSurfaceGame_s* surf, float lodpercentage);
void R_InfoWorldTris_f(void);
void R_PrintInfoWorldtris(void);

extern int g_nStaticSurfaces;
extern qboolean g_bInfostaticmodels;
extern qboolean g_bInfoworldtris;


void RB_StreamBegin(shader_t* shader);
void RB_StreamEnd(void);
void RB_StreamBeginDrawSurf(void);
void RB_StreamEndDrawSurf(void);
void RB_Vertex3fv(vec3_t v);
void RB_Vertex3f(vec_t x, vec_t y, vec_t z);
void RB_Vertex2f(vec_t x, vec_t y);
/* the single 2D placement site: every 2D vertex writer ends with one of these */
void RB_2DPlaceVertex(float *xyz);
void RB_2DPlaceRange(int first, int count);
void RB_Color4f(vec_t r, vec_t g, vec_t b, vec_t a);
void RB_Color3f(vec_t r, vec_t g, vec_t b);
void RB_Color3fv(vec3_t col);
void RB_Color4bv(unsigned char* colors);
void RB_Texcoord2f(float s, float t);
void RB_Texcoord2fv(vec2_t st);
void R_DrawDebugNumber(const vec3_t org, float number, float scale, float r, float g, float b, int precision);
void R_DebugRotatedBBox(const vec3_t org, const vec3_t ang, const vec3_t mins, const vec3_t maxs, float r, float g, float b, float alpha);
void R_DebugCircle(const vec3_t org, float radius, float r, float g, float b, float alpha, qboolean horizontal);
void R_DebugLine(const vec3_t start, const vec3_t end, float r, float g, float b, float alpha);
int RE_GetShaderWidth(qhandle_t hShader);
int RE_GetShaderHeight(qhandle_t hShader);
const char* RE_GetShaderName(qhandle_t hShader);
const char* RE_GetModelName(qhandle_t hModel);

void	R_TransformModelToClip( const vec3_t src, const float *modelMatrix, const float *projectionMatrix,
							vec4_t eye, vec4_t dst );
void	R_TransformClipToWindow( const vec4_t clip, const viewParms_t *view, vec4_t normalized, vec4_t window );

void	RB_DeformTessGeometry( void );

void	RB_CalcEnvironmentTexCoords( float *dstTexCoords );
void	RB_CalcEnvironmentTexCoords2( float *dstTexCoords );
void	RB_CalcAngleBasedEnvironmentTexCoords( float *dstTexCoords );
void	RB_CalcSunReflectionTexCoords( float *dstTexCoords );
void	RB_CalcOffsetTexCoords(const float* offset, float* st);
void	RB_CalcParallaxTexCoords(const float* rate, float* st);
void	RB_TextureAxisFromPlane(const vec3_t normal, vec3_t xv, vec3_t yv);
void	RB_QuakeTextureVecs(const vec3_t normal, const vec2_t scale, vec3_t mappingVecs[2]);
void	RB_CalcMacroTexCoords(const  float* rate, float* st);
void	RB_CalcScrollTexCoords( const float scroll[2], float *dstTexCoords );
void	RB_CalcRotateTexCoords( float degsPerSecond, float degsPerSecondCoef, float* st, float start );
void	RB_CalcScaleTexCoords( const float scale[2], float *dstTexCoords );
void	RB_CalcTurbulentTexCoords( const waveForm_t *wf, float *dstTexCoords );
void	RB_CalcTransformTexCoords( const texModInfo_t *tmi, float *dstTexCoords );
void	RB_CalcWaveAlpha( const waveForm_t *wf, unsigned char *dstColors );
void	RB_CalcWaveColor(const waveForm_t* wf, unsigned char* dstColors, unsigned char* constantColor);
void	RB_CalcAlphaFromEntity( unsigned char *dstColors );
void	RB_CalcAlphaFromOneMinusEntity( unsigned char *dstColors );
void	RB_CalcStretchTexCoords( const waveForm_t *wf, float *texCoords );
void	RB_CalcColorFromEntity( unsigned char *dstColors );
void	RB_CalcColorFromEntityLighting( unsigned char *dstColors );
void	RB_CalcColorFromOneMinusEntity( unsigned char *dstColors );
void	RB_CalcColorFromConstant(unsigned char* dstColors, unsigned char* constantColor);
void	RB_CalcRGBFromDot(unsigned char* colors, float alphaMin, float alphaMax);
void	RB_CalcRGBFromOneMinusDot(unsigned char* colors, float alphaMin, float alphaMax);
void	RB_CalcAlphaFromConstant(unsigned char* dstColors, int constantAlpha);
void	RB_CalcAlphaFromDot(unsigned char* colors, float alphaMin, float alphaMax);
void	RB_CalcAlphaFromHeightFade(unsigned char* colors, float alphaMin, float alphaMax);
void	RB_CalcAlphaFromOneMinusDot(unsigned char* colors, float alphaMin, float alphaMax);
void	RB_CalcAlphaFromTexCoords(unsigned char* colors, float alphaMin, float alphaMax, int alphaMinCap, int alphaCap, float sWeight, float tWeight, float* st);
void	RB_CalcRGBFromTexCoords(unsigned char* colors, float alphaMin, float alphaMax, int alphaMinCap, int alphaCap, float sWeight, float tWeight, float* st);
void	RB_CalcSpecularAlpha(unsigned char* alphas, float alphaMax, vec3_t specOrigin);
void	RB_CalcAlphaFromDotView(unsigned char* colors, float alphaMin, float alphaMax);
void	RB_CalcAlphaFromOneMinusDotView(unsigned char* colors, float alphaMin, float alphaMax);
void	RB_CalcDiffuseColor( unsigned char *colors );

void	GetColorBlockColors( byte *data, byte *c0, byte *c1, byte *c2, byte *c3, unsigned short *bits );	 
void	DecodeColorBlock( unsigned int *dst, byte *block, int width, unsigned int *c0, unsigned int *c1, unsigned int *c2, unsigned int *c3 );	 
void	decodeAlphaExplicit( unsigned int *dst, unsigned short *src, int stride, unsigned int mask );	 
int	GetDDSOffset( int level, int levels, int width, int height, int blockBytes );			 
void	R_MipMap2( byte *in, int inWidth, int inHeight );						 
void	verifyProperTextureSize( const char *name, int width, int height );				 
int	getGLClampType( int wrapAxis, int textureMode );						 
void	VK_SetTextureAnisotropicFiltering( unsigned int target );						 
void	VK_SetupAnisotropicFiltering( void );								 
void	RB_CalcModulateAlphasByFog_Alpha( unsigned char *dstColors );					 
void	RB_CalcModulateColorsByFog_Alpha( unsigned char *dstColors );					 
void	RB_CalcModulateRGBAsByFog_Alpha( unsigned char *dstColors );					 
void	RB_CalcModulateColorsByFog( unsigned char *dstColors );						 
void	RB_CalcModulateAlphasByFog( unsigned char *dstColors );						 
void	RB_CalcModulateRGBAsByFog( unsigned char *dstColors );						 
void	RB_CalcAlphaFogDensities( void );											 
void	RB_CalcFogTexCoords( float *dstTexCoords );									 
void	RB_CalcAlphaFogBlend( unsigned char *dstColors );							 

void	RB_CalcStretchTexCoords(const waveForm_t* wf, float* st);
void	RB_CalcTransWaveTexCoords(const waveForm_t* wf, float* st);
void	RB_CalcTransWaveTexCoordsT(const waveForm_t* wf, float* st);
void	RB_CalcBulgeTexCoords(const waveForm_t* wf, float* st);


void RB_RenderThread( void );
void RB_ExecuteRenderCommands( const void *data );


#define	MAX_RENDER_COMMANDS	0x40000

typedef struct suninfo_s {
	vec3_t color;
	vec3_t direction;
	vec3_t flaredirection;
	char szFlareName[64];
	qboolean exists;
} suninfo_t;

extern suninfo_t s_sun;
extern vec3_t spheredef[SPHERE_NUM_BASIS];

typedef struct {
	byte	cmds[MAX_RENDER_COMMANDS];
	int		used;
} renderCommandList_t;

typedef struct {
	int		commandId;
	float	color[4];
} setColorCommand_t;

typedef struct {
	int		commandId;
	int		buffer;
} drawBufferCommand_t;

typedef struct {
	int		commandId;
	image_t	*image;
	int		width;
	int		height;
	void	*data;
} subImageCommand_t;

typedef struct {
	int		commandId;
} swapBuffersCommand_t;

typedef struct {
	int		commandId;
	int		buffer;
} endFrameCommand_t;

typedef struct {
	int		commandId;
	shader_t	*shader;
	float	x, y;
	float	w, h;
	float	s1, t1;
	float	s2, t2;
} stretchPicCommand_t;

typedef struct {
	int		commandId;
	trRefdef_t	refdef;
	viewParms_t	viewParms;
	drawSurf_t *drawSurfs;
	int		numDrawSurfs;
} drawSurfsCommand_t;

typedef struct {
	int commandId;
	int x;
	int y;
	int width;
	int height;
	char *fileName;
} screenshotCommand_t;

typedef struct {
	int						commandId;
	int						width;
	int						height;
	byte					*captureBuffer;
	byte					*encodeBuffer;
	qboolean			motionJpeg;
} videoFrameCommand_t;

typedef struct
{
	int commandId;

	qboolean	rgba[4];
} colorMaskCommand_t;

typedef struct
{
	int commandId;
} clearDepthCommand_t;

/* the 2D overlay commands: appended, every field resolved on the record thread */
typedef struct {
	int			commandId;
	byte		color[4];
} color2DCommand_t;

typedef struct {
	int			commandId;
	shader_t	*shader;
	float		x, y, w, h;
	float		s1, t1, s2, t2;
} stretchPic2DCommand_t;

typedef struct {
	int			commandId;
	shader_t	*shader;
	float		x, y, w, h;
	float		s1, t1, s2, t2;
	float		sx, sy;
} stretchPic2Command_t;

typedef struct {
	int			commandId;
	int			x, y, w, h;
	int			cols, rows;
} stretchRaw2DCommand_t;

typedef struct {
	int			commandId;
	shader_t	*shader;
	float		x, y, w, h;
	float		picw, pich;
	int			offsetX, offsetY;
} tilePic2DCommand_t;

typedef struct {
	int			commandId;
	shader_t	*shader;
	float		points[3][2];
	float		texCoords[3][2];
} trianglePic2DCommand_t;

typedef struct {
	int			commandId;
	float		x, y, w, h;
	int			blend;			/* 0 = AddBox (ONE/ONE), 1 = DrawBox (SRC_ALPHA) */
} box2DCommand_t;

typedef struct {
	int			commandId;
	int			count;
	int			stipple_factor;
	int			stipple_mask;
	/* count vec2_t points follow, padded to sizeof(void *) */
} lineLoop2DCommand_t;

typedef struct {
	int			commandId;
	int			x, y, w, h;
	float		left, right, bottom, top, n, f;
} setWindow2DCommand_t;

typedef struct {
	int			commandId;
	int			x, y, width, height;
} scissor2DCommand_t;

/* the widget's placement inside the pass's one screen-space 2D window: retail keeps glViewport/glOrtho constant for every widget and puts
   the placement in the vertices (retail-menu-solomatch-final.trace 10103-10124), so this record carries the offset the 2D vertex writers add. */
typedef struct {
	int			commandId;
	float		x, y;
} origin2DCommand_t;

typedef struct {
	int			commandId;
	float		startTime;
} shaderTime2DCommand_t;

typedef struct {
	float		x, y;
	float		pos[2];
	float		size[2];
} fontGlyph_t;

typedef struct {
	int			commandId;
	shader_t	*shader;
	int			numGlyphs;
	int			scaled;
	float		z;
	float		charHeight;
	float		glyphScale;
	float		widthScale, heightScale;
	/* the placement and clip the string was QUEUED under: the run flushes after
	   later widgets' state records, so the walk's live mirror is not its own */
	float		originX, originY;
	int			scissorValid;
	int			scissor[4];
	byte		color[4];
} fontString2DCommand_t;

typedef enum {
	RC_END_OF_LIST,
	RC_SET_COLOR,
	RC_STRETCH_PIC,
	RC_DRAW_SURFS,
	RC_SPRITE_SURFS,
	RC_DRAW_BUFFER,
	RC_SWAP_BUFFERS,
	RC_SCREENSHOT,
	RC_VIDEOFRAME,
	RC_COLORMASK,
	RC_CLEARDEPTH,
	RC_2D_COLOR,
	RC_2D_STRETCH_PIC,
	RC_2D_STRETCH_PIC2,
	RC_2D_TILE_PIC,
	RC_2D_TILE_PIC_OFFSET,
	RC_2D_TRIANGLE_PIC,
	RC_2D_BOX,
	RC_2D_LINE_LOOP,
	RC_2D_SET_WINDOW,
	RC_2D_SCISSOR,
	RC_2D_ORIGIN,
	RC_2D_FONT_STRING,
	RC_2D_SHADER_TIME,
	RC_2D_STRETCH_RAW,

	/* this module's own records: the frame prologue and the present */
	RC_VK_BEGIN,
	RC_VK_PRESENT
} renderCommand_t;


#define	MAX_POLYS		4096
#define	MAX_POLYVERTS	16384

typedef struct {
	drawSurf_t	drawSurfs[MAX_DRAWSURFS];
	drawSurf_t  spriteSurfs[MAX_SPRITESURFS];
	dlight_t	dlights[MAX_DLIGHTS];
	lensflare_t	lensflares[MAX_LENSFLARES];
	trRefEntity_t	entities[MAX_ENTITIES];
	srfPoly_t	*polys;
	polyVert_t	*polyVerts;
	refSprite_t sprites[2048];
	cStaticModelUnpacked_t* staticModels;
	byte* staticModelData;
	renderCommandList_t	commands;
} backEndData_t;

extern	int		max_polys;
extern	int		max_polyverts;

extern	backEndData_t	*backEndData;
extern	backEndData_t	*backEndDataBuf[SMP_FRAMES];

extern	volatile renderCommandList_t	*renderCommandList;

extern	volatile qboolean	renderThreadActive;


void *R_GetCommandBuffer( int bytes );
void R_AddDrawSurfCmd( drawSurf_t *drawSurfs, int numDrawSurfs );
void R_AddSpriteSurfCmd( drawSurf_t* drawSurfs, int numDrawSurfs );
void RB_ExecuteRenderCommands( const void *data );
void RB_RenderThread( void );
qboolean GLimp_GLContextMakeCurrent( qboolean makeCurrent );
qboolean GLimp_GLMainOwnsContext( void );

int		RB_FrameSmpFrame( void );	
int		RB_FrameCount( void );		
int		RB_FrameBoneArena( void );	
int		RB_FrameBackEndMsec( void );	
void	RB_FrameBackEndReset( void );

void R_SavePerformanceCounters(void);
void R_InitCommandBuffers( void );
void R_ShutdownCommandBuffers( void );

void R_SyncRenderThread( void );
void R_FlushRenderCommands( void );

void R_AddDrawSurfCmd( drawSurf_t *drawSurfs, int numDrawSurfs );

void RE_SetColor( const float *rgba );
void RE_StretchPic ( float x, float y, float w, float h, 
					  float s1, float t1, float s2, float t2, qhandle_t hShader );
int			RE_GetRegistrationSequence( void );	 
qhandle_t	RE_ReplaceShader( const char *name, const char *text );	 
void RE_BeginFrame( stereoFrame_t stereoFrame );
void RE_EndFrame( int *frontEndMsec, int *backEndMsec );
void VK_PerfFpsRow( void );
void RE_SaveJPG(char* filename, int quality, int image_width, int image_height,
    unsigned char* image_buffer, int padding);
size_t RE_SaveJPGToBuffer(byte* buffer, size_t bufSize, int quality,
    int image_width, int image_height, byte* image_buffer, int padding);
void RE_TakeVideoFrame( int width, int height,
		byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg );

void SaveJPG(char * filename, int quality, int image_width, int image_height, unsigned char *image_buffer);

void UI_LoadResource(const char* name);

#define SMP_RLZ_world		0
#define SMP_RLZ_bspread	1
#define SMP_RLZ_shaders	2
#define SMP_RLZ_lightmap	3
#define SMP_RLZ_lods		4
#define SMP_RLZ_reginit	5
#define SMP_RLZ_imginit	6
#define SMP_RLZ_setup		7
#define SMP_RLZ_findimg	8
#define SMP_RLZ_decode		9
#define SMP_RLZ_transcode	10
#define SMP_RLZ_upload		11
#define SMP_RLZ_modelreg	12
#define SMP_RLZ_shparse	13
#define SMP_RLZ_findsync	14
#define SMP_RLZ_texcreate	15
#define SMP_RLZ_COUNT		16

extern	int		smp_rloadDepth;	
extern	cvar_t	*smp_rloadStats;	

int		SMP_RLoadNow( void );
void	SMP_RLoadAdd( int zone, int msec, int depth );

static inline int SMP_RLoadArmed( void ) {
	/* master gate (qa_probe.h); the GLES3 lane already carried this AND */
	return ( smp_rloadStats && smp_rloadStats->integer && R_ProbeStatsOn() ) ? 1 : 0;
}

#define SMP_RLOAD_BEGIN(z) \
	int smp_ra_##z = SMP_RLoadArmed(); \
	int smp_rld_##z = ( smp_ra_##z ? smp_rloadDepth++ : 0 ); \
	int smp_rlt_##z = ( smp_ra_##z ? SMP_RLoadNow() : 0 )
#define SMP_RLOAD_END(z) \
	do { \
		if ( smp_ra_##z ) { \
			smp_rloadDepth--; \
			SMP_RLoadAdd( SMP_RLZ_##z, SMP_RLoadNow() - smp_rlt_##z, smp_rld_##z ); \
		} \
	} while ( 0 )

typedef enum {
	BE_FAM_BE = 0,
	BE_FAM_DS,
	BE_FAM_PROG,
	BE_FAM_FE,
	BE_FAM_COUNT
} be_family_t;

typedef enum {
	BE_BEZ_surfs = 0, BE_BEZ_sprites, BE_BEZ_sprwalk, BE_BEZ_pic, BE_BEZ_buf,
	BE_BEZ_swap, BE_BEZ_color, BE_BEZ_other,
	BE_BE_ZONES
} be_be_zone_t;

typedef enum {
	BE_BEC_lists = 0, BE_BEC_cmds, BE_BEC_surfs, BE_BEC_draws,
	BE_BEC_empty, BE_BEC_sprN, BE_BEC_sprBatch, BE_BEC_sprDraw,
	BE_BE_COUNTERS
} be_be_counter_t;

typedef enum {
	BE_DSZ_walk = 0, BE_DSZ_end, BE_DSZ_flare, BE_DSZ_shadow,
	BE_DSZ_other,
	BE_DS_ZONES
} be_ds_zone_t;

typedef enum {
	BE_DSZ_sub_stage = 0, BE_DSZ_sub_vbo, BE_DSZ_sub_unif,
	BE_DSZ_sub_shadow,
	BE_DSZ_sub_pack, BE_DSZ_sub_upload, BE_DSZ_sub_draw,
	BE_DS_SUBS
} be_ds_sub_t;

typedef enum {
	BE_DSC_surfs = 0, BE_DSC_batches, BE_DSC_verts, BE_DSC_stages,
	BE_DSC_draws, BE_DSC_unifs, BE_DSC_unifskip, BE_DSC_binds, BE_DSC_edges,
	BE_DS_COUNTERS
} be_ds_counter_t;

typedef enum {
	BE_PZ_compile = 0, BE_PZ_link, BE_PZ_other,
	BE_PROG_ZONES
} be_prog_zone_t;

typedef enum {
	BE_PC_made = 0,
	BE_PROG_COUNTERS
} be_prog_counter_t;

/* the reference renderer's note, carried in full in its own tree */
typedef enum {
	BE_FEZ_view = 0, BE_FEZ_world, BE_FEZ_ents, BE_FEZ_poly, BE_FEZ_sprite,
	BE_FEZ_sort, BE_FEZ_exec, BE_FEZ_other,
	BE_FE_ZONES
} be_fe_zone_t;

typedef enum {
	BE_FEC_surfs = 0, BE_FEC_ents, BE_FEC_views, BE_FEC_portals,
	BE_FE_COUNTERS
} be_fe_counter_t;

/* One [BE:send] window, printed with the [BE:ds] and [BE:be] rows. */
typedef struct {
	int		batches, runs, sum, first, mid, max, gapMax, verts, big, bigVerts;
	/* World surfaces in the window, the accumulator [BE:be] prints as `surfN'. */
	int		surfs;
	/* the batches the store could not hold and the ring drew instead */
	int		fall;
	/* writes/local/statics/multi/seq: the reference renderer's arena arms */
	int		writes, local, statics, multi, seq;
	int		locVisits, locVtx, locIdx, locHash, locFirstAt;
	unsigned locFirst, locWas, locNow;
	/* Gate rejections, one reason a batch; `recs' is records seen. */
	int		gateWorld, gateDlight, gateIter, gateSig, gateRec, recs;
	/* the 2D share of batches/sum */
	int		d2Batches, d2Sum;
} beSendRow_t;

qboolean	RB_VboSendTake( beSendRow_t *out );

/* 1 the frame store, 0 the ring, printed with the send row */
int			RB_VboArenaMode( void );

int		BE_Microseconds( void );
int		BE_Armed( void );

void	BE_RootBegin( int family );
void	BE_RootEnd( int family );

void	BE_Zone( int family, int zone, int usec );
void	BE_Sub( int family, int sub, int usec );
void	BE_Bump( int family, int counter, int n );

void	BE_Name( int family, const char *name, int usec );

int		BE_RootUsec( int family );

static inline int BE_On( void ) {
	return BE_Armed();
}

#define BE_ZONE(fam, z, stmt) \
	do { \
		if ( BE_On() ) { \
			int be_t0 = BE_Microseconds(); \
			stmt; \
			BE_Zone( (fam), (z), BE_Microseconds() - be_t0 ); \
		} else { \
			stmt; \
		} \
	} while ( 0 )

#define BE_SUB(fam, s, stmt) \
	do { \
		if ( BE_On() ) { \
			int be_t0 = BE_Microseconds(); \
			stmt; \
			BE_Sub( (fam), (s), BE_Microseconds() - be_t0 ); \
		} else { \
			stmt; \
		} \
	} while ( 0 )

/* the texture-env combine tokens, the reference renderer's own ordinals under this module's name */
#define	VK_TENV_ADD		0x0104
#define	VK_TENV_MODULATE	0x2100
#define	VK_TENV_DECAL		0x2101
#define	VK_TENV_REPLACE		0x1E01
#define	VK_TENV_BLEND		0x0BE2

/* the 2D immediate stream's primitive kinds */
#define	VK_IMM_TRIANGLES		0
#define	VK_IMM_LINES			1
#define	VK_IMM_LINE_LOOP		2
#define	VK_IMM_TRIANGLE_STRIP	3
/* this module's own declarations */
void RB_BeginSurface(shader_t *shader, int fogNum);
void RB_ImmBegin( int topology );
VkPipelineLayout VK_Draw_Layout( void );
void RB_ImmBeginWithImage( int topology, image_t *image, unsigned long stateBits );
void RB_ImmCommit( void );
void RB_ImmEnd( void );
void RB_ImmEndDeferred( void );
void RB_ImmFlush( void );
int  RB_ImmStreamVerts( void );
void RB_ImmVertex2f( float x, float y );
void RB_ImmColor4f( float r, float g, float b, float a );
void RB_ImmTexCoord2f( float s, float t );
void		R_ShutdownImages( void );
int			RE_GetShaderWidth( qhandle_t hShader );
int			RE_GetShaderHeight( qhandle_t hShader );
void		R_ShutdownShaders( void );
void		R_InitShaders( void );
void		R_LoadShaderStates( void );
void		R_SetGlobalColour( float r, float g, float b );
void		R_ChangeTextColor( int index );
void		R_SetFontHeightScale( float scale );
void		R_SetFontScale( float scale );
void		R_SetFontZ( float zed );
extern cvar_t	*r_developer;
extern cvar_t	*r_customwidth;
extern cvar_t	*r_customheight;
extern cvar_t	*r_customaspect;
void R_RegisterCvars( void );
qboolean BE_On( void );
void R_FreeImage( image_t *image );
void R_BuildGammaTables( void );
void R_CreateBuiltinImages( void );
void R_LoadBMP( const char *name, byte **pic, int *width, int *height );
void R_LoadTGA( const char *name, byte **pic, int *width, int *height );
void	R_DumpTextureMemory(void);
void	 VK_IteratorTouched( const char *what );
void	 ProjectDlightTexture( void );
void	R_SetupShaders( void );
void	R_StartupShaders( void );
void	R_DeleteImageFromHash( image_t *image );
int		R_Bundle_StampFold( const textureBundle_t *b, const image_t *bound, int *frame );

int      R_CountTextureMemory( void );
qboolean R_ImageExists( const char *name );
qboolean R_LoadRawImage( const char *name, byte **pic, int *width, int *height );
void     R_FreeRawImage( byte *pic );
void     RE_RegisterFont( const char *fontName, int pointSize, fontInfo_t *font );
extern int vk_back_end_slot;
void rb_set_slot( int slot );
void R_UploadScratchImage( int cols, int rows, const byte *rgb );
void R_UploadScratchRGBA( int cols, int rows, const byte *rgba );
void RE_StretchRaw( int x, int y, int w, int h, int cols, int rows, int components, const byte *data,
					qboolean dirty );
void RE_UploadCinematic( int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty );
void RB_StretchRawDraw( const stretchRaw2DCommand_t *cmd );
unsigned char *VK_VertexRing_Mapped( void );
int		VK_VertexRing_ReserveBytes( int bytes, int *byteBaseOut );
void	VK_Draw_BindRingOffset( VkCommandBuffer cmd, VkDeviceSize vertexOffset );
void	VK_Draw_BindVertexBuffer( VkCommandBuffer cmd, VkBuffer buffer, VkDeviceSize offset );
void	VK_Draw_BindIndexBuffer( VkCommandBuffer cmd, VkBuffer buffer );
void RB_Texcoord2f( float s, float t );
void RB_Vertex2f( vec_t x, vec_t y );
void RB_Color4bv( unsigned char *colors );
void RB_StreamEnd( void );
qboolean VKimp_GLMainOwnsContext( void );
qboolean RB_RecordingFrame( void );
void RB_FontRunDraw( VkCommandBuffer cb, fontString2DCommand_t *cmd, const fontGlyph_t *glyphs );
void R_IssuePendingRenderCommands( void );
void *R_GetCommandBufferReserved( int bytes, int reservedBytes );
void RB_InitBackend( void );
void RB_FrameOpen( int slot );
qboolean RB_RecordSurfaces( VkCommandBuffer cmd );
void RB_FramePrologue( void );

/* this module's 3D state machine (the reference keeps these in tr_backend.c
   and tr_program.c; the bodies live in vk_shade.c) */
void		VK_SelectTexture( int tmu );
void		VK_Bind( image_t *image );
void		VK_BindUnit0( image_t *image );
void		VK_BindMultitexture( image_t *image, int tmu );
void		VK_TexEnv( int env );
void		VK_DepthState( qboolean test, qboolean write );
void		VK_DepthStateReset( void );
void		VK_Shade_DlightWitness( const dlight_t *dl, int light, int numIndexes );
void		ProjectDlightTexture( void );
void		RB_StageIteratorSky( void );
void		RB_StageIteratorVertexLitTextureUnfogged( void );
void		RB_StageIteratorLightmappedMultitextureUnfogged( void );
void		RB_GLFogStageIteratorFunc( void );
void		RB_ShadowTessEnd( void );
void		RB_ShadowFinish( void );
void		RB_ProjectionShadowDeform( void );
void		RB_DeformTessGeometry( void );
void		R_InitSkyTexCoords( float cloudLayerHeight );
void		R_BuildCloudData( shaderCommands_t *shader );
void		LoadGHOST( const char *name, byte **pic, int *width, int *height );
void		R_SetGhostImage( const char *name, image_t *image );
void		R_UpdateGhostTextures( void );
qboolean	VKimp_GLMainOwnsContext( void );
skelHeaderGame_t *probe_TIKI_GetSkel_decl( void );
qboolean	VK_Shade_Bind( VkCommandBuffer cmd, const shader_t *shader );
VkCommandBuffer VK_Shade_CommandBuffer( void );
qboolean	VK_Shade_Descriptors_Init( void );
void		VK_Shade_Descriptors_Shutdown( void );
qboolean	VK_Shade_Init( void );
void		VK_Shade_Shutdown( void );
void		VK_Shade_NewFrame( void );
/* drop the walk's texture latch: a registration recycles every slot */
void		VK_Shade_RegistrationReset( void );
/* build a shader's stage pipelines before the draw that would build them */
VkPipeline	VK_Shade_Pipeline( const char *shader, unsigned stateBits, int family,
							   unsigned defines, cullType_t cull,
							   qboolean polygonOffset, int topology );
void		VK_Shade_WarmReset( void );
void		VK_Shade_WarmArm( void );
int			VK_Shade_WarmRegistration( int budgetMsec );
void		VK_Shade_WarmFrameStep( int budgetMsec );
int			VK_Shade_WarmTakeNewPipelines( void );
void		VK_Shade_WarmNoteBuild( const char *shader, unsigned stateBits, long long usec );
void		VK_2DOrder_Note( const char *shader );
void		VK_Shade_NoteDraw( int shade );
void		VK_Shade_NoteSurfCmds( int surfs );
void		VK_Shade_NoteBatchSurf( void );
void		VK_Shade_NoteStage( const struct shader_s *shader );
void		VK_Shade_NoteFlat( const struct shader_s *shader );
int			VK_WorldFlatMode( void );
/* one row per animated shader per second: the shader clock's inputs */
void		VK_Shade_NoteShaderTime( const char *kind );
	void		VK_Shade_ReadbackFrame( unsigned imageIndex );
/* a UI 3D widget's screen rect, so the presented frame can be asked what ended
   up inside it: CONTROLS draws two RenderModels and one of them is covered */
void		VK_Shade_Note3DRect( int x, int y, int w, int h );
int		VK_Shade_FrameBatches( void );
void		VK_Shade_NotePerm( int miss );

int		VK_Shade_ModelViewDepth( void );
int		VK_Shade_ProjectionDepth( void );

/* ---- the legacy spellings the mirrored 3D front end writes, resolved onto
   this module's own tokens; no legacy raster header is involved ---- */
#undef VK_LINES
#define VK_LINES				VK_IMM_LINES
#undef VK_TRIANGLES
#define VK_TRIANGLES			VK_IMM_TRIANGLES
#undef VK_TRIANGLE_STRIP
#define VK_TRIANGLE_STRIP		VK_IMM_TRIANGLE_STRIP
#undef VK_LINE_LOOP
#define VK_LINE_LOOP			VK_IMM_LINE_LOOP
#undef VK_IMM_TRIANGLE_FAN
#define VK_IMM_TRIANGLE_FAN		4
#undef VK_TRIANGLE_FAN
#define VK_TRIANGLE_FAN			VK_IMM_TRIANGLE_FAN
#undef VK_FALSE
#define VK_FALSE				0
#undef VK_TRUE
#define VK_TRUE					1
#undef VK_CULL_FACE
#define VK_CULL_FACE			1
#undef VK_POLYGON_OFFSET_FILL
#define VK_POLYGON_OFFSET_FILL	2
#undef VK_TEXTURE_2D
#define VK_TEXTURE_2D			3
#undef VK_RGBA
#define VK_RGBA					4
#undef VK_UNSIGNED_BYTE
#define VK_UNSIGNED_BYTE		5
#undef VK_FRONT
#define VK_FRONT				6
#undef VK_BACK
#define VK_BACK					7
#undef VK_KEEP
#define VK_KEEP					8
#undef VK_INCR
#define VK_INCR					9
#undef VK_DECR
#define VK_DECR					10
#undef VK_ALWAYS
#define VK_ALWAYS				11
#undef VK_NOTEQUAL
#define VK_NOTEQUAL				12
#undef VK_EQUAL
#define VK_EQUAL				13
#undef VK_LEQUAL
#define VK_LEQUAL				14

/* the sub-stream: folded into the pending pipeline key, or dropped where this pass has none */
void	vkEnable( int cap );
void	vkDisable( int cap );
void	vkCullFace( int mode );
void	vkStencilOp( int fail, int zfail, int zpass );
void	vkColorMask( qboolean r, qboolean g, qboolean b, qboolean a );
void	vkLineWidth( float width );
void	vkDepthRangef( float n, float f );
void	vkDepthRange( float n, float f );
void	VK_Draw_DepthRangeLast( float *n, float *f );
qboolean R_ProbeMarkOn( void );
void	vkPolygonOffset( float factor, float units );
qboolean VK_Shade_PolygonOffset( void );
void	VK_Shade_SetStencil( qboolean test, int func, int ref, unsigned int mask );

/* ---- the vertex ring ----------------------------------------------------- */

typedef struct {
	float	xyz[3];
	float	st[2];
	byte	color[4];
} vkTessVertex_t;

#define VK_TESS_VERTEX_BYTES ( (int)sizeof( vkTessVertex_t ) )
/* a level frame, not a menu frame, is the demand the ring has to hold, and the engine does not bound the draw-surf count that sets it.  The
   ceiling has been raised twice by that check (1,481 draw surfs refused 155 at 24,000 slots; 3,596 refused 529 at 72,000, that frame's demand
   111,259 slots) while streaming efficiency stayed flat, so 48 batches / 144,000 slots is capacity 29% over the measured worst frame; `demand=` and `peak=` name both numbers every boot. */
#define VK_RING_MAX_VERTEXES ( SHADER_MAX_VERTEXES * 48 )
#define VK_RING_MAX_INDEXES  ( SHADER_MAX_INDEXES * 48 )

qboolean VK_VertexRing_Init( void );
void     VK_VertexRing_Shutdown( void );
void     VK_VertexRing_ResetFrame( void );
void     VK_VertexRing_SetFrame( int slot );
vkTessVertex_t *VK_VertexRing_Reserve( int vertexes, int *baseOut );
vkIndex_t *VK_VertexRing_ReserveIndexes( int indexes, int *baseOut );
void     VK_VertexRing_RollBack( int vertexes, int indexes );
vkTessVertex_t *VK_VertexRing_Tail( int back );
void     vk_ring_note( int vertexHigh, int indexHigh );
VkBuffer  VK_VertexRing_Buffer( void );
VkBuffer  VK_VertexRing_IndexBuffer( void );
int       VK_VertexRing_Used( void );
int       VK_VertexRing_Flushed( void );

/* one surface's packed slice, reused by the surface's remaining stages: retail locks a surface's vertex array once and issues every stage's
   draw against it (the traced menu frame calls glLockArraysEXT five times and draws thirteen times), and reserving per stage multiplied the ring's demand by the stage count.  The key is the whole pack source tuple, so a slice is shared only when every stage packs byte-identical vertices. */
qboolean  VK_VertexRing_StreamMatch( int vertexes, int indexes,
                                     const void *tc0, const void *tc1, int tcStride,
                                     const void *colors, const void *normals,
                                     int *baseOut, int *indexBaseOut );
void      VK_VertexRing_StreamStore( int vertexes, int indexes,
                                     const void *tc0, const void *tc1, int tcStride,
                                     const void *colors, const void *normals,
                                     int base, int indexBase );
void      VK_VertexRing_StreamForget( void );
int       VK_VertexRing_SliceToken( void );
void      VK_VertexRing_SurfaceBegin( void );
void      VK_VertexRing_FrameDemand( int *usedOut, int *vertexesOut, int *indexesOut,
                                     int *dropsOut, int *surfacesOut );

/* ---- pipelines ----------------------------------------------------------- */

typedef struct {
	int			topology;			/* VK_IMM_* */
	unsigned	stateBits;			/* the stage's state-bit tuple */
	cullType_t	cull;
	qboolean	depthTest;
	qboolean	depthWrite;
	qboolean	textured;			/* samples the stage bundle, or paints flat */
} vkPipelineKey_t;

VkPipeline VK_Draw_Pipeline( const vkPipelineKey_t *key );

/* The only two sites that bind a pipeline or a set, so the per-command-buffer
   state cache behind them cannot go stale: a repeated bind is skipped. */
void     VK_Draw_BindPipeline( VkCommandBuffer cmd, VkPipeline pipeline );
void     VK_Draw_BindSet( VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet set,
						  uint32_t dynamicOffset, uint32_t dynamicCount );
qboolean VK_Draw_BindTexture( VkCommandBuffer cmd, VkImageView view, VkSampler sampler );
extern	 int	vk_2dWindow[4];		/* x y w h of the 2D window in force */
extern	 float	vk_2dOrigin[2];	/* the widget's placement in that window */
void     VK_Draw_Set2DOrigin( float x, float y );
extern	 int	vk_2dScissor[4];	/* x y w h of the 2D scissor in force */
void     VK_Draw_Set2D( VkCommandBuffer cmd, int x, int y, int w, int h,
						float left, float right, float bottom, float top, float n, float f );
void     VK_Draw_SetScissor( VkCommandBuffer cmd, int x, int y, int width, int height );
/* The 2D state in force, and its re-application: a batch that spans a window or
   scissor change draws once per span, each with the state its quads were
   authored under (tr_draw.c RB_2D_Span*). */
qboolean VK_Draw_2DStateGet( float *ortho, int *win, int *scn );
void     VK_Draw_2DStateApply( VkCommandBuffer cmd, const float *ortho, const int *win, const int *scn );
qboolean VK_Draw_2DIssuedScissor( const int *scn );
void     VK_Draw_SetAlphaTest( VkCommandBuffer cmd, unsigned stateBits );
void     VK_Draw_DisableScissor( VkCommandBuffer cmd );
void     VK_Draw_DrawIndexed( VkCommandBuffer cmd, int indexCount, int firstIndex, int vertexBase );
void     VK_Draw_RecordedDraws( int *draws, int *indexes );
void     VK_Draw_ResetRecordedDraws( void );
void     VK_Draw_Draw( VkCommandBuffer cmd, int vertexCount, int firstVertex );
const float *VK_Draw_CurrentOrtho( void );
void     VK_Draw_Window2D( VkCommandBuffer cmd, int x, int y, int w, int h );
void     VK_Draw_View3D( VkCommandBuffer cmd, int x, int y, int w, int h );
void     VK_Draw_ViewportRect( VkCommandBuffer cmd, int x, int y, int w, int h,
							   float minDepth, float maxDepth );
int      VK_Draw_TopY( int y, int h );

/* The arena bake's copies: a command buffer of their own, submitted ahead of the
   frame's (a buffer copy may not be recorded inside a render pass), or
   VK_NULL_HANDLE when the walk baked nothing. */
VkCommandBuffer VK_VboStaticXferClose( int slot );

/* ---- textures ------------------------------------------------------------ */

/* One queued mip of one chain: the bytes sit in the chain's staging buffer and
   the copy runs when that buffer is flushed. */
typedef struct {
	int				level;
	int				width;
	int				height;
	VkDeviceSize	offset;
	size_t			bytes;
} vkPendingUpload_t;

typedef struct {
	VkImage		image;
	VkDeviceMemory memory;
	VkImageView	view;
	int			width, height;
	int			mipLevels;
	VkFormat	format;
	qboolean	inUse;
	/* The layout the image is in right now.  Every barrier this module records
	   declares this as its oldLayout and writes the new one back, so a
	   transition can never claim a layout the image is not in (Vulkan 1.4
	   s7.9.2: `oldLayout` must match the layout the image is actually in, or be
	   VK_IMAGE_LAYOUT_UNDEFINED). */
	VkImageLayout layout;
	/* a chain's uploads are staged, not submitted: the epoch holds the bytes, the queue names the copies, the flush runs both.  Two lifetimes
	   with separate owners; a record is never queued without a mapped buffer behind it, and one thread queues a chain at a time, which stagingOwner names. */
	VkBuffer			staging;
	VkDeviceMemory		stagingMemory;
	byte				*stagingMapped;
	VkDeviceSize		stagingSize;
	VkDeviceSize		stagingUsed;
	/* The thread that opened the epoch.  0 is no epoch; a flush from any other
	   thread is refused whole instead of freeing a map that thread is writing. */
	unsigned long		stagingOwner;
	vkPendingUpload_t	*pending;
	int					pendingCount;
	int					pendingCap;
	/* armed by the queue: a refilled chain is sampled until its copies run, so
	   the flush parks the walk before its submit */
	qboolean			pendingQuiesce;
} vkTexture_t;

#define VK_MAX_TEXTURES MAX_DRAWIMAGES

VkFormat VK_Texture_FormatFor( int internalFormat, qboolean *supported );
qboolean VK_Texture_ReadPixels( int handle, unsigned char *out, size_t outBytes );
qboolean VK_Texture_Create( int width, int height, int mipLevels, VkFormat format, int *handleOut );
qboolean VK_Texture_Upload( int handle, int mipLevel, int width, int height,
							const void *data, size_t bytes );
qboolean VK_Texture_Flush( int handle );
qboolean VK_Texture_Publish( int handle );
qboolean VK_Texture_ReadTexel( int handle, int x, int y, unsigned char out[4], int *blocked );
qboolean VK_Texture_ReadBlock( int handle, int x, int y, unsigned char out[4],
							   unsigned char blockOut[16], int *blocked );
void     VK_Texture_MakeMips( int handle );
void     VK_Texture_Destroy( int handle );
VkImageView VK_Texture_View( int handle );
qboolean VK_Texture_Sampler( qboolean clamp, qboolean mipmap, qboolean linear, VkSampler *out );
void     VK_Texture_FramePurge( void );
int      VK_Texture_Count( void );
int      VK_Texture_BytesUsed( void );

/* the legacy internal-format constants the loaders report, restated so the
   texture table maps them without that header */
#define VK_TF_RGB				3
#define VK_TF_RGBA				4
#define VK_TF_RGBA8				0x8058
#define VK_TF_RGB8				0x8051
#define VK_TF_RGB5_A1			0x8057
#define VK_TF_RGBA4				0x8055
#define VK_TF_RGB5				0x8050
/* LUMINANCE8 (0x8040), not RGB5: they were written with one value, so a
   luminance request resolved as RGB5 */
#define VK_TF_LUM8				0x8040
#define VK_TF_DXT1				0x83F0
#define VK_TF_DXT3				0x83F2
#define VK_TF_DXT5				0x83F3
/* the two block formats this module's own transcoder emits: a transcode chain
   is not the S3TC payload it came from, and internalFormat says what was
   uploaded */
#define VK_TF_ETC2_RGBA8		0x9278
#define VK_TF_ASTC_4x4			0x93B0

/* the wrap decisions an image carries in wrapClampModeX/Y: repeat unless the
   shader asked for a clamp */
#define VK_TWRAP_REPEAT				10497
#define VK_TWRAP_CLAMP				10496
#define VK_TWRAP_CLAMP_EDGE			33071

/* ---- the frame's 2D/3D target state ------------------------------------- */

void VK_Frame_RecordDraws( int slot );

#ifdef __cplusplus
}
#endif

#endif
