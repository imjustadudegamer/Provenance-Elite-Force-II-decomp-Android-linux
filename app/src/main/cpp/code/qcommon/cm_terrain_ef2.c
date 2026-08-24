/*
===========================================================================
Reconstructed from the retail Star Trek: Voyager -- Elite Force II binaries.
The publisher released no source for this file.  This port asserts no copyright
in it and grants no rights to it: it remains the property of its respective
rights holders, and it is covered neither by the GNU GPL nor by the Ritual
Elite Force II SDK agreement.  See LICENSING.md.
===========================================================================
*/
// SPDX-License-Identifier: LicenseRef-EF2-Retail-Reconstruction

#include "cm_local.h"

#define EF2_TERRAIN_THICKNESS   2.0f

static const int ef2_terrainLevels[8] = { 0, 1, 3, 7, 15, 31, 63, 127 };

static const byte ef2_terrainTris[2][64][6] = {
	{ 
		{   0,   9,  10,   0,  10,   1 }, {   9,  18,  10,  18,  19,  10 }, {   1,  10,   2,  10,  11,   2 }, {  10,  19,  20,  10,  20,  11 },
		{  18,  27,  28,  18,  28,  19 }, {  27,  36,  28,  36,  37,  28 }, {  19,  28,  20,  28,  29,  20 }, {  28,  37,  38,  28,  38,  29 },
		{   2,  11,  12,   2,  12,   3 }, {  11,  20,  12,  20,  21,  12 }, {   3,  12,   4,  12,  13,   4 }, {  12,  21,  22,  12,  22,  13 },
		{  20,  29,  30,  20,  30,  21 }, {  29,  38,  30,  38,  39,  30 }, {  21,  30,  22,  30,  31,  22 }, {  30,  39,  40,  30,  40,  31 },
		{  36,  45,  46,  36,  46,  37 }, {  45,  54,  46,  54,  55,  46 }, {  37,  46,  38,  46,  47,  38 }, {  46,  55,  56,  46,  56,  47 },
		{  54,  63,  64,  54,  64,  55 }, {  63,  72,  64,  72,  73,  64 }, {  55,  64,  56,  64,  65,  56 }, {  64,  73,  74,  64,  74,  65 },
		{  38,  47,  48,  38,  48,  39 }, {  47,  56,  48,  56,  57,  48 }, {  39,  48,  40,  48,  49,  40 }, {  48,  57,  58,  48,  58,  49 },
		{  56,  65,  66,  56,  66,  57 }, {  65,  74,  66,  74,  75,  66 }, {  57,  66,  58,  66,  67,  58 }, {  66,  75,  76,  66,  76,  67 },
		{   4,  13,  14,   4,  14,   5 }, {  13,  22,  14,  22,  23,  14 }, {   5,  14,   6,  14,  15,   6 }, {  14,  23,  24,  14,  24,  15 },
		{  22,  31,  32,  22,  32,  23 }, {  31,  40,  32,  40,  41,  32 }, {  23,  32,  24,  32,  33,  24 }, {  32,  41,  42,  32,  42,  33 },
		{   6,  15,  16,   6,  16,   7 }, {  15,  24,  16,  24,  25,  16 }, {   7,  16,   8,  16,  17,   8 }, {  16,  25,  26,  16,  26,  17 },
		{  24,  33,  34,  24,  34,  25 }, {  33,  42,  34,  42,  43,  34 }, {  25,  34,  26,  34,  35,  26 }, {  34,  43,  44,  34,  44,  35 },
		{  40,  49,  50,  40,  50,  41 }, {  49,  58,  50,  58,  59,  50 }, {  41,  50,  42,  50,  51,  42 }, {  50,  59,  60,  50,  60,  51 },
		{  58,  67,  68,  58,  68,  59 }, {  67,  76,  68,  76,  77,  68 }, {  59,  68,  60,  68,  69,  60 }, {  68,  77,  78,  68,  78,  69 },
		{  42,  51,  52,  42,  52,  43 }, {  51,  60,  52,  60,  61,  52 }, {  43,  52,  44,  52,  53,  44 }, {  52,  61,  62,  52,  62,  53 },
		{  60,  69,  70,  60,  70,  61 }, {  69,  78,  70,  78,  79,  70 }, {  61,  70,  62,  70,  71,  62 }, {  70,  79,  80,  70,  80,  71 }
	},
	{ 
		{   0,  10,   9,   0,   1,  10 }, {   9,  10,  18,  18,  10,  19 }, {   1,   2,  10,  10,   2,  11 }, {  10,  20,  19,  10,  11,  20 },
		{  18,  28,  27,  18,  19,  28 }, {  27,  28,  36,  36,  28,  37 }, {  19,  20,  28,  28,  20,  29 }, {  28,  38,  37,  28,  29,  38 },
		{   2,  12,  11,   2,   3,  12 }, {  11,  12,  20,  20,  12,  21 }, {   3,   4,  12,  12,   4,  13 }, {  12,  22,  21,  12,  13,  22 },
		{  20,  30,  29,  20,  21,  30 }, {  29,  30,  38,  38,  30,  39 }, {  21,  22,  30,  30,  22,  31 }, {  30,  40,  39,  30,  31,  40 },
		{  36,  46,  45,  36,  37,  46 }, {  45,  46,  54,  54,  46,  55 }, {  37,  38,  46,  46,  38,  47 }, {  46,  56,  55,  46,  47,  56 },
		{  54,  64,  63,  54,  55,  64 }, {  63,  64,  72,  72,  64,  73 }, {  55,  56,  64,  64,  56,  65 }, {  64,  74,  73,  64,  65,  74 },
		{  38,  48,  47,  38,  39,  48 }, {  47,  48,  56,  56,  48,  57 }, {  39,  40,  48,  48,  40,  49 }, {  48,  58,  57,  48,  49,  58 },
		{  56,  66,  65,  56,  57,  66 }, {  65,  66,  74,  74,  66,  75 }, {  57,  58,  66,  66,  58,  67 }, {  66,  76,  75,  66,  67,  76 },
		{   4,  14,  13,   4,   5,  14 }, {  13,  14,  22,  22,  14,  23 }, {   5,   6,  14,  14,   6,  15 }, {  14,  24,  23,  14,  15,  24 },
		{  22,  32,  31,  22,  23,  32 }, {  31,  32,  40,  40,  32,  41 }, {  23,  24,  32,  32,  24,  33 }, {  32,  42,  41,  32,  33,  42 },
		{   6,  16,  15,   6,   7,  16 }, {  15,  16,  24,  24,  16,  25 }, {   7,   8,  16,  16,   8,  17 }, {  16,  26,  25,  16,  17,  26 },
		{  24,  34,  33,  24,  25,  34 }, {  33,  34,  42,  42,  34,  43 }, {  25,  26,  34,  34,  26,  35 }, {  34,  44,  43,  34,  35,  44 },
		{  40,  50,  49,  40,  41,  50 }, {  49,  50,  58,  58,  50,  59 }, {  41,  42,  50,  50,  42,  51 }, {  50,  60,  59,  50,  51,  60 },
		{  58,  68,  67,  58,  59,  68 }, {  67,  68,  76,  76,  68,  77 }, {  59,  60,  68,  68,  60,  69 }, {  68,  78,  77,  68,  69,  78 },
		{  42,  52,  51,  42,  43,  52 }, {  51,  52,  60,  60,  52,  61 }, {  43,  44,  52,  52,  44,  53 }, {  52,  62,  61,  52,  53,  62 },
		{  60,  70,  69,  60,  61,  70 }, {  69,  70,  78,  78,  70,  79 }, {  61,  62,  70,  70,  62,  71 }, {  70,  80,  79,  70,  71,  80 }
	}
};

