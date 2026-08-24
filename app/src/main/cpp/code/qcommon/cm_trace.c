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



sphere_t sphere;


void RotatePoint(vec3_t point,  vec3_t matrix[3]) { 
	vec3_t tvec;

	VectorCopy(point, tvec);
	point[0] = DotProduct(matrix[0], tvec);
	point[1] = DotProduct(matrix[1], tvec);
	point[2] = DotProduct(matrix[2], tvec);
}

void CreateRotationMatrix(const vec3_t angles, vec3_t matrix[3]) {
	AngleVectorsLeft(angles, matrix[0], matrix[1], matrix[2]);
	VectorInverse(matrix[1]);
}

void CM_ProjectPointOntoVector( vec3_t point, vec3_t vStart, vec3_t vDir, vec3_t vProj )
{
	vec3_t pVec;

	VectorSubtract( point, vStart, pVec );
	VectorMA( vStart, DotProduct( pVec, vDir ), vDir, vProj );
}

float CM_DistanceFromLineSquared(vec3_t p, vec3_t lp1, vec3_t lp2, vec3_t dir) {
	vec3_t proj, t;
	int j;

	CM_ProjectPointOntoVector(p, lp1, dir, proj);
	for (j = 0; j < 3; j++)
		if ((proj[j] > lp1[j] && proj[j] > lp2[j]) ||
			(proj[j] < lp1[j] && proj[j] < lp2[j]))
			break;
	if (j < 3) {
		if (fabs(proj[j] - lp1[j]) < fabs(proj[j] - lp2[j]))
			VectorSubtract(p, lp1, t);
		else
			VectorSubtract(p, lp2, t);
		return VectorLengthSquared(t);
	}
	VectorSubtract(p, proj, t);
	return VectorLengthSquared(t);
}

float CM_VectorDistanceSquared(vec3_t p1, vec3_t p2) {
	vec3_t dir;

	VectorSubtract(p2, p1, dir);
	return VectorLengthSquared(dir);
}

float SquareRootFloat(float number) {
	union {
		float f;
		int i;
	} t;
	float x, y;
	const float f = 1.5F;

	x = number * 0.5F;
	t.f  = number;
	t.i  = 0x5f3759df - ( t.i >> 1 );
	y  = t.f;
	y  = y * ( f - ( x * y * y ) );
	y  = y * ( f - ( x * y * y ) );
	return number * y;
}



void CM_TestBoxInBrush( traceWork_t *tw, cbrush_t *brush ) {
	int			i;
	cplane_t	*plane;
	float		dist;
	float		d1;
	cbrushside_t	*side;
	float		t;

	if (!brush->numsides) {
		return;
	}

	if ( tw->bounds[0][0] > brush->bounds[1][0]
		|| tw->bounds[0][1] > brush->bounds[1][1]
		|| tw->bounds[0][2] > brush->bounds[1][2]
		|| tw->bounds[1][0] < brush->bounds[0][0]
		|| tw->bounds[1][1] < brush->bounds[0][1]
		|| tw->bounds[1][2] < brush->bounds[0][2]
		) {
		return;
	}

   if ( sphere.use ) {
		for ( i = 6 ; i < brush->numsides ; i++ ) {
			side = brush->sides + i;
			plane = side->plane;

			t = DotProduct( plane->normal, sphere.offset );
			if( t < 0 )
			{
				t = -t;
			}

			dist = t + plane->dist + sphere.radius;

			d1 = DotProduct( tw->start, plane->normal ) - dist;
			if ( d1 > 0 ) {
				return;
			}
		}
	} else {
		for ( i = 6 ; i < brush->numsides ; i++ ) {
			side = brush->sides + i;
			plane = side->plane;

			dist = plane->dist - DotProduct( tw->offsets[ plane->signbits ], plane->normal );

			d1 = DotProduct( tw->start, plane->normal ) - dist;

			if ( d1 > 0 ) {
				return;
			}
		}
	}

	tw->trace.startsolid = tw->trace.allsolid = qtrue;
	tw->trace.fraction = 0;
	tw->trace.contents = brush->contents;
}



