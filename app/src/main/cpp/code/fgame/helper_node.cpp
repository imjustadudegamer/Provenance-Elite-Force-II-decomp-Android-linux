
#include "actor.h"
#include "behavior.h"
#include "helper_node.h"
#include "player.h"
#include "scriptmaster.h"
#include "scriptthread.h"
#include "path.h" 

extern Event EV_SpawnFlags;

Event EV_HelperNodeController_SendEvent (	"sendevent",
	EV_DEFAULT,
	"ssSSSSSSS",
	"targetname eventName token1 token2 token3 token4 token5 token6 token7",
	"Sends the event to the specified node"
);



char CoverTypeStrings[COVER_TYPE_TOTAL_NUMBER][32]      = { "none", "crate", "wall" };
char CoverDirStrings[COVER_DIRECTION_TOTAL_NUMBER][32]  = { "none", "left", "right", "all" };
char DescriptorTypeStrings[DESCRIPTOR_TOTAL_NUMBER][32] = { "none", "corridor" };

Container<HelperNodePtr> HelperNodes;

void AddHelperNodeToList(HelperNode *node)
{
    if (node) {
#pragma GCC diagnostic push
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wdangling-pointer"
#endif
        HelperNodes.AddUniqueObject(HelperNodePtr(node));
#pragma GCC diagnostic pop
    }
}

void RemoveHelperNodeFromList(HelperNode *node)
{
    HelperNode *checkNode;

    for (int i = HelperNodes.NumObjects(); i > 0; i--) {
        checkNode = HelperNodes.ObjectAt(i);
        if (checkNode == node) {
            delete checkNode;
            checkNode = NULL;
            HelperNodes.RemoveObjectAt(i);
            return;
        }
    }
}


Event EV_HelperNode_SetNodeAnim (    "setnodeanim",
    EV_SCRIPTONLY,
    "s",
    "animation",
    "Sets the animation that will be played by the actor on this node"
);
Event EV_HelperNode_SetNodeEntryThread (    "setnodeentrythread",
    EV_SCRIPTONLY,
    "s",
    "thread",
    "Sets the thread that will be called by the actor when it reaches this node"
);
Event EV_HelperNode_SetNodeExitThread (    "setnodeexitthread",
    EV_SCRIPTONLY,
    "s",
    "thread",
    "Sets the thread that will be called by the actor when it leaves this node"
);
Event EV_HelperNode_SetAnimCount (    "animcount",
    EV_SCRIPTONLY,
    "i",
    "num_times",
    "Sets the max number of times that an animation can be set on this node"
);
Event EV_HelperNode_SetAnimTarget (    "animtarget",
    EV_SCRIPTONLY,
    "s",
    "targetname",
    "Allows you to specify a target name to apply the animation to"
);
Event EV_HelperNode_SetAnimActive (    "animactive",
    EV_SCRIPTONLY,
    "b",
    "active",
    "Specifies if the animation component is active"
);
Event EV_HelperNode_SetCoverType (    "setcovertype",
    EV_SCRIPTONLY,
    "s",
    "cover_type",
    "Specifies the covertype of the node"
);
Event EV_HelperNode_SetCoverDirection (    "setcoverdir",
    EV_SCRIPTONLY,
    "s",
    "cover_dir",
    "Specifies the direction from the cover that the AI can attack"
);
extern Event EV_Trigger_SetWait;

Event EV_HelperNode_SetWait (    "wait",
    EV_SCRIPTONLY,
    "f",
    "time",
    "Makes a patrolling AI pause at this node for the specified time");
Event EV_HelperNode_SetWaitRandom (    "waitrandom",
    EV_SCRIPTONLY,
    "f",
    "max_time",
    "Makes a patrolling AI pause randomly for a min time of 1 to a max time of specified"
);
Event EV_HelperNode_WaitForAnim (    "waitforanim",
    EV_SCRIPTONLY,
    "b",
    "wait_for_anim",
    "Makes the AI wait until the anim is finished before finishing a behavior"
);
Event EV_HelperNode_SetKillTarget (    "attacktarget",
    EV_SCRIPTONLY,
    "s",
    "targetname",
    "Specifies a target for a sniping AI to attack"
);
Event EV_HelperNode_SetCustomType (    "setcustomtype",
    EV_SCRIPTONLY,
    "s",
    "type",
    "Specifies a custom helper node type"
);
Event EV_HelperNode_SetMaxKills (    "maxkills",
    EV_SCRIPTONLY,
    "i",
    "max_kills",
    "Specifies the maximum number of kills that can be done from this node"
);
Event EV_HelperNode_SetMinHealth (    "minhealth",
    EV_SCRIPTONLY,
    "f",
    "min_health",
    "Specifies the minimum health an actor is allowed to get to before leaving this node"
);
Event EV_HelperNode_SetMinEnemyRange (    "minenemyrange",
    EV_SCRIPTONLY,
    "f",
    "min_range",
    "Specifies the minimum range an enemy is allowed to get before actor leaves this node"
);
Event EV_HelperNode_SetActivationRange (    "activationrange",
    EV_SCRIPTONLY,
    "f",
    "activation_range",
    "Specifies the range an actor needs to be to consider using the node"
);
Event EV_HelperNode_SetPriority (    "priority",
    EV_SCRIPTONLY,
    "f",
    "priority",
    "Specfies the priority for this node"
);
Event EV_HelperNode_SetNodeID (    "nodeID",
    EV_SCRIPTONLY,
    "i",
    "node_id",
    "Assigns an ID to this node"
);
extern Event EV_Actor_SetNodeID;

