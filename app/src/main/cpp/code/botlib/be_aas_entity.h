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
void AAS_InvalidateEntities(void);
void AAS_UnlinkInvalidEntities(void);
void AAS_ResetEntityLinks(void);
int AAS_UpdateEntity(int ent, bot_entitystate_t *state);
void AAS_EntityBSPData(int entnum, bsp_entdata_t *entdata);
#endif 

void AAS_EntitySize(int entnum, vec3_t mins, vec3_t maxs);
int AAS_EntityModelNum(int entnum);
int AAS_OriginOfMoverWithModelNum(int modelnum, vec3_t origin);
int AAS_BestReachableEntityArea(int entnum);
void AAS_EntityInfo(int entnum, aas_entityinfo_t *info);
int AAS_NextEntity(int entnum);
void AAS_EntityOrigin(int entnum, vec3_t origin);
int AAS_EntityType(int entnum);
int AAS_EntityModelindex(int entnum);

