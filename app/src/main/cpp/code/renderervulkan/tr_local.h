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
 * Everything here is the API-neutral half of the GLES3 renderer's own local
 * header: the shader/image/tessellation structures carry the same fields with
 * the same meanings, so the parsers that fill them (tr_shader.c, tr_image.c,
 * tr_font.cpp) are the same code in both modules.  What differs is only the
 * substrate the structures describe: `image_t::texnum` is an index into this
 * module's texture table rather than a GL texture object name, and the
 * back-end state records the frame's Vulkan command buffer instead of a GL
 * context.  No GL header is included and no gl*() symbol is named anywhere in
 * this module (the GLES3 twin's GL typedefs come in through qgl.h, which this
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

typedef unsigned int glIndex_t;

#define	SMP_FRAMES		2

#define	MAX_SHADERS				16384
#define MAX_STATES_PER_SHADER 32
#define MAX_STATE_NAME 32

#define	MAX_MOD_KNOWN	1024
#define	MAX_DRAWIMAGES			2048
#define	MAX_SKINS				1024

/* ---- the shader state bits (GLES3's same encoding; the back end maps them
 *      onto the equivalent Vulkan fixed-function state) -------------------- */
#define GLS_SRCBLEND_ZERO						0x00000001
#define GLS_SRCBLEND_ONE						0x00000002
#define GLS_SRCBLEND_DST_COLOR					0x00000003
#define GLS_SRCBLEND_ONE_MINUS_DST_COLOR		0x00000004
#define GLS_SRCBLEND_SRC_ALPHA					0x00000005
#define GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA		0x00000006
#define GLS_SRCBLEND_DST_ALPHA					0x00000007
#define GLS_SRCBLEND_ONE_MINUS_DST_ALPHA		0x00000008
#define GLS_SRCBLEND_ALPHA_SATURATE				0x00000009
#define		GLS_SRCBLEND_BITS					0x0000000f

#define GLS_DSTBLEND_ZERO						0x00000010
#define GLS_DSTBLEND_ONE						0x00000020
#define GLS_DSTBLEND_SRC_COLOR					0x00000030
#define GLS_DSTBLEND_ONE_MINUS_SRC_COLOR		0x00000040
#define GLS_DSTBLEND_SRC_ALPHA					0x00000050
#define GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA		0x00000060
#define GLS_DSTBLEND_DST_ALPHA					0x00000070
#define GLS_DSTBLEND_ONE_MINUS_DST_ALPHA		0x00000080
#define		GLS_DSTBLEND_BITS					0x000000f0

#define GLS_DEPTHMASK_TRUE						0x00000100
#define GLS_POLYMODE_LINE						0x00000200
#define GLS_DEPTHTEST_DISABLE					0x00000400
#define GLS_DEPTHFUNC_EQUAL						0x00000800

#define GLS_CLAMP_EDGE							0x00001000
#define GLS_MULTITEXTURE						0x00002000
#define GLS_MULTITEXTURE_ENV					0x00004000
#define GLS_FOG									0x00008000

#define GLS_CUBEMAP								0x00010000

#define GLS_ATEST_GT_0							0x10000000
#define GLS_ATEST_LT_80							0x20000000
#define GLS_ATEST_GE_80							0x40000000
#define GLS_ATEST_BITS							0x70000000

#define GLS_DEFAULT			GLS_DEPTHMASK_TRUE

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

#define LIGHTMAP_2D			-4
#define LIGHTMAP_BY_VERTEX	-3
#define LIGHTMAP_WHITEIMAGE	-2
#define	LIGHTMAP_NONE		-1

#define	LIGHTMAP_BASE_MASK	( DYNAMIC_LIGHTMAP_BIT - 1 )
#define MAX_LIGHTMAPS		256

/* the multitexture environments the shader parser records, named in the
   module's own terms; the back end maps the one it implements */
#define VK_TENV_ADD      0x0104
#define VK_TENV_MODULATE 0x2100
#define VK_TENV_DECAL    0x2101
#define VK_TENV_REPLACE  0x1E01
#define VK_TENV_BLEND    0x0BE2

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

#define	FUNCTABLE_SIZE		2048
#define	FUNCTABLE_SIZE2		11
#define	FUNCTABLE_MASK		(FUNCTABLE_SIZE-1)

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
	texMod_t	type;

	waveForm_t	wave;

	float		matrix[2][2];
	float		translate[2];

	float		scale[2];

	float		rate[2];

	float		scroll[2];

	float		rotateSpeed;
	float		rotateStart;
	float		rotateCoef;

} texModInfo_t;

