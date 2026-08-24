//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/powerups.h                                    $
// $Revision:: 30                                                             $
//   $Author:: Steven                                                         $
//     $Date:: 5/16/03 8:41p                                                  $
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

#include "g_local.h"
#include "item.h"

class Powerup;
class PowerupBase;
class Rune;
class HoldableItem;

class PowerupBase : public Item
{
protected:
    SentientPtr _owner;
    str         _modelName;

    str   _modelToAttachOnUse;
    str   _modelToAttachOnUseTag;
    float _modelToAttachOnUseRemoveTime;
    str   _modelToSpawn;
    str   _shaderToDisplayOnUse;

    void init(const str& modelname, Sentient *ownerSent);

public:
    CLASS_PROTOTYPE(PowerupBase);

    PowerupBase();
    virtual ~PowerupBase() {}

    virtual void amountEvent(Event *ev);

    virtual void givePlayerItem(Player *player) {}

    virtual void update(float frameTime) { specificUpdate(frameTime); }
    virtual void specificUpdate(float frameTime) { };
    virtual float getDamageDone(float damage, int meansOfDeath) {  return damage; };

    Item *ItemPickup(Entity *other, qboolean add_to_inventory = qtrue, qboolean checkautopickup = qtrue) override;


    void setModelToAttachOnUse(Event *ev);
    void setModelToSpawnOnUse(Event *ev);
    void setShaderToDisplayOnUse(Event *ev);

    void getModelToAttachOnUse(str& modelName, str& tagName, float& modelRemoveTime);
    void getShaderToDisplayOnUse(str& modelName);

    virtual bool canDrop(void) { return true; }
    virtual void spawn(const Vector &spawnOrigin) {}  

    virtual bool canOwnerRegenerate(void) { return true; }
    virtual float getMoveMultiplier(void) { return 1.0f; }  

    virtual meansOfDeath_t changetMeansOfDeath(meansOfDeath_t meansOfDeath) { return meansOfDeath; }

    virtual float getDamageTaken(Entity *attacker, float damage, int meansOfDeath) { return damage; }

    void Archive(Archiver& arc) override;






public:
    float RespawnTime( void ) override; 
};

inline void PowerupBase::Archive(Archiver& arc)
{
    Item::Archive(arc);

    arc.ArchiveSafePointer(&_owner);
    arc.ArchiveString(&_modelName);
    arc.ArchiveString(&_modelToAttachOnUse);
    arc.ArchiveString(&_modelToAttachOnUseTag);
    arc.ArchiveFloat(&_modelToAttachOnUseRemoveTime);
    arc.ArchiveString(&_shaderToDisplayOnUse);
    // NOTE: SDK PowerupBase::Archive (powerups.h:85-95) does NOT archive
}

class Powerup : public PowerupBase
{
protected:
    float _timeLeft;

public:
    CLASS_PROTOTYPE(Powerup);

    Powerup();
    virtual ~Powerup() {}

    void amountEvent(Event *ev) override;

    float getTimeLeft(void) const { return _timeLeft; }

    void setTimeLeft(float timeLeft) { _timeLeft = timeLeft; }

    virtual bool canStack(void) { return true; }

    void update(float frameTime) override;  
    void spawn(const Vector &origin) override;
    void givePlayerItem(Player *player) override;
    static Powerup *CreatePowerup(const str &className, const str &modelName, Sentient *sentient);
    void cacheStrings(void) override;

    void Archive(Archiver& arc) override;


};

inline void Powerup::Archive(Archiver& arc)
{
    PowerupBase::Archive(arc);

    arc.ArchiveFloat(&_timeLeft);
}

class PowerupSpeed : public Powerup
{
public:
    CLASS_PROTOTYPE(PowerupSpeed);

    PowerupSpeed() {};
    ~PowerupSpeed() {};

    virtual float getMoveMultiplier(void) override {  return 1.5f; };
};

class PowerupStrength : public Powerup
{
public:
    CLASS_PROTOTYPE(PowerupStrength);

    PowerupStrength() {};
    ~PowerupStrength() {};

    virtual float getDamageDone(float damage, int meansOfDeath) override {  return damage * 2.0f; };
};

class PowerupProtection : public Powerup
{
public:
    CLASS_PROTOTYPE(PowerupProtection);

    PowerupProtection() {};
    ~PowerupProtection() {};

    virtual float getDamageTaken(Entity *attacker, float damage, int meansOfDeath) override;
};

class PowerupProtectionTemp : public PowerupProtection
{
public:
    CLASS_PROTOTYPE(PowerupProtectionTemp);

    PowerupProtectionTemp() {};
    ~PowerupProtectionTemp() {};

    virtual float getDamageDone(float damage, int meansOfDeath) override;
    virtual bool canDrop(void) override { return false; }
    virtual bool canStack(void) override { return false; }
};

class PowerupRegen : public Powerup
{
private:
    static const float REGEN_SPEED;

public:
    CLASS_PROTOTYPE(PowerupRegen);

    PowerupRegen() {};
    ~PowerupRegen() {};

    virtual void specificUpdate(float frameTime) override;
};

class PowerupInvisibility : public Powerup
{
private:
    bool _started;

public:
    CLASS_PROTOTYPE(PowerupInvisibility);

    PowerupInvisibility();
    ~PowerupInvisibility();

    virtual void specificUpdate(float frameTime) override;

    void Archive(Archiver& arc) override;
};

inline void PowerupInvisibility::Archive(Archiver& arc)
{
    Powerup::Archive(arc);

    arc.ArchiveBool(&_started);
}

