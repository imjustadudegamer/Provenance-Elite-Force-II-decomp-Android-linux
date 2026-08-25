#include "tr_local.h"
#include "tr_vbo.h"
#include "tr_program.h"





void RB_CheckOverflow( int verts, int indexes ) {
	RB_EndSurface();

	if ( verts >= SHADER_MAX_VERTEXES ) {
		ri.Error(ERR_DROP, "RB_CheckOverflow: verts > MAX (%d > %d)", verts, SHADER_MAX_VERTEXES );
	}
	if ( indexes >= SHADER_MAX_INDEXES ) {
		ri.Error(ERR_DROP, "RB_CheckOverflow: indices > MAX (%d > %d)", indexes, SHADER_MAX_INDEXES );
	}

	RB_BeginSurface(tess.shader, tess.fogNum);
}


void RB_AddQuadStampExt( vec3_t origin, vec3_t left, vec3_t up, byte *color, float s1, float t1, float s2, float t2 ) {
	vec3_t		normal;
	int			ndx;

	RB_CHECKOVERFLOW( 4, 6 );

	ndx = tess.numVertexes;

	tess.indexes[ tess.numIndexes ] = ndx;
	tess.indexes[ tess.numIndexes + 1 ] = ndx + 1;
	tess.indexes[ tess.numIndexes + 2 ] = ndx + 3;

	tess.indexes[ tess.numIndexes + 3 ] = ndx + 3;
	tess.indexes[ tess.numIndexes + 4 ] = ndx + 1;
	tess.indexes[ tess.numIndexes + 5 ] = ndx + 2;

	tess.xyz[ndx][0] = origin[0] + left[0] + up[0];
	tess.xyz[ndx][1] = origin[1] + left[1] + up[1];
	tess.xyz[ndx][2] = origin[2] + left[2] + up[2];

	tess.xyz[ndx+1][0] = origin[0] - left[0] + up[0];
	tess.xyz[ndx+1][1] = origin[1] - left[1] + up[1];
	tess.xyz[ndx+1][2] = origin[2] - left[2] + up[2];

	tess.xyz[ndx+2][0] = origin[0] - left[0] - up[0];
	tess.xyz[ndx+2][1] = origin[1] - left[1] - up[1];
	tess.xyz[ndx+2][2] = origin[2] - left[2] - up[2];

	tess.xyz[ndx+3][0] = origin[0] + left[0] - up[0];
	tess.xyz[ndx+3][1] = origin[1] + left[1] - up[1];
	tess.xyz[ndx+3][2] = origin[2] + left[2] - up[2];


	VectorSubtract( vec3_origin, backEnd.viewParms.ori.axis[0], normal );

	tess.normal[ndx][0] = tess.normal[ndx+1][0] = tess.normal[ndx+2][0] = tess.normal[ndx+3][0] = normal[0];
	tess.normal[ndx][1] = tess.normal[ndx+1][1] = tess.normal[ndx+2][1] = tess.normal[ndx+3][1] = normal[1];
	tess.normal[ndx][2] = tess.normal[ndx+1][2] = tess.normal[ndx+2][2] = tess.normal[ndx+3][2] = normal[2];
	
	tess.texCoords[ndx][0][0] = tess.texCoords[ndx][1][0] = s1;
	tess.texCoords[ndx][0][1] = tess.texCoords[ndx][1][1] = t1;

	tess.texCoords[ndx+1][0][0] = tess.texCoords[ndx+1][1][0] = s2;
	tess.texCoords[ndx+1][0][1] = tess.texCoords[ndx+1][1][1] = t1;

	tess.texCoords[ndx+2][0][0] = tess.texCoords[ndx+2][1][0] = s2;
	tess.texCoords[ndx+2][0][1] = tess.texCoords[ndx+2][1][1] = t2;

	tess.texCoords[ndx+3][0][0] = tess.texCoords[ndx+3][1][0] = s1;
	tess.texCoords[ndx+3][0][1] = tess.texCoords[ndx+3][1][1] = t2;

	* ( unsigned int * ) &tess.vertexColors[ndx] = 
	* ( unsigned int * ) &tess.vertexColors[ndx+1] = 
	* ( unsigned int * ) &tess.vertexColors[ndx+2] = 
	* ( unsigned int * ) &tess.vertexColors[ndx+3] = 
		* ( unsigned int * )color;


	tess.numVertexes += 4;
	tess.numIndexes += 6;
}

