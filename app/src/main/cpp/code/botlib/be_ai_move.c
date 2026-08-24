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
#include "l_memory.h"
#include "l_libvar.h"
#include "l_utils.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_aas_funcs.h"
#include "be_interface.h"

#include "be_ea.h"
#include "be_ai_goal.h"
#include "be_ai_move.h"



typedef struct bot_movestate_s
{
	vec3_t origin;								
	vec3_t velocity;							
	vec3_t viewoffset;							
	int entitynum;								
	int client;									
	float thinktime;							
	int presencetype;							
	vec3_t viewangles;							
	int areanum;								
	int lastareanum;							
	int lastgoalareanum;						
	int lastreachnum;							
	vec3_t lastorigin;							
	int reachareanum;							
	int moveflags;								
	int jumpreach;								
	float grapplevisible_time;					
	float lastgrappledist;						
	float reachability_time;					
	int avoidreach[MAX_AVOIDREACH];				
	float avoidreachtimes[MAX_AVOIDREACH];		
	int avoidreachtries[MAX_AVOIDREACH];		
	bot_avoidspot_t avoidspots[MAX_AVOIDSPOTS];	
	int numavoidspots;
} bot_movestate_t;

#define AVOIDREACH
#define AVOIDREACH_TIME			6		
#define AVOIDREACH_TRIES		4
#define PREDICTIONTIME_JUMP	3		
#define PREDICTIONTIME_MOVE	2		
#define WEAPONINDEX_ROCKET_LAUNCHER		5
#define WEAPONINDEX_BFG					9

#define MODELTYPE_FUNC_PLAT		1
#define MODELTYPE_FUNC_BOB		2
#define MODELTYPE_FUNC_DOOR		3
#define MODELTYPE_FUNC_STATIC	4

libvar_t *sv_maxstep;
libvar_t *sv_maxbarrier;
libvar_t *sv_gravity;
libvar_t *weapindex_rocketlauncher;
libvar_t *weapindex_bfg10k;
libvar_t *weapindex_grapple;
libvar_t *entitytypemissile;
libvar_t *offhandgrapple;
libvar_t *cmd_grappleoff;
libvar_t *cmd_grappleon;
int modeltypes[MAX_MODELS];

bot_movestate_t *botmovestates[MAX_CLIENTS+1];

int BotAllocMoveState(void)
{
	int i;

	for (i = 1; i <= MAX_CLIENTS; i++)
	{
		if (!botmovestates[i])
		{
			botmovestates[i] = GetClearedMemory(sizeof(bot_movestate_t));
			return i;
		} 
	} 
	return 0;
} 
void BotFreeMoveState(int handle)
{
	if (handle <= 0 || handle > MAX_CLIENTS)
	{
		botimport.Print(PRT_FATAL, "move state handle %d out of range\n", handle);
		return;
	} 
	if (!botmovestates[handle])
	{
		botimport.Print(PRT_FATAL, "invalid move state %d\n", handle);
		return;
	} 
	FreeMemory(botmovestates[handle]);
	botmovestates[handle] = NULL;
} 
bot_movestate_t *BotMoveStateFromHandle(int handle)
{
	if (handle <= 0 || handle > MAX_CLIENTS)
	{
		botimport.Print(PRT_FATAL, "move state handle %d out of range\n", handle);
		return NULL;
	} 
	if (!botmovestates[handle])
	{
		botimport.Print(PRT_FATAL, "invalid move state %d\n", handle);
		return NULL;
	} 
	return botmovestates[handle];
} 
void BotInitMoveState(int handle, bot_initmove_t *initmove)
{
	bot_movestate_t *ms;

	ms = BotMoveStateFromHandle(handle);
	if (!ms) return;
	VectorCopy(initmove->origin, ms->origin);
	VectorCopy(initmove->velocity, ms->velocity);
	VectorCopy(initmove->viewoffset, ms->viewoffset);
	ms->entitynum = initmove->entitynum;
	ms->client = initmove->client;
	ms->thinktime = initmove->thinktime;
	ms->presencetype = initmove->presencetype;
	VectorCopy(initmove->viewangles, ms->viewangles);
	ms->moveflags &= ~MFL_ONGROUND;
	if (initmove->or_moveflags & MFL_ONGROUND) ms->moveflags |= MFL_ONGROUND;
	ms->moveflags &= ~MFL_TELEPORTED;	
	if (initmove->or_moveflags & MFL_TELEPORTED) ms->moveflags |= MFL_TELEPORTED;
	ms->moveflags &= ~MFL_WATERJUMP;
	if (initmove->or_moveflags & MFL_WATERJUMP) ms->moveflags |= MFL_WATERJUMP;
	ms->moveflags &= ~MFL_WALK;
	if (initmove->or_moveflags & MFL_WALK) ms->moveflags |= MFL_WALK;
	ms->moveflags &= ~MFL_GRAPPLEPULL;
	if (initmove->or_moveflags & MFL_GRAPPLEPULL) ms->moveflags |= MFL_GRAPPLEPULL;
} 
float AngleDiff(float ang1, float ang2)
{
	float diff;

	diff = ang1 - ang2;
	if (ang1 > ang2)
	{
		if (diff > 180.0) diff -= 360.0;
	} 
	else
	{
		if (diff < -180.0) diff += 360.0;
	} 
	return diff;
} 
int BotFuzzyPointReachabilityArea(vec3_t origin)
{
	int firstareanum, j, x, y, z;
	int areas[10], numareas, areanum, bestareanum;
	float dist, bestdist;
	vec3_t points[10], v, end;

	firstareanum = 0;
	areanum = AAS_PointAreaNum(origin);
	if (areanum)
	{
		firstareanum = areanum;
		if (AAS_AreaReachability(areanum)) return areanum;
	} 
	VectorCopy(origin, end);
	end[2] += 4;
	numareas = AAS_TraceAreas(origin, end, areas, points, 10);
	for (j = 0; j < numareas; j++)
	{
		if (AAS_AreaReachability(areas[j])) return areas[j];
	} 
	bestdist = 999999;
	bestareanum = 0;
	for (z = 1; z >= -1; z -= 1)
	{
		for (x = 1; x >= -1; x -= 1)
		{
			for (y = 1; y >= -1; y -= 1)
			{
				VectorCopy(origin, end);
				end[0] += x * 8;
				end[1] += y * 8;
				end[2] += z * 12;
				numareas = AAS_TraceAreas(origin, end, areas, points, 10);
				for (j = 0; j < numareas; j++)
				{
					if (AAS_AreaReachability(areas[j]))
					{
						VectorSubtract(points[j], origin, v);
						dist = VectorLength(v);
						if (dist < bestdist)
						{
							bestareanum = areas[j];
							bestdist = dist;
						} 
					} 
					if (!firstareanum) firstareanum = areas[j];
				} 
			} 
		} 
		if (bestareanum) return bestareanum;
	} 
	return firstareanum;
} 
int BotReachabilityArea(vec3_t origin, int client)
{
	int modelnum, modeltype, reachnum, areanum;
	aas_reachability_t reach;
	vec3_t org, end, mins, maxs, up = {0, 0, 1};
	bsp_trace_t bsptrace;
	aas_trace_t trace;

	AAS_PresenceTypeBoundingBox(PRESENCE_CROUCH, mins, maxs);
	VectorMA(origin, -3, up, end);
	bsptrace = AAS_Trace(origin, mins, maxs, end, client, CONTENTS_SOLID|CONTENTS_PLAYERCLIP);
	if (!bsptrace.startsolid && bsptrace.fraction < 1 && bsptrace.ent != ENTITYNUM_NONE)
	{
		if (bsptrace.ent == ENTITYNUM_WORLD)
		{
			return BotFuzzyPointReachabilityArea(origin);
		} 

		modelnum = AAS_EntityModelindex(bsptrace.ent);
		modeltype = modeltypes[modelnum];

		if (modeltype == MODELTYPE_FUNC_PLAT || modeltype == MODELTYPE_FUNC_BOB)
		{
			reachnum = AAS_NextModelReachability(0, modelnum);
			if (reachnum)
			{
				AAS_ReachabilityFromNum(reachnum, &reach);
				return reach.areanum;
			} 
		} 

		if (AAS_Swimming(origin))
		{
			return BotFuzzyPointReachabilityArea(origin);
		} 
		areanum = BotFuzzyPointReachabilityArea(origin);
		if (areanum && AAS_AreaReachability(areanum)) return areanum;
		VectorCopy(origin, org);
		VectorCopy(org, end);
		end[2] -= 800;
		trace = AAS_TraceClientBBox(org, end, PRESENCE_CROUCH, -1);
		if (!trace.startsolid)
		{
			VectorCopy(trace.endpos, org);
		} 
		return BotFuzzyPointReachabilityArea(org);
	} 
	return BotFuzzyPointReachabilityArea(origin);
} 
int BotOnMover(vec3_t origin, int entnum, aas_reachability_t *reach)
{
	int i, modelnum;
	vec3_t mins, maxs, modelorigin, org, end;
	vec3_t angles = {0, 0, 0};
	vec3_t boxmins = {-16, -16, -8}, boxmaxs = {16, 16, 8};
	bsp_trace_t trace;

	modelnum = reach->facenum & 0x0000FFFF;
	AAS_BSPModelMinsMaxsOrigin(modelnum, angles, mins, maxs, NULL);
	if (!AAS_OriginOfMoverWithModelNum(modelnum, modelorigin))
	{
		botimport.Print(PRT_MESSAGE, "no entity with model %d\n", modelnum);
		return qfalse;
	} 
	for (i = 0; i < 2; i++)
	{
		if (origin[i] > modelorigin[i] + maxs[i] + 16) return qfalse;
		if (origin[i] < modelorigin[i] + mins[i] - 16) return qfalse;
	} 
	VectorCopy(origin, org);
	org[2] += 24;
	VectorCopy(origin, end);
	end[2] -= 48;
	trace = AAS_Trace(org, boxmins, boxmaxs, end, entnum, CONTENTS_SOLID|CONTENTS_PLAYERCLIP);
	if (!trace.startsolid && !trace.allsolid)
	{
		if (trace.ent != ENTITYNUM_NONE && AAS_EntityModelNum(trace.ent) == modelnum)
		{
			return qtrue;
		} 
	} 
	return qfalse;
} 
int MoverDown(aas_reachability_t *reach)
{
	int modelnum;
	vec3_t mins, maxs, origin;
	vec3_t angles = {0, 0, 0};

	modelnum = reach->facenum & 0x0000FFFF;
	AAS_BSPModelMinsMaxsOrigin(modelnum, angles, mins, maxs, origin);
	if (!AAS_OriginOfMoverWithModelNum(modelnum, origin))
	{
		botimport.Print(PRT_MESSAGE, "no entity with model %d\n", modelnum);
		return qfalse;
	} 
	if (origin[2] + maxs[2] < reach->start[2]) return qtrue;
	return qfalse;
} 
void BotSetBrushModelTypes(void)
{
	int ent, modelnum;
	char classname[MAX_EPAIRKEY], model[MAX_EPAIRKEY];

	Com_Memset(modeltypes, 0, MAX_MODELS * sizeof(int));
	for (ent = AAS_NextBSPEntity(0); ent; ent = AAS_NextBSPEntity(ent))
	{
		if (!AAS_ValueForBSPEpairKey(ent, "classname", classname, MAX_EPAIRKEY)) continue;
		if (!AAS_ValueForBSPEpairKey(ent, "model", model, MAX_EPAIRKEY)) continue;
		if (model[0]) modelnum = atoi(model+1);
		else modelnum = 0;

		if (modelnum < 0 || modelnum >= MAX_MODELS)
		{
			botimport.Print(PRT_MESSAGE, "entity %s model number out of range\n", classname);
			continue;
		} 

		if (!Q_stricmp(classname, "func_bobbing"))
			modeltypes[modelnum] = MODELTYPE_FUNC_BOB;
		else if (!Q_stricmp(classname, "func_plat"))
			modeltypes[modelnum] = MODELTYPE_FUNC_PLAT;
		else if (!Q_stricmp(classname, "func_door"))
			modeltypes[modelnum] = MODELTYPE_FUNC_DOOR;
		else if (!Q_stricmp(classname, "func_static"))
			modeltypes[modelnum] = MODELTYPE_FUNC_STATIC;
	} 
} 
int BotOnTopOfEntity(bot_movestate_t *ms)
{
	vec3_t mins, maxs, end, up = {0, 0, 1};
	bsp_trace_t trace;

	AAS_PresenceTypeBoundingBox(ms->presencetype, mins, maxs);
	VectorMA(ms->origin, -3, up, end);
	trace = AAS_Trace(ms->origin, mins, maxs, end, ms->entitynum, CONTENTS_SOLID|CONTENTS_PLAYERCLIP);
	if (!trace.startsolid && (trace.ent != ENTITYNUM_WORLD && trace.ent != ENTITYNUM_NONE) )
	{
		return trace.ent;
	} 
	return -1;
} 
int BotValidTravel(vec3_t origin, aas_reachability_t *reach, int travelflags)
{
	if (AAS_TravelFlagForType(reach->traveltype) & ~travelflags) return qfalse;
	if (AAS_AreaContentsTravelFlags(reach->areanum) & ~travelflags) return qfalse;
	return qtrue;
} 
void BotAddToAvoidReach(bot_movestate_t *ms, int number, float avoidtime)
{
	int i;

	for (i = 0; i < MAX_AVOIDREACH; i++)
	{
		if (ms->avoidreach[i] == number)
		{
			if (ms->avoidreachtimes[i] > AAS_Time()) ms->avoidreachtries[i]++;
			else ms->avoidreachtries[i] = 1;
			ms->avoidreachtimes[i] = AAS_Time() + avoidtime;
			return;
		} 
	} 
	for (i = 0; i < MAX_AVOIDREACH; i++)
	{
		if (ms->avoidreachtimes[i] < AAS_Time())
		{
			ms->avoidreach[i] = number;
			ms->avoidreachtimes[i] = AAS_Time() + avoidtime;
			ms->avoidreachtries[i] = 1;
			return;
		} 
	} 
} 
float DistanceFromLineSquared(vec3_t p, vec3_t lp1, vec3_t lp2)
{
	vec3_t proj, dir;
	int j;

	AAS_ProjectPointOntoVector(p, lp1, lp2, proj);
	for (j = 0; j < 3; j++)
		if ((proj[j] > lp1[j] && proj[j] > lp2[j]) ||
			(proj[j] < lp1[j] && proj[j] < lp2[j]))
			break;
	if (j < 3) {
		if (fabs(proj[j] - lp1[j]) < fabs(proj[j] - lp2[j]))
			VectorSubtract(p, lp1, dir);
		else
			VectorSubtract(p, lp2, dir);
		return VectorLengthSquared(dir);
	}
	VectorSubtract(p, proj, dir);
	return VectorLengthSquared(dir);
} 
float VectorDistanceSquared(vec3_t p1, vec3_t p2)
{
	vec3_t dir;
	VectorSubtract(p2, p1, dir);
	return VectorLengthSquared(dir);
} 
int BotAvoidSpots(vec3_t origin, aas_reachability_t *reach, bot_avoidspot_t *avoidspots, int numavoidspots)
{
	int checkbetween, i, type;
	float squareddist, squaredradius;

	switch(reach->traveltype & TRAVELTYPE_MASK)
	{
		case TRAVEL_WALK: checkbetween = qtrue; break;
		case TRAVEL_CROUCH: checkbetween = qtrue; break;
		case TRAVEL_BARRIERJUMP: checkbetween = qtrue; break;
		case TRAVEL_LADDER: checkbetween = qtrue; break;
		case TRAVEL_WALKOFFLEDGE: checkbetween = qfalse; break;
		case TRAVEL_JUMP: checkbetween = qfalse; break;
		case TRAVEL_SWIM: checkbetween = qtrue; break;
		case TRAVEL_WATERJUMP: checkbetween = qtrue; break;
		case TRAVEL_TELEPORT: checkbetween = qfalse; break;
		case TRAVEL_ELEVATOR: checkbetween = qfalse; break;
		case TRAVEL_GRAPPLEHOOK: checkbetween = qfalse; break;
		case TRAVEL_ROCKETJUMP: checkbetween = qfalse; break;
		case TRAVEL_BFGJUMP: checkbetween = qfalse; break;
		case TRAVEL_JUMPPAD: checkbetween = qfalse; break;
		case TRAVEL_FUNCBOB: checkbetween = qfalse; break;
		default: checkbetween = qtrue; break;
	} 

	type = AVOID_CLEAR;
	for (i = 0; i < numavoidspots; i++)
	{
		squaredradius = Square(avoidspots[i].radius);
		squareddist = DistanceFromLineSquared(avoidspots[i].origin, origin, reach->start);
		if (squareddist < squaredradius &&
			VectorDistanceSquared(avoidspots[i].origin, origin) > squareddist)
		{
			type = avoidspots[i].type;
		} 
		else if (checkbetween) {
			squareddist = DistanceFromLineSquared(avoidspots[i].origin, reach->start, reach->end);
			if (squareddist < squaredradius &&
				VectorDistanceSquared(avoidspots[i].origin, reach->start) > squareddist)
			{
				type = avoidspots[i].type;
			} 
		} 
		else
		{
			VectorDistanceSquared(avoidspots[i].origin, reach->end);
			if (squareddist < squaredradius && 
				VectorDistanceSquared(avoidspots[i].origin, reach->start) > squareddist)
			{
				type = avoidspots[i].type;
			} 
		} 
		if (type == AVOID_ALWAYS)
			return type;
	} 
	return type;
} 
void BotAddAvoidSpot(int movestate, vec3_t origin, float radius, int type)
{
	bot_movestate_t *ms;

	ms = BotMoveStateFromHandle(movestate);
	if (!ms) return;
	if (type == AVOID_CLEAR)
	{
		ms->numavoidspots = 0;
		return;
	} 

	if (ms->numavoidspots >= MAX_AVOIDSPOTS)
		return;
	VectorCopy(origin, ms->avoidspots[ms->numavoidspots].origin);
	ms->avoidspots[ms->numavoidspots].radius = radius;
	ms->avoidspots[ms->numavoidspots].type = type;
	ms->numavoidspots++;
} 
int BotGetReachabilityToGoal(vec3_t origin, int areanum,
									  int lastgoalareanum, int lastareanum,
									  int *avoidreach, float *avoidreachtimes, int *avoidreachtries,
									  bot_goal_t *goal, int travelflags,
									  struct bot_avoidspot_s *avoidspots, int numavoidspots, int *flags)
{
	int i, t, besttime, bestreachnum, reachnum;
	aas_reachability_t reach;

