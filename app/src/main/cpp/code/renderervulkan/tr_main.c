/*
===========================================================================
This file is part of the port of Star Trek: Voyager -- Elite Force II, whose
engine descends from id Tech 3 through OpenMoHAA.

It is free software; you can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU General Public License, COPYING.txt at the repository
root, for more details.
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later
// Modified 2026-10-04 for this port; the changes are listed in PROVENANCE.tsv.
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
#include "tr_vbo.h"
#include "tr_program.h"

trGlobals_t		tr;

static float	s_flipMatrix[16] = {
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};


refimport_t	ri;

surfaceType_t	entitySurface = SF_ENTITY;

int R_CullLocalBoxOffset(const vec3_t offset, vec3_t bounds[2]) {
	int		i, j;
	vec3_t	transformed[8];
	float	dists[8];
	vec3_t	v;
	cplane_t	*frust;
	int			anyBack;
	int			front, back;

	if ( r_nocull->integer ) {
		return CULL_CLIP;
	}

	for (i = 0 ; i < 8 ; i++) {
		v[0] = bounds[i&1][0];
		v[1] = bounds[(i>>1)&1][1];
		v[2] = bounds[(i>>2)&1][2];

		VectorCopy( tr.ori.origin, transformed[i] );
		VectorAdd(transformed[i], offset, transformed[i]);
		VectorMA( transformed[i], v[0], tr.ori.axis[0], transformed[i] );
		VectorMA( transformed[i], v[1], tr.ori.axis[1], transformed[i] );
		VectorMA( transformed[i], v[2], tr.ori.axis[2], transformed[i] );
	}

	anyBack = 0;
	for (i = 0 ; i < tr.viewParms.fog.extrafrustums + 4 ; i++) {
		frust = &tr.viewParms.frustum[i];

		front = back = 0;
		for (j = 0 ; j < 8 ; j++) {
			dists[j] = DotProduct(transformed[j], frust->normal);
			if ( dists[j] > frust->dist ) {
				front = 1;
				if ( back ) {
					break;		
				}
			} else {
				back = 1;
			}
		}
		if ( !front ) {
			return CULL_OUT;
		}
		anyBack |= back;
	}

	if ( !anyBack ) {
		return CULL_IN;		
	}

	return CULL_CLIP;		
}

int R_CullLocalBox(vec3_t bounds[2]) {
	return R_CullLocalBoxOffset(vec3_origin, bounds);
}

int R_CullLocalPointAndRadius( vec3_t pt, float radius )
{
	vec3_t transformed;

	R_LocalPointToWorld( pt, transformed );

	return R_CullPointAndRadius( transformed, radius );
}

int R_CullPointAndRadius( vec3_t pt, float radius )
{
	int		i;
	float	dist;
	cplane_t	*frust;
	qboolean mightBeClipped = qfalse;

	if ( r_nocull->integer ) {
		return CULL_CLIP;
	}

	for (i = 0 ; i < tr.viewParms.fog.extrafrustums + 4; i++)
	{
		frust = &tr.viewParms.frustum[i];

		dist = DotProduct( pt, frust->normal) - frust->dist;
		if ( dist < -radius )
		{
			return CULL_OUT;
		}
		else if ( dist <= radius ) 
		{
			mightBeClipped = qtrue;
		}
	}

	if ( mightBeClipped )
	{
		return CULL_CLIP;
	}

	return CULL_IN;		
}

int R_DistanceCullLocalPointAndRadius(float fDist, const vec3_t pt, float radius) {
	vec3_t transformed;

	R_LocalPointToWorld(pt, transformed);

	return R_DistanceCullPointAndRadius(fDist, transformed, radius);
}

int R_DistanceCullPointAndRadius(float fDist, const vec3_t pt, float radius) {
	if (!r_nocull->integer)
	{
		vec3_t vDelta;
		float fLengthSquared;
		float fTotalDistSquared;

		VectorSubtract(pt, tr.viewParms.ori.origin, vDelta);
		fLengthSquared = VectorLengthSquared(vDelta);
		fTotalDistSquared = Square(fDist + radius);

		if (fTotalDistSquared < Square(vDelta[2]) + fLengthSquared) {
			return CULL_OUT;
		} else if (Square(fDist - radius) > Square(vDelta[2]) + fLengthSquared) {
			return CULL_IN;
		}
	}

	return CULL_CLIP;
}

void R_LocalNormalToWorld (const vec3_t local, vec3_t world) {
	world[0] = local[0] * tr.ori.axis[0][0] + local[1] * tr.ori.axis[1][0] + local[2] * tr.ori.axis[2][0];
	world[1] = local[0] * tr.ori.axis[0][1] + local[1] * tr.ori.axis[1][1] + local[2] * tr.ori.axis[2][1];
	world[2] = local[0] * tr.ori.axis[0][2] + local[1] * tr.ori.axis[1][2] + local[2] * tr.ori.axis[2][2];
}

void R_LocalPointToWorld (const vec3_t local, vec3_t world) {
	world[0] = local[0] * tr.ori.axis[0][0] + local[1] * tr.ori.axis[1][0] + local[2] * tr.ori.axis[2][0] + tr.ori.origin[0];
	world[1] = local[0] * tr.ori.axis[0][1] + local[1] * tr.ori.axis[1][1] + local[2] * tr.ori.axis[2][1] + tr.ori.origin[1];
	world[2] = local[0] * tr.ori.axis[0][2] + local[1] * tr.ori.axis[1][2] + local[2] * tr.ori.axis[2][2] + tr.ori.origin[2];
}

void R_WorldToLocal (vec3_t world, vec3_t local) {
	local[0] = DotProduct(world, tr.ori.axis[0]);
	local[1] = DotProduct(world, tr.ori.axis[1]);
	local[2] = DotProduct(world, tr.ori.axis[2]);
}

void R_TransformModelToClip( const vec3_t src, const float *modelMatrix, const float *projectionMatrix,
							vec4_t eye, vec4_t dst ) {
	int i;

	for ( i = 0 ; i < 4 ; i++ ) {
		eye[i] = 
			src[0] * modelMatrix[ i + 0 * 4 ] +
			src[1] * modelMatrix[ i + 1 * 4 ] +
			src[2] * modelMatrix[ i + 2 * 4 ] +
			1 * modelMatrix[ i + 3 * 4 ];
	}

	for ( i = 0 ; i < 4 ; i++ ) {
		dst[i] = 
			eye[0] * projectionMatrix[ i + 0 * 4 ] +
			eye[1] * projectionMatrix[ i + 1 * 4 ] +
			eye[2] * projectionMatrix[ i + 2 * 4 ] +
			eye[3] * projectionMatrix[ i + 3 * 4 ];
	}
}

void R_TransformClipToWindow( const vec4_t clip, const viewParms_t *view, vec4_t normalized, vec4_t window ) {
	normalized[0] = clip[0] / clip[3];
	normalized[1] = clip[1] / clip[3];
	normalized[2] = ( clip[2] + clip[3] ) / ( 2 * clip[3] );

	window[0] = 0.5f * ( 1.0f + normalized[0] ) * view->viewportWidth;
	window[1] = 0.5f * ( 1.0f + normalized[1] ) * view->viewportHeight;
	window[2] = normalized[2];

	window[0] = (int) ( window[0] + 0.5 );
	window[1] = (int) ( window[1] + 0.5 );
}


void myGlMultMatrix( const float *a, const float *b, float *out ) {
	int		i, j;

	for ( i = 0 ; i < 4 ; i++ ) {
		for ( j = 0 ; j < 4 ; j++ ) {
			out[ i * 4 + j ] =
				a [ i * 4 + 0 ] * b [ 0 * 4 + j ]
				+ a [ i * 4 + 1 ] * b [ 1 * 4 + j ]
				+ a [ i * 4 + 2 ] * b [ 2 * 4 + j ]
				+ a [ i * 4 + 3 ] * b [ 3 * 4 + j ];
		}
	}
}

// R_AddSpriteSurfaces (@0x0001ECC0) makes no such call -- sprites do not widen

void R_RotateForEntity( const trRefEntity_t *ent, const viewParms_t *viewParms,
					   orientationr_t *ori ) {
	float	vkMatrix[16];
	vec3_t	delta;
	float	axisLength;

	if ( ent->e.reType != RT_MODEL ) {
		*ori = viewParms->world;
		return;
	}

	VectorCopy( ent->e.origin, ori->origin );

	VectorCopy( ent->e.axis[0], ori->axis[0] );
	VectorCopy( ent->e.axis[1], ori->axis[1] );
	VectorCopy( ent->e.axis[2], ori->axis[2] );

	vkMatrix[0] = ori->axis[0][0];
	vkMatrix[4] = ori->axis[1][0];
	vkMatrix[8] = ori->axis[2][0];
	vkMatrix[12] = ori->origin[0];

	vkMatrix[1] = ori->axis[0][1];
	vkMatrix[5] = ori->axis[1][1];
	vkMatrix[9] = ori->axis[2][1];
	vkMatrix[13] = ori->origin[1];

	vkMatrix[2] = ori->axis[0][2];
	vkMatrix[6] = ori->axis[1][2];
	vkMatrix[10] = ori->axis[2][2];
	vkMatrix[14] = ori->origin[2];

	vkMatrix[3] = 0;
	vkMatrix[7] = 0;
	vkMatrix[11] = 0;
	vkMatrix[15] = 1;

	myGlMultMatrix( vkMatrix, viewParms->world.modelMatrix, ori->modelMatrix );

	VectorSubtract( viewParms->ori.origin, ori->origin, delta );

	if ( ent->e.nonNormalizedAxes ) {
		axisLength = VectorLength( ent->e.axis[0] );
		if ( !axisLength ) {
			axisLength = 0;
		} else {
			axisLength = 1.0f / axisLength;
		}
	} else {
		axisLength = 1.0f;
	}

	ori->viewOrigin[0] = DotProduct( delta, ori->axis[0] ) * axisLength;
	ori->viewOrigin[1] = DotProduct( delta, ori->axis[1] ) * axisLength;
	ori->viewOrigin[2] = DotProduct( delta, ori->axis[2] ) * axisLength;
}

void R_RotateForStaticModel( cStaticModelUnpacked_t *SM, const viewParms_t *viewParms,
					   orientationr_t *ori ) {
	float	vkMatrix[16];
	vec3_t	delta;
	float	tiki_scale;

	tiki_scale = SM->tiki->load_scale * SM->scale;

	VectorCopy( SM->origin, ori->origin );

	VectorCopy( SM->axis[0], ori->axis[0] );
	VectorCopy( SM->axis[1], ori->axis[1] );
	VectorCopy( SM->axis[2], ori->axis[2] );

	vkMatrix[0]  = ori->axis[0][0] * tiki_scale;
	vkMatrix[4]  = ori->axis[1][0] * tiki_scale;
	vkMatrix[8]  = ori->axis[2][0] * tiki_scale;
	vkMatrix[12] = ori->origin[0];

	vkMatrix[1]  = ori->axis[0][1] * tiki_scale;
	vkMatrix[5]  = ori->axis[1][1] * tiki_scale;
	vkMatrix[9]  = ori->axis[2][1] * tiki_scale;
	vkMatrix[13] = ori->origin[1];

	vkMatrix[2]  = ori->axis[0][2] * tiki_scale;
	vkMatrix[6]  = ori->axis[1][2] * tiki_scale;
	vkMatrix[10] = ori->axis[2][2] * tiki_scale;
	vkMatrix[14] = ori->origin[2];

	vkMatrix[3]  = 0;
	vkMatrix[7]  = 0;
	vkMatrix[11] = 0;
	vkMatrix[15] = 1;

	myGlMultMatrix( vkMatrix, viewParms->world.modelMatrix, ori->modelMatrix );

	VectorSubtract( viewParms->ori.origin, ori->origin, delta );

	ori->viewOrigin[0] = DotProduct( delta, ori->axis[0] );
	ori->viewOrigin[1] = DotProduct( delta, ori->axis[1] );
	ori->viewOrigin[2] = DotProduct( delta, ori->axis[2] );
}

void R_RotateForViewer (void) 
{
	float	viewerMatrix[16];
	vec3_t	origin;

	Com_Memset (&tr.ori, 0, sizeof(tr.ori));
	tr.ori.axis[0][0] = 1;
	tr.ori.axis[1][1] = 1;
	tr.ori.axis[2][2] = 1;
	VectorCopy (tr.viewParms.ori.origin, tr.ori.viewOrigin);

	VectorCopy( tr.viewParms.ori.origin, origin );

	viewerMatrix[0] = tr.viewParms.ori.axis[0][0];
	viewerMatrix[4] = tr.viewParms.ori.axis[0][1];
	viewerMatrix[8] = tr.viewParms.ori.axis[0][2];
	viewerMatrix[12] = -origin[0] * viewerMatrix[0] + -origin[1] * viewerMatrix[4] + -origin[2] * viewerMatrix[8];

	viewerMatrix[1] = tr.viewParms.ori.axis[1][0];
	viewerMatrix[5] = tr.viewParms.ori.axis[1][1];
	viewerMatrix[9] = tr.viewParms.ori.axis[1][2];
	viewerMatrix[13] = -origin[0] * viewerMatrix[1] + -origin[1] * viewerMatrix[5] + -origin[2] * viewerMatrix[9];

	viewerMatrix[2] = tr.viewParms.ori.axis[2][0];
	viewerMatrix[6] = tr.viewParms.ori.axis[2][1];
	viewerMatrix[10] = tr.viewParms.ori.axis[2][2];
	viewerMatrix[14] = -origin[0] * viewerMatrix[2] + -origin[1] * viewerMatrix[6] + -origin[2] * viewerMatrix[10];

	viewerMatrix[3] = 0;
	viewerMatrix[7] = 0;
	viewerMatrix[11] = 0;
	viewerMatrix[15] = 1;

	myGlMultMatrix( viewerMatrix, s_flipMatrix, tr.ori.modelMatrix );

	tr.viewParms.world = tr.ori;

}

static void SetFarClip( void )
{
	float	farthestCornerDistance = 0;
	int		i;

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		tr.viewParms.zFar = 2048;
		return;
	}

	farthestCornerDistance = 0;
	for ( i = 0; i < 8; i++ )
	{
		vec3_t v;
		vec3_t vecTo;
		float distance;

		if ( i & 1 )
		{
			v[0] = tr.viewParms.visBounds[0][0];
		}
		else
		{
			v[0] = tr.viewParms.visBounds[1][0];
		}

		if ( i & 2 )
		{
			v[1] = tr.viewParms.visBounds[0][1];
		}
		else
		{
			v[1] = tr.viewParms.visBounds[1][1];
		}

		if ( i & 4 )
		{
			v[2] = tr.viewParms.visBounds[0][2];
		}
		else
		{
			v[2] = tr.viewParms.visBounds[1][2];
		}

		VectorSubtract( v, tr.viewParms.ori.origin, vecTo );

		distance = vecTo[0] * vecTo[0] + vecTo[1] * vecTo[1] + vecTo[2] * vecTo[2];

		if ( distance > farthestCornerDistance )
		{
			farthestCornerDistance = distance;
		}
	}
	tr.viewParms.zFar = sqrt( farthestCornerDistance );
}


void R_SetupProjection( void ) {
	float	xmin, xmax, ymin, ymax;
	float	width, height, depth;
	float	zNear, zFar;

	SetFarClip();

	zNear	= r_znear->value;
	zFar	= tr.viewParms.zFar;

	ymax = zNear * tan( tr.refdef.fov_y * M_PI / 360.0f );
	ymin = -ymax;

	xmax = zNear * tan( tr.refdef.fov_x * M_PI / 360.0f );
	xmin = -xmax;

	width = xmax - xmin;
	height = ymax - ymin;
	depth = zFar - zNear;

	tr.viewParms.projectionMatrix[0] = 2 * zNear / width;
	tr.viewParms.projectionMatrix[4] = 0;
	tr.viewParms.projectionMatrix[8] = ( xmax + xmin ) / width;	
	tr.viewParms.projectionMatrix[12] = 0;

	tr.viewParms.projectionMatrix[1] = 0;
	tr.viewParms.projectionMatrix[5] = 2 * zNear / height;
	tr.viewParms.projectionMatrix[9] = ( ymax + ymin ) / height;	
	tr.viewParms.projectionMatrix[13] = 0;

	tr.viewParms.projectionMatrix[2] = 0;
	tr.viewParms.projectionMatrix[6] = 0;
	tr.viewParms.projectionMatrix[10] = -( zFar + zNear ) / depth;
	tr.viewParms.projectionMatrix[14] = -2 * zFar * zNear / depth;

	tr.viewParms.projectionMatrix[3] = 0;
	tr.viewParms.projectionMatrix[7] = 0;
	tr.viewParms.projectionMatrix[11] = -1;
	tr.viewParms.projectionMatrix[15] = 0;
}

void R_SetupFrustum (void) {
	int		i;
	float	xs, xc;
	float	ang;

	ang = tr.viewParms.fovX / 180 * M_PI * 0.5f;
	xs = sin( ang );
	xc = cos( ang );

	VectorScale( tr.viewParms.ori.axis[0], xs, tr.viewParms.frustum[0].normal );
	VectorMA( tr.viewParms.frustum[0].normal, xc, tr.viewParms.ori.axis[1], tr.viewParms.frustum[0].normal );

	VectorScale( tr.viewParms.ori.axis[0], xs, tr.viewParms.frustum[1].normal );
	VectorMA( tr.viewParms.frustum[1].normal, -xc, tr.viewParms.ori.axis[1], tr.viewParms.frustum[1].normal );

	ang = tr.viewParms.fovY / 180 * M_PI * 0.5f;
	xs = sin( ang );
	xc = cos( ang );

	VectorScale( tr.viewParms.ori.axis[0], xs, tr.viewParms.frustum[2].normal );
	VectorMA( tr.viewParms.frustum[2].normal, xc, tr.viewParms.ori.axis[2], tr.viewParms.frustum[2].normal );

	VectorScale( tr.viewParms.ori.axis[0], xs, tr.viewParms.frustum[3].normal );
	VectorMA( tr.viewParms.frustum[3].normal, -xc, tr.viewParms.ori.axis[2], tr.viewParms.frustum[3].normal );

	for (i=0 ; i<4 ; i++) {
		tr.viewParms.frustum[i].type = PLANE_NON_AXIAL;
		tr.viewParms.frustum[i].dist = DotProduct (tr.viewParms.ori.origin, tr.viewParms.frustum[i].normal);
		SetPlaneSignbits( &tr.viewParms.frustum[i] );
	}

	if ( r_farplane->integer )
	{
		tr.viewParms.farplane_distance = r_farplane->value;

		sscanf(
			r_farplane_color->string,
			"%f %f %f",
			&tr.viewParms.farplane_color[0],
			&tr.viewParms.farplane_color[1],
			&tr.viewParms.farplane_color[2]);

			if (r_farplane_nocull->integer > 0) {
				tr.viewParms.farplane_cull = FARPLANE_CULL_NONE;
			} else if (r_farplane_nocull->integer == 0) {
				tr.viewParms.farplane_cull = FARPLANE_CULL_STANDARD;
            } else {
                tr.viewParms.farplane_cull = FARPLANE_CULL_PORTALSKY;
            }
	}

	if (tr.viewParms.farplane_distance)
	{
		vec3_t farPoint;

		tr.viewParms.fog.len = tr.viewParms.farplane_distance;
		tr.viewParms.fog.oolen = 1.0 / tr.viewParms.farplane_distance;
		tr.viewParms.fogColorInt = ColorBytes4( tr.viewParms.farplane_color[0],
												tr.viewParms.farplane_color[1],
												tr.viewParms.farplane_color[2],
												1.0f );
		VectorMA(tr.viewParms.ori.origin, tr.viewParms.farplane_distance, tr.viewParms.ori.axis[0], farPoint);

		VectorNegate(tr.viewParms.ori.axis[0], tr.viewParms.frustum[4].normal);
		tr.viewParms.frustum[4].type = PLANE_NON_AXIAL;
		tr.viewParms.frustum[4].dist = DotProduct(farPoint, tr.viewParms.frustum[4].normal);
		SetPlaneSignbits(&tr.viewParms.frustum[4]);

		tr.viewParms.fog.enabled = r_farplane_nofog->integer == 0 && tr.viewParms.farplane_fog;
		if (tr.viewParms.farplane_cull != FARPLANE_CULL_NONE) {
			tr.viewParms.fog.extrafrustums = 1;
		} else {
			tr.viewParms.fog.extrafrustums = 0;
		}
	}
	else
	{
		tr.viewParms.fog.enabled = 0;
		tr.viewParms.fog.extrafrustums = 0;
	}
}


void R_MirrorPoint (vec3_t in, orientation_t *surface, orientation_t *camera, vec3_t out) {
	int		i;
	vec3_t	local;
	vec3_t	transformed;
	float	d;

	VectorSubtract( in, surface->origin, local );

	VectorClear( transformed );
	for ( i = 0 ; i < 3 ; i++ ) {
		d = DotProduct(local, surface->axis[i]);
		VectorMA( transformed, d, camera->axis[i], transformed );
	}

	VectorAdd( transformed, camera->origin, out );
}

void R_MirrorVector (vec3_t in, orientation_t *surface, orientation_t *camera, vec3_t out) {
	int		i;
	float	d;

	VectorClear( out );
	for ( i = 0 ; i < 3 ; i++ ) {
		d = DotProduct(in, surface->axis[i]);
		VectorMA( out, d, camera->axis[i], out );
	}
}


void R_PlaneForSurface (surfaceType_t *surfType, cplane_t *plane) {
	srfTriangles_t	*tri;
	srfPoly_t		*poly;
	drawVert_t		*v1, *v2, *v3;
	vec4_t			plane4;

	if (!surfType) {
		Com_Memset (plane, 0, sizeof(*plane));
		plane->normal[0] = 1;
		return;
	}
	switch (*surfType) {
	case SF_FACE:
		*plane = ((srfSurfaceFace_t *)surfType)->plane;
		return;
	case SF_TRIANGLES:
		tri = (srfTriangles_t *)surfType;
		v1 = tri->verts + tri->indexes[0];
		v2 = tri->verts + tri->indexes[1];
		v3 = tri->verts + tri->indexes[2];
		PlaneFromPoints( plane4, v1->xyz, v2->xyz, v3->xyz );
		VectorCopy( plane4, plane->normal ); 
		plane->dist = plane4[3];
		return;
	case SF_POLY:
		poly = (srfPoly_t *)surfType;
		PlaneFromPoints( plane4, poly->verts[0].xyz, poly->verts[1].xyz, poly->verts[2].xyz );
		VectorCopy( plane4, plane->normal ); 
		plane->dist = plane4[3];
		return;
	default:
		Com_Memset (plane, 0, sizeof(*plane));
		plane->normal[0] = 1;		
		return;
	}
}

qboolean R_GetPortalOrientations( drawSurf_t *drawSurf, int entityNum, 
							 orientation_t *surface, orientation_t *camera,
							 vec3_t pvsOrigin, qboolean *mirror ) {
	int			i;
	cplane_t	originalPlane, plane;
	trRefEntity_t	*e;
	float		d;
	vec3_t		axis1, axis2;

	R_PlaneForSurface( drawSurf->surface, &originalPlane );

	VectorCopy( originalPlane.normal, surface->axis[0] );
	PerpendicularVector( surface->axis[1], surface->axis[0] );
	CrossProduct( surface->axis[0], surface->axis[1], surface->axis[2] );

	if ( entityNum != ENTITYNUM_WORLD ) {
		tr.currentEntityNum = entityNum;
		tr.currentEntity = &tr.refdef.entities[entityNum];

		R_RotateForEntity( tr.currentEntity, &tr.viewParms, &tr.ori );

		VectorCopy( surface->axis[1], axis1 );
		VectorCopy( surface->axis[2], axis2 );
		R_LocalNormalToWorld( axis1, surface->axis[1] );
		R_LocalNormalToWorld( axis2, surface->axis[2] );

		R_LocalNormalToWorld( originalPlane.normal, plane.normal );
		VectorCopy( plane.normal, surface->axis[0] );
		plane.dist = originalPlane.dist + DotProduct( plane.normal, tr.ori.origin );

		originalPlane.dist = originalPlane.dist + DotProduct( originalPlane.normal, tr.ori.origin );
	} else {
		plane = originalPlane;
	}

	for ( i = 0 ; i < tr.refdef.num_entities ; i++ ) {
		e = &tr.refdef.entities[i];
		if ( e->e.reType != RT_PORTALSURFACE ) {
			continue;
		}

		d = DotProduct( e->e.origin, originalPlane.normal ) - originalPlane.dist;
		if ( d > 64 || d < -64) {
			continue;
		}

		VectorCopy( e->e.attachOrigin, pvsOrigin );

		if ( e->e.attachOrigin[0] == e->e.origin[0] &&
			e->e.attachOrigin[1] == e->e.origin[1] &&
			e->e.attachOrigin[2] == e->e.origin[2] ) {
			VectorScale( plane.normal, plane.dist, surface->origin );
			VectorCopy( surface->origin, camera->origin );
			VectorSubtract( vec3_origin, surface->axis[0], camera->axis[0] );
			VectorCopy( surface->axis[1], camera->axis[1] );
			VectorCopy( surface->axis[2], camera->axis[2] );

			*mirror = qtrue;
			return qtrue;
		}

		d = DotProduct( e->e.origin, plane.normal ) - plane.dist;
		VectorMA( e->e.origin, -d, surface->axis[0], surface->origin );
			
		VectorCopy( e->e.attachOrigin, camera->origin );
		AxisCopy( e->e.axis, camera->axis );
		VectorSubtract( vec3_origin, camera->axis[0], camera->axis[0] );
		VectorSubtract( vec3_origin, camera->axis[1], camera->axis[1] );

		// not exist in the retail function: after the two axis negations it
		*mirror = qfalse;
		return qtrue;
	}




	return qfalse;
}

static qboolean Q_UNUSED_VAR IsMirror( const drawSurf_t *drawSurf, int entityNum )
{
	int			i;
	cplane_t	originalPlane, plane;
	trRefEntity_t	*e;
	float		d;

	R_PlaneForSurface( drawSurf->surface, &originalPlane );

	if ( entityNum != ENTITYNUM_WORLD ) 
	{
		tr.currentEntityNum = entityNum;
		tr.currentEntity = &tr.refdef.entities[entityNum];

		R_RotateForEntity( tr.currentEntity, &tr.viewParms, &tr.ori );

		R_LocalNormalToWorld( originalPlane.normal, plane.normal );
		plane.dist = originalPlane.dist + DotProduct( plane.normal, tr.ori.origin );

		originalPlane.dist = originalPlane.dist + DotProduct( originalPlane.normal, tr.ori.origin );
	} 
	else 
	{
		plane = originalPlane;
	}

	for ( i = 0 ; i < tr.refdef.num_entities ; i++ ) 
	{
		e = &tr.refdef.entities[i];
		if ( e->e.reType != RT_PORTALSURFACE ) {
			continue;
		}

		d = DotProduct( e->e.origin, originalPlane.normal ) - originalPlane.dist;
		if ( d > 64 || d < -64) {
			continue;
		}

		if ( e->e.attachOrigin[0] == e->e.origin[0] &&
			e->e.attachOrigin[1] == e->e.origin[1] &&
			e->e.attachOrigin[2] == e->e.origin[2] )
		{
			return qtrue;
		}

		return qfalse;
	}
	return qfalse;
}

qboolean SurfIsOffscreen(const srfSurfaceFace_t* surface, shader_t* shader, int entityNum) {
	float shortest = 100000000;
	int numTriangles;
	orientationr_t surfOr;
	vec4_t clip, eye;
	int i;
	unsigned int pointAnd = (unsigned int)~0;

	if (surface->surfaceType != SF_FACE) {
		ri.Printf(PRINT_WARNING, "WARNING: SurfIsOffscreen called on non-bmodel!\n");
		return qfalse;
	}
	
	if ( vkConfig.smpActive ) {		
		return qfalse;
	}

	if (entityNum == ENTITYNUM_WORLD) {
		surfOr = tr.viewParms.world;
	} else {
		R_RotateForEntity(&tr.refdef.entities[entityNum], &tr.viewParms, &surfOr);
	}

	for ( i = 0; i < surface->numPoints; i++ )
	{
		int j;
		unsigned int pointFlags = 0;

		R_TransformModelToClip( surface->points[i], surfOr.modelMatrix, tr.viewParms.projectionMatrix, eye, clip );

		for ( j = 0; j < 3; j++ )
		{
			if ( clip[j] >= clip[3] )
			{
				pointFlags |= (1 << (j*2));
			}
			else if ( clip[j] <= -clip[3] )
			{
				pointFlags |= ( 1 << (j*2+1));
			}
		}
		pointAnd &= pointFlags;
	}

	if ( pointAnd )
	{
		return qtrue;
	}

	numTriangles = surface->numIndices / 3;

	for ( i = 0; i < surface->numIndices; i += 3 )
	{
		vec3_t normal;
		float dot;
		float len;
		unsigned* indices;

		indices = (unsigned*)(((char*)surface) + surface->ofsIndices);

		VectorSubtract( surface->points[indices[i]], surfOr.viewOrigin, normal);

		if (shader->fDistRange > 0) {
			len = VectorLengthSquared(normal);			
			if (len < shortest)
			{
				shortest = len;
			}
		}

		if ( ( dot = DotProduct( normal, surface->plane.normal ) ) >= 0 )
		{
			numTriangles--;
		}
	}
	if ( !numTriangles )
	{
		return qtrue;
	}

	if (shader->fDistRange > 0.0)
	{
		if (shortest > Square(shader->fDistRange))
		{
			return qtrue;
		}
	}

	return qfalse;
}

static qboolean DrawSurfIsOffscreen(drawSurf_t* drawSurf) {
	int entityNum;
	shader_t* shader;
	int fogNum;
	int dlighted;

	R_DecomposeSort(drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted);
	return SurfIsOffscreen((srfSurfaceFace_t*)drawSurf->surface, shader, entityNum);
}

qboolean R_MirrorViewBySurface (drawSurf_t *drawSurf, int entityNum) {
	viewParms_t		newParms;
	viewParms_t		oldParms;
	orientation_t	surface, camera;

	if (tr.viewParms.isPortal) {
		ri.Printf( PRINT_DEVELOPER, "WARNING: recursive mirror/portal found\n" );
		return qfalse;
	}

	if ( r_noportals->integer || (r_fastsky->integer == 1) ) {
		return qfalse;
	}

	if (DrawSurfIsOffscreen( drawSurf ) ) {
		return qfalse;
	}

	oldParms = tr.viewParms;

	newParms = tr.viewParms;
	newParms.isPortal = qtrue;
	if ( !R_GetPortalOrientations( drawSurf, entityNum, &surface, &camera, 
		newParms.pvsOrigin, &newParms.isMirror ) ) {
		return qfalse;		
	}

	R_MirrorPoint (oldParms.ori.origin, &surface, &camera, newParms.ori.origin );

	VectorSubtract( vec3_origin, camera.axis[0], newParms.portalPlane.normal );
	newParms.portalPlane.dist = DotProduct( camera.origin, newParms.portalPlane.normal );
	
	R_MirrorVector (oldParms.ori.axis[0], &surface, &camera, newParms.ori.axis[0]);
	R_MirrorVector (oldParms.ori.axis[1], &surface, &camera, newParms.ori.axis[1]);
	R_MirrorVector (oldParms.ori.axis[2], &surface, &camera, newParms.ori.axis[2]);


	tr.portalRendered = qtrue;

	R_RenderView (&newParms);

	tr.viewParms = oldParms;

	return qtrue;
}


static ID_INLINE void R_Radix( int offset, int size, drawSurf_t *source, drawSurf_t *dest )
{
  int           count[ 256 ] = { 0 };
  int           index[ 256 ];
  int           i;
  unsigned char *sortKey = NULL;
  unsigned char *end = NULL;

  sortKey = ( (unsigned char *)&source[ 0 ].sort ) + offset;
  end = sortKey + ( size * sizeof( drawSurf_t ) );
  for( ; sortKey < end; sortKey += sizeof( drawSurf_t ) )
    ++count[ *sortKey ];

  index[ 0 ] = 0;

  for( i = 1; i < 256; ++i )
    index[ i ] = index[ i - 1 ] + count[ i - 1 ];

  sortKey = ( (unsigned char *)&source[ 0 ].sort ) + offset;
  for( i = 0; i < size; ++i, sortKey += sizeof( drawSurf_t ) )
    dest[ index[ *sortKey ]++ ] = source[ i ];
}

static void R_RadixSort( drawSurf_t *source, int size )
{
  static drawSurf_t scratch[ MAX_DRAWSURFS ];
#ifdef Q3_LITTLE_ENDIAN
  R_Radix( 0, size, source, scratch );
  R_Radix( 1, size, scratch, source );
  R_Radix( 2, size, source, scratch );
  R_Radix( 3, size, scratch, source );
#else
  R_Radix( 3, size, source, scratch );
  R_Radix( 2, size, scratch, source );
  R_Radix( 1, size, source, scratch );
  R_Radix( 0, size, scratch, source );
#endif 
}


void R_AddDrawSurf(surfaceType_t* surface, shader_t* shader, int fogNum, int dlightMap) {
	int			index;

	if ( !r_detailSurfaces->integer && shader->detailShader ) {
		return;
	}

	index = tr.refdef.numDrawSurfs & DRAWSURF_MASK;
	tr.refdef.drawSurfs[index].sort = (shader->sortedIndex << QSORT_SHADERNUM_SHIFT)
		| tr.shiftedEntityNum
		| (fogNum << QSORT_FOGNUM_SHIFT)
		| (dlightMap & QSORT_DLIGHTMAP_MASK);
	tr.refdef.drawSurfs[index].surface = surface;
	tr.refdef.numDrawSurfs++;
}

void R_AddSpriteSurf(surfaceType_t* surface, shader_t* shader, float zDistance)
{
	int index;

	if (zDistance > MAX_SPRITE_DIST_SQUARED) {
		zDistance = MAX_SPRITE_DIST_SQUARED;
	}

	index = tr.refdef.numSpriteSurfs % MAX_SPRITES;
    tr.refdef.spriteSurfs[index].sort = (int)(MAX_SPRITE_DIST_SQUARED - zDistance) | (shader->sortedIndex << QSORT_SHADERNUM_SHIFT);
    tr.refdef.spriteSurfs[index].surface = surface;
    tr.refdef.numSpriteSurfs++;
}

void R_DecomposeSort(unsigned int sort, int* entityNum, shader_t** shader, int* fogNum, int* dlightMap) {
	int index = ( sort >> QSORT_SHADERNUM_SHIFT ) & (MAX_SHADERS-1);

	/* The array is only filled to tr.numShaders, so a key carrying a shader
	   number past it dereferences null two lines later at shader->sort -- the
	   in-map fault at R_SortDrawSurfs+0x188, address 0x10c.  Naming the key and
	   the filled count turns that into a diagnosis instead of a crash. */
	*shader = tr.sortedShaders[ index ];
	if ( *shader == NULL ) {
		static int nullShaderRows;

		if ( nullShaderRows < 8 || ( nullShaderRows % 500 ) == 0 ) {
			VK_Guard_Hit( VK_GSITE_QUIRK, va( "draw surf names shader %d of %d registered",
											  index, tr.numShaders ), VK_INCOMPLETE );
		}
		nullShaderRows++;
	}
	*entityNum = ( sort >> QSORT_ENTITYNUM_SHIFT ) & QSORT_ENTITYNUM_MASK;
	*fogNum = ( sort & 0xFF ) >> QSORT_FOGNUM_SHIFT;
	*dlightMap = sort & QSORT_DLIGHTMAP_MASK;
}

