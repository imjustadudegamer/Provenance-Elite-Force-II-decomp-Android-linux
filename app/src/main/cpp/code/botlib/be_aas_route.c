

#include "../qcommon/q_shared.h"
#include "l_utils.h"
#include "l_memory.h"
#include "l_log.h"
#include "l_crc.h"
#include "l_libvar.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_aas_funcs.h"
#include "be_interface.h"
#include "be_aas_def.h"

#define ROUTING_DEBUG

#define DISTANCEFACTOR_CROUCH		1.3f		
#define DISTANCEFACTOR_SWIM			1		
#define DISTANCEFACTOR_WALK			0.33f	

#define CACHE_REFRESHTIME		15.0f	

#define MAX_FRAMEROUTINGUPDATES		10



#ifdef ROUTING_DEBUG
int numareacacheupdates;
int numportalcacheupdates;
#endif 

int routingcachesize;
int max_routingcachesize;

#define LIMIT_RT_WINDOW_MS	1000
#define LIMIT_RT_MAX_FRAMES	3600
#define LIMIT_RT_COUNTERS	10
#define LIMIT_RT_BUSEC		9	

static int	limit_rtOn = -1;			
static int	limit_rtCnt[LIMIT_RT_COUNTERS];
static int	limit_rtStart, limit_rtTicks, limit_rtBmax;

static const char *limit_rtName[LIMIT_RT_COUNTERS] = {
	"calls", "same", "oor", "nra", "hit", "miss", "phit", "pmiss", "evict", "busec"
};

static int LIMIT_RouteOn(void)
{
	if (limit_rtOn < 0)
	{
		limit_rtOn = LibVarValue("limit_stats", "1") != 0;
	} 
	return limit_rtOn;
} 

static int LIMIT_RouteMicro(void)
{
#ifndef _WIN32
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int)((ts.tv_sec % 1000000) * 1000000 + ts.tv_nsec / 1000);
#else
	return Sys_MilliSeconds() * 1000;
#endif
} 

void LIMIT_RouteCount(int which, int n)
{
	if (!LIMIT_RouteOn() || n <= 0 || which < 0 || which >= LIMIT_RT_COUNTERS)
	{
		return;
	} 
	limit_rtCnt[which] += n;
} 

void LIMIT_RouteBusec(int usec)
{
	if (!LIMIT_RouteOn() || usec <= 0)
	{
		return;
	} 
	limit_rtCnt[LIMIT_RT_BUSEC] += usec;
} 

void LIMIT_RouteFrame(void)
{
	int now, window, i;
	char line[1024], part[64];

	if (!LIMIT_RouteOn())
	{
		for (i = 0; i < LIMIT_RT_COUNTERS; i++)
		{
			limit_rtCnt[i] = 0;
		} 
		limit_rtStart = limit_rtTicks = limit_rtBmax = 0;
		return;
	} 

	if (!limit_rtStart)
	{
		limit_rtStart = LIMIT_RouteMicro();
		return;
	} 

	now = LIMIT_RouteMicro();
	window = (now - limit_rtStart) / 1000;
	limit_rtTicks++;
	if (window < LIMIT_RT_WINDOW_MS && limit_rtTicks < LIMIT_RT_MAX_FRAMES)
	{
		return;
	} 

	line[0] = '\0';
	for (i = 0; i < LIMIT_RT_COUNTERS; i++)
	{
		Com_sprintf(part, sizeof(part), " %s=%d", limit_rtName[i], limit_rtCnt[i]);
		Q_strcat(line, sizeof(line), part);
	} 
	Com_sprintf(part, sizeof(part), " bmax=%d", limit_rtBmax);
	Q_strcat(line, sizeof(line), part);
	botimport.Print(PRT_MESSAGE, "[LIMIT:route] window=%dms%s\n", window, line);

	for (i = 0; i < LIMIT_RT_COUNTERS; i++)
	{
		limit_rtCnt[i] = 0;
	} 
	limit_rtStart = LIMIT_RouteMicro();
	limit_rtTicks = limit_rtBmax = 0;
} 

