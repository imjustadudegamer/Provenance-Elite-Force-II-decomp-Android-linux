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

#include "server.h"
#include "../corepp/tiki.h"

clipHandle_t SV_ClipHandleForEntity( const gentity_t *ent ) {
	if ( ent->bmodel && ent->solid != SOLID_BBOX ) {
		return CM_InlineModel( ent->s.modelindex );
	}

	return CM_TempBoxModel( ent->mins, ent->maxs, ent->contents );
}




typedef struct worldSector_s {
	int		axis;		
	float	dist;
	struct worldSector_s	*children[2];
	svEntity_t	*entities;
} worldSector_t;

#define	AREA_DEPTH	4
#define	AREA_NODES	64

worldSector_t	sv_worldSectors[AREA_NODES];
int			sv_numworldSectors;


void SV_SectorList_f( void ) {
	int				i, c;
	worldSector_t	*sec;
	svEntity_t		*ent;

	for ( i = 0 ; i < AREA_NODES ; i++ ) {
		sec = &sv_worldSectors[i];

		c = 0;
		for ( ent = sec->entities ; ent ; ent = ent->nextEntityInWorldSector ) {
			c++;
		}
		Com_Printf( "sector %i: %i entities\n", i, c );
	}
}

static worldSector_t *SV_CreateworldSector( int depth, vec3_t mins, vec3_t maxs ) {
	worldSector_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1, mins2, maxs2;

	anode = &sv_worldSectors[sv_numworldSectors];
	sv_numworldSectors++;

	if (depth == AREA_DEPTH) {
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}
	
	VectorSubtract (maxs, mins, size);
	if (size[0] > size[1]) {
		anode->axis = 0;
	} else {
		anode->axis = 1;
	}

	anode->dist = 0.5 * (maxs[anode->axis] + mins[anode->axis]);
	VectorCopy (mins, mins1);	
	VectorCopy (mins, mins2);	
	VectorCopy (maxs, maxs1);	
	VectorCopy (maxs, maxs2);	
	
	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;
	
	anode->children[0] = SV_CreateworldSector (depth+1, mins2, maxs2);
	anode->children[1] = SV_CreateworldSector (depth+1, mins1, maxs1);

	return anode;
}

void SV_ClearWorld( void ) {
	clipHandle_t	h;
	vec3_t			mins, maxs;
	int				i;
	int				num;
	char			name[ 16 ];

	Com_Memset( sv_worldSectors, 0, sizeof(sv_worldSectors) );
	sv_numworldSectors = 0;

	h = CM_InlineModel( 0 );
	CM_ModelBounds( h, mins, maxs );
	SV_CreateworldSector( 0, mins, maxs );

	num = CM_NumInlineModels();
	for( i = 1; i < num; i++ )
	{
		Com_sprintf( name, sizeof( name ), "*%i", i );
		SV_ModelIndex( name );
	}
}


void SV_UnlinkEntity( gentity_t *gEnt ) {
	svEntity_t		*ent;
	svEntity_t		*scan;
	worldSector_t	*ws;

	ent = SV_SvEntityForGentity( gEnt );

	gEnt->linked = qfalse;

	ws = ent->worldSector;
	if ( !ws ) {
		return;		
	}
	ent->worldSector = NULL;

	if ( ws->entities == ent ) {
		ws->entities = ent->nextEntityInWorldSector;
		return;
	}

	for ( scan = ws->entities ; scan ; scan = scan->nextEntityInWorldSector ) {
		if ( scan->nextEntityInWorldSector == ent ) {
			scan->nextEntityInWorldSector = ent->nextEntityInWorldSector;
			return;
		}
	}

	Com_Printf( "WARNING: SV_UnlinkEntity: not found in worldSector\n" );
}


