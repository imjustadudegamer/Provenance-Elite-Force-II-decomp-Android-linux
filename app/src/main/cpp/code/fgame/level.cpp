//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/level.cpp                                 $
// $Revision:: 90                                                             $
//     $Date:: 9/26/03 2:36p                                                  $
//
// Copyright (C) 1999 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).


#include "g_local.h"

#define PROBE_GET(probe_n, probe_d, probe_f) gi.cvar(probe_n, probe_d, probe_f)
#define PROBE_PRINT                       gi.Printf
#include "../qcommon/qa_probe.h"

#include "g_spawn.h"
#include "level.h"
#include "earthquake.h" 
#include "armor.h"       
#include "navigate.h"
#include "smp_spawnstats.h"
#include "helper_node.h"
#include "gravpath.h"
#include "entity.h"
#include "player.h"
#include "health.h"

#include "fuzzyengine.h" 

#include "scriptmaster.h"
#include "scriptthread.h"
#include "scriptvariable.h"
#include "scriptexception.h"
#include "gamescript.h" 
#include "program.h"     
#include "CinematicArmature.h"   
#include "mp_manager.hpp"        
#include "teammateroster.hpp"    

#include <cfloat>


Level level;

gclient_t *spawn_client = NULL;




CLASS_DECLARATION(Listener, Level, NULL) {
    {NULL, NULL}
};




Level::Level()
{
    Init();
}

void Level::enemySpawned(Entity *enemy)
{
    _totalEnemiesSpawned++;
    gameVars.SetVariable("total_enemies_spawned", _totalEnemiesSpawned);
}


Level::~Level()
{
    _earthquakes.FreeObjectList();  
}

void Level::Init(void)
{

    spawn_entnum = -1;
    _totalEnemiesSpawned = 0;

    _earthquakes.ClearObjectList();  


    framenum       = 0;
    time           = 0;
    frametime      = 0;
    fixedframetime = 0.05f; 

    level_name = "";
    mapname    = "";
    spawnpoint = "";
    nextmap    = "";

    total_secrets = 0;
    found_secrets = 0;


    playerfrozen = false;

    // NOTE: retail's Init does NOT clear _playerDeathThread -- only CleanUp does

    next_edict = NULL;  

    intermissiontime         = 0.0f;
    exitintermission         = 0;
    timeInLevel              = 0.0f;  
    startTime                = 0;     
    _showIntermission        = true; 

    /* `intermission_advancetime' is deliberately not cleared here.  Retail's
       Level::Init @0x00238950 writes intermissiontime (+0xc4) and
       exitintermission (+0xc8) only; the advance time is written once per map
       by G_InitGame @0x001D20F0 (0x001D21AC) from `g_intermissiontime'.  Init
       runs from SpawnEntities, which the engine calls after InitGame, so
       clearing it here disarms the ten seconds ClientThink waits on. */

    cinematic = false;
    ai_on     = true;

    near_exit      = false;
    mission_failed = false;
    died_already   = false;
    started        = false;

    water_color = vec_zero;
    water_alpha = 0.0f;

    slime_color = vec_zero;  
    slime_alpha = 0.0f;      

    lava_color = vec_zero;
    lava_alpha = 0.0f;

    saved_soundtrack   = "";
    current_soundtrack = "";

    consoleThread = NULL;  

    automatic_cameras.ClearObjectList();

    levelVars.ClearList(); 

    m_fade_time_start = 0.0f;
    m_fade_time       = -1.0f;
    m_fade_color      = vec_zero;
    m_fade_alpha      = 0;
    m_fade_style      = additive;
    m_fade_type       = fadein;  

    m_letterbox_fraction   = 0;
    m_letterbox_time       = -1;
    m_letterbox_time_start = 0;
    m_letterbox_dir        = letterbox_out;

    hNodeController = NULL;  

    total_specialItems = 0;
    found_specialItems = 0;

    restart = false;

    _cleanup = false;

    _saveOrientation = true;

    currentInstanceNumber = 0;  


    memset(&impact_trace, 0, sizeof(trace_t));

}




