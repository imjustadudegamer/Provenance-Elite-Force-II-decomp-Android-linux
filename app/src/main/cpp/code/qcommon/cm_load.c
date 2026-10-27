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

#include "cm_local.h"
#include "../client/client.h"

#ifdef BSPC

#include "../bspc/l_qfiles.h"

void SetPlaneSignbits (cplane_t *out) {
	int	bits, j;

	bits = 0;
	for (j=0 ; j<3 ; j++) {
		if (out->normal[j] < 0) {
			bits |= 1<<j;
		}
	}
	out->signbits = bits;
}
#endif 

#define	BOX_BRUSHES		1
#define	BOX_SIDES		6
#define	BOX_LEAFS		2
#define	BOX_PLANES		12

#define	LL(x) x=LittleLong(x)


clipMap_t	cm;
int			c_pointcontents;
int			c_traces, c_brush_traces, c_patch_traces, c_terrain_patch_traces;


byte		*cmod_base;

#ifndef BSPC
cvar_t		*cm_noAreas;
cvar_t		*cm_noCurves;
cvar_t		*cm_playerCurveClip;
cvar_t		*cm_terrainCollision;
#endif

cmodel_t	box_model;
cplane_t	*box_planes;
cbrush_t	*box_brush;

static unsigned int numleafbrushes;



void	CM_InitBoxHull (void);
void	CM_FloodAreaConnections (void);



void CMod_LoadShaders( gamelump_t *l ) {
	dshader_t		*in;
	cshader_t		*out;
	int				i, count;

	in = ( dshader_t * )l->buffer;
	if ( l->length % sizeof( dshader_t ) ) {
		Com_Error( ERR_DROP, "CM_LoadMap: funny lump size in %s", cm.name );
	}
	count = l->length / sizeof( dshader_t );

	if (count < 1) {
		Com_Error (ERR_DROP, "Map with no shaders");
	}

	cm.shaders = Hunk_Alloc( count * sizeof( *cm.shaders ), h_dontcare);
	cm.numShaders = count;

	out = cm.shaders;
	for ( i=0 ; i<count ; i++, in++, out++ ) {
		Q_strncpyz( out->shader, in->shader, sizeof( out->shader ) );
		out->contentFlags = LittleLong( in->contentFlags );
		out->surfaceFlags = LittleLong( in->surfaceFlags );
		out->subdivisions = LittleLong( in->subdivisions );
	}
}


void CMod_LoadSubmodels( gamelump_t *l ) {
	dmodel_t	*in;
	cmodel_t	*out;
	int			i, j, count;
	int			*indexes;

	in = ( dmodel_t * )l->buffer;
	if (l->length % sizeof(*in))
		Com_Error( ERR_DROP, "CM_LoadMap: funny lump size in %s", cm.name );
	count = l->length / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no models");
	cm.cmodels = Hunk_Alloc( count * sizeof( *cm.cmodels ), h_dontcare);
	cm.numSubModels = count;

	if ( count > MAX_SUBMODELS ) {
		Com_Error( ERR_DROP, "MAX_SUBMODELS exceeded" );
	}

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out = &cm.cmodels[i];

		for (j=0 ; j<3 ; j++)
		{	
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
		}

		if ( i == 0 ) {
			continue;	
		}

		out->leaf.numLeafBrushes = LittleLong( in->numBrushes );
		if( out->leaf.numLeafBrushes )
		{
			indexes = Hunk_Alloc( out->leaf.numLeafBrushes * 4, h_dontcare);
			out->leaf.firstLeafBrush = indexes - cm.leafbrushes;
			for( j = 0; j < out->leaf.numLeafBrushes; j++ ) {
				indexes[ j ] = LittleLong( in->firstBrush ) + j;
			}
		}
		else
		{
			out->leaf.firstLeafBrush = -1;
		}

		out->leaf.numLeafSurfaces = LittleLong( in->numSurfaces );
		if( out->leaf.numLeafSurfaces )
		{
			indexes = Hunk_Alloc( out->leaf.numLeafSurfaces * 4, h_dontcare);
			out->leaf.firstLeafSurface = indexes - cm.leafsurfaces;
			for( j = 0; j < out->leaf.numLeafSurfaces; j++ ) {
				indexes[ j ] = LittleLong( in->firstSurface ) + j;
			}
		}
		else
		{
			out->leaf.numLeafSurfaces = -1;
		}
	}
}