#define MAX_TOTAL_ENT_LEAFS		128
void SV_LinkEntity( gentity_t *gEnt ) {
	worldSector_t	*node;
	int			leafs[MAX_TOTAL_ENT_LEAFS];
	int			cluster;
	int			num_leafs;
	int			i;
	int			area;
	int			lastLeaf;
	float		*origin;
	svEntity_t	*ent;

	ent = SV_SvEntityForGentity( gEnt );

	if ( ent->worldSector ) {
		SV_UnlinkEntity( gEnt );	
	}

	if ( !gEnt->inuse ) {
		return;
	}

	switch( gEnt->solid )
	{
	case SOLID_TRIGGER:
		gEnt->s.solid = 0;
		break;
	case SOLID_BBOX:
		if( !gEnt->contents || ( gEnt->contents == CONTENTS_SHOOTABLE_ONLY )
			|| ( gEnt->contents == CONTENTS_CORPSE ) )
		{
			gEnt->s.solid = 0;
		}
		else
		{
			gEnt->s.solid = BoundingBoxToInteger( gEnt->mins, gEnt->maxs );
		}
		break;
	case SOLID_BSP:
		if( gEnt->contents != -1
			&& ( ( gEnt->contents & CONTENTS_SHOOTABLE_ONLY ) || ( gEnt->contents & CONTENTS_CORPSE ) ) )
		{
			gEnt->s.solid = 0;
		}
		else
		{
			gEnt->s.solid = SOLID_BMODEL;
		}
		break;
	default:
		gEnt->s.solid = gEnt->solid;
		break;

	}

	origin = gEnt->s.origin;

	if( gEnt->solid != SOLID_BSP || ( !gEnt->currentAngles[ 0 ] && !gEnt->currentAngles[ 1 ] && !gEnt->currentAngles[ 2 ] ) ) {
		VectorAdd( gEnt->currentOrigin, gEnt->mins, gEnt->absmin );
		VectorAdd( gEnt->currentOrigin, gEnt->maxs, gEnt->absmax );
	} else {
		VectorCopy( gEnt->mins, gEnt->absmin );
		VectorCopy( gEnt->maxs, gEnt->absmax );
		CalculateRotatedBounds2( gEnt->s.mat, gEnt->absmin, gEnt->absmax );
		VectorAdd( origin, gEnt->absmin, gEnt->absmin );
		VectorAdd( origin, gEnt->absmax, gEnt->absmax );
	}

	gEnt->absmin[0] -= 1;
	gEnt->absmin[1] -= 1;
	gEnt->absmin[2] -= 1;
	gEnt->absmax[0] += 1;
	gEnt->absmax[1] += 1;
	gEnt->absmax[2] += 1;

	ent->numClusters = 0;
	ent->lastCluster = 0;
	ent->areanum = -1;
	ent->areanum2 = -1;

	num_leafs = CM_BoxLeafnums( gEnt->absmin, gEnt->absmax,
		leafs, MAX_TOTAL_ENT_LEAFS, &lastLeaf );

	if ( !num_leafs ) {
		return;
	}

	for (i=0 ; i<num_leafs ; i++) {
		area = CM_LeafArea (leafs[i]);
		if (area != -1) {
			if (ent->areanum != -1 && ent->areanum != area) {
				if (ent->areanum2 != -1 && ent->areanum2 != area && sv.state == SS_LOADING
					&& gEnt->s.parent == ENTITYNUM_NONE) {
					Com_WDPrintf ("Object %i touching 3 areas at %.2f %.2f %.2f with absmin %.2f %.2f %.2f and absmax %.2f %.2f %.2f\n",
						gEnt->s.number,
						gEnt->s.origin[ 0 ], gEnt->s.origin[ 1 ], gEnt->s.origin[ 2 ],
						gEnt->absmin[ 0 ], gEnt->absmin[ 1 ], gEnt->absmin[ 2 ],
						gEnt->absmax[ 0 ], gEnt->absmax[ 1 ], gEnt->absmax[ 2 ] );
				}
				ent->areanum2 = area;
			} else {
				ent->areanum = area;
			}
		}
	}

	gEnt->areanum = ent->areanum;

	ent->numClusters = 0;
	for (i=0 ; i < num_leafs ; i++) {
		cluster = CM_LeafCluster( leafs[i] );
		if ( cluster != -1 ) {
			ent->clusternums[ent->numClusters++] = cluster;
			if ( ent->numClusters == MAX_ENT_CLUSTERS ) {
				break;
			}
		}
	}

	if ( i != num_leafs ) {
		ent->lastCluster = CM_LeafCluster( lastLeaf );
	}

	gEnt->linkcount++;

	node = sv_worldSectors;
	while (1)
	{
		if (node->axis == -1)
			break;
		if( gEnt->absmin[ node->axis ] > node->dist )
			node = node->children[0];
		else if( gEnt->absmax[ node->axis ] < node->dist )
			node = node->children[1];
		else
			break;		
	}
	
	ent->worldSector = node;
	ent->nextEntityInWorldSector = node->entities;
	node->entities = ent;

	gEnt->linked = qtrue;
}


