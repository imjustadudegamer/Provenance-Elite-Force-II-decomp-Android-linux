//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/dispenser.cpp                             $
// $Revision:: 20                                                             $
//   $Author:: Singlis                                                        $
//     $Date:: 9/26/03 2:36p                                                  $
//
// Copyright (C) 2002 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// DESCRIPTION:
// This is all the dispensing type objects in the world.  They basically work
// by the player using them.  As the player uses them they give the player
// something (health/ammo/etc.).
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "dispenser.h"
#include "player.h"
#include "item.h"
#include "armor.h"
#include "g_utils.h"

Event EV_Dispenser_SetMaxAmount (	"dispenser_maxamount",
	EV_DEFAULT,
	"f",
	"maxAmount",
	"Sets the total amount that this dispenser can give out."
);


Event EV_Dispenser_SetDispenseRate (    "dispenser_rate",
    EV_DEFAULT,
    "f",
    "dispenseRate",
    "Sets the dispense rate (how many units per second to give out)."
);
Event EV_Dispenser_SetRegenRate (    "dispenser_regenrate",
    EV_DEFAULT,
    "f",
    "regenRate",
    "Sets the regen rate (how many units per second to regen)."
);
Event EV_Dispenser_SetType (    "dispenser_type",
    EV_DEFAULT,
    "s",
    "type",
    "Sets the type of thing to give out.\nOnly health and ammo are valid currently.");
Event EV_Dispenser_SetSubtype (    "dispenser_subtype",
    EV_DEFAULT,
    "s",
    "subtype",
    "Sets the subtype of thing to give out.\nThere is no subtype for health, the subtype for ammo is the ammo type.");
Event EV_Dispenser_SetOpenDistance (    "dispenser_openDistance",
    EV_DEFAULT,
    "f",
    "openDistance",
    "Sets the distance from the player that the dispenser will open/close\n");
Event EV_Dispenser_SetInstant (    "dispenser_instant",
    EV_DEFAULT,
    NULL,
    NULL,
    "Makes the dispenser give out its stuff instantly\n");
Event EV_Dispenser_AnimDone (    "dispenser_animdone",
    EV_CODEONLY,
    NULL,
    NULL,
    "Called when the dispenser's animation is done"   
);
Event EV_Dispenser_SetSoundName (    "dispenser_soundName",
    EV_DEFAULT,
    "s",
    "soundName",
    "Makes the dispenser use the specified looping sound when its being used\n");

CLASS_DECLARATION(Entity, Dispenser, NULL) {
    {&EV_Use,                       &Dispenser::useEvent       },
    {&EV_Dispenser_SetMaxAmount,    &Dispenser::setMaxAmount   },
    {&EV_Dispenser_SetDispenseRate, &Dispenser::setDispenseRate},
    {&EV_Dispenser_SetRegenRate,    &Dispenser::setRegenRate   },
    {&EV_Dispenser_SetType,         &Dispenser::setType        },
    {&EV_Dispenser_SetSubtype,      &Dispenser::setSubtype     },
    {&EV_Dispenser_SetOpenDistance, &Dispenser::setOpenDistance},
    {&EV_Dispenser_SetInstant,      &Dispenser::setInstant     },
    {&EV_Dispenser_AnimDone,        &Dispenser::animDone       },
    {&EV_Dispenser_SetSoundName,    &Dispenser::setSoundName   },
    {NULL,                          NULL                       }
};

Dispenser::Dispenser()
{
    animate = new Animate(this);
    setMoveType(MOVETYPE_NONE);
    setSolidType(SOLID_BBOX);
    setContents(CONTENTS_BODY);

    _maxAmount = 0;
    _amount    = 0;

    _dispenseRate = 0;
    _regenRate    = 0;

    _dispenseType = DISPENSE_TYPE_NONE;

    _lastTimeUsed = 0;

    edict->s.eType = ET_MODELANIM;

    _openDistance = 200.0f;

    if (!LoadingSavegame) {
        if (animate->HasAnim("closed_idle")) {
            animate->RandomAnimate("closed_idle");
        }
    }

    _state = DISPENSER_STATE_CLOSED;

    _instant = false;

    _entityDispensingTo = NULL;

    flags |= FL_THINK;

    _playingSound  = false;
    _lastSoundTime = 0.0f;
}