Event EV_HelperNode_SetCriticalChange (    "criticalchange",
    EV_SCRIPTONLY,
    NULL,
    NULL,
    "Alerts the node to a critical change"
);
Event EV_HelperNode_SetDescriptor (    "descriptor",
    EV_SCRIPTONLY,
    "s",
    "descriptorValue",
    "sets an additional descriptor for a node"
);
Event EV_HelperNode_SendCommand (    "sendnodecommand",
    EV_SCRIPTONLY,
    "s",
    "commandToSend",
    "sends an event to the operator of the node"
);
Event EV_HelperNode_AddAnimation (    "addanim",
    EV_SCRIPTONLY,
    "ss",
    "anim waittype",
    "Adds an animation to the custom anim list"
);

CLASS_DECLARATION(Entity, HelperNodeController, "helpernodecontroller") {
    {&EV_HelperNodeController_SendEvent, &HelperNodeController::SendEventToNode},
    {NULL,                               NULL                                  }
};

HelperNodeController::HelperNodeController()
{
    targetname = "nodecontroller";
}

HelperNodeController::~HelperNodeController() {}

void HelperNodeController::SendEventToNode(Event *ev)
{
    str         nodeTargetName;
    str         eventName;
    Event      *nodeEvent;
    HelperNode *node;
    int         i;

    nodeTargetName = ev->GetString(1);
    eventName      = ev->GetString(2);

    node = NULL;
    for (i = HelperNodes.NumObjects(); i > 0; i--) {
        node = HelperNodes.ObjectAt(i);
        if (node->targetname == nodeTargetName) {
            break;
        }
    }

    if (!node) {
        return;
    }

    nodeEvent = new Event(eventName.c_str());
    if (!nodeEvent) {
        return;
    }

    for (i = 3; i <= ev->NumArgs(); i++) { 
        nodeEvent->AddToken(ev->GetToken(i));
    }

    node->ProcessEvent(nodeEvent);
}

// Note retail does NOT chain Entity::Archive here.  The frame body is exactly an int
void HelperNodeController::Archive(Archiver& arc)
{
    HelperNode *checkNode;
    int         num, i;

    if (arc.Saving()) {
        num = HelperNodes.NumObjects();

        arc.ArchiveInteger(&num);

        for (i = 1; i <= num; i++) {
            checkNode = HelperNodes.ObjectAt(i);
            arc.ArchiveObject(checkNode);
        }
    } else {
        arc.ArchiveInteger(&num);

        HelperNodes.ClearObjectList();
        HelperNodes.Resize(num);

        for (i = 1; i <= num; i++) {
            checkNode = new HelperNode;
            arc.ArchiveObject(checkNode);
            HelperNodes.AddObject(checkNode);
        }
    }
}


CLASS_DECLARATION(Listener, HelperNode, "info_helpernode") {
    {&EV_SetTargetName,                 &HelperNode::SetTargetName     },
    {&EV_SetTarget,                     &HelperNode::SetTarget         },
    {&EV_SetAngle,                      &HelperNode::SetAngle          },
    {&EV_SetOrigin,                     &HelperNode::SetOrigin         },
    {&EV_SpawnFlags,                    &HelperNode::SetFlags          },
    {&EV_HelperNode_SetNodeAnim,        &HelperNode::SetAnim           },
    {&EV_HelperNode_SetNodeEntryThread, &HelperNode::SetEntryThread    },
    {&EV_HelperNode_SetNodeExitThread,  &HelperNode::SetExitThread     },
    {&EV_HelperNode_SetAnimCount,       &HelperNode::SetAnimCount      },
    {&EV_HelperNode_SetAnimTarget,      &HelperNode::SetAnimTarget     },
    {&EV_HelperNode_SetAnimActive,      &HelperNode::SetAnimActive     },
    {&EV_HelperNode_SetCoverType,       &HelperNode::SetCoverType      },
    {&EV_HelperNode_SetCoverDirection,  &HelperNode::SetCoverDir       },
    {&EV_HelperNode_SetWait,            &HelperNode::SetWait           },
    {&EV_HelperNode_SetWaitRandom,      &HelperNode::SetWaitRandom     },
    {&EV_HelperNode_WaitForAnim,        &HelperNode::SetWaitForAnim    },
    {&EV_HelperNode_SetKillTarget,      &HelperNode::SetKillTarget     },
    {&EV_HelperNode_SetCustomType,      &HelperNode::SetCustomType     },
    {&EV_HelperNode_SetMaxKills,        &HelperNode::SetMaxKills       },
    {&EV_HelperNode_SetMinHealth,       &HelperNode::SetMinHealth      },
    {&EV_HelperNode_SetMinEnemyRange,   &HelperNode::SetMinEnemyRange  },
    {&EV_HelperNode_SetActivationRange, &HelperNode::SetActivationRange},
    {&EV_HelperNode_SetPriority,        &HelperNode::SetPriority       },
    {&EV_HelperNode_SetNodeID,          &HelperNode::SetID             },
    {&EV_HelperNode_SetCriticalChange,  &HelperNode::SetCriticalChange },
    {&EV_HelperNode_SetDescriptor,      &HelperNode::SetDescriptor     },
    {&EV_HelperNode_SendCommand,        &HelperNode::SendCommand       },
    {&EV_HelperNode_AddAnimation,       &HelperNode::AddAnimation      },
    {NULL,                              NULL                          }
};

HelperNode::HelperNode()
{
    if (!LoadingSavegame) {
        AddHelperNodeToList(this);
        _init();
    }
}

void HelperNode::SetTarget(Event *ev)
{
    SetTarget(ev->GetString(1));
}

void HelperNode::SetTargetName(Event *ev)
{
    SetTargetName(ev->GetString(1));
}

void HelperNode::SetAngle(Event *ev)
{
    Vector movedir = G_GetMovedir(ev->GetFloat(1));
    SetAngle(movedir.toAngles());
}

void HelperNode::SetOrigin(Event *ev)
{
    SetOrigin(ev->GetVector(1));
}

void HelperNode::SetTarget(const str& name)
{
    target = name;
}

void HelperNode::SetTargetName(const str& name)
{
    targetname = name;
}

