//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/doAttack.cpp                               $
// $Revision:: 11                                                             $
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
// DoAttack Behavior Implementation
//	-- DoAttack will rotate the actor to face his current enemy, after the
//	   the rotation( with optional animation ) has completed, it will check
//	   if the actor is able to hit its current enemy with an attack.  If that
//	   check is successful, then it will play its specified attack animation.
//
//	   Currently, you must put the attack commands inside the animation.
//
//	ANIMATIONS:
//		Attack Animation   : Parameter
//		Rotation Animation : Optional 
//--------------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "actor.h"
#include "doAttack.hpp"
#include "../qcommon/gameplaymanager.h"

CLASS_DECLARATION( Behavior, DoAttack, NULL )
	{
		{ &EV_Behavior_Args,		&DoAttack::SetArgs	},
		{ &EV_Behavior_AnimDone,	&DoAttack::AnimDone	}, 
		{ NULL, NULL }
	};



void DoAttack::SetArgs( Event *ev )
{
	_anim = ev->GetString( 1 );
	if ( ev->NumArgs() > 1 )
		_turnspeed = ev->GetFloat( 2 );
   else
		_turnspeed = 30.0f;

   if ( ev->NumArgs() > 2 )
		_forceAttack = ev->GetBoolean( 3 );
   else
		_forceAttack = false;

   if ( ev->NumArgs() > 3 )
		_rotateAnim = ev->GetString( 4 );
   else
		_rotateAnim = "";
}



void DoAttack::AnimDone( Event *ev )
{
	_state = ATTACK_STATE_COMPLETE;
}



void DoAttack::Begin( Actor &self )
{       	
	init ( self );
	_setupRotate( self );
	_state = ATTACK_STATE_ROTATE;   
}



BehaviorReturnCode_t	DoAttack::Evaluate( Actor &self )
{
	if ( _state != ATTACK_STATE_ANIMATING )
		_rotate( self );

	if ( _state == ATTACK_STATE_START_ANIM )
		{
		_playAttackAnim( self );
		_state = ATTACK_STATE_ANIMATING;
		}

	if ( _state == ATTACK_STATE_FAILED )
		return BEHAVIOR_FAILED;

	if ( _state == ATTACK_STATE_COMPLETE )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;   
}



void DoAttack::End(Actor &self)
{ 
	_rotateBehavior.End(self);
}




void DoAttack::_setupRotate( Actor &self )
{
	Entity *currentEnemy;
	currentEnemy = self.enemyManager->GetCurrentEnemy();
	
	if ( currentEnemy )
		{
		 _rotateBehavior.SetEntity( currentEnemy );
		_rotateBehavior.SetTurnSpeed( _turnspeed ); 
		if ( _rotateAnim.length() )
			_rotateBehavior.SetAnim( _rotateAnim );

		_rotateBehavior.Begin( self );	 
		}         
}



void DoAttack::_rotate( Actor &self )
{  
	BehaviorReturnCode_t result;

	result = _rotateBehavior.Evaluate( self );

	if ( result == BEHAVIOR_FAILED )
		{
		_setupRotate( self );
		result = _rotateBehavior.Evaluate( self );

		if ( result == BEHAVIOR_FAILED )
			{
			str failureReason;
			failureReason = "Rotate Component Failed: Returned -- ";
			failureReason += _rotateBehavior.GetFailureReason();
			SetFailureReason( failureReason );
			_state = ATTACK_STATE_FAILED;
			}

		}

	if ( result == BEHAVIOR_SUCCESS )
		{
		if ( _canAttack( self ) )
			{
			if ( _state != ATTACK_STATE_ANIMATING && _state != ATTACK_STATE_COMPLETE )
				{
				_state = ATTACK_STATE_START_ANIM;
				return;
				}		
			}
		
	else
		_state = ATTACK_STATE_FAILED;
		
	}

}



void DoAttack::_playAttackAnim( Actor &self )
{
	self.ClearTorsoAnim();
	self.SetAnim( _anim , EV_Actor_NotifyBehavior);
	_state = ATTACK_STATE_ANIMATING;
}



bool DoAttack::_canAttack( Actor &self )
	{  
	Entity *currentEnemy;
	currentEnemy = self.enemyManager->GetCurrentEnemy();

	if ( _forceAttack )
		return true;

	if ( !currentEnemy)
		return false;
   
	if ( self.combatSubsystem->CanAttackTarget( currentEnemy ) )
		return true;
	else
		return false;
	}

void DoAttack::init( Actor &self )
{
	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	if ( !gpm->hasObject(self.getArchetype()) )
		return;

	if ( !self.combatSubsystem->HaveWeapon() )
		return;

	str objname = self.combatSubsystem->GetActiveWeaponArchetype();	
	objname = "Hold" + objname;

	if ( gpm->hasProperty(objname, "Melee") )
		_anim = gpm->getStringValue( objname , "Melee" );
	else
		{
		str weaponName = self.combatSubsystem->GetActiveWeaponName();
	
		if ( !weaponName.length() )
			{
			weaponName = "UNSPECIFIED";
			}

		gi.WDPrintf( "\n\n\nDoAttack: TargetName %s ( Entnum %i ) has a weapon, but does not have a MeleeProperty ( Trying Weapon: %s ) in the GPD,  Please notify a programmer of this error\n\n\n" , self.TargetName() , self.entnum, weaponName.c_str() );
		}
	
}
