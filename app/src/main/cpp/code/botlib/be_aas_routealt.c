

#include "../qcommon/q_shared.h"
#include "l_utils.h"
#include "l_memory.h"
#include "l_log.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_aas_funcs.h"
#include "be_interface.h"
#include "be_aas_def.h"

#define ENABLE_ALTROUTING

typedef struct midrangearea_s
{
	int valid;
	unsigned short starttime;
	unsigned short goaltime;
} midrangearea_t;

midrangearea_t *midrangeareas;
int *clusterareas;
int numclusterareas;

void AAS_AltRoutingFloodCluster_r(int areanum)
{
	int i, otherareanum;
	aas_area_t *area;
	aas_face_t *face;

	clusterareas[numclusterareas] = areanum;
	numclusterareas++;
	midrangeareas[areanum].valid = qfalse;
	area = &aasworld.areas[areanum];
	for (i = 0; i < area->numfaces; i++)
	{
		face = &aasworld.faces[abs(aasworld.faceindex[area->firstface + i])];
		if (face->frontarea == areanum) otherareanum = face->backarea;
		else otherareanum = face->frontarea;
		if (!otherareanum) continue;
		if (!midrangeareas[otherareanum].valid) continue;
		AAS_AltRoutingFloodCluster_r(otherareanum);
	} 
} 
int AAS_AlternativeRouteGoals(vec3_t start, int startareanum, vec3_t goal, int goalareanum, int travelflags,
										 aas_altroutegoal_t *altroutegoals, int maxaltroutegoals,
										 int type)
{
#ifndef ENABLE_ALTROUTING
	return 0;
#else
	int i, j, bestareanum;
	int numaltroutegoals, nummidrangeareas;
	int starttime, goaltime, goaltraveltime;
	float dist, bestdist;
	vec3_t mid, dir;
#ifdef ALTROUTE_DEBUG
	int startmillisecs;

	startmillisecs = Sys_MilliSeconds();
#endif

	if (!startareanum || !goalareanum)
		return 0;
	goaltraveltime = AAS_AreaTravelTimeToGoalArea(startareanum, start, goalareanum, travelflags);
	Com_Memset(midrangeareas, 0, aasworld.numareas * sizeof(midrangearea_t));
	numaltroutegoals = 0;
	nummidrangeareas = 0;
	for (i = 1; i < aasworld.numareas; i++)
	{
		if (!(type & ALTROUTEGOAL_ALL))
		{
			if (!(type & ALTROUTEGOAL_CLUSTERPORTALS && (aasworld.areasettings[i].contents & AREACONTENTS_CLUSTERPORTAL)))
			{
				if (!(type & ALTROUTEGOAL_VIEWPORTALS && (aasworld.areasettings[i].contents & AREACONTENTS_VIEWPORTAL)))
				{
					continue;
				} 
			} 
		} 
		if (!AAS_AreaReachability(i)) continue;
		starttime = AAS_AreaTravelTimeToGoalArea(startareanum, start, i, travelflags);
		if (!starttime) continue;
		if (starttime > (float) 1.1 * goaltraveltime) continue;
		goaltime = AAS_AreaTravelTimeToGoalArea(i, NULL, goalareanum, travelflags);
		if (!goaltime) continue;
		if (goaltime > (float) 0.8 * goaltraveltime) continue;
		midrangeareas[i].valid = qtrue;
		midrangeareas[i].starttime = starttime;
		midrangeareas[i].goaltime = goaltime;
		Log_Write("%d midrange area %d", nummidrangeareas, i);
		nummidrangeareas++;
	} 
	for (i = 1; i < aasworld.numareas; i++)
	{
		if (!midrangeareas[i].valid) continue;
		numclusterareas = 0;
		AAS_AltRoutingFloodCluster_r(i);
		VectorClear(mid);
		for (j = 0; j < numclusterareas; j++)
		{
			VectorAdd(mid, aasworld.areas[clusterareas[j]].center, mid);
		} 
		VectorScale(mid, 1.0 / numclusterareas, mid);
		bestdist = 999999;
		bestareanum = 0;
		for (j = 0; j < numclusterareas; j++)
		{
			VectorSubtract(mid, aasworld.areas[clusterareas[j]].center, dir);
			dist = VectorLength(dir);
			if (dist < bestdist)
			{
				bestdist = dist;
				bestareanum = clusterareas[j];
			} 
		} 
		VectorCopy(aasworld.areas[bestareanum].center, altroutegoals[numaltroutegoals].origin);
		altroutegoals[numaltroutegoals].areanum = bestareanum;
		altroutegoals[numaltroutegoals].starttraveltime = midrangeareas[bestareanum].starttime;
		altroutegoals[numaltroutegoals].goaltraveltime = midrangeareas[bestareanum].goaltime;
		altroutegoals[numaltroutegoals].extratraveltime =
					(midrangeareas[bestareanum].starttime + midrangeareas[bestareanum].goaltime) -
								goaltraveltime;
		numaltroutegoals++;
#ifdef ALTROUTE_DEBUG
		AAS_ShowAreaPolygons(bestareanum, 1, qtrue);
#endif
		if (numaltroutegoals >= maxaltroutegoals) break;
	} 
#ifdef ALTROUTE_DEBUG
	botimport.Print(PRT_MESSAGE, "alternative route goals in %d msec\n", Sys_MilliSeconds() - startmillisecs);
#endif
	return numaltroutegoals;
#endif
} 
void AAS_InitAlternativeRouting(void)
{
#ifdef ENABLE_ALTROUTING
	if (midrangeareas) FreeMemory(midrangeareas);
	midrangeareas = (midrangearea_t *) GetMemory(aasworld.numareas * sizeof(midrangearea_t));
	if (clusterareas) FreeMemory(clusterareas);
	clusterareas = (int *) GetMemory(aasworld.numareas * sizeof(int));
#endif
} 
void AAS_ShutdownAlternativeRouting(void)
{
#ifdef ENABLE_ALTROUTING
	if (midrangeareas) FreeMemory(midrangeareas);
	midrangeareas = NULL;
	if (clusterareas) FreeMemory(clusterareas);
	clusterareas = NULL;
	numclusterareas = 0;
#endif
} 