void CMod_LoadNodes( gamelump_t *l ) {
	dnode_t		*in;
	int			child;
	cNode_t		*out;
	int			i, j, count;

	in = ( dnode_t * )l->buffer;
	if (l->length % sizeof(*in))
		Com_Error( ERR_DROP, "CM_LoadMap: funny lump size in %s", cm.name );
	count = l->length / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map has no nodes");
	cm.nodes = Hunk_Alloc( count * sizeof( *cm.nodes ), h_dontcare);
	cm.numNodes = count;

	out = cm.nodes;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->plane = cm.planes + LittleLong( in->planeNum );
		for (j=0 ; j<2 ; j++)
		{
			child = LittleLong (in->children[j]);
			out->children[j] = child;
		}
	}
}

void CM_BoundBrush( cbrush_t *b ) {
	b->bounds[0][0] = -b->sides[0].plane->dist;
	b->bounds[1][0] = b->sides[1].plane->dist;

	b->bounds[0][1] = -b->sides[2].plane->dist;
	b->bounds[1][1] = b->sides[3].plane->dist;

	b->bounds[0][2] = -b->sides[4].plane->dist;
	b->bounds[1][2] = b->sides[5].plane->dist;
}


void CMod_LoadBrushes( gamelump_t *l ) {
	dbrush_t	*in;
	cbrush_t	*out;
	int			i, count;

	in = ( dbrush_t * )l->buffer;
	if (l->length % sizeof(*in)) {
		Com_Error( ERR_DROP, "CM_LoadMap: funny lump size in %s", cm.name );
	}
	count = l->length / sizeof(*in);

	cm.brushes = Hunk_Alloc( ( BOX_BRUSHES + count ) * sizeof( *cm.brushes ), h_dontcare);
	cm.numBrushes = count;

	out = cm.brushes;

	for ( i=0 ; i<count ; i++, out++, in++ ) {
		out->sides = cm.brushsides + LittleLong(in->firstSide);
		out->numsides = LittleLong(in->numSides);

		out->shaderNum = LittleLong( in->shaderNum );
		if ( out->shaderNum < 0 || out->shaderNum >= cm.numShaders ) {
			Com_Error( ERR_DROP, "CMod_LoadBrushes: bad shaderNum: %i", out->shaderNum );
		}
		out->contents = cm.shaders[out->shaderNum].contentFlags;

		CM_BoundBrush( out );
	}

}

void CMod_LoadLeafs( gamelump_t *l )
{
	int			i;
	cLeaf_t		*out;
	dleaf_t 	*in;
	int			count;

	in = ( dleaf_t * )l->buffer;
	if( l->length % sizeof( *in ) )
		Com_Error( ERR_DROP, "CM_LoadMap: funny lump size in %s", cm.name );
	count = l->length / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no leafs");

	cm.leafs = Hunk_Alloc( ( BOX_LEAFS + count ) * sizeof( *cm.leafs ), h_dontcare);
	cm.numLeafs = count;

	out = cm.leafs;
	for( i = 0; i<count; i++, in++, out++ )
	{
		out->cluster = LittleLong( in->cluster );
		out->area = LittleLong( in->area );
		out->firstLeafBrush = LittleLong( in->firstLeafBrush );
		out->numLeafBrushes = LittleLong( in->numLeafBrushes );
		out->firstLeafSurface = LittleLong( in->firstLeafSurface );
		out->numLeafSurfaces = LittleLong( in->numLeafSurfaces );

		if( out->cluster >= cm.numClusters )
			cm.numClusters = out->cluster + 1;
		if( out->area >= cm.numAreas )
			cm.numAreas = out->area + 1;
	}

	cm.areas = Hunk_Alloc( cm.numAreas * sizeof( *cm.areas ), h_dontcare);
	cm.areaPortals = Hunk_Alloc( cm.numAreas * cm.numAreas * sizeof( *cm.areaPortals ), h_dontcare);
}

void CMod_LoadPlanes( gamelump_t *l )
{
	int			i, j;
	cplane_t	*out;
	dplane_t 	*in;
	int			count;
	int			bits;

	in = ( dplane_t * )l->buffer;
	if( l->length % sizeof( *in ) )
		Com_Error( ERR_DROP, "CM_LoadMap: funny lump size in %s", cm.name );
	count = l->length / sizeof( *in );

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no planes");
	cm.planes = Hunk_Alloc( ( BOX_PLANES + count ) * sizeof( *cm.planes ), h_dontcare);
	cm.numPlanes = count;

	out = cm.planes;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1<<j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = PlaneTypeForNormal( out->normal );
		out->signbits = bits;
	}
}

