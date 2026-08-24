#include "qbsp.h"

typedef struct edgePoint_s {
	float		intercept;
	vec3_t		xyz;
	struct edgePoint_s	*prev, *next;
} edgePoint_t;

typedef struct edgeLine_s {
	vec3_t		normal1;
	float		dist1;
	
	vec3_t		normal2;
	float		dist2;
	
	vec3_t		origin;
	vec3_t		dir;

	edgePoint_t	chain;		
} edgeLine_t;

typedef struct {
	float		length;
	drawVert_t	*dv[2];
} originalEdge_t;

#define	MAX_ORIGINAL_EDGES	0x10000
originalEdge_t	originalEdges[MAX_ORIGINAL_EDGES];
int				numOriginalEdges;


#define	MAX_EDGE_LINES		0x10000
edgeLine_t		edgeLines[MAX_EDGE_LINES];
int				numEdgeLines;

int				c_degenerateEdges;
int				c_addedVerts;
int				c_totalVerts;

int				c_natural, c_rotate, c_cant;

#define	LINE_POSITION_EPSILON	0.25
#define	POINT_ON_LINE_EPSILON	0.25

void InsertPointOnEdge( vec3_t v, edgeLine_t *e ) {
	vec3_t		delta;
	float		d;
	edgePoint_t	*p, *scan;

	VectorSubtract( v, e->origin, delta );
	d = DotProduct( delta, e->dir );

	p = malloc( sizeof(edgePoint_t) );
	p->intercept = d;
	VectorCopy( v, p->xyz );

	if ( e->chain.next == &e->chain ) {
		e->chain.next = e->chain.prev = p;
		p->next = p->prev = &e->chain;
		return;
	}

	scan = e->chain.next;
	for ( ; scan != &e->chain ; scan = scan->next ) {
		d = p->intercept - scan->intercept;
		if ( d > -LINE_POSITION_EPSILON && d < LINE_POSITION_EPSILON ) {
			free( p );
			return;		
		}

		if ( p->intercept < scan->intercept ) {
			p->prev = scan->prev;
			p->next = scan;
			scan->prev->next = p;
			scan->prev = p;
			return;
		}
	}

	p->prev = scan->prev;
	p->next = scan;
	scan->prev->next = p;
	scan->prev = p;
}


int AddEdge( vec3_t v1, vec3_t v2, qboolean createNonAxial ) {
	int			i;
	edgeLine_t	*e;
	float		d;
	vec3_t		dir;

	VectorSubtract( v2, v1, dir );
	d = VectorNormalize( dir, dir );
	if ( d < 0.1 ) {
		c_degenerateEdges++;
		return -1;
	}

	if ( !createNonAxial ) {
		if ( fabs( dir[0] + dir[1] + dir[2] ) != 1.0 ) {
			if ( numOriginalEdges == MAX_ORIGINAL_EDGES ) {
				Error( "MAX_ORIGINAL_EDGES" );
			}
			originalEdges[ numOriginalEdges ].dv[0] = (drawVert_t *)v1;
			originalEdges[ numOriginalEdges ].dv[1] = (drawVert_t *)v2;
			originalEdges[ numOriginalEdges ].length = d;
			numOriginalEdges++;
			return -1;
		}
	}

	for ( i = 0 ; i < numEdgeLines ; i++ ) {
		e = &edgeLines[i];

		d = DotProduct( v1, e->normal1 ) - e->dist1;
		if ( d < -POINT_ON_LINE_EPSILON || d > POINT_ON_LINE_EPSILON ) {
			continue;
		}
		d = DotProduct( v1, e->normal2 ) - e->dist2;
		if ( d < -POINT_ON_LINE_EPSILON || d > POINT_ON_LINE_EPSILON ) {
			continue;
		}

		d = DotProduct( v2, e->normal1 ) - e->dist1;
		if ( d < -POINT_ON_LINE_EPSILON || d > POINT_ON_LINE_EPSILON ) {
			continue;
		}
		d = DotProduct( v2, e->normal2 ) - e->dist2;
		if ( d < -POINT_ON_LINE_EPSILON || d > POINT_ON_LINE_EPSILON ) {
			continue;
		}

		InsertPointOnEdge( v1, e );
		InsertPointOnEdge( v2, e );
		return i;
	}

	if ( numEdgeLines >= MAX_EDGE_LINES ) {
		Error( "MAX_EDGE_LINES" );
	}

	e = &edgeLines[ numEdgeLines ];
	numEdgeLines++;

	e->chain.next = e->chain.prev = &e->chain;

	VectorCopy( v1, e->origin );
	VectorCopy( dir, e->dir );

	MakeNormalVectors( e->dir, e->normal1, e->normal2 );
	e->dist1 = DotProduct( e->origin, e->normal1 );
	e->dist2 = DotProduct( e->origin, e->normal2 );

	InsertPointOnEdge( v1, e );
	InsertPointOnEdge( v2, e );

	return numEdgeLines - 1;
}