	if (!areanum) return 0;
	if (AAS_AreaDoNotEnter(areanum) || AAS_AreaDoNotEnter(goal->areanum))
	{
		travelflags |= TFL_DONOTENTER;
	} 
	besttime = 0;
	bestreachnum = 0;
	for (reachnum = AAS_NextAreaReachability(areanum, 0); reachnum;
		reachnum = AAS_NextAreaReachability(areanum, reachnum))
	{
#ifdef AVOIDREACH
		for (i = 0; i < MAX_AVOIDREACH; i++)
		{
			if (avoidreach[i] == reachnum && avoidreachtimes[i] >= AAS_Time()) break;
		} 
		if (i != MAX_AVOIDREACH && avoidreachtries[i] > AVOIDREACH_TRIES)
		{
#ifdef DEBUG
			if (botDeveloper)
			{
				botimport.Print(PRT_MESSAGE, "avoiding reachability %d\n", avoidreach[i]);
			} 
#endif 
			continue;
		} 
#endif 
		AAS_ReachabilityFromNum(reachnum, &reach);
		//NOTE: do not go back to the previous area if the goal didn't change
		if (lastgoalareanum == goal->areanum && reach.areanum == lastareanum) continue;
		if (!BotValidTravel(origin, &reach, travelflags)) continue;
		t = AAS_AreaTravelTimeToGoalArea(reach.areanum, reach.end, goal->areanum, travelflags);
		if (!t) continue;
		if (BotAvoidSpots(origin, &reach, avoidspots, numavoidspots)) {
			if (flags) {
				*flags |= MOVERESULT_BLOCKEDBYAVOIDSPOT;
			}
			continue;
		}
		t += reach.traveltime;
		if (!besttime || t < besttime)
		{
			besttime = t;
			bestreachnum = reachnum;
		} 
	} 
	return bestreachnum;
} 
int BotAddToTarget(vec3_t start, vec3_t end, float maxdist, float *dist, vec3_t target)
{
	vec3_t dir;
	float curdist;

	VectorSubtract(end, start, dir);
	curdist = VectorNormalize(dir);
	if (*dist + curdist < maxdist)
	{
		VectorCopy(end, target);
		*dist += curdist;
		return qfalse;
	} 
	else
	{
		VectorMA(start, maxdist - *dist, dir, target);
		*dist = maxdist;
		return qtrue;
	} 
} 