void R_SortDrawSurfs( drawSurf_t *drawSurfs, int numDrawSurfs, drawSurf_t *spriteSurfs, int numSpriteSurfs ) {
	shader_t		*shader;
	int				entityNum;
	int				fogNum;
	int				dlighted;
	int				i;

	if ( numDrawSurfs < 1 ) {
		R_AddDrawSurfCmd( drawSurfs, numDrawSurfs );
		return;
	}

    if (numDrawSurfs > MAX_DRAWSURFS) {
        numDrawSurfs = MAX_DRAWSURFS;
    }
    if (numSpriteSurfs > MAX_SPRITESURFS) {
		numSpriteSurfs = MAX_SPRITESURFS;
    }

	R_RadixSort (drawSurfs, numDrawSurfs );
	R_RadixSort (spriteSurfs, numSpriteSurfs );

	R_Sky_Render();

	if (!tr.viewParms.isPortal)
	{
		for (i = 0; i < numDrawSurfs; i++) {
			R_DecomposeSort((drawSurfs + i)->sort, &entityNum, &shader, &fogNum, &dlighted);

			if ( shader == NULL ) {
				continue;
			}

			if (shader->sort > SS_PORTAL) {
				break;
			}

			if (shader->sort == SS_BAD) {
				ri.Error(ERR_DROP, "Shader '%s'with sort == SS_BAD", shader->name);
			}

			if (R_MirrorViewBySurface((drawSurfs + i), entityNum)) {
				if (r_portalOnly->integer) {
					return;
				}
				break;		
			}
		}
	}

    R_AddDrawSurfCmd(drawSurfs, numDrawSurfs);
    R_AddSpriteSurfCmd(spriteSurfs, numSpriteSurfs);
}