typedef struct {
	const float	*mins;
	const float	*maxs;
	int			*list;
	int			count, maxcount;
} areaParms_t;

static qboolean SV_TIKI_DoFullTrace( const dtiki_t *tiki );


static void SV_AreaEntities_r( worldSector_t *node, areaParms_t *ap, qboolean fullTrace ) {
	svEntity_t	*check, *next;
	gentity_t		*gcheck;
	vec3_t			mins, maxs;
	dtiki_t			*tiki;
	float			scale, radius;
	int				i;

	for( check = node->entities; check; check = next ) {
		next = check->nextEntityInWorldSector;

		gcheck = SV_GEntityForSvEntity( check );

		if ( !gcheck ) {
			continue;
		}

		if ( fullTrace ) {
			tiki = TIKI_Handle_For_ModelIndex( gcheck->s.modelindex );

			if ( tiki && SV_TIKI_DoFullTrace( tiki ) ) {
				scale = ( gcheck->s.scale != 0.0f ? gcheck->s.scale : 1.0f );

				TIKI_Full_Frame_Bounds( tiki,
					gcheck->s.anim, ge->GetEntityCurrentAnimFrame( gcheck->s.number, 0 ),
					gcheck->s.torso_anim, ge->GetEntityCurrentAnimFrame( gcheck->s.number, 1 ),
					scale, mins, maxs );

				radius = 0.0f;
				for ( i = 0 ; i < 2 ; i++ ) {
					if ( radius < ( float )fabs( mins[ i ] ) ) {
						radius = ( float )fabs( mins[ i ] );
					}
					if ( radius < ( float )fabs( maxs[ i ] ) ) {
						radius = ( float )fabs( maxs[ i ] );
					}
				}

				mins[ 0 ] = -radius;
				mins[ 1 ] = -radius;
				maxs[ 0 ] = radius;
				maxs[ 1 ] = radius;

				if ( gcheck->currentOrigin[ 0 ] + mins[ 0 ] > ap->maxs[ 0 ]
					|| gcheck->currentOrigin[ 1 ] + mins[ 1 ] > ap->maxs[ 1 ]
					|| gcheck->currentOrigin[ 2 ] + mins[ 2 ] > ap->maxs[ 2 ]
					|| gcheck->currentOrigin[ 0 ] + maxs[ 0 ] < ap->mins[ 0 ]
					|| gcheck->currentOrigin[ 1 ] + maxs[ 1 ] < ap->mins[ 1 ]
					|| gcheck->currentOrigin[ 2 ] + maxs[ 2 ] < ap->mins[ 2 ] ) {
					continue;
				}

				if( ap->count == ap->maxcount ) {
					Com_WPrintf( "SV_AreaEntities: MAXCOUNT\n" );
					return;
				}

				ap->list[ ap->count ] = check - sv.svEntities;
				ap->count++;
				continue;
			}
		}

		if( gcheck->absmin[ 0 ] > ap->maxs[ 0 ]
			|| gcheck->absmin[ 1 ] > ap->maxs[ 1 ]
			|| gcheck->absmin[ 2 ] > ap->maxs[ 2 ]
			|| gcheck->absmax[ 0 ] < ap->mins[ 0 ]
			|| gcheck->absmax[ 1 ] < ap->mins[ 1 ]
			|| gcheck->absmax[ 2 ] < ap->mins[ 2 ] ) {
			continue;
		}

		if( ap->count == ap->maxcount ) {
			Com_WPrintf( "SV_AreaEntities: MAXCOUNT\n" );
			return;
		}

		ap->list[ ap->count ] = check - sv.svEntities;
		ap->count++;
	}

	if ( node->axis != -1 ) {
		if ( ap->maxs[ node->axis ] > node->dist ) {
			SV_AreaEntities_r( node->children[ 0 ], ap, fullTrace );
		}

		if ( ap->mins[ node->axis ] < node->dist ) {
			SV_AreaEntities_r( node->children[ 1 ], ap, fullTrace );
		}
	}
}