void RB_AddQuadStamp( vec3_t origin, vec3_t left, vec3_t up, byte *color ) {
	RB_AddQuadStampExt( origin, left, up, color, 0, 0, 1, 1 );
}

static void RB_SurfaceSprite( void ) {
	vec3_t		left, up;
	float		radius;

	radius = backEnd.currentEntity->e.radius;
	if ( backEnd.currentEntity->e.rotation == 0 ) {
		VectorScale( backEnd.viewParms.ori.axis[1], radius, left );
		VectorScale( backEnd.viewParms.ori.axis[2], radius, up );
	} else {
		float	s, c;
		float	ang;
		
		ang = M_PI * backEnd.currentEntity->e.rotation / 180;
		s = sin( ang );
		c = cos( ang );

		VectorScale( backEnd.viewParms.ori.axis[1], c * radius, left );
		VectorMA( left, -s * radius, backEnd.viewParms.ori.axis[2], left );

		VectorScale( backEnd.viewParms.ori.axis[2], c * radius, up );
		VectorMA( up, s * radius, backEnd.viewParms.ori.axis[1], up );
	}
	if ( backEnd.viewParms.isMirror ) {
		VectorSubtract( vec3_origin, left, left );
	}

	RB_AddQuadStamp( backEnd.currentEntity->e.origin, left, up, backEnd.currentEntity->e.shaderRGBA );
}


void RB_SurfacePolychain( srfPoly_t *p ) {
	int		i;
	int		numv;

	RB_CHECKOVERFLOW( p->numVerts, 3*(p->numVerts - 2) );

	numv = tess.numVertexes;
	for ( i = 0; i < p->numVerts; i++ ) {
		VectorCopy( p->verts[i].xyz, tess.xyz[numv] );
		tess.texCoords[numv][0][0] = p->verts[i].st[0];
		tess.texCoords[numv][0][1] = p->verts[i].st[1];
		*(int *)&tess.vertexColors[numv] = *(int *)p->verts[ i ].modulate;

		numv++;
	}

	for ( i = 0; i < p->numVerts-2; i++ ) {
		tess.indexes[tess.numIndexes + 0] = tess.numVertexes;
		tess.indexes[tess.numIndexes + 1] = tess.numVertexes + i + 1;
		tess.indexes[tess.numIndexes + 2] = tess.numVertexes + i + 2;
		tess.numIndexes += 3;
	}

	tess.vertexColorValid = qtrue;
	tess.numVertexes = numv;
}


static float R_CalcStaticModelLodDist( const staticLodModel_t *rec ) {
	vec3_t	delta;
	float	dist, scaled, perCent;

	if ( r_lodbias->value > 0.5f ) {
		return 1.0f;
	}

	VectorSubtract( tr.viewParms.ori.origin, rec->origin, delta );
	dist = VectorLength( delta ) * r_lodScaleDistance->value;

	if ( dist <= 0.0f ) {
		return rec->startPerCentVerts;
	}

	scaled = (float)( dist * tan( (double)M_PI * (double)tr.viewParms.fovX / (double)360.0 ) );

	if ( scaled < rec->stopDistance ) {
		return rec->startPerCentVerts;
	}
	if ( scaled >= rec->alphaFadeDistance ) {
		return r_lodscale->value * rec->stopPerCentVerts;
	}

	perCent = rec->startPerCentVerts
			+ ( scaled - rec->stopDistance )
			* ( rec->stopPerCentVerts - rec->startPerCentVerts )
			/ ( rec->alphaFadeDistance - rec->stopDistance );

	return r_lodscale->value * perCent;
}

