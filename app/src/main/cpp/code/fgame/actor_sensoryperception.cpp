//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/actor_sensoryperception.cpp                    $
// $Revision:: 21                                                             $
//   $Author:: Sketcher                                                       $
//     $Date:: 4/09/03 5:04p                                                  $
//
// Copyright (C) 2001 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "actor.h"
#include "actor_sensoryperception.h"
#include "actor_enemymanager.h"
#include "worldspawn.h"
#include "player.h"

SensoryPerception::SensoryPerception()
{
    gi.Error(ERR_FATAL, "SensoryPerception::SensoryPerception -- Default Constructor Called");
    act = NULL;
    _init();
}

SensoryPerception::SensoryPerception(Actor *actor)
{
    if (actor) {
        act = actor;
    } else {
        gi.Error(ERR_DROP, "SensoryPerception::SensoryPerception -- actor is NULL");
        act = NULL;
    }

    _init();
}

SensoryPerception::~SensoryPerception() {}

void SensoryPerception::_init()
{
    _stimuli           = STIMULI_ALL;
    _permanent_stimuli = STIMULI_ALL;

    _noise_position  = vec_zero;
    _noise_time      = 0;
    _fov             = EF2_DEFAULT_FOV;
    _fovdot          = (float)cos((double)_fov * 0.5 * M_PI / 180.0);
    _vision_distance = EF2_DEFAULT_VISION_DISTANCE;
    _last_soundType  = 0;  
    _nextSenseTime   = 0.0f;

    _lineOfSight.entNum        = 0;
    _lineOfSight.inLineOfSight = false;
    _lineOfSight.time          = 0.0f;
}

void SensoryPerception::Stimuli(int stimuli)
{
    if (ShouldRespondToStimuli(stimuli)) {
        act->Wakeup();
        act->ActivateAI();
    }
}

void SensoryPerception::Stimuli(int new_stimuli, Entity *ent)
{
    if (!ent) {
        return;
    }

    if (ShouldRespondToStimuli(new_stimuli) && EntityIsValidTarget(ent)) {
        act->enemyManager->TryToAddToHateList(ent);
        act->Wakeup();
        act->ActivateAI();
    }
}

void SensoryPerception::Stimuli(int new_stimuli, const Vector& pos)
{
    if (ShouldRespondToStimuli(new_stimuli)) {
        if (!act->GetActorFlag(ACTOR_FLAG_NOISE_HEARD)) {
            _noise_position = pos;
            _noise_time     = level.time;
            act->SetActorFlag(ACTOR_FLAG_NOISE_HEARD, qtrue);
        }

        if (act->last_time_active + 10.0f < level.time) {
            act->Wakeup();
            act->ActivateAI();
        }
    }
}

void SensoryPerception::Stimuli(int new_stimuli, const Vector& pos, int sound_Type)
{
    if (ShouldRespondToStimuli(new_stimuli)) {
        if (!act->GetActorFlag(ACTOR_FLAG_NOISE_HEARD)) {
            _noise_position = pos;
            _last_soundType = sound_Type;
            _noise_time     = level.time;
            act->SetActorFlag(ACTOR_FLAG_NOISE_HEARD, qtrue);
        }

        if (act->last_time_active + 10.0f < level.time) {
            act->Wakeup();
            act->ActivateAI();
        }
    }
}

void SensoryPerception::RespondTo(const str& stimuli_name, qboolean respond)
{
    if (!Q_stricmp(stimuli_name.c_str(), "sight")) {
        RespondTo(STIMULI_SIGHT, respond);
    } else if (!Q_stricmp(stimuli_name.c_str(), "sound")) {
        RespondTo(STIMULI_SOUND, respond);
    } else if (!Q_stricmp(stimuli_name.c_str(), "pain")) {
        RespondTo(STIMULI_PAIN, respond);
    } else if (!Q_stricmp(stimuli_name.c_str(), "script")) {
        RespondTo(STIMULI_SCRIPT, respond);
    } else if (!Q_stricmp(stimuli_name.c_str(), "all")) {
        RespondTo(STIMULI_ALL, respond);
    } else if (!Q_stricmp(stimuli_name.c_str(), "none")) {
        RespondTo(STIMULI_NONE, respond);
    }
}

