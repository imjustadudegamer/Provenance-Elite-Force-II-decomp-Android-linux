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
#include "light.h"



#define	CURVE_FACET_ERROR	8

int				c_totalTrace;
int				c_cullTrace, c_testTrace;
int				c_testFacets;

surfaceTest_t	*surfaceTest[MAX_MAP_DRAW_SURFS];

void CM_GenerateBoundaryForPoints( float boundary[4], float plane[4], vec3_t a, vec3_t b ) {
	vec3_t	d1;

	VectorSubtract( b, a, d1 );
	CrossProduct( plane, d1, boundary );
	VectorNormalize( boundary, boundary );
	boundary[3] = DotProduct( a, boundary );
}

void TextureMatrixFromPoints( cFacet_t *f, drawVert_t *a, drawVert_t *b, drawVert_t *c ) {
	int			i, j;
	float		t;
	float		m[3][4];
	float		s;

	for ( i = 0 ; i < 2 ; i++ ) {

		m[0][0] = a->xyz[0];
		m[0][1] = a->xyz[1];
		m[0][2] = a->xyz[2];
		m[0][3] = a->st[i];

		m[1][0] = b->xyz[0];
		m[1][1] = b->xyz[1];
		m[1][2] = b->xyz[2];
		m[1][3] = b->st[i];

		m[2][0] = c->xyz[0];
		m[2][1] = c->xyz[1];
		m[2][2] = c->xyz[2];
		m[2][3] = c->st[i];

		if ( fabs(m[1][0]) > fabs(m[0][0]) && fabs(m[1][0]) > fabs(m[2][0]) ) {
			for ( j = 0 ; j < 4 ; j ++ ) {
				t = m[0][j];
				m[0][j] = m[1][j];
				m[1][j] = t;
			}
		} else if ( fabs(m[2][0]) > fabs(m[0][0]) && fabs(m[2][0]) > fabs(m[1][0]) ) {
			for ( j = 0 ; j < 4 ; j ++ ) {
				t = m[0][j];
				m[0][j] = m[2][j];
				m[2][j] = t;
			}
		}

		s = 1.0 / m[0][0];
		m[0][0] *= s;
		m[0][1] *= s;
		m[0][2] *= s;
		m[0][3] *= s;

		s = m[1][0];
		m[1][0] -= m[0][0] * s;
		m[1][1] -= m[0][1] * s;
		m[1][2] -= m[0][2] * s;
		m[1][3] -= m[0][3] * s;

		s = m[2][0];
		m[2][0] -= m[0][0] * s;
		m[2][1] -= m[0][1] * s;
		m[2][2] -= m[0][2] * s;
		m[2][3] -= m[0][3] * s;

		if ( fabs(m[2][1]) > fabs(m[1][1]) ) {
			for ( j = 0 ; j < 4 ; j ++ ) {
				t = m[1][j];
				m[1][j] = m[2][j];
				m[2][j] = t;
			}
		}

		s = 1.0 / m[1][1];
		m[1][0] *= s;
		m[1][1] *= s;
		m[1][2] *= s;
		m[1][3] *= s;

		s = m[2][1];
		m[2][0] -= m[1][0] * s;
		m[2][1] -= m[1][1] * s;
		m[2][2] -= m[1][2] * s;
		m[2][3] -= m[1][3] * s;

		s = 1.0 / m[2][2];
		m[2][0] *= s;
		m[2][1] *= s;
		m[2][2] *= s;
		m[2][3] *= s;

		f->textureMatrix[i][2] = m[2][3];
		f->textureMatrix[i][1] = m[1][3] - f->textureMatrix[i][2] * m[1][2];
		f->textureMatrix[i][0] = m[0][3] - f->textureMatrix[i][2] * m[0][2] - f->textureMatrix[i][1] * m[0][1];

		f->textureMatrix[i][3] = 0;
	}
}

