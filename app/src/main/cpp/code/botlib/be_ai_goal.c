

#include "../qcommon/q_shared.h"
#include "l_utils.h"
#include "l_libvar.h"
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
#include "be_ai_weight.h"
#include "be_ai_goal.h"
#include "be_ai_move.h"

#ifdef RANDOMIZE
#define UNDECIDEDFUZZY
#endif 
#define DROPPEDWEIGHT
#define AVOID_MINIMUM_TIME		10
#define AVOID_DEFAULT_TIME		30
#define AVOID_DROPPED_TIME		10
#define TRAVELTIME_SCALE		0.01
#define IFL_NOTFREE				1		
#define IFL_NOTTEAM				2		
#define IFL_NOTSINGLE			4		
#define IFL_NOTBOT				8		
#define IFL_ROAM				16		

typedef struct maplocation_s
{
	vec3_t origin;
	int areanum;
	char name[MAX_EPAIRKEY];
	struct maplocation_s *next;
} maplocation_t;

typedef struct campspot_s
{
	vec3_t origin;
	int areanum;
	char name[MAX_EPAIRKEY];
	float range;
	float weight;
	float wait;
	float random;
	struct campspot_s *next;
} campspot_t;


typedef struct levelitem_s
{
	int number;							
	int iteminfo;						
	int flags;							
	float weight;						
	vec3_t origin;						
	int goalareanum;					
	vec3_t goalorigin;					
	int entitynum;						
	float timeout;						
	struct levelitem_s *prev, *next;
} levelitem_t;

typedef struct iteminfo_s
{
	char classname[32];					
	char name[MAX_STRINGFIELD];			
	char model[MAX_STRINGFIELD];		
	int modelindex;						
	int type;							
	int index;							
	float respawntime;					
	vec3_t mins;						
	vec3_t maxs;						
	int number;							
} iteminfo_t;

#define ITEMINFO_OFS(x)	(size_t)&(((iteminfo_t *)0)->x)

fielddef_t iteminfo_fields[] =
{
{"name", ITEMINFO_OFS(name), FT_STRING},
{"model", ITEMINFO_OFS(model), FT_STRING},
{"modelindex", ITEMINFO_OFS(modelindex), FT_INT},
{"type", ITEMINFO_OFS(type), FT_INT},
{"index", ITEMINFO_OFS(index), FT_INT},
{"respawntime", ITEMINFO_OFS(respawntime), FT_FLOAT},
{"mins", ITEMINFO_OFS(mins), FT_FLOAT|FT_ARRAY, 3},
{"maxs", ITEMINFO_OFS(maxs), FT_FLOAT|FT_ARRAY, 3},
{NULL, 0, 0}
};

structdef_t iteminfo_struct =
{
	sizeof(iteminfo_t), iteminfo_fields
};

typedef struct itemconfig_s
{
	int numiteminfo;
	iteminfo_t *iteminfo;
} itemconfig_t;

typedef struct bot_goalstate_s
{
	struct weightconfig_s *itemweightconfig;	
	int *itemweightindex;						
	int client;									
	int lastreachabilityarea;					
	bot_goal_t goalstack[MAX_GOALSTACK];		
	int goalstacktop;							
	int avoidgoals[MAX_AVOIDGOALS];				
	float avoidgoaltimes[MAX_AVOIDGOALS];		
} bot_goalstate_t;

bot_goalstate_t *botgoalstates[MAX_CLIENTS + 1]; 
itemconfig_t *itemconfig = NULL;
levelitem_t *levelitemheap = NULL;
levelitem_t *freelevelitems = NULL;
levelitem_t *levelitems = NULL;
int numlevelitems = 0;
maplocation_t *maplocations = NULL;
campspot_t *campspots = NULL;
int g_gametype = 0;
libvar_t *droppedweight = NULL;