static const byte ef2_terrainFaces[64][2] = {
	{   1,   0 }, {  16,  17 }, {   2,   3 }, {  19,  18 }, {  33,  32 }, {  48,  49 }, {  34,  35 }, {  51,  50 },
	{   5,   4 }, {  20,  21 }, {   6,   7 }, {  23,  22 }, {  37,  36 }, {  52,  53 }, {  38,  39 }, {  55,  54 },
	{  65,  64 }, {  80,  81 }, {  66,  67 }, {  83,  82 }, {  97,  96 }, { 112, 113 }, {  98,  99 }, { 115, 114 },
	{  69,  68 }, {  84,  85 }, {  70,  71 }, {  87,  86 }, { 101, 100 }, { 116, 117 }, { 102, 103 }, { 119, 118 },
	{   9,   8 }, {  24,  25 }, {  10,  11 }, {  27,  26 }, {  41,  40 }, {  56,  57 }, {  42,  43 }, {  59,  58 },
	{  13,  12 }, {  28,  29 }, {  14,  15 }, {  31,  30 }, {  45,  44 }, {  60,  61 }, {  46,  47 }, {  63,  62 },
	{  73,  72 }, {  88,  89 }, {  74,  75 }, {  91,  90 }, { 105, 104 }, { 120, 121 }, { 106, 107 }, { 123, 122 },
	{  77,  76 }, {  92,  93 }, {  78,  79 }, {  95,  94 }, { 109, 108 }, { 124, 125 }, { 110, 111 }, { 127, 126 }
};

