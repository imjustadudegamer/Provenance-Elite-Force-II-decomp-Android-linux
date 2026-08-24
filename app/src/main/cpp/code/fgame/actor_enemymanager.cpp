//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/actor_enemymanager.cpp                     $
// $Revision:: 41                                                             $
//   $Author:: Singlis                                                        $
//     $Date:: 9/26/03 2:35p                                                  $
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
#include "actor_enemymanager.h"
#include "worldspawn.h"
#include "player.h"
#include "groupcoordinator.h"
#include "rageai.h"
#include "actor_sensoryperception.h"
#include "navigate.h"
#include "actor_combatsubsystem.h"

EnemyManager::EnemyManager()
{
    gi.Error(ERR_FATAL, "EnemyManager::EnemyManager -- Default Constructor Called");
    act                   = NULL;
    _lockedOnCurrentEnemy = false;
    _currentEnemyHate     = 0.0f;
}

EnemyManager::EnemyManager(Actor *actor)
{
    if (actor) {
        act = actor;
    } else {
        gi.Error(ERR_DROP, "EnemyManager::EnemyManager -- actor is NULL");
        act = NULL;
    }

    _currentEnemy         = NULL;
    _lastEnemy            = NULL;
    _alternateTarget      = NULL;
    _lockedOnCurrentEnemy = false;
    _currentEnemyHate     = 0.0f;
}

EnemyManager::~EnemyManager() {}

void EnemyManager::ClearCurrentEnemy()
{
    _lockedOnCurrentEnemy = false;
    _currentEnemy         = NULL;
}

void EnemyManager::ClearHateList()
{
    for (int i = _hateList.NumObjects(); i > 0; i--) {
        _hateList.RemoveObjectAt(i);
    }

    _hateList.FreeObjectList();
}

// PORT GAP (deliberate): retail falls back to `FindHighestHateEnemy()` when
EntityPtr EnemyManager::GetCurrentEnemy()
{
    if (act && act->forcedEnemy) {
        if (act->forcedEnemy->health <= 0) {
            act->forcedEnemy      = NULL;
            _lockedOnCurrentEnemy = false;
            return _currentEnemy;
        } else {
            return act->forcedEnemy;
        }
    }

    if (!_currentEnemy) {
        FindHighestHateEnemy();
    }

    return _currentEnemy;
}

void EnemyManager::SetCurrentEnemy(Entity *enemy)
{
    if (enemy) {
        _currentEnemy = enemy;
    } else {
        _lockedOnCurrentEnemy = false;
    }
}

EntityPtr EnemyManager::GetAlternateTarget()
{
    return _alternateTarget;
}

void EnemyManager::SetAlternateTarget(Entity *target)
{
    _alternateTarget = target;
}

// PORT GAP (deliberate): retail's guard is
void EnemyManager::TryToAddToHateList(Entity *enemy)
{
    if (!enemy || !act) {
        return;
    }

    if (!IsValidEnemy(enemy)) {
        return;
    }

    if (!IsInHateList(enemy)) {
        // SDK actor_enemymanager.cpp (retail @0x00070B40): the target must be reachable
        if (!act->combatSubsystem->CanAttackTarget(enemy) && !CanGetToEntity(enemy)) {
            return;
        }
        _AddToHateList(enemy);
    }
}

void EnemyManager::_AddToHateList(Entity *enemy)
{
    HateListEntry_t listIndex;

    if (!enemy) {
        return;
    }

    listIndex.enemy         = enemy;
    listIndex.damageCaused  = 0;
    listIndex.hate          = EF2_DEFAULT_INITIAL_HATE;
    listIndex.lastSightTime = level.time;
    listIndex.canSee        = false;
    listIndex.lastDistance  = 0.0f;
    listIndex.nextSightTime = level.time + EF2_MIN_SIGHT_DELAY;
    UpdateDistance(&listIndex);
    _hateList.AddObject(listIndex);
}

int EnemyManager::_findEntityInHateList(Entity *searchEnt)
{
    if (!searchEnt) {
        return 0;
    }

    for (int i = 1; i <= _hateList.NumObjects(); i++) {
        if (_hateList.ObjectAt(i).enemy == searchEnt) {
            return i;
        }
    }

    return 0;
}

qboolean EnemyManager::IsInHateList(Entity *enemy)
{
    return _findEntityInHateList(enemy) > 0;
}

