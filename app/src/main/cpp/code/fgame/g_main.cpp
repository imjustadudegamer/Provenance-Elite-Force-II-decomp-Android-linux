

#include "g_local.h"

#define PROBE_GET(probe_n, probe_d, probe_f) gi.cvar(probe_n, probe_d, probe_f)
#define PROBE_PRINT                       gi.Printf
#include "../qcommon/qa_probe.h"

#include "scriptvariable.h"
#include "g_spawn.h"
#include "g_phys.h"
#include "debuglines.h"
#include "entity.h"
#include "gamecmds.h"
#include "player.h"
#include "CameraPath.h"
#include "scriptmaster.h"
#include "scriptexception.h"
#include "lightstyleclass.h"
#include "gravpath.h"    
#include "gamescript.h"  
#include "viewthing.h"
#include "actor.h" 
#include "mp_manager.hpp"    
#include "rageai.h"          
#include "../qcommon/gameplaymanager.h"  
#include "program.h"          
#include "teammateroster.hpp"   

#include "../corepp/tiki.h"

Container<int> extraEntitiesList;  
#ifdef WIN32
#    include <intrin.h>
#endif

// load here, and saves written here load in retail EF2 -- so the port must both accept
#define SAVEGAME_VERSION   8
#define SAVEGAME_GAME_API  4
#define PERSISTANT_VERSION 1

static char G_ErrorMessage[4096];

qboolean  LoadingSavegame = false;
qboolean  LoadingServer   = false;
Archiver *currentArc      = NULL;

game_export_t globals;
game_import_t gi;

gentity_t active_edicts;
gentity_t free_edicts;

int sv_numtraces = 0;

int           g_protocol    = 0;
gentity_t    *g_entities;
qboolean g_iInThinks = 0;

usercmd_t  *current_ucmd;
Player     *g_pPlayer;

gclient_t g_clients[MAX_CLIENTS];

void (*SV_Error)(int type, const char *fmt, ...) Q_PRINTF_FUNC(2, 3);
void *(*SV_Malloc)(int size);
void (*SV_Free)(void *ptr);

qboolean LevelArchiveValid(Archiver& arc);
void     ClosePlayerLogFile(void);

void QDECL G_Error(const char *fmt, ...)
{
    va_list argptr;
    char    text[1024];

    va_start(argptr, fmt);
    Q_vsnprintf(text, sizeof(text), fmt, argptr);
    va_end(argptr);

    gi.Error(ERR_DROP, "%s", text);

#if defined(__GNUC__) || defined(__clang__)
    __builtin_unreachable();
#endif
}

void QDECL G_Error(int level_shadow1, const char *fmt, ...)
{
    va_list argptr;
    char    error[4096];

    va_start(argptr, fmt);
    vsprintf(error, fmt, argptr);
    va_end(argptr);

    assert(0);

    throw error;
}

void G_ExitWithError(const char *error)
{

    Q_strncpyz(G_ErrorMessage, error, sizeof(G_ErrorMessage));

    globals.error_message = G_ErrorMessage;
}



int  BotAISetup(int restart);
int  BotAILoadMap(int restart);
int  BotAIStartFrame(int time);
int  BotAIShutdown(int restart);
void G_InitBots(qboolean restart);

void SV_AddBot_f(void);

extern "C" void G_CleanupGame( qboolean restart );

extern "C" void G_InitGame( int startTime, int randomSeed )
{
	gi.DPrintf ("==== InitGame ====\n");

	SV_Error = gi.Error;
	gi.Error = G_Error;

	try
	{
		srand( randomSeed );

		CVAR_Init();

		FillBehaviorPackageList();

		gameVars.ClearList();
		level.intermission_advancetime = (float)g_intermissiontime->integer;
		level.fixedframetime           = ( 1.0f / sv_fps->value );
		level.startTime                = startTime;

		G_InitConsoleCommands();
		L_InitEvents();

		sv_numtraces = 0;

		Viewmodel.Init();

		multiplayerManager.init();

		game.maxentities = maxentities->integer;
		if ( maxclients->integer * 8 > game.maxentities )
		{
			game.maxentities = maxclients->integer * 8;
		}
		game.maxclients = maxclients->integer;

		GameplayManager::create();

		G_AllocGameData();
	}

	catch( const char *error )
	{
		G_ExitWithError( error );
	}
}

void G_AllocGameData(void)
{
    int i;

    G_DeAllocGameData();

    G_AllocDebugLines();


    game.maxentities = maxentities->integer;

    g_entities = (gentity_t *)gi.Malloc(game.maxentities * sizeof(g_entities[0]));

    memset(g_entities, 0, game.maxentities * sizeof(g_entities[0]));
    globals.gentities    = g_entities;
    globals.max_entities = game.maxentities;

    LL_Reset(&free_edicts, next, prev);
    LL_Reset(&active_edicts, next, prev);

    for (i = 0; i < game.maxentities; i++) {
        LL_Add(&free_edicts, &g_entities[i], next, prev);
    }

    game.clients = (gclient_t *)gi.Malloc(game.maxclients * sizeof(game.clients[0]));
    memset(game.clients, 0, game.maxclients * sizeof(game.clients[0]));

    for (i = 0; i < game.maxclients; i++) {
        g_entities[i].client = game.clients + i;

        G_InitClientPersistant(&game.clients[i]);
    }

    globals.num_entities = game.maxclients;

    gi.LocateGameData(
        g_entities, globals.num_entities, sizeof(gentity_t), &game.clients[0].ps, sizeof(game.clients[0])
    );

    if (gi.Cvar_VariableIntegerValue("bot_enable")) {
        BotAISetup(0);   
        BotAILoadMap(0); 
        G_InitBots(0);   
    }
}

void G_DeAllocGameData(void)
{
    G_DeAllocDebugLines();

    if (g_entities) {
        gi.Free(g_entities);
        g_entities = NULL;
    }

    if (game.clients) {
        gi.Free(game.clients);
        game.clients = NULL;
    }
}