static cplane_t		ef2_boxPlanes[6];
static cbrushside_t	ef2_boxSides[6];
static cbrush_t		ef2_boxBrush;

static cplane_t		ef2_triPlanes[6];
static cbrushside_t	ef2_triSides[6];
static cbrush_t		ef2_triBrush;

void CM_Terrain_InitializeEF2( void ) {
	int i;

	VectorSet( ef2_boxPlanes[0].normal,  0,  0,  1 ); ef2_boxPlanes[0].type = 2; ef2_boxPlanes[0].signbits = 0;
	VectorSet( ef2_boxPlanes[1].normal,  0,  0, -1 ); ef2_boxPlanes[1].type = 2; ef2_boxPlanes[1].signbits = 4;
	VectorSet( ef2_boxPlanes[2].normal, -1,  0,  0 ); ef2_boxPlanes[2].type = 3; ef2_boxPlanes[2].signbits = 1;
	VectorSet( ef2_boxPlanes[3].normal,  0, -1,  0 ); ef2_boxPlanes[3].type = 3; ef2_boxPlanes[3].signbits = 2;
	VectorSet( ef2_boxPlanes[4].normal,  1,  0,  0 ); ef2_boxPlanes[4].type = 0; ef2_boxPlanes[4].signbits = 0;
	VectorSet( ef2_boxPlanes[5].normal,  0,  1,  0 ); ef2_boxPlanes[5].type = 1; ef2_boxPlanes[5].signbits = 0;

	for ( i = 0; i < 6; i++ ) {
		ef2_boxSides[i].plane        = &ef2_boxPlanes[i];
		ef2_boxSides[i].surfaceFlags = 0;
		ef2_boxSides[i].shaderNum    = 0;

		ef2_triSides[i].plane        = &ef2_triPlanes[i];
		ef2_triSides[i].surfaceFlags = 0;
		ef2_triSides[i].shaderNum    = 0;
	}

	Com_Memset( &ef2_boxBrush, 0, sizeof( ef2_boxBrush ) );
	ef2_boxBrush.numsides = 6;
	ef2_boxBrush.sides    = ef2_boxSides;

	Com_Memset( &ef2_triBrush, 0, sizeof( ef2_triBrush ) );
	ef2_triBrush.numsides = 5;
	ef2_triBrush.sides    = ef2_triSides;
}

static ID_INLINE qboolean CM_Terrain_FaceIsHole( const int holes[4], int face ) {
	return ( holes[face >> 5] & ( 1 << ( face & 31 ) ) ) != 0;
}

static void CM_Terrain_CalculateAABBBounds( cTerrainEF2_t *t, int nodeNum,
		byte *oMinXY, byte *oMinZ, byte *oMaxXY, byte *oMaxZ ) {
	ef2TerrainNode_t	*node = &t->nodes[nodeNum];
	const byte			*tri  = &node->tri[0][0];
	int					i;

	if ( node->children[0] == EF2_TERRAIN_NONE && node->children[1] == EF2_TERRAIN_NONE ) {
		node->bounds[0] = node->bounds[1] = node->bounds[2] = node->bounds[3] = tri[0];

		for ( i = 0; i < 6; i++ ) {
			byte v = tri[i];

			if ( t->pts[v][0] < t->pts[node->bounds[0]][0]
			  || t->pts[v][1] < t->pts[node->bounds[0]][1] ) {
				node->bounds[0] = v;
			}
			if ( t->pts[v][2] < t->pts[node->bounds[1]][2] ) {
				node->bounds[1] = v;
			}
			if ( t->pts[v][0] > t->pts[node->bounds[2]][0]
			  || t->pts[v][1] > t->pts[node->bounds[2]][1] ) {
				node->bounds[2] = v;
			}
			if ( t->pts[v][2] > t->pts[node->bounds[3]][2] ) {
				node->bounds[3] = v;
			}
		}
	} else {
		byte a0, a1, a2, a3, b0, b1, b2, b3;

		CM_Terrain_CalculateAABBBounds( t, node->children[0], &a0, &a1, &a2, &a3 );
		CM_Terrain_CalculateAABBBounds( t, node->children[1], &b0, &b1, &b2, &b3 );

		node->bounds[0] = ( t->pts[a0][0] >= t->pts[b0][0] && t->pts[a0][1] >= t->pts[b0][1] ) ? b0 : a0;
		node->bounds[1] = ( t->pts[a1][2] >= t->pts[b1][2] ) ? b1 : a1;
		node->bounds[2] = ( t->pts[a2][0] <= t->pts[b2][0] && t->pts[a2][1] <= t->pts[b2][1] ) ? b2 : a2;
		node->bounds[3] = ( t->pts[a3][2] <= t->pts[b3][2] ) ? b3 : a3;
	}

	if ( oMinXY && oMinZ && oMaxXY && oMaxZ ) {
		*oMinXY = node->bounds[0];
		*oMinZ  = node->bounds[1];
		*oMaxXY = node->bounds[2];
		*oMaxZ  = node->bounds[3];
	}
}

