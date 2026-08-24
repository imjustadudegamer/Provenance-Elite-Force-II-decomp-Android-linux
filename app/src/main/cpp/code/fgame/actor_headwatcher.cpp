//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/actor_headwatcher.cpp                          $
// $Revision:: 21                                                             $
//   $Author:: Sketcher                                                       $
//     $Date:: 5/04/03 5:49p                                                  $
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
#include "actor_headwatcher.h"
#include "worldspawn.h"
#include "player.h"

HeadWatcher::HeadWatcher()
{
    gi.Error(ERR_FATAL, "HeadWatcher::HeadWatcher -- Default Constructor Called");
}

HeadWatcher::HeadWatcher(Actor *actor)
{
    if (actor) {
        act = actor;
    } else {
        gi.Error(ERR_DROP, "HeadWatcher::HeadWatcher -- actor is NULL");
    }

    _init();
}

HeadWatcher::~HeadWatcher() {}

void HeadWatcher::_init()
{
    act->SetControllerTag(ACTOR_HEAD_TAG, gi.Tag_NumForName(act->edict->s.modelindex, "Bip01 Head"));

    _currentHeadAngles = act->GetControllerAngles(ACTOR_HEAD_TAG);
    _watchTarget       = NULL;

    _maxHeadTurnSpeed = 30.0f;
    _turnThreshold    = 0.0f;
    _maxHeadYaw       = 60.0f;
    _maxHeadPitch     = 45.0f;

    _twitchHead         = false;
    _nextTwitchHeadTime = 0.0f;
    _maxDistance        = -1.0f;

    _explicitSet       = false;
    _ignoreWatchTarget = false;
}

void HeadWatcher::SetWatchTarget(Entity *ent)
{
    _watchTarget = ent;
    _explicitSet = true;
}

void HeadWatcher::SetWatchTarget(const str& targetName)
{
    str         name = targetName;
    TargetList *tlist;
    int         numObjects;

    tlist      = world->GetTargetList(name);
    numObjects = tlist->list.NumObjects();

    if (numObjects == 0) {
        gi.WDPrintf("No target with target name %s specified\n", targetName.c_str());
        return;
    } else if (numObjects > 1) {
        gi.WDPrintf("More than one target with target name %s specified, grabbing first one\n", targetName.c_str());
    }

    _watchTarget = (Entity *)(Entity *)tlist->list.ObjectAt(1);
    _explicitSet = true;
}

void HeadWatcher::SetWatchSpeed(float speed)
{
    _maxHeadTurnSpeed = speed;
}

void HeadWatcher::ClearWatchTarget()
{
    _watchTarget = NULL;
    _explicitSet = false;
}

Entity *HeadWatcher::GetWatchTarget()
{
    return _watchTarget;
}

void HeadWatcher::HeadWatchTarget()
{
    int          tagNum;
    Vector       tagPos;
    Vector       watchPosition;
    Actor       *actTarget = NULL;
    orientation_t o;

    tagNum = gi.Tag_NumForName(act->edict->s.modelindex, "Bip01 Head");

    if (tagNum < 0) {
        return;
    }

    if (act->CurrentAnim(0) < 0) {
        return;
    }

    act->GetTag("Bip01 Head", &o);
    tagPos = o.origin;

    if (!_watchTarget || _ignoreWatchTarget) {
        if (_twitchHead) {
            twitchHead();
            return;
        } else {
            LerpHeadBySpeed(vec_zero, false);
            return;
        }
    }

    if (act->GetActorFlag(ACTOR_FLAG_DIALOG_PLAYING) && act->GetActorFlag(ACTOR_FLAG_USING_HUD)) {
        LerpHeadBySpeed(vec_zero, false);
        return;
    }

    if (_maxDistance > 0) {
        Vector selfToTarget = _watchTarget->origin - act->origin;
        float  dist         = selfToTarget.length();

        if (dist > _maxDistance) {
            LerpHeadBySpeed(vec_zero, false);
            return;
        }
    }

    if (_watchTarget->isSubclassOf(Actor)) {
        actTarget = (Actor *)(Entity *)_watchTarget;

        if (actTarget->deadflag) {  
            _watchTarget = NULL;
            actTarget    = NULL;
            return;
        }
    }

    if (actTarget && (actTarget->watch_offset != vec_zero)) {
        MatrixTransformVector(actTarget->watch_offset, _watchTarget->orientation, watchPosition);
        watchPosition += _watchTarget->origin;
    } else {
        tagNum = gi.Tag_NumForName(_watchTarget->edict->s.modelindex, "Bip01 Head");

        if (tagNum < 0) {
            watchPosition = _watchTarget->centroid;
        } else {
            orientation_t wo;
            _watchTarget->GetTag("Bip01 Head", &wo);
            watchPosition = wo.origin;
        }
    }

    AdjustHeadAngles(tagPos, watchPosition);
}