void SensoryPerception::RespondTo(int stimuli, qboolean respond)
{
    if (stimuli == STIMULI_ALL) {
        _stimuli = respond ? STIMULI_ALL : STIMULI_NONE;
        return;
    }

    if (stimuli == STIMULI_NONE) {
        _stimuli = respond ? STIMULI_NONE : STIMULI_ALL;
        return;
    }

    if (respond) {
        _stimuli |= stimuli;
    } else {
        _stimuli &= ~stimuli;
    }
}

void SensoryPerception::PermanentlyRespondTo(const str& stimuli_name, qboolean respond)
{
    if (!Q_stricmp(stimuli_name.c_str(), "sight")) {
        if (respond) {
            _permanent_stimuli |= STIMULI_SIGHT;
        } else {
            _permanent_stimuli &= ~STIMULI_SIGHT;
        }
    } else if (!Q_stricmp(stimuli_name.c_str(), "sound")) {
        if (respond) {
            _permanent_stimuli |= STIMULI_SOUND;
        } else {
            _permanent_stimuli &= ~STIMULI_SOUND;
        }
    } else if (!Q_stricmp(stimuli_name.c_str(), "pain")) {
        if (respond) {
            _permanent_stimuli |= STIMULI_PAIN;
        } else {
            _permanent_stimuli &= ~STIMULI_PAIN;
        }
    } else if (!Q_stricmp(stimuli_name.c_str(), "script")) {
        if (respond) {
            _permanent_stimuli |= STIMULI_SCRIPT;
        } else {
            _permanent_stimuli &= ~STIMULI_SCRIPT;
        }
    } else if (!Q_stricmp(stimuli_name.c_str(), "all")) {
        _permanent_stimuli = respond ? STIMULI_ALL : STIMULI_NONE;
    } else if (!Q_stricmp(stimuli_name.c_str(), "none")) {
        _permanent_stimuli = respond ? STIMULI_NONE : STIMULI_ALL;
    }
}

qboolean SensoryPerception::ShouldRespondToStimuli(int new_stimuli)
{
    if ((act->targetType == ATTACK_SCRIPTED_ONLY) && (new_stimuli != STIMULI_SCRIPT)) {
        return false;
    }

    if (_stimuli == STIMULI_ALL) {
        return true;
    }

    if (_stimuli == STIMULI_NONE) {
        return false;
    }

    return (((new_stimuli & _stimuli) && (new_stimuli & _permanent_stimuli)) || (new_stimuli == STIMULI_SCRIPT));
}

void SensoryPerception::ShowInfo()
{
    if (ShouldRespondToStimuli(STIMULI_ALL)) {
        gi.Printf("Actor is Responding To: ALL");
        return;
    }

    if (ShouldRespondToStimuli(STIMULI_NONE)) {
        gi.Printf("Actor is Responding To: NONE");
        return;
    }

    if (ShouldRespondToStimuli(STIMULI_SIGHT)) {
        gi.Printf("Actor is Responding To: SIGHT");
    }

    if (ShouldRespondToStimuli(STIMULI_SOUND)) {
        gi.Printf("Actor is Responding To: SOUND");
    }

    if (ShouldRespondToStimuli(STIMULI_PAIN)) {
        gi.Printf("Actor is Responding To: PAIN");
    }

    if (ShouldRespondToStimuli(STIMULI_SCRIPT)) {
        gi.Printf("Actor is Responding To: SCRIPT");
    }
}

qboolean SensoryPerception::WithinVisionDistance(const Entity *ent)
{
    float distance;

    if (!ent) {
        return false;
    }

    if (!act) {
        gi.Error(ERR_DROP, "SensoryPerception::WithinVisionDistance -- actor is NULL");
        return false;
    }

    if ((world->farplane_distance != 0.0f) && (world->farplane_distance < _vision_distance)) {
        distance = world->farplane_distance;
    } else {
        distance = _vision_distance;
    }

    return act->WithinDistance((Entity *)ent, distance);
}