qboolean CM_GenerateFacetFor3Points( cFacet_t *f, drawVert_t *a, drawVert_t *b, drawVert_t *c ) {
	if ( !PlaneFromPoints( f->surface, a->xyz, b->xyz, c->xyz ) ) {
		f->numBoundaries = 0;
		return qfalse;
	}

	f->numBoundaries = 3;

	CM_GenerateBoundaryForPoints( f->boundaries[0], f->surface, a->xyz, b->xyz );
	CM_GenerateBoundaryForPoints( f->boundaries[1], f->surface, b->xyz, c->xyz );
	CM_GenerateBoundaryForPoints( f->boundaries[2], f->surface, c->xyz, a->xyz );

	VectorCopy( a->xyz, f->points[0] );
	VectorCopy( b->xyz, f->points[1] );
	VectorCopy( c->xyz, f->points[2] );

	TextureMatrixFromPoints( f, a, b, c );

	return qtrue;
}

#define	PLANAR_EPSILON	0.1
qboolean CM_GenerateFacetFor4Points( cFacet_t *f, drawVert_t *a, drawVert_t *b, drawVert_t *c, drawVert_t *d ) {
	float	dist;
	int		i;
	vec4_t	plane;

	if ( !PlaneFromPoints( f->surface, a->xyz, b->xyz, c->xyz ) ) {
		f->numBoundaries = 0;
		return qfalse;
	}

	dist = DotProduct( d->xyz, f->surface ) - f->surface[3];
	if ( fabs( dist ) > PLANAR_EPSILON ) {
		f->numBoundaries = 0;
		return qfalse;
	}

	f->numBoundaries = 4;

	CM_GenerateBoundaryForPoints( f->boundaries[0], f->surface, a->xyz, b->xyz );
	CM_GenerateBoundaryForPoints( f->boundaries[1], f->surface, b->xyz, c->xyz );
	CM_GenerateBoundaryForPoints( f->boundaries[2], f->surface, c->xyz, d->xyz );
	CM_GenerateBoundaryForPoints( f->boundaries[3], f->surface, d->xyz, a->xyz );

	VectorCopy( a->xyz, f->points[0] );
	VectorCopy( b->xyz, f->points[1] );
	VectorCopy( c->xyz, f->points[2] );
	VectorCopy( d->xyz, f->points[3] );

	for (i = 1; i < 4; i++)
	{
		if ( !PlaneFromPoints( plane, f->points[i], f->points[(i+1) % 4], f->points[(i+2) % 4]) ) {
			f->numBoundaries = 0;
			return qfalse;
		}

		if (DotProduct(f->surface, plane) < 0.9) {
			f->numBoundaries = 0;
			return qfalse;
		}
	}

	TextureMatrixFromPoints( f, a, b, c );

	return qtrue;
}




void SphereFromBounds( vec3_t mins, vec3_t maxs, vec3_t origin, float *radius ) {
	vec3_t		temp;

	VectorAdd( mins, maxs, origin );
	VectorScale( origin, 0.5, origin );
	VectorSubtract( maxs, origin, temp );
	*radius = VectorLength( temp );
}


void FacetsForTriangleSurface( dsurface_t *dsurf, shaderInfo_t *si, surfaceTest_t *test ) {
	int			i;
	drawVert_t	*v1, *v2, *v3, *v4;
	int			count;
	int			i1, i2, i3, i4, i5, i6;

	test->patch = qfalse;
	test->numFacets = dsurf->numIndexes / 3;
	test->facets = malloc( sizeof( test->facets[0] ) * test->numFacets );
	test->shader = si;

	count = 0;
	for ( i = 0 ; i < test->numFacets ; i++ ) {
		i1 = drawIndexes[ dsurf->firstIndex + i*3 ];
		i2 = drawIndexes[ dsurf->firstIndex + i*3 + 1 ];
		i3 = drawIndexes[ dsurf->firstIndex + i*3 + 2 ];

		v1 = &drawVerts[ dsurf->firstVert + i1 ];
		v2 = &drawVerts[ dsurf->firstVert + i2 ];
		v3 = &drawVerts[ dsurf->firstVert + i3 ];

		if ( i != test->numFacets - 1 ) {
			i4 = drawIndexes[ dsurf->firstIndex + i*3 + 3 ];
			i5 = drawIndexes[ dsurf->firstIndex + i*3 + 4 ];
			i6 = drawIndexes[ dsurf->firstIndex + i*3 + 5 ];
			if ( i4 == i3 && i5 == i2 ) {
				v4 = &drawVerts[ dsurf->firstVert + i6 ];
				if ( CM_GenerateFacetFor4Points( &test->facets[count], v1, v2, v4, v3 ) ) {
					count++;
					i++;		
					continue;
				}
			}
		}

		if (CM_GenerateFacetFor3Points( &test->facets[count], v1, v2, v3 ))
			count++;
	}		

	test->numFacets = count;
}