void EnemyManager::AdjustHate(Entity *enemy, float adjustment)
{
    HateListEntry_t *listIndex;

    if (!enemy) {
        return;
    }

    for (int i = 1; i <= _hateList.NumObjects(); i++) {
        listIndex = &_hateList.ObjectAt(i);

        if (listIndex->enemy == enemy) {
            listIndex->hate += adjustment;
            if (listIndex->hate < 0.0f) {
                listIndex->hate = 0.0f;
            }

            return;
        }
    }
}

// mutate.  Do not "fix" this to a reference: the divergence is retail's.
void EnemyManager::AdjustDamageCaused(Entity *enemy, float adjustment)
{
    HateListEntry_t listIndex;

    if (!enemy) {
        return;
    }

    for (int i = 1; i <= _hateList.NumObjects(); i++) {
        listIndex = _hateList.ObjectAt(i);

        if (listIndex.enemy == enemy) {
            listIndex.damageCaused += adjustment;
            return;
        }
    }
}

void EnemyManager::TrivialUpdate()
{
    for (int i = _hateList.NumObjects(); i > 0; i--) {
        _hateList.ObjectAt(i).lastDistance = 0.0f;
    }
}

void EnemyManager::Update()
{
    HateListEntry_t *listIndex;

    if (!act) {
        return;
    }

    for (int i = _hateList.NumObjects(); i > 0; i--) {
        listIndex = &_hateList.ObjectAt(i);

        if (!act->IsEntityAlive(listIndex->enemy)) {
            if (listIndex->enemy == _currentEnemy) {
                ClearCurrentEnemy();
            }

            _hateList.RemoveObjectAt(i);
            continue;
        }

        UpdateDistance(listIndex);
        UpdateCanSee(listIndex);

        if (listIndex->enemy && listIndex->enemy->isSubclassOf(Player)) {
            if (act->GetActorFlag(ACTOR_FLAG_UPDATE_ACTION_LEVEL)) {
                Player *player = (Player *)(Entity *)listIndex->enemy;
                player->IncreaseActionLevel(100);
            }
        }

        if (act->GetActorFlag(ACTOR_FLAG_UPDATE_HATE_WITH_ATTACKERS)) {
            UpdateAttackers(listIndex);
        }


        if ((act->timeBetweenSleepChecks > 0.0f)
            && (listIndex->lastSightTime + act->timeBetweenSleepChecks <= level.time
                && !gi.inPVS(listIndex->enemy->centroid, act->centroid))) {
            if (listIndex->enemy == _currentEnemy && !_lockedOnCurrentEnemy) {
                ClearCurrentEnemy();
            }

            _hateList.RemoveObjectAt(i);
        }
    }

    TrySleep();

    if (act->sensoryPerception) {
        act->sensoryPerception->SenseEnemies();
    }

    if (GetCurrentEnemy() || act->GetActorFlag(ACTOR_FLAG_INVESTIGATING) || act->mode == ACTOR_MODE_SCRIPT
        || act->mode == ACTOR_MODE_TALK) {
        act->last_time_active = level.time;
    }

    act->strategos->Evaluate();
}


void EnemyManager::TrySleep(void)
{
    const Entity *currentEnemy = GetCurrentEnemy();

    if (!level.ai_on) {
        act->last_time_active = level.time;
    }

    if (!currentEnemy && (act->max_inactive_time > 0.0f)
        && ((act->last_time_active + act->max_inactive_time < level.time))) {
        Player *player = 0;
        for (int i = 0; i < game.maxclients; i++) {
            player = GetPlayer(i);

            if (player) {
                if (((act->mode == ACTOR_MODE_IDLE) && !(player->flags & FL_NOTARGET)
                        && gi.inPVS(player->centroid, act->centroid))
                    || act->sensoryPerception->WithinVisionDistance(player)
                    || gi.inPVS(player->centroid, act->centroid)) {
                    act->last_time_active = level.time;
                }

                else {
                    if (act->mode == ACTOR_MODE_AI) {
                        act->EndMode();

                        act->Sleep();

                        if (act->idle_thread.length() > 1) {
                            ExecuteThread(act->idle_thread, false);
                        }
                    }

                    else {
                        act->Sleep();
                    }
                }
            }
        }
    }
}

void EnemyManager::UpdateDistance(HateListEntry_t *listIndex)
{
    Vector distance;

    if (!listIndex || !listIndex->enemy || !act) {
        return;
    }

    distance = listIndex->enemy->origin - act->origin;

    listIndex->lastDistance = distance.length();
}

