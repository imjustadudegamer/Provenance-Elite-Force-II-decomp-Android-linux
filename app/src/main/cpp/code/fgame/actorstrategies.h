//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/actorstrategies.h                              $
// $Revision:: 19                                                             $
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
//
// DESCRIPTION:
// I am replacing the old way that Actor::Think was done by implementing a group of strategies for it
// instead.  This will form the foundation for new flexiblility within the actor class.  
//
// What I am trying to achieve is a specialized "think" for different types of actors.  A Boss, for instance,
// would use BossThink, an Non-Attacking-NPC could use NPCThink.  Using the event system we already have in place
// it will be easy to change thinking modalities on the fly, allowing us to make a cowardly NPC turn into a 
// roaring death machine if he gets shot.
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#ifndef __ACTORSTRATEGIES_H__
#define __ACTORSTRATEGIES_H__


class Actor;
class Entity;
class Archiver;

typedef SafePtr<Actor> ActorThinkActorPtr;

class ActorThink
{
public:
    virtual ~ActorThink() {}

    virtual void DoArchive(Archiver& arc);
    virtual void Think(Actor&) = 0;
    virtual bool isSimple(void) { return false; }

    virtual void ActorStateUpdate(Actor& actor);  
    virtual void CheckGround(Actor& actor);       
    virtual void TryDrown(Actor& actor);          
    virtual void InanimateObject(Actor& actor);   
    virtual void DoMove(Actor& actor);            

    virtual void ProcessBehaviors(Actor& actor);    
    virtual void UpdateBossHealth(Actor& actor);    
};

class DefaultThink : public ActorThink
{
public:
    virtual void Think(Actor& actor) override;  
};

class SimplifiedThink : public ActorThink
{
public:
    SimplifiedThink(Actor *actor);
    ~SimplifiedThink(void);

    virtual bool isSimple(void) override { return true; }

    virtual void DoArchive(Archiver& arc) override;  
    virtual void DoMove(Actor& actor) override;      
    virtual void Think(Actor& actor) override;       

protected:

private:
    const SimplifiedThink& operator=(const SimplifiedThink&);

    ActorThinkActorPtr _actor;
    int                _previousContents;




};

#endif 
