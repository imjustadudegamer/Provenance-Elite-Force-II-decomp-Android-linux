//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/FollowPathToPoint.cpp                    $
// $Revision:: 9                                                              $
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
#include "FollowPathToPoint.h"
#include "actor.h"

CLASS_DECLARATION( FollowPath, FollowPathToPoint, NULL )
{
	{ NULL, NULL }
};

FollowPathToPoint::FollowPathToPoint():_targetPoint(0,0,0)
{
}

void FollowPathToPoint::SetGoal(const Vector &targetPoint, const float radius, Actor &self)
{
	SetRadius(radius);
	if (!Vector::CloseEnough(_targetPoint, targetPoint, 1.0f))
	{
		_targetPoint = targetPoint;
		GetPath().SetPath(self, self.origin, GetGoalPosition());
	}
}