void CMod_LoadLeafBrushes( gamelump_t *l )
{
	int			i;
	int			*out;
	int		 	*in;
	int			count;

	in = ( int * )l->buffer;
	if( l->length % sizeof( *in ) )
		Com_Error( ERR_DROP, "CM_LoadMap: funny lump size in %s", cm.name );
	count = l->length / sizeof(*in);

	cm.leafbrushes = Hunk_Alloc( (count + BOX_BRUSHES) * sizeof( *cm.leafbrushes ), h_dontcare);
	cm.numLeafBrushes = count;

	out = cm.leafbrushes;

	for ( i=0 ; i<count ; i++, in++, out++) {
		*out = LittleLong( *in );
	}
}

void CMod_LoadLeafSurfaces( gamelump_t *l )
{
	int			i;
	int			*out;
	int		 	*in;
	int			count;

	in = ( int * )l->buffer;
	if( l->length % sizeof( *in ) )
		Com_Error( ERR_DROP, "CM_LoadMap: funny lump size in %s", cm.name );
	count = l->length / sizeof(*in);

	cm.leafsurfaces = Hunk_Alloc( count * sizeof( *cm.leafsurfaces ), h_dontcare);
	cm.numLeafSurfaces = count;

	out = cm.leafsurfaces;

	for ( i=0 ; i<count ; i++, in++, out++) {
		*out = LittleLong (*in);
	}
}

void CMod_LoadBrushSides( gamelump_t *l )
{
	int				i;
	cbrushside_t	*out;
	dbrushside_t 	*in;
	int				count;
	int				num;

	in = ( dbrushside_t * )l->buffer;
	if( l->length % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CM_LoadMap: funny lump size in %s", cm.name );
	}
	count = l->length / sizeof(*in);

	cm.brushsides = Hunk_Alloc( ( BOX_SIDES + count ) * sizeof( *cm.brushsides ), h_dontcare);
	cm.numBrushSides = count;

	out = cm.brushsides;

	for ( i=0 ; i<count ; i++, in++, out++) {
		num = LittleLong( in->planeNum );
		out->plane = &cm.planes[num];
		out->shaderNum = LittleLong( in->shaderNum );
		if ( out->shaderNum < 0 || out->shaderNum >= cm.numShaders ) {
			Com_Error( ERR_DROP, "CMod_LoadBrushSides: bad shaderNum: %i", out->shaderNum );
		}
		out->surfaceFlags = cm.shaders[out->shaderNum].surfaceFlags;
	}
}


void CMod_LoadEntityString( gamelump_t *l ) {
	cm.entityString = Hunk_Alloc( l->length, h_dontcare);
	cm.numEntityChars = l->length;
	Com_Memcpy( cm.entityString, l->buffer, l->length );
}

void CMod_LoadLightingGroups( gamelump_t *l ) {
	int		i;

	cm.numLightingGroups = 0;
	cm.lightingGroups = NULL;

	if ( l->length % LIGHTING_GROUP_NAME_SIZE ) {
		Com_DPrintf( "WARNING: funny lighting group lump size in %s\n", cm.name );
		return;
	}

	cm.numLightingGroups = l->length / LIGHTING_GROUP_NAME_SIZE;

	if ( !cm.numLightingGroups ) {
		return;
	}

	cm.lightingGroups = Hunk_Alloc( l->length, h_dontcare );
	Com_Memset( cm.lightingGroups, 0, l->length );

	for ( i = 0 ; i < cm.numLightingGroups ; i++ ) {
		Q_strncpyz( cm.lightingGroups + i * LIGHTING_GROUP_NAME_SIZE,
			( const char * )l->buffer + i * LIGHTING_GROUP_NAME_SIZE,
			LIGHTING_GROUP_NAME_SIZE );
	}
}

int CM_GetLightingGroup( const char *group_name ) {
	int		i;

	for ( i = 0 ; i < cm.numLightingGroups ; i++ ) {
		if ( !strcmp( cm.lightingGroups + i * LIGHTING_GROUP_NAME_SIZE, group_name ) ) {
			return i;
		}
	}

	return -1;
}

