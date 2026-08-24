//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/rotateToEntity.cpp                         $
// $Revision:: 12                                                             $
//   $Author:: Singlis                                                        $
//     $Date:: 9/26/03 2:36p                                                  $
//
// Copyright (C) 2002 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// RotateToEntity Behavior Implementation.
//	-- Rotates the actor to face an entity.  If no entity or entityType is specified, then it
//	   will rotate the actor to face its currentEnemy
//	
// PARAMETERS:
//		Optional turnspeed
//		Optional entityType to turn to, either "enemy" (default) or "player"
//
// ANIMATIONS:
//		Rotation Animation : Optional
//--------------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "actor.h"
#include "rotateToEntity.hpp"

CLASS_DECLARATION( Behavior, RotateToEntity, NULL )
	{
		{ &EV_Behavior_Args,			&RotateToEntity::SetArgs },
		{ NULL, NULL }
	};



RotateToEntity::RotateToEntity()
{
	_anim = "";
	_turnspeed = 10.0f;
	_ent = NULL;
	_entityType = "enemy";
}


RotateToEntity::~RotateToEntity()
{
}

void RotateToEntity::SetArgs( Event *ev )
	{

	int parmCount= ev->NumArgs();

	if ( parmCount > 0 ) 
		_turnspeed = ev->GetFloat( 1 );

	if ( parmCount > 1 ) 
		_anim = ev->GetString( 2 );

	if ( parmCount > 2 ) 
		_entityType = ev->GetString( 3 );

	}



void RotateToEntity::Begin( Actor &self )
	{       
	
	if ( !_ent )
		{
		if ( _entityType == "enemy" )
			{
			self.enemyManager->FindHighestHateEnemy();
			_ent = self.enemyManager->GetCurrentEnemy();
			}
		else
			{
			Entity *player = (Entity*)GetPlayer(0);

			if ( player && !(player->flags & FL_NOTARGET) )
				{
				_ent = player;
				}
			}
		}
  
	if ( _anim.length() && self.animate->HasAnim( _anim.c_str() ) )
		self.SetAnim( _anim );

	_oldTurnSpeed = self.movementSubsystem->getTurnSpeed();
	self.movementSubsystem->setTurnSpeed( _turnspeed );
	
	}


BehaviorReturnCode_t RotateToEntity::Evaluate( Actor &self )
	{
	Vector dir;
	Vector selfToEntity;
	
	float yawDiff;

	if ( !_ent )
		{
		SetFailureReason( "RotateToEntity: Has no Entity to Rotate To" );
		return BEHAVIOR_FAILED;
		}

	Vector entityPos;
	entityPos = _ent->centroid;
	entityPos.z = self.centroid.z;

	if ( self.IsEntityAlive( _ent ) )
		{		
		dir = self.movementSubsystem->getMoveDir();
		self.movementSubsystem->Accelerate( 
			self.movementSubsystem->SteerTowardsPoint( entityPos, vec_zero, dir, 1.0f)
			);
		}

	selfToEntity = entityPos - self.centroid;
	selfToEntity = selfToEntity.toAngles();	

	yawDiff = selfToEntity[YAW] - self.angles[YAW];

	yawDiff = AngleNormalize180(yawDiff);
	if ( yawDiff > -1.5 && yawDiff < 1.5 )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;

     
   }



void RotateToEntity::End(Actor &self)
{
	self.movementSubsystem->setTurnSpeed( _oldTurnSpeed );
}