extern "C" void G_SpawnEntities(const char *mapname, const char *entities, int levelTime)
{
    try {
        level.NewMap(mapname, entities, levelTime);
    }

    catch (const char *error) {
        G_ExitWithError(error);
    }
}

extern "C" int G_GetTotalGameFrames(void)
{
    Player *player = (Player *)g_entities[0].entity;

    if (player && player->isSubclassOf(Player)) {
        return player->getTotalGameFrames();
    }

    return 0;
}

extern "C" int G_GetEntityCurrentAnimFrame(int entnum, int bodyPart)
{
    try {
        if (entnum < 0 || entnum >= ENTITYNUM_NONE) {
            return -1;
        }

        if (!g_entities[entnum].inuse || !g_entities[entnum].entity) {
            return -1;
        }

        return g_entities[entnum].entity->CurrentFrame(bodyPart);
    }

    catch (const char *error) {
        G_ExitWithError(error);
    }

    return -1;
}

extern "C" void G_ExtraEntitiesToSend(gentity_t *clientEntity, int *numExtraEntities, int *extraEntities)
{
    int i;

    try {
        *numExtraEntities = 0;

        if (!multiplayerManager.inMultiplayer() && clientEntity && clientEntity->entity
            && clientEntity->entity->isSubclassOf(Player)) {
            ((Player *)clientEntity->entity)->ExtraEntitiesToSendToClient(numExtraEntities, extraEntities);

            for (i = 1; i <= extraEntitiesList.NumObjects(); i++) {
                if (*numExtraEntities >= MAX_EXTRA_ENTITIES_FROM_GAME) {
                    break;
                }

                extraEntities[*numExtraEntities] = extraEntitiesList.ObjectAt(i);
                (*numExtraEntities)++;
            }
        }
    }

    catch (const char *error) {
        G_ExitWithError(error);
    }
}

void G_ShutdownGame()
{
    try {
        gi.DPrintf("==== ShutdownGame ====\n");


        ClosePlayerLogFile();

        if (gi.Cvar_VariableIntegerValue("bot_enable")) {
            BotAIShutdown(qfalse); 
        }

        ClearBehaviorPackageList();

        level.CleanUp();

        L_ShutdownEvents();

        G_DeAllocGameData();

        GameplayManager::shutdown();
    }

    catch (const char *error) {
        G_ExitWithError(error);
    }
}


void QDECL Com_Error(int errorLevel, const char *error, ...)
{
    va_list argptr;
    char    text[1024];

    va_start(argptr, error);
    Q_vsnprintf(text, sizeof(text), error, argptr);
    va_end(argptr);

    G_Error("%s", text);
}

void QDECL Com_Printf(const char *msg, ...)
{
    va_list argptr;
    char    text[1024];

    va_start(argptr, msg);
    Q_vsnprintf(text, sizeof(text), msg, argptr);
    va_end(argptr);

    gi.DPrintf("%s", text);
}


