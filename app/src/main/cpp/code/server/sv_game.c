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
#include "../fgame/g_import_verify_gen.h"
#include "../botlib/botlib.h"
#include "../qcommon/stringresource.h" 
#include "../botlib/l_memory.h"
#include "../botlib/l_log.h"
#include "../botlib/l_libvar.h"
#include "../botlib/l_script.h"
#include "../botlib/l_precomp.h"
#include "../botlib/l_struct.h"
#include "../botlib/aasfile.h"
#include "../botlib/be_aas.h"
#include "../botlib/be_aas_funcs.h"
#include "../botlib/be_interface.h"
#include "../botlib/be_aas_def.h"
#include "../botlib/be_ea.h"
#include "../botlib/be_ai_char.h"
#include "../botlib/be_ai_chat.h"
#include "../botlib/be_ai_goal.h"
#include "../botlib/be_ai_move.h"
#include "../botlib/be_ai_weap.h"
#include "../botlib/be_ai_gen.h"

#include "../client/snd_public.h"
#include "../client/client.h"
#include "../corepp/tiki.h"
#include "../qcommon/crc.h"
#include "../qcommon/alias.h"

debugline_t *DebugLines;
int numDebugLines;
debugstring_t *DebugStrings;
int numDebugStrings;

static dtiki_t *sv_modelTiki[ MAX_MODELS ];

void SV_GameError( const char *string ) {
	Com_Error( ERR_DROP, "%s", string );
}

void SV_GamePrint( const char *string ) {
	Com_Printf( "%s", string );
}

int	SV_NumForGentity( gentity_t *ent ) {
	int		num;

	num = ( (byte *)ent - (byte *)sv.gentities ) / sv.gentitySize;

	return num;
}

gentity_t *SV_GentityNum( int num ) {
	gentity_t *ent;

	ent = (gentity_t *)((byte *)sv.gentities + sv.gentitySize*(num));

	return ent;
}

playerState_t *SV_GameClientNum( int num ) {
	playerState_t	*ps;

	ps = (playerState_t *)((byte *)sv.gameClients + sv.gameClientSize*(num));

	return ps;
}

svEntity_t	*SV_SvEntityForGentity( gentity_t *gEnt ) {
	if ( !gEnt || gEnt->s.number >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "SV_SvEntityForGentity: bad gEnt" );
	}
	return &sv.svEntities[ gEnt->s.number ];
}

gentity_t *SV_GEntityForSvEntity( svEntity_t *svEnt ) {
	int		num;

	num = svEnt - sv.svEntities;
	return SV_GentityNum( num );
}

typedef struct {
	byte *data;
	int cursize;
	byte *datatypes;
	int dtindex;
} cgm_t;

cgm_t g_CGMessages[MAX_CLIENTS];
qboolean g_CGMRecieve[MAX_CLIENTS] = { qfalse };
int nextTime[MAX_CLIENTS] = { 0 };

void SV_ClearCGMessage (int iClient)
{
	g_CGMessages[iClient].cursize = 0;
	g_CGMessages[iClient].dtindex = 0;
	if(g_CGMessages[iClient].data) {
		Z_Free(g_CGMessages[iClient].data);
		g_CGMessages[iClient].data = 0;
	}
	if(g_CGMessages[iClient].datatypes) {
		Z_Free(g_CGMessages[iClient].datatypes);
		g_CGMessages[iClient].datatypes = 0;
	}
	g_CGMRecieve[iClient] = 0;
}

void SV_ClearAllCGMessages (void)
{
	cgm_t *pCGM;
	int i;
	pCGM = g_CGMessages;
	for(i = 0; i < MAX_CLIENTS; i++,pCGM++) {
		pCGM->cursize = 0;
		pCGM->dtindex = 0;
		if(pCGM->data) {
			Z_Free(pCGM->data);
			pCGM->data = 0;
		}
		if(pCGM->datatypes) {
			Z_Free(pCGM->datatypes);
			pCGM->datatypes = 0;
		}
		g_CGMRecieve[i] = 0;
	}
}

#define CGM_DATA_SIZE 4096
#define CGM_DATATYPES_SIZE 8192

void SV_InitCGMessage (int iClient)
{
	cgm_t *pCGM;

	pCGM = &g_CGMessages[iClient];

	if ( pCGM->data == 0 )
		pCGM->data = Z_Malloc(CGM_DATA_SIZE);

	if ( pCGM->datatypes == 0 )
		pCGM->datatypes = Z_Malloc(CGM_DATA_SIZE);

	pCGM->cursize = 0;
	pCGM->dtindex = 0;

	g_CGMRecieve[iClient] = 0;
}

void SV_InitAllCGMessages (void)
{
	int i;

	SV_ClearAllCGMessages();
	for ( i = 0; i < sv_maxclients->integer; ++i )
		SV_InitCGMessage(i);
}

static void MSG_HandleCGMBufferOverflow(cgm_t* pCGM, qboolean isDatum) {
	int index;

	index = pCGM - g_CGMessages;
	if (!nextTime[index] || nextTime[index] < svs.time)
	{
		if (isDatum) {
			Com_DPrintf("CGM buffer for client %i overflowed number of datum\n", index);
		} else {
			Com_DPrintf("CGM buffer for client %i overflowed size of data\n", index);
		}
		nextTime[index] = svs.time + 5000;
	}
}

static void MSG_WriteCGMBits (cgm_t *pCGM, int value, int bits)
{
	if (CGM_DATA_SIZE - pCGM->cursize < 4) {
		MSG_HandleCGMBufferOverflow(pCGM, qfalse);
		return;
	}
	if(CGM_DATATYPES_SIZE - pCGM->dtindex < 1) {
		MSG_HandleCGMBufferOverflow(pCGM, qtrue);
		return;
	}

    if (bits > 32) {
        Com_Error(1, "PF_MSG_WriteBits: bad bits %i", bits);
    }

	if(bits < 0)
		bits = -bits;

	if(bits <= 8) {
		pCGM->data[pCGM->cursize] = value;
		pCGM->cursize++;
	} else if(bits <= 16) {
		*((short*)(&pCGM->data[pCGM->cursize])) = value;
		pCGM->cursize+=2;
	} else if(bits <= 32) {
		*((int*)(&pCGM->data[pCGM->cursize])) = value;	
		pCGM->cursize+=4;
	}
	pCGM->datatypes[pCGM->dtindex] = bits;
	pCGM->dtindex++;
}

void PF_MSG_WriteBits (int value, int bits)
{
	cgm_t *pCGM;
	int i;
	pCGM = g_CGMessages;
	for( i = 0; i < sv_maxclients->integer; i++, pCGM++ ) {
		if(g_CGMRecieve[i] == 0) 
			continue;
		MSG_WriteCGMBits(pCGM,value,bits);
	}
}

void PF_MSG_WriteChar (int c)
{
	cgm_t *pCGM;
	int i;
	pCGM = g_CGMessages;
	for(i = 0; i < sv_maxclients->integer; i++,pCGM++) {
		if(g_CGMRecieve[i] == 0) 
			continue;
		MSG_WriteCGMBits(pCGM,c,8);
	}
}

void PF_MSG_WriteByte (int c)
{
	cgm_t *pCGM;
	int i;
	pCGM = g_CGMessages;
	for(i = 0; i < sv_maxclients->integer; i++,pCGM++) {
		if(g_CGMRecieve[i] == 0) 
			continue;
		MSG_WriteCGMBits(pCGM,c,8);
	}
}

void PF_MSG_WriteSVC (int c)
{
	cgm_t *pCGM;
	int i;
	pCGM = g_CGMessages;
	for(i = 0; i < sv_maxclients->integer; i++,pCGM++) {
		if(g_CGMRecieve[i] == 0) 
			continue;
		MSG_WriteCGMBits(pCGM,c,8);
	}
}

void PF_MSG_WriteShort (int c)
{
	cgm_t *pCGM;
	int i;
	pCGM = g_CGMessages;
	for(i = 0; i < sv_maxclients->integer; i++,pCGM++) {
		if(g_CGMRecieve[i] == 0) 
			continue;
		MSG_WriteCGMBits(pCGM,c,16);
	}
}

void PF_MSG_WriteLong (int c)
{
 	cgm_t *pCGM;
	int i;
	pCGM = g_CGMessages;
	for(i = 0; i < sv_maxclients->integer; i++,pCGM++) {
		if(g_CGMRecieve[i] == 0) 
			continue;
		MSG_WriteCGMBits(pCGM,c,32);
	}
}

void PF_MSG_WriteFloat (float f)
{
	union {
		float fl;
		int l;
	} dat;
 	cgm_t *pCGM;
	int i;
	dat.fl = f;
	pCGM = g_CGMessages;
	for(i = 0; i < sv_maxclients->integer; i++,pCGM++) {
		if(g_CGMRecieve[i] == 0) 
			continue;
		MSG_WriteCGMBits(pCGM,dat.l,32);
	}
}

void PF_MSG_WriteString (const char *s)
{
 	cgm_t *pCGM;
	int i;
	size_t j, l;
	l = strlen(s);
	l+=1; 
	pCGM = g_CGMessages;
	for(i = 0; i < sv_maxclients->integer; i++,pCGM++) {
		if(g_CGMRecieve[i] == 0) 
			continue;
		for(j = 0; j < l; j++) {
			MSG_WriteCGMBits(pCGM,s[j],8);
		}
	}
}

void PF_MSG_WriteAngle8 (float f)
{
 	cgm_t *pCGM;
	int i;
	pCGM = g_CGMessages;
	for(i = 0; i < sv_maxclients->integer; i++,pCGM++) {
		if(g_CGMRecieve[i] == 0) 
			continue;
		MSG_WriteCGMBits(pCGM,f * 256.0 / 360.0,8);
	}
}

void PF_MSG_WriteAngle16 (float f)
{
 	cgm_t *pCGM;
	int i;
	pCGM = g_CGMessages;
	for(i = 0; i < sv_maxclients->integer; i++,pCGM++) {
		if(g_CGMRecieve[i] == 0) 
			continue;
		MSG_WriteCGMBits(pCGM,ANGLE2SHORT(f),16);
	}
}

void PF_MSG_WriteCoord (float f)
{
 	cgm_t *pCGM;
	int i;

	int write = abs((int)(f * 16.f));
	if(f < 0) {
		write |= 262144;
	} else {
		write &= ~262144;
	}

	pCGM = g_CGMessages;
	for(i = 0; i < sv_maxclients->integer; i++,pCGM++) {
		if(g_CGMRecieve[i] == 0) 
			continue;
		MSG_WriteCGMBits(pCGM, write, 19);
	}
}

void PF_MSG_WriteDir (vec_t *dir)
{
 	cgm_t *pCGM;
	int i;
	byte b;
	b = DirToByte(dir);
	pCGM = g_CGMessages;
	for(i = 0; i < sv_maxclients->integer; i++,pCGM++) {
		if(g_CGMRecieve[i] == 0) 
			continue;
		MSG_WriteCGMBits(pCGM,b,8);
	}
}

void PF_MSG_StartCGM (int type)
{
    cgm_t *pCGM;
    int i;

	pCGM = g_CGMessages;
	for(i = 0; i < sv_maxclients->integer; i++,pCGM++) {
		if(g_CGMRecieve[i] == 0) 
			continue;
		if(pCGM->cursize <= 0) {
			MSG_WriteCGMBits(pCGM,10,8);
		} else {
			MSG_WriteCGMBits(pCGM, 1, 1);
		}
		MSG_WriteCGMBits(pCGM, type, 6);
	}
}

void PF_MSG_EndCGM (void)
{
	memset(g_CGMRecieve,0,sizeof(g_CGMRecieve));
}

void PF_MSG_SetClient (int iClient)
{
	memset(g_CGMRecieve,0,sizeof(g_CGMRecieve));
	if(g_CGMessages[iClient].data && (g_CGMessages[iClient].cursize <= 3967))
	{
		g_CGMRecieve[iClient] = 1;
	}
}

void MSG_SetBroadcastAll(void)
{
	cgm_t *pCGM;
	client_t* client;
	int i;

	pCGM = g_CGMessages;
	for(i = 0; i < sv_maxclients->integer; i++,pCGM++) {
		client = &svs.clients[i];

		if (!client->gentity || !client->gentity->inuse) {
			continue;
		}

		if (client->state == CS_FREE) {
			continue;
		}

		if (!pCGM->data || pCGM->cursize >= 3968) {
			MSG_HandleCGMBufferOverflow(pCGM, qfalse);
			continue;
		}

		g_CGMRecieve[i] = qtrue;
	}
}

void MSG_SetBroadcastVisible(const vec_t* vPos, const vec_t* vPosB)
{
    byte *clientpvs;
    int posBcluster;
    int poscluster;
    int posBarea;
    int posarea;
    int posBleaf;
    int posleaf;
    int clientcluster;
    int clientarea;
    int leafnum;
    client_t *pClient;
    int i;
	cgm_t *pCGM;

	posleaf = CM_PointLeafnum(vPos);
	posarea = CM_LeafArea(posleaf);
	poscluster = CM_LeafCluster(posleaf);

	if (vPosB) {
		posBleaf = CM_PointLeafnum(vPosB);
		posBarea = CM_LeafArea(posBleaf);
		posBcluster = CM_LeafCluster(posBleaf);
	} else {
		posBarea = 0;
		posBcluster = 0;
	}

	pCGM = g_CGMessages;
	pClient = svs.clients;

	for(i = 0; i < sv_maxclients->integer; i++,pCGM++,pClient++) {
		if (!pClient->gentity || !pClient->gentity->inuse) {
			continue;
		}

		if(pClient->state == CS_FREE) {
			continue;
		}

		leafnum = CM_PointLeafnum(pClient->gentity->s.origin);
		clientarea = CM_LeafArea(leafnum);
		clientcluster = CM_LeafCluster(leafnum);
		clientpvs = CM_ClusterPVS(clientcluster);

		if (!CM_AreasConnected(clientarea, posarea) && (!vPosB || !CM_AreasConnected(clientarea, posBarea))) {
			continue;
		}

		if (!(clientpvs[poscluster >> 3] & (1 << (poscluster & 7))) && (!vPosB || !(clientpvs[posBcluster >> 3] & (1 << (posBcluster & 7))))) {
			continue;
		}

		if (!pCGM->data || pCGM->cursize >= 3968) {
			MSG_HandleCGMBufferOverflow(pCGM, qfalse);
			continue;
		}

		g_CGMRecieve[i] = qtrue;
	}
}
void MSG_SetBroadcastHearable ( const vec_t *vPos, const vec_t *vPosB)
{
	MSG_SetBroadcastAll();
}
void SV_WriteCGMToClient (client_t *client, msg_t *msg)
{
	cgm_t *pCGMMsg;
	int clientNum;

	clientNum = client - svs.clients;;
	pCGMMsg = &g_CGMessages[clientNum];
	if ( pCGMMsg->data )
	{
		if ( pCGMMsg->cursize > 0 )
		{
			int curDTIndex;
			byte *data;
			MSG_WriteCGMBits(pCGMMsg, 0, 1);
			data = pCGMMsg->data;
			curDTIndex = 0;
			for(curDTIndex = 0; curDTIndex < pCGMMsg->dtindex; curDTIndex++) {
				int bits = pCGMMsg->datatypes[curDTIndex];
				if(bits <= 8) {
					MSG_WriteBits(msg, *data, bits);
					data++;
				} else if(bits <= 16) {
					MSG_WriteBits(msg, *(short*)data, bits);
					data+=2;
				} else if(bits <= 32) {
					MSG_WriteBits(msg, *(int*)data, bits);
					data+=4;
				}
			
			}
			pCGMMsg->cursize = 0;
			pCGMMsg->dtindex = 0;
		}
	}
}