void FacetsForPatch( dsurface_t *dsurf, shaderInfo_t *si, surfaceTest_t *test ) {
	int			i, j;
	drawVert_t	*v1, *v2, *v3, *v4;
	int			count;
	mesh_t		srcMesh, *subdivided, *mesh;

	srcMesh.width = dsurf->patchWidth;
	srcMesh.height = dsurf->patchHeight;
	srcMesh.verts = &drawVerts[ dsurf->firstVert ];

	mesh = SubdivideMesh( srcMesh, 8, 999 );
	PutMeshOnCurve( *mesh );
	MakeMeshNormals( *mesh );

	subdivided = RemoveLinearMeshColumnsRows( mesh );
	FreeMesh(mesh);

	test->patch = qtrue;
	test->numFacets = ( subdivided->width - 1 ) * ( subdivided->height - 1 ) * 2;
	test->facets = malloc( sizeof( test->facets[0] ) * test->numFacets );
	test->shader = si;

	count = 0;
	for ( i = 0 ; i < subdivided->width - 1 ; i++ ) {
		for ( j = 0 ; j < subdivided->height - 1 ; j++ ) {

			v1 = subdivided->verts + j * subdivided->width + i;
			v2 = v1 + 1;
			v3 = v1 + subdivided->width + 1;
			v4 = v1 + subdivided->width;

			if ( CM_GenerateFacetFor4Points( &test->facets[count], v1, v4, v3, v2 ) ) {
				count++;
			} else {
				if (CM_GenerateFacetFor3Points( &test->facets[count], v1, v4, v3 ))
					count++;
				if (CM_GenerateFacetFor3Points( &test->facets[count], v1, v3, v2 ))
					count++;
			}
		}
	}
	test->numFacets = count;
	FreeMesh(subdivided);
}


void InitSurfacesForTesting( void ) {

	int				i, j;
	dsurface_t		*dsurf;
	surfaceTest_t	*test;
	drawVert_t		*dvert;
	shaderInfo_t	*si;

	for ( i = 0 ; i < numDrawSurfaces ; i++ ) {
		dsurf = &drawSurfaces[ i ];
		if ( !dsurf->numIndexes && !dsurf->patchWidth ) {
			continue;
		}

		si = ShaderInfoForShader( dshaders[ dsurf->shaderNum].shader );
		if ( (si->contents & CONTENTS_TRANSLUCENT) && !(si->surfaceFlags & SURF_ALPHASHADOW) ) {
			continue;
		}

		test = malloc( sizeof( *test ) );
		surfaceTest[i] = test;
		ClearBounds( test->mins, test->maxs );

		dvert = &drawVerts[ dsurf->firstVert ];
		for ( j = 0 ; j < dsurf->numVerts ; j++, dvert++ ) {
			AddPointToBounds( dvert->xyz, test->mins, test->maxs );
		}

		SphereFromBounds( test->mins, test->maxs, test->origin, &test->radius );

		if ( dsurf->surfaceType == MST_TRIANGLE_SOUP || dsurf->surfaceType == MST_PLANAR ) {
			FacetsForTriangleSurface( dsurf, si, test );
		} else if ( dsurf->surfaceType == MST_PATCH ) {
			FacetsForPatch( dsurf, si, test );
		}
	}
}