#define	VIS_HEADER	8
void CMod_LoadVisibility( gamelump_t *l ) {
	int		len;
	byte	*buf;

    len = l->length;
	if ( !len ) {
		cm.clusterBytes = ( cm.numClusters + 31 ) & ~31;
		cm.visibility = Hunk_Alloc( cm.clusterBytes, h_dontcare);
		Com_Memset( cm.visibility, 255, cm.clusterBytes );
		return;
	}
	buf = ( byte * )l->buffer;

	cm.vised = qtrue;
	cm.visibility = Hunk_Alloc( len, h_dontcare);
	cm.numClusters = LittleLong( ( ( int * )buf )[ 0 ] );
	cm.clusterBytes = LittleLong( ( ( int * )buf )[ 1 ] );
	Com_Memcpy( cm.visibility, buf + VIS_HEADER, len - VIS_HEADER );
}



#define	MAX_PATCH_VERTS		1024

#define	CM_SHADER_KEEP  ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP | CONTENTS_CAMERACLIP | CONTENTS_WEAPONCLIP )
void CMod_LoadPatches( gamelump_t *surfs, gamelump_t *verts ) {
	drawVert_t	*dv, *dv_p;
	dsurface_t	*in;
	int			count;
	int			i, j;
	int			c;
	cPatch_t	*patch;
	vec3_t		points[MAX_PATCH_VERTS];
	int			width, height;
	int			shaderNum;
	char		tempName[MAX_QPATH + 1];

	in = ( dsurface_t * )surfs->buffer;
	if( surfs->length % sizeof( *in ) )
		Com_Error( ERR_DROP, "CM_LoadMap: funny lump size in %s", cm.name );
	cm.numSurfaces = count = surfs->length / sizeof(*in);
	cm.surfaces = Hunk_Alloc( cm.numSurfaces * sizeof( cm.surfaces[0] ), h_dontcare);
	memset( cm.surfaces, 0, cm.numSurfaces * sizeof( cm.surfaces[ 0 ] ) );

	dv = ( drawVert_t * )verts->buffer;
	if( verts->length % sizeof( *dv ) )
		Com_Error( ERR_DROP, "CM_LoadMap: funny lump size in %s", cm.name );

	for ( i = 0 ; i < count ; i++, in++ ) {
		if ( LittleLong( in->surfaceType ) != MST_PATCH ) {
			assert(cm.surfaces[ i ] == 0);
			continue;		
		}

		shaderNum = LittleLong( in->shaderNum );
		if( !( cm.shaders[ shaderNum ].contentFlags & CONTENTS_SOLID ) &&
			!( cm.shaders[ shaderNum ].contentFlags & CM_SHADER_KEEP ) )
		{
			cm.surfaces[ i ] = NULL;
			continue;
		}

		cm.surfaces[ i ] = patch = Hunk_Alloc( sizeof( *patch ), h_dontcare);
		assert( cm.surfaces[ i ] != ( cPatch_t * )0 );
		assert( cm.surfaces[ i ] != ( cPatch_t * )0x4 );

		width = LittleLong( in->patchWidth );
		height = LittleLong( in->patchHeight );
		c = width * height;
		if ( c > MAX_PATCH_VERTS ) {
			Com_Error( ERR_DROP, "ParseMesh: MAX_PATCH_VERTS" );
		}

		dv_p = dv + LittleLong( in->firstVert );
		for ( j = 0 ; j < c ; j++, dv_p++ ) {
			points[j][0] = LittleFloat( dv_p->xyz[0] );
			points[j][1] = LittleFloat( dv_p->xyz[1] );
			points[j][2] = LittleFloat( dv_p->xyz[2] );
		}

		shaderNum = LittleLong( in->shaderNum );
		patch->shaderNum = shaderNum;
		patch->contents = cm.shaders[ shaderNum ].contentFlags;
		patch->surfaceFlags = cm.shaders[ shaderNum ].surfaceFlags;

		/* retail clamps the subdivision floor at 16 (CMod_LoadPatches: the raw
		   value is passed through when it is 16 or above, otherwise 16); the
		   unclamped 4 and 5 in these shaders subdivide the corridor's curved
		   panels about four times finer than retail and bevel planes that
		   retail never builds come out of them */
		patch->subdivisions = cm.shaders[ shaderNum ].subdivisions;
		if ( patch->subdivisions < 16 ) {
			patch->subdivisions = 16;
		}

		patch->pc = CM_GeneratePatchCollide( width, height, points, patch->subdivisions );

		Com_sprintf(tempName, sizeof(tempName), "s%d", i);
		UI_LoadResource(tempName);
	}
}