qboolean SV_inPVS (const vec3_t p1, const vec3_t p2)
{
	int		leafnum;
	int		cluster;
	int		area1, area2;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	area1 = CM_LeafArea (leafnum);
	mask = CM_ClusterPVS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);
	area2 = CM_LeafArea (leafnum);
	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return qfalse;
	if (!CM_AreasConnected (area1, area2))
		return qfalse;		
	return qtrue;
}


qboolean SV_inPVSIgnorePortals( const vec3_t p1, const vec3_t p2)
{
	int		leafnum;
	int		cluster;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	mask = CM_ClusterPVS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);

	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return qfalse;

	return qtrue;
}

int PF_NumAnims( dtiki_t *tiki )
{
    if (!tiki) {
        return 0;
    }

	return TIKI_NumAnims( tiki );
}

int PF_NumSurfaces( dtiki_t *tiki )
{
    if (!tiki) {
        return 0;
    }

	return TIKI_NumSurfaces( tiki );
}

int PF_Morph_NumForName( dtiki_t *tiki, const char *name )
{
    if (!tiki) {
        return -1;
    }

	return TIKI_Morph_NumForName( tiki, name );
}

int PF_TIKI_NumMorphs( dtiki_t *tiki )
{
    int num = 0;
    int m;

    if (!tiki) {
        return 0;
    }

    for (m = 0; m < tiki->numMeshes; m++) {
        skelHeaderGame_t *skel = TIKI_GetSkel( tiki->mesh[ m ] );

        if (skel && skel->pMorphTargets) {
            num += skel->numMorphTargets;
        }
    }

    return num;
}

const char *PF_TIKI_Morph_NameForNum( dtiki_t *tiki, int num )
{
    int base = 0;
    int m;

    if (!tiki || num < 0) {
        return NULL;
    }

    for (m = 0; m < tiki->numMeshes; m++) {
        skelHeaderGame_t *skel = TIKI_GetSkel( tiki->mesh[ m ] );
        const char       *p;
        int               i;

        if (!skel || !skel->pMorphTargets) {
            continue;
        }

        if (num < base + skel->numMorphTargets) {
            p = skel->pMorphTargets;
            for (i = 0; i < num - base; i++) {
                p += strlen( p ) + 1;
            }
            return p;
        }

        base += skel->numMorphTargets;
    }

    return NULL;
}

int PF_ModelIndex( const char *name )
{
	int     modelindex;
	size_t  len;

	modelindex = SV_ModelIndex( name );

	len = name ? strlen( name ) : 0;
	if ( len < 3 || Q_stricmp( name + len - 3, "tik" ) ) {
		sv_modelTiki[ modelindex ] = NULL;
	} else if ( !sv_modelTiki[ modelindex ] ) {
		CL_ProcessLoadingScreen( "" );
		sv_modelTiki[ modelindex ] = TIKI_GetTiki( TIKI_RegisterTiki( name ) );
		if ( !sv_modelTiki[ modelindex ] ) {
			return -1;
		}
	}

	return modelindex;
}

void PF_TikiLoadFromTS( const char *path, const char *tikidata )
{
}

void *PF_ToolServerGetData( void )
{
    return NULL;
}

extern void *MissionObjective_NewObjective(const char *name);
extern void MissionObjective_ClearObjectiveList(void);
extern const char *MissionObjective_GetDescription(const char *name);
extern void MissionObjective_SetDescription(const char *name, const char *desc);
extern qboolean MissionObjective_GetShowObjective(const char *name);
extern void MissionObjective_SetShowObjective(const char *name, qboolean show);
extern qboolean MissionObjective_GetObjectiveComplete(const char *name);
extern void MissionObjective_SetObjectiveComplete(const char *name, qboolean complete);
extern qboolean MissionObjective_GetObjectiveFailed(const char *name);
extern void MissionObjective_SetObjectiveFailed(const char *name, qboolean failed);
extern const char *MissionObjective_GetNameFromIndex(unsigned int index);
extern int MissionObjective_GetIndexFromName(const char *name);
extern void MissionObjectives_ParseObjectiveFile(const char *levelname);
extern void MissionObjectives_Update(const char *objectivesName);
extern int MissionObjective_GetNumObjectives(void);
extern int MissionObjective_GetNumActiveObjectives(void);
extern int MissionObjective_GetNumCompleteObjectives(void);
extern int MissionObjective_GetNumFailedObjectives(void);
extern int MissionObjective_GetNumIncompleteObjectives(void);
extern const char *MissionInformation_GetShader(const char *name);
extern void MissionInformation_SetShader(const char *name, const char *shader);
extern const char *MissionInformation_GetInformationData(const char *name);
extern void MissionInformation_SetInformationData(const char *name, const char *data);
extern const char *MissionInformation_GetNameFromIndex(unsigned int index);
extern int MissionInformation_GetIndexFromName(const char *name);
extern void *MissionInformation_NewInformation(const char *name);
extern void MissionInformation_ClearInformationList(void);
extern void MissionInformation_SetShowInformation(const char *name, qboolean show);
extern qboolean MissionInformation_GetShowInformation(const char *name);

static const char *PF_MObjective_GetDescription(const char *name) { return MissionObjective_GetDescription(name); }
static void PF_MObjective_SetDescription(const char *name, const char *desc) { MissionObjective_SetDescription(name, desc); }
static qboolean PF_MObjective_GetShowObjective(const char *name) { return MissionObjective_GetShowObjective(name); }
static void PF_MObjective_SetShowObjective(const char *name, qboolean show) { MissionObjective_SetShowObjective(name, show); }
static qboolean PF_MObjective_GetObjectiveComplete(const char *name) { return MissionObjective_GetObjectiveComplete(name); }
static void PF_MObjective_SetObjectiveComplete(const char *name, qboolean complete) { MissionObjective_SetObjectiveComplete(name, complete); }
static qboolean PF_MObjective_GetObjectiveFailed(const char *name) { return MissionObjective_GetObjectiveFailed(name); }
static void PF_MObjective_SetObjectiveFailed(const char *name, qboolean failed) { MissionObjective_SetObjectiveFailed(name, failed); }
static const char *PF_MObjective_GetNameFromIndex(unsigned int index) { return MissionObjective_GetNameFromIndex(index); }
static int PF_MObjective_GetIndexFromName(const char *name) { return MissionObjective_GetIndexFromName(name); }
static void PF_MObjective_NewObjective(const char *name) { MissionObjective_NewObjective(name); }
static void PF_MObjective_ClearObjectiveList(void) { MissionObjective_ClearObjectiveList(); }
static void PF_MObjective_ParseObjectiveFile(const char *levelname) { MissionObjectives_ParseObjectiveFile(levelname); }
static void PF_MObjective_Update(const char *objectivesName) { MissionObjectives_Update(objectivesName); }
static int PF_MObjective_GetNumObjectives(void) { return MissionObjective_GetNumObjectives(); }
static int PF_MObjective_GetNumActiveObjectives(void) { return MissionObjective_GetNumActiveObjectives(); }
static int PF_MObjective_GetNumCompleteObjectives(void) { return MissionObjective_GetNumCompleteObjectives(); }
static int PF_MObjective_GetNumFailedObjectives(void) { return MissionObjective_GetNumFailedObjectives(); }
static int PF_MObjective_GetNumIncompleteObjectives(void) { return MissionObjective_GetNumIncompleteObjectives(); }

static const char *PF_MI_GetShader(const char *name) { return MissionInformation_GetShader(name); }
static void PF_MI_SetShader(const char *name, const char *shader) { MissionInformation_SetShader(name, shader); }
static const char *PF_MI_GetInformationData(const char *name) { return MissionInformation_GetInformationData(name); }
static void PF_MI_SetInformationData(const char *name, const char *data) { MissionInformation_SetInformationData(name, data); }
static const char *PF_MI_GetNameFromIndex(unsigned int index) { return MissionInformation_GetNameFromIndex(index); }
static int PF_MI_GetIndexFromName(const char *name) { return MissionInformation_GetIndexFromName(name); }
static void PF_MI_NewInformation(const char *name) { MissionInformation_NewInformation(name); }
static void PF_MI_ClearInformationList(void) { MissionInformation_ClearInformationList(); }
static void PF_MI_SetShowInformation(const char *name, qboolean show) { MissionInformation_SetShowInformation(name, show); }
static qboolean PF_MI_GetShowInformation(const char *name) { return MissionInformation_GetShowInformation(name); }



int PF_NumTags( dtiki_t *tiki )
{
    if (!tiki) {
        return 0;
    }

	return TIKI_NumTags( tiki );
}

float PF_Cross_Time( dtiki_t *tiki, int anim )
{
    if (!tiki) {
        return 0;
    }

	return TIKI_Anim_CrossblendTime( tiki, anim );
}

void PF_Frame_Bounds( dtiki_t *tiki, int animnum, int framenum, float scale, vec3_t mins, vec3_t maxs )
{
	TIKI_Frame_Bounds( tiki, animnum, framenum, scale, mins, maxs );
}

void PF_CalculateBounds( dtiki_t *tiki, float scale, vec3_t mins, vec3_t maxs )
{
    if (!tiki) {
        return;
    }

	TIKI_CalculateBounds( tiki, scale, mins, maxs );
}

const char *PF_Anim_NameForNum( dtiki_t *tiki, int animnum )
{
    if (!tiki) {
        return 0;
    }

	return TIKI_Anim_NameForNum( tiki, animnum );
}

int PF_Anim_NumForName( dtiki_t *tiki, const char *name )
{
    if (!tiki) {
        return -1;
    }

	return TIKI_Anim_NumForName( tiki, name );
}

int PF_Anim_Random( dtiki_t *tiki, const char *name )
{
    if (!tiki) {
        return -1;
    }

	return TIKI_Anim_Random( tiki, name );
}

int PF_Anim_NumFrames( dtiki_t *tiki, int animnum )
{
    if (!tiki) {
        return 0;
    }

	return TIKI_Anim_NumFrames( tiki, animnum );
}

float PF_Anim_Time( dtiki_t *tiki, int animnum )
{
    if (!tiki) {
        return 0;
    }

	return TIKI_Anim_Time( tiki, animnum );
}

float PF_Anim_Frametime( dtiki_t *tiki, int animnum )
{
	if (!tiki) {
		return 0;
	}

	return TIKI_Anim_Frametime( tiki, animnum );
}

void PF_Anim_Delta( dtiki_t *tiki, int animnum, vec3_t delta )
{
	if (!tiki) {
		return;
	}

	TIKI_Anim_Delta( tiki, animnum, delta );
}

void PF_Anim_AngularDelta(dtiki_t* tiki, int animnum, vec3_t delta)
{
    if (!tiki) {
        return;
    }

	TIKI_Anim_AngularDelta(tiki, animnum, delta);
}

qboolean PF_Anim_HasDelta( dtiki_t *tiki, int animnum )
{
    if (!tiki) {
        return qfalse;
    }

	return TIKI_Anim_HasDelta( tiki, animnum );
}

void PF_Anim_DeltaOverTime( dtiki_t *tiki, int iAnimnum, float fTime1, float fTime2, vec3_t vDelta )
{
    if (!tiki) {
        return;
    }

	TIKI_Anim_DeltaOverTime( tiki, iAnimnum, fTime1, fTime2, vDelta );
}

void PF_Anim_AngularDeltaOverTime(dtiki_t* tiki, int iAnimnum, float fTime1, float fTime2, float * fDelta)
{
    if (!tiki) {
        return;
    }

	TIKI_Anim_AngularDeltaOverTime(tiki, iAnimnum, fTime1, fTime2, fDelta);
}

int PF_Anim_Flags( dtiki_t *tiki, int animnum )
{
    if (!tiki) {
        return 0;
    }

	return TIKI_Anim_Flags( tiki, animnum );
}

int PF_Anim_FlagsSkel( dtiki_t *tiki, int animnum )
{
    if (!tiki) {
        return 0;
    }

	return TIKI_Anim_FlagsSkel( tiki, animnum );
}

qboolean PF_Anim_HasCommands( dtiki_t *tiki, int animnum )
{
    if (!tiki) {
        return qfalse;
    }

	return TIKI_Anim_HasServerCommands( tiki, animnum );
}

qboolean PF_Anim_HasCommands_Client(dtiki_t* tiki, int animnum)
{
    if (!tiki) {
        return qfalse;
    }

    return TIKI_Anim_HasClientCommands(tiki, animnum);
}

dtiki_t *PF_ModelTiki( const char *name )
{
	return TIKI_GetTiki( TIKI_RegisterTiki( name ) );
}

dtikianim_t *PF_ModelTikiAnim( const char *name )
{
	return TIKI_RegisterTikiAnimFlags( name, qtrue );
}

void PF_TIKI_CacheAnim( const char *path )
{
	TIKI_RegisterAnim( path, qtrue );
}

