
// retail bug, reproduced deliberately; see Spawn::AttackMode).


#include "spawners.h"
#include "game.h"
#include "g_phys.h"
#include "g_spawn.h"
#include "entity.h"
#define R120_PROBE_GET(name, def, flags) gi.cvar(name, def, flags)
#define R120_PROBE_PRINT                 gi.Printf
#include "../qcommon/qa_r120.h"










Event EV_Spawn_ModelName
(
	"modelname",
	EV_SCRIPTONLY,
	"sSSSSSSSS",
	"model_name1 model_name2 model_name3 model_name4 model_name5 model_name6 model_name7 model_name8 model_name9",
	"Sets up to nine model names for this spawn entity."
);

Event EV_RandomSpawn_MinTime
(
	"min_time",
	EV_SCRIPTONLY,
	"f",
	"minTime",
	"Minimum time between random spawns."
);

Event EV_SpawnChain_Use3rdPersonCamera
(
	"use_3rd_person_camera",
	EV_SCRIPTONLY,
	"b",
	"bool",
	"Whether or not to check from the camera or character."
);




Event EV_SpawnOutOfSight_CheckFOV (	"checkFOV",
	EV_DEFAULT,
	"B",
	"boolean",
	"Use checkFOV to indicate that it's OK to spawn as long as the spawner is not in the player's FOV."
);




















Event EV_Spawn_SpawnTargetName (    "spawntargetname",
    EV_SCRIPTONLY,
    "s",
    "spawntargetname",
    "Sets spawn target name for this spawn entity.");
Event EV_Spawn_SpawnTarget (    "spawntarget",
    EV_SCRIPTONLY,
    "s",
    "spawntarget",
    "Sets spawn target for this spawn entity.");
Event EV_Spawn_SpawnNow (    "spawnnow",
    EV_SCRIPTONLY,
    "@e",
    "return_entity",
    "Spawns the entity and returns it.");
extern Event EV_Actor_AttackMode;

// different event, owned by Item (SDK item.cpp:149-156), and is the key SetArgs
Event EV_Spawn_PickupThread (    "pickupthread",
    EV_SCRIPTONLY,
    "s",
    "threadName",
    "Sets the pickup thread for the spawned entity.");
// retail deliberately prefixes these because the bare names are Actor's own
Event EV_Spawn_AddSpawnItem (    "spawn_spawnitem",
    EV_SCRIPTONLY,
    "s",
    "spawn_item_name",
    "Adds this named item to what will be spawned when this spawned entity is killed, if it is an actor.");
Event EV_Spawn_SetSpawnChance (    "spawn_spawnchance",
    EV_SCRIPTONLY,
    "f",
    "spawn_chance",
    "Sets the chance that this spawned entity will spawn something when killed, if it is an actor.");
Event EV_Spawn_SetStartHidden (    "starthidden",
    EV_SCRIPTONLY,
    "B",
    "hidden_bool",
    "Sets whether or not the entity starts hidden.");
Event EV_Spawn_SetSpawnEffect (    "spawneffectname",
    EV_SCRIPTONLY,
    "sS",
    "effectType effectName",
    "Displays this effect on spawn.");
Event EV_Spawn_SetAnimName (    "startanim",
    EV_SCRIPTONLY,
    "s",
    "animname",
    "Animation to spawn the actor with");
Event EV_Spawn_SetSpawnVelocity (    "setspawnvelocity",
    EV_SCRIPTONLY,
    "v",
    "velocity",
    "Sets the velocity to spawn with");
Event EV_Spawn_SetSpawnGroupID (    "setspawngroupID",
    EV_SCRIPTONLY,
    "i",
    "groupID",
    "Sets the groupID to spawn with");
Event EV_Spawn_SetSpawnMasterStateMap (    "setspawnmasterstatemap",
    EV_SCRIPTONLY,
    "s",
    "masterstatemap",
    "Sets the masterstatemap to spawn with");
Event EV_Spawn_SetSpawnGroupDeathThread (    "setspawngroupdeaththread",
    EV_SCRIPTONLY,
    "s",
    "group_death_thread",
    "Sets the group death thread to spawn with");
Event EV_Spawn_SetSpawnKeyValue (    "setSpawnKeyValue",
    EV_SCRIPTONLY,
    "ss",
    "key value",
    "Sets any key/value pair to set on the spawned entity.");
Event EV_Spawn_ClearSpawnKeyValues (    "clearSpawnKeyValues",
    EV_SCRIPTONLY,
    NULL,
    NULL,
    "Clears all of the key/value pairs for the spawner.");