static void CMod_LoadTerrainEF2( gamelump_t *rawSurfs, gamelump_t *verts ) {
	const byte		*in;
	drawVert_t		*dv;
	int				i, j, count, built = 0;

	cm.terrainEF2 = NULL;
	if ( !rawSurfs->buffer || !rawSurfs->length ) {
		return;
	}
	if ( rawSurfs->length % sizeof( dsurface_t ) ) {
		Com_Error( ERR_DROP, "CMod_LoadTerrainEF2: funny lump size in %s", cm.name );
	}
	count = rawSurfs->length / sizeof( dsurface_t );
	if ( count != cm.numSurfaces ) {
		Com_Error( ERR_DROP, "CMod_LoadTerrainEF2: %i raw surfaces but %i converted in %s",
			count, cm.numSurfaces, cm.name );
	}

	dv = ( drawVert_t * )verts->buffer;
	if ( !dv ) {
		return;
	}

	CM_Terrain_InitializeEF2();

	cm.terrainEF2 = Hunk_Alloc( count * sizeof( cm.terrainEF2[0] ), h_dontcare );
	Com_Memset( cm.terrainEF2, 0, count * sizeof( cm.terrainEF2[0] ) );

	in = ( const byte * )rawSurfs->buffer;
	for ( i = 0; i < count; i++, in += sizeof( dsurface_t ) ) {
		cTerrainEF2_t		*t;
		const drawVert_t	*vp;
		int					shaderNum, firstVert, flip, holes[4], any;

		if ( LittleLong( *( const int * )( in + 0x08 ) ) != MST_TERRAIN ) {
			continue;
		}

		shaderNum = LittleLong( *( const int * )( in + 0x00 ) );
		if ( shaderNum < 0 || shaderNum >= cm.numShaders ) {
			continue;
		}
		if ( !( cm.shaders[shaderNum].contentFlags & CONTENTS_SOLID )
		  && !( cm.shaders[shaderNum].contentFlags & CM_SHADER_KEEP ) ) {
			continue;
		}

		firstVert = LittleLong( *( const int * )( in + 0x0C ) );
		if ( firstVert < 0
		  || ( firstVert + EF2_TERRAIN_VERTS ) * (int)sizeof( *dv ) > verts->length ) {
			continue;
		}

		flip = LittleLong( *( const int * )( in + 0x70 ) );
		any  = 0;
		for ( j = 0; j < 4; j++ ) {
			holes[j] = LittleLong( *( const int * )( in + 0x74 + 4 * j ) );
			any |= holes[j];
		}
		if ( !any ) {
			holes[0] = holes[1] = holes[2] = holes[3] = 0;
		}

		t = Hunk_Alloc( sizeof( *t ), h_dontcare );
		Com_Memset( t, 0, sizeof( *t ) );
		cm.terrainEF2[i] = t;

		vp = dv + firstVert;
		for ( j = 0; j < EF2_TERRAIN_VERTS; j++, vp++ ) {
			t->pts[j][0] = LittleFloat( vp->xyz[0] );
			t->pts[j][1] = LittleFloat( vp->xyz[1] );
			t->pts[j][2] = LittleFloat( vp->xyz[2] );
		}

		t->shaderNum    = shaderNum;
		t->contents     = cm.shaders[shaderNum].contentFlags;
		t->surfaceFlags = cm.shaders[shaderNum].surfaceFlags | SURF_TERRAIN;

		CM_Terrain_BuildAABBTreeEF2( t, flip, holes );
		built++;
	}

	if ( built ) {
		Com_Printf( "  [EF2-CM] terrain: %i collidable surfaces of %i\n", built, count );
	}
}



unsigned CM_Checksum( dheader_t *header ) {
	return LittleLong(header->checksum);
}

static byte       *cm_bspFileBuffer;
static int         cm_bspFileLength;
static const char *cm_bspFileName;   

int CM_LoadLump( fileHandle_t handle, lump_t *lump, gamelump_t *glump, int size )
{
	glump->buffer = NULL;
	glump->length = lump->filelen;

	if( lump->filelen ) {
		if( !cm_bspFileBuffer ) {
			glump->buffer = Hunk_AllocateTempMemory( lump->filelen );

			if( FS_Seek( handle, lump->fileofs, FS_SEEK_SET ) < 0 ) {
				Com_Error( ERR_DROP, "CM_LoadLump: Error seeking to lump." );
			}

			FS_Read( glump->buffer, lump->filelen, handle );
		} else {
			if( lump->fileofs < 0 || lump->fileofs + lump->filelen > cm_bspFileLength ) {
				Com_Error( ERR_DROP,
					"CM_LoadLump: lump offset %i length %i outside %s (%i bytes)",
					lump->fileofs, lump->filelen, cm_bspFileName, cm_bspFileLength );
			}

			glump->buffer = Hunk_AllocateTempMemory( lump->filelen );
			Com_Memcpy( glump->buffer, cm_bspFileBuffer + lump->fileofs, lump->filelen );
		}

		if( size ) {
			return lump->filelen / size;
		}
	}

	return 0;
}