extern "C" void G_RunFrame(int levelTime, int frameTime)
{
    gentity_t *edict;
    Entity    *ent;
    int        num;
    qboolean   showentnums;
    qboolean   showactnums;
    int        start;
    int        end;

    try {
        if (level.restart) {
            level.Restart();
        }

        level.update(levelTime, frameTime);

        G_EntTrash_Tick();

        if (g_showmem->integer) {
            DisplayMemoryUsage();
        }

        if (level.exitintermission) {
            G_ExitLevel();
            return;
        }

        G_InitDebugLines();

        if (csys_draw->integer) {
            G_DrawCSystem();
        }

        thePathManager.ShowNodes();

        AI_DisplayInfo();

        showentnums = (sv_showentnums->integer && (!multiplayerManager.inMultiplayer() || sv_cheats->integer));
        showactnums = (sv_showactnums->integer && (!multiplayerManager.inMultiplayer() || sv_cheats->integer));

        AI_SenseEnemies();

        L_ProcessPendingEvents();

        for (edict = active_edicts.next, num = 0; edict != &active_edicts; edict = level.next_edict, num++) {
            assert(edict);
            assert(edict->inuse);
            assert(edict->entity);

            level.next_edict = edict->next;

            assert(num <= MAX_GENTITIES);
            if (num > MAX_GENTITIES) {
                gi.WDPrintf("Possible infinite loop in G_RunFrame.\n");
                break;
            }

            ent = edict->entity;
            if (g_timeents->integer) {
                start = gi.Milliseconds();
                G_RunEntity(ent);
                end = gi.Milliseconds();

                if (g_timeents->value <= (float)(end - start)) {
                    gi.DPrintf("%d: '%s'(%d) : %d\n", level.framenum, ent->targetname.c_str(), ent->entnum, end - start);
                }
            } else {
                G_RunEntity(ent);
            }

            if ((edict->svflags & SVF_SENDONCE) && (edict->svflags & SVF_SENT)) {
                ent->PostEvent(EV_Remove, 0.0f);
            }

            if (showentnums) {
                G_DrawDebugNumber(ent->origin + Vector(0.0f, 0.0f, ent->maxs.z + 2.0f),
                                  ent->entnum,
                                  2.0f,
                                  1.0f,
                                  1.0f,
                                  0.0f);
            }
            if (showactnums && ent->isSubclassOf(Actor)) {
                G_DrawDebugNumber(ent->origin + Vector(0.0f, 0.0f, ent->maxs.z + 2.0f),
                                  ent->entnum,
                                  2.0f,
                                  1.0f,
                                  0.0f,
                                  0.0f);
            }
        }

        if (PROBE_On()) {
            static const int r1_marks[11]  = { 500,   2000,   8000,   20000,  40000,
                                               60000, 90000,  120000, 180000, 300000,
                                               600000 };
            static int       r1_sweeps = 0;
            static int       r1_last   = -1;

            if (r1_sweeps < 11 && level.inttime >= r1_marks[r1_sweeps] && r1_last != r1_sweeps) {
                r1_last = r1_sweeps;
                r1_sweeps++;

                int nact = 0, nthink = 0, nai = 0, nsmap = 0, nmmap = 0, nenemy = 0;
                int nsleep = 0, nlist_none = 0;

                WIDGET_PROBE("R1:ai", "SWEEP %d inttime=%d map=%s nents=%d -- BEGIN\n",
                           r1_sweeps, level.inttime, level.mapname.c_str(),
                           globals.num_entities);

                for (int r1_i = 0; r1_i < globals.num_entities; r1_i++) {
                    gentity_t *r1_ed = &g_entities[r1_i];
                    if (!r1_ed->inuse || !r1_ed->entity) {
                        continue;
                    }
                    if (!r1_ed->entity->isSubclassOf(Actor)) {
                        continue;
                    }
                    Actor *r1_a = (Actor *)r1_ed->entity;
                    nact++;

                    bool think_on  = (r1_a->flags & FL_THINK) ? true : false;
                    bool immobile  = (r1_a->flags & FL_IMMOBILE) ? true : false;
                    bool notarget  = (r1_a->flags & FL_NOTARGET) ? true : false;
                    bool started   = r1_a->GetActorFlag(ACTOR_FLAG_STARTED) ? true : false;
                    bool in_sleep  = false;
                    bool in_active = false;

                    for (int r1_j = 1; r1_j <= SleepList.NumObjects(); r1_j++) {
                        if (SleepList.ObjectAt(r1_j) == r1_a) {
                            in_sleep = true;
                            break;
                        }
                    }
                    if (!in_sleep) {
                        for (int r1_j = 1; r1_j <= ActiveList.NumObjects(); r1_j++) {
                            if (ActiveList.ObjectAt(r1_j) == r1_a) {
                                in_active = true;
                                break;
                            }
                        }
                    }
                    if (in_sleep) {
                        nsleep++;
                    } else if (!in_active) {
                        nlist_none++;
                    }

                    Entity *r1_enemy = r1_a->enemyManager ? r1_a->enemyManager->GetCurrentEnemy()
                                                          : NULL;
                    if (think_on) {
                        nthink++;
                    }
                    if (r1_a->mode == ACTOR_MODE_AI) {
                        nai++;
                    }
                    if (r1_a->statemap) {
                        nsmap++;
                    }
                    if (r1_a->masterstatemap) {
                        nmmap++;
                    }
                    if (r1_enemy) {
                        nenemy++;
                    }

                    const char *r1_st = r1_a->currentState ? r1_a->currentState->getName()
                                                          : "";
                    const char *r1_mst = r1_a->currentMasterState
                                             ? r1_a->currentMasterState->getName()
                                             : "";

                    const char *r1_pkg   = "";
                    const char *r1_bhv   = "-";
                    const char *r1_anim  = "-";
                    const char *r1_slash = NULL;

                    if (r1_a->statemap_name.length()) {
                        r1_pkg = r1_a->statemap_name.c_str();
                        r1_slash = strrchr(r1_pkg, '/');
                        if (r1_slash) {
                            r1_pkg = r1_slash + 1;
                        }
                    }
                    if (r1_a->behavior) {
                        r1_bhv = r1_a->behavior->getClassname();
                    }
                    if (r1_a->animname.length()) {
                        r1_anim = r1_a->animname.c_str();
                    }

                    float r1_edist = -1.0f;
                    if (r1_enemy) {
                        r1_edist = (r1_enemy->centroid - r1_a->centroid).length();
                    }

                    Entity *r1_hwt = r1_a->headWatcher ? r1_a->headWatcher->GetWatchTarget() : NULL;

                    WIDGET_PROBE("R1:ai",
                               "e=%d t=%d type=%d mode=%d think=%d immob=%d notgt=%d "
                               "started=%d strat=%d sm=%d msm=%d st='%s' mst='%s' enm=%d "
                               "pkg='%s' bhv='%s' anim='%s' edist=%.1f "
                               "hwt=%d hwd=%.0f hwx=%d "
                               "list=%c "
                               "tend=%.2f/%.2f/%.2f hp=%.0f org=(%.0f,%.0f,%.0f) "
                               "wedge=ge=%d mv=%d mt=%d td=(%.2f,%.2f,%.2f) vel=(%.1f,%.1f,%.1f) "
                               "hull=(%.0f,%.0f,%.0f)-(%.0f,%.0f,%.0f) m='%s'\n",
                               r1_i,
                               level.inttime,
                               (int)r1_a->actortype,
                               (int)r1_a->mode,
                               think_on ? 1 : 0,
                               immobile ? 1 : 0,
                               notarget ? 1 : 0,
                               started ? 1 : 0,
                               r1_a->thinkStrategy ? 1 : 0,
                               r1_a->statemap ? 1 : 0,
                               r1_a->masterstatemap ? 1 : 0,
                               r1_st,
                               r1_mst,
                               r1_enemy ? r1_enemy->entnum : -1,
                               r1_pkg,
                               r1_bhv,
                               r1_anim,
                               r1_edist,
                               r1_hwt ? r1_hwt->entnum : -1,
                               r1_a->headWatcher ? r1_a->headWatcher->GetWatchMaxDistance() : -1.0f,
                               r1_a->headWatcher ? (int)r1_a->headWatcher->GetWatchExplicit() : -1,
                               in_sleep ? 'S' : (in_active ? 'A' : '-'),
                               r1_a->personality ? r1_a->personality->GetTendency("hibernate") : -1.0f,
                               r1_a->personality ? r1_a->personality->GetTendency("work") : -1.0f,
                               r1_a->personality ? r1_a->personality->GetTendency("patrol") : -1.0f,
                               r1_a->health,
                               r1_a->origin.x,
                               r1_a->origin.y,
                               r1_a->origin.z,
                               r1_a->groundentity ? r1_a->groundentity->entity
                                                        ? r1_a->groundentity->entity->entnum
                                                        : -2
                                                  : -1,
                               (int)r1_a->movetype,
                               r1_a->movementSubsystem ? (int)r1_a->movementSubsystem->getMovementType() : -1,
                               r1_a->total_delta.x,
                               r1_a->total_delta.y,
                               r1_a->total_delta.z,
                               r1_a->velocity.x,
                               r1_a->velocity.y,
                               r1_a->velocity.z,
                               r1_a->mins.x,
                               r1_a->mins.y,
                               r1_a->mins.z,
                               r1_a->maxs.x,
                               r1_a->maxs.y,
                               r1_a->maxs.z,
                               r1_a->model.c_str());
                }

                WIDGET_PROBE("R1:ai",
                           "SWEEP %d SUMMARY actors=%d think_on=%d mode_AI=%d statemap=%d "
                           "masterstatemap=%d with_enemy=%d on_SleepList=%d on_NO_list=%d "
                           "SleepList_total=%d\n",
                           r1_sweeps,
                           nact,
                           nthink,
                           nai,
                           nsmap,
                           nmmap,
                           nenemy,
                           nsleep,
                           nlist_none,
                           SleepList.NumObjects());
            }
        }

        multiplayerManager.update(level.frametime);

        L_ProcessPendingEvents();

        G_ClientEndServerFrames();

        G_ClientDrawBoundingBoxes();

        G_ClientDrawSplines();

        if (sv_traceinfo->integer) {
            if (sv_traceinfo->integer == 3) {
                gi.DebugPrintf("%0.2f : Total traces %d\n", level.time, sv_numtraces);
            } else {
                gi.DPrintf("%0.2f : Total traces %d\n", level.time, sv_numtraces);
            }
        }

        sv_numtraces = 0;

        level.framenum++;

        levelTime += frameTime;
        level.setTime(levelTime, frameTime);
    }

    catch (const char *error) {
        G_ExitWithError(error);
    }
}