int SV_AreaEntities( const vec3_t mins, const vec3_t maxs, int *entityList, int maxcount, qboolean fullTrace ) {
	areaParms_t		ap;

	ap.mins = mins;
	ap.maxs = maxs;
	ap.list = entityList;
	ap.count = 0;
	ap.maxcount = maxcount;

	SV_AreaEntities_r( sv_worldSectors, &ap, fullTrace );

	return ap.count;
}





typedef struct {
	vec3_t		boxmins, boxmaxs;
	const float	*mins;
	const float *maxs;	
	const float	*start;
	vec3_t		end;
	trace_t		trace;
	int			passEntityNum;
	int			contentmask;
	qboolean	cylinder;

	qboolean	fulltrace;
} moveclip_t;



static int SV_TIKI_GetBoneMappings( const dtiki_t *tiki, const tikiBoneMapping_t **mappings ) {
	*mappings = NULL;

	if ( !tiki ) {
		return 0;
	}

	*mappings = tiki->bone_mappings;
	return tiki->num_bone_mappings;
}

static qboolean SV_TIKI_DoFullTrace( const dtiki_t *tiki ) {
	const tikiBoneMapping_t *mappings;

	if ( !tiki ) {
		return qfalse;
	}

	if ( tiki->numMeshes <= 0 || tiki->mesh[0] < 0 ) {
		return qfalse;
	}

	return SV_TIKI_GetBoneMappings( tiki, &mappings ) != 0;
}

static void SV_TIKI_BoneOrientation( const dtiki_t *tiki, int handle, const gentity_t *touch,
									 int bone, orientation_t *orient ) {
	const int animBlend  = ( 1 << 10 );								 
	const int animMask   = ~( ( 1 << 9 ) | ( 1 << 10 ) | ( 1 << 12 ) );	 
	const int frameMask  = ~( 1 << 13 );							 

	TIKI_Tag_OrientationEx( orient, handle,
							touch->s.anim & animMask,
							touch->s.frame & frameMask,
							bone,
							touch->s.scale,
							touch->s.bone_tag,
							touch->s.bone_quat,
							0, 0, 1.0f,
							( qboolean )( ( touch->s.anim & animBlend ) != 0 ),
							( qboolean )( ( touch->s.torso_anim & animBlend ) != 0 ),
							touch->s.torso_anim & animMask,
							touch->s.torso_frame & frameMask,
							0, 0, 1.0f );
}

