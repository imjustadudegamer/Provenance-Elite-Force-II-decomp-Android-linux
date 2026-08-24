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


#include "../qcommon/q_shared.h"
#include "l_log.h"
#include "l_memory.h"
#include "l_script.h"
#include "l_libvar.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_aas_funcs.h"
#include "be_aas_def.h"

extern int Sys_MilliSeconds(void);


extern botlib_import_t botimport;


#define AAS_MAX_REACHABILITYSIZE			65536
#define REACHABILITYAREASPERCYCLE			15
#define INSIDEUNITS							2
#define INSIDEUNITS_WALKEND					5
#define INSIDEUNITS_WALKSTART				0.1
#define INSIDEUNITS_WATERJUMP				15
#define AREA_WEAPONJUMP						8192	
int reach_swim;			
int reach_equalfloor;	
int reach_step;			
int reach_walk;			
int reach_barrier;		
int reach_waterjump;	
int reach_walkoffledge;	
int reach_jump;			
int reach_ladder;		
int reach_teleport;		
int reach_elevator;		
int reach_funcbob;		
int reach_grapple;		
int reach_doublejump;	
int reach_rampjump;		
int reach_strafejump;	
int reach_rocketjump;	
int reach_bfgjump;		
int reach_jumppad;		
int calcgrapplereach;
typedef struct aas_lreachability_s
{
	int areanum;					
	int facenum;					
	int edgenum;					
	vec3_t start;					
	vec3_t end;						
	int traveltype;					
	unsigned short int traveltime;	
	struct aas_lreachability_s *next;
} aas_lreachability_t;
aas_lreachability_t *reachabilityheap;	
aas_lreachability_t *nextreachability;	
aas_lreachability_t **areareachability;	
int numlreachabilities;