void GenerateBoundaryForPoints( float boundary[4], float plane[4], vec3_t a, vec3_t b ) {
	vec3_t	d1;

	VectorSubtract( b, a, d1 );
	CrossProduct( plane, d1, boundary );
	VectorNormalize( boundary, boundary );
	boundary[3] = DotProduct( a, boundary );
}


void SetFacetFilter( traceWork_t *tr, shaderInfo_t *shader, cFacet_t *facet, vec3_t point ) {
	float	s, t;
	int		is, it;
	byte	*image;
	int		b;

	if ( !(shader->surfaceFlags & SURF_ALPHASHADOW) ) {
		VectorClear( tr->trace->filter );
		return;
	}

	s = DotProduct( point, facet->textureMatrix[0] ) + facet->textureMatrix[0][3];
	t = DotProduct( point, facet->textureMatrix[1] ) + facet->textureMatrix[1][3];

	if ( !shader->pixels ) {
		VectorClear( point );
		return;
	}

	s = s - floor( s );
	t = t - floor( t );

	is = s * shader->width;
	it = t * shader->height;

	image = shader->pixels + 4 * ( it * shader->width + is );

	b = image[3];

	b = b < 128 ? 0 : 255;

	tr->trace->filter[0] = tr->trace->filter[0] * (255-b) / 255;
	tr->trace->filter[1] = tr->trace->filter[1] * (255-b) / 255;
	tr->trace->filter[2] = tr->trace->filter[2] * (255-b) / 255;
}


void TraceAgainstFacet( traceWork_t *tr, shaderInfo_t *shader, cFacet_t *facet ) {
	int			j;
	float		d1, d2, d, f;
	vec3_t		point;
	float		dist;

	if ( facet->numBoundaries < 3 ) {
		return;
	}

	dist = facet->surface[3];

	d1 = DotProduct( tr->start, facet->surface ) - dist;
	if ( d1 > -1 && d1 < 1 ) {
		return;		
	}
	d2 = DotProduct( tr->end, facet->surface ) - dist;
	if ( d2 > -1 && d2 < 1 ) {
		return;		
	}

	f = ( d1 - ON_EPSILON ) / ( d1 - d2 );
	if ( f <= 0 ) {
		return;
	}
	if ( f >= tr->trace->hitFraction ) {
		return;			
	}

	for ( j = 0 ; j < 3 ; j++ ) {
		point[j] = tr->start[j] + f * ( tr->end[j] - tr->start[j] );
	}

	for ( j = 0 ; j < facet->numBoundaries ; j++ ) {
		dist = facet->boundaries[j][3];

		d = DotProduct( point, facet->boundaries[j] );
		if ( d > dist + ON_EPSILON ) {
			break;		
		}
	}

	if ( j != facet->numBoundaries ) {
		return;			
	}


	if ( shader->surfaceFlags & SURF_ALPHASHADOW ) {
		SetFacetFilter( tr, shader, facet, point );
	} else {
		VectorClear( tr->trace->filter );
		tr->trace->hitFraction = f;
	}

}




#define	TRACE_ON_EPSILON	0.1

typedef struct tnode_s
{
	int		type;
	vec3_t	normal;
	float	dist;
	int		children[2];
	int		planeNum;
} tnode_t;

#define	MAX_TNODES	(MAX_MAP_NODES*4)
tnode_t		*tnodes, *tnode_p;