void G_ClientDrawBoundingBoxes(void)
{
    gentity_t *edict;
    Entity    *ent;
    Vector     eye;

    if ((!g_showgravpath->integer && !sv_showbboxes->integer) || (multiplayerManager.inMultiplayer() && !sv_cheats->integer)) {
        return;
    }

    if (sv_showbboxes->integer) {
        edict = g_entities;
        ent   = edict->entity;
        if (ent) {
            eye = ent->origin;
            ent = findradius(NULL, eye, 1000.0f);
            while (ent) {
                switch ((int)sv_showbboxes->integer) {
                case 1:
                    if (ent->edict != edict && ent->edict->s.solid) {
                        G_DebugBBox(ent->origin, ent->mins, ent->maxs, 1.0f, 1.0f, 0.0f, 1.0f);
                    }
                    break;
                case 2:
                    if (ent->edict != edict && ent->edict->s.solid) {
                        G_DebugBBox(vec_zero, ent->edict->absmin, ent->edict->absmax, 1.0f, 0.0f, 1.0f, 1.0f);
                    }
                    break;
                case 3:
                    if (ent->edict->s.modelindex && !(ent->edict->s.renderfx & RF_DONTDRAW)) {
                        G_DebugBBox(ent->origin, ent->mins, ent->maxs, 1.0f, 1.0f, 0.0f, 1.0f);
                    }
                    break;
                case 4:
                    G_DebugBBox(ent->origin, ent->mins, ent->maxs, 1.0f, 1.0f, 0.0f, 1.0f);
                    break;
                case 5:
                default:
                    if (ent->animate && gi.IsModel(ent->edict->s.modelindex)) {
                        vec3_t mins, maxs;

                        gi.Frame_Bounds(ent->edict->s.modelindex, ent->CurrentAnim(), ent->CurrentFrame(), ent->edict->s.scale, mins, maxs);
                        G_DebugBBox(ent->origin, mins, maxs, 0.0f, 1.0f, 0.0f, 1.0f);
                    } else {
                        G_DebugBBox(ent->origin, ent->mins, ent->maxs, 1.0f, 1.0f, 0.0f, 1.0f);
                    }
                    break;
                }
                ent = findradius(ent, eye, 1000.0f);
            }
        }
    }

    if (g_showgravpath->integer) {
        gravPathManager.DrawGravPaths();
    }
}


qboolean G_LevelArchiveValid(const char *filename)
{
    try {
        qboolean ret;

        Archiver arc;

        if (!arc.Read(filename)) {
            return qfalse;
        }

        ret = LevelArchiveValid(arc);

        arc.Close();

        return ret;
    }

    catch (const char *error) {
        G_ExitWithError(error);
        return qfalse;
    }
}









void G_ArchivePersistantData(Archiver& arc, qboolean sublevelTransition)
{
    gentity_t *ed;
    int        i;

    for (i = 0; i < game.maxclients; i++) {
        Entity *ent;

        ed = &g_entities[i];
        if (!ed->inuse || !ed->entity) {
            continue;
        }

        ent = ed->entity;
        if (!ent->isSubclassOf(Player)) {
            continue;
        }
        ((Player *)ent)->ArchivePersistantData(arc, sublevelTransition);
    }
}

qboolean G_ArchivePersistant(const char *name, qboolean loading, qboolean sublevelTransition)
{
    int      version;
    Archiver arc;

    if (loading) {
        if (!arc.Read(name, qfalse)) {
            return qfalse;
        }

        arc.ArchiveInteger(&version);
        if (version < PERSISTANT_VERSION) {
            gi.Printf("Persistent data from an older version (%d) of EF2.\n", version);
            arc.Close();
            return qfalse;
        } else if (version > PERSISTANT_VERSION) {
            gi.DPrintf("Persistent data from newer version %d of EF2.\n", version);
            arc.Close();
            return qfalse;
        }

    } else {
        arc.Create(name);

        version = PERSISTANT_VERSION;
        arc.ArchiveInteger(&version);
    }

    arc.ArchiveObject(&gameVars);
    G_ArchivePersistantData(arc, sublevelTransition);

    arc.Close();
    return qtrue;
}

