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
// SPDX-License-Identifier: GPL-2.0-or-later
#include "tr_local.h"



static qboolean	R_CullGrid( srfGridMesh_t *cv ) {
	int 	boxCull;
	int 	sphereCull;

	if ( r_nocurves->integer ) {
		return qtrue;
	}

	if ( tr.currentEntityNum == ENTITYNUM_WORLD ) {
		sphereCull = R_CullPointAndRadius( cv->localOrigin, cv->meshRadius );
	} else {
		sphereCull = R_CullLocalPointAndRadius( cv->localOrigin, cv->meshRadius );
	}

	if ( sphereCull == CULL_OUT )
	{
		tr.pc.c_sphere_cull_patch_out++;
		return qtrue;
	}
	else if ( sphereCull == CULL_CLIP )
	{
		tr.pc.c_sphere_cull_patch_clip++;

		boxCull = R_CullLocalBox( cv->meshBounds );

		if ( boxCull == CULL_OUT )
		{
			tr.pc.c_box_cull_patch_out++;
			return qtrue;
		}
		else if ( boxCull == CULL_IN )
		{
			tr.pc.c_box_cull_patch_in++;
		}
		else
		{
			tr.pc.c_box_cull_patch_clip++;
		}
	}
	else
	{
		tr.pc.c_sphere_cull_patch_in++;
	}

	return qfalse;
}


static qboolean	R_CullSurface( surfaceType_t *surface, shader_t *shader ) {
	srfSurfaceFace_t *sface;
	float			d;

	if ( r_nocull->integer ) {
		return qfalse;
	}

	if ( *surface == SF_GRID ) {
		return R_CullGrid( (srfGridMesh_t *)surface );
	}

	if ( *surface == SF_TRIANGLES || *surface == SF_TERRAIN ) {
		return R_CullLocalBox( ((srfTriangles_t *)surface)->bounds ) == CULL_OUT;
	}

	if ( *surface != SF_FACE ) {
		return qfalse;
	}

	if ( shader->cullType == CT_TWO_SIDED ) {
		return qfalse;
	}

	if ( !r_facePlaneCull->integer ) {
		return qfalse;
	}

	sface = ( srfSurfaceFace_t * ) surface;
	d = DotProduct (tr.ori.viewOrigin, sface->plane.normal);

	if ( shader->cullType == CT_FRONT_SIDED ) {
		if ( d < sface->plane.dist - 8 ) {
			return qtrue;
		}
	} else {
		if ( d > sface->plane.dist + 8 ) {
			return qtrue;
		}
	}

	return qfalse;
}

static int R_FastDlightFace( srfSurfaceFace_t *face, int dlightBits ) {
	float		d;
	int			i;
	dlight_t	*dl;

	for ( i = 0 ; i < tr.refdef.num_dlights ; i++ ) {
		if ( ! ( dlightBits & ( 1 << i ) ) ) {
			continue;
		}
		dl = &tr.refdef.dlights[i];
		d = DotProduct( dl->transformed, face->plane.normal ) - face->plane.dist;
		if ( d < -dl->radius || d > dl->radius ) {
			dlightBits &= ~( 1 << i );
		}
	}

	if ( !dlightBits ) {
		tr.pc.c_dlightSurfacesCulled++;
	}

	face->dlightBits[ tr.smpFrame ] = dlightBits;
	return dlightBits;
}

static int R_DlightGrid( srfGridMesh_t *grid, int dlightBits ) {
	int			i;
	dlight_t	*dl;

	for ( i = 0 ; i < tr.refdef.num_dlights ; i++ ) {
		if ( ! ( dlightBits & ( 1 << i ) ) ) {
			continue;
		}
		dl = &tr.refdef.dlights[i];
		if ( dl->transformed[0] - dl->radius > grid->meshBounds[1][0]
			|| dl->transformed[0] + dl->radius < grid->meshBounds[0][0]
			|| dl->transformed[1] - dl->radius > grid->meshBounds[1][1]
			|| dl->transformed[1] + dl->radius < grid->meshBounds[0][1]
			|| dl->transformed[2] - dl->radius > grid->meshBounds[1][2]
			|| dl->transformed[2] + dl->radius < grid->meshBounds[0][2] ) {
			dlightBits &= ~( 1 << i );
		}
	}

	if ( !dlightBits ) {
		tr.pc.c_dlightSurfacesCulled++;
	}

	grid->dlightBits[ tr.smpFrame ] = dlightBits;
	return dlightBits;
}