/* ---- images --------------------------------------------------------------
 * `image_t`, `imgType_t` and `imgFlags_t` come from renderercommon/tr_common.h,
 * the same header the GLES3 twin's local header includes them from, so the two
 * renderers carry one image model.  `texnum` is a GL object name there and this
 * module's texture-table index here; nothing else differs. */

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
} textureBundle_t;

#define NUM_TEXTURE_BUNDLES 2

typedef struct {
	qboolean		active;

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
	int			noMerge;
	void		(*optimalStageIteratorFunc)( void );
	struct shader_s *remappedShader;
	char		forcedAlphaShader[64];
	int			forcedAlphaKey;
	struct	shader_s	*next;

	int			flags;
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

typedef struct shaderState_s {
  char shaderName[MAX_QPATH];
  char name[MAX_STATE_NAME];
  char stateShader[MAX_QPATH];
  int cycleTime;
  shader_t *shader;
} shaderState_t;


/* the skin and ghost surfaces the image layer carries */
typedef struct {
	char		name[MAX_QPATH];
	shader_t	*shader;
} skinSurface_t;

typedef struct skin_s {
	char		name[MAX_QPATH];
	int			numSurfaces;
	skinSurface_t	*surfaces[MD3_MAX_SURFACES];
} skin_t;

/* ---- the tessellation store ----------------------------------------------
 * The front end fills these arrays exactly as the GLES3 front end does; the
 * back end reads them out into the frame's vertex ring. */

typedef byte color4ub_t[4];

typedef struct stageVars
{
	color4ub_t	colors[SHADER_MAX_VERTEXES];
	vec2_t		texcoords[NUM_TEXTURE_BUNDLES][SHADER_MAX_VERTEXES];
} stageVars_t;

typedef struct shaderCommands_s
{
	glIndex_t	indexes[SHADER_MAX_INDEXES] Q_ALIGN(16);
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
} fontString2DCommand_t;

/* The 2D (overlay) commands.  Under r_smp the tr_draw.c entry points append
 * one of these instead of recording into the frame's command buffer on the
 * front-end thread; the render thread runs the same body the single-threaded
 * path runs.  Every field is resolved on the recording thread so the handler
 * never reads front-end state. */
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
	float		startTime;
} shaderTime2DCommand_t;

typedef struct {
	int			commandId;
	int			x, y, w, h;
	float		left, right, bottom, top, n, f;
} setWindow2DCommand_t;

typedef struct {
	int			commandId;
	int			x, y, width, height;
} scissor2DCommand_t;

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
	RC_2D_FONT_STRING,
	RC_2D_SHADER_TIME,
	RC_VK_BEGIN,
	RC_VK_PRESENT
} renderCommand_t;



extern	shaderCommands_t	tess;

void RB_BeginSurface(shader_t *shader, int fogNum);
void RB_EndSurface(void);
void RB_CheckOverflow( int verts, int indexes );
#define RB_CHECKOVERFLOW(v,i) if (tess.numVertexes + (v) >= SHADER_MAX_VERTEXES || tess.numIndexes + (i) >= SHADER_MAX_INDEXES ) {RB_CheckOverflow(v,i);}

/* ---- the 2D immediate stream --------------------------------------------
 * Boxes and line loops draw through the same ring the tess surfaces use; the
 * GLES3 twin keeps a private GL stream for them, this module keeps a private
 * slice of the same vertex ring. */

void RB_ImmBegin( int topology );
void RB_ImmBeginWithImage( int topology, image_t *image );
void RB_ImmCommit( void );
void RB_ImmEnd( void );
void RB_ImmEndDeferred( void );
void RB_ImmFlush( void );
int  RB_ImmStreamVerts( void );
void RB_ImmVertex2f( float x, float y );
void RB_ImmColor4f( float r, float g, float b, float a );
void RB_ImmTexCoord2f( float s, float t );

/* primitive topologies, named for the record and mapped to Vulkan's enum */
#define VK_IMM_TRIANGLES   0
#define VK_IMM_LINES       1
#define VK_IMM_LINE_LOOP   2
#define VK_IMM_TRIANGLE_STRIP 3

