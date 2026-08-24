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
void AAS_InitRouting(void);
void AAS_FreeRoutingCaches(void);
unsigned short int AAS_AreaTravelTime(int areanum, vec3_t start, vec3_t end);
void AAS_CreateAllRoutingCache(void);
void AAS_WriteRouteCache(void);
void LIMIT_RouteFrame(void);
void LIMIT_RouteCount(int which, int n);
void LIMIT_RouteBusec(int usec);
void AAS_RoutingInfo(void);
#endif 

int AAS_TravelFlagForType(int traveltype);
int AAS_AreaContentsTravelFlags(int areanum);
int AAS_NextAreaReachability(int areanum, int reachnum);
void AAS_ReachabilityFromNum(int num, struct aas_reachability_s *reach);
int AAS_RandomGoalArea(int areanum, int travelflags, int *goalareanum, vec3_t goalorigin);
int AAS_EnableRoutingArea(int areanum, int enable);
unsigned short int AAS_AreaTravelTime(int areanum, vec3_t start, vec3_t end);
int AAS_AreaTravelTimeToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags);
int AAS_PredictRoute(struct aas_predictroute_s *route, int areanum, vec3_t origin,
							int goalareanum, int travelflags, int maxareas, int maxtime,
							int stopevent, int stopcontents, int stoptfl, int stopareanum);


