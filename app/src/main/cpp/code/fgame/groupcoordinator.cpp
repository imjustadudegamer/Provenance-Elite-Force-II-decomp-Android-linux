
#include "groupcoordinator.h"
#include "actor.h"
#include "gamescript.h"
#include "scriptmaster.h"

CLASS_DECLARATION(Listener, Group, "group") {
    {NULL, NULL}
};

Group::Group()
{
    _id = 0;
}

Group::~Group()
{
    ClearList();
}

void Group::AddMember(Entity *entity)
{
    Entity *checkEntity;

    for (int i = 1; i <= _memberList.NumObjects(); i++) {
        checkEntity = _memberList.ObjectAt(i);

        if (!checkEntity) {
            continue;
        }

        if (checkEntity == entity) {
            return;
        }
    }

    _memberList.AddObject(entity);
    entity->SetGroupID(_id);
}

void Group::RemoveMember(Entity *entity)
{
    if (_memberList.ObjectInList(entity)) {
        _memberList.RemoveObject(entity);
    }
}

void Group::ClearList()
{
    _memberList.FreeObjectList();
}

bool Group::IsThisTargetNameInGroup(const str& name)
{
    Entity *checkEntity;

    for (int i = 1; i <= _memberList.NumObjects(); i++) {
        checkEntity = _memberList.ObjectAt(i);

        if (!checkEntity) {
            continue;
        }

        if (!Q_stricmp(name.c_str(), checkEntity->TargetName())) {
            return true;
        }
    }

    return false;
}

bool Group::IsThisEntityInGroup(Entity *entity)
{
    Entity *checkEntity;

    for (int i = 1; i <= _memberList.NumObjects(); i++) {
        checkEntity = _memberList.ObjectAt(i);

        if (!checkEntity) {
            continue;
        }

        if (checkEntity == entity) {
            return true;
        }
    }

    return false;
}

void Group::SendEventToGroup(Event *ev)
{
    Entity *entity;
    Event  *copiedEvent;

    for (int i = 1; i <= _memberList.NumObjects(); i++) {
        entity = _memberList.ObjectAt(i);

        if (!entity) {
            continue;
        }

        copiedEvent = new Event(*ev);
        entity->ProcessEvent(copiedEvent);
    }
}

void Group::SendEventToGroup(Event& ev)
{
    Entity *entity;
    Event  *copiedEvent;

    for (int i = 1; i <= _memberList.NumObjects(); i++) {
        entity = _memberList.ObjectAt(i);

        if (!entity) {
            continue;
        }

        copiedEvent = new Event(ev);
        entity->ProcessEvent(copiedEvent);
    }
}

void Group::GroupCancelEventsOfType(Event *ev)
{
    Entity *entity;

    for (int i = 1; i <= _memberList.NumObjects(); i++) {
        entity = _memberList.ObjectAt(i);

        if (!entity) {
            continue;
        }

        entity->CancelEventsOfType(ev);
    }
}

void Group::Archive(Archiver& arc)
{
    int        num, i;
    EntityPtr  ent;
    EntityPtr *entityPointer;


    ent = NULL;

    if (arc.Saving()) {
        num = _memberList.NumObjects();
        arc.ArchiveInteger(&num);

        for (i = 1; i <= num; i++) {
            ent = _memberList.ObjectAt(i);
            arc.ArchiveSafePointer(&ent);
        }
    } else {
        arc.ArchiveInteger(&num);

        _memberList.ClearObjectList();
        _memberList.Resize(num);

        for (i = 1; i <= num; i++) {
            _memberList.AddObject(ent);

            entityPointer = &_memberList.ObjectAt(i);

            arc.ArchiveSafePointer(entityPointer);
        }
    }

    arc.ArchiveInteger(&_id);
    arc.ArchiveString(&_groupDeathThread);
}



void Group::RunGroupDeathThread(Entity *entity)
{
    if (!_groupDeathThread.length()) {
        return;
    }

    // added a port-only `gi.DPrintf` on the failure path, i.e. an extra console line retail
    ExecuteThread(_groupDeathThread, true, entity);
}

CLASS_DECLARATION(Group, ActorGroup, "actorgroup") {
    {NULL, NULL}
};

ActorGroup::ActorGroup() {}

ActorGroup::~ActorGroup() {}

int ActorGroup::CountMembersWithThisName(const str& name)
{
    Entity *entity = 0;
    Actor  *actor  = 0;
    int     count  = 0;

    entity = GetNextMember(entity);

    if (!entity) {
        return 0;
    }

    if (!entity->isSubclassOf(Actor)) {
        return 0;
    }

    while (entity != NULL) {
        if (entity->isSubclassOf(Actor)) {
            actor = (Actor *)entity;

            if (!Q_stricmp(name.c_str(), actor->name.c_str())) {
                count++;
            }
        }

        entity = GetNextMember(entity);
    }

    return count;
}

int ActorGroup::CountMembersAttackingEnemy(Entity *enemy)
{
    Entity *entity = 0;
    Actor  *actor  = 0;
    int     count  = 0;

    entity = GetNextMember(entity);

    if (!entity) {
        return 0;
    }

    if (!entity->isSubclassOf(Actor)) {
        return 0;
    }

    while (entity != NULL) {
        if (entity->isSubclassOf(Actor)) {
            actor = (Actor *)entity;

            if (!actor->GetActorFlag(ACTOR_FLAG_ATTACKING_ENEMY)) {
                entity = GetNextMember(entity);
                continue;
            }

            if (!enemy) {
                count++;
            } else if (actor->enemyManager->GetCurrentEnemy() == enemy) {
                count++;
            }
        }

        entity = GetNextMember(entity);
    }

    return count;
}