Event EV_Spawn_CheckForSpace (    "checkForSpace",
    EV_SCRIPTONLY,
    NULL,
    NULL,
    "Checks to see if there is space for the spawn entity.");

Event EV_Spawn_AttackMode
(
	"attackmode",
	EV_SCRIPTONLY,
	"i",
	"attackmode",
	"Sets the _attackMode for this spawn entity."
);

CLASS_DECLARATION(ScriptSlave, Spawn, "func_spawn") {
    {&EV_Activate,                         &Spawn::DoSpawn                 },
    {&EV_Spawn_ModelName,                  &Spawn::ModelName               },
    {&EV_Spawn_SpawnTargetName,            &Spawn::SpawnTargetName         },
    {&EV_Spawn_AttackMode,                 &Spawn::AttackMode              },
    {&EV_Spawn_SpawnTarget,                &Spawn::SpawnTarget             },
    {&EV_Spawn_PickupThread,               &Spawn::SetPickupThread         },
    {&EV_SetAngle,                         &Spawn::SetAngleEvent           },
    {&EV_Spawn_AddSpawnItem,               &Spawn::SetSpawnItem            },
    {&EV_Spawn_SetSpawnChance,             &Spawn::SetSpawnChance          },
    {&EV_Spawn_SetStartHidden,             &Spawn::SetStartHidden          },
    {&EV_Spawn_SetSpawnEffect,             &Spawn::SetSpawnEffect          },
    {&EV_Spawn_SetAnimName,                &Spawn::SetAnimName             },
    {&EV_Spawn_SpawnNow,                   &Spawn::SpawnNow                },
    {&EV_Spawn_SetSpawnVelocity,           &Spawn::SetSpawnVelocity        },
    {&EV_Spawn_SetSpawnGroupID,            &Spawn::SetSpawnGroupID         },
    {&EV_Spawn_SetSpawnMasterStateMap,     &Spawn::SetSpawnMasterStateMap  },
    {&EV_Spawn_SetSpawnGroupDeathThread,   &Spawn::SetSpawnGroupDeathThread},
    {&EV_Spawn_SetSpawnKeyValue,           &Spawn::setSpawnKeyValue        },
    {&EV_Spawn_ClearSpawnKeyValues,        &Spawn::clearSpawnKeyValues     },
    {&EV_Spawn_CheckForSpace,              &Spawn::setCheckForSpace        },
    {NULL,                                 NULL                            }
};

void Spawn::SetAngleEvent(Event *ev)
{
    Entity::SetAngleEvent(ev);
}

void Spawn::SetPickupThread(Event *ev)
{
    _pickupThread = ev->GetString(1);
}

void Spawn::SetAnimName(Event *ev)
{
    _animName = ev->GetString(1);
}

void Spawn::SetSpawnVelocity(Event *ev)
{
    _velocity = ev->GetVector(1);
}

void Spawn::SetSpawnGroupID(Event *ev)
{
    _spawnGroupID = ev->GetInteger(1);
}

void Spawn::ModelName(Event *ev)
{
    assert(ev->NumArgs() > 0);

    _modelNames.ClearObjectList();

    for (int i = 1; i <= ev->NumArgs(); i++) {
        str modelname = ev->GetString(i);
        _modelNames.AddObject(modelname);
        CacheResource(modelname.c_str(), this);
    }
}

void Spawn::SpawnTargetName(Event *ev)
{
    _spawnTargetName = ev->GetString(1);
}

void Spawn::SpawnTarget(Event *ev)
{
    _spawnTarget = ev->GetString(1);
}

void Spawn::AttackMode(Event *ev)
{
    _spawnTarget = ev->GetInteger(1);
}

void Spawn::SetSpawnItem(Event *ev)
{
    _spawnItem = ev->GetString(1);
}

void Spawn::SetSpawnChance(Event *ev)
{
    _spawnChance = ev->GetFloat(1);
}

void Spawn::SetStartHidden(Event *ev)
{
    if (ev->NumArgs() > 0) {
        _startHidden = ev->GetBoolean(1);
    } else {
        _startHidden = true;
    }
}

void Spawn::SetSpawnEffect(Event *ev)
{
    _effectType = ev->GetString(1);

    if (ev->NumArgs() > 1) {
        _effectName = ev->GetString(2);
    } else {
        _effectName = "";
    }
}

void Spawn::SetSpawnMasterStateMap(Event *ev)
{
    _masterStateMap = ev->GetString(1);
}

void Spawn::SetSpawnGroupDeathThread(Event *ev)
{
    _spawnGroupDeathThread = ev->GetString(1);
}