static void SV_TIKI_Intersect( const dtiki_t *tiki, const moveclip_t *clip, trace_t *trace,
							   const gentity_t *touch, int contentmask ) {
	const tikiBoneMapping_t	*mappings;
	int						numMappings;
	int						handle;
	int						i;
	int						best;
	float					bestFraction;
	vec3_t					entAxis[3];
	vec3_t					boneOrigins[TIKI_MAX_BONES];

	numMappings = SV_TIKI_GetBoneMappings( tiki, &mappings );
	if ( numMappings <= 0 ) {
		return;
	}

	handle = TIKI_HandleForTiki( tiki );

	AnglesToAxis( touch->s.angles, entAxis );

	for ( i = 0 ; i < numMappings ; i++ ) {
		const tikiBoneMapping_t	*map = &mappings[i];
		orientation_t			orient;
		int						b;

		for ( b = 0 ; b < 2 ; b++ ) {
			int bone = b ? map->parentIndex : map->boneIndex;

			if ( bone < 0 || bone >= TIKI_MAX_BONES ) {
				continue;
			}

			SV_TIKI_BoneOrientation( tiki, handle, touch, bone, &orient );

			VectorCopy( touch->s.origin, boneOrigins[bone] );
			VectorMA( boneOrigins[bone], orient.origin[0], entAxis[0], boneOrigins[bone] );
			VectorMA( boneOrigins[bone], orient.origin[1], entAxis[1], boneOrigins[bone] );
			VectorMA( boneOrigins[bone], orient.origin[2], entAxis[2], boneOrigins[bone] );
		}
	}

	best = -1;
	bestFraction = 1.0f;

	for ( i = 0 ; i < numMappings ; i++ ) {
		const tikiBoneMapping_t	*map = &mappings[i];
		trace_t					boneTrace;
		vec3_t					dir;
		vec3_t					boneAngles;
		vec3_t					boxMins, boxMaxs;
		clipHandle_t			boxModel;
		float					length;

		if ( map->radius <= 0.0f || map->parentIndex < 0 || map->boneIndex < 0 ) {
			continue;
		}
		if ( map->boneIndex >= TIKI_MAX_BONES || map->parentIndex >= TIKI_MAX_BONES ) {
			continue;
		}

		VectorSubtract( boneOrigins[map->boneIndex], boneOrigins[map->parentIndex], dir );
		length = map->length + VectorLength( dir );

		vectoangles( dir, boneAngles );
		boneAngles[PITCH] += 90.0f;

		boxMins[0] = -map->radius;
		boxMins[1] = -map->radius;
		boxMins[2] = 0.0f;
		boxMaxs[0] = map->radius;
		boxMaxs[1] = map->radius;
		boxMaxs[2] = length;

		boxModel = CM_TempBoxModel( boxMins, boxMaxs, contentmask );

		CM_TransformedBoxTrace( &boneTrace, clip->start, clip->end,
			clip->mins, clip->maxs, boxModel, clip->contentmask,
			boneOrigins[map->parentIndex], boneAngles, qfalse );

		if ( boneTrace.fraction >= bestFraction ) {
			if ( boneTrace.startsolid ) {
				bestFraction = 0.0f;
				best = i;
			}
		} else {
			bestFraction = boneTrace.fraction;
			best = i;
		}
	}

	if ( best < 0 ) {
		trace->fraction = 1.0f;
		return;
	}

	trace->fraction					= bestFraction;
	trace->intersect.valid			= qtrue;
	trace->intersect.surface		= mappings[best].surfaceIndex;
	trace->intersect.bone			= mappings[best].boneIndex;

	if ( tiki->surfaces && mappings[best].surfaceIndex >= 0
		&& mappings[best].surfaceIndex < tiki->num_surfaces ) {
		trace->intersect.damage_multiplier =
			tiki->surfaces[ mappings[best].surfaceIndex ].damage_multiplier;
	}
}

static clipHandle_t SV_TIKI_FullTraceBounds( const dtiki_t *tiki, const gentity_t *touch ) {
	vec3_t	mins, maxs;
	float	scale;
	float	m;
	int		i;

	if ( !tiki->a ) {
		return CM_TempBoxModel( touch->mins, touch->maxs, touch->contents );
	}

	scale = ( touch->s.scale != 0.0f ? touch->s.scale : 1.0f );

	TIKI_Full_Frame_Bounds( (dtiki_t *)tiki,
		touch->s.anim, ge->GetEntityCurrentAnimFrame( touch->s.number, 0 ),
		touch->s.torso_anim, ge->GetEntityCurrentAnimFrame( touch->s.number, 1 ),
		scale, mins, maxs );

	if ( VectorCompare( mins, vec3_origin ) && VectorCompare( maxs, vec3_origin ) ) {
		scale = tiki->load_scale * scale;

		for ( i = 0 ; i < 3 ; i++ ) {
			mins[i] = tiki->a->mins[i] * scale;
			maxs[i] = tiki->a->maxs[i] * scale;
		}
	}

	m = 0.0f;
	if ( m <= (float)abs( (int)mins[0] ) ) { m = (float)abs( (int)mins[0] ); }
	if ( m <= (float)abs( (int)mins[1] ) ) { m = (float)abs( (int)mins[1] ); }
	if ( m <= (float)abs( (int)maxs[0] ) ) { m = (float)abs( (int)maxs[0] ); }
	if ( m <= (float)abs( (int)maxs[1] ) ) { m = (float)abs( (int)maxs[1] ); }

	mins[0] = mins[1] = -m;
	maxs[0] = maxs[1] = m;

	return CM_TempBoxModel( mins, maxs, touch->contents );
}