float AAS_FaceArea(aas_face_t *face)
{
	int i, edgenum, side;
	float total;
	vec_t *v;
	vec3_t d1, d2, cross;
	aas_edge_t *edge;

	edgenum = aasworld.edgeindex[face->firstedge];
	side = edgenum < 0;
	edge = &aasworld.edges[abs(edgenum)];
	v = aasworld.vertexes[edge->v[side]];

	total = 0;
	for (i = 1; i < face->numedges - 1; i++)
	{
		edgenum = aasworld.edgeindex[face->firstedge + i];
		side = edgenum < 0;
		edge = &aasworld.edges[abs(edgenum)];
		VectorSubtract(aasworld.vertexes[edge->v[side]], v, d1);
		VectorSubtract(aasworld.vertexes[edge->v[!side]], v, d2);
		CrossProduct(d1, d2, cross);
		total += 0.5 * VectorLength(cross);
	} 
	return total;
} 
float AAS_AreaVolume(int areanum)
{
	int i, edgenum, facenum, side;
	vec_t d, a, volume;
	vec3_t corner;
	aas_plane_t *plane;
	aas_edge_t *edge;
	aas_face_t *face;
	aas_area_t *area;

	area = &aasworld.areas[areanum];
	facenum = aasworld.faceindex[area->firstface];
	face = &aasworld.faces[abs(facenum)];
	edgenum = aasworld.edgeindex[face->firstedge];
	edge = &aasworld.edges[abs(edgenum)];
	VectorCopy(aasworld.vertexes[edge->v[0]], corner);

	volume = 0;
	for (i = 0; i < area->numfaces; i++)
	{
		facenum = abs(aasworld.faceindex[area->firstface + i]);
		face = &aasworld.faces[facenum];
		side = face->backarea != areanum;
		plane = &aasworld.planes[face->planenum ^ side];
		d = -(DotProduct (corner, plane->normal) - plane->dist);
		a = AAS_FaceArea(face);
		volume += d * a;
	} 

	volume /= 3;
	return volume;
} 
int AAS_BestReachableLinkArea(aas_link_t *areas)
{
	aas_link_t *link;

	for (link = areas; link; link = link->next_area)
	{
		if (AAS_AreaGrounded(link->areanum) || AAS_AreaSwim(link->areanum))
		{
			return link->areanum;
		} 
	} 
	for (link = areas; link; link = link->next_area)
	{
		if (link->areanum) return link->areanum;
		if (AAS_AreaReachability(link->areanum))
			return link->areanum;
	} 
	return 0;
} 
int AAS_GetJumpPadInfo(int ent, vec3_t areastart, vec3_t absmins, vec3_t absmaxs, vec3_t velocity)
{
	int modelnum, ent2;
	float speed, height, gravity, time, dist, forward;
	vec3_t origin, angles, teststart, ent2origin;
	aas_trace_t trace;
	char model[MAX_EPAIRKEY];
	char target[MAX_EPAIRKEY], targetname[MAX_EPAIRKEY];

	AAS_FloatForBSPEpairKey(ent, "speed", &speed);
	if (!speed) speed = 1000;
	VectorClear(angles);
	AAS_ValueForBSPEpairKey(ent, "model", model, MAX_EPAIRKEY);
	if (model[0]) modelnum = atoi(model+1);
	else modelnum = 0;
	AAS_BSPModelMinsMaxsOrigin(modelnum, angles, absmins, absmaxs, origin);
	VectorAdd(origin, absmins, absmins);
	VectorAdd(origin, absmaxs, absmaxs);
	VectorAdd(absmins, absmaxs, origin);
	VectorScale (origin, 0.5, origin);

	VectorCopy(origin, teststart);
	teststart[2] += 64;
	trace = AAS_TraceClientBBox(teststart, origin, PRESENCE_CROUCH, -1);
	if (trace.startsolid)
	{
		botimport.Print(PRT_MESSAGE, "trigger_push start solid\n");
		VectorCopy(origin, areastart);
	} 
	else
	{
		VectorCopy(trace.endpos, areastart);
	} 
	areastart[2] += 0.125;
	AAS_ValueForBSPEpairKey(ent, "target", target, MAX_EPAIRKEY);
	for (ent2 = AAS_NextBSPEntity(0); ent2; ent2 = AAS_NextBSPEntity(ent2))
	{
		if (!AAS_ValueForBSPEpairKey(ent2, "targetname", targetname, MAX_EPAIRKEY)) continue;
		if (!strcmp(targetname, target)) break;
	} 
	if (!ent2)
	{
		botimport.Print(PRT_MESSAGE, "trigger_push without target entity %s\n", target);
		return qfalse;
	} 
	AAS_VectorForBSPEpairKey(ent2, "origin", ent2origin);
	height = ent2origin[2] - origin[2];
	gravity = aassettings.phys_gravity;
	time = sqrt( height / ( 0.5 * gravity ) );
	if (!time)
	{
		botimport.Print(PRT_MESSAGE, "trigger_push without time\n");
		return qfalse;
	} 
	VectorSubtract ( ent2origin, origin, velocity);
	dist = VectorNormalize( velocity);
	forward = dist / time;
	forward *= 1.1f;
	VectorScale(velocity, forward, velocity);
	velocity[2] = time * gravity;
	return qtrue;
} 
int AAS_BestReachableFromJumpPadArea(vec3_t origin, vec3_t mins, vec3_t maxs)
{
	int ent, bot_visualizejumppads, bestareanum;
	float volume, bestareavolume;
	vec3_t areastart, cmdmove, bboxmins, bboxmaxs;
	vec3_t absmins, absmaxs, velocity;
	aas_clientmove_t move;
	aas_link_t *areas, *link;
	char classname[MAX_EPAIRKEY];

#ifdef BSPC
	bot_visualizejumppads = 0;
#else
	bot_visualizejumppads = LibVarValue("bot_visualizejumppads", "0");
#endif
	VectorAdd(origin, mins, bboxmins);
	VectorAdd(origin, maxs, bboxmaxs);
	for (ent = AAS_NextBSPEntity(0); ent; ent = AAS_NextBSPEntity(ent))
	{
		if (!AAS_ValueForBSPEpairKey(ent, "classname", classname, MAX_EPAIRKEY)) continue;
		if (strcmp(classname, "trigger_push")) continue;
		if (!AAS_GetJumpPadInfo(ent, areastart, absmins, absmaxs, velocity)) continue;
		areas = AAS_LinkEntityClientBBox(absmins, absmaxs, -1, PRESENCE_CROUCH);
		for (link = areas; link; link = link->next_area)
		{
			if (AAS_AreaJumpPad(link->areanum)) break;
		} 
		if (!link)
		{
			botimport.Print(PRT_MESSAGE, "trigger_push not in any jump pad area\n");
			AAS_UnlinkFromAreas(areas);
			continue;
		} 
		VectorSet(cmdmove, 0, 0, 0);
		Com_Memset(&move, 0, sizeof(aas_clientmove_t));
		AAS_ClientMovementHitBBox(&move, -1, areastart, PRESENCE_NORMAL, qfalse,
								velocity, cmdmove, 0, 30, 0.1f, bboxmins, bboxmaxs, bot_visualizejumppads);
		if (move.frames < 30)
		{
			bestareanum = 0;
			bestareavolume = 0;
			for (link = areas; link; link = link->next_area)
			{
				if (!AAS_AreaJumpPad(link->areanum)) continue;
				volume = AAS_AreaVolume(link->areanum);
				if (volume >= bestareavolume)
				{
					bestareanum = link->areanum;
					bestareavolume = volume;
				} 
			} 
			AAS_UnlinkFromAreas(areas);
			return bestareanum;
		} 
		AAS_UnlinkFromAreas(areas);
	} 
	return 0;
} 
int AAS_BestReachableArea(vec3_t origin, vec3_t mins, vec3_t maxs, vec3_t goalorigin)
{
	int areanum, i, j, k, l;
	aas_link_t *areas;
	vec3_t absmins, absmaxs;
	vec3_t start, end;
	aas_trace_t trace;

	if (!aasworld.loaded)
	{
		botimport.Print(PRT_ERROR, "AAS_BestReachableArea: aas not loaded\n");
		return 0;
	} 
	VectorCopy(origin, start);
	areanum = AAS_PointAreaNum(start);
	for (i = 0; i < 5 && !areanum; i++)
	{
		for (j = 0; j < 5 && !areanum; j++)
		{
			for (k = -1; k <= 1 && !areanum; k++)
			{
				for (l = -1; l <= 1 && !areanum; l++)
				{
					VectorCopy(origin, start);
					start[0] += (float) j * 4 * k;
					start[1] += (float) j * 4 * l;
					start[2] += (float) i * 4;
					areanum = AAS_PointAreaNum(start);
				} 
			} 
		} 
	} 
	if (areanum)
	{
		VectorCopy(start, end);
		start[2] += 0.25;
		end[2] -= 50;
		trace = AAS_TraceClientBBox(start, end, PRESENCE_CROUCH, -1);
		if (!trace.startsolid)
		{
			areanum = AAS_PointAreaNum(trace.endpos);
			VectorCopy(trace.endpos, goalorigin);
			if (areanum) return areanum;
		} 
		else
		{
#if 0
			if (AAS_PointAreaNum(start))
			{
				Log_Write("point %f %f %f in area %d but trace startsolid", start[0], start[1], start[2], areanum);
				AAS_DrawPermanentCross(start, 4, LINECOLOR_RED);
			} 
			botimport.Print(PRT_MESSAGE, "AAS_BestReachableArea: start solid\n");
#endif
			VectorCopy(start, goalorigin);
			return areanum;
		} 
	} 
	//NOTE: the goal origin does not have to be in the goal area
	VectorCopy(origin, goalorigin);
	VectorAdd(origin, mins, absmins);
	VectorAdd(origin, maxs, absmaxs);
	areas = AAS_LinkEntityClientBBox(absmins, absmaxs, -1, PRESENCE_CROUCH);
	areanum = AAS_BestReachableLinkArea(areas);
	AAS_UnlinkFromAreas(areas);
	return areanum;
} 
void AAS_SetupReachabilityHeap(void)
{
	int i;

	reachabilityheap = (aas_lreachability_t *) GetClearedMemory(
						AAS_MAX_REACHABILITYSIZE * sizeof(aas_lreachability_t));
	for (i = 0; i < AAS_MAX_REACHABILITYSIZE-1; i++)
	{
		reachabilityheap[i].next = &reachabilityheap[i+1];
	} 
	reachabilityheap[AAS_MAX_REACHABILITYSIZE-1].next = NULL;
	nextreachability = reachabilityheap;
	numlreachabilities = 0;
} 
void AAS_ShutDownReachabilityHeap(void)
{
	FreeMemory(reachabilityheap);
	numlreachabilities = 0;
} 
aas_lreachability_t *AAS_AllocReachability(void)
{
	aas_lreachability_t *r;

	if (!nextreachability) return NULL;
	if (!nextreachability->next) AAS_Error("AAS_MAX_REACHABILITYSIZE\n");
	r = nextreachability;
	nextreachability = nextreachability->next;
	numlreachabilities++;
	return r;
} 
void AAS_FreeReachability(aas_lreachability_t *lreach)
{
	Com_Memset(lreach, 0, sizeof(aas_lreachability_t));

	lreach->next = nextreachability;
	nextreachability = lreach;
	numlreachabilities--;
} 
int AAS_AreaReachability(int areanum)
{
	if (areanum < 0 || areanum >= aasworld.numareas)
	{
		AAS_Error("AAS_AreaReachability: areanum %d out of range\n", areanum);
		return 0;
	} 
	return aasworld.areasettings[areanum].numreachableareas;
} 
float AAS_AreaGroundFaceArea(int areanum)
{
	int i;
	float total;
	aas_area_t *area;
	aas_face_t *face;

	total = 0;
	area = &aasworld.areas[areanum];
	for (i = 0; i < area->numfaces; i++)
	{
		face = &aasworld.faces[abs(aasworld.faceindex[area->firstface + i])];
		if (!(face->faceflags & FACE_GROUND)) continue;
		total += AAS_FaceArea(face);
	} 
	return total;
} 
void AAS_FaceCenter(int facenum, vec3_t center)
{
	int i;
	float scale;
	aas_face_t *face;
	aas_edge_t *edge;

	face = &aasworld.faces[facenum];

	VectorClear(center);
	for (i = 0; i < face->numedges; i++)
	{
		edge = &aasworld.edges[abs(aasworld.edgeindex[face->firstedge + i])];
		VectorAdd(center, aasworld.vertexes[edge->v[0]], center);
		VectorAdd(center, aasworld.vertexes[edge->v[1]], center);
	} 
	scale = 0.5 / face->numedges;
	VectorScale(center, scale, center);
} 
int AAS_FallDamageDistance(void)
{
	float maxzvelocity, gravity, t;

	maxzvelocity = sqrt(30 * 10000);
	gravity = aassettings.phys_gravity;
	t = maxzvelocity / gravity;
	return 0.5 * gravity * t * t;
} 
float AAS_FallDelta(float distance)
{
	float t, delta, gravity;

	gravity = aassettings.phys_gravity;
	t = sqrt(fabs(distance) * 2 / gravity);
	delta = t * gravity;
	return delta * delta * 0.0001;
} 
float AAS_MaxJumpHeight(float phys_jumpvel)
{
	float phys_gravity;

	phys_gravity = aassettings.phys_gravity;
	return 0.5 * phys_gravity * (phys_jumpvel / phys_gravity) * (phys_jumpvel / phys_gravity);
} 
float AAS_MaxJumpDistance(float phys_jumpvel)
{
	float phys_gravity, phys_maxvelocity, t;

	phys_gravity = aassettings.phys_gravity;
	phys_maxvelocity = aassettings.phys_maxvelocity;
	t = sqrt(aassettings.rs_maxjumpfallheight / (0.5 * phys_gravity));
	return phys_maxvelocity * (t + phys_jumpvel / phys_gravity);
} 
int AAS_AreaCrouch(int areanum)
{
	if (!(aasworld.areasettings[areanum].presencetype & PRESENCE_NORMAL)) return qtrue;
	else return qfalse;
} 
int AAS_AreaSwim(int areanum)
{
	if (aasworld.areasettings[areanum].areaflags & AREA_LIQUID) return qtrue;
	else return qfalse;
} 
int AAS_AreaLiquid(int areanum)
{
	if (aasworld.areasettings[areanum].areaflags & AREA_LIQUID) return qtrue;
	else return qfalse;
} 
int AAS_AreaLava(int areanum)
{
	return (aasworld.areasettings[areanum].contents & AREACONTENTS_LAVA);
} 
int AAS_AreaSlime(int areanum)
{
	return (aasworld.areasettings[areanum].contents & AREACONTENTS_SLIME);
} 
int AAS_AreaGrounded(int areanum)
{
	return (aasworld.areasettings[areanum].areaflags & AREA_GROUNDED);
} 
int AAS_AreaLadder(int areanum)
{
	return (aasworld.areasettings[areanum].areaflags & AREA_LADDER);
} 
int AAS_AreaJumpPad(int areanum)
{
	return (aasworld.areasettings[areanum].contents & AREACONTENTS_JUMPPAD);
} 
int AAS_AreaTeleporter(int areanum)
{
	return (aasworld.areasettings[areanum].contents & AREACONTENTS_TELEPORTER);
} 
int AAS_AreaClusterPortal(int areanum)
{
	return (aasworld.areasettings[areanum].contents & AREACONTENTS_CLUSTERPORTAL);
} 
int AAS_AreaDoNotEnter(int areanum)
{
	return (aasworld.areasettings[areanum].contents & AREACONTENTS_DONOTENTER);
} 
unsigned short int AAS_BarrierJumpTravelTime(void)
{
	return aassettings.phys_jumpvel / (aassettings.phys_gravity * 0.1);
} 
qboolean AAS_ReachabilityExists(int area1num, int area2num)
{
	aas_lreachability_t *r;

	for (r = areareachability[area1num]; r; r = r->next)
	{
		if (r->areanum == area2num) return qtrue;
	} 
	return qfalse;
} 
int AAS_NearbySolidOrGap(vec3_t start, vec3_t end)
{
	vec3_t dir, testpoint;
	int areanum;

	VectorSubtract(end, start, dir);
	dir[2] = 0;
	VectorNormalize(dir);
	VectorMA(end, 48, dir, testpoint);

	areanum = AAS_PointAreaNum(testpoint);
	if (!areanum)
	{
		testpoint[2] += 16;
		areanum = AAS_PointAreaNum(testpoint);
		if (!areanum) return qtrue;
	} 
	VectorMA(end, 64, dir, testpoint);
	areanum = AAS_PointAreaNum(testpoint);
	if (areanum)
	{
		if (!AAS_AreaSwim(areanum) && !AAS_AreaGrounded(areanum)) return qtrue;
	} 
	return qfalse;
} 
int AAS_Reachability_Swim(int area1num, int area2num)
{
	int i, j, face1num, face2num, side1;
	aas_area_t *area1, *area2;
	aas_lreachability_t *lreach;
	aas_face_t *face1;
	aas_plane_t *plane;
	vec3_t start;

	if (!AAS_AreaSwim(area1num) || !AAS_AreaSwim(area2num)) return qfalse;
	if (!(aasworld.areasettings[area2num].presencetype & PRESENCE_NORMAL)) return qfalse;

	area1 = &aasworld.areas[area1num];
	area2 = &aasworld.areas[area2num];

	for (i = 0; i < 3; i++)
	{
		if (area1->mins[i] > area2->maxs[i] + 10) return qfalse;
		if (area1->maxs[i] < area2->mins[i] - 10) return qfalse;
	} 
	for (i = 0; i < area1->numfaces; i++)
	{
		face1num = aasworld.faceindex[area1->firstface + i];
		side1 = face1num < 0;
		face1num = abs(face1num);
		for (j = 0; j < area2->numfaces; j++)
		{
			face2num = abs(aasworld.faceindex[area2->firstface + j]);
			if (face1num == face2num)
			{
				AAS_FaceCenter(face1num, start);
				if (AAS_PointContents(start) & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER))
				{
					face1 = &aasworld.faces[face1num];
					lreach = AAS_AllocReachability();
					if (!lreach) return qfalse;
					lreach->areanum = area2num;
					lreach->facenum = face1num;
					lreach->edgenum = 0;
					VectorCopy(start, lreach->start);
					plane = &aasworld.planes[face1->planenum ^ side1];
					VectorMA(lreach->start, -INSIDEUNITS, plane->normal, lreach->end);
					lreach->traveltype = TRAVEL_SWIM;
					lreach->traveltime = 1;
					if (AAS_AreaVolume(area2num) < 800)
						lreach->traveltime += 200;
					lreach->next = areareachability[area1num];
					areareachability[area1num] = lreach;
					reach_swim++;
					return qtrue;
				} 
			} 
		} 
	} 
	return qfalse;
} 
int AAS_Reachability_EqualFloorHeight(int area1num, int area2num)
{
	int i, j, edgenum, edgenum1, edgenum2, foundreach, side;
	float height, bestheight, length, bestlength;
	vec3_t dir, start, end, normal, invgravity, gravitydirection = {0, 0, -1};
	vec3_t edgevec;
	aas_area_t *area1, *area2;
	aas_face_t *face1, *face2;
	aas_edge_t *edge;
	aas_plane_t *plane2;
	aas_lreachability_t lr, *lreach;

	if (!AAS_AreaGrounded(area1num) || !AAS_AreaGrounded(area2num)) return qfalse;

	area1 = &aasworld.areas[area1num];
	area2 = &aasworld.areas[area2num];
	for (i = 0; i < 2; i++)
	{
		if (area1->mins[i] > area2->maxs[i] + 10) return qfalse;
		if (area1->maxs[i] < area2->mins[i] - 10) return qfalse;
	} 
	if (area2->mins[2] > area1->maxs[2]) return qfalse;
	VectorCopy(gravitydirection, invgravity);
	VectorInverse(invgravity);
	bestheight = 99999;
	bestlength = 0;
	foundreach = qfalse;
	Com_Memset(&lr, 0, sizeof(aas_lreachability_t)); 
	for (i = 0; i < area1->numfaces; i++)
	{
		face1 = &aasworld.faces[abs(aasworld.faceindex[area1->firstface + i])];
		if (!(face1->faceflags & FACE_GROUND)) continue;
		for (j = 0; j < area2->numfaces; j++)
		{
			face2 = &aasworld.faces[abs(aasworld.faceindex[area2->firstface + j])];
			if (!(face2->faceflags & FACE_GROUND)) continue;
			for (edgenum1 = 0; edgenum1 < face1->numedges; edgenum1++)
			{
				for (edgenum2 = 0; edgenum2 < face2->numedges; edgenum2++)
				{
					if (abs(aasworld.edgeindex[face1->firstedge + edgenum1]) !=
							abs(aasworld.edgeindex[face2->firstedge + edgenum2]))
									continue;
					edgenum = aasworld.edgeindex[face1->firstedge + edgenum1];
					side = edgenum < 0;
					edge = &aasworld.edges[abs(edgenum)];
					VectorSubtract(aasworld.vertexes[edge->v[1]],
								aasworld.vertexes[edge->v[0]], dir);
					length = VectorLength(dir);
					VectorAdd(aasworld.vertexes[edge->v[0]],
								aasworld.vertexes[edge->v[1]], start);
					VectorScale(start, 0.5, start);
					VectorCopy(start, end);
					VectorSubtract(aasworld.vertexes[edge->v[side]],
								aasworld.vertexes[edge->v[!side]], edgevec);
					plane2 = &aasworld.planes[face2->planenum];
					CrossProduct(edgevec, plane2->normal, normal);
					VectorNormalize(normal);
					VectorMA(end, INSIDEUNITS_WALKEND, normal, end);
					VectorMA(start, INSIDEUNITS_WALKSTART, normal, start);
					end[2] += 0.125;
					height = DotProduct(invgravity, start);
					if (height < bestheight ||
							(height < bestheight + 1 && length > bestlength))
					{
						bestheight = height;
						bestlength = length;
						lr.areanum = area2num;
						lr.facenum = 0;
						lr.edgenum = edgenum;
						VectorCopy(start, lr.start);
						VectorCopy(end, lr.end);
						lr.traveltype = TRAVEL_WALK;
						lr.traveltime = 1;
						foundreach = qtrue;
					} 
				} 
			} 
		} 
	} 
	if (foundreach)
	{
		lreach = AAS_AllocReachability();
		if (!lreach) return qfalse;
		lreach->areanum = lr.areanum;
		lreach->facenum = lr.facenum;
		lreach->edgenum = lr.edgenum;
		VectorCopy(lr.start, lreach->start);
		VectorCopy(lr.end, lreach->end);
		lreach->traveltype = lr.traveltype;
		lreach->traveltime = lr.traveltime;
		lreach->next = areareachability[area1num];
		areareachability[area1num] = lreach;
		if (!AAS_AreaCrouch(area1num) && AAS_AreaCrouch(area2num))
		{
			lreach->traveltime += aassettings.rs_startcrouch;
		} 
		reach_equalfloor++;
		return qtrue;
	} 
	return qfalse;
} 
int AAS_Reachability_Step_Barrier_WaterJump_WalkOffLedge(int area1num, int area2num)
{
	int i, j, k, l, edge1num, edge2num, areas[10], numareas;
	int ground_bestarea2groundedgenum, ground_foundreach;
	int water_bestarea2groundedgenum, water_foundreach;
	int side1, area1swim, faceside1, groundface1num;
	float dist, dist1, dist2, diff, ortdot;
	float x1, x2, x3, x4, y1, y2, y3, y4, tmp, y;
	float length, ground_bestlength, water_bestlength, ground_bestdist, water_bestdist;
	vec3_t v1, v2, v3, v4, tmpv, p1area1, p1area2, p2area1, p2area2;
	vec3_t normal, ort, edgevec, start, end, dir;
	vec3_t ground_beststart = {0, 0, 0}, ground_bestend = {0, 0, 0}, ground_bestnormal = {0, 0, 0};
	vec3_t water_beststart = {0, 0, 0}, water_bestend = {0, 0, 0}, water_bestnormal = {0, 0, 0};
	vec3_t invgravity = {0, 0, 1};
	vec3_t testpoint;
	aas_plane_t *plane;
	aas_area_t *area1, *area2;
	aas_face_t *groundface1, *groundface2;
	aas_edge_t *edge1, *edge2;
	aas_lreachability_t *lreach;
	aas_trace_t trace;

	if (!AAS_AreaGrounded(area1num) && !AAS_AreaSwim(area1num)) return qfalse;
	if (!AAS_AreaGrounded(area2num) && !AAS_AreaSwim(area2num)) return qfalse;
	area1 = &aasworld.areas[area1num];
	area2 = &aasworld.areas[area2num];
	area1swim = AAS_AreaSwim(area1num);
	for (i = 0; i < 2; i++)
	{
		if (area1->mins[i] > area2->maxs[i] + 10) return qfalse;
		if (area1->maxs[i] < area2->mins[i] - 10) return qfalse;
	} 
	ground_foundreach = qfalse;
	ground_bestdist = 99999;
	ground_bestlength = 0;
	ground_bestarea2groundedgenum = 0;
	water_foundreach = qfalse;
	water_bestdist = 99999;
	water_bestlength = 0;
	water_bestarea2groundedgenum = 0;
	for (i = 0; i < area1->numfaces; i++)
	{
		groundface1num = aasworld.faceindex[area1->firstface + i];
		faceside1 = groundface1num < 0;
		groundface1 = &aasworld.faces[abs(groundface1num)];
		if (!(groundface1->faceflags & FACE_GROUND))
		{
			if (area1swim)
			{
				plane = &aasworld.planes[groundface1->planenum ^ (!faceside1)];
				if (DotProduct(plane->normal, invgravity) < 0.7) continue;
			} 
			else
			{
				continue;
			} 
		} 
		for (k = 0; k < groundface1->numedges; k++)
		{
			edge1num = aasworld.edgeindex[groundface1->firstedge + k];
			side1 = (edge1num < 0);
			//NOTE: for water faces we must take the side area 1 is
			if (!(groundface1->faceflags & FACE_GROUND)) side1 = (side1 == faceside1);
			edge1num = abs(edge1num);
			edge1 = &aasworld.edges[edge1num];
			VectorCopy(aasworld.vertexes[edge1->v[!side1]], v1);
			VectorCopy(aasworld.vertexes[edge1->v[side1]], v2);
			VectorSubtract(v2, v1, edgevec);
			CrossProduct(edgevec, invgravity, normal);
			VectorNormalize(normal);
			dist = DotProduct(normal, v1);
			for (j = 0; j < area2->numfaces; j++)
			{
				groundface2 = &aasworld.faces[abs(aasworld.faceindex[area2->firstface + j])];
				if (!(groundface2->faceflags & FACE_GROUND)) continue;
				for (l = 0; l < groundface2->numedges; l++)
				{
					edge2num = abs(aasworld.edgeindex[groundface2->firstedge + l]);
					edge2 = &aasworld.edges[edge2num];
					VectorCopy(aasworld.vertexes[edge2->v[0]], v3);
					VectorCopy(aasworld.vertexes[edge2->v[1]], v4);
					diff = DotProduct(normal, v3) - dist;
					if (diff < -0.1 || diff > 0.1) continue;
					diff = DotProduct(normal, v4) - dist;
					if (diff < -0.1 || diff > 0.1) continue;
					CrossProduct(invgravity, normal, ort);
					ortdot = DotProduct(ort, ort);
					y1 = v1[2];
					y2 = v2[2];
					y3 = v3[2];
					y4 = v4[2];
					x1 = DotProduct(v1, ort) / ortdot;
					x2 = DotProduct(v2, ort) / ortdot;
					x3 = DotProduct(v3, ort) / ortdot;
					x4 = DotProduct(v4, ort) / ortdot;
					if (x1 > x2)
					{
						tmp = x1; x1 = x2; x2 = tmp;
						tmp = y1; y1 = y2; y2 = tmp;
						VectorCopy(v1, tmpv); VectorCopy(v2, v1); VectorCopy(tmpv, v2);
					} 
					if (x3 > x4)
					{
						tmp = x3; x3 = x4; x4 = tmp;
						tmp = y3; y3 = y4; y4 = tmp;
						VectorCopy(v3, tmpv); VectorCopy(v4, v3); VectorCopy(tmpv, v4);
					} 
					if (x2 <= x3 || x4 <= x1)
					{
						continue;
					} 
					if ((x1 - 0.5 < x3 && x4 < x2 + 0.5) &&
							(x3 - 0.5 < x1 && x2 < x4 + 0.5))
					{
						dist1 = y3 - y1;
						dist2 = y4 - y2;
						VectorCopy(v1, p1area1);
						VectorCopy(v2, p2area1);
						VectorCopy(v3, p1area2);
						VectorCopy(v4, p2area2);
					} 
					else
					{
						if (x1 > x3 - 0.1 && x1 < x3 + 0.1)
						{
							dist1 = y3 - y1;
							VectorCopy(v1, p1area1);
							VectorCopy(v3, p1area2);
						} 
						else if (x1 < x3)
						{
							y = y1 + (x3 - x1) * (y2 - y1) / (x2 - x1);
							dist1 = y3 - y;
							VectorCopy(v3, p1area1);
							p1area1[2] = y;
							VectorCopy(v3, p1area2);
						} 
						else
						{
							y = y3 + (x1 - x3) * (y4 - y3) / (x4 - x3);
							dist1 = y - y1;
							VectorCopy(v1, p1area1);
							VectorCopy(v1, p1area2);
							p1area2[2] = y;
						} 
						if (x2 > x4 - 0.1 && x2 < x4 + 0.1)
						{
							dist2 = y4 - y2;
							VectorCopy(v2, p2area1);
							VectorCopy(v4, p2area2);
						} 
						else if (x2 < x4)
						{
							y = y3 + (x2 - x3) * (y4 - y3) / (x4 - x3);
							dist2 = y - y2;
							VectorCopy(v2, p2area1);
							VectorCopy(v2, p2area2);
							p2area2[2] = y;
						} 
						else
						{
							y = y1 + (x4 - x1) * (y2 - y1) / (x2 - x1);
							dist2 = y4 - y;
							VectorCopy(v4, p2area1);
							p2area1[2] = y;
							VectorCopy(v4, p2area2);
						} 
					} 
					if (dist1 > dist2 - 1 && dist1 < dist2 + 1)
					{
						dist = dist1;
						VectorAdd(p1area1, p2area1, start);
						VectorScale(start, 0.5, start);
						VectorAdd(p1area2, p2area2, end);
						VectorScale(end, 0.5, end);
					} 
					else if (dist1 < dist2)
					{
						dist = dist1;
						VectorCopy(p1area1, start);
						VectorCopy(p1area2, end);
					} 
					else
					{
						dist = dist2;
						VectorCopy(p2area1, start);
						VectorCopy(p2area2, end);
					} 
					VectorSubtract(p2area2, p1area2, dir);
					length = VectorLength(dir);
					if (groundface1->faceflags & FACE_GROUND)
					{
						if (dist < ground_bestdist ||
								(dist < ground_bestdist + 1 && length > ground_bestlength))
						{
							ground_bestdist = dist;
							ground_bestlength = length;
							ground_foundreach = qtrue;
							ground_bestarea2groundedgenum = edge1num;
							VectorCopy(start, ground_beststart);
							VectorCopy(normal, ground_bestnormal);
							VectorCopy(end, ground_bestend);
						} 
					} 
					else
					{
						if (dist < water_bestdist ||
								(dist < water_bestdist + 1 && length > water_bestlength))
						{
							water_bestdist = dist;
							water_bestlength = length;
							water_foundreach = qtrue;
							water_bestarea2groundedgenum = edge1num;
							VectorCopy(start, water_beststart);
							VectorCopy(normal, water_bestnormal);
							VectorCopy(end, water_bestend);
						} 
					} 
				} 
			} 
		} 
	} 
	if (ground_foundreach)
	{
		if (ground_bestdist >= 0 && ground_bestdist < aassettings.phys_maxstep)
		{
			lreach = AAS_AllocReachability();
			if (!lreach) return qfalse;
			lreach->areanum = area2num;
			lreach->facenum = 0;
			lreach->edgenum = ground_bestarea2groundedgenum;
			VectorMA(ground_beststart, INSIDEUNITS_WALKSTART, ground_bestnormal, lreach->start);
			VectorMA(ground_bestend, INSIDEUNITS_WALKEND, ground_bestnormal, lreach->end);
			lreach->traveltype = TRAVEL_WALK;
			lreach->traveltime = 0;
			if (!AAS_AreaCrouch(area1num) && AAS_AreaCrouch(area2num))
			{
				lreach->traveltime += aassettings.rs_startcrouch;
			} 
			lreach->next = areareachability[area1num];
			areareachability[area1num] = lreach;
			reach_step++;
			return qtrue;
		} 
	} 
	if (water_foundreach)
	{
		VectorMA(water_bestend, -INSIDEUNITS, water_bestnormal, testpoint);
		testpoint[2] -= aassettings.phys_maxwaterjump;
		if (aasworld.areasettings[AAS_PointAreaNum(testpoint)].areaflags & AREA_LIQUID)
		{
			if (water_bestdist < aassettings.phys_maxwaterjump + 24)
			{
				if ((aasworld.areasettings[area1num].presencetype & PRESENCE_NORMAL) &&
						(aasworld.areasettings[area2num].presencetype & PRESENCE_NORMAL))
				{
					lreach = AAS_AllocReachability();
					if (!lreach) return qfalse;
					lreach->areanum = area2num;
					lreach->facenum = 0;
					lreach->edgenum = water_bestarea2groundedgenum;
					VectorCopy(water_beststart, lreach->start);
					VectorMA(water_bestend, INSIDEUNITS_WATERJUMP, water_bestnormal, lreach->end);
					lreach->traveltype = TRAVEL_WATERJUMP;
					lreach->traveltime = aassettings.rs_waterjump;
					lreach->next = areareachability[area1num];
					areareachability[area1num] = lreach;
					reach_waterjump++;
					return qtrue;
				} 
			} 
		} 
	} 
	if (ground_foundreach)
	{
		if (ground_bestdist > 0 && ground_bestdist < aassettings.phys_maxbarrier)
		{
			if (!water_foundreach || (ground_bestdist - water_bestdist < 16))
			{
				if (!AAS_AreaCrouch(area1num) && !AAS_AreaCrouch(area2num))
				{
					lreach = AAS_AllocReachability();
					if (!lreach) return qfalse;
					lreach->areanum = area2num;
					lreach->facenum = 0;
					lreach->edgenum = ground_bestarea2groundedgenum;
					VectorMA(ground_beststart, INSIDEUNITS_WALKSTART, ground_bestnormal, lreach->start);
					VectorMA(ground_bestend, INSIDEUNITS_WALKEND, ground_bestnormal, lreach->end);
					lreach->traveltype = TRAVEL_BARRIERJUMP;
					lreach->traveltime = aassettings.rs_barrierjump;
					lreach->next = areareachability[area1num];
					areareachability[area1num] = lreach;
					reach_barrier++;
					return qtrue;
				} 
			} 
		} 
	} 
	if (ground_foundreach)
	{
		if (ground_bestdist < 0)
		{
			if (ground_bestdist > -aassettings.phys_maxstep)
			{
				lreach = AAS_AllocReachability();
				if (!lreach) return qfalse;
				lreach->areanum = area2num;
				lreach->facenum = 0;
				lreach->edgenum = ground_bestarea2groundedgenum;
				VectorMA(ground_beststart, INSIDEUNITS_WALKSTART, ground_bestnormal, lreach->start);
				VectorMA(ground_bestend, INSIDEUNITS_WALKEND, ground_bestnormal, lreach->end);
				lreach->traveltype = TRAVEL_WALK;
				lreach->traveltime = 1;
				lreach->next = areareachability[area1num];
				areareachability[area1num] = lreach;
				reach_walk++;
				return qtrue;
			} 
			if (!aassettings.rs_maxfallheight || fabs(ground_bestdist) < aassettings.rs_maxfallheight) {
				VectorMA(ground_bestend, INSIDEUNITS, ground_bestnormal, ground_bestend);
				VectorCopy(ground_bestend, start);
				start[2] = ground_beststart[2];
				VectorCopy(ground_bestend, end);
				end[2] += 4;
				trace = AAS_TraceClientBBox(start, end, PRESENCE_NORMAL, -1);
				if (!trace.startsolid && trace.fraction >= 1.0)
				{
					trace.endpos[2] += 1;
					if (AAS_PointAreaNum(trace.endpos) == area2num)
					{
						numareas = AAS_TraceAreas(start, end, areas, NULL, ARRAY_LEN(areas));
						for (i = 0; i < numareas; i++)
							if (AAS_AreaClusterPortal(areas[i]))
								break;
						if (i >= numareas)
						{
							lreach = AAS_AllocReachability();
							if (!lreach) return qfalse;
							lreach->areanum = area2num;
							lreach->facenum = 0;
							lreach->edgenum = ground_bestarea2groundedgenum;
							VectorCopy(ground_beststart, lreach->start);
							VectorCopy(ground_bestend, lreach->end);
							lreach->traveltype = TRAVEL_WALKOFFLEDGE;
							lreach->traveltime = aassettings.rs_startwalkoffledge + fabs(ground_bestdist) * 50 / aassettings.phys_gravity;
							if (!AAS_AreaSwim(area2num) && !AAS_AreaJumpPad(area2num))
							{
								if (AAS_FallDelta(ground_bestdist) > aassettings.phys_falldelta5)
								{
									lreach->traveltime += aassettings.rs_falldamage5;
								} 
								if (AAS_FallDelta(ground_bestdist) > aassettings.phys_falldelta10)
								{
									lreach->traveltime += aassettings.rs_falldamage10;
								} 
							} 
							lreach->next = areareachability[area1num];
							areareachability[area1num] = lreach;
							reach_walkoffledge++;
							return qtrue;
						} 
					} 
				} 
			} 
		} 
	} 
	return qfalse;
} 
float VectorDistance(vec3_t v1, vec3_t v2)
{
	vec3_t dir;

	VectorSubtract(v2, v1, dir);
	return VectorLength(dir);
} 
int VectorBetweenVectors(vec3_t v, vec3_t v1, vec3_t v2)
{
	vec3_t dir1, dir2;

	VectorSubtract(v, v1, dir1);
	VectorSubtract(v, v2, dir2);
	return (DotProduct(dir1, dir2) <= 0);
} 
void VectorMiddle(vec3_t v1, vec3_t v2, vec3_t middle)
{
	VectorAdd(v1, v2, middle);
	VectorScale(middle, 0.5, middle);
} 