int PF_ArcheTypeIndex( const char *name )
{
	return SV_FindIndex( name, CS_ARCHETYPE, MAX_ARCHETYPES, qtrue );
}

qboolean PF_Anim_HasClientCommands( dtiki_t *tiki, int animnum )
{
	if (!tiki) {
		return qfalse;
	}

	return TIKI_Anim_HasClientCommands( tiki, animnum );
}

qboolean PF_Frame_Commands( dtiki_t *tiki, int animnum, int framenum, tiki_cmd_t *tiki_cmds )
{
    if (!tiki) {
        return qfalse;
    }

	return TIKI_Frame_Commands_Server( tiki, animnum, framenum, tiki_cmds );
}

qboolean PF_Frame_Commands_Client( dtiki_t *tiki, int animnum, int framenum, tiki_cmd_t *tiki_cmds )
{
    if (!tiki) {
        return qfalse;
    }

	return TIKI_Frame_Commands_Client( tiki, animnum, framenum, tiki_cmds, NULL );
}

int PF_Surface_NameToNum( dtiki_t *tiki, const char *name )
{
    if (!tiki) {
        return -1;
    }

	return TIKI_Surface_NameToNum( tiki, name );
}

const char *PF_Surface_NumToName( dtiki_t *tiki, int num )
{
    if (!tiki) {
        return NULL;
    }

	return TIKI_Surface_NumToName( tiki, num );
}

int PF_Surface_Flags( dtiki_t *tiki, int num )
{
	if ( !tiki || num < 0 || num >= tiki->num_surfaces ) {
		return 0;
	}

	return tiki->surfaces[ num ].flags;
}

int PF_Surface_NumSkins( dtiki_t *tiki, int num )
{
	if ( !tiki || num < 0 || num >= tiki->num_surfaces ) {
		return 0;
	}

	return tiki->surfaces[ num ].numskins;
}

int PF_Tag_NameToNum( dtiki_t *tiki, const char *name )
{
	if (!tiki) {
		return -1;
	}

	return TIKI_Tag_NameToNum( tiki, name );
}

const char *PF_Tag_NumToName( dtiki_t *tiki, int num )
{
	if (!tiki) {
		return NULL;
	}

	return TIKI_Tag_NumToName( tiki, num );
}

/* dtiki_t::alias_list, never a->alias_list: the client registers the character sound
   families into the dtiki_t list and server and client share that one object. */
qboolean PF_Alias_Add( dtiki_t *pmdl, const char *alias, const char *name, const char *parameters )
{
	if (!pmdl) {
		return qfalse;
	}

	if (!pmdl->alias_list) {
		pmdl->alias_list = AliasList_New( pmdl->name );
	}
	return Alias_ListAdd( (AliasList_t *)pmdl->alias_list, alias, name, parameters );
}

const char *PF_Alias_Find( dtiki_t *tiki, const char *alias )
{
	AliasList_t* alias_list = NULL;

	if (tiki) {
		alias_list = (AliasList_t *)tiki->alias_list;
	}

	return alias_list ? Alias_ListFind(alias_list, alias) : NULL;
}

void *PF_Alias_GetList( dtiki_t *tiki )
{
	return tiki ? tiki->alias_list : NULL;
}

void PF_Alias_AddActorDialog( dtiki_t *tiki, const char *alias, int actor_number, int number_of_times_played, byte been_played_this_loop, int last_time_played )
{
	AliasList_t* alias_list = NULL;

	if (tiki) {
		alias_list = (AliasList_t *)tiki->alias_list;
	}

	if (alias_list) {
		Alias_ListAddActorDialog(alias_list, alias, actor_number, number_of_times_played, been_played_this_loop, last_time_played);
	}
}

const char *PF_Alias_FindRandom( dtiki_t *tiki, const char *alias, AliasListNode_t **ret )
{
	AliasList_t* alias_list = NULL;

	if (tiki) {
		alias_list = (AliasList_t *)tiki->alias_list;
	}

	if (alias_list) {
		return Alias_ListFindRandom(alias_list, alias);
	} else {
		return NULL;
	}
}

void PF_Alias_UpdateDialog( dtiki_t *tiki, const char *alias, int number_of_times_played, byte been_played_this_loop, int last_time_played )
{
	AliasList_t* alias_list = NULL;

	if (tiki) {
		alias_list = (AliasList_t *)tiki->alias_list;
	}

    if (alias_list) {
        Alias_ListUpdateDialog(alias_list, alias, number_of_times_played, been_played_this_loop, last_time_played);
    }
}

const char *PF_Alias_FindDialog(
    dtiki_t *tiki, const char *alias, qboolean random_flag, int actor_number
)
{
	AliasList_t* alias_list = NULL;

	if (tiki) {
		alias_list = (AliasList_t *)tiki->alias_list;
	}

	if (!alias_list) {
		return NULL;
	}

	return Alias_ListFindDialog(alias_list, alias, random_flag, actor_number);
}

const char *PF_Alias_FindSpecificAnim( dtiki_t *tiki, const char *name )
{
	AliasList_t* alias_list = NULL;

	if (tiki) {
		alias_list = (AliasList_t *)tiki->alias_list;
	}

	return Alias_ListFindSpecificAnim(alias_list, name);
}

qboolean PF_Alias_CheckLoopAnim( dtiki_t *tiki, const char *name )
{
	AliasList_t* alias_list = NULL;

	if (tiki) {
		alias_list = (AliasList_t *)tiki->alias_list;
	}

	return Alias_ListCheckLoopAnim(alias_list, name);
}


char *PF_GetNextMorphTarget(const char *name, int *index, int *frame, float *amplitude)
{
    return GetNextMorphTarget(name, index, frame, amplitude);
}

void PF_Alias_Dump( dtiki_t *tiki )
{
    AliasList_t* alias_list = NULL;

    if (tiki) {
        alias_list = (AliasList_t *)tiki->alias_list;
    }

    if (alias_list) {
        Alias_ListDump(alias_list);
    }
}

void PF_Alias_Clear( dtiki_t *tiki )
{
	PF_Alias_Dump( tiki );
}

const char *PF_NameForNum( dtiki_t *tiki )
{
	if (!tiki) {
		return NULL;
	}

	return TIKI_Name( tiki );
}

dtiki_t *TIKI_Handle_For_ModelIndex( int modelindex )
{
	const char *s;
	char        name[ MAX_QPATH ];
	size_t      len;
	dtiki_t    *cached;
	const char *cachedname;

	if ( modelindex <= 0 || modelindex >= MAX_MODELS ) {
		return NULL;
	}

	char *text_p = sv.configstrings[ CS_MODELS + modelindex ];

	s = COM_Parse( &text_p );
	if ( !s || !s[ 0 ] ) {
		return NULL;
	}

	Q_strncpyz( name, s, sizeof( name ) );

	len = strlen( name );
	if ( len < 3 || Q_stricmp( name + len - 3, "tik" ) ) {
		return NULL;
	}

	cached = sv_modelTiki[ modelindex ];
	if ( cached ) {
		// levels -- retail never does that either -- a cached row is always a
		cachedname = cached->name; 
		if ( cachedname && !Q_stricmp( cachedname, name ) ) {
			return cached;
		}
		sv_modelTiki[ modelindex ] = NULL;
	}

	CL_ProcessLoadingScreen( "" );
	sv_modelTiki[ modelindex ] = TIKI_RegisterTikiFlags( name, qtrue );

	return sv_modelTiki[ modelindex ];
}

qboolean PF_IsModel( int index )
{
	return TIKI_Handle_For_ModelIndex( index ) != NULL;
}

void PF_setviewmodel( gentity_t *ent, const char *name )
{
	int         modelindex;
	clipHandle_t h;
	vec3_t      mins, maxs;
	size_t      len;

	if ( !name ) {
		Com_Error( ERR_DROP, "PF_setmodel: NULL" );
	}

	if ( *name == '*' ) {
		modelindex = atoi( name + 1 );
		ent->s.modelindex = modelindex;
		h = CM_InlineModel( modelindex );
		CM_ModelBounds( h, mins, maxs );
		VectorCopy( mins, ent->mins );
		VectorCopy( maxs, ent->maxs );
		ent->bmodel = qtrue;
		SV_LinkEntity( ent );
		return;
	}

	ent->s.viewmodelindex = SV_ModelIndex( name );
	ent->bmodel = qfalse;

	len = strlen( name );
	if ( len < 3 || Q_stricmp( name + len - 3, "tik" ) ) {
		sv_modelTiki[ ent->s.viewmodelindex ] = NULL;
	} else if ( !sv_modelTiki[ ent->s.viewmodelindex ] ) {
		CL_ProcessLoadingScreen( "" );
		sv_modelTiki[ ent->s.viewmodelindex ] = TIKI_GetTiki( TIKI_RegisterTiki( name ) );
	}
}

qboolean PF_setmodel( gentity_t *ent, const char *name )
{
	clipHandle_t h;
	vec3_t      mins, maxs;
	int         modelindex;
	size_t      len;

	if ( !name ) {
		Com_Error( ERR_DROP, "PF_setmodel: NULL" );
	}

	if ( *name == '*' ) {
		modelindex = atoi( name + 1 );
		ent->s.modelindex = modelindex;
		h = CM_InlineModel( modelindex );
		CM_ModelBounds( h, mins, maxs );
		VectorCopy( mins, ent->mins );
		VectorCopy( maxs, ent->maxs );
		ent->bmodel = qtrue;
		SV_LinkEntity( ent );
		return qtrue;
	}

	modelindex = SV_ModelIndex( name );
	ent->s.modelindex = modelindex;
	ent->bmodel = qfalse;

	len = strlen( name );
	if ( len < 3 || Q_stricmp( name + len - 3, "tik" ) ) {
		sv_modelTiki[ modelindex ] = NULL;
	} else if ( !sv_modelTiki[ modelindex ] ) {
		CL_ProcessLoadingScreen( "" );
		sv_modelTiki[ modelindex ] = TIKI_GetTiki( TIKI_RegisterTiki( name ) );
	}

	return qtrue;
}

void PF_clearmodel( gentity_t *ent )
{
	PF_setmodel( ent, "" );
}

void SV_GameSendServerCommand( int clientNum, const char *text, ... ) Q_PRINTF_FUNC(2, 3);
void SV_GameSendServerCommand( int clientNum, const char *text, ... )
{
	char tmp[1] = { 0 };
	char* buffer;
    size_t length = 1;
    va_list va;

    va_start(va, text);
    length = vsnprintf(tmp, 0, text, va);
    va_end(va);

    if (!length || length == -1) {
        return;
    }

	buffer = (char*)alloca(length + 1);

    va_start(va, text);
    vsnprintf(buffer, length + 1, text, va);
    va_end(va);

    if (clientNum == -1) {
        SV_SendServerCommand(NULL, "%s", buffer);
    } else {
        if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
            return;
        }
        SV_SendServerCommand(svs.clients + clientNum, "%s", buffer);
    }
}

void SV_GameDropClient( int clientNum, const char *reason ) {
	if ( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
		return;
	}
	SV_DropClient( svs.clients + clientNum, reason, qfalse );	
}

void SV_SetBrushModel( gentity_t *ent, const char *name ) {
	clipHandle_t	h;
	vec3_t			mins, maxs;

	if( !name ) {
		Com_Error( ERR_DROP, "SV_SetBrushModel: NULL" );
	}

	if( name[ 0 ] != '*' ) {
		Com_Error( ERR_DROP, "SV_SetBrushModel: %s isn't a brush model", name );
	}

	ent->s.modelindex = atoi( name + 1 );
	h = CM_InlineModel( ent->s.modelindex );
	CM_ModelBounds( h, mins, maxs );
	VectorCopy( mins, ent->mins );
	VectorCopy( maxs, ent->maxs );
	ent->bmodel = qtrue;

	if( !ent->contents )
		ent->contents = -1; 

	SV_LinkEntity( ent );
}

void SV_AdjustAreaPortalState( gentity_t *ent, qboolean open ) {
	svEntity_t	*svEnt;

	if ( !ent || ent->s.number >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "SV_SvEntityForGentity: bad gEnt" );
	}

	svEnt = SV_SvEntityForGentity( ent );
	if( svEnt->areanum2 == -1 ) {
		return;
	}
	CM_AdjustAreaPortalState( svEnt->areanum, svEnt->areanum2, open );
}

qboolean SV_EntityContact( vec3_t mins, vec3_t maxs, gentity_t *gEnt )
{
	clipHandle_t ch;
	trace_t trace;

	ch = SV_ClipHandleForEntity( gEnt );
	CM_TransformedBoxTrace( &trace, vec3_origin, vec3_origin, mins, maxs, ch, -1, gEnt->s.origin, gEnt->currentAngles, qfalse );
	return trace.startsolid;
}


void SV_GetServerinfo( char *buffer, int bufferSize ) {
	if( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetServerinfo: bufferSize == %i", bufferSize );
	}
	Q_strncpyz( buffer, Cvar_InfoString( CVAR_SERVERINFO ), bufferSize );
}

void SV_LocateGameData( gentity_t *gEnts, int numGEntities, int sizeofGEntity_t,
	playerState_t *clients, int sizeofGameClient ) {
	sv.gentities = ( gentity_t * )gEnts;
	sv.gentitySize = sizeofGEntity_t;
	sv.num_entities = numGEntities;

	sv.gameClients = clients;
	sv.gameClientSize = sizeofGameClient;
}

void SV_SetFarPlane( int farplane, qboolean farplane_cull )
{
	if( farplane )
	{
		sv.farplane = ( farplane + 32 ) * ( farplane + 32 );
		sv.farplane_cull = farplane_cull;
	}
	else
	{
		sv.farplane = 0;
		sv.farplane_cull = 0;
	}
}

void SV_SetSkyPortal( qboolean skyportal )
{
	sv.skyportal = skyportal;
}

int SV_GetLightingGroup( const char *group_name )
{
	return CM_GetLightingGroup( group_name );
}

void SV_SetDynamicLight( int dynamic_light, float intensity )
{
	sv.worldState.dynamic_light_intensities[ dynamic_light ] =
		(byte)(int)( DYNAMIC_LIGHT_MAX_INTENSITY * intensity );
}

