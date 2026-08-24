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
#include "../qcommon/qcommon.h"  
#include "l_memory.h"
#include "l_libvar.h"
#include "l_utils.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "l_log.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_aas_funcs.h"
#include "be_interface.h"
#include "be_aas_def.h"

aas_t aasworld;

libvar_t *saveroutingcache;

void QDECL AAS_Error(char *fmt, ...)
{
	char str[1024];
	va_list arglist;

	va_start(arglist, fmt);
	Q_vsnprintf(str, sizeof(str), fmt, arglist);
	va_end(arglist);
	botimport.Print(PRT_FATAL, "%s", str);
} 
int AAS_Loaded(void)
{
	return aasworld.loaded;
} 
int AAS_Initialized(void)
{
	return aasworld.initialized;
} 
void AAS_SetInitialized(void)
{
	aasworld.initialized = qtrue;
	botimport.Print(PRT_MESSAGE, "AAS initialized.\n");
#ifdef DEBUG
#endif
} 
void AAS_ContinueInit(float time)
{
	if (!aasworld.loaded) return;
	if (aasworld.initialized) return;
	if (AAS_ContinueInitReachability(time)) return;
	AAS_InitClustering();
	if (aasworld.savefile || ((int)LibVarGetValue("forcewrite")))
	{
		if ((int)LibVarValue("aasoptimize", "0")) AAS_Optimize();
		if (AAS_WriteAASFile(aasworld.filename))
		{
			botimport.Print(PRT_MESSAGE, "%s written succesfully\n", aasworld.filename);
		} 
		else
		{
			botimport.Print(PRT_ERROR, "couldn't write %s\n", aasworld.filename);
		} 
	} 
	AAS_InitRouting();
	AAS_SetInitialized();
} 
int AAS_StartFrame(float time)
{
	aasworld.time = time;
	AAS_UnlinkInvalidEntities();
	AAS_InvalidateEntities();
	AAS_ContinueInit(time);
	aasworld.frameroutingupdates = 0;
	if (botDeveloper)
	{
		if (LibVarGetValue("showcacheupdates"))
		{
			AAS_RoutingInfo();
			LibVarSet("showcacheupdates", "0");
		} 
		if (LibVarGetValue("showmemoryusage"))
		{
			PrintUsedMemorySize();
			LibVarSet("showmemoryusage", "0");
		} 
		if (LibVarGetValue("memorydump"))
		{
			PrintMemoryLabels();
			LibVarSet("memorydump", "0");
		} 
	} 
	if (saveroutingcache->value)
	{
		AAS_WriteRouteCache();
		LibVarSet("saveroutingcache", "0");
	} 
	aasworld.numframes++;
	LIMIT_RouteFrame();
	return BLERR_NOERROR;
} 
float AAS_Time(void)
{
	return aasworld.time;
} 
void AAS_ProjectPointOntoVector( vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj )
{
	vec3_t pVec, vec;

	VectorSubtract( point, vStart, pVec );
	VectorSubtract( vEnd, vStart, vec );
	VectorNormalize( vec );
	VectorMA( vStart, DotProduct( pVec, vec ), vec, vProj );
} 
int AAS_LoadFiles(const char *mapname)
{
	int errnum;
	char aasfile[MAX_QPATH];

	Q_strncpyz(aasworld.mapname, mapname, sizeof(aasworld.mapname));
	AAS_ResetEntityLinks();
	AAS_LoadBSPFile();

	Com_sprintf(aasfile, sizeof(aasfile), "maps/%s.aas", mapname);
	errnum = AAS_LoadAASFile(aasfile);
	if (errnum != BLERR_NOERROR)
		return errnum;

	botimport.Print(PRT_MESSAGE, "loaded %s\n", aasfile);
	Q_strncpyz(aasworld.filename, aasfile, sizeof(aasworld.filename));
	return BLERR_NOERROR;
} 
int AAS_LoadMap(const char *mapname)
{
	int	errnum;

	if (!mapname)
	{
		return 0;
	} 
	aasworld.initialized = qfalse;
	AAS_FreeRoutingCaches();
	errnum = AAS_LoadFiles(mapname);
	if (errnum != BLERR_NOERROR)
	{
		aasworld.loaded = qfalse;
		return errnum;
	} 
	AAS_InitSettings();
	AAS_InitAASLinkHeap();
	AAS_InitAASLinkedEntities();
	AAS_InitReachability();
	AAS_InitAlternativeRouting();
	return 0;
} 
cvar_t *aas_sv_maxspeed;
cvar_t *aas_sv_jumpvelocity;
cvar_t *aas_sv_gravity;

int AAS_Setup(void)
{
	if (!aas_sv_maxspeed)     aas_sv_maxspeed     = Cvar_Get("sv_maxspeed",     "400", 0);
	if (!aas_sv_jumpvelocity) aas_sv_jumpvelocity = Cvar_Get("sv_jumpvelocity", "325", 0);
	if (!aas_sv_gravity)      aas_sv_gravity      = Cvar_Get("sv_gravity",      "800", 0);

	aasworld.maxclients = (int) LibVarValue("maxclients", "128");
	aasworld.maxentities = (int) LibVarValue("maxentities", "1024");
	saveroutingcache = LibVar("saveroutingcache", "0");
	if (aasworld.entities) FreeMemory(aasworld.entities);
	aasworld.entities = (aas_entity_t *) GetClearedHunkMemory(aasworld.maxentities * sizeof(aas_entity_t));
	AAS_InvalidateEntities();
	aasworld.numframes = 0;
	return BLERR_NOERROR;
} 
void AAS_Shutdown(void)
{
	AAS_ShutdownAlternativeRouting();
	AAS_DumpBSPData();
	AAS_FreeRoutingCaches();
	AAS_FreeAASLinkHeap();
	AAS_FreeAASLinkedEntities();
	AAS_DumpAASData();
	if (aasworld.entities) FreeMemory(aasworld.entities);
	Com_Memset(&aasworld, 0, sizeof(aas_t));
	aasworld.initialized = qfalse;
	botimport.Print(PRT_MESSAGE, "AAS shutdown.\n");
} 