void MakeTnode (int nodenum)
{
	tnode_t			*t;
	dplane_t		*plane;
	int				i;
	dnode_t 		*node;
	int				leafNum;

	t = tnode_p++;

	node = dnodes + nodenum;
	plane = dplanes + node->planeNum;

	t->planeNum = node->planeNum;
	t->type = PlaneTypeForNormal( plane->normal );
	VectorCopy (plane->normal, t->normal);
	t->dist = plane->dist;
	
	for (i=0 ; i<2 ; i++)
	{
		if (node->children[i] < 0) {
			leafNum = -node->children[i] - 1;
			if ( dleafs[leafNum].cluster == -1  ) {
				t->children[i] = leafNum | ( 1 << 31 ) | ( 1 << 30 );
			} else {
				t->children[i] = leafNum | ( 1 << 31 );
			}
		} else {
			t->children[i] = tnode_p - tnodes;
			MakeTnode (node->children[i]);
		}
	}
			
}

void InitTrace( void ) {
	tnodes = malloc( (MAX_TNODES+1) * sizeof(tnode_t));
	tnodes = (tnode_t *)(((int)tnodes + 31)&~31);
	tnode_p = tnodes;

	MakeTnode (0);

	InitSurfacesForTesting();
}


qboolean PointInSolid_r( vec3_t start, int node ) {
	tnode_t	*tnode;
	float	front;

	while ( !(node & (1<<31) ) ) {
		tnode = &tnodes[node];
		switch (tnode->type) {
		case PLANE_X:
			front = start[0] - tnode->dist;
			break;
		case PLANE_Y:
			front = start[1] - tnode->dist;
			break;
		case PLANE_Z:
			front = start[2] - tnode->dist;
			break;
		default:
			front = (start[0]*tnode->normal[0] + start[1]*tnode->normal[1] + start[2]*tnode->normal[2]) - tnode->dist;
			break;
		}

		if ( front == 0 ) {
			return (qboolean) ( PointInSolid_r( start, tnode->children[0] ) 
				| PointInSolid_r( start, tnode->children[1] ) );
		}

		if ( front > 0 ) {
			node = tnode->children[0];
		} else {
			node = tnode->children[1];
		}
	}

	if ( node & ( 1 << 30 ) ) {
		return qtrue;
	}
	return qfalse;
}

qboolean PointInSolid( vec3_t start ) {
	return PointInSolid_r( start, 0 );
}


int TraceLine_r( int node, const vec3_t start, const vec3_t stop, traceWork_t *tw ) {
	tnode_t	*tnode;
	float	front, back;
	vec3_t	mid;
	float	frac;
	int		side;
	int		r;

	if (node & (1<<31)) {
		if (node & ( 1 << 30 ) ) {
			VectorCopy (start, tw->trace->hit);
			tw->trace->passSolid = qtrue;
			return qtrue;
		} else {
			if ( tw->numOpenLeafs == MAX_MAP_LEAFS ) {
				return qfalse;
			}
			tw->openLeafNumbers[ tw->numOpenLeafs ] = node & ~(3 << 30);
			tw->numOpenLeafs++;
			return qfalse;
		}
	}

	tnode = &tnodes[node];
	switch (tnode->type) {
	case PLANE_X:
		front = start[0] - tnode->dist;
		back = stop[0] - tnode->dist;
		break;
	case PLANE_Y:
		front = start[1] - tnode->dist;
		back = stop[1] - tnode->dist;
		break;
	case PLANE_Z:
		front = start[2] - tnode->dist;
		back = stop[2] - tnode->dist;
		break;
	default:
		front = (start[0]*tnode->normal[0] + start[1]*tnode->normal[1] + start[2]*tnode->normal[2]) - tnode->dist;
		back = (stop[0]*tnode->normal[0] + stop[1]*tnode->normal[1] + stop[2]*tnode->normal[2]) - tnode->dist;
		break;
	}

	if (front >= -TRACE_ON_EPSILON && back >= -TRACE_ON_EPSILON) {
		return TraceLine_r (tnode->children[0], start, stop, tw);
	}
	
	if (front < TRACE_ON_EPSILON && back < TRACE_ON_EPSILON) {
		return TraceLine_r (tnode->children[1], start, stop, tw);
	}

	side = front < 0;
	
	frac = front / (front-back);

	mid[0] = start[0] + (stop[0] - start[0])*frac;
	mid[1] = start[1] + (stop[1] - start[1])*frac;
	mid[2] = start[2] + (stop[2] - start[2])*frac;

	r = TraceLine_r (tnode->children[side], start, mid, tw);

	if (r) {
		return r;
	}

	return TraceLine_r (tnode->children[!side], mid, stop, tw);
}



