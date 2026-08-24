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
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "l_log.h"
#include "l_memory.h"
#include "l_libvar.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_aas_funcs.h"
#include "be_aas_def.h"
#include "be_aas_cluster.h"

extern botlib_import_t botimport;

#define AAS_MAX_PORTALS					65536
#define AAS_MAX_PORTALINDEXSIZE			65536
#define AAS_MAX_CLUSTERS				65536
#define MAX_PORTALAREAS			1024

int nofaceflood = qtrue;

void AAS_RemoveClusterAreas(void)
{
	int i;

	for (i = 1; i < aasworld.numareas; i++)
	{
		aasworld.areasettings[i].cluster = 0;
	} 
} 
void AAS_ClearCluster(int clusternum)
{
	int i;

	for (i = 1; i < aasworld.numareas; i++)
	{
		if (aasworld.areasettings[i].cluster == clusternum)
		{
			aasworld.areasettings[i].cluster = 0;
		} 
	} 
} 
void AAS_RemovePortalsClusterReference(int clusternum)
{
	int portalnum;

	for (portalnum = 1; portalnum < aasworld.numportals; portalnum++)
	{
		if (aasworld.portals[portalnum].frontcluster == clusternum)
		{
			aasworld.portals[portalnum].frontcluster = 0;
		} 
		if (aasworld.portals[portalnum].backcluster == clusternum)
		{
			aasworld.portals[portalnum].backcluster = 0;
		} 
	} 
} 
int AAS_UpdatePortal(int areanum, int clusternum)
{
	int portalnum;
	aas_portal_t *portal;
	aas_cluster_t *cluster;

	for (portalnum = 1; portalnum < aasworld.numportals; portalnum++)
	{
		if (aasworld.portals[portalnum].areanum == areanum) break;
	} 
	if (portalnum == aasworld.numportals)
	{
		AAS_Error("no portal of area %d\n", areanum);
		return qtrue;
	} 
	portal = &aasworld.portals[portalnum];
	if (portal->frontcluster == clusternum) return qtrue;
	if (portal->backcluster == clusternum) return qtrue;
	if (!portal->frontcluster)
	{
		portal->frontcluster = clusternum;
	} 
	else if (!portal->backcluster)
	{
		portal->backcluster = clusternum;
	} 
	else
	{
		aasworld.areasettings[areanum].contents &= ~AREACONTENTS_CLUSTERPORTAL;
		Log_Write("portal area %d is separating more than two clusters\r\n", areanum);
		return qfalse;
	} 
	if (aasworld.portalindexsize >= AAS_MAX_PORTALINDEXSIZE)
	{
		AAS_Error("AAS_MAX_PORTALINDEXSIZE\n");
		return qtrue;
	} 
	aasworld.areasettings[areanum].cluster = -portalnum;
	cluster = &aasworld.clusters[clusternum];
	aasworld.portalindex[cluster->firstportal + cluster->numportals] = portalnum;
	aasworld.portalindexsize++;
	cluster->numportals++;
	return qtrue;
} 
int AAS_FloodClusterAreas_r(int areanum, int clusternum)
{
	aas_area_t *area;
	aas_face_t *face;
	int facenum, i;

	if (areanum <= 0 || areanum >= aasworld.numareas)
	{
		AAS_Error("AAS_FloodClusterAreas_r: areanum out of range\n");
		return qfalse;
	} 
	if (aasworld.areasettings[areanum].cluster > 0)
	{
		if (aasworld.areasettings[areanum].cluster == clusternum) return qtrue;
		AAS_Error("cluster %d touched cluster %d at area %d\n",
				clusternum, aasworld.areasettings[areanum].cluster, areanum);
		return qfalse;
	} 
	if (aasworld.areasettings[areanum].contents & AREACONTENTS_CLUSTERPORTAL)
	{
		return AAS_UpdatePortal(areanum, clusternum);
	} 
	aasworld.areasettings[areanum].cluster = clusternum;
	aasworld.areasettings[areanum].clusterareanum =
				aasworld.clusters[clusternum].numareas;
	aasworld.clusters[clusternum].numareas++;

	area = &aasworld.areas[areanum];
	if (!nofaceflood)
	{
		for (i = 0; i < area->numfaces; i++)
		{
			facenum = abs(aasworld.faceindex[area->firstface + i]);
			face = &aasworld.faces[facenum];
			if (face->frontarea == areanum)
			{
				if (face->backarea) if (!AAS_FloodClusterAreas_r(face->backarea, clusternum)) return qfalse;
			} 
			else
			{
				if (face->frontarea) if (!AAS_FloodClusterAreas_r(face->frontarea, clusternum)) return qfalse;
			} 
		} 
	} 
	for (i = 0; i < aasworld.areasettings[areanum].numreachableareas; i++)
	{
		if (!aasworld.reachability[
					aasworld.areasettings[areanum].firstreachablearea + i].areanum)
		{
			continue;
		} 
		if (!AAS_FloodClusterAreas_r(aasworld.reachability[
				aasworld.areasettings[areanum].firstreachablearea + i].areanum, clusternum)) return qfalse;
	} 
	return qtrue;
} 
int AAS_FloodClusterAreasUsingReachabilities(int clusternum)
{
	int i, j, areanum;

	for (i = 1; i < aasworld.numareas; i++)
	{
		if (aasworld.areasettings[i].cluster)
			continue;
		if (aasworld.areasettings[i].contents & AREACONTENTS_CLUSTERPORTAL)
			continue;
		for (j = 0; j < aasworld.areasettings[i].numreachableareas; j++)
		{
			areanum = aasworld.reachability[aasworld.areasettings[i].firstreachablearea + j].areanum;
			if (aasworld.areasettings[areanum].contents & AREACONTENTS_CLUSTERPORTAL)
				continue;
			if (aasworld.areasettings[areanum].cluster)
			{
				if (!AAS_FloodClusterAreas_r(i, clusternum))
					return qfalse;
				i = 0;
				break;
			} 
		} 
	} 
	return qtrue;
} 
void AAS_NumberClusterPortals(int clusternum)
{
	int i, portalnum;
	aas_cluster_t *cluster;
	aas_portal_t *portal;

	cluster = &aasworld.clusters[clusternum];
	for (i = 0; i < cluster->numportals; i++)
	{
		portalnum = aasworld.portalindex[cluster->firstportal + i];
		portal = &aasworld.portals[portalnum];
		if (portal->frontcluster == clusternum)
		{
			portal->clusterareanum[0] = cluster->numareas++;
		} 
		else
		{
			portal->clusterareanum[1] = cluster->numareas++;
		} 
	} 
} 
void AAS_NumberClusterAreas(int clusternum)
{
	int i, portalnum;
	aas_cluster_t *cluster;
	aas_portal_t *portal;

	aasworld.clusters[clusternum].numareas = 0;
	aasworld.clusters[clusternum].numreachabilityareas = 0;
	for (i = 1; i < aasworld.numareas; i++)
	{
		if (aasworld.areasettings[i].cluster != clusternum) continue;
		if (!AAS_AreaReachability(i)) continue;
		aasworld.areasettings[i].clusterareanum = aasworld.clusters[clusternum].numareas;
		aasworld.clusters[clusternum].numareas++;
		aasworld.clusters[clusternum].numreachabilityareas++;
	} 
	cluster = &aasworld.clusters[clusternum];
	for (i = 0; i < cluster->numportals; i++)
	{
		portalnum = aasworld.portalindex[cluster->firstportal + i];
		portal = &aasworld.portals[portalnum];
		if (!AAS_AreaReachability(portal->areanum)) continue;
		if (portal->frontcluster == clusternum)
		{
			portal->clusterareanum[0] = cluster->numareas++;
			aasworld.clusters[clusternum].numreachabilityareas++;
		} 
		else
		{
			portal->clusterareanum[1] = cluster->numareas++;
			aasworld.clusters[clusternum].numreachabilityareas++;
		} 
	} 
	for (i = 1; i < aasworld.numareas; i++)
	{
		if (aasworld.areasettings[i].cluster != clusternum) continue;
		if (AAS_AreaReachability(i)) continue;
		aasworld.areasettings[i].clusterareanum = aasworld.clusters[clusternum].numareas;
		aasworld.clusters[clusternum].numareas++;
	} 
	cluster = &aasworld.clusters[clusternum];
	for (i = 0; i < cluster->numportals; i++)
	{
		portalnum = aasworld.portalindex[cluster->firstportal + i];
		portal = &aasworld.portals[portalnum];
		if (AAS_AreaReachability(portal->areanum)) continue;
		if (portal->frontcluster == clusternum)
		{
			portal->clusterareanum[0] = cluster->numareas++;
		} 
		else
		{
			portal->clusterareanum[1] = cluster->numareas++;
		} 
	} 
} 
int AAS_FindClusters(void)
{
	int i;
	aas_cluster_t *cluster;

	AAS_RemoveClusterAreas();
	for (i = 1; i < aasworld.numareas; i++)
	{
		if (aasworld.areasettings[i].cluster)
			continue;
		if (nofaceflood)
		{
			if (!aasworld.areasettings[i].numreachableareas)
				continue;
		} 
		if (aasworld.areasettings[i].contents & AREACONTENTS_CLUSTERPORTAL)
			continue;
		if (aasworld.numclusters >= AAS_MAX_CLUSTERS)
		{
			AAS_Error("AAS_MAX_CLUSTERS\n");
			return qfalse;
		} 
		cluster = &aasworld.clusters[aasworld.numclusters];
		cluster->numareas = 0;
		cluster->numreachabilityareas = 0;
		cluster->firstportal = aasworld.portalindexsize;
		cluster->numportals = 0;
		if (!AAS_FloodClusterAreas_r(i, aasworld.numclusters))
			return qfalse;
		if (!AAS_FloodClusterAreasUsingReachabilities(aasworld.numclusters))
			return qfalse;
		AAS_NumberClusterAreas(aasworld.numclusters);
		aasworld.numclusters++;
	} 
	return qtrue;
} 
void AAS_CreatePortals(void)
{
	int i;
	aas_portal_t *portal;

	for (i = 1; i < aasworld.numareas; i++)
	{
		if (aasworld.areasettings[i].contents & AREACONTENTS_CLUSTERPORTAL)
		{
			if (aasworld.numportals >= AAS_MAX_PORTALS)
			{
				AAS_Error("AAS_MAX_PORTALS\n");
				return;
			} 
			portal = &aasworld.portals[aasworld.numportals];
			portal->areanum = i;
			portal->frontcluster = 0;
			portal->backcluster = 0;
			aasworld.numportals++;
		} 
	} 
} 
void AAS_ConnectedAreas_r(int *areanums, int numareas, int *connectedareas, int curarea)
{
	int i, j, otherareanum, facenum;
	aas_area_t *area;
	aas_face_t *face;

	connectedareas[curarea] = qtrue;
	area = &aasworld.areas[areanums[curarea]];
	for (i = 0; i < area->numfaces; i++)
	{
		facenum = abs(aasworld.faceindex[area->firstface + i]);
		face = &aasworld.faces[facenum];
		if (face->faceflags & FACE_SOLID) continue;
		if (face->frontarea != areanums[curarea]) otherareanum = face->frontarea;
		else otherareanum = face->backarea;
		for (j = 0; j < numareas; j++)
		{
			if (areanums[j] == otherareanum) break;
		} 
		if (j == numareas) continue;
		if (connectedareas[j]) continue;
		AAS_ConnectedAreas_r(areanums, numareas, connectedareas, j);
	} 
} 
qboolean AAS_ConnectedAreas(int *areanums, int numareas)
{
	int connectedareas[MAX_PORTALAREAS], i;

	Com_Memset(connectedareas, 0, sizeof(connectedareas));
	if (numareas < 1) return qfalse;
	if (numareas == 1) return qtrue;
	AAS_ConnectedAreas_r(areanums, numareas, connectedareas, 0);
	for (i = 0; i < numareas; i++)
	{
		if (!connectedareas[i]) return qfalse;
	} 
	return qtrue;
} 
int AAS_GetAdjacentAreasWithLessPresenceTypes_r(int *areanums, int numareas, int curareanum)
{
	int i, j, presencetype, otherpresencetype, otherareanum, facenum;
	aas_area_t *area;
	aas_face_t *face;

	areanums[numareas++] = curareanum;
	area = &aasworld.areas[curareanum];
	presencetype = aasworld.areasettings[curareanum].presencetype;
	for (i = 0; i < area->numfaces; i++)
	{
		facenum = abs(aasworld.faceindex[area->firstface + i]);
		face = &aasworld.faces[facenum];
		if (face->faceflags & FACE_SOLID) continue;
		if (face->frontarea != curareanum) otherareanum = face->frontarea;
		else otherareanum = face->backarea;
		otherpresencetype = aasworld.areasettings[otherareanum].presencetype;
		if ((presencetype & ~otherpresencetype) &&
				!(otherpresencetype & ~presencetype))
		{
			for (j = 0; j < numareas; j++)
			{
				if (otherareanum == areanums[j]) break;
			} 
			if (j == numareas)
			{
				if (numareas >= MAX_PORTALAREAS)
				{
					AAS_Error("MAX_PORTALAREAS\n");
					return numareas;
				} 
				numareas = AAS_GetAdjacentAreasWithLessPresenceTypes_r(areanums, numareas, otherareanum);
			} 
		} 
	} 
	return numareas;
} 
int AAS_CheckAreaForPossiblePortals(int areanum)
{
	int i, j, k, fen, ben, frontedgenum, backedgenum, facenum;
	int areanums[MAX_PORTALAREAS], numareas, otherareanum;
	int numareafrontfaces[MAX_PORTALAREAS], numareabackfaces[MAX_PORTALAREAS];
	int frontfacenums[MAX_PORTALAREAS], backfacenums[MAX_PORTALAREAS];
	int numfrontfaces, numbackfaces;
	int frontareanums[MAX_PORTALAREAS], backareanums[MAX_PORTALAREAS];
	int numfrontareas, numbackareas;
	int frontplanenum, backplanenum, faceplanenum;
	aas_area_t *area;
	aas_face_t *frontface, *backface, *face;

	if (aasworld.areasettings[areanum].contents & AREACONTENTS_CLUSTERPORTAL) return 0;
	if (!(aasworld.areasettings[areanum].areaflags & AREA_GROUNDED)) return 0;
	Com_Memset(numareafrontfaces, 0, sizeof(numareafrontfaces));
	Com_Memset(numareabackfaces, 0, sizeof(numareabackfaces));
	numfrontfaces = numbackfaces = 0;
	numfrontareas = numbackareas = 0;
	frontplanenum = backplanenum = -1;
	numareas = AAS_GetAdjacentAreasWithLessPresenceTypes_r(areanums, 0, areanum);
	for (i = 0; i < numareas; i++)
	{
		area = &aasworld.areas[areanums[i]];
		for (j = 0; j < area->numfaces; j++)
		{
			facenum = abs(aasworld.faceindex[area->firstface + j]);
			face = &aasworld.faces[facenum];
			if (face->faceflags & FACE_SOLID) continue;
			for (k = 0; k < numareas; k++)
			{
				if (k == i) continue;
				if (face->frontarea == areanums[k] || face->backarea == areanums[k]) break;
			} 
			if (k != numareas) continue;
			if (face->frontarea == areanums[i]) otherareanum = face->backarea;
			else otherareanum = face->frontarea;
			if (aasworld.areasettings[otherareanum].contents & AREACONTENTS_CLUSTERPORTAL) return 0;
			faceplanenum = face->planenum & ~1;
			if (frontplanenum < 0 || faceplanenum == frontplanenum)
			{
				frontplanenum = faceplanenum;
				frontfacenums[numfrontfaces++] = facenum;
				for (k = 0; k < numfrontareas; k++)
				{
					if (frontareanums[k] == otherareanum) break;
				} 
				if (k == numfrontareas) frontareanums[numfrontareas++] = otherareanum;
				numareafrontfaces[i]++;
			} 
			else if (backplanenum < 0 || faceplanenum == backplanenum)
			{
				backplanenum = faceplanenum;
				backfacenums[numbackfaces++] = facenum;
				for (k = 0; k < numbackareas; k++)
				{
					if (backareanums[k] == otherareanum) break;
				} 
				if (k == numbackareas) backareanums[numbackareas++] = otherareanum;
				numareabackfaces[i]++;
			} 
			else
			{
				return 0;
			} 
		} 
	} 
	for (i = 0; i < numareas; i++)
	{
		if (!numareafrontfaces[i] || !numareabackfaces[i]) return 0;
	} 
	if (!AAS_ConnectedAreas(frontareanums, numfrontareas)) return 0;
	if (!AAS_ConnectedAreas(backareanums, numbackareas)) return 0;
	for (i = 0; i < numfrontfaces; i++)
	{
		frontface = &aasworld.faces[frontfacenums[i]];
		for (fen = 0; fen < frontface->numedges; fen++)
		{
			frontedgenum = abs(aasworld.edgeindex[frontface->firstedge + fen]);
			for (j = 0; j < numbackfaces; j++)
			{
				backface = &aasworld.faces[backfacenums[j]];
				for (ben = 0; ben < backface->numedges; ben++)
				{
					backedgenum = abs(aasworld.edgeindex[backface->firstedge + ben]);
					if (frontedgenum == backedgenum) break;
				} 
				if (ben != backface->numedges) break;
			} 
			if (j != numbackfaces) break;
		} 
		if (fen != frontface->numedges) break;
	} 
	if (i != numfrontfaces) return 0;
	for (i = 0; i < numareas; i++)
	{
		aasworld.areasettings[areanums[i]].contents |= AREACONTENTS_CLUSTERPORTAL;
		aasworld.areasettings[areanums[i]].contents |= AREACONTENTS_ROUTEPORTAL;
		Log_Write("possible portal: %d\r\n", areanums[i]);
	} 
	return numareas;
} 
void AAS_FindPossiblePortals(void)
{
	int i, numpossibleportals;

	numpossibleportals = 0;
	for (i = 1; i < aasworld.numareas; i++)
	{
		numpossibleportals += AAS_CheckAreaForPossiblePortals(i);
	} 
	botimport.Print(PRT_MESSAGE, "\r%6d possible portal areas\n", numpossibleportals);
} 
void AAS_RemoveAllPortals(void)
{
	int i;

	for (i = 1; i < aasworld.numareas; i++)
	{
		aasworld.areasettings[i].contents &= ~AREACONTENTS_CLUSTERPORTAL;
	} 
} 