void HelperNode::SetAngle(const Vector& ang)
{
    angles = ang;
}

void HelperNode::SetOrigin(const Vector& pos)
{
    origin = pos;
}

HelperNode::~HelperNode()
{
    customAnimListEntry_t *checkEntry;

    for (int i = _customAnimList.NumObjects(); i > 0; i--) {
        checkEntry = _customAnimList.ObjectAt(i);
        delete checkEntry;
        checkEntry = NULL;
        _customAnimList.RemoveObjectAt(i);
    }
}

void HelperNode::_init()
{
    _entryThread = "";
    _exitThread  = "";
    _anim        = "";
    _animtarget  = "";
    _killtarget  = "";
    _customType  = "";

    _id        = -1;
    _animcount = -1;
    _maxkills  = -1;

    _animactive     = qtrue;
    _coveractive    = qtrue;
    _waitrandom     = qfalse;
    _waitforanim    = qfalse;
    _criticalchange = qfalse;

    _waittime        = 0.0f;
    _minhealth       = 0.0f;
    _minenemyrange   = 0.0f;
    _activationRange = 0.0f;
    _priority        = 0.0f;
    _lastUseTime     = 0.0f;

    _covertype  = COVER_TYPE_NONE;
    _coverdir   = COVER_DIRECTION_NONE;
    _descriptor = DESCRIPTOR_NONE;

    _reserved  = false;
    _nodeflags = 0;

    _customAnimListIndex = 1; 
    _usingCustomAnimList = false;
    _user                = NULL;
}

CoverType_t HelperNode::_CoverTypeForString(const str& covertype)
{
    CoverType_t coverType;

    for (int i = 0; i < COVER_TYPE_TOTAL_NUMBER; i++) {
        if (!Q_stricmp(covertype.c_str(), CoverTypeStrings[i])) {
            coverType = (CoverType_t)i;
            return coverType;
        }
    }

    gi.WDPrintf("Unknown CoverType - %s\n", covertype.c_str());
    return COVER_TYPE_NONE;
}

CoverDirection_t HelperNode::_CoverDirectionForString(const str& coverdir)
{
    CoverDirection_t coverDir;

    for (int i = 0; i < COVER_DIRECTION_TOTAL_NUMBER; i++) {
        if (!Q_stricmp(coverdir.c_str(), CoverDirStrings[i])) {
            coverDir = (CoverDirection_t)i;
            return coverDir;
        }
    }

    gi.WDPrintf("Unknown CoverDirection - %s\n", coverdir.c_str());
    return COVER_DIRECTION_NONE;
}

NodeDescriptorType_t HelperNode::_DescriptorForString(const str& descriptor)
{
    NodeDescriptorType_t descriptorType;

    for (int i = 0; i < DESCRIPTOR_TOTAL_NUMBER; i++) {
        if (!Q_stricmp(descriptor.c_str(), DescriptorTypeStrings[i])) {
            descriptorType = (NodeDescriptorType_t)i;
            return descriptorType;
        }
    }

    gi.WDPrintf("Unknown Descriptor - %s\n", descriptor.c_str());
    return DESCRIPTOR_NONE;
}

// retail, not WAITTYPE_ANIM.  The port used Q_stricmp (case-insensitive).  Retail's
waitType_t HelperNode::GetWaitType(const str& waitType)
{
    if (waitType == "anim") {
        return WAITTYPE_ANIM;
    } else if (waitType == "event") {
        return WAITTYPE_EVENT;
    } else if (waitType == "time") {
        return WAITTYPE_TIME;
    }

    assert("Unknown waitType");
    return WAITTYPE_ERROR;
}

void HelperNode::SetEntryThread(Event *ev) { SetEntryThread(ev->GetString(1)); }
void HelperNode::SetExitThread(Event *ev) { SetExitThread(ev->GetString(1)); }
void HelperNode::SetAnim(Event *ev) { SetAnim(ev->GetString(1)); }
void HelperNode::SetAnimTarget(Event *ev) { SetAnimTarget(ev->GetString(1)); }
void HelperNode::SetKillTarget(Event *ev) { SetKillTarget(ev->GetString(1)); }
void HelperNode::SetCustomType(Event *ev) { SetCustomType(ev->GetString(1)); }
void HelperNode::SetID(Event *ev) { SetID(ev->GetInteger(1)); }
void HelperNode::SetAnimCount(Event *ev) { SetAnimCount(ev->GetInteger(1)); }
void HelperNode::SetMaxKills(Event *ev) { SetMaxKills(ev->GetInteger(1)); }
void HelperNode::SetWait(Event *ev) { SetWait(ev->GetFloat(1)); }
void HelperNode::SetWaitRandom(Event *ev) { SetWaitRandom(ev->GetFloat(1)); }
void HelperNode::SetMinHealth(Event *ev) { SetMinHealth(ev->GetFloat(1)); }
void HelperNode::SetMinEnemyRange(Event *ev) { SetMinEnemyRange(ev->GetFloat(1)); }
void HelperNode::SetActivationRange(Event *ev) { SetActivationRange(ev->GetFloat(1)); }
void HelperNode::SetPriority(Event *ev) { SetPriority(ev->GetFloat(1)); }
void HelperNode::SetFlags(Event *ev) { SetFlags((unsigned int)ev->GetInteger(1)); }
void HelperNode::SetAnimActive(Event *ev) { SetAnimActive(ev->GetBoolean(1)); }
void HelperNode::SetCoverActive(Event *ev) { SetCoverActive(ev->GetBoolean(1)); }
void HelperNode::SetWaitForAnim(Event *ev) { SetWaitForAnim(ev->GetBoolean(1)); }
void HelperNode::SetCoverType(Event *ev) { SetCoverType(_CoverTypeForString(ev->GetString(1))); }
void HelperNode::SetCoverDir(Event *ev) { SetCoverDir(_CoverDirectionForString(ev->GetString(1))); }
void HelperNode::SetDescriptor(Event *ev) { SetDescriptor(_DescriptorForString(ev->GetString(1))); }

