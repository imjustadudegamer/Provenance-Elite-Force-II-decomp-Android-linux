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

#pragma once

#include "../corepp/class.h"
#include "../corepp/container.h"
#include "../corepp/vector.h"
#include "../corepp/listener.h"

class Entity;
class Damage;
class DamageModifier;
class DamageModificationSystem;

#if defined(UNDEFINED) || defined(TIKI_NAME) || defined(NAME) || defined(GROUP) \
    || defined(ACTOR_TYPE) || defined(TARGETNAME) || defined(DAMAGE_TYPE)
#    error "DamageModifierType token is macro-shadowed (fgame/match.h?) -- include order must be fixed"
#endif

typedef enum {
    UNDEFINED = 0,
    TIKI_NAME,
    NAME,
    GROUP,
    ACTOR_TYPE,
    TARGETNAME,
    DAMAGE_TYPE 
} DamageModifierType;

static_assert(UNDEFINED == 0 && TIKI_NAME == 1 && NAME == 2 && GROUP == 3
                  && ACTOR_TYPE == 4 && TARGETNAME == 5 && DAMAGE_TYPE == 6,
              "DamageModifierType ordinals are savegame file format");

class Damage : public Class
{
public:
    float   damage;
    Entity *inflictor;
    Entity *attacker;
    Vector  position;
    Vector  direction;
    Vector  normal;
    int     knockback;
    int     dflags;
    int     meansofdeath;
    int     surfaceNumber;
    int     boneNumber;
    bool    showPain;
    Entity *weapon;

    Damage();
    Damage(Event *ev);
    virtual ~Damage() override;
};

class DamageModifier : public Class
{
    friend class DamageModificationSystem;

private:
    float _multiplier;
    float _chance;
    float _painBaseLine;

protected:
    DamageModifierType _type;

    virtual void resolveDamage(Damage& damage) = 0;
    virtual void resolvePain(Damage& damage)   = 0;

public:
    DamageModifier()
        : _multiplier(0.0f)
        , _chance(0.0f)
        , _painBaseLine(0.0f)
        , _type(UNDEFINED)
    {}

    DamageModifier(DamageModifierType type, float multiplier, float chance, float painBaseLine)
        : _multiplier(multiplier)
        , _chance(chance)
        , _painBaseLine(painBaseLine)
        , _type(type)
    {}

    virtual ~DamageModifier() {}

    DamageModifierType getType() { return _type; }

    float getMultiplier() { return _multiplier; }

    float getPainBaseLine() { return _painBaseLine; }

    float getChance() { return _chance; }

    void Archive(Archiver& arc) override;
};

class DamageModifierTikiName : public DamageModifier
{
private:
    str _tikiName;

protected:
    void resolveDamage(Damage& damage) override;
    void resolvePain(Damage& damage) override;

public:
    DamageModifierTikiName() { _type = TIKI_NAME; }

    DamageModifierTikiName(
        DamageModifierType type, const str& tikiname, float multiplier, float chance, float painBaseLine
    )
        : DamageModifier(type, multiplier, chance, painBaseLine)
        , _tikiName(tikiname)
    {}

    virtual ~DamageModifierTikiName() {}

    void Archive(Archiver& arc) override;
};

class DamageModifierName : public DamageModifier
{
private:
    str _name;

protected:
    void resolveDamage(Damage& damage) override;
    void resolvePain(Damage& damage) override;

public:
    DamageModifierName() { _type = NAME; }

    DamageModifierName(DamageModifierType type, const str& name, float multiplier, float chance, float painBaseLine)
        : DamageModifier(type, multiplier, chance, painBaseLine)
        , _name(name)
    {}

    virtual ~DamageModifierName() {}

    void Archive(Archiver& arc) override;
};

class DamageModifierGroup : public DamageModifier
{
private:
    int _group;

protected:
    void resolveDamage(Damage& damage) override;
    void resolvePain(Damage& damage) override;

public:
    DamageModifierGroup()
        : _group(0)
    {
        _type = GROUP;
    }

    DamageModifierGroup(DamageModifierType type, int group, float multiplier, float chance, float painBaseLine)
        : DamageModifier(type, multiplier, chance, painBaseLine)
        , _group(group)
    {}

    virtual ~DamageModifierGroup() {}

    void Archive(Archiver& arc) override;
};

class DamageModifierActorType : public DamageModifier
{
private:
    int _actortype;

protected:
    void resolveDamage(Damage& damage) override;
    void resolvePain(Damage& damage) override;

public:
    DamageModifierActorType()
        : _actortype(0)
    {
        _type = ACTOR_TYPE;
    }

    DamageModifierActorType(DamageModifierType type, int actortype, float multiplier, float chance, float painBaseLine)
        : DamageModifier(type, multiplier, chance, painBaseLine)
        , _actortype(actortype)
    {}

    virtual ~DamageModifierActorType() {}

    void Archive(Archiver& arc) override;
};

class DamageModifierTargetName : public DamageModifier
{
private:
    str _targetname;

protected:
    void resolveDamage(Damage& damage) override;
    void resolvePain(Damage& damage) override;

public:
    DamageModifierTargetName() { _type = TARGETNAME; }

    DamageModifierTargetName(
        DamageModifierType type, const str& targetname, float multiplier, float chance, float painBaseLine
    )
        : DamageModifier(type, multiplier, chance, painBaseLine)
        , _targetname(targetname)
    {}

    virtual ~DamageModifierTargetName() {}

    void Archive(Archiver& arc) override;
};

class DamageModifierDamageType : public DamageModifier
{
private:
    int _damagetype;

protected:
    void resolveDamage(Damage& damage) override;
    void resolvePain(Damage& damage) override;

public:
    DamageModifierDamageType()
        : _damagetype(0)
    {
        _type = DAMAGE_TYPE;
    }

    DamageModifierDamageType(DamageModifierType type, int damagetype, float multiplier, float chance, float painBaseLine)
        : DamageModifier(type, multiplier, chance, painBaseLine)
        , _damagetype(damagetype)
    {}

    virtual ~DamageModifierDamageType() {}

    void Archive(Archiver& arc) override;
};

class DamageModificationSystem : public Class
{
private:
    Container<DamageModifier *> damageModifiers;

public:
    DamageModificationSystem();
    virtual ~DamageModificationSystem() override;

    void addDamageModifier(
        const str& damagemodtype, const str& value, float multiplier, float chance, float painBaseLine
    );
    void addDamageModifier(DamageModifier *newModifier);
    void resolveDamage(Damage &damage);

    Container<DamageModifier *>& getModifierList() { return damageModifiers; }
};