void Level::CleanUp(qboolean restart_shadow1)
{
    _cleanup = true;

    if (multiplayerManager.inMultiplayer()) {
        multiplayerManager.cleanup(restart_shadow1);
    } else {
        Player *p = GetPlayer(0);
        if (p) p->LevelCleanup();
    }

    ClearCachedStatemaps();
    ClearCachedFuzzyEngines();

    _playerDeathThread = "";

#ifndef DEDICATED
    gi.MObjective_ClearObjectiveList();
#endif

    AdaptiveArmor::ClearAdaptionList();

    theCinematicArmature.clearCinematicsList();

    HelperNode::CleanupHelperNodeList();

    assert(active_edicts.next);
    assert(active_edicts.next->prev == &active_edicts);
    assert(active_edicts.prev);
    assert(active_edicts.prev->next == &active_edicts);
    assert(free_edicts.next);
    assert(free_edicts.next->prev == &free_edicts);
    assert(free_edicts.prev);
    assert(free_edicts.prev->next == &free_edicts);

    while (active_edicts.next != &active_edicts) {
        assert(active_edicts.next != &free_edicts);
        assert(active_edicts.prev != &free_edicts);

        assert(active_edicts.next);
        assert(active_edicts.next->prev == &active_edicts);
        assert(active_edicts.prev);
        assert(active_edicts.prev->next == &active_edicts);
        assert(free_edicts.next);
        assert(free_edicts.next->prev == &free_edicts);
        assert(free_edicts.prev);
        assert(free_edicts.prev->next == &free_edicts);

        if (active_edicts.next->entity) {
            delete active_edicts.next->entity;
        } else {
            FreeEdict(active_edicts.next);
        }
    }

    cinematic = false;
    ai_on     = true;

    mission_failed = false;
    died_already   = false;
    near_exit      = false;
    started        = false; // SDK level.cpp:245 -- cleared here, not only in Init()

    globals.num_entities = game.maxclients + 1;

    // NOTE: retail does NOT call gi.LocateGameData() from CleanUp.  CleanUp__5LevelFi

    thePathManager.ResetNodes();

    gravPathManager.Reset();

    if (consoleThread) {
        Director.KillThread(consoleThread->ThreadNum());
        consoleThread = NULL;
    }

    Director.CloseScript();

    Director.PlayerNotReady();

    automatic_cameras.ClearObjectList();

    levelVars.ClearList();
    gameVars.ClearList();

    L_ClearEventList();

    ResetEdicts();

    gi.cvar_set("bosshealth", "0");


    _earthquakes.ClearObjectList();  

    // NOTE: retail's CleanUp does NOT call world->FreeTargetList().  Retail calls it

    _cleanup = false;  
}

void Level::ResetEdicts(void)
{
    int i;

    memset(g_entities, 0, game.maxentities * sizeof(g_entities[0]));

    LL_Reset(&free_edicts, next, prev);
    LL_Reset(&active_edicts, next, prev);
    for (i = 0; i < game.maxentities; i++) {
        LL_Add(&free_edicts, &g_entities[i], next, prev);
    }

    for (i = 0; i < game.maxclients; i++) {
        g_entities[i].client = game.clients + i;

        G_InitClientPersistant(&game.clients[i]);
    }

    globals.num_entities = game.maxclients;
}

qboolean Level::inhibitEntity(int spawnFlags)
{
    if (!detail->integer && (spawnFlags & SPAWNFLAG_DETAIL)) {
        return qtrue;
    }

    if (g_gametype->integer != GT_SINGLE_PLAYER) {
        return (spawnFlags & SPAWNFLAG_NOT_DEATHMATCH) ? qtrue : qfalse;
    }

    if (!developer->integer && (spawnFlags & SPAWNFLAG_DEVELOPMENT)) {
        return qtrue;
    }

    if (!Q_stricmp(mapname, "t3l2")) {
        return (spawnFlags & SPAWNFLAG_NOT_EASY) ? qtrue : qfalse;
    }

    switch (skill->integer) {
    case 0:
        return (spawnFlags & SPAWNFLAG_NOT_EASY) != 0;

    case 1:
        return (spawnFlags & SPAWNFLAG_NOT_MEDIUM) != 0;

    case 2:
    case 3:
        return (spawnFlags & SPAWNFLAG_NOT_HARD);
    }


    return qfalse;
}

void Level::setSkill(int value)
{
    int skill_level;

    skill_level = floor((float)value);
    skill_level = bound(skill_level, 0, 3);

    gi.cvar_set("skill", va("%d", skill_level));

    gameVars.SetVariable("skill", skill_level);
}