extern "C" qboolean G_ReadPersistant(const char *name, qboolean sublevelTransition)
{
    try {
        return G_ArchivePersistant(name, true, sublevelTransition);
    }

    catch (const char *error) {
        G_ExitWithError(error);
    }

    return qfalse;
}


extern "C" void G_WritePersistant(const char *name, qboolean sublevelTransition)
{
    try {
        G_ArchivePersistant(name, false, sublevelTransition);
    }

    catch (const char *error) {
        G_ExitWithError(error);
    }
}

str getAliasParmString(AliasListNode_t *aliasNode)
{
    str parms;

    if (aliasNode->global_flag) {
        parms += "global ";
    }

    if (aliasNode->stop_flag) {
        parms += "stop ";
    }

    if (aliasNode->timeout > 0.0f) {
        parms += "timeout ";
        parms += aliasNode->timeout;
        parms += " ";
    }

    if (aliasNode->maximum_use > 0) {
        parms += "maxuse ";
        parms += aliasNode->maximum_use;
        parms += " ";
    }

    if (aliasNode->weight > 0) {
        parms += "weight ";
        parms += aliasNode->weight;
        parms += " ";
    }

    if (strlen(aliasNode->anim_name) > 0) {
        parms += "anim ";
        parms += aliasNode->anim_name;
        parms += " ";
    }

    if (aliasNode->loop_anim) {
        parms += "loopanim 1 ";
    }

    return parms;
}



void ArchiveAliases(Archiver& arc)
{
    int                      i;
    byte                     another;
    AliasList_t             *alias_list;
    AliasListNode_t         *alias_node;
    AliasActorNode_t        *actor_node;
    str                      alias_name;
    str                      realName;
    str                      model_name;
    const char              *name;
    int                      model_index;
    int                      actor_number;
    int                      number_of_times_played;
    byte                     been_played_this_loop;
    int                      last_time_played;

    if (arc.Saving()) {
        for (i = 0; i < MAX_MODELS; i++) {
            alias_list = (AliasList_t *)gi.Alias_GetList(i);

            if (alias_list) {
                alias_node = alias_list->data_list;
            } else {
                alias_node = NULL;
            }

            if (alias_node) {
                name = gi.NameForNum(i);

                if (name) {
                    another = true;
                    arc.ArchiveByte(&another);

                    model_name = name;
                    arc.ArchiveString(&model_name);


                    while (alias_node) {
                        str parms;

                        another = true;
                        arc.ArchiveByte(&another);

                        alias_name = alias_node->alias_name;
                        realName   = alias_node->real_name;

                        arc.ArchiveString(&alias_name);
                        arc.ArchiveString(&realName);

                        parms = getAliasParmString(alias_node);
                        arc.ArchiveString(&parms);

                        arc.ArchiveInteger(&alias_node->number_of_times_played);
                        arc.ArchiveByte(&alias_node->been_played_this_loop);
                        arc.ArchiveInteger(&alias_node->last_time_played);

                        actor_node = alias_node->actor_list;


                        while (actor_node) {
                            another = true;
                            arc.ArchiveByte(&another);

                            arc.ArchiveInteger(&actor_node->actor_number);
                            arc.ArchiveInteger(&actor_node->number_of_times_played);
                            arc.ArchiveByte(&actor_node->been_played_this_loop);
                            arc.ArchiveInteger(&actor_node->last_time_played);

                            actor_node = actor_node->next;
                        }

                        another = false;
                        arc.ArchiveByte(&another);

                        alias_node = alias_node->next;
                    }

                    another = false;
                    arc.ArchiveByte(&another);
                }
            }
        }

        another = false;
        arc.ArchiveByte(&another);
    } else {
        arc.ArchiveByte(&another);

        while (another) {
            arc.ArchiveString(&model_name);

            model_index = gi.modelindex(model_name.c_str());

            arc.ArchiveByte(&another);

            while (another) {
                str parms;


                arc.ArchiveString(&alias_name);
                arc.ArchiveString(&realName);
                arc.ArchiveString(&parms);

                arc.ArchiveInteger(&number_of_times_played);
                arc.ArchiveByte(&been_played_this_loop);
                arc.ArchiveInteger(&last_time_played);


                if (!gi.Alias_Find(model_index, alias_name.c_str())) {
                    gi.Alias_Add(model_index, alias_name.c_str(), realName.c_str(), parms.c_str());
                }


                gi.Alias_UpdateDialog(model_index, alias_name.c_str(), number_of_times_played, been_played_this_loop, last_time_played);

                arc.ArchiveByte(&another);

                while (another) {

                    arc.ArchiveInteger(&actor_number);
                    arc.ArchiveInteger(&number_of_times_played);
                    arc.ArchiveByte(&been_played_this_loop);
                    arc.ArchiveInteger(&last_time_played);

                    gi.Alias_AddActorDialog(model_index, alias_name.c_str(), actor_number, number_of_times_played, been_played_this_loop, last_time_played);

                    arc.ArchiveByte(&another);
                }

                arc.ArchiveByte(&another);
            }

            arc.ArchiveByte(&another);
        }
    }
}

qboolean LevelArchiveValid(Archiver& arc)
{
    int version;
    int savegame_version;

    arc.ArchiveInteger(&version);
    arc.ArchiveInteger(&savegame_version);

    if (version != SAVEGAME_GAME_API) {
        gi.Printf("Savegame from an incompatible game-API version (%d).\n", version);
        return qfalse;
    }

    if (savegame_version != SAVEGAME_VERSION) {
        if (savegame_version == 8) {
            gi.Printf(
                "This is a RETAIL Elite Force II savegame (version 8).  The container and "
                "the slot header are already read correctly, but the level archive stream "
                "itself is not yet retail-compatible, so it cannot be loaded.\n"
            );
        } else {
            gi.Printf("Savegame from an incompatible version (%d).\n", savegame_version);
        }
        return qfalse;
    }
    return qtrue;
}