void RB_SurfaceTriangles( srfTriangles_t *srf ) {
	int			i, j;
	drawVert_t	*dv;
	float		*xyz, *normal, *texCoords;
	byte		*color;
	int			dlightBits;
	int			vertBase;
	int			numVerts;
	int			remap[1200];		 
	qboolean	needsNormal;

	if ( srf->numVerts > 1200 ) {
		return;
	}

	dlightBits = srf->dlightBits[ RB_FrameSmpFrame() ];
	tess.dlightBits |= dlightBits;

	if ( tess.shader->surfaceFlags & ( SURF_NODLIGHT | SURF_SKY ) ) {
		tess.dlightBits = 0;
	}

	numVerts = srf->numVerts;

	if ( srf->verts[2].lodExtra != -1000.0f && srf->verts[2].lodExtra != -1001.0f ) {
		const staticLodModel_t	*rec;
		staticLodModel_t		*mrec;
		int						lod;

		mrec = &tr.world->staticLodModels[ (int)srf->verts[0].lodExtra ];
		rec = mrec;

		if ( RB_FrameCount() != rec->frameCount ) {
			// truncation -- so the (float) cast here is retail's, not decoration.
			lod = (int)(float)( (double)rec->totalNumberOfVerts * R_CalcStaticModelLodDist( rec ) );
			mrec->lodNumberOfVerts = lod;
			mrec->frameCount = RB_FrameCount();
		} else {
			lod = rec->lodNumberOfVerts;
		}

		if ( lod != rec->totalNumberOfVerts ) {
			int count = 0;

			for ( i = 0; i < srf->numVerts; i++ ) {
				if ( ( srf->verts[i].collapseMap >> 16 ) > lod ) {
					break;
				}
				count++;
			}

			if ( count == 0 ) {
				return;
			}

			numVerts = count;
		}
	}

	// (r22), not srf->numVerts (0x4B4A0 add r5, r0, r22).  The first pass is the ordinary
	if ( tess.numVertexes + numVerts >= SHADER_MAX_VERTEXES
		|| tess.numIndexes + srf->numIndexes >= SHADER_MAX_INDEXES ) {
		int maxVerts = SHADER_MAX_VERTEXES;

		if ( tess.shader == tr.shadowShader ) {
			maxVerts = 1500;
		}

		if ( tess.numVertexes + numVerts >= maxVerts
			|| tess.numIndexes + srf->numIndexes >= SHADER_MAX_INDEXES ) {
			RB_EndSurface();

			if ( numVerts >= SHADER_MAX_VERTEXES ) {
				ri.Error( ERR_DROP, "RB_CheckOverflow: verts > MAX (%d > %d)", numVerts, SHADER_MAX_VERTEXES );
			}
			if ( srf->numIndexes >= SHADER_MAX_INDEXES ) {
				ri.Error( ERR_DROP, "RB_CheckOverflow: indices > MAX (%d > %d)", srf->numIndexes, SHADER_MAX_INDEXES );
			}

			RB_BeginSurface( tess.shader, tess.fogNum );
		}
	}

	vertBase = tess.numVertexes;

	if ( numVerts == srf->numVerts ) {
		for ( i = 0 ; i < srf->numIndexes ; i++ ) {
			tess.indexes[ tess.numIndexes + i ] = tess.numVertexes + srf->indexes[ i ];
		}
		tess.numIndexes += srf->numIndexes;

		backEnd.pc.c_indexes += srf->numIndexes;
		backEnd.pc.c_indexBytes += srf->numIndexes * tess.numPasses;
	} else {
		for ( i = 0; i < numVerts; i++ ) {
			remap[i] = i;
		}
		for ( ; i < srf->numVerts; i++ ) {
			remap[i] = remap[ srf->verts[i].collapseMap & 0xFFFF ];
		}

		for ( i = 0; i < srf->numIndexes; i += 3 ) {
			int a = remap[ srf->indexes[i + 0] ];
			int b = remap[ srf->indexes[i + 1] ];
			int c = remap[ srf->indexes[i + 2] ];

			if ( a == b || b == c || c == a ) {
				continue;
			}

			tess.indexes[ tess.numIndexes + 0 ] = tess.numVertexes + a;
			tess.indexes[ tess.numIndexes + 1 ] = tess.numVertexes + b;
			tess.indexes[ tess.numIndexes + 2 ] = tess.numVertexes + c;
			tess.numIndexes += 3;

			backEnd.pc.c_indexes += 3;
			backEnd.pc.c_indexBytes += 3 * tess.numPasses;
		}
	}

	dv        = srf->verts;
	xyz       = tess.xyz[ vertBase ];
	normal    = tess.normal[ vertBase ];
	texCoords = tess.texCoords[ vertBase ][0];
	color     = tess.vertexColors[ vertBase ];

	if ( r_shownormals->integer || tess.shader->needsNormal || tess.shader->needsLSpherical
		|| tr.refdef.num_dlights ) {
		needsNormal = qtrue;
	} else {
		needsNormal = qfalse;
	}

	tess.vertexColorValid = qtrue;

	for ( i = 0 ; i < numVerts ; i++, dv++, xyz += 4, normal += 4, texCoords += 4, color += 4 ) {
		xyz[0] = dv->xyz[0];                       
		xyz[1] = dv->xyz[1];
		xyz[2] = dv->xyz[2];

		if ( needsNormal ) {                        
			normal[0] = dv->normal[0];             
			normal[1] = dv->normal[1];
			normal[2] = dv->normal[2];
		}

		texCoords[0] = dv->st[0];                  
		texCoords[1] = dv->st[1];                  

		texCoords[2] = dv->lightmap[0];            
		texCoords[3] = dv->lightmap[1];            

		if ( srf->color ) {
			color[0] = srf->color[ 3 * i + 0 ];
			color[1] = srf->color[ 3 * i + 1 ];
			color[2] = srf->color[ 3 * i + 2 ];
			color[3] = dv->color[3];
		} else {
			*(int *)color = *(int *)dv->color;      
		}
	}

	for ( j = 0 ; j < srf->numVerts ; j++ ) {
		tess.vertexDlightBits[ vertBase + j ] = dlightBits;
	}

	// W31-HOLO: this is retail's ENTITY-FADE slot, not the waveform clock.
	tess.entityFade = srf->shaderTime;

	tess.numVertexes += numVerts;
	backEnd.pc.c_vertexes += numVerts;
}