void SV_ClipToEntity( trace_t *trace, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int entityNum, int contentmask ) {
	gentity_t		*touch;
	clipHandle_t	clipHandle;
	const float		*origin, *angles;

	touch = SV_GentityNum( entityNum );

	Com_Memset(trace, 0, sizeof(trace_t));

	if ( ! ( contentmask & touch->contents ) ) {
		trace->fraction = 1.0;
		return;
	}

	if ( !touch->bmodel || touch->solid == SOLID_BBOX ) {
		origin = touch->currentOrigin;
		angles = vec3_origin;
	} else {
		origin = touch->currentOrigin;
		angles = touch->currentAngles;
	}

	clipHandle = SV_ClipHandleForEntity (touch);

	CM_TransformedBoxTrace ( trace, start, end,
		mins, maxs, clipHandle,  contentmask,
		origin, angles, qfalse );

	if ( trace->fraction < 1 ) {
		trace->entityNum = touch->s.number;
	}
}


static void SV_ClipMoveToEntities( moveclip_t *clip ) {
	int			i, num;
	int			touchlist[MAX_GENTITIES];
	gentity_t	*touch;
	int			passOwnerNum;
	trace_t		trace;
	clipHandle_t	clipHandle;
	dtiki_t		*tiki;
	const float	*angles;

	num = SV_AreaEntities( clip->boxmins, clip->boxmaxs, touchlist, MAX_GENTITIES, clip->fulltrace );

	if ( clip->passEntityNum != ENTITYNUM_NONE ) {
		passOwnerNum = ( SV_GentityNum( clip->passEntityNum ) )->ownerNum;
		if ( passOwnerNum == ENTITYNUM_NONE ) {
			passOwnerNum = -1;
		}
	} else {
		passOwnerNum = -1;
	}

	for ( i=0 ; i<num ; i++ ) {
		if ( clip->trace.allsolid ) {
			return;
		}
		touch = SV_GentityNum( touchlist[i] );

		if( touch->solid == SOLID_NOT ) {
			continue;
		}

		if( touchlist[i] == ENTITYNUM_WORLD ) {
			continue;
		}

		if( touch->solid == SOLID_TRIGGER && !( clip->contentmask & touch->contents & CONTENTS_TARGETABLE ) ) {
			continue;
		}

		if ( clip->passEntityNum != ENTITYNUM_NONE ) {
			if ( touchlist[i] == clip->passEntityNum ) {
				continue;	
			}
			if (touchlist[i] == passOwnerNum) {
				continue; 
			}
			if ( touch->ownerNum == clip->passEntityNum ) {
				continue;	
			}
			if ( touch->ownerNum == passOwnerNum ) {
				continue;	
			}
		}

		if ( ! ( clip->contentmask & touch->contents ) ) {
			continue;
		}

		{
			qboolean	fullTraceEnt;

			tiki = TIKI_Handle_For_ModelIndex( touch->s.modelindex );

			fullTraceEnt = clip->fulltrace && tiki != NULL
						&& SV_TIKI_DoFullTrace( tiki );

			if ( fullTraceEnt ) {
				clipHandle = SV_TIKI_FullTraceBounds( tiki, touch );
			} else if ( !touch->bmodel || touch->solid == SOLID_BBOX ) {
				clipHandle = SV_ClipHandleForEntity( touch );
			} else {
				clipHandle = CM_InlineModel( touch->s.modelindex );
			}

			if ( !touch->bmodel || touch->solid == SOLID_BBOX ) {
				angles = vec3_origin;
			} else {
				angles = touch->currentAngles;
			}

			CM_TransformedBoxTrace( &trace, clip->start, clip->end,
				clip->mins, clip->maxs, clipHandle, clip->contentmask,
				touch->currentOrigin, angles, clip->cylinder );

			if ( fullTraceEnt && ( trace.fraction < 1.0f || trace.startsolid ) ) {
				int		saveFrame		= touch->s.frame;
				int		saveTorsoFrame	= touch->s.torso_frame;

				touch->s.frame			= ge->GetEntityCurrentAnimFrame( touch->s.number, 0 );
				touch->s.torso_frame	= ge->GetEntityCurrentAnimFrame( touch->s.number, 1 );

				SV_TIKI_Intersect( tiki, clip, &trace, touch, touch->contents );

				touch->s.frame			= saveFrame;
				touch->s.torso_frame	= saveTorsoFrame;
			}
		}

		if ( trace.allsolid ) {
			clip->trace.allsolid = qtrue;
			clip->trace.entityNum = touch->s.number;
			trace.entityNum = touch->s.number;
		} else if ( trace.startsolid ) {
			clip->trace.startsolid = qtrue;
			clip->trace.entityNum = touch->s.number;
			trace.entityNum = touch->s.number;
		}

		if ( trace.fraction < clip->trace.fraction ) {
			qboolean	oldStart;

			oldStart = clip->trace.startsolid;

			trace.entityNum = touch->s.number;
			clip->trace = trace;
			clip->trace.startsolid |= oldStart;
		}
	}
}