void HelperNode::SetCriticalChange(Event *ev)
{
    SetCriticalChange(qtrue);
}

void HelperNode::SendCommand(Event *ev)
{
    Event *commandEvent;
    int    numArgs;


    if (!_user) {
        return;
    }

    commandEvent = new Event(EV_HelperNodeCommand);

    numArgs = ev->NumArgs();
    for (int i = 1; i <= numArgs; i++) {
        commandEvent->AddToken(ev->GetToken(i));
    }

    _user->ProcessEvent(commandEvent);
}

void HelperNode::AddAnimation(Event *ev)
{
    str        anim     = ev->GetString(1);
    waitType_t waitType = GetWaitType(ev->GetString(2));
    float      time     = ev->NumArgs() > 2 ? ev->GetFloat(3) : 0.0f;

    AddAnimation(anim, waitType, time);
}

void HelperNode::AddAnimation(const str& anim, waitType_t waitType, float time)
{
    customAnimListEntry_t *entry = new customAnimListEntry_t;

    entry->anim     = anim;
    entry->waitType = waitType;
    entry->time     = time;

    _customAnimList.AddObject(entry);
    _usingCustomAnimList = true;
}

void HelperNode::SetEntryThread(const str& thread) { _entryThread = thread; }
void HelperNode::SetExitThread(const str& thread) { _exitThread = thread; }
void HelperNode::SetAnim(const str& anim) { _anim = anim; }
void HelperNode::SetAnimTarget(const str& animtarget) { _animtarget = animtarget; }
void HelperNode::SetKillTarget(const str& killtarget) { _killtarget = killtarget; }
void HelperNode::SetID(int ID) { _id = ID; }
void HelperNode::SetAnimCount(int animcount) { _animcount = animcount; }
void HelperNode::SetMaxKills(int maxkills) { _maxkills = maxkills; }
void HelperNode::SetWait(float wait) { _waittime = wait; }
void HelperNode::SetMinHealth(float health) { _minhealth = health; }
void HelperNode::SetMinEnemyRange(float range) { _minenemyrange = range; }
void HelperNode::SetActivationRange(float range) { _activationRange = range; }
void HelperNode::SetFlags(unsigned int flags) { _nodeflags = flags; }
void HelperNode::SetAnimActive(qboolean animactive) { _animactive = animactive; }
void HelperNode::SetCoverActive(qboolean coveractive) { _coveractive = coveractive; }
void HelperNode::SetWaitForAnim(qboolean wait) { _waitforanim = wait; }
void HelperNode::SetCoverType(CoverType_t covertype) { _covertype = covertype; }
void HelperNode::SetCoverDir(CoverDirection_t coverdir) { _coverdir = coverdir; }
void HelperNode::SetCriticalChange(qboolean change) { _criticalchange = change; }
void HelperNode::SetDescriptor(NodeDescriptorType_t descriptor) { _descriptor = descriptor; }
void HelperNode::SetPriority(float priority) { _priority = priority; }

void HelperNode::SetWaitRandom(float wait)
{
    _waittime   = wait;
    _waitrandom = qtrue;
}

void HelperNode::SetCustomType(const str& customtype)
{
    _customType = customtype;
    _nodeflags |= NODETYPE_CUSTOM;
}

const str& HelperNode::GetEntryThread() { return _entryThread; }
const str& HelperNode::GetExitThread() { return _exitThread; }
const str& HelperNode::GetAnim() { return _anim; }
const str& HelperNode::GetAnimTarget() { return _animtarget; }
const str& HelperNode::GetKillTarget() { return _killtarget; }
const str& HelperNode::GetCustomType() { return _customType; }

int HelperNode::GetID() { return _id; }
int HelperNode::GetAnimCount() { return _animcount; }
int HelperNode::GetMaxKills() { return _maxkills; }

float HelperNode::GetWaitTime()
{
    return _waittime;
}

float HelperNode::GetMinHealth() { return _minhealth; }
float HelperNode::GetMinEnemyRange() { return _minenemyrange; }
float HelperNode::GetActivationRange() { return _activationRange; }
float HelperNode::GetPriority() { return _priority; }

CoverType_t          HelperNode::GetCoverType() { return _covertype; }
CoverDirection_t     HelperNode::GetCoverDirection() { return _coverdir; }
NodeDescriptorType_t HelperNode::GetDescriptor() { return _descriptor; }

customAnimListEntry_t *HelperNode::GetCurrentAnimEntryFromList()
{
    if (_customAnimListIndex < 1 || _customAnimListIndex > _customAnimList.NumObjects()) {
        return NULL;
    }
    return _customAnimList.ObjectAt(_customAnimListIndex);
}

customAnimListEntry_t *HelperNode::GetNextAnimEntryFromList()
{
    int animCount = _customAnimList.NumObjects();
    int index     = _customAnimListIndex;

    if (index < animCount) {
        index++;
    }

    return _customAnimList.ObjectAt(index);
}

qboolean HelperNode::isOfType(unsigned int mask) { return (_nodeflags & mask) != 0; }
qboolean HelperNode::isAnimActive() { return _animactive; }
qboolean HelperNode::isCoverActive() { return _coveractive; }
qboolean HelperNode::isWaitRandom() { return _waitrandom; }
qboolean HelperNode::isWaitForAnim() { return _waitforanim; }
qboolean HelperNode::isChanged() { return _criticalchange; }
qboolean HelperNode::isReserved() { return _reserved; }

bool HelperNode::isAnimListFinished()
{
    return _customAnimListIndex > _customAnimList.NumObjects();
}

// throwing when the label does not resolve -- retail's warn-and-continue.