void HeadWatcher::AdjustHeadAngles(const Vector& tagPos, const Vector& watchPosition)
{
    Vector dir;
    Vector angles;
    Vector anglesDiff;
    float  yawChange;
    float  pitchChange;

    dir    = watchPosition - tagPos;
    angles = dir.toAngles();

    anglesDiff = angles - act->angles;

    anglesDiff[YAW]   = AngleNormalize180(anglesDiff[YAW]);
    anglesDiff[PITCH] = AngleNormalize180(anglesDiff[PITCH]);

    yawChange   = anglesDiff[YAW];
    pitchChange = anglesDiff[PITCH];

    if (_turnThreshold && (yawChange < _turnThreshold) && (yawChange > -_turnThreshold)
        && (pitchChange < _turnThreshold) && (pitchChange > -_turnThreshold)) {
        return;
    }

    if (anglesDiff[YAW] < -_maxHeadYaw) {
        anglesDiff[YAW] = -_maxHeadYaw;
    } else if (anglesDiff[YAW] > _maxHeadYaw) {
        anglesDiff[YAW] = _maxHeadYaw;
    }

    if (anglesDiff[PITCH] < -_maxHeadPitch) {
        anglesDiff[PITCH] = -_maxHeadPitch;
    } else if (anglesDiff[PITCH] > _maxHeadPitch) {
        anglesDiff[PITCH] = _maxHeadPitch;
    }

    anglesDiff[ROLL] = 0.0f;

    LerpHeadBySpeed(anglesDiff);
}

void HeadWatcher::LerpHeadBySpeed(const Vector& angleDelta, bool useTorsoAngles)
{
    Vector anglesDiff;
    Vector change;
    Vector finalAngles;
    Vector currentTorsoAngles;

    anglesDiff = angleDelta;

    act->SetControllerTag(ACTOR_TORSO_TAG, gi.Tag_NumForName(act->edict->s.modelindex, "Bip01 Spine1"));
    currentTorsoAngles = act->GetControllerAngles(ACTOR_TORSO_TAG);

    act->SetControllerTag(ACTOR_HEAD_TAG, gi.Tag_NumForName(act->edict->s.modelindex, "Bip01 Head"));

    change = anglesDiff - _currentHeadAngles;

    if (change[YAW] > _maxHeadTurnSpeed) {
        anglesDiff[YAW] = _currentHeadAngles[YAW] + _maxHeadTurnSpeed;
    } else if (change[YAW] < -_maxHeadTurnSpeed) {
        anglesDiff[YAW] = _currentHeadAngles[YAW] - _maxHeadTurnSpeed;
    }

    if (change[PITCH] > _maxHeadTurnSpeed) {
        anglesDiff[PITCH] = _currentHeadAngles[PITCH] + _maxHeadTurnSpeed;
    } else if (change[PITCH] < -_maxHeadTurnSpeed) {
        anglesDiff[PITCH] = _currentHeadAngles[PITCH] - _maxHeadTurnSpeed;
    }

    if (change[ROLL] > _maxHeadTurnSpeed) {
        anglesDiff[ROLL] = _currentHeadAngles[ROLL] + _maxHeadTurnSpeed;
    } else if (change[ROLL] < -_maxHeadTurnSpeed) {
        anglesDiff[ROLL] = _currentHeadAngles[ROLL] - _maxHeadTurnSpeed;
    }

    finalAngles = anglesDiff;

    if (useTorsoAngles) {
        finalAngles[YAW] = anglesDiff[YAW] - currentTorsoAngles[YAW];
    } else {
        finalAngles[YAW] = anglesDiff[YAW];
    }

    act->SetControllerAngles(ACTOR_HEAD_TAG, finalAngles);
    act->real_head_pitch = anglesDiff[PITCH];

    _currentHeadAngles = anglesDiff;
}

void HeadWatcher::setHeadTwitch(bool twitchHeadOn)
{
    _twitchHead = twitchHeadOn;

    if (_twitchHead) {
        _nextTwitchHeadTime = 0.0f;
    }
}

void HeadWatcher::twitchHead(void)
{
    float oldMaxHeadTurnSpeed;

    if (level.time > _nextTwitchHeadTime) {
        _headTwitchAngles = Vector(G_CRandom(4.0f), G_CRandom(4.0f), G_CRandom(4.0f));

        _nextTwitchHeadTime = level.time + 1.0f + G_CRandom(0.5f);
    }

    oldMaxHeadTurnSpeed = _maxHeadTurnSpeed;

    _maxHeadTurnSpeed = 0.25f;

    LerpHeadBySpeed(_headTwitchAngles, false);

    _maxHeadTurnSpeed = oldMaxHeadTurnSpeed;
}

void HeadWatcher::DoArchive(Archiver& arc, Actor *actor)
{
    Archive(arc);
    if (actor) {
        act = actor;
    } else {
        gi.Error(ERR_FATAL, "HeadWatcher::DoArchive -- actor is NULL");
    }
}

void HeadWatcher::Archive(Archiver& arc)
{
    arc.ArchiveSafePointer(&_watchTarget);
    arc.ArchiveVector(&_currentHeadAngles);
    arc.ArchiveFloat(&_maxHeadTurnSpeed);
    arc.ArchiveFloat(&_turnThreshold);
    arc.ArchiveBoolean(&_explicitSet);
    arc.ArchiveFloat(&_maxHeadYaw);
    arc.ArchiveFloat(&_maxHeadPitch);
    arc.ArchiveBool(&_twitchHead);
    arc.ArchiveFloat(&_nextTwitchHeadTime);
    arc.ArchiveVector(&_headTwitchAngles);
    arc.ArchiveFloat(&_maxDistance);
    arc.ArchiveBool(&_ignoreWatchTarget);
}