float R_CalcWorldSurfaceAlpha( srfTriangles_t *srf, const vec3_t vec ) {
	vec3_t	delta;
	float	f4, f31, f30, d, f0;
	float	alpha;

	// retail's fixed order: shaderTime is stamped BEFORE the sentinel test
	alpha = 1.0f;
	srf->shaderTime = 1.0f;

	if ( srf->verts[2].lodExtra == -1000.0f ) {
		return 1.0f;
	}

	f4 = 1.0f;
	if ( srf->numVerts > 3 ) {
		f4 = srf->verts[3].lodExtra;
	}

	f31 = f4 * ( tr.viewParms.worldSurfaceFade * r_fadeDistMod->value );
	f30 = 0.9f * f31;

	if ( f31 <= 0.0f ) {
		srf->shaderTime = alpha;
		return alpha;
	}

	delta[0] = tr.viewParms.ori.origin[0] - ( vec[0] + srf->localOrigin[0] );
	delta[1] = tr.viewParms.ori.origin[1] - ( vec[1] + srf->localOrigin[1] );
	delta[2] = tr.viewParms.ori.origin[2] - ( vec[2] + srf->localOrigin[2] );
	d = VectorLength( delta );

	f0 = d * (float)tan( M_PI * tr.refdef.fov_x / 360.0 );

	if ( f0 > f31 ) {
		alpha = 0.0f;
	} else if ( f0 >= f30 ) {
		alpha = 1.0f - ( f0 - f30 ) / ( f31 - f30 );
	}

	srf->shaderTime = alpha;
	return alpha;
}

int R_CalcFogNum(const vec3_t worldOrigin, float radius) {
	int		i;
	fog_t	*fog;
	float	r = radius * 0.5f;
	float	z = worldOrigin[2] + r;

	if (tr.refdef.rdflags & RDF_NOWORLDMODEL) {
		return 0;
	}
	if (tr.world->numfogs <= 1) {
		return 0;
	}
	if (tr.viewParms.isPortalSky) {
		return 0;
	}

	for (i = 1; i < tr.world->numfogs; i++) {
		fog = &tr.world->fogs[i];
		if (worldOrigin[0] - r < fog->bounds[1][0] && worldOrigin[0] + r > fog->bounds[0][0]
			&& worldOrigin[1] - r < fog->bounds[1][1] && worldOrigin[1] + r > fog->bounds[0][1]
			&& z - r < fog->bounds[1][2] && z + r > fog->bounds[0][2]) {
			return i;
		}
	}
	return 0;
}

