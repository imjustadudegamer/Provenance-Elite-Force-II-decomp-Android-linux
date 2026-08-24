#include "qbsp.h"



int					numSortShaders;
mapDrawSurface_t	*surfsOnShader[MAX_MAP_SHADERS];


int		allocated[LIGHTMAP_WIDTH];

int		numLightmaps = 1;
int		c_exactLightmap;


void PrepareNewLightmap( void ) {
	memset( allocated, 0, sizeof( allocated ) );
	numLightmaps++;
}

qboolean AllocLMBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;

	best = LIGHTMAP_HEIGHT;

	for ( i=0 ; i <= LIGHTMAP_WIDTH-w ; i++ ) {
		best2 = 0;

		for (j=0 ; j<w ; j++) {
			if (allocated[i+j] >= best) {
				break;
			}
			if (allocated[i+j] > best2) {
				best2 = allocated[i+j];
			}
		}
		if (j == w)	{	
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > LIGHTMAP_HEIGHT) {
		return qfalse;
	}

	for (i=0 ; i<w ; i++) {
		allocated[*x + i] = best + h;
	}

	return qtrue;
}



void AllocateLightmapForPatch( mapDrawSurface_t *ds ) {
	int			i, j, k;
	drawVert_t	*verts;
	int			w, h;
	int			x, y;
	float		s, t;
	mesh_t		mesh, *subdividedMesh, *tempMesh, *newmesh;
	int			widthtable[LIGHTMAP_WIDTH], heighttable[LIGHTMAP_HEIGHT], ssize;

	verts = ds->verts;

	mesh.width = ds->patchWidth;
	mesh.height = ds->patchHeight;
	mesh.verts = verts;
	newmesh = SubdivideMesh( mesh, 8, 999 );

	PutMeshOnCurve( *newmesh );
	tempMesh = RemoveLinearMeshColumnsRows( newmesh );
	FreeMesh(newmesh);

	ssize = samplesize;
	if (ds->shaderInfo->lightmapSampleSize)
		ssize = ds->shaderInfo->lightmapSampleSize;

#ifdef LIGHTMAP_PATCHSHIFT
	subdividedMesh = SubdivideMeshQuads( tempMesh, ssize, LIGHTMAP_WIDTH-1, widthtable, heighttable);
#else
	subdividedMesh = SubdivideMeshQuads( tempMesh, ssize, LIGHTMAP_WIDTH, widthtable, heighttable);
#endif

	w = subdividedMesh->width;
	h = subdividedMesh->height;

#ifdef LIGHTMAP_PATCHSHIFT
	w++;
	h++;
#endif

	FreeMesh(subdividedMesh);

	c_exactLightmap += w * h;

	if ( !AllocLMBlock( w, h, &x, &y ) ) {
		PrepareNewLightmap();
		if ( !AllocLMBlock( w, h, &x, &y ) ) {
			Error("Entity %i, brush %i: Lightmap allocation failed", 
				ds->mapBrush->entitynum, ds->mapBrush->brushnum );
		}
	}

#ifdef LIGHTMAP_PATCHSHIFT
	w--;
	h--;
#endif

	ds->lightmapNum = numLightmaps - 1;
	ds->lightmapWidth = w;
	ds->lightmapHeight = h;
	ds->lightmapX = x;
	ds->lightmapY = y;

	for ( i = 0 ; i < ds->patchWidth ; i++ ) {
		for ( k = 0 ; k < w ; k++ ) {
			if ( originalWidths[k] >= i ) {
				break;
			}
		}
		if (k >= w)
			k = w-1;
		s = x + k;
		for ( j = 0 ; j < ds->patchHeight ; j++ ) {
			for ( k = 0 ; k < h ; k++ ) {
				if ( originalHeights[k] >= j ) {
					break;
				}
			}
			if (k >= h)
				k = h-1;
			t = y + k;
			verts[i + j * ds->patchWidth].lightmap[0] = ( s + 0.5 ) / LIGHTMAP_WIDTH;
			verts[i + j * ds->patchWidth].lightmap[1] = ( t + 0.5 ) / LIGHTMAP_HEIGHT;
		}
	}
}