int R_SpriteFogNum(const vec3_t origin, float radius) {
	int		i;
	fog_t	*fog;

	if (tr.refdef.rdflags & RDF_NOWORLDMODEL) {
		return 0;
	}
	if (tr.viewParms.isPortalSky) {
		return 0;
	}
	if (tr.world->numfogs <= 1) {
		return 0;
	}

	for (i = 1; i < tr.world->numfogs; i++) {
		fog = &tr.world->fogs[i];
		if ((origin[0] - radius) < fog->bounds[1][0] && (origin[0] + radius) > fog->bounds[0][0]
			&& (origin[1] - radius) < fog->bounds[1][1] && (origin[1] + radius) > fog->bounds[0][1]
			&& (origin[2] - radius) < fog->bounds[1][2] && (origin[2] + radius) > fog->bounds[0][2]) {
			return i;
		}
	}
	return 0;
}

void R_AddEntitySurfaces (void) {
	trRefEntity_t	*ent;
	shader_t		*shader;

	if ( !r_drawentities->integer ) {
		return;
	}

	for ( tr.currentEntityNum = 0; 
	      tr.currentEntityNum < tr.refdef.num_entities; 
		  tr.currentEntityNum++ ) {
		ent = tr.currentEntity = &tr.refdef.entities[tr.currentEntityNum];

		ent->needDlights = qfalse;

		tr.shiftedEntityNum = tr.currentEntityNum << QSORT_ENTITYNUM_SHIFT;

		if ( (ent->e.renderfx & RF_FIRST_PERSON) && tr.viewParms.isPortal) {
			continue;
		}

		if ( (ent->e.renderfx & RF_WEAPONMODEL) && (ent->e.renderfx & RF_DEPTHHACK) ) {
			if ( tr.viewParms.isPortal ) {
				continue;
			}
			if ( !r_drawviewmodel->integer ) {
				continue;
			}
		}

		if (tr.viewParms.isPortalSky) {
			if (!(ent->e.renderfx & RF_SKYENTITY)) {
				continue;
			}
		} else {
			if (ent->e.renderfx & RF_SKYENTITY) {
				continue;
			}
		}

		if (tr.viewParms.isPortal) {
			if (!(ent->e.renderfx & (RF_PORTALENTITY | RF_DUALENTITY))) {
				continue;
			}
		} else {
			if (ent->e.renderfx & RF_PORTALENTITY) {
				continue;
			}
		}

		switch ( ent->e.reType ) {
		case RT_PORTALSURFACE:
			break;		
		case RT_SPRITE:
		case RT_BEAM:
			if ( (ent->e.renderfx & RF_THIRD_PERSON) && !tr.viewParms.isPortal) {
				continue;
			}
			shader = R_GetShaderByHandle( ent->e.customShader );
			R_AddDrawSurf( &entitySurface, shader, R_SpriteFogNum( ent->e.origin, ent->e.radius ), 0 );
			break;

		case RT_MODEL:
			R_RotateForEntity( ent, &tr.viewParms, &tr.ori );

			tr.currentModel = R_GetModelByHandle( ent->e.hModel );
			if (!tr.currentModel) {
				SMP_ShaderDefaultProbe( "entity-nomodel", "-", "R_AddEntitySurfaces/null-model",
										 ent->e.entityNumber, ent->e.hModel );
				R_AddDrawSurf( &entitySurface, tr.defaultShader, 0, 0 );
			} else {
				switch ( tr.currentModel->type ) {
				case MOD_SPRITE:
					ri.Printf(PRINT_ALL, "sprite model '%s' being added to renderer!\n", tr.currentModel->name);
					break;
				case MOD_TIKI:
					// Retail @0x26950 is the MOD_TIKI entry point, not R_AddSkelSurfaces:
					R_AddTikiSurfaces( ent );
					break;
				case MOD_BRUSH:
					R_AddBrushModelSurfaces( ent );
					break;
				case MOD_BAD:		
					if ( (ent->e.renderfx & RF_THIRD_PERSON) && !tr.viewParms.isPortal) {
						break;
					}
					shader = R_GetShaderByHandle( ent->e.customShader );
					SMP_ShaderDefaultProbe( "entity-nomodel", tr.currentModel->name, "R_AddEntitySurfaces/MOD_BAD",
											 ent->e.entityNumber, ent->e.hModel );
					R_AddDrawSurf( &entitySurface, tr.defaultShader, 0, 0 );
					break;
				default:
					ri.Error( ERR_DROP, "R_AddEntitySurfaces: Bad modeltype" );
					break;
				}
			}
			break;
		default:
			ri.Error( ERR_DROP, "R_AddEntitySurfaces: Bad reType" );
		}
	}
}

