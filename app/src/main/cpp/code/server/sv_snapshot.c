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
#include "../client/client.h"
#include "../uilib/ui_public.h" 


static void SV_EmitPacketEntities( clientSnapshot_t *from, clientSnapshot_t *to, msg_t *msg ) {
	entityState_t	*oldent, *newent;
	int		oldindex, newindex;
	int		oldnum, newnum;
	int		from_num_entities;

	if ( !from ) {
		from_num_entities = 0;
	} else {
		from_num_entities = from->num_entities;
	}

	newent = NULL;
	oldent = NULL;
	newindex = 0;
	oldindex = 0;
	while ( newindex < to->num_entities || oldindex < from_num_entities ) {
		if ( newindex >= to->num_entities ) {
			newnum = 9999;
		} else {
			newent = &svs.snapshotEntities[(to->first_entity+newindex) % svs.numSnapshotEntities];
			newnum = newent->number;
		}

		if ( oldindex >= from_num_entities ) {
			oldnum = 9999;
		} else {
			oldent = &svs.snapshotEntities[(from->first_entity+oldindex) % svs.numSnapshotEntities];
			oldnum = oldent->number;
		}

		if ( newnum == oldnum ) {
			MSG_WriteDeltaEntity (msg, oldent, newent, qfalse);
			oldindex++;
			newindex++;
			continue;
		}

		if ( newnum < oldnum ) {
			MSG_WriteDeltaEntity (msg, &sv.svEntities[newnum].baseline, newent, qtrue);
			newindex++;
			continue;
		}

		if ( newnum > oldnum ) {
			MSG_WriteDeltaEntity (msg, oldent, NULL, qtrue);
			oldindex++;
			continue;
		}
	}

	MSG_WriteEntityNum(msg, (MAX_GENTITIES - 1));	
}



static void SV_WriteSnapshotToClient( client_t *client, msg_t *msg ) {
	clientSnapshot_t	*frame, *oldframe;
	int					lastframe;
	int					i;
	int					snapFlags;

	frame = &client->frames[ client->netchan.outgoingSequence & PACKET_MASK ];

	if ( client->deltaMessage <= 0 || client->state != CS_ACTIVE ) {
		oldframe = NULL;
		lastframe = 0;
	} else if ( client->netchan.outgoingSequence - client->deltaMessage 
		>= (PACKET_BACKUP - 3) ) {
		Com_DPrintf ("%s: Delta request from out of date packet.\n", client->name);
		oldframe = NULL;
		lastframe = 0;
	} else {
		oldframe = &client->frames[ client->deltaMessage & PACKET_MASK ];
		lastframe = client->netchan.outgoingSequence - client->deltaMessage;

		if ( oldframe->first_entity <= svs.nextSnapshotEntities - svs.numSnapshotEntities ) {
			Com_DPrintf ("%s: Delta request from out of date entities.\n", client->name);
			oldframe = NULL;
			lastframe = 0;
		}
	}

	MSG_WriteSVC(msg, svc_snapshot);


	MSG_WriteLong (msg, svs.time);

	if ( sv.timeResidual > 254 )
		MSG_WriteByte( msg, 255 );
	else MSG_WriteByte( msg, sv.timeResidual );

	MSG_WriteByte (msg, lastframe);

	snapFlags = svs.snapFlagServerBit;
	if ( client->rateDelayed ) {
		snapFlags |= SNAPFLAG_RATE_DELAYED;
	}
	if ( client->state != CS_ACTIVE ) {
		snapFlags |= SNAPFLAG_NOT_ACTIVE;
	}

	MSG_WriteByte (msg, snapFlags);

	MSG_WriteByte (msg, frame->areabytes);
	MSG_WriteData (msg, frame->areabits, frame->areabytes);

	if ( oldframe ) {
		MSG_WriteDeltaPlayerstate( msg, &oldframe->ps, &frame->ps );
	} else {
		MSG_WriteDeltaPlayerstate( msg, NULL, &frame->ps );
	}

	SV_EmitPacketEntities (oldframe, frame, msg);

	MSG_WriteSounds( msg, client->server_sounds, client->number_of_server_sounds );

	MSG_WriteWorldInfo( msg, &client->ws, &sv.worldState );

	if ( client->stringToPrint[0] ) {
		if ( client->locprint ) {
			MSG_WriteSVC( msg, svc_locprint );
			MSG_WriteShort( msg, client->XOffset );
			MSG_WriteShort( msg, client->YOffset );
			MSG_WriteString( msg, client->stringToPrint );
			client->locprint = qfalse;
		}
		else {
			MSG_WriteSVC( msg, svc_centerprint );
			MSG_WriteString( msg, client->stringToPrint);
		}
	}

	if ( sv_padPackets->integer ) {
		for ( i = 0 ; i < sv_padPackets->integer ; i++ ) {
			MSG_WriteSVC(msg, svc_nop);
		}
	}
}