void RB_SurfaceTerrain( srfTriangles_t *srf ) {
	int			i, j;
	drawVert_t	*dv;
	float		*xyz, *normal, *texCoords;
	byte		*color;
	int			dlightBits;
	int			vertBase;
	qboolean	needsNormal;

	if ( srf->numVerts > 1200 ) {
		return;
	}

	dv = srf->verts;

	dlightBits = srf->dlightBits[ RB_FrameSmpFrame() ];
	tess.dlightBits |= dlightBits;

	if ( tess.shader->surfaceFlags & ( SURF_NODLIGHT | SURF_SKY ) ) {
		tess.dlightBits = 0;
	}

	RB_CHECKOVERFLOW( srf->numVerts, srf->numIndexes );

	for ( i = 0 ; i < srf->numIndexes ; i++ ) {
		tess.indexes[ tess.numIndexes + i ] = tess.numVertexes + srf->indexes[ i ];
	}
	tess.numIndexes += srf->numIndexes;

	backEnd.pc.c_indexes += srf->numIndexes;
	backEnd.pc.c_indexBytes += srf->numIndexes * tess.numPasses;

	vertBase  = tess.numVertexes;
	xyz       = tess.xyz[ vertBase ];
	normal    = tess.normal[ vertBase ];
	texCoords = tess.texCoords[ vertBase ][0];
	color     = tess.vertexColors[ vertBase ];
	dv        = srf->verts;

	if ( r_shownormals->integer || tess.shader->needsNormal || tess.shader->needsLSpherical
		|| tr.refdef.num_dlights ) {
		needsNormal = qtrue;
	} else {
		needsNormal = qfalse;
	}

	tess.vertexColorValid = qtrue;

	for ( i = 0 ; i < srf->numVerts ; i++, dv++, xyz += 4, normal += 4, texCoords += 4, color += 4 ) {
		xyz[0] = dv->xyz[0];
		xyz[1] = dv->xyz[1];
		xyz[2] = dv->xyz[2];

		if ( needsNormal ) {
			normal[0] = dv->normal[0];
			normal[1] = dv->normal[1];
			normal[2] = dv->normal[2];
		}

		texCoords[0] = dv->st[0];
		texCoords[1] = dv->st[1];

		texCoords[2] = dv->lightmap[0];
		texCoords[3] = dv->lightmap[1];

		if ( srf->color ) {
			color[0] = srf->color[ 3 * i + 0 ];
			color[1] = srf->color[ 3 * i + 1 ];
			color[2] = srf->color[ 3 * i + 2 ];
			color[3] = dv->color[3];
		} else {
			*(int *)color = *(int *)dv->color;
		}
	}

	for ( j = 0 ; j < srf->numVerts ; j++ ) {
		tess.vertexDlightBits[ vertBase + j ] = dlightBits;
	}

	// W31-HOLO: this is retail's ENTITY-FADE slot, not the waveform clock.
	tess.entityFade = srf->shaderTime;

	tess.numVertexes += srf->numVerts;			 
	backEnd.pc.c_vertexes += srf->numVerts;		 
}



