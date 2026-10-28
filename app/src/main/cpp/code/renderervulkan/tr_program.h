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
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of a retail-exact decompilation port of Star Trek: Elite
Force II.  It is the successor to the fixed-function pipeline
pipeline, forked from the fixed-function renderer's sources.

PROGRAM / STATE LAYER -- the replacement for the fixed-function pipeline.

WHY THIS FILE EXISTS
--------------------
The fixed-function predecessor (OpenMoHAA's fixed-function backend) drove a fixed-function
pipeline: per stage it calls VK_State(pStage->stateBits), binds 1 or 2 texture
bundles, and hands the fixed-function unit a per-vertex colour array computed
on the CPU by tr_shade_calc.c.  GLES 3.0 has none of that: there is no colour
array, no texture env, no vkAlphaFunc, no fog, no polygon mode and no
fixed-function matrix stack, so every one of those arms has to become an
explicit program permutation plus a render-state object.

This header declares the mapping.  The implementation template is the idTech3
program-driven backend (OpenMoHAA's renderergl2), whose shared program source
shows the two-axis split this file uses:

  * PROGRAM axis -- a small, enumerable set of compiled programs, selected by
    a #define bitmask (GLP_RGBAGEN, GLP_TCGEN0, GLP_TCGEN1, GLP_TCMOD0,
    VKP_TCMOD1, VKP_FOG, VKP_LIGHTMAP, VKP_VERTEXLIT, VKP_2D, ...).
    This is the axis that costs a shader compile, so it is kept small.

  * UNIFORM axis -- the per-stage generator arm, selected by integer uniforms
    (u_ColorGen = pStage->rgbGen, u_AlphaGen = pStage->alphaGen,
    u_TCGen0/1 = bundle[b].tcGen) plus their parameters.  This is retail's own
    dispatch: the fixed-function renderer's tr_shade.c ComputeColors() switches on exactly
    these ordinals, so the uniform values are the SAME integers it switches
    on and no translation table is needed (renderergl2 tr_local.h:649-657
    carries UNIFORM_TCGEN0 / UNIFORM_COLORGEN / UNIFORM_ALPHAGEN for the same
    reason).

The exact arithmetic of every arm lives in the reference renderer's shared include; every
function there cites the tr_shade_calc.c line range it mirrors.

STATE -> ENUMERABLE STATE DEFS
------------------------------
The state word is vkStateBits (tr_local.h:2418-2492).  this module renders it as
a lookup into gles3StateDefs[] (one row per distinct word the shipped corpus
carries, tr_program.c) plus VK_ApplyStateBits(), the replacement for
VK_State() (tr_backend.c:263 in the fixed-function renderer).  Every state word the
shipped shaders carry must map to one of those rows; a shipped stage whose
state word has no row renders undefined.

TWO WRITERS, ONE LOCK-FREE LAYER -- THE STATE OF THIS LAYER
----------------------------------------------------------
r_smp is forced on, so a render thread does walk this module; nothing here takes
a lock, and the single-EXECUTOR rule below is what keeps that correct.  The earlier claim that "nothing in this layer ...
assumes a second writer" is NOT true of the code as it stands, and the two-writer
audit says exactly where: VK_ApplyStateBits
writes the module static s_currentAlphaTest and is reached from BOTH the back end
(VK_State, tr_backend.c) and the front end's immediate 2D path (tr_draw.c);
VK_SelectTexture writes vkState.currenttmu from both; and VK_GetProgram
lazily compiles into gles3Programs[] / gles3NumPrograms / s_programsInitialised
with no guard.  Making that true is the audit's work list: build every
program during registration and give the tracked-state writes one owner per
frame.  Until then the layer is single-EXECUTOR, which is what keeps it correct.
==========================================================================
*/

#include "tr_vbo.h"

#ifndef __TR_PROGRAM_H__
#define __TR_PROGRAM_H__

#include "tr_local.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VKP_RGBAGEN     0x00000001u  
#define VKP_TCGEN0      0x00000002u  
#define VKP_TCGEN1      0x00000004u  
#define VKP_TCMOD0      0x00000008u  
#define VKP_TCMOD1      0x00000010u  
#define VKP_FOG         0x00000020u  
#define VKP_LIGHTMAP    0x00000040u  
#define VKP_VERTEXLIT   0x00000080u  
#define VKP_LIGHTMAP_MT 0x00000100u  
/* 0x00000200 .. 0x00100000 are the per-arm selectors the programs read; they
 * are derived, not free: VK_StageDefines() is the single writer. */
#define VKP_DLIGHT      0x00000200u  
#define VKP_2D          0x00000400u  
#define VKP_SKY         0x00000800u  
#define VKP_ALPHATEST   0x00001000u  
#define VKP_SKIP_COLOR  0x00002000u  
#define VKP_NORMAL      0x00004000u  
#define VKP_WAVE        0x00008000u  
#define VKP_SPECULAR    0x00010000u  
#define VKP_ENTITY_SPACE 0x00020000u 
#define VKP_DEFORM      0x00040000u  
#define VKP_SHADOW      0x00080000u  
#define VKP_HEIGHTFOG   0x00100000u  

#define VKP_TEXENV_ADD         0x00200000u
#define VKP_TEXENV_INTERPOLATE 0x00400000u
#define VKP_TEXENV_COMBINE4    0x00800000u

#define VKP_FAMILY_MASK     0xff000000u
#define VKP_FAMILY_STAGE    0x00000000u  
#define VKP_FAMILY_2D       0x01000000u  
#define VKP_FAMILY_DLIGHT   0x02000000u  
#define VKP_FAMILY_SKY      0x04000000u  
#define VKP_FAMILY_SPRITE   0x08000000u  
#define VKP_FAMILY_SUNFLARE 0x10000000u  
#define VKP_FAMILY_SHADOW   0x20000000u  
#define VKP_FAMILY_WIRE     0x40000000u  

#define VKP_MAX_PROGRAMS 512

typedef struct {
	unsigned		stateMask;		
	unsigned		stateValue;		
	const char		*name;			
} vkStateDef_t;

extern const vkStateDef_t	vkStateDefs[];
extern const int				vkNumStateDefs;

typedef struct {
	unsigned		mask;
	unsigned		value;
	const char		*name;
} vkStateBitArm_t;

extern const vkStateBitArm_t	vkStateBitArms[];
extern const int				vkNumStateBitArms;

typedef struct {
	unsigned		rgbGen;			
	unsigned		alphaGen;		
	unsigned		tcGen0;			
	unsigned		tcGen1;			
	unsigned		bundleCount;	
	unsigned		texEnv;			
	unsigned		fogStage;		
	unsigned		atest;			
	unsigned		defines;		
	const char		*program;		
	const char		*label;			
} vkProgramKey_t;

extern const vkProgramKey_t	vkProgramKeys[];
extern const int				vkNumProgramKeys;

typedef struct {
	unsigned		gen;
	unsigned		value;
	const char		*symbol;		
	const char		*label;			
} vkGenArm_t;

extern const vkGenArm_t		vkGenArms[];
extern const int				vkNumGenArms;

typedef struct {
	unsigned		type;			
	const char		*symbol;
	const char		*label;
} vkTcModArm_t;

extern const vkTcModArm_t	vkTcModArms[];
extern const int				vkNumTcModArms;

typedef struct {
	unsigned		acff;			
	const char		*symbol;
	const char		*label;
} vkAcffArm_t;

extern const vkAcffArm_t		vkAcffArms[];
extern const int				vkNumAcffArms;

typedef struct {
	const char		*family;		
	const char		*program;
	const char		*label;
} vkFastPath_t;

extern const vkFastPath_t	vkFastPaths[];
extern const int				vkNumFastPaths;

typedef struct {
	const char		*name;			 
	unsigned		family;			
	const char		*vsName;		
	const char		*fsName;
} vkProgramDef_t;

extern const vkProgramDef_t	vkProgramDefs[];
extern const int				vkNumProgramDefs;

#define VK_UNIFORM_TABLE \
	X( MVP,               "u_MVP" )               \
	X( MODELVIEW,         "u_ModelView" )         \
	X( MODELMATRIX,       "u_ModelMatrix" )       \
	X( VIEWORIGIN,        "u_ViewOrigin" )        \
	X( VIEWAXIS0,         "u_ViewAxis0" )         \
	X( LOCALVIEWORIGIN,   "u_LocalViewOrigin" )   \
	X( ENTITYORIGIN,      "u_EntityOrigin" )      \
	X( ENTITYAXIS,        "u_EntityAxis" )        \
	X( ENTITYCOLOR,       "u_EntityColor" )       \
	X( BASECOLOR,         "u_BaseColor" )         \
	X( VERTCOLOR,         "u_VertColor" )         \
	X( IDENTITYLIGHT,     "u_IdentityLight" )     \
	X( IDENTITYLIGHTBYTE, "u_IdentityLightByte" ) \
	X( ENTITYFADE,        "u_EntityFade" )        \
	X( PORTALRANGE,       "u_PortalRange" )       \
	X( SPECORIGIN,        "u_SpecOrigin" )        \
	X( SUNDIRECTION,      "u_SunDirection" )      \
	X( SKYALPHA,          "u_SkyAlpha" )          \
	X( SKYSCROLL,         "u_SkyScroll" )         \
	X( SKYALPHAMODE,      "u_SkyAlphaMode" )      \
	X( ENTITYTEXCOORD,    "u_EntityTexCoord" )    \
	X( COLORGEN,          "u_ColorGen" )          \
	X( ALPHAGEN,          "u_AlphaGen" )          \
	X( ALPHAMIN,          "u_AlphaMin" )          \
	X( ALPHAMAX,          "u_AlphaMax" )          \
	X( ALPHACONST,        "u_AlphaConst" )        \
	X( ALPHACONSTMIN,     "u_AlphaConstMin" )     \
	X( WAVERGB,           "u_WaveRGB" )           \
	X( WAVEALPHA,         "u_WaveAlpha" )         \
	X( FREQRGB,           "u_FreqRGB" )           \
	X( FREQALPHA,         "u_FreqAlpha" )         \
	X( TCGEN0,            "u_TCGen0" )            \
	X( TCGEN1,            "u_TCGen1" )            \
	X( TCGEN0VEC0,        "u_TCGen0Vec0" )        \
	X( TCGEN0VEC1,        "u_TCGen0Vec1" )        \
	X( TCGEN1VEC0,        "u_TCGen1Vec0" )        \
	X( TCGEN1VEC1,        "u_TCGen1Vec1" )        \
	X( NUMTCMOD0,         "u_NumTexMod0" )        \
	X( NUMTCMOD1,         "u_NumTexMod1" )        \
	X( TCMOD0,            "u_TexMod0" )           \
	X( TCMOD1,            "u_TexMod1" )           \
	X( ALPHATEST,         "u_AlphaTest" )         \
	X( FORCEALPHA,        "u_ForceAlpha" )        \
	X( TEXENV0,           "u_TexEnv0" )           \
	X( TEXENV1,           "u_TexEnv1" )           \
	X( TEX0,              "u_Tex0" )              \
	X( TEX1,              "u_Tex1" )              \
	X( TEX1COLOR,         "u_Tex1Color" )         \
	X( WAVETABLE,         "u_WaveTable" )         \
	X( CLIPPLANE,         "u_ClipPlane" )         \
	X( CLIPPLANEENABLE,   "u_ClipPlaneEnable" )   \
	X( SHADERTIME,        "u_ShaderTime" )        \
	X( ACFF,              "u_Acff" )              \
	X( FOGNUM,            "u_FogNum" )            \
	X( FOGTCSCALE,        "u_FogTcScale" )        \
	X( FOGSCONSTANT,      "u_FogSConstant" )      \
	X( FOGSVECTOR,        "u_FogSVector" )        \
	X( FOGTCONSTANT,      "u_FogTConstant" )      \
	X( FOGTVECTOR,        "u_FogTVector" )        \
	X( FOGVIEWOUTSIDE,    "u_FogViewOutside" )    \
	X( FOGCOLOR,          "u_FogColor" )          \
	X( FOGDENSITYARM,     "u_FogDensityArm" )     \
	X( FOGOOLEN,          "u_FogOolen" )          \
	X( FOGPLANECONST,     "u_FogPlaneConst" )     \
	X( FOGDVIEW,          "u_FogDView" )          \
	X( FOGDENSITYTVECTOR, "u_FogDensityTVector" ) \
	X( FOGMODE,           "u_FogMode" )           \
	X( FOGSTART,          "u_FogStart" )          \
	X( FOGEND,            "u_FogEnd" )            \
	X( FOGFIXCOLOR,       "u_FogFixColor" )       \
	X( FLATMODE,          "u_FlatMode" )

typedef enum {
#define X( e, n ) VK_U_##e,
	VK_UNIFORM_TABLE
#undef X
	VK_U_COUNT
} vkUniform_t;

typedef struct {
	char			name[64];
	unsigned		defines;
	unsigned		program;		
	int				uniforms[VK_U_COUNT];
	qboolean		linked;
} vkProgram_t;

extern vkProgram_t	vkPrograms[VKP_MAX_PROGRAMS];
extern int				vkNumPrograms;

void		VK_ProgramsInit( void );
void		VK_ProgramsShutdown( void );
void		VK_PrebuildPrograms( void );
void		VK_CloseProgramRegistration( void );
int			VK_ProgramsCreatedAfterRegistration( void );
vkProgram_t *VK_GetProgram( unsigned defines );
vkProgram_t *VK_StageProgram( const shaderStage_t *pStage, const shader_t *shader, unsigned extraDefines );
vkProgram_t *VK_FamilyProgram( unsigned family );

unsigned	VK_StageDefines( const shaderStage_t *pStage, const shader_t *shader );
unsigned	VK_StageDefines_Full( const shaderStage_t *pStage, const shader_t *shader,
									  unsigned bundleCount, unsigned texEnv, unsigned fogStage,
									  unsigned extra );
unsigned	VK_TexEnvForStage( const shaderStage_t *pStage, const shader_t *shader );

void		VK_SetUniform1i( vkProgram_t *p, vkUniform_t u, int v );
void		VK_SetUniform1f( vkProgram_t *p, vkUniform_t u, float v );
void		VK_SetUniform2f( vkProgram_t *p, vkUniform_t u, float x, float y );
void		VK_SetUniform3f( vkProgram_t *p, vkUniform_t u, float x, float y, float z );
void		VK_SetUniform4f( vkProgram_t *p, vkUniform_t u, float x, float y, float z, float w );
void		VK_SetUniformMat4( vkProgram_t *p, vkUniform_t u, const float *m );
void		VK_SetUniform3fv( vkProgram_t *p, vkUniform_t u, const float *v );
void		VK_SetUniform4fv( vkProgram_t *p, vkUniform_t u, const float *v );

void		VK_Mat4_ClipToWindow( float *m );

void		VK_ApplyStateBits( unsigned long stateBits );
unsigned	VK_CurrentAlphaTest( void );

qboolean	VK_StageNeedsCPUColor( const shaderStage_t *pStage );
qboolean	VK_StageReadsVertexColor( const shaderStage_t *pStage );
qboolean	VK_StageNeedsCPUTexCoords( const shaderStage_t *pStage );

void		VK_SelectTexture( int unit );
void		VK_BindAnimatedImage( const textureBundle_t *bundle, int unit );

void		VK_SetupStageUniforms( vkProgram_t *p, const shaderStage_t *pStage, const shader_t *shader );
void		VK_SetupFogUniforms( vkProgram_t *p, int fogNum, qboolean fogDensityArm );
void		VK_SetupViewUniforms( vkProgram_t *p );
void		VK_DrawStage( const shaderCommands_t *input, const shaderStage_t *pStage,
							 const shader_t *shader, unsigned stateBits, float forceAlpha,
							  qboolean hudColorClobber );

/* Shared by every stage path that binds a bundle, so the lightmapped
   multitexture fast path reports with the rest */
void		VK_FullbrightNote( void );
void		VK_StageBindCensus( const shader_t *shader, const shaderStage_t *pStage,
								   int stageIndex, qboolean in2D,
								   unsigned w0, unsigned w1, unsigned u0, unsigned u1 );
void		VK_PassCensus( const shader_t *shader, const shaderStage_t *pStage,
						   unsigned stateBits, const struct shaderCommands_s *input,
						   const unsigned char *colors, qboolean in2D );

void		VK_BindProgram( vkProgram_t *p );
void		VK_RefreshClipPlane( void );

vkProgram_t	*VK_BeginFamily( unsigned family, int texEnv );
void		VK_PixelProbe( const char *src, int x, int y, int w, int h );
qboolean	VK_ModelPixelAtSwap( int x, int y, byte out[4] );
int			VK_ModelSwapFrame( void );
qboolean	RB_2DCover_ReadbackArmed( void );
int			RB_2DCover_ReadbackFrame( void );
void		VK_GlErrAt( const char *what );
void		VK_Begin2D( void );
void		VK_BeginWire( void );		
void		VK_BeginSprite( void );		
void		VK_BeginSunflare( void );		
void		VK_BeginShadow( void );		
void		VK_SetBaseColor4f( vkProgram_t *p, float r, float g, float b, float a );

/* Frees the draw census's owned key strings.  Renderer lifetime, not per purge. */
void		VK_CensusShutdownAll( void );

void		VK_WireBegin( void );
void		VK_WireVertex( const float *xyz, const byte *rgba );
void		VK_WireEnd( int mode );

void		VK_CreateWaveTable( void );
void		VK_DestroyWaveTable( void );

#ifdef __cplusplus
}	
#endif

#endif 