static void R_CalcSurfLightmap( msurface_t *surf, baseLightingSurf_t *bls ) {
	world_t			*world = tr.world;
	int				i, j, k;
	int				found;
	int				src, dst, srcStride, dstStride;
	int				v[3];
	int				num, first, inlineColor;
	float			scale;
	lightingSurf_t	*ls;

	(void)surf;

	if ( !world ) {
		return;
	}
	if ( !world->baseLightmapFrame ) {
		return;
	}

	found = 0;
	for ( i = 0; i < bls->numLightingSurfaces; i++ ) {
		ls = &world->lightingSurfs[ bls->firstLightingSurface + i ];
		if ( world->lightGroupFrame[ ls->lightGroupNum ] >= bls->lastUpdatedFrame ) {
			found = 1;
			break;
		}
	}
	if ( !found ) {
		return;
	}

	world->baseLightmapFrame[ bls->baseLightmapNum ] = 1;
	bls->lastUpdatedFrame = tr.frameCount;	 

	src = 3 * ( bls->baseLightmapNum * ( LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT )
		+ bls->baseLightmapY * LIGHTMAP_WIDTH + bls->baseLightmapX );
	dst = 4 * ( bls->baseLightmapNum * ( LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT )
		+ bls->baseLightmapY * LIGHTMAP_WIDTH + bls->baseLightmapX );
	srcStride = 3 * ( LIGHTMAP_WIDTH - bls->baseLightmapWidth );
	dstStride = 4 * ( LIGHTMAP_WIDTH - bls->baseLightmapWidth );

	for ( i = 0; i < bls->baseLightmapHeight; i++ ) {
		for ( j = 0; j < bls->baseLightmapWidth; j++ ) {
			world->baseLightmapImage[ dst + 0 ] = world->baseLightmapData[ src + 0 ];
			world->baseLightmapImage[ dst + 1 ] = world->baseLightmapData[ src + 1 ];
			world->baseLightmapImage[ dst + 2 ] = world->baseLightmapData[ src + 2 ];
			src += 3;
			dst += 4;
		}
		src += srcStride;
		dst += dstStride;
	}

	num = bls->numLightingSurfaces;
	first = bls->firstLightingSurface;

	for ( i = 0; i < num; i++ ) {
		ls = &world->lightingSurfs[ first + i ];

		scale = world->lightGroupIntensity[ ls->lightGroupNum ];
		if ( !scale ) {
			continue;
		}
		scale = scale / 255.0f;		 

		inlineColor = ( ls->lightmapNum == -1 );	 

		src = 3 * ( ls->lightmapNum * ( LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT )
			+ ls->lightmapY * LIGHTMAP_WIDTH + ls->lightmapX );
		dst = 4 * ( bls->baseLightmapNum * ( LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT )
			+ bls->baseLightmapY * LIGHTMAP_WIDTH + bls->baseLightmapX );

		for ( j = 0; j < bls->baseLightmapHeight; j++ ) {
			for ( k = 0; k < bls->baseLightmapWidth; k++ ) {
				int c0;

				for ( c0 = 0; c0 < 3; c0++ ) {
					int c = inlineColor ? ls->color[c0] : world->contLightmapData[ src ];
					int d = world->baseLightmapImage[ dst ];

					if ( !inlineColor ) {
						src++;
					}
					v[c0] = (int)( (float)c * scale + (float)d );
					dst++;
				}

				if ( ( v[0] | v[1] | v[2] ) > 255 ) {
					int max = v[0] > v[1] ? v[0] : v[1];
					max = max > v[2] ? max : v[2];
					v[0] = 255 * v[0] / max;
					v[1] = 255 * v[1] / max;
					v[2] = 255 * v[2] / max;
				}

				world->baseLightmapImage[ dst - 3 ] = v[0];
				world->baseLightmapImage[ dst - 2 ] = v[1];
				world->baseLightmapImage[ dst - 1 ] = v[2];
				dst++;	/* RGB reads and writes three bytes, the buffer holds four */
			}
			src += srcStride;
			dst += dstStride;
		}
	}
}

