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
#ifndef __QFILES_H__
#define __QFILES_H__

#include "q_shared.h"


#define	SHADER_MAX_VERTEXES		3000
#define	SHADER_MAX_INDEXES		(6*SHADER_MAX_VERTEXES)
#define	SHADER_MAX_TRIANGLES	(2*SHADER_MAX_VERTEXES)

#define	MAX_QPATH		256


#define	VM_MAGIC			0x12721444
#define	VM_MAGIC_VER2	0x12721445
typedef struct {
	int		vmMagic;

	int		instructionCount;

	int		codeOffset;
	int		codeLength;

	int		dataOffset;
	int		dataLength;
	int		litLength;			
	int		bssLength;			

	int		jtrgLength;			
} vmHeader_t;



typedef struct {
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char	reserved;
    char	color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char	filler[58];
    unsigned char	data[1];			
} pcx_t;



typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;

#define FENCEMASK_VERSION			2
#define FTX_EXTENSION				".ftx"

typedef struct ftx_s {
	int width;
	int height;
	int has_alpha;
} ftx_t;


#define MD3_IDENT			(('3'<<24)+('P'<<16)+('D'<<8)+'I')
#define MD3_VERSION			15

#define MD3_MAX_LODS		3
#define	MD3_MAX_TRIANGLES	8192	
#define MD3_MAX_VERTS		4096	
#define MD3_MAX_SHADERS		256		
#define MD3_MAX_FRAMES		1024	
#define	MD3_MAX_SURFACES	32		
#define MD3_MAX_TAGS		16		

#define	MD3_XYZ_SCALE		(1.0/64)

typedef struct md3Frame_s {
	vec3_t		bounds[2];
	vec3_t		localOrigin;
	float		radius;
	char		name[16];
} md3Frame_t;

typedef struct md3Tag_s {
	char		name[MAX_QPATH];	
	vec3_t		origin;
	vec3_t		axis[3];
} md3Tag_t;

typedef struct {
	int		ident;				

	char	name[MAX_QPATH];	

	int		flags;
	int		numFrames;			

	int		numShaders;			
	int		numVerts;

	int		numTriangles;
	int		ofsTriangles;

	int		ofsShaders;			
	int		ofsSt;				
	int		ofsXyzNormals;		

	int		ofsEnd;				
} md3Surface_t;

typedef struct {
	char			name[MAX_QPATH];
	int				shaderIndex;	
} md3Shader_t;

typedef struct {
	int			indexes[3];
} md3Triangle_t;

typedef struct {
	float		st[2];
} md3St_t;

typedef struct {
	short		xyz[3];
	short		normal;
} md3XyzNormal_t;

typedef struct {
	int			ident;
	int			version;

	char		name[MAX_QPATH];	

	int			flags;

	int			numFrames;
	int			numTags;
	int			numSurfaces;

	int			numSkins;

	int			ofsFrames;			
	int			ofsTags;			
	int			ofsSurfaces;		

	int			ofsEnd;				
} md3Header_t;


#define MDR_IDENT	(('5'<<24)+('M'<<16)+('D'<<8)+'R')
#define MDR_VERSION	2
#define	MDR_MAX_BONES	128

typedef struct {
	int			boneIndex;	
	float		   boneWeight;		
	vec3_t		offset;
} mdrWeight_t;

typedef struct {
	vec3_t		normal;
	vec2_t		texCoords;
	int			numWeights;
	mdrWeight_t	weights[1];		
} mdrVertex_t;

typedef struct {
	int			indexes[3];
} mdrTriangle_t;

typedef struct {
	int			ident;

	char		name[MAX_QPATH];	
	char		shader[MAX_QPATH];
	int			shaderIndex;	

	int			ofsHeader;	

	int			numVerts;
	int			ofsVerts;

	int			numTriangles;
	int			ofsTriangles;

	int			numBoneReferences;
	int			ofsBoneReferences;

	int			ofsEnd;		
} mdrSurface_t;

typedef struct {
	float		matrix[3][4];
} mdrBone_t;