void Level::loadLevelStrings(void)
{
    const char *environmentName = NULL;
    const char *levelName       = NULL;
    const char *sublevelName    = NULL;

    gi.GetLevelDefs(mapname.c_str(), &environmentName, &levelName, &sublevelName);
    gi.SR_LoadLevelStrings(environmentName ? environmentName : "");
}

void Level::setTime(int levelTime, int frameTime)
{
    inttime        = levelTime;
    fixedframetime = 1.0f / sv_fps->value;
    frametime      = ((float)frameTime / 1000.0f);
    time           = ((float)levelTime / 1000.0f);

    if (intermissiontime == 0.0f && mission_failed == false) {
        timeInLevel = time;
    }
}

void Level::SpawnEntities(const char *themapname, const char *entities, int levelSvsTime)
{
    int         inhibit, count = 0;
    const char *value;
    SpawnArgs   args;
    const char *spawnpos;

    SMP_SpawnEnter( themapname, 0 );
    SMP_SP_BEGIN( ents );

    {
        SMP_SP_BEGIN( init );
        Init();
        SMP_SP_END( init );
    }

    spawnpos = strchr(themapname, '$');
    if (spawnpos) {
        mapname    = str(themapname, 0, spawnpos - themapname);
        spawnpoint = spawnpos + 1;
    } else {
        mapname    = themapname;
        spawnpoint = "";
    }

    Com_Printf("-------------------- Spawning Entities -----------------------\n");

    setTime(levelSvsTime, 1000 / 20);


    setSkill(skill->integer);

    sv_numtraces = 0;

    if (!LoadingServer) {
        SMP_SP_BEGIN( pinit );
        thePathManager.Init(mapname.c_str());
        SMP_SP_END( pinit );
    }

    SMP_SP_BEGIN( wparse );
    entities     = args.Parse((char *)entities);
    spawn_entnum = ENTITYNUM_WORLD;
    args.Spawn();
    SMP_SP_END( wparse );

    if (!world) {
        Com_Error(ERR_FATAL, "No world\n");
    }

    if (g_gametype->integer == GT_MULTIPLAYER || g_gametype->integer == GT_BOT_SINGLE_PLAYER) {
        multiplayerManager.initMultiplayerGame();
    }

    // NOTE: retail's SpawnEntities does not touch the global HelperNodes list

    inhibit = 0;

    SMP_SP_BEGIN( loop );
    for (entities = args.Parse((char *)entities); entities != NULL; entities = args.Parse((char *)entities)) {
        spawnflags = 0;
        value      = args.getArg("spawnflags");

        if (value) {
            spawnflags = atoi(value);
            if (inhibitEntity(spawnflags)) {
                inhibit++;
                continue;
            }
        }

        {
            const char *smp_cn = NULL;
            int         smp_t0 = 0;

            if ( smp_spawnOn ) {
                smp_cn = args.getArg( "classname" );
                smp_t0 = SMP_SpawnNow();
            }

            args.Spawn();

            if ( smp_spawnOn ) {
                SMP_SpawnClass( smp_cn, SMP_SpawnNow() - smp_t0 );
            }
        }
        count++;

        gi.ProcessLoadingScreen("$$SpawningEntities$$");
    }
    SMP_SP_END( loop );

    gi.DPrintf("%i entities spawned\n", count);
    gi.DPrintf("%i entities inhibited\n", inhibit);

    SMP_SP_BEGIN( drain );
    L_ProcessPendingEvents();
    SMP_SP_END( drain );

    if (multiplayerManager.inMultiplayer()) {
        multiplayerManager.initItems();
    }


    if (!LoadingServer || game.autosaved) {
        SMP_SP_BEGIN( start );
        Start();
        SMP_SP_END( start );
    }

    {
        SMP_SP_BEGIN( postload );
        postLoad();
        SMP_SP_END( postload );
    }

    hNodeController = new HelperNodeController;
    if (hNodeController) {
        hNodeController->SetTargetName("HelperNodeController");
    }

    if (game.maxclients == 1) {
        SMP_SP_BEGIN( player );
        spawn_entnum = 0;
        new Player;
        SMP_SP_END( player );
    }

    SMP_SP_END( ents );
    SMP_SpawnLeave( count, inhibit );
}