void HelperNode::RunEntryThread()
{
    if (_entryThread.length() <= 0) {
        return;
    }

    CThread *thread;

    thread = Director.CreateThread(_entryThread);

    if (thread) {
        thread->DelayedStart(0.0f);
    }
}

void HelperNode::RunExitThread()
{
    if (_exitThread.length() <= 0) {
        return;
    }

    CThread *thread;

    thread = Director.CreateThread(_exitThread);

    if (thread) {
        thread->DelayedStart(0.0f);
    }
}

void HelperNode::NextAnim()
{
    _customAnimListIndex++;
}

void HelperNode::Archive(Archiver& arc)
{

    arc.ArchiveVector(&origin);
    arc.ArchiveString(&target);
    arc.ArchiveString(&targetname);
    arc.ArchiveVector(&angles);

    arc.ArchiveString(&_entryThread);
    arc.ArchiveString(&_exitThread);
    arc.ArchiveString(&_anim);
    arc.ArchiveString(&_animtarget);
    arc.ArchiveString(&_killtarget);
    arc.ArchiveString(&_customType);

    arc.ArchiveInteger(&_id);
    arc.ArchiveInteger(&_animcount);
    arc.ArchiveInteger(&_maxkills);

    arc.ArchiveBoolean(&_animactive);
    arc.ArchiveBoolean(&_coveractive);
    arc.ArchiveBoolean(&_waitrandom);
    arc.ArchiveBoolean(&_waitforanim);
    arc.ArchiveBoolean(&_criticalchange);
    arc.ArchiveBool(&_reserved);

    arc.ArchiveFloat(&_waittime);
    arc.ArchiveFloat(&_minhealth);
    arc.ArchiveFloat(&_minenemyrange);
    arc.ArchiveFloat(&_activationRange);
    arc.ArchiveFloat(&_priority);
    arc.ArchiveFloat(&_lastUseTime);

    ArchiveEnum(_covertype, CoverType_t);
    ArchiveEnum(_coverdir, CoverDirection_t);

    arc.ArchiveUnsigned(&_nodeflags);
    ArchiveEnum(_descriptor, NodeDescriptorType_t);

    arc.ArchiveSafePointer(&_user);

    arc.ArchiveInteger(&_customAnimListIndex);
    arc.ArchiveBool(&_usingCustomAnimList);

    int count = _customAnimList.NumObjects();
    arc.ArchiveInteger(&count);

    if (arc.Loading()) {
        for (int i = _customAnimList.NumObjects(); i > 0; i--) {
            delete _customAnimList.ObjectAt(i);
        }
        _customAnimList.FreeObjectList();

        for (int i = 0; i < count; i++) {
            _customAnimList.AddObject(new customAnimListEntry_t);
        }
    }

    for (int i = 1; i <= count; i++) {
        customAnimListEntry_t *entry = _customAnimList.ObjectAt(i);
        arc.ArchiveString(&entry->anim);
        ArchiveEnum(entry->waitType, waitType_t);
        arc.ArchiveFloat(&entry->time);
    }
}

unsigned int HelperNode::GetHelperNodeMask(const str& type)
{
    unsigned int mask = 0;

    if (!Q_stricmp(type.c_str(), "flee")) {
        mask |= NODETYPE_FLEE;
    } else if (!Q_stricmp(type.c_str(), "work")) {
        mask |= NODETYPE_WORK;
    } else if (!Q_stricmp(type.c_str(), "anim")) {
        mask |= NODETYPE_ANIM;
    } else if (!Q_stricmp(type.c_str(), "cover")) {
        mask |= NODETYPE_COVER;
    } else if (!Q_stricmp(type.c_str(), "patrol")) {
        mask |= NODETYPE_PATROL;
    } else if (!Q_stricmp(type.c_str(), "sniper")) {
        mask |= NODETYPE_SNIPER;
    } else if (!Q_stricmp(type.c_str(), "custom")) {
        mask |= NODETYPE_CUSTOM;
    } else if (!Q_stricmp(type.c_str(), "combat")) {
        mask |= NODETYPE_COMBAT;
    }

    return mask;
}

// @0x0022aff0 (84 bytes).  HelperNode derives from Listener, NOT Entity, so
void HelperNode::CleanupHelperNodeList()
{
    HelperNode *checkNode;

    for (int i = HelperNodes.NumObjects(); i > 0; i--) {
        checkNode = HelperNodes.ObjectAt(i);
        if (checkNode) {
            delete checkNode;
            checkNode = NULL;
            HelperNodes.RemoveObjectAt(i);
        }
    }
}


HelperNode *HelperNode::FindClosestHelperNode(
    Actor& self, int mask, float maxDist, float minDistanceFromPlayer, bool unreserveCurrentNode
)
{
    //   1. the `node_id` ownership gate (retail: `if (nodeID != -1 && nodeID !=
    HelperNode      *node;
    HelperNode      *bestNode;
    float            pathLen;
    float            bestDist;
    float            activationRange;
    FindMovementPath find;
    Path            *path;
    int              nodeID;


    find.heuristic.self = &self;
    find.heuristic.setSize( self.size );
    find.heuristic.entnum = self.entnum;

    bestNode   = NULL;
    bestDist   = 999999999.8f;
    pathLen    = bestDist - 1.0f;


    for ( int i = 1 ; i <= HelperNodes.NumObjects() ; i++ )
    {
        node = NULL;
        node = HelperNodes.ObjectAt( i );
        nodeID = node->GetID();

        if ( nodeID != -1 && nodeID != self.currentHelperNode.nodeID )
            continue;

        if ( node && node->isOfType(mask))
        {
            activationRange = node->GetActivationRange();


            if ( activationRange > 0 )
            {
                if ( !self.WithinDistance( node->origin, activationRange ) )
                    continue;
            }
            else
            {
                if ( !self.WithinDistance( node->origin, maxDist ) )
                    continue;
            }

            if ( node == self.ignoreHelperNode.node )
                continue;

            if ( node->isReserved() )
                continue;

            Player* player;
            player = GetPlayer(0);
            if ( !player ) return NULL;

            if ( player->WithinDistance( node->origin , minDistanceFromPlayer ) )
                continue;

            path = find.FindPath( self.origin, node->origin );

            if ( path )
                pathLen = path->Length();

            delete path;
            path = NULL;

            if ( pathLen < bestDist )
            {
                bestDist = pathLen;
                bestNode = node;
            }

        }


    }

    if ( self.currentHelperNode.node && unreserveCurrentNode )
        self.currentHelperNode.node->UnreserveNode();

    self.currentHelperNode.node = bestNode;
    self.currentHelperNode.mask = mask;

    if ( bestNode )
        bestNode->ReserveNode();

    return bestNode;
}

