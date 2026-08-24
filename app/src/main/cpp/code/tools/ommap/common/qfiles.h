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
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#ifndef __QFILES_H__
#define __QFILES_H__


#define	SHADER_MAX_VERTEXES	1000
#define	SHADER_MAX_INDEXES	(6*SHADER_MAX_VERTEXES)


#define	MAX_QPATH		64



#define	VM_MAGIC	0x12721444
typedef struct {
	int		vmMagic;

	int		instructionCount;

	int		codeOffset;
	int		codeLength;

	int		dataOffset;
	int		dataLength;
	int		litLength;			
	int		bssLength;			
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
    unsigned char	data;			
} pcx_t;



typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;




#define MD3_IDENT			(('3'<<24)+('P'<<16)+('D'<<8)+'I')
#define MD3_VERSION			15

#define MD3_MAX_LODS		4
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



#define MD4_IDENT			(('4'<<24)+('P'<<16)+('D'<<8)+'I')
#define MD4_VERSION			1
#define	MD4_MAX_BONES		128

typedef struct {
	int			boneIndex;		
	float		boneWeight;		
} md4Weight_t;

typedef struct {
	vec3_t		vertex;
	vec3_t		normal;
	float		texCoords[2];
	int			numWeights;
	md4Weight_t	weights[1];		
} md4Vertex_t;

typedef struct {
	int			indexes[3];
} md4Triangle_t;

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
} md4Surface_t;

typedef struct {
	float		matrix[3][4];
} md4Bone_t;

typedef struct {
	vec3_t		bounds[2];			
	vec3_t		localOrigin;		
	float		radius;				
	char		name[16];
	md4Bone_t	bones[1];			
} md4Frame_t;

typedef struct {
	int			numSurfaces;
	int			ofsSurfaces;		
	int			ofsEnd;				
} md4LOD_t;

typedef struct {
	int			ident;
	int			version;

	char		name[MAX_QPATH];	

	int			numFrames;
	int			numBones;
	int			ofsFrames;			

	int			numLODs;
	int			ofsLODs;

	int			ofsEnd;				
} md4Header_t;




#define BSP_IDENT	(('P'<<24)+('S'<<16)+('B'<<8)+'I')

#define BSP_VERSION			46


#define	MAX_MAP_MODELS		0x400
#define	MAX_MAP_BRUSHES		0x8000
#define	MAX_MAP_ENTITIES	0x800
#define	MAX_MAP_ENTSTRING	0x40000
#define	MAX_MAP_SHADERS		0x400

#define	MAX_MAP_AREAS		0x100	
#define	MAX_MAP_FOGS		0x100
#define	MAX_MAP_PLANES		0x20000
#define	MAX_MAP_NODES		0x20000
#define	MAX_MAP_BRUSHSIDES	0x20000
#define	MAX_MAP_LEAFS		0x20000
#define	MAX_MAP_LEAFFACES	0x20000
#define	MAX_MAP_LEAFBRUSHES 0x40000
#define	MAX_MAP_PORTALS		0x20000
#define	MAX_MAP_LIGHTING	0x800000
#define	MAX_MAP_LIGHTGRID	0x800000
#define	MAX_MAP_VISIBILITY	0x200000

#define	MAX_MAP_DRAW_SURFS	0x20000
#define	MAX_MAP_DRAW_VERTS	0x80000
#define	MAX_MAP_DRAW_INDEXES	0x80000


#define	MAX_KEY				32
#define	MAX_VALUE			1024

#define	ANGLE_UP			-1
#define	ANGLE_DOWN			-2

#define	LIGHTMAP_WIDTH		128
#define	LIGHTMAP_HEIGHT		128

#define MAX_WORLD_COORD		( 128*1024 )
#define MIN_WORLD_COORD		( -128*1024 )
#define WORLD_SIZE			( MAX_WORLD_COORD - MIN_WORLD_COORD )



typedef struct {
	int		fileofs, filelen;
} lump_t;

#define	LUMP_ENTITIES		0
#define	LUMP_SHADERS		1
#define	LUMP_PLANES			2
#define	LUMP_NODES			3
#define	LUMP_LEAFS			4
#define	LUMP_LEAFSURFACES	5
#define	LUMP_LEAFBRUSHES	6
#define	LUMP_MODELS			7
#define	LUMP_BRUSHES		8
#define	LUMP_BRUSHSIDES		9
#define	LUMP_DRAWVERTS		10
#define	LUMP_DRAWINDEXES	11
#define	LUMP_FOGS			12
#define	LUMP_SURFACES		13
#define	LUMP_LIGHTMAPS		14
#define	LUMP_LIGHTGRID		15
#define	LUMP_VISIBILITY		16
#define	HEADER_LUMPS		17

typedef struct {
	int			ident;
	int			version;

	lump_t		lumps[HEADER_LUMPS];
} dheader_t;

typedef struct {
	float		mins[3], maxs[3];
	int			firstSurface, numSurfaces;
	int			firstBrush, numBrushes;
} dmodel_t;

typedef struct {
	char		shader[MAX_QPATH];
	int			surfaceFlags;
	int			contentFlags;
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
	int			cluster;			
	int			area;

	int			mins[3];			
	int			maxs[3];

	int			firstLeafSurface;
	int			numLeafSurfaces;

	int			firstLeafBrush;
	int			numLeafBrushes;
} dleaf_t;

typedef struct {
	int			planeNum;			
	int			shaderNum;
} dbrushside_t;

typedef struct {
	int			firstSide;
	int			numSides;
	int			shaderNum;		
} dbrush_t;

typedef struct {
	char		shader[MAX_QPATH];
	int			brushNum;
	int			visibleSide;	
} dfog_t;

typedef struct {
	vec3_t		xyz;
	float		st[2];
	float		lightmap[2];
	vec3_t		normal;
	byte		color[4];
} drawVert_t;

typedef enum {
	MST_BAD,
	MST_PLANAR,
	MST_PATCH,
	MST_TRIANGLE_SOUP,
	MST_FLARE
} mapSurfaceType_t;

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
} dsurface_t;


#endif