static void R_CalcSurfLightVerts( msurface_t *surf, baseLightingSurf_t *bls,
	srfSurfaceFace_t *face, srfTriangles_t *tris ) {
	world_t				*world = tr.world;
	int					i, j, k;
	int					found_group, updated, count;
	int					src, dst;
	int					v[3];
	int					inlineColor;
	float				scale;
	lightingVertSurf_t	*lvs;

	(void)surf;

	found_group = 0;
	updated = 0;

	for ( i = 0; i < bls->numLightingVertSurfaces; i++ ) {
		lvs = &world->lightingVertSurfs[ bls->firstLightingVertSurface + i ];
		if ( world->lightGroupIntensity[ lvs->lightGroupNum ] >= 255 ) {
			continue;
		}
		found_group = 1;
		if ( world->lightGroupFrame[ lvs->lightGroupNum ] >= bls->lastUpdatedFrame ) {
			updated = 1;
			break;
		}
	}

	count = 0;
	if ( found_group ) {
		if ( face ) {
			face->color = world->baseLightingVertsWork + 3 * bls->baseLightingVerts;
			count = face->numPoints;
		} else if ( tris ) {
			tris->color = world->baseLightingVertsWork + 3 * bls->baseLightingVerts;
			count = tris->numVerts;
		}
	}

	if ( !updated ) {
		return;
	}

	bls->lastUpdatedFrame = tr.frameCount;

	Com_Memcpy( world->baseLightingVertsWork + 3 * bls->baseLightingVerts,
		world->baseLightingVerts + 3 * bls->baseLightingVerts, 3 * count );

	for ( i = 0; i < bls->numLightingVertSurfaces; i++ ) {
		lvs = &world->lightingVertSurfs[ bls->firstLightingVertSurface + i ];

		scale = world->lightGroupIntensity[ lvs->lightGroupNum ];
		if ( !scale ) {
			continue;
		}
		scale = scale / 255.0f;

		inlineColor = ( lvs->firstLightingVert == -1 );

		src = 3 * lvs->firstLightingVert;
		dst = 3 * bls->baseLightingVerts;

		for ( j = 0; j < count; j++ ) {
			for ( k = 0; k < 3; k++ ) {
				int c = inlineColor ? lvs->color[k] : world->contLightingVerts[ src ];
				int d = world->baseLightingVertsWork[ dst ];

				if ( !inlineColor ) {
					src++;
				}
				v[k] = (int)( (float)c * scale + (float)d );
				dst++;
			}

			if ( ( v[0] | v[1] | v[2] ) > 255 ) {
				int max = v[0] > v[1] ? v[0] : v[1];
				max = max > v[2] ? max : v[2];
				v[0] = 255 * v[0] / max;
				v[1] = 255 * v[1] / max;
				v[2] = 255 * v[2] / max;
			}

			world->baseLightingVertsWork[ dst - 3 ] = v[0];
			world->baseLightingVertsWork[ dst - 2 ] = v[1];
			world->baseLightingVertsWork[ dst - 1 ] = v[2];
		}
	}
}

void R_CalcSurfLighting( msurface_t *surf ) {
	world_t				*world;
	int					*v5;
	int					v2;
	int					v6;
	srfSurfaceFace_t	*v3 = NULL;
	srfTriangles_t		*v4 = NULL;
	baseLightingSurf_t	*bls;

	world = tr.world;
	if ( !world || !world->numBaseLightingSurfs ) {
		return;
	}

	v5 = (int *)surf->data;
	v6 = v5[0];
	v2 = -1;

	if ( v6 == SF_GRID ) {
		v2 = v5[18];			
	} else if ( v6 >= 3 ) {
		if ( v6 < 5 ) {			
			v2 = v5[14];		
			v4 = (srfTriangles_t *)v5;
			v4->color = NULL;
		}
	} else if ( v6 >= 2 ) {		
		v2 = v5[8];				
		v3 = (srfSurfaceFace_t *)v5;
		v3->color = NULL;
	}

	if ( v2 == -1 ) {
		return;
	}

	bls = (baseLightingSurf_t *)( (byte *)world->baseLightingSurfs + 44 * v2 );

	if ( bls->baseLightmapNum != -1 ) {
		R_CalcSurfLightmap( surf, bls );
		return;
	}

	if ( bls->baseLightingVerts == -1 ) {
		return;
	}
	if ( !v3 && !v4 ) {
		return;
	}

	R_CalcSurfLightVerts( surf, bls, v3, v4 );
}

void R_ClearDynamicLightmaps( void ) {
	int		i;

	if ( !tr.world ) {
		return;
	}

	for ( i = 0; i < tr.numBaseLightmaps; i++ ) {
		tr.world->baseLightmapFrame[i] = 0;
	}
}