#if 0
void AAS_FloodCluster_r(int areanum, int clusternum)
{
	int i, otherareanum;
	aas_face_t *face;
	aas_area_t *area;

	aasworld.areasettings[areanum].cluster = clusternum;
	area = &aasworld.areas[areanum];
	for (i = 0; i < area->numfaces; i++)
	{
		face = &aasworld.faces[abs(aasworld.faceindex[area->firstface + i])];
		if (face->frontarea != areanum) otherareanum = face->frontarea;
		else otherareanum = face->backarea;
		if (!otherareanum) continue;
		if (aasworld.areasettings[otherareanum].contents & AREACONTENTS_CLUSTERPORTAL) continue;
		if (aasworld.areasettings[otherareanum].cluster) continue;
		AAS_FloodCluster_r(otherareanum, clusternum);
	} 
	for (i = 0; i < aasworld.areasettings[areanum].numreachableareas; i++)
	{
		otherareanum = aasworld.reachability[
					aasworld.areasettings[areanum].firstreachablearea + i].areanum;
		if (!otherareanum)
		{
			continue;
			AAS_Error("reachability %d has zero area\n", aasworld.areasettings[areanum].firstreachablearea + i);
		} 
		if (aasworld.areasettings[otherareanum].contents & AREACONTENTS_CLUSTERPORTAL) continue;
		if (aasworld.areasettings[otherareanum].cluster) continue;
		AAS_FloodCluster_r(otherareanum, clusternum);
	} 
} 
void AAS_RemoveTeleporterPortals(void)
{
	int i, j, areanum;

	for (i = 1; i < aasworld.numareas; i++)
	{
		for (j = 0; j < aasworld.areasettings[i].numreachableareas; j++)
		{
			areanum = aasworld.reachability[aasworld.areasettings[i].firstreachablearea + j].areanum;
			if (aasworld.reachability[aasworld.areasettings[i].firstreachablearea + j].traveltype == TRAVEL_TELEPORT)
			{
				aasworld.areasettings[i].contents &= ~AREACONTENTS_CLUSTERPORTAL;
				aasworld.areasettings[areanum].contents &= ~AREACONTENTS_CLUSTERPORTAL;
				break;
			} 
		} 
	} 
} 
void AAS_FloodClusterReachabilities(int clusternum)
{
	int i, j, areanum;

	for (i = 1; i < aasworld.numareas; i++)
	{
		if (aasworld.areasettings[i].cluster) continue;
		if (aasworld.areasettings[i].contents & AREACONTENTS_CLUSTERPORTAL) continue;
		for (j = 0; j < aasworld.areasettings[i].numreachableareas; j++)
		{
			areanum = aasworld.reachability[aasworld.areasettings[i].firstreachablearea + j].areanum;
			if (aasworld.areasettings[areanum].contents & AREACONTENTS_CLUSTERPORTAL) continue;
			if (aasworld.areasettings[areanum].cluster == clusternum)
			{
				AAS_FloodCluster_r(i, clusternum);
				i = 0;
				break;
			} 
		} 
	} 
} 