void SV_SetDynamicLightDefault( int dynamic_light, float intensity )
{
	if ( intensity >= 0.0f ) {
		sv.worldState.dynamic_light_default_intensities[ dynamic_light ] =
			(byte)(int)( DYNAMIC_LIGHT_MAX_INTENSITY * intensity );
	} else {
		sv.worldState.dynamic_light_default_intensities[ dynamic_light ] =
			DYNAMIC_LIGHT_NO_DEFAULT;
	}
}

void SV_SetWindDirection( vec3_t direction )
{
	sv.worldState.wind_direction[ 0 ] = direction[ 0 ];
	sv.worldState.wind_direction[ 1 ] = direction[ 1 ];
	sv.worldState.wind_direction[ 2 ] = direction[ 2 ];
}

void SV_SetWindIntensity( float wind_intensity )
{
	sv.worldState.wind_intensity = wind_intensity;
}

void SV_SetWeatherInfo( weather_t type, int intensity )
{
	sv.worldState.weather_type = type;
	sv.worldState.weather_intensity = intensity;
}

void SV_SetTimeScale( float time_scale )
{
	sv.worldState.time_scale = time_scale;
}

int SV_FailedCondition( const char *name )
{
	SV_SetConfigstring( CS_FAILEDCONDITION, name );
	return CS_FAILEDCONDITION;
}

qboolean SV_FS_Exists( const char *filename )
{
	if ( !filename || !filename[ 0 ] ) {
		return qfalse;
	}

	return FS_ReadFileEx( filename, NULL, qtrue ) >= 0 ? qtrue : qfalse;
}

int SV_GameGetNumFreeReliableServerCommands( int clientNum )
{
	const client_t *svcl;

	if ( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
		return 0;
	}

	svcl = &svs.clients[ clientNum ];

	return ( MAX_RELIABLE_COMMANDS - 1 ) - ( svcl->reliableSequence - svcl->reliableAcknowledge );
}

qboolean SV_IsClientActive( const gentity_t *ent )
{
	if ( !ent ) {
		return qfalse;
	}

	if ( ent->s.number < 0 || ent->s.number >= sv_maxclients->integer ) {
		return qfalse;
	}

	return svs.clients[ ent->s.number ].state == CS_ACTIVE ? qtrue : qfalse;
}

void SV_GetUsercmd( int clientNum, usercmd_t *cmd ) {
	if( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
		Com_Error( ERR_DROP, "SV_GetUsercmd: bad clientNum:%i", clientNum );
	}
	*cmd = svs.clients[ clientNum ].lastUsercmd;
}

void PF_centerprintf( const gentity_t *ent, CenterPrintImportance importance, const char *fmt, ... ) Q_PRINTF_FUNC(3, 4);
void PF_centerprintf( const gentity_t *ent, CenterPrintImportance importance, const char *fmt, ... )
{
	va_list va;
	char msg[ 2048 ];
	client_t *client;

	if( ent->s.number >= sv_maxclients->integer )
	{
		return;
	}

	client = &svs.clients[ ent->s.number ];

	if( client->stringToPrint[ 0 ] && client->currentImportance > importance )
	{
		return;
	}

	va_start( va, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, va );
	va_end( va );

	if( strlen( msg ) > 256 ) {
		Com_DPrintf( "Centerprint text exceeds buffer size\n" );
	}

	strncpy( client->stringToPrint, msg, sizeof( client->stringToPrint ) );
	client->currentImportance = importance;
}

void PF_locationprintf( const gentity_t *ent, int x, int y, const char *fmt, ... ) Q_PRINTF_FUNC(4, 5);
void PF_locationprintf( const gentity_t *ent, int x, int y, const char *fmt, ... )
{
	va_list va;
	char msg[ 2048 ];

	if( ent->s.number >= sv_maxclients->integer )
	{
		return;
	}

	va_start( va, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, va );
	va_end( va );

	if( strlen( msg ) > 256 ) {
		Com_DPrintf( "Locationprint text exceeds buffer size\n" );
	}

	strncpy( svs.clients[ ent->s.number ].stringToPrint, msg, sizeof(svs.clients[ent->s.number].stringToPrint) );
	svs.clients[ ent->s.number ].XOffset = x;
	svs.clients[ ent->s.number ].YOffset = y;
	svs.clients[ ent->s.number ].locprint = qtrue;
}

void SV_AddGameCommand( const char *cmdName, xcommand_t function ) {
	Cmd_AddCommand( cmdName, function );
}

void *SV_Malloc( size_t size )
{
	return Z_TagMalloc( size, TAG_GAME );
}

void SV_Free( void* ptr )
{
	Z_Free( ptr );
}

void SV_ShutdownGameProgs( void ) {
	if( !ge ) {
		return;
	}

	ge->Shutdown();
	Sys_UnloadGame();

	Z_FreeTags(TAG_GAME);

	ge = NULL;
}

void PF_UI_PopMenu( int iClient, int i )
{
	SV_GameSendServerCommand( iClient, "stufftext \"popmenu %i\"\n", i );
}

void PF_UI_ShowMenu( int iClient, const char *name, qboolean bForce )
{
	SV_GameSendServerCommand( iClient, "stufftext \"showmenu %s %i\"\n", name,  bForce );
}

void PF_UI_HideMenu( int iClient, const char *name, qboolean bForce )
{
	SV_GameSendServerCommand( iClient, "stufftext \"hidemenu %s %i\"\n", name, bForce );
}

void PF_UI_PushMenu( int iClient, const char *name )
{
	SV_GameSendServerCommand( iClient, "stufftext \"pushmenu %s\"\n", name );
}

void PF_UI_HideMouse_f( int iClient )
{
	SV_GameSendServerCommand( iClient, "stufftext \"ui_hidemouse\"\n" );
}

void PF_UI_ShowMouse_f( int iClient )
{
	SV_GameSendServerCommand( iClient, "stufftext \"ui_showmouse\"\n" );
}

int PF_Key_StringToKeynum( const char *str )
{
	if( com_cl_running->integer ) {
		return PF_Key_StringToKeynum( str );
	} else {
		return 0;
	}
}

const char *PF_Key_KeynumToBindString( int keynum )
{	if( com_cl_running->integer ) {
		return Key_KeynumToBindString( keynum );
	} else {
		return 0;
	}
}

void PF_Key_GetKeysForCommand( const char *command, int *key1, int *key2 )
{
	if( com_cl_running->integer ) {
		Key_GetKeysForCommand( command, key1, key2 );
	} else {
		*key1 = 0;
		*key2 = 0;
	}
}


unsigned int PF_SV_Client_NumPendingCommands(int clientNum)
{
	if ( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
		return 0;
	}

	return SV_Client_GetNumPendingCommands(svs.clients + clientNum);
}

unsigned int PF_SV_Client_MaxPendingCommands(int clientNum)
{
	if ( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
		return 0;
	}

	return SV_Client_GetMaxPendingCommands(svs.clients + clientNum);
}


static qboolean sv_warned_outoflane[6];

static void SV_OutOfLaneWarn(int slot, const char *name)
{
    if (sv_warned_outoflane[slot]) {
        return;
    }
    sv_warned_outoflane[slot] = qtrue;
    Com_DPrintf("SV_InitGameProgs: gi.%s is not backed by this engine yet\n", name);
}

int PF_Test_Unimplemented(int parm0, char *parm1, vec3_t parm2, vec3_t parm3)
{
    (void)parm0;
    (void)parm1;
    (void)parm2;
    (void)parm3;
    Com_Error(ERR_DROP, "gi.Test: the SDK's test hook has no retail engine implementation");
    return 0;
}

static dtiki_t *PF_TikiForModelIndexOrNULL(int modelindex)
{
    if (modelindex <= 0) {
        return NULL;
    }
    return TIKI_Handle_For_ModelIndex(modelindex);
}

int PF_NumAnims_mi(int modelindex)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_NumAnims(tiki) : 0;
}

int PF_NumSkins_mi(int modelindex)
{
    (void)modelindex;
    return 0;
}

int PF_NumSurfaces_mi(int modelindex)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_NumSurfaces(tiki) : 0;
}

int PF_NumTags_mi(int modelindex)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_NumTags(tiki) : 0;
}

int PF_NumMorphs_mi(int modelindex)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_TIKI_NumMorphs(tiki) : 0;
}

qboolean PF_InitCommands_mi(int modelindex, tiki_cmd_t *tiki_cmd)
{
    dtiki_t   *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    dtikicmd_t *pcmd;
    int        i, j;

    if (!tiki || !tiki->a || !tiki_cmd) {
        return qfalse;
    }

    tiki_cmd->num_cmds = 0;
    for (i = 0; i < tiki->a->num_server_initcmds; i++) {
        pcmd = &tiki->a->server_initcmds[i];

        if (tiki_cmd->num_cmds >= TIKI_CMD_MAX_CMDS) {
            TIKI_Error("PF_InitCommands: too many init commands in %s.\n", tiki->a->name);
            return qfalse;
        }

        if (pcmd->num_args > TIKI_CMD_MAX_ARGS) {
            TIKI_Error("PF_InitCommands: init command in %s has too many args (%d).\n", tiki->a->name, pcmd->num_args);
            continue;
        }

        tiki_cmd->cmds[tiki_cmd->num_cmds].num_args = pcmd->num_args;
        for (j = 0; j < pcmd->num_args; j++) {
            tiki_cmd->cmds[tiki_cmd->num_cmds].args[j] = pcmd->args[j];
        }
        tiki_cmd->num_cmds++;
    }


    return qtrue;
}

void PF_CalculateBounds_mi(int modelindex, float scale, vec3_t mins, vec3_t maxs)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);

    if (!tiki) {
        return;
    }
    PF_CalculateBounds(tiki, scale, mins, maxs);
}

const char *PF_Anim_NameForNum_mi(int modelindex, int animnum)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Anim_NameForNum(tiki, animnum) : NULL;
}

int PF_Anim_NumForName_mi(int modelindex, const char *name)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    int      num  = tiki ? PF_Anim_NumForName(tiki, name) : -1;


    return num;
}

int PF_Anim_Random_mi(int modelindex, const char *name)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Anim_Random(tiki, name) : -1;
}

int PF_Anim_NumFrames_mi(int modelindex, int animnum)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Anim_NumFrames(tiki, animnum) : 0;
}

float PF_Anim_Time_mi(int modelindex, int animnum)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Anim_Time(tiki, animnum) : 0;
}

void PF_Anim_Delta_mi(int modelindex, int animnum, vec3_t delta)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);

    if (tiki) {
        PF_Anim_Delta(tiki, animnum, delta);
    } else {
        VectorClear(delta);
    }
}

void PF_Anim_AbsoluteDelta_mi(int modelindex, int animnum, vec3_t delta)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);

    if (tiki) {
        TIKI_Anim_AbsoluteDelta(tiki, animnum, delta);
    } else {
        VectorClear(delta);
    }
}

int PF_Anim_Flags_mi(int modelindex, int animnum)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Anim_Flags(tiki, animnum) : 0;
}

qboolean PF_Anim_HasCommands_mi(int modelindex, int animnum)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Anim_HasCommands(tiki, animnum) : qfalse;
}

qboolean PF_Frame_Commands_mi(int modelindex, int animnum, int framenum, tiki_cmd_t *tiki_cmd)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Frame_Commands(tiki, animnum, framenum, tiki_cmd) : qfalse;
}

// @0x000FFE70, the PER-FRAME delta of one frame -- not TIKI_Anim_Delta
void PF_Frame_Delta_mi(int modelindex, int animnum, int framenum, vec3_t delta)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);

    if (tiki) {
        TIKI_Frame_Delta(tiki, animnum, framenum, delta);
    }
}

// NOT TIKI_Anim_Time @0x000FF030, which is frametime * (numFrames [- 1]).
float PF_Frame_Time_mi(int modelindex, int animnum, int framenum)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? TIKI_Frame_Time(tiki, animnum, framenum) : 0;
}

void PF_Frame_Bounds_mi(int modelindex, int animnum, int framenum, float scale, vec3_t mins, vec3_t maxs)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);

    if (tiki) {
        PF_Frame_Bounds(tiki, animnum, framenum, scale, mins, maxs);
    }
}

int PF_Surface_NameToNum_mi(int modelindex, const char *name)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Surface_NameToNum(tiki, name) : -1;
}

const char *PF_Surface_NumToName_mi(int modelindex, int num)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Surface_NumToName(tiki, num) : NULL;
}

int PF_Surface_Flags_mi(int modelindex, int num)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Surface_Flags(tiki, num) : 0;
}

int PF_Surface_NumSkins_mi(int modelindex, int num)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Surface_NumSkins(tiki, num) : 0;
}

int PF_Morph_NumForName_mi(int modelindex, const char *name)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Morph_NumForName(tiki, name) : -1;
}

const char *PF_Morph_NameForNum_mi(int modelindex, int num)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_TIKI_Morph_NameForNum(tiki, num) : NULL;
}

dtikimorphtarget_t *PF_GetExpression_mi(int modelindex, const char *expression_name, int *number_of_morph_targets)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);

    if (tiki) {
        return TIKI_GetExpression(tiki, expression_name, number_of_morph_targets);
    }

    *number_of_morph_targets = 0;
    return NULL;
}

int PF_Tag_NumForName_mi(int modelindex, const char *name)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Tag_NameToNum(tiki, name) : -1;
}

const char *PF_Tag_NameForNum_mi(int modelindex, int num)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Tag_NumToName(tiki, num) : NULL;
}

orientation_t PF_Tag_Orientation_mi(
    int modelindex, int anim, int frame, int num, float scale, int *bone_tag, vec4_t *bone_quat
)
{
    orientation_t or;

    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    if (!tiki) {
        Com_WDPrintf("PF_Tag_Orientation: not a tiki model\n");
        memset(&or, 0, sizeof(or));
        AnglesToAxis(vec3_origin, or.axis);
        return or;
    }
    TIKI_Tag_Orientation(
        &or, TIKI_HandleForTiki(tiki), anim, frame, num, scale, bone_tag, (const vec4_t *)bone_quat
    );
    return or;
}