void R_UploadDynamicLightmaps( void ) {
	int		i;

	if ( !tr.world ) {
		return;
	}

	for ( i = 0; i < tr.numBaseLightmaps; i++ ) {
		if ( !tr.world->baseLightmapFrame[i] ) {
			continue;
		}
		GL_Bind( tr.baseLightmaps[i] );
		qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT,
			GL_RGBA, GL_UNSIGNED_BYTE,
			tr.world->baseLightmapImage + i * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 4 );
	}
}

static void R_AddWorldSurface( msurface_t *surf, int dlightBits, const trRefEntity_t *ent ) {
	vec3_t			vec;
	surfaceType_t	*surface;
	shader_t		*shader;
	float			alpha;

	vec[0] = vec[1] = vec[2] = 0.0f;

	if ( surf->viewCount == tr.viewCount ) {
		return;
	}

	if ( !r_drawcurves->integer && *surf->data == SF_GRID ) {
		return;
	}

	if ( !r_drawterrain->integer ) {
		int type = *surf->data;

		if ( type == SF_TERRAIN ) {
			return;
		}
		if ( type == SF_TRIANGLES
			&& ((srfTriangles_t *)surf->data)->verts[2].lodExtra == -1000.0f ) {
			return;
		}
	}

	surf->viewCount = tr.viewCount;

	if ( ent ) {
		VectorCopy( ent->e.origin, vec );
	}

	surface = surf->data;
	shader = surf->shader;

	if ( *surface == SF_TRIANGLES ) {
		alpha = R_CalcWorldSurfaceAlpha( (srfTriangles_t *)surface, vec );
	} else {
		alpha = 1.0f;
	}

	if ( alpha == 0.0f ) {
		return;
	}

	if ( alpha < 1.0f ) {
		shader = R_FindShaderForceAlphaLM( surf->shader->name, -3 );
	}

	if ( R_CullSurface( surface, shader ) ) {
		return;
	}

	if ( dlightBits ) {
		if ( *surface == SF_FACE ) {
			dlightBits = R_FastDlightFace( (srfSurfaceFace_t *)surface, dlightBits );
		} else if ( *surface == SF_GRID ) {
			dlightBits = R_DlightGrid( (srfGridMesh_t *)surface, dlightBits );
		} else if ( *surface == SF_TRIANGLES || *surface == SF_TERRAIN ) {
			((srfTriangles_t *)surface)->dlightBits[ tr.smpFrame ] = dlightBits;
		} else {
			dlightBits = 0;
		}

		if ( dlightBits ) {
			tr.pc.c_dlightSurfaces++;
		}

		dlightBits = ( dlightBits != 0 );
	}

	if ( shader && shader->isPortalSky ) {
		// 0x2CDF8, so the lighting call must run before this return.
		R_CalcSurfLighting( surf );
		R_Sky_AddSurf( surf );
		return;
	}

	R_AddDrawSurf( surface, shader, surf->fogIndex, dlightBits );

	R_CalcSurfLighting( surf );
}


void R_AddBrushModelSurfaces ( trRefEntity_t *ent ) {
	bmodel_t	*bmodel;
	int			clip;
	model_t		*pModel;
	int			i;

	pModel = R_GetModelByHandle( ent->e.hModel );

	bmodel = pModel->d.bmodel;

	clip = R_CullLocalBox( bmodel->bounds );
	if ( clip == CULL_OUT ) {
		return;
	}
	
	R_DlightBmodel( bmodel );

	for ( i = 0 ; i < bmodel->numSurfaces ; i++ ) {
		R_AddWorldSurface( bmodel->firstSurface + i, tr.currentEntity->needDlights, tr.currentEntity );
	}
}