qboolean SensoryPerception::WithinVisionDistance(const Vector& pos)
{
    float distance;

    if (!act) {
        gi.Error(ERR_DROP, "SensoryPerception::WithinVisionDistance -- actor is NULL");
        return false;
    }

    if ((world->farplane_distance != 0.0f) && (world->farplane_distance < _vision_distance)) {
        distance = world->farplane_distance;
    } else {
        distance = _vision_distance;
    }

    return act->WithinDistance(pos, distance);
}

qboolean SensoryPerception::InFOV(const Vector& pos, float check_fov, float check_fovdot)
{
    Vector delta;
    float  dot;
    int    tagNum;

    if (!act) {
        gi.Error(ERR_DROP, "SensoryPerception::InFOV -- actor is NULL");
        return false;
    }

    if (check_fov == 360.0f) {
        return true;
    }

    delta = pos - act->EyePosition();

    if (!delta.x && !delta.y) {
        return true;
    }

    delta.z = 0;

    delta.normalize();

    tagNum = gi.Tag_NumForName(act->edict->s.modelindex, "tag_eyes");

    if (tagNum >= 0) {
        Vector tag_pos;
        Vector forward;

        act->GetTag(tagNum, &tag_pos, &forward);
        dot = DotProduct(&forward[0], delta);
    } else {
        dot = DotProduct(act->orientation[0], delta);
    }

    return (dot > check_fovdot);
}

qboolean SensoryPerception::InFOV(const Vector& pos)
{
    return InFOV(pos, _fov, _fovdot);
}

qboolean SensoryPerception::InFOV(const Entity *ent)
{
    return InFOV(ent->centroid);
}

qboolean SensoryPerception::isInLineOfSight(const Vector& position, const int entNum)
{
    trace_t trace;
    Vector  startPos;
    Vector  endPos;
    Entity *traceEnt;

    startPos  = act->origin;
    startPos.z += 15;

    endPos  = position;
    endPos.z += 15;

    Vector modMins;
    Vector modMaxs;

    modMins = act->mins;
    modMins *= 1.5;

    modMaxs = act->maxs;
    modMaxs *= 1.5;

    trace = G_Trace(startPos, modMins, modMaxs, endPos, act, act->edict->clipmask, false, "isInLineOfSight");

    _lineOfSight.entNum = entNum;
    _lineOfSight.time   = level.time;

    if (trace.entityNum == entNum || entNum == ENTITYNUM_NONE) {
        _lineOfSight.inLineOfSight = true;
    } else {
        traceEnt = G_GetEntity(trace.entityNum);
        _lineOfSight.inLineOfSight = false;

        if (traceEnt && traceEnt->isSubclassOf(Actor)) {
            Actor *traceActor = static_cast<Actor *>(traceEnt);

            _lineOfSight.inLineOfSight = traceActor->sensoryPerception->checkInLineOfSight(position, entNum);
        }
    }

    return _lineOfSight.inLineOfSight;
}

qboolean SensoryPerception::checkInLineOfSight(const Vector& position, const int entNum)
{
    float timeDelta;

    timeDelta = level.time - _lineOfSight.time;

    if (timeDelta > 0.5 || entNum != _lineOfSight.entNum) {
        return isInLineOfSight(position, entNum);
    } else {
        return _lineOfSight.inLineOfSight;
    }
}

qboolean SensoryPerception::CanSeeEntityComplex(Entity *start, Entity *target, qboolean useFOV, qboolean useVisionDistance)
{
    if (!start || !target) {
        return false;
    }

    if (!ShouldRespondToStimuli(STIMULI_SIGHT)) {
        return false;
    }

    if (!act) {
        gi.Error(ERR_DROP, "SensoryPerception::CanSeeEntityComplex -- actor is NULL");
    }

    if (!act->IsEntityAlive(target)) {
        return false;
    }

    return _CanSeeComplex(start->centroid, target, useFOV, useVisionDistance);
}

