//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/actor_combatsubsystem.h                        $
// $Revision:: 21                                                             $
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
// Combat Related Classes
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#pragma once

#include "g_local.h"
#include "sentient.h"  

class Actor;
class Archiver;

// without adding a virtual destructor retail does not have.
class CombatSubsystem final
{
public:
    CombatSubsystem();
    CombatSubsystem(Actor *actor);
    ~CombatSubsystem();

    bool HaveWeapon();
    bool CanAttackEnemy();
    void _init();                                  
    void UseActorWeapon(const str &weaponName, weaponhand_t hand);  
    WeaponPtr GetBestAvailableWeapon(Entity *target);               
    float getModifiedPowerRating(Entity *target, Weapon *weapon);   
    float GetActiveWeaponPowerRating(Entity *target);               
    float GetAimGunYaw(const Vector &target);                       
    float GetAimGunPitch(const Vector &target);                     
    void OverrideSpread(float spreadX, float spreadY);              
    str  GetActiveWeaponName();
    str  GetActiveWeaponArchetype();
    void SetTraceInterval(float interval);  

    const str GetAnimForMyWeapon(const str& property);
    float     GetDataForMyWeapon(const str& property);

    // Retail's behaviors do not aim by turning the actor: they swing the weapon's
    void FireWeapon();
    void StopFireWeapon();
    void AimWeaponTag(const Vector& targetPos);
    void AimWeaponTag(Entity *target);
    Vector GetLeadingTargetPos(float projSpeed, Vector originalTargetPos, Entity *target);
    void ClearAim();
    void GetGunPositionData(Vector *pos, Vector *forward = NULL, Vector *right = NULL, Vector *up = NULL);

    bool shouldArcProjectile();
    float GetLowArcRange();
    bool  GetProjectileLaunchAngles(
        Vector&      launchAngles,
        const Vector& launchPoint,
        const float   initialSpeed,
        const float   gravity,
        const bool    useHighTrajectory
    ) const;


    bool CanAttackTarget(Entity *target);
    bool CanAttackTargetFrom(Entity *target, const Vector& startPos);
    bool IsTargetInWeaponRange(Entity *target);
    bool UsingWeaponNamed(const str &weaponName);  
    bool WeaponIsFireType(firetype_t fire_type);  

    virtual void Archive(Archiver& arc);
    void         DoArchive(Archiver& arc, Actor *actor);

private:
    ActiveWeapon _activeWeapon;
    float        _nextTimeTracedToTarget;
    float        _traceInterval;
    bool         _canShootTarget;
    float        _yawDiff;

    bool _traceHitTarget(Entity *target, const Vector& startPos);

    Actor *act;
};