void RB_SurfaceBeam( void ) 
{
#define NUM_BEAM_SEGS 6
	refEntity_t *e;
	int	i;
	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t	start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	e = &backEnd.currentEntity->e;

	oldorigin[0] = e->attachOrigin[0];
	oldorigin[1] = e->attachOrigin[1];
	oldorigin[2] = e->attachOrigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if ( VectorNormalize( normalized_direction ) == 0 )
		return;

	PerpendicularVector( perpvec, normalized_direction );

	VectorScale( perpvec, 4, perpvec );

	for ( i = 0; i < NUM_BEAM_SEGS ; i++ )
	{
		RotatePointAroundVector( start_points[i], normalized_direction, perpvec, (360.0/NUM_BEAM_SEGS)*i );
		VectorAdd( start_points[i], direction, end_points[i] );
	}

	GL_BindUnit0( tr.whiteImage );

	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE );

	GLES3_BeginWire();

	RB_ImmColor3f( 1, 0, 0 );
	RB_ImmBegin( GL_TRIANGLE_STRIP );
	for ( i = 0; i <= NUM_BEAM_SEGS; i++ ) {
		RB_ImmVertex3fv( start_points[ i % NUM_BEAM_SEGS] );
		RB_ImmVertex3fv( end_points[ i % NUM_BEAM_SEGS] );
	}
	RB_ImmEnd();
	RB_ImmFlush();
}