void CM_FreeLump( gamelump_t *lump )
{
	if( lump->buffer )
	{
		Hunk_FreeTempMemory( lump->buffer );
		lump->buffer = NULL;
		lump->length = 0;
	}
}

#define _R( id ) UI_LoadResource( "*" #id )

#define _L() CL_ProcessLoadingScreen( "$$LoadingMap$$" )

void CM_LoadMap( const char *name, qboolean clientload, int *checksum ) {
	gamelump_t		lump, lump2;
	int				i;
	dheader_t		header;
	int				length;
	fileHandle_t	h;
	static unsigned	last_checksum;

	if ( !name || !name[0] ) {
		Com_Error( ERR_DROP, "CM_LoadMap: NULL name" );
	}

#ifndef BSPC
	cm_noAreas = Cvar_Get( "cm_noAreas", "0", CVAR_CHEAT );
	cm_noCurves = Cvar_Get( "cm_noCurves", "0", CVAR_CHEAT );
	cm_playerCurveClip = Cvar_Get( "cm_playerCurveClip", "1", CVAR_ARCHIVE | CVAR_CHEAT );
	cm_terrainCollision = Cvar_Get( "cm_terrainCollision", "1", CVAR_CHEAT );
#endif
	Com_DPrintf( "CM_LoadMap( %s, %i )\n", name, clientload );

	if ( !strcmp( cm.name, name ) && clientload ) {
		*checksum = last_checksum;
		return;
	}

	Com_Memset( &cm, 0, sizeof( cm ) );
	CM_ClearLevelPatches();

	if ( !name[0] ) {
		cm.numLeafs = 1;
		cm.numClusters = 1;
		cm.numAreas = 1;
		cm.cmodels = Hunk_Alloc( sizeof( *cm.cmodels ), h_dontcare);
		*checksum = 0;
		return;
	}

	Alias_Clear();

#ifndef BSPC
	length = FS_ReadFile( name, (void **)&cm_bspFileBuffer );
	if( length <= 0 || !cm_bspFileBuffer ) {
		cm_bspFileBuffer = NULL;
		Com_Error( ERR_DROP, "Couldn't load %s", name );
	}
	cm_bspFileLength = (int)length;
	cm_bspFileName   = name;
	h = 0;

	if( cm_bspFileLength < (int)sizeof( dheader_t ) ) {
		FS_FreeFile( cm_bspFileBuffer );
		cm_bspFileBuffer = NULL;
		Com_Error( ERR_DROP, "CM_LoadMap: %s is too small to hold a BSP header", name );
	}

	Com_Memcpy( &header, cm_bspFileBuffer, sizeof( dheader_t ) );
#else
	length = LoadQuakeFile((quakefile_t *) name, (void **)&buf);

	if( length <= 0 ) {
		Com_Error( ERR_DROP, "Couldn't load %s", name );
	}

	FS_Read( &header, sizeof( dheader_t ), h );
#endif

	last_checksum = LittleLong(header.checksum);
	*checksum = last_checksum;

	for (i=0 ; i<sizeof(dheader_t)/4 ; i++) {
		((int *)&header)[i] = LittleLong ( ((int *)&header)[i]);
	}

	if ( header.version != BSP_VERSION ) {
		Com_Error( ERR_DROP, "CM_LoadMap: %s has wrong version number (%i should be %i)",
			name, header.version, BSP_VERSION );
	}

	_R( 0 );
	CM_LoadLump( h, &header.lumps[ LUMP_SHADERS ], &lump, 0 );
	_R( 1 );
	CMod_LoadShaders( &lump );
	_R( 2 );
	CM_FreeLump( &lump );
	_R( 3 );
	_L();
	CM_LoadLump( h, &header.lumps[ LUMP_PLANES ], &lump, 0 );
	_R( 4 );
	CMod_LoadPlanes( &lump );
	_R( 5 );
	CM_FreeLump( &lump );
	_R( 6 );
	_L();
	CM_LoadLump( h, &header.lumps[ LUMP_SURFACES ], &lump, 0 );
	_R( 7 );
	CM_LoadLump( h, &header.lumps[ LUMP_DRAWVERTS ], &lump2, 0 );
	_R( 8 );
	CMod_LoadPatches( &lump, &lump2 );
	CMod_LoadTerrainEF2( &lump, &lump2 );
	CM_FreeLump( &lump2 );
	_R( 9 );
	CM_FreeLump( &lump );
	_R( 10 );
	_L();
	cm.numLeafBrushes = CM_LoadLump( h, &header.lumps[ LUMP_LEAFBRUSHES ], &lump, sizeof( int ) );
	_R( 11 );
	CMod_LoadLeafBrushes( &lump );
	_R( 12 );
	CM_FreeLump( &lump );
	_R( 13 );
	_L();
	CM_LoadLump( h, &header.lumps[ LUMP_LEAFSURFACES ], &lump, 0 );
	_R( 14 );
	CMod_LoadLeafSurfaces( &lump );
	_R( 15 );
	CM_FreeLump( &lump );
	_R( 16 );
	_L();
	CM_LoadLump( h, &header.lumps[ LUMP_LEAFS ], &lump, 0 );
	_R( 17 );
	CMod_LoadLeafs( &lump );
	_R( 18 );
	CM_FreeLump( &lump );
	_R( 19 );
	_L();
	CM_LoadLump( h, &header.lumps[ LUMP_NODES ], &lump, 0 );
	_R( 20 );
	CMod_LoadNodes( &lump );
	_R( 21 );
	CM_FreeLump( &lump );
	_R( 22 );
	_L();
	CM_LoadLump( h, &header.lumps[ LUMP_BRUSHSIDES ], &lump, 0 );
	_R( 23 );
	CMod_LoadBrushSides( &lump );
	_R( 24 );
	CM_FreeLump( &lump );
	_R( 25 );
	_L();
	CM_LoadLump( h, &header.lumps[ LUMP_BRUSHES ], &lump, 0 );
	_R( 26 );
	CMod_LoadBrushes( &lump );
	_R( 27 );
	CM_FreeLump( &lump );
	_R( 28 );
	_L();
	CM_LoadLump( h, &header.lumps[ LUMP_MODELS ], &lump, 0 );
	_R( 29 );
	CMod_LoadSubmodels( &lump );
	_R( 30 );
	CM_FreeLump( &lump );
	_R( 31 );
	_L();
	CM_LoadLump( h, &header.lumps[ LUMP_ENTITIES ], &lump, 0 );
	_R( 32 );
	CMod_LoadEntityString( &lump );
	_R( 33 );
	CM_FreeLump( &lump );
	_R( 34 );
	_L();
	CM_LoadLump( h, &header.lumps[ LUMP_VISIBILITY ], &lump, 0 );
	_R( 35 );
	CMod_LoadVisibility( &lump );
	_R( 36 );
	CM_FreeLump( &lump );
	_R( 37 );
	_L();
	CM_LoadLump( h, &header.lumps[ LUMP_LIGHTINGGROUPS ], &lump, 0 );
	CMod_LoadLightingGroups( &lump );
	CM_FreeLump( &lump );
	_L();
	_R( 38 );
	FS_FreeFile( cm_bspFileBuffer );
	cm_bspFileBuffer = NULL;
	cm_bspFileLength = 0;
	_R( 39 );
	CM_InitBoxHull();
	_R( 40 );
	CM_FloodAreaConnections();
	_R( 41 );

	if ( !clientload ) {
		Q_strncpyz( cm.name, name, sizeof( cm.name ) );
	}

	_R( 51 );
}