bot_goalstate_t *BotGoalStateFromHandle(int handle)
{
	if (handle <= 0 || handle > MAX_CLIENTS)
	{
		botimport.Print(PRT_FATAL, "goal state handle %d out of range\n", handle);
		return NULL;
	} 
	if (!botgoalstates[handle])
	{
		botimport.Print(PRT_FATAL, "invalid goal state %d\n", handle);
		return NULL;
	} 
	return botgoalstates[handle];
} 
void BotInterbreedGoalFuzzyLogic(int parent1, int parent2, int child)
{
	bot_goalstate_t *p1, *p2, *c;

	p1 = BotGoalStateFromHandle(parent1);
	p2 = BotGoalStateFromHandle(parent2);
	c = BotGoalStateFromHandle(child);

	if (!p1 || !p2 || !c)
		return;

	InterbreedWeightConfigs(p1->itemweightconfig, p2->itemweightconfig,
									c->itemweightconfig);
} 
void BotSaveGoalFuzzyLogic(int goalstate, char *filename)
{

} 
void BotMutateGoalFuzzyLogic(int goalstate, float range)
{
	bot_goalstate_t *gs;

	gs = BotGoalStateFromHandle(goalstate);
	if (!gs) return;
	EvolveWeightConfig(gs->itemweightconfig);
} 
itemconfig_t *LoadItemConfig(char *filename)
{
	int max_iteminfo;
	token_t token;
	char path[MAX_QPATH];
	source_t *source;
	itemconfig_t *ic;
	iteminfo_t *ii;

	max_iteminfo = (int) LibVarValue("max_iteminfo", "256");
	if (max_iteminfo < 0)
	{
		botimport.Print(PRT_ERROR, "max_iteminfo = %d\n", max_iteminfo);
		max_iteminfo = 256;
		LibVarSet( "max_iteminfo", "256" );
	}

	Q_strncpyz(path, filename, sizeof(path));
	PC_SetBaseFolder(BOTFILESBASEFOLDER);
	source = LoadSourceFile( path );
	if( !source ) {
		botimport.Print( PRT_ERROR, "couldn't load %s\n", path );
		return NULL;
	} 
	ic = (itemconfig_t *) GetClearedHunkMemory(sizeof(itemconfig_t) +
														max_iteminfo * sizeof(iteminfo_t));
	ic->iteminfo = (iteminfo_t *) ((char *) ic + sizeof(itemconfig_t));
	ic->numiteminfo = 0;
	while(PC_ReadToken(source, &token))
	{
		if (!strcmp(token.string, "iteminfo"))
		{
			if (ic->numiteminfo >= max_iteminfo)
			{
				SourceError(source, "more than %d item info defined", max_iteminfo);
				FreeMemory(ic);
				FreeSource(source);
				return NULL;
			} 
			ii = &ic->iteminfo[ic->numiteminfo];
			Com_Memset(ii, 0, sizeof(iteminfo_t));
			if (!PC_ExpectTokenType(source, TT_STRING, 0, &token))
			{
				FreeMemory(ic);
				FreeSource(source);
				return NULL;
			} 
			StripDoubleQuotes(token.string);
			Q_strncpyz(ii->classname, token.string, sizeof(ii->classname));
			if (!ReadStructure(source, &iteminfo_struct, (char *) ii))
			{
				FreeMemory(ic);
				FreeSource(source);
				return NULL;
			} 
			ii->number = ic->numiteminfo;
			ic->numiteminfo++;
		} 
		else
		{
			SourceError(source, "unknown definition %s", token.string);
			FreeMemory(ic);
			FreeSource(source);
			return NULL;
		} 
	} 
	FreeSource(source);
	if (!ic->numiteminfo) botimport.Print(PRT_WARNING, "no item info loaded\n");
	botimport.Print(PRT_MESSAGE, "loaded %s\n", path);
	return ic;
} 
int *ItemWeightIndex(weightconfig_t *iwc, itemconfig_t *ic)
{
	int *index, i;

	index = (int *) GetClearedMemory(sizeof(int) * ic->numiteminfo);

	for (i = 0; i < ic->numiteminfo; i++)
	{
		index[i] = FindFuzzyWeight(iwc, ic->iteminfo[i].classname);
		if (index[i] < 0)
		{
			Log_Write("item info %d \"%s\" has no fuzzy weight\r\n", i, ic->iteminfo[i].classname);
		} 
	} 
	return index;
} 
void InitLevelItemHeap(void)
{
	int i, max_levelitems;

	if (levelitemheap) FreeMemory(levelitemheap);

	max_levelitems = (int) LibVarValue("max_levelitems", "256");
	levelitemheap = (levelitem_t *) GetClearedMemory(max_levelitems * sizeof(levelitem_t));

	for (i = 0; i < max_levelitems-1; i++)
	{
		levelitemheap[i].next = &levelitemheap[i + 1];
	} 
	levelitemheap[max_levelitems-1].next = NULL;
	freelevelitems = levelitemheap;
} 
levelitem_t *AllocLevelItem(void)
{
	levelitem_t *li;

	li = freelevelitems;
	if (!li)
	{
		botimport.Print(PRT_FATAL, "out of level items\n");
		return NULL;
	} 
	freelevelitems = freelevelitems->next;
	Com_Memset(li, 0, sizeof(levelitem_t));
	return li;
} 
void FreeLevelItem(levelitem_t *li)
{
	li->next = freelevelitems;
	freelevelitems = li;
} 
void AddLevelItemToList(levelitem_t *li)
{
	if (levelitems) levelitems->prev = li;
	li->prev = NULL;
	li->next = levelitems;
	levelitems = li;
} 
void RemoveLevelItemFromList(levelitem_t *li)
{
	if (li->prev) li->prev->next = li->next;
	else levelitems = li->next;
	if (li->next) li->next->prev = li->prev;
} 
void BotFreeInfoEntities(void)
{
	maplocation_t *ml, *nextml;
	campspot_t *cs, *nextcs;

	for (ml = maplocations; ml; ml = nextml)
	{
		nextml = ml->next;
		FreeMemory(ml);
	} 
	maplocations = NULL;
	for (cs = campspots; cs; cs = nextcs)
	{
		nextcs = cs->next;
		FreeMemory(cs);
	} 
	campspots = NULL;
} 
void BotInitInfoEntities(void)
{
	char classname[MAX_EPAIRKEY];
	maplocation_t *ml;
	campspot_t *cs;
	int ent, numlocations, numcampspots;

	BotFreeInfoEntities();
	numlocations = 0;
	numcampspots = 0;
	for (ent = AAS_NextBSPEntity(0); ent; ent = AAS_NextBSPEntity(ent))
	{
		if (!AAS_ValueForBSPEpairKey(ent, "classname", classname, MAX_EPAIRKEY)) continue;

		if (!strcmp(classname, "target_location"))
		{
			ml = (maplocation_t *) GetClearedMemory(sizeof(maplocation_t));
			AAS_VectorForBSPEpairKey(ent, "origin", ml->origin);
			AAS_ValueForBSPEpairKey(ent, "message", ml->name, sizeof(ml->name));
			ml->areanum = AAS_PointAreaNum(ml->origin);
			ml->next = maplocations;
			maplocations = ml;
			numlocations++;
		} 
		else if (!strcmp(classname, "info_camp"))
		{
			cs = (campspot_t *) GetClearedMemory(sizeof(campspot_t));
			AAS_VectorForBSPEpairKey(ent, "origin", cs->origin);
			AAS_ValueForBSPEpairKey(ent, "message", cs->name, sizeof(cs->name));
			AAS_FloatForBSPEpairKey(ent, "range", &cs->range);
			AAS_FloatForBSPEpairKey(ent, "weight", &cs->weight);
			AAS_FloatForBSPEpairKey(ent, "wait", &cs->wait);
			AAS_FloatForBSPEpairKey(ent, "random", &cs->random);
			cs->areanum = AAS_PointAreaNum(cs->origin);
			if (!cs->areanum)
			{
				botimport.Print(PRT_MESSAGE, "camp spot at %1.1f %1.1f %1.1f in solid\n", cs->origin[0], cs->origin[1], cs->origin[2]);
				FreeMemory(cs);
				continue;
			} 
			cs->next = campspots;
			campspots = cs;
			numcampspots++;
		} 
	} 
	if (botDeveloper)
	{
		botimport.Print(PRT_MESSAGE, "%d map locations\n", numlocations);
		botimport.Print(PRT_MESSAGE, "%d camp spots\n", numcampspots);
	} 
} 
void BotInitLevelItems(void)
{
	int i, spawnflags, value;
	char classname[MAX_EPAIRKEY];
	vec3_t origin, end;
	int ent, goalareanum;
	itemconfig_t *ic;
	levelitem_t *li;
	bsp_trace_t trace;

	BotInitInfoEntities();

	InitLevelItemHeap();
	levelitems = NULL;
	numlevelitems = 0;
	ic = itemconfig;
	if (!ic) return;

	if (!AAS_Loaded()) return;

	for (i = 0; i < ic->numiteminfo; i++)
	{
		if (!ic->iteminfo[i].modelindex)
		{
			Log_Write("item %s has modelindex 0", ic->iteminfo[i].classname);
		} 
	} 

	for (ent = AAS_NextBSPEntity(0); ent; ent = AAS_NextBSPEntity(ent))
	{
		if (!AAS_ValueForBSPEpairKey(ent, "classname", classname, MAX_EPAIRKEY)) continue;
		spawnflags = 0;
		AAS_IntForBSPEpairKey(ent, "spawnflags", &spawnflags);
		for (i = 0; i < ic->numiteminfo; i++)
		{
			if (!strcmp(classname, ic->iteminfo[i].classname)) break;
		} 
		if (i >= ic->numiteminfo)
		{
			Log_Write("entity %s unknown item\r\n", classname);
			continue;
		} 
		if (!AAS_VectorForBSPEpairKey(ent, "origin", origin))
		{
			botimport.Print(PRT_ERROR, "item %s without origin\n", classname);
			continue;
		} 
		goalareanum = 0;
		if (spawnflags & 1)
		{
			if (!(AAS_PointContents(origin) & CONTENTS_WATER))
			{
				VectorCopy(origin, end);
				end[2] -= 32;
				trace = AAS_Trace(origin, ic->iteminfo[i].mins, ic->iteminfo[i].maxs, end, -1, CONTENTS_SOLID|CONTENTS_PLAYERCLIP);
				if (trace.fraction >= 1)
				{
					goalareanum = AAS_BestReachableFromJumpPadArea(origin, ic->iteminfo[i].mins, ic->iteminfo[i].maxs);
					Log_Write("item %s reachable from jumppad area %d\r\n", ic->iteminfo[i].classname, goalareanum);
					if (!goalareanum) continue;
				} 
			} 
		} 

		li = AllocLevelItem();
		if (!li) return;
		li->number = ++numlevelitems;
		li->timeout = 0;
		li->entitynum = 0;
		li->flags = 0;
		AAS_IntForBSPEpairKey(ent, "notfree", &value);
		if (value) li->flags |= IFL_NOTFREE;
		AAS_IntForBSPEpairKey(ent, "notteam", &value);
		if (value) li->flags |= IFL_NOTTEAM;
		AAS_IntForBSPEpairKey(ent, "notsingle", &value);
		if (value) li->flags |= IFL_NOTSINGLE;
		AAS_IntForBSPEpairKey(ent, "notbot", &value);
		if (value) li->flags |= IFL_NOTBOT;
		if (!strcmp(classname, "item_botroam"))
		{
			li->flags |= IFL_ROAM;
			AAS_FloatForBSPEpairKey(ent, "weight", &li->weight);
		} 
		if (!(spawnflags & 1))
		{
			if (!AAS_DropToFloor(origin, ic->iteminfo[i].mins, ic->iteminfo[i].maxs))
			{
				botimport.Print(PRT_MESSAGE, "%s in solid at (%1.1f %1.1f %1.1f)\n",
												classname, origin[0], origin[1], origin[2]);
			} 
		} 
		li->iteminfo = i;
		VectorCopy(origin, li->origin);
		if (goalareanum)
		{
			li->goalareanum = goalareanum;
			VectorCopy(origin, li->goalorigin);
		} 
		else
		{
			li->goalareanum = AAS_BestReachableArea(origin,
							ic->iteminfo[i].mins, ic->iteminfo[i].maxs,
							li->goalorigin);
			if (!li->goalareanum)
			{
				botimport.Print(PRT_MESSAGE, "%s not reachable for bots at (%1.1f %1.1f %1.1f)\n",
												classname, origin[0], origin[1], origin[2]);
			} 
		} 
		AddLevelItemToList(li);
	} 
	botimport.Print(PRT_MESSAGE, "found %d level items\n", numlevelitems);
} 
void BotGoalName(int number, char *name, int size)
{
	levelitem_t *li;

	if (!itemconfig) return;
	for (li = levelitems; li; li = li->next)
	{
		if (li->number == number)
		{
			Q_strncpyz(name, itemconfig->iteminfo[li->iteminfo].name, size);
			return;
		} 
	} 
	strcpy(name, "");
} 
void BotResetAvoidGoals(int goalstate)
{
	bot_goalstate_t *gs;

	gs = BotGoalStateFromHandle(goalstate);
	if (!gs) return;
	Com_Memset(gs->avoidgoals, 0, MAX_AVOIDGOALS * sizeof(int));
	Com_Memset(gs->avoidgoaltimes, 0, MAX_AVOIDGOALS * sizeof(float));
} 
void BotDumpAvoidGoals(int goalstate)
{
	int i;
	bot_goalstate_t *gs;
	char name[32];

	gs = BotGoalStateFromHandle(goalstate);
	if (!gs) return;
	for (i = 0; i < MAX_AVOIDGOALS; i++)
	{
		if (gs->avoidgoaltimes[i] >= AAS_Time())
		{
			BotGoalName(gs->avoidgoals[i], name, 32);
			Log_Write("avoid goal %s, number %d for %f seconds", name,
				gs->avoidgoals[i], gs->avoidgoaltimes[i] - AAS_Time());
		} 
	} 
} 
void BotAddToAvoidGoals(bot_goalstate_t *gs, int number, float avoidtime)
{
	int i;

	for (i = 0; i < MAX_AVOIDGOALS; i++)
	{
		if (gs->avoidgoals[i] == number)
		{
			gs->avoidgoals[i] = number;
			gs->avoidgoaltimes[i] = AAS_Time() + avoidtime;
			return;
		} 
	} 

	for (i = 0; i < MAX_AVOIDGOALS; i++)
	{
		if (gs->avoidgoaltimes[i] < AAS_Time())
		{
			gs->avoidgoals[i] = number;
			gs->avoidgoaltimes[i] = AAS_Time() + avoidtime;
			return;
		} 
	} 
} 
void BotRemoveFromAvoidGoals(int goalstate, int number)
{
	int i;
	bot_goalstate_t *gs;

	gs = BotGoalStateFromHandle(goalstate);
	if (!gs) return;
	for (i = 0; i < MAX_AVOIDGOALS; i++)
	{
		if (gs->avoidgoals[i] == number && gs->avoidgoaltimes[i] >= AAS_Time())
		{
			gs->avoidgoaltimes[i] = 0;
			return;
		} 
	} 
} 
float BotAvoidGoalTime(int goalstate, int number)
{
	int i;
	bot_goalstate_t *gs;

	gs = BotGoalStateFromHandle(goalstate);
	if (!gs) return 0;
	for (i = 0; i < MAX_AVOIDGOALS; i++)
	{
		if (gs->avoidgoals[i] == number && gs->avoidgoaltimes[i] >= AAS_Time())
		{
			return gs->avoidgoaltimes[i] - AAS_Time();
		} 
	} 
	return 0;
} 
void BotSetAvoidGoalTime(int goalstate, int number, float avoidtime)
{
	bot_goalstate_t *gs;
	levelitem_t *li;

	gs = BotGoalStateFromHandle(goalstate);
	if (!gs)
		return;
	if (avoidtime < 0)
	{
		if (!itemconfig)
			return;
		for (li = levelitems; li; li = li->next)
		{
			if (li->number == number)
			{
				avoidtime = itemconfig->iteminfo[li->iteminfo].respawntime;
				if (!avoidtime)
					avoidtime = AVOID_DEFAULT_TIME;
				if (avoidtime < AVOID_MINIMUM_TIME)
					avoidtime = AVOID_MINIMUM_TIME;
				BotAddToAvoidGoals(gs, number, avoidtime);
				return;
			} 
		} 
		return;
	} 
	else
	{
		BotAddToAvoidGoals(gs, number, avoidtime);
	} 
} 
int BotGetLevelItemGoal(int index, char *name, bot_goal_t *goal)
{
	levelitem_t *li;

	if (!itemconfig) return -1;
	li = levelitems;
	if (index >= 0)
	{
		for (; li; li = li->next)
		{
			if (li->number == index)
			{
				li = li->next;
				break;
			} 
		} 
	} 
	for (; li; li = li->next)
	{
		if (g_gametype == GT_SINGLE_PLAYER) {
			if (li->flags & IFL_NOTSINGLE) continue;
		}
		else if (g_gametype >= GT_TEAM) {
			if (li->flags & IFL_NOTTEAM) continue;
		}
		else {
			if (li->flags & IFL_NOTFREE) continue;
		}
		if (li->flags & IFL_NOTBOT) continue;
		if (!Q_stricmp(name, itemconfig->iteminfo[li->iteminfo].name))
		{
			goal->areanum = li->goalareanum;
			VectorCopy(li->goalorigin, goal->origin);
			goal->entitynum = li->entitynum;
			VectorCopy(itemconfig->iteminfo[li->iteminfo].mins, goal->mins);
			VectorCopy(itemconfig->iteminfo[li->iteminfo].maxs, goal->maxs);
			goal->number = li->number;
			goal->flags = GFL_ITEM;
			if (li->timeout) goal->flags |= GFL_DROPPED;
			goal->iteminfo = li->iteminfo;
			return li->number;
		} 
	} 
	return -1;
} 
int BotGetMapLocationGoal(char *name, bot_goal_t *goal)
{
	maplocation_t *ml;
	vec3_t mins = {-8, -8, -8}, maxs = {8, 8, 8};

	for (ml = maplocations; ml; ml = ml->next)
	{
		if (!Q_stricmp(ml->name, name))
		{
			goal->areanum = ml->areanum;
			VectorCopy(ml->origin, goal->origin);
			goal->entitynum = 0;
			VectorCopy(mins, goal->mins);
			VectorCopy(maxs, goal->maxs);
			goal->number = 0;
			goal->flags = 0;
			goal->iteminfo = 0;
			return qtrue;
		} 
	} 
	return qfalse;
} 
int BotGetNextCampSpotGoal(int num, bot_goal_t *goal)
{
	int i;
	campspot_t *cs;
	vec3_t mins = {-8, -8, -8}, maxs = {8, 8, 8};

	if (num < 0) num = 0;
	i = num;
	for (cs = campspots; cs; cs = cs->next)
	{
		if (--i < 0)
		{
			goal->areanum = cs->areanum;
			VectorCopy(cs->origin, goal->origin);
			goal->entitynum = 0;
			VectorCopy(mins, goal->mins);
			VectorCopy(maxs, goal->maxs);
			goal->number = 0;
			goal->flags = 0;
			goal->iteminfo = 0;
			return num+1;
		} 
	} 
	return 0;
} 
void BotFindEntityForLevelItem(levelitem_t *li)
{
	int ent, modelindex;
	itemconfig_t *ic;
	aas_entityinfo_t entinfo;
	vec3_t dir;

	ic = itemconfig;
	if (!itemconfig) return;
	for (ent = AAS_NextEntity(0); ent; ent = AAS_NextEntity(ent))
	{
		modelindex = AAS_EntityModelindex(ent);
		if (!modelindex) continue;
		AAS_EntityInfo(ent, &entinfo);
		if (entinfo.origin[0] != entinfo.lastvisorigin[0] ||
				entinfo.origin[1] != entinfo.lastvisorigin[1] ||
				entinfo.origin[2] != entinfo.lastvisorigin[2]) continue;
		if (ic->iteminfo[li->iteminfo].modelindex == modelindex)
		{
			VectorSubtract(li->origin, entinfo.origin, dir);
			if (VectorLength(dir) < 30)
			{
				li->entitynum = ent;
			} 
		} 
	} 
} 