void EnemyManager::UpdateAttackers(HateListEntry_t *listIndex)
{
    int         numberOfAttackers;
    ActorGroup *group;

    if (!listIndex || !act || !groupcoordinator) {
        return;
    }

    if (_currentEnemy == listIndex->enemy) {
        return;
    }

    group = (ActorGroup *)groupcoordinator->GetGroup(act->GetGroupID());

    if (!group) {
        return;
    }

    numberOfAttackers = group->CountMembersAttackingEnemy(listIndex->enemy);

    listIndex->hate = listIndex->hate / (numberOfAttackers + 1);
}

void EnemyManager::UpdateCanSee(HateListEntry_t *listIndex)
{
    if (!act || !listIndex || (listIndex->nextSightTime > level.time)) {
        return;
    }

    qboolean canSee;
    canSee = act->sensoryPerception->CanSeeEntity(act, listIndex->enemy, qtrue, qtrue);  

    if (canSee != listIndex->canSee) {
        if (act->strategos) {
            act->strategos->NotifySightStatusChanged(listIndex->enemy, canSee);
        }
    }

    listIndex->canSee = canSee;

    float next_sight_delay;
    if (listIndex->enemy == _currentEnemy) {
        next_sight_delay = act->max_inactive_time / 5.0f + G_CRandom(0.5f);
    } else {
        next_sight_delay = EF2_MIN_SIGHT_DELAY + (act->max_inactive_time / 2.0f + G_CRandom(0.5f));
    }

    if (next_sight_delay < 1.0f) {
        next_sight_delay = 1.0f;
    }

    listIndex->nextSightTime = level.time + next_sight_delay;

    if (canSee) {
        listIndex->lastSightTime = level.time;
    }

    if (listIndex->lastSightTime + act->max_inactive_time < level.time) {
        ClearCurrentEnemy();
    }
}

void EnemyManager::LockOnCurrentEnemy(qboolean lock)
{
    _lockedOnCurrentEnemy = lock;
}

qboolean EnemyManager::IsLockedOnCurrentEnemy()
{
    return _lockedOnCurrentEnemy;
}

bool EnemyManager::HasEnemy()
{
    if (_currentEnemy) {
        return true;
    }

    return false;
}

float EnemyManager::getEnemyCount()
{
    return (float)_hateList.NumObjects();
}

qboolean EnemyManager::IsValidEnemy(Entity *enemy)
{
    if (!enemy || enemy == (Entity *)world || enemy == (Entity *)act
        || (enemy->flags & FL_NOTARGET) || enemy->takedamage == DAMAGE_NO
        || enemy->deadflag) {
        return false;
    }

    if (!Hates(enemy)) {
        return false;
    }

    if (CanAttack(enemy)) {
        return (enemy->edict->s.renderfx & RF_DONTDRAW) == 0;
    }

    return false;
}

qboolean EnemyManager::CanAttack(Entity *ent)
{
    if (!ent) {
        return false;
    }

    if (act->targetType == ATTACK_ANY) {
        return true;
    }

    if (ent->isSubclassOf(Player) && act->targetType == ATTACK_PLAYER_ONLY) {
        return true;
    }

    if (ent->isSubclassOf(Actor) && act->targetType == ATTACK_ACTORS_ONLY) {
        return true;
    }

    if (ent->isSubclassOf(LevelInteractionTrigger)
        && act->targetType == ATTACK_LEVEL_INTERACTION) {
        return true;
    }

    return false;
}

qboolean EnemyManager::CanAttackAnyEnemy()
{
    for (int i = 1; i <= _hateList.NumObjects(); i++) {
        HateListEntry_t& listIndex = _hateList.ObjectAt(i);
        Entity *target = listIndex.enemy;

        if (target && act->combatSubsystem->CanAttackTarget(target)) {
            return true;
        }
    }

    return false;
}

qboolean EnemyManager::IsLastInHateList(Entity *enemy)
{
    int index = _findEntityInHateList(enemy);
    return index == _hateList.NumObjects();
}

void EnemyManager::DoArchive(Archiver& arc, Actor *actor)
{
    Archive(arc);
    if (actor) {
        act = actor;
    } else {
        gi.Error(ERR_FATAL, "EnemyManager::DoArchive -- actor is NULL");
    }
}