void R_AddSpriteSurfaces(void)
{
	refSprite_t* sprite;
	vec3_t delta;

	if (!r_drawsprites->integer) {
		return;
	}

	for (tr.currentSpriteNum = 0; tr.currentSpriteNum < tr.refdef.num_sprites; ++tr.currentSpriteNum)
	{
		sprite = &tr.refdef.sprites[tr.currentSpriteNum];

		if ((sprite->renderfx & RF_THIRD_PERSON) && !tr.viewParms.isPortal) {
			continue;
		}

        if (tr.viewParms.isPortalSky) {
			if (!(sprite->renderfx & RF_SKYENTITY)) {
				continue;
			}
        } else {
			if (sprite->renderfx & RF_SKYENTITY) {
				continue;
			}
		}

		if (tr.viewParms.isPortal) {
			if (!(sprite->renderfx & (RF_PORTALENTITY | RF_DUALENTITY))) {
				continue;
			}
		} else {
			if (sprite->renderfx & RF_PORTALENTITY) {
				continue;
			}
		}

		tr.currentEntityNum = ENTITYNUM_WORLD;
        tr.shiftedEntityNum = tr.currentEntityNum << QSORT_ENTITYNUM_SHIFT;
		tr.currentModel = R_GetModelByHandle(sprite->hModel);

		if (!tr.currentModel || !tr.currentModel->d.sprite) {
			continue;
		}

		sprite->shaderNum = tr.currentModel->d.sprite->shader->sortedIndex;

		sprite->fogNum = R_SpriteFogNum(
			sprite->origin,
			sprite->scale * (tr.currentModel->d.sprite->width * tr.currentModel->d.sprite->scale));

		VectorSubtract(sprite->origin, tr.refdef.vieworg, delta);
		R_AddSpriteSurf(&sprite->surftype, tr.currentModel->d.sprite->shader, VectorLengthSquared(delta));
	}
}