qboolean G_ArchiveLevel(const char *filename, qboolean autosave, qboolean loading)
{
    try {
        int         i;
        int         num;
        Archiver    arc;
        gentity_t  *edict;

        if (loading) {
            LoadingSavegame = true;
            LoadingServer = true;

            level.CleanUp(false);

            arc.Read(filename);
            if (!LevelArchiveValid(arc)) {
                arc.Close();
                return qfalse;
            }

            L_UnarchiveEvents(arc);
        } else {
            int temp;

            arc.Create(filename);

            temp = SAVEGAME_GAME_API;
            arc.ArchiveInteger(&temp);
            temp = SAVEGAME_VERSION;
            arc.ArchiveInteger(&temp);

            L_ArchiveEvents(arc);
        }


        arc.ArchiveObject(&game);

        arc.ArchiveObject(&gameVars);

        arc.ArchiveObject(&level);

        if (arc.Loading()) {
            WIDGET_PROBE("archive", "stage: level done, inttime=%d\n", level.inttime);
        }
        if (arc.Loading()) {
            thePathManager.Init(level.mapname.c_str());
        }

        arc.ArchiveObject(&ScriptLib);
        WIDGET_PROBE("archive", "stage: ScriptLib done\n");

        arc.ArchiveObject(&gravPathManager);

        arc.ArchiveObject(&CameraMan);

        arc.ArchiveObject(&thePathManager);

        arc.ArchiveObject(&Director);

        arc.ArchiveObject(&lightStyles);

        if (arc.Loading()) {
            levelVars.ClearList();
        }
        arc.ArchiveObject(&levelVars);

        if (arc.Saving()) {
            num = 0;
            for (i = 0; i < globals.num_entities; i++) {
                edict = &g_entities[i];
                if (edict->inuse && edict->entity && !(edict->entity->flags & FL_DONTSAVE)) {
                    num++;
                }
            }
        }

        arc.ArchiveInteger(&globals.num_entities);
        arc.ArchiveInteger(&num);

        if (arc.Saving()) {
            arc.ArchiveObject(world);

            for (i = 0; i < globals.num_entities; i++) {
                edict = &g_entities[i];
                if (edict->inuse && edict->entity && !(edict->entity->flags & FL_DONTSAVE)) {
                    arc.ArchiveObject(edict->entity);
                }
            }
        } else {
            gi.LocateGameData(
                g_entities, globals.num_entities, sizeof(gentity_t), &game.clients[0].ps, sizeof(game.clients[0])
            );

            arc.ReadObject();


            for (i = 0; i < num; i++) {
                arc.ReadObject();
            }

            WIDGET_PROBE("archive", "stage: entity run done (%d entities)\n", num);
        }

        // BELOW the retail tail.  That is not cosmetic: it extends the stretch of the
        if (arc.Loading()) {
            WIDGET_PROBE("archive", "tail: program.FreeData()\n");
            program.FreeData();
            WIDGET_PROBE("archive", "tail: program.FreeData() done\n");
        }

        WIDGET_PROBE("archive", "tail: ArchiveObject(Program)\n");
        arc.ArchiveObject(&program);
        WIDGET_PROBE("archive", "tail: ArchiveObject(Program) done\n");

        WIDGET_PROBE("archive", "tail: ArchiveAliases\n");
        ArchiveAliases(arc);
        WIDGET_PROBE("archive", "tail: ArchiveAliases done\n");

        WIDGET_PROBE("archive", "tail: TeammateRoster\n");
        TeammateRoster::getInstance()->Archive(arc);
        WIDGET_PROBE("archive", "tail: TeammateRoster done\n");

        WIDGET_PROBE("archive", "tail: GameplayManager at arc pos=%lu\n",
                   (unsigned long)arc.Pos());
        GameplayManager *gpmArchive = GameplayManager::getTheGameplayManager();
        if (gpmArchive) {
            gpmArchive->Archive(arc);
            WIDGET_PROBE("archive", "tail: GameplayManager done at arc pos=%lu\n",
                       (unsigned long)arc.Pos());
        } else {
            WIDGET_PROBE("archive",
                       "tail: GameplayManager SINGLETON IS NULL -- create() has not run\n");
        }



        arc.Close();

        WIDGET_PROBE("archive", "stage: arc.Close() done (fixups resolved)\n");

        if (arc.Loading()) {
            LoadingSavegame = false;

            level.Precache();
            WIDGET_PROBE("archive", "stage: level.Precache() done\n");
        } else {
            gi.centerprintf(&g_entities[0], CENTERPRINT_IMPORTANCE_NORMAL,
                            "@textures/menu/gamesaved");
        }

        if (arc.Loading()) {

            for (i = 0; i < game.maxclients; i++) {
                edict = &g_entities[i];

                if (edict->inuse && edict->entity) {
                    Player *player = (Player *)edict->entity;
                    player->Loaded();
                }
            }
            WIDGET_PROBE("archive", "stage: player->Loaded() done, G_ArchiveLevel complete\n");
        }

        return qtrue;
    }

    catch (const char *error) {
        G_ExitWithError(error);
    }
    return qfalse;
}

//   * retail SV_ArchiveLevelFile (@0x001D6AC0, 292 B) never dereferences the cgame
void G_WriteLevel(const char *filename, qboolean autosave)
{
    game.autosaved = autosave;
    G_ArchiveLevel(filename, autosave, qfalse);
    game.autosaved = false;
}

qboolean G_ReadLevel(const char *filename)
{
    qboolean status;

    status = G_ArchiveLevel(filename, qfalse, qtrue);
    if (!status) {
        LoadingSavegame = false;
        LoadingServer   = false;
    }
    return status;
}

void G_PrepFrame(void)
{
}

