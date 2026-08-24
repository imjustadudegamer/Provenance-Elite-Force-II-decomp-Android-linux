
#include "stationaryvehicle.hpp"
#include "player.h"


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

	_yawDeltaDegrees	=  SHORT2ANGLE(ucmd->deltaAngles[ YAW ] );
	_pitchDeltaDegrees	= SHORT2ANGLE(ucmd->deltaAngles[ PITCH ] );

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
}