// SV_ExecuteClientMessage), never here.  No scrambled strings (retail sends


#define	MAX_SNAPSHOT_ENTITIES	1024
typedef struct {
	int		numSnapshotEntities;
	int		snapshotEntities[MAX_SNAPSHOT_ENTITIES];	
} snapshotEntityNumbers_t;

static int QDECL SV_QsortEntityNumbers( const void *a, const void *b ) {
	int	*ea, *eb;

	ea = (int *)a;
	eb = (int *)b;

	if ( *ea == *eb ) {
		Com_Error( ERR_DROP, "SV_QsortEntityStates: duplicated entity" );
	}

	if ( *ea < *eb ) {
		return -1;
	}

	return 1;
}

static qboolean SV_ClipMoveToBSPEntities(const vec3_t start, const vec3_t end, int mask) {
	int				i, num;
	int				touchlist[MAX_GENTITIES];
	gentity_t		*touch;
	trace_t			trace;
	clipHandle_t	clipHandle;
	vec3_t			boxmins, boxmaxs;

	for (i = 0; i < 3; i++) {
		if (end[i] > start[i]) {
			boxmins[i] = start[i] - 1;
			boxmaxs[i] = end[i] + 1;
		}
		else {
			boxmins[i] = end[i] - 1;
			boxmaxs[i] = start[i] + 1;
		}
	}

	num = SV_AreaEntities(boxmins, boxmaxs, touchlist, MAX_GENTITIES, qfalse);

	for (i = 0; i < num; i++) {
		touch = SV_GentityNum(touchlist[i]);

		if (touch->solid != SOLID_BSP) {
			continue;
		}

		if (!(mask & touch->contents)) {
			continue;
		}

		clipHandle = SV_ClipHandleForEntity(touch);

		CM_TransformedBoxTrace(&trace, start, end,
			vec3_origin, vec3_origin, clipHandle, mask,
			touch->s.origin, touch->currentAngles, qfalse);

		if (trace.allsolid || trace.startsolid || trace.fraction != 1) {
			return qfalse;
		}
	}

	return qtrue;
}

qboolean SV_WorldTrace(const vec3_t start, const vec3_t end, int mask)
{
	trace_t trace = { 0 };

    CM_BoxTrace(&trace, start, end, vec3_origin, vec3_origin, 0, mask, qfalse);
	if (trace.fraction != 1) {
		return qfalse;
	}

	return SV_ClipMoveToBSPEntities(start, end, mask);
}

static void SV_AddEntToSnapshot( gentity_t *clientEnt, svEntity_t *svEnt, gentity_t *gEnt, snapshotEntityNumbers_t *eNums, svEntity_t* portalEnt, qboolean portalsky) {
	// are different bits that retail leaves untouched on this path, and it never set
	if ( svEnt->snapshotCounter == sv.snapshotCounter ) {
		gEnt->s.renderfx = (gEnt->s.renderfx & ~(RF_PORTALENTITY | RF_DUALENTITY)) | RF_DUALENTITY;
		return;
	}
	svEnt->snapshotCounter = sv.snapshotCounter;

	if ( eNums->numSnapshotEntities == MAX_SNAPSHOT_ENTITIES ) {
		return;
	}

	if ( ge->SendEntity && !ge->SendEntity( clientEnt, gEnt ) ) {
		return;
	}

	gEnt->s.renderfx &= ~(RF_SKYENTITY | RF_PORTALENTITY | RF_DUALENTITY);

	if ( portalEnt ) {
		gEnt->s.renderfx |= RF_PORTALENTITY;
	} else if ( portalsky ) {
		gEnt->s.renderfx |= RF_SKYENTITY;
	}

	eNums->snapshotEntities[ eNums->numSnapshotEntities ] = gEnt->s.number;
	eNums->numSnapshotEntities++;
}