void Dispenser::Think(void)
{
    bool    playerNearby;
    Player *player;
    Vector  dir;
    float   distance;

    if (_playingSound && level.time > _lastSoundTime + 0.25f) {
        StopLoopSound();
        _playingSound = false;
    }

    if (_entityDispensingTo) {
        bool dispensedStuff;

        dispensedStuff = dispenseStuff(_entityDispensingTo);

        if (dispensedStuff) {
            return;
        } else {
            _entityDispensingTo = NULL;
        }
    }


    if (_regenRate > 0.0f) {
        _amount += _regenRate * level.frametime;

        if (_amount > _maxAmount) {
            _amount = _maxAmount;
        }

        setDispenserAlpha();
    }


    playerNearby = false;

    if (_state != DISPENSER_STATE_EMPTY) {
        player = (Player *)g_entities[0].entity;

        if (player) {
            dir      = player->origin - origin;
            distance = dir.length();

            if (distance < _openDistance) {
                playerNearby = true;
            }
        }
    }

    if (_state == DISPENSER_STATE_CLOSED) {
        if (playerNearby) {
            animate->RandomAnimate( "opening", EV_Dispenser_AnimDone );  
            _state = DISPENSER_STATE_OPENING;
        }
    } else if (_state == DISPENSER_STATE_OPEN) {
        if (!playerNearby) {
            animate->RandomAnimate( "closing", EV_Dispenser_AnimDone );  
            _state = DISPENSER_STATE_CLOSING;
        }
    }
}

void Dispenser::useEvent(Event *ev)
{
    Sentient *sent;
    Entity   *ent;

    if (_amount <= 0) {
        return;
    }

    if (_lastTimeUsed == level.time) {
        return;
    }

    ent = ev->GetEntity(1);

    if (!ent || !ent->isSubclassOf(Sentient)) {
        return;
    }

    sent = (Sentient *)ent;

    if (_instant) {
        _entityDispensingTo = sent;
    } else {
        dispenseStuff(sent);
    }

    _lastTimeUsed = level.time;
}

