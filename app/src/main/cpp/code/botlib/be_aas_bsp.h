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


#ifdef AASINTERN
int AAS_LoadBSPFile(void);
void AAS_DumpBSPData(void);
void AAS_UnlinkFromBSPLeaves(bsp_link_t *leaves);
bsp_link_t *AAS_BSPLinkEntity(vec3_t absmins,
										vec3_t absmaxs,
										int entnum,
										int modelnum);

qboolean AAS_EntityCollision(int entnum,
										vec3_t start,
										vec3_t boxmins,
										vec3_t boxmaxs,
										vec3_t end,
										int contentmask,
										bsp_trace_t *trace);
void AAS_PrintFreeBSPLinks(char *str);
#endif 

#define MAX_EPAIRKEY		128

bsp_trace_t AAS_Trace(	vec3_t start,
								vec3_t mins,
								vec3_t maxs,
								vec3_t end,
								int passent,
								int contentmask);
int AAS_PointContents(vec3_t point);
qboolean AAS_inPVS(vec3_t p1, vec3_t p2);
qboolean AAS_inPHS(vec3_t p1, vec3_t p2);
qboolean AAS_AreasConnected(int area1, int area2);
int AAS_BoxEntities(vec3_t absmins, vec3_t absmaxs, int *list, int maxcount);
void AAS_BSPModelMinsMaxsOrigin(int modelnum, vec3_t angles, vec3_t mins, vec3_t maxs, vec3_t origin);
int AAS_NextBSPEntity(int ent);
int AAS_ValueForBSPEpairKey(int ent, char *key, char *value, int size);
int AAS_VectorForBSPEpairKey(int ent, char *key, vec3_t v);
int AAS_FloatForBSPEpairKey(int ent, char *key, float *value);
int AAS_IntForBSPEpairKey(int ent, char *key, int *value);