void AddSurfaceEdges( mapDrawSurface_t *ds ) {
	int		i;

	for ( i = 0 ; i < ds->numVerts ; i++ ) {
		ds->verts[i].lightmap[0] = 
			AddEdge( ds->verts[i].xyz, ds->verts[(i+1) % ds->numVerts].xyz, qfalse );
	}
}

qboolean ColinearEdge( vec3_t v1, vec3_t v2, vec3_t v3 ) {
	vec3_t	midpoint, dir, offset, on;
	float	d;

	VectorSubtract( v2, v1, midpoint );
	VectorSubtract( v3, v1, dir );
	d = VectorNormalize( dir, dir );
	if ( d == 0 ) {
		return qfalse;	
	}

	d = DotProduct( midpoint, dir );
	VectorScale( dir, d, on );
	VectorSubtract( midpoint, on, offset );
	d = VectorLength ( offset );

	if ( d < 0.1 ) {
		return qtrue;
	}

	return qfalse;
}

void AddPatchEdges( mapDrawSurface_t *ds ) {
	int		i;
	float	*v1, *v2, *v3;

	for ( i = 0 ; i < ds->patchWidth - 2; i+=2 ) {
		v1 = ds->verts[ i ].xyz;
		v2 = ds->verts[ i + 1 ].xyz;
		v3 = ds->verts[ i + 2 ].xyz;

		if ( ColinearEdge( v1, v2, v3 ) ) {
			AddEdge( v1, v3, qfalse );
		}

		v1 = ds->verts[ ( ds->patchHeight - 1 ) * ds->patchWidth + i ].xyz;
		v2 = ds->verts[ ( ds->patchHeight - 1 ) * ds->patchWidth + i + 1 ].xyz;
		v3 = ds->verts[ ( ds->patchHeight - 1 ) * ds->patchWidth + i + 2 ].xyz;

		if ( ColinearEdge( v1, v2, v3 ) ) {
			AddEdge( v1, v3, qfalse );
		}
	}

	for ( i = 0 ; i < ds->patchHeight - 2 ; i+=2 ) {
		v1 = ds->verts[ i * ds->patchWidth ].xyz;
		v2 = ds->verts[ ( i + 1 ) * ds->patchWidth ].xyz;
		v3 = ds->verts[ ( i + 2 ) * ds->patchWidth ].xyz;

		if ( ColinearEdge( v1, v2, v3 ) ) {
			AddEdge( v1, v3, qfalse );
		}

		v1 = ds->verts[ ( ds->patchWidth - 1 ) + i * ds->patchWidth ].xyz;
		v2 = ds->verts[ ( ds->patchWidth - 1 ) + ( i + 1 ) * ds->patchWidth ].xyz;
		v3 = ds->verts[ ( ds->patchWidth - 1 ) + ( i + 2 ) * ds->patchWidth ].xyz;

		if ( ColinearEdge( v1, v2, v3 ) ) {
			AddEdge( v1, v3, qfalse );
		}
	}


}


