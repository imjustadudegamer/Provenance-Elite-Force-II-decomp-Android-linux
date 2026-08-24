//-----------------------------------------------------------------------------
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//-----------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "damagemodification.h"
#include "entity.h"
#include "actor.h"
#include "archive.h"
#include "g_utils.h"



DamageModificationSystem::DamageModificationSystem()
{
    damageModifiers.ClearObjectList();
}

DamageModificationSystem::~DamageModificationSystem()
{
    int i;

    for (i = 1; i <= damageModifiers.NumObjects(); i++) {
        DamageModifier *damMod = damageModifiers.ObjectAt(i);
        delete damMod;
    }

    damageModifiers.FreeObjectList();
}

// DIVERGENCE (deliberate): retail falls out of this if-chain with newMod still NULL for
void DamageModificationSystem::addDamageModifier(
    const str& damagemodtype, const str& value, float multiplier, float chance, float painBaseLine
)
{
    DamageModifier *newMod = NULL;

    if (damagemodtype == "tikiname") {
        newMod = new DamageModifierTikiName(TIKI_NAME, value, multiplier, chance, painBaseLine);
    } else if (damagemodtype == "name") {
        newMod = new DamageModifierName(NAME, value, multiplier, chance, painBaseLine);
    } else if (damagemodtype == "group") {
        int groupVal = atoi(value);
        newMod       = new DamageModifierGroup(GROUP, groupVal, multiplier, chance, painBaseLine);
    } else if (damagemodtype == "actortype") {
        int actortype = Actor::ActorTypeStringToInt(value);
        newMod        = new DamageModifierActorType(ACTOR_TYPE, actortype, multiplier, chance, painBaseLine);
    } else if (damagemodtype == "targetname") {
        newMod = new DamageModifierTargetName(TARGETNAME, value, multiplier, chance, painBaseLine);
    } else if (damagemodtype == "damagetype") {

        int damagetypeVal = MOD_NameToNum(value);  
        newMod = new DamageModifierDamageType(GROUP, damagetypeVal, multiplier, chance, painBaseLine);
    }

    damageModifiers.AddObject(newMod);
}

void DamageModificationSystem::addDamageModifier(DamageModifier *newModifier)
{
    if (newModifier) {
        damageModifiers.AddObject(newModifier);
    }
}

void DamageModificationSystem::resolveDamage(Damage& damage)
{
    int i;

    for (i = 1; i <= damageModifiers.NumObjects(); i++) {
        DamageModifier *damageMod = damageModifiers.ObjectAt(i);
        damageMod->resolveDamage(damage);
    }

    damage.showPain = false;
    for (i = 1; i <= damageModifiers.NumObjects(); i++) {
        DamageModifier *damageMod = damageModifiers.ObjectAt(i);
        damageMod->resolvePain(damage);
    }
}


Damage::Damage()
{
    damage        = 0.0f;
    inflictor     = NULL;
    attacker      = NULL;
    position      = vec_zero;
    direction     = vec_zero;
    normal        = vec_zero;
    knockback     = 0;
    dflags        = 0;
    meansofdeath  = -1;
    surfaceNumber = -1;
    boneNumber    = -1;
    showPain      = false;
    weapon        = NULL;
}

Damage::Damage(Event *ev)
{
    damage       = ev->GetFloat(1);
    inflictor    = ev->GetEntity(2);
    attacker     = ev->GetEntity(3);
    position     = ev->GetVector(4);
    direction    = ev->GetVector(5);
    normal       = ev->GetVector(6);
    knockback    = ev->GetInteger(7);
    dflags       = ev->GetInteger(8);
    meansofdeath = ev->GetInteger(9);

    if (ev->NumArgs() > 9) {
        surfaceNumber = ev->GetInteger(10);
    } else {
        surfaceNumber = -1;
    }

    if (ev->NumArgs() > 10) {
        boneNumber = ev->GetInteger(11);
    } else {
        boneNumber = -1;
    }

    if (ev->NumArgs() > 11) {
        weapon = ev->GetEntity(12);
    } else {
        weapon = NULL;
    }

    showPain = false;
}

Damage::~Damage() {}


void DamageModifier::Archive(Archiver& arc)
{
    Class::Archive(arc);

    arc.ArchiveFloat(&_multiplier);
    arc.ArchiveFloat(&_chance);
    arc.ArchiveFloat(&_painBaseLine);
}


void DamageModifierTikiName::Archive(Archiver& arc)
{
    DamageModifier::Archive(arc);
    arc.ArchiveString(&_tikiName);
}

