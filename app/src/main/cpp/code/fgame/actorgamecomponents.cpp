//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/actorgamecomponents.cpp                        $
// $Revision:: 18                                                             $
//   $Author:: Steven                                                         $
//     $Date:: 10/11/02 3:34a                                                 $
//
// Copyright (C) 2001 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// 
//
// DESCRIPTION:
// What I am trying to do here, is encapsulate any game specific pieces for actor.  Each game will 
// subclass off of the base class, then the Actor class will point to the component class it will
// use.  I am hoping this will make life much easier for mod makers as well
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "actor.h"
#include "actorgamecomponents.h"
#include "player.h"

CLASS_DECLARATION(Listener, ActorGameComponent, "actor_game_component") {
    {NULL, NULL}
};

CLASS_DECLARATION(ActorGameComponent, EFGameComponent, "ef_game_component") {
    {NULL, NULL}
};

EFGameComponent::EFGameComponent()
{
    gi.Error(ERR_FATAL, "EFGameComponent::EFGameComponent -- Default Constructor Called");
    act = NULL;
}

EFGameComponent::EFGameComponent(const Actor *actor)
{
    act = NULL;

    if (actor) {
        act = (Actor *)actor;
    }
}

void EFGameComponent::DoArchive(Archiver& arc, const Actor *actor)
{
    if (actor) {
        act = (Actor *)actor;
    } else {
        gi.Error(ERR_FATAL, "EFGameComponnet::DoArchive -- actor is NULL");
    }
}

void EFGameComponent::HandleDeath(const Entity *ent) {}

void EFGameComponent::HandleArmorDamage(Event *ev) {}

void EFGameComponent::HandleThink() {}

void EFGameComponent::HandleEvent(Event *ev)
{
    Event *new_event;
    new_event = new Event(*ev);
    ProcessEvent(new_event);
}

qboolean EFGameComponent::DoCheck(const Conditional& condition)
{
    return false;
}