Spawn::Spawn()
{
    setSolidType(SOLID_NOT);
    setMoveType(MOVETYPE_NONE);
    hideModel();

    _spawnChance = 0;
    _attackMode  = 0;
    _startHidden = false;
    _animName    = "idle";
    _velocity    = Vector(0, 0, -1);
    _spawnGroupID          = -9999;
    _masterStateMap        = "";
    _spawnGroupDeathThread = "";

    _checkForSpace = false;
}

Spawn::~Spawn()
{
    _modelNames.FreeObjectList();

    _keys.FreeObjectList();
    _values.FreeObjectList();
}

void Spawn::SetArgs(SpawnArgs& args)
{
    int i;

    args.setArg("origin", va("%f %f %f", origin[0], origin[1], origin[2]));
    args.setArg("angle", va("%f", angles[1]));
    args.setArg("angles", va("%f %f %f", angles[0], angles[1], angles[2]));

    if (_modelNames.NumObjects() > 0) {
        int randomModelIndex = (int)ceil(G_Random() * _modelNames.NumObjects());
        randomModelIndex     = iClamp(randomModelIndex, 1, _modelNames.NumObjects());
        args.setArg("model", _modelNames.ObjectAt(randomModelIndex).c_str());
    }

    args.setArg("attackMode", va("%i", _attackMode));
    args.setArg("scale", va("%f", edict->s.scale));
    if (_spawnTargetName.length()) {
        args.setArg("targetname", _spawnTargetName.c_str());
    }
    if (_spawnTarget.length()) {
        args.setArg("target", _spawnTarget.c_str());
    }
    if (_pickupThread.length()) {
        // "pickup_thread" (SDK item.cpp:151), so retail's pass-through never
        args.setArg("pickupThread", _pickupThread.c_str());
    }
    if (_spawnItem.length()) {
        args.setArg("spawnItem", _spawnItem.c_str());
        args.setArg("spawnChance", va("%f", _spawnChance));
    }
    if (_startHidden) {
        args.setArg("hide", "");
    }


    for (i = 1; i <= _keys.NumObjects(); i++) {
        args.setArg(_keys.ObjectAt(i).c_str(), _values.ObjectAt(i).c_str());
    }

    if (_effectType.length()) {
        str effectString;

        effectString = _effectType;

        if (_effectName.length()) {
            effectString += "-";
            effectString += _effectName;
        }

        args.setArg("displayeffect", effectString.c_str());
    }
}

void Spawn::postSpawn(Entity *spawn)
{
    if (!spawn) {
        return;
    }

    if (_spawnGroupID > 0) {
        spawn->AddToGroup(_spawnGroupID);
    }

    Event *e = new Event(EV_Anim);  
    e->AddString(_animName);
    spawn->PostEvent(e, EV_SPAWNARG);

    if (g_debugtargets->integer) {
        void (*debugTargets)(Entity *, str) = &G_DebugTargets;
        debugTargets(spawn, "Spawn::DoSpawn");
    }

    if (_masterStateMap.length()) {
        Event *smapEvent = new Event("masterstatemap");
        smapEvent->AddString(_masterStateMap);
        spawn->PostEvent(smapEvent, EV_POSTSPAWN);
    }

    if (_spawnGroupDeathThread.length()) {
        Event *groupDeathThreadEvent = new Event("groupdeaththread");
        groupDeathThreadEvent->AddString(_spawnGroupDeathThread);
        spawn->PostEvent(groupDeathThreadEvent, EV_POSTSPAWN);
    }
}

void Spawn::DoSpawn(Event *ev)
{
    Entity   *spawn;
    SpawnArgs args;

    {
        static int r133_dfx_n;
        if (R120_On() && R120_Throttle(&r133_dfx_n, 64, 64)) {
            R120_PROBE("DFX:spawn", "ent=%d targetname='%s' nmodels=%d first='%s' spawntargetname='%s' displayeffect='%s'\n",
                       entnum,
                       targetname.c_str(),
                       _modelNames.NumObjects(),
                       _modelNames.NumObjects() > 0 ? _modelNames.ObjectAt(1).c_str() : "",
                       _spawnTargetName.c_str(),
                       _effectType.c_str());
        }
    }

    if (_modelNames.NumObjects() == 0) {
        warning("Spawn", "No models set");
    }

    SetArgs(args);

    spawn = (Entity *)args.Spawn();

    if (spawn) {
        spawn->velocity = _velocity;

        postSpawn(spawn);

        if (_checkForSpace && checkStuck(spawn)) {
            spawn->PostEvent(EV_Remove, 0.0f);
        }
    }
}