extern "C" qboolean G_SendEntity(gentity_t *clientEntity, gentity_t *entityToSend);
extern "C" void     G_UpdateEntityStateForClient(gentity_t *clientEntity, entityState_t *state);
extern "C" void     G_UpdatePlayerStateForClient(gentity_t *clientEntity, playerState_t *state);
extern "C" void     G_PostLoad(void);
extern "C" void     G_PostSublevelLoad(const char *spawnPosName);
extern "C" qboolean G_InMultiplayerGame(void);
extern "C" void     G_WritePersistant(const char *name, qboolean sublevelTransition);
extern "C" qboolean G_ReadPersistant(const char *name, qboolean sublevelTransition);

extern "C" game_export_t *GetGameAPI(game_import_t *import)
{
    gi = *import;

    globals.apiversion = GAME_API_VERSION;

    globals.Init     = G_InitGame;
    globals.Shutdown = G_ShutdownGame;
    globals.Cleanup  = G_CleanupGame;

    globals.SpawnEntities       = G_SpawnEntities;
    globals.PostLoad            = G_PostLoad;
    globals.PostSublevelLoad    = G_PostSublevelLoad;

    globals.ClientConnect       = G_ClientConnect;

    globals.ClientBegin            = G_ClientBegin;
    globals.ClientUserinfoChanged  = G_ClientUserinfoChanged;
    globals.ClientDisconnect       = G_ClientDisconnect;
    globals.ClientCommand          = G_ClientCommand;
    globals.ClientThink            = G_ClientThink;

    globals.PrepFrame = G_PrepFrame;
    globals.RunFrame  = G_RunFrame;
    globals.SendEntity                 = G_SendEntity;
    globals.UpdateEntityStateForClient = G_UpdateEntityStateForClient;
    globals.UpdatePlayerStateForClient = G_UpdatePlayerStateForClient;
    globals.ExtraEntitiesToSend        = G_ExtraEntitiesToSend;
    globals.GetEntityCurrentAnimFrame  = G_GetEntityCurrentAnimFrame;

    globals.ConsoleCommand = G_ConsoleCommand;

    globals.WritePersistant = G_WritePersistant;
    globals.ReadPersistant  = G_ReadPersistant;

    globals.WriteLevel        = G_WriteLevel;
    globals.ReadLevel         = G_ReadLevel;
    globals.LevelArchiveValid = G_LevelArchiveValid;

    globals.inMultiplayerGame = G_InMultiplayerGame;
    globals.isDefined = NULL;
    globals.getDefine = NULL;

    globals.BotAIStartFrame = BotAIStartFrame;
    globals.AddBot_f = SV_AddBot_f;

    globals.GetTotalGameFrames = G_GetTotalGameFrames;

    globals.gentities   = g_entities;
    globals.gentitySize = sizeof(g_entities[0]);
    globals.num_entities = 0;
    globals.max_entities = game.maxentities;

    globals.error_message = NULL;

    return &globals;
}

void G_ClientEndServerFrames(void)
{
    int        i;
    gentity_t *ent;

    for (i = 0; i < game.maxclients; i++) {
        ent = g_entities + i;
        if (!ent->inuse || !ent->client || !ent->entity) {
            continue;
        }

        ent->entity->ProcessEvent(EV_ClientEndFrame);
    }
}


void FindIntermissionPoint(void)
{
    Entity *sent;
    Entity *starget;
    vec3_t        dir;

    sent = static_cast<Entity *>(G_FindClass(NULL, "info_player_intermission"));
    if (!sent) {
        level.m_intermission_origin = vec_zero;
        level.m_intermission_angle  = vec_zero;
        return;
    }

    level.m_intermission_origin = sent->origin;
    level.m_intermission_angle  = sent->angles;

    if (!sent->target.length()) {
        return;
    }

    starget = static_cast<Entity *>(G_FindTarget(NULL, sent->Target()));
    if (!starget) {
        return;
    }

    VectorSubtract(starget->origin, level.m_intermission_origin, dir);
    vectoangles(dir, level.m_intermission_angle);
}

void G_MoveClientToIntermission(Entity *ent)
{
    if (!ent) {
        return;
    }

    if (ent->isClient()) {
        Player *player = static_cast<Player *>(ent);

        if (multiplayerManager.inMultiplayer() && (level.m_intermission_angle != vec_zero)
            && (level.m_intermission_origin != vec_zero)) {
            player->setOrigin(level.m_intermission_origin);
            player->setAngles(level.m_intermission_angle);
            player->SetViewAngles(level.m_intermission_angle);
            player->CameraCut();
        }

        player->client->ps.pm_flags |= PMF_INTERMISSION;
        player->flags |= FL_IMMOBILE;
    }
}

void G_SentInitialMessages(void) {}



void G_BeginIntermission2(void)
{
    gentity_t *client;
    Entity    *ent;
    int        i;

    if (level.intermissiontime) {
        return;
    }

    level.playerfrozen     = qtrue;
    level.intermissiontime = level.time;

    ent = (Entity *)G_FindClass(NULL, "info_player_intermission");

    G_FadeSound(4.0f);

    if (ent) {
        SetCamera(ent, 0.5f);
    } else {
        G_FadeOut(2.0f);
    }

    for (i = 0, client = g_entities; i < game.maxclients; i++, client++) {
        if (!client->inuse || !client->entity || !client->client) {
            continue;
        }

        G_MoveClientToIntermission(client->entity);
    }

}

