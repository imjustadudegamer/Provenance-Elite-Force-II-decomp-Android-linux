//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/selectBestWeapon.cpp              $
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
//	SelectBestWeapon Implementation
// 
// PARAMETERS:
//
// ANIMATIONS:
//
//--------------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "actor_posturecontroller.h"
#include "actor.h"
#include "selectBestWeapon.hpp"

CLASS_DECLARATION( Behavior, SelectBestWeapon, NULL )
	{
		{ &EV_Behavior_Args,			&SelectBestWeapon::SetArgs	},
		{ &EV_Behavior_AnimDone,		&SelectBestWeapon::AnimDone	}, 		
		{ NULL, NULL }
	};


SelectBestWeapon::SelectBestWeapon()
{
	_self = NULL;
}

SelectBestWeapon::~SelectBestWeapon()
{
}

void SelectBestWeapon::SetArgs( Event *ev )
{
}



void SelectBestWeapon::AnimDone( Event *ev )
{
	_animDone = true;
}


void SelectBestWeapon::Begin( Actor &self )
	{       
	init( self );
	}


BehaviorReturnCode_t SelectBestWeapon::Evaluate( Actor &self )
{
	BehaviorReturnCode_t stateResult;

	think();

	switch ( _state )
	{
	case SBW_SELECT_WEAPON:
		stateResult = evaluateStateSelectWeapon();

		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( SBW_FAILED );

		if ( stateResult == BEHAVIOR_SUCCESS )
			{	
			if ( _self->combatSubsystem->UsingWeaponNamed( _bestWeaponName ) )
				transitionToState( SBW_SUCCESS );								
			else
				transitionToState( SBW_PUT_AWAY_CURRENT_WEAPON );

			}

	break;

	case SBW_PUT_AWAY_CURRENT_WEAPON:
		stateResult = evaluateStatePutAwayCurrentWeapon();

		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( SBW_FAILED );

		if ( stateResult == BEHAVIOR_SUCCESS )
			transitionToState( SBW_PULL_OUT_NEW_WEAPON );

	break;

	case SBW_PULL_OUT_NEW_WEAPON:
		stateResult = evaluateStatePullOutNewWeapon();

		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( SBW_FAILED );


		if ( stateResult == BEHAVIOR_SUCCESS )
			transitionToState( SBW_SUCCESS );

	break;

	case SBW_READY_NEW_WEAPON:
		stateResult = evaluateStateReadyNewWeapon();

		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( SBW_FAILED );

		if ( stateResult == BEHAVIOR_SUCCESS )
			transitionToState( SBW_SUCCESS );

	break;

	case SBW_SUCCESS:
		return BEHAVIOR_SUCCESS;
	break;

	case SBW_FAILED:
		return BEHAVIOR_FAILED;
	break;
	}
	

	return BEHAVIOR_EVALUATING;   

}



void SelectBestWeapon::End(Actor &self)
{   
}



void SelectBestWeapon::transitionToState( selectBestWeaponStates_t state )
{
	
	switch ( state )
	{	
	case SBW_SELECT_WEAPON:
		setupStateSelectWeapon();
		setInternalState( state , "SBW_SELECT_WEAPON" );
	break;		
			
	case SBW_PUT_AWAY_CURRENT_WEAPON:
		setupStatePutAwayCurrentWeapon();
		setInternalState( state , "SBW_PUT_AWAY_CURRENT_WEAPON" );
	break;

	case SBW_PULL_OUT_NEW_WEAPON:
		setupStatePullOutNewWeapon();
		setInternalState( state , "SWB_PULL_OUT_NEW_WEAPON" );
	break;

	case SBW_READY_NEW_WEAPON:
		setupStateReadyNewWeapon();
		setInternalState( state , "SBW_READY_NEW_WEAPON" );
	break;

	case SBW_SUCCESS:
		setInternalState( state , "SBW_SUCCESS" );
	break;

	case SBW_FAILED:
		setInternalState( state , "SBW_FAILED" );
	break;
	}
	
}


void SelectBestWeapon::setInternalState( selectBestWeaponStates_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );
}

void SelectBestWeapon::init( Actor &self )
{
	_self = &self;	
	_animDone = false;
	transitionToState( SBW_SELECT_WEAPON );
	_currentWeaponName = self.combatSubsystem->GetActiveWeaponName();
	
}

void SelectBestWeapon::think()
{
	if ( !_bestWeapon )
		{
		failureStateSelectWeapon( "No Best Weapon" );
		transitionToState( SBW_SUCCESS );
		}
}

void SelectBestWeapon::setupStateSelectWeapon()
{
	if ( !_currentEnemy )
		{
		_self->enemyManager->FindHighestHateEnemy();
		_currentEnemy = _self->enemyManager->GetCurrentEnemy();
		}
}

BehaviorReturnCode_t SelectBestWeapon::evaluateStateSelectWeapon()
{
	str bestWeaponName;
	float powerRating;
	str currentPostureState;


	powerRating = _self->combatSubsystem->GetActiveWeaponPowerRating( _currentEnemy );
	if ( powerRating < .05 )
		{
		_self->InContext( "weaponuseless" , 0 );
		}


	_bestWeapon = _self->combatSubsystem->GetBestAvailableWeapon( _currentEnemy );

	if ( !_bestWeapon )
		{
		failureStateSelectWeapon( "No Best Weapon" );
		return BEHAVIOR_SUCCESS;
		}

	_bestWeaponName = _bestWeapon->getName();


	return BEHAVIOR_SUCCESS;
}

void SelectBestWeapon::failureStateSelectWeapon(const str& failureReason)
{
}


void SelectBestWeapon::setupStatePutAwayCurrentWeapon()
{
	str anim;
	anim = _currentWeaponName + "_putaway";
	_self->SetAnim( anim , EV_Actor_NotifyBehavior, torso );
	_animDone = false;
}

BehaviorReturnCode_t SelectBestWeapon::evaluateStatePutAwayCurrentWeapon()
{
	if ( _animDone )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void SelectBestWeapon::failureStatePutAwayCurrentWeapon(const str& failureReason)
{
}

void SelectBestWeapon::setupStatePullOutNewWeapon()
{
	str anim;
	anim = _bestWeaponName + "_draw";
	_self->SetAnim( anim , EV_Actor_NotifyBehavior , torso );
	_animDone = false;
}

BehaviorReturnCode_t SelectBestWeapon::evaluateStatePullOutNewWeapon()
{
	if ( _animDone )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void SelectBestWeapon::failureStatePullOutNewWeapon(const str& failureReason)
{
}

void SelectBestWeapon::setupStateReadyNewWeapon()
{
	str anim;
	anim = _bestWeaponName + "_idle";
	_self->SetAnim( anim , EV_Actor_NotifyBehavior , torso );
	_animDone = false;
}

BehaviorReturnCode_t SelectBestWeapon::evaluateStateReadyNewWeapon()
{
	if ( _animDone )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void SelectBestWeapon::failureStateReadyNewWeapon( const str &failureReason )
{
}

bool SelectBestWeapon::CanExecute( Actor &self )
{
	str currentPosture;
	currentPosture = self.postureController->getCurrentPostureName();

	if ( currentPosture == "STAND" )
		return true;

	return false;
}