void Spawn::SpawnNow(Event *ev)
{
    Entity   *spawn;
    SpawnArgs args;

    if (_modelNames.NumObjects() == 0) {
        warning("Spawn", "No models set");
    }

    SetArgs(args);

    spawn = (Entity *)args.Spawn();

    if (spawn) {
        spawn->CancelEventsOfType(EV_ProcessInitCommands);
        spawn->ProcessInitCommands(spawn->edict->s.modelindex);

        spawn->velocity = Vector(0, 0, -1);

        postSpawn(spawn);

        if (_checkForSpace && checkStuck(spawn)) {
            spawn->CancelEventsOfType(EV_DisplayEffect);
            spawn->PostEvent(EV_Remove, 0.0f);
            ev->ReturnEntity(NULL);
            return;
        }
    }

    ev->ReturnEntity(spawn);
}

bool Spawn::checkStuck(Entity *spawn)
{
    int        i;
    int        num;
    int        touch[MAX_GENTITIES];
    gentity_t *hit;
    Vector     min;
    Vector     max;

    min = origin + spawn->mins;
    max = origin + spawn->maxs;

    num = gi.AreaEntities(min, max, touch, MAX_GENTITIES, qfalse);

    for (i = 0; i < num; i++) {
        hit = &g_entities[touch[i]];

        if (hit->inuse && hit->entity && (hit->entity != spawn) && (hit->entity->edict->solid == SOLID_BBOX)) {
            return true;
        }
    }

    return false;
}

void Spawn::setSpawnKeyValue(Event *ev)
{
    str spawnKey;
    str value;

    spawnKey = ev->GetString(1);
    value    = ev->GetString(2);

    _keys.AddObject(spawnKey);
    _values.AddObject(value);
}

void Spawn::clearSpawnKeyValues(Event *ev)
{
    _keys.ClearObjectList();
    _values.ClearObjectList();
}

void Spawn::setCheckForSpace(Event *ev)
{
    _checkForSpace = true;
}



Event EV_RandomSpawn_MaxTime (    "max_time",
    EV_SCRIPTONLY,
    "f",
    "maxTime",
    "Maximum time between random spawns.");

Event EV_RandomSpawn_Think (    "_randomspawn_think",
    EV_CODEONLY,
    NULL,
    NULL,
    "The function that actually spawns things in.");

// (SDK spawners.cpp:612-618) -- a func_randomspawn never toggled on activation.
CLASS_DECLARATION(Spawn, RandomSpawn, "func_randomspawn") {
    {&EV_Activate,            &RandomSpawn::ToggleSpawn},
    {&EV_RandomSpawn_MinTime, &RandomSpawn::MinTime    },
    {&EV_RandomSpawn_MaxTime, &RandomSpawn::MaxTime    },
    {&EV_RandomSpawn_Think,   &RandomSpawn::Think      },
    {NULL,                    NULL                     }
};

RandomSpawn::RandomSpawn()
{
    min_time = 0.2f;
    max_time = 1.0f;
    if (!LoadingSavegame && !(spawnflags & 1)) {
        PostEvent(EV_RandomSpawn_Think, min_time + (G_Random(max_time - min_time)));
    }
}

void RandomSpawn::MinTime(Event *ev)
{
    min_time = ev->GetFloat(1);
}

void RandomSpawn::MaxTime(Event *ev)
{
    max_time = ev->GetFloat(1);
}

void RandomSpawn::ToggleSpawn(Event *ev)
{
    if (EventPending(EV_RandomSpawn_Think)) {
        CancelEventsOfType(EV_RandomSpawn_Think);
    } else {
        Think(NULL);
    }
}

void RandomSpawn::Think(Event *ev)
{
    CancelEventsOfType(EV_RandomSpawn_Think);

    DoSpawn(NULL);

    PostEvent(EV_RandomSpawn_Think, min_time + (G_Random(max_time - min_time)));
}


CLASS_DECLARATION(Spawn, ReSpawn, "func_respawn") {
    {NULL, NULL}
};

void ReSpawn::DoSpawn(Event *ev)
{
    Entity   *spawn;
    SpawnArgs args;

    SetArgs(args);

    args.setArg("targetname", TargetName());
    args.setArg("target", TargetName());

    spawn = (Entity *)args.Spawn();
    if (spawn) {
        spawn->velocity = Vector(0, 0, -1);
    }
}


CLASS_DECLARATION(Spawn, SpawnOutOfSight, "func_spawnoutofsight") {
    {&EV_SpawnOutOfSight_CheckFOV, &SpawnOutOfSight::CheckFOV},
    {NULL,                         NULL                      }
};

SpawnOutOfSight::SpawnOutOfSight()
{
    checkFOV = false;
}