void CM_ClearMap( void ) {
	Com_Memset( &cm, 0, sizeof( cm ) );
	CM_ClearLevelPatches();
}

cmodel_t	*CM_ClipHandleToModel( clipHandle_t handle ) {
	if ( handle < 0 ) {
		Com_Error( ERR_DROP, "CM_ClipHandleToModel: bad handle %i", handle );
	}
	if ( handle < cm.numSubModels ) {
		return &cm.cmodels[handle];
	}
	if ( handle == BOX_MODEL_HANDLE ) {
		return &box_model;
	}
	if ( handle < MAX_SUBMODELS ) {
		Com_Error( ERR_DROP, "CM_ClipHandleToModel: bad handle %i < %i < %i",
			cm.numSubModels, handle, MAX_SUBMODELS );
	}
	Com_Error( ERR_DROP, "CM_ClipHandleToModel: bad handle %i", handle + MAX_SUBMODELS );

	return NULL;

}

clipHandle_t	CM_InlineModel( int index ) {
	if ( index < 0 || index >= cm.numSubModels ) {
		Com_Error (ERR_DROP, "CM_InlineModel: bad number");
	}
	return index;
}

int		CM_NumClusters( void ) {
	return cm.numClusters;
}

int		CM_NumInlineModels( void ) {
	return cm.numSubModels;
}