void RB_SurfaceFace( srfSurfaceFace_t *surf ) {
	int			i;
	qboolean	needsNormal;
	unsigned	*indices, *tessIndexes;
	float		*v;
	float		*normal;
	int			ndx;
	int			Bob;
	int			numPoints;
	int			dlightBits;

	RB_CHECKOVERFLOW( surf->numPoints, surf->numIndices );

	dlightBits = surf->dlightBits[RB_FrameSmpFrame()];
	tess.dlightBits |= dlightBits;

	if ((tess.shader->surfaceFlags & SURF_NODLIGHT) || (tess.shader->surfaceFlags & SURF_SKY)) {
		tess.dlightBits = 0;
	}

	indices = ( unsigned * ) ( ( ( char  * ) surf ) + surf->ofsIndices );

	Bob = tess.numVertexes;
	tessIndexes = tess.indexes + tess.numIndexes;
	for ( i = surf->numIndices-1 ; i >= 0  ; i-- ) {
		tessIndexes[i] = indices[i] + Bob;
	}

	tess.numIndexes += surf->numIndices;

	numPoints = surf->numPoints;

	needsNormal = qfalse;
	if (r_shownormals->integer || tess.shader->needsNormal || tess.shader->needsLSpherical
		|| tr.refdef.num_dlights) {
		needsNormal = qtrue;
	}

	v = surf->points[0];

	ndx = tess.numVertexes;

	if (needsNormal) {
		normal = surf->plane.normal;
		for ( i = 0, ndx = tess.numVertexes; i < numPoints; i++, ndx++ ) {
			VectorCopy( normal, tess.normal[ndx] );
		}
	}

	for (i = 0, v = surf->points[0], ndx = tess.numVertexes; i < numPoints; i++, v += VERTEXSIZE, ndx++) {
		VectorCopy(v, tess.xyz[ndx]);
		tess.texCoords[ndx][0][0] = v[3];
		tess.texCoords[ndx][0][1] = v[4];
		tess.texCoords[ndx][1][0] = v[5];
		tess.texCoords[ndx][1][1] = v[6];

		if ( surf->color ) {
			const byte *src = surf->color + 3 * i;
			byte	   *dst = tess.vertexColors[ndx];
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = ( (const byte *)&v[7] )[3];
		} else {
			*(unsigned int*)&tess.vertexColors[ndx] = *(unsigned int*)&v[7];
		}
		tess.vertexDlightBits[ndx] = dlightBits;
	}


	tess.vertexColorValid = qtrue;

	tess.numVertexes += surf->numPoints;
}


static float	LodErrorForVolume( vec3_t local, float radius ) {
	vec3_t		world;
	float		d;

	if ( r_lodCurveError->value < 0 ) {
		return 0;
	}

	world[0] = local[0] * backEnd.ori.axis[0][0] + local[1] * backEnd.ori.axis[1][0] + 
		local[2] * backEnd.ori.axis[2][0] + backEnd.ori.origin[0];
	world[1] = local[0] * backEnd.ori.axis[0][1] + local[1] * backEnd.ori.axis[1][1] + 
		local[2] * backEnd.ori.axis[2][1] + backEnd.ori.origin[1];
	world[2] = local[0] * backEnd.ori.axis[0][2] + local[1] * backEnd.ori.axis[1][2] + 
		local[2] * backEnd.ori.axis[2][2] + backEnd.ori.origin[2];

	VectorSubtract( world, backEnd.viewParms.ori.origin, world );
	d = DotProduct( world, backEnd.viewParms.ori.axis[0] );

	if ( d < 0 ) {
		d = -d;
	}
	d -= radius;
	if ( d < 1 ) {
		d = 1;
	}

	return r_lodCurveError->value / d;
}