static void SV_AddEntitiesVisibleFromPoint(gentity_t *clientEnt, const vec3_t origin, clientSnapshot_t* frame, snapshotEntityNumbers_t* eNums, svEntity_t* portalEnt, qboolean portalsky ) {
	int		e, i;
	gentity_t *ent;
	gentity_t *parentEnt;
	svEntity_t	*svEnt, *svCheckEnt;
	int		l;
	int		clientarea, clientcluster;
	int		leafnum;
	byte	*clientpvs;
	byte	*bitvector;
	gentity_t* skyorigin = NULL;
	if ( !sv.state ) {
		return;
	}


	leafnum = CM_PointLeafnum (origin);
	clientarea = CM_LeafArea (leafnum);
	if ( clientarea == -1 ) {
		clientarea = clientEnt->areanum;
	}
	clientcluster = CM_LeafCluster (leafnum);

	frame->areabytes = CM_WriteAreaBits( frame->areabits, clientarea );

	clientpvs = CM_ClusterPVS (clientcluster);

	for ( e = 0 ; e < sv.num_entities ; e++ ) {
		ent = SV_GentityNum(e);

		if (!ent->inuse) {
			continue;
		}
		if (ent->s.number != e) {
			Com_WDPrintf("FIXING ENT->S.NUMBER!!!");
			ent->s.number = e;
		}

		ent->svflags |= SVF_SENT;

		if ( !ent->linked ) {
			continue;
		}
		if ( ent->svflags & SVF_NOCLIENT ) {
			continue;
		}

		parentEnt = NULL;
		if (ent->s.parent != ENTITYNUM_NONE) {
			parentEnt = SV_GentityNum(ent->s.parent);
		} else if (ent->s.bindparent != ENTITYNUM_NONE) {
			parentEnt = SV_GentityNum(ent->s.bindparent);
		}

		if (parentEnt) {
			if ((parentEnt->svflags & SVF_NOCLIENT)
				|| ((parentEnt->s.renderfx & RF_DONTDRAW) && parentEnt->s.solid)) {
				continue;
			}
		}

		svEnt = SV_SvEntityForGentity( ent );

		// snapshotCounter test that stood here is ioq3's, not retail's.  Retail
		if (ent->s.renderfx & RF_SKYORIGIN) {
			if (sv.skyportal && !portalsky && !portalEnt) {
				if (skyorigin) {
					Com_Error(ERR_DROP, "SV_AddEntitiesVisibleFromPoint: duplicate sky origin");
				}
				skyorigin = ent;
				SV_AddEntToSnapshot(clientEnt, svEnt, ent, eNums, NULL, qfalse);
			}
			continue;
		}

		if ( (ent->svflags & SVF_BROADCAST) || (ent->svflags & SVF_SENDONCE)  ) {
			SV_AddEntToSnapshot( clientEnt, svEnt, ent, eNums, portalEnt, portalsky);
			continue;
		}

		if (parentEnt) {
			svEntity_t* parentSvEnt = SV_SvEntityForGentity(parentEnt);
			if (parentSvEnt->snapshotCounter == sv.snapshotCounter) {
				SV_AddEntToSnapshot(clientEnt, svEnt, ent, eNums, portalEnt, portalsky);
				continue;
			}

			svCheckEnt = parentSvEnt;
		} else {
			svCheckEnt = svEnt;
		}

		if (!(ent->svflags & SVF_SENDPVS) && !ent->s.modelindex && !ent->s.loopSound) {
			continue;
		}

		// stood here went with RF_ALWAYSDRAW, a MOHAA-only bit retail does not have.
		if (ent->s.loopSound && ent->s.loopSoundMinDist == LEVEL_WIDE_MIN_DIST) {
			SV_AddEntToSnapshot(clientEnt, svEnt, ent, eNums, portalEnt, portalsky);
            continue;
		}

		if ( !CM_AreasConnected( clientarea, svCheckEnt->areanum ) ) {
			if ( !CM_AreasConnected( clientarea, svCheckEnt->areanum2 ) ) {
				continue;		
			}
		}

		if (sv.farplane && sv.farplane_cull) {
			const gentity_t *distEnt = parentEnt ? parentEnt : ent;
			vec3_t           dir;
			float            distSq;

			VectorSubtract(distEnt->centroid, origin, dir);
			distSq = DotProduct(dir, dir);
			if (distSq - distEnt->radius * distEnt->radius > (float)sv.farplane) {
				continue;
			}
		}


		bitvector = clientpvs;

		l = 0;
		for ( i=0 ; i < svCheckEnt->numClusters ; i++ ) {
			l = svCheckEnt->clusternums[i];
			if ( bitvector[l >> 3] & (1 << (l&7) ) ) {
				break;
			}
		}

		if ( i == svCheckEnt->numClusters ) {
			if ( svCheckEnt->lastCluster ) {
				for ( ; l <= svCheckEnt->lastCluster ; l++ ) {
					if ( bitvector[l >> 3] & (1 << (l&7) ) ) {
						break;
					}
				}
				if ( l == svCheckEnt->lastCluster ) {
					continue;	
				}
			} else {
				continue;
			}
		}

		// not what retail writes, and running it here as well as inside SV_AddEntToSnapshot

		SV_AddEntToSnapshot( clientEnt, svEnt, ent, eNums, portalEnt, portalsky);

		if ( ent->svflags & SVF_PORTAL && svEnt != portalEnt ) {
			SV_AddEntitiesVisibleFromPoint( clientEnt, ent->s.origin2, frame, eNums, svEnt, qfalse );
		}
	}

	if (!portalsky && skyorigin && !portalEnt) {
		SV_AddEntitiesVisibleFromPoint(clientEnt, skyorigin->s.origin, frame, eNums, NULL, qtrue);
	}
}


