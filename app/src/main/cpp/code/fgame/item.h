//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/item.h                                        $
// $Revision:: 24                                                             $
//   $Author:: Steven                                                         $
//     $Date:: 2/14/03 5:37p                                                  $
//
// Copyright (C) 1997 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// Base class for respawnable, carryable objects.
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).


#pragma once

#include "g_local.h"
#include "entity.h"
#include "trigger.h"
#include "sentient.h"
#include "gamescript.h"

extern Event EV_Item_Pickup;
extern Event EV_Item_DropToFloor;
extern Event EV_Item_Respawn;
extern Event EV_Item_SetAmount;
extern Event EV_Item_SetMaxAmount;
extern Event EV_Item_IconName;  
extern Event EV_Item_RespawnSound;
extern Event EV_Item_DialogNeeded;
extern Event EV_Item_PickupDone;

#define DROPPED_ITEM        0x00008000
#define DROPPED_PLAYER_ITEM 0x00010000

#ifndef EF2_MULTIPLAYERITEMTYPE_DEFINED
#    define EF2_MULTIPLAYERITEMTYPE_DEFINED
typedef enum {
    MP_ITEM_TYPE_NORMAL,
    MP_ITEM_TYPE_POWERUP,
    MP_ITEM_TYPE_RUNE,
    MP_ITEM_TYPE_WEAPON,
    MP_ITEM_TYPE_ARMOR
} MultiplayerItemType;
#endif

class Item : public Trigger
{
private:
    float _skillLevel;

    int _iconIndex;

protected:
    SentientPtr owner;
    qboolean    respawnable;
    qboolean    playrespawn;

    qboolean coolitem;
    qboolean coolitemforced;
    str      cool_dialog;
    str      cool_anim;

    float respawntime;
    str   dialog_needed;
    int   item_index;
    str   item_name;

    float maximum_amount;
    float amount;

    str pickup_thread;

    qboolean no_remove;

    int bot_inventory_index;

    MultiplayerItemType _mpItemType;

    int _missingSkin;

    // never archived: every retail pickup path hardcodes the literal

    void ItemTouch(Event *ev);

public:
    qboolean has_been_looked_at;

    float _nextPickupTime;

    CLASS_PROTOTYPE(Item);

    Item();
    ~Item();
    virtual void      PlaceItem(void);
    virtual void      SetOwner(Sentient *ent);
    virtual Sentient *GetOwner(void) const;  
    void              SetNoRemove(Event *ev);
    void              iconNameEvent(Event *ev);     
    void              setMissingSkin(Event *ev);    
    void              ChangeSkin(int skinNum, qboolean state);  

    int getIcon(void) const { return _iconIndex; }

    virtual void      DropToFloor(Event *ev);
    virtual Item     *ItemPickup(Entity *other, qboolean add_to_inventory = qtrue, qboolean checkautopickup = qtrue);  
    virtual void      Respawn(Event *ev);
    virtual void      setRespawn(qboolean flag);
    virtual qboolean  Respawnable(void);
    virtual void      setRespawnTime(float time);
    virtual float     RespawnTime(void);
    void              RespawnDone(Event *ev);
    void              PickupDone(Event *ev);

    virtual int GetItemIndex(void) {  return item_index; };

    virtual float getAmount(void);
    virtual void  setAmount(float startamount);

    virtual float    MaxAmount(void);
    virtual qboolean Pickupable(Entity *other);

    virtual void setName(const char *i);
    virtual const str getName(void) const override;  
    virtual int  getIndex(void);
    virtual void SetAmountEvent(Event *ev);
    virtual void SetMaxAmount(Event *ev);
    virtual void SetItemName(Event *ev);

    virtual void     SetMax(int maxamount);
    virtual void     Add(int num);
    virtual void     Remove(int num);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
    virtual qboolean Use(int amount);
#pragma GCC diagnostic pop
    virtual qboolean Removable(void);
    virtual void     Pickup(Event *ev);
    virtual qboolean Drop(void);
    virtual void     RespawnSound(Event *ev);
    virtual void     DialogNeeded(Event *ev);
    virtual str      GetDialogNeeded(void);
    void             Landed(Event *ev);

    virtual void SetPickupThread(Event *ev);

    void     CoolItemEvent(Event *ev);
    void     ForceCoolItemEvent(Event *ev);
    qboolean IsItemCool(str *dialog, str *anim, qboolean *force);
    void     SetCoolItem(qboolean cool, const str& dialog, const str& anim);

    int  GetBotInventoryIndex(void);
    void SetBotInventoryIndex(Event *ev);

    void  SetSkillLevel(float skillLevel) { _skillLevel = skillLevel; }
    float GetSkillLevel(void) { return _skillLevel; }

    MultiplayerItemType getMultiplayerItemType(void);

    virtual void cacheStrings(void);
    void         postSpawn(Event *ev);

    void Archive(Archiver& arc) override;
    virtual void ArchivePersistantData(Archiver &arc) {}  

public:




public:
    void setRespawn( Event *ev ); 
    void setRespawnTime( Event *ev ); 
};

// Two fields retail deliberately does NOT archive:
//   * anything MOHAA-only (sPickupSound), which retail's Item does not have at all.
inline void Item::Archive(Archiver& arc)
{
    str iconName;

    Trigger::Archive(arc);

    arc.ArchiveFloat(&_skillLevel);

    // imageindex("") == 0, not -1 -- retail's own behaviour, deliberately reproduced:
    if (arc.Loading()) {
        arc.ArchiveString(&iconName);

        _iconIndex = gi.imageindex(iconName.c_str());
    } else {
        if (_iconIndex >= 0) {
            iconName = gi.getConfigstring(CS_IMAGES + _iconIndex);
        } else {
            iconName = "";
        }

        arc.ArchiveString(&iconName);
    }

    arc.ArchiveSafePointer(&owner);
    arc.ArchiveBoolean(&respawnable);
    arc.ArchiveBoolean(&playrespawn);
    arc.ArchiveBoolean(&coolitem);
    arc.ArchiveBoolean(&coolitemforced);
    arc.ArchiveString(&cool_dialog);
    arc.ArchiveString(&cool_anim);
    arc.ArchiveFloat(&respawntime);
    arc.ArchiveString(&dialog_needed);
    arc.ArchiveString(&item_name);
    if (arc.Loading()) {
        setName(item_name.c_str());
    }
    arc.ArchiveFloat(&maximum_amount);
    arc.ArchiveFloat(&amount);
    arc.ArchiveString(&pickup_thread);
    arc.ArchiveBoolean(&no_remove);

    arc.ArchiveInteger(&bot_inventory_index);
    ArchiveEnum(_mpItemType, MultiplayerItemType);
    arc.ArchiveInteger(&_missingSkin);

    arc.ArchiveBoolean(&has_been_looked_at);

    arc.ArchiveFloat(&_nextPickupTime);
}

class MultiplayerItem : public Item
{
public:
    CLASS_PROTOTYPE(MultiplayerItem);

    MultiplayerItem();

    void notifyMultiplayerItemTriggered(Event *ev);
    void notifyMultiplayerItemDestroyed(Event *ev);
    void notifyMultiplayerItemUsed(Event *ev);

    void damageEvent(Event *ev);

    void cacheStrings(void) override;
};

class SecretItem : public Item
{
public:
    CLASS_PROTOTYPE(SecretItem);

    SecretItem();

    Item *ItemPickup(Entity *other, qboolean add_to_inventory = qtrue, qboolean checkautopickup = qtrue) override;  

    void cacheStrings(void) override;
};