void RB_SurfaceGrid( srfGridMesh_t *cv ) {
	int		i, j;
	float	*xyz;
	float	*texCoords;
	float	*normal;
	unsigned char *color;
	drawVert_t	*dv;
	int		rows, irows, vrows;
	int		used;
	int		widthTable[MAX_GRID_SIZE];
	int		heightTable[MAX_GRID_SIZE];
	float	lodError;
	int		lodWidth, lodHeight;
	int		numVertexes;
	int		dlightBits;
	int		*vDlightBits;
	qboolean	needsNormal;

	dlightBits = cv->dlightBits[RB_FrameSmpFrame()];
	tess.dlightBits |= dlightBits;

	if (tess.shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY)) {
		tess.dlightBits = 0;
	}

	lodError = LodErrorForVolume( cv->lodOrigin, cv->lodRadius );

	widthTable[0] = 0;
	lodWidth = 1;
	for ( i = 1 ; i < cv->width-1 ; i++ ) {
		if ( cv->widthLodError[i] <= lodError ) {
			widthTable[lodWidth] = i;
			lodWidth++;
		}
	}
	widthTable[lodWidth] = cv->width-1;
	lodWidth++;

	heightTable[0] = 0;
	lodHeight = 1;
	for ( i = 1 ; i < cv->height-1 ; i++ ) {
		if ( cv->heightLodError[i] <= lodError ) {
			heightTable[lodHeight] = i;
			lodHeight++;
		}
	}
	heightTable[lodHeight] = cv->height-1;
	lodHeight++;



	used = 0;
	rows = 0;
	while ( used < lodHeight - 1 ) {
		do {
			vrows = ( SHADER_MAX_VERTEXES - tess.numVertexes ) / lodWidth;
			irows = ( SHADER_MAX_INDEXES - tess.numIndexes ) / ( lodWidth * 6 );

			if ( vrows < 2 || irows < 1 ) {
				RB_EndSurface();
				RB_BeginSurface(tess.shader, tess.fogNum);
			} else {
				break;
			}
		} while ( 1 );
		
		rows = irows;
		if ( vrows < irows + 1 ) {
			rows = vrows - 1;
		}
		if ( used + rows > lodHeight ) {
			rows = lodHeight - used;
		}

		numVertexes = tess.numVertexes;

		xyz = tess.xyz[numVertexes];
		normal = tess.normal[numVertexes];
		texCoords = tess.texCoords[numVertexes][0];
		color = ( unsigned char * ) &tess.vertexColors[numVertexes];
		vDlightBits = &tess.vertexDlightBits[numVertexes];
		needsNormal = r_shownormals->integer || tess.shader->needsNormal
			|| tess.shader->needsLSpherical || tr.refdef.num_dlights;

		for ( i = 0 ; i < rows ; i++ ) {
			for ( j = 0 ; j < lodWidth ; j++ ) {
				dv = cv->verts + heightTable[ used + i ] * cv->width
					+ widthTable[ j ];

				xyz[0] = dv->xyz[0];
				xyz[1] = dv->xyz[1];
				xyz[2] = dv->xyz[2];
				texCoords[0] = dv->st[0];
				texCoords[1] = dv->st[1];
				texCoords[2] = dv->lightmap[0];
				texCoords[3] = dv->lightmap[1];
				if ( needsNormal ) {
					normal[0] = dv->normal[0];
					normal[1] = dv->normal[1];
					normal[2] = dv->normal[2];
				}
				* ( unsigned int * ) color = * ( unsigned int * ) dv->color;
				*vDlightBits++ = dlightBits;
				xyz += 4;
				normal += 4;
				texCoords += 4;
				color += 4;
			}
		}


		{
			int		numIndexes;
			int		w, h;

			h = rows - 1;
			w = lodWidth - 1;
			numIndexes = tess.numIndexes;
			for (i = 0 ; i < h ; i++) {
				for (j = 0 ; j < w ; j++) {
					int		v1, v2, v3, v4;
			
					v1 = numVertexes + i*lodWidth + j + 1;
					v2 = v1 - 1;
					v3 = v2 + lodWidth;
					v4 = v3 + 1;

					tess.indexes[numIndexes] = v2;
					tess.indexes[numIndexes+1] = v3;
					tess.indexes[numIndexes+2] = v1;
					
					tess.indexes[numIndexes+3] = v1;
					tess.indexes[numIndexes+4] = v3;
					tess.indexes[numIndexes+5] = v4;
					numIndexes += 6;
				}
			}

			tess.numIndexes = numIndexes;
		}

		tess.numVertexes += rows * lodWidth;

		used += rows - 1;
	}

	tess.vertexColorValid = qtrue;
}



void RB_SurfaceAxis( void ) {
	GL_BindUnit0( tr.whiteImage );
	GLES3_BeginWire();	
	qglLineWidth( 3 );
	RB_ImmBegin( GL_LINES );
	RB_ImmColor3f( 1,0,0 );
	RB_ImmVertex3f( 0,0,0 );
	RB_ImmVertex3f( 16,0,0 );
	RB_ImmColor3f( 0,1,0 );
	RB_ImmVertex3f( 0,0,0 );
	RB_ImmVertex3f( 0,16,0 );
	RB_ImmColor3f( 0,0,1 );
	RB_ImmVertex3f( 0,0,0 );
	RB_ImmVertex3f( 0,0,16 );
	RB_ImmEnd();
	RB_ImmFlush();
	qglLineWidth( 1 );
}


