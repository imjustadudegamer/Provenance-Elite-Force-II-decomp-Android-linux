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
void AAS_InitReachability(void);
int AAS_ContinueInitReachability(float time);
int AAS_BestReachableLinkArea(aas_link_t *areas);
#endif 

int AAS_AreaReachability(int areanum);
int AAS_BestReachableArea(vec3_t origin, vec3_t mins, vec3_t maxs, vec3_t goalorigin);
int AAS_BestReachableFromJumpPadArea(vec3_t origin, vec3_t mins, vec3_t maxs);
int AAS_NextModelReachability(int num, int modelnum);
float AAS_AreaGroundFaceArea(int areanum);
int AAS_AreaCrouch(int areanum);
int AAS_AreaSwim(int areanum);
int AAS_AreaLiquid(int areanum);
int AAS_AreaLava(int areanum);
int AAS_AreaSlime(int areanum);
int AAS_AreaGrounded(int areanum);
int AAS_AreaLadder(int areanum);
int AAS_AreaJumpPad(int areanum);
int AAS_AreaDoNotEnter(int areanum);
