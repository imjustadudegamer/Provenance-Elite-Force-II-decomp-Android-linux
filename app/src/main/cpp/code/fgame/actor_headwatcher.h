//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/actor_headwatcher.h                            $
// $Revision:: 10                                                             $
//   $Author:: Steven                                                         $
//     $Date:: 10/13/03 8:53a                                                 $
//
// Copyright (C) 2001 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// Head Watcher Class
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#ifndef __ACTOR_HEADWATCHER_H__
#define __ACTOR_HEADWATCHER_H__


class Actor;
class Entity;
class Archiver;

class HeadWatcher
{
public:
    HeadWatcher();
    HeadWatcher(Actor *actor);
    ~HeadWatcher();

    void SetWatchTarget(Entity *ent);
    void SetWatchSpeed(float speed);
    void SetMaxHeadYaw(float max);
    void SetMaxHeadPitch(float max);
    void SetMaxDistance(float distance);
    void SetIgnoreWatchTarget(bool ignore);

    Entity *GetWatchTarget();

    float    GetWatchMaxDistance(void) const { return _maxDistance; }
    qboolean GetWatchExplicit(void) const { return _explicitSet; }

    void ClearWatchTarget();
    void HeadWatchTarget();
    void AdjustHeadAngles(const Vector& tagPos, const Vector& watchPosition);
    void LerpHeadBySpeed(const Vector& angleDelta, bool useTorsoAngles = true);

    void setHeadTwitch(bool twitchHead);
    void twitchHead(void);

    void Archive(Archiver& arc);
    void DoArchive(Archiver& arc, Actor *actor);

protected:
    void _init();

private:
    EntityPtr _watchTarget;
    Vector    _currentHeadAngles;
    float     _maxHeadTurnSpeed;
    float     _turnThreshold;
    qboolean  _explicitSet;
    float     _maxHeadYaw;
    float     _maxHeadPitch;

    bool   _twitchHead;
    float  _nextTwitchHeadTime;
    Vector _headTwitchAngles;
    float  _maxDistance;
    bool   _ignoreWatchTarget;

    Actor *act;






public:
    void SetWatchTarget( const str &targetName ); 
};

inline void HeadWatcher::SetMaxHeadPitch(float max)
{
    _maxHeadPitch = max;
}

inline void HeadWatcher::SetMaxHeadYaw(float max)
{
    _maxHeadYaw = max;
}

inline void HeadWatcher::SetMaxDistance(float distance)
{
    _maxDistance = distance;
}

inline void HeadWatcher::SetIgnoreWatchTarget(bool ignore)
{
    _ignoreWatchTarget = ignore;
}

#endif 