void SpawnOutOfSight::CheckFOV(Event *ev)
{
    if (ev->NumArgs() >= 1) {
        checkFOV = ev->GetBoolean(1);
    } else {
        checkFOV = true;
    }
}

void SpawnOutOfSight::DoSpawn(Event *ev)
{
    Entity    *ent;
    gentity_t *ed;
    trace_t    trace;

    for (int i = 0; i < game.maxclients; i++) {
        ed = &g_entities[i];
        if (!ed->inuse || !ed->entity) {
            continue;
        }

        ent = ed->entity;
        if ((ent->health < 0.0f) || (ent->flags & FL_NOTARGET)) {
            continue;
        }

        if (checkFOV) {
            Vector clientFwd(ed->client->ps.viewangles);
            clientFwd.AngleVectors(&clientFwd);
            clientFwd.z = 0.0f;
            clientFwd.normalize();

            Vector clientToSpawner(origin);
            clientToSpawner -= Vector(ed->centroid);
            clientToSpawner.normalize();

            float clientFOV = DEG2RAD(ed->client->ps.fov + 10.0f);
            if ((clientFOV / 2.0f) > Vector::AngleBetween(clientToSpawner, clientFwd)) {
                return;
            }
        } else {
            trace = G_Trace(
                origin, vec_zero, vec_zero, ent->centroid, this, MASK_OPAQUE, false, "SpawnOutOfSight::DoSpawn"
            );
            if (trace.fraction == 1.0f) {
                return;
            }
        }
    }

    Spawn::DoSpawn(ev);
}



CLASS_DECLARATION(Spawn, SpawnChain, "func_spawnchain") {
    {&EV_SpawnChain_Use3rdPersonCamera, &SpawnChain::Use3rdPersonCamera},
    {NULL,                              NULL                           }
};

SpawnChain::SpawnChain(void)
{
    use3rdPersonCamera = false;
}

void SpawnChain::Use3rdPersonCamera(Event *ev)
{
    use3rdPersonCamera = ev->GetBoolean(1);
}

void SpawnChain::DoSpawn(Event *ev)
{
    qboolean seen = false;

    for (int i = 0; i < game.maxclients; i++) {
        gentity_t *ed = &g_entities[i];
        if (ed->inuse && ed->entity) {
            Vector playerPosition;
            Vector entityForward;
            float entityFOV(DEG2RAD(ed->client->ps.fov));
            if (use3rdPersonCamera) {
                const Vector cameraAngles(ed->client->ps.camera_angles);
                cameraAngles.AngleVectors(&entityForward);
                playerPosition = ed->client->ps.camera_origin;
                entityFOV      = DEG2RAD(ed->client->ps.fov + 10.0f);
            } else {
                const Vector viewAngles(ed->client->ps.viewangles);
                viewAngles.AngleVectors(&entityForward);
                playerPosition = ed->centroid;
                entityFOV      = DEG2RAD(ed->client->ps.fov + 10.0f);
            }

            Vector directionFromEntityToMe(origin - playerPosition);
            directionFromEntityToMe.normalize();

            entityForward.z = 0.0f;
            entityForward.normalize();

            assert(fCloseEnough(directionFromEntityToMe.length(), 1.0f, fEpsilon()));
            assert(fCloseEnough(entityForward.length(), 1.0f, fEpsilon()));

            const float angleToMe(acos(Vector::Dot(directionFromEntityToMe, entityForward)));
            if (angleToMe < entityFOV / 2.0f) {
                Entity *entity = ed->entity;
                if ((entity->health >= 0.0f) && !(entity->flags & FL_NOTARGET)) {
                    trace_t trace = G_Trace(
                        origin, vec_zero, vec_zero, entity->centroid, this, MASK_OPAQUE, false, "SpawnChain::DoSpawn"
                    );
                    if (trace.fraction == 1.0f) {
                        seen = true;
                        break;
                    }
                }
            }
        }
    }

    Vector  spawnMins(-64, -64, 16);
    Vector  spawnMaxs(64, 64, 112);
    trace_t trace = G_Trace(origin, spawnMins, spawnMaxs, origin, NULL, MASK_MONSTERSOLID, false, "spawnchain");

    if (trace.fraction != 1.0f || trace.startsolid || seen) {
        const char *name = Target();
        if (name && strcmp(name, "")) {
            Entity *entity = NULL;
            do {
                entity = (Entity *)G_FindTarget(entity, name);
                if (!entity) {
                    break;
                }
                Event *event = new Event(EV_Activate);  
                event->AddEntity(this);
                entity->PostEvent(event, 0.0f);
            } while (1);
        }
        return;
    }

    Spawn::DoSpawn(ev);
}