void R_GenerateDrawSurfs( void ) {
	BE_ZONE( BE_FAM_FE, BE_FEZ_world, R_AddWorldSurfaces() );
	if (!(tr.refdef.rdflags & RDF_NOWORLDMODEL)) {
		BE_ZONE( BE_FAM_FE, BE_FEZ_poly, R_AddSwipeSurfaces() );
	}

	BE_ZONE( BE_FAM_FE, BE_FEZ_poly, R_AddPolygonSurfaces() );


	BE_ZONE( BE_FAM_FE, BE_FEZ_ents, R_AddEntitySurfaces() );

	BE_ZONE( BE_FAM_FE, BE_FEZ_sprite, R_AddSpriteSurfaces() );

	BE_ZONE( BE_FAM_FE, BE_FEZ_view, R_SetupProjection() );
}

void R_DebugPolygon( int color, int numPoints, float *points ) {
	int		i;

	VK_State( VKS_DEPTHMASK_TRUE | VKS_SRCBLEND_ONE | VKS_DSTBLEND_ONE );

	VK_BeginWire();


	RB_ImmColor3f( color&1, (color>>1)&1, (color>>2)&1 );
	RB_ImmBegin( VK_TRIANGLE_FAN );
	for ( i = 0 ; i < numPoints ; i++ ) {
		RB_ImmVertex3fv( points + i * 3 );
	}
	RB_ImmEnd();

	VK_State( VKS_POLYMODE_LINE | VKS_DEPTHMASK_TRUE | VKS_SRCBLEND_ONE | VKS_DSTBLEND_ONE );
	vkDepthRangef( 0, 0 );
	RB_ImmColor3f( 1, 1, 1 );
	RB_ImmBegin( VK_LINE_LOOP );
	for ( i = 0 ; i < numPoints ; i++ ) {
		RB_ImmVertex3fv( points + i * 3 );
	}
	RB_ImmEnd();
	vkDepthRangef( 0, 1 );
	RB_ImmFlush();
}