void Level::Precache(void)
{
    str         filename;
    const char *environmentName;
    const char *levelName;
    const char *sublevelName;
    int         i;
    str         mapName;
    const char *spawnPoint;

    for (i = 0; i < 100; i++) {
        filename = va("global/global%i.scr", i);

        if (gi.FS_ReadFile(filename.c_str(), NULL, qtrue) != -1) {
            if (consoleThread) {
                WIDGET_PROBE("archive", "precache: parsing %s\n", filename.c_str());
                consoleThread->Parse(filename.c_str());
            }
        } else {
            break;
        }
    }
    WIDGET_PROBE("archive", "precache: global.scr chain done (%d files)\n", i);

    if (!mapname.length()) {
        return;
    }

    mapName   = mapname;
    spawnPoint = strstr(mapName.c_str(), "$");
    if (spawnPoint) {
        mapName.CapLength((int)(spawnPoint - mapName.c_str()));
    }

    gi.GetLevelDefs(mapName.c_str(), &environmentName, &levelName, &sublevelName);

    for (i = 0; i < 10; i++) {
        if (i == 0) {
            filename = va("precache/server/global.txt");
        } else {
            filename = va("precache/server/global%d.txt", i);
        }

        if (gi.FS_ReadFile(filename.c_str(), NULL, qtrue) != -1) {
            if (consoleThread) {
                consoleThread->Parse(filename.c_str());
            }
        } else {
            break;
        }
    }

    if (environmentName && environmentName[0]) {
        for (i = 0; i < 10; i++) {
            if (i == 0) {
                filename = va("precache/server/%s.txt", environmentName);
            } else {
                filename = va("precache/server/%s%d.txt", environmentName, i);
            }

            if (gi.FS_ReadFile(filename.c_str(), NULL, qtrue) != -1) {
                if (consoleThread) {
                    WIDGET_PROBE("archive", "precache: parsing %s\n", filename.c_str());
                    consoleThread->Parse(filename.c_str());
                }
            } else {
                break;
            }
        }
    }

    if (levelName && levelName[0]) {
        for (i = 0; i < 10; i++) {
            if (i == 0) {
                filename = va("precache/server/%s.txt", levelName);
            } else {
                filename = va("precache/server/%s%d.txt", levelName, i);
            }

            if (gi.FS_ReadFile(filename.c_str(), NULL, qtrue) != -1) {
                if (consoleThread) {
                    WIDGET_PROBE("archive", "precache: parsing %s\n", filename.c_str());
                    consoleThread->Parse(filename.c_str());
                }
            } else {
                break;
            }
        }
    }

    if (sublevelName && sublevelName[0] && Q_stricmp(levelName, sublevelName)) {
        for (i = 0; i < 10; i++) {
            if (i == 0) {
                filename = va("precache/server/%s.txt", sublevelName);
            } else {
                filename = va("precache/server/%s%d.txt", sublevelName, i);
            }

            if (gi.FS_ReadFile(filename.c_str(), NULL, qtrue) != -1) {
                if (consoleThread) {
                    WIDGET_PROBE("archive", "precache: parsing %s\n", filename.c_str());
                    consoleThread->Parse(filename.c_str());
                }
            } else {
                break;
            }
        }
    }
}


void Level::FindTeams()
{
    gentity_t *ent, *ent2;
    Entity    *e, *e2;
    int        i, j;
    int        c, c2;

    c  = 0;
    c2 = 0;
    for (i = 1, ent = g_entities + i; i < globals.num_entities; i++, ent++) {
        if (!ent->inuse) {
            continue;
        }

        e = ent->entity;

        if (!e->bind_info || !e->bind_info->moveteam.length()) {
            continue;
        }

        if (e->flags & FL_TEAMSLAVE) {
            continue;
        }

        e->bind_info->teammaster = e;
        c++;
        c2++;

        for (j = i + 1, ent2 = ent + 1; j < globals.num_entities; j++, ent2++) {
            if (!ent2->inuse) {
                continue;
            }

            e2 = ent2->entity;

            if (!e2->bind_info || !e2->bind_info->moveteam.length()) {
                continue;
            }

            if (e2->flags & FL_TEAMSLAVE) {
                continue;
            }

            if (!strcmp(e->bind_info->moveteam, e2->bind_info->moveteam)) {
                c2++;
                e2->bind_info->teamchain  = e->bind_info->teamchain;
                e->bind_info->teamchain   = e2;
                e2->bind_info->teammaster = e;
                e2->flags |= FL_TEAMSLAVE;

                if (e2->targetname) {
                    e->targetname  = e2->targetname;
                    e2->targetname = "";
                }
            }
        }
    }

    gi.DPrintf("%i teams with %i entities\n", c, c2);
}