orientation_t PF_Tag_OrientationEx_mi(
    int modelindex, int anim, int frame, int num, float scale, int *bone_tag, vec4_t *bone_quat, int crossblend_anim,
    int crossblend_frame, float crossblend_lerp, qboolean uselegs, qboolean usetorso, int torso_anim,
    int torso_frame, int torso_crossblend_anim, int torso_crossblend_frame, float torso_crossblend_lerp
)
{
    orientation_t or;

    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    if (!tiki) {
        Com_WDPrintf("PF_Tag_OrientationEx: not a tiki model\n");
        memset(&or, 0, sizeof(or));
        AnglesToAxis(vec3_origin, or.axis);
        return or;
    }
    TIKI_Tag_OrientationEx(
        &or, TIKI_HandleForTiki(tiki), anim, frame, num, scale, bone_tag, (const vec4_t *)bone_quat, crossblend_anim,
        crossblend_frame, crossblend_lerp, uselegs, usetorso, torso_anim, torso_frame, torso_crossblend_anim,
        torso_crossblend_frame, torso_crossblend_lerp
    );
    return or;
}

int PF_Bone_GetParentNum_mi(int modelindex, int bonenum)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    if (!tiki) {
        return -1;
    }
    return TIKI_Bone_GetParentNum(tiki, bonenum);
}

qboolean PF_Alias_Add_mi(int modelindex, const char *alias, const char *name, const char *parameters)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Alias_Add(tiki, alias, name, parameters) : qfalse;
}

const char *PF_Alias_FindRandom_mi(int modelindex, const char *alias)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Alias_FindRandom(tiki, alias, NULL) : NULL;
}

const char *PF_Alias_Find_mi(int modelindex, const char *alias)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Alias_Find(tiki, alias) : NULL;
}

void PF_Alias_Dump_mi(int modelindex)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    if (tiki) {
        PF_Alias_Dump(tiki);
    }
}

void PF_Alias_Clear_mi(int modelindex)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    if (tiki) {
        PF_Alias_Clear(tiki);
    }
}

const char *PF_Alias_FindDialog_mi(int modelindex, const char *alias, int random, int entity_number)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Alias_FindDialog(tiki, alias, random, entity_number) : NULL;
}

const char *PF_Alias_FindSpecificAnim_mi(int modelindex, const char *name)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Alias_FindSpecificAnim(tiki, name) : NULL;
}

qboolean PF_Alias_CheckLoopAnim_mi(int modelindex, const char *name)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Alias_CheckLoopAnim(tiki, name) : qfalse;
}

void *PF_Alias_GetList_mi(int modelindex)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_Alias_GetList(tiki) : NULL;
}

void PF_Alias_UpdateDialog_mi(
    int modelindex, const char *alias, int number_of_times_played, byte been_played_this_loop, int last_time_played
)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);

    if (tiki) {
        PF_Alias_UpdateDialog(tiki, alias, number_of_times_played, been_played_this_loop, last_time_played);
    }
}

void PF_Alias_AddActorDialog_mi(
    int modelindex, const char *alias, int actor_number, int number_of_times_played, byte been_played_this_loop,
    int last_time_played
)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    if (tiki) {
        PF_Alias_AddActorDialog(tiki, alias, actor_number, number_of_times_played, been_played_this_loop,
            last_time_played);
    }
}

const char *PF_NameForNum_mi(int modelindex)
{
    dtiki_t *tiki = PF_TikiForModelIndexOrNULL(modelindex);
    return tiki ? PF_NameForNum(tiki) : "";
}

int PF_PointBrushNum(const vec3_t point, int passEntityNum)
{
    gentity_t *ent;

    if (passEntityNum <= 0 || passEntityNum >= ENTITYNUM_NONE) {
        return -1;
    }
    ent = SV_GentityNum(passEntityNum);
    if (!ent || !ent->bmodel) {
        return -1;
    }
    return CM_PointBrushNum(point, CM_InlineModel(ent->s.modelindex));
}

int SV_ObjectiveNameIndex(const char *name)
{
    return SV_FindIndex(name, 1970, 5, 1);
}

void PF_ReloadTiki(const char *modelstr)
{
    TIKI_ReloadTiki(modelstr);
}

void SV_FS_DeleteFile(const char *filename)
{
    char ospath[MAX_OSPATH];

    Com_sprintf(ospath, sizeof(ospath), "%s/%s", com_homepath->string, filename);
    FS_Remove(ospath);
}

const char *PF_GlobalAlias_Find(const char *alias)
{
    return Alias_Find(alias);
}

const char *PF_GlobalAlias_FindRandom(const char *alias)
{
    return Alias_FindRandom(alias);
}

void PF_Sound_Retail(
    vec3_t *org, int entnum, int channel, const char *sound_name, float volume, float attenuation,
    float pitch_modifier, qboolean onlySendToSameEntity
)
{
    SV_Sound(org, entnum, channel, sound_name, volume, attenuation, pitch_modifier, 0.0f, onlySendToSameEntity);
}


void PF_WidgetPrintf(const char *widgetName, const char *fmt, ...)
{
    SV_OutOfLaneWarn(0, "WidgetPrintf");
}


// getters below are retail's, not the shipped subset.

#define VM_MAX_MODES 32
#define VM_SCRATCH   VM_MAX_MODES
#define VM_SLOTS     (VM_MAX_MODES + 1)
#define VM_NAME_LEN  64
#define VM_MAX_CLASS 8

static int   vm_count;
static int   vm_loaded;
static char  vm_name[VM_SLOTS][VM_NAME_LEN];         
static int   vm_slot[VM_SLOTS];                      
static int   vm_sendIn[VM_SLOTS];                    
static int   vm_sendNotIn[VM_SLOTS];                 
static int   vm_hasColor[VM_SLOTS];                  
static float vm_color[VM_SLOTS][3];                  
static int   vm_hasShader[VM_SLOTS];                 
static char  vm_shader[VM_SLOTS][VM_NAME_LEN];       
static int   vm_hasAlpha[VM_SLOTS];                  
static float vm_alpha[VM_SLOTS];                     
static int   vm_fullbright[VM_SLOTS];                
static int   vm_hasScreenBlend[VM_SLOTS];            
static float vm_sbColor[VM_SLOTS][3];                
static float vm_sbAlpha[VM_SLOTS];                   
static int   vm_sbAdditive[VM_SLOTS];                
static int   vm_hasHud[VM_SLOTS];                    
static char  vm_hud[VM_SLOTS][VM_NAME_LEN];          
static int   vm_effectAll[VM_SLOTS];                 
static char  vm_effectClass[VM_SLOTS][VM_MAX_CLASS][VM_NAME_LEN]; 
static int   vm_numEffectClass[VM_SLOTS];

static void vm_init_mode(int m)
{
    Q_strncpyz(vm_name[m], "", sizeof(vm_name[m]));
    vm_slot[m]             = m;       
    vm_sendIn[m]           = 1;       
    vm_sendNotIn[m]        = 0;
    vm_hasColor[m]         = 0;
    vm_color[m][0] = vm_color[m][1] = vm_color[m][2] = 1.0f;
    vm_hasShader[m]        = 0;
    vm_alpha[m]            = 0;
    vm_hasAlpha[m]         = 0;
    vm_fullbright[m]       = 0;
    vm_hasScreenBlend[m]   = 0;
    vm_sbColor[m][0] = vm_sbColor[m][1] = vm_sbColor[m][2] = 1.0f;
    vm_sbAlpha[m]          = 0;
    vm_sbAdditive[m]       = 0;
    vm_hasHud[m]           = 0;
    vm_effectAll[m]        = 0;
    vm_numEffectClass[m]   = 0;
}

static void vm_load_mode_info(int m, const char **scan)
{
    const char *tok;

    for (;;) {
        tok = COM_ParseExt((char **)scan, qtrue);
        if (!tok || !*tok) break;                     

        if (!Q_stricmp(tok, "sendWhenInMode")) {                       
            const char *v = COM_ParseExt((char **)scan, qfalse);
            if (v && *v) vm_sendIn[m] = atoi(v) ? 1 : 0;
            else Com_WDPrintf("No value specified for sendWhenInMode in viewmode %s\n", vm_name[m]);
        } else if (!Q_stricmp(tok, "sendWhenNotInMode")) {             
            const char *v = COM_ParseExt((char **)scan, qfalse);
            if (v && *v) vm_sendNotIn[m] = atoi(v) ? 1 : 0;
            else Com_WDPrintf("No value specified for sendWhenNotInMode in viewmode %s\n", vm_name[m]);
        } else if (!Q_stricmp(tok, "useFullBright")) {                 
            const char *v = COM_ParseExt((char **)scan, qfalse);
            if (v && *v) vm_fullbright[m] = atoi(v) ? 1 : 0;
            else         vm_fullbright[m] = 1;
        } else if (!Q_stricmp(tok, "effectAll")) {                     
            const char *v = COM_ParseExt((char **)scan, qfalse);
            if (v && *v) vm_effectAll[m] = atoi(v) ? 1 : 0;
            else Com_WDPrintf("No value specified for effectAll in viewmode %s\n", vm_name[m]);
        } else if (!Q_stricmp(tok, "effectClass")) {                   
            const char *v = COM_ParseExt((char **)scan, qfalse);
            if (v && *v) {
                if (m != VM_SCRATCH && vm_numEffectClass[m] < VM_MAX_CLASS) {
                    Q_strncpyz(vm_effectClass[m][vm_numEffectClass[m]], v, VM_NAME_LEN);
                    vm_numEffectClass[m]++;
                }
            } else {
                Com_WDPrintf("No value specified for effectClass in viewmode %s\n", vm_name[m]);
            }
        } else if (!Q_stricmp(tok, "shader")) {                        
            const char *v = COM_ParseExt((char **)scan, qfalse);
            if (v && *v) { Q_strncpyz(vm_shader[m], v, sizeof(vm_shader[m])); vm_hasShader[m] = 1; }
            else Com_WDPrintf("No value specified for shader in viewmode %s\n", vm_name[m]);
        } else if (!Q_stricmp(tok, "hud")) {                           
            const char *v = COM_ParseExt((char **)scan, qfalse);
            if (v && *v) { Q_strncpyz(vm_hud[m], v, sizeof(vm_hud[m])); vm_hasHud[m] = 1; }
            else Com_WDPrintf("No value specified for hud in viewmode %s\n", vm_name[m]);
        } else if (!Q_stricmp(tok, "alpha")) {                         
            const char *v = COM_ParseExt((char **)scan, qfalse);
            if (v && *v) { vm_alpha[m] = atof(v); vm_hasAlpha[m] = 1; }
            else Com_WDPrintf("No value specified for alpha in viewmode %s\n", vm_name[m]);
        } else if (!Q_stricmp(tok, "color")) {                         
            const char *v = COM_ParseExt((char **)scan, qfalse);
            if (v && *v) vm_color[m][0] = atof(v);
            else Com_WDPrintf("No value specified for red for color in viewmode %s\n", vm_name[m]);
            v = COM_ParseExt((char **)scan, qfalse);
            if (v && *v) vm_color[m][1] = atof(v);
            else Com_WDPrintf("No value specified for green for color in viewmode %s\n", vm_name[m]);
            v = COM_ParseExt((char **)scan, qfalse);
            if (v && *v) vm_color[m][2] = atof(v);
            else Com_WDPrintf("No value specified for blue for color in viewmode %s\n", vm_name[m]);
            vm_hasColor[m] = 1;
        } else if (!Q_stricmp(tok, "screenblend")) {                   
            const char *v = COM_ParseExt((char **)scan, qfalse);
            if (v && *v) vm_sbColor[m][0] = atof(v);
            else Com_WDPrintf("No value specified for screenblend red for color in viewmode %s\n", vm_name[m]);
            v = COM_ParseExt((char **)scan, qfalse);
            if (v && *v) vm_sbColor[m][1] = atof(v);
            else Com_WDPrintf("No value specified for screenblend green for color in viewmode %s\n", vm_name[m]);
            v = COM_ParseExt((char **)scan, qfalse);
            if (v && *v) vm_sbColor[m][2] = atof(v);
            else Com_WDPrintf("No value specified for screenblend blue for color in viewmode %s\n", vm_name[m]);
            v = COM_ParseExt((char **)scan, qfalse);
            if (v && *v) vm_sbAlpha[m] = atof(v);
            else Com_WDPrintf("No value specified for screenblend alpha for color in viewmode %s\n", vm_name[m]);
            v = COM_ParseExt((char **)scan, qfalse);
            if (v && *v) vm_sbAdditive[m] = atoi(v) ? 1 : 0;
            else Com_WDPrintf("No value specified for screenblend additive for color in viewmode %s\n", vm_name[m]);
            vm_hasScreenBlend[m] = 1;
        } else if (!Q_stricmp(tok, "end")) {                           
            return;
        } else {
            break;
        }
    }
    Com_WDPrintf("No end statement found for %s\n", vm_name[m]);
}

static void vm_parse_mode_file(const char *path)
{
    void       *buf = NULL;
    const char *scan;
    const char *tok;

    FS_ReadFileEx(path, &buf, qtrue);
    if (!buf) return;
    scan = (const char *)buf;
    for (;;) {
        tok = COM_ParseExt((char **)&scan, qtrue);
        if (!tok || !*tok) break;
        if (!Q_stricmp(tok, "begin")) {
            const char *nm = COM_ParseExt((char **)&scan, qfalse);
            int         m;

            if (!nm || !*nm) break;              
            m = (vm_count < VM_MAX_MODES) ? vm_count++ : VM_SCRATCH;
            vm_init_mode(m);
            if (nm) Q_strncpyz(vm_name[m], nm, sizeof(vm_name[m]));
            vm_load_mode_info(m, &scan);
        }
    }
    FS_FreeFile(buf);
}

void LoadViewModes(void)
{
    char path[64];
    int  i;

    if (vm_loaded) return;
    vm_loaded = 1;
    vm_count  = 0;
    Com_Printf("Loading viewmodes\n");
    for (i = 0; i < 10; i++) {
        if (i == 0) {
            Q_strncpyz(path, "global/viewmodes.txt", sizeof(path));
        } else {
            Com_sprintf(path, sizeof(path), "global/viewmodes%d.txt", i);
        }
        vm_parse_mode_file(path);
    }
}

unsigned int PF_GetViewModeMask(const char *viewModeName);
unsigned int PF_GetViewModeClassMask(const char *className);
qboolean PF_GetViewModeSendInMode(unsigned int viewModeMask);
qboolean PF_GetViewModeSendNotInMode(unsigned int viewModeMask);
qboolean PF_GetViewModeScreenBlend(unsigned int viewModeMask, vec3_t color, float *alpha, qboolean *additive);

int GetNumViewModes(void)
{
    return vm_count;
}