void R_GetInlineModelBounds(int iIndex, vec3_t vMins, vec3_t vMaxs)
{
	bmodel_t* bmodel;

	bmodel = &tr.world->bmodels[iIndex];
	VectorCopy(bmodel->bounds[0], vMins);
	VectorCopy(bmodel->bounds[1], vMaxs);
}


static void R_RecursiveWorldNode( mnode_t *node, int planeBits, int dlightBits ) {

	do {
		if (node->visframe != tr.visCount) {
			return;
		}


		if ( !r_nocull->integer ) {
			int		r;

			if (planeBits & 1) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[0]);
				if (r == 2) {
					return;						
				}
				if (r == 1) {
					planeBits &= ~1;			
				}
			}

			if (planeBits & 2) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[1]);
				if (r == 2) {
					return;						
				}
				if (r == 1) {
					planeBits &= ~2;			
				}
			}

			if (planeBits & 4) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[2]);
				if (r == 2) {
					return;						
				}
				if (r == 1) {
					planeBits &= ~4;			
				}
			}

			if (planeBits & 8) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[3]);
				if (r == 2) {
					return;						
				}
				if (r == 1) {
					planeBits &= ~8;			
				}
			}

			if ((planeBits & 16) && tr.viewParms.fog.extrafrustums) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[4]);
				if (r == 2) {
					return;						
				}
				if (r == 1) {
					planeBits &= ~16;			
				}
			}

		}

		if ( node->contents != -1 ) {
			break;
		}

		R_RecursiveWorldNode (node->children[0], planeBits, dlightBits);

		node = node->children[1];
	} while ( 1 );

	{
		int			c;
		msurface_t	**mark;

		tr.pc.c_leafs++;

		if ( node->mins[0] < tr.viewParms.visBounds[0][0] ) {
			tr.viewParms.visBounds[0][0] = node->mins[0];
		}
		if ( node->mins[1] < tr.viewParms.visBounds[0][1] ) {
			tr.viewParms.visBounds[0][1] = node->mins[1];
		}
		if ( node->mins[2] < tr.viewParms.visBounds[0][2] ) {
			tr.viewParms.visBounds[0][2] = node->mins[2];
		}

		if ( node->maxs[0] > tr.viewParms.visBounds[1][0] ) {
			tr.viewParms.visBounds[1][0] = node->maxs[0];
		}
		if ( node->maxs[1] > tr.viewParms.visBounds[1][1] ) {
			tr.viewParms.visBounds[1][1] = node->maxs[1];
		}
		if ( node->maxs[2] > tr.viewParms.visBounds[1][2] ) {
			tr.viewParms.visBounds[1][2] = node->maxs[2];
		}

		tr.portalsky.cntNode = node;

		mark = node->firstmarksurface;
		c = node->nummarksurfaces;
		while (c--) {
			R_AddWorldSurface(*mark, dlightBits, NULL);
			mark++;
		}
	}

}