gentity_t *Level::AllocEdict(Entity *entity)
{
    int        i;
    gentity_t *edict;

    if (spawn_entnum >= 0) {
        edict        = &g_entities[spawn_entnum];
        spawn_entnum = -1;

        assert(!edict->inuse && !edict->entity);

        if (edict->inuse && edict->entity) {
            delete edict->entity;
        }
    } else {
        edict = &g_entities[game.maxclients];

        for (i = game.maxclients; i < globals.num_entities; i++, edict++) {
            // further 0.5 s of level time that retail does not charge it.  That
            if (!edict->inuse && ((edict->freetime < (2.0f + startTime)) || (time - edict->freetime > 0.5f))) {
                break;
            }
        }

        if (i == globals.max_entities - 2.0f) {

            edict = &g_entities[game.maxclients];

            for (i = game.maxclients; i < globals.num_entities; i++, edict++) {
                if (!edict->inuse) {
                    break;
                }
            }

            if (i == globals.max_entities - 2.0f) {
                gi.Error(ERR_DROP, "Level::AllocEdict: no free edicts");
            }
        }
    }

    LL_Remove(edict, next, prev);

    InitEdict(edict);

    LL_Add(&active_edicts, edict, next, prev);

    if ((edict->s.number < ENTITYNUM_WORLD) && (globals.num_entities <= edict->s.number)) {
        globals.num_entities = edict->s.number + 1;
        gi.LocateGameData(
            g_entities, globals.num_entities, sizeof(gentity_t), &game.clients[0].ps, sizeof(game.clients[0])
        );
    }

    edict->entity = entity;

    edict->s.instanceNumber = currentInstanceNumber;
    currentInstanceNumber++;

    if (currentInstanceNumber < 0) {
        currentInstanceNumber = 0;
    }

    return edict;
}

void Level::FreeEdict(gentity_t *ed)
{
    gclient_t *client;

    gi.unlinkentity(ed);

    assert(ed->next);
    assert(ed->prev);

    if (next_edict == ed) {
        next_edict = ed->next;
    }

    LL_Remove(ed, next, prev);

    client = ed->client;

    memset(ed, 0, sizeof(*ed));

    ed->client   = client;
    ed->freetime = time;
    ed->inuse    = false;
    ed->s.number = ed - g_entities;

    LL_Add(&free_edicts, ed, next, prev);
}

void Level::InitEdict(gentity_t *e)
{
    int i;

    e->inuse = true;

    e->s.renderfx |= RF_FRAMELERP;
    e->s.number = e - g_entities;

    e->s.scale = 1.0f;
    e->s.constantLight = 0xffffff;

    e->s.frame = 0;
    e->spawntime  = time;

    e->svflags = 0;

    for (i = 0; i < NUM_BONE_CONTROLLERS; i++) {
        e->s.bone_tag[i] = -1;
        VectorClear(e->s.bone_angles[i]);
        EulerToQuat(e->s.bone_angles[i], e->s.bone_quat[i]);
    }

    for (i = 0; i < NUM_MORPH_CONTROLLERS; i++) {
        e->s.morph_controllers[i].index = -1;
        e->s.morph_controllers[i].percent = 0.0;
    }

    e->s.animationRate = 1.0f;
}

void Level::AddAutomaticCamera(Camera *cam)
{
    automatic_cameras.AddUniqueObject(cam);
}


















void Level::addEarthquake(Earthquake *e)
{
    if (e && !_earthquakes.ObjectInList(e)) {
        _earthquakes.AddObject(e);
    }
}

void Level::removeEarthquake(Earthquake *e)
{
    if (e && _earthquakes.ObjectInList(e)) {
        _earthquakes.RemoveObject(e);
    }
}