static void SV_RealTrace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask, qboolean cylinder, qboolean fulltrace ) {
	moveclip_t	clip;
	int			i;

	if( !mins ) {
		mins = vec3_origin;
	}
	if( !maxs ) {
		maxs = vec3_origin;
	}

	Com_Memset ( &clip, 0, sizeof ( moveclip_t ) );

	CM_BoxTrace( &clip.trace, start, end, mins, maxs, 0, contentmask, cylinder );
	clip.trace.entityNum = clip.trace.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;

	if ( clip.trace.startsolid ) {
		clip.trace.entityNum = ENTITYNUM_WORLD;
	}

	if ( clip.trace.fraction == 0 ) {
		*results = clip.trace;
		return;		
	}

	clip.contentmask = contentmask;
	clip.start = start;
	VectorCopy( end, clip.end );
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passEntityNum = passEntityNum;
	clip.cylinder = cylinder;
	clip.fulltrace = fulltrace;

	for ( i=0 ; i<3 ; i++ ) {
		if ( end[i] > start[i] ) {
			clip.boxmins[i] = clip.start[i] + clip.mins[i] - 1;
			clip.boxmaxs[i] = clip.end[i] + clip.maxs[i] + 1;
		} else {
			clip.boxmins[i] = clip.end[i] + clip.mins[i] - 1;
			clip.boxmaxs[i] = clip.start[i] + clip.maxs[i] + 1;
		}
	}

	SV_ClipMoveToEntities( &clip );

	*results = clip.trace;
}

void SV_Trace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask, qboolean cylinder ) {
	SV_RealTrace( results, start, mins, maxs, end, passEntityNum, contentmask, cylinder, qfalse );

	results->intersect.surface = -1;
	results->intersect.bone = -1;
}

void SV_FullTrace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask, qboolean cylinder ) {
	SV_RealTrace( results, start, mins, maxs, end, passEntityNum, contentmask, cylinder, qtrue );

	if ( !results->intersect.valid ) {
		results->intersect.surface = -1;
		results->intersect.bone = -1;
	}
}

dshader_t *SV_GetShaderPointer( int iShaderNum )
{
	return CM_ShaderPointer( iShaderNum );
}

int SV_PointContents( const vec3_t p, int passEntityNum ) {
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	int			i, num;
	int			contents, c2;
	clipHandle_t	clipHandle;

	contents = CM_PointContents( p, 0 );

	num = SV_AreaEntities( p, p, touch, MAX_GENTITIES, qfalse );

	for ( i=0 ; i<num ; i++ ) {
		if ( touch[i] == passEntityNum ) {
			continue;
		}
		hit = SV_GentityNum( touch[i] );

		if ( !hit->contents ) {
			continue;
		}

		clipHandle = SV_ClipHandleForEntity( hit );

		c2 = CM_TransformedPointContents (p, clipHandle, hit->s.origin, hit->s.angles);

		contents |= c2;
	}

	return contents;
}