qboolean SensoryPerception::CanSeeEntityComplex(Vector& start, Entity *target, qboolean useFOV, qboolean useVisionDistance)
{
    if (!target) {
        return false;
    }

    if (!ShouldRespondToStimuli(STIMULI_SIGHT)) {
        return false;
    }

    if (!act) {
        gi.Error(ERR_DROP, "SensoryPerception::CanSeeEntityComplex -- actor is NULL");
    }

    if (!act->IsEntityAlive(target)) {
        return false;
    }

    return _CanSeeComplex(start, target, useFOV, useVisionDistance);
}

qboolean SensoryPerception::CanSeePosition(const Vector& start, const Vector& position, qboolean useFOV, qboolean useVisionDistance)
{
    if (!ShouldRespondToStimuli(STIMULI_SIGHT)) {
        return false;
    }

    if (useFOV) {
        if (!InFOV(position)) {
            return false;
        }
    }

    if (useVisionDistance) {
        if (!WithinVisionDistance(position)) {
            return false;
        }
    }

    trace_t trace;
    Vector  eyePos;

    eyePos = vec_zero;

    trace = G_Trace(start, vec_zero, vec_zero, position, act, MASK_OPAQUE, false, "SensoryPerception::CanSeeEntity");
    if (trace.fraction == 1.0f) {
        return true;
    }

    return false;
}

qboolean SensoryPerception::_CanSeeComplex(Vector& start, Entity *target, qboolean useFOV, qboolean useVisionDistance)
{
    Vector d;
    Vector p1;
    Vector p2;

    if (!target) {
        return false;
    }

    d = target->centroid - start;
    d.z = 0;
    d.normalize();

    p1.x = -d.y;
    p1.y = d.x;
    p1 *= (act->size.x > act->size.y ? act->size.x : act->size.y) * 1.44f * 0.5f;  
    p2 = p1;

    p1.z = act->mins.z;
    p2.z = act->maxs.z;
    if (CanSeeEntity(start + p1, target, useFOV, useVisionDistance)) {
        return true;
    }
    if (CanSeeEntity(start + p2, target, useFOV, useVisionDistance)) {
        return true;
    }

    p1.z = -p1.z;
    p2.z = -p2.z;
    if (CanSeeEntity(start - p1, target, useFOV, useVisionDistance)) {
        return true;
    }
    if (CanSeeEntity(start - p2, target, useFOV, useVisionDistance)) {
        return true;
    }

    return false;
}


Vector SensoryPerception::GetNoisePosition()
{
    return _noise_position;
}

void SensoryPerception::SetLastSoundType(int soundtype)
{
    _last_soundType = soundtype;
}

int SensoryPerception::GetLastSoundType()
{
    return _last_soundType;
}

void SensoryPerception::SetFOV(float fov)
{
    _fov = fov;
}

void SensoryPerception::SetVisionDistance(float vision_distance)
{
    _vision_distance = vision_distance;
}

float SensoryPerception::GetVisionDistance()
{
    return _vision_distance;
}

void SensoryPerception::DoArchive(Archiver& arc, Actor *actor)
{
    Archive(arc);
    if (actor) {
        act = actor;
    } else {
        gi.Error(ERR_FATAL, "SensoryPerception::DoArchive -- actor is NULL");
    }
}


qboolean EntityIsValidTarget(const Entity *ent)
{
    if (ent && (ent->flags & FL_NOTARGET)) {
        return false;
    }

    if (ent && (ent->entnum == ENTITYNUM_WORLD)) {
        return false;
    }

    return true;
}

void SensoryPerception::SenseEnemies()
{
    int i;

    if (_nextSenseTime > level.time) {
        return;
    }

    _nextSenseTime = level.time + 0.5f + G_Random(0.2f);

    if ((act->actortype == IS_ENEMY) || (act->actortype == IS_MONSTER)) {
        for (int ti = 1; ti <= TeamMateList.NumObjects(); ti++) {
            Sentient *teammate = TeamMateList.ObjectAt(ti);
            if (_SenseEntity(teammate)) {
                return;
            }
        }
    } else {
        Entity *ent;

        for (i = 1; i <= ActiveList.NumObjects(); i++) {
            ent = ActiveList.ObjectAt(i);
            _SenseEntity(ent);
        }

        _SenseEntity(G_GetEntity(0));
    }
}