static FILE *sv_orl_es_fp;
static unsigned sv_orl_es_frame;

void SV_OracleLogEntityState( const entityState_t *s, int slot )
{
	if ( !sv_orl_es_fp ) {
		const char *path = getenv( "EF2_ORACLE_ES" );

		if ( !path || !*path ) {
			return;                    
		}
		sv_orl_es_fp = fopen( path, "a" );
		if ( !sv_orl_es_fp ) {
			return;
		}
		fprintf( sv_orl_es_fp, "# EF2 runtime oracle | port entityState | provenanceef2 port\n" );
	}

	fprintf( sv_orl_es_fp,
			 "# port es f=%u ent=%d client=%d etype=0x%08x eflags=0x%08x"
			 " postype=%d postime=%d posdur=%d"
			 " trbase=0x%08x,0x%08x,0x%08x trdelta=0x%08x,0x%08x,0x%08x"
			 " origin=0x%08x,0x%08x,0x%08x netorigin=0x%08x,0x%08x,0x%08x"
			 " angles=0x%08x,0x%08x,0x%08x netangles=0x%08x,0x%08x,0x%08x"
			 " viewangles=0x%08x,0x%08x,0x%08x"
			 " model=%d anim=0x%08x frame=0x%08x torso=0x%08x tframe=0x%08x"
			 " parent=%d tag=%d ground=%d solid=%u"
			 " scale=0x%08x alpha=0x%08x renderfx=0x%08x eff0=%d"
			 " inst=%d slot=%d\n",
			 sv_orl_es_frame, s->number, s->clientNum, s->eType, s->eFlags,
			 s->pos.trType, s->pos.trTime, s->pos.trDuration,
			 *(unsigned *)&s->pos.trBase[0], *(unsigned *)&s->pos.trBase[1],
			 *(unsigned *)&s->pos.trBase[2],
			 *(unsigned *)&s->pos.trDelta[0], *(unsigned *)&s->pos.trDelta[1],
			 *(unsigned *)&s->pos.trDelta[2],
			 *(unsigned *)&s->origin[0], *(unsigned *)&s->origin[1],
			 *(unsigned *)&s->origin[2],
			 *(unsigned *)&s->netorigin[0], *(unsigned *)&s->netorigin[1],
			 *(unsigned *)&s->netorigin[2],
			 *(unsigned *)&s->angles[0], *(unsigned *)&s->angles[1],
			 *(unsigned *)&s->angles[2],
			 *(unsigned *)&s->netangles[0], *(unsigned *)&s->netangles[1],
			 *(unsigned *)&s->netangles[2],
			 *(unsigned *)&s->viewangles[0], *(unsigned *)&s->viewangles[1],
			 *(unsigned *)&s->viewangles[2],
			 s->modelindex, s->anim, s->frame, s->torso_anim, s->torso_frame,
			 s->parent, s->tag_num, s->groundEntityNum, s->solid,
			 *(unsigned *)&s->scale, *(unsigned *)&s->alpha, s->renderfx,
			 s->effectsAnims[0], s->instanceNumber, slot );

	if ( ( sv_orl_es_frame & 31 ) == 0 ) {
		fflush( sv_orl_es_fp );
	}
}