HelperNode *HelperNode::FindClosestHelperNode(Actor& self, int mask, NodeDescriptorType_t descriptor, float maxDist)
{
    HelperNode      *node;
    HelperNode      *bestNode;
    float            pathLen;
    float            bestDist;
    float            activationRange;
    FindMovementPath find;
    Path            *path;
    int              nodeID;

    find.heuristic.self = &self;
    find.heuristic.setSize(self.size);
    find.heuristic.entnum = self.entnum;

    bestNode   = NULL;
    bestDist   = 999999999.8f;
    pathLen    = bestDist - 1.0f;

    for (int i = 1; i <= HelperNodes.NumObjects(); i++) {
        node   = NULL;
        node   = HelperNodes.ObjectAt(i);
        nodeID = node->GetID();

        if (nodeID != -1 && nodeID != self.currentHelperNode.nodeID) {
            continue;
        }

        if (node && node->isOfType(mask) && node->GetDescriptor() == descriptor) {
            activationRange = node->GetActivationRange();

            if (node->isReserved()) {
                continue;
            }

            if (node == self.ignoreHelperNode.node) {
                continue;
            }

            if (activationRange > 0) {
                if (!self.WithinDistance(node->origin, activationRange)) {
                    continue;
                }
            } else {
                if (!self.WithinDistance(node->origin, maxDist)) {
                    continue;
                }
            }

            path = find.FindPath(self.origin, node->origin);

            if (path) {
                pathLen = path->Length();
            }

            delete path;
            path = NULL;

            if (pathLen < bestDist) {
                bestDist = pathLen;
                bestNode = node;
            }
        }
    }

    if (self.currentHelperNode.node) {
        self.currentHelperNode.node->UnreserveNode();
    }

    self.currentHelperNode.node = bestNode;
    self.currentHelperNode.mask = mask;

    if (bestNode) {
        bestNode->ReserveNode();
    }

    return bestNode;
}

HelperNode *HelperNode::FindClosestHelperNodeWithoutPathing(Actor& self, float maxDist)
{
    HelperNode *bestNode = NULL;
    float       bestDistance = 999999999.8f;

    for (int i = 1; i <= HelperNodes.NumObjects(); i++) {
        HelperNode *node = NULL;
        node = HelperNodes.ObjectAt(i);
        int nodeID = node->GetID();

        if ((nodeID != -1) && (nodeID != self.currentHelperNode.nodeID)) {
            continue;
        }

        float activationRange = node->GetActivationRange();

        if (activationRange > 0) {
            if (!self.WithinDistance(node->origin, activationRange)) {
                continue;
            }
        } else {
            if (!self.WithinDistance(node->origin, maxDist)) {
                continue;
            }
        }

        float currentDistance = Vector::Distance(self.origin, node->origin);

        if (currentDistance < bestDistance) {
            bestDistance = currentDistance;
            bestNode     = node;
        }
    }

    self.currentHelperNode.node = bestNode;
    return bestNode;
}
HelperNode *HelperNode::FindClosestHelperNodeAtDistanceFrom(
    Actor& self, Entity *ent, int mask, NodeDescriptorType_t descriptor, float maxDistFromSelf, float minDistFromEnt
)
{
    HelperNode      *node;
    HelperNode      *bestNode;
    float            pathLen;
    float            bestDist;
    float            activationRange;
    StandardMovePath find;
    Path            *path;
    int              nodeID;

    find.heuristic.setSize(self.size);
    find.heuristic.entnum = self.entnum;

    bestNode = NULL;
    bestDist = 999999999.8f;
    pathLen  = bestDist - 1.0f;

    for (int i = 1; i <= HelperNodes.NumObjects(); i++) {
        node   = NULL;
        node   = HelperNodes.ObjectAt(i);
        nodeID = node->GetID();

        if (nodeID != -1 && nodeID != self.currentHelperNode.nodeID) {
            continue;
        }

        if (node && node->isOfType(mask) && node->GetDescriptor() == descriptor) {
            activationRange = node->GetActivationRange();

            if (node->isReserved()) {
                continue;
            }

            if (activationRange > 0) {
                if (!self.WithinDistance(node->origin, activationRange)) {
                    continue;
                }
            } else {
                if (!self.WithinDistance(node->origin, maxDistFromSelf)) {
                    continue;
                }
            }

            if (ent->WithinDistance(node->origin, minDistFromEnt)) {
                continue;
            }

            path = find.FindPath(self.origin, node->origin);

            if (path) {
                pathLen = path->Length();
            }

            delete path;
            path = NULL;

            if (pathLen < bestDist) {
                bestDist = pathLen;
                bestNode = node;
            }
        }
    }

    self.currentHelperNode.node = bestNode;
    self.currentHelperNode.mask = mask;
    return bestNode;
}