float AAS_ClosestEdgePoints(vec3_t v1, vec3_t v2, vec3_t v3, vec3_t v4,
							aas_plane_t *plane1, aas_plane_t *plane2,
							vec3_t beststart1, vec3_t bestend1,
							vec3_t beststart2, vec3_t bestend2, float bestdist)
{
	vec3_t dir1, dir2, p1, p2, p3, p4;
	float a1, a2, b1, b2, dist, dist1, dist2;
	int founddist;

	VectorSubtract(v2, v1, dir1);
	VectorSubtract(v4, v3, dir2);
	dir1[2] = 0;
	dir2[2] = 0;
	if (dir2[0])
	{
		a2 = dir2[1] / dir2[0];
		b2 = v3[1] - a2 * v3[0];
		p1[0] = (DotProduct(v1, dir2) - (a2 * dir2[0] + b2 * dir2[1])) / dir2[0];
		p1[1] = a2 * p1[0] + b2;
		p2[0] = (DotProduct(v2, dir2) - (a2 * dir2[0] + b2 * dir2[1])) / dir2[0];
		p2[1] = a2 * p2[0] + b2;
	} 
	else
	{
		p1[0] = v3[0];
		p1[1] = v1[1];
		p2[0] = v3[0];
		p2[1] = v2[1];
	} 
	if (dir1[0])
	{
		a1 = dir1[1] / dir1[0];
		b1 = v1[1] - a1 * v1[0];
		p3[0] = (DotProduct(v3, dir1) - (a1 * dir1[0] + b1 * dir1[1])) / dir1[0];
		p3[1] = a1 * p3[0] + b1;
		p4[0] = (DotProduct(v4, dir1) - (a1 * dir1[0] + b1 * dir1[1])) / dir1[0];
		p4[1] = a1 * p4[0] + b1;
	} 
	else
	{
		p3[0] = v1[0];
		p3[1] = v3[1];
		p4[0] = v1[0];
		p4[1] = v4[1];
	} 
	p1[2] = 0;
	p2[2] = 0;
	p3[2] = 0;
	p4[2] = 0;
	p1[2] = (plane2->dist - DotProduct(plane2->normal, p1)) / plane2->normal[2];
	p2[2] = (plane2->dist - DotProduct(plane2->normal, p2)) / plane2->normal[2];
	p3[2] = (plane1->dist - DotProduct(plane1->normal, p3)) / plane1->normal[2];
	p4[2] = (plane1->dist - DotProduct(plane1->normal, p4)) / plane1->normal[2];
	founddist = qfalse;
	if (VectorBetweenVectors(p1, v3, v4))
	{
		dist = VectorDistance(v1, p1);
		if (dist > bestdist - 0.5 && dist < bestdist + 0.5)
		{
			dist1 = VectorDistance(beststart1, v1);
			dist2 = VectorDistance(beststart2, v1);
			if (dist1 > dist2)
			{
				if (dist1 > VectorDistance(beststart1, beststart2)) VectorCopy(v1, beststart2);
			} 
			else
			{
				if (dist2 > VectorDistance(beststart1, beststart2)) VectorCopy(v1, beststart1);
			} 
			dist1 = VectorDistance(bestend1, p1);
			dist2 = VectorDistance(bestend2, p1);
			if (dist1 > dist2)
			{
				if (dist1 > VectorDistance(bestend1, bestend2)) VectorCopy(p1, bestend2);
			} 
			else
			{
				if (dist2 > VectorDistance(bestend1, bestend2)) VectorCopy(p1, bestend1);
			} 
		} 
		else if (dist < bestdist)
		{
			bestdist = dist;
			VectorCopy(v1, beststart1);
			VectorCopy(v1, beststart2);
			VectorCopy(p1, bestend1);
			VectorCopy(p1, bestend2);
		} 
		founddist = qtrue;
	} 
	if (VectorBetweenVectors(p2, v3, v4))
	{
		dist = VectorDistance(v2, p2);
		if (dist > bestdist - 0.5 && dist < bestdist + 0.5)
		{
			dist1 = VectorDistance(beststart1, v2);
			dist2 = VectorDistance(beststart2, v2);
			if (dist1 > dist2)
			{
				if (dist1 > VectorDistance(beststart1, beststart2)) VectorCopy(v2, beststart2);
			} 
			else
			{
				if (dist2 > VectorDistance(beststart1, beststart2)) VectorCopy(v2, beststart1);
			} 
			dist1 = VectorDistance(bestend1, p2);
			dist2 = VectorDistance(bestend2, p2);
			if (dist1 > dist2)
			{
				if (dist1 > VectorDistance(bestend1, bestend2)) VectorCopy(p2, bestend2);
			} 
			else
			{
				if (dist2 > VectorDistance(bestend1, bestend2)) VectorCopy(p2, bestend1);
			} 
		} 
		else if (dist < bestdist)
		{
			bestdist = dist;
			VectorCopy(v2, beststart1);
			VectorCopy(v2, beststart2);
			VectorCopy(p2, bestend1);
			VectorCopy(p2, bestend2);
		} 
		founddist = qtrue;
	} 
	if (VectorBetweenVectors(p3, v1, v2))
	{
		dist = VectorDistance(v3, p3);
		if (dist > bestdist - 0.5 && dist < bestdist + 0.5)
		{
			dist1 = VectorDistance(beststart1, p3);
			dist2 = VectorDistance(beststart2, p3);
			if (dist1 > dist2)
			{
				if (dist1 > VectorDistance(beststart1, beststart2)) VectorCopy(p3, beststart2);
			} 
			else
			{
				if (dist2 > VectorDistance(beststart1, beststart2)) VectorCopy(p3, beststart1);
			} 
			dist1 = VectorDistance(bestend1, v3);
			dist2 = VectorDistance(bestend2, v3);
			if (dist1 > dist2)
			{
				if (dist1 > VectorDistance(bestend1, bestend2)) VectorCopy(v3, bestend2);
			} 
			else
			{
				if (dist2 > VectorDistance(bestend1, bestend2)) VectorCopy(v3, bestend1);
			} 
		} 
		else if (dist < bestdist)
		{
			bestdist = dist;
			VectorCopy(p3, beststart1);
			VectorCopy(p3, beststart2);
			VectorCopy(v3, bestend1);
			VectorCopy(v3, bestend2);
		} 
		founddist = qtrue;
	} 
	if (VectorBetweenVectors(p4, v1, v2))
	{
		dist = VectorDistance(v4, p4);
		if (dist > bestdist - 0.5 && dist < bestdist + 0.5)
		{
			dist1 = VectorDistance(beststart1, p4);
			dist2 = VectorDistance(beststart2, p4);
			if (dist1 > dist2)
			{
				if (dist1 > VectorDistance(beststart1, beststart2)) VectorCopy(p4, beststart2);
			} 
			else
			{
				if (dist2 > VectorDistance(beststart1, beststart2)) VectorCopy(p4, beststart1);
			} 
			dist1 = VectorDistance(bestend1, v4);
			dist2 = VectorDistance(bestend2, v4);
			if (dist1 > dist2)
			{
				if (dist1 > VectorDistance(bestend1, bestend2)) VectorCopy(v4, bestend2);
			} 
			else
			{
				if (dist2 > VectorDistance(bestend1, bestend2)) VectorCopy(v4, bestend1);
			} 
		} 
		else if (dist < bestdist)
		{
			bestdist = dist;
			VectorCopy(p4, beststart1);
			VectorCopy(p4, beststart2);
			VectorCopy(v4, bestend1);
			VectorCopy(v4, bestend2);
		} 
		founddist = qtrue;
	} 
	if (!founddist)
	{
		dist = VectorDistance(v1, v3);
		if (dist < bestdist)
		{
			bestdist = dist;
			VectorCopy(v1, beststart1);
			VectorCopy(v1, beststart2);
			VectorCopy(v3, bestend1);
			VectorCopy(v3, bestend2);
		} 
		dist = VectorDistance(v1, v4);
		if (dist < bestdist)
		{
			bestdist = dist;
			VectorCopy(v1, beststart1);
			VectorCopy(v1, beststart2);
			VectorCopy(v4, bestend1);
			VectorCopy(v4, bestend2);
		} 
		dist = VectorDistance(v2, v3);
		if (dist < bestdist)
		{
			bestdist = dist;
			VectorCopy(v2, beststart1);
			VectorCopy(v2, beststart2);
			VectorCopy(v3, bestend1);
			VectorCopy(v3, bestend2);
		} 
		dist = VectorDistance(v2, v4);
		if (dist < bestdist)
		{
			bestdist = dist;
			VectorCopy(v2, beststart1);
			VectorCopy(v2, beststart2);
			VectorCopy(v4, bestend1);
			VectorCopy(v4, bestend2);
		} 
	} 
	return bestdist;
} 
int AAS_Reachability_Jump(int area1num, int area2num)
{
	int i, j, k, l, face1num, face2num, edge1num, edge2num, traveltype;
	int stopevent, areas[10], numareas;
	float phys_jumpvel, maxjumpdistance, maxjumpheight, height, bestdist, speed;
	vec_t *v1, *v2, *v3, *v4;
	vec3_t beststart = {0}, beststart2 = {0}, bestend = {0}, bestend2 = {0};
	vec3_t teststart, testend, dir, velocity, cmdmove, up = {0, 0, 1}, sidewards;
	aas_area_t *area1, *area2;
	aas_face_t *face1, *face2;
	aas_edge_t *edge1, *edge2;
	aas_plane_t *plane1, *plane2, *plane;
	aas_trace_t trace;
	aas_clientmove_t move;
	aas_lreachability_t *lreach;

	if (!AAS_AreaGrounded(area1num) || !AAS_AreaGrounded(area2num)) return qfalse;
	if (AAS_AreaCrouch(area1num) || AAS_AreaCrouch(area2num)) return qfalse;
	area1 = &aasworld.areas[area1num];
	area2 = &aasworld.areas[area2num];
	phys_jumpvel = aassettings.phys_jumpvel;
	maxjumpdistance = 2 * AAS_MaxJumpDistance(phys_jumpvel);
	maxjumpheight = AAS_MaxJumpHeight(phys_jumpvel);

	for (i = 0; i < 2; i++)
	{
		if (area1->mins[i] > area2->maxs[i] + maxjumpdistance) return qfalse;
		if (area1->maxs[i] < area2->mins[i] - maxjumpdistance) return qfalse;
	} 
	if (area2->mins[2] > area1->maxs[2] + maxjumpheight) return qfalse;
	bestdist = 999999;
	for (i = 0; i < area1->numfaces; i++)
	{
		face1num = aasworld.faceindex[area1->firstface + i];
		face1 = &aasworld.faces[abs(face1num)];
		if (!(face1->faceflags & FACE_GROUND)) continue;
		for (j = 0; j < area2->numfaces; j++)
		{
			face2num = aasworld.faceindex[area2->firstface + j];
			face2 = &aasworld.faces[abs(face2num)];
			if (!(face2->faceflags & FACE_GROUND)) continue;
			for (k = 0; k < face1->numedges; k++)
			{
				edge1num = abs(aasworld.edgeindex[face1->firstedge + k]);
				edge1 = &aasworld.edges[edge1num];
				for (l = 0; l < face2->numedges; l++)
				{
					edge2num = abs(aasworld.edgeindex[face2->firstedge + l]);
					edge2 = &aasworld.edges[edge2num];
					v1 = aasworld.vertexes[edge1->v[0]];
					v2 = aasworld.vertexes[edge1->v[1]];
					v3 = aasworld.vertexes[edge2->v[0]];
					v4 = aasworld.vertexes[edge2->v[1]];
					plane1 = &aasworld.planes[face1->planenum];
					plane2 = &aasworld.planes[face2->planenum];
					bestdist = AAS_ClosestEdgePoints(v1, v2, v3, v4, plane1, plane2,
														beststart, bestend,
														beststart2, bestend2, bestdist);
				} 
			} 
		} 
	} 
	VectorMiddle(beststart, beststart2, beststart);
	VectorMiddle(bestend, bestend2, bestend);
	if (bestdist > 4 && bestdist < maxjumpdistance)
	{
		if (bestdist <= 48 && fabs(beststart[2] - bestend[2]) < 8)
		{
			speed = 400;
			traveltype = TRAVEL_WALKOFFLEDGE;
		} 
		else if (AAS_HorizontalVelocityForJump(0, beststart, bestend, &speed))
		{
			speed *= 1.2f;
			traveltype = TRAVEL_WALKOFFLEDGE;
		} 
		else
		{
			if (!AAS_HorizontalVelocityForJump(phys_jumpvel, beststart, bestend, &speed))
				return qfalse;
			speed *= 1.05f;
			traveltype = TRAVEL_JUMP;
			VectorSubtract(bestend, beststart, dir);
			dir[2] = 0;
			if (VectorLength(dir) < 10)
				return qfalse;
		} 
		VectorSubtract(bestend, beststart, dir);
		VectorNormalize(dir);
		VectorMA(beststart, 1, dir, teststart);
		VectorCopy(teststart, testend);
		testend[2] -= 100;
		trace = AAS_TraceClientBBox(teststart, testend, PRESENCE_NORMAL, -1);
		if (trace.startsolid)
			return qfalse;
		if (trace.fraction < 1)
		{
			plane = &aasworld.planes[trace.planenum];
			if (DotProduct(plane->normal, up) >= 0.7)
			{
				if (!(AAS_PointContents(trace.endpos) & (CONTENTS_LAVA|CONTENTS_SLIME)))
				{
					if (teststart[2] - trace.endpos[2] <= aassettings.phys_maxbarrier)
						return qfalse;
				} 
			} 
		} 
		VectorMA(bestend, -1, dir, teststart);
		VectorCopy(teststart, testend);
		testend[2] -= 100;
		trace = AAS_TraceClientBBox(teststart, testend, PRESENCE_NORMAL, -1);
		if (trace.startsolid)
			return qfalse;
		if (trace.fraction < 1)
		{
			plane = &aasworld.planes[trace.planenum];
			if (DotProduct(plane->normal, up) >= 0.7)
			{
				if (!(AAS_PointContents(trace.endpos) & (CONTENTS_LAVA|CONTENTS_SLIME)))
				{
					if (teststart[2] - trace.endpos[2] <= aassettings.phys_maxbarrier)
						return qfalse;
				} 
			} 
		} 
		VectorClear(cmdmove);
		if ((traveltype & TRAVELTYPE_MASK) == TRAVEL_JUMP)
			cmdmove[2] = aassettings.phys_jumpvel;
		else
			cmdmove[2] = 0;
		VectorSubtract(bestend, beststart, dir);
		dir[2] = 0;
		VectorNormalize(dir);
		CrossProduct(dir, up, sidewards);
		stopevent = SE_HITGROUND|SE_ENTERWATER|SE_ENTERSLIME|SE_ENTERLAVA|SE_HITGROUNDDAMAGE;
		if (!AAS_AreaClusterPortal(area1num) && !AAS_AreaClusterPortal(area2num))
			stopevent |= SE_TOUCHCLUSTERPORTAL;
		for (i = 0; i < 3; i++)
		{
			if (i == 1)
				VectorAdd(testend, sidewards, testend);
			else if (i == 2)
				VectorSubtract(bestend, sidewards, testend);
			else
				VectorCopy(bestend, testend);
			VectorSubtract(testend, beststart, dir);
			dir[2] = 0;
			VectorNormalize(dir);
			VectorScale(dir, speed, velocity);
			AAS_PredictClientMovement(&move, -1, beststart, PRESENCE_NORMAL, qtrue,
										velocity, cmdmove, 3, 30, 0.1f,
										stopevent, 0, qfalse);
			if (move.frames >= 30)
				return qfalse;
			if (move.stopevent & (SE_ENTERSLIME|SE_ENTERLAVA))
				return qfalse;
			if (move.stopevent & SE_TOUCHCLUSTERPORTAL)
				return qfalse;
			VectorMA(move.endpos, -64, dir, teststart);
			teststart[2] += 1;
			numareas = AAS_TraceAreas(move.endpos, teststart, areas, NULL, ARRAY_LEN(areas));
			for (j = 0; j < numareas; j++)
			{
				if (areas[j] == area2num)
					break;
			} 
			if (j < numareas)
				break;
		}
		if (i >= 3)
			return qfalse;
#ifdef REACH_DEBUG
		Log_Write("jump reachability between %d and %d\r\n", area1num, area2num);
#endif 
		lreach = AAS_AllocReachability();
		if (!lreach) return qfalse;
		lreach->areanum = area2num;
		lreach->facenum = 0;
		lreach->edgenum = 0;
		VectorCopy(beststart, lreach->start);
		VectorCopy(bestend, lreach->end);
		lreach->traveltype = traveltype;

		VectorSubtract(bestend, beststart, dir);
		height = dir[2];
		dir[2] = 0;
		if ((traveltype & TRAVELTYPE_MASK) == TRAVEL_WALKOFFLEDGE && height > VectorLength(dir))
		{
			lreach->traveltime = aassettings.rs_startwalkoffledge + height * 50 / aassettings.phys_gravity;
		}
		else
		{
			lreach->traveltime = aassettings.rs_startjump + VectorDistance(bestend, beststart) * 240 / aassettings.phys_maxwalkvelocity;
		} 
		if (!AAS_AreaJumpPad(area2num))
		{
			if (AAS_FallDelta(beststart[2] - bestend[2]) > aassettings.phys_falldelta5)
			{
				lreach->traveltime += aassettings.rs_falldamage5;
			} 
			else if (AAS_FallDelta(beststart[2] - bestend[2]) > aassettings.phys_falldelta10)
			{
				lreach->traveltime += aassettings.rs_falldamage10;
			} 
		} 
		lreach->next = areareachability[area1num];
		areareachability[area1num] = lreach;
		if ((traveltype & TRAVELTYPE_MASK) == TRAVEL_JUMP)
			reach_jump++;
		else
			reach_walkoffledge++;
	} 
	return qfalse;
} 
int AAS_Reachability_Ladder(int area1num, int area2num)
{
	int i, j, k, l, edge1num, edge2num, sharededgenum = 0, lowestedgenum = 0;
	int face1num, face2num, ladderface1num = 0, ladderface2num = 0;
	int ladderface1vertical, ladderface2vertical, firstv;
	float face1area, face2area, bestface1area = -9999, bestface2area = -9999;
	float phys_jumpvel, maxjumpheight;
	vec3_t area1point, area2point, v1, v2, up = {0, 0, 1};
	vec3_t mid, lowestpoint = {0, 0}, start, end, sharededgevec, dir;
	aas_area_t *area1, *area2;
	aas_face_t *face1, *face2, *ladderface1 = NULL, *ladderface2 = NULL;
	aas_plane_t *plane1, *plane2;
	aas_edge_t *sharededge, *edge1;
	aas_lreachability_t *lreach;
	aas_trace_t trace;

	if (!AAS_AreaLadder(area1num) || !AAS_AreaLadder(area2num)) return qfalse;
	phys_jumpvel = aassettings.phys_jumpvel;
	maxjumpheight = AAS_MaxJumpHeight(phys_jumpvel);

	area1 = &aasworld.areas[area1num];
	area2 = &aasworld.areas[area2num];
	
	for (i = 0; i < area1->numfaces; i++)
	{
		face1num = aasworld.faceindex[area1->firstface + i];
		face1 = &aasworld.faces[abs(face1num)];
		if (!(face1->faceflags & FACE_LADDER)) continue;
		for (j = 0; j < area2->numfaces; j++)
		{
			face2num = aasworld.faceindex[area2->firstface + j];
			face2 = &aasworld.faces[abs(face2num)];
			if (!(face2->faceflags & FACE_LADDER)) continue;
			for (k = 0; k < face1->numedges; k++)
			{
				edge1num = aasworld.edgeindex[face1->firstedge + k];
				for (l = 0; l < face2->numedges; l++)
				{
					edge2num = aasworld.edgeindex[face2->firstedge + l];
					if (abs(edge1num) == abs(edge2num))
					{
						face1area = AAS_FaceArea(face1);
						face2area = AAS_FaceArea(face2);
						if (face1area > bestface1area && face2area > bestface2area)
						{
							bestface1area = face1area;
							bestface2area = face2area;
							ladderface1 = face1;
							ladderface2 = face2;
							ladderface1num = face1num;
							ladderface2num = face2num;
							sharededgenum = edge1num;
						} 
						break;
					} 
				} 
				if (l != face2->numedges) break;
			} 
		} 
	} 
	if (ladderface1 && ladderface2)
	{
		sharededge = &aasworld.edges[abs(sharededgenum)];
		firstv = sharededgenum < 0;
		VectorCopy(aasworld.vertexes[sharededge->v[firstv]], v1);
		VectorCopy(aasworld.vertexes[sharededge->v[!firstv]], v2);
		VectorAdd(v1, v2, area1point);
		VectorScale(area1point, 0.5, area1point);
		VectorCopy(area1point, area2point);
		plane1 = &aasworld.planes[ladderface1->planenum ^ (ladderface1num < 0)];
		plane2 = &aasworld.planes[ladderface2->planenum ^ (ladderface2num < 0)];
		VectorSubtract(v2, v1, sharededgevec);
		CrossProduct(plane1->normal, sharededgevec, dir);
		VectorNormalize(dir);
		VectorMA(area1point, -32, dir, area1point);
		VectorMA(area2point, 32, dir, area2point);
		ladderface1vertical = fabsf(DotProduct(plane1->normal, up)) < 0.1;
		ladderface2vertical = fabsf(DotProduct(plane2->normal, up)) < 0.1;
		if (!ladderface1vertical && !ladderface2vertical) return qfalse;
		if (ladderface1vertical && ladderface2vertical
					&& DotProduct(plane1->normal, plane2->normal) > 0.7
					&& fabsf(DotProduct(sharededgevec, up)) < 0.7)
		{
			lreach = AAS_AllocReachability();
			if (!lreach) return qfalse;
			lreach->areanum = area2num;
			lreach->facenum = ladderface1num;
			lreach->edgenum = abs(sharededgenum);
			VectorCopy(area1point, lreach->start);
			VectorMA(area2point, -3, plane1->normal, lreach->end);
			lreach->traveltype = TRAVEL_LADDER;
			lreach->traveltime = 10;
			lreach->next = areareachability[area1num];
			areareachability[area1num] = lreach;
			reach_ladder++;
			lreach = AAS_AllocReachability();
			if (!lreach) return qfalse;
			lreach->areanum = area1num;
			lreach->facenum = ladderface2num;
			lreach->edgenum = abs(sharededgenum);
			VectorCopy(area2point, lreach->start);
			VectorMA(area1point, -3, plane1->normal, lreach->end);
			lreach->traveltype = TRAVEL_LADDER;
			lreach->traveltime = 10;
			lreach->next = areareachability[area2num];
			areareachability[area2num] = lreach;
			reach_ladder++;
			return qtrue;
		} 
		if (ladderface1vertical && (ladderface2->faceflags & FACE_GROUND))
		{
			lreach = AAS_AllocReachability();
			if (!lreach) return qfalse;
			lreach->areanum = area2num;
			lreach->facenum = ladderface1num;
			lreach->edgenum = abs(sharededgenum);
			VectorCopy(area1point, lreach->start);
			VectorCopy(area2point, lreach->end);
			lreach->end[2] += 16;
			VectorMA(lreach->end, -15, plane1->normal, lreach->end);
			lreach->traveltype = TRAVEL_LADDER;
			lreach->traveltime = 10;
			lreach->next = areareachability[area1num];
			areareachability[area1num] = lreach;
			reach_ladder++;
			lreach = AAS_AllocReachability();
			if (!lreach) return qfalse;
			lreach->areanum = area1num;
			lreach->facenum = ladderface2num;
			lreach->edgenum = abs(sharededgenum);
			VectorCopy(area2point, lreach->start);
			VectorCopy(area1point, lreach->end);
			lreach->traveltype = TRAVEL_WALKOFFLEDGE;
			lreach->traveltime = 10;
			lreach->next = areareachability[area2num];
			areareachability[area2num] = lreach;
			reach_walkoffledge++;
			return qtrue;
		} 
		if (ladderface1vertical)
		{
			lowestpoint[2] = 99999;
			for (i = 0; i < ladderface1->numedges; i++)
			{
				edge1num = abs(aasworld.edgeindex[ladderface1->firstedge + i]);
				edge1 = &aasworld.edges[edge1num];
				VectorCopy(aasworld.vertexes[edge1->v[0]], v1);
				VectorCopy(aasworld.vertexes[edge1->v[1]], v2);
				VectorAdd(v1, v2, mid);
				VectorScale(mid, 0.5, mid);
				if (mid[2] < lowestpoint[2])
				{
					VectorCopy(mid, lowestpoint);
					lowestedgenum = edge1num;
				} 
			} 
			plane1 = &aasworld.planes[ladderface1->planenum];
			VectorMA(lowestpoint, 5, plane1->normal, start);
			VectorCopy(start, end);
			start[2] += 5;
			end[2] -= 100;
			trace = AAS_TraceClientBBox(start, end, PRESENCE_NORMAL, -1);
#ifdef REACH_DEBUG
			if (trace.startsolid)
			{
				Log_Write("trace from area %d started in solid\r\n", area1num);
			} 
#endif 
			trace.endpos[2] += 1;
			area2num = AAS_PointAreaNum(trace.endpos);
			area2 = &aasworld.areas[area2num];
			for (i = 0; i < area2->numfaces; i++)
			{
				face2num = aasworld.faceindex[area2->firstface + i];
				face2 = &aasworld.faces[abs(face2num)];
				if (face2->faceflags & FACE_LADDER)
				{
					plane2 = &aasworld.planes[face2->planenum];
					if (fabsf(DotProduct(plane2->normal, up)) < 0.1) break;
				} 
			} 
			if (i >= area2->numfaces && area2num != area1num &&
						!AAS_ReachabilityExists(area1num, area2num) &&
						!AAS_ReachabilityExists(area2num, area1num))
			{
				if (start[2] - trace.endpos[2] < maxjumpheight)
				{
					lreach = AAS_AllocReachability();
					if (!lreach) return qfalse;
					lreach->areanum = area2num;
					lreach->facenum = ladderface1num;
					lreach->edgenum = lowestedgenum;
					VectorCopy(lowestpoint, lreach->start);
					VectorCopy(trace.endpos, lreach->end);
					lreach->traveltype = TRAVEL_LADDER;
					lreach->traveltime = 10;
					lreach->next = areareachability[area1num];
					areareachability[area1num] = lreach;
					reach_ladder++;
					lreach = AAS_AllocReachability();
					if (!lreach) return qfalse;
					lreach->areanum = area1num;
					lreach->facenum = ladderface1num;
					lreach->edgenum = lowestedgenum;
					VectorCopy(trace.endpos, lreach->start);
					VectorMA(lowestpoint, -5, plane1->normal, lreach->end);
					lreach->end[2] += 10;
					lreach->traveltype = TRAVEL_JUMP;
					lreach->traveltime = 10;
					lreach->next = areareachability[area2num];
					areareachability[area2num] = lreach;
					reach_jump++;	
					return qtrue;
#ifdef REACH_DEBUG
					Log_Write("jump up to ladder reach between %d and %d\r\n", area2num, area1num);
#endif 
				} 
#ifdef REACH_DEBUG
				else Log_Write("jump too high between area %d and %d\r\n", area2num, area1num);
#endif 
			} 
		} 
	} 
	return qfalse;
} 
int AAS_TravelFlagsForTeam(int ent)
{
	int notteam;

	if (!AAS_IntForBSPEpairKey(ent, "bot_notteam", &notteam))
		return 0;
	if (notteam == 1)
		return TRAVELFLAG_NOTTEAM1;
	if (notteam == 2)
		return TRAVELFLAG_NOTTEAM2;
	return 0;
} 
void AAS_Reachability_Teleport(void)
{
	int area1num, area2num;
	char target[MAX_EPAIRKEY], targetname[MAX_EPAIRKEY];
	char classname[MAX_EPAIRKEY], model[MAX_EPAIRKEY];
	int ent, dest;
	float angle;
	vec3_t origin, destorigin, mins, maxs, end, angles;
	vec3_t mid, velocity, cmdmove;
	aas_lreachability_t *lreach;
	aas_clientmove_t move;
	aas_trace_t trace;
	aas_link_t *areas, *link;

	for (ent = AAS_NextBSPEntity(0); ent; ent = AAS_NextBSPEntity(ent))
	{
		if (!AAS_ValueForBSPEpairKey(ent, "classname", classname, MAX_EPAIRKEY)) continue;
		if (!strcmp(classname, "trigger_multiple"))
		{
			AAS_ValueForBSPEpairKey(ent, "model", model, MAX_EPAIRKEY);
			botimport.Print(PRT_MESSAGE, "trigger_multiple model = \"%s\"\n", model);
			VectorClear(angles);
			AAS_BSPModelMinsMaxsOrigin(atoi(model+1), angles, mins, maxs, origin);
			if (!AAS_ValueForBSPEpairKey(ent, "target", target, MAX_EPAIRKEY))
			{
				botimport.Print(PRT_ERROR, "trigger_multiple at %1.0f %1.0f %1.0f without target\n",
									origin[0], origin[1], origin[2]);
				continue;
			} 
			for (dest = AAS_NextBSPEntity(0); dest; dest = AAS_NextBSPEntity(dest))
			{
				if (!AAS_ValueForBSPEpairKey(dest, "classname", classname, MAX_EPAIRKEY)) continue;
				if (!strcmp(classname, "target_teleporter"))
				{
					if (!AAS_ValueForBSPEpairKey(dest, "targetname", targetname, MAX_EPAIRKEY)) continue;
					if (!strcmp(targetname, target))
					{
						break;
					} 
				} 
			} 
			if (!dest)
			{
				continue;
			} 
			if (!AAS_ValueForBSPEpairKey(dest, "target", target, MAX_EPAIRKEY))
			{
				botimport.Print(PRT_ERROR, "target_teleporter without target\n");
				continue;
			} 
		} 
		else if (!strcmp(classname, "trigger_teleport"))
		{
			AAS_ValueForBSPEpairKey(ent, "model", model, MAX_EPAIRKEY);
			botimport.Print(PRT_MESSAGE, "trigger_teleport model = \"%s\"\n", model);
			VectorClear(angles);
			AAS_BSPModelMinsMaxsOrigin(atoi(model+1), angles, mins, maxs, origin);
			if (!AAS_ValueForBSPEpairKey(ent, "target", target, MAX_EPAIRKEY))
			{
				botimport.Print(PRT_ERROR, "trigger_teleport at %1.0f %1.0f %1.0f without target\n",
									origin[0], origin[1], origin[2]);
				continue;
			} 
		} 
		else
		{
			continue;
		} 
		for (dest = AAS_NextBSPEntity(0); dest; dest = AAS_NextBSPEntity(dest))
		{
			if (AAS_ValueForBSPEpairKey(dest, "targetname", targetname, MAX_EPAIRKEY))
			{
				if (!strcmp(targetname, target))
				{
					break;
				} 
			} 
		} 
		if (!dest)
		{
			botimport.Print(PRT_ERROR, "teleporter without misc_teleporter_dest (%s)\n", target);
			continue;
		} 
		if (!AAS_VectorForBSPEpairKey(dest, "origin", destorigin))
		{
			botimport.Print(PRT_ERROR, "teleporter destination (%s) without origin\n", target);
			continue;
		} 
		area2num = AAS_PointAreaNum(destorigin);
		if (!AAS_AreaTeleporter(area2num) && !AAS_AreaJumpPad(area2num))
		{
			VectorCopy(destorigin, end);
			end[2] -= 64;
			trace = AAS_TraceClientBBox(destorigin, end, PRESENCE_CROUCH, -1);
			if (trace.startsolid)
			{
				botimport.Print(PRT_ERROR, "teleporter destination (%s) in solid\n", target);
				continue;
			} 
			{
				AAS_FloatForBSPEpairKey(dest, "angle", &angle);
				if (angle)
				{
					VectorSet(angles, 0, angle, 0);
					AngleVectors(angles, velocity, NULL, NULL);
					VectorScale(velocity, 400, velocity);
				} 
				else
				{
					VectorClear(velocity);
				} 
				VectorClear(cmdmove);
				AAS_PredictClientMovement(&move, -1, destorigin, PRESENCE_NORMAL, qfalse,
										velocity, cmdmove, 0, 30, 0.1f,
										SE_HITGROUND|SE_ENTERWATER|SE_ENTERSLIME|
										SE_ENTERLAVA|SE_HITGROUNDDAMAGE|SE_TOUCHJUMPPAD|SE_TOUCHTELEPORTER, 0, qfalse); 
				area2num = AAS_PointAreaNum(move.endpos);
				if (move.stopevent & (SE_ENTERSLIME|SE_ENTERLAVA))
				{
					botimport.Print(PRT_WARNING, "teleported into slime or lava at dest %s\n", target);
				} 
				VectorCopy(move.endpos, destorigin);
			} 
		} 
		VectorAdd(origin, mins, mins);
		VectorAdd(origin, maxs, maxs);
		VectorAdd(mins, maxs, mid);
		VectorScale(mid, 0.5, mid);
		areas = AAS_LinkEntityClientBBox(mins, maxs, -1, PRESENCE_CROUCH);
		if (!areas) botimport.Print(PRT_MESSAGE, "trigger_multiple not in any area\n");
		for (link = areas; link; link = link->next_area)
		{
			if (!AAS_AreaTeleporter(link->areanum)) continue;
			area1num = link->areanum;
			lreach = AAS_AllocReachability();
			if (!lreach) break;
			lreach->areanum = area2num;
			lreach->facenum = 0;
			lreach->edgenum = 0;
			VectorCopy(mid, lreach->start);
			VectorCopy(destorigin, lreach->end);
			lreach->traveltype = TRAVEL_TELEPORT;
			lreach->traveltype |= AAS_TravelFlagsForTeam(ent);
			lreach->traveltime = aassettings.rs_teleport;
			lreach->next = areareachability[area1num];
			areareachability[area1num] = lreach;
			reach_teleport++;
		} 
		AAS_UnlinkFromAreas(areas);
	} 
} 
void AAS_Reachability_Elevator(void)
{
	int area1num, area2num, modelnum, i, j, k, l, n, p;
	float lip, height, speed;
	char model[MAX_EPAIRKEY], classname[MAX_EPAIRKEY];
	int ent;
	vec3_t mins, maxs, origin, angles = {0, 0, 0};
	vec3_t pos1, pos2, mids, platbottom, plattop;
	vec3_t bottomorg, toporg, start, end, dir;
	vec_t xvals[8], yvals[8], xvals_top[8], yvals_top[8];
	aas_lreachability_t *lreach;
	aas_trace_t trace;

#ifdef REACH_DEBUG
	Log_Write("AAS_Reachability_Elevator\r\n");
#endif 
	for (ent = AAS_NextBSPEntity(0); ent; ent = AAS_NextBSPEntity(ent))
	{
		if (!AAS_ValueForBSPEpairKey(ent, "classname", classname, MAX_EPAIRKEY)) continue;
		if (!strcmp(classname, "func_plat"))
		{
#ifdef REACH_DEBUG
			Log_Write("found func plat\r\n");
#endif 
			if (!AAS_ValueForBSPEpairKey(ent, "model", model, MAX_EPAIRKEY))
			{
				botimport.Print(PRT_ERROR, "func_plat without model\n");
				continue;
			} 
			modelnum = atoi(model+1);
			if (modelnum <= 0)
			{
				botimport.Print(PRT_ERROR, "func_plat with invalid model number\n");
				continue;
			} 
			AAS_BSPModelMinsMaxsOrigin(modelnum, angles, mins, maxs, origin);
			AAS_VectorForBSPEpairKey(ent, "origin", origin);
			VectorCopy(origin, pos1);
			VectorCopy(origin, pos2);
			AAS_FloatForBSPEpairKey(ent, "lip", &lip);
			if (!lip) lip = 8;
			AAS_FloatForBSPEpairKey(ent, "height", &height);
			if (!height) height = (maxs[2] - mins[2]) - lip;
			AAS_FloatForBSPEpairKey(ent, "speed", &speed);
			if (!speed) speed = 200;
			pos2[2] -= height;
			VectorAdd(mins, maxs, mids);
			VectorMA(pos2, 0.5, mids, platbottom);
			platbottom[2] = maxs[2] - (pos1[2] - pos2[2]) + 2;
			VectorAdd(mins, maxs, mids);
			VectorMA(pos2, 0.5, mids, plattop);
			plattop[2] = maxs[2] + 2;
			for (i = 0; i < 3; i++)
			{
				mins[i] -= 1;
				maxs[i] += 1;
			} 
			VectorAdd(mins, maxs, mids);
			VectorScale(mids, 0.5, mids);
			xvals[0] = mins[0]; xvals[1] = mids[0]; xvals[2] = maxs[0]; xvals[3] = mids[0];
			yvals[0] = mids[1]; yvals[1] = maxs[1]; yvals[2] = mids[1]; yvals[3] = mins[1];
			xvals[4] = mins[0]; xvals[5] = maxs[0]; xvals[6] = maxs[0]; xvals[7] = mins[0];
			yvals[4] = maxs[1]; yvals[5] = maxs[1]; yvals[6] = mins[1]; yvals[7] = mins[1];
			for (i = 0; i < 9; i++)
			{
				if (i < 8) 
				{
					bottomorg[0] = origin[0] + xvals[i];
					bottomorg[1] = origin[1] + yvals[i];
					bottomorg[2] = platbottom[2] + 16;
					area1num = AAS_PointAreaNum(bottomorg);
					for (k = 0; k < 16; k++)
					{
						if (area1num)
						{
							if (AAS_AreaGrounded(area1num) || AAS_AreaSwim(area1num)) break;
						} 
						bottomorg[2] += 4;
						area1num = AAS_PointAreaNum(bottomorg);
					} 
					if (k >= 16)
					{
						continue;
					} 
				} 
				else 
				{
					VectorCopy(plattop, bottomorg);
					bottomorg[2] += 24;
					area1num = AAS_PointAreaNum(bottomorg);
					if (!area1num) continue;
					VectorCopy(platbottom, bottomorg);
					bottomorg[2] += 24;
				} 
				for (n = 0; n < 3; n++)
				{
					for (k = 0; k < 3; k++)
					{
						mins[k] -= 4;
						maxs[k] += 4;
					} 
					xvals_top[0] = mins[0]; xvals_top[1] = mids[0]; xvals_top[2] = maxs[0]; xvals_top[3] = mids[0];
					yvals_top[0] = mids[1]; yvals_top[1] = maxs[1]; yvals_top[2] = mids[1]; yvals_top[3] = mins[1];
					xvals_top[4] = mins[0]; xvals_top[5] = maxs[0]; xvals_top[6] = maxs[0]; xvals_top[7] = mins[0];
					yvals_top[4] = maxs[1]; yvals_top[5] = maxs[1]; yvals_top[6] = mins[1]; yvals_top[7] = mins[1];
					for (j = 0; j < 8; j++)
					{
						toporg[0] = origin[0] + xvals_top[j];
						toporg[1] = origin[1] + yvals_top[j];
						toporg[2] = plattop[2] + 16;
						area2num = AAS_PointAreaNum(toporg);
						for (l = 0; l < 16; l++)
						{
							if (area2num)
							{
								if (AAS_AreaGrounded(area2num) || AAS_AreaSwim(area2num))
								{
									VectorCopy(plattop, start);
									start[2] += 32;
									VectorCopy(toporg, end);
									end[2] += 1;
									trace = AAS_TraceClientBBox(start, end, PRESENCE_CROUCH, -1);
									if (trace.fraction >= 1) break;
								} 
							} 
							toporg[2] += 4;
							area2num = AAS_PointAreaNum(toporg);
						} 
						if (l >= 16) continue;
						if (area2num == area1num) continue;
						if (!AAS_AreaGrounded(area2num)) continue;
						if (AAS_ReachabilityExists(area1num, area2num)) continue;
						VectorSubtract(bottomorg, platbottom, dir);
						VectorNormalize(dir);
						dir[0] = bottomorg[0] + 24 * dir[0];
						dir[1] = bottomorg[1] + 24 * dir[1];
						dir[2] = bottomorg[2];
						for (p = 0; p < 3; p++)
							if (dir[p] < origin[p] + mins[p] || dir[p] > origin[p] + maxs[p]) break;
						if (p >= 3) continue;
						lreach = AAS_AllocReachability();
						if (!lreach) continue;
						lreach->areanum = area2num;
						lreach->facenum = modelnum;
						lreach->edgenum = (int) height;
						VectorCopy(dir, lreach->start);
						VectorCopy(toporg, lreach->end);
						lreach->traveltype = TRAVEL_ELEVATOR;
						lreach->traveltype |= AAS_TravelFlagsForTeam(ent);
						lreach->traveltime = aassettings.rs_startelevator + height * 100 / speed;
						lreach->next = areareachability[area1num];
						areareachability[area1num] = lreach;
						n = 9999;
#ifdef REACH_DEBUG
						Log_Write("elevator reach from %d to %d\r\n", area1num, area2num);
#endif 
						reach_elevator++;
					} 
				} 
			} 
		} 
	} 
} 
aas_lreachability_t *AAS_FindFaceReachabilities(vec3_t *facepoints, int numpoints, aas_plane_t *plane, int towardsface)
{
	int i, j, k, l;
	int facenum, edgenum, bestfacenum;
	float *v1, *v2, *v3, *v4;
	float bestdist, speed, hordist, dist;
	vec3_t beststart = {0}, beststart2 = {0}, bestend = {0}, bestend2 = {0}, tmp, hordir, testpoint;
	aas_lreachability_t *lreach, *lreachabilities;
	aas_area_t *area;
	aas_face_t *face;
	aas_edge_t *edge;
	aas_plane_t *faceplane, *bestfaceplane;

	lreachabilities = NULL;
	bestfacenum = 0;
	bestfaceplane = NULL;
	for (i = 1; i < aasworld.numareas; i++)
	{
		area = &aasworld.areas[i];
		bestdist = 999999;
		for (j = 0; j < area->numfaces; j++)
		{
			facenum = aasworld.faceindex[area->firstface + j];
			face = &aasworld.faces[abs(facenum)];
			if (!(face->faceflags & FACE_GROUND)) continue;
			faceplane = &aasworld.planes[face->planenum];
			for (k = 0; k < face->numedges; k++)
			{
				edgenum = abs(aasworld.edgeindex[face->firstedge + k]);
				edge = &aasworld.edges[edgenum];
				v1 = aasworld.vertexes[edge->v[0]];
				v2 = aasworld.vertexes[edge->v[1]];
				for (l = 0; l < numpoints; l++)
				{
					v3 = facepoints[l];
					v4 = facepoints[(l+1) % numpoints];
					dist = AAS_ClosestEdgePoints(v1, v2, v3, v4, faceplane, plane,
													beststart, bestend,
													beststart2, bestend2, bestdist);
					if (dist < bestdist)
					{
						bestfacenum = facenum;
						bestfaceplane = faceplane;
						bestdist = dist;
					} 
				} 
			} 
		} 
		if (bestdist > 192) continue;
		VectorMiddle(beststart, beststart2, beststart);
		VectorMiddle(bestend, bestend2, bestend);
		if (!towardsface)
		{
			VectorCopy(beststart, tmp);
			VectorCopy(bestend, beststart);
			VectorCopy(tmp, bestend);
		} 
		VectorSubtract(bestend, beststart, hordir);
		hordir[2] = 0;
		hordist = VectorLength(hordir);
		if (hordist > 2 * AAS_MaxJumpDistance(aassettings.phys_jumpvel)) continue;
		if (bestend[2] - 32 > beststart[2]) continue;
		if (bestend[2] < beststart[2] - 128) continue;
		if (hordist > 32)
		{
			if (!AAS_HorizontalVelocityForJump(0, beststart, bestend, &speed)) continue;
		} 
		beststart[2] += 1;
		bestend[2] += 1;
		if (towardsface) VectorCopy(bestend, testpoint);
		else VectorCopy(beststart, testpoint);
		if (bestfaceplane != NULL)
			testpoint[2] = (bestfaceplane->dist - DotProduct(bestfaceplane->normal, testpoint)) / bestfaceplane->normal[2];
		else
			testpoint[2] = 0;
		if (!AAS_PointInsideFace(bestfacenum, testpoint, 0.1f))
		{
			if (bestend[2] - 16 > beststart[2]) continue;
		} 
		lreach = AAS_AllocReachability();
		if (!lreach) return lreachabilities;
		lreach->areanum = i;
		lreach->facenum = 0;
		lreach->edgenum = 0;
		VectorCopy(beststart, lreach->start);
		VectorCopy(bestend, lreach->end);
		lreach->traveltype = 0;
		lreach->traveltime = 0;
		lreach->next = lreachabilities;
		lreachabilities = lreach;
#ifndef BSPC
		if (towardsface) AAS_PermanentLine(lreach->start, lreach->end, 1);
		else AAS_PermanentLine(lreach->start, lreach->end, 2);
#endif
	} 
	return lreachabilities;
} 
int AAS_Reachability_Grapple(int area1num, int area2num)
{
	int face2num, i, j, areanum, numareas, areas[20];
	float mingrappleangle, z, hordist;
	bsp_trace_t bsptrace;
	aas_trace_t trace;
	aas_face_t *face2;
	aas_area_t *area1, *area2;
	aas_lreachability_t *lreach;
	vec3_t areastart = {0, 0, 0}, facecenter, start, end, dir, down = {0, 0, -1};
	vec_t *v;

	if (!AAS_AreaGrounded(area1num) && !AAS_AreaSwim(area1num)) return qfalse;
	if (!(AAS_AreaPresenceType(area1num) & PRESENCE_NORMAL)) return qfalse;
	if (AAS_AreaSwim(area1num)) return qfalse;
	area1 = &aasworld.areas[area1num];
	area2 = &aasworld.areas[area2num];
	if (area2->maxs[2] < area1->mins[2]) return qfalse;
	VectorCopy(aasworld.areas[area1num].center, start);
	if (!AAS_AreaSwim(area1num))
	{
		if (!AAS_PointAreaNum(start)) Log_Write("area %d center %f %f %f in solid?\r\n", area1num,
								start[0], start[1], start[2]);
		VectorCopy(start, end);
		end[2] -= 1000;
		trace = AAS_TraceClientBBox(start, end, PRESENCE_CROUCH, -1);
		if (trace.startsolid) return qfalse;
		VectorCopy(trace.endpos, areastart);
	} 
	else
	{
		if (!(AAS_PointContents(start) & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER))) return qfalse;
	} 
	for (i = 0; i < area2->numfaces; i++)
	{
		face2num = aasworld.faceindex[area2->firstface + i];
		face2 = &aasworld.faces[abs(face2num)];
		if (!(face2->faceflags & FACE_SOLID)) continue;
		v = aasworld.vertexes[aasworld.edges[abs(aasworld.edgeindex[face2->firstedge])].v[0]];
		VectorSubtract(v, areastart, dir);
		if (DotProduct(aasworld.planes[face2->planenum].normal, dir) > 0) continue;
		AAS_FaceCenter(face2num, facecenter);
		if (facecenter[2] < areastart[2] + 64) continue;
		if (DotProduct(aasworld.planes[face2->planenum].normal, down) < 0) continue;
		VectorSubtract(facecenter, areastart, dir);
		z = dir[2];
		dir[2] = 0;
		hordist = VectorLength(dir);
		if (!hordist) continue;
		if (hordist > 2000) continue;
		mingrappleangle = 15; 
		if (z / hordist < tan(2 * M_PI * mingrappleangle / 360)) continue;
		VectorCopy(facecenter, start);
		VectorMA(facecenter, -500, aasworld.planes[face2->planenum].normal, end);
		bsptrace = AAS_Trace(start, NULL, NULL, end, 0, CONTENTS_SOLID);
		if ((bsptrace.surface.flags & SURF_SKY) || (bsptrace.fraction * 500 > 32)) continue;
		VectorSubtract(facecenter, areastart, dir);
		VectorNormalize(dir);
		VectorMA(areastart, 4, dir, start);
		VectorCopy(bsptrace.endpos, end);
		trace = AAS_TraceClientBBox(start, end, PRESENCE_NORMAL, -1);
		VectorSubtract(trace.endpos, facecenter, dir);
		if (VectorLength(dir) > 24) continue;
		VectorCopy(trace.endpos, start);
		VectorCopy(trace.endpos, end);
		end[2] -= AAS_FallDamageDistance();
		trace = AAS_TraceClientBBox(start, end, PRESENCE_NORMAL, -1);
		if (trace.fraction >= 1) continue;
		areanum = AAS_PointAreaNum(trace.endpos);
		if (aasworld.areasettings[areanum].contents & (AREACONTENTS_SLIME|AREACONTENTS_LAVA))
		{
			continue;
		} 
		if (areanum == area1num) continue;
		if (AAS_ReachabilityExists(area1num, areanum)) continue;
		if (!AAS_AreaGrounded(areanum)) continue;
		numareas = AAS_TraceAreas(areastart, bsptrace.endpos, areas, NULL, 20);
		if (numareas >= 20) continue;
		for (j = 0; j < numareas; j++)
		{
			if (aasworld.areasettings[areas[j]].contents & AREACONTENTS_CLUSTERPORTAL) break;
		} 
		if (j < numareas) continue;
		lreach = AAS_AllocReachability();
		if (!lreach) return qfalse;
		lreach->areanum = areanum;
		lreach->facenum = face2num;
		lreach->edgenum = 0;
		VectorCopy(areastart, lreach->start);
		VectorCopy(bsptrace.endpos, lreach->end);
		lreach->traveltype = TRAVEL_GRAPPLEHOOK;
		VectorSubtract(lreach->end, lreach->start, dir);
		lreach->traveltime = aassettings.rs_startgrapple + VectorLength(dir) * 0.25;
		lreach->next = areareachability[area1num];
		areareachability[area1num] = lreach;
		reach_grapple++;
	} 
	return qfalse;
} 
void AAS_SetWeaponJumpAreaFlags(void)
{
	int ent, i;
	vec3_t mins = {-15, -15, -15}, maxs = {15, 15, 15};
	vec3_t origin;
	int areanum, weaponjumpareas, spawnflags;
	char classname[MAX_EPAIRKEY];

	weaponjumpareas = 0;
	for (ent = AAS_NextBSPEntity(0); ent; ent = AAS_NextBSPEntity(ent))
	{
		if (!AAS_ValueForBSPEpairKey(ent, "classname", classname, MAX_EPAIRKEY)) continue;
		if (
			!strcmp(classname, "item_armor_body") ||
			!strcmp(classname, "item_armor_combat") ||
			!strcmp(classname, "item_health_mega") ||
			!strcmp(classname, "weapon_grenadelauncher") ||
			!strcmp(classname, "weapon_rocketlauncher") ||
			!strcmp(classname, "weapon_lightning") ||
			!strcmp(classname, "weapon_plasmagun") ||
			!strcmp(classname, "weapon_railgun") ||
			!strcmp(classname, "weapon_bfg") ||
			!strcmp(classname, "item_quad") ||
			!strcmp(classname, "item_regen") ||
			!strcmp(classname, "item_invulnerability"))
		{
			if (AAS_VectorForBSPEpairKey(ent, "origin", origin))
			{
				spawnflags = 0;
				AAS_IntForBSPEpairKey(ent, "spawnflags", &spawnflags);
				if (!(spawnflags & 1))
				{
					if (!AAS_DropToFloor(origin, mins, maxs))
					{
						botimport.Print(PRT_MESSAGE, "%s in solid at (%1.1f %1.1f %1.1f)\n",
														classname, origin[0], origin[1], origin[2]);
					} 
				} 
				areanum = AAS_BestReachableArea(origin, mins, maxs, origin);
				aasworld.areasettings[areanum].areaflags |= AREA_WEAPONJUMP;
				weaponjumpareas++;
			} 
		} 
	} 
	for (i = 1; i < aasworld.numareas; i++)
	{
		if (aasworld.areasettings[i].contents & AREACONTENTS_JUMPPAD)
		{
			aasworld.areasettings[i].areaflags |= AREA_WEAPONJUMP;
			weaponjumpareas++;
		} 
	} 
	botimport.Print(PRT_MESSAGE, "%d weapon jump areas\n", weaponjumpareas);
} 
int AAS_Reachability_WeaponJump(int area1num, int area2num)
{
	int face2num, i, n, ret, visualize;
	float speed, zvel;
	aas_face_t *face2;
	aas_area_t *area1, *area2;
	aas_lreachability_t *lreach;
	vec3_t areastart, facecenter, start, end, dir, cmdmove;
	vec3_t velocity;
	aas_clientmove_t move;
	aas_trace_t trace;

	visualize = qfalse;
	if (!AAS_AreaGrounded(area1num) || AAS_AreaSwim(area1num)) return qfalse;
	if (!AAS_AreaGrounded(area2num)) return qfalse;
	if (!(aasworld.areasettings[area2num].areaflags & AREA_WEAPONJUMP)) return qfalse;
	area1 = &aasworld.areas[area1num];
	area2 = &aasworld.areas[area2num];
	if (area2->maxs[2] < area1->mins[2]) return qfalse;
	VectorCopy(aasworld.areas[area1num].center, start);
	if (!AAS_PointAreaNum(start)) Log_Write("area %d center %f %f %f in solid?\r\n", area1num,
							start[0], start[1], start[2]);
	VectorCopy(start, end);
	end[2] -= 1000;
	trace = AAS_TraceClientBBox(start, end, PRESENCE_CROUCH, -1);
	if (trace.startsolid) return qfalse;
	VectorCopy(trace.endpos, areastart);
	for (i = 0; i < area2->numfaces; i++)
	{
		face2num = aasworld.faceindex[area2->firstface + i];
		face2 = &aasworld.faces[abs(face2num)];
		if (!(face2->faceflags & FACE_GROUND)) continue;
		AAS_FaceCenter(face2num, facecenter);
		if (facecenter[2] < areastart[2] + 64) continue;
		for (n = 0; n < 1; n++)
		{
			if (n) zvel = AAS_BFGJumpZVelocity(areastart);
			else zvel = AAS_RocketJumpZVelocity(areastart);
			ret = AAS_HorizontalVelocityForJump(zvel, areastart, facecenter, &speed);
			if (ret && speed < 300)
			{
				VectorSubtract(facecenter, areastart, dir);
				dir[2] = 0;
				{
					VectorScale(dir, speed, cmdmove);
					VectorSet(velocity, 0, 0, zvel);
					AAS_PredictClientMovement(&move, -1, areastart, PRESENCE_NORMAL, qtrue,
												velocity, cmdmove, 30, 30, 0.1f,
												SE_ENTERWATER|SE_ENTERSLIME|
												SE_ENTERLAVA|SE_HITGROUNDDAMAGE|
												SE_TOUCHJUMPPAD|SE_HITGROUND|SE_HITGROUNDAREA, area2num, visualize);
					if (move.frames < 30 && 
							!(move.stopevent & (SE_ENTERSLIME|SE_ENTERLAVA|SE_HITGROUNDDAMAGE))
								&& (move.stopevent & (SE_HITGROUNDAREA|SE_TOUCHJUMPPAD)))
					{
						lreach = AAS_AllocReachability();
						if (!lreach) return qfalse;
						lreach->areanum = area2num;
						lreach->facenum = 0;
						lreach->edgenum = 0;
						VectorCopy(areastart, lreach->start);
						VectorCopy(facecenter, lreach->end);
						if (n)
						{
							lreach->traveltype = TRAVEL_BFGJUMP;
							lreach->traveltime = aassettings.rs_bfgjump;
						} 
						else
						{
							lreach->traveltype = TRAVEL_ROCKETJUMP;
							lreach->traveltime = aassettings.rs_rocketjump;
						} 
						lreach->next = areareachability[area1num];
						areareachability[area1num] = lreach;
						reach_rocketjump++;
						return qtrue;
					} 
				} 
			} 
		} 
	} 
	return qfalse;
} 
void AAS_Reachability_WalkOffLedge(int areanum)
{
	int i, j, k, l, m, n, p, areas[10], numareas;
	int face1num, face2num, face3num, edge1num, edge2num, edge3num;
	int otherareanum, gap, reachareanum, side;
	aas_area_t *area, *area2;
	aas_face_t *face1, *face2, *face3;
	aas_edge_t *edge;
	aas_plane_t *plane;
	vec_t *v1, *v2;
	vec3_t sharededgevec, mid, dir, testend;
	aas_lreachability_t *lreach;
	aas_trace_t trace;

	if (!AAS_AreaGrounded(areanum) || AAS_AreaSwim(areanum)) return;
	area = &aasworld.areas[areanum];
	for (i = 0; i < area->numfaces; i++)
	{
		face1num = aasworld.faceindex[area->firstface + i];
		face1 = &aasworld.faces[abs(face1num)];
		if (!(face1->faceflags & FACE_GROUND)) continue;
		for (k = 0; k < face1->numedges; k++)
		{
			edge1num = aasworld.edgeindex[face1->firstedge + k];
			for (j = 0; j < area->numfaces; j++)
			{
				face2num = aasworld.faceindex[area->firstface + j];
				face2 = &aasworld.faces[abs(face2num)];
				if (face2->faceflags & FACE_GROUND) continue;
				for (l = 0; l < face2->numedges; l++)
				{
					edge2num = aasworld.edgeindex[face2->firstedge + l];
					if (abs(edge1num) == abs(edge2num))
					{
						if (face2->frontarea == areanum) otherareanum = face2->backarea;
						else otherareanum = face2->frontarea;
						area2 = &aasworld.areas[otherareanum];
						if (aasworld.areasettings[otherareanum].areaflags & AREA_GROUNDED)
						{
							gap = qfalse;
							for (n = 0; n < area2->numfaces; n++)
							{
								face3num = aasworld.faceindex[area2->firstface + n];
								if (abs(face3num) == abs(face2num)) continue;
								face3 = &aasworld.faces[abs(face3num)];
								for (m = 0; m < face3->numedges; m++)
								{
									edge3num = aasworld.edgeindex[face3->firstedge + m];
									if (abs(edge3num) == abs(edge1num))
									{
										if (!(face3->faceflags & FACE_SOLID))
										{
											gap = qtrue;
											break;
										} 
										if (face3->faceflags & FACE_GROUND)
										{
											gap = qfalse;
											break;
										} 
										gap = qtrue;
										break;
									} 
								} 
								if (m < face3->numedges) break;
							} 
							if (!gap) break;
						} 
						edge = &aasworld.edges[abs(edge1num)];
						side = edge1num < 0;
						v1 = aasworld.vertexes[edge->v[side]];
						v2 = aasworld.vertexes[edge->v[!side]];
						plane = &aasworld.planes[face1->planenum];
						VectorSubtract(v2, v1, sharededgevec);
						CrossProduct(plane->normal, sharededgevec, dir);
						VectorNormalize(dir);
						VectorAdd(v1, v2, mid);
						VectorScale(mid, 0.5, mid);
						VectorMA(mid, 8, dir, mid);
						VectorCopy(mid, testend);
						testend[2] -= 1000;
						trace = AAS_TraceClientBBox(mid, testend, PRESENCE_CROUCH, -1);
						if (trace.startsolid)
						{
							break;
						} 
						reachareanum = AAS_PointAreaNum(trace.endpos);
						if (reachareanum == areanum)
						{
							break;
						} 
						if (AAS_ReachabilityExists(areanum, reachareanum))
						{
							break;
						} 
						if (!AAS_AreaGrounded(reachareanum) && !AAS_AreaSwim(reachareanum))
						{
							break;
						} 
						if (aasworld.areasettings[reachareanum].contents & (AREACONTENTS_SLIME
																						| AREACONTENTS_LAVA))
						{
							break;
						} 
						numareas = AAS_TraceAreas(mid, testend, areas, NULL, ARRAY_LEN(areas));
						for (p = 0; p < numareas; p++)
							if (AAS_AreaClusterPortal(areas[p]))
								break;
						if (p < numareas)
							break;
						if (aassettings.rs_maxfallheight && fabs(mid[2] - trace.endpos[2]) > aassettings.rs_maxfallheight)
							break;
						lreach = AAS_AllocReachability();
						if (!lreach) break;
						lreach->areanum = reachareanum;
						lreach->facenum = 0;
						lreach->edgenum = edge1num;
						VectorCopy(mid, lreach->start);
						VectorCopy(trace.endpos, lreach->end);
						lreach->traveltype = TRAVEL_WALKOFFLEDGE;
						lreach->traveltime = aassettings.rs_startwalkoffledge + fabs(mid[2] - trace.endpos[2]) * 50 / aassettings.phys_gravity;
						if (!AAS_AreaSwim(reachareanum) && !AAS_AreaJumpPad(reachareanum))
						{
							if (AAS_FallDelta(mid[2] - trace.endpos[2]) > aassettings.phys_falldelta5)
							{
								lreach->traveltime += aassettings.rs_falldamage5;
							} 
							else if (AAS_FallDelta(mid[2] - trace.endpos[2]) > aassettings.phys_falldelta10)
							{
								lreach->traveltime += aassettings.rs_falldamage10;
							} 
						} 
						lreach->next = areareachability[areanum];
						areareachability[areanum] = lreach;
						reach_walkoffledge++;
					} 
				} 
			} 
		} 
	} 
} 
void AAS_StoreReachability(void)
{
	int i;
	aas_areasettings_t *areasettings;
	aas_lreachability_t *lreach;
	aas_reachability_t *reach;

	if (aasworld.reachability) FreeMemory(aasworld.reachability);
	aasworld.reachability = (aas_reachability_t *) GetClearedMemory((numlreachabilities + 10) * sizeof(aas_reachability_t));
	aasworld.reachabilitysize = 1;
	for (i = 0; i < aasworld.numareas; i++)
	{
		areasettings = &aasworld.areasettings[i];
		areasettings->firstreachablearea = aasworld.reachabilitysize;
		areasettings->numreachableareas = 0;
		for (lreach = areareachability[i]; lreach; lreach = lreach->next)
		{
			reach = &aasworld.reachability[areasettings->firstreachablearea +
													areasettings->numreachableareas];
			reach->areanum = lreach->areanum;
			reach->facenum = lreach->facenum;
			reach->edgenum = lreach->edgenum;
			VectorCopy(lreach->start, reach->start);
			VectorCopy(lreach->end, reach->end);
			reach->traveltype = lreach->traveltype;
			reach->traveltime = lreach->traveltime;
			areasettings->numreachableareas++;
		} 
		aasworld.reachabilitysize += areasettings->numreachableareas;
	} 
} 
int AAS_ContinueInitReachability(float time)
{
	int i, j, todo, start_time;
	static float framereachability, reachability_delay;
	static int lastpercentage;

	if (!aasworld.loaded) return qfalse;
	if (aasworld.numreachabilityareas >= aasworld.numareas + 2) return qfalse;
	if (aasworld.numreachabilityareas == 1)
	{
		botimport.Print(PRT_MESSAGE, "calculating reachability...\n");
		lastpercentage = 0;
		framereachability = 2000;
		reachability_delay = 1000;
	} 
	todo = aasworld.numreachabilityareas + (int) framereachability;
	start_time = Sys_MilliSeconds();
	for (i = aasworld.numreachabilityareas; i < aasworld.numareas && i < todo; i++)
	{
		aasworld.numreachabilityareas++;
		if (aasworld.areasettings[i].contents & AREACONTENTS_JUMPPAD)
		{
			continue;
		} 
		for (j = 1; j < aasworld.numareas; j++)
		{
			if (i == j) continue;
			if (aasworld.areasettings[i].contents & (AREACONTENTS_TELEPORTER|AREACONTENTS_JUMPPAD))
			{
				if (!(aasworld.areasettings[j].contents & (AREACONTENTS_TELEPORTER|AREACONTENTS_JUMPPAD)))
				{
					continue;
				} 
			} 
			if (AAS_ReachabilityExists(i, j)) continue;
			if (AAS_Reachability_Swim(i, j)) continue;
			if (AAS_Reachability_EqualFloorHeight(i, j)) continue;
			if (AAS_Reachability_Step_Barrier_WaterJump_WalkOffLedge(i, j)) continue;
			if (AAS_Reachability_Ladder(i, j)) continue;
			if (AAS_Reachability_Jump(i, j)) continue;
		} 
		if (aasworld.areasettings[i].contents & (AREACONTENTS_TELEPORTER|AREACONTENTS_JUMPPAD))
		{
			continue;
		} 
		for (j = 1; j < aasworld.numareas; j++)
		{
			if (i == j) continue;
			if (AAS_ReachabilityExists(i, j)) continue;
			if (calcgrapplereach) AAS_Reachability_Grapple(i, j);
			AAS_Reachability_WeaponJump(i, j);
		} 
		if (Sys_MilliSeconds() - start_time > (int) reachability_delay) break;
		if (aasworld.numreachabilityareas * 1000 / aasworld.numareas > lastpercentage) break;
	} 
	if (aasworld.numreachabilityareas == aasworld.numareas)
	{
		botimport.Print(PRT_MESSAGE, "\r%6.1f%%", (float) 100.0);
		botimport.Print(PRT_MESSAGE, "\nplease wait while storing reachability...\n");
		aasworld.numreachabilityareas++;
	} 
	else if (aasworld.numreachabilityareas == aasworld.numareas + 1)
	{
		for (i = 1; i < aasworld.numareas; i++)
		{
			if (aasworld.areasettings[i].contents & AREACONTENTS_JUMPPAD)
			{
				continue;
			} 
			AAS_Reachability_WalkOffLedge(i);
		} 
		// FuncBobbing passes do not exist in the retail binary at all.
		AAS_Reachability_Teleport();
		AAS_Reachability_Elevator();
#ifdef DEBUG
		botimport.Print(PRT_MESSAGE, "%6d reach swim\n", reach_swim);
		botimport.Print(PRT_MESSAGE, "%6d reach equal floor\n", reach_equalfloor);
		botimport.Print(PRT_MESSAGE, "%6d reach step\n", reach_step);
		botimport.Print(PRT_MESSAGE, "%6d reach barrier\n", reach_barrier);
		botimport.Print(PRT_MESSAGE, "%6d reach waterjump\n", reach_waterjump);
		botimport.Print(PRT_MESSAGE, "%6d reach walkoffledge\n", reach_walkoffledge);
		botimport.Print(PRT_MESSAGE, "%6d reach jump\n", reach_jump);
		botimport.Print(PRT_MESSAGE, "%6d reach ladder\n", reach_ladder);
		botimport.Print(PRT_MESSAGE, "%6d reach walk\n", reach_walk);
		botimport.Print(PRT_MESSAGE, "%6d reach teleport\n", reach_teleport);
		botimport.Print(PRT_MESSAGE, "%6d reach funcbob\n", reach_funcbob);
		botimport.Print(PRT_MESSAGE, "%6d reach elevator\n", reach_elevator);
		botimport.Print(PRT_MESSAGE, "%6d reach grapple\n", reach_grapple);
		botimport.Print(PRT_MESSAGE, "%6d reach rocketjump\n", reach_rocketjump);
		botimport.Print(PRT_MESSAGE, "%6d reach jumppad\n", reach_jumppad);
#endif
		AAS_StoreReachability();
		numlreachabilities = 0;
		aasworld.numreachabilityareas++;
		botimport.Print(PRT_MESSAGE, "calculating clusters...\n");
	} 
	else
	{
		lastpercentage = aasworld.numreachabilityareas * 1000 / aasworld.numareas;
		botimport.Print(PRT_MESSAGE, "\r%6.1f%%", (float) lastpercentage / 10);
	} 
	return qtrue;
} 
void AAS_InitReachability(void)
{
	if (!aasworld.loaded) return;

	if (aasworld.reachabilitysize)
	{
#ifndef BSPC
		if (!((int)LibVarGetValue("forcereachability")))
		{
			aasworld.numreachabilityareas = aasworld.numareas + 2;
			return;
		} 
#else
		aasworld.numreachabilityareas = aasworld.numareas + 2;
		return;
#endif 
	} 
#ifndef BSPC
	calcgrapplereach = LibVarGetValue("grapplereach");
#endif
	aasworld.savefile = qtrue;
	aasworld.numreachabilityareas = 1;
	AAS_SetupReachabilityHeap();
	areareachability = (aas_lreachability_t **) GetClearedMemory(
									aasworld.numareas * sizeof(aas_lreachability_t *));
	AAS_SetWeaponJumpAreaFlags();
} 
