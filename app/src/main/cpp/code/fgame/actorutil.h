//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/actorutil.h                                    $
// $Revision:: 28                                                             $
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
// Utility Functions and classes for Actor
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).


class FindCoverMovement;
class FindFleeMovement;
class FindEnemyMovement;

#ifndef __ACTORUTIL_H__
#define __ACTORUTIL_H__

#include "actor.h"
#include "actorincludes.h"
#include "weapon.h"

qboolean EntityIsValidTarget( const Entity *ent );

class FindMovement : public StandardMovement
	{
	public:
		Actor			*self;

	qboolean validpath (	PathNode *node, int i );
	qboolean done ( PathNode *node, const PathNode *end );
	};



class FindCoverMovement : public StandardMovement
	{
	public:
		Actor			*self;

	



public:
    qboolean validpath ( PathNode *node, int i ); 
    qboolean done ( PathNode *node, const PathNode *end ); 
};



class FindFleeMovement : public StandardMovement
	{
	public:
		Actor			*self;

	qboolean validpath ( PathNode *node, int i );  
	qboolean done ( PathNode *node, const PathNode *end );  

	};

class FindEnemyMovement : public StandardMovement
	{
	public:
		Actor			*self;

	qboolean done ( PathNode *node, const PathNode *end ); 

	};


#endif 