static void R_RecursiveWorldNodeGlobal( mnode_t *node, int planeBits, int dlightBits ) {

	do {
		// NOTE: there is deliberately NO `node->visframe != tr.visCount' test here --
		if ( !r_nocull->integer ) {
			int		r;

			if (planeBits & 1) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[0]);
				if (r == 2) {
					return;						
				}
				if (r == 1) {
					planeBits &= ~1;			
				}
			}

			if (planeBits & 2) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[1]);
				if (r == 2) {
					return;						
				}
				if (r == 1) {
					planeBits &= ~2;			
				}
			}

			if (planeBits & 4) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[2]);
				if (r == 2) {
					return;						
				}
				if (r == 1) {
					planeBits &= ~4;			
				}
			}

			if (planeBits & 8) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[3]);
				if (r == 2) {
					return;						
				}
				if (r == 1) {
					planeBits &= ~8;			
				}
			}

			if (tr.viewParms.fog.extrafrustums && (planeBits & 16)) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[4]);
				if (r == 2) {
					return;						
				}
				if (r == 1) {
					planeBits &= ~16;			
				}
			}
		}

		if ( node->maxs[2] < tr.viewParms.terrainGlobalMinZ ) {
			return;
		}

		if ( node->contents != -1 ) {
			break;
		}

		R_RecursiveWorldNodeGlobal (node->children[0], planeBits, dlightBits);

		node = node->children[1];
	} while ( 1 );

	{
		int			c;
		msurface_t	**mark;

		tr.pc.c_leafs++;

		if ( node->mins[0] < tr.viewParms.visBounds[0][0] ) {
			tr.viewParms.visBounds[0][0] = node->mins[0];
		}
		if ( node->mins[1] < tr.viewParms.visBounds[0][1] ) {
			tr.viewParms.visBounds[0][1] = node->mins[1];
		}
		if ( node->mins[2] < tr.viewParms.visBounds[0][2] ) {
			tr.viewParms.visBounds[0][2] = node->mins[2];
		}

		if ( node->maxs[0] > tr.viewParms.visBounds[1][0] ) {
			tr.viewParms.visBounds[1][0] = node->maxs[0];
		}
		if ( node->maxs[1] > tr.viewParms.visBounds[1][1] ) {
			tr.viewParms.visBounds[1][1] = node->maxs[1];
		}
		if ( node->maxs[2] > tr.viewParms.visBounds[1][2] ) {
			tr.viewParms.visBounds[1][2] = node->maxs[2];
		}

		tr.portalsky.cntNode = node;

		mark = node->firstmarksurface;
		c = node->nummarksurfaces;
		while (c--) {
			msurface_t	*surf = *mark;
			surfaceType_t *surface = surf->data;

			if ( *surface == SF_TRIANGLES ) {
				srfTriangles_t	*tri = (srfTriangles_t *)surface;
				drawVert_t		*verts = tri->verts;
				int				found;
				int				n;
				int				ofs;

				if ( verts[2].lodExtra == -1000.0f ) {
					found = 0;
					n = tri->numVerts;
					ofs = 0;
					while ( n > 0 ) {
						if ( *(float *)( (byte *)verts + ofs + 8 ) > tr.viewParms.terrainGlobalMinZ ) {
							found = 1;
							break;
						}
						ofs += 48;
						n--;
					}
					if ( found ) {
						R_AddWorldSurface( surf, (1 << tr.refdef.num_dlights) - 1, NULL );
					}
				}
			}
			mark++;
		}
	}

}

int R_SphereInLeafs(const vec3_t p, float r, mnode_t** nodes, int nMaxNodes) {
	mnode_t* nodestack[1024];
	int iNodeStackPos;
	mnode_t* pCurNode;
	int nFoundNodes;

	iNodeStackPos = 0;
	pCurNode = tr.world->nodes;
	nFoundNodes = 0;

	while (1)
    {
        cplane_t* plane;
		float d;

		while (pCurNode->contents == -1)
		{
			plane = pCurNode->plane;
			if (plane->type >= PLANE_NON_AXIAL) {
				d = DotProduct(p, plane->normal) - plane->dist;
			} else {
				d = p[plane->type] - plane->dist;
			}

            if (d < r) {
                if (d > -r) {
                    nodestack[iNodeStackPos++] = pCurNode->children[0];
                }
                pCurNode = pCurNode->children[1];
            } else {
                pCurNode = pCurNode->children[0];
			}
		}

		nodes[nFoundNodes++] = pCurNode;
		if (!iNodeStackPos || nFoundNodes == nMaxNodes) {
			break;
		}

		iNodeStackPos--;
        pCurNode = nodestack[iNodeStackPos];
	}

	return nFoundNodes;
}
mnode_t *R_PointInLeaf( const vec3_t p ) {
	mnode_t		*node;
	float		d;
	cplane_t	*plane;
	
	if ( !tr.world ) {
		ri.Error (ERR_DROP, "R_PointInLeaf: bad model");
	}
	
	node = tr.world->nodes;
	while( 1 ) {
		if (node->contents != -1) {
			break;
		}
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0) {
			node = node->children[0];
		} else {
			node = node->children[1];
		}
	}
	
	return node;
}