float Level::getEarthquakeMagnitudeAtPosition(const Vector& pos)
{
    float totalMagnitude = 0.0f;

    for (int i = 1; i <= _earthquakes.NumObjects(); i++) {
        Earthquake *e = _earthquakes.ObjectAt(i);

        if (e) {
            totalMagnitude += e->getMagnitudeAtPosition(pos);
        }
    }

    {
        static int uivk_dfx_n;
        if (PROBE_On() && PROBE_Throttle(&uivk_dfx_n, 48, 48)) {
            PROBE("DFX:quake-lookup", "nquakes=%d total=%.3f\n",
                       _earthquakes.NumObjects(), totalMagnitude);
        }
    }

    return totalMagnitude;
}





















void Level::Archive(Archiver& arc)
{
    int      num   = 0;
    int      i     = 0;
    [[maybe_unused]] float    fzero = 0.0f;
    [[maybe_unused]] qboolean qzero = qfalse;

    Class::Archive(arc);

    if (arc.Saving()) {
        SafePtr<Earthquake> ent;

        num = _earthquakes.NumObjects();
        arc.ArchiveInteger(&num);

        for (i = 1; i <= num; i++) {
            ent = _earthquakes.ObjectAt(i);
            arc.ArchiveSafePointer(&ent);
        }
    } else {
        SafePtr<Earthquake>          ent;
        SafePtr<Earthquake> *entityPointer;

        arc.ArchiveInteger(&num);

        _earthquakes.ClearObjectList();
        _earthquakes.Resize(num);

        for (i = 1; i <= num; i++) {
            _earthquakes.AddObject(ent);

            entityPointer = &_earthquakes.ObjectAt(i);
            arc.ArchiveSafePointer(entityPointer);
        }
    }

    arc.ArchiveInteger(&_totalEnemiesSpawned);
    arc.ArchiveInteger(&currentInstanceNumber);  

    arc.ArchiveInteger(&framenum);
    arc.ArchiveInteger(&inttime);
    arc.ArchiveFloat(&time);
    arc.ArchiveFloat(&timeInLevel);
    arc.ArchiveFloat(&frametime);
    arc.ArchiveFloat(&fixedframetime);
    arc.ArchiveInteger(&startTime);

    arc.ArchiveString(&level_name);
    arc.ArchiveString(&mapname);
    arc.ArchiveString(&spawnpoint);
    arc.ArchiveString(&nextmap);

    arc.ArchiveBoolean(&restart);
    arc.ArchiveBoolean(&started);

    arc.ArchiveBoolean(&playerfrozen);

    arc.ArchiveFloat(&intermissiontime);
    arc.ArchiveInteger(&exitintermission);
    arc.ArchiveFloat(&intermission_advancetime);
    arc.ArchiveBool(&_showIntermission);
    arc.ArchiveBool(&_saveOrientation);

    arc.ArchiveInteger(&total_secrets);
    arc.ArchiveInteger(&found_secrets);
    arc.ArchiveInteger(&total_specialItems);
    arc.ArchiveInteger(&found_specialItems);

    arc.ArchiveString(&game_script);  

    arc.ArchiveBoolean(&cinematic);
    arc.ArchiveBoolean(&ai_on);

    arc.ArchiveBoolean(&mission_failed);
    arc.ArchiveBoolean(&died_already);

    arc.ArchiveBoolean(&near_exit);

    arc.ArchiveVector(&water_color);
    arc.ArchiveFloat(&water_alpha);

    arc.ArchiveVector(&slime_color);
    arc.ArchiveFloat(&slime_alpha);

    arc.ArchiveVector(&lava_color);
    arc.ArchiveFloat(&lava_alpha);

    arc.ArchiveString(&current_soundtrack);
    arc.ArchiveString(&saved_soundtrack);

    arc.ArchiveObjectPointer((Class **)&consoleThread);

    arc.ArchiveVector(&m_fade_color);
    arc.ArchiveFloat(&m_fade_alpha);
    arc.ArchiveFloat(&m_fade_time);
    arc.ArchiveFloat(&m_fade_time_start);
    ArchiveEnum(m_fade_type, fadetype_t);     
    ArchiveEnum(m_fade_style, fadestyle_t);   

    arc.ArchiveFloat(&m_letterbox_fraction);
    arc.ArchiveFloat(&m_letterbox_time);
    arc.ArchiveFloat(&m_letterbox_time_start);
    ArchiveEnum(m_letterbox_dir, letterboxdir_t);  

    arc.ArchiveBool(&_cleanup); 

    arc.ArchiveString(&_playerDeathThread);  

    arc.ArchiveObjectPointer((Class **)&hNodeController);

    arc.ArchiveVector(&m_intermission_origin);
    arc.ArchiveVector(&m_intermission_angle);

    if (arc.Loading()) {
        str temp_soundtrack;

        temp_soundtrack = saved_soundtrack;
        ChangeSoundtrack(current_soundtrack.c_str());
        saved_soundtrack = temp_soundtrack;

        next_edict = NULL;
        memset(&impact_trace, 0, sizeof(impact_trace));

        loadLevelStrings();
    }
}