int GetViewModeMask(const char *name)
{
    return (int)PF_GetViewModeMask(name);
}

int GetViewModeSendInMode(unsigned mask)        { return PF_GetViewModeSendInMode(mask); }
int GetViewModeSendNotInMode(unsigned mask)     { return PF_GetViewModeSendNotInMode(mask); }

int GetViewModeUseFullbright(unsigned mask)
{
    int i;
    for (i = 0; i < vm_count; i++) {
        if ((mask & (1u << i)) && vm_fullbright[i]) return 1;
    }
    return 0;
}

int GetViewModeHud(unsigned mask, const char **hudName)
{
    int i;
    for (i = 0; i < vm_count; i++) {
        if ((mask & (1u << i)) && vm_hasHud[i]) {
            *hudName = vm_hud[i];
            return 1;
        }
    }
    *hudName = NULL;
    return 0;
}

int GetViewModeEntityColor(unsigned mask, float rgb[3])
{
    int i;
    int found = 0;
    rgb[0] = rgb[1] = rgb[2] = 1.0f;
    for (i = 0; i < vm_count; i++) {
        if ((mask & (1u << i)) && vm_hasColor[i]) {
            found = 1;
            rgb[0] += vm_color[i][0];
            rgb[1] += vm_color[i][1];
            rgb[2] += vm_color[i][2];
        }
    }
    return found;
}

int GetViewModeEntityShader(unsigned mask, const char **name)
{
    int i;
    for (i = 0; i < vm_count; i++) {
        if ((mask & (1u << i)) && vm_hasShader[i]) {
            *name = vm_shader[i];
            return 1;
        }
    }
    *name = NULL;
    return 0;
}

int GetViewModeEntityAlpha(unsigned mask, float *alpha)
{
    int i;
    for (i = 0; i < vm_count; i++) {
        if ((mask & (1u << i)) && vm_hasAlpha[i]) {
            *alpha = vm_alpha[i];
            return 1;
        }
    }
    return 0;
}

unsigned int PF_GetViewModeMask(const char *viewModeName)
{
    int i;
    if (!Q_stricmp(viewModeName, "none") || !Q_stricmp(viewModeName, "normal")) {
        return 0;
    }
    for (i = 0; i < vm_count; i++) {
        if (viewModeName && strcmp(vm_name[i], viewModeName) == 0) {
            return 1u << i;
        }
    }
    return 0;
}

unsigned int PF_GetViewModeClassMask(const char *className)
{
    unsigned int mask = 0;
    int i, j;
    for (i = 0; i < vm_count; i++) {
        if (vm_effectAll[i]) {
            mask |= 1u << i;
            continue;
        }
        for (j = 0; j < vm_numEffectClass[i]; j++) {
            if (!Q_stricmp(className, vm_effectClass[i][j])) {
                mask |= 1u << i;
            }
        }
    }
    return mask;
}

qboolean PF_GetViewModeSendInMode(unsigned int viewModeMask)
{
    int i;
    for (i = 0; i < vm_count; i++) {
        /* retail: the mask is tested against the RECORD's own slot (record+28),
           not the loop index -- see the vm_slot note above. */
        if (vm_slot[i] >= 0 && vm_slot[i] < 32
            && (viewModeMask & (1u << vm_slot[i])) && vm_sendIn[i]) return qtrue;
    }
    return qfalse;
}

qboolean PF_GetViewModeSendNotInMode(unsigned int viewModeMask)
{
    int i;
    for (i = 0; i < vm_count; i++) {
        /* GetViewModeSendNotInMode @0x00110EF0: `a1 & (1 << *(v4 + 28))' -- the
           record's own slot, never the loop index. */
        if (vm_slot[i] >= 0 && vm_slot[i] < 32
            && (viewModeMask & (1u << vm_slot[i])) && vm_sendNotIn[i]) return qtrue;
    }
    return qfalse;
}

qboolean PF_GetViewModeScreenBlend(unsigned int viewModeMask, vec3_t color, float *alpha, qboolean *additive_shadow1)
{
    int i;
    for (i = 0; i < vm_count; i++) {
        if ((viewModeMask & (1u << i)) && vm_hasScreenBlend[i]) {
            VectorCopy(vm_sbColor[i], color);
            if (alpha) *alpha = vm_sbAlpha[i];
            if (additive_shadow1) *additive_shadow1 = vm_sbAdditive[i] ? qtrue : qfalse;
            return qtrue;
        }
    }
    return qfalse;
}







typedef struct ef2_ld_sub_s {
    char environmentName[MAX_QPATH];
    char levelName[MAX_QPATH];
    char sublevelName[MAX_QPATH];
} ef2_ld_sub_t;

#define EF2_LD_MAX 256
static ef2_ld_sub_t  ld_subs[EF2_LD_MAX];
static int           ld_numSubs;
static qboolean      ld_loaded;

static void ld_add(const char *env, const char *level, const char *sub)
{
    if (ld_numSubs >= EF2_LD_MAX) return;
    Q_strncpyz(ld_subs[ld_numSubs].environmentName, env ? env : "", MAX_QPATH);
    Q_strncpyz(ld_subs[ld_numSubs].levelName,       level ? level : "", MAX_QPATH);
    Q_strncpyz(ld_subs[ld_numSubs].sublevelName,    sub ? sub : "", MAX_QPATH);
    ld_numSubs++;
}

static void ld_load(void)
{
    int i;

    if (ld_loaded) return;
    ld_loaded = qtrue;

    for (i = 0; i < 10; i++) {
        char   filename[MAX_QPATH];
        char  *buffer = NULL;
        char  *data;
        long   len;
        char   environmentName[MAX_QPATH] = "";
        char   levelName[MAX_QPATH] = "";
        qboolean haveLevel    = qfalse;
        qboolean levelHasSubs = qfalse;
        const char *token;

        if (i) {
            Com_sprintf(filename, sizeof(filename), "global/leveldefs%d.txt", i);
        } else {
            Q_strncpyz(filename, "global/leveldefs.txt", sizeof(filename));
        }

        len = FS_ReadFile(filename, (void **)&buffer);
        if (len <= 0 || !buffer) {
            continue;
        }
        data = buffer;

        while (1) {
            token = COM_ParseExt(&data, qtrue);
            if (!token || !token[0]) {
                break;
            }

            if (!Q_stricmp(token, "env")) {
                if (haveLevel && !levelHasSubs) {
                    ld_add(environmentName, levelName, levelName);
                }
                token = COM_ParseExt(&data, qfalse);
                if (!token || !token[0]) {
                    continue;
                }
                Q_strncpyz(environmentName, token, sizeof(environmentName));
                token = COM_ParseExt(&data, qfalse);
                if (token && token[0]) {
                    COM_ParseExt(&data, qfalse);
                }
                haveLevel    = qfalse;
                levelHasSubs = qfalse;
            } else if (!Q_stricmp(token, "level")) {
                if (haveLevel && !levelHasSubs) {
                    ld_add(environmentName, levelName, levelName);
                }
                token = COM_ParseExt(&data, qfalse);
                if (!token || !token[0]) {
                    continue;
                }
                Q_strncpyz(levelName, token, sizeof(levelName));
                token = COM_ParseExt(&data, qfalse);
                if (token && token[0]) {
                    COM_ParseExt(&data, qfalse);
                }
                haveLevel    = qtrue;
                levelHasSubs = qfalse;
            } else if (!Q_stricmp(token, "sublevel")) {
                char sublevelToken[256];

                token = COM_ParseExt(&data, qfalse);
                if (!token || !token[0]) {
                    continue;
                }
                if (haveLevel) {
                    convertStringResourceIds(token, sublevelToken, sizeof(sublevelToken));
                    ld_add(environmentName, levelName, sublevelToken);
                    levelHasSubs = qtrue;
                }
            }
        }

        if (haveLevel && !levelHasSubs) {
            ld_add(environmentName, levelName, levelName);
        }

        FS_FreeFile(buffer);
    }
}

void PF_GetLevelDefs(
    const char *name, const char **environmentName, const char **levelName, const char **sublevelName
)
{
    int i;

    ld_load();

    if (environmentName) {
        *environmentName = "";
    }
    if (levelName) {
        *levelName = "";
    }
    if (sublevelName) {
        *sublevelName = "";
    }

    if (!name || !name[0]) {
        return;
    }

    for (i = 0; i < ld_numSubs; i++) {
        if (!Q_stricmp(name, ld_subs[i].sublevelName)) {
            if (environmentName) {
                *environmentName = ld_subs[i].environmentName;
            }
            if (levelName) {
                *levelName = ld_subs[i].levelName;
            }
            if (sublevelName) {
                *sublevelName = ld_subs[i].sublevelName;
            }
            return;
        }
    }
}

qboolean PF_areSublevels(const char *oldMapName, const char *newMapName)
{
    char        oldBase[64], newBase[64];
    const char *oldEnv, *oldLevel, *oldSub;
    const char *newEnv, *newLevel, *newSub;

    SV_ParseMapName(oldMapName, oldBase, NULL, NULL);
    SV_ParseMapName(newMapName, newBase, NULL, NULL);

    PF_GetLevelDefs(oldBase, &oldEnv, &oldLevel, &oldSub);
    PF_GetLevelDefs(newBase, &newEnv, &newLevel, &newSub);

    if (Q_stricmp(oldEnv, newEnv)) {
        return qfalse;
    }
    if (Q_stricmp(oldLevel, newLevel)) {
        return qfalse;
    }
    return Q_stricmp(oldSub, newSub) ? qtrue : qfalse;
}


static game_import_t import_shadow1;

void SV_ClearGameInterface( void ) {
	memset( &import_shadow1, 0, sizeof( import_shadow1 ) );
}


static int PF_AreaEntities_SDK(const vec3_t mins, const vec3_t maxs, int *entityList, int maxcount, qboolean intersection)
{
    return SV_AreaEntities(mins, maxs, entityList, maxcount, intersection);
}

static const char *PF_Args_SDK(void)
{
    return (const char *)Cmd_Args();
}

static int PF_SoundIndex_SDK(const char *name)
{
    return SV_SoundIndex(name);
}


static void *PF_Malloc_SDK(int size)
{
    return SV_Malloc((size_t)size);
}

static const char *PF_GameDir_SDK(void)
{
    return (const char *)FS_Gamedir();
}

static void PF_AddCommand_SDK(const char *cmdName)
{
    SV_AddGameCommand(cmdName, NULL);
}

static int PF_FS_ReadFile_SDK(const char *qpath, void **buffer, qboolean quiet)
{
    return (int)FS_ReadFileEx(qpath, buffer, quiet);
}

static void PF_FS_WriteFile_SDK(const char *qpath, const void *buffer, int size)
{
    FS_WriteFile(qpath, buffer, size);
}

static int PF_FS_FOpenFile_SDK(const char *filename, fileHandle_t *file, fsMode_t mode)
{
    return (int)FS_FOpenFileRead(filename, file, qfalse, mode == FS_READ ? qtrue : qfalse);
}

static char *PF_FS_PrepFileWrite_SDK(const char *filename)
{
    return (char *)FS_PrepFileWrite(filename);
}

static int PF_FS_Write_SDK(const void *buffer, int len, fileHandle_t f)
{
    return (int)FS_Write(buffer, len, f);
}

static int PF_FS_Read_SDK(void *buffer, int len, fileHandle_t f)
{
    return (int)FS_Read(buffer, len, f);
}

static char **PF_FS_ListFiles_SDK(const char *path, const char *extension, int *numfiles)
{
    return FS_ListFiles(path, extension, qfalse, numfiles);
}

static void PF_DebugGraph_SDK(float value, int color)
{
    SCR_DebugGraph(value);
}

static void PF_setmodel_SDK(gentity_t *ent, const char *name)
{
    PF_setmodel(ent, name);
}

void SV_BotInitImport( game_import_t *import_shadow2 );

