//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/GoDirectlyToPoint.cpp                      $
// $Revision:: 6                                                              $
//   $Author:: Singlis                                                        $
//     $Date:: 9/26/03 2:36p                                                  $
//
// Copyright (C) 2001 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// 
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "_pch_cpp.h"
#include "actor.h"
#include "actor_locomotion.h"
#include "GoDirectlyToPoint.h"


CLASS_DECLARATION( Steering, GoDirectlyToPoint, NULL )
{
	{ NULL, NULL }
};

GoDirectlyToPoint::GoDirectlyToPoint()
{
}

void GoDirectlyToPoint::Begin(Actor &self)
{
}

const Steering::ReturnValue GoDirectlyToPoint::Evaluate( Actor &self )
{
	ResetForces();

	if (AtDestination( self ))
	{
		return Steering::SUCCESS;
	}
	
	Vector newSteeringForce = self.movementSubsystem->SteerTowardsPoint
											( 
											_destination,			
											vec_zero,											
											self.movementSubsystem->getMoveDir(),		
											self.movementSubsystem->getMoveSpeed()		
											);
	self.movementSubsystem->Accelerate( newSteeringForce );
	
	return Steering::EVALUATING ;
}

void GoDirectlyToPoint::ShowInfo( Actor &self )
{
	Steering::ShowInfo( self );
	
	gi.Printf( "\n destination : ( %f, %f, %f ) \n", _destination.x, _destination.y, _destination.z );
}



const bool GoDirectlyToPoint::AtDestination(const Actor &self) const
{
	if ((Vector::DistanceXY( _destination, self.origin ) ) <= _radius )
	{
		return true;
	}

	return false;
}