void EnemyManager::Archive(Archiver& arc)
{
    HateListEntry_t  hateEntry;
    HateListEntry_t *realHateEntry;
    int              numEntries;

    if (arc.Saving()) {
        numEntries = _hateList.NumObjects();
        arc.ArchiveInteger(&numEntries);

        for (int i = 1; i <= numEntries; i++) {
            hateEntry = _hateList.ObjectAt(i);

            arc.ArchiveSafePointer(&hateEntry.enemy);
            arc.ArchiveFloat(&hateEntry.lastSightTime);
            arc.ArchiveFloat(&hateEntry.nextSightTime);
            arc.ArchiveBoolean(&hateEntry.canSee);
            arc.ArchiveFloat(&hateEntry.damageCaused);
            arc.ArchiveFloat(&hateEntry.hate);
            arc.ArchiveFloat(&hateEntry.lastDistance);
        }
    } else {
        arc.ArchiveInteger(&numEntries);

        _hateList.FreeObjectList();
        _hateList.Resize(numEntries);

        for (int i = 1; i <= numEntries; i++) {
            _hateList.AddObject(hateEntry);

            realHateEntry = &_hateList.ObjectAt(i);

            arc.ArchiveSafePointer(&realHateEntry->enemy);
            arc.ArchiveFloat(&realHateEntry->lastSightTime);
            arc.ArchiveFloat(&realHateEntry->nextSightTime);
            arc.ArchiveBoolean(&realHateEntry->canSee);
            arc.ArchiveFloat(&realHateEntry->damageCaused);
            arc.ArchiveFloat(&realHateEntry->hate);
            arc.ArchiveFloat(&realHateEntry->lastDistance);
        }
    }

    arc.ArchiveSafePointer(&_currentEnemy);
    arc.ArchiveSafePointer(&_lastEnemy);

    arc.ArchiveSafePointer(&_alternateTarget);

    arc.ArchiveBoolean(&_lockedOnCurrentEnemy);
    arc.ArchiveFloat(&_currentEnemyHate);
}

void EnemyManager::FindHighestHateEnemy()
{
    float           hateValue = 0;
    HateListEntry_t listIndex;
    float           hateFactor;

    if (IsLockedOnCurrentEnemy()) {
        return;
    }

    for (int i = _hateList.NumObjects(); i > 0; i--) {
        hateFactor = 100;

        listIndex      = _hateList.ObjectAt(i);
        listIndex.hate = listIndex.hate * (1 / listIndex.lastDistance);

        if (!listIndex.enemy) {
            _hateList.RemoveObjectAt(i);
            continue;
        }

        if (listIndex.enemy.Pointer() && listIndex.enemy->isSubclassOf(Sentient)
            && !act->GetActorFlag(ACTOR_FLAG_UPDATE_HATE_WITH_ATTACKERS)) {
            hateFactor *= static_cast<Sentient *>(listIndex.enemy.Pointer())->GetHateModifier();
        }

        listIndex.hate *= hateFactor;

        if (listIndex.hate >= hateValue && listIndex.hate > 0.0) {
            _currentEnemy = listIndex.enemy;
            hateValue     = listIndex.hate;
        }
    }
}

void EnemyManager::FindClosestEnemy()
{
    float distance = 99999999.9;

    for (int i = 1; i <= _hateList.NumObjects(); i++) {
        EntityPtr       enemy;
        HateListEntry_t listIndex;

        listIndex = _hateList.ObjectAt(i);

        if (!act->combatSubsystem->CanAttackTarget(listIndex.enemy)) {
            continue;
        }

        if (listIndex.lastDistance < distance) {
            _currentEnemy = listIndex.enemy;
            distance      = listIndex.lastDistance;
        }
    }
}

void EnemyManager::FindNextEnemy()
{
    HateListEntry_t listIndex;
    float           hateValue;

    hateValue = 0;
    for (int i = 1; i <= _hateList.NumObjects(); i++) {
        listIndex = _hateList.ObjectAt(i);
        if (listIndex.hate <= _currentEnemyHate && listIndex.hate > hateValue) {
            hateValue          = listIndex.hate;
            _lastEnemy         = _currentEnemy;
            _currentEnemy      = listIndex.enemy;
            _currentEnemyHate  = hateValue;
        }
    }
}

qboolean EnemyManager::Likes(Entity *ent)
{
    Actor *actor;

    if (!ent) {
        return false;
    }

    if (ent->isClient()) {
        return (act->actortype == IS_FRIEND);
    }

    if (act->actortype == IS_MONSTER) {
        return false;
    }

    if (ent->isSubclassOf(Actor)) {
        actor = (Actor *)ent;

        if (actor->enemytype == act->enemytype) {
            return true;
        }

        if (actor->actortype != act->actortype) {
            return false;
        } else if (act->actortype == IS_FRIEND) {
            return true;
        }
    }

    return false;
}