void AAS_RemoveNotClusterClosingPortals(void)
{
	int i, j, k, facenum, otherareanum, nonclosingportals;
	aas_area_t *area;
	aas_face_t *face;

	AAS_RemoveTeleporterPortals();
	nonclosingportals = 0;
	for (i = 1; i < aasworld.numareas; i++)
	{
		if (!(aasworld.areasettings[i].contents & AREACONTENTS_CLUSTERPORTAL)) continue;
		area = &aasworld.areas[i];
		for (j = 0; j < area->numfaces; j++)
		{
			facenum = abs(aasworld.faceindex[area->firstface + j]);
			face = &aasworld.faces[facenum];
			if (face->frontarea != i) otherareanum = face->frontarea;
			else otherareanum = face->backarea;
			if (!otherareanum) continue;
			if (aasworld.areasettings[otherareanum].contents & AREACONTENTS_CLUSTERPORTAL)
			{
				continue;
			} 
			AAS_RemoveClusterAreas();
			AAS_FloodCluster_r(otherareanum, 1);
			AAS_FloodClusterReachabilities(1);
			for (k = 0; k < area->numfaces; k++)
			{
				facenum = abs(aasworld.faceindex[area->firstface + k]);
				face = &aasworld.faces[facenum];
				if (face->frontarea != i) otherareanum = face->frontarea;
				else otherareanum = face->backarea;
				if (!otherareanum) continue;
				if (aasworld.areasettings[otherareanum].contents & AREACONTENTS_CLUSTERPORTAL)
				{
					continue;
				} 
				if (!aasworld.areasettings[otherareanum].cluster) break;
			} 
			if (k >= area->numfaces)
			{
				aasworld.areasettings[i].contents &= ~AREACONTENTS_CLUSTERPORTAL;
				nonclosingportals++;
				i = 0;
				break;
			} 
		} 
	} 
	botimport.Print(PRT_MESSAGE, "\r%6d non closing portals removed\n", nonclosingportals);
} 


