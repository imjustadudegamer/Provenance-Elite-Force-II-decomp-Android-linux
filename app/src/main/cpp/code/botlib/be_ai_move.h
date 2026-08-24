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


#define MOVE_WALK						1
#define MOVE_CROUCH						2
#define MOVE_JUMP						4
#define MOVE_GRAPPLE					8
#define MOVE_ROCKETJUMP					16
#define MOVE_BFGJUMP					32
#define MFL_BARRIERJUMP					1		
#define MFL_ONGROUND					2		
#define MFL_SWIMMING					4		
#define MFL_AGAINSTLADDER				8		
#define MFL_WATERJUMP					16		
#define MFL_TELEPORTED					32		
#define MFL_GRAPPLEPULL					64		
#define MFL_ACTIVEGRAPPLE				128		
#define MFL_GRAPPLERESET				256		
#define MFL_WALK						512		
#define MOVERESULT_MOVEMENTVIEW			1		
#define MOVERESULT_SWIMVIEW				2		
#define MOVERESULT_WAITING				4		
#define MOVERESULT_MOVEMENTVIEWSET		8		
#define MOVERESULT_MOVEMENTWEAPON		16		
#define MOVERESULT_ONTOPOFOBSTACLE		32		
#define MOVERESULT_ONTOPOF_FUNCBOB		64		
#define MOVERESULT_ONTOPOF_ELEVATOR		128		
#define MOVERESULT_BLOCKEDBYAVOIDSPOT	256		
#define MAX_AVOIDREACH					1
#define MAX_AVOIDSPOTS					32
#define AVOID_CLEAR						0		
#define AVOID_ALWAYS					1		
#define AVOID_DONTBLOCK					2		
#define RESULTTYPE_ELEVATORUP			1		
#define RESULTTYPE_WAITFORFUNCBOBBING	2		
#define RESULTTYPE_BADGRAPPLEPATH		4		
#define RESULTTYPE_INSOLIDAREA			8		

typedef struct bot_initmove_s
{
	vec3_t origin;				
	vec3_t velocity;			
	vec3_t viewoffset;			
	int entitynum;				
	int client;					
	float thinktime;			
	int presencetype;			
	vec3_t viewangles;			
	int or_moveflags;			
} bot_initmove_t;

typedef struct bot_moveresult_s
{
	int failure;				
	int type;					
	int blocked;				
	int blockentity;			
	int traveltype;				
	int flags;					
	int weapon;					
	vec3_t movedir;				
	vec3_t ideal_viewangles;	
} bot_moveresult_t;

#define bot_moveresult_t_cleared(x) bot_moveresult_t (x) = {0, 0, 0, 0, 0, 0, 0, {0, 0, 0}, {0, 0, 0}}

typedef struct bot_avoidspot_s
{
	vec3_t origin;
	float radius;
	int type;
} bot_avoidspot_t;

void BotResetMoveState(int movestate);
void BotMoveToGoal(bot_moveresult_t *result, int movestate, bot_goal_t *goal, int travelflags);
int BotMoveInDirection(int movestate, vec3_t dir, float speed, int type);
void BotResetAvoidReach(int movestate);
void BotResetLastAvoidReach(int movestate);
int BotReachabilityArea(vec3_t origin, int client);
int BotMovementViewTarget(int movestate, bot_goal_t *goal, int travelflags, float lookahead, vec3_t target);
int BotPredictVisiblePosition(vec3_t origin, int areanum, bot_goal_t *goal, int travelflags, vec3_t target);
int BotAllocMoveState(void);
void BotFreeMoveState(int handle);
void BotInitMoveState(int handle, bot_initmove_t *initmove);
void BotAddAvoidSpot(int movestate, vec3_t origin, float radius, int type);
void BotSetBrushModelTypes(void);
int BotSetupMoveAI(void);
void BotShutdownMoveAI(void);