static void SV_BuildClientSnapshot( client_t *client ) {
	vec3_t						org;
	clientSnapshot_t			*frame;
	snapshotEntityNumbers_t		entityNumbers;
	int							i;
	gentity_t					*ent;
	entityState_t				*state;
	svEntity_t					*svEnt;
	gentity_t					*clent;
	gentity_t					*clientEnt;
	int							clientNum;
	playerState_t				*ps;

	sv.snapshotCounter++;

	frame = &client->frames[ client->netchan.outgoingSequence & PACKET_MASK ];

	entityNumbers.numSnapshotEntities = 0;
	Com_Memset( frame->areabits, 0, sizeof( frame->areabits ) );

	frame->num_entities = 0;
	
	clent = client->gentity;
	if ( !clent || client->state == CS_ZOMBIE ) {
		return;
	}

	clientEnt = clent;

	ps = SV_GameClientNum( client - svs.clients );
	frame->ps = *ps;

	clientNum = frame->ps.clientNum;
	if ( clientNum < 0 || clientNum >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "SV_SvEntityForGentity: bad gEnt" );
	}
	svEnt = &sv.svEntities[ clientNum ];
	

	if (ps->pm_flags & PMF_CAMERA_VIEW)
	{
		VectorCopy(ps->camera_origin, org);
	}
	else
	{
		VectorCopy(ps->origin, org);
		org[2] += ps->viewheight;
	}

	if ( ge->UpdatePlayerStateForClient ) {
		ge->UpdatePlayerStateForClient( clientEnt, &frame->ps );
	}

	SV_AddEntToSnapshot(clientEnt, svEnt, SV_GentityNum(client - svs.clients), &entityNumbers, NULL, qfalse);

	SV_AddEntitiesVisibleFromPoint( clientEnt, org, frame, &entityNumbers, NULL, qfalse );

	if ( ge->ExtraEntitiesToSend ) {
		int extraEntities[MAX_EXTRA_ENTITIES_FROM_GAME];
		int numExtraEntities = 0;

		ge->ExtraEntitiesToSend( clent, &numExtraEntities, extraEntities );

		if ( numExtraEntities > MAX_EXTRA_ENTITIES_FROM_GAME ) {
			numExtraEntities = MAX_EXTRA_ENTITIES_FROM_GAME;
		}

		for ( i = 0 ; i < numExtraEntities ; i++ ) {
			int extraNum = extraEntities[i];

			if ( extraNum < 0 || extraNum >= MAX_GENTITIES ) {
				continue;
			}

			ent = SV_GentityNum( extraNum );

			if ( !ent->inuse ) {
				continue;
			}

			SV_AddEntToSnapshot( clientEnt, SV_SvEntityForGentity( ent ), ent, &entityNumbers, NULL, qfalse );
		}
	}

	qsort( entityNumbers.snapshotEntities, entityNumbers.numSnapshotEntities, 
		sizeof( entityNumbers.snapshotEntities[0] ), SV_QsortEntityNumbers );

	for ( i = 0 ; i < MAX_MAP_AREA_BYTES/4 ; i++ ) {
		((int *)frame->areabits)[i] = ((int *)frame->areabits)[i] ^ -1;
	}

	
	frame->num_entities = 0;
	frame->first_entity = svs.nextSnapshotEntities;
	sv_orl_es_frame++;
	for ( i = 0 ; i < entityNumbers.numSnapshotEntities ; i++ ) {
		ent = SV_GentityNum(entityNumbers.snapshotEntities[i]);
		state = &svs.snapshotEntities[svs.nextSnapshotEntities % svs.numSnapshotEntities];
		*state = ent->s;

		if ( ge->UpdateEntityStateForClient ) {
			ge->UpdateEntityStateForClient( clientEnt, state );
		}

		SV_OracleLogEntityState( state, frame->num_entities );

		svs.nextSnapshotEntities++;
		if ( svs.nextSnapshotEntities >= 0x7FFFFFFE ) {
			Com_Error(ERR_FATAL, "svs.nextSnapshotEntities wrapped");
		}
		frame->num_entities++;
	}

}