bool Dispenser::dispenseStuff(Sentient *sent)
{
    DispenseType currentDispenseType = DISPENSE_TYPE_HEALTH;
    float        maxAmountToGive;
    float        amountToGive;
    float        entityMaxAmount     = 0.0f;
    float        entityCurrentAmount = 0.0f;

    if (!_playingSound && _useSoundName.length()) {
        LoopSound(_useSoundName);
        _playingSound = true;
    }

    _lastSoundTime = level.time;


    if (_dispenseType == DISPENSE_TYPE_HEALTH) {
        entityMaxAmount     = sent->max_health;
        entityCurrentAmount = sent->health;

        currentDispenseType = DISPENSE_TYPE_HEALTH;

        if (entityCurrentAmount >= entityMaxAmount) {
            Item *baseArmor = sent->FindBaseArmor();  

            entityMaxAmount     = 100.0f;
            entityCurrentAmount = baseArmor ? (float)baseArmor->getAmount() : 0.0f;

            currentDispenseType = DISPENSE_TYPE_ARMOR;
        }
    } else if (_dispenseType == DISPENSE_TYPE_AMMO) {
        entityMaxAmount     = (float)sent->MaxAmmoCount(_dispenseSubtype);
        entityCurrentAmount = (float)sent->AmmoCount(_dispenseSubtype);

        currentDispenseType = DISPENSE_TYPE_AMMO;
    }


    maxAmountToGive = _dispenseRate * level.frametime;

    if (_dispenseType == DISPENSE_TYPE_AMMO) {
        amountToGive = maxAmountToGive;
    } else {
        if (maxAmountToGive > entityMaxAmount - entityCurrentAmount) {
            amountToGive = entityMaxAmount - entityCurrentAmount;
        } else {
            amountToGive = maxAmountToGive;
        }
    }

    if (amountToGive > _amount) {
        amountToGive = _amount;
    }

    if (amountToGive < 0.0f) {
        amountToGive = 0.0f;
    }


    if (amountToGive) {
        if (currentDispenseType == DISPENSE_TYPE_HEALTH) {
            sent->health = entityCurrentAmount + amountToGive;

            if (sent->health > sent->max_health) {
                sent->health = sent->max_health;
            }
        } else if (currentDispenseType == DISPENSE_TYPE_ARMOR) {
            Event *armorEvent = new Event(EV_Sentient_GiveArmor);
            armorEvent->AddString("BasicArmor");
            armorEvent->AddFloat(amountToGive);
            sent->ProcessEvent(armorEvent);
        } else if (currentDispenseType == DISPENSE_TYPE_AMMO) {
            int ammoGiven = sent->GiveAmmo(_dispenseSubtype, (int)amountToGive, false);
            amountToGive  = (float)ammoGiven;
        }
    }


    _amount -= amountToGive;

    setDispenserAlpha();


    if ((_amount <= 0.0f) && (_regenRate == 0.0f)) {
        animate->RandomAnimate( "emptying", EV_Dispenser_AnimDone );  
        _state = DISPENSER_STATE_EMPTYING;
    }

    return (amountToGive > 0);
}

void Dispenser::setMaxAmount(Event *ev)
{
    _maxAmount = ev->GetFloat(1);
    _amount    = _maxAmount;
}

void Dispenser::setDispenseRate(Event *ev)
{
    _dispenseRate = ev->GetFloat(1);
}

void Dispenser::setRegenRate(Event *ev)
{
    _regenRate = ev->GetFloat(1);
}

void Dispenser::setType(Event *ev)
{
    str typeName;

    typeName = ev->GetString(1);

    if (typeName == "health") {
        _dispenseType = DISPENSE_TYPE_HEALTH;
    } else if (typeName == "ammo") {
        _dispenseType = DISPENSE_TYPE_AMMO;
    } else {
        gi.WDPrintf("Unknown dispense type %s\n", typeName.c_str());
        _dispenseType = DISPENSE_TYPE_NONE;
    }
}

void Dispenser::setSubtype(Event *ev)
{
    _dispenseSubtype = ev->GetString(1);
}

void Dispenser::setOpenDistance(Event *ev)
{
    _openDistance = ev->GetFloat(1);
}

void Dispenser::animDone(Event *ev)
{
    if (_state == DISPENSER_STATE_CLOSING) {
        animate->RandomAnimate( "closed_idle" );  
        _state = DISPENSER_STATE_CLOSED;
    } else if (_state == DISPENSER_STATE_OPENING) {
        animate->RandomAnimate( "open_idle" );  
        _state = DISPENSER_STATE_OPEN;
    } else if (_state == DISPENSER_STATE_EMPTYING) {
        animate->RandomAnimate( "empty_idle" );  
        _state = DISPENSER_STATE_EMPTY;
    }
}

void Dispenser::setDispenserAlpha(void)
{
    float percentLeft;

    if (_maxAmount) {
        percentLeft = _amount / _maxAmount;

        setAlpha(percentLeft);
        G_SetConstantLight(&edict->s.constantLight, &percentLeft, &percentLeft, &percentLeft, NULL, NULL);
    }
}

void Dispenser::setInstant(Event *ev)
{
    _instant = true;
}

void Dispenser::setSoundName(Event *ev)
{
    _useSoundName = ev->GetString(1);
}