void SV_InitGameProgs( void ) {
	const char *err;
	int i;

	bot_enable  = Cvar_Get( "bot_enable", "1", CVAR_LATCH );
	sv_botEnable = bot_enable->integer;



	import_shadow1.Printf						= Com_Printf;
	import_shadow1.DPrintf						= Com_DPrintf;
	import_shadow1.WPrintf						= Com_WPrintf;
	import_shadow1.WDPrintf						= Com_WDPrintf;
	import_shadow1.DebugPrintf					= Com_DebugPrintf;
	import_shadow1.Error						= Com_Error;
	import_shadow1.GetArchiveFileName			= Com_GetArchiveFileName;
	import_shadow1.Milliseconds					= Sys_Milliseconds;
	import_shadow1.SR_LoadLevelStrings			= loadLevelStrings;  

	import_shadow1.SendServerCommand			= SV_GameSendServerCommand;
	import_shadow1.DropClient					= SV_GameDropClient;


	import_shadow1.linkentity					= SV_LinkEntity;
	import_shadow1.unlinkentity					= SV_UnlinkEntity;

	import_shadow1.AreaEntities = PF_AreaEntities_SDK;
	
	import_shadow1.trace						= SV_Trace;
	import_shadow1.fulltrace = SV_FullTrace;
	import_shadow1.pointcontents				= SV_PointContents;
	import_shadow1.pointbrushnum				= CM_PointBrushNum;
	import_shadow1.SetBrushModel				= SV_SetBrushModel;
	import_shadow1.ClipToEntity					= SV_ClipToEntity;
	import_shadow1.setConfigstring				= SV_SetConfigstring;
	import_shadow1.getConfigstring				= SV_GetConfigstring;

	import_shadow1.Malloc = PF_Malloc_SDK;
	import_shadow1.Free							= SV_Free;

	import_shadow1.cvar_get						= Cvar_FindVar;
	import_shadow1.cvar							= Cvar_Get;
	import_shadow1.argc							= Cmd_Argc;
	import_shadow1.argv							= Cmd_Argv;
	import_shadow1.args = PF_Args_SDK;
	import_shadow1.cvar_set						= Cvar_Set;


	import_shadow1.AddCommand = PF_AddCommand_SDK;
	import_shadow1.SendConsoleCommand			= Cbuf_AddText;

	import_shadow1.FS_ReadFile = PF_FS_ReadFile_SDK;
	import_shadow1.FS_FreeFile					= FS_FreeFile;
	import_shadow1.FS_WriteFile = PF_FS_WriteFile_SDK;
	import_shadow1.FS_FOpenFileWrite			= FS_FOpenFileWrite_HomeData;
	import_shadow1.FS_FOpenFileAppend			= FS_FOpenFileAppend_HomeData;
	import_shadow1.FS_FOpenFile = PF_FS_FOpenFile_SDK;
	import_shadow1.FS_PrepFileWrite = PF_FS_PrepFileWrite_SDK;
	import_shadow1.FS_Write = PF_FS_Write_SDK;
	import_shadow1.FS_Read = PF_FS_Read_SDK;
	import_shadow1.FS_FCloseFile				= FS_FCloseFile;
	import_shadow1.FS_Seek						= FS_Seek;
	import_shadow1.FS_Flush						= FS_ForceFlush;
	import_shadow1.FS_ListFiles = PF_FS_ListFiles_SDK;

	import_shadow1.DebugGraph = PF_DebugGraph_SDK;
	import_shadow1.AdjustAreaPortalState		= SV_AdjustAreaPortalState;
	import_shadow1.AreasConnected				= CM_AreasConnected;
	import_shadow1.inPVS						= SV_inPVS;
	import_shadow1.GameDir = PF_GameDir_SDK;

	import_shadow1.setmodel = PF_setmodel_SDK;
	import_shadow1.setviewmodel					= PF_setviewmodel;
	import_shadow1.IsModel						= PF_IsModel;
	import_shadow1.NumAnims					= PF_NumAnims_mi;
	import_shadow1.NumSurfaces					= PF_NumSurfaces_mi;
	import_shadow1.Morph_NumForName			= PF_Morph_NumForName_mi;
	import_shadow1.NumMorphs					= PF_NumMorphs_mi;
	import_shadow1.Morph_NameForNum			= PF_Morph_NameForNum_mi;
	import_shadow1.modelindex				= PF_ModelIndex;
	import_shadow1.TikiLoadFromTS			= PF_TikiLoadFromTS;
	import_shadow1.ToolServerGetData		= PF_ToolServerGetData;
	import_shadow1.NumTags						= PF_NumTags_mi;
	import_shadow1.CalculateBounds			= PF_CalculateBounds_mi;
	import_shadow1.Frame_Bounds					= PF_Frame_Bounds_mi;
	import_shadow1.ProcessLoadingScreen			= CL_ProcessLoadingScreen;
	import_shadow1.Anim_NameForNum				= PF_Anim_NameForNum_mi;
	import_shadow1.Anim_NumForName				= PF_Anim_NumForName_mi;
	import_shadow1.Anim_Random					= PF_Anim_Random_mi;
	import_shadow1.Anim_NumFrames				= PF_Anim_NumFrames_mi;
	import_shadow1.Anim_Time					= PF_Anim_Time_mi;
	import_shadow1.Anim_Delta					= PF_Anim_Delta_mi;
	import_shadow1.Anim_Flags					= PF_Anim_Flags_mi;
	import_shadow1.Anim_HasCommands				= PF_Anim_HasCommands_mi;
	import_shadow1.Frame_Commands				= PF_Frame_Commands_mi;
	import_shadow1.Surface_NameToNum			= PF_Surface_NameToNum_mi;
	import_shadow1.Surface_NumToName			= PF_Surface_NumToName_mi;
	import_shadow1.Tag_NumForName				= PF_Tag_NumForName_mi;
	import_shadow1.Tag_NameForNum				= PF_Tag_NameForNum_mi;


	import_shadow1.Alias_Add					= PF_Alias_Add_mi;
	import_shadow1.Alias_FindRandom				= PF_Alias_FindRandom_mi;
	import_shadow1.Alias_FindDialog				= PF_Alias_FindDialog_mi;
	import_shadow1.Alias_Dump					= PF_Alias_Dump_mi;
	import_shadow1.Alias_Clear					= PF_Alias_Clear_mi;
	import_shadow1.Alias_UpdateDialog			= PF_Alias_UpdateDialog_mi;
	import_shadow1.Alias_FindSpecificAnim		= PF_Alias_FindSpecificAnim_mi;
	import_shadow1.Alias_CheckLoopAnim			= PF_Alias_CheckLoopAnim_mi;
	import_shadow1.GetNextMorphTarget			= PF_GetNextMorphTarget;

	import_shadow1.MObjective_GetDescription		= PF_MObjective_GetDescription;
	import_shadow1.MObjective_SetDescription		= PF_MObjective_SetDescription;
	import_shadow1.MObjective_GetShowObjective		= PF_MObjective_GetShowObjective;
	import_shadow1.MObjective_SetShowObjective		= PF_MObjective_SetShowObjective;
	import_shadow1.MObjective_GetObjectiveComplete	= PF_MObjective_GetObjectiveComplete;
	import_shadow1.MObjective_SetObjectiveComplete	= PF_MObjective_SetObjectiveComplete;
	import_shadow1.MObjective_GetObjectiveFailed	= PF_MObjective_GetObjectiveFailed;
	import_shadow1.MObjective_SetObjectiveFailed	= PF_MObjective_SetObjectiveFailed;
	import_shadow1.MObjective_GetNameFromIndex		= PF_MObjective_GetNameFromIndex;
	import_shadow1.MObjective_GetIndexFromName		= PF_MObjective_GetIndexFromName;
	import_shadow1.MObjective_NewObjective			= PF_MObjective_NewObjective;
	import_shadow1.MObjective_ClearObjectiveList	= PF_MObjective_ClearObjectiveList;
	import_shadow1.MObjective_ParseObjectiveFile	= PF_MObjective_ParseObjectiveFile;
	import_shadow1.MObjective_Update				= PF_MObjective_Update;
	import_shadow1.MObjective_GetNumObjectives		= PF_MObjective_GetNumObjectives;
	import_shadow1.MObjective_GetNumActiveObjectives	= PF_MObjective_GetNumActiveObjectives;
	import_shadow1.MObjective_GetNumCompleteObjectives	= PF_MObjective_GetNumCompleteObjectives;
	import_shadow1.MObjective_GetNumFailedObjectives	= PF_MObjective_GetNumFailedObjectives;
	import_shadow1.MObjective_GetNumIncompleteObjectives	= PF_MObjective_GetNumIncompleteObjectives;

	import_shadow1.MI_GetShader					= PF_MI_GetShader;
	import_shadow1.MI_SetShader					= PF_MI_SetShader;
	import_shadow1.MI_GetInformationData			= PF_MI_GetInformationData;
	import_shadow1.MI_SetInformationData			= PF_MI_SetInformationData;
	import_shadow1.MI_GetNameFromIndex				= PF_MI_GetNameFromIndex;
	import_shadow1.MI_GetIndexFromName				= PF_MI_GetIndexFromName;
	import_shadow1.MI_NewInformation				= PF_MI_NewInformation;
	import_shadow1.MI_ClearInformationList			= PF_MI_ClearInformationList;
	import_shadow1.MI_SetShowInformation			= PF_MI_SetShowInformation;
	import_shadow1.MI_GetShowInformation			= PF_MI_GetShowInformation;
	import_shadow1.GlobalAlias_Add				= Alias_Add;
	import_shadow1.GlobalAlias_FindRandom		= PF_GlobalAlias_FindRandom;
	import_shadow1.GlobalAlias_Dump				= Alias_Dump;
	import_shadow1.GlobalAlias_Clear			= Alias_Clear;

	import_shadow1.soundindex = PF_SoundIndex_SDK;
	import_shadow1.archetypeindex				= PF_ArcheTypeIndex;
	import_shadow1.TIKI_CacheAnim				= PF_TIKI_CacheAnim;
	import_shadow1.imageindex					= SV_ImageIndex;
	import_shadow1.itemindex					= SV_ItemIndex;
	import_shadow1.SetLightStyle				= SV_SetLightStyle;

	import_shadow1.numDebugLines				= &numDebugLines;
	import_shadow1.DebugLines					= &DebugLines;

	import_shadow1.CalcCRC						= CRC_Block;
	import_shadow1.Sound						= PF_Sound_Retail;
	import_shadow1.StopSound					= SV_StopSound;
	import_shadow1.SoundLength					= SV_SoundLength;
	import_shadow1.LocalizeFilePath				= Com_LocalizeFilePath;
	import_shadow1.centerprintf					= PF_centerprintf;
	import_shadow1.locationprintf				= PF_locationprintf;
	import_shadow1.LocateGameData				= SV_LocateGameData;
	import_shadow1.SetFarPlane					= SV_SetFarPlane;
	import_shadow1.SetSkyPortal					= SV_SetSkyPortal;

	import_shadow1.GetLightingGroup				= SV_GetLightingGroup;
	import_shadow1.SetDynamicLight				= SV_SetDynamicLight;
	import_shadow1.SetDynamicLightDefault		= SV_SetDynamicLightDefault;
	import_shadow1.SetWindDirection				= SV_SetWindDirection;
	import_shadow1.SetWindIntensity				= SV_SetWindIntensity;
	import_shadow1.SetWeatherInfo				= SV_SetWeatherInfo;
	import_shadow1.SetTimeScale					= SV_SetTimeScale;




    import_shadow1.Cvar_VariableStringBuffer    = Cvar_VariableStringBuffer;
    import_shadow1.Cvar_VariableValue           = Cvar_VariableValue;
    import_shadow1.Cvar_VariableIntegerValue    = Cvar_VariableIntegerValue;
    import_shadow1.Cvar_Register                = Cvar_Register;
    import_shadow1.FS_GetFileList               = FS_GetFileList;


	import_shadow1.failedcondition				= SV_FailedCondition;
	import_shadow1.inPVSIgnorePortals			= SV_inPVSIgnorePortals;
	import_shadow1.FS_Exists					= SV_FS_Exists;
	import_shadow1.GetNumFreeReliableServerCommands = SV_GameGetNumFreeReliableServerCommands;
	import_shadow1.isClientActive				= SV_IsClientActive;
	import_shadow1.Surface_Flags = PF_Surface_Flags_mi;
	import_shadow1.Surface_NumSkins = PF_Surface_NumSkins_mi;

	SV_BotInitImport( &import_shadow1 );

	GIMP_VerifyBound( &import_shadow1 );

	ge = Sys_GetGameAPI( &import_shadow1 );

	if( !ge ) {
		Com_Error( ERR_DROP, "failed to load game DLL" );
	}

	if( ge->apiversion != GAME_API_VERSION )
	{
		Com_Error( ERR_DROP, "game is version %i, not %i", ge->apiversion,
			GAME_API_VERSION );
	}

	ge->Init( svs.time, Com_Milliseconds() );

	err = ge->error_message;
	if( err )
	{
		ge->error_message = NULL;
		Com_Error(ERR_DROP, "%s", err);
	}

	if( svs.clients )
	{
		for( i = 0; i < sv_maxclients->integer; i++ )
		{
			svs.clients[ i ].gentity = NULL;
		}
	}
}

