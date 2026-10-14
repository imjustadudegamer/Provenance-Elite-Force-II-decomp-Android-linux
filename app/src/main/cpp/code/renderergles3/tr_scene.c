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
#include "../corepp/tiki.h"

int			r_firstSceneDrawSurf;
int			r_firstSceneSpriteSurf;

int			r_numdlights;
int			r_firstSceneDlight;

int			r_numlensflares;
int			r_firstSceneLensflare;

int			r_numentities;
int			r_firstSceneEntity;

int			r_numsprites;
int			r_firstSceneSprite;

int			r_numpolys;
int			r_firstScenePoly;

int			r_numpolyverts;


void R_InitNextFrame( void ) {
	tr.smpFrame ^= 1;
	backEndData = backEndDataBuf[tr.smpFrame];
	backEndData->commands.used = 0;

	/* The frame's pose arena, rewound ONCE per frame -- here, at the boundary --
	   and never per view.  A reset inside RE_RenderScene rewinds the cursor at the
	   second view of a frame, and that view's poses overwrite the first view's
	   before the walk has read them, so the first view draws with the second
	   view's skeleton.  Kept in step with renderervulkan/tr_scene.c; control:
	   docs/controls/bonearm. */
	TIKI_Reset_Caches_Arena( tr.smpFrame );

	r_firstSceneDrawSurf = 0;
	r_firstSceneSpriteSurf = 0;

	r_numdlights = 0;
	r_firstSceneDlight = 0;

	r_numlensflares = 0;
	r_firstSceneLensflare = 0;

	r_numentities = 0;
	r_firstSceneEntity = 0;

	r_numsprites = 0;
	r_firstSceneSprite = 0;

	r_numpolys = 0;
	r_firstScenePoly = 0;

	r_numpolyverts = 0;
}


void RE_ClearScene( void ) {
	r_firstSceneDlight = r_numdlights;
	r_firstSceneLensflare = r_numlensflares;
	r_firstSceneEntity = r_numentities;
	r_firstSceneSprite = r_numsprites;
	r_firstScenePoly = r_numpolys;
}


void R_AddPolygonSurfaces( void ) {
	int			i;
	shader_t	*sh;
	srfPoly_t	*poly;

	tr.currentEntityNum = ENTITYNUM_WORLD;
	tr.shiftedEntityNum = ENTITYNUM_WORLD << QSORT_ENTITYNUM_SHIFT;

	for ( i = 0, poly = tr.refdef.polys; i < tr.refdef.numPolys ; i++, poly++ ) {
		if ( ( poly->renderfx & RF_THIRD_PERSON ) && !tr.viewParms.isPortal ) {
			continue;
		}

		if ( ( ( poly->renderfx & RF_SKYENTITY ) != 0 )
			!= ( tr.viewParms.isPortalSky != 0 ) ) {
			continue;
		}

		if ( tr.viewParms.isPortal ) {
			if ( !( poly->renderfx & ( RF_PORTALENTITY | RF_DUALENTITY ) ) ) {
				continue;
			}
		} else if ( poly->renderfx & RF_PORTALENTITY ) {
			continue;
		}

		sh = R_GetShaderByHandle( poly->hShader );
		R_AddDrawSurf( ( void * )poly, sh, poly->fogNum, 0 );
	}
}

qboolean RE_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t* verts, int renderfx) {
	srfPoly_t	*poly;

	if ( !tr.registered ) {
		return qfalse;
	}

	if (numVerts + r_numpolyverts > max_polyverts || r_numpolys >= max_polys) {
		ri.Printf(PRINT_WARNING, "Exceeded MAX POLYS\n");
		return qfalse;
	}

	poly = &backEndData->polys[r_numpolys];
	poly->surfaceType = SF_POLY;
	poly->hShader = hShader;
	poly->numVerts = numVerts;
	poly->verts = &backEndData->polyVerts[r_numpolyverts];
	poly->renderfx = renderfx;

	Com_Memcpy(poly->verts, verts, sizeof(polyVert_t) * numVerts);
	++r_numpolys;
	r_numpolyverts += numVerts;

	{
		vec3_t	mins, maxs;
		int		j;

		if (tr.world->numfogs == 1 || tr.viewParms.isPortalSky) {
			poly->fogNum = 0;
		} else {
			VectorCopy(poly->verts[0].xyz, mins);
			VectorCopy(poly->verts[0].xyz, maxs);
			for (j = 1; j < numVerts; j++) {
				AddPointToBounds(poly->verts[j].xyz, mins, maxs);
			}
			for (j = 1; j < tr.world->numfogs; j++) {
				fog_t *fog = &tr.world->fogs[j];
				if (maxs[0] >= fog->bounds[0][0] && maxs[1] >= fog->bounds[0][1] && maxs[2] >= fog->bounds[0][2]
					&& mins[0] <= fog->bounds[1][0] && mins[1] <= fog->bounds[1][1] && mins[2] <= fog->bounds[1][2]) {
					break;
				}
			}
			poly->fogNum = (j == tr.world->numfogs) ? 0 : j;
		}
	}

	return qtrue;
}