/* ---- back-end globals ---------------------------------------------------- */

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

typedef struct {
	int			smpFrame;
	int			currentEntityNum;
	int			currentSpriteNum;
	int			shiftedEntityNum;
	int			dsStreamVert;
	qboolean	in2D;
	byte		color2D[4];
	qboolean	vertexes2D;
	int			backEndMsec;
	float		shaderStartTime;
	backEndCounters_t	pc;
	int			refdefTime;
	float		refdefFloatTime;
} backEndState_t;

typedef struct {
	qboolean	registered;
	qboolean	registrating;

	int			visCount;
	int			frameCount;
	int			sceneCount;
	int			viewCount;
	int			smpFrame;
	int			frameSceneNum;

	image_t		*defaultImage;
	image_t		*scratchImage;
	image_t		*whiteImage;
	image_t		*identityLightImage;
	image_t		*dlightImage;
	image_t		*fogImage;

	int			overbrightShift;
	float		overbrightMult;

	int			numSkins;
	struct skin_s	*skins[MAX_SKINS];

	int			numLightmaps;
	int			numBaseLightmaps;
	image_t		*lightmaps[MAX_LIGHTMAPS];
	image_t		*baseLightmaps[MAX_LIGHTMAPS];

	shader_t	*shadowShader;
	shader_t	*projectionShadowShader;
	shader_t	*flareShader;
	shader_t	*sunShader;

	vec3_t		sunLight;
	vec3_t		sunDirection;

	shader_t	**currentShader;

	shader_t	*defaultShader;

	int			numImages;
	image_t		images[MAX_DRAWIMAGES];

	int			numShaders;
	shader_t	*shaders[MAX_SHADERS];
	shader_t	*sortedShaders[MAX_SHADERS];

	float		identityLight;
	int			identityLightByte;
	int			overbrightBits;
	int			needsLightScale;

	frontEndCounters_t	pc;

	float		sinTable[FUNCTABLE_SIZE];
	float		squareTable[FUNCTABLE_SIZE];
	float		triangleTable[FUNCTABLE_SIZE];
	float		sawToothTable[FUNCTABLE_SIZE];
	float		inverseSawToothTable[FUNCTABLE_SIZE];

	qboolean	shadersParsed;
	int			rendererhandle;
} trGlobals_t;

extern backEndState_t	backEnd;
extern trGlobals_t	tr;
extern glconfig_t	glConfig;
extern int r_sequencenumber;

/* ---- images -------------------------------------------------------------- */

image_t     *R_FindImageFile( const char *name, imgType_t type, imgFlags_t flags );
image_t     *R_RefreshImageFile( const char *name, imgType_t type, imgFlags_t flags );
image_t     *R_CreateImage( const char *name, byte *pic, int width, int height,
							imgType_t type, imgFlags_t flags, int internalFormat );
void		R_ShutdownImages( void );
void		R_InitImages( void );

int			RE_GetShaderWidth( qhandle_t hShader );
int			RE_GetShaderHeight( qhandle_t hShader );

/* ---- shaders ------------------------------------------------------------- */

qhandle_t	 RE_RegisterShaderLightMap( const char *name, int lightmapIndex );
qhandle_t	 RE_RegisterShader( const char *name );
qhandle_t	 RE_RegisterShaderNoMip( const char *name );
qhandle_t	 RE_RefreshShaderNoMip( const char *name );
qhandle_t	 RE_RegisterShaderFromImage(const char *name, int lightmapIndex, image_t *image, qboolean mipRawImage);
shader_t	*R_FindShader( const char *name, int lightmapIndex, qboolean mipRawImage, qboolean picmip,
						   qboolean allowPicmip, qboolean compress );
shader_t	*R_GetShaderByHandle( qhandle_t hShader );
void		R_ShutdownShaders( void );
void		R_InitShaders( void );
void		R_LoadShaderStates( void );
void		R_SetGlobalColour( float r, float g, float b );
void		R_ChangeTextColor( int index );
void		R_SetFontHeightScale( float scale );
void		R_SetFontScale( float scale );
void		R_SetFontZ( float zed );

/* ---- fonts --------------------------------------------------------------- */

void R_ShutdownFont(void);
fontheader_t* R_LoadFont(const char* name);
void R_LoadFontShader(fontheader_sgl_t* font);
void R_DrawString(fontheader_t* font, const char* text, float x, float y, int maxlen, const float *pvVirtualScreen);
float R_GetFontHeight(const fontheader_t* font);
float R_GetFontStringWidth(const fontheader_t* font, const char* s);

