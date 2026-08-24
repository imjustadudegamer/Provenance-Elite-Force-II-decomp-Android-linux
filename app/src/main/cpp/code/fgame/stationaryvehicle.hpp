//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/specialfx.cpp                                 $
// $Revision:: 4                                                              $
//   $Author:: Squirrel                                                       $
//     $Date:: 5/13/02 2:49p                                                  $
//
// Copyright (C) 1997 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// $Log:: /Code/DLLs/game/specialfx.cpp                                      $
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#ifndef STATIONARY_VEHICLE_HPP
#define STATIONARY_VEHICLE_HPP

#include "vehicle.h"


class StationaryVehicle : public Vehicle
{
	public:
		CLASS_PROTOTYPE( StationaryVehicle );
		StationaryVehicle();
		virtual ~StationaryVehicle();

		 void		Postthink() override;
		 qboolean	Drive( usercmd_t *ucmd ) override;
		void					PositionVehicleAndDriver(void);

		void					Killed(Event* event);

		virtual void			Archive(	Archiver &arc ) override;


	private:
		float					_yawDeltaDegrees;
		float					_pitchDeltaDegrees;
};

inline void StationaryVehicle::Archive( Archiver &arc )
{
	Vehicle::Archive( arc );

	arc.ArchiveFloat( &_yawDeltaDegrees );
	arc.ArchiveFloat( &_pitchDeltaDegrees );
}

#endif 