void RB_SurfaceEntity( surfaceType_t *surfType ) {
	switch( backEnd.currentEntity->e.reType ) {
	case RT_SPRITE:
		RB_SurfaceSprite();
		break;
	case RT_BEAM:
		RB_SurfaceBeam();
		break;
	default:
		RB_SurfaceAxis();
		break;
	}
	return;
}

void RB_SurfaceBad( surfaceType_t *surfType ) {
	ri.Printf( PRINT_ALL, "Bad surface tesselated.\n" );
}

#if 0

void RB_SurfaceFlare( srfFlare_t *surf ) {
	vec3_t		left, up;
	float		radius;
	byte		color[4];
	vec3_t		dir;
	vec3_t		origin;
	float		d;

	radius = 30;
	VectorScale( backEnd.viewParms.ori.axis[1], radius, left );
	VectorScale( backEnd.viewParms.ori.axis[2], radius, up );
	if ( backEnd.viewParms.isMirror ) {
		VectorSubtract( vec3_origin, left, left );
	}

	color[0] = color[1] = color[2] = color[3] = 255;

	VectorMA( surf->origin, 3, surf->normal, origin );
	VectorSubtract( origin, backEnd.viewParms.ori.origin, dir );
	VectorNormalize( dir );
	VectorMA( origin, r_ignore->value, dir, origin );

	d = -DotProduct( dir, surf->normal );
	if ( d < 0 ) {
		return;
	}
#if 0
	color[0] *= d;
	color[1] *= d;
	color[2] *= d;
#endif

	RB_AddQuadStamp( origin, left, up, color );
}

#else

void RB_SurfaceFlare( srfFlare_t *surf ) {
#if 0
	vec3_t		left, up;
	byte		color[4];

	color[0] = surf->color[0] * 255;
	color[1] = surf->color[1] * 255;
	color[2] = surf->color[2] * 255;
	color[3] = 255;

	VectorClear( left );
	VectorClear( up );

	left[0] = r_ignore->value;

	up[1] = r_ignore->value;
	
	RB_AddQuadStampExt( surf->origin, left, up, color, 0, 0, 1, 1 );
#endif
}

#endif



void RB_SurfaceDisplayList( srfDisplayList_t *surf ) {
	static qboolean	s_reported;
	(void)surf;
	if ( !s_reported ) {
		s_reported = qtrue;
		ri.Printf( PRINT_ALL, "RB_SurfaceDisplayList: GLES3 has no display lists; "
							 "no display list is ever created in this renderer (0 qglNewList call sites)\n" );
	}
}

void RB_SurfaceSkip( void *surf ) {
}

void (*rb_surfaceTable[SF_NUM_SURFACE_TYPES])( void *) = {
	(void(*)(void*))RB_SurfaceBad,			 
	(void(*)(void*))RB_SurfaceSkip,			 
	(void(*)(void*))RB_SurfaceFace,			 
	(void(*)(void*))RB_SurfaceGrid,			 
	(void(*)(void*))RB_SurfaceTriangles,	 
	(void(*)(void*))RB_SurfacePolychain,	 
	(void(*)(void*))RB_SurfaceBad,			 
	(void(*)(void*))RB_SurfaceBad,			 
	(void(*)(void*))RB_SurfaceFlare,		 
	(void(*)(void*))RB_SurfaceEntity,		 
	(void(*)(void*))RB_SurfaceDisplayList,	 
	(void(*)(void*))RB_TikiMesh,			 
	(void(*)(void*))RB_SkelMesh,			 
	(void(*)(void*))RB_DrawSwipeSurface,	 
	(void(*)(void*))RB_DrawSprite,			 
	(void(*)(void*))RB_SurfaceTerrain,		 

};

