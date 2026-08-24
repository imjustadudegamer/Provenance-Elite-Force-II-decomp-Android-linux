//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/actorgamecomponents.h                          $
// $Revision:: 12                                                             $
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
// What I am trying to do here, is encapsulate any game specific pieces for actor.  Each game will 
// subclass off of the base class, then the Actor class will point to the component class it will
// use.  I am hoping this will make life much easier for mod makers as well
//
//============================
// Forward Declarations
//============================
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#ifndef __ACTORGAMECOMPONENTS_H__
#define __ACTORGAMECOMPONENTS_H__


class Actor;
class Entity;
class Archiver;

class ActorGameComponent : public Listener
{
public:
    CLASS_PROTOTYPE(ActorGameComponent);

    ActorGameComponent() {}

    virtual void HandleEvent(Event *ev) {}
    virtual void HandleArmorDamage(Event *ev) {}
    virtual void HandleDeath(const Entity *ent) {}
    virtual void HandleThink() {}

    virtual qboolean DoCheck(const Conditional& condition) { return false; }
    virtual void     DoArchive(Archiver& arc, const Actor *act) {}
};

class EFGameComponent : public ActorGameComponent
{
public:
    CLASS_PROTOTYPE(EFGameComponent);

    EFGameComponent();
    EFGameComponent(const Actor *actor);

    void HandleEvent(Event *ev) override;
    void HandleArmorDamage(Event *ev) override;
    void HandleDeath(const Entity *ent) override;
    void HandleThink() override;

    qboolean DoCheck(const Conditional& condition) override;
    void     DoArchive(Archiver& arc, const Actor *act) override;

private:
    Actor *act;
};

#endif 