/* ---- the cvars the data-model layer reads -------------------------------- */

extern cvar_t	*r_picmip;
extern cvar_t	*r_roundImagesDown;
extern cvar_t	*r_colorMipLevels;
extern cvar_t	*r_texturebits;
extern cvar_t	*r_colorbits;
extern cvar_t	*r_draw2d;
extern cvar_t	*r_vertexLight;
extern cvar_t	*r_ignoreFastPath;
extern cvar_t	*r_developer;
extern cvar_t	*r_printShaders;
extern cvar_t	*r_overBrightBits;
extern cvar_t	*r_mapOverBrightBits;
extern cvar_t	*r_intensity;
extern cvar_t	*r_gamma;
extern cvar_t	*r_detailtextures;

void R_RegisterCvars( void );

/* ---- the CPU-side instruments the data-model layer reads ------------------
 * The GLES3 twin rolls a per-call CPU cost into a BE_/SMP_RLOAD ledger; this
 * module's instruments are the [VK:*] families, so the same call sites keep a
 * clock and a gate but the ledger is the one this module prints. */
qboolean R_ProbeStatsOn( void );
int      BE_Microseconds( void );
qboolean BE_On( void );

#define SMP_RLOAD_BEGIN(z) \
	int smp_ra_##z = 0; \
	( void )smp_ra_##z
#define SMP_RLOAD_END(z) \
	do { } while ( 0 )

/* the transcoded-texture decision: the loaders ask, the device probe answers.
   Desktop defaults to none; the Android leg arms it from the device's own
   ASTC/ETC2 support. */
extern int       smp_texTranscode;
extern qboolean  smp_hasAstcLdr;
extern qboolean  smp_hasAstcSliced3d;
extern qboolean  smp_hasEtc2Core;

void R_FreeImage( image_t *image );
void R_UpdateGhostTextures(void);
void R_SetGhostImage(const char* name, image_t* image);
void LoadGHOST(const char* name, byte** pic, int* width, int* height);
void R_DeleteTextures( void );
void R_SetColorMappings( void );
void R_BuildGammaTables( void );
void R_CreateBuiltinImages( void );
void R_GammaCorrect( byte *buffer, int bufSize );
void R_LoadBMP( const char *name, byte **pic, int *width, int *height );
void R_LoadTGA( const char *name, byte **pic, int *width, int *height );
qhandle_t RE_RegisterSkin( const char *name );
skin_t	*R_GetSkinByHandle( qhandle_t hSkin );
void	R_SkinList_f( void );
void	R_DumpTextureMemory(void);
shader_t *R_FindShaderByName( const char *name );
image_t *R_FindImageFileOld( const char *name, qboolean mipmap, qboolean allowPicmip,
							 qboolean force32bit, int textureMode );
image_t *R_RefreshImageFileOld( const char *name, qboolean mipmap, qboolean allowPicmip,
								qboolean force32bit, int textureMode );
void	 R_InitSkyTexCoords( float cloudLayerHeight );
void	 R_BuildCloudData( shaderCommands_t *shader );
void	 RB_StageIteratorGeneric( void );
void	 RB_StageIteratorSky( void );
void	 RB_StageIteratorVertexLitTextureUnfogged( void );
void	 RB_StageIteratorLightmappedMultitextureUnfogged( void );
void	 RB_GLFogStageIteratorFunc( void );
void	 RB_ShadowTessEnd( void );
void	 RB_ShadowFinish( void );
void	 RB_ProjectionShadowDeform( void );
void	 RB_DeformTessGeometry( void );
void	 ProjectDlightTexture( void );
void	 RB_AddQuadStampExt( vec3_t origin, vec3_t left, vec3_t up, byte *color, float s1, float t1, float s2, float t2 );
void	R_TotalTextureMemInfo_f( void );
void	R_ImageList_f( void );
void	R_ShaderList_f( void );
void	SMP_ShaderDefaultProbe( const char *kind, const char *requested, const char *source, int a, int b );
shader_t*R_FindShaderForceAlpha( const char *name );
shader_t*R_FindShaderForceAlphaLM( const char *name, int lightmapIndex );
void	R_SetupShaders( void );
void	R_StartupShaders( void );
void	R_LoadShaderStates( void );
extern qboolean haveClampToEdge;
extern float	displayAspect;
void	R_DeleteImageFromHash( image_t *image );
void	R_FreeUnusedImages( void );

