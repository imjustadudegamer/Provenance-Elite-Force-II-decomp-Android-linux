
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
