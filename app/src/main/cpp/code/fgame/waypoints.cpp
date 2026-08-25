
#include "g_local.h"
#include "waypoints.h"

Event EV_WayPoint_SetActorAnim (	"setactoranim",
	EV_SCRIPTONLY,
	"s",
	"anim_name",
	"sets what primary anim to set an actor to, when it reaches this waypoint"
);


Event EV_WayPoint_SetThread (    "setwaypointthread",
    EV_SCRIPTONLY,
    "s",
    "thread_name",
    "sets the script thread to be called when the actor reaches this waypoint");

CLASS_DECLARATION(Entity, WayPointNode, "info_waypointnode_waypointnode") {
    {&EV_WayPoint_SetActorAnim, &WayPointNode::SetActorAnim     },
    {&EV_WayPoint_SetThread,    &WayPointNode::SetWayPointThread},
    {NULL,                      NULL                           }
};


WayPointNode::WayPointNode()
{
    Thread    = "";
    ActorAnim = "";
}

WayPointNode::~WayPointNode() {}

const str& WayPointNode::GetThread()
{
    return Thread;
}

const str& WayPointNode::GetActorAnim()
{
    return ActorAnim;
}

void WayPointNode::SetActorAnim(const str& anim)
{
    ActorAnim = anim;
}

void WayPointNode::SetWayPointThread(const str& thread)
{
    Thread = thread;
}

void WayPointNode::SetWayPointThread(Event *ev)
{
    Thread = ev->GetString(1);
}

void WayPointNode::SetActorAnim(Event *ev)
{
    ActorAnim = ev->GetString(1);
}

CLASS_DECLARATION(Entity, PatrolWayPointNode, "info_waypointnode_patrolwaypointnode") {
    {&EV_WayPoint_SetActorAnim, &PatrolWayPointNode::SetActorAnim     },
    {&EV_WayPoint_SetThread,    &PatrolWayPointNode::SetWayPointThread},
    {NULL,                      NULL                                  }
};

PatrolWayPointNode::PatrolWayPointNode()
{
    SetWayPointThread(str(""));
    SetActorAnim(str(""));
}

PatrolWayPointNode::~PatrolWayPointNode() {}

CLASS_DECLARATION(Entity, CallVolumeWayPointNode, "info_waypointnode_callvolume") {
    {&EV_WayPoint_SetActorAnim, &CallVolumeWayPointNode::SetActorAnim     },
    {&EV_WayPoint_SetThread,    &CallVolumeWayPointNode::SetWayPointThread},
    {NULL,                      NULL                                      }
};

CallVolumeWayPointNode::CallVolumeWayPointNode()
{
    SetWayPointThread(str(""));
    SetActorAnim(str(""));
}

CallVolumeWayPointNode::~CallVolumeWayPointNode() {}

CLASS_DECLARATION(Entity, PositionWayPointNode, "info_waypointnode_position") {
    {&EV_WayPoint_SetActorAnim, &PositionWayPointNode::SetActorAnim     },
    {&EV_WayPoint_SetThread,    &PositionWayPointNode::SetWayPointThread},
    {NULL,                      NULL                                    }
};

PositionWayPointNode::PositionWayPointNode()
{
    SetWayPointThread(str(""));
    SetActorAnim(str(""));
    _reserved = qfalse;
}

PositionWayPointNode::~PositionWayPointNode() {}

void PositionWayPointNode::Reserve(qboolean reserve)
{
    _reserved = reserve;
}

qboolean PositionWayPointNode::IsReserved()
{
    return _reserved;
}
