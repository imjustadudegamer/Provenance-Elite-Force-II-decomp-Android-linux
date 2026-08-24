//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/GoDirectlyToPoint.h                            $
// $Revision:: 4                                                              $
//   $Author:: Steven                                                         $
//     $Date:: 10/13/03 8:54a                                                 $
//
// Copyright (C) 2001 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// Simple Steering class that moves directly to the desired point 
// without any regard for obstacle or world avoidance
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#ifndef __GO_DIRECTLY_TO_POINT__
#define __GO_DIRECTLY_TO_POINT__

#include "g_local.h"
#include "steering.h"

class Actor;


class GoDirectlyToPoint : public Steering
{
public:
								CLASS_PROTOTYPE( GoDirectlyToPoint );

								GoDirectlyToPoint();
	virtual						~GoDirectlyToPoint() { }
	virtual void				Begin( Actor &self ) override;
	virtual const ReturnValue	Evaluate( Actor &self ) override;
	virtual void				ShowInfo( Actor &self ) override;

	void						SetDestination( const Vector &destination ) { _destination = destination; }
	void						SetRadius( const float radius ) { _radius = radius; }
	virtual void				Archive( Archiver &arc ) override;
	const bool					AtDestination( const Actor &self ) const;

private:
	Vector						_destination;
	float						_radius;
};

inline void GoDirectlyToPoint::Archive( Archiver &arc )
	
{
	Steering::Archive( arc );
	arc.ArchiveVector( &_destination );
	arc.ArchiveFloat( &_radius );
	
}

#endif 
