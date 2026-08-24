//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/closeInOnEnemyWhileFiringWeapon.cpp              $
// $Revision:: 5                                                              $
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
//	CloseInOnEnemyWhileFiringWeapon Implementation
// 
// PARAMETERS:
//		
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

#include "actor.h"
#include "closeInOnEnemyWhileFiringWeapon.hpp"

CLASS_DECLARATION( Behavior, CloseInOnEnemyWhileFiringWeapon, NULL )
	{
		{ &EV_Behavior_Args,			&CloseInOnEnemyWhileFiringWeapon::SetArgs },      
		{ NULL, NULL }
	};


CloseInOnEnemyWhileFiringWeapon::CloseInOnEnemyWhileFiringWeapon()
{
	_approachAnim	= "";  
	_aimAnim		= "";
	_fireAnim		= "";
	_fireTimeMin	= 0.0f;
	_fireTimeMax	= 0.0f;
	_pauseTimeMin	= 0.0f;
	_pauseTimeMax	= 0.0f;
	_dist			= 0.0f;
	_nextFireTime	= 0.0f;
	_nextPauseTime	= 0.0f;
}

CloseInOnEnemyWhileFiringWeapon::~CloseInOnEnemyWhileFiringWeapon()
{
}


void CloseInOnEnemyWhileFiringWeapon::SetArgs( Event *ev )
{
	_approachAnim	= ev->GetString( 1 );  
	_aimAnim		= ev->GetString( 2 );
	_fireAnim		= ev->GetString( 3 );
	_fireTimeMin	= ev->GetFloat( 4 );
	_fireTimeMax	= ev->GetFloat( 5 );
	_pauseTimeMin	= ev->GetFloat( 6 );
	_pauseTimeMax	= ev->GetFloat( 7 );
	_dist			= ev->GetFloat( 8 );
}



void CloseInOnEnemyWhileFiringWeapon::Begin( Actor &self )
{          
	init( self );
}