CLASS_DECLARATION(Listener, GroupCoordinator, "groupcoordinator") {
    {NULL, NULL}
};

GroupCoordinatorPtr groupcoordinator;

GroupCoordinator::GroupCoordinator() {}

GroupCoordinator::~GroupCoordinator()
{
    ClearGroupList();
}

Group *GroupCoordinator::GetGroup(int ID)
{
    Group *group;
    group = NULL;

    for (int i = 1; i <= _groupList.NumObjects(); i++) {
        group = _groupList.ObjectAt(i);

        if (!group) {
            continue;
        }

        if (group->GetGroupID() == ID) {
            return group;
        }
    }

    return NULL;
}

void GroupCoordinator::MemberDied(Entity *entity, int ID)
{
    Group *group;
    int    i;

    for (i = 1; i <= _groupList.NumObjects(); i++) {
        group = _groupList.ObjectAt(i);

        if (!group) {
            continue;
        }

        if (group->GetGroupID() == ID) {
            if (group->CountMembers() == 1) {
                group->RunGroupDeathThread(entity);
                break;
            }
        }
    }

    RemoveEntityFromGroup(entity, ID);
}

void GroupCoordinator::SetGroupDeathThread(const str& threadName, int ID)
{
    Group *group;
    int    i;

    for (i = 1; i <= _groupList.NumObjects(); i++) {
        group = _groupList.ObjectAt(i);

        if (!group) {
            continue;
        }

        if (group->GetGroupID() == ID) {
            group->SetGroupDeathThread(threadName);
            return;
        }
    }
}

void GroupCoordinator::AddEntityToGroup(Entity *ent, int ID)
{
    Group *group;
    int    i;

    for (i = 1; i <= _groupList.NumObjects(); i++) {
        group = _groupList.ObjectAt(i);

        if (!group) {
            continue;
        }

        if (group->IsThisEntityInGroup(ent) && group->GetGroupID() != ID) {
            group->RemoveMember(ent);
        }
    }

    for (i = 1; i <= _groupList.NumObjects(); i++) {
        group = _groupList.ObjectAt(i);

        if (!group) {
            continue;
        }

        if (group->GetGroupID() == ID) {
            group->AddMember(ent);
            return;
        }
    }

    group = new Group;
    group->SetGroupID(ID);
    group->AddMember(ent);

    _groupList.AddObject(group);
}

void GroupCoordinator::RemoveEntityFromGroup(Entity *ent, int ID)
{
    Group *group;
    int    i;

    for (i = 1; i <= _groupList.NumObjects(); i++) {
        group = _groupList.ObjectAt(i);

        if (!group) {
            continue;
        }

        if (group->GetGroupID() == ID) {
            group->RemoveMember(ent);
            return;
        }
    }
}

void GroupCoordinator::RemoveGroup(int ID)
{
    Group *group;

    for (int i = _groupList.NumObjects(); i > 0; i--) {
        group = _groupList.ObjectAt(i);

        if (!group) {
            continue;
        }

        if (group->GetGroupID() == ID) {
            _groupList.RemoveObjectAt(i);
            delete group;
            group = NULL;
            return;
        }
    }
}

void GroupCoordinator::ClearGroupList()
{
    Group *group;

    for (int i = _groupList.NumObjects(); i > 0; i--) {
        group = _groupList.ObjectAt(i);

        if (!group) {
            continue;
        }

        RemoveGroup(group->GetGroupID());
    }
}

void GroupCoordinator::SendEventToGroup(Event *ev, int ID)
{
    Group *group;

    if (!ev) {
        return;
    }

    for (int i = 1; i <= _groupList.NumObjects(); i++) {
        group = _groupList.ObjectAt(i);

        if (!group) {
            continue;
        }

        if (group->GetGroupID() == ID) {
            group->SendEventToGroup(ev);
            break;
        }
    }

    delete ev;
    ev = NULL;
}

void GroupCoordinator::SendEventToGroup(Event& ev, int ID)
{
    Group *group;

    for (int i = 1; i <= _groupList.NumObjects(); i++) {
        group = _groupList.ObjectAt(i);

        if (!group) {
            continue;
        }

        if (group->GetGroupID() == ID) {
            group->SendEventToGroup(ev);
            return;
        }
    }
}

void GroupCoordinator::GroupCancelEventsOfType(Event *ev, int ID)
{
    Group *group;

    if (!ev) {
        return;
    }

    for (int i = 1; i <= _groupList.NumObjects(); i++) {
        group = _groupList.ObjectAt(i);

        if (!group) {
            continue;
        }

        if (group->GetGroupID() == ID) {
            group->GroupCancelEventsOfType(ev);
            break;
        }
    }

    delete ev;
}

void GroupCoordinator::Archive(Archiver& arc)
{
    int    num, i;
    Group *group;


    group = NULL;

    if (arc.Saving()) {
        num = _groupList.NumObjects();
        arc.ArchiveInteger(&num);

        for (i = 1; i <= num; i++) {
            group = _groupList.ObjectAt(i);
            arc.ArchiveObject(group);
        }
    } else {
        arc.ArchiveInteger(&num);

        _groupList.ClearObjectList();
        _groupList.Resize(num);

        for (i = 1; i <= num; i++) {
            group = new Group;

            _groupList.AddObject(group);

            arc.ArchiveObject(group);
        }
    }
}