#ifdef ROUTING_DEBUG
void AAS_RoutingInfo(void)
{
	botimport.Print(PRT_MESSAGE, "%d area cache updates\n", numareacacheupdates);
	botimport.Print(PRT_MESSAGE, "%d portal cache updates\n", numportalcacheupdates);
	botimport.Print(PRT_MESSAGE, "%d bytes routing cache\n", routingcachesize);
} 
#endif 
static ID_INLINE int AAS_ClusterAreaNum(int cluster, int areanum)
{
	int side, areacluster;

	areacluster = aasworld.areasettings[areanum].cluster;
	if (areacluster > 0) return aasworld.areasettings[areanum].clusterareanum;
	else
	{
		side = aasworld.portals[-areacluster].frontcluster != cluster;
		return aasworld.portals[-areacluster].clusterareanum[side];
	} 
} 
void AAS_InitTravelFlagFromType(void)
{
	int i;

	for (i = 0; i < MAX_TRAVELTYPES; i++)
	{
		aasworld.travelflagfortype[i] = TFL_INVALID;
	} 
	aasworld.travelflagfortype[TRAVEL_INVALID] = TFL_INVALID;
	aasworld.travelflagfortype[TRAVEL_WALK] = TFL_WALK;
	aasworld.travelflagfortype[TRAVEL_CROUCH] = TFL_CROUCH;
	aasworld.travelflagfortype[TRAVEL_BARRIERJUMP] = TFL_BARRIERJUMP;
	aasworld.travelflagfortype[TRAVEL_JUMP] = TFL_JUMP;
	aasworld.travelflagfortype[TRAVEL_LADDER] = TFL_LADDER;
	aasworld.travelflagfortype[TRAVEL_WALKOFFLEDGE] = TFL_WALKOFFLEDGE;
	aasworld.travelflagfortype[TRAVEL_SWIM] = TFL_SWIM;
	aasworld.travelflagfortype[TRAVEL_WATERJUMP] = TFL_WATERJUMP;
	aasworld.travelflagfortype[TRAVEL_TELEPORT] = TFL_TELEPORT;
	aasworld.travelflagfortype[TRAVEL_ELEVATOR] = TFL_ELEVATOR;
	aasworld.travelflagfortype[TRAVEL_ROCKETJUMP] = TFL_ROCKETJUMP;
	aasworld.travelflagfortype[TRAVEL_BFGJUMP] = TFL_BFGJUMP;
	aasworld.travelflagfortype[TRAVEL_GRAPPLEHOOK] = TFL_GRAPPLEHOOK;
	aasworld.travelflagfortype[TRAVEL_DOUBLEJUMP] = TFL_DOUBLEJUMP;
	aasworld.travelflagfortype[TRAVEL_RAMPJUMP] = TFL_RAMPJUMP;
	aasworld.travelflagfortype[TRAVEL_STRAFEJUMP] = TFL_STRAFEJUMP;
	aasworld.travelflagfortype[TRAVEL_JUMPPAD] = TFL_JUMPPAD;
	aasworld.travelflagfortype[TRAVEL_FUNCBOB] = TFL_FUNCBOB;
} 
static ID_INLINE int AAS_TravelFlagForType_inline(int traveltype)
{
	int tfl = 0;

	if (traveltype & TRAVELFLAG_NOTTEAM1)
		tfl |= TFL_NOTTEAM1;
	if (traveltype & TRAVELFLAG_NOTTEAM2)
		tfl |= TFL_NOTTEAM2;
	traveltype &= TRAVELTYPE_MASK;
	if (traveltype < 0 || traveltype >= MAX_TRAVELTYPES)
		return TFL_INVALID;
	tfl |= aasworld.travelflagfortype[traveltype];
	return tfl;
} 
int AAS_TravelFlagForType(int traveltype)
{
	return AAS_TravelFlagForType_inline(traveltype);
} 
void AAS_UnlinkCache(aas_routingcache_t *cache)
{
	if (cache->time_next) cache->time_next->time_prev = cache->time_prev;
	else aasworld.newestcache = cache->time_prev;
	if (cache->time_prev) cache->time_prev->time_next = cache->time_next;
	else aasworld.oldestcache = cache->time_next;
	cache->time_next = NULL;
	cache->time_prev = NULL;
} 
void AAS_LinkCache(aas_routingcache_t *cache)
{
	if (aasworld.newestcache)
	{
		aasworld.newestcache->time_next = cache;
		cache->time_prev = aasworld.newestcache;
	} 
	else
	{
		aasworld.oldestcache = cache;
		cache->time_prev = NULL;
	} 
	cache->time_next = NULL;
	aasworld.newestcache = cache;
} 
void AAS_FreeRoutingCache(aas_routingcache_t *cache)
{
	AAS_UnlinkCache(cache);
	routingcachesize -= cache->size;
	FreeMemory(cache);
} 
void AAS_RemoveRoutingCacheInCluster( int clusternum )
{
	int i;
	aas_routingcache_t *cache, *nextcache;
	aas_cluster_t *cluster;

	if (!aasworld.clusterareacache)
		return;
	cluster = &aasworld.clusters[clusternum];
	for (i = 0; i < cluster->numareas; i++)
	{
		for (cache = aasworld.clusterareacache[clusternum][i]; cache; cache = nextcache)
		{
			nextcache = cache->next;
			AAS_FreeRoutingCache(cache);
		} 
		aasworld.clusterareacache[clusternum][i] = NULL;
	} 
} 
void AAS_RemoveRoutingCacheUsingArea( int areanum )
{
	int i, clusternum;
	aas_routingcache_t *cache, *nextcache;

	clusternum = aasworld.areasettings[areanum].cluster;
	if (clusternum > 0)
	{
		AAS_RemoveRoutingCacheInCluster( clusternum );
	} 
	else
	{
		AAS_RemoveRoutingCacheInCluster( aasworld.portals[-clusternum].frontcluster );
		AAS_RemoveRoutingCacheInCluster( aasworld.portals[-clusternum].backcluster );
	} 
	for (i = 0; i < aasworld.numareas; i++)
	{
		for (cache = aasworld.portalcache[i]; cache; cache = nextcache)
		{
			nextcache = cache->next;
			AAS_FreeRoutingCache(cache);
		} 
		aasworld.portalcache[i] = NULL;
	} 
} 
int AAS_EnableRoutingArea(int areanum, int enable)
{
	int flags;

	if (areanum <= 0 || areanum >= aasworld.numareas)
	{
		if (botDeveloper)
		{
			botimport.Print(PRT_ERROR, "AAS_EnableRoutingArea: areanum %d out of range\n", areanum);
		} 
		return 0;
	} 
	flags = aasworld.areasettings[areanum].areaflags & AREA_DISABLED;
	if (enable < 0)
		return !flags;

	if (enable)
		aasworld.areasettings[areanum].areaflags &= ~AREA_DISABLED;
	else
		aasworld.areasettings[areanum].areaflags |= AREA_DISABLED;
	if ( (flags & AREA_DISABLED) != (aasworld.areasettings[areanum].areaflags & AREA_DISABLED) )
	{
		AAS_RemoveRoutingCacheUsingArea( areanum );
	} 
	return !flags;
} 
static ID_INLINE float AAS_RoutingTime(void)
{
	return AAS_Time();
} 
int AAS_GetAreaContentsTravelFlags(int areanum)
{
	int contents, tfl;

	contents = aasworld.areasettings[areanum].contents;
	tfl = 0;
	if (contents & AREACONTENTS_WATER)
		tfl |= TFL_WATER;
	else if (contents & AREACONTENTS_SLIME)
		tfl |= TFL_SLIME;
	else if (contents & AREACONTENTS_LAVA)
		tfl |= TFL_LAVA;
	else
		tfl |= TFL_AIR;
	if (contents & AREACONTENTS_DONOTENTER)
		tfl |= TFL_DONOTENTER;
	if (contents & AREACONTENTS_NOTTEAM1)
		tfl |= TFL_NOTTEAM1;
	if (contents & AREACONTENTS_NOTTEAM2)
		tfl |= TFL_NOTTEAM2;
	if (aasworld.areasettings[areanum].areaflags & AREA_BRIDGE)
		tfl |= TFL_BRIDGE;
	return tfl;
} 
static ID_INLINE int AAS_AreaContentsTravelFlags_inline(int areanum)
{
	return aasworld.areacontentstravelflags[areanum];
} 
int AAS_AreaContentsTravelFlags(int areanum)
{
	return aasworld.areacontentstravelflags[areanum];
} 
void AAS_InitAreaContentsTravelFlags(void)
{
	int i;

	if (aasworld.areacontentstravelflags) FreeMemory(aasworld.areacontentstravelflags);
	aasworld.areacontentstravelflags = (int *) GetClearedMemory(aasworld.numareas * sizeof(int));
	for (i = 0; i < aasworld.numareas; i++) {
		aasworld.areacontentstravelflags[i] = AAS_GetAreaContentsTravelFlags(i);
	}
} 
void AAS_CreateReversedReachability(void)
{
	int i, n;
	aas_reversedlink_t *revlink;
	aas_reachability_t *reach;
	aas_areasettings_t *settings;
	char *ptr;
#ifdef DEBUG
	int starttime;

	starttime = Sys_MilliSeconds();
#endif
	if (aasworld.reversedreachability) FreeMemory(aasworld.reversedreachability);
	ptr = (char *) GetClearedMemory(aasworld.numareas * sizeof(aas_reversedreachability_t) +
							aasworld.reachabilitysize * sizeof(aas_reversedlink_t));
	aasworld.reversedreachability = (aas_reversedreachability_t *) ptr;
	ptr += aasworld.numareas * sizeof(aas_reversedreachability_t);
	for (i = 1; i < aasworld.numareas; i++)
	{
		settings = &aasworld.areasettings[i];
		if (settings->numreachableareas >= 128)
			botimport.Print(PRT_WARNING, "area %d has more than 128 reachabilities\n", i);
		for (n = 0; n < settings->numreachableareas && n < 128; n++)
		{
			reach = &aasworld.reachability[settings->firstreachablearea + n];
			revlink = (aas_reversedlink_t *) ptr;
			ptr += sizeof(aas_reversedlink_t);
			revlink->areanum = i;
			revlink->linknum = settings->firstreachablearea + n;
			revlink->next = aasworld.reversedreachability[reach->areanum].first;
			aasworld.reversedreachability[reach->areanum].first = revlink;
			aasworld.reversedreachability[reach->areanum].numlinks++;
		} 
	} 
#ifdef DEBUG
	botimport.Print(PRT_MESSAGE, "reversed reachability %d msec\n", Sys_MilliSeconds() - starttime);
#endif
} 
unsigned short int AAS_AreaTravelTime(int areanum, vec3_t start, vec3_t end)
{
	int intdist;
	float dist;
	vec3_t dir;

	VectorSubtract(start, end, dir);
	dist = VectorLength(dir);
	if (AAS_AreaCrouch(areanum)) dist *= DISTANCEFACTOR_CROUCH;
	else if (AAS_AreaSwim(areanum)) dist *= DISTANCEFACTOR_SWIM;
	else dist *= DISTANCEFACTOR_WALK;
	intdist = (int) dist;
	if (intdist <= 0) intdist = 1;
	return intdist;
} 
void AAS_CalculateAreaTravelTimes(void)
{
	int i, l, n, size;
	char *ptr;
	vec3_t end;
	aas_reversedreachability_t *revreach;
	aas_reversedlink_t *revlink;
	aas_reachability_t *reach;
	aas_areasettings_t *settings;
#ifdef DEBUG
	int starttime;

	starttime = Sys_MilliSeconds();
#endif
	if (aasworld.areatraveltimes) FreeMemory(aasworld.areatraveltimes);
	size = aasworld.numareas * sizeof(unsigned short **);
	for (i = 0; i < aasworld.numareas; i++)
	{
		revreach = &aasworld.reversedreachability[i];
		settings = &aasworld.areasettings[i];
		size += settings->numreachableareas * sizeof(unsigned short *);
		size += settings->numreachableareas *
			revreach->numlinks * sizeof(unsigned short);
	} 
	ptr = (char *) GetClearedMemory(size);
	aasworld.areatraveltimes = (unsigned short ***) ptr;
	ptr += aasworld.numareas * sizeof(unsigned short **);
	for (i = 0; i < aasworld.numareas; i++)
	{
		revreach = &aasworld.reversedreachability[i];
		settings = &aasworld.areasettings[i];
		aasworld.areatraveltimes[i] = (unsigned short **) ptr;
		ptr += settings->numreachableareas * sizeof(unsigned short *);
		for (l = 0; l < settings->numreachableareas; l++)
		{
			aasworld.areatraveltimes[i][l] = (unsigned short *) ptr;
			ptr += revreach->numlinks * sizeof(unsigned short);
			reach = &aasworld.reachability[settings->firstreachablearea + l];
			for (n = 0, revlink = revreach->first; revlink; revlink = revlink->next, n++)
			{
				VectorCopy(aasworld.reachability[revlink->linknum].end, end);
				aasworld.areatraveltimes[i][l][n] = AAS_AreaTravelTime(i, end, reach->start);
			} 
		} 
	} 
#ifdef DEBUG
	botimport.Print(PRT_MESSAGE, "area travel times %d msec\n", Sys_MilliSeconds() - starttime);
#endif
} 
int AAS_PortalMaxTravelTime(int portalnum)
{
	int l, n, t, maxt;
	aas_portal_t *portal;
	aas_reversedreachability_t *revreach;
	aas_reversedlink_t *revlink;
	aas_areasettings_t *settings;

	portal = &aasworld.portals[portalnum];
	revreach = &aasworld.reversedreachability[portal->areanum];
	settings = &aasworld.areasettings[portal->areanum];
	maxt = 0;
	for (l = 0; l < settings->numreachableareas; l++)
	{
		for (n = 0, revlink = revreach->first; revlink; revlink = revlink->next, n++)
		{
			t = aasworld.areatraveltimes[portal->areanum][l][n];
			if (t > maxt)
			{
				maxt = t;
			} 
		} 
	} 
	return maxt;
} 
void AAS_InitPortalMaxTravelTimes(void)
{
	int i;

	if (aasworld.portalmaxtraveltimes) FreeMemory(aasworld.portalmaxtraveltimes);

	aasworld.portalmaxtraveltimes = (int *) GetClearedMemory(aasworld.numportals * sizeof(int));

	for (i = 0; i < aasworld.numportals; i++)
	{
		aasworld.portalmaxtraveltimes[i] = AAS_PortalMaxTravelTime(i);
	} 
} 
int AAS_FreeOldestCache(void)
{
	int clusterareanum;
	aas_routingcache_t *cache;

	for (cache = aasworld.oldestcache; cache; cache = cache->time_next) {
		if (cache->type == CACHETYPE_AREA && aasworld.areasettings[cache->areanum].cluster < 0) {
			continue;
		}
		break;
	}
	if (cache) {
		if (cache->type == CACHETYPE_AREA) {
			clusterareanum = AAS_ClusterAreaNum(cache->cluster, cache->areanum);
			if (cache->prev) cache->prev->next = cache->next;
			else aasworld.clusterareacache[cache->cluster][clusterareanum] = cache->next;
			if (cache->next) cache->next->prev = cache->prev;
		}
		else {
			if (cache->prev) cache->prev->next = cache->next;
			else aasworld.portalcache[cache->areanum] = cache->next;
			if (cache->next) cache->next->prev = cache->prev;
		}
		AAS_FreeRoutingCache(cache);
		LIMIT_RouteCount(8, 1);		
		return qtrue;
	}
	return qfalse;
} 
aas_routingcache_t *AAS_AllocRoutingCache(int numtraveltimes)
{
	aas_routingcache_t *cache;
	int size;

	size = sizeof(aas_routingcache_t)
						+ numtraveltimes * sizeof(unsigned short int)
						+ numtraveltimes * sizeof(unsigned char);
	routingcachesize += size;
	if (LIMIT_RouteOn() && routingcachesize > limit_rtBmax)	
	{
		limit_rtBmax = routingcachesize;
	} 
	cache = (aas_routingcache_t *) GetClearedMemory(size);
	cache->reachabilities = (unsigned char *) cache + sizeof(aas_routingcache_t)
								+ numtraveltimes * sizeof(unsigned short int);
	cache->size = size;
	return cache;
} 
void AAS_FreeAllClusterAreaCache(void)
{
	int i, j;
	aas_routingcache_t *cache, *nextcache;
	aas_cluster_t *cluster;

	if (!aasworld.clusterareacache) return;
	for (i = 0; i < aasworld.numclusters; i++)
	{
		cluster = &aasworld.clusters[i];
		for (j = 0; j < cluster->numareas; j++)
		{
			for (cache = aasworld.clusterareacache[i][j]; cache; cache = nextcache)
			{
				nextcache = cache->next;
				AAS_FreeRoutingCache(cache);
			} 
			aasworld.clusterareacache[i][j] = NULL;
		} 
	} 
	FreeMemory(aasworld.clusterareacache);
	aasworld.clusterareacache = NULL;
} 
void AAS_InitClusterAreaCache(void)
{
	int i, size;
	char *ptr;

	for (size = 0, i = 0; i < aasworld.numclusters; i++)
	{
		size += aasworld.clusters[i].numareas;
	} 
	ptr = (char *) GetClearedMemory(
				aasworld.numclusters * sizeof(aas_routingcache_t **) +
				size * sizeof(aas_routingcache_t *));
	aasworld.clusterareacache = (aas_routingcache_t ***) ptr;
	ptr += aasworld.numclusters * sizeof(aas_routingcache_t **);
	for (i = 0; i < aasworld.numclusters; i++)
	{
		aasworld.clusterareacache[i] = (aas_routingcache_t **) ptr;
		ptr += aasworld.clusters[i].numareas * sizeof(aas_routingcache_t *);
	} 
} 
void AAS_FreeAllPortalCache(void)
{
	int i;
	aas_routingcache_t *cache, *nextcache;

	if (!aasworld.portalcache) return;
	for (i = 0; i < aasworld.numareas; i++)
	{
		for (cache = aasworld.portalcache[i]; cache; cache = nextcache)
		{
			nextcache = cache->next;
			AAS_FreeRoutingCache(cache);
		} 
		aasworld.portalcache[i] = NULL;
	} 
	FreeMemory(aasworld.portalcache);
	aasworld.portalcache = NULL;
} 
void AAS_InitPortalCache(void)
{
	aasworld.portalcache = (aas_routingcache_t **) GetClearedMemory(
								aasworld.numareas * sizeof(aas_routingcache_t *));
} 
void AAS_InitRoutingUpdate(void)
{
	int i, maxreachabilityareas;

	if (aasworld.areaupdate) FreeMemory(aasworld.areaupdate);
	maxreachabilityareas = 0;
	for (i = 0; i < aasworld.numclusters; i++)
	{
		if (aasworld.clusters[i].numreachabilityareas > maxreachabilityareas)
		{
			maxreachabilityareas = aasworld.clusters[i].numreachabilityareas;
		} 
	} 
	aasworld.areaupdate = (aas_routingupdate_t *) GetClearedMemory(
									maxreachabilityareas * sizeof(aas_routingupdate_t));
	if (aasworld.portalupdate) FreeMemory(aasworld.portalupdate);
	aasworld.portalupdate = (aas_routingupdate_t *) GetClearedMemory(
									(aasworld.numportals+1) * sizeof(aas_routingupdate_t));
} 
void AAS_CreateAllRoutingCache(void)
{
	int i, j;

	aasworld.initialized = qtrue;
	botimport.Print(PRT_MESSAGE, "AAS_CreateAllRoutingCache\n");
	for (i = 1; i < aasworld.numareas; i++)
	{
		if (!AAS_AreaReachability(i)) continue;
		for (j = 1; j < aasworld.numareas; j++)
		{
			if (i == j) continue;
			if (!AAS_AreaReachability(j)) continue;
			AAS_AreaTravelTimeToGoalArea(i, aasworld.areas[i].center, j, TFL_DEFAULT);
		} 
	} 
	aasworld.initialized = qfalse;
} 