typedef struct {
	vec3_t		bounds[2];		
	vec3_t		localOrigin;		
	float		radius;			
	char		name[16];
	mdrBone_t	bones[1];		
} mdrFrame_t;

typedef struct {
        unsigned char Comp[24]; 
} mdrCompBone_t;

typedef struct {
        vec3_t          bounds[2];		
        vec3_t          localOrigin;		
        float           radius;			
        mdrCompBone_t   bones[1];		
} mdrCompFrame_t;

typedef struct {
	int			numSurfaces;
	int			ofsSurfaces;		
	int			ofsEnd;				
} mdrLOD_t;

typedef struct {
        int                     boneIndex;
        char            name[32];
} mdrTag_t;

typedef struct {
	int			ident;
	int			version;

	char		name[MAX_QPATH];	

	int			numFrames;
	int			numBones;
	int			ofsFrames;			

	int			numLODs;
	int			ofsLODs;

        int                     numTags;
        int                     ofsTags;

	int			ofsEnd;				
} mdrHeader_t;





#define BSP_HEADER  (('!'<<24)+('2'<<16)+('F'<<8)+'E')
#define BSP_VERSION			20


#define	MAX_MAP_MODELS		      0x400
#define	MAX_MAP_BRUSHES		   0x8000
#define	MAX_MAP_ENTITIES	      0x1000
#define	MAX_MAP_ENTSTRING	      0x400000  
#define	MAX_MAP_SHADERS		   0x400

#define  MAX_MAP_SHADERSTRING    0x4000
#define  MAX_MAP_NUM_SHADERS     4096

#define	MAX_MAP_AREAS		      0x100	
#define	MAX_MAP_FOGS		      0x100
#define	MAX_MAP_PLANES		      0x20000
#define	MAX_MAP_NODES		      0x20000
#define	MAX_MAP_BRUSHSIDES	   0x40000
#define	MAX_MAP_LEAFS		      0x20000
#define	MAX_MAP_LEAFFACES	      0x20000
#define	MAX_MAP_LEAFBRUSHES     0x40000
#define	MAX_MAP_PORTALS		   0x20000
#define	MAX_MAP_LIGHTING	      0x1000000
#define	MAX_MAP_LIGHTGRID	      0x800000
#define	MAX_MAP_VISIBILITY	   0x200000
#define  MAX_MAP_SPHERE_LIGHTS   0x400

#define	MAX_MAP_DRAW_SURFS	   0x20000
#define	MAX_MAP_DRAW_VERTS	   0x80000
#define	MAX_MAP_DRAW_INDEXES	   0x80000

#define	MAX_MAP_LIGHTING_VERTS		0x80000
#define	MAX_MAP_LIGHTING_SURFS		0x20000
#define	MAX_MAP_LIGHTING_VERTSURFS 0x20000

#define  MAX_MAP_LIGHTDEFS       MAX_MAP_DRAW_SURFS

#define	BSP_MAX_QPATH			64

#define	MAX_LIGHTING_GROUPS		32 // NOTE: do NOT change this... it is apparently used as "the max number of bitfields stored in an int" (grumble grumble).

#define	MAX_STATIC_LOD_MODELS	0x1000


#define	MAX_KEY				      32
#define	MAX_VALUE			      1024

#define	ANGLE_UP			               -1
#define	ANGLE_DOWN			            -2

#define  DEFAULT_CURVE_SUBDIVISIONS    4
#define  DEFAULT_LIGHTMAP_RESOLUTION   16
#define  DEFAULT_TERRAIN_LIGHTMAP_RESOLUTION   32
#define  MIN_LIGHTMAP_RESOLUTION       4
#define  MAX_LIGHTMAP_RESOLUTION       128

#define	LIGHTMAP_WIDTH		            128
#define	LIGHTMAP_HEIGHT		         128

#define	LIGHTMAP_SIZE			         ( LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 3 )

#define MIN_WORLD_COORD ( -65536 )
#define MAX_WORLD_COORD	( 65536 )

#define WORLD_SIZE		( MAX_WORLD_COORD - MIN_WORLD_COORD )