class Rune : public PowerupBase
{
protected:
    Vector _originalOrigin;
    bool   _originalOriginSet;

public:
    CLASS_PROTOTYPE(Rune);

    Rune();
    virtual ~Rune() {}

    using Entity::setOrigin; 
    void setOrigin(const Vector &point) override;  
    void setOriginalOrigin(const Vector& point, bool force);

    Vector getOriginalOrigin(void) { return _originalOrigin; }
    void spawn(const Vector &origin) override; 

    void respawnAtOriginalOrigin(Event *ev);

    Item *ItemPickup(Entity *other, qboolean add_to_inventory = qtrue, qboolean checkautopickup = qtrue) override;
    void givePlayerItem(Player *player) override;
    static Rune *CreateRune(const str &className, const str &modelName, Sentient *sentient);
    void cacheStrings(void) override;

    void Archive(Archiver& arc) override;
};

inline void Rune::Archive(Archiver& arc)
{
    PowerupBase::Archive(arc);

    arc.ArchiveVector(&_originalOrigin);
    arc.ArchiveBool(&_originalOriginSet);
}

class RuneDeathQuad : public Rune
{
public:
    CLASS_PROTOTYPE(RuneDeathQuad);

    RuneDeathQuad() {};
    ~RuneDeathQuad() {};

    virtual void specificUpdate(float frameTime) override;
    virtual float getDamageDone(float damage, int meansOfDeath) override {  return damage * 4.0f; };
    virtual bool canOwnerRegenerate(void) override { return false; }
};

class RuneAmmoRegen : public Rune
{
private:
    float _nextGiveTime;

public:
    CLASS_PROTOTYPE(RuneAmmoRegen);

    RuneAmmoRegen();
    ~RuneAmmoRegen() {};

    virtual void specificUpdate(float frameTime) override;

    void Archive(Archiver& arc) override;
};

inline void RuneAmmoRegen::Archive(Archiver& arc)
{
    Rune::Archive(arc);

    arc.ArchiveFloat(&_nextGiveTime);
}

class RuneEmpathyShield : public Rune
{
public:
    CLASS_PROTOTYPE(RuneEmpathyShield);

    RuneEmpathyShield() {};
    ~RuneEmpathyShield() {};

    virtual float getDamageTaken(Entity *attacker, float damage, int meansOfDeath) override;
};

class RuneArmorPiercing : public Rune
{
public:
    CLASS_PROTOTYPE(RuneArmorPiercing);

    RuneArmorPiercing() {};
    ~RuneArmorPiercing() {};

    virtual meansOfDeath_t changetMeansOfDeath(meansOfDeath_t meansOfDeath) override {  return MOD_ARMOR_PIERCING; };
};

class HoldableItem : public PowerupBase
{
public:
    CLASS_PROTOTYPE(HoldableItem);

    HoldableItem();
    virtual ~HoldableItem() {}

    virtual bool use(void) { return true; }

    void givePlayerItem(Player *player) override;
    static HoldableItem *createHoldableItem(const str &className, const str &modelName, Sentient *sentient);
    void cacheStrings(void) override;
};

class HoldableItemSpawnPowerup : public HoldableItem
{
private:
    str _powerupToSpawn;

public:
    CLASS_PROTOTYPE(HoldableItemSpawnPowerup);

    bool use(void) override;

    void powerupToSpawn(Event *ev);

    void Archive(Archiver& arc) override;
};

inline void HoldableItemSpawnPowerup::Archive(Archiver& arc)
{
    HoldableItem::Archive(arc);

    arc.ArchiveString(&_powerupToSpawn);
}

class HoldableItemHealth : public HoldableItem
{
public:
    CLASS_PROTOTYPE(HoldableItemHealth);

    HoldableItemHealth() {};
    ~HoldableItemHealth() {};

    virtual bool use(void) override;
};

class HoldableItemProtection : public HoldableItem
{
public:
    CLASS_PROTOTYPE(HoldableItemProtection);

    HoldableItemProtection() {};
    ~HoldableItemProtection() {};

    virtual bool use(void) override;
};

class HoldableItemTransporter : public HoldableItem
{
public:
    CLASS_PROTOTYPE(HoldableItemTransporter);

    HoldableItemTransporter() {};
    ~HoldableItemTransporter() {};

    virtual bool use(void) override;
};

class HoldableItemExplosive : public HoldableItem
{
private:
    bool      _explosiveArmed;
    bool      _explosiveAlive;
    float     _explosiveArmTime;
    EntityPtr _explosive;
    float     _lastSoundTime;
    float     _nextProximitySoundTime;

    bool findPlaceToSet(Vector &newOrigin, Vector &newAngles);
    bool isPlayerInRange(const Vector &position, float maxDistance);

public:
    CLASS_PROTOTYPE(HoldableItemExplosive);

    HoldableItemExplosive();
    ~HoldableItemExplosive();

    virtual bool use(void) override;
    virtual void specificUpdate(float frameTime) override;

    void Archive(Archiver& arc) override;
};

inline void HoldableItemExplosive::Archive(Archiver& arc)
{
    HoldableItem::Archive(arc);

    arc.ArchiveBool(&_explosiveArmed);
    arc.ArchiveBool(&_explosiveAlive);
    arc.ArchiveFloat(&_explosiveArmTime);
    arc.ArchiveSafePointer(&_explosive);

    arc.ArchiveFloat(&_lastSoundTime);
    arc.ArchiveFloat(&_nextProximitySoundTime);
}