void DamageModifierTikiName::resolveDamage(Damage& damage)
{
    if (damage.attacker && damage.attacker->model == _tikiName) {
        float rand = G_Random();
        if (rand < getChance()) {
            damage.damage *= getMultiplier();
        } else {
            damage.damage = 0.0f;
        }
    }
}

void DamageModifierTikiName::resolvePain(Damage& damage)
{
    if (damage.attacker && damage.attacker->model == _tikiName) {
        if (G_Random() <= (damage.damage / getPainBaseLine())) {
            damage.showPain = true;
        }
    }
}


void DamageModifierName::Archive(Archiver& arc)
{
    DamageModifier::Archive(arc);
    arc.ArchiveString(&_name);
}

void DamageModifierName::resolveDamage(Damage& damage)
{
    if (damage.attacker && damage.attacker->isSubclassOf(Actor)) {
        Actor *act = (Actor *)damage.attacker;
        if (act->name == _name) {
            float rand = G_Random();
            if (rand < getChance()) {
                damage.damage *= getMultiplier();
            } else {
                damage.damage = 0.0f;
            }
        }
    }
}

void DamageModifierName::resolvePain(Damage& damage)
{
    if (damage.attacker && damage.attacker->isSubclassOf(Actor)) {
        Actor *act = (Actor *)damage.attacker;
        if (act->name == _name) {
            if (G_Random() <= (damage.damage / getPainBaseLine())) {
                damage.showPain = true;
            }
        }
    }
}


void DamageModifierGroup::Archive(Archiver& arc)
{
    DamageModifier::Archive(arc);
    arc.ArchiveInteger(&_group);
}

void DamageModifierGroup::resolveDamage(Damage& damage)
{
    if (damage.attacker && damage.attacker->GetGroupID() == _group) {
        float rand = G_Random();
        if (rand < getChance()) {
            damage.damage *= getMultiplier();
        } else {
            damage.damage = 0.0f;
        }
    }
}

void DamageModifierGroup::resolvePain(Damage& damage)
{
    if (damage.attacker && damage.attacker->GetGroupID() == _group) {
        if (G_Random() <= (damage.damage / getPainBaseLine())) {
            damage.showPain = true;
        }
    }
}


void DamageModifierActorType::Archive(Archiver& arc)
{
    DamageModifier::Archive(arc);
    arc.ArchiveInteger(&_actortype);
}

void DamageModifierActorType::resolveDamage(Damage& damage)
{
    if (damage.attacker && damage.attacker->isSubclassOf(Actor)) {
        Actor *act = (Actor *)damage.attacker;
        if (act->actortype == _actortype) {
            float rand = G_Random();
            if (rand < getChance()) {
                damage.damage *= getMultiplier();
            } else {
                damage.damage = 0.0f;
            }
        }
    }
}

void DamageModifierActorType::resolvePain(Damage& damage)
{
    if (damage.attacker && damage.attacker->isSubclassOf(Actor)) {
        Actor *act = (Actor *)damage.attacker;
        if (act->actortype == _actortype) {
            if (G_Random() <= (damage.damage / getPainBaseLine())) {
                damage.showPain = true;
            }
        }
    }
}


void DamageModifierTargetName::Archive(Archiver& arc)
{
    DamageModifier::Archive(arc);
    arc.ArchiveString(&_targetname);
}

void DamageModifierTargetName::resolveDamage(Damage& damage)
{
    if (damage.attacker && damage.attacker->targetname == _targetname) {
        float rand = G_Random();
        if (rand < getChance()) {
            damage.damage *= getMultiplier();
        } else {
            damage.damage = 0.0f;
        }
    }
}

void DamageModifierTargetName::resolvePain(Damage& damage)
{
    if (damage.attacker && damage.attacker->targetname == _targetname) {
        if (G_Random() <= (damage.damage / getPainBaseLine())) {
            damage.showPain = true;
        }
    }
}


void DamageModifierDamageType::Archive(Archiver& arc)
{
    DamageModifier::Archive(arc);
    arc.ArchiveInteger(&_damagetype);
}

void DamageModifierDamageType::resolveDamage(Damage& damage)
{
    if (damage.meansofdeath == _damagetype) {
        float rand = G_Random();
        if (rand < getChance()) {
            damage.damage *= getMultiplier();
        } else {
            damage.damage = 0.0f;
        }
    }
}

void DamageModifierDamageType::resolvePain(Damage& damage)
{
    if (damage.meansofdeath == _damagetype) {
        if (G_Random() <= (damage.damage / getPainBaseLine())) {
            damage.showPain = true;
        }
    }
}
