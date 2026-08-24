/*
===========================================================================
Copyright (C) 2025 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "../corepp/str.h"

class Archiver;
class Entity;
class Entity;
class CThread;

void G_ArchiveEdict(Archiver& arc, gentity_t *edict);
class Event;

void G_AllocDebugLines(void);
void G_DeAllocDebugLines(void);

void G_TouchTriggers(Entity *ent);
void G_TouchTeleporters(Entity *ent);  
void G_TouchSolids(Entity *ent);

Entity *G_FindClass(Entity *ent, const char *classname);
Entity *G_NextEntity(Entity *ent);

void G_CalcBoundsOfMove(Vector& start, Vector& end, Vector& mins, Vector& maxs, Vector *minbounds, Vector *maxbounds);

void G_ShowTrace(const trace_t *trace, const gentity_t *passent, const char *reason, Vector& start, Vector& end);

trace_t G_Trace(
    const Vector& start,
    const Vector& mins,
    const Vector& maxs,
    const Vector& end,
    const Entity *passent,
    int           contentmask,
    qboolean      cylindrical,
    const char   *reason,
    qboolean      fulltrace = qfalse
);
trace_t G_Trace(
    vec3_t           start,
    vec3_t           mins,
    vec3_t           maxs,
    vec3_t           end,
    const gentity_t *passent,
    int              contentmask,
    qboolean         cylindrical,
    const char      *reason,
    qboolean         fulltrace = qfalse
);

trace_t G_FullTrace(
    const Vector& start,
    const Vector& mins,
    const Vector& maxs,
    const Vector& end,
    Entity       *passent,
    int           contentmask,
    qboolean      cylindrical,
    const char   *reason
);
trace_t G_FullTrace(
    vec3_t     start,
    vec3_t     mins,
    vec3_t     maxs,
    vec3_t     end,
    gentity_t *passent,
    int        contentmask,
    qboolean   cylindrical,
    const char *reason
);
void G_TraceEntities(
    Vector&              start,
    Vector&              mins,
    Vector&              maxs,
    Vector&              end,
    Container<Entity *> *victimlist,
    int                  contentmask,
    qboolean             bIncludeTriggers = qfalse
);

void SelectSpawnPoint(Vector& org, Vector& angles, str& thread);

Entity       *G_FindTarget(Entity *ent, const char *name);
Entity       *G_NextEntity(Entity *ent);

qboolean M_CheckBottom(Entity *ent);

Vector   G_GetMovedir(float angle);
qboolean KillBox(Entity *ent);
qboolean IsNumeric(const char *str);

Entity *findradius(Entity *startent, Vector org, float rad);
Entity *findclientsinradius(Entity *startent, Vector org, float rad);

Vector G_CalculateImpulse(Vector start, Vector end, float speed, float gravity);
Vector G_PredictPosition(Vector start, Vector target, Vector targetvelocity, float speed);

qboolean      G_LoadAndExecScript(const char *filename, const char *label = NULL, qboolean quiet = qfalse);
CThread *ExecuteThread( const str &thread_name, qboolean start = true, Entity *currentEnt = NULL );  

qboolean MOD_matches(int incoming_damage, int damage_type);
int      MOD_NameToNum(const str &meansOfDeath);
const char *MOD_NumToName(int meansOfDeath);
float G_GetDatabaseFloat( const str &prefix, const str &objectName, const str &varName );  
str G_GetDatabaseString( const str &prefix, const str &objectName, const str &varName );  
void CacheResource(const char *stuff, Entity *ent = NULL);
void G_CacheStateMachineAnims(Entity *ent, const char *stateMachineName);
int      MOD_NameToNum(const str &meansOfDeath);
const char *MOD_NumToName(int meansOfDeath);

void       G_MissionFailed(const str& reason = "DefaultFailure");
void       G_FinishMissionFailed(void);
void G_FadeOut(float delaytime);
void G_FadeSound(float delaytime);
void G_RestartLevelWithDelay(float delaytime);
void G_AutoFadeIn(void);
void G_ClearFade(void);
void G_StartCinematic(void);
void G_StopCinematic(void);

void CloneEntity(Entity *dest, const Entity *src);  

qboolean OnSameTeam(Entity *ent1, Entity *ent2);


int  modelIndex(const char *mdl);

void G_SetTrajectory(gentity_t *ent, vec3_t org);
void G_SetConstantLight(unsigned int *constantlight, const float *red, const float *green, const float *blue, const float *radius = NULL, int *lightStyle = NULL);  

void ChangeMusic(const char *current, const char *fallback, qboolean force);

void ChangeMusicVolume(float volume, float fade_time);

void RestoreMusicVolume(float fade_time);

void ChangeSoundtrack(const char *soundtrack);
int G_FindConfigstringIndex(const char *name, int start, int max, qboolean create);  
void G_AllowMusicDucking(bool allowMusicDucking);  
void G_AllowActionMusic(bool allowActionMusic);  

void RestoreSoundtrack(void);

void G_BroadcastSound(
    Entity *ent, const Vector& origin, float radius = SOUND_RADIUS, int soundType = SOUNDTYPE_GENERAL
);
void G_BroadcastAlert(Entity *ent, const Vector& origin, const Vector& enemy, float radius = SOUND_RADIUS);

int Soundtype_string_to_int(const str& soundtype_string);

Entity               *G_FindClass(Entity *ent, const char *classname);

qboolean G_SendCommandToPlayer(const gentity_t *ent, const char *command);
qboolean G_EnableWidgetOfPlayer(const gentity_t *ent, const char *widgetName, qboolean enableFlag);
qboolean G_SetWidgetTextOfPlayer(const gentity_t *ent, const char *widgetName, const char *widgetText);



inline Entity *G_GetEntity(int entnum)
{
    if ((entnum < 0) || (entnum >= globals.max_entities)) {
        gi.Error(ERR_DROP, "G_GetEntity: %d out of valid range.", entnum);
    }

    return (Entity *)g_entities[entnum].entity;
}




inline float G_Random(float n)
{
    return G_Random() * n;
}



float G_CRandom(void);


float G_CRandom(float n);


inline str G_FixSlashes(const char *filename)
{
    int    i;
    size_t len;
    str    text;

    if (filename) {
        text = filename;
        len  = text.length();
        for (i = 0; i < (int)len; i++) {
            if (text[i] == '\\') {
                text[i] = '/';
            }
        }
    }

    return text;
}

typedef enum {
    WEAPON_RIGHT,
    WEAPON_LEFT,
    WEAPON_DUAL,
    WEAPON_ANY,
    WEAPON_ERROR
} weaponhand_t;

#define NUM_ACTIVE_WEAPONS WEAPON_ANY

typedef enum {
    FIRE_MODE1,
    FIRE_MODE2,
    FIRE_MODE3, 
    MAX_FIREMODES,
    FIRE_ERROR
} firemode_t;

firemode_t   WeaponModeNameToNum(str mode);
const char  *WeaponHandNumToName(weaponhand_t hand);
weaponhand_t WeaponHandNameToNum(str side);
void         G_DebugTargets(Entity *e, str from);
void         G_DebugDamage(float damage, Entity *victim, Entity *attacker, Entity *inflictor);



