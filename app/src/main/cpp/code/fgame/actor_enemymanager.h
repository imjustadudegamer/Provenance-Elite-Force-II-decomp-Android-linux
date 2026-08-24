//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/actor_enemymanager.h                           $
// $Revision:: 16                                                             $
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
// EnemyManager class for Actors -- Handles all the hatelist management
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#ifndef __ACTOR_ENEMYMANAGER_H__
#define __ACTOR_ENEMYMANAGER_H__


class Actor;
class Entity;
class Archiver;

#define EF2_MIN_SIGHT_DELAY      2
#define EF2_DEFAULT_INITIAL_HATE 100

// a virtual destructor retail does not have.  Same treatment as CombatSubsystem.
class EnemyManager final
{
public:
    EnemyManager();
    EnemyManager(Actor *actor);
    ~EnemyManager();

    void      ClearCurrentEnemy();
    void      ClearHateList();
    EntityPtr GetCurrentEnemy();
    void      SetCurrentEnemy(Entity *enemy);

    EntityPtr GetAlternateTarget();
    void      SetAlternateTarget(Entity *target);

    void     TryToAddToHateList(Entity *enemy);
    qboolean IsInHateList(Entity *enemy);

    void AdjustHate(Entity *enemy, float adjustment);
    void AdjustDamageCaused(Entity *enemy, float adjustment);

    void TrivialUpdate();

    void Update();
    void UpdateDistance(HateListEntry_t *listIndex);
    void UpdateAttackers(HateListEntry_t *listIndex);
    void UpdateCanSee(HateListEntry_t *listIndex);

    void     LockOnCurrentEnemy(qboolean lock);
    qboolean IsLockedOnCurrentEnemy();

    bool  HasEnemy();
    float getEnemyCount();

    qboolean IsValidEnemy(Entity *enemy);
    qboolean CanAttack(Entity *ent);
    qboolean CanAttackAnyEnemy();
    qboolean IsLastInHateList(Entity *enemy);

    void     FindHighestHateEnemy();
    void     FindClosestEnemy();
    void     FindNextEnemy();
    qboolean Likes(Entity *ent);
    qboolean Hates(Entity *ent);
    Vector   GetAwayFromEnemies();
    qboolean InEnemyLineOfFire();
    float    GetDistanceFromEnemy();
    void     TrySleep();
    bool     IsAnyEnemyInRange(float range);
    bool     CanGetToEntity(Entity *ent);

    virtual void Archive(Archiver& arc);
    void         DoArchive(Archiver& arc, Actor *actor);

protected:
    void _AddToHateList(Entity *enemy);
    int  _findEntityInHateList(Entity *searchEnt);

private:
    Container<HateListEntry_t> _hateList;
    EntityPtr                  _currentEnemy;
    EntityPtr                  _lastEnemy;
    EntityPtr                  _alternateTarget;
    qboolean                   _lockedOnCurrentEnemy;
    float                      _currentEnemyHate;

    Actor *act;
};

#endif 