void AAS_RemoveNotClusterClosingPortals(void)
{
	int i, j, facenum, otherareanum, nonclosingportals, numseperatedclusters;
	aas_area_t *area;
	aas_face_t *face;

	AAS_RemoveTeleporterPortals();
	nonclosingportals = 0;
	for (i = 1; i < aasworld.numareas; i++)
	{
		if (!(aasworld.areasettings[i].contents & AREACONTENTS_CLUSTERPORTAL)) continue;
		numseperatedclusters = 0;
		AAS_RemoveClusterAreas();
		area = &aasworld.areas[i];
		for (j = 0; j < area->numfaces; j++)
		{
			facenum = abs(aasworld.faceindex[area->firstface + j]);
			face = &aasworld.faces[facenum];
			if (face->frontarea != i) otherareanum = face->frontarea;
			else otherareanum = face->backarea;
			if (!otherareanum) continue;
			if (aasworld.areasettings[otherareanum].contents & AREACONTENTS_CLUSTERPORTAL) continue;
			if (aasworld.areasettings[otherareanum].cluster) continue;
			numseperatedclusters++;
			AAS_FloodCluster_r(otherareanum, numseperatedclusters);
			AAS_FloodClusterReachabilities(numseperatedclusters);
		} 
		for (j = 0; j < aasworld.areasettings[i].numreachableareas; j++)
		{
			otherareanum = aasworld.reachability[
						aasworld.areasettings[i].firstreachablearea + j].areanum;
			if (!otherareanum) continue;
			if (aasworld.areasettings[otherareanum].contents & AREACONTENTS_CLUSTERPORTAL) continue;
			if (aasworld.areasettings[otherareanum].cluster) continue;
			numseperatedclusters++;
			AAS_FloodCluster_r(otherareanum, numseperatedclusters);
			AAS_FloodClusterReachabilities(numseperatedclusters);
		} 
		if (numseperatedclusters != 2)
		{
			aasworld.areasettings[i].contents &= ~AREACONTENTS_CLUSTERPORTAL;
			nonclosingportals++;
			i = 0;
		} 
	} 
	botimport.Print(PRT_MESSAGE, "\r%6d non closing portals removed\n", nonclosingportals);
} 