BehaviorReturnCode_t CloseInOnEnemyWhileFiringWeapon::Evaluate( Actor &self )
{

	BehaviorReturnCode_t stateResult;

	think();

	switch ( _state )
		{
		case CIWF_APPROACH_SETUP_APPROACH:
			stateResult = evaluateStateSetupApproach();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( CIWF_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( CIWF_APPROACH_FIRE );
		break;

		case CIWF_APPROACH_FIRE:
			stateResult = evaluateStateApproachFire();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( CIWF_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( CIWF_APPROACH_FIRE_PAUSE );
		break;		

		case CIWF_APPROACH_FIRE_PAUSE:
			stateResult = evaluateStateApproachFirePause();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( CIWF_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( CIWF_APPROACH_FIRE );
		break;	

		case CIWF_SUCCESS:
			return BEHAVIOR_SUCCESS;
		break;

		case CIWF_FAILED:
			return BEHAVIOR_FAILED;
		break;
		}
	
	return BEHAVIOR_EVALUATING;
}



void CloseInOnEnemyWhileFiringWeapon::End(Actor &self)
{   
	_chaseEnemy.End( *_self );
	_fireWeapon.End( *_self );
}


void CloseInOnEnemyWhileFiringWeapon::transitionToState( CIWFStates_t state )
{
	switch( state )
		{
		case CIWF_APPROACH_SETUP_APPROACH:
			setupStateSetupApproach();
			setInternalState( state , "CIWF_APPROACH_SETUP_APPROACH" );
		break;

		case CIWF_APPROACH_FIRE:
			setupStateApproachFire();
			setInternalState( state , "CIWF_APPROACH_FIRE" );
		break;

		case CIWF_APPROACH_FIRE_PAUSE:
			setupStateApproachFirePause();
			setInternalState( state , "CIWF_APPROACH_FIRE_PAUSE" );
		break;

		case CIWF_SUCCESS:
			setInternalState( state , "CIWF_SUCCESS" );
		break;

		case CIWF_FAILED:
			setInternalState( state , "CIWF_FAILED" );
		break;
		}
}


void CloseInOnEnemyWhileFiringWeapon::setInternalState( CIWFStates_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );	
}

void CloseInOnEnemyWhileFiringWeapon::init( Actor &self )
{
	_self = &self;
	updateEnemy();	
	transitionToState(CIWF_APPROACH_SETUP_APPROACH);
}

void CloseInOnEnemyWhileFiringWeapon::think()
{	
}


void CloseInOnEnemyWhileFiringWeapon::updateEnemy()
{
	Entity *currentEnemy = _self->enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		{
		_self->enemyManager->FindHighestHateEnemy();
		currentEnemy = _self->enemyManager->GetCurrentEnemy();
		if ( !currentEnemy )
			{
			SetFailureReason( "CloseInOnEnemyWhileFiringWeapon::updateEnemy -- No Enemy" );
			transitionToState( CIWF_FAILED );
			}
			
		}

	_currentEnemy = currentEnemy;	
}

void CloseInOnEnemyWhileFiringWeapon::setTorsoAnim()
{
	_self->SetAnim( _aimAnim , NULL , torso );
}

void CloseInOnEnemyWhileFiringWeapon::setupStateSetupApproach()
{
   _chaseEnemy.SetAnim( _approachAnim );
   _chaseEnemy.SetDistance( _dist );   
   _chaseEnemy.SetEntity( *_self, _currentEnemy );

   setTorsoAnim();
   _chaseEnemy.Begin( *_self );
}

BehaviorReturnCode_t CloseInOnEnemyWhileFiringWeapon::evaluateStateSetupApproach()
{
	return BEHAVIOR_SUCCESS;
}

void CloseInOnEnemyWhileFiringWeapon::failureStateSetupApproach( const str& failureReason )
{
}

void CloseInOnEnemyWhileFiringWeapon::setupStateApproachFire()
{
	_nextFireTime = level.time + G_Random( _fireTimeMax ) + _fireTimeMin;
	_fireWeapon.SetAnim( _fireAnim ); 
	_fireWeapon.Begin( *_self );
}

BehaviorReturnCode_t CloseInOnEnemyWhileFiringWeapon::evaluateStateApproachFire()
{
	BehaviorReturnCode_t result;

	result = _chaseEnemy.Evaluate( *_self );

	if ( result != BEHAVIOR_SUCCESS &&
		 result != BEHAVIOR_EVALUATING
	   )
		{
		return BEHAVIOR_FAILED;
		}

	_fireWeapon.Evaluate( *_self );
	_self->combatSubsystem->AimWeaponTag(_currentEnemy);	
	
	if ( level.time > _nextFireTime )
		return BEHAVIOR_SUCCESS;	

	return BEHAVIOR_EVALUATING;
}

void CloseInOnEnemyWhileFiringWeapon::failureStateApproachFire( const str& failureReason )
{
}

void CloseInOnEnemyWhileFiringWeapon::setupStateApproachFirePause()
{
	_nextPauseTime = level.time + G_Random( _pauseTimeMax ) + _pauseTimeMin;
	_fireWeapon.End( *_self );
	setTorsoAnim();
}

BehaviorReturnCode_t CloseInOnEnemyWhileFiringWeapon::evaluateStateApproachFirePause()
{
	BehaviorReturnCode_t result;

	result = _chaseEnemy.Evaluate( *_self );

	if ( result != BEHAVIOR_SUCCESS &&
		 result != BEHAVIOR_EVALUATING
	   )
		{
		return BEHAVIOR_FAILED;
		}

	
	if ( level.time > _nextPauseTime )
		return BEHAVIOR_SUCCESS;	

	return BEHAVIOR_EVALUATING;
}

void CloseInOnEnemyWhileFiringWeapon::failureStateApproachFirePause( const str& failureReason )
{
}