void Level::Restart()
{
	theCinematicArmature.deleteAllCinematics();

	SpawnEntities( current_map, current_entities, inttime );

	L_ProcessPendingEvents();

	G_ClientConnect( 0, true, false, true );
	G_ClientBegin( &g_entities[ 0 ], NULL );
}



void Level::EndIntermission()
{
	exitintermission = true;
}


void Level::NewMap( const char *mapname_shadow1, const char *entities, int levelTime )
{
	theCinematicArmature.deleteAllCinematics();
	current_map = mapname_shadow1;
	current_entities = entities;
	
	SpawnEntities( current_map, current_entities, levelTime );
}


void Level::PlayerRestart( void )
{
	gi.SendConsoleCommand( "restart\n" );
	level.mission_failed = false;
}


void Level::SetGameScript( const char *scriptname )
{
	game_script = scriptname;
}


void Level::SetIntermissionAdvanceTime(float time_shadow1)
{
	intermission_advancetime = time_shadow1;
}


void Level::Start( void )
{
	CThread *gamescript;


	levelVars.SetVariable( "total_secrets", total_secrets );
	levelVars.SetVariable( "found_secrets", found_secrets );
	levelVars.SetVariable( "total_specialItems" , total_specialItems );
	levelVars.SetVariable( "found_specialItems" , found_specialItems );
	levelVars.SetVariable( "total_enemies_spawned", _totalEnemiesSpawned );
	

	FindTeams();

	
	{
		SMP_SP_BEGIN( precache );
		Precache();
		SMP_SP_END( precache );
	}


	if ( game_script.length() )
	{
		gi.ProcessLoadingScreen( "$$LoadingScript$$" );

		program.Load( game_script );

		gi.ProcessLoadingScreen( "$$DoneLoadingScript$$" );


		gamescript = Director.CreateThread( "main" );

		if ( gamescript )
		{

			if ( gamescript->labelExists( "precache" ) )
			{
				CThread *precache_script;
				precache_script = Director.CreateThread( "precache" );

				if ( precache_script )
					precache_script->DelayedStart( 0.0f );
			}


			gamescript->DelayedStart( 0.0f );
		}
	}

	loadLevelStrings();
	started = true;
}


void Level::postLoad( void )
{
	thePathManager.FindAllTargets();
	TeammateRoster::getInstance()->clearTeammates();

	thePathManager.SavePaths();
}


void Level::postSublevelLoad( const char *spawnPosName )
{
	int i;
	gentity_t *ent;
	Player *player;



	spawnpoint = spawnPosName;


	for( i = 0; i < game.maxclients; i++ )
	{
		ent = &g_entities[ i ];

		if ( !ent->inuse || !ent->client || !ent->entity )
			continue;

		if ( ent->entity->isSubclassOf( Player ) )
		{
			player = (Player *)ent->entity;

			player->ChooseSpawnPoint();
		}
	}



	G_ClearFade();
}


void Level::setPlayerDeathThread( const str &threadName )
{
	_playerDeathThread = threadName;
}


void Level::update( int levelTime, int frameTime )
{
	setTime(levelTime, frameTime);

	if ( level.intermissiontime && dedicated->integer )
	{
		if ( g_endintermission->integer > 0 )
		{
			g_endintermission->integer = 0;
			level.exitintermission = true;
		}


		if ( ( ( level.time - level.intermissiontime ) > level.intermission_advancetime ) &&
			 ( level.intermission_advancetime != 0 ) )
		{
			if ( multiplayerManager.inMultiplayer() )
			{
				level.exitintermission = true;
			}
		}
	}
}




str	Level::getPlayerDeathThread( void )
{
	return _playerDeathThread;
}


int Level::getSkill( void )
{
	ScriptVariable *skill_var = gameVars.GetVariable("skill");
	return  skill_var->intValue();
}

