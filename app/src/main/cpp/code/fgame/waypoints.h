//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/waypoints.h                                   $
// $Revision:: 9                                                              $
//   $Author:: Steven                                                         $
//     $Date:: 10/13/03 8:54a                                                 $
//
// Copyright (C) 2001 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#pragma once

#include "entity.h"

class WayPointNode;
typedef SafePtr<WayPointNode> WayPointNodePtr;

extern Event EV_WayPoint_SetActorAnim;
extern Event EV_WayPoint_SetThread;

class WayPointNode : public Entity
{
private:
    str Thread;
    str ActorAnim;

public:
    CLASS_PROTOTYPE(WayPointNode);

    WayPointNode();
    ~WayPointNode();

    virtual void SetWayPointThread(Event *ev);
    virtual void SetActorAnim(Event *ev);
    virtual void SetWayPointThread(const str& thread);  
    virtual void SetActorAnim(const str& anim);         


    virtual const str& GetThread();
    virtual const str& GetActorAnim();

    void Archive(Archiver& arc) override;




};

inline void WayPointNode::Archive(Archiver& arc)
{
    Entity::Archive(arc);

    arc.ArchiveString(&Thread);
    arc.ArchiveString(&ActorAnim);
}

class PatrolWayPointNode : public WayPointNode
{
public:
    CLASS_PROTOTYPE(PatrolWayPointNode);

    PatrolWayPointNode();
    ~PatrolWayPointNode();
};

class CallVolumeWayPointNode : public WayPointNode
{
public:
    CLASS_PROTOTYPE(CallVolumeWayPointNode);

    CallVolumeWayPointNode();
    ~CallVolumeWayPointNode();
};

class PositionWayPointNode : public WayPointNode
{
private:
    qboolean _reserved;

public:
    CLASS_PROTOTYPE(PositionWayPointNode);

    PositionWayPointNode();
    ~PositionWayPointNode();

    void     Reserve(qboolean reserve);
    qboolean IsReserved();

    void Archive(Archiver& arc) override;
};

inline void PositionWayPointNode::Archive(Archiver& arc)
{
    WayPointNode::Archive(arc);

    arc.ArchiveBoolean(&_reserved);
}