qboolean	SphereCull( vec3_t start, vec3_t stop, vec3_t origin, float radius ) {
	vec3_t		v;
	float		d;
	vec3_t		dir;
	float		len;
	vec3_t		on;

	VectorSubtract( stop, start, dir );
	len = VectorNormalize( dir, dir );

	VectorSubtract( origin, start, v );
	d = DotProduct( v, dir );
	if ( d > len + radius ) {
		return qtrue;		
	}
	if ( d < -radius ) {
		return qtrue;		
	}
	VectorMA( start, d, dir, on );
	
	VectorSubtract( on, origin, v );

	len = VectorLength( v );

	if ( len > radius ) {
		return qtrue;		
	}

	return qfalse;		
}

void	TraceAgainstSurface( traceWork_t *tw, surfaceTest_t *surf ) {
	int		i;

	if ( SphereCull( tw->start, tw->end, surf->origin, surf->radius ) ) {
		if ( numthreads == 1 ) {
			c_cullTrace++;
		}
		return;
	}

	if ( numthreads == 1 ) {
		c_testTrace++;
		c_testFacets += surf->numFacets;
	}


	for ( i = 0 ; i < surf->numFacets ; i++ ) {
		TraceAgainstFacet( tw, surf->shader, surf->facets + i );
	}
}

extern qboolean	patchshadows;

void TraceLine( const vec3_t start, const vec3_t stop, trace_t *trace, qboolean testAll, traceWork_t *tw ) {
	int				r;
	int				i, j;
	dleaf_t			*leaf;
	float			oldHitFrac;
	surfaceTest_t	*test;
	int				surfaceNum;
	byte			surfaceTested[MAX_MAP_DRAW_SURFS/8];
	;

	if ( numthreads == 1 ) {
		c_totalTrace++;
	}

	trace->filter[0] = 1.0;
	trace->filter[1] = 1.0;
	trace->filter[2] = 1.0;

	VectorCopy( start, tw->start );
	VectorCopy( stop, tw->end );
	tw->trace = trace;

	tw->numOpenLeafs = 0;

	trace->passSolid = qfalse;
	trace->hitFraction = 1.0;

	r = TraceLine_r( 0, start, stop, tw );

	if ( r && !testAll ) {
		return;
	}

	if ( noSurfaces ) {
		return;
	}

	memset( surfaceTested, 0, (numDrawSurfaces+7)/8 );
	oldHitFrac = trace->hitFraction;

	for ( i = 0 ; i < tw->numOpenLeafs ; i++ ) {
		leaf = &dleafs[ tw->openLeafNumbers[ i ] ];
		for ( j = 0 ; j < leaf->numLeafSurfaces ; j++ ) {
			surfaceNum = dleafsurfaces[ leaf->firstLeafSurface + j ];

			if ( surfaceTested[ surfaceNum>>3 ] & ( 1 << ( surfaceNum & 7) ) ) {
				continue;
			}
			surfaceTested[ surfaceNum>>3 ] |= ( 1 << ( surfaceNum & 7 ) );

			test = surfaceTest[ surfaceNum ];
			if ( !test ) {
				continue;
			}
			if ( !tw->patchshadows && test->patch ) {
				continue;
			}
			TraceAgainstSurface( tw, test );
		}

		if ( trace->hitFraction < oldHitFrac ) {
			trace->passSolid = qtrue;
			break;
		}
	}
	
	for ( i = 0 ; i < 3 ; i++ ) {
		trace->hit[i] = start[i] + ( stop[i] - start[i] ) * trace->hitFraction;
	}
}

