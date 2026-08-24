//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/FollowPathToEntity.cpp                    $
// $Revision:: 12                                                             $
//   $Author:: Singlis                                                        $
//     $Date:: 9/26/03 2:36p                                                  $
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// 
//
// DESCRIPTION:
// Specialization of FollowPath used to follow moving entities
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "_pch_cpp.h"
#include "actor.h"
#include "FollowPathToEntity.h"

CLASS_DECLARATION( FollowPath, FollowPathToEntity, NULL )
{
	{ NULL, NULL }
};

FollowPathToEntity::FollowPathToEntity():
	_targetEntity(NULL),
	_oldGoalPosition(0,0,0)
{
}

void FollowPathToEntity::SetGoal(Entity *entity, const float radius, Actor &self)
{
	SetRadius(radius);
	if (
			( _targetEntity == NULL ) ||
			( _targetEntity->entnum != entity->entnum)
		)
	{
		_targetEntity = entity;
		_oldGoalPosition = GetGoalPosition();
		GetPath().SetPath(self, self.origin, GetGoalPosition());
	}
}

const bool FollowPathToEntity::ClearTraceToGoal( Actor &self, const trace_t &traceToGoal, const float radius ) const
{
	assert (_targetEntity != NULL);

	bool traceHitGoal = (
						(_targetEntity->entnum == traceToGoal.entityNum) && 
						(traceToGoal.entityNum != ENTITYNUM_NONE)
						);	
	

	return ( FollowPath::ClearTraceToGoal( self, traceToGoal, radius ) || (!traceToGoal.startsolid && traceHitGoal ) );
}

const Steering::ReturnValue FollowPathToEntity::Evaluate( Actor &self)
{
	if (Vector::Distance( _oldGoalPosition, GetGoalPosition()) > GetAvoidanceDistance() )
	{
		_oldGoalPosition = GetGoalPosition();
		GetPath().SetPath(self, self.origin, GetGoalPosition() );
	}


	return FollowPath::Evaluate( self);
}
