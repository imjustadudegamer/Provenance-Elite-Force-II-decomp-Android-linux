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


void AAS_ClearShownDebugLines(void);
void AAS_ClearShownPolygons(void);
void AAS_DebugLine(vec3_t start, vec3_t end, int color);
void AAS_PermanentLine(vec3_t start, vec3_t end, int color);
void AAS_DrawPermanentCross(vec3_t origin, float size, int color);
void AAS_DrawPlaneCross(vec3_t point, vec3_t normal, float dist, int type, int color);
void AAS_ShowBoundingBox(vec3_t origin, vec3_t mins, vec3_t maxs);
void AAS_ShowFace(int facenum);
void AAS_ShowArea(int areanum, int groundfacesonly);
void AAS_ShowAreaPolygons(int areanum, int color, int groundfacesonly);
void AAS_DrawCross(vec3_t origin, float size, int color);
void AAS_PrintTravelType(int traveltype);
void AAS_DrawArrow(vec3_t start, vec3_t end, int linecolor, int arrowcolor);
void AAS_ShowReachability(struct aas_reachability_s *reach);
void AAS_ShowReachableAreas(int areanum);