void AllocateLightmapForSurface( mapDrawSurface_t *ds ) {
	vec3_t		mins, maxs, size, exactSize, delta;
	int			i;
	drawVert_t	*verts;
	int			w, h;
	int			x, y, ssize;
	int			axis;
	vec3_t		vecs[2];
	float		s, t;
	vec3_t		origin;
	plane_t		*plane;
	float		d;
	vec3_t		planeNormal;

	if ( ds->patch ) {
		AllocateLightmapForPatch( ds );
		return;
	}

	ssize = samplesize;
	if (ds->shaderInfo->lightmapSampleSize)
		ssize = ds->shaderInfo->lightmapSampleSize;

	plane = &mapplanes[ ds->side->planenum ];

	ClearBounds( mins, maxs );
	verts = ds->verts;
	for ( i = 0 ; i < ds->numVerts ; i++ ) {
		AddPointToBounds( verts[i].xyz, mins, maxs );
	}

	for ( i = 0 ; i < 3 ; i++ ) {
		exactSize[i] = maxs[i] - mins[i];
		mins[i] = ssize * floor( mins[i] / ssize );
		maxs[i] = ssize * ceil( maxs[i] / ssize );
		size[i] = (maxs[i] - mins[i]) / ssize + 1;
	}

	memset( vecs, 0, sizeof( vecs ) );

	planeNormal[0] = fabs( plane->normal[0] );
	planeNormal[1] = fabs( plane->normal[1] );
	planeNormal[2] = fabs( plane->normal[2] );

	if ( planeNormal[0] >= planeNormal[1] && planeNormal[0] >= planeNormal[2] ) {
		w = size[1];
		h = size[2];
		axis = 0;
		vecs[0][1] = 1.0 / ssize;
		vecs[1][2] = 1.0 / ssize;
	} else if ( planeNormal[1] >= planeNormal[0] && planeNormal[1] >= planeNormal[2] ) {
		w = size[0];
		h = size[2];
		axis = 1;
		vecs[0][0] = 1.0 / ssize;
		vecs[1][2] = 1.0 / ssize;
	} else {
		w = size[0];
		h = size[1];
		axis = 2;
		vecs[0][0] = 1.0 / ssize;
		vecs[1][1] = 1.0 / ssize;
	}

	if ( !plane->normal[axis] ) {
		Error( "Chose a 0 valued axis" );
	}

	if ( w > LIGHTMAP_WIDTH ) {
		VectorScale ( vecs[0], (float)LIGHTMAP_WIDTH/w, vecs[0] );
		w = LIGHTMAP_WIDTH;
	}
	
	if ( h > LIGHTMAP_HEIGHT ) {
		VectorScale ( vecs[1], (float)LIGHTMAP_HEIGHT/h, vecs[1] );
		h = LIGHTMAP_HEIGHT;
	}
	
	c_exactLightmap += w * h;

	if ( !AllocLMBlock( w, h, &x, &y ) ) {
		PrepareNewLightmap();
		if ( !AllocLMBlock( w, h, &x, &y ) ) {
			Error("Entity %i, brush %i: Lightmap allocation failed", 
				ds->mapBrush->entitynum, ds->mapBrush->brushnum );
		}
	}

	ds->lightmapNum = numLightmaps - 1;
	ds->lightmapWidth = w;
	ds->lightmapHeight = h;
	ds->lightmapX = x;
	ds->lightmapY = y;

	for ( i = 0 ; i < ds->numVerts ; i++ ) {
		VectorSubtract( verts[i].xyz, mins, delta );
		s = DotProduct( delta, vecs[0] ) + x + 0.5;
		t = DotProduct( delta, vecs[1] ) + y + 0.5;
		verts[i].lightmap[0] = s / LIGHTMAP_WIDTH;
		verts[i].lightmap[1] = t / LIGHTMAP_HEIGHT;
	}


	d = DotProduct( mins, plane->normal ) - plane->dist;
	d /= plane->normal[ axis ];
	VectorCopy( mins, origin );
	origin[axis] -= d;

	for ( i = 0 ; i < 2 ; i++ ) {
		vec3_t	normalized;
		float	len;

		len = VectorNormalize( vecs[i], normalized );
		VectorScale( normalized, (1.0/len), vecs[i] );
		d = DotProduct( vecs[i], plane->normal );
		d /= plane->normal[ axis ];
		vecs[i][axis] -= d;
	}

	VectorCopy( origin, ds->lightmapOrigin );
	VectorCopy( vecs[0], ds->lightmapVecs[0] );
	VectorCopy( vecs[1], ds->lightmapVecs[1] );
	VectorCopy( plane->normal, ds->lightmapVecs[2] );
}

void AllocateLightmaps( entity_t *e ) {
	int				i, j;
	mapDrawSurface_t	*ds;
	shaderInfo_t	*si;

	qprintf ("--- AllocateLightmaps ---\n");


	numSortShaders = 0;

	for ( i = e->firstDrawSurf ; i < numMapDrawSurfs ; i++ ) {
		ds = &mapDrawSurfs[i];
		if ( !ds->numVerts ) {
			continue;		
		}
		if ( ds->miscModel ) {
			continue;
		}
		if ( !ds->patch ) {
			VectorCopy( mapplanes[ds->side->planenum].normal, ds->lightmapVecs[2] );
		}

		for ( j = 0 ; j < numSortShaders ; j++ ) {
			if ( ds->shaderInfo == surfsOnShader[j]->shaderInfo ) {
				ds->nextOnShader = surfsOnShader[j];
				surfsOnShader[j] = ds;
				break;
			}
		}
		if ( j == numSortShaders ) {
			if ( numSortShaders >= MAX_MAP_SHADERS ) {
				Error( "MAX_MAP_SHADERS" );
			}
			surfsOnShader[j] = ds;
			numSortShaders++;
		}
	}
	qprintf( "%5i unique shaders\n", numSortShaders );



	for ( i = 0 ; i < numSortShaders ; i++ ) {
		si = surfsOnShader[i]->shaderInfo;

		for ( ds = surfsOnShader[i] ; ds ; ds = ds->nextOnShader ) {
			if ( si->surfaceFlags & SURF_NOLIGHTMAP ) {
				ds->lightmapNum = -1;
#ifdef SURF_POINTLIGHT
			} else if ( si->surfaceFlags & SURF_POINTLIGHT ) {
				ds->lightmapNum = -3;
#endif
			} else {
				AllocateLightmapForSurface( ds );
			}
		}
	}

	qprintf( "%7i exact lightmap texels\n", c_exactLightmap );
	qprintf( "%7i block lightmap texels\n", numLightmaps * LIGHTMAP_WIDTH*LIGHTMAP_HEIGHT );
}