typedef struct {
   void     *buffer;
   int      length;
} gamelump_t;

typedef struct {
	int		fileofs, filelen;
} lump_t;

#define  LUMP_SHADERS				0
#define  LUMP_PLANES				1
#define  LUMP_LIGHTMAPS				2
#define  LUMP_BASELIGHTMAPS			3
#define  LUMP_CONTLIGHTMAPS			4
#define  LUMP_SURFACES				5
#define  LUMP_DRAWVERTS				6
#define  LUMP_DRAWINDEXES			7
#define  LUMP_LEAFBRUSHES			8
#define  LUMP_LEAFSURFACES			9
#define  LUMP_LEAFS					10
#define  LUMP_NODES					11
#define  LUMP_BRUSHSIDES			12
#define  LUMP_BRUSHES				13
#define  LUMP_FOGS					14
#define  LUMP_MODELS				15
#define  LUMP_ENTITIES				16
#define  LUMP_VISIBILITY			17
#define  LUMP_LIGHTGRID				18
#define  LUMP_ENTLIGHTS				19
#define  LUMP_ENTLIGHTSVIS			20
#define  LUMP_LIGHTDEFS				21
#define  LUMP_BASELIGHTINGVERTS		22
#define  LUMP_CONTLIGHTINGVERTS		23
#define  LUMP_BASELIGHTINGSURFS		24
#define  LUMP_LIGHTINGSURFS			25
#define  LUMP_LIGHTINGVERTSURFS		26
#define  LUMP_LIGHTINGGROUPS		27
#define  LUMP_STATIC_LOD_MODELS		28
#define  LUMP_BSPINFO				29
#define  HEADER_LUMPS				30


typedef struct {
	int			ident;
	int			version;
   int         checksum;

	lump_t		lumps[HEADER_LUMPS];
} dheader_t;

typedef struct {
	float		mins[3], maxs[3];
	int		firstSurface, numSurfaces;
	int		firstBrush, numBrushes;
} dmodel_t;

typedef struct {
	char		shader[BSP_MAX_QPATH];
	int		surfaceFlags;
	int		contentFlags;
   int      subdivisions;
} dshader_t;


typedef struct {
	float		normal[3];
	float		dist;
} dplane_t;

typedef struct {
	int			planeNum;
	int			children[2];	
	int			mins[3];		
	int			maxs[3];
} dnode_t;

typedef struct {
   int         cluster;       
	int			area;

	int			mins[3];			
	int			maxs[3];

	int			numLeafSurfaces;
	int			firstLeafSurface;

	int			firstLeafBrush;
	int			numLeafBrushes;
} dleaf_t;

typedef struct {
	int			shaderNum;
	int			planeNum;			
} dbrushside_t;

typedef struct {
	int			numSides;
	int			firstSide;
	int			shaderNum;		
} dbrush_t;

typedef struct {
	char		   shader[BSP_MAX_QPATH];
	int			brushNum;
	int			visibleSide;	
} dfog_t;

#ifndef QERADIANT

typedef struct {
	vec3_t		xyz;
	float		   st[2];
	vec3_t		normal;
	byte		   color[4];
	float			lodExtra;
	union
		{
		float		   lightmap[2];
		int         collapseMap;
		};
} drawVert_t;

typedef char qfiles_stride_dheader_t[( sizeof( dheader_t ) == 252 ) ? 1 : -1];
typedef char qfiles_stride_dshader_t[( sizeof( dshader_t ) == 76 ) ? 1 : -1];
typedef char qfiles_stride_dplane_t[( sizeof( dplane_t ) == 16 ) ? 1 : -1];
typedef char qfiles_stride_dnode_t[( sizeof( dnode_t ) == 36 ) ? 1 : -1];
typedef char qfiles_stride_dleaf_t[( sizeof( dleaf_t ) == 48 ) ? 1 : -1];
typedef char qfiles_stride_dbrushside_t[( sizeof( dbrushside_t ) == 8 ) ? 1 : -1];
typedef char qfiles_stride_dbrush_t[( sizeof( dbrush_t ) == 12 ) ? 1 : -1];
typedef char qfiles_stride_dfog_t[( sizeof( dfog_t ) == 72 ) ? 1 : -1];
typedef char qfiles_stride_dmodel_t[( sizeof( dmodel_t ) == 40 ) ? 1 : -1];
typedef char qfiles_stride_drawVert_t[( sizeof( drawVert_t ) == 48 ) ? 1 : -1];

