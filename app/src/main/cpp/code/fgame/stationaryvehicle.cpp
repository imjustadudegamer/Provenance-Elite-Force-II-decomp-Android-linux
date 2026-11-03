//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/stationaryvehicle.cpp                     $
// $Revision:: 8                                                              $
//   $Author:: Singlis                                                        $
//     $Date:: 9/26/03 2:36p                                                  $
//
// Copyright (C) 1997 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "stationaryvehicle.hpp"
#include "player.h"

#define PROBE_GET(probe_n, probe_d, probe_f) gi.cvar(probe_n, probe_d, probe_f)
#define PROBE_PRINT                          gi.Printf
#include "../qcommon/qa_probe.h"


CLASS_DECLARATION(Vehicle, StationaryVehicle, NULL)
{
	{ NULL, NULL }
};



StationaryVehicle::StationaryVehicle()
{
	_yawDeltaDegrees	= 0.0f;
	_pitchDeltaDegrees	= 0.0f;

}


StationaryVehicle::~StationaryVehicle()
{

}



void StationaryVehicle::Postthink()
{
	float apply_yaw      = 0.0f;
	float apply_pitch    = 0.0f;
	int   apply_restrict = 0;

	if (PROBE_On()) {
		apply_yaw      = _yawDeltaDegrees;
		apply_pitch    = _pitchDeltaDegrees;
		apply_restrict = ( _restrictPitch ? 1 : 0 ) | ( _restrictYaw ? 2 : 0 );
	}

	if(drivable)
	{
		turnangle  = turnangle   * 0.25f + turnimpulse;
		pitchangle = pitchangle  * 0.25f + pitchimpulse;
		
		float turn = turnangle	 *	( 1.0f / 200.0f );
		float pitch = pitchangle *	( 1.0f / 200.0f );

		avelocity *= 0.05f;
		avelocity.y += turn  * 200.0f;
		avelocity.x += pitch * 200.0f;
		
		angles += avelocity * level.frametime;
		setAngles(angles);
	}

	PositionVehicleAndDriver();

	if (PROBE_On()) {
		static int probe_throttle = 0;

		if (PROBE_Throttle(&probe_throttle, 1, 100)) {
			PROBE("turret:delta",
			      "ent=%d dyaw=%.4f dpitch=%.4f angles=(%.2f,%.2f,%.2f) restr=%d",
			      entnum, apply_yaw, apply_pitch,
			      angles[0], angles[1], angles[2], apply_restrict);
		}
	}

}





qboolean StationaryVehicle::Drive(usercmd_t* ucmd)
{
	Vector i, j, k;

	if ( !driver || !driver->isClient() )
	{
		return false;
	}

	if ( !drivable )
	{
		driver->client->ps.pm_flags |= PMF_FROZEN;
		ucmd->forwardmove = 0.0f;
		ucmd->rightmove   = 0.0f;
		return false;
	}

	if(_noPrediction )
	{
		driver->client->ps.pm_flags |= PMF_NO_PREDICTION;
	}

	turnimpulse		=	angledist( SHORT2ANGLE( ucmd->angles[ YAW ] )   - driver->client->cmd_angles[ YAW ] );
	pitchimpulse	=	angledist( SHORT2ANGLE( ucmd->angles[ PITCH ] ) - driver->client->cmd_angles[ PITCH ] );

	// A server frame consumes every usercmd the client produced, but only one Postthink runs
	// in it, so a last-wins store would discard all but the final cmd's look delta.
	_yawDeltaDegrees	+= SHORT2ANGLE(ucmd->deltaAngles[ YAW ] );
	_pitchDeltaDegrees	+= SHORT2ANGLE(ucmd->deltaAngles[ PITCH ] );

	return qtrue;
}



void StationaryVehicle::PositionVehicleAndDriver(void)
{	
	if(driver == 0)
		return;

	if(!driver->isSubclassOf(Player))
		return;

	Vector i,j,k;
	i = Vector( orientation[ 0 ] );
	j = Vector( orientation[ 1 ] );
	k = Vector( orientation[ 2 ] );

	Player * player;
	player = ( Player * )( Sentient * )driver;
	player->setOrigin( origin + ( i * driveroffset[PITCH] ) + ( j * driveroffset[YAW] ) + ( k * driveroffset[ROLL] ) );

	if(!drivable)
		return;

	player->velocity = vec_zero;

	angles[PITCH] += _pitchDeltaDegrees;
	angles[PITCH] = AngleNormalizeArbitrary( angles[PITCH], _pitchSeam);
	if( _restrictPitch )
	{
		if(angles[PITCH] < _minimumPitch)
		{
			angles[PITCH] = _minimumPitch;
		}

		if(angles[PITCH] > _maximumPitch)
		{
			angles[PITCH] = _maximumPitch;
		}
	}

	angles[YAW] += _yawDeltaDegrees;
	angles[YAW] = AngleNormalizeArbitrary( angles[YAW], _yawSeam);
	if( _restrictYaw )
	{

		if( angles[ YAW ] > _maximumYaw )
		{
			angles[ YAW ] = _maximumYaw;
		}

		if( angles[ YAW ] < _minimumYaw )
		{
			angles[ YAW ] = _minimumYaw;
		}
	}

	player->SetViewAngles(angles);

	// Applied: clear so the next frame sums only the cmds it actually consumed.
	_pitchDeltaDegrees = 0.0f;
	_yawDeltaDegrees   = 0.0f;
}