HelperNode *HelperNode::FindClosestHelperNode(Actor& self, const str& customType, float maxDist)
{
    HelperNode      *node;
    HelperNode      *bestNode;
    float            pathLen;
    float            bestDist;
    FindMovementPath find;
    Path            *path;
    int              nodeID;

    find.heuristic.self = &self;
    find.heuristic.setSize(self.size);
    find.heuristic.entnum = self.entnum;

    bestNode   = NULL;
    bestDist   = 999999999.8f;
    pathLen    = bestDist - 1.0f;


    for (int i = 1; i <= HelperNodes.NumObjects(); i++) {
        node   = NULL;
        node   = HelperNodes.ObjectAt(i);
        nodeID = node->GetID();

        if (nodeID != -1 && nodeID != self.currentHelperNode.nodeID) {
            continue;
        }

        if (node == self.ignoreHelperNode.node) {
            continue;
        }

        if (node->isReserved()) {
            continue;
        }

        if (node && node->isOfType(NODETYPE_CUSTOM)) {
            str type;
            type = node->GetCustomType();

            if (!Q_stricmp(type.c_str(), customType.c_str())) {
                if (!self.WithinDistance(node->origin, maxDist)) {
                    continue;
                }

                path = find.FindPath(self.origin, node->origin);

                if (path) {
                    pathLen = path->Length();
                }

                delete path;
                path = NULL;

                if (pathLen < bestDist) {
                    bestDist = pathLen;
                    bestNode = node;
                }
            }
        }
    }

    if (self.currentHelperNode.node) {
        self.currentHelperNode.node->UnreserveNode();
    }

    self.currentHelperNode.node = bestNode;
    self.currentHelperNode.mask = NODETYPE_CUSTOM;

    if (bestNode) {
        bestNode->ReserveNode();
    }

    return bestNode;
}

HelperNode *HelperNode::FindClosestHelperNode(Actor& self, const str& targetName)
{
    HelperNode      *node;
    HelperNode      *bestNode;
    float            pathLen;
    float            bestDist;
    FindMovementPath find;
    Path            *path;

    find.heuristic.self = &self;
    find.heuristic.setSize(self.size);
    find.heuristic.entnum = self.entnum;

    bestNode   = NULL;
    bestDist   = 999999999.8f;
    pathLen    = bestDist - 1.0f;

    for (int i = 1; i <= HelperNodes.NumObjects(); i++) {
        node = NULL;
        node = HelperNodes.ObjectAt(i);

        if (node && node->targetname == targetName) {
            path = find.FindPath(self.origin, node->origin);

            if (path) {
                pathLen = path->Length();
            }

            delete path;
            path = NULL;

            if (pathLen < bestDist) {
                bestDist = pathLen;
                bestNode = node;
            }
        }
    }

    self.currentHelperNode.node = bestNode;
    return bestNode;
}

HelperNode *HelperNode::FindClosestHelperNodeThatCannotSeeEntity(
    Actor& self, int mask, unsigned int clipMask, float maxDist, float minDist, Entity *ent, float minDistFromPlayer
)
{
    HelperNode      *node;
    HelperNode      *bestNode;
    float            pathLen;
    float            bestDist;
    float            activationRange;
    StandardMovePath find;
    Path            *path;
    int              nodeID;
    trace_t          trace;

    find.heuristic.setSize(self.size);
    find.heuristic.entnum = self.entnum;

    bestNode = NULL;
    bestDist = 999999999.8f;
    pathLen  = bestDist - 1.0f;

    for (int i = 1; i <= HelperNodes.NumObjects(); i++) {
        node   = NULL;
        node   = HelperNodes.ObjectAt(i);
        nodeID = node->GetID();

        if (nodeID != -1 && nodeID != self.currentHelperNode.nodeID) {
            continue;
        }

        if (node && node->isOfType(mask)) {
            activationRange = node->GetActivationRange();

            if (activationRange > 0) {
                if (!self.WithinDistance(node->origin, activationRange)) {
                    continue;
                }
            } else {
                if (!self.WithinDistance(node->origin, maxDist)) {
                    continue;
                }
            }

            if (node->isReserved()) {
                continue;
            }

            Player *player;
            player = GetPlayer(0);
            if (!player) {
                return NULL;  
            }

            if (player->WithinDistance(node->origin, minDistFromPlayer)) {
                continue;
            }

            path = find.FindPath(self.origin, node->origin);

            if (!path) {
                continue;
            }

            pathLen = path->Length();

            delete path;
            path = NULL;

            if (pathLen < bestDist && !ent->WithinDistance(node->origin, minDist)) {
                Vector endPos;
                endPos = ent->centroid;

                if (ent->isSubclassOf(Actor)) {
                    Actor *theActor;
                    theActor = (Actor *)ent;
                    endPos   = theActor->EyePosition();
                }

                trace = G_Trace(node->origin, vec_zero, vec_zero, endPos, NULL, MASK_OPAQUE, false, "FindClosestHelperNode");
                if (trace.fraction > 0.9f) {
                    continue;
                }

                bestDist = pathLen;
                bestNode = node;
            }
        }
    }

    if (bestNode && self.currentHelperNode.node) {
        self.currentHelperNode.node->UnreserveNode();
    }

    if (bestNode) {
        self.currentHelperNode.node = bestNode;
        self.currentHelperNode.mask = mask;
    }

    if (bestNode) {
        bestNode->ReserveNode();
    }

    return bestNode;
}
// HelperNode_Usable (whose isReserved test is loosened against retail) and never
HelperNode *HelperNode::FindHighestPriorityNode(Actor& self, const str& customType)
{
    HelperNode *node;
    HelperNode *bestNode;
    float       priority;
    float       bestPriority;
    int         nodeID;

    bestNode     = NULL;
    priority     = 0.0f;
    bestPriority = 0.0f;

    for (int i = 1; i <= HelperNodes.NumObjects(); i++) {
        node   = NULL;
        node   = HelperNodes.ObjectAt(i);
        nodeID = node->GetID();

        if (nodeID != -1 && nodeID != self.currentHelperNode.nodeID) {
            continue;
        }

        if (node->isReserved()) {
            continue;
        }

        if (node->isOfType(NODETYPE_CUSTOM)) {
            str type;
            type = node->GetCustomType();

            if (!Q_stricmp(type.c_str(), customType.c_str())) {
                priority = node->GetPriority();
                if (priority > bestPriority) {
                    bestPriority = priority;
                    bestNode     = node;
                }
            }
        }
    }

    self.currentHelperNode.node = bestNode;
    self.currentHelperNode.mask = NODETYPE_CUSTOM;
    return bestNode;
}