#define ET_ITEM			2

void BotUpdateEntityItems(void)
{
	int ent, i, modelindex;
	vec3_t dir;
	levelitem_t *li, *nextli;
	aas_entityinfo_t entinfo;
	itemconfig_t *ic;

	for (li = levelitems; li; li = nextli)
	{
		nextli = li->next;
		if (li->timeout)
		{
			if (li->timeout < AAS_Time())
			{
				RemoveLevelItemFromList(li);
				FreeLevelItem(li);
			} 
		} 
	} 
	ic = itemconfig;
	if (!itemconfig) return;
	for (ent = AAS_NextEntity(0); ent; ent = AAS_NextEntity(ent))
	{
		if (AAS_EntityType(ent) != ET_ITEM) continue;
		modelindex = AAS_EntityModelindex(ent);
		if (!modelindex) continue;
		AAS_EntityInfo(ent, &entinfo);
		if (entinfo.origin[0] != entinfo.lastvisorigin[0] ||
				entinfo.origin[1] != entinfo.lastvisorigin[1] ||
				entinfo.origin[2] != entinfo.lastvisorigin[2]) continue;
		for (li = levelitems; li; li = li->next)
		{
			if (li->entitynum && li->entitynum == ent)
			{
				if (ic->iteminfo[li->iteminfo].modelindex != modelindex)
				{
					RemoveLevelItemFromList(li);
					FreeLevelItem(li);
					li = NULL;
					break;
				} 
				else
				{
					if (entinfo.origin[0] != li->origin[0] ||
						entinfo.origin[1] != li->origin[1] ||
						entinfo.origin[2] != li->origin[2])
					{
						VectorCopy(entinfo.origin, li->origin);
						li->goalareanum = AAS_BestReachableArea(li->origin,
										ic->iteminfo[li->iteminfo].mins, ic->iteminfo[li->iteminfo].maxs,
										li->goalorigin);
					} 
					break;
				} 
			} 
		} 
		if (li) continue;
		for (li = levelitems; li; li = li->next)
		{
			if (li->entitynum) continue;
			if (g_gametype == GT_SINGLE_PLAYER) {
				if (li->flags & IFL_NOTSINGLE) continue;
			}
			else if (g_gametype >= GT_TEAM) {
				if (li->flags & IFL_NOTTEAM) continue;
			}
			else {
				if (li->flags & IFL_NOTFREE) continue;
			}
			if (ic->iteminfo[li->iteminfo].modelindex == modelindex)
			{
				VectorSubtract(li->origin, entinfo.origin, dir);
				if (VectorLength(dir) < 30)
				{
					li->entitynum = ent;
					if (entinfo.origin[0] != li->origin[0] ||
						entinfo.origin[1] != li->origin[1] ||
						entinfo.origin[2] != li->origin[2])
					{
						VectorCopy(entinfo.origin, li->origin);
						li->goalareanum = AAS_BestReachableArea(li->origin,
										ic->iteminfo[li->iteminfo].mins, ic->iteminfo[li->iteminfo].maxs,
										li->goalorigin);
					} 
#ifdef DEBUG
					Log_Write("linked item %s to an entity", ic->iteminfo[li->iteminfo].classname);
#endif 
					break;
				} 
			} 
		} 
		if (li) continue;
		for (i = 0; i < ic->numiteminfo; i++)
		{
			if (ic->iteminfo[i].modelindex == modelindex)
			{
				break;
			} 
		} 
		if (i >= ic->numiteminfo) continue;
		li = AllocLevelItem();
		if (!li) continue;
		li->entitynum = ent;
		li->number = numlevelitems + ent;
		li->iteminfo = i;
		VectorCopy(entinfo.origin, li->origin);
		li->goalareanum = AAS_BestReachableArea(li->origin,
									ic->iteminfo[i].mins, ic->iteminfo[i].maxs,
									li->goalorigin);
		if (AAS_AreaJumpPad(li->goalareanum))
		{
			FreeLevelItem(li);
			continue;
		} 
		li->timeout = AAS_Time() + 30;
		AddLevelItemToList(li);
	} 
} 
void BotDumpGoalStack(int goalstate)
{
	int i;
	bot_goalstate_t *gs;
	char name[32];

	gs = BotGoalStateFromHandle(goalstate);
	if (!gs) return;
	for (i = 1; i <= gs->goalstacktop; i++)
	{
		BotGoalName(gs->goalstack[i].number, name, 32);
		Log_Write("%d: %s", i, name);
	} 
} 
void BotPushGoal(int goalstate, bot_goal_t *goal)
{
	bot_goalstate_t *gs;

	gs = BotGoalStateFromHandle(goalstate);
	if (!gs) return;
	if (gs->goalstacktop >= MAX_GOALSTACK-1)
	{
		botimport.Print(PRT_ERROR, "goal heap overflow\n");
		BotDumpGoalStack(goalstate);
		return;
	} 
	gs->goalstacktop++;
	Com_Memcpy(&gs->goalstack[gs->goalstacktop], goal, sizeof(bot_goal_t));
} 
void BotPopGoal(int goalstate)
{
	bot_goalstate_t *gs;

	gs = BotGoalStateFromHandle(goalstate);
	if (!gs) return;
	if (gs->goalstacktop > 0) gs->goalstacktop--;
} 
void BotEmptyGoalStack(int goalstate)
{
	bot_goalstate_t *gs;

	gs = BotGoalStateFromHandle(goalstate);
	if (!gs) return;
	gs->goalstacktop = 0;
} 
int BotGetTopGoal(int goalstate, bot_goal_t *goal)
{
	bot_goalstate_t *gs;

	gs = BotGoalStateFromHandle(goalstate);
	if (!gs) return qfalse;
	if (!gs->goalstacktop) return qfalse;
	Com_Memcpy(goal, &gs->goalstack[gs->goalstacktop], sizeof(bot_goal_t));
	return qtrue;
} 
int BotGetSecondGoal(int goalstate, bot_goal_t *goal)
{
	bot_goalstate_t *gs;

	gs = BotGoalStateFromHandle(goalstate);
	if (!gs) return qfalse;
	if (gs->goalstacktop <= 1) return qfalse;
	Com_Memcpy(goal, &gs->goalstack[gs->goalstacktop-1], sizeof(bot_goal_t));
	return qtrue;
} 
int BotChooseLTGItem(int goalstate, vec3_t origin, int *inventory, int travelflags)
{
	int areanum, t, weightnum;
	float weight, bestweight, avoidtime;
	iteminfo_t *iteminfo;
	itemconfig_t *ic;
	levelitem_t *li, *bestitem;
	bot_goal_t goal;
	bot_goalstate_t *gs;

	gs = BotGoalStateFromHandle(goalstate);
	if (!gs)
		return qfalse;
	if (!gs->itemweightconfig)
		return qfalse;
	areanum = BotReachabilityArea(origin, gs->client);
	if (!areanum || !AAS_AreaReachability(areanum))
	{
		areanum = gs->lastreachabilityarea;
	} 
	gs->lastreachabilityarea = areanum;
	if (!areanum)
		return qfalse;
	ic = itemconfig;
	if (!itemconfig)
		return qfalse;
	bestweight = 0;
	bestitem = NULL;
	Com_Memset(&goal, 0, sizeof(bot_goal_t));
	for (li = levelitems; li; li = li->next)
	{
		if (g_gametype == GT_SINGLE_PLAYER) {
			if (li->flags & IFL_NOTSINGLE)
				continue;
		}
		else if (g_gametype >= GT_TEAM) {
			if (li->flags & IFL_NOTTEAM)
				continue;
		}
		else {
			if (li->flags & IFL_NOTFREE)
				continue;
		}
		if (li->flags & IFL_NOTBOT)
			continue;
		if (!li->goalareanum)
			continue;
		//FIXME: is this a good thing? added this for items that never spawned into the game (f.i. CTF flags in obelisk)
		if (!li->entitynum && !(li->flags & IFL_ROAM))
			continue;
		iteminfo = &ic->iteminfo[li->iteminfo];
		weightnum = gs->itemweightindex[iteminfo->number];
		if (weightnum < 0)
			continue;

#ifdef UNDECIDEDFUZZY
		weight = FuzzyWeightUndecided(inventory, gs->itemweightconfig, weightnum);
#else
		weight = FuzzyWeight(inventory, gs->itemweightconfig, weightnum);
#endif 
#ifdef DROPPEDWEIGHT
		if (li->timeout)
			weight += droppedweight->value;
#endif 
		if (li->flags & IFL_ROAM) weight *= li->weight;
		if (weight > 0)
		{
			t = AAS_AreaTravelTimeToGoalArea(areanum, origin, li->goalareanum, travelflags);
			if (t > 0)
			{
				avoidtime = BotAvoidGoalTime(goalstate, li->number);
				if (avoidtime - t * 0.009 > 0)
					continue;
				weight /= (float) t * TRAVELTIME_SCALE;
				if (weight > bestweight)
				{
					bestweight = weight;
					bestitem = li;
				} 
			} 
		} 
	} 
	if (!bestitem)
	{
		return qfalse;
	} 
	iteminfo = &ic->iteminfo[bestitem->iteminfo];
	VectorCopy(bestitem->goalorigin, goal.origin);
	VectorCopy(iteminfo->mins, goal.mins);
	VectorCopy(iteminfo->maxs, goal.maxs);
	goal.areanum = bestitem->goalareanum;
	goal.entitynum = bestitem->entitynum;
	goal.number = bestitem->number;
	goal.flags = GFL_ITEM;
	if (bestitem->timeout)
		goal.flags |= GFL_DROPPED;
	if (bestitem->flags & IFL_ROAM)
		goal.flags |= GFL_ROAM;
	goal.iteminfo = bestitem->iteminfo;
	if (bestitem->timeout)
	{
		avoidtime = AVOID_DROPPED_TIME;
	} 
	else
	{
		avoidtime = iteminfo->respawntime;
		if (!avoidtime)
			avoidtime = AVOID_DEFAULT_TIME;
		if (avoidtime < AVOID_MINIMUM_TIME)
			avoidtime = AVOID_MINIMUM_TIME;
	} 
	BotAddToAvoidGoals(gs, bestitem->number, avoidtime);
	BotPushGoal(goalstate, &goal);
	return qtrue;
} 
int BotChooseNBGItem(int goalstate, vec3_t origin, int *inventory, int travelflags,
														bot_goal_t *ltg, float maxtime)
{
	int areanum, t, weightnum, ltg_time;
	float weight, bestweight, avoidtime;
	iteminfo_t *iteminfo;
	itemconfig_t *ic;
	levelitem_t *li, *bestitem;
	bot_goal_t goal;
	bot_goalstate_t *gs;

	gs = BotGoalStateFromHandle(goalstate);
	if (!gs)
		return qfalse;
	if (!gs->itemweightconfig)
		return qfalse;
	areanum = BotReachabilityArea(origin, gs->client);
	if (!areanum || !AAS_AreaReachability(areanum))
	{
		areanum = gs->lastreachabilityarea;
	} 
	gs->lastreachabilityarea = areanum;
	if (!areanum)
		return qfalse;
	if (ltg) ltg_time = AAS_AreaTravelTimeToGoalArea(areanum, origin, ltg->areanum, travelflags);
	else ltg_time = 99999;
	ic = itemconfig;
	if (!itemconfig)
		return qfalse;
	bestweight = 0;
	bestitem = NULL;
	Com_Memset(&goal, 0, sizeof(bot_goal_t));
	for (li = levelitems; li; li = li->next)
	{
		if (g_gametype == GT_SINGLE_PLAYER) {
			if (li->flags & IFL_NOTSINGLE)
				continue;
		}
		else if (g_gametype >= GT_TEAM) {
			if (li->flags & IFL_NOTTEAM)
				continue;
		}
		else {
			if (li->flags & IFL_NOTFREE)
				continue;
		}
		if (li->flags & IFL_NOTBOT)
			continue;
		if (!li->goalareanum)
			continue;
		//FIXME: is this a good thing? added this for items that never spawned into the game (f.i. CTF flags in obelisk)
		if (!li->entitynum && !(li->flags & IFL_ROAM))
			continue;
		iteminfo = &ic->iteminfo[li->iteminfo];
		weightnum = gs->itemweightindex[iteminfo->number];
		if (weightnum < 0)
			continue;
#ifdef UNDECIDEDFUZZY
		weight = FuzzyWeightUndecided(inventory, gs->itemweightconfig, weightnum);
#else
		weight = FuzzyWeight(inventory, gs->itemweightconfig, weightnum);
#endif 
#ifdef DROPPEDWEIGHT
		if (li->timeout)
			weight += droppedweight->value;
#endif 
		if (li->flags & IFL_ROAM) weight *= li->weight;
		if (weight > 0)
		{
			t = AAS_AreaTravelTimeToGoalArea(areanum, origin, li->goalareanum, travelflags);
			if (t > 0 && t < maxtime)
			{
				avoidtime = BotAvoidGoalTime(goalstate, li->number);
				if (avoidtime - t * 0.009 > 0)
					continue;
				weight /= (float) t * TRAVELTIME_SCALE;
				if (weight > bestweight)
				{
					t = 0;
					if (ltg && !li->timeout)
					{
						t = AAS_AreaTravelTimeToGoalArea(li->goalareanum, li->goalorigin, ltg->areanum, travelflags);
					} 
					if (t <= ltg_time)
					{
						bestweight = weight;
						bestitem = li;
					} 
				} 
			} 
		} 
	} 
	if (!bestitem)
		return qfalse;
	iteminfo = &ic->iteminfo[bestitem->iteminfo];
	VectorCopy(bestitem->goalorigin, goal.origin);
	VectorCopy(iteminfo->mins, goal.mins);
	VectorCopy(iteminfo->maxs, goal.maxs);
	goal.areanum = bestitem->goalareanum;
	goal.entitynum = bestitem->entitynum;
	goal.number = bestitem->number;
	goal.flags = GFL_ITEM;
	if (bestitem->timeout)
		goal.flags |= GFL_DROPPED;
	if (bestitem->flags & IFL_ROAM)
		goal.flags |= GFL_ROAM;
	goal.iteminfo = bestitem->iteminfo;
	if (bestitem->timeout)
	{
		avoidtime = AVOID_DROPPED_TIME;
	} 
	else
	{
		avoidtime = iteminfo->respawntime;
		if (!avoidtime)
			avoidtime = AVOID_DEFAULT_TIME;
		if (avoidtime < AVOID_MINIMUM_TIME)
			avoidtime = AVOID_MINIMUM_TIME;
	} 
	BotAddToAvoidGoals(gs, bestitem->number, avoidtime);
	BotPushGoal(goalstate, &goal);
	return qtrue;
} 
int BotTouchingGoal(vec3_t origin, bot_goal_t *goal)
{
	int i;
	vec3_t boxmins, boxmaxs;
	vec3_t absmins, absmaxs;
	vec3_t safety_maxs = {0, 0, 0}; 
	vec3_t safety_mins = {0, 0, 0}; 

	AAS_PresenceTypeBoundingBox(PRESENCE_NORMAL, boxmins, boxmaxs);
	VectorSubtract(goal->mins, boxmaxs, absmins);
	VectorSubtract(goal->maxs, boxmins, absmaxs);
	VectorAdd(absmins, goal->origin, absmins);
	VectorAdd(absmaxs, goal->origin, absmaxs);
	VectorSubtract(absmaxs, safety_maxs, absmaxs);
	VectorSubtract(absmins, safety_mins, absmins);

	for (i = 0; i < 3; i++)
	{
		if (origin[i] < absmins[i] || origin[i] > absmaxs[i]) return qfalse;
	} 
	return qtrue;
} 
int BotItemGoalInVisButNotVisible(int viewer, vec3_t eye, vec3_t viewangles, bot_goal_t *goal)
{
	aas_entityinfo_t entinfo;
	bsp_trace_t trace;
	vec3_t middle;

	if (!(goal->flags & GFL_ITEM)) return qfalse;
	VectorAdd(goal->mins, goal->mins, middle);
	VectorScale(middle, 0.5, middle);
	VectorAdd(goal->origin, middle, middle);
	trace = AAS_Trace(eye, NULL, NULL, middle, viewer, CONTENTS_SOLID);
	if (trace.fraction >= 1)
	{
		if (goal->entitynum <= 0)
			return qfalse;
		AAS_EntityInfo(goal->entitynum, &entinfo);
		if (entinfo.ltime < AAS_Time() - 0.5)
			return qtrue;
	} 
	return qfalse;
} 
void BotResetGoalState(int goalstate)
{
	bot_goalstate_t *gs;

	gs = BotGoalStateFromHandle(goalstate);
	if (!gs) return;
	Com_Memset(gs->goalstack, 0, MAX_GOALSTACK * sizeof(bot_goal_t));
	gs->goalstacktop = 0;
	BotResetAvoidGoals(goalstate);
} 
int BotLoadItemWeights(int goalstate, char *filename)
{
	bot_goalstate_t *gs;

	gs = BotGoalStateFromHandle(goalstate);
	if (!gs) return BLERR_CANNOTLOADITEMWEIGHTS;
	gs->itemweightconfig = ReadWeightConfig(filename);
	if (!gs->itemweightconfig)
	{
		botimport.Print(PRT_FATAL, "couldn't load weights\n");
		return BLERR_CANNOTLOADITEMWEIGHTS;
	} 
	if (!itemconfig) return BLERR_CANNOTLOADITEMWEIGHTS;
	gs->itemweightindex = ItemWeightIndex(gs->itemweightconfig, itemconfig);
	return BLERR_NOERROR;
} 
void BotFreeItemWeights(int goalstate)
{
	bot_goalstate_t *gs;

	gs = BotGoalStateFromHandle(goalstate);
	if (!gs) return;
	if (gs->itemweightconfig) FreeWeightConfig(gs->itemweightconfig);
	if (gs->itemweightindex) FreeMemory(gs->itemweightindex);
} 
int BotAllocGoalState(int client)
{
	int i;

	for (i = 1; i <= MAX_CLIENTS; i++)
	{
		if (!botgoalstates[i])
		{
			botgoalstates[i] = GetClearedMemory(sizeof(bot_goalstate_t));
			botgoalstates[i]->client = client;
			return i;
		} 
	} 
	return 0;
} 
void BotFreeGoalState(int handle)
{
	if (handle <= 0 || handle > MAX_CLIENTS)
	{
		botimport.Print(PRT_FATAL, "goal state handle %d out of range\n", handle);
		return;
	} 
	if (!botgoalstates[handle])
	{
		botimport.Print(PRT_FATAL, "invalid goal state handle %d\n", handle);
		return;
	} 
	BotFreeItemWeights(handle);
	FreeMemory(botgoalstates[handle]);
	botgoalstates[handle] = NULL;
} 
int BotSetupGoalAI(void)
{
	char *filename;

	g_gametype = LibVarValue("g_gametype", "0");
	filename = LibVarString("itemconfig", "items.c");
	itemconfig = LoadItemConfig(filename);
	if (!itemconfig)
	{
		botimport.Print(PRT_FATAL, "couldn't load item config\n");
		return BLERR_CANNOTLOADITEMCONFIG;
	} 
	droppedweight = LibVar("droppedweight", "1000");
	return BLERR_NOERROR;
} 
void BotShutdownGoalAI(void)
{
	int i;

	if (itemconfig) FreeMemory(itemconfig);
	itemconfig = NULL;
	if (levelitemheap) FreeMemory(levelitemheap);
	levelitemheap = NULL;
	freelevelitems = NULL;
	levelitems = NULL;
	numlevelitems = 0;

	BotFreeInfoEntities();

	for (i = 1; i <= MAX_CLIENTS; i++)
	{
		if (botgoalstates[i])
		{
			BotFreeGoalState(i);
		} 
	} 
} 