void R_DebugGraphics( void ) {
	if ( !r_debugSurface->integer ) {
		return;
	}

	R_FlushRenderCommands();

	VK_BindUnit0( tr.whiteImage);
	VK_Cull( CT_FRONT_SIDED );
	ri.CM_DrawDebugSurface( R_DebugPolygon );
}

#define CIRCLE_LENGTH		25

void R_DebugCircle(const vec3_t org, float radius, float r, float g, float b, float alpha, qboolean horizontal) {
    int				i;
    float			ang;
	debugline_t* line;
	vec3_t			forward, right;
	vec3_t			pos, lastpos;

	if (!ri.DebugLines || !ri.numDebugLines) {
		return;
	}

	if (horizontal)
	{
		VectorSet(forward, 1, 0, 0);
		VectorSet(right, 0, 1, 0);
	}
	else
	{
		VectorCopy(tr.refdef.viewaxis[1], right);
		VectorCopy(tr.refdef.viewaxis[2], forward);
	}

	VectorClear(pos);
	VectorClear(lastpos);

	for (i = 0; i < CIRCLE_LENGTH; i++) {
		VectorCopy(pos, lastpos);

		ang = DEG2RAD((float)(i * 15));
		pos[0] = (org[0] + sin(ang) * radius * forward[0]) +
			cos(ang) * radius * right[0];
		pos[1] = (org[1] + sin(ang) * radius * forward[1]) +
			cos(ang) * radius * right[1];
		pos[2] = (org[2] + sin(ang) * radius * forward[2]) +
			cos(ang) * radius * right[2];

		if (i > 0)
		{
			if (*ri.numDebugLines >= r_numdebuglines->integer) {
				ri.Printf(PRINT_ALL, "R_DebugCircle: Exceeded MAX_DEBUG_LINES\n");
				return;
			}

			line = &(*ri.DebugLines)[*ri.numDebugLines];
			(*ri.numDebugLines)++;
			VectorCopy(lastpos, line->start);
			VectorCopy(pos, line->end);
			VectorSet(line->color, r, g, b);
			line->alpha = alpha;
			line->width = 1.0;
			line->factor = 1;
			line->pattern = -1;
		}
	}
}