#ifdef USE_VOIP
#endif

int SV_SendMessageToClient(msg_t *msg, client_t *client)
{
	int size;
	int rateMsec;

	client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageSize = msg->cursize;
	client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageSent = svs.time;
	client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageAcked = -1;

	size = SV_Netchan_Transmit(client, msg);

	if ( client->netchan.remoteAddress.type == NA_LOOPBACK ) {
		if ( client->state == CS_ACTIVE ) {
			client->nextSnapshotTime = svs.time + 1000 / sv_fps->integer;
		}
	} else {
		rateMsec = SV_RateMsec( client, size );
		if ( rateMsec >= client->snapshotMsec ) {
			client->rateDelayed = qtrue;
		} else {
			client->rateDelayed = qfalse;
			rateMsec = client->snapshotMsec;
		}
		client->nextSnapshotTime = svs.time + rateMsec;

		if ( client->state != CS_ACTIVE ) {
			if ( client->nextSnapshotTime < svs.time + 1000 ) {
				client->nextSnapshotTime = svs.time + 1000;
			}
		}
	}

	return size;
}


static qboolean SV_ShouldPassthrough( client_t *client ) {
	return com_passthrough->integer && client->netchan.remoteAddress.type == NA_LOOPBACK;
}

static void SV_ServerCommandsPassthrough( client_t *client ) {
	int i;

	for ( i = client->reliableAcknowledge + 1; i <= client->reliableSequence; i++ ) {
		CL_AddCommandString( i, client->reliableCommands[ i & ( MAX_RELIABLE_COMMANDS - 1 ) ] );
	}
}

static void SV_SnapshotPassthrough( client_t *client ) {
	clientSnapshot_t *frame;
	clSnapshot_t     newSnap;
	int              i, len;

	frame = &client->frames[ client->netchan.outgoingSequence & PACKET_MASK ];

	clc.serverMessageSequence = client->netchan.outgoingSequence;

	memset( &newSnap, 0, sizeof( newSnap ) );

	newSnap.valid = qtrue;
	newSnap.snapFlags = ( client->rateDelayed ? SNAPFLAG_RATE_DELAYED : 0 )
	                  | ( client->state != CS_ACTIVE ? SNAPFLAG_NOT_ACTIVE : 0 );

	newSnap.serverTime = svs.time;
	newSnap.serverTimeResidual = sv.timeResidual > 254 ? 255 : sv.timeResidual;

	newSnap.messageNum = clc.serverMessageSequence;

	newSnap.cmdNum = client->lastClientCommand;

	len = frame->areabytes > 255 ? 255 : frame->areabytes;
	if ( len > (int)sizeof( newSnap.areamask ) ) {
		len = sizeof( newSnap.areamask );
	}
	memcpy( newSnap.areamask, frame->areabits, len );

	newSnap.ps = frame->ps;

	newSnap.parseEntitiesNum = cl.parseEntitiesNum;
	newSnap.numEntities = frame->num_entities;
	for ( i = 0; i < frame->num_entities; i++ ) {
		cl.parseEntities[ cl.parseEntitiesNum & ( MAX_PARSE_ENTITIES - 1 ) ] =
			svs.snapshotEntities[ ( frame->first_entity + i ) % svs.numSnapshotEntities ];
		cl.parseEntitiesNum++;
		MSG_SetupClientEntity( &cl.parseEntities[ ( cl.parseEntitiesNum - 1 ) & ( MAX_PARSE_ENTITIES - 1 ) ] );
	}

	newSnap.number_of_sounds = client->number_of_server_sounds;
	memcpy( newSnap.sounds, client->server_sounds, client->number_of_server_sounds * sizeof( server_sound_t ) );

	newSnap.ws = sv.worldState;

	newSnap.serverCommandNum = clc.serverCommandSequence;

	CL_SnapshotWorldState( &newSnap.ws );

	CL_CommitSnapshot( &newSnap );

	if ( client->stringToPrint[0] ) {
		if ( client->locprint ) {
			UI_UpdateLocationPrint( client->XOffset, client->YOffset, client->stringToPrint, 1.0 );
		} else {
			UI_UpdateCenterPrint( client->stringToPrint, 1.0 );
		}
	}
}