void SV_BotInitImport( game_import_t *import_shadow2 ) {
	import_shadow2->AAS_EntityInfo                    = AAS_EntityInfo;
	import_shadow2->AAS_Initialized                   = AAS_Initialized;
	import_shadow2->AAS_PresenceTypeBoundingBox       = AAS_PresenceTypeBoundingBox;
	import_shadow2->AAS_Time                          = AAS_Time;
	import_shadow2->AAS_PointAreaNum                  = AAS_PointAreaNum;
	import_shadow2->AAS_PointReachabilityAreaIndex    = AAS_PointReachabilityAreaIndex;
	import_shadow2->AAS_TraceAreas                    = AAS_TraceAreas;
	import_shadow2->AAS_BBoxAreas                     = AAS_BBoxAreas;
	import_shadow2->AAS_AreaInfo                      = AAS_AreaInfo;
	import_shadow2->AAS_PointContents                 = AAS_PointContents;
	import_shadow2->AAS_NextBSPEntity                 = AAS_NextBSPEntity;
	import_shadow2->AAS_ValueForBSPEpairKey           = AAS_ValueForBSPEpairKey;
	import_shadow2->AAS_VectorForBSPEpairKey          = AAS_VectorForBSPEpairKey;
	import_shadow2->AAS_FloatForBSPEpairKey           = AAS_FloatForBSPEpairKey;
	import_shadow2->AAS_IntForBSPEpairKey             = AAS_IntForBSPEpairKey;
	import_shadow2->AAS_AreaReachability              = AAS_AreaReachability;
	import_shadow2->AAS_AreaTravelTimeToGoalArea      = AAS_AreaTravelTimeToGoalArea;
	import_shadow2->AAS_EnableRoutingArea             = AAS_EnableRoutingArea;
	import_shadow2->AAS_PredictRoute                  = AAS_PredictRoute;
	import_shadow2->AAS_AlternativeRouteGoals         = AAS_AlternativeRouteGoals;
	import_shadow2->AAS_Swimming                      = AAS_Swimming;
	import_shadow2->AAS_PredictClientMovement         = AAS_PredictClientMovement;
	import_shadow2->EA_Command                        = EA_Command;
	import_shadow2->EA_Say                            = EA_Say;
	import_shadow2->EA_SayTeam                        = EA_SayTeam;
	import_shadow2->EA_Action                         = EA_Action;
	import_shadow2->EA_Gesture                        = EA_Gesture;
	import_shadow2->EA_Talk                           = EA_Talk;
	import_shadow2->EA_ToggleFireState                = EA_ToggleFireState;
	import_shadow2->EA_Attack                         = EA_Attack;
	import_shadow2->EA_Use                            = EA_Use;
	import_shadow2->EA_Respawn                        = EA_Respawn;
	import_shadow2->EA_MoveUp                         = EA_MoveUp;
	import_shadow2->EA_MoveDown                       = EA_MoveDown;
	import_shadow2->EA_MoveForward                    = EA_MoveForward;
	import_shadow2->EA_MoveBack                       = EA_MoveBack;
	import_shadow2->EA_MoveLeft                       = EA_MoveLeft;
	import_shadow2->EA_MoveRight                      = EA_MoveRight;
	import_shadow2->EA_Crouch                         = EA_Crouch;
	import_shadow2->EA_SelectWeapon                   = EA_SelectWeapon;
	import_shadow2->EA_Jump                           = EA_Jump;
	import_shadow2->EA_DelayedJump                    = EA_DelayedJump;
	import_shadow2->EA_Move                           = EA_Move;
	import_shadow2->EA_View                           = EA_View;
	import_shadow2->EA_EndRegular                     = EA_EndRegular;
	import_shadow2->EA_GetInput                       = EA_GetInput;
	import_shadow2->EA_ResetInput                     = EA_ResetInput;
	import_shadow2->BotLoadCharacter                  = BotLoadCharacter;
	import_shadow2->BotFreeCharacter                  = BotFreeCharacter;
	import_shadow2->Characteristic_Float              = Characteristic_Float;
	import_shadow2->Characteristic_BFloat             = Characteristic_BFloat;
	import_shadow2->Characteristic_Integer            = Characteristic_Integer;
	import_shadow2->Characteristic_BInteger           = Characteristic_BInteger;
	import_shadow2->Characteristic_String             = Characteristic_String;
	import_shadow2->BotAllocChatState                 = BotAllocChatState;
	import_shadow2->BotFreeChatState                  = BotFreeChatState;
	import_shadow2->BotQueueConsoleMessage            = BotQueueConsoleMessage;
	import_shadow2->BotRemoveConsoleMessage           = BotRemoveConsoleMessage;
	import_shadow2->BotNextConsoleMessage             = BotNextConsoleMessage;
	import_shadow2->BotNumConsoleMessages             = BotNumConsoleMessages;
	import_shadow2->BotInitialChat                    = BotInitialChat;
	import_shadow2->BotNumInitialChats                = BotNumInitialChats;
	import_shadow2->BotReplyChat                      = BotReplyChat;
	import_shadow2->BotChatLength                     = BotChatLength;
	import_shadow2->BotEnterChat                      = BotEnterChat;
	import_shadow2->BotGetChatMessage                 = BotGetChatMessage;
	import_shadow2->StringContains                    = StringContains;
	import_shadow2->BotFindMatch                      = BotFindMatch;
	import_shadow2->BotMatchVariable                  = BotMatchVariable;
	import_shadow2->UnifyWhiteSpaces                  = UnifyWhiteSpaces;
	import_shadow2->BotReplaceSynonyms                = BotReplaceSynonyms;
	import_shadow2->BotLoadChatFile                   = BotLoadChatFile;
	import_shadow2->BotSetChatGender                  = BotSetChatGender;
	import_shadow2->BotSetChatName                    = BotSetChatName;
	import_shadow2->BotResetGoalState                 = BotResetGoalState;
	import_shadow2->BotResetAvoidGoals                = BotResetAvoidGoals;
	import_shadow2->BotRemoveFromAvoidGoals           = BotRemoveFromAvoidGoals;
	import_shadow2->BotPushGoal                       = BotPushGoal;
	import_shadow2->BotPopGoal                        = BotPopGoal;
	import_shadow2->BotEmptyGoalStack                 = BotEmptyGoalStack;
	import_shadow2->BotDumpAvoidGoals                 = BotDumpAvoidGoals;
	import_shadow2->BotDumpGoalStack                  = BotDumpGoalStack;
	import_shadow2->BotGoalName                       = BotGoalName;
	import_shadow2->BotGetTopGoal                     = BotGetTopGoal;
	import_shadow2->BotGetSecondGoal                  = BotGetSecondGoal;
	import_shadow2->BotChooseLTGItem                  = BotChooseLTGItem;
	import_shadow2->BotChooseNBGItem                  = BotChooseNBGItem;
	import_shadow2->BotTouchingGoal                   = BotTouchingGoal;
	import_shadow2->BotItemGoalInVisButNotVisible     = BotItemGoalInVisButNotVisible;
	import_shadow2->BotGetLevelItemGoal               = BotGetLevelItemGoal;
	import_shadow2->BotGetNextCampSpotGoal            = BotGetNextCampSpotGoal;
	import_shadow2->BotGetMapLocationGoal             = BotGetMapLocationGoal;
	import_shadow2->BotAvoidGoalTime                  = BotAvoidGoalTime;
	import_shadow2->BotSetAvoidGoalTime               = BotSetAvoidGoalTime;
	import_shadow2->BotInitLevelItems                 = BotInitLevelItems;
	import_shadow2->BotUpdateEntityItems              = BotUpdateEntityItems;
	import_shadow2->BotLoadItemWeights                = BotLoadItemWeights;
	import_shadow2->BotFreeItemWeights                = BotFreeItemWeights;
	import_shadow2->BotInterbreedGoalFuzzyLogic       = BotInterbreedGoalFuzzyLogic;
	import_shadow2->BotSaveGoalFuzzyLogic             = BotSaveGoalFuzzyLogic;
	import_shadow2->BotMutateGoalFuzzyLogic           = BotMutateGoalFuzzyLogic;
	import_shadow2->BotAllocGoalState                 = BotAllocGoalState;
	import_shadow2->BotFreeGoalState                  = BotFreeGoalState;
	import_shadow2->BotResetMoveState                 = BotResetMoveState;
	import_shadow2->BotMoveToGoal                     = BotMoveToGoal;
	import_shadow2->BotMoveInDirection                = BotMoveInDirection;
	import_shadow2->BotResetAvoidReach                = BotResetAvoidReach;
	import_shadow2->BotResetLastAvoidReach            = BotResetLastAvoidReach;
	import_shadow2->BotReachabilityArea               = BotReachabilityArea;
	import_shadow2->BotMovementViewTarget             = BotMovementViewTarget;
	import_shadow2->BotPredictVisiblePosition         = BotPredictVisiblePosition;
	import_shadow2->BotAllocMoveState                 = BotAllocMoveState;
	import_shadow2->BotFreeMoveState                  = BotFreeMoveState;
	import_shadow2->BotInitMoveState                  = BotInitMoveState;
	import_shadow2->BotAddAvoidSpot                   = BotAddAvoidSpot;
	import_shadow2->BotChooseBestFightWeapon          = BotChooseBestFightWeapon;
	import_shadow2->BotGetWeaponInfo                  = BotGetWeaponInfo;
	import_shadow2->BotLoadWeaponWeights              = BotLoadWeaponWeights;
	import_shadow2->BotAllocWeaponState               = BotAllocWeaponState;
	import_shadow2->BotFreeWeaponState                = BotFreeWeaponState;
	import_shadow2->BotResetWeaponState               = BotResetWeaponState;
	import_shadow2->GeneticParentsAndChildSelection   = GeneticParentsAndChildSelection;
	import_shadow2->Print                             = BotImport_Print;
	import_shadow2->PointContents                     = BotImport_PointContents;
	import_shadow2->BSPEntityData                     = BotImport_BSPEntityData;
	import_shadow2->BSPModelMinsMaxsOrigin            = BotImport_BSPModelMinsMaxsOrigin;
	import_shadow2->BotClientCommand                  = BotClientCommand;
	import_shadow2->AvailableMemory                   = BotImport_AvailableMemory;
	import_shadow2->HunkAlloc                         = BotImport_HunkAlloc;
	import_shadow2->DebugLineCreate                   = BotImport_DebugLineCreate;
	import_shadow2->DebugLineDelete                   = BotImport_DebugLineDelete;
	import_shadow2->DebugLineShow                     = BotImport_DebugLineShow;
	import_shadow2->DebugPolygonCreate                = BotImport_DebugPolygonCreate;
	import_shadow2->DebugPolygonDelete                = BotImport_DebugPolygonDelete;
	import_shadow2->BotAllocateClient                 = SV_BotAllocateClient;
	import_shadow2->BotGetSnapshotEntity              = SV_BotGetSnapshotEntity;
	import_shadow2->BotGetConsoleMessage              = SV_BotGetConsoleMessage;
	import_shadow2->BotUserCommand                    = SV_BotUserCommand;
	import_shadow2->BotLibSetup                       = SV_BotLibSetup;
	import_shadow2->BotLibShutdown                    = SV_BotLibShutdown;
	import_shadow2->BotLibVarSet                      = SV_BotLibVarSet;
	import_shadow2->BotLibVarGet                      = SV_BotLibVarGet;
	import_shadow2->PC_AddGlobalDefine                = SV_PC_AddGlobalDefine;
	import_shadow2->PC_LoadSourceHandle               = SV_PC_LoadSourceHandle;
	import_shadow2->PC_FreeSourceHandle               = SV_PC_FreeSourceHandle;
	import_shadow2->PC_SourceFileAndLine              = SV_PC_SourceFileAndLine;
	import_shadow2->BotLibStartFrame                  = SV_BotLibStartFrame;
	import_shadow2->BotLibLoadMap                     = SV_BotLibLoadMap;
	import_shadow2->BotLibUpdateEntity                = SV_BotLibUpdateEntity;

	import_shadow2->cvar_get						= Cvar_FindVar;
	import_shadow2->Cvar_Update					= Cvar_Update;
	import_shadow2->FS_FTell						= FS_FTell;
	import_shadow2->FS_FSeek						= FS_Seek;
	import_shadow2->FS_DeleteFile				= SV_FS_DeleteFile;
	import_shadow2->setUserinfo					= SV_SetUserinfo;
	import_shadow2->getUserinfo					= SV_GetUserinfo;
	import_shadow2->pointbrushnum				= PF_PointBrushNum;
	import_shadow2->objectivenameindex			= SV_ObjectiveNameIndex;
	import_shadow2->SV_GetServerinfo				= SV_GetServerinfo;

	import_shadow2->SurfaceTypeToName				= SurfaceTypeToName;

	// reaching for a hook retail never implemented, and Com_Error naming the slot is
	import_shadow2->Test							= PF_Test_Unimplemented;

	import_shadow2->NumAnims						= PF_NumAnims_mi;
	import_shadow2->NumSkins						= PF_NumSkins_mi;
	import_shadow2->NumSurfaces					= PF_NumSurfaces_mi;
	import_shadow2->NumTags						= PF_NumTags_mi;
	import_shadow2->NumMorphs					= PF_NumMorphs_mi;
	import_shadow2->InitCommands					= PF_InitCommands_mi;
	import_shadow2->CalculateBounds				= PF_CalculateBounds_mi;
	import_shadow2->Anim_NameForNum				= PF_Anim_NameForNum_mi;
	import_shadow2->Anim_NumForName				= PF_Anim_NumForName_mi;
	import_shadow2->Anim_Random					= PF_Anim_Random_mi;
	import_shadow2->Anim_NumFrames				= PF_Anim_NumFrames_mi;
	import_shadow2->Anim_Time					= PF_Anim_Time_mi;
	import_shadow2->Anim_Delta					= PF_Anim_Delta_mi;
	import_shadow2->Anim_AbsoluteDelta			= PF_Anim_AbsoluteDelta_mi;
	import_shadow2->Anim_Flags					= PF_Anim_Flags_mi;
	import_shadow2->Anim_HasCommands				= PF_Anim_HasCommands_mi;
	import_shadow2->Frame_Commands				= PF_Frame_Commands_mi;
	import_shadow2->Frame_Delta					= PF_Frame_Delta_mi;
	import_shadow2->Frame_Time					= PF_Frame_Time_mi;
	import_shadow2->Frame_Bounds					= PF_Frame_Bounds_mi;
	import_shadow2->Surface_NameToNum			= PF_Surface_NameToNum_mi;
	import_shadow2->Surface_NumToName			= PF_Surface_NumToName_mi;
	import_shadow2->Surface_Flags				= PF_Surface_Flags_mi;
	import_shadow2->Surface_NumSkins				= PF_Surface_NumSkins_mi;
	import_shadow2->Morph_NumForName				= PF_Morph_NumForName_mi;
	import_shadow2->Morph_NameForNum				= PF_Morph_NameForNum_mi;
	import_shadow2->GetExpression				= PF_GetExpression_mi;
	import_shadow2->Tag_NumForName				= PF_Tag_NumForName_mi;
	import_shadow2->Tag_NameForNum				= PF_Tag_NameForNum_mi;
	import_shadow2->Tag_Orientation				= PF_Tag_Orientation_mi;
	import_shadow2->Tag_OrientationEx			= PF_Tag_OrientationEx_mi;
	import_shadow2->Bone_GetParentNum			= PF_Bone_GetParentNum_mi;
	import_shadow2->Alias_Add					= PF_Alias_Add_mi;
	import_shadow2->Alias_FindRandom				= PF_Alias_FindRandom_mi;
	import_shadow2->Alias_Find					= PF_Alias_Find_mi;
	import_shadow2->Alias_Dump					= PF_Alias_Dump_mi;
	import_shadow2->Alias_Clear					= PF_Alias_Clear_mi;
	import_shadow2->Alias_FindDialog				= PF_Alias_FindDialog_mi;
	import_shadow2->Alias_FindSpecificAnim		= PF_Alias_FindSpecificAnim_mi;
	import_shadow2->Alias_CheckLoopAnim			= PF_Alias_CheckLoopAnim_mi;
	import_shadow2->Alias_GetList				= PF_Alias_GetList_mi;
	import_shadow2->Alias_UpdateDialog			= PF_Alias_UpdateDialog_mi;
	import_shadow2->Alias_AddActorDialog			= PF_Alias_AddActorDialog_mi;
	import_shadow2->NameForNum					= PF_NameForNum_mi;
	import_shadow2->GlobalAlias_Find				= PF_GlobalAlias_Find;

	import_shadow2->TikiReload					= PF_ReloadTiki;
	import_shadow2->WidgetPrintf					= PF_WidgetPrintf;
	import_shadow2->SR_InitializeStringResource	= initializeStringResource;
	import_shadow2->SR_UninitializeStringResource	= uninitializeStringResource;
	import_shadow2->SR_LoadLevelStrings			= loadLevelStrings;

	import_shadow2->GetViewModeMask				= PF_GetViewModeMask;
	import_shadow2->GetViewModeClassMask			= PF_GetViewModeClassMask;
	import_shadow2->GetViewModeSendInMode		= PF_GetViewModeSendInMode;
	import_shadow2->GetViewModeSendNotInMode		= PF_GetViewModeSendNotInMode;
	import_shadow2->GetViewModeScreenBlend		= PF_GetViewModeScreenBlend;
	import_shadow2->GetLevelDefs					= PF_GetLevelDefs;
	import_shadow2->areSublevels					= PF_areSublevels;

	import_shadow2->AreaEntities = PF_AreaEntities_SDK;
}


qboolean SV_GameCommand( void ) {
	if ( sv.state != SS_GAME ) {
		return qfalse;
	}

	return ge->ConsoleCommand();
}