void AAS_AddTeleporterPortals(void)
{
	int j, area2num, facenum, otherareanum;
	char *target, *targetname, *classname;
	bsp_entity_t *entities, *ent, *dest;
	vec3_t origin, destorigin, mins, maxs, end;
	vec3_t bbmins, bbmaxs;
	aas_area_t *area;
	aas_face_t *face;
	aas_trace_t trace;
	aas_link_t *areas, *link;

	entities = AAS_ParseBSPEntities();

	for (ent = entities; ent; ent = ent->next)
	{
		classname = AAS_ValueForBSPEpairKey(ent, "classname");
		if (classname && !strcmp(classname, "misc_teleporter"))
		{
			if (!AAS_VectorForBSPEpairKey(ent, "origin", origin))
			{
				botimport.Print(PRT_ERROR, "teleporter (%s) without origin\n", target);
				continue;
			} 
			target = AAS_ValueForBSPEpairKey(ent, "target");
			if (!target)
			{
				botimport.Print(PRT_ERROR, "teleporter (%s) without target\n", target);
				continue;
			} 
			for (dest = entities; dest; dest = dest->next)
			{
				classname = AAS_ValueForBSPEpairKey(dest, "classname");
				if (classname && !strcmp(classname, "misc_teleporter_dest"))
				{
					targetname = AAS_ValueForBSPEpairKey(dest, "targetname");
					if (targetname && !strcmp(targetname, target))
					{
						break;
					} 
				} 
			} 
			if (!dest)
			{
				botimport.Print(PRT_ERROR, "teleporter without destination (%s)\n", target);
				continue;
			} 
			if (!AAS_VectorForBSPEpairKey(dest, "origin", destorigin))
			{
				botimport.Print(PRT_ERROR, "teleporter destination (%s) without origin\n", target);
				continue;
			} 
			destorigin[2] += 24; 
			VectorCopy(destorigin, end);
			end[2] -= 100;
			trace = AAS_TraceClientBBox(destorigin, end, PRESENCE_CROUCH, -1);
			if (trace.startsolid)
			{
				botimport.Print(PRT_ERROR, "teleporter destination (%s) in solid\n", target);
				continue;
			} 
			VectorCopy(trace.endpos, destorigin);
			area2num = AAS_PointAreaNum(destorigin);
			for (j = 0; j < aasworld.numareas; j++)
			{
				aasworld.areasettings[j].cluster = 0;
			} 
			VectorSet(mins, -8, -8, 8);
			VectorSet(maxs, 8, 8, 24);
			AAS_PresenceTypeBoundingBox(PRESENCE_CROUCH, bbmins, bbmaxs);
			VectorAdd(origin, mins, mins);
			VectorAdd(origin, maxs, maxs);
			VectorSubtract(mins, bbmaxs, mins);
			VectorSubtract(maxs, bbmins, maxs);
			areas = AAS_AASLinkEntity(mins, maxs, -1);
			for (link = areas; link; link = link->next_area)
			{
				if (!AAS_AreaGrounded(link->areanum)) continue;
				aasworld.areasettings[link->areanum].contents |= AREACONTENTS_CLUSTERPORTAL |
																			AREACONTENTS_TELEPORTAL;
			} 
			for (link = areas; link; link = link->next_area)
			{
				if (!AAS_AreaGrounded(link->areanum)) continue;
				area = &aasworld.areas[link->areanum];
				for (j = 0; j < area->numfaces; j++)
				{
					facenum = abs(aasworld.faceindex[area->firstface + j]);
					face = &aasworld.faces[facenum];
					if (face->frontarea != link->areanum) otherareanum = face->frontarea;
					else otherareanum = face->backarea;
					if (!otherareanum) continue;
					if (aasworld.areasettings[otherareanum].contents & AREACONTENTS_CLUSTERPORTAL)
					{
						continue;
					} 
					AAS_FloodCluster_r(otherareanum, 1);
				} 
			} 
			if (aasworld.areasettings[area2num].cluster)
			{
				for (link = areas; link; link = link->next_area)
				{
					if (!AAS_AreaGrounded(link->areanum)) continue;
					aasworld.areasettings[link->areanum].contents &= ~(AREACONTENTS_CLUSTERPORTAL |
																				AREACONTENTS_TELEPORTAL);
				} 
			} 
		} 
	} 
	AAS_FreeBSPEntities(entities);
} 