static qboolean CM_Terrain_MarkEmptyBoxes( cTerrainEF2_t *t, int nodeNum ) {
	ef2TerrainNode_t *node = &t->nodes[nodeNum];

	if ( node->children[0] == EF2_TERRAIN_NONE && node->children[1] == EF2_TERRAIN_NONE ) {
		if ( node->tri[0][0] != EF2_TERRAIN_NONE || node->tri[1][0] != EF2_TERRAIN_NONE ) {
			return qtrue;
		}
	} else {
		qboolean left  = CM_Terrain_MarkEmptyBoxes( t, node->children[0] );
		qboolean right = CM_Terrain_MarkEmptyBoxes( t, node->children[1] );

		if ( left || right ) {
			return qtrue;
		}
	}

	node->bounds[0] = node->bounds[1] = node->bounds[2] = node->bounds[3] = EF2_TERRAIN_NONE;
	return qfalse;
}

void CM_Terrain_BuildAABBTreeEF2( cTerrainEF2_t *t, int flip, const int holes[4] ) {
	const byte	(*tris)[6];
	int			level, n, child;

	tris = ef2_terrainTris[ flip ? 1 : 0 ];
	if ( flip ) {
		t->flipFlag = 1;
	}

	for ( level = 0; level < 7; level++ ) {
		int start = ef2_terrainLevels[level];
		int end   = ef2_terrainLevels[level + 1];

		child = 0;
		for ( n = start; n < end; n++ ) {
			ef2TerrainNode_t *node = &t->nodes[n];

			if ( level > 5 ) {
				node->children[0] = EF2_TERRAIN_NONE;
				node->children[1] = EF2_TERRAIN_NONE;
			} else {
				node->children[0] = (byte)( end + child );
				node->children[1] = (byte)( end + child + 1 );
				child += 2;
			}
			Com_Memset( node->tri, EF2_TERRAIN_NONE, sizeof( node->tri ) );
		}
	}

	for ( n = ef2_terrainLevels[6]; n < ef2_terrainLevels[7]; n++ ) {
		Com_Memcpy( t->nodes[n].tri, tris[n - ef2_terrainLevels[6]], 6 );
	}

	CM_Terrain_CalculateAABBBounds( t, 0, NULL, NULL, NULL, NULL );

	for ( n = ef2_terrainLevels[6]; n < ef2_terrainLevels[7]; n++ ) {
		const byte *faces = ef2_terrainFaces[n - ef2_terrainLevels[6]];

		if ( CM_Terrain_FaceIsHole( holes, faces[0] ) ) {
			Com_Memset( t->nodes[n].tri[0], EF2_TERRAIN_NONE, 3 );
		}
		if ( CM_Terrain_FaceIsHole( holes, faces[1] ) ) {
			Com_Memset( t->nodes[n].tri[1], EF2_TERRAIN_NONE, 3 );
		}
	}

	CM_Terrain_MarkEmptyBoxes( t, 0 );
}

