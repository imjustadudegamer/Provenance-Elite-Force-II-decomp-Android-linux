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
#ifndef TR_COMMON_H
#define TR_COMMON_H

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_public.h"

/* The shared decoders (tr_image_jpg/png/bmp/pcx, tr_noise, tr_subs) are
   compiled into every renderer target.  The Vulkan target carries no GL header
   and names no gl*() symbol, so its build of this header gets the one typedef
   the structures below need and nothing else; every other target keeps qgl.h
   exactly as it was.  Verified by building both renderers from this header. */
#ifdef EF2_RENDERER_VULKAN
typedef unsigned int GLuint;
#else
#include "qgl.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	IMGTYPE_COLORALPHA, 
	IMGTYPE_NORMAL,
	IMGTYPE_NORMALHEIGHT,
	IMGTYPE_DELUXE, 
} imgType_t;

typedef enum
{
	IMGFLAG_NONE           = 0x0000,
	IMGFLAG_MIPMAP         = 0x0001,
	IMGFLAG_PICMIP         = 0x0002,
	IMGFLAG_CUBEMAP        = 0x0004,
	IMGFLAG_NO_COMPRESSION = 0x0010,
	IMGFLAG_NOLIGHTSCALE   = 0x0020,
	IMGFLAG_CLAMPTOEDGE    = 0x0040,
	IMGFLAG_GENNORMALMAP   = 0x0080,
} imgFlags_t;

typedef struct image_s {
	char		imgName[MAX_QPATH];		
	int			width, height;				
	int			uploadWidth, uploadHeight;	
	unsigned int	texnum;		/* a GL object name, or the Vulkan texture table index */

	int			frameUsed;			

	int			internalFormat;
	int			TMU;				

	imgType_t   type;
	imgFlags_t  flags;

	struct image_s*	next;

    int			bytesUsed;
    int			numMipmaps;
	qboolean	dynamicallyUpdated;
	qboolean	allowPicmip;
	qboolean	force32bit;
    int			wrapClampModeX;
    int			wrapClampModeY;
    int			wrapSetX, wrapSetY;	/* what the object holds: the flip writes the bound object */
	// (`v29[28] = a12' in R_CreateImages @0x00013A00), not the resolved GL enum.  The
	int			textureMode;
	int			r_sequence;
	int			UseCount;
	int			imgStamp;	/* creation ordinal: a recycled slot wears a new one */
} image_t;

#define LIGHTMAP_2D         -4	
#define LIGHTMAP_BY_VERTEX  -3	
#define LIGHTMAP_WHITEIMAGE -2
#define LIGHTMAP_NONE       -1

extern	refimport_t		ri;
extern glconfig_t	glConfig;		

extern qboolean  textureFilterAnisotropic;
extern int       maxAnisotropy;
extern float     displayAspect;
extern qboolean  haveClampToEdge;

extern cvar_t *r_stencilbits;			
extern cvar_t *r_depthbits;			
extern cvar_t *r_colorbits;			
extern cvar_t *r_texturebits;			
extern cvar_t *r_arb_samples;
extern cvar_t *r_ext_nv_support_multisample;
extern cvar_t *r_precompressed_textures;
extern cvar_t *r_ati_fsaa_samples;
extern cvar_t *r_nv_fsaa_samples;

extern cvar_t *r_mode;				
extern cvar_t *r_fullscreen;
extern cvar_t *r_ignorehwgamma;		
extern cvar_t *r_drawBuffer;
extern cvar_t *r_swapInterval;

extern cvar_t *r_allowExtensions;				
extern cvar_t *r_ext_compress_textures;		 
extern cvar_t *r_ext_multitexture;
extern cvar_t *r_ext_compiled_vertex_array;
extern cvar_t *r_ext_texture_env_add;

extern cvar_t *r_anisotropicFiltering;

extern cvar_t *r_stereo;

extern	cvar_t	*r_saveFontData;

qboolean	R_GetModeInfo( int *width, int *height, float *windowAspect, int mode );

float R_NoiseGet4f( float x, float y, float z, double t );
void  R_NoiseInit( void );

image_t     *R_FindImageFile( const char *name, imgType_t type, imgFlags_t flags );
image_t     *R_RefreshImageFile( const char *name, imgType_t type, imgFlags_t flags );
image_t *R_CreateImage( const char *name, byte *pic, int width, int height, imgType_t type, imgFlags_t flags, int internalFormat );

void R_IssuePendingRenderCommands( void );
qhandle_t		 RE_RegisterShaderLightMap( const char *name, int lightmapIndex );
qhandle_t		 RE_RegisterShader( const char *name );
qhandle_t		 RE_RegisterShaderNoMip( const char *name );
qhandle_t RE_RegisterShaderFromImage(const char *name, int lightmapIndex, image_t *image, qboolean mipRawImage);

void R_InitFreeType( void );
void R_DoneFreeType( void );
void RE_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font);


void R_LoadBMP( const char *name, byte **pic, int *width, int *height );
void R_LoadJPG( const char *name, byte **pic, int *width, int *height );
void R_LoadPCX( const char *name, byte **pic, int *width, int *height );
void R_LoadPNG( const char *name, byte **pic, int *width, int *height );
void R_LoadPVR( const char *name, byte **pic, int *width, int *height );
void R_LoadTGA( const char *name, byte **pic, int *width, int *height );


void		GLimp_Init( qboolean fixedFunction );
void		GLimp_Shutdown( void );
void		GLimp_EndFrame( void );

void		GLimp_LogComment( char *comment );
void		GLimp_Minimize(void);

void		GLimp_SetGamma( unsigned char red[256],
		unsigned char green[256],
		unsigned char blue[256] );


#ifdef __cplusplus
}
#endif

#endif