void AAS_AddTeleporterPortals(void)
{
	int i, j, areanum;

	for (i = 1; i < aasworld.numareas; i++)
	{
		for (j = 0; j < aasworld.areasettings[i].numreachableareas; j++)
		{
			if (aasworld.reachability[aasworld.areasettings[i].firstreachablearea + j].traveltype != TRAVEL_TELEPORT) continue;
			areanum = aasworld.reachability[aasworld.areasettings[i].firstreachablearea + j].areanum;
			aasworld.areasettings[areanum].contents |= AREACONTENTS_CLUSTERPORTAL;
		} 
	} 
} 

#endif

int AAS_TestPortals(void)
{
	int i;
	aas_portal_t *portal;

	for (i = 1; i < aasworld.numportals; i++)
	{
		portal = &aasworld.portals[i];
		if (!portal->frontcluster)
		{
			aasworld.areasettings[portal->areanum].contents &= ~AREACONTENTS_CLUSTERPORTAL;
			Log_Write("portal area %d has no front cluster\r\n", portal->areanum);
			return qfalse;
		} 
		if (!portal->backcluster)
		{
			aasworld.areasettings[portal->areanum].contents &= ~AREACONTENTS_CLUSTERPORTAL;
			Log_Write("portal area %d has no back cluster\r\n", portal->areanum);
			return qfalse;
		} 
	} 
	return qtrue;
} 
void AAS_CountForcedClusterPortals(void)
{
	int num, i;

	num = 0;
	for (i = 1; i < aasworld.numareas; i++)
	{
		if (aasworld.areasettings[i].contents & AREACONTENTS_CLUSTERPORTAL)
		{
			Log_Write("area %d is a forced portal area\r\n", i);
			num++;
		} 
	} 
	botimport.Print(PRT_MESSAGE, "%6d forced portal areas\n", num);
} 
void AAS_CreateViewPortals(void)
{
	int i;

	for (i = 1; i < aasworld.numareas; i++)
	{
		if (aasworld.areasettings[i].contents & AREACONTENTS_CLUSTERPORTAL)
		{
			aasworld.areasettings[i].contents |= AREACONTENTS_VIEWPORTAL;
		} 
	} 
} 
void AAS_SetViewPortalsAsClusterPortals(void)
{
	int i;

	for (i = 1; i < aasworld.numareas; i++)
	{
		if (aasworld.areasettings[i].contents & AREACONTENTS_VIEWPORTAL)
		{
			aasworld.areasettings[i].contents |= AREACONTENTS_CLUSTERPORTAL;
		} 
	} 
} 
void AAS_InitClustering(void)
{
	int i, removedPortalAreas;
	int n, total, numreachabilityareas;

	if (!aasworld.loaded) return;
	if (aasworld.numclusters >= 1)
	{
#ifndef BSPC
		if (!((int)LibVarGetValue("forceclustering")) &&
			!((int)LibVarGetValue("forcereachability"))) return;
#endif
	} 
	AAS_SetViewPortalsAsClusterPortals();
	AAS_CountForcedClusterPortals();
	AAS_RemoveClusterAreas();
	AAS_FindPossiblePortals();
	AAS_CreateViewPortals();
	if (aasworld.portals) FreeMemory(aasworld.portals);
	aasworld.portals = (aas_portal_t *) GetClearedMemory(AAS_MAX_PORTALS * sizeof(aas_portal_t));
	if (aasworld.portalindex) FreeMemory(aasworld.portalindex);
	aasworld.portalindex = (aas_portalindex_t *) GetClearedMemory(AAS_MAX_PORTALINDEXSIZE * sizeof(aas_portalindex_t));
	if (aasworld.clusters) FreeMemory(aasworld.clusters);
	aasworld.clusters = (aas_cluster_t *) GetClearedMemory(AAS_MAX_CLUSTERS * sizeof(aas_cluster_t));
	removedPortalAreas = 0;
	botimport.Print(PRT_MESSAGE, "\r%6d removed portal areas", removedPortalAreas);
	while(1)
	{
		botimport.Print(PRT_MESSAGE, "\r%6d", removedPortalAreas);
		aasworld.numportals = 1;		
		aasworld.portalindexsize = 0;
		aasworld.numclusters = 1;		
		AAS_CreatePortals();
		removedPortalAreas++;
		if (!AAS_FindClusters())
			continue;
		if (!AAS_TestPortals())
			continue;
		break;
	} 
	botimport.Print(PRT_MESSAGE, "\n");
	aasworld.savefile = qtrue;
	for (i = 1; i < aasworld.numportals; i++)
	{
		Log_Write("portal %d: area %d\r\n", i, aasworld.portals[i].areanum);
	} 
	botimport.Print(PRT_MESSAGE, "%6d portals created\n", aasworld.numportals);
	botimport.Print(PRT_MESSAGE, "%6d clusters created\n", aasworld.numclusters);
	for (i = 1; i < aasworld.numclusters; i++)
	{
		botimport.Print(PRT_MESSAGE, "cluster %d has %d reachability areas\n", i,
				aasworld.clusters[i].numreachabilityareas);
	} 
	numreachabilityareas = 0;
	total = 0;
	for (i = 0; i < aasworld.numclusters; i++) {
		n = aasworld.clusters[i].numreachabilityareas;
		numreachabilityareas += n;
		total += n * n;
	}
	total += numreachabilityareas * aasworld.numportals;
	botimport.Print(PRT_MESSAGE, "%6i total reachability areas\n", numreachabilityareas);
	botimport.Print(PRT_MESSAGE, "%6i AAS memory/CPU usage (the lower the better)\n", total * 3);
} 