static void CM_Terrain_TraceThroughLeafTriangles( traceWork_t *tw, cTerrainEF2_t *t, int nodeNum,
		const vec3_t mins, const vec3_t maxs ) {
	ef2TerrainNode_t	*node = &t->nodes[nodeNum];
	int					h, i;

	VectorCopy( mins, ef2_triBrush.bounds[0] );
	VectorCopy( maxs, ef2_triBrush.bounds[1] );
	ef2_triBrush.numsides   = 5;
	ef2_triBrush.checkcount = 0;
	ef2_triBrush.contents   = t->contents;
	ef2_triBrush.shaderNum  = t->shaderNum;

	for ( i = 0; i < 6; i++ ) {
		ef2_triSides[i].surfaceFlags = t->surfaceFlags;
		ef2_triSides[i].shaderNum    = t->shaderNum;
	}

	for ( h = 0; h < 2; h++ ) {
		const byte	*tri = node->tri[h];
		int			atMinX = 0, atMaxX = 0, atMinY = 0, atMaxY = 0;
		vec3_t		u, v;
		int			np;

		if ( tri[0] == EF2_TERRAIN_NONE ) {
			continue;
		}

		for ( i = 0; i < 3; i++ ) {
			const float *p = t->pts[tri[i]];

			if ( mins[0] == p[0] ) atMinX++;
			if ( maxs[0] == p[0] ) atMaxX++;
			if ( mins[1] == p[1] ) atMinY++;
			if ( maxs[1] == p[1] ) atMaxY++;
		}

		VectorSubtract( t->pts[tri[2]], t->pts[tri[0]], u );
		VectorSubtract( t->pts[tri[1]], t->pts[tri[0]], v );
		CrossProduct( u, v, ef2_triPlanes[0].normal );
		VectorNormalize( ef2_triPlanes[0].normal );
		ef2_triPlanes[0].dist = DotProduct( ef2_triPlanes[0].normal, t->pts[tri[0]] );
		ef2_triPlanes[0].type = 3;
		SetPlaneSignbits( &ef2_triPlanes[0] );

		if ( t->flipFlag ) {
			VectorSet( ef2_triPlanes[1].normal, 0, 0, 1 );
			ef2_triPlanes[1].dist     = maxs[2];
			ef2_triPlanes[1].signbits = 0;
		} else {
			VectorSet( ef2_triPlanes[1].normal, 0, 0, -1 );
			ef2_triPlanes[1].dist     = -mins[2];
			ef2_triPlanes[1].signbits = 4;
		}
		ef2_triPlanes[1].type = 2;

		np = 2;
		if ( atMinX == 2 ) {
			VectorSet( ef2_triPlanes[np].normal, -1, 0, 0 );
			ef2_triPlanes[np].dist     = -mins[0];
			ef2_triPlanes[np].type     = 0;
			ef2_triPlanes[np].signbits = 1;
			np++;
		}
		if ( atMinY == 2 ) {
			VectorSet( ef2_triPlanes[np].normal, 0, -1, 0 );
			ef2_triPlanes[np].dist     = -mins[1];
			ef2_triPlanes[np].type     = 1;
			ef2_triPlanes[np].signbits = 2;
			np++;
		}
		if ( atMaxX == 2 ) {
			VectorSet( ef2_triPlanes[np].normal, 1, 0, 0 );
			ef2_triPlanes[np].dist     = maxs[0];
			ef2_triPlanes[np].type     = 0;
			ef2_triPlanes[np].signbits = 0;
			np++;
		}
		if ( atMaxY == 2 ) {
			VectorSet( ef2_triPlanes[np].normal, 0, 1, 0 );
			ef2_triPlanes[np].dist     = maxs[1];
			ef2_triPlanes[np].type     = 1;
			ef2_triPlanes[np].signbits = 0;
			np++;
		}

		if ( np > 5 ) {
			continue;               
		}
		if ( atMinX == 2 && atMaxY == 2 ) {
			VectorSet( ef2_triPlanes[np].normal, 0.70710677f, -0.70710677f, 0 );
			ef2_triPlanes[np].signbits = 2;
			ef2_triPlanes[np].dist     = ef2_triPlanes[np].normal[0] * maxs[0]
			                           + ef2_triPlanes[np].normal[1] * maxs[1]
			                           + ef2_triPlanes[np].normal[2] * maxs[2];
		} else if ( atMinY == 2 && atMaxX == 2 ) {
			VectorSet( ef2_triPlanes[np].normal, -0.70710677f, 0.70710677f, 0 );
			ef2_triPlanes[np].signbits = 1;
			ef2_triPlanes[np].dist     = ef2_triPlanes[np].normal[0] * maxs[0]
			                           + ef2_triPlanes[np].normal[1] * maxs[1]
			                           + ef2_triPlanes[np].normal[2] * maxs[2];
		} else if ( atMaxX == 2 && atMaxY == 2 ) {
			VectorSet( ef2_triPlanes[np].normal, -0.70710677f, -0.70710677f, 0 );
			ef2_triPlanes[np].signbits = 3;
			ef2_triPlanes[np].dist     = ef2_triPlanes[np].normal[0] * maxs[0]
			                           + ef2_triPlanes[np].normal[1] * mins[1]
			                           + ef2_triPlanes[np].normal[2] * mins[2];
		} else if ( atMinX == 2 && atMinY == 2 ) {
			VectorSet( ef2_triPlanes[np].normal, 0.70710677f, 0.70710677f, 0 );
			ef2_triPlanes[np].signbits = 0;
			ef2_triPlanes[np].dist     = ef2_triPlanes[np].normal[0] * maxs[0]
			                           + ef2_triPlanes[np].normal[1] * mins[1]
			                           + ef2_triPlanes[np].normal[2] * mins[2];
		} else {
			continue;               
		}
		ef2_triPlanes[np].type = 3;

		CM_TraceThroughBrush( tw, &ef2_triBrush );
	}
}