typedef struct routecacheheader_s
{
	int ident;
	int version;
	int numareas;
	int numclusters;
	int areacrc;
	int clustercrc;
	int numportalcache;
	int numareacache;
} routecacheheader_t;

#define RCID						(('C'<<24)+('R'<<16)+('E'<<8)+'M')
#define RCVERSION					2


void AAS_WriteRouteCache(void)
{
	int i, j, numportalcache, numareacache, totalsize;
	aas_routingcache_t *cache;
	aas_cluster_t *cluster;
	fileHandle_t fp;
	char filename[MAX_QPATH];
	routecacheheader_t routecacheheader;

	numportalcache = 0;
	for (i = 0; i < aasworld.numareas; i++)
	{
		for (cache = aasworld.portalcache[i]; cache; cache = cache->next)
		{
			numportalcache++;
		} 
	} 
	numareacache = 0;
	for (i = 0; i < aasworld.numclusters; i++)
	{
		cluster = &aasworld.clusters[i];
		for (j = 0; j < cluster->numareas; j++)
		{
			for (cache = aasworld.clusterareacache[i][j]; cache; cache = cache->next)
			{
				numareacache++;
			} 
		} 
	} 
	Com_sprintf(filename, MAX_QPATH, "maps/%s.rcd", aasworld.mapname);
	botimport.FS_FOpenFile( filename, &fp, FS_WRITE );
	if (!fp)
	{
		AAS_Error("Unable to open file: %s\n", filename);
		return;
	} 
	routecacheheader.ident = RCID;
	routecacheheader.version = RCVERSION;
	routecacheheader.numareas = aasworld.numareas;
	routecacheheader.numclusters = aasworld.numclusters;
	routecacheheader.areacrc = CRC_ProcessString( (unsigned char *)aasworld.areas, sizeof(aas_area_t) * aasworld.numareas );
	routecacheheader.clustercrc = CRC_ProcessString( (unsigned char *)aasworld.clusters, sizeof(aas_cluster_t) * aasworld.numclusters );
	routecacheheader.numportalcache = numportalcache;
	routecacheheader.numareacache = numareacache;
	botimport.FS_Write(&routecacheheader, sizeof(routecacheheader_t), fp);
	totalsize = 0;
	for (i = 0; i < aasworld.numareas; i++)
	{
		for (cache = aasworld.portalcache[i]; cache; cache = cache->next)
		{
			botimport.FS_Write(cache, cache->size, fp);
			totalsize += cache->size;
		} 
	} 
	for (i = 0; i < aasworld.numclusters; i++)
	{
		cluster = &aasworld.clusters[i];
		for (j = 0; j < cluster->numareas; j++)
		{
			for (cache = aasworld.clusterareacache[i][j]; cache; cache = cache->next)
			{
				botimport.FS_Write(cache, cache->size, fp);
				totalsize += cache->size;
			} 
		} 
	} 
	botimport.FS_FCloseFile(fp);
	botimport.Print(PRT_MESSAGE, "\nroute cache written to %s\n", filename);
	botimport.Print(PRT_MESSAGE, "written %d bytes of routing cache\n", totalsize);
} 
aas_routingcache_t *AAS_ReadCache(fileHandle_t fp)
{
	int size;
	aas_routingcache_t *cache;

	botimport.FS_Read(&size, sizeof(size), fp);
	cache = (aas_routingcache_t *) GetMemory(size);
	cache->size = size;
	botimport.FS_Read((unsigned char *)cache + sizeof(size), size - sizeof(size), fp);
	cache->reachabilities = (unsigned char *) cache + sizeof(aas_routingcache_t) - sizeof(unsigned short) +
		(size - sizeof(aas_routingcache_t) + sizeof(unsigned short)) / 3 * 2;
	return cache;
} 
int AAS_ReadRouteCache(void)
{
	int i, clusterareanum;
	fileHandle_t fp;
	char filename[MAX_QPATH];
	routecacheheader_t routecacheheader;
	aas_routingcache_t *cache;

	Com_sprintf(filename, MAX_QPATH, "maps/%s.rcd", aasworld.mapname);
	botimport.FS_FOpenFile( filename, &fp, FS_READ );
	if (!fp)
	{
		return qfalse;
	} 
	botimport.FS_Read(&routecacheheader, sizeof(routecacheheader_t), fp );
	// Retail AAS_ReadRouteCache @0x001308F0 does NOT return silently on all six
	if (routecacheheader.ident != RCID)
	{
		AAS_Error("%s is not a route cache dump\n", filename);
		return qfalse;
	} 
	if (routecacheheader.version != RCVERSION)
	{
		AAS_Error("route cache dump has wrong version %d, should be %d",
			routecacheheader.version, RCVERSION);
		return qfalse;
	} 
	if (routecacheheader.numareas != aasworld.numareas)
	{
		return qfalse;
	} 
	if (routecacheheader.numclusters != aasworld.numclusters)
	{
		return qfalse;
	} 
	if (routecacheheader.areacrc !=
		CRC_ProcessString( (unsigned char *)aasworld.areas, sizeof(aas_area_t) * aasworld.numareas ))
	{
		return qfalse;
	} 
	if (routecacheheader.clustercrc !=
		CRC_ProcessString( (unsigned char *)aasworld.clusters, sizeof(aas_cluster_t) * aasworld.numclusters ))
	{
		return qfalse;
	} 
	for (i = 0; i < routecacheheader.numportalcache; i++)
	{
		cache = AAS_ReadCache(fp);
		cache->next = aasworld.portalcache[cache->areanum];
		cache->prev = NULL;
		if (aasworld.portalcache[cache->areanum])
			aasworld.portalcache[cache->areanum]->prev = cache;
		aasworld.portalcache[cache->areanum] = cache;
	} 
	for (i = 0; i < routecacheheader.numareacache; i++)
	{
		cache = AAS_ReadCache(fp);
		clusterareanum = AAS_ClusterAreaNum(cache->cluster, cache->areanum);
		cache->next = aasworld.clusterareacache[cache->cluster][clusterareanum];
		cache->prev = NULL;
		if (aasworld.clusterareacache[cache->cluster][clusterareanum])
			aasworld.clusterareacache[cache->cluster][clusterareanum]->prev = cache;
		aasworld.clusterareacache[cache->cluster][clusterareanum] = cache;
	} 
	botimport.FS_FCloseFile(fp);
	return qtrue;
} 
#define MAX_REACHABILITYPASSAREAS		32