void CM_TestInLeaf( traceWork_t *tw, cLeaf_t *leaf ) {
	int			k;
	int			brushnum;
	cbrush_t	*b;
	cPatch_t	*patch;

	for (k=0 ; k<leaf->numLeafBrushes ; k++) {
		brushnum = cm.leafbrushes[leaf->firstLeafBrush+k];
		b = &cm.brushes[brushnum];
		if (b->checkcount == cm.checkcount) {
			continue;	
		}
		b->checkcount = cm.checkcount;

		if ( !(b->contents & tw->contents)) {
			continue;
		}

		CM_TestBoxInBrush( tw, b );
		if ( tw->trace.allsolid ) {
			return;
		}
	}

#ifdef BSPC
	if (1) {
#else
	if ( !cm_noCurves->integer ) {
#endif 
		for ( k = 0 ; k < leaf->numLeafSurfaces ; k++ ) {
			patch = cm.surfaces[ cm.leafsurfaces[ leaf->firstLeafSurface + k ] ];
			if ( !patch ) {
				continue;
			}
			if ( patch->checkcount == cm.checkcount ) {
				continue;	
			}
			patch->checkcount = cm.checkcount;

			if ( !(patch->contents & tw->contents)) {
				continue;
			}

			if( CM_PositionTestInPatchCollide( tw, patch->pc ) ) {
				tw->trace.fraction = 0;
				tw->trace.startsolid = tw->trace.allsolid = qtrue;
				return;
			}
		}
	}

	if ( cm.terrainEF2 ) {
		for ( k = 0; k < leaf->numLeafSurfaces; k++ ) {
			cTerrainEF2_t *ter = cm.terrainEF2[ cm.leafsurfaces[ leaf->firstLeafSurface + k ] ];
			if ( !ter || ter->checkcount == cm.checkcount ) {
				continue;
			}
			ter->checkcount = cm.checkcount;
			if ( !( ter->contents & tw->contents ) ) {
				continue;
			}
			CM_TraceThroughTerrainEF2( tw, ter );
			if ( tw->trace.allsolid ) {
				return;
			}
		}
	}
}

void CM_TestCapsuleInCapsule( traceWork_t *tw, clipHandle_t model ) {
	int i;
	vec3_t mins, maxs;
	vec3_t top, bottom;
	vec3_t p1, p2, tmp;
	vec3_t offset, symetricSize[2];
	float radius, halfwidth, halfheight, offs, r;

	CM_ModelBounds(model, mins, maxs);

	VectorAdd(tw->start, sphere.offset, top);
	VectorSubtract(tw->start, sphere.offset, bottom);
	for ( i = 0 ; i < 3 ; i++ ) {
		offset[i] = ( mins[i] + maxs[i] ) * 0.5;
		symetricSize[0][i] = mins[i] - offset[i];
		symetricSize[1][i] = maxs[i] - offset[i];
	}
	halfwidth = symetricSize[ 1 ][ 0 ];
	halfheight = symetricSize[ 1 ][ 2 ];
	radius = ( halfwidth > halfheight ) ? halfheight : halfwidth;
	offs = halfheight - radius;

	r = Square(sphere.radius + radius);
	VectorCopy(offset, p1);
	p1[2] += offs;
	VectorSubtract(p1, top, tmp);
	if ( VectorLengthSquared(tmp) < r ) {
		tw->trace.startsolid = tw->trace.allsolid = qtrue;
		tw->trace.fraction = 0;
	}
	VectorSubtract(p1, bottom, tmp);
	if ( VectorLengthSquared(tmp) < r ) {
		tw->trace.startsolid = tw->trace.allsolid = qtrue;
		tw->trace.fraction = 0;
	}
	VectorCopy(offset, p2);
	p2[2] -= offs;
	VectorSubtract(p2, top, tmp);
	if ( VectorLengthSquared(tmp) < r ) {
		tw->trace.startsolid = tw->trace.allsolid = qtrue;
		tw->trace.fraction = 0;
	}
	VectorSubtract(p2, bottom, tmp);
	if ( VectorLengthSquared(tmp) < r ) {
		tw->trace.startsolid = tw->trace.allsolid = qtrue;
		tw->trace.fraction = 0;
	}
	if ( (top[2] >= p1[2] && top[2] <= p2[2]) ||
		(bottom[2] >= p1[2] && bottom[2] <= p2[2]) ) {
		top[2] = p1[2] = 0;
		VectorSubtract(top, p1, tmp);
		if ( VectorLengthSquared(tmp) < r ) {
			tw->trace.startsolid = tw->trace.allsolid = qtrue;
			tw->trace.fraction = 0;
		}
	}
}

void CM_TestBoundingBoxInCapsule( traceWork_t *tw, clipHandle_t model ) {
	vec3_t mins, maxs, offset, size[2];
	clipHandle_t h;
	cmodel_t *cmod;
	int i;

	CM_ModelBounds(model, mins, maxs);

	for ( i = 0 ; i < 3 ; i++ ) {
		offset[i] = ( mins[i] + maxs[i] ) * 0.5;
		size[0][i] = mins[i] - offset[i];
		size[1][i] = maxs[i] - offset[i];
		tw->start[i] -= offset[i];
		tw->end[i] -= offset[i];
	}

	sphere.use = qtrue;
	sphere.radius = ( size[1][0] > size[1][2] ) ? size[1][2]: size[1][0];
	VectorSet( sphere.offset, 0, 0, size[1][2] - sphere.radius );

	h = CM_TempBoxModel(tw->size[0], tw->size[1], qfalse);
	cmod = CM_ClipHandleToModel( h );
	CM_TestInLeaf( tw, &cmod->leaf );
}

#define	MAX_POSITION_LEAFS	1024
void CM_PositionTest( traceWork_t *tw ) {
	int		leafs[MAX_POSITION_LEAFS];
	int		i;
	leafList_t	ll;

	VectorAdd( tw->start, tw->size[0], ll.bounds[0] );
	VectorAdd( tw->start, tw->size[1], ll.bounds[1] );

	for (i=0 ; i<3 ; i++) {
		ll.bounds[0][i] -= 1;
		ll.bounds[1][i] += 1;
	}

	ll.count = 0;
	ll.maxcount = MAX_POSITION_LEAFS;
	ll.list = leafs;
	ll.storeLeafs = CM_StoreLeafs;
	ll.lastLeaf = 0;
	ll.overflowed = qfalse;

	cm.checkcount++;

	CM_BoxLeafnums_r( &ll, 0 );


	cm.checkcount++;

	for (i=0 ; i < ll.count ; i++) {
		CM_TestInLeaf( tw, &cm.leafs[leafs[i]] );
		if ( tw->trace.allsolid ) {
			break;
		}
	}
}




void CM_TraceThroughPatch( traceWork_t *tw, cPatch_t *patch ) {
	float		oldFrac;

	c_patch_traces++;

	oldFrac = tw->trace.fraction;

	CM_TraceThroughPatchCollide( tw, patch->pc );

	if ( tw->trace.fraction < oldFrac ) {
		tw->trace.surfaceFlags = patch->surfaceFlags;
		tw->trace.contents = patch->contents;
	}
}

void CM_TraceThroughBrush( traceWork_t *tw, cbrush_t *brush ) {
	int			i;
	cplane_t	*plane, *clipplane, *clipplane2;
	float		dist;
	float		enterFrac, leaveFrac, leaveFrac2;
	float		d1, d2;
	qboolean	getout, startout;
	float		f;
	cbrushside_t	*side, *leadside, *leadside2;
	float		t;

	if( !brush->numsides ) {
		return;
	}

	enterFrac = -1.0;
	leaveFrac = 1.0;
	clipplane = NULL;

	c_brush_traces++;

	getout = qfalse;
	startout = qfalse;

	leadside = NULL;
	if( !( brush->contents & CONTENTS_SETCLIP ) || !tw->isPoint ) {
		if( sphere.use ) {
			for( i = 0; i < brush->numsides; i++ ) {
				side = brush->sides + i;
				plane = side->plane;

				t = DotProduct( plane->normal, sphere.offset );
				if( t < 0 )
				{
					t = -t;
				}

				dist = t + plane->dist + sphere.radius;

				d1 = DotProduct( tw->start, plane->normal ) - dist;
				d2 = DotProduct( tw->end, plane->normal ) - dist;

				if( d1 <= 0 && d2 <= 0 ) {
					continue;
				}

				if( d2 > 0 ) {
					getout = qtrue;	
				}
				if( d1 > 0 ) {
					startout = qtrue;
				}

				if( d1 > 0 && ( d2 >= SURFACE_CLIP_EPSILON || d2 >= d1 ) ) {
					return;
				}

				if( d1 > d2 ) {	
					f = ( d1 - SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
					if( f < 0 ) {
						f = 0;
					}
					if( f > enterFrac ) {
						enterFrac = f;
						clipplane = plane;
						leadside = side;
					}
				}
				else {	
					f = ( d1 + SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
					if( f > 1 ) {
						f = 1;
					}
					if( f < leaveFrac ) {
						leaveFrac = f;
					}
				}
			}
		} else {
			for( i = 0; i < brush->numsides; i++ ) {
				side = brush->sides + i;
				plane = side->plane;

				dist = plane->dist - DotProduct( tw->offsets[ plane->signbits ], plane->normal );

				d1 = DotProduct( tw->start, plane->normal ) - dist;
				d2 = DotProduct( tw->end, plane->normal ) - dist;

				if( d1 <= 0 && d2 <= 0 ) {
					continue;
				}

				if( d2 > 0 ) {
					getout = qtrue;	
				}
				if( d1 > 0 ) {
					startout = qtrue;

					if( d2 >= SURFACE_CLIP_EPSILON || d2 >= d1 ) {
						return;
					}
				}

				if( d1 > d2 ) { 
					f = ( d1 - SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
					if( f < 0 ) {
						f = 0;
					}
					if( f > enterFrac ) {
						enterFrac = f;
						clipplane = plane;
						leadside = side;
					}
				} else { 
					f = ( d1 + SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
					if( f > 1 ) {
						f = 1;
					}
					if( f < leaveFrac ) {
						leaveFrac = f;
					}
				}
			}
		}

		if( !startout ) {	
			tw->trace.startsolid = qtrue;
			if( !getout ) {
				tw->trace.fraction = 0;
				tw->trace.allsolid = qtrue;
			}
			return;
		}

		if( enterFrac <= leaveFrac ) {
			if( enterFrac > -1 && enterFrac < tw->trace.fraction ) {
				if( enterFrac < 0 ) {
					enterFrac = 0;
				}
				tw->trace.fraction = enterFrac;
				tw->trace.plane = *clipplane;
				tw->trace.surfaceFlags = leadside->surfaceFlags;
				tw->trace.contents = brush->contents;
			}
		}
	} else {
		leaveFrac2 = 1.0;
		clipplane2 = NULL;
		leadside2 = NULL;

		if (!(tw->contents & CONTENTS_SETCLIP)) {
			return;
		}

		for( i = 0; i < brush->numsides; i++ ) {
			side = brush->sides + i;
			plane = side->plane;

			dist = plane->dist - DotProduct( tw->offsets[ plane->signbits ], plane->normal );

			d1 = DotProduct( tw->start, plane->normal ) - dist;
			d2 = DotProduct( tw->end, plane->normal ) - dist;

			if( d1 <= 0 && d2 <= 0 ) {
				continue;
			}

			if( d1 > 0 ) {
				if( d2 >= SURFACE_CLIP_EPSILON || d2 >= d1 ) {
					return;
				}
			}

			if( d1 > d2 ) {	
				f = ( d1 - SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
				if( f < 0 ) {
					f = 0;
				}
				if( f > enterFrac ) {
					enterFrac = f;
					clipplane = plane;
					leadside = side;
				}
			} else {	
				f = ( d1 + SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
				if( f > 1 ) {
					f = 1;
				}
				if( f < leaveFrac ) {
					leaveFrac = f;
					clipplane2 = plane;
					leadside2 = side;
					leaveFrac2 = ( d1 - SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
				}
			}
		}

		if( enterFrac <= leaveFrac ) {
			if( enterFrac > -1 && enterFrac < tw->trace.fraction ) {
				if( enterFrac < 0 ) {
					enterFrac = 0;
				}

				tw->trace.fraction = enterFrac;
				tw->trace.plane = *clipplane;
				tw->trace.surfaceFlags = leadside->surfaceFlags;
				tw->trace.contents = brush->contents;
				return;
			}

			if( leaveFrac2 < 1.0 ) {
				if( leaveFrac2 < tw->trace.fraction ) {
					tw->trace.fraction = leaveFrac2;
					tw->trace.plane = *clipplane2;
					tw->trace.surfaceFlags = leadside2->surfaceFlags;
					tw->trace.contents = brush->contents;
					return;
				}
			}
		}
	}
}

void CM_TraceToLeaf( traceWork_t *tw, cLeaf_t *leaf, int leafFlags ) {
	int k;
	cbrush_t *b;
	cPatch_t *patch;

	for( k = 0; k<leaf->numLeafBrushes; k++ ) {
		b = &cm.brushes[ cm.leafbrushes[ leaf->firstLeafBrush + k ] ];
		if( b->checkcount == cm.checkcount ) {
			continue;	
		}
		b->checkcount = cm.checkcount;

		if( !( b->contents & tw->contents ) ) {
			continue;
		}

		CM_TraceThroughBrush( tw, b );
		if( !tw->trace.fraction ) {
			return;
		}
	}

#ifdef BSPC
	if( 1 ) {
#else
	if( !( leafFlags & 1 ) && !cm_noCurves->integer ) {
#endif 
		for( k = 0; k < leaf->numLeafSurfaces; k++ ) {
			patch = cm.surfaces[ cm.leafsurfaces[ leaf->firstLeafSurface + k ] ];
			if( !patch ) {
				continue;
			}
			if( patch->checkcount == cm.checkcount ) {
				continue;	
			}
			patch->checkcount = cm.checkcount;

			if( !( patch->contents & tw->contents ) ) {
				continue;
			}

			CM_TraceThroughPatch( tw, patch );
			if( !tw->trace.fraction ) {
				return;
			}
		}
	}

	if ( !( leafFlags & 2 ) && cm.terrainEF2 ) {
		for ( k = 0; k < leaf->numLeafSurfaces; k++ ) {
			cTerrainEF2_t *ter = cm.terrainEF2[ cm.leafsurfaces[ leaf->firstLeafSurface + k ] ];
			if ( !ter || ter->checkcount == cm.checkcount ) {
				continue;
			}
			ter->checkcount = cm.checkcount;
			if ( !( ter->contents & tw->contents ) ) {
				continue;
			}
			CM_TraceThroughTerrainEF2( tw, ter );
			if ( !tw->trace.fraction ) {
				return;
			}
		}
	}
}

#define RADIUS_EPSILON		1.0f

void CM_TraceThroughSphere( traceWork_t *tw, vec3_t origin, float radius, vec3_t start, vec3_t end ) {
	float l1, l2, length, scale, fraction;
	float b, c, d, sqrtd;
	vec3_t v1, dir, intersection;

	VectorSubtract(start, origin, dir);
	l1 = VectorLengthSquared(dir);
	if (l1 < Square(radius)) {
		tw->trace.fraction = 0;
		tw->trace.startsolid = qtrue;
		VectorSubtract(end, origin, dir);
		l1 = VectorLengthSquared(dir);
		if (l1 < Square(radius)) {
			tw->trace.allsolid = qtrue;
		}
		return;
	}
	VectorSubtract(end, start, dir);
	length = VectorNormalize(dir);
	l1 = CM_DistanceFromLineSquared(origin, start, end, dir);
	VectorSubtract(end, origin, v1);
	l2 = VectorLengthSquared(v1);
	if (l1 >= Square(radius) && l2 > Square(radius+SURFACE_CLIP_EPSILON)) {
		return;
	}
	VectorSubtract(start, origin, v1);
	b = 2.0f * (dir[0] * v1[0] + dir[1] * v1[1] + dir[2] * v1[2]);
	c = v1[0] * v1[0] + v1[1] * v1[1] + v1[2] * v1[2] - (radius+RADIUS_EPSILON) * (radius+RADIUS_EPSILON);

	d = b * b - 4.0f * c;
	if (d > 0) {
		sqrtd = SquareRootFloat(d);
		fraction = (- b - sqrtd) * 0.5f; 
		if (fraction < 0) {
			fraction = 0;
		}
		else {
			fraction /= length;
		}
		if ( fraction < tw->trace.fraction ) {
			tw->trace.fraction = fraction;
			VectorSubtract(end, start, dir);
			VectorMA(start, fraction, dir, intersection);
			VectorSubtract(intersection, origin, dir);
			#ifdef CAPSULE_DEBUG
				l2 = VectorLength(dir);
				if (l2 < radius) {
					int bah = 1;
				}
			#endif
			scale = 1 / (radius+RADIUS_EPSILON);
			VectorScale(dir, scale, dir);
			VectorCopy(dir, tw->trace.plane.normal);
			tw->trace.plane.dist = DotProduct(tw->trace.plane.normal, intersection);
			tw->trace.contents = CONTENTS_BODY;
		}
	}
	else if (d == 0) {
	}
}

void CM_TraceThroughVerticalCylinder( traceWork_t *tw, vec3_t origin, float radius, float halfheight, vec3_t start, vec3_t end) {
	float length, scale, fraction, l1, l2;
	float b, c, d, sqrtd;
	vec3_t v1, dir, start2d, end2d, org2d, intersection;

	VectorSet(start2d, start[0], start[1], 0);
	VectorSet(end2d, end[0], end[1], 0);
	VectorSet(org2d, origin[0], origin[1], 0);
	if (start[2] <= origin[2] + halfheight &&
				start[2] >= origin[2] - halfheight) {
		VectorSubtract(start2d, org2d, dir);
		l1 = VectorLengthSquared(dir);
		if (l1 < Square(radius)) {
			tw->trace.fraction = 0;
			tw->trace.startsolid = qtrue;
			VectorSubtract(end2d, org2d, dir);
			l1 = VectorLengthSquared(dir);
			if (l1 < Square(radius)) {
				tw->trace.allsolid = qtrue;
			}
			return;
		}
	}
	VectorSubtract(end2d, start2d, dir);
	length = VectorNormalize(dir);
	l1 = CM_DistanceFromLineSquared(org2d, start2d, end2d, dir);
	VectorSubtract(end2d, org2d, v1);
	l2 = VectorLengthSquared(v1);
	if (l1 >= Square(radius) && l2 > Square(radius+SURFACE_CLIP_EPSILON)) {
		return;
	}
	VectorSubtract(start, origin, v1);
	b = 2.0f * (v1[0] * dir[0] + v1[1] * dir[1]);
	c = v1[0] * v1[0] + v1[1] * v1[1] - (radius+RADIUS_EPSILON) * (radius+RADIUS_EPSILON);

	d = b * b - 4.0f * c;
	if (d > 0) {
		sqrtd = SquareRootFloat(d);
		fraction = (- b - sqrtd) * 0.5f;
		if (fraction < 0) {
			fraction = 0;
		}
		else {
			fraction /= length;
		}
		if ( fraction < tw->trace.fraction ) {
			VectorSubtract(end, start, dir);
			VectorMA(start, fraction, dir, intersection);
			if (intersection[2] <= origin[2] + halfheight &&
						intersection[2] >= origin[2] - halfheight) {
				tw->trace.fraction = fraction;
				VectorSubtract(intersection, origin, dir);
				dir[2] = 0;
				#ifdef CAPSULE_DEBUG
					l2 = VectorLength(dir);
					if (l2 <= radius) {
						int bah = 1;
					}
				#endif
				scale = 1 / (radius+RADIUS_EPSILON);
				VectorScale(dir, scale, dir);
				VectorCopy(dir, tw->trace.plane.normal);
				tw->trace.plane.dist = DotProduct(tw->trace.plane.normal, intersection);
				tw->trace.contents = CONTENTS_BODY;
			}
		}
	}
	else if (d == 0) {
	}
}

void CM_TraceCapsuleThroughCapsule( traceWork_t *tw, clipHandle_t model ) {
	int i;
	vec3_t mins, maxs;
	vec3_t top, bottom, starttop, startbottom, endtop, endbottom;
	vec3_t offset, symetricSize[2];
	float radius, halfwidth, halfheight, offs, h;

	CM_ModelBounds(model, mins, maxs);
	if ( tw->bounds[0][0] > maxs[0] + RADIUS_EPSILON
		|| tw->bounds[0][1] > maxs[1] + RADIUS_EPSILON
		|| tw->bounds[0][2] > maxs[2] + RADIUS_EPSILON
		|| tw->bounds[1][0] < mins[0] - RADIUS_EPSILON
		|| tw->bounds[1][1] < mins[1] - RADIUS_EPSILON
		|| tw->bounds[1][2] < mins[2] - RADIUS_EPSILON
		) {
		return;
	}
	VectorAdd(tw->start, sphere.offset, starttop);
	VectorSubtract(tw->start, sphere.offset, startbottom);
	VectorAdd(tw->end, sphere.offset, endtop);
	VectorSubtract(tw->end, sphere.offset, endbottom);

	for ( i = 0 ; i < 3 ; i++ ) {
		offset[i] = ( mins[i] + maxs[i] ) * 0.5;
		symetricSize[0][i] = mins[i] - offset[i];
		symetricSize[1][i] = maxs[i] - offset[i];
	}
	halfwidth = symetricSize[ 1 ][ 0 ];
	halfheight = symetricSize[ 1 ][ 2 ];
	radius = ( halfwidth > halfheight ) ? halfheight : halfwidth;
	offs = halfheight - radius;
	VectorCopy(offset, top);
	top[2] += offs;
	VectorCopy(offset, bottom);
	bottom[2] -= offs;
	radius += sphere.radius;
	if ( tw->start[0] != tw->end[0] || tw->start[1] != tw->end[1] ) {
		h = halfheight - radius;
		if ( h > 0 ) {
			CM_TraceThroughVerticalCylinder(tw, offset, radius, h, tw->start, tw->end);
		}
	}
	CM_TraceThroughSphere(tw, top, radius, startbottom, endbottom);
	CM_TraceThroughSphere(tw, bottom, radius, starttop, endtop);
}

void CM_TraceBoundingBoxThroughCapsule( traceWork_t *tw, clipHandle_t model ) {
	vec3_t mins, maxs, offset, size[2];
	clipHandle_t h;
	cmodel_t *cmod;
	int i;

	CM_ModelBounds(model, mins, maxs);

	for ( i = 0 ; i < 3 ; i++ ) {
		offset[i] = ( mins[i] + maxs[i] ) * 0.5;
		size[0][i] = mins[i] - offset[i];
		size[1][i] = maxs[i] - offset[i];
		tw->start[i] -= offset[i];
		tw->end[i] -= offset[i];
	}

	sphere.use = qtrue;
	sphere.radius = ( size[1][0] > size[1][2] ) ? size[1][2]: size[1][0];
	VectorSet( sphere.offset, 0, 0, size[1][2] - sphere.radius );

	h = CM_TempBoxModel(tw->size[0], tw->size[1], qfalse);
	cmod = CM_ClipHandleToModel( h );
	CM_TraceToLeaf( tw, &cmod->leaf, 0 );
}


void CM_TraceThroughTree( traceWork_t *tw, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, int leafFlags ) {
	cNode_t		*node;
	cplane_t	*plane;
	float		t1, t2, offset;
	float		frac, frac2;
	float		idist;
	vec3_t		mid;
	int			side;
	float		midf;

	if (tw->trace.fraction <= p1f) {
		return;		
	}

	if (num < 0) {
		CM_TraceToLeaf( tw, &cm.leafs[-1-num], leafFlags );
		return;
	}

	node = cm.nodes + num;
	plane = node->plane;

	if ( plane->type < 3 ) {
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
		offset = tw->extents[plane->type];
	} else {
		t1 = DotProduct (plane->normal, p1) - plane->dist;
		t2 = DotProduct (plane->normal, p2) - plane->dist;
		if ( tw->isPoint ) {
			offset = 0;
		} else {
			offset = 2048;
		}
	}

	if ( t1 >= offset + 1 && t2 >= offset + 1 ) {
		CM_TraceThroughTree( tw, node->children[0], p1f, p2f, p1, p2, leafFlags );
		return;
	}
	if ( t1 < -offset - 1 && t2 < -offset - 1 ) {
		CM_TraceThroughTree( tw, node->children[1], p1f, p2f, p1, p2, leafFlags );
		return;
	}

	if ( t1 < t2 ) {
		idist = 1.0/(t1-t2);
		side = 1;
		frac2 = (t1 + offset + SURFACE_CLIP_EPSILON)*idist;
		frac = (t1 - offset + SURFACE_CLIP_EPSILON)*idist;
	} else if (t1 > t2) {
		idist = 1.0/(t1-t2);
		side = 0;
		frac2 = (t1 - offset - SURFACE_CLIP_EPSILON)*idist;
		frac = (t1 + offset + SURFACE_CLIP_EPSILON)*idist;
	} else {
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	if ( frac < 0 ) {
		frac = 0;
	}
	if ( frac > 1 ) {
		frac = 1;
	}

	midf = p1f + (p2f - p1f)*frac;

	mid[0] = p1[0] + frac*(p2[0] - p1[0]);
	mid[1] = p1[1] + frac*(p2[1] - p1[1]);
	mid[2] = p1[2] + frac*(p2[2] - p1[2]);

	CM_TraceThroughTree( tw, node->children[side], p1f, midf, p1, mid, leafFlags );


	if ( frac2 < 0 ) {
		frac2 = 0;
	}
	if ( frac2 > 1 ) {
		frac2 = 1;
	}

	midf = p1f + (p2f - p1f)*frac2;

	mid[0] = p1[0] + frac2*(p2[0] - p1[0]);
	mid[1] = p1[1] + frac2*(p2[1] - p1[1]);
	mid[2] = p1[2] + frac2*(p2[2] - p1[2]);

	CM_TraceThroughTree( tw, node->children[side^1], midf, p2f, mid, p2, leafFlags );
}


static void CM_BoxTraceInternal( trace_t *results, const vec3_t start, const vec3_t end,
                                 const vec3_t mins, const vec3_t maxs,
                                 clipHandle_t model, int brushmask, int cylinder,
                                 int leafFlags ) {
	int			i;
	traceWork_t	tw;
	vec3_t		offset;
	cmodel_t	*cmod;

	cmod = CM_ClipHandleToModel( model );

	cm.checkcount++;		

	c_traces++;				

	Com_Memset( &tw, 0, sizeof( tw ) );
	tw.trace.fraction = 1;	

	if( !cm.numNodes ) {	
		Com_Memset( results, 0, sizeof( *results ) );
		results->fraction = 1;
		return;
	}

	if( !mins ) {
		mins = vec3_origin;
	}
	if( !maxs ) {
		maxs = vec3_origin;
	}

	tw.contents = brushmask;

	for( i = 0; i < 3; i++ ) {
		offset[ i ] = ( mins[ i ] + maxs[ i ] ) * 0.5;
		tw.size[ 0 ][ i ] = mins[ i ] - offset[ i ];
		tw.size[ 1 ][ i ] = maxs[ i ] - offset[ i ];
		tw.start[ i ] = start[ i ] + offset[ i ];
		tw.end[ i ] = end[ i ] + offset[ i ];
	}

	tw.height = tw.size[ 1 ][ 2 ];
	tw.radius = tw.size[ 1 ][ 0 ];

	if( cylinder && !sphere.use )
	{
		sphere.use = qtrue;
		sphere.radius = ( tw.size[ 1 ][ 0 ] > tw.size[ 1 ][ 2 ] ) ? tw.size[ 1 ][ 2 ] : tw.size[ 1 ][ 0 ];
		VectorSet( sphere.offset, 0, 0, tw.size[ 1 ][ 2 ] - sphere.radius );
	}
	tw.maxOffset = tw.size[ 1 ][ 0 ] + tw.size[ 1 ][ 1 ] + tw.size[ 1 ][ 2 ];

	tw.offsets[ 0 ][ 0 ] = tw.size[ 0 ][ 0 ];
	tw.offsets[ 0 ][ 1 ] = tw.size[ 0 ][ 1 ];
	tw.offsets[ 0 ][ 2 ] = tw.size[ 0 ][ 2 ];

	tw.offsets[ 1 ][ 0 ] = tw.size[ 1 ][ 0 ];
	tw.offsets[ 1 ][ 1 ] = tw.size[ 0 ][ 1 ];
	tw.offsets[ 1 ][ 2 ] = tw.size[ 0 ][ 2 ];

	tw.offsets[ 2 ][ 0 ] = tw.size[ 0 ][ 0 ];
	tw.offsets[ 2 ][ 1 ] = tw.size[ 1 ][ 1 ];
	tw.offsets[ 2 ][ 2 ] = tw.size[ 0 ][ 2 ];

	tw.offsets[ 3 ][ 0 ] = tw.size[ 1 ][ 0 ];
	tw.offsets[ 3 ][ 1 ] = tw.size[ 1 ][ 1 ];
	tw.offsets[ 3 ][ 2 ] = tw.size[ 0 ][ 2 ];

	tw.offsets[ 4 ][ 0 ] = tw.size[ 0 ][ 0 ];
	tw.offsets[ 4 ][ 1 ] = tw.size[ 0 ][ 1 ];
	tw.offsets[ 4 ][ 2 ] = tw.size[ 1 ][ 2 ];

	tw.offsets[ 5 ][ 0 ] = tw.size[ 1 ][ 0 ];
	tw.offsets[ 5 ][ 1 ] = tw.size[ 0 ][ 1 ];
	tw.offsets[ 5 ][ 2 ] = tw.size[ 1 ][ 2 ];

	tw.offsets[ 6 ][ 0 ] = tw.size[ 0 ][ 0 ];
	tw.offsets[ 6 ][ 1 ] = tw.size[ 1 ][ 1 ];
	tw.offsets[ 6 ][ 2 ] = tw.size[ 1 ][ 2 ];

	tw.offsets[ 7 ][ 0 ] = tw.size[ 1 ][ 0 ];
	tw.offsets[ 7 ][ 1 ] = tw.size[ 1 ][ 1 ];
	tw.offsets[ 7 ][ 2 ] = tw.size[ 1 ][ 2 ];

	for( i = 0; i < 3; i++ ) {
		if( tw.start[ i ] < tw.end[ i ] ) {
			tw.bounds[ 0 ][ i ] = tw.start[ i ] + tw.size[ 0 ][ i ];
			tw.bounds[ 1 ][ i ] = tw.end[ i ] + tw.size[ 1 ][ i ];
		}
		else {
			tw.bounds[ 0 ][ i ] = tw.end[ i ] + tw.size[ 0 ][ i ];
			tw.bounds[ 1 ][ i ] = tw.start[ i ] + tw.size[ 1 ][ i ];
		}
	}

	if( start[ 0 ] == end[ 0 ] && start[ 1 ] == end[ 1 ] && start[ 2 ] == end[ 2 ] ) {
		if( model ) {
			CM_TestInLeaf( &tw, &cmod->leaf );
		} else {
			CM_PositionTest( &tw );
		}
	}
	else {
		if( tw.size[ 0 ][ 0 ] == 0 && tw.size[ 0 ][ 1 ] == 0 && tw.size[ 0 ][ 2 ] == 0 ) {
			tw.isPoint = qtrue;
			VectorClear( tw.extents );
		}
		else {
			tw.isPoint = qfalse;
			tw.extents[ 0 ] = tw.size[ 1 ][ 0 ];
			tw.extents[ 1 ] = tw.size[ 1 ][ 1 ];
			tw.extents[ 2 ] = tw.size[ 1 ][ 2 ];
		}

		if( model ) {
			CM_TraceToLeaf( &tw, &cmod->leaf, 0 );
		} else {
			CM_TraceThroughTree( &tw, 0, 0, 1, tw.start, tw.end, 0 );
		}
	}

	if( tw.trace.fraction == 1 ) {
		VectorCopy( end, tw.trace.endpos );
	}
	else {
		for( i = 0; i<3; i++ ) {
			tw.trace.endpos[ i ] = start[ i ] + tw.trace.fraction * ( end[ i ] - start[ i ] );
		}
	}

	assert( tw.trace.allsolid ||
		tw.trace.fraction == 1.0 ||
		VectorLengthSquared( tw.trace.plane.normal ) > 0.9999 );
	*results = tw.trace;
	sphere.use = qfalse;
}

void CM_BoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
				  const vec3_t mins, const vec3_t maxs,
				  clipHandle_t model, int brushmask, int cylinder ) {
	CM_BoxTraceInternal( results, start, end, mins, maxs, model, brushmask, cylinder, 0 );
}

void CM_BoxTraceEx( trace_t *results, const vec3_t start, const vec3_t end,
					const vec3_t mins, const vec3_t maxs,
					clipHandle_t model, int brushmask, int cylinder, int leafFlags ) {
	CM_BoxTraceInternal( results, start, end, mins, maxs, model, brushmask, cylinder, leafFlags );
}

void CM_TransformedBoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
						  const vec3_t mins, const vec3_t maxs,
						  clipHandle_t model, int brushmask,
						  const vec3_t origin, const vec3_t angles, int cylinder ) {
	trace_t		trace;
	vec3_t		start_l, end_l;
	vec3_t		a;
	vec3_t		forward, left, up;
	vec3_t		temp;
	qboolean	rotated;
	vec3_t		offset;
	vec3_t		symetricSize[2];
	int			i;
	float		halfwidth;
	float		halfheight;
	float		t;

	if( !mins ) {
		mins = vec3_origin;
	}
	if( !maxs ) {
		maxs = vec3_origin;
	}

	for ( i = 0 ; i < 3 ; i++ ) {
		offset[i] = ( mins[i] + maxs[i] ) * 0.5;
		symetricSize[0][i] = mins[i] - offset[i];
		symetricSize[1][i] = maxs[i] - offset[i];
		start_l[i] = start[i] + offset[i];
		end_l[i] = end[i] + offset[i];
	}

	VectorSubtract( start_l, origin, start_l );
	VectorSubtract( end_l, origin, end_l );

	if ( angles[0] || angles[1] || angles[2] ) {
		rotated = qtrue;
	} else {
		rotated = qfalse;
	}

	halfwidth = symetricSize[ 1 ][ 0 ];
	halfheight = symetricSize[ 1 ][ 2 ];

	sphere.use = cylinder;
	sphere.radius = ( halfwidth > halfheight ) ? halfheight : halfwidth;
	t = halfheight - sphere.radius;

	if( rotated ) {
		AngleVectorsLeft( angles, forward, left, up );

		VectorCopy( start_l, temp );
		start_l[ 0 ] = DotProduct( temp, forward );
		start_l[ 1 ] = DotProduct( temp, left );
		start_l[ 2 ] = DotProduct( temp, up );

		VectorCopy( end_l, temp );
		end_l[ 0 ] = DotProduct( temp, forward );
		end_l[ 1 ] = DotProduct( temp, left );
		end_l[ 2 ] = DotProduct( temp, up );

		sphere.offset[ 0 ] = forward[ 2 ] * t;
		sphere.offset[ 1 ] = left[ 2 ] * t;
		sphere.offset[ 2 ] = up[ 2 ] * t;
	}
	else {
		VectorSet( sphere.offset, 0, 0, t );
	}

	CM_BoxTrace( &trace, start_l, end_l, symetricSize[0], symetricSize[1], model, brushmask, cylinder );

	sphere.use = qfalse;

	if ( rotated && trace.fraction != 1.0 ) {
		VectorNegate( angles, a );
		AngleVectorsLeft( a, forward, left, up );

		VectorCopy( trace.plane.normal, temp );
		trace.plane.normal[ 0 ] = DotProduct( temp, forward );
		trace.plane.normal[ 1 ] = DotProduct( temp, left );
		trace.plane.normal[ 2 ] = DotProduct( temp, up );
	}

	trace.endpos[0] = start[0] + trace.fraction * (end[0] - start[0]);
	trace.endpos[1] = start[1] + trace.fraction * (end[1] - start[1]);
	trace.endpos[2] = start[2] + trace.fraction * (end[2] - start[2]);

	*results = trace;
}

qboolean CM_SightTraceThroughPatch( traceWork_t *tw, cPatch_t *patch )
{
	CM_TraceThroughPatchCollide( tw, patch->pc );

	if( tw->trace.allsolid || tw->trace.startsolid || tw->trace.fraction < 1 ) {
		return qfalse;
	}
	return qtrue;
}

qboolean CM_SightTraceThroughBrush( traceWork_t *tw, cbrush_t *brush )
{
	int				i;
	cplane_t		*plane;
	float			dist;
	float			enterFrac, leaveFrac, leaveFrac2;
	float			d1, d2;
	qboolean		startout;
	float			f;
	cbrushside_t	*side;
	float			t;

	if( !brush->numsides ) {
		return qtrue;
	}

	enterFrac = -1.0;
	leaveFrac = 1.0;

	c_brush_traces++;

	startout = qfalse;

	if( !( brush->contents & CONTENTS_SETCLIP ) || !tw->isPoint ) {
		if( sphere.use ) {
			for( i = 0; i < brush->numsides; i++ ) {
				side = brush->sides + i;
				plane = side->plane;

				t = DotProduct( plane->normal, sphere.offset );
				if( t < 0 )
				{
					t = -t;
				}

				dist = t + plane->dist + sphere.radius;

				d1 = DotProduct( tw->start, plane->normal ) - dist;
				d2 = DotProduct( tw->end, plane->normal ) - dist;

				if( d1 <= 0 && d2 <= 0 ) {
					continue;
				}

				if( d1 > 0 ) {
					startout = qtrue;
				}

				if( d1 > 0 && ( d2 >= SURFACE_CLIP_EPSILON || d2 >= d1 ) ) {
					return qtrue;
				}

				if( d1 > d2 ) {	
					f = ( d1 - SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
					if( f < 0 ) {
						f = 0;
					}
					if( f > enterFrac ) {
						enterFrac = f;
					}
				}
				else {	
					f = ( d1 + SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
					if( f > 1 ) {
						f = 1;
					}
					if( f < leaveFrac ) {
						leaveFrac = f;
					}
				}
			}
		} else {
			for( i = 0; i < brush->numsides; i++ ) {
				side = brush->sides + i;
				plane = side->plane;

				dist = plane->dist - DotProduct( tw->offsets[ plane->signbits ], plane->normal );

				d1 = DotProduct( tw->start, plane->normal ) - dist;
				d2 = DotProduct( tw->end, plane->normal ) - dist;

				if( d1 <= 0 && d2 <= 0 ) {
					continue;
				}

				if( d1 > 0 ) {
					startout = qtrue;

					if( d2 >= SURFACE_CLIP_EPSILON || d2 >= d1 ) {
						return qtrue;
					}
				}

				if( d1 > d2 ) { 
					f = ( d1 - SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
					if( f < 0 ) {
						f = 0;
					}
					if( f > enterFrac ) {
						enterFrac = f;
					}
				} else { 
					f = ( d1 + SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
					if( f > 1 ) {
						f = 1;
					}
					if( f < leaveFrac ) {
						leaveFrac = f;
					}
				}
			}
		}

		if( !startout ) {	
			return qfalse;
		}

		if( enterFrac <= leaveFrac ) {
			if( enterFrac > -1 ) {
				return qfalse;
			}
		}
	} else {
		leaveFrac2 = 1.0;
		for( i = 0; i < brush->numsides; i++ ) {
			side = brush->sides + i;
			plane = side->plane;

			dist = plane->dist - DotProduct( tw->offsets[ plane->signbits ], plane->normal );

			d1 = DotProduct( tw->start, plane->normal ) - dist;
			d2 = DotProduct( tw->end, plane->normal ) - dist;

			if( d1 <= 0 && d2 <= 0 ) {
				continue;
			}

			if( d1 > 0 ) {
				if( d2 >= SURFACE_CLIP_EPSILON || d2 >= d1 ) {
					return qtrue;
				}
			}

			if( d1 > d2 ) {	
				f = ( d1 - SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
				if( f < 0 ) {
					f = 0;
				}
				if( f > enterFrac ) {
					enterFrac = f;
				}
			} else {	
				f = ( d1 + SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
				if( f > 1 ) {
					f = 1;
				}
				if( f < leaveFrac ) {
					leaveFrac = f;
					leaveFrac2 = ( d1 - SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
				}
			}
		}

		if( enterFrac <= leaveFrac ) {
			if( enterFrac > -1 && enterFrac < tw->trace.fraction ) {
				if( enterFrac < 0 ) {
					enterFrac = 0;
				}

				return qfalse;
			}

			if( leaveFrac2 < 1.0 ) {
				if( leaveFrac2 < tw->trace.fraction ) {
					return qfalse;
				}
			}
		}
	}

	return qtrue;
}

qboolean CM_SightTraceToLeaf( traceWork_t *tw, cLeaf_t *leaf ) {
	int k;
	cbrush_t *b;
	cPatch_t *patch;

	for( k = 0; k<leaf->numLeafBrushes; k++ ) {
		b = &cm.brushes[ cm.leafbrushes[ leaf->firstLeafBrush + k ] ];
		if( b->checkcount == cm.checkcount ) {
			continue;	
		}
		b->checkcount = cm.checkcount;

		if( !( b->contents & tw->contents ) ) {
			continue;
		}

		if( !CM_SightTraceThroughBrush( tw, b ) ) {
			return qfalse;
		}
	}

#ifdef BSPC
	if( 1 ) {
#else
	if( !cm_noCurves->integer ) {
#endif 
		for( k = 0; k < leaf->numLeafSurfaces; k++ ) {
			patch = cm.surfaces[ cm.leafsurfaces[ leaf->firstLeafSurface + k ] ];
			if( !patch ) {
				continue;
			}
			if( patch->checkcount == cm.checkcount ) {
				continue;	
			}
			patch->checkcount = cm.checkcount;

			if( !( patch->contents & tw->contents ) ) {
				continue;
			}

			if( !CM_SightTraceThroughPatch( tw, patch ) ) {
				return qfalse;
			}
		}
	}

	if ( cm.terrainEF2 ) {
		for ( k = 0; k < leaf->numLeafSurfaces; k++ ) {
			cTerrainEF2_t *ter = cm.terrainEF2[ cm.leafsurfaces[ leaf->firstLeafSurface + k ] ];
			if ( !ter || ter->checkcount == cm.checkcount ) {
				continue;
			}
			ter->checkcount = cm.checkcount;
			if ( !( ter->contents & tw->contents ) ) {
				continue;
			}
			CM_TraceThroughTerrainEF2( tw, ter );
			if ( tw->trace.fraction < 1.0f ) {
				return qfalse;
			}
		}
	}

	return qtrue;
}

qboolean CM_SightTraceThroughTree( traceWork_t *tw, int num, float p1f, float p2f, vec3_t p1, vec3_t p2 ) {
	cNode_t		*node;
	cplane_t	*plane;
	float		t1, t2, offset;
	float		frac, frac2;
	float		idist;
	vec3_t		mid;
	int			side;
	float		midf;

	if( num < 0 ) {
		return CM_SightTraceToLeaf( tw, &cm.leafs[ -1 - num ] );
	}

	node = cm.nodes + num;
	plane = node->plane;

	if( plane->type < 3 ) {
		t1 = p1[ plane->type ] - plane->dist;
		t2 = p2[ plane->type ] - plane->dist;
		offset = tw->extents[ plane->type ];
	}
	else {
		t1 = DotProduct( plane->normal, p1 ) - plane->dist;
		t2 = DotProduct( plane->normal, p2 ) - plane->dist;
		if( tw->isPoint ) {
			offset = 0;
		}
		else {
			offset = 2048;
		}
	}

	if( t1 >= offset + 1 && t2 >= offset + 1 ) {
		return CM_SightTraceThroughTree( tw, node->children[ 0 ], p1f, p2f, p1, p2 );
	}
	if( t1 < -offset - 1 && t2 < -offset - 1 ) {
		return CM_SightTraceThroughTree( tw, node->children[ 1 ], p1f, p2f, p1, p2 );
	}

	if( t1 < t2 ) {
		idist = 1.0 / ( t1 - t2 );
		side = 1;
		frac2 = ( t1 + offset + SURFACE_CLIP_EPSILON )*idist;
		frac = ( t1 - offset + SURFACE_CLIP_EPSILON )*idist;
	}
	else if( t1 > t2 ) {
		idist = 1.0 / ( t1 - t2 );
		side = 0;
		frac2 = ( t1 - offset - SURFACE_CLIP_EPSILON )*idist;
		frac = ( t1 + offset + SURFACE_CLIP_EPSILON )*idist;
	}
	else {
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	if( frac < 0 ) {
		frac = 0;
	}
	if( frac > 1 ) {
		frac = 1;
	}

	midf = p1f + ( p2f - p1f )*frac;

	mid[ 0 ] = p1[ 0 ] + frac*( p2[ 0 ] - p1[ 0 ] );
	mid[ 1 ] = p1[ 1 ] + frac*( p2[ 1 ] - p1[ 1 ] );
	mid[ 2 ] = p1[ 2 ] + frac*( p2[ 2 ] - p1[ 2 ] );

	if( !CM_SightTraceThroughTree( tw, node->children[ side ], p1f, midf, p1, mid ) ) {
		return qfalse;
	}


	if( frac2 < 0 ) {
		frac2 = 0;
	}
	if( frac2 > 1 ) {
		frac2 = 1;
	}

	midf = p1f + ( p2f - p1f )*frac2;

	mid[ 0 ] = p1[ 0 ] + frac2*( p2[ 0 ] - p1[ 0 ] );
	mid[ 1 ] = p1[ 1 ] + frac2*( p2[ 1 ] - p1[ 1 ] );
	mid[ 2 ] = p1[ 2 ] + frac2*( p2[ 2 ] - p1[ 2 ] );

	return CM_SightTraceThroughTree( tw, node->children[ side ^ 1 ], midf, p2f, mid, p2 );
}

qboolean CM_BoxSightTrace( const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask, qboolean cylinder )
{
	int			i;
	traceWork_t	tw;
	vec3_t		offset;
	cmodel_t	*cmod;
	qboolean	bPassed;

	cmod = CM_ClipHandleToModel( model );

	cm.checkcount++;		

	c_traces++;				

	if( !cm.numNodes ) {
		return qfalse;
	}

	Com_Memset( &tw, 0, sizeof( tw ) );
	tw.trace.fraction = 1;	

	tw.contents = brushmask;

	for( i = 0; i < 3; i++ ) {
		offset[ i ] = ( mins[ i ] + maxs[ i ] ) * 0.5;
		tw.size[ 0 ][ i ] = mins[ i ] - offset[ i ];
		tw.size[ 1 ][ i ] = maxs[ i ] - offset[ i ];
		tw.start[ i ] = start[ i ] + offset[ i ];
		tw.end[ i ] = end[ i ] + offset[ i ];
	}

	tw.height = tw.size[ 1 ][ 2 ];
	tw.radius = tw.size[ 1 ][ 0 ];

	if( cylinder && !sphere.use )
	{
		sphere.use = qtrue;
		sphere.radius = ( tw.size[ 1 ][ 0 ] > tw.size[ 1 ][ 2 ] ) ? tw.size[ 1 ][ 2 ] : tw.size[ 1 ][ 0 ];
		VectorSet( sphere.offset, 0, 0, tw.size[ 1 ][ 2 ] - sphere.radius );
	}
	tw.maxOffset = tw.size[ 1 ][ 0 ] + tw.size[ 1 ][ 1 ] + tw.size[ 1 ][ 2 ];

	tw.offsets[ 0 ][ 0 ] = tw.size[ 0 ][ 0 ];
	tw.offsets[ 0 ][ 1 ] = tw.size[ 0 ][ 1 ];
	tw.offsets[ 0 ][ 2 ] = tw.size[ 0 ][ 2 ];

	tw.offsets[ 1 ][ 0 ] = tw.size[ 1 ][ 0 ];
	tw.offsets[ 1 ][ 1 ] = tw.size[ 0 ][ 1 ];
	tw.offsets[ 1 ][ 2 ] = tw.size[ 0 ][ 2 ];

	tw.offsets[ 2 ][ 0 ] = tw.size[ 0 ][ 0 ];
	tw.offsets[ 2 ][ 1 ] = tw.size[ 1 ][ 1 ];
	tw.offsets[ 2 ][ 2 ] = tw.size[ 0 ][ 2 ];

	tw.offsets[ 3 ][ 0 ] = tw.size[ 1 ][ 0 ];
	tw.offsets[ 3 ][ 1 ] = tw.size[ 1 ][ 1 ];
	tw.offsets[ 3 ][ 2 ] = tw.size[ 0 ][ 2 ];

	tw.offsets[ 4 ][ 0 ] = tw.size[ 0 ][ 0 ];
	tw.offsets[ 4 ][ 1 ] = tw.size[ 0 ][ 1 ];
	tw.offsets[ 4 ][ 2 ] = tw.size[ 1 ][ 2 ];

	tw.offsets[ 5 ][ 0 ] = tw.size[ 1 ][ 0 ];
	tw.offsets[ 5 ][ 1 ] = tw.size[ 0 ][ 1 ];
	tw.offsets[ 5 ][ 2 ] = tw.size[ 1 ][ 2 ];

	tw.offsets[ 6 ][ 0 ] = tw.size[ 0 ][ 0 ];
	tw.offsets[ 6 ][ 1 ] = tw.size[ 1 ][ 1 ];
	tw.offsets[ 6 ][ 2 ] = tw.size[ 1 ][ 2 ];

	tw.offsets[ 7 ][ 0 ] = tw.size[ 1 ][ 0 ];
	tw.offsets[ 7 ][ 1 ] = tw.size[ 1 ][ 1 ];
	tw.offsets[ 7 ][ 2 ] = tw.size[ 1 ][ 2 ];

	for( i = 0; i < 3; i++ ) {
		if( tw.start[ i ] < tw.end[ i ] ) {
			tw.bounds[ 0 ][ i ] = tw.start[ i ] + tw.size[ 0 ][ i ];
			tw.bounds[ 1 ][ i ] = tw.end[ i ] + tw.size[ 1 ][ i ];
		}
		else {
			tw.bounds[ 0 ][ i ] = tw.end[ i ] + tw.size[ 0 ][ i ];
			tw.bounds[ 1 ][ i ] = tw.start[ i ] + tw.size[ 1 ][ i ];
		}
	}

	if( start[ 0 ] == end[ 0 ] && start[ 1 ] == end[ 1 ] && start[ 2 ] == end[ 2 ] ) {
		if( model ) {
			CM_TestInLeaf( &tw, &cmod->leaf );
		}
		else {
			CM_PositionTest( &tw );
		}
		bPassed = !tw.trace.startsolid;
	}
	else {
		if( tw.size[ 0 ][ 0 ] == 0 && tw.size[ 0 ][ 1 ] == 0 && tw.size[ 0 ][ 2 ] == 0 ) {
			tw.isPoint = qtrue;
			VectorClear( tw.extents );
		}
		else {
			tw.isPoint = qfalse;
			tw.extents[ 0 ] = tw.size[ 1 ][ 0 ];
			tw.extents[ 1 ] = tw.size[ 1 ][ 1 ];
			tw.extents[ 2 ] = tw.size[ 1 ][ 2 ];
		}

		if( model ) {
			bPassed = CM_SightTraceToLeaf( &tw, &cmod->leaf );
		}
		else {
			bPassed = CM_SightTraceThroughTree( &tw, 0, 0, 1, tw.start, tw.end );
		}
	}

	sphere.use = qfalse;

	return bPassed;
}

qboolean CM_TransformedBoxSightTrace( const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles, qboolean cylinder )
{
	vec3_t		start_l, end_l;
	vec3_t		forward, left, up;
	vec3_t		temp;
	qboolean	rotated;
	vec3_t		offset;
	vec3_t		symetricSize[ 2 ];
	int			i;
	float		halfwidth;
	float		halfheight;
	float		t;

	for( i = 0; i < 3; i++ ) {
		offset[ i ] = ( mins[ i ] + maxs[ i ] ) * 0.5;
		symetricSize[ 0 ][ i ] = mins[ i ] - offset[ i ];
		symetricSize[ 1 ][ i ] = maxs[ i ] - offset[ i ];
		start_l[ i ] = start[ i ] + offset[ i ];
		end_l[ i ] = end[ i ] + offset[ i ];
	}

	VectorSubtract( start_l, origin, start_l );
	VectorSubtract( end_l, origin, end_l );

	if( angles[ 0 ] || angles[ 1 ] || angles[ 2 ] ) {
		rotated = qtrue;
	}
	else {
		rotated = qfalse;
	}

	halfwidth = symetricSize[ 1 ][ 0 ];
	halfheight = symetricSize[ 1 ][ 2 ];

	sphere.use = cylinder;
	sphere.radius = ( halfwidth > halfheight ) ? halfheight : halfwidth;
	t = halfheight - sphere.radius;

	if( rotated ) {
		AngleVectorsLeft( angles, forward, left, up );

		VectorCopy( start_l, temp );
		start_l[ 0 ] = DotProduct( temp, forward );
		start_l[ 1 ] = DotProduct( temp, left );
		start_l[ 2 ] = DotProduct( temp, up );

		VectorCopy( end_l, temp );
		end_l[ 0 ] = DotProduct( temp, forward );
		end_l[ 1 ] = DotProduct( temp, left );
		end_l[ 2 ] = DotProduct( temp, up );

		sphere.offset[ 0 ] = forward[ 2 ] * t;
		sphere.offset[ 1 ] = left[ 2 ] * t;
		sphere.offset[ 2 ] = up[ 2 ] * t;
	}
	else {
		VectorSet( sphere.offset, 0, 0, t );
	}

	return CM_BoxSightTrace( start_l, end_l, symetricSize[ 0 ], symetricSize[ 1 ], model, brushmask, cylinder );
}