static void CM_Terrain_TraceThroughTerrainAABB( traceWork_t *tw, cTerrainEF2_t *t, int nodeNum ) {
	ef2TerrainNode_t	*node = &t->nodes[nodeNum];
	vec3_t				mins, maxs;
	traceWork_t			scratch;
	int					i;

	if ( node->bounds[0] == EF2_TERRAIN_NONE ) {
		return;						
	}

	mins[0] = t->pts[node->bounds[0]][0];
	mins[1] = t->pts[node->bounds[0]][1];
	mins[2] = t->pts[node->bounds[1]][2];
	maxs[0] = t->pts[node->bounds[2]][0];
	maxs[1] = t->pts[node->bounds[2]][1];
	maxs[2] = t->pts[node->bounds[3]][2];

	if ( t->flipFlag ) {
		maxs[2] += EF2_TERRAIN_THICKNESS;
	} else {
		mins[2] -= EF2_TERRAIN_THICKNESS;
	}

	for ( i = 0; i < 3; i++ ) {
		if ( tw->bounds[0][i] > maxs[i] || tw->bounds[1][i] < mins[i] ) {
			return;
		}
	}

	VectorCopy( mins, ef2_boxBrush.bounds[0] );
	VectorCopy( maxs, ef2_boxBrush.bounds[1] );
	ef2_boxBrush.numsides   = 6;
	ef2_boxBrush.checkcount = 0;
	ef2_boxBrush.contents   = t->contents;
	ef2_boxBrush.shaderNum  = t->shaderNum;

	ef2_boxPlanes[0].dist =  maxs[2];
	ef2_boxPlanes[1].dist = -mins[2];
	ef2_boxPlanes[2].dist = -mins[0];
	ef2_boxPlanes[3].dist = -mins[1];
	ef2_boxPlanes[4].dist =  maxs[0];
	ef2_boxPlanes[5].dist =  maxs[1];

	for ( i = 0; i < 6; i++ ) {
		ef2_boxSides[i].surfaceFlags = t->surfaceFlags;
		ef2_boxSides[i].shaderNum    = t->shaderNum;
	}

	scratch = *tw;
	CM_TraceThroughBrush( &scratch, &ef2_boxBrush );
	if ( scratch.trace.fraction == tw->trace.fraction && !scratch.trace.startsolid ) {
		return;						
	}

	if ( node->children[0] == EF2_TERRAIN_NONE ) {
		CM_Terrain_TraceThroughLeafTriangles( tw, t, nodeNum, mins, maxs );
	} else {
		CM_Terrain_TraceThroughTerrainAABB( tw, t, node->children[0] );
		CM_Terrain_TraceThroughTerrainAABB( tw, t, node->children[1] );
	}
}

void CM_TraceThroughTerrainEF2( traceWork_t *tw, cTerrainEF2_t *terrain ) {
	float oldFrac = tw->trace.fraction;

	CM_Terrain_TraceThroughTerrainAABB( tw, terrain, 0 );

	if ( tw->trace.fraction < oldFrac ) {
		tw->trace.surfaceFlags = terrain->surfaceFlags;
		tw->trace.contents     = terrain->contents;
	}
}