void SV_SendClientSnapshot( client_t *client ) {
	byte		msg_buf[MAX_MSGLEN];
	msg_t		msg;
	int			i;

	SV_BuildClientSnapshot( client );

	if ( !client->gentity || ( client->gentity->svflags & SVF_BOT ) ) {
		return;
	}

	if ( SV_ShouldPassthrough( client ) ) {
		SV_ServerCommandsPassthrough( client );
		SV_SnapshotPassthrough( client );
		client->netchan.outgoingSequence++;
		if ( client->state == CS_ACTIVE ) {
			client->nextSnapshotTime = svs.time + 1000 / sv_fps->integer;
		}
	} else {
		MSG_Init(&msg, msg_buf, sizeof(msg_buf));
		msg.allowoverflow = qtrue;

		MSG_WriteLong(&msg, client->lastClientCommand);

		for ( i = client->reliableAcknowledge + 1 ; i <= client->reliableSequence ; i++ ) {
			MSG_WriteSVC( &msg, svc_serverCommand );
			MSG_WriteLong( &msg, i );
			MSG_WriteString( &msg, client->reliableCommands[ i & ( MAX_RELIABLE_COMMANDS - 1 ) ] );
		}

		SV_WriteSnapshotToClient( client, &msg );

		if ( msg.overflowed ) {
			Com_WPrintf( "WARNING: msg overflowed for %s\n", client->name );
			MSG_Clear( &msg );
		}

		SV_SendMessageToClient( &msg, client );
	}

	SV_ClearSounds( client );

	Com_Memcpy( &client->ws, &sv.worldState, sizeof( worldState_t ) );
	client->stringToPrint[ 0 ] = 0;
	client->locprint = qfalse;
}

void SV_SendClientMessages(void)
{
	int				i;
	int				rate;
	int				size;
	client_t		*c;

	for ( i = 0 ; i < sv_maxclients->integer ; i++ )
	{
		c = &svs.clients[i];

		if ( c->state == CS_PRIMED && c->netchan.unsentFragments )
		{
			if ( svs.time >= c->nextFragmentTime )
			{
				size = SV_Netchan_TransmitNextFragment( c );
				if ( c->netchan.remoteAddress.type == NA_LOOPBACK ) {
					c->nextFragmentTime = 0;
				} else {
					rate = SV_RateMsec( c, size );
					c->nextFragmentTime = svs.time + rate;
				}
				c->gamestateTime = svs.time;
			}
		}
		else if ( c->state == CS_ACTIVE && svs.time >= c->nextSnapshotTime )
		{
			if ( c->netchan.unsentFragments )
			{
				size = c->netchan.unsentLength - c->netchan.unsentFragmentStart;
				if ( size > 1500 ) {
					size = 1500;
				}
				rate = SV_RateMsec( c, size );
				c->nextSnapshotTime = svs.time + rate;
				SV_Netchan_TransmitNextFragment( c );
			}
			else
			{
				SV_SendClientSnapshot( c );
			}
		}
	}
}