/* the image layer's own helpers, defined at the bottom of tr_image.c */
int      getGLClampType( int wrapAxis, int textureMode );
void     verifyProperTextureSize( const char *name, int width, int height );
void     GL_SetTextureAnisotropicFiltering( int target );
void     GL_SetupAnisotropicFiltering( void );
void     GL_TextureMode( const char *string );
int      R_SumOfUsedImages( void );
int      R_CountTextureMemory( void );
qboolean R_ImageExists( const char *name );
qboolean R_LoadRawImage( const char *name, byte **pic, int *width, int *height );
void     R_FreeRawImage( byte *pic );
void     RE_RegisterFont( const char *fontName, int pointSize, fontInfo_t *font );

/* ---- the 2D front end (tr_draw.c) --------------------------------------- */

void Draw_SetColor(const vec4_t rgba);
void Draw_StretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);
void Draw_StretchPic2(float x, float y, float w, float h, float s1, float t1, float s2, float t2, float sx, float sy, qhandle_t hShader);
void Draw_TilePic(float x, float y, float w, float h, qhandle_t hShader);
void Draw_TilePicOffset(float x, float y, float w, float h, qhandle_t hShader, int offsetX, int offsetY);
void Draw_TrianglePic(const vec2_t vPoints[3], const vec2_t vTexCoords[3], qhandle_t hShader);
void AddBox(float x, float y, float w, float h);
void DrawBox(float x, float y, float w, float h);
void DrawLineLoop(const vec2_t* points, int count, int stipple_factor, int stipple_mask);
void Set2DWindow(int x, int y, int w, int h, float left, float right, float bottom, float top, float n, float f);
void RE_Scissor(int x, int y, int width, int height);
void Set2DInitialShaderTime(float startTime);
void RB_2D_Flush( void );
void R_UploadScratchImage( int cols, int rows, const byte *rgb );
void RB_Texcoord2f( float s, float t );
void RB_Vertex2f( vec_t x, vec_t y );
void RB_Color4bv( unsigned char *colors );
void RB_StreamEnd( void );
qboolean VKimp_GLMainOwnsContext( void );
qboolean RB_RecordingFrame( void );
int BE_Microseconds( void );
void RB_2D_OwnersPurged( void );
void RB_2D_SetColor( const byte *rgba );
void RB_2D_StretchPic( shader_t *shader, float x, float y, float w, float h, float s1, float t1, float s2, float t2 );
void RB_2D_StretchPic2( shader_t *shader, float x, float y, float w, float h, float s1, float t1, float s2, float t2, float sx, float sy );
void RB_2D_TilePic( shader_t *shader, float x, float y, float w, float h, float picw, float pich );
void RB_2D_TilePicOffset( shader_t *shader, float x, float y, float w, float h, float picw, float pich, int offsetX, int offsetY );
void RB_2D_TrianglePic( shader_t *shader, const vec2_t vPoints[3], const vec2_t vTexCoords[3] );
void RB_2D_Box( float x, float y, float w, float h, qboolean blend );
void RB_2D_LineLoop( const vec2_t *points, int count, int stipple_factor, int stipple_mask );
void RB_2D_SetWindow( int x, int y, int w, int h, float left, float right, float bottom, float top, float n, float f );
void RB_2D_Scissor( int x, int y, int width, int height );
void RB_2D_ShaderTime( float startTime );
void D2_StringBegin( int *t0 );
void D2_StringEnd( int t0 );
void RB_FontRunDraw( fontString2DCommand_t *cmd, const fontGlyph_t *glyphs );
void R_FlushRenderCommands( void );
void R_IssuePendingRenderCommands( void );
void *R_GetCommandBuffer( int bytes );
void *R_GetCommandBufferReserved( int bytes, int reservedBytes );
qboolean R_PendingRenderCommands( void );
void R_Invalidate2DWindowCache( void );

/* ---- the back end (tr_backend.c) ---------------------------------------- */

void RB_InitBackend( void );
void RB_RecordFramePrologue( void );
qboolean RB_RecordSurfaces( VkCommandBuffer cmd );
void RB_FramePrologue( void );

/* ---- the vertex ring ----------------------------------------------------- */