HelperNode *HelperNode::FindHighestPriorityNode(Actor& self, const str& customType, const str& targetedTo)
{
    HelperNode *node;
    HelperNode *bestNode;
    float       priority;
    float       bestPriority;
    int         nodeID;

    bestNode     = NULL;
    priority     = 0.0f;
    bestPriority = 999999.0f;

    for (int i = 1; i <= HelperNodes.NumObjects(); i++) {
        node   = NULL;
        node   = HelperNodes.ObjectAt(i);
        nodeID = node->GetID();

        if (nodeID != -1 && nodeID != self.currentHelperNode.nodeID) {
            continue;
        }

        if (node->isReserved()) {
            continue;
        }

        if (node->isOfType(NODETYPE_CUSTOM)) {
            str type;
            type = node->GetCustomType();

            if (!Q_stricmp(type.c_str(), customType.c_str())
                && !Q_stricmp(node->target.c_str(), targetedTo.c_str())) {
                priority = node->GetPriority();
                if (priority < bestPriority) {
                    bestPriority = priority;
                    bestNode     = node;
                }
            }
        }
    }

    self.currentHelperNode.node = bestNode;
    self.currentHelperNode.mask = NODETYPE_CUSTOM;
    return bestNode;
}

HelperNode *HelperNode::GetTargetedHelperNode(const str& targetName)
{
    for (int i = 1; i <= HelperNodes.NumObjects(); i++) {
        HelperNode *node = HelperNodes.ObjectAt(i);

        if (!node) {
            continue;
        }
        if (!Q_stricmp(node->targetname.c_str(), targetName.c_str())) {
            return node;
        }
    }

    return NULL;
}

HelperNode *HelperNode::FindHelperNodeClosestTo(Actor& self, Entity *ent, int mask, float maxDist)
{
    HelperNode      *node;
    HelperNode      *bestNode;
    float            pathLen;
    float            bestDist;
    float            activationRange;
    StandardMovePath find; 
    Path            *path;
    int              nodeID;

    if (!ent) {
        return NULL;
    }

    find.heuristic.setSize(self.size);
    find.heuristic.entnum = self.entnum;

    bestNode = NULL;
    bestDist = 999999999.8f;
    pathLen  = bestDist - 1.0f;

    for (int i = 1; i <= HelperNodes.NumObjects(); i++) {
        node   = NULL;
        node   = HelperNodes.ObjectAt(i);
        nodeID = node->GetID();

        if (nodeID != -1 && nodeID != self.currentHelperNode.nodeID) {
            continue;
        }

        if (node && node->isOfType(mask)) {
            activationRange = node->GetActivationRange();

            if (activationRange > 0) {
                if (!ent->WithinDistance(node->origin, activationRange)) {
                    continue;
                }
            } else {
                if (!ent->WithinDistance(node->origin, maxDist)) {
                    continue;
                }
            }

            if (node == self.ignoreHelperNode.node) {
                continue;
            }

            if (node->isReserved()) {
                continue;
            }

            path = find.FindPath(ent->origin, node->origin);

            if (path) {
                pathLen = path->Length();
            }

            delete path;
            path = NULL;

            if (pathLen < bestDist) {
                bestDist = pathLen;
                bestNode = node;
            }
        }
    }

    if (self.currentHelperNode.node) {
        self.currentHelperNode.node->UnreserveNode();
    }

    self.currentHelperNode.node = bestNode;
    self.currentHelperNode.mask = mask;

    if (bestNode) {
        bestNode->ReserveNode();
    }

    return bestNode;
}

HelperNode *HelperNode::FindHelperNodeClosestToWithoutPathing(Actor& self, Entity *ent, int mask, float maxDist)
{
    HelperNode *node;
    HelperNode *bestNode;
    float       pathLen;
    float       bestDist;
    float       activationRange;
    int         nodeID;

    if (!ent) {
        return NULL;
    }

    bestNode = NULL;
    bestDist = 999999999.8f;
    pathLen  = bestDist - 1.0f;

    for (int i = 1; i <= HelperNodes.NumObjects(); i++) {
        node   = NULL;
        node   = HelperNodes.ObjectAt(i);
        nodeID = node->GetID();

        if (nodeID != -1 && nodeID != self.currentHelperNode.nodeID) {
            continue;
        }

        if (node && node->isOfType(mask)) {
            activationRange = node->GetActivationRange();

            if (activationRange > 0) {
                if (!ent->WithinDistance(node->origin, activationRange)) {
                    continue;
                }
            } else {
                if (!ent->WithinDistance(node->origin, maxDist)) {
                    continue;
                }
            }

            if (node == self.ignoreHelperNode.node) {
                continue;
            }

            if (node->isReserved()) {
                continue;
            }

            Vector pathToNode = ent->origin - node->origin;
            pathLen           = pathToNode.length();

            if (pathLen < bestDist) {
                bestDist = pathLen;
                bestNode = node;
            }
        }
    }

    if (self.currentHelperNode.node) {
        self.currentHelperNode.node->UnreserveNode();
    }

    self.currentHelperNode.node = bestNode;
    self.currentHelperNode.mask = mask;

    if (bestNode) {
        bestNode->ReserveNode();
    }

    return bestNode;
}

bool HelperNode::isHelperNodeInRange(Actor& self, int mask, float range)
{
    HelperNode *node;

    node = FindHelperNodeClosestTo(self, &self, mask, range);
    if (node) {
        return true;
    }

    return false;
}