qboolean SensoryPerception::_SenseEntity(Entity *ent)
{
    if (!ent) {
        return false;
    }

    if (!EntityIsValidTarget(ent)) {
        return false;
    }

    if (ent->entnum == act->entnum) {
        return false;
    }

    if (ent->isSubclassOf(Actor)) {
        if (!ent->isThinkOn()) {
            return false;
        }
    }

    if (gi.inPVS(ent->centroid, act->centroid)) {  
        Vector enemyToSelf;
        float  visionDistance;

        enemyToSelf = act->origin - ent->origin;

        visionDistance = GetVisionDistance();
        if (enemyToSelf.length() < visionDistance) {
            if (CanSeeEntity(act, ent, true, true)) {
                Stimuli(STIMULI_SIGHT, ent);
            }
        }
    }

    return false;
}

qboolean SensoryPerception::CanSeeEntity(Entity *start, const Entity *target, qboolean useFOV, qboolean useVisionDistance)
{
    if (!start || !target) {
        return false;
    }

    if (!ShouldRespondToStimuli(STIMULI_SIGHT)) {
        return false;
    }

    if (useFOV) {
        if (!InFOV(target)) {
            return false;
        }
    }

    if (useVisionDistance) {
        if (!WithinVisionDistance(target)) {
            return false;
        }
    }

    trace_t trace;
    Vector  p;
    Vector  eyePos;

    p      = target->centroid;
    eyePos = vec_zero;

    if (start->isSubclassOf(Actor)) {
        Actor *a = static_cast<Actor *>(start);

        if (!a) {
            return false;
        }

        eyePos = a->EyePosition();
    }

    trace = G_Trace(eyePos, vec_zero, vec_zero, p, target, MASK_OPAQUE, false, "SensoryPerception::CanSeeEntity");
    if ((trace.fraction == 1.0f) || (trace.ent == target->edict)) {
        return true;
    }

    p.z = target->absmax.z;
    trace = G_Trace(eyePos, vec_zero, vec_zero, p, target, MASK_OPAQUE, false, "SensoryPerception::CanSeeEntity");
    if (trace.fraction == 1.0f || trace.ent == target->edict) {
        return true;
    }

    return false;
}

qboolean SensoryPerception::CanSeeEntity(const Vector& start, const Entity *target, qboolean useFOV, qboolean useVisionDistance)
{
    Vector realStart;

    if (!target) {
        return false;
    }

    if (!ShouldRespondToStimuli(STIMULI_SIGHT)) {
        return false;
    }

    if (useFOV) {
        if (!InFOV(target)) {
            return false;
        }
    }

    if (useVisionDistance) {
        if (!WithinVisionDistance(target)) {
            return false;
        }
    }

    trace_t trace;
    Vector  p;
    Vector  eyePos;

    realStart = start;
    p         = target->centroid;

    eyePos = act->EyePosition() - act->origin;
    realStart += eyePos;

    trace = G_Trace(realStart, vec_zero, vec_zero, p, act, MASK_OPAQUE, false, "SensoryPerception::CanSeeEntity");
    if (trace.fraction == 1.0f || trace.ent == target->edict) {
        return true;
    }

    p.z = target->absmax.z;
    trace = G_Trace(realStart, vec_zero, vec_zero, p, act, MASK_OPAQUE, false, "SensoryPerception::CanSeeEntity");
    if (trace.fraction == 1.0f || trace.ent == target->edict) {
        return true;
    }

    return false;
}

void SensoryPerception::Archive(Archiver& arc)
{
    arc.ArchiveInteger(&_stimuli);
    arc.ArchiveInteger(&_permanent_stimuli);
    arc.ArchiveVector(&_noise_position);
    arc.ArchiveInteger(&_last_soundType);
    arc.ArchiveFloat(&_noise_time);
    arc.ArchiveFloat(&_fov);
    arc.ArchiveFloat(&_fovdot);
    arc.ArchiveFloat(&_vision_distance);
    arc.ArchiveFloat(&_nextSenseTime);

    arc.ArchiveInteger(&_lineOfSight.entNum);
    arc.ArchiveFloat(&_lineOfSight.time);
    arc.ArchiveBoolean(&_lineOfSight.inLineOfSight);
}