typedef struct {
	float	xyz[3];
	float	st[2];
	byte	color[4];
} vkTessVertex_t;

#define VK_TESS_VERTEX_BYTES ( (int)sizeof( vkTessVertex_t ) )
#define VK_RING_MAX_VERTEXES ( SHADER_MAX_VERTEXES * 8 )
#define VK_RING_MAX_INDEXES  ( SHADER_MAX_INDEXES * 8 )

qboolean VK_VertexRing_Init( void );
void     VK_VertexRing_Shutdown( void );
void     VK_VertexRing_ResetFrame( void );
vkTessVertex_t *VK_VertexRing_Reserve( int vertexes, int *baseOut );
glIndex_t *VK_VertexRing_ReserveIndexes( int indexes, int *baseOut );
void     VK_VertexRing_RollBack( int vertexes, int indexes );
vkTessVertex_t *VK_VertexRing_Tail( int back );
void     vk_ring_note( int vertexHigh, int indexHigh );
VkBuffer  VK_VertexRing_Buffer( void );
VkBuffer  VK_VertexRing_IndexBuffer( void );
int       VK_VertexRing_Used( void );
int       VK_VertexRing_Flushed( void );

/* ---- pipelines ----------------------------------------------------------- */

typedef struct {
	int			topology;			/* VK_IMM_* */
	unsigned	stateBits;			/* the stage's GLS_* tuple */
	cullType_t	cull;
	qboolean	depthTest;
	qboolean	depthWrite;
	qboolean	textured;			/* samples the stage bundle, or paints flat */
} vkPipelineKey_t;

qboolean VK_Draw_Init( void );
void     VK_Draw_Shutdown( void );
VkPipeline VK_Draw_Pipeline( const vkPipelineKey_t *key );
qboolean VK_Draw_BindTexture( VkCommandBuffer cmd, VkImageView view, VkSampler sampler );
void     VK_Draw_Set2D( VkCommandBuffer cmd, int x, int y, int w, int h,
						float left, float right, float bottom, float top, float n, float f );
void     VK_Draw_SetScissor( VkCommandBuffer cmd, int x, int y, int width, int height );
void     VK_Draw_DisableScissor( VkCommandBuffer cmd );
void     VK_Draw_DrawIndexed( VkCommandBuffer cmd, int indexCount, int firstIndex, int vertexBase );
void     VK_Draw_Draw( VkCommandBuffer cmd, int vertexCount, int firstVertex );

/* ---- textures ------------------------------------------------------------ */

typedef struct {
	VkImage		image;
	VkDeviceMemory memory;
	VkImageView	view;
	int			width, height;
	int			mipLevels;
	VkFormat	format;
	qboolean	inUse;
} vkTexture_t;

#define VK_MAX_TEXTURES MAX_DRAWIMAGES

qboolean VK_Texture_Init( void );
void     VK_Texture_Shutdown( void );
VkFormat VK_Texture_FormatFor( int internalFormat, qboolean *supported );
qboolean VK_Texture_Create( int width, int height, int mipLevels, VkFormat format, int *handleOut );
qboolean VK_Texture_Upload( int handle, int mipLevel, int width, int height,
							const void *data, size_t bytes );
void     VK_Texture_MakeMips( int handle );
void     VK_Texture_Destroy( int handle );
VkImageView VK_Texture_View( int handle );
qboolean VK_Texture_Sampler( qboolean clamp, qboolean mipmap, qboolean linear, VkSampler *out );
void     VK_Texture_FramePurge( void );
int      VK_Texture_Count( void );
int      VK_Texture_BytesUsed( void );

/* the GL internal-format constants the loaders report, restated here so the
   texture table can map them without naming a GL header */
#define VK_TF_RGB				3
#define VK_TF_RGBA				4
#define VK_TF_RGBA8				0x8058
#define VK_TF_RGB8				0x8051
#define VK_TF_RGB5_A1			0x8057
#define VK_TF_RGBA4				0x8055
#define VK_TF_RGB5				0x8050
#define VK_TF_LUM8				0x8050
#define VK_TF_DXT1				0x83F0
#define VK_TF_DXT3				0x83F2
#define VK_TF_DXT5				0x83F3

/* ---- the frame's 2D/3D target state ------------------------------------- */

void VK_Frame_RecordDraws( int slot );

#ifdef __cplusplus
}
#endif

#endif