#define	MAX_SURFACE_VERTS	256
void FixSurfaceJunctions( mapDrawSurface_t *ds ) {
	int			i, j, k;
	edgeLine_t	*e;
	edgePoint_t	*p;
	int			originalVerts;
	int			counts[MAX_SURFACE_VERTS];
	int			originals[MAX_SURFACE_VERTS];
	int			firstVert[MAX_SURFACE_VERTS];
	drawVert_t	verts[MAX_SURFACE_VERTS], *v1, *v2;
	int			numVerts;
	float		start, end, frac;
	vec3_t		delta;

	originalVerts = ds->numVerts;

	numVerts = 0;
	for ( i = 0 ; i < ds->numVerts ; i++ ) {
		counts[i] = 0;
		firstVert[i] = numVerts;

		if ( numVerts == MAX_SURFACE_VERTS ) {
			Error( "MAX_SURFACE_VERTS" );
		}
		verts[numVerts] = ds->verts[i];
		originals[numVerts] = i;
		numVerts++;

		v1 = &ds->verts[i];
		v2 = &ds->verts[ (i+1) % ds->numVerts ];

		j = (int)ds->verts[i].lightmap[0];
		if ( j == -1 ) {
			continue;		
		}
		e = &edgeLines[ j ];
		
		VectorSubtract( v1->xyz, e->origin, delta );
		start = DotProduct( delta, e->dir );

		VectorSubtract( v2->xyz, e->origin, delta );
		end = DotProduct( delta, e->dir );


		if ( start < end ) {
			p = e->chain.next;
		} else {
			p = e->chain.prev;
		}

		for (  ; p != &e->chain ;  ) {
			if ( start < end ) {
				if ( p->intercept > end - ON_EPSILON ) {
					break;
				}
			} else {
				if ( p->intercept < end + ON_EPSILON ) {
					break;
				}
			}

			if ( 
				( start < end && p->intercept > start + ON_EPSILON ) ||
				( start > end && p->intercept < start - ON_EPSILON ) ) {
				if ( numVerts == MAX_SURFACE_VERTS ) {
					Error( "MAX_SURFACE_VERTS" );
				}

				VectorCopy( p->xyz, verts[ numVerts ].xyz );

				VectorCopy( v1->normal, verts[ numVerts ].normal );

				frac = ( p->intercept - start ) / ( end - start );
				for ( j = 0 ; j < 2 ; j++ ) {
					verts[ numVerts ].st[j] = v1->st[j] + 
						frac * ( v2->st[j] - v1->st[j] );
				}
				originals[numVerts] = i;
				numVerts++;
				counts[i]++;
			}

			if ( start < end ) {
				p = p->next;
			} else {
				p = p->prev;
			}
		}
	}

	c_addedVerts += numVerts - ds->numVerts;
	c_totalVerts += numVerts;



	for ( i = 0 ; i < numVerts ; i++ ) {
		if ( originals[ (i+1) % numVerts ] == originals[ i ] ) {
			continue;
		}
		j = (i + numVerts - 1 ) % numVerts;
		k = (i + numVerts - 2 ) % numVerts;
		if ( originals[ j ] == originals[ k ] ) {
			continue;
		}
		break;
	}

	if ( i == 0 ) {
		c_natural++;

		ds->numVerts = numVerts;
		ds->verts = malloc( numVerts * sizeof( *ds->verts ) );
		memcpy( ds->verts, verts, numVerts * sizeof( *ds->verts ) );

		return;
	}
	if ( i == numVerts ) {
		c_cant++;

	} else {
		c_rotate++;

	}

	ds->numVerts = numVerts;
	ds->verts = malloc( numVerts * sizeof( *ds->verts ) );

	for ( j = 0 ; j < ds->numVerts ; j++ ) {
		ds->verts[j] = verts[ ( j + i ) % ds->numVerts ];
	}
}

int EdgeCompare( const void *elem1, const void *elem2 ) {
	float	d1, d2;

	d1 = ((originalEdge_t *)elem1)->length;
	d2 = ((originalEdge_t *)elem2)->length;

	if ( d1 < d2 ) {
		return -1;
	}
	if ( d2 > d1 ) {
		return 1;
	}
	return 0;
}


void FixTJunctions( entity_t *ent ) {
	int					i;
	mapDrawSurface_t	*ds;
	int					axialEdgeLines;
	originalEdge_t		*e;

	qprintf("----- FixTJunctions -----\n");

	numEdgeLines = 0;
	numOriginalEdges = 0;

	for ( i = ent->firstDrawSurf ; i < numMapDrawSurfs ; i++ ) {
		ds = &mapDrawSurfs[i];
		if ( ds->patch ) {
			AddPatchEdges( ds );
		} else if ( ds->shaderInfo->autosprite || ds->shaderInfo->notjunc || ds->miscModel ) {
		} else {
			AddSurfaceEdges( ds );
		}
	}

	axialEdgeLines = numEdgeLines;

	qsort( originalEdges, numOriginalEdges, sizeof(originalEdges[0]), EdgeCompare );

	for ( i = 0 ; i < numOriginalEdges ; i++ ) {
		e = &originalEdges[i];
		e->dv[0]->lightmap[0] = AddEdge( e->dv[0]->xyz, e->dv[1]->xyz, qtrue );
	}

	qprintf( "%6i axial edge lines\n", axialEdgeLines );
	qprintf( "%6i non-axial edge lines\n", numEdgeLines - axialEdgeLines );
	qprintf( "%6i degenerate edges\n", c_degenerateEdges );

	for ( i = ent->firstDrawSurf ; i < numMapDrawSurfs ; i++ ) {
		ds = &mapDrawSurfs[i];
		if ( ds->patch ) {
			continue;
		}
		if ( ds->shaderInfo->autosprite || ds->shaderInfo->notjunc || ds->miscModel ) {
			continue;
		}

		FixSurfaceJunctions( ds );
	}

	qprintf( "%6i verts added for tjunctions\n", c_addedVerts );
	qprintf( "%6i total verts\n", c_totalVerts );
	qprintf( "%6i naturally ordered\n", c_natural );
	qprintf( "%6i rotated orders\n", c_rotate );
	qprintf( "%6i can't order\n", c_cant );
}