static const byte *R_ClusterPVS (int cluster) {
	if (!tr.world || !tr.world->vis || cluster < 0 || cluster >= tr.world->numClusters ) {
		return tr.world->novis;
	}

	return tr.world->vis + cluster * tr.world->clusterBytes;
}

qboolean R_inPVS( const vec3_t p1, const vec3_t p2 ) {
	mnode_t *leaf;
	byte	*vis;

	leaf = R_PointInLeaf( p1 );
	vis = ri.CM_ClusterPVS( leaf->cluster );
	leaf = R_PointInLeaf( p2 );

	if ( !(vis[leaf->cluster>>3] & (1<<(leaf->cluster&7))) ) {
		return qfalse;
	}
	return qtrue;
}

static void R_MarkLeaves (void) {
	const byte	*vis;
	mnode_t	*leaf, *parent;
	int		i;
	int		cluster;

	if ( r_lockpvs->integer ) {
		return;
	}

	leaf = R_PointInLeaf( tr.viewParms.pvsOrigin );
	cluster = leaf->cluster;


	if ( tr.viewCluster == cluster && !tr.refdef.areamaskModified 
		&& !r_showcluster->modified ) {
		return;
	}

	if ( r_showcluster->modified || r_showcluster->integer ) {
		r_showcluster->modified = qfalse;
		if ( r_showcluster->integer ) {
			ri.Printf( PRINT_ALL, "cluster:%i  area:%i\n", cluster, leaf->area );
		}
	}

	tr.visCount++;
	tr.viewCluster = cluster;

	if ( r_novis->integer || tr.viewCluster == -1 ) {
		for (i=0 ; i<tr.world->numnodes ; i++) {
			if (tr.world->nodes[i].contents != CONTENTS_SOLID) {
				tr.world->nodes[i].visframe = tr.visCount;
			}
		}
		return;
	}

	vis = R_ClusterPVS (tr.viewCluster);
	
	for (i=0,leaf=tr.world->nodes ; i<tr.world->numnodes ; i++, leaf++) {
		cluster = leaf->cluster;
		if ( cluster < 0 || cluster >= tr.world->numClusters ) {
			continue;
		}

		if ( !(vis[cluster>>3] & (1<<(cluster&7))) ) {
			continue;
		}

		if ( (tr.refdef.areamask[leaf->area>>3] & (1<<(leaf->area&7)) ) ) {
			continue;		
		}

		parent = leaf;
		do {
			if (parent->visframe == tr.visCount)
				break;
			parent->visframe = tr.visCount;
			parent = parent->parent;
		} while (parent);
	}
}


void R_AddWorldSurfaces (void) {
	if (!r_drawworld->integer) {
		return;
	}

	if (tr.refdef.rdflags & RDF_NOWORLDMODEL) {
		return;
	}

	tr.currentEntityNum = ENTITYNUM_WORLD;
	tr.shiftedEntityNum = tr.currentEntityNum << QSORT_ENTITYNUM_SHIFT;

	R_ClearDynamicLightmaps();

	R_MarkLeaves();

	ClearBounds(tr.viewParms.visBounds[0], tr.viewParms.visBounds[1]);

	if (tr.refdef.num_dlights > 32) {
		tr.refdef.num_dlights = 32;
	}
	R_RecursiveWorldNode(tr.world->nodes, 31, (1 << tr.refdef.num_dlights) - 1);

	if ( r_terrain_global->integer ) {
		tr.viewParms.terrainGlobalRun = 1;
		tr.viewParms.terrainGlobalMinZ = r_terrain_global_min->value;
	}
	if ( tr.viewParms.terrainGlobalRun ) {
		R_RecursiveWorldNodeGlobal( tr.world->nodes, 31, (1 << tr.refdef.num_dlights) - 1 );
	}

	R_UploadDynamicLightmaps();
}