void RE_AddTerrainMarkToScene(int iTerrainIndex, qhandle_t hShader, int numVerts, const polyVert_t* verts, int renderfx) {
    (void)iTerrainIndex;

    RE_AddPolyToScene(hShader, numVerts, verts, renderfx);
}


refEntity_t* RE_GetRenderEntity(int entityNumber) {
    int i;

    for (i = 0; i < r_numentities; i++) {
        if (backEndData->entities[i].e.entityNumber == entityNumber) {
            return &backEndData->entities[i].e;
        }
    }

    return NULL;
}

void RE_AddRefEntityToScene( const refEntity_t *ent ) {
	if ( !tr.registered ) {
		return;
	}
	if ( r_numentities >= 1021 ) {
		return;
	}
	if ( (unsigned int)ent->reType >= RT_MAX_REF_ENTITY_TYPE ) {
		ri.Error( ERR_DROP, "RE_AddRefEntityToScene: bad reType %i", ent->reType );
	}

	backEndData->entities[r_numentities].e = *ent;

	backEndData->entities[r_numentities].lightDir[0] = 0.0f;

	r_numentities++;
}

void RE_AddRefSpriteToScene(const refEntity_t* ent) {
	refSprite_t* spr;
	int i;

	if (!tr.registered) {
		return;
	}

	if (r_numsprites >= MAX_SPRITES) {
		return;
	}

	spr = &backEndData->sprites[r_numsprites];
	VectorCopy(ent->origin, spr->origin);
	spr->surftype = SF_SPRITE;
    spr->hModel = ent->hModel;
    spr->scale = ent->scale;
    spr->renderfx = ent->renderfx;
    spr->shaderTime = ent->shaderTime;
	AxisCopy(ent->axis, spr->axis);

	for (i = 0; i < 4; ++i) {
		spr->shaderRGBA[i] = ent->shaderRGBA[i];
	}

    ++r_numsprites;
}

void RE_AddDynamicLightToScene( const vec3_t org, float intensity, float r, float g, float b, int type ) {
	dlight_t	*dl;

	if ( !tr.registered ) {
		return;
	}
	if ( r_numdlights >= MAX_DLIGHTS ) {
		return;
	}
	if ( intensity <= 0 ) {
		return;
	}
	dl = &backEndData->dlights[r_numdlights++];
	VectorCopy (org, dl->origin);
	dl->radius = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
	dl->type = type;
}

void RE_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b, int type ) {
	RE_AddDynamicLightToScene( org, intensity, r, g, b, type );
}


void RE_AddAdditiveLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	RE_AddDynamicLightToScene( org, intensity, r, g, b, qtrue );
}

void RE_AddLensflareToScene( void *key, const vec3_t org, int lensflareNum, float r, float g, float b, float scale, qhandle_t replacementShader ) {
	lensflare_t	*lf;

	if ( !tr.registered ) {
		return;
	}
	if ( r_numlensflares >= MAX_LENSFLARES ) {
		return;
	}

	lf = &backEndData->lensflares[r_numlensflares++];
	lf->key = key;
	VectorCopy( org, lf->origin );
	lf->num = lensflareNum;
	lf->color[0] = r;
	lf->color[1] = g;
	lf->color[2] = b;
	lf->scale = scale;
	lf->replacementShader = R_GetShaderByHandle( replacementShader );
}

