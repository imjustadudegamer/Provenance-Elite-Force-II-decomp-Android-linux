//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/health.cpp                                $
// $Revision:: 21                                                             $
//   $Author:: Singlis                                                        $
//     $Date:: 9/26/03 2:36p                                                  $
//
// Copyright (C) 1997 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// Health powerup
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).


#include "g_local.h"
#include "mp_manager.hpp"
#include "item.h"
#include "inventoryitem.h"
#include "sentient.h"
#include "health.h"
#include "weaputils.h"
#include "player.h"
#include "g_phys.h"
#include "../qcommon/gameplaymanager.h"
#include "usedata.h" 

CLASS_DECLARATION(Item, Health, "health_020") {
    {&EV_Item_Pickup, &Health::PickupHealth},
    {NULL,            NULL                 }
};

Health::Health()
{
    if (multiplayerManager.checkFlag(MP_FLAG_NO_HEALTH)) {
        PostEvent(EV_Remove, EV_REMOVE);
        return;
    }

    setAmount(20);

    item_name = "health";
}

// Also restored: retail admits any Sentient, not just the Player (the port tested
void Health::PickupHealth(Event *ev)
{
    Sentient *sen;
    Entity   *other;

    other = ev->GetEntity(1);
    if (!other || !other->isSubclassOf(Sentient)) {
        return;
    }

    sen = (Sentient *)other;

    if (sen->health >= sen->max_health) {
        return;
    }

    if (!ItemPickup(other, qfalse)) {
        return;
    }

    {
        GameplayManager *gpm = GameplayManager::getTheGameplayManager();

        if (gpm && gpm->hasObject(getArchetype())) {
            if (gpm->hasFormula("HealthPotion")) {
                GameplayFormulaData fd(this);  

                amount = gpm->calculate("HealthPotion", fd, sen->max_health);
            }

            str useObject = getArchetype() + ".Use";
            str snd       = gpm->getStringValue(useObject.c_str(), "wav");

            if (snd.length()) {
                int   channel = CHAN_BODY;
                float volume  = -1.0f;
                float mindist = -1.0f;

                if (gpm->hasProperty(useObject.c_str(), "channel")) {
                    channel = (int)gpm->getFloatValue(useObject.c_str(), "channel");
                }
                if (gpm->hasProperty(useObject.c_str(), "volume")) {
                    volume = (int)gpm->getFloatValue(useObject.c_str(), "volume");
                }
                if (gpm->hasProperty(useObject.c_str(), "mindist")) {
                    mindist = (int)gpm->getFloatValue(useObject.c_str(), "mindist");
                }

                Sound(snd, channel, volume, mindist);
            }
        }
    }

    sen->health += amount;

    if (sen->health > sen->max_health) {
        sen->health = sen->max_health;
    }

    sen->ProcessEvent(EV_Sentient_StopOnFire);
}

CLASS_DECLARATION(InventoryItem, HealthInventoryItem, "") {
    {&EV_InventoryItem_Use,       &HealthInventoryItem::Use},
    {&EV_ProcessGameplayData,     &HealthInventoryItem::processGameplayData},
    {NULL,                        NULL                      }
};






void HealthInventoryItem::Use( Event *ev )
{
	Entity      *other;
	Sentient    *sen;
	str			sound_name;
	
	
	other = ev->GetEntity( 1 );
	if ( !other || !other->isSubclassOf( Sentient ) )
	{
		return;
	}
	
	sen = ( Sentient * )other;
	
	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	if ( gpm->hasObject(getArchetype()) )
	{
		if ( gpm->hasFormula("HealthPotion") )
		{
			GameplayFormulaData fd(this);
			amount = gpm->calculate("HealthPotion", fd, sen->max_health);
		}
		str snd = gpm->getStringValue(getArchetype() + ".Use", "wav");
		if ( snd.length() )
		{
			int channel = CHAN_BODY;
			float volume = -1.0f;
			float mindist = -1.0f;
			if ( gpm->hasProperty(getArchetype() + ".Use","channel") )
				channel = (int)gpm->getFloatValue(getArchetype() + ".Use", "channel");
			if ( gpm->hasProperty(getArchetype() + ".Use","volume") )
				volume = (int)gpm->getFloatValue(getArchetype() + ".Use", "volume");
			if ( gpm->hasProperty(getArchetype() + ".Use","mindist") )
				mindist = (int)gpm->getFloatValue(getArchetype() + ".Use", "mindist");
			Sound(snd, channel, volume, mindist);
		}
	}
	
	sen->health += (float)amount;
	
	if ( sen->health > sen->max_health )
	{
		sen->health = sen->max_health;
	}
	
	
	sen->ProcessEvent( EV_Sentient_StopOnFire );
	
	
	
	PostEvent( EV_Remove, 0.0f );
}



void HealthInventoryItem::processGameplayData( Event *ev )
{
	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	if ( !gpm->hasObject(getArchetype()) )
		return;

	str objname = getArchetype();
	str useanim, usetype, usethread;
	if ( gpm->hasProperty(objname, "useanim") )
		useanim = gpm->getStringValue(objname, "useanim");
	if ( gpm->hasProperty(objname, "usethread") )
		usethread = gpm->getStringValue(objname, "usethread");
	if ( gpm->hasProperty(objname + ".Pickup", "icon") )
		usetype = gpm->getStringValue(objname + ".Pickup", "icon");

	if ( useanim.length() || usethread.length() || usetype.length() )
	{
		if ( !useData )
			useData = new UseData();
		
		useData->setUseAnim(useanim);
		useData->setUseThread(usethread);
		useData->setUseType(usetype);
	}
}