char	*CM_EntityString( void ) {
	return cm.entityString;
}

const char	*CM_MapTime( void ) {
	const char *com_token;
	char *data;

	data = CM_EntityString();
	if( !data || *data != '{' ) {
		return NULL;
	}

	while( 1 )
	{
		com_token = COM_Parse( &data );
		if( !data || *com_token == '}' ) {
			break;
		}

		if( !strcmp( com_token, "map_time" ) )
		{
			com_token = COM_Parse( &data );
			if( com_token && *com_token != '}' ) {
				return com_token;
			} else {
				return "";
			}
		}
	}

	return "";
}

int		CM_LeafCluster( int leafnum ) {
	if (leafnum < 0 || leafnum >= cm.numLeafs) {
		Com_Error (ERR_DROP, "CM_LeafCluster: bad number");
	}
	return cm.leafs[leafnum].cluster;
}

int		CM_LeafArea( int leafnum ) {
	if ( leafnum < 0 || leafnum >= cm.numLeafs ) {
		Com_Error (ERR_DROP, "CM_LeafArea: bad number");
	}
	return cm.leafs[leafnum].area;
}



void CM_InitBoxHull (void)
{
	int			i;
	int			side;
	cplane_t	*p;
	cbrushside_t	*s;

	box_planes = &cm.planes[cm.numPlanes];

	box_brush = &cm.brushes[cm.numBrushes];
	box_brush->numsides = 6;
	box_brush->sides = cm.brushsides + cm.numBrushSides;
	box_brush->contents = CONTENTS_BODY;

	box_model.leaf.numLeafBrushes = 1;
	box_model.leaf.firstLeafBrush = cm.numLeafBrushes;
	cm.leafbrushes[cm.numLeafBrushes] = cm.numBrushes;

	for (i=0 ; i<6 ; i++)
	{
		side = i&1;

		s = &cm.brushsides[cm.numBrushSides+i];
		s->plane = 	cm.planes + (cm.numPlanes+i*2+side);
		s->surfaceFlags = 0;

		p = &box_planes[i*2];
		p->type = i>>1;
		p->signbits = 0;
		VectorClear (p->normal);
		p->normal[i>>1] = 1;

		p = &box_planes[i*2+1];
		p->type = 3 + (i>>1);
		p->signbits = 0;
		VectorClear (p->normal);
		p->normal[i>>1] = -1;

		SetPlaneSignbits( p );
	}
}

clipHandle_t CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, int contents ) {
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = -maxs[0];
	box_planes[2].dist = mins[0];
	box_planes[3].dist = -mins[0];
	box_planes[4].dist = maxs[1];
	box_planes[5].dist = -maxs[1];
	box_planes[6].dist = mins[1];
	box_planes[7].dist = -mins[1];
	box_planes[8].dist = maxs[2];
	box_planes[9].dist = -maxs[2];
	box_planes[10].dist = mins[2];
	box_planes[11].dist = -mins[2];

	VectorCopy( mins, box_brush->bounds[0] );
	VectorCopy( maxs, box_brush->bounds[1] );
	box_brush->contents = contents;

	return BOX_MODEL_HANDLE;
}

void CM_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs ) {
	cmodel_t	*cmod;

	cmod = CM_ClipHandleToModel( model );
	VectorCopy( cmod->mins, mins );
	VectorCopy( cmod->maxs, maxs );
}

void CM_ModelBoundsFromName( const char *name, vec3_t mins, vec3_t maxs ) {
	clipHandle_t h;

	if( !name ) {
		Com_Error( ERR_DROP, "SV_SetBrushModel: NULL" );
	}

	if( *name != '*' ) {
		Com_Error( ERR_DROP, "SV_SetBrushModel: %s isn't a brush model", name );
	}

	h = CM_InlineModel( atoi( name + 1 ) );
	CM_ModelBounds( h, mins, maxs );
}

byte *CM_VisibilityPointer( void )
{
	if( cm.vised ) {
		return cm.visibility;
	} else {
		return NULL;
	}
}

void CM_PrintBSPFileSizes( void )
{
	Com_Printf("%6u   leafbrushes           %7zu\n", numleafbrushes, numleafbrushes * sizeof(int));
}