typedef struct {
	byte		   color[3];
} lightingVert_t;

typedef struct {
	int			baseLightmapNum;
	int			baseLightmapX, baseLightmapY;
	int			baseLightmapWidth, baseLightmapHeight;

	int			lastUpdatedFrame;

	int			firstLightingSurface;
	int			numLightingSurfaces;

	int			baseLightingVerts;
	int			firstLightingVertSurface;
	int			numLightingVertSurfaces;
} baseLightingSurf_t;

typedef struct {
	int			lightmapNum;
	
	int			lightmapX, lightmapY;
	int			lightGroupNum;

	byte		   color[3];
} lightingSurf_t;

typedef struct {
	int			lightGroupNum;
	int			firstLightingVert;
	byte		   color[3];
} lightingVertSurf_t;

#define DYNAMIC_LIGHTMAP_BIT  ( 1 << 30 )

#define MAX_LIGHT_GROUP_NAME_LENGTH  64

typedef struct {
	char			name[ MAX_LIGHT_GROUP_NAME_LENGTH ];
} lightingGroup_t;

typedef struct {
	int	totalNumberOfVerts;
	float origin[3];
	float startDistance;
	float startPerCentVerts;
	float stopDistance;
	float stopPerCentVerts;
	float alphaFadeDistance;
	int	lodNumberOfVerts;
	int	frameCount;
} staticLodModel_t;

#endif
typedef enum {
	MST_BAD,
	MST_PLANAR,
	MST_PATCH,
	MST_TRIANGLE_SOUP,
	MST_FLARE,
	MST_TERRAIN
} mapSurfaceType_t;

#define LIGHTMAP_NONE		-1
#define LIGHTMAP_NEEDED		-2
#define NUMBER_TERRAIN_VERTS 9

typedef struct {
	int			shaderNum;
	int			fogNum;
	int			surfaceType;

	int			firstVert;
	int			numVerts;

	int			firstIndex;
	int			numIndexes;

	int			lightmapNum;
	int			lightmapX, lightmapY;
	int			lightmapWidth, lightmapHeight;

	vec3_t		lightmapOrigin;
	vec3_t		lightmapVecs[3];	

	int			patchWidth;
	int			patchHeight;

   float       subdivisions;

	int			baseLightingSurface;
	int			inverted;
	int			faceFlags[4];
} dsurface_t;

typedef char qfiles_stride_dsurface_t[( sizeof( dsurface_t ) == 132 ) ? 1 : -1];

typedef struct {
	vec3_t		origin;
	vec3_t		axis;
	int			bounds[3];
} dlightGrid_t;

typedef struct {
	int			lightIntensity;
	int			lightAngle;
	int			lightmapResolution;
	qboolean	twoSided;
	qboolean	lightLinear;
	vec3_t		lightColor;
	float		lightFalloff;
	float		backsplashFraction;
	float		backsplashDistance;
	float		lightSubdivide;
	qboolean	autosprite;
} dlightdef_t;

typedef struct {
	vec3_t       origin;                     
	vec3_t       color;                      
	int          leaf;                       
	qboolean     needs_trace;                
	qboolean     lensflare;                  
	int          type;                       
	int          group_num;                  
	vec3_t       targetNormal;               
	float        dotProduct_hotspot;         
	float        dotProduct_penumbra;        
	float        falloff_start_dist;         
	float        falloff_end_dist;           
	float        curveOffset;                
	float        spherical_ambient;          
	float        brightness;                 
	int          exclusiveLightNameCount;    
	unsigned int exclusiveLightNameHashList[4]; 
} mapspherel_t;

#endif