void RE_RenderScene( const refdef_t *fd ) {
	viewParms_t		parms;
	int				startTime;

	if ( !tr.registered ) {
		return;
	}
	GLimp_LogComment( "====== RE_RenderScene =====\n" );

	if ( r_norefresh->integer ) {
		return;
	}

	startTime = ri.Milliseconds();

	/* FE:scene measures every front-end leg of this scene against one root, so a
	 * single [FE:scene] line decomposes one renderFront number. */
	BE_RootBegin( BE_FAM_FE );

	if (!tr.world && !( fd->rdflags & RDF_NOWORLDMODEL ) ) {
		ri.Error (ERR_DROP, "R_RenderScene: NULL worldmodel");
	}

	backEnd.in2D = qfalse;
	/* the view reopens depth, so the overlay phase has to be re-stated after it:
	   a cached window would let the next 2D draw skip its own state block */
	RB_2D_InvalidateWindow();
	tr.refdef.x = fd->x;
	tr.refdef.y = fd->y;
	tr.refdef.width = fd->width;
	tr.refdef.height = fd->height;
	tr.refdef.fov_x = fd->fov_x;
	tr.refdef.fov_y = fd->fov_y;

	VectorCopy( fd->vieworg, tr.refdef.vieworg );
	VectorCopy( fd->viewaxis[0], tr.refdef.viewaxis[0] );
	VectorCopy( fd->viewaxis[1], tr.refdef.viewaxis[1] );
	VectorCopy( fd->viewaxis[2], tr.refdef.viewaxis[2] );

	tr.refdef.time = fd->time;
	tr.refdef.rdflags = fd->rdflags;

	tr.refdef.areamaskModified = qfalse;
	if ( ! (tr.refdef.rdflags & RDF_NOWORLDMODEL) ) {
		int		areaDiff;
		int		i;

		areaDiff = 0;
		for (i = 0 ; i < MAX_MAP_AREA_BYTES/4 ; i++) {
			areaDiff |= ((int *)tr.refdef.areamask)[i] ^ ((int *)fd->areamask)[i];
			((int *)tr.refdef.areamask)[i] = ((int *)fd->areamask)[i];
		}

		if ( areaDiff ) {
			tr.refdef.areamaskModified = qtrue;
		}
	}

	tr.refdef.sky_alpha = fd->sky_alpha;
	tr.refdef.sky_portal = fd->sky_portal;

	VectorCopy(fd->sky_origin, tr.refdef.sky_origin);
	VectorCopy(fd->sky_axis[0], tr.refdef.sky_axis[0]);
	VectorCopy(fd->sky_axis[1], tr.refdef.sky_axis[1]);
	VectorCopy(fd->sky_axis[2], tr.refdef.sky_axis[2]);


	tr.refdef.floatTime = tr.refdef.time * 0.001f;

	tr.refdef.numDrawSurfs = r_firstSceneDrawSurf;
	tr.refdef.drawSurfs = backEndData->drawSurfs;

    tr.refdef.numSpriteSurfs = r_firstSceneSpriteSurf;
    tr.refdef.spriteSurfs = backEndData->spriteSurfs;

	tr.refdef.num_entities = r_numentities - r_firstSceneEntity;
	tr.refdef.entities = &backEndData->entities[r_firstSceneEntity];

	tr.refdef.num_sprites = r_numsprites - r_firstSceneSprite;
	tr.refdef.sprites = &backEndData->sprites[r_firstSceneSprite];

	tr.refdef.num_dlights = r_numdlights - r_firstSceneDlight;
	tr.refdef.dlights = &backEndData->dlights[r_firstSceneDlight];

	tr.refdef.num_lensflares = r_numlensflares - r_firstSceneLensflare;
	tr.refdef.lensflares = &backEndData->lensflares[r_firstSceneLensflare];

	tr.refdef.numPolys = r_numpolys - r_firstScenePoly;
	tr.refdef.polys = &backEndData->polys[r_firstScenePoly];

	backEndData->staticModelData = tr.refdef.staticModelData;

	if (r_dynamiclight->integer == 0 || r_vertexLight->integer == 1) {
		tr.refdef.num_dlights = 0;
	}

	tr.frameSceneNum++;
	tr.sceneCount++;

	tr.skyRendered = qfalse;
	tr.portalRendered = qfalse;
	Com_Memset( &parms, 0, sizeof( parms ) );
	parms.viewportX = tr.refdef.x;
	parms.viewportY = glConfig.vidHeight - ( tr.refdef.y + tr.refdef.height );
	parms.viewportWidth = tr.refdef.width;
	parms.viewportHeight = tr.refdef.height;
	parms.isPortal = qfalse;

	parms.fovX = tr.refdef.fov_x;
	parms.fovY = tr.refdef.fov_y;

	VectorCopy( fd->vieworg, parms.ori.origin );
	VectorCopy( fd->viewaxis[0], parms.ori.axis[0] );
	VectorCopy( fd->viewaxis[1], parms.ori.axis[1] );
	VectorCopy( fd->viewaxis[2], parms.ori.axis[2] );

	VectorCopy( fd->vieworg, parms.pvsOrigin );

	parms.farplane_distance = fd->farplane_distance;       
	parms.farplane_color[0] = fd->farplane_color[0];       
	parms.farplane_color[1] = fd->farplane_color[1];       
	parms.farplane_color[2] = fd->farplane_color[2];       
	parms.farplane_cull = fd->farplane_cull;               
	parms.farplane_fog = fd->farplane_fog;                 
	parms.terrainGlobalRun = fd->terrain_global_bool;      
	parms.terrainGlobalMinZ = fd->terrain_global_min;      
	parms.worldSurfaceFade = fd->entity_fade_dist;         

	parms.farplane_bias = fd->farplane_bias;



	R_RenderView( &parms );

	BE_RootEnd( BE_FAM_FE );

	r_firstSceneDrawSurf = tr.refdef.numDrawSurfs;
	r_firstSceneSpriteSurf = tr.refdef.numSpriteSurfs;
	r_firstSceneEntity = r_numentities;
	r_firstSceneSprite = r_numsprites;
	r_firstSceneDlight = r_numdlights;
	r_firstSceneLensflare = r_numlensflares;
	r_firstScenePoly = r_numpolys;

	tr.frontEndMsec += ri.Milliseconds() - startTime;
}
