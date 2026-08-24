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

#include "q_shared.h"
#include "qcommon.h"
#include "cm_polylib.h"
#include "cm_terrain_ef2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define	MAX_SUBMODELS			1024
#define	BOX_MODEL_HANDLE		(MAX_SUBMODELS-1)
#define CAPSULE_MODEL_HANDLE	510

typedef struct {
	cplane_t	*plane;
	int			children[2];		
} cNode_t;

typedef struct {
	int			cluster;
	int			area;

	intptr_t	firstLeafBrush;
	int			numLeafBrushes;

	intptr_t	firstLeafSurface;
	int			numLeafSurfaces;
} cLeaf_t;

typedef struct {
	char shader[ BSP_MAX_QPATH ];
	int surfaceFlags;
	int contentFlags;
	int subdivisions;
} cshader_t;

typedef char cshader_is_dshader_t[( sizeof( cshader_t ) == sizeof( dshader_t ) ) ? 1 : -1];

typedef struct cmodel_s {
	vec3_t		mins, maxs;
	cLeaf_t		leaf;			
} cmodel_t;

typedef struct {
	cplane_t			*plane;
	int					surfaceFlags;
	int					shaderNum;
} cbrushside_t;

typedef struct {
	int			shaderNum;		
	int			contents;
	vec3_t		bounds[2];
	int			numsides;
	cbrushside_t	*sides;
	int			checkcount;		
} cbrush_t;


typedef struct {
	int			checkcount;				
	int			surfaceFlags;
	int			contents;

	int			shaderNum;
	int			subdivisions;

	struct patchCollide_s	*pc;
} cPatch_t;

typedef struct {
	int			floodnum;
	int			floodvalid;
} cArea_t;

typedef struct {
	int			checkcount;				

	struct terPatchCollide_s	*tc;
} cterPatch_t;

typedef struct {
	char			name[ MAX_QPATH ];

	int				numShaders;
	cshader_t		*shaders;

	int				numBrushSides;
	cbrushside_t	*brushsides;

	int				numPlanes;
	cplane_t		*planes;

	int				numNodes;
	cNode_t			*nodes;

	int				numLeafs;
	cLeaf_t			*leafs;

	int				numLeafBrushes;
	int				*leafbrushes;

	int				numLeafSurfaces;
	int				*leafsurfaces;

	int				numSubModels;
	cmodel_t		*cmodels;

	int				numBrushes;
	cbrush_t		*brushes;

	int				numClusters;
	int				clusterBytes;
	byte			*visibility;
	qboolean		vised;			

	int				numEntityChars;
	char			*entityString;

	int				numAreas;
	cArea_t			*areas;
	int				*areaPortals;	

	int				numSurfaces;
	cPatch_t		**surfaces;			

	// NULL wherever the surface is not collidable terrain. Retail does the
	cTerrainEF2_t	**terrainEF2;

	int				numLightingGroups;
	char			*lightingGroups;			

	int				floodvalid;
	int				checkcount;					
} clipMap_t;


#define	SURFACE_CLIP_EPSILON	(0.125)


typedef struct
{
	qboolean	use;
	float		radius;
	vec3_t		offset;
} sphere_t;

typedef struct traceWork_s {
	vec3_t		start;
	vec3_t		end;
	vec3_t		size[2];	
	vec3_t		offsets[8];	
	float		maxOffset;	
	vec3_t		extents;	
	vec3_t		bounds[2];	
	float		height;
	float		radius;
	int			contents;	
	qboolean	isPoint;	
	trace_t		trace;		
} traceWork_t;

void CM_TraceThroughBrush( traceWork_t *tw, cbrush_t *brush );

typedef struct leafList_s {
	int		count;
	int		maxcount;
	qboolean	overflowed;
	int		*list;
	vec3_t	bounds[2];
	int		lastLeaf;		
	void	(*storeLeafs)( struct leafList_s *ll, int nodenum );
} leafList_t;


extern	clipMap_t	cm;
extern	int			c_pointcontents;
extern	int			c_traces, c_brush_traces, c_patch_traces, c_terrain_patch_traces;
extern	cvar_t		*cm_noAreas;
extern	cvar_t		*cm_noCurves;
extern	cvar_t		*cm_playerCurveClip;
extern	cvar_t		*cm_terrainCollision;
extern	sphere_t	sphere;


int CM_BoxBrushes( const vec3_t mins, const vec3_t maxs, cbrush_t **list, int listsize );

void CM_StoreLeafs( leafList_t *ll, int nodenum );
void CM_StoreBrushes( leafList_t *ll, int nodenum );

void CM_BoxLeafnums_r( leafList_t *ll, int nodenum );

cmodel_t	*CM_ClipHandleToModel( clipHandle_t handle );
qboolean CM_BoundsIntersect( const vec3_t mins, const vec3_t maxs, const vec3_t mins2, const vec3_t maxs2 );
qboolean CM_BoundsIntersectPoint( const vec3_t mins, const vec3_t maxs, const vec3_t point );


qboolean CM_PlaneFromPoints( vec4_t plane, vec3_t a, vec3_t b, vec3_t c );
struct patchCollide_s	*CM_GeneratePatchCollide( int width, int height, vec3_t *points, float subdivisions );
void CM_TraceThroughPatchCollide( traceWork_t *tw, const struct patchCollide_s *pc );
qboolean CM_PositionTestInPatchCollide( traceWork_t *tw, const struct patchCollide_s *pc );
void CM_ClearLevelPatches( void );


#ifdef __cplusplus
}
#endif