void R_DebugLine(const vec3_t start, const vec3_t end, float r, float g, float b, float alpha) {
	debugline_t* line;

	if (!ri.DebugLines || !ri.numDebugLines) {
		return;
	}

	if (*ri.numDebugLines >= r_numdebuglines->integer) {
		ri.Printf(PRINT_ALL, "R_DebugLine: Exceeded MAX_DEBUG_LINES\n");
		return;
	}

	line = &(*ri.DebugLines)[*ri.numDebugLines];
	(*ri.numDebugLines)++;
	VectorCopy(start, line->start);
	VectorCopy(end, line->end);
	VectorSet(line->color, r, g, b);
	line->alpha = alpha;
	line->width = 1.0;
	line->factor = 1;
	line->pattern = -1;
}

static void R_DrawStippledLine( const vec3_t start, const vec3_t end, qboolean stipple,
							   int stippleFactor, unsigned short pattern, float lineWidth ) {
	vec4_t		eye0, clip0, eye1, clip1;
	float		dx, dy, pixelLen;
	float		covered, t0, pxPerBit;
	int			bit;

	if ( stipple ) {
		vkLineWidth( lineWidth );
	}

	if ( !stipple || pattern == 0xffff ) {
		RB_ImmBegin( VK_LINES );
		RB_ImmVertex3fv( start );
		RB_ImmVertex3fv( end );
		RB_ImmEnd();
		goto flush;
	}

	if ( stippleFactor < 1 ) {
		stippleFactor = 1;
	}

	R_TransformModelToClip( start, backEnd.ori.modelMatrix, backEnd.viewParms.projectionMatrix, eye0, clip0 );
	R_TransformModelToClip( end,   backEnd.ori.modelMatrix, backEnd.viewParms.projectionMatrix, eye1, clip1 );

	if ( clip0[3] <= 0.0f || clip1[3] <= 0.0f || backEnd.viewParms.viewportWidth <= 0 ) {
		RB_ImmBegin( VK_LINES );
		RB_ImmVertex3fv( start );
		RB_ImmVertex3fv( end );
		RB_ImmEnd();
		goto flush;
	}

	dx = ( clip0[0] / clip0[3] - clip1[0] / clip1[3] ) * 0.5f * backEnd.viewParms.viewportWidth;
	dy = ( clip0[1] / clip0[3] - clip1[1] / clip1[3] ) * 0.5f * backEnd.viewParms.viewportHeight;
	pixelLen = ( float )sqrt( ( double )( dx * dx + dy * dy ) );

	if ( pixelLen <= 0.0f ) {
		goto flush;
	}

	pxPerBit = ( float )stippleFactor;
	covered = 0.0f;
	t0 = 0.0f;
	bit = 0;
	while ( covered < pixelLen ) {
		float	seg = pxPerBit;
		float	t1;

		if ( covered + seg > pixelLen ) {
			seg = pixelLen - covered;
		}
		t1 = ( covered + seg ) / pixelLen;

		if ( ( pattern >> bit ) & 1 ) {
			vec3_t	a, b;
			int		k;

			for ( k = 0; k < 3; k++ ) {
				a[k] = start[k] + ( end[k] - start[k] ) * t0;
				b[k] = start[k] + ( end[k] - start[k] ) * t1;
			}
			RB_ImmBegin( VK_LINES );
			RB_ImmVertex3fv( a );
			RB_ImmVertex3fv( b );
			RB_ImmEnd();
		}

		covered += seg;
		t0 = t1;
		bit = ( bit + 1 ) & 15;
	}

flush:
	RB_ImmFlush();
}