int BotMovementViewTarget(int movestate, bot_goal_t *goal, int travelflags, float lookahead, vec3_t target)
{
	aas_reachability_t reach;
	int reachnum, lastareanum;
	bot_movestate_t *ms;
	vec3_t end;
	float dist;

	ms = BotMoveStateFromHandle(movestate);
	if (!ms) return qfalse;
	if (!ms->lastreachnum || !goal) return qfalse;

	reachnum = ms->lastreachnum;
	VectorCopy(ms->origin, end);
	lastareanum = ms->lastareanum;
	dist = 0;
	while(reachnum && dist < lookahead)
	{
		AAS_ReachabilityFromNum(reachnum, &reach);
		if (BotAddToTarget(end, reach.start, lookahead, &dist, target)) return qtrue;
		if ((reach.traveltype & TRAVELTYPE_MASK) == TRAVEL_TELEPORT) return qtrue;
		if ((reach.traveltype & TRAVELTYPE_MASK) == TRAVEL_ROCKETJUMP) return qtrue;
		if ((reach.traveltype & TRAVELTYPE_MASK) == TRAVEL_BFGJUMP) return qtrue;
		if ((reach.traveltype & TRAVELTYPE_MASK) != TRAVEL_JUMPPAD &&
			(reach.traveltype & TRAVELTYPE_MASK) != TRAVEL_ELEVATOR &&
			(reach.traveltype & TRAVELTYPE_MASK) != TRAVEL_FUNCBOB)
		{
			if (BotAddToTarget(reach.start, reach.end, lookahead, &dist, target)) return qtrue;
		} 
		reachnum = BotGetReachabilityToGoal(reach.end, reach.areanum,
						ms->lastgoalareanum, lastareanum,
							ms->avoidreach, ms->avoidreachtimes, ms->avoidreachtries,
									goal, travelflags, NULL, 0, NULL);
		VectorCopy(reach.end, end);
		lastareanum = reach.areanum;
		if (lastareanum == goal->areanum)
		{
			BotAddToTarget(reach.end, goal->origin, lookahead, &dist, target);
			return qtrue;
		} 
	} 
	return qfalse;
} 
int BotVisible(int ent, vec3_t eye, vec3_t target)
{
	bsp_trace_t trace;

	trace = AAS_Trace(eye, NULL, NULL, target, ent, CONTENTS_SOLID|CONTENTS_PLAYERCLIP);
	if (trace.fraction >= 1) return qtrue;
	return qfalse;
} 
int BotPredictVisiblePosition(vec3_t origin, int areanum, bot_goal_t *goal, int travelflags, vec3_t target)
{
	aas_reachability_t reach;
	int reachnum, lastgoalareanum, lastareanum, i;
	int avoidreach[MAX_AVOIDREACH];
	float avoidreachtimes[MAX_AVOIDREACH];
	int avoidreachtries[MAX_AVOIDREACH];
	vec3_t end;

	if (!goal) return qfalse;
	if (!areanum) return qfalse;
	if (!goal->areanum) return qfalse;

	Com_Memset(avoidreach, 0, MAX_AVOIDREACH * sizeof(int));
	lastgoalareanum = goal->areanum;
	lastareanum = areanum;
	VectorCopy(origin, end);
	for (i = 0; i < 20 && (areanum != goal->areanum); i++)
	{
		reachnum = BotGetReachabilityToGoal(end, areanum,
						lastgoalareanum, lastareanum,
							avoidreach, avoidreachtimes, avoidreachtries,
									goal, travelflags, NULL, 0, NULL);
		if (!reachnum) return qfalse;
		AAS_ReachabilityFromNum(reachnum, &reach);
		if (BotVisible(goal->entitynum, goal->origin, reach.start))
		{
			VectorCopy(reach.start, target);
			return qtrue;
		} 
		if (BotVisible(goal->entitynum, goal->origin, reach.end))
		{
			VectorCopy(reach.end, target);
			return qtrue;
		} 
		if (reach.areanum == goal->areanum)
		{
			VectorCopy(reach.end, target);
			return qtrue;
		} 
		lastareanum = areanum;
		areanum = reach.areanum;
		VectorCopy(reach.end, end);
	} 
	return qfalse;
} 
void MoverBottomCenter(aas_reachability_t *reach, vec3_t bottomcenter)
{
	int modelnum;
	vec3_t mins, maxs, origin, mids;
	vec3_t angles = {0, 0, 0};

	modelnum = reach->facenum & 0x0000FFFF;
	AAS_BSPModelMinsMaxsOrigin(modelnum, angles, mins, maxs, origin);
	if (!AAS_OriginOfMoverWithModelNum(modelnum, origin))
	{
		botimport.Print(PRT_MESSAGE, "no entity with model %d\n", modelnum);
	} 
	VectorAdd(mins, maxs, mids);
	VectorMA(origin, 0.5, mids, bottomcenter);
	bottomcenter[2] = reach->start[2];
} 
float BotGapDistance(vec3_t origin, vec3_t hordir, int entnum)
{
	int dist;
	float startz;
	vec3_t start, end;
	aas_trace_t trace;

	{
		VectorCopy(origin, start);
		VectorCopy(origin, end);
		end[2] -= 60;
		trace = AAS_TraceClientBBox(start, end, PRESENCE_CROUCH, entnum);
		if (trace.fraction >= 1) return 1;
		startz = trace.endpos[2] + 1;
	}
	for (dist = 8; dist <= 100; dist += 8)
	{
		VectorMA(origin, dist, hordir, start);
		start[2] = startz + 24;
		VectorCopy(start, end);
		end[2] -= 48 + sv_maxbarrier->value;
		trace = AAS_TraceClientBBox(start, end, PRESENCE_CROUCH, entnum);
		if (!trace.startsolid)
		{
			if (trace.endpos[2] < startz - sv_maxstep->value - 8)
			{
				VectorCopy(trace.endpos, end);
				end[2] -= 20;
				if (AAS_PointContents(end) & CONTENTS_WATER) break;
				return dist;
			} 
			startz = trace.endpos[2];
		} 
	} 
	return 0;
} 
int BotCheckBarrierJump(bot_movestate_t *ms, vec3_t dir, float speed)
{
	vec3_t start, hordir, end;
	aas_trace_t trace;

	VectorCopy(ms->origin, end);
	end[2] += sv_maxbarrier->value;
	trace = AAS_TraceClientBBox(ms->origin, end, PRESENCE_NORMAL, ms->entitynum);
	if (trace.startsolid) return qfalse;
	if (trace.endpos[2] - ms->origin[2] < sv_maxstep->value) return qfalse;
	hordir[0] = dir[0];
	hordir[1] = dir[1];
	hordir[2] = 0;
	VectorNormalize(hordir);
	VectorMA(ms->origin, ms->thinktime * speed * 0.5, hordir, end);
	VectorCopy(trace.endpos, start);
	end[2] = trace.endpos[2];
	trace = AAS_TraceClientBBox(start, end, PRESENCE_NORMAL, ms->entitynum);
	if (trace.startsolid) return qfalse;
	VectorCopy(trace.endpos, start);
	VectorCopy(trace.endpos, end);
	end[2] = ms->origin[2];
	trace = AAS_TraceClientBBox(start, end, PRESENCE_NORMAL, ms->entitynum);
	if (trace.startsolid) return qfalse;
	if (trace.fraction >= 1.0) return qfalse;
	if (trace.endpos[2] - ms->origin[2] < sv_maxstep->value) return qfalse;
	EA_Jump(ms->client);
	EA_Move(ms->client, hordir, speed);
	ms->moveflags |= MFL_BARRIERJUMP;
	return qtrue;
} 
int BotSwimInDirection(bot_movestate_t *ms, vec3_t dir, float speed, int type)
{
	vec3_t normdir;

	VectorCopy(dir, normdir);
	VectorNormalize(normdir);
	EA_Move(ms->client, normdir, speed);
	return qtrue;
} 
int BotWalkInDirection(bot_movestate_t *ms, vec3_t dir, float speed, int type)
{
	vec3_t hordir, cmdmove, velocity, tmpdir, origin;
	int presencetype, maxframes, cmdframes, stopevent;
	aas_clientmove_t move;
	float dist;

	if (AAS_OnGround(ms->origin, ms->presencetype, ms->entitynum)) ms->moveflags |= MFL_ONGROUND;
	if (ms->moveflags & MFL_ONGROUND)
	{
		if (BotCheckBarrierJump(ms, dir, speed)) return qtrue;
		ms->moveflags &= ~MFL_BARRIERJUMP;
		if ((type & MOVE_CROUCH) && !(type & MOVE_JUMP)) presencetype = PRESENCE_CROUCH;
		else presencetype = PRESENCE_NORMAL;
		hordir[0] = dir[0];
		hordir[1] = dir[1];
		hordir[2] = 0;
		VectorNormalize(hordir);
		if (!(type & MOVE_JUMP))
		{
			if (BotGapDistance(ms->origin, hordir, ms->entitynum) > 0) type |= MOVE_JUMP;
		} 
		VectorScale(hordir, speed, cmdmove);
		VectorCopy(ms->velocity, velocity);
		if (type & MOVE_JUMP)
		{
			cmdmove[2] = 400;
			maxframes = PREDICTIONTIME_JUMP / 0.1;
			cmdframes = 1;
			stopevent = SE_HITGROUND|SE_HITGROUNDDAMAGE|
						SE_ENTERWATER|SE_ENTERSLIME|SE_ENTERLAVA;
		} 
		else
		{
			maxframes = 2;
			cmdframes = 2;
			stopevent = SE_HITGROUNDDAMAGE|
						SE_ENTERWATER|SE_ENTERSLIME|SE_ENTERLAVA;
		} 
		VectorCopy(ms->origin, origin);
		origin[2] += 0.5;
		AAS_PredictClientMovement(&move, ms->entitynum, origin, presencetype, qtrue,
									velocity, cmdmove, cmdframes, maxframes, 0.1f,
									stopevent, 0, qfalse);
		if (move.frames >= maxframes && (type & MOVE_JUMP))
		{
			return qfalse;
		} 
		if (move.stopevent & (SE_ENTERSLIME|SE_ENTERLAVA|SE_HITGROUNDDAMAGE))
		{
			return qfalse;
		} 
		if (move.stopevent & SE_HITGROUND)
		{
			VectorNormalize2(move.velocity, tmpdir);
			dist = BotGapDistance(move.endpos, tmpdir, ms->entitynum);
			if (dist > 0) return qfalse;
			dist = BotGapDistance(move.endpos, hordir, ms->entitynum);
			if (dist > 0) return qfalse;
		} 
		tmpdir[0] = move.endpos[0] - ms->origin[0];
		tmpdir[1] = move.endpos[1] - ms->origin[1];
		tmpdir[2] = 0;
		if (VectorLength(tmpdir) < speed * ms->thinktime * 0.5) return qfalse;
		if (type & MOVE_JUMP) EA_Jump(ms->client);
		if (type & MOVE_CROUCH) EA_Crouch(ms->client);
		EA_Move(ms->client, hordir, speed);
		return qtrue;
	} 
	else
	{
		if (ms->moveflags & MFL_BARRIERJUMP)
		{
			if (ms->velocity[2] < 50)
			{
				EA_Move(ms->client, dir, speed);
			} 
		} 
		return qtrue;
	} 
} 
int BotMoveInDirection(int movestate, vec3_t dir, float speed, int type)
{
	bot_movestate_t *ms;

	ms = BotMoveStateFromHandle(movestate);
	if (!ms) return qfalse;
	if (AAS_Swimming(ms->origin))
	{
		return BotSwimInDirection(ms, dir, speed, type);
	} 
	else
	{
		return BotWalkInDirection(ms, dir, speed, type);
	} 
} 
int Intersection(vec2_t p1, vec2_t p2, vec2_t p3, vec2_t p4, vec2_t out)
{
   float x1, dx1, dy1, x2, dx2, dy2, d;

   dx1 = p2[0] - p1[0];
   dy1 = p2[1] - p1[1];
   dx2 = p4[0] - p3[0];
   dy2 = p4[1] - p3[1];

   d = dy1 * dx2 - dx1 * dy2;
   if (d != 0)
   {
      x1 = p1[1] * dx1 - p1[0] * dy1;
      x2 = p3[1] * dx2 - p3[0] * dy2;
      out[0] = (int) ((dx1 * x2 - dx2 * x1) / d);
      out[1] = (int) ((dy1 * x2 - dy2 * x1) / d);
		return qtrue;
   } 
   else
   {
      return qfalse;
   } 
} 
void BotCheckBlocked(bot_movestate_t *ms, vec3_t dir, int checkbottom, bot_moveresult_t *result)
{
	vec3_t mins, maxs, end, up = {0, 0, 1};
	bsp_trace_t trace;

	AAS_PresenceTypeBoundingBox(ms->presencetype, mins, maxs);
	if (fabs(DotProduct(dir, up)) < 0.7)
	{
		mins[2] += sv_maxstep->value; 
		maxs[2] -= 10; 
	} 
	VectorMA(ms->origin, 3, dir, end);
	trace = AAS_Trace(ms->origin, mins, maxs, end, ms->entitynum, CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_BODY);
	if (!trace.startsolid && (trace.ent != ENTITYNUM_WORLD && trace.ent != ENTITYNUM_NONE) )
	{
		result->blocked = qtrue;
		result->blockentity = trace.ent;
#ifdef DEBUG
#endif 
	} 
	else if (checkbottom && !AAS_AreaReachability(ms->areanum))
	{
		AAS_PresenceTypeBoundingBox(ms->presencetype, mins, maxs);
		VectorMA(ms->origin, -3, up, end);
		trace = AAS_Trace(ms->origin, mins, maxs, end, ms->entitynum, CONTENTS_SOLID|CONTENTS_PLAYERCLIP);
		if (!trace.startsolid && (trace.ent != ENTITYNUM_WORLD && trace.ent != ENTITYNUM_NONE) )
		{
			result->blocked = qtrue;
			result->blockentity = trace.ent;
			result->flags |= MOVERESULT_ONTOPOFOBSTACLE;
#ifdef DEBUG
#endif 
		} 
	} 
} 
bot_moveresult_t BotTravel_Walk(bot_movestate_t *ms, aas_reachability_t *reach)
{
	float dist, speed;
	vec3_t hordir;
	bot_moveresult_t_cleared( result );

	hordir[0] = reach->start[0] - ms->origin[0];
	hordir[1] = reach->start[1] - ms->origin[1];
	hordir[2] = 0;
	dist = VectorNormalize(hordir);
	BotCheckBlocked(ms, hordir, qtrue, &result);
	if (dist < 10)
	{
		hordir[0] = reach->end[0] - ms->origin[0];
		hordir[1] = reach->end[1] - ms->origin[1];
		hordir[2] = 0;
		dist = VectorNormalize(hordir);
	} 
	if (!(AAS_AreaPresenceType(reach->areanum) & PRESENCE_NORMAL))
	{
		if (dist < 20) EA_Crouch(ms->client);
	} 
	dist = BotGapDistance(ms->origin, hordir, ms->entitynum);
	if (ms->moveflags & MFL_WALK)
	{
		if (dist > 0) speed = 200 - (180 - 1 * dist);
		else speed = 200;
		EA_Walk(ms->client);
	} 
	else
	{
		if (dist > 0) speed = 400 - (360 - 2 * dist);
		else speed = 400;
	} 
	EA_Move(ms->client, hordir, speed);
	VectorCopy(hordir, result.movedir);
	return result;
} 
bot_moveresult_t BotFinishTravel_Walk(bot_movestate_t *ms, aas_reachability_t *reach)
{
	vec3_t hordir;
	float dist, speed;
	bot_moveresult_t_cleared( result );
	hordir[0] = reach->end[0] - ms->origin[0];
	hordir[1] = reach->end[1] - ms->origin[1];
	hordir[2] = 0;
	dist = VectorNormalize(hordir);
	if (dist > 100) dist = 100;
	speed = 400 - (400 - 3 * dist);
	EA_Move(ms->client, hordir, speed);
	VectorCopy(hordir, result.movedir);
	return result;
} 
bot_moveresult_t BotTravel_Crouch(bot_movestate_t *ms, aas_reachability_t *reach)
{
	float speed;
	vec3_t hordir;
	bot_moveresult_t_cleared( result );

	speed = 400;
	hordir[0] = reach->end[0] - ms->origin[0];
	hordir[1] = reach->end[1] - ms->origin[1];
	hordir[2] = 0;
	VectorNormalize(hordir);
	BotCheckBlocked(ms, hordir, qtrue, &result);
	EA_Crouch(ms->client);
	EA_Move(ms->client, hordir, speed);
	VectorCopy(hordir, result.movedir);
	return result;
} 
bot_moveresult_t BotTravel_BarrierJump(bot_movestate_t *ms, aas_reachability_t *reach)
{
	float dist, speed;
	vec3_t hordir;
	bot_moveresult_t_cleared( result );

	hordir[0] = reach->start[0] - ms->origin[0];
	hordir[1] = reach->start[1] - ms->origin[1];
	hordir[2] = 0;
	dist = VectorNormalize(hordir);
	BotCheckBlocked(ms, hordir, qtrue, &result);
	if (dist < 9)
	{
		EA_Jump(ms->client);
	} 
	else
	{
		if (dist > 60) dist = 60;
		speed = 360 - (360 - 6 * dist);
		EA_Move(ms->client, hordir, speed);
	} 
	VectorCopy(hordir, result.movedir);
	return result;
} 
bot_moveresult_t BotFinishTravel_BarrierJump(bot_movestate_t *ms, aas_reachability_t *reach)
{
	vec3_t hordir;
	bot_moveresult_t_cleared( result );

	if (ms->velocity[2] < 250)
	{
		hordir[0] = reach->end[0] - ms->origin[0];
		hordir[1] = reach->end[1] - ms->origin[1];
		hordir[2] = 0;
		BotCheckBlocked(ms, hordir, qtrue, &result);
		EA_Move(ms->client, hordir, 400);
		VectorCopy(hordir, result.movedir);
	} 
	return result;
} 
bot_moveresult_t BotTravel_Swim(bot_movestate_t *ms, aas_reachability_t *reach)
{
	vec3_t dir;
	bot_moveresult_t_cleared( result );

	VectorSubtract(reach->start, ms->origin, dir);
	VectorNormalize(dir);
	BotCheckBlocked(ms, dir, qtrue, &result);
	EA_Move(ms->client, dir, 400);
	VectorCopy(dir, result.movedir);
	Vector2Angles(dir, result.ideal_viewangles);
	result.flags |= MOVERESULT_SWIMVIEW;
	return result;
} 
bot_moveresult_t BotTravel_WaterJump(bot_movestate_t *ms, aas_reachability_t *reach)
{
	vec3_t dir, hordir;
	float dist;
	bot_moveresult_t_cleared( result );

	VectorSubtract(reach->end, ms->origin, dir);
	VectorCopy(dir, hordir);
	hordir[2] = 0;
	dir[2] += 15 + crandom() * 40;
	VectorNormalize(dir);
	dist = VectorNormalize(hordir);
	EA_MoveForward(ms->client);
	if (dist < 40) EA_MoveUp(ms->client);
	Vector2Angles(dir, result.ideal_viewangles);
	result.flags |= MOVERESULT_MOVEMENTVIEW;
	VectorCopy(dir, result.movedir);
	return result;
} 
bot_moveresult_t BotFinishTravel_WaterJump(bot_movestate_t *ms, aas_reachability_t *reach)
{
	vec3_t dir, pnt;
	bot_moveresult_t_cleared( result );

	if (ms->moveflags & MFL_WATERJUMP) return result;
	VectorCopy(ms->origin, pnt);
	pnt[2] -= 32;	
	if (!(AAS_PointContents(pnt) & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER))) return result;
	VectorSubtract(reach->end, ms->origin, dir);
	dir[0] += crandom() * 10;
	dir[1] += crandom() * 10;
	dir[2] += 70 + crandom() * 10;
	EA_Move(ms->client, dir, 400);
	Vector2Angles(dir, result.ideal_viewangles);
	result.flags |= MOVERESULT_MOVEMENTVIEW;
	VectorCopy(dir, result.movedir);
	return result;
} 
bot_moveresult_t BotTravel_WalkOffLedge(bot_movestate_t *ms, aas_reachability_t *reach)
{
	vec3_t hordir, dir;
	float dist, speed, reachhordist;
	bot_moveresult_t_cleared( result );

	VectorSubtract(reach->start, ms->origin, dir);
	VectorNormalize(dir);
	BotCheckBlocked(ms, dir, qtrue, &result);
	VectorSubtract(reach->end, reach->start, dir);
	dir[2] = 0;
	reachhordist = VectorLength(dir);
	hordir[0] = reach->start[0] - ms->origin[0];
	hordir[1] = reach->start[1] - ms->origin[1];
	hordir[2] = 0;
	dist = VectorNormalize(hordir);
	if (dist < 48)
	{
		hordir[0] = reach->end[0] - ms->origin[0];
		hordir[1] = reach->end[1] - ms->origin[1];
		hordir[2] = 0;
		VectorNormalize(hordir);
		if (reachhordist < 20)
		{
			speed = 100;
		} 
		else if (!AAS_HorizontalVelocityForJump(0, reach->start, reach->end, &speed))
		{
			speed = 400;
		} 
	} 
	else
	{
		if (reachhordist < 20)
		{
			if (dist > 64) dist = 64;
			speed = 400 - (256 - 4 * dist);
		} 
		else
		{
			speed = 400;
		} 
	} 
	BotCheckBlocked(ms, hordir, qtrue, &result);
	EA_Move(ms->client, hordir, speed);
	VectorCopy(hordir, result.movedir);
	return result;
} 
int BotAirControl(vec3_t origin, vec3_t velocity, vec3_t goal, vec3_t dir, float *speed)
{
	vec3_t org, vel;
	float dist;
	int i;

	VectorCopy(origin, org);
	VectorScale(velocity, 0.1, vel);
	for (i = 0; i < 50; i++)
	{
		vel[2] -= sv_gravity->value * 0.01;
		if (vel[2] < 0 && org[2] + vel[2] < goal[2])
		{
			VectorScale(vel, (goal[2] - org[2]) / vel[2], vel);
			VectorAdd(org, vel, org);
			VectorSubtract(goal, org, dir);
			dist = VectorNormalize(dir);
			if (dist > 32) dist = 32;
			*speed = 400 - (400 - 13 * dist);
			return qtrue;
		} 
		else
		{
			VectorAdd(org, vel, org);
		} 
	} 
	VectorSet(dir, 0, 0, 0);
	*speed = 400;
	return qfalse;
} 
bot_moveresult_t BotFinishTravel_WalkOffLedge(bot_movestate_t *ms, aas_reachability_t *reach)
{
	vec3_t dir, hordir, end, v;
	float dist, speed;
	bot_moveresult_t_cleared( result );

	VectorSubtract(reach->end, ms->origin, dir);
	BotCheckBlocked(ms, dir, qtrue, &result);
	VectorSubtract(reach->end, ms->origin, v);
	v[2] = 0;
	dist = VectorNormalize(v);
	if (dist > 16) VectorMA(reach->end, 16, v, end);
	else VectorCopy(reach->end, end);
	if (!BotAirControl(ms->origin, ms->velocity, end, hordir, &speed))
	{
		VectorCopy(dir, hordir);
		hordir[2] = 0;
		speed = 400;
	} 
	EA_Move(ms->client, hordir, speed);
	VectorCopy(hordir, result.movedir);
	return result;
} 
bot_moveresult_t BotTravel_Jump(bot_movestate_t *ms, aas_reachability_t *reach)
{
	vec3_t hordir, dir1, dir2, start, end, runstart;
	int gapdist;
	float dist1, dist2, speed;
	bot_moveresult_t_cleared( result );

	AAS_JumpReachRunStart(reach, runstart);
	hordir[0] = runstart[0] - reach->start[0];
	hordir[1] = runstart[1] - reach->start[1];
	hordir[2] = 0;
	VectorNormalize(hordir);
	VectorCopy(reach->start, start);
	start[2] += 1;
	VectorMA(reach->start, 80, hordir, runstart);
	for (gapdist = 0; gapdist < 80; gapdist += 10)
	{
		VectorMA(start, gapdist+10, hordir, end);
		end[2] += 1;
		if (AAS_PointAreaNum(end) != ms->reachareanum) break;
	} 
	if (gapdist < 80) VectorMA(reach->start, gapdist, hordir, runstart);
	VectorSubtract(ms->origin, reach->start, dir1);
	dir1[2] = 0;
	dist1 = VectorNormalize(dir1);
	VectorSubtract(ms->origin, runstart, dir2);
	dir2[2] = 0;
	dist2 = VectorNormalize(dir2);
	if (DotProduct(dir1, dir2) < -0.8 || dist2 < 5)
	{
		hordir[0] = reach->end[0] - ms->origin[0];
		hordir[1] = reach->end[1] - ms->origin[1];
		hordir[2] = 0;
		VectorNormalize(hordir);
		if (dist1 < 24) EA_Jump(ms->client);
		else if (dist1 < 32) EA_DelayedJump(ms->client);
		EA_Move(ms->client, hordir, 600);
		ms->jumpreach = ms->lastreachnum;
	} 
	else
	{
		hordir[0] = runstart[0] - ms->origin[0];
		hordir[1] = runstart[1] - ms->origin[1];
		hordir[2] = 0;
		VectorNormalize(hordir);
		if (dist2 > 80) dist2 = 80;
		speed = 400 - (400 - 5 * dist2);
		EA_Move(ms->client, hordir, speed);
	} 
	VectorCopy(hordir, result.movedir);
	return result;
} 
bot_moveresult_t BotFinishTravel_Jump(bot_movestate_t *ms, aas_reachability_t *reach)
{
	vec3_t hordir, hordir2;
	float speed, dist;
	bot_moveresult_t_cleared( result );

	if (!ms->jumpreach) return result;
	hordir[0] = reach->end[0] - ms->origin[0];
	hordir[1] = reach->end[1] - ms->origin[1];
	hordir[2] = 0;
	dist = VectorNormalize(hordir);
	hordir2[0] = reach->end[0] - reach->start[0];
	hordir2[1] = reach->end[1] - reach->start[1];
	hordir2[2] = 0;
	VectorNormalize(hordir2);
	if (DotProduct(hordir, hordir2) < -0.5 && dist < 24) return result;
	speed = 800;
	EA_Move(ms->client, hordir, speed);
	VectorCopy(hordir, result.movedir);
	return result;
} 
bot_moveresult_t BotTravel_Ladder(bot_movestate_t *ms, aas_reachability_t *reach)
{
	vec3_t dir, viewdir;
	vec3_t origin = {0, 0, 0};
	bot_moveresult_t_cleared( result );

	{
		VectorSubtract(reach->end, ms->origin, dir);
		VectorNormalize(dir);
		viewdir[0] = dir[0];
		viewdir[1] = dir[1];
		viewdir[2] = 3 * dir[2];
		Vector2Angles(viewdir, result.ideal_viewangles);
		EA_Move(ms->client, origin, 0);
		EA_MoveForward(ms->client);
		result.flags |= MOVERESULT_MOVEMENTVIEW;
	} 
	VectorCopy(dir, result.movedir);
	return result;
} 
bot_moveresult_t BotTravel_Teleport(bot_movestate_t *ms, aas_reachability_t *reach)
{
	vec3_t hordir;
	float dist;
	bot_moveresult_t_cleared( result );

	if (ms->moveflags & MFL_TELEPORTED) return result;

	VectorSubtract(reach->start, ms->origin, hordir);
	if (!(ms->moveflags & MFL_SWIMMING)) hordir[2] = 0;
	dist = VectorNormalize(hordir);
	BotCheckBlocked(ms, hordir, qtrue, &result);

	if (dist < 30) EA_Move(ms->client, hordir, 200);
	else EA_Move(ms->client, hordir, 400);

	if (ms->moveflags & MFL_SWIMMING) result.flags |= MOVERESULT_SWIMVIEW;

	VectorCopy(hordir, result.movedir);
	return result;
} 
bot_moveresult_t BotTravel_Elevator(bot_movestate_t *ms, aas_reachability_t *reach)
{
	vec3_t dir, dir1, dir2, hordir, bottomcenter;
	float dist, dist1, dist2, speed;
	bot_moveresult_t_cleared( result );

	if (BotOnMover(ms->origin, ms->entitynum, reach))
	{
#ifdef DEBUG_ELEVATOR
		botimport.Print(PRT_MESSAGE, "bot on elevator\n");
#endif 
		if (fabsf(ms->origin[2] - reach->end[2]) < sv_maxbarrier->value)
		{
#ifdef DEBUG_ELEVATOR
			botimport.Print(PRT_MESSAGE, "bot moving to end\n");
#endif 
			VectorSubtract(reach->end, ms->origin, hordir);
			hordir[2] = 0;
			VectorNormalize(hordir);
			if (!BotCheckBarrierJump(ms, hordir, 100))
			{
				EA_Move(ms->client, hordir, 400);
			} 
			VectorCopy(hordir, result.movedir);
		} 
		else
		{
			MoverBottomCenter(reach, bottomcenter);
			VectorSubtract(bottomcenter, ms->origin, hordir);
			hordir[2] = 0;
			dist = VectorNormalize(hordir);
			if (dist > 10)
			{
#ifdef DEBUG_ELEVATOR
				botimport.Print(PRT_MESSAGE, "bot moving to center\n");
#endif 
				if (dist > 100) dist = 100;
				speed = 400 - (400 - 4 * dist);
				EA_Move(ms->client, hordir, speed);
				VectorCopy(hordir, result.movedir);
			} 
		} 
	} 
	else
	{
#ifdef DEBUG_ELEVATOR
		botimport.Print(PRT_MESSAGE, "bot not on elevator\n");
#endif 
		VectorSubtract(reach->end, ms->origin, dir);
		dist = VectorLength(dir);
		if (dist < 64)
		{
			if (dist > 60) dist = 60;
			speed = 360 - (360 - 6 * dist);
			if ((ms->moveflags & MFL_SWIMMING) || !BotCheckBarrierJump(ms, dir, 50))
			{
				if (speed > 5) EA_Move(ms->client, dir, speed);
			} 
			VectorCopy(dir, result.movedir);
			if (ms->moveflags & MFL_SWIMMING) result.flags |= MOVERESULT_SWIMVIEW;
			ms->reachability_time = 0;
			return result;
		} 
		VectorSubtract(reach->start, ms->origin, dir1);
		if (!(ms->moveflags & MFL_SWIMMING)) dir1[2] = 0;
		dist1 = VectorNormalize(dir1);
		if (!MoverDown(reach))
		{
#ifdef DEBUG_ELEVATOR
			botimport.Print(PRT_MESSAGE, "elevator not down\n");
#endif 
			dist = dist1;
			VectorCopy(dir1, dir);
			BotCheckBlocked(ms, dir, qfalse, &result);
			if (dist > 60) dist = 60;
			speed = 360 - (360 - 6 * dist);
			if (!(ms->moveflags & MFL_SWIMMING) && !BotCheckBarrierJump(ms, dir, 50))
			{
				if (speed > 5) EA_Move(ms->client, dir, speed);
			} 
			VectorCopy(dir, result.movedir);
			if (ms->moveflags & MFL_SWIMMING) result.flags |= MOVERESULT_SWIMVIEW;
			result.type = RESULTTYPE_ELEVATORUP;
			result.flags |= MOVERESULT_WAITING;
			return result;
		} 
		MoverBottomCenter(reach, bottomcenter);
		VectorSubtract(bottomcenter, ms->origin, dir2);
		if (!(ms->moveflags & MFL_SWIMMING)) dir2[2] = 0;
		dist2 = VectorNormalize(dir2);
		if (dist1 < 20 || dist2 < dist1 || DotProduct(dir1, dir2) < 0)
		{
#ifdef DEBUG_ELEVATOR
			botimport.Print(PRT_MESSAGE, "bot moving to center\n");
#endif 
			dist = dist2;
			VectorCopy(dir2, dir);
		} 
		else 
		{
#ifdef DEBUG_ELEVATOR
			botimport.Print(PRT_MESSAGE, "bot moving to start\n");
#endif 
			dist = dist1;
			VectorCopy(dir1, dir);
		} 
		BotCheckBlocked(ms, dir, qfalse, &result);
		if (dist > 60) dist = 60;
		speed = 400 - (400 - 6 * dist);
		if (!(ms->moveflags & MFL_SWIMMING) && !BotCheckBarrierJump(ms, dir, 50))
		{
			EA_Move(ms->client, dir, speed);
		} 
		VectorCopy(dir, result.movedir);
		if (ms->moveflags & MFL_SWIMMING) result.flags |= MOVERESULT_SWIMVIEW;
	} 
	return result;
} 
bot_moveresult_t BotFinishTravel_Elevator(bot_movestate_t *ms, aas_reachability_t *reach)
{
	vec3_t bottomcenter, bottomdir, topdir;
	bot_moveresult_t_cleared( result );

	MoverBottomCenter(reach, bottomcenter);
	VectorSubtract(bottomcenter, ms->origin, bottomdir);
	VectorSubtract(reach->end, ms->origin, topdir);
	if (fabs(bottomdir[2]) < fabs(topdir[2]))
	{
		VectorNormalize(bottomdir);
		EA_Move(ms->client, bottomdir, 300);
	} 
	else
	{
		VectorNormalize(topdir);
		EA_Move(ms->client, topdir, 300);
	} 
	return result;
} 
void BotFuncBobStartEnd(aas_reachability_t *reach, vec3_t start, vec3_t end, vec3_t origin)
{
	int spawnflags, modelnum;
	vec3_t mins, maxs, mid, angles = {0, 0, 0};
	int num0, num1;

	modelnum = reach->facenum & 0x0000FFFF;
	if (!AAS_OriginOfMoverWithModelNum(modelnum, origin))
	{
		botimport.Print(PRT_MESSAGE, "BotFuncBobStartEnd: no entity with model %d\n", modelnum);
		VectorSet(start, 0, 0, 0);
		VectorSet(end, 0, 0, 0);
		return;
	} 
	AAS_BSPModelMinsMaxsOrigin(modelnum, angles, mins, maxs, NULL);
	VectorAdd(mins, maxs, mid);
	VectorScale(mid, 0.5, mid);
	VectorCopy(mid, start);
	VectorCopy(mid, end);
	spawnflags = reach->facenum >> 16;
	num0 = reach->edgenum >> 16;
	if (num0 > 0x00007FFF) num0 |= 0xFFFF0000;
	num1 = reach->edgenum & 0x0000FFFF;
	if (num1 > 0x00007FFF) num1 |= 0xFFFF0000;
	if (spawnflags & 1)
	{
		start[0] = num0;
		end[0] = num1;
		origin[0] += mid[0];
		origin[1] = mid[1];
		origin[2] = mid[2];
	} 
	else if (spawnflags & 2)
	{
		start[1] = num0;
		end[1] = num1;
		origin[0] = mid[0];
		origin[1] += mid[1];
		origin[2] = mid[2];
	} 
	else
	{
		start[2] = num0;
		end[2] = num1;
		origin[0] = mid[0];
		origin[1] = mid[1];
		origin[2] += mid[2];
	} 
} 
bot_moveresult_t BotTravel_FuncBobbing(bot_movestate_t *ms, aas_reachability_t *reach)
{
	vec3_t dir, dir1, dir2, hordir, bottomcenter, bob_start, bob_end, bob_origin;
	float dist, dist1, dist2, speed;
	bot_moveresult_t_cleared( result );

	BotFuncBobStartEnd(reach, bob_start, bob_end, bob_origin);
	if (BotOnMover(ms->origin, ms->entitynum, reach))
	{
#ifdef DEBUG_FUNCBOB
		botimport.Print(PRT_MESSAGE, "bot on func_bobbing\n");
#endif
		VectorSubtract(bob_origin, bob_end, dir);
		if (VectorLength(dir) < 24)
		{
#ifdef DEBUG_FUNCBOB
			botimport.Print(PRT_MESSAGE, "bot moving to reachability end\n");
#endif
			VectorSubtract(reach->end, ms->origin, hordir);
			hordir[2] = 0;
			VectorNormalize(hordir);
			if (!BotCheckBarrierJump(ms, hordir, 100))
			{
				EA_Move(ms->client, hordir, 400);
			} 
			VectorCopy(hordir, result.movedir);
		} 
		else
		{
			MoverBottomCenter(reach, bottomcenter);
			VectorSubtract(bottomcenter, ms->origin, hordir);
			hordir[2] = 0;
			dist = VectorNormalize(hordir);
			if (dist > 10)
			{
#ifdef DEBUG_FUNCBOB
				botimport.Print(PRT_MESSAGE, "bot moving to func_bobbing center\n");
#endif
				if (dist > 100) dist = 100;
				speed = 400 - (400 - 4 * dist);
				EA_Move(ms->client, hordir, speed);
				VectorCopy(hordir, result.movedir);
			} 
		} 
	} 
	else
	{
#ifdef DEBUG_FUNCBOB
		botimport.Print(PRT_MESSAGE, "bot not ontop of func_bobbing\n");
#endif
		VectorSubtract(reach->end, ms->origin, dir);
		dist = VectorLength(dir);
		if (dist < 64)
		{
#ifdef DEBUG_FUNCBOB
			botimport.Print(PRT_MESSAGE, "bot moving to end\n");
#endif
			if (dist > 60) dist = 60;
			speed = 360 - (360 - 6 * dist);
			if ((ms->moveflags & MFL_SWIMMING) || !BotCheckBarrierJump(ms, dir, 50))
			{
				if (speed > 5) EA_Move(ms->client, dir, speed);
			} 
			VectorCopy(dir, result.movedir);
			if (ms->moveflags & MFL_SWIMMING) result.flags |= MOVERESULT_SWIMVIEW;
			ms->reachability_time = 0;
			return result;
		} 
		VectorSubtract(reach->start, ms->origin, dir1);
		if (!(ms->moveflags & MFL_SWIMMING)) dir1[2] = 0;
		dist1 = VectorNormalize(dir1);
		VectorSubtract(bob_origin, bob_start, dir);
		if (VectorLength(dir) > 16)
		{
#ifdef DEBUG_FUNCBOB
			botimport.Print(PRT_MESSAGE, "func_bobbing not at start\n");
#endif
			dist = dist1;
			VectorCopy(dir1, dir);
			BotCheckBlocked(ms, dir, qfalse, &result);
			if (dist > 60) dist = 60;
			speed = 360 - (360 - 6 * dist);
			if (!(ms->moveflags & MFL_SWIMMING) && !BotCheckBarrierJump(ms, dir, 50))
			{
				if (speed > 5) EA_Move(ms->client, dir, speed);
			} 
			VectorCopy(dir, result.movedir);
			if (ms->moveflags & MFL_SWIMMING) result.flags |= MOVERESULT_SWIMVIEW;
			result.type = RESULTTYPE_WAITFORFUNCBOBBING;
			result.flags |= MOVERESULT_WAITING;
			return result;
		} 
		MoverBottomCenter(reach, bottomcenter);
		VectorSubtract(bottomcenter, ms->origin, dir2);
		if (!(ms->moveflags & MFL_SWIMMING)) dir2[2] = 0;
		dist2 = VectorNormalize(dir2);
		if (dist1 < 20 || dist2 < dist1 || DotProduct(dir1, dir2) < 0)
		{
#ifdef DEBUG_FUNCBOB
			botimport.Print(PRT_MESSAGE, "bot moving to func_bobbing center\n");
#endif
			dist = dist2;
			VectorCopy(dir2, dir);
		} 
		else 
		{
#ifdef DEBUG_FUNCBOB
			botimport.Print(PRT_MESSAGE, "bot moving to reachability start\n");
#endif
			dist = dist1;
			VectorCopy(dir1, dir);
		} 
		BotCheckBlocked(ms, dir, qfalse, &result);
		if (dist > 60) dist = 60;
		speed = 400 - (400 - 6 * dist);
		if (!(ms->moveflags & MFL_SWIMMING) && !BotCheckBarrierJump(ms, dir, 50))
		{
			EA_Move(ms->client, dir, speed);
		} 
		VectorCopy(dir, result.movedir);
		if (ms->moveflags & MFL_SWIMMING) result.flags |= MOVERESULT_SWIMVIEW;
	} 
	return result;
} 
bot_moveresult_t BotFinishTravel_FuncBobbing(bot_movestate_t *ms, aas_reachability_t *reach)
{
	vec3_t bob_origin, bob_start, bob_end, dir, hordir, bottomcenter;
	bot_moveresult_t_cleared( result );
	float dist, speed;

	BotFuncBobStartEnd(reach, bob_start, bob_end, bob_origin);
	VectorSubtract(bob_origin, bob_end, dir);
	dist = VectorLength(dir);
	if (dist < 16)
	{
		VectorSubtract(reach->end, ms->origin, hordir);
		if (!(ms->moveflags & MFL_SWIMMING)) hordir[2] = 0;
		dist = VectorNormalize(hordir);
		if (dist > 60) dist = 60;
		speed = 360 - (360 - 6 * dist);
		if (speed > 5) EA_Move(ms->client, dir, speed);
		VectorCopy(dir, result.movedir);
		if (ms->moveflags & MFL_SWIMMING) result.flags |= MOVERESULT_SWIMVIEW;
	} 
	else
	{
		MoverBottomCenter(reach, bottomcenter);
		VectorSubtract(bottomcenter, ms->origin, hordir);
		if (!(ms->moveflags & MFL_SWIMMING)) hordir[2] = 0;
		dist = VectorNormalize(hordir);
		if (dist > 5)
		{
			if (dist > 100) dist = 100;
			speed = 400 - (400 - 4 * dist);
			EA_Move(ms->client, hordir, speed);
			VectorCopy(hordir, result.movedir);
		} 
	} 
	return result;
} 
int GrappleState(bot_movestate_t *ms, aas_reachability_t *reach)
{
	int i;
	aas_entityinfo_t entinfo;

	if (ms->moveflags & MFL_GRAPPLEPULL)
		return 2;
	for (i = AAS_NextEntity(0); i; i = AAS_NextEntity(i))
	{
		if (AAS_EntityType(i) == (int) entitytypemissile->value)
		{
			AAS_EntityInfo(i, &entinfo);
			if (entinfo.weapon == (int) weapindex_grapple->value)
			{
				return 1;
			} 
		} 
	} 
	return 0;
} 
void BotResetGrapple(bot_movestate_t *ms)
{
	aas_reachability_t reach;

	AAS_ReachabilityFromNum(ms->lastreachnum, &reach);
	if ((reach.traveltype & TRAVELTYPE_MASK) != TRAVEL_GRAPPLEHOOK)
	{
		if ((ms->moveflags & MFL_ACTIVEGRAPPLE) || ms->grapplevisible_time)
		{
			if (offhandgrapple->value)
				EA_Command(ms->client, cmd_grappleoff->string);
			ms->moveflags &= ~MFL_ACTIVEGRAPPLE;
			ms->grapplevisible_time = 0;
#ifdef DEBUG_GRAPPLE
			botimport.Print(PRT_MESSAGE, "reset grapple\n");
#endif 
		} 
	} 
} 
bot_moveresult_t BotTravel_Grapple(bot_movestate_t *ms, aas_reachability_t *reach)
{
	bot_moveresult_t_cleared( result );
	float dist, speed;
	vec3_t dir, viewdir, org;
	int state, areanum;
	bsp_trace_t trace;

#ifdef DEBUG_GRAPPLE
	static int debugline;
	if (!debugline) debugline = botimport.DebugLineCreate();
	botimport.DebugLineShow(debugline, reach->start, reach->end, LINECOLOR_BLUE);
#endif 

	if (ms->moveflags & MFL_GRAPPLERESET)
	{
		if (offhandgrapple->value)
			EA_Command(ms->client, cmd_grappleoff->string);
		ms->moveflags &= ~MFL_ACTIVEGRAPPLE;
		return result;
	} 
	if (!(int) offhandgrapple->value)
	{
		result.weapon = weapindex_grapple->value;
		result.flags |= MOVERESULT_MOVEMENTWEAPON;
	} 
	if (ms->moveflags & MFL_ACTIVEGRAPPLE)
	{
#ifdef DEBUG_GRAPPLE
		botimport.Print(PRT_MESSAGE, "BotTravel_Grapple: active grapple\n");
#endif 
		state = GrappleState(ms, reach);
		VectorSubtract(reach->end, ms->origin, dir);
		dir[2] = 0;
		dist = VectorLength(dir);
		if (state && dist < 48)
		{
			if (ms->lastgrappledist - dist < 1)
			{
#ifdef DEBUG_GRAPPLE
				botimport.Print(PRT_ERROR, "grapple normal end\n");
#endif 
				if (offhandgrapple->value)
					EA_Command(ms->client, cmd_grappleoff->string);
				ms->moveflags &= ~MFL_ACTIVEGRAPPLE;
				ms->moveflags |= MFL_GRAPPLERESET;
				ms->reachability_time = 0;	
				return result;
			} 
		} 
		else if (!state || (state == 2 && dist > ms->lastgrappledist - 2))
		{
			if (ms->grapplevisible_time < AAS_Time() - 0.4)
			{
#ifdef DEBUG_GRAPPLE
				botimport.Print(PRT_ERROR, "grapple not visible\n");
#endif 
				if (offhandgrapple->value)
					EA_Command(ms->client, cmd_grappleoff->string);
				ms->moveflags &= ~MFL_ACTIVEGRAPPLE;
				ms->moveflags |= MFL_GRAPPLERESET;
				ms->reachability_time = 0;	
				return result;
			} 
		} 
		else
		{
			ms->grapplevisible_time = AAS_Time();
		} 
		if (!(int) offhandgrapple->value)
		{
			EA_Attack(ms->client, qtrue, qfalse);
		} 
		ms->lastgrappledist = dist;
	} 
	else
	{
#ifdef DEBUG_GRAPPLE
		botimport.Print(PRT_MESSAGE, "BotTravel_Grapple: inactive grapple\n");
#endif 
		ms->grapplevisible_time = AAS_Time();
		VectorSubtract(reach->start, ms->origin, dir);
		if (!(ms->moveflags & MFL_SWIMMING)) dir[2] = 0;
		VectorAdd(ms->origin, ms->viewoffset, org);
		VectorSubtract(reach->end, org, viewdir);
		dist = VectorNormalize(dir);
		Vector2Angles(viewdir, result.ideal_viewangles);
		result.flags |= MOVERESULT_MOVEMENTVIEW;
		if (dist < 5 &&
			fabs(AngleDiff(result.ideal_viewangles[0], ms->viewangles[0])) < 2 &&
			fabs(AngleDiff(result.ideal_viewangles[1], ms->viewangles[1])) < 2)
		{
#ifdef DEBUG_GRAPPLE
			botimport.Print(PRT_MESSAGE, "BotTravel_Grapple: activating grapple\n");
#endif 
			VectorAdd(ms->origin, ms->viewoffset, org);
			trace = AAS_Trace(org, NULL, NULL, reach->end, ms->entitynum, CONTENTS_SOLID);
			VectorSubtract(reach->end, trace.endpos, dir);
			if (VectorLength(dir) > 16)
			{
				result.failure = qtrue;
				return result;
			} 
			if (offhandgrapple->value)
			{
				EA_Command(ms->client, cmd_grappleon->string);
			} 
			else
			{
				EA_Attack(ms->client, qtrue, qfalse);
			} 
			ms->moveflags |= MFL_ACTIVEGRAPPLE;
			ms->lastgrappledist = 999999;
		} 
		else
		{
			if (dist < 70) speed = 300 - (300 - 4 * dist);
			else speed = 400;
			BotCheckBlocked(ms, dir, qtrue, &result);
			EA_Move(ms->client, dir, speed);
			VectorCopy(dir, result.movedir);
		} 
		areanum = AAS_PointAreaNum(ms->origin);
		if (areanum && areanum != ms->reachareanum) ms->reachability_time = 0;
	} 
	return result;
} 
bot_moveresult_t BotTravel_RocketJump(bot_movestate_t *ms, aas_reachability_t *reach)
{
	vec3_t hordir;
	float dist, speed;
	bot_moveresult_t_cleared( result );

	hordir[0] = reach->start[0] - ms->origin[0];
	hordir[1] = reach->start[1] - ms->origin[1];
	hordir[2] = 0;
	dist = VectorNormalize(hordir);
	Vector2Angles(hordir, result.ideal_viewangles);
	result.ideal_viewangles[PITCH] = 90;
	if (dist < 5 &&
			fabs(AngleDiff(result.ideal_viewangles[0], ms->viewangles[0])) < 5 &&
			fabs(AngleDiff(result.ideal_viewangles[1], ms->viewangles[1])) < 5)
	{
		hordir[0] = reach->end[0] - ms->origin[0];
		hordir[1] = reach->end[1] - ms->origin[1];
		hordir[2] = 0;
		VectorNormalize(hordir);
		EA_Jump(ms->client);
		EA_Attack(ms->client, qtrue, qfalse);
		EA_Move(ms->client, hordir, 800);
		ms->jumpreach = ms->lastreachnum;
	} 
	else
	{
		if (dist > 80) dist = 80;
		speed = 400 - (400 - 5 * dist);
		EA_Move(ms->client, hordir, speed);
	} 
	Vector2Angles(hordir, result.ideal_viewangles);
	result.ideal_viewangles[PITCH] = 90;
	EA_View(ms->client, result.ideal_viewangles);
	result.flags |= MOVERESULT_MOVEMENTVIEWSET;
	EA_SelectWeapon(ms->client, (int) weapindex_rocketlauncher->value);
	result.weapon = (int) weapindex_rocketlauncher->value;
	result.flags |= MOVERESULT_MOVEMENTWEAPON;
	VectorCopy(hordir, result.movedir);
	return result;
} 
bot_moveresult_t BotTravel_BFGJump(bot_movestate_t *ms, aas_reachability_t *reach)
{
	vec3_t hordir;
	float dist, speed;
	bot_moveresult_t_cleared( result );

	hordir[0] = reach->start[0] - ms->origin[0];
	hordir[1] = reach->start[1] - ms->origin[1];
	hordir[2] = 0;
	dist = VectorNormalize(hordir);
	if (dist < 5 &&
			fabs(AngleDiff(result.ideal_viewangles[0], ms->viewangles[0])) < 5 &&
			fabs(AngleDiff(result.ideal_viewangles[1], ms->viewangles[1])) < 5)
	{
		hordir[0] = reach->end[0] - ms->origin[0];
		hordir[1] = reach->end[1] - ms->origin[1];
		hordir[2] = 0;
		VectorNormalize(hordir);
		EA_Jump(ms->client);
		EA_Attack(ms->client, qtrue, qfalse);
		EA_Move(ms->client, hordir, 800);
		ms->jumpreach = ms->lastreachnum;
	} 
	else
	{
		if (dist > 80) dist = 80;
		speed = 400 - (400 - 5 * dist);
		EA_Move(ms->client, hordir, speed);
	} 
	Vector2Angles(hordir, result.ideal_viewangles);
	result.ideal_viewangles[PITCH] = 90;
	EA_View(ms->client, result.ideal_viewangles);
	result.flags |= MOVERESULT_MOVEMENTVIEWSET;
	EA_SelectWeapon(ms->client, (int) weapindex_bfg10k->value);
	result.weapon = (int) weapindex_bfg10k->value;
	result.flags |= MOVERESULT_MOVEMENTWEAPON;
	VectorCopy(hordir, result.movedir);
	return result;
} 
bot_moveresult_t BotFinishTravel_WeaponJump(bot_movestate_t *ms, aas_reachability_t *reach)
{
	vec3_t hordir;
	float speed;
	bot_moveresult_t_cleared( result );

	if (!ms->jumpreach) return result;
	if (!BotAirControl(ms->origin, ms->velocity, reach->end, hordir, &speed))
	{
		VectorSubtract(reach->end, ms->origin, hordir);
		hordir[2] = 0;
		VectorNormalize(hordir);
		speed = 400;
	} 
	EA_Move(ms->client, hordir, speed);
	VectorCopy(hordir, result.movedir);
	return result;
} 
bot_moveresult_t BotTravel_JumpPad(bot_movestate_t *ms, aas_reachability_t *reach)
{
	vec3_t hordir;
	bot_moveresult_t_cleared( result );

	hordir[0] = reach->start[0] - ms->origin[0];
	hordir[1] = reach->start[1] - ms->origin[1];
	hordir[2] = 0;
	BotCheckBlocked(ms, hordir, qtrue, &result);
	EA_Move(ms->client, hordir, 400);
	VectorCopy(hordir, result.movedir);
	return result;
} 
bot_moveresult_t BotFinishTravel_JumpPad(bot_movestate_t *ms, aas_reachability_t *reach)
{
	float speed;
	vec3_t hordir;
	bot_moveresult_t_cleared( result );

	if (!BotAirControl(ms->origin, ms->velocity, reach->end, hordir, &speed))
	{
		hordir[0] = reach->end[0] - ms->origin[0];
		hordir[1] = reach->end[1] - ms->origin[1];
		hordir[2] = 0;
		VectorNormalize(hordir);
		speed = 400;
	} 
	BotCheckBlocked(ms, hordir, qtrue, &result);
	EA_Move(ms->client, hordir, speed);
	VectorCopy(hordir, result.movedir);
	return result;
} 
int BotReachabilityTime(aas_reachability_t *reach)
{
	switch(reach->traveltype & TRAVELTYPE_MASK)
	{
		case TRAVEL_WALK: return 5;
		case TRAVEL_CROUCH: return 5;
		case TRAVEL_BARRIERJUMP: return 5;
		case TRAVEL_LADDER: return 6;
		case TRAVEL_WALKOFFLEDGE: return 5;
		case TRAVEL_JUMP: return 5;
		case TRAVEL_SWIM: return 5;
		case TRAVEL_WATERJUMP: return 5;
		case TRAVEL_TELEPORT: return 5;
		case TRAVEL_ELEVATOR: return 10;
		case TRAVEL_GRAPPLEHOOK: return 8;
		case TRAVEL_ROCKETJUMP: return 6;
		case TRAVEL_BFGJUMP: return 6;
		case TRAVEL_JUMPPAD: return 10;
		case TRAVEL_FUNCBOB: return 10;
		default:
		{
			botimport.Print(PRT_ERROR, "travel type %d not implemented yet\n", reach->traveltype);
			return 8;
		} 
	} 
} 
bot_moveresult_t BotMoveInGoalArea(bot_movestate_t *ms, bot_goal_t *goal)
{
	bot_moveresult_t_cleared( result );
	vec3_t dir;
	float dist, speed;

#ifdef DEBUG
#endif 
	dir[0] = goal->origin[0] - ms->origin[0];
	dir[1] = goal->origin[1] - ms->origin[1];
	if (ms->moveflags & MFL_SWIMMING)
	{
		dir[2] = goal->origin[2] - ms->origin[2];
		result.traveltype = TRAVEL_SWIM;
	} 
	else
	{
		dir[2] = 0;
		result.traveltype = TRAVEL_WALK;
	} 
	dist = VectorNormalize(dir);
	if (dist > 100) dist = 100;
	speed = 400 - (400 - 4 * dist);
	if (speed < 10) speed = 0;
	BotCheckBlocked(ms, dir, qtrue, &result);
	EA_Move(ms->client, dir, speed);
	VectorCopy(dir, result.movedir);
	if (ms->moveflags & MFL_SWIMMING)
	{
		Vector2Angles(dir, result.ideal_viewangles);
		result.flags |= MOVERESULT_SWIMVIEW;
	} 
	ms->lastreachnum = 0;
	ms->lastareanum = 0;
	ms->lastgoalareanum = goal->areanum;
	VectorCopy(ms->origin, ms->lastorigin);
	return result;
} 
void BotMoveToGoal(bot_moveresult_t *result, int movestate, bot_goal_t *goal, int travelflags)
{
	int reachnum, lastreachnum, foundjumppad, ent, resultflags;
	aas_reachability_t reach, lastreach;
	bot_movestate_t *ms;

	result->failure = qfalse;
	result->type = 0;
	result->blocked = qfalse;
	result->blockentity = 0;
	result->traveltype = 0;
	result->flags = 0;

	ms = BotMoveStateFromHandle(movestate);
	if (!ms) return;
	BotResetGrapple(ms);
	if (!goal)
	{
#ifdef DEBUG
		botimport.Print(PRT_MESSAGE, "client %d: movetogoal -> no goal\n", ms->client);
#endif 
		result->failure = qtrue;
		return;
	} 
	ms->moveflags &= ~(MFL_SWIMMING|MFL_AGAINSTLADDER);
	if (AAS_OnGround(ms->origin, ms->presencetype, ms->entitynum)) ms->moveflags |= MFL_ONGROUND;
	if (ms->moveflags & MFL_ONGROUND)
	{
		int modeltype, modelnum;

		ent = BotOnTopOfEntity(ms);

		if (ent != -1)
		{
			modelnum = AAS_EntityModelindex(ent);
			if (modelnum >= 0 && modelnum < MAX_MODELS)
			{
				modeltype = modeltypes[modelnum];

				if (modeltype == MODELTYPE_FUNC_PLAT)
				{
					AAS_ReachabilityFromNum(ms->lastreachnum, &reach);
					if ((reach.traveltype & TRAVELTYPE_MASK) != TRAVEL_ELEVATOR ||
						(reach.facenum & 0x0000FFFF) != modelnum)
					{
						reachnum = AAS_NextModelReachability(0, modelnum);
						if (reachnum)
						{
							AAS_ReachabilityFromNum(reachnum, &reach);
							ms->lastreachnum = reachnum;
							ms->reachability_time = AAS_Time() + BotReachabilityTime(&reach);
						} 
						else
						{
							if (botDeveloper)
							{
								botimport.Print(PRT_MESSAGE, "client %d: on func_plat without reachability\n", ms->client);
							} 
							result->blocked = qtrue;
							result->blockentity = ent;
							result->flags |= MOVERESULT_ONTOPOFOBSTACLE;
							return;
						} 
					} 
					result->flags |= MOVERESULT_ONTOPOF_ELEVATOR;
				} 
				else if (modeltype == MODELTYPE_FUNC_BOB)
				{
					AAS_ReachabilityFromNum(ms->lastreachnum, &reach);
					if ((reach.traveltype & TRAVELTYPE_MASK) != TRAVEL_FUNCBOB ||
						(reach.facenum & 0x0000FFFF) != modelnum)
					{
						reachnum = AAS_NextModelReachability(0, modelnum);
						if (reachnum)
						{
							AAS_ReachabilityFromNum(reachnum, &reach);
							ms->lastreachnum = reachnum;
							ms->reachability_time = AAS_Time() + BotReachabilityTime(&reach);
						} 
						else
						{
							if (botDeveloper)
							{
								botimport.Print(PRT_MESSAGE, "client %d: on func_bobbing without reachability\n", ms->client);
							} 
							result->blocked = qtrue;
							result->blockentity = ent;
							result->flags |= MOVERESULT_ONTOPOFOBSTACLE;
							return;
						} 
					} 
					result->flags |= MOVERESULT_ONTOPOF_FUNCBOB;
				} 
				else if (modeltype == MODELTYPE_FUNC_STATIC || modeltype == MODELTYPE_FUNC_DOOR)
				{
					ms->areanum = BotFuzzyPointReachabilityArea(ms->origin);
					if (!AAS_AreaReachability(ms->areanum))
					{
						result->blocked = qtrue;
						result->blockentity = ent;
						result->flags |= MOVERESULT_ONTOPOFOBSTACLE;
						return;
					} 
				} 
				else
				{
					result->blocked = qtrue;
					result->blockentity = ent;
					result->flags |= MOVERESULT_ONTOPOFOBSTACLE;
					return;
				} 
			} 
		} 
	} 
	if (AAS_Swimming(ms->origin)) ms->moveflags |= MFL_SWIMMING;
	if (AAS_AgainstLadder(ms->origin)) ms->moveflags |= MFL_AGAINSTLADDER;
	if (ms->moveflags & (MFL_ONGROUND|MFL_SWIMMING|MFL_AGAINSTLADDER))
	{
		AAS_ReachabilityFromNum(ms->lastreachnum, &lastreach);
		ms->areanum = BotFuzzyPointReachabilityArea(ms->origin);
		if ( !ms->areanum )
		{
			result->failure = qtrue;
			result->blocked = qtrue;
			result->blockentity = 0;
			result->type = RESULTTYPE_INSOLIDAREA;
			return;
		} 
		if (ms->areanum == goal->areanum)
		{
			*result = BotMoveInGoalArea(ms, goal);
			return;
		} 
		reachnum = ms->lastreachnum;
		if (reachnum)
		{
			AAS_ReachabilityFromNum(reachnum, &reach);
			if (!(AAS_TravelFlagForType(reach.traveltype) & travelflags))
			{
				reachnum = 0;
			} 
			else if ((reach.traveltype & TRAVELTYPE_MASK) == TRAVEL_GRAPPLEHOOK)
			{
				if (ms->reachability_time < AAS_Time() ||
					(ms->moveflags & MFL_GRAPPLERESET))
				{
					reachnum = 0;
				} 
			} 
			else if ((reach.traveltype & TRAVELTYPE_MASK) == TRAVEL_ELEVATOR ||
				(reach.traveltype & TRAVELTYPE_MASK) == TRAVEL_FUNCBOB)
			{
				if ((result->flags & MOVERESULT_ONTOPOF_ELEVATOR) ||
					(result->flags & MOVERESULT_ONTOPOF_FUNCBOB))
				{
					ms->reachability_time = AAS_Time() + 5;
				} 
				if (ms->areanum == reach.areanum ||
					ms->reachability_time < AAS_Time())
				{
					reachnum = 0;
				} 
			} 
			else
			{
#ifdef DEBUG
				if (botDeveloper)
				{
					if (ms->reachability_time < AAS_Time())
					{
						botimport.Print(PRT_MESSAGE, "client %d: reachability timeout in ", ms->client);
						AAS_PrintTravelType(reach.traveltype & TRAVELTYPE_MASK);
						botimport.Print(PRT_MESSAGE, "\n");
					} 
				} 
#endif 
				if (ms->lastgoalareanum != goal->areanum ||
						ms->reachability_time < AAS_Time() ||
						ms->lastareanum != ms->areanum)
				{
					reachnum = 0;
				} 
			} 
		} 
		resultflags = 0;
		if (!reachnum)
		{
			if (!AAS_AreaReachability(ms->areanum))
			{
#ifdef DEBUG
				if (botDeveloper)
				{
					botimport.Print(PRT_MESSAGE, "area %d no reachability\n", ms->areanum);
				} 
#endif 
			} 
			reachnum = BotGetReachabilityToGoal(ms->origin, ms->areanum,
								ms->lastgoalareanum, ms->lastareanum,
											ms->avoidreach, ms->avoidreachtimes, ms->avoidreachtries,
														goal, travelflags,
																ms->avoidspots, ms->numavoidspots, &resultflags);
			ms->reachareanum = ms->areanum;
			ms->jumpreach = 0;						
			ms->moveflags &= ~MFL_GRAPPLERESET;	
			if (reachnum)
			{
				AAS_ReachabilityFromNum(reachnum, &reach);
				ms->reachability_time = AAS_Time() + BotReachabilityTime(&reach);
#ifdef AVOIDREACH
				BotAddToAvoidReach(ms, reachnum, AVOIDREACH_TIME);
#endif 
			} 
#ifdef DEBUG
			
			else if (botDeveloper)
			{
				botimport.Print(PRT_MESSAGE, "goal not reachable\n");
				Com_Memset(&reach, 0, sizeof(aas_reachability_t)); 
			} 
			if (botDeveloper)
			{
				if (ms->lastgoalareanum == goal->areanum)
				{
					if (ms->lastareanum == reach.areanum)
					{
						botimport.Print(PRT_MESSAGE, "same goal, going back to previous area\n");
					} 
				} 
			} 
#endif 
		} 
		ms->lastreachnum = reachnum;
		ms->lastgoalareanum = goal->areanum;
		ms->lastareanum = ms->areanum;
		if (reachnum)
		{
			AAS_ReachabilityFromNum(reachnum, &reach);
			result->traveltype = reach.traveltype;
#ifdef DEBUG_AI_MOVE
			AAS_ClearShownDebugLines();
			AAS_PrintTravelType(reach.traveltype & TRAVELTYPE_MASK);
			AAS_ShowReachability(&reach);
#endif 
#ifdef DEBUG
#endif 
			switch(reach.traveltype & TRAVELTYPE_MASK)
			{
				case TRAVEL_WALK: *result = BotTravel_Walk(ms, &reach); break;
				case TRAVEL_CROUCH: *result = BotTravel_Crouch(ms, &reach); break;
				case TRAVEL_BARRIERJUMP: *result = BotTravel_BarrierJump(ms, &reach); break;
				case TRAVEL_LADDER: *result = BotTravel_Ladder(ms, &reach); break;
				case TRAVEL_WALKOFFLEDGE: *result = BotTravel_WalkOffLedge(ms, &reach); break;
				case TRAVEL_JUMP: *result = BotTravel_Jump(ms, &reach); break;
				case TRAVEL_SWIM: *result = BotTravel_Swim(ms, &reach); break;
				case TRAVEL_WATERJUMP: *result = BotTravel_WaterJump(ms, &reach); break;
				case TRAVEL_TELEPORT: *result = BotTravel_Teleport(ms, &reach); break;
				case TRAVEL_ELEVATOR: *result = BotTravel_Elevator(ms, &reach); break;
				case TRAVEL_GRAPPLEHOOK: *result = BotTravel_Grapple(ms, &reach); break;
				case TRAVEL_ROCKETJUMP: *result = BotTravel_RocketJump(ms, &reach); break;
				case TRAVEL_BFGJUMP: *result = BotTravel_BFGJump(ms, &reach); break;
				case TRAVEL_JUMPPAD: *result = BotTravel_JumpPad(ms, &reach); break;
				case TRAVEL_FUNCBOB: *result = BotTravel_FuncBobbing(ms, &reach); break;
				default:
				{
					botimport.Print(PRT_FATAL, "travel type %d not implemented yet\n", (reach.traveltype & TRAVELTYPE_MASK));
					break;
				} 
			} 
			result->traveltype = reach.traveltype;
			result->flags |= resultflags;
		} 
		else
		{
			result->failure = qtrue;
			result->flags |= resultflags;
			Com_Memset(&reach, 0, sizeof(aas_reachability_t));
		} 
#ifdef DEBUG
		if (botDeveloper)
		{
			if (result->failure)
			{
				botimport.Print(PRT_MESSAGE, "client %d: movement failure in ", ms->client);
				AAS_PrintTravelType(reach.traveltype & TRAVELTYPE_MASK);
				botimport.Print(PRT_MESSAGE, "\n");
			} 
		} 
#endif 
	} 
	else
	{
		int i, numareas, areas[16];
		vec3_t end;

		foundjumppad = qfalse;
		VectorMA(ms->origin, -2 * ms->thinktime, ms->velocity, end);
		numareas = AAS_TraceAreas(ms->origin, end, areas, NULL, 16);
		for (i = numareas-1; i >= 0; i--)
		{
			if (AAS_AreaJumpPad(areas[i]))
			{
				foundjumppad = qtrue;
				lastreachnum = BotGetReachabilityToGoal(end, areas[i],
							ms->lastgoalareanum, ms->lastareanum,
							ms->avoidreach, ms->avoidreachtimes, ms->avoidreachtries,
							goal, TFL_JUMPPAD, ms->avoidspots, ms->numavoidspots, NULL);
				if (lastreachnum)
				{
					ms->lastreachnum = lastreachnum;
					ms->lastareanum = areas[i];
					break;
				} 
				else
				{
					for (lastreachnum = AAS_NextAreaReachability(areas[i], 0); lastreachnum;
						lastreachnum = AAS_NextAreaReachability(areas[i], lastreachnum))
					{
						AAS_ReachabilityFromNum(lastreachnum, &reach);
						if ((reach.traveltype & TRAVELTYPE_MASK) == TRAVEL_JUMPPAD)
						{
							ms->lastreachnum = lastreachnum;
							ms->lastareanum = areas[i];
						} 
					} 
					if (lastreachnum) break;
				} 
			} 
		} 
		if (botDeveloper)
		{
			if (foundjumppad && !ms->lastreachnum)
			{
				botimport.Print(PRT_MESSAGE, "client %d didn't find jumppad reachability\n", ms->client);
			} 
		} 
		if (ms->lastreachnum)
		{
			AAS_ReachabilityFromNum(ms->lastreachnum, &reach);
			result->traveltype = reach.traveltype;
#ifdef DEBUG
#endif 
			switch(reach.traveltype & TRAVELTYPE_MASK)
			{
				case TRAVEL_WALK: *result = BotTravel_Walk(ms, &reach); break;
				case TRAVEL_CROUCH:  break;
				case TRAVEL_BARRIERJUMP: *result = BotFinishTravel_BarrierJump(ms, &reach); break;
				case TRAVEL_LADDER: *result = BotTravel_Ladder(ms, &reach); break;
				case TRAVEL_WALKOFFLEDGE: *result = BotFinishTravel_WalkOffLedge(ms, &reach); break;
				case TRAVEL_JUMP: *result = BotFinishTravel_Jump(ms, &reach); break;
				case TRAVEL_SWIM: *result = BotTravel_Swim(ms, &reach); break;
				case TRAVEL_WATERJUMP: *result = BotFinishTravel_WaterJump(ms, &reach); break;
				case TRAVEL_TELEPORT:  break;
				case TRAVEL_ELEVATOR: *result = BotFinishTravel_Elevator(ms, &reach); break;
				case TRAVEL_GRAPPLEHOOK: *result = BotTravel_Grapple(ms, &reach); break;
				case TRAVEL_ROCKETJUMP:
				case TRAVEL_BFGJUMP: *result = BotFinishTravel_WeaponJump(ms, &reach); break;
				case TRAVEL_JUMPPAD: *result = BotFinishTravel_JumpPad(ms, &reach); break;
				case TRAVEL_FUNCBOB: *result = BotFinishTravel_FuncBobbing(ms, &reach); break;
				default:
				{
					botimport.Print(PRT_FATAL, "(last) travel type %d not implemented yet\n", (reach.traveltype & TRAVELTYPE_MASK));
					break;
				} 
			} 
			result->traveltype = reach.traveltype;
#ifdef DEBUG
			if (botDeveloper)
			{
				if (result->failure)
				{
					botimport.Print(PRT_MESSAGE, "client %d: movement failure in finish ", ms->client);
					AAS_PrintTravelType(reach.traveltype & TRAVELTYPE_MASK);
					botimport.Print(PRT_MESSAGE, "\n");
				} 
			} 
#endif 
		} 
	} 
	if (result->blocked) ms->reachability_time -= 10 * ms->thinktime;
	VectorCopy(ms->origin, ms->lastorigin);
} 
void BotResetAvoidReach(int movestate)
{
	bot_movestate_t *ms;

	ms = BotMoveStateFromHandle(movestate);
	if (!ms) return;
	Com_Memset(ms->avoidreach, 0, MAX_AVOIDREACH * sizeof(int));
	Com_Memset(ms->avoidreachtimes, 0, MAX_AVOIDREACH * sizeof(float));
	Com_Memset(ms->avoidreachtries, 0, MAX_AVOIDREACH * sizeof(int));
} 
void BotResetLastAvoidReach(int movestate)
{
	int i, latest;
	float latesttime;
	bot_movestate_t *ms;

	ms = BotMoveStateFromHandle(movestate);
	if (!ms) return;
	latesttime = 0;
	latest = 0;
	for (i = 0; i < MAX_AVOIDREACH; i++)
	{
		if (ms->avoidreachtimes[i] > latesttime)
		{
			latesttime = ms->avoidreachtimes[i];
			latest = i;
		} 
	} 
	if (latesttime)
	{
		ms->avoidreachtimes[latest] = 0;
		if (ms->avoidreachtries[latest] > 0) ms->avoidreachtries[latest]--;
	} 
} 
void BotResetMoveState(int movestate)
{
	bot_movestate_t *ms;

	ms = BotMoveStateFromHandle(movestate);
	if (!ms) return;
	Com_Memset(ms, 0, sizeof(bot_movestate_t));
} 
int BotSetupMoveAI(void)
{
	BotSetBrushModelTypes();
	sv_maxstep = LibVar("sv_step", "18");
	sv_maxbarrier = LibVar("sv_maxbarrier", "32");
	sv_gravity = LibVar("sv_gravity", "800");
	weapindex_rocketlauncher = LibVar("weapindex_rocketlauncher", "5");
	weapindex_bfg10k = LibVar("weapindex_bfg10k", "9");
	weapindex_grapple = LibVar("weapindex_grapple", "10");
	entitytypemissile = LibVar("entitytypemissile", "3");
	offhandgrapple = LibVar("offhandgrapple", "0");
	cmd_grappleon = LibVar("cmd_grappleon", "grappleon");
	cmd_grappleoff = LibVar("cmd_grappleoff", "grappleoff");
	return BLERR_NOERROR;
} 
void BotShutdownMoveAI(void)
{
	int i;

	for (i = 1; i <= MAX_CLIENTS; i++)
	{
		if (botmovestates[i])
		{
			FreeMemory(botmovestates[i]);
			botmovestates[i] = NULL;
		} 
	} 
} 