void G_BeginIntermission(const char *map_name)
{
    Entity    *camera;
    Entity    *node;
    Event     *ev;
    gentity_t *client;
    int        i;

    assert(map_name);
    if (!map_name) {
        gi.WDPrintf("G_BeginIntermission : Null map name\n");
        return;
    }

    if (level.intermissiontime || g_gametype->integer != GT_SINGLE_PLAYER) {
        return;
    }

    level.intermissiontime = level.time;

    G_FadeOut(2.0f);

    G_FadeSound(4.0f);

    level.nextmap = map_name;

    if (gi.areSublevels(level.mapname.c_str(), map_name) || !level._showIntermission) {
        level.exitintermission = true;
        return;
    }

    camera = (Entity *)G_FindClass(NULL, "info_player_intermission");
    if (camera) {
        SetCamera(camera, 0.5f);

        ev = new Event(EV_Camera_Orbit);

        node = (Entity *)G_FindTarget(NULL, "endnode1");
        if (node && node->isSubclassOf(Entity)) {
            ev->AddEntity(node);
            camera->ProcessEvent(ev);
            camera->ProcessEvent(EV_Camera_Cut);
        }
    }

    for (i = 0, client = g_entities; i < game.maxclients; client++, i++) {
        if (!client->inuse || !client->entity) {
            continue;
        }

    }

}

void G_ExitLevel(void)
{
    char               command[256];
    int                j;
    gentity_t         *ent;

    if (level.mission_failed) {
        return;
    }

    ClosePlayerLogFile();

    Com_sprintf(command, sizeof(command), "stopsound\n");
    gi.SendConsoleCommand(command);

    if (multiplayerManager.inMultiplayer()) {
        if (sv_nextmap->string && sv_nextmap->string[0]) {
            level.nextmap = sv_nextmap->string;
            gi.cvar_set("nextmap", "");
        }

        if (!level.nextmap.length()) {
            Com_sprintf(command, sizeof(command), "gamemap \"%s\"\n", level.mapname.c_str());
        } else {
            Com_sprintf(command, sizeof(command), "gamemap \"%s\"\n", level.nextmap.c_str());
        }
        gi.SendConsoleCommand(command);
    } else {
        Com_sprintf(command, sizeof(command), "gamemap \"%s\"\n", level.nextmap.c_str());
        gi.SendConsoleCommand(command);
    }

    for (j = 0; j < game.maxclients; j++) {
        ent = &g_entities[j];
        if (!ent->inuse || !ent->entity) {
            continue;
        }

        ent->entity->ProcessEvent(EV_Player_EndLevel);
    }

    level.nextmap = "";

    level.exitintermission = 0;
    level.intermissiontime = 0;

    G_ClientEndServerFrames();

    Director.PlayerNotReady();
}

void G_CheckIntermissionExit(void)
{
    if (!level.exitintermission) {
        return;
    }

    G_ExitLevel();
}









void G_AddEntityToExtraList( int entityNum )
{
	extraEntitiesList.AddUniqueObject( entityNum );
}

extern "C" void G_CleanupGame( qboolean restart )
{
	try
	{
		gi.DPrintf ("==== CleanupGame ====\n");
		
		level.CleanUp( restart );
	}
	
	catch( const char *error )
	{
		G_ExitWithError( error );
	}
}

void G_ClientDrawSplines( void )
{
	int i;
	Entity *entity;
	gentity_t *ed;
	
	
	if ( !sv_showsplines->integer )
		return;
	
	for ( i = 0; i < MAX_GENTITIES; i++ )
	{
		ed = &g_entities[i];
		
		if ( !ed->inuse || !ed->entity )
			continue;
		
		entity = g_entities[i].entity;
		
		if ( entity->isSubclassOf( SplinePath ) )
		{
			SplinePath *path = ( SplinePath *)entity;
			
			SplinePath	*node;
			BSpline	*splinePath;
			
			splinePath = new BSpline;
			splinePath->SetType( SPLINE_CLAMP );
			
			node = path;
			
			while( node != NULL )
			{
				splinePath->AppendControlPoint( node->origin, node->angles, node->speed );
				
				node = node->GetNext();
				
				if ( node == path )
					break;
			}
			
			G_Color3f( 1.0f, 1.0f, 0.0f );
			splinePath->DrawCurve( 10 );
			
			delete splinePath;
			continue;
		}
	}
}

extern "C" qboolean G_InMultiplayerGame( void )
{
	return multiplayerManager.inMultiplayer();
}


extern "C" void G_PostLoad( void )
{
	try
	{
		level.postLoad();
	}

	catch( const char *error )
	{
		G_ExitWithError( error );
	}
}


extern "C" void G_PostSublevelLoad( const char *spawnPosName )
{
	try
	{
		level.postSublevelLoad( spawnPosName );
	}

	catch( const char *error )
	{
		G_ExitWithError( error );
	}
}

void G_RemoveEntityFromExtraList( int entityNum )
{
	int index;

	index = extraEntitiesList.IndexOfObject( entityNum );

	if ( index > 0 )
	{
		extraEntitiesList.RemoveObjectAt( index );
	}
}

extern "C" qboolean G_SendEntity( gentity_t *clientEntity, gentity_t *entityToSend )
{
	try
	{
		if ( clientEntity->entity && clientEntity->entity->isSubclassOf( Player ) && entityToSend->entity )
		{
			Player *player;
			
			player = ( Player *)clientEntity->entity;
			
			return player->ShouldSendToClient( entityToSend->entity );
		}
	}
	
	catch( const char *error )
	{
		G_ExitWithError( error );
	}
	
	return false;
}

extern "C" void G_UpdateEntityStateForClient( gentity_t *clientEntity, entityState_t *state )
{
	try
	{
		if ( clientEntity->entity && clientEntity->entity->isSubclassOf( Player ) )
		{
			Player *player;
			
			player = ( Player *)clientEntity->entity;
			
			player->UpdateEntityStateForClient( state );
		}
	}
	
	catch( const char *error )
	{
		G_ExitWithError( error );
	}
	
}

extern "C" void G_UpdatePlayerStateForClient( gentity_t *clientEntity, playerState_t *state )
{
	try
	{
		if ( clientEntity->entity && clientEntity->entity->isSubclassOf( Player ) )
		{
			Player *player;
			
			player = ( Player *)clientEntity->entity;
			
			player->UpdatePlayerStateForClient( state );
		}
	}
	
	catch( const char *error )
	{
		G_ExitWithError( error );
	}
	
}