void R_DrawDebugLines(void) {
	debugline_t* line;
	int i;
	float width;
	int factor;
	unsigned short pattern;

	if (!ri.DebugLines || !ri.numDebugLines) {
		return;
	}

	if (!*ri.numDebugLines) {
		return;
	}

	if (tr.refdef.rdflags & RDF_NOWORLDMODEL) {
		return;
	}

	if (tr.viewParms.isPortalSky) {
		return;
	}

	R_FlushRenderCommands();
    VK_BindUnit0(tr.whiteImage);
	if (r_debuglines_depthmask->integer) {
		VK_State(VKS_POLYMODE_LINE | VKS_DEPTHMASK_TRUE);
	} else {
		VK_State(VKS_POLYMODE_LINE);
	}

	VK_BeginWire();


    width = 1.0;
    factor = 4;
    pattern = -1;

	(void)width; (void)factor; (void)pattern;

	for (i = 0; i < *ri.numDebugLines; i++) {
		line = &(*ri.DebugLines)[i];

		RB_ImmColor4f(line->color[0], line->color[1], line->color[2], line->alpha);
		R_DrawStippledLine(line->start, line->end, r_stipplelines->integer != 0,
						   line->factor, (unsigned short)line->pattern, line->width);
	}

	RB_ImmFlush();

	vkLineWidth(1.0);

    vkDepthRangef(0.0, 1.0);
}

void R_DrawDebugStrings(void) {
	int i;
	debugstring_t* string;

	if (!ri.DebugStrings || !ri.numDebugStrings) {
		return;
	}

	if (!*ri.numDebugStrings) {
		return;
	}

    if (tr.refdef.rdflags & RDF_NOWORLDMODEL) {
        return;
    }

    if (tr.viewParms.isPortalSky) {
        return;
    }

	for (i = 0; i < *ri.numDebugStrings; i++) {
		string = &(*ri.DebugStrings)[i];

		R_DrawFloatingString(
			tr.pFontDebugStrings,
			string->szText,
			string->pos,
			string->color,
			string->scale,
			64
		);
	}
}

void R_RenderView (viewParms_t *parms) {
	int		firstDrawSurf;
	int		firstSpriteSurf;

	if ( parms->viewportWidth <= 0 || parms->viewportHeight <= 0 ) {
		return;
	}

	tr.viewCount++;

	tr.viewParms = *parms;
	tr.viewParms.frameSceneNum = tr.frameSceneNum;
	tr.viewParms.frameCount = tr.frameCount;

	firstDrawSurf = tr.refdef.numDrawSurfs;
	firstSpriteSurf = tr.refdef.numSpriteSurfs;

	tr.viewCount++;

	BE_Bump( BE_FAM_FE, BE_FEC_views, 1 );
	BE_Bump( BE_FAM_FE, BE_FEC_portals, parms->isPortal ? 1 : 0 );

	BE_ZONE( BE_FAM_FE, BE_FEZ_view, R_RotateForViewer() );

	BE_ZONE( BE_FAM_FE, BE_FEZ_view, R_SetupFrustum() );

	BE_ZONE( BE_FAM_FE, BE_FEZ_view, R_Sky_Reset() );

	BE_ZONE( BE_FAM_FE, BE_FEZ_other, R_DrawDebugStrings() );

	R_GenerateDrawSurfs();

	BE_Bump( BE_FAM_FE, BE_FEC_surfs, tr.refdef.numDrawSurfs - firstDrawSurf );
	BE_Bump( BE_FAM_FE, BE_FEC_ents, tr.refdef.num_entities );

    BE_ZONE( BE_FAM_FE, BE_FEZ_sort, R_SortDrawSurfs( tr.refdef.drawSurfs + firstDrawSurf,
		tr.refdef.numDrawSurfs - firstDrawSurf, tr.refdef.spriteSurfs + firstSpriteSurf,
		tr.refdef.numSpriteSurfs - firstSpriteSurf ) );

	BE_ZONE( BE_FAM_FE, BE_FEZ_exec, R_IssuePendingRenderCommands() );

	BE_ZONE( BE_FAM_FE, BE_FEZ_other, R_DrawDebugLines() );

	BE_ZONE( BE_FAM_FE, BE_FEZ_other, R_DebugGraphics() );
}