qboolean EnemyManager::Hates(Entity *ent)
{
    Actor *actor;

    if (!ent) {
        return false;
    }

    if (ent->isClient()) {
        return (act->actortype != IS_CIVILIAN)
            && ((act->actortype != IS_FRIEND) && (act->actortype != IS_TEAMMATE));
    } else if (ent->isSubclassOf(Actor) && (act->actortype != IS_INANIMATE)) {
        actor = (Actor *)ent;
        if ((actor->actortype <= IS_ENEMY) && (act->actortype <= IS_ENEMY)) {
            return false;
        }
        if ((actor->actortype == IS_FRIEND) && (act->actortype <= IS_ENEMY)) {
            return true;
        }
        if ((actor->actortype == IS_TEAMMATE) && (act->actortype <= IS_ENEMY)) {
            return true;
        }
        if ((actor->actortype <= IS_ENEMY) && (act->actortype == IS_FRIEND)) {
            return true;
        }
        if ((actor->actortype <= IS_ENEMY) && (act->actortype == IS_TEAMMATE)) {
            return true;
        }
        if ((actor->actortype == IS_TEAMMATE) && (act->actortype == IS_TEAMMATE)) {
            return false;
        }
    }

    return false;
}

Vector EnemyManager::GetAwayFromEnemies()
{
    EntityPtr       enemy;
    HateListEntry_t listIndex;

    Vector enemyToSelf;
    Vector temp;
    float  mag;

    Vector bestAwayVector;

    for (int i = 1; i <= _hateList.NumObjects(); i++) {
        listIndex = _hateList.ObjectAt(i);
        enemy     = listIndex.enemy;

        enemyToSelf = act->origin - enemy->origin;
        mag         = enemyToSelf.lengthSquared();

        temp = enemyToSelf * (1 / mag);

        bestAwayVector = bestAwayVector + temp;
    }

    bestAwayVector.normalize();

    return bestAwayVector;
}

qboolean EnemyManager::InEnemyLineOfFire()
{
    Actor  *enemyActor  = NULL;
    Player *enemyPlayer = NULL;

    Vector enemyToSelf;

    float angleDiff = 0.0f;

    if (!_currentEnemy) {
        return false;
    }

    enemyToSelf = act->origin - _currentEnemy->origin;
    enemyToSelf = enemyToSelf.toAngles();

    if (_currentEnemy->isSubclassOf(Actor)) {
        enemyActor = (Actor *)(Entity *)_currentEnemy;

        if (!enemyActor) {
            return false;
        }

        angleDiff = abs((int)AngleNormalize180(enemyActor->angles[YAW] - enemyToSelf[YAW]));
    }

    if (_currentEnemy->isSubclassOf(Player)) {
        enemyPlayer = (Player *)(Entity *)_currentEnemy;

        if (!enemyPlayer) {
            return false;
        }

        Vector pAngles = enemyPlayer->GetVAngles();

        angleDiff = abs((int)AngleNormalize180(pAngles[YAW] - enemyToSelf[YAW]));
    }

    if (angleDiff < 10.0f) {
        return true;
    }

    return false;
}

float EnemyManager::GetDistanceFromEnemy()
{
    Vector selfToEnemy;

    if (!_currentEnemy) {
        return -1;
    }

    selfToEnemy = _currentEnemy->origin - act->origin;

    return selfToEnemy.length();
}

bool EnemyManager::IsAnyEnemyInRange(float range)
{
    HateListEntry_t *listIndex;

    for (int i = _hateList.NumObjects(); i > 0; i--) {
        listIndex = &_hateList.ObjectAt(i);

        if (!act->IsEntityAlive(listIndex->enemy)) {
            if (listIndex->enemy == _currentEnemy) {
                ClearCurrentEnemy();
            }

            _hateList.RemoveObjectAt(i);
            continue;
        }

        if (act->WithinDistance(listIndex->enemy, range)) {
            return true;
        }
    }

    return false;
}

bool EnemyManager::CanGetToEntity(Entity *ent)
{
    if (!ent) {
        return false;
    }

    StandardMovePath find;
    Path            *path;
    bool             success;

    success = false;
    find.heuristic.setSize(act->size);
    find.heuristic.entnum = act->entnum;

    path = find.FindPath(act->origin, ent->origin);

    if (path) {
        success = true;
    }

    delete path;
    path = NULL;

    return success;
}