void AAS_InitReachabilityAreas(void)
{
	int i, j, numareas, areas[MAX_REACHABILITYPASSAREAS];
	int numreachareas;
	aas_reachability_t *reach;
	vec3_t start, end;

	if (aasworld.reachabilityareas)
		FreeMemory(aasworld.reachabilityareas);
	if (aasworld.reachabilityareaindex)
		FreeMemory(aasworld.reachabilityareaindex);

	aasworld.reachabilityareas = (aas_reachabilityareas_t *)
				GetClearedMemory(aasworld.reachabilitysize * sizeof(aas_reachabilityareas_t));
	aasworld.reachabilityareaindex = (int *)
				GetClearedMemory(aasworld.reachabilitysize * MAX_REACHABILITYPASSAREAS * sizeof(int));
	numreachareas = 0;
	for (i = 0; i < aasworld.reachabilitysize; i++)
	{
		reach = &aasworld.reachability[i];
		numareas = 0;
		switch(reach->traveltype & TRAVELTYPE_MASK)
		{
			case TRAVEL_BARRIERJUMP:
			case TRAVEL_WATERJUMP:
				VectorCopy(reach->start, end);
				end[2] = reach->end[2];
				numareas = AAS_TraceAreas(reach->start, end, areas, NULL, MAX_REACHABILITYPASSAREAS);
				break;
			case TRAVEL_WALKOFFLEDGE:
				VectorCopy(reach->end, start);
				start[2] = reach->start[2];
				numareas = AAS_TraceAreas(start, reach->end, areas, NULL, MAX_REACHABILITYPASSAREAS);
				break;
			case TRAVEL_GRAPPLEHOOK:
				numareas = AAS_TraceAreas(reach->start, reach->end, areas, NULL, MAX_REACHABILITYPASSAREAS);
				break;

			case TRAVEL_JUMP: break;
			case TRAVEL_ROCKETJUMP: break;
			case TRAVEL_BFGJUMP: break;
			case TRAVEL_JUMPPAD: break;

			case TRAVEL_ELEVATOR: break;
			case TRAVEL_FUNCBOB: break;

			case TRAVEL_WALK: break;
			case TRAVEL_CROUCH: break;
			case TRAVEL_LADDER: break;
			case TRAVEL_SWIM: break;
			case TRAVEL_TELEPORT: break;
			default: break;
		} 
		aasworld.reachabilityareas[i].firstarea = numreachareas;
		aasworld.reachabilityareas[i].numareas = numareas;
		for (j = 0; j < numareas; j++)
		{
			aasworld.reachabilityareaindex[numreachareas++] = areas[j];
		} 
	} 
} 
void AAS_InitRouting(void)
{
	AAS_InitTravelFlagFromType();
	AAS_InitAreaContentsTravelFlags();
	AAS_InitRoutingUpdate();
	AAS_CreateReversedReachability();
	AAS_InitClusterAreaCache();
	AAS_InitPortalCache();
	AAS_CalculateAreaTravelTimes();
	AAS_InitPortalMaxTravelTimes();
	AAS_InitReachabilityAreas();
#ifdef ROUTING_DEBUG
	numareacacheupdates = 0;
	numportalcacheupdates = 0;
#endif 
	routingcachesize = 0;
	max_routingcachesize = 1024 * (int) LibVarValue("max_routingcache", "4096");
	AAS_ReadRouteCache();
} 
void AAS_FreeRoutingCaches(void)
{
	AAS_FreeAllClusterAreaCache();
	AAS_FreeAllPortalCache();
	if (aasworld.areatraveltimes) FreeMemory(aasworld.areatraveltimes);
	aasworld.areatraveltimes = NULL;
	if (aasworld.portalmaxtraveltimes) FreeMemory(aasworld.portalmaxtraveltimes);
	aasworld.portalmaxtraveltimes = NULL;
	if (aasworld.reversedreachability) FreeMemory(aasworld.reversedreachability);
	aasworld.reversedreachability = NULL;
	if (aasworld.areaupdate) FreeMemory(aasworld.areaupdate);
	aasworld.areaupdate = NULL;
	if (aasworld.portalupdate) FreeMemory(aasworld.portalupdate);
	aasworld.portalupdate = NULL;
	if (aasworld.reachabilityareas) FreeMemory(aasworld.reachabilityareas);
	aasworld.reachabilityareas = NULL;
	if (aasworld.reachabilityareaindex) FreeMemory(aasworld.reachabilityareaindex);
	aasworld.reachabilityareaindex = NULL;
	if (aasworld.areacontentstravelflags) FreeMemory(aasworld.areacontentstravelflags);
	aasworld.areacontentstravelflags = NULL;
} 
void AAS_UpdateAreaRoutingCache(aas_routingcache_t *areacache)
{
	int i, nextareanum, cluster, badtravelflags, clusterareanum, linknum;
	int numreachabilityareas;
	unsigned short int t, startareatraveltimes[128]; //NOTE: not more than 128 reachabilities per area allowed
	aas_routingupdate_t *updateliststart, *updatelistend, *curupdate, *nextupdate;
	aas_reachability_t *reach;
	aas_reversedreachability_t *revreach;
	aas_reversedlink_t *revlink;
	int limit_t0 = LIMIT_RouteOn() ? LIMIT_RouteMicro() : 0;

#ifdef ROUTING_DEBUG
	numareacacheupdates++;
#endif 
	numreachabilityareas = aasworld.clusters[areacache->cluster].numreachabilityareas;
	aasworld.frameroutingupdates++;
	badtravelflags = ~areacache->travelflags;
	clusterareanum = AAS_ClusterAreaNum(areacache->cluster, areacache->areanum);
	if (clusterareanum >= numreachabilityareas) return;
	Com_Memset(startareatraveltimes, 0, sizeof(startareatraveltimes));
	curupdate = &aasworld.areaupdate[clusterareanum];
	curupdate->areanum = areacache->areanum;
	curupdate->areatraveltimes = startareatraveltimes;
	curupdate->tmptraveltime = areacache->starttraveltime;
	areacache->traveltimes[clusterareanum] = areacache->starttraveltime;
	curupdate->next = NULL;
	curupdate->prev = NULL;
	updateliststart = curupdate;
	updatelistend = curupdate;
	while (updateliststart)
	{
		curupdate = updateliststart;
		if (curupdate->next) curupdate->next->prev = NULL;
		else updatelistend = NULL;
		updateliststart = curupdate->next;
		curupdate->inlist = qfalse;
		revreach = &aasworld.reversedreachability[curupdate->areanum];
		for (i = 0, revlink = revreach->first; revlink; revlink = revlink->next, i++)
		{
			linknum = revlink->linknum;
			reach = &aasworld.reachability[linknum];
			if (AAS_TravelFlagForType_inline(reach->traveltype) & badtravelflags) continue;
			if (aasworld.areasettings[reach->areanum].areaflags & AREA_DISABLED) continue;
			if (AAS_AreaContentsTravelFlags_inline(reach->areanum) & badtravelflags) continue;
			nextareanum = revlink->areanum;
			cluster = aasworld.areasettings[nextareanum].cluster;
			if (cluster > 0 && cluster != areacache->cluster) continue;
			clusterareanum = AAS_ClusterAreaNum(areacache->cluster, nextareanum);
			if (clusterareanum >= numreachabilityareas) continue;
			t = curupdate->tmptraveltime +
						curupdate->areatraveltimes[i] +
							reach->traveltime;
			if (!areacache->traveltimes[clusterareanum] ||
					areacache->traveltimes[clusterareanum] > t)
			{
				areacache->traveltimes[clusterareanum] = t;
				areacache->reachabilities[clusterareanum] = linknum - aasworld.areasettings[nextareanum].firstreachablearea;
				nextupdate = &aasworld.areaupdate[clusterareanum];
				nextupdate->areanum = nextareanum;
				nextupdate->tmptraveltime = t;
				nextupdate->areatraveltimes = aasworld.areatraveltimes[nextareanum][linknum -
													aasworld.areasettings[nextareanum].firstreachablearea];
				if (!nextupdate->inlist)
				{
					nextupdate->next = NULL;
					nextupdate->prev = updatelistend;
					if (updatelistend) updatelistend->next = nextupdate;
					else updateliststart = nextupdate;
					updatelistend = nextupdate;
					nextupdate->inlist = qtrue;
				} 
			} 
		} 
	} 
	LIMIT_RouteBusec(LIMIT_RouteOn() ? (LIMIT_RouteMicro() - limit_t0) : 0);
} 
aas_routingcache_t *AAS_GetAreaRoutingCache(int clusternum, int areanum, int travelflags)
{
	int clusterareanum;
	aas_routingcache_t *cache, *clustercache;

	clusterareanum = AAS_ClusterAreaNum(clusternum, areanum);
	clustercache = aasworld.clusterareacache[clusternum][clusterareanum];
	for (cache = clustercache; cache; cache = cache->next)
	{
		if (cache->travelflags == travelflags) break;
	} 
	if (!cache)
	{
		cache = AAS_AllocRoutingCache(aasworld.clusters[clusternum].numreachabilityareas);
		cache->cluster = clusternum;
		cache->areanum = areanum;
		VectorCopy(aasworld.areas[areanum].center, cache->origin);
		cache->starttraveltime = 1;
		cache->travelflags = travelflags;
		cache->prev = NULL;
		cache->next = clustercache;
		if (clustercache) clustercache->prev = cache;
		aasworld.clusterareacache[clusternum][clusterareanum] = cache;
		AAS_UpdateAreaRoutingCache(cache);
		LIMIT_RouteCount(5, 1);		
	} 
	else
	{
		AAS_UnlinkCache(cache);
		LIMIT_RouteCount(4, 1);		
	} 
	cache->time = AAS_RoutingTime();
	cache->type = CACHETYPE_AREA;
	AAS_LinkCache(cache);
	return cache;
} 
void AAS_UpdatePortalRoutingCache(aas_routingcache_t *portalcache)
{
	int i, portalnum, clusterareanum, clusternum;
	unsigned short int t;
	aas_portal_t *portal;
	aas_cluster_t *cluster;
	aas_routingcache_t *cache;
	aas_routingupdate_t *updateliststart, *updatelistend, *curupdate, *nextupdate;

#ifdef ROUTING_DEBUG
	numportalcacheupdates++;
#endif 
	curupdate = &aasworld.portalupdate[aasworld.numportals];
	curupdate->cluster = portalcache->cluster;
	curupdate->areanum = portalcache->areanum;
	curupdate->tmptraveltime = portalcache->starttraveltime;
	clusternum = aasworld.areasettings[portalcache->areanum].cluster;
	if (clusternum < 0)
	{
		portalcache->traveltimes[-clusternum] = portalcache->starttraveltime;
	} 
	curupdate->next = NULL;
	curupdate->prev = NULL;
	updateliststart = curupdate;
	updatelistend = curupdate;
	while (updateliststart)
	{
		curupdate = updateliststart;
		if (curupdate->next) curupdate->next->prev = NULL;
		else updatelistend = NULL;
		updateliststart = curupdate->next;
		curupdate->inlist = qfalse;
		cluster = &aasworld.clusters[curupdate->cluster];
		cache = AAS_GetAreaRoutingCache(curupdate->cluster,
								curupdate->areanum, portalcache->travelflags);
		for (i = 0; i < cluster->numportals; i++)
		{
			portalnum = aasworld.portalindex[cluster->firstportal + i];
			portal = &aasworld.portals[portalnum];
			if (portal->areanum == curupdate->areanum) continue;
			clusterareanum = AAS_ClusterAreaNum(curupdate->cluster, portal->areanum);
			if (clusterareanum >= cluster->numreachabilityareas) continue;
			t = cache->traveltimes[clusterareanum];
			if (!t) continue;
			t += curupdate->tmptraveltime;
			if (!portalcache->traveltimes[portalnum] ||
					portalcache->traveltimes[portalnum] > t)
			{
				portalcache->traveltimes[portalnum] = t;
				nextupdate = &aasworld.portalupdate[portalnum];
				if (portal->frontcluster == curupdate->cluster)
				{
					nextupdate->cluster = portal->backcluster;
				} 
				else
				{
					nextupdate->cluster = portal->frontcluster;
				} 
				nextupdate->areanum = portal->areanum;
				nextupdate->tmptraveltime = t + aasworld.portalmaxtraveltimes[portalnum];
				if (!nextupdate->inlist)
				{
					nextupdate->next = NULL;
					nextupdate->prev = updatelistend;
					if (updatelistend) updatelistend->next = nextupdate;
					else updateliststart = nextupdate;
					updatelistend = nextupdate;
					nextupdate->inlist = qtrue;
				} 
			} 
		} 
	} 
} 
aas_routingcache_t *AAS_GetPortalRoutingCache(int clusternum, int areanum, int travelflags)
{
	aas_routingcache_t *cache;

	for (cache = aasworld.portalcache[areanum]; cache; cache = cache->next)
	{
		if (cache->travelflags == travelflags) break;
	} 
	if (!cache)
	{
		cache = AAS_AllocRoutingCache(aasworld.numportals);
		cache->cluster = clusternum;
		cache->areanum = areanum;
		VectorCopy(aasworld.areas[areanum].center, cache->origin);
		cache->starttraveltime = 1;
		cache->travelflags = travelflags;
		cache->prev = NULL;
		cache->next = aasworld.portalcache[areanum];
		if (aasworld.portalcache[areanum]) aasworld.portalcache[areanum]->prev = cache;
		aasworld.portalcache[areanum] = cache;
		AAS_UpdatePortalRoutingCache(cache);
		LIMIT_RouteCount(7, 1);		
	} 
	else
	{
		AAS_UnlinkCache(cache);
		LIMIT_RouteCount(6, 1);		
	} 
	cache->time = AAS_RoutingTime();
	cache->type = CACHETYPE_PORTAL;
	AAS_LinkCache(cache);
	return cache;
} 
int AAS_AreaRouteToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags, int *traveltime, int *reachnum)
{
	int clusternum, goalclusternum, portalnum, i, clusterareanum, bestreachnum;
	unsigned short int t, besttime;
	aas_portal_t *portal;
	aas_cluster_t *cluster;
	aas_routingcache_t *areacache, *portalcache;
	aas_reachability_t *reach;

	LIMIT_RouteCount(0, 1);		

	if (!aasworld.initialized) return qfalse;

	if (areanum == goalareanum)
	{
		*traveltime = 1;
		*reachnum = 0;
		LIMIT_RouteCount(1, 1);	
		return qtrue;
	}
	if (areanum <= 0 || areanum >= aasworld.numareas)
	{
		if (botDeveloper)
		{
			botimport.Print(PRT_ERROR, "AAS_AreaTravelTimeToGoalArea: areanum %d out of range\n", areanum);
		} 
		LIMIT_RouteCount(2, 1);	
		return qfalse;
	} 
	if (goalareanum <= 0 || goalareanum >= aasworld.numareas)
	{
		if (botDeveloper)
		{
			botimport.Print(PRT_ERROR, "AAS_AreaTravelTimeToGoalArea: goalareanum %d out of range\n", goalareanum);
		} 
		LIMIT_RouteCount(2, 1);	
		return qfalse;
	} 
	if ( !aasworld.areasettings[areanum].numreachableareas
		|| !aasworld.areasettings[goalareanum].numreachableareas )
	{
		LIMIT_RouteCount(3, 1);
	}
	if (AAS_AreaDoNotEnter(areanum) || AAS_AreaDoNotEnter(goalareanum))
	{
		travelflags |= TFL_DONOTENTER;
	} 
	clusternum = aasworld.areasettings[areanum].cluster;
	goalclusternum = aasworld.areasettings[goalareanum].cluster;
	if (clusternum < 0 && goalclusternum > 0)
	{
		portal = &aasworld.portals[-clusternum];
		if (portal->frontcluster == goalclusternum ||
				portal->backcluster == goalclusternum)
		{
			clusternum = goalclusternum;
		} 
	} 
	else if (clusternum > 0 && goalclusternum < 0)
	{
		portal = &aasworld.portals[-goalclusternum];
		if (portal->frontcluster == clusternum ||
				portal->backcluster == clusternum)
		{
			goalclusternum = clusternum;
		} 
	} 
	if (clusternum > 0 && goalclusternum > 0 && clusternum == goalclusternum)
	{
		areacache = AAS_GetAreaRoutingCache(clusternum, goalareanum, travelflags);
		clusterareanum = AAS_ClusterAreaNum(clusternum, areanum);
		cluster = &aasworld.clusters[clusternum];
		if (clusterareanum >= cluster->numreachabilityareas) return 0;
		if (areacache->traveltimes[clusterareanum] != 0)
		{
			*reachnum = aasworld.areasettings[areanum].firstreachablearea +
							areacache->reachabilities[clusterareanum];
			if (!origin) {
				*traveltime = areacache->traveltimes[clusterareanum];
				return qtrue;
			}
			reach = &aasworld.reachability[*reachnum];
			*traveltime = areacache->traveltimes[clusterareanum] +
							AAS_AreaTravelTime(areanum, origin, reach->start);
			return qtrue;
		} 
	} 
	clusternum = aasworld.areasettings[areanum].cluster;
	goalclusternum = aasworld.areasettings[goalareanum].cluster;
	if (goalclusternum < 0)
	{
		portal = &aasworld.portals[-goalclusternum];
		goalclusternum = portal->frontcluster;
	} 
	portalcache = AAS_GetPortalRoutingCache(goalclusternum, goalareanum, travelflags);
	if (clusternum < 0)
	{
		*traveltime = portalcache->traveltimes[-clusternum];
		*reachnum = aasworld.areasettings[areanum].firstreachablearea +
						portalcache->reachabilities[-clusternum];
		return qtrue;
	} 
	besttime = 0;
	bestreachnum = -1;
	cluster = &aasworld.clusters[clusternum];
	for (i = 0; i < cluster->numportals; i++)
	{
		portalnum = aasworld.portalindex[cluster->firstportal + i];
		if (!portalcache->traveltimes[portalnum]) continue;
		portal = &aasworld.portals[portalnum];
		areacache = AAS_GetAreaRoutingCache(clusternum, portal->areanum, travelflags);
		clusterareanum = AAS_ClusterAreaNum(clusternum, areanum);
		if (clusterareanum >= cluster->numreachabilityareas) continue;
		if (!areacache->traveltimes[clusterareanum]) continue;
		t = portalcache->traveltimes[portalnum] + areacache->traveltimes[clusterareanum];
		t += aasworld.portalmaxtraveltimes[portalnum];
		if (origin)
		{
			*reachnum = aasworld.areasettings[areanum].firstreachablearea +
							areacache->reachabilities[clusterareanum];
			reach = aasworld.reachability + *reachnum;
			t += AAS_AreaTravelTime(areanum, origin, reach->start);
		} 
		if (!besttime || t < besttime)
		{
			bestreachnum = *reachnum;
			besttime = t;
		} 
	} 
	if (bestreachnum < 0) {
		return qfalse;
	}
	*reachnum = bestreachnum;
	*traveltime = besttime;
	return qtrue;
} 
int AAS_AreaTravelTimeToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags)
{
	int traveltime, reachnum = 0;

	if (AAS_AreaRouteToGoalArea(areanum, origin, goalareanum, travelflags, &traveltime, &reachnum))
	{
		return traveltime;
	}
	return 0;
} 
int AAS_AreaReachabilityToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags)
{
	int traveltime, reachnum = 0;

	if (AAS_AreaRouteToGoalArea(areanum, origin, goalareanum, travelflags, &traveltime, &reachnum))
	{
		return reachnum;
	}
	return 0;
} 
int AAS_PredictRoute(struct aas_predictroute_s *route, int areanum, vec3_t origin,
							int goalareanum, int travelflags, int maxareas, int maxtime,
							int stopevent, int stopcontents, int stoptfl, int stopareanum)
{
	int curareanum, reachnum, i, j, testareanum;
	vec3_t curorigin;
	aas_reachability_t *reach;
	aas_reachabilityareas_t *reachareas;

	route->stopevent = RSE_NONE;
	route->endarea = goalareanum;
	route->endcontents = 0;
	route->endtravelflags = 0;
	VectorCopy(origin, route->endpos);
	route->time = 0;

	curareanum = areanum;
	VectorCopy(origin, curorigin);

	for (i = 0; curareanum != goalareanum && (!maxareas || i < maxareas) && i < aasworld.numareas; i++)
	{
		reachnum = AAS_AreaReachabilityToGoalArea(curareanum, curorigin, goalareanum, travelflags);
		if (!reachnum)
		{
			route->stopevent = RSE_NOROUTE;
			return qfalse;
		} 
		reach = &aasworld.reachability[reachnum];
		if (stopevent & RSE_USETRAVELTYPE)
		{
			if (AAS_TravelFlagForType_inline(reach->traveltype) & stoptfl)
			{
				route->stopevent = RSE_USETRAVELTYPE;
				route->endarea = curareanum;
				route->endcontents = aasworld.areasettings[curareanum].contents;
				route->endtravelflags = AAS_TravelFlagForType_inline(reach->traveltype);
				VectorCopy(reach->start, route->endpos);
				return qtrue;
			} 
			if (AAS_AreaContentsTravelFlags_inline(reach->areanum) & stoptfl)
			{
				route->stopevent = RSE_USETRAVELTYPE;
				route->endarea = reach->areanum;
				route->endcontents = aasworld.areasettings[reach->areanum].contents;
				route->endtravelflags = AAS_AreaContentsTravelFlags_inline(reach->areanum);
				VectorCopy(reach->end, route->endpos);
				route->time += AAS_AreaTravelTime(areanum, origin, reach->start);
				route->time += reach->traveltime;
				return qtrue;
			} 
		} 
		reachareas = &aasworld.reachabilityareas[reachnum];
		for (j = 0; j < reachareas->numareas + 1; j++)
		{
			if (j >= reachareas->numareas)
				testareanum = reach->areanum;
			else
				testareanum = aasworld.reachabilityareaindex[reachareas->firstarea + j];
			if (stopevent & RSE_ENTERCONTENTS)
			{
				if (aasworld.areasettings[testareanum].contents & stopcontents)
				{
					route->stopevent = RSE_ENTERCONTENTS;
					route->endarea = testareanum;
					route->endcontents = aasworld.areasettings[testareanum].contents;
					VectorCopy(reach->end, route->endpos);
					route->time += AAS_AreaTravelTime(areanum, origin, reach->start);
					route->time += reach->traveltime;
					return qtrue;
				} 
			} 
			if (stopevent & RSE_ENTERAREA)
			{
				if (testareanum == stopareanum)
				{
					route->stopevent = RSE_ENTERAREA;
					route->endarea = testareanum;
					route->endcontents = aasworld.areasettings[testareanum].contents;
					VectorCopy(reach->start, route->endpos);
					return qtrue;
				} 
			} 
		} 

		route->time += AAS_AreaTravelTime(areanum, origin, reach->start);
		route->time += reach->traveltime;
		route->endarea = reach->areanum;
		route->endcontents = aasworld.areasettings[reach->areanum].contents;
		route->endtravelflags = AAS_TravelFlagForType_inline(reach->traveltype);
		VectorCopy(reach->end, route->endpos);
		curareanum = reach->areanum;
		VectorCopy(reach->end, curorigin);
		if (maxtime && route->time > maxtime)
			break;
	} 
	if (curareanum != goalareanum)
		return qfalse;
	return qtrue;
} 
int AAS_BridgeWalkable(int areanum)
{
	return qfalse;
} 
void AAS_ReachabilityFromNum(int num, struct aas_reachability_s *reach)
{
	if (!aasworld.initialized)
	{
		Com_Memset(reach, 0, sizeof(aas_reachability_t));
		return;
	} 
	if (num < 0 || num >= aasworld.reachabilitysize)
	{
		Com_Memset(reach, 0, sizeof(aas_reachability_t));
		return;
	} 
	Com_Memcpy(reach, &aasworld.reachability[num], sizeof(aas_reachability_t));;
} 
int AAS_NextAreaReachability(int areanum, int reachnum)
{
	aas_areasettings_t *settings;

	if (!aasworld.initialized) return 0;

	if (areanum <= 0 || areanum >= aasworld.numareas)
	{
		botimport.Print(PRT_ERROR, "AAS_NextAreaReachability: areanum %d out of range\n", areanum);
		return 0;
	} 

	settings = &aasworld.areasettings[areanum];
	if (!reachnum)
	{
		return settings->firstreachablearea;
	} 
	if (reachnum < settings->firstreachablearea)
	{
		botimport.Print(PRT_FATAL, "AAS_NextAreaReachability: reachnum < settings->firstreachableara");
		return 0;
	} 
	reachnum++;
	if (reachnum >= settings->firstreachablearea + settings->numreachableareas)
	{
		return 0;
	} 
	return reachnum;
} 
int AAS_NextModelReachability(int num, int modelnum)
{
	int i;

	if (num <= 0) num = 1;
	else if (num >= aasworld.reachabilitysize) return 0;
	else num++;
	for (i = num; i < aasworld.reachabilitysize; i++)
	{
		if ((aasworld.reachability[i].traveltype & TRAVELTYPE_MASK) == TRAVEL_ELEVATOR)
		{
			if (aasworld.reachability[i].facenum == modelnum) return i;
		} 
		else if ((aasworld.reachability[i].traveltype & TRAVELTYPE_MASK) == TRAVEL_FUNCBOB)
		{
			if ((aasworld.reachability[i].facenum & 0x0000FFFF) == modelnum) return i;
		} 
	} 
	return 0;
} 
int AAS_RandomGoalArea(int areanum, int travelflags, int *goalareanum, vec3_t goalorigin)
{
	int i, n, t;
	vec3_t start, end;
	aas_trace_t trace;

	if (!AAS_AreaReachability(areanum)) return qfalse;
	n = aasworld.numareas * random();
	for (i = 0; i < aasworld.numareas; i++)
	{
		if (n <= 0) n = 1;
		if (n >= aasworld.numareas) n = 1;
		if (AAS_AreaReachability(n))
		{
			t = AAS_AreaTravelTimeToGoalArea(areanum, aasworld.areas[areanum].center, n, travelflags);
			if (t > 0)
			{
				if (AAS_AreaSwim(n))
				{
					*goalareanum = n;
					VectorCopy(aasworld.areas[n].center, goalorigin);
					return qtrue;
				} 
				VectorCopy(aasworld.areas[n].center, start);
				if (!AAS_PointAreaNum(start))
					Log_Write("area %d center %f %f %f in solid?", n, start[0], start[1], start[2]);
				VectorCopy(start, end);
				end[2] -= 300;
				trace = AAS_TraceClientBBox(start, end, PRESENCE_CROUCH, -1);
				if (!trace.startsolid && trace.fraction < 1 && AAS_PointAreaNum(trace.endpos) == n)
				{
					if (AAS_AreaGroundFaceArea(n) > 300)
					{
						*goalareanum = n;
						VectorCopy(trace.endpos, goalorigin);
						return qtrue;
					} 
				} 
			} 
		} 
		n++;
	} 
	return qfalse;
} 
int AAS_AreaVisible(int srcarea, int destarea)
{
	return qfalse;
} 
float DistancePointToLine(vec3_t v1, vec3_t v2, vec3_t point)
{
	vec3_t vec, p2;

	AAS_ProjectPointOntoVector(point, v1, v2, p2);
	VectorSubtract(point, p2, vec);
	return VectorLength(vec);
} 
int AAS_NearestHideArea(int srcnum, vec3_t origin, int areanum, int enemynum, vec3_t enemyorigin, int enemyareanum, int travelflags)
{
	int i, j, nextareanum, badtravelflags, numreach, bestarea;
	unsigned short int t, besttraveltime;
	static unsigned short int *hidetraveltimes;
	aas_routingupdate_t *updateliststart, *updatelistend, *curupdate, *nextupdate;
	aas_reachability_t *reach;
	float dist1, dist2;
	vec3_t v1, v2, p;
	qboolean startVisible;

	if (!hidetraveltimes)
	{
		hidetraveltimes = (unsigned short int *) GetClearedMemory(aasworld.numareas * sizeof(unsigned short int));
	} 
	else
	{
		Com_Memset(hidetraveltimes, 0, aasworld.numareas * sizeof(unsigned short int));
	} 
	besttraveltime = 0;
	bestarea = 0;
	startVisible = qtrue;
	badtravelflags = ~travelflags;
	curupdate = &aasworld.areaupdate[areanum];
	curupdate->areanum = areanum;
	VectorCopy(origin, curupdate->start);
	curupdate->areatraveltimes = aasworld.areatraveltimes[areanum][0];
	curupdate->tmptraveltime = 0;
	curupdate->next = NULL;
	curupdate->prev = NULL;
	updateliststart = curupdate;
	updatelistend = curupdate;
	while (updateliststart)
	{
		curupdate = updateliststart;
		if (curupdate->next) curupdate->next->prev = NULL;
		else updatelistend = NULL;
		updateliststart = curupdate->next;
		curupdate->inlist = qfalse;
		numreach = aasworld.areasettings[curupdate->areanum].numreachableareas;
		reach = &aasworld.reachability[aasworld.areasettings[curupdate->areanum].firstreachablearea];
		for (i = 0; i < numreach; i++, reach++)
		{
			if (AAS_TravelFlagForType_inline(reach->traveltype) & badtravelflags) continue;
			if (AAS_AreaContentsTravelFlags_inline(reach->areanum) & badtravelflags) continue;
			nextareanum = reach->areanum;
			if (nextareanum == enemyareanum) continue;
			t = curupdate->tmptraveltime +
						AAS_AreaTravelTime(curupdate->areanum, curupdate->start, reach->start) +
							reach->traveltime;

			AAS_ProjectPointOntoVector(enemyorigin, curupdate->start, reach->end, p);
			for (j = 0; j < 3; j++)
				if ((p[j] > curupdate->start[j] && p[j] > reach->end[j]) ||
					(p[j] < curupdate->start[j] && p[j] < reach->end[j]))
					break;
			if (j < 3)
			{
				VectorSubtract(enemyorigin, reach->end, v2);
			} 
			else
			{
				VectorSubtract(enemyorigin, p, v2);
			} 
			dist2 = VectorLength(v2);
			if (dist2 < 40) continue;
			VectorSubtract(enemyorigin, curupdate->start, v1);
			dist1 = VectorLength(v1);
			if (dist2 < dist1)
			{
				t += (dist1 - dist2) * 10;
			}
			if (!startVisible && AAS_AreaVisible(enemyareanum, nextareanum)) {
				continue;
			}
			if (besttraveltime && t >= besttraveltime) continue;
			if (!hidetraveltimes[nextareanum] ||
					hidetraveltimes[nextareanum] > t)
			{
				if (!AAS_AreaVisible(enemyareanum, nextareanum))
				{
					besttraveltime = t;
					bestarea = nextareanum;
				} 
				hidetraveltimes[nextareanum] = t;
				nextupdate = &aasworld.areaupdate[nextareanum];
				nextupdate->areanum = nextareanum;
				nextupdate->tmptraveltime = t;
				VectorCopy(reach->end, nextupdate->start);
				if (!nextupdate->inlist)
				{
					nextupdate->next = NULL;
					nextupdate->prev = updatelistend;
					if (updatelistend) updatelistend->next = nextupdate;
					else updateliststart = nextupdate;
					updatelistend = nextupdate;
					nextupdate->inlist = qtrue;
				} 
			} 
		} 
	} 
	return bestarea;
} 
