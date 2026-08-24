//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/coverCombatWithRangedWeapon.cpp            $
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
//	CoverCombatWithRangedWeapon Implementation
// 
// PARAMETERS:
//		str _movementAnim -- The animation to play while moving to the cover node
//
// ANIMATIONS:
//		_movementAnim : PARAMETER
//--------------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "actor.h"
#include "coverCombatWithRangedWeapon.hpp"
#include "actor_posturecontroller.h"

extern Event EV_PostureChanged_Completed;

const float CoverCombatWithRangedWeapon::NODE_RADIUS = 32.0f;


CLASS_DECLARATION( Behavior, CoverCombatWithRangedWeapon, NULL )
	{
		{ &EV_Behavior_Args,			&CoverCombatWithRangedWeapon::SetArgs		},
		{ &EV_Behavior_AnimDone,		&CoverCombatWithRangedWeapon::AnimDone		}, 
		{ &EV_PostureChanged_Completed, 	&CoverCombatWithRangedWeapon::PostureDone 	},
		{ NULL, NULL }
	};


CoverCombatWithRangedWeapon::CoverCombatWithRangedWeapon()
{	
	_self =  NULL;
}

CoverCombatWithRangedWeapon::~CoverCombatWithRangedWeapon()
{
}

void CoverCombatWithRangedWeapon::SetArgs( Event *ev )
{
	_torsoAnim	  = ev->GetString ( 1 );
	_fireAnim	  = ev->GetString ( 2 );
	_movementAnim = ev->GetString ( 3 );
	_maxDistance  = ev->GetFloat  ( 4 );
	_fireTimeMin  = ev->GetFloat  ( 5 );
	_fireTimeMax  = ev->GetFloat  ( 6 );
	_pauseTimeMin = ev->GetFloat  ( 7 );
	_pauseTimeMax = ev->GetFloat  ( 8 );

}



void CoverCombatWithRangedWeapon::AnimDone( Event *ev )
{
}


void CoverCombatWithRangedWeapon::PostureDone( Event *ev )
{
	_finishedPostureTransition = true;
}


void CoverCombatWithRangedWeapon::Begin( Actor &self )
{       
	init( self );
}


BehaviorReturnCode_t CoverCombatWithRangedWeapon::Evaluate( Actor &self )
{
	BehaviorReturnCode_t stateResult;


	think();

	switch ( _state )
	{
	case COVERCOMBAT_WRW_FIND_COVER:
		stateResult = evaluateStateFindCover();

		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( COVERCOMBAT_WRW_FAILED );

		if ( stateResult == BEHAVIOR_SUCCESS )
			transitionToState( COVERCOMBAT_WRW_MOVE_TO_COVER );
		
	break;

	case COVERCOMBAT_WRW_FIND_BETTER_COVER:
		stateResult = evaluateStateFindBetterCover();

		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( COVERCOMBAT_WRW_FAILED );

		if ( stateResult == BEHAVIOR_SUCCESS )
			transitionToState( COVERCOMBAT_WRW_MOVE_TO_COVER );
	break;

	case COVERCOMBAT_WRW_MOVE_TO_COVER:
		stateResult = evaluateStateMoveToCover();
		
		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( COVERCOMBAT_WRW_FAILED );

		if ( stateResult == BEHAVIOR_SUCCESS )
			transitionToState( COVERCOMBAT_WRW_CHANGE_POSTURE_DUCK );

	break;

	case COVERCOMBAT_WRW_CHANGE_POSTURE_DUCK:
		if ( _spotted )
			{
			transitionToState( COVERCOMBAT_WRW_SPOTTED );
			( void )BEHAVIOR_EVALUATING;
			}

		stateResult = evaluateStateChangePostureDuck();

		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( COVERCOMBAT_WRW_FAILED );

		if ( stateResult == BEHAVIOR_SUCCESS )
			transitionToState( COVERCOMBAT_WRW_DUCKED );
	break;

	case COVERCOMBAT_WRW_CHANGE_POSTURE_STAND:
		if ( _spotted )
			{
			transitionToState( COVERCOMBAT_WRW_SPOTTED );
			return BEHAVIOR_EVALUATING;
			}		
		
		stateResult = evaluateStateChangePostureStand();

		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( COVERCOMBAT_WRW_FAILED );

		if ( stateResult == BEHAVIOR_SUCCESS )
			transitionToState( COVERCOMBAT_WRW_STANDING );
		
	break;

	case COVERCOMBAT_WRW_FIRE_STANDING:
		if ( _spotted )
			{
			_fireWeapon.End( *_self );
			setTorsoAnim();
			transitionToState( COVERCOMBAT_WRW_SPOTTED );
			return BEHAVIOR_EVALUATING;
			}

		stateResult = evaluateRotate();
		stateResult = evaluateStateFireStanding();

		if ( stateResult == BEHAVIOR_FAILED )
			{
			transitionToState( COVERCOMBAT_WRW_FAILED );
			_fireWeapon.End( *_self );
			setTorsoAnim();
			return BEHAVIOR_EVALUATING;
			}

		if ( stateResult == BEHAVIOR_SUCCESS )
			{
			transitionToState( COVERCOMBAT_WRW_FIRE_PAUSE_STANDING );
			_fireWeapon.End( *_self );
			setTorsoAnim();
			return BEHAVIOR_EVALUATING;
			}

		stateResult = evaluateStateStanding();

		if ( stateResult == BEHAVIOR_FAILED )
			{
			transitionToState( COVERCOMBAT_WRW_FAILED );			
			_fireWeapon.End( *_self );
			setTorsoAnim();
			}

		if ( stateResult == BEHAVIOR_SUCCESS )
			{
			transitionToState( COVERCOMBAT_WRW_CHANGE_POSTURE_DUCK );
			_fireWeapon.End( *_self );
			setTorsoAnim();
			}
		
	break;

	case COVERCOMBAT_WRW_FIRE_PAUSE_STANDING:
		if ( _spotted )
			{
			transitionToState( COVERCOMBAT_WRW_SPOTTED );
			( void )BEHAVIOR_EVALUATING;
			}

		stateResult = evaluateRotate();
		stateResult = evaluateStateFirePauseStanding();
		if ( stateResult == BEHAVIOR_FAILED )
			{
			transitionToState( COVERCOMBAT_WRW_FAILED );			
			_fireWeapon.End( *_self );
			setTorsoAnim();
			return BEHAVIOR_EVALUATING;
			}

		if ( stateResult == BEHAVIOR_SUCCESS )
			{
			transitionToState(COVERCOMBAT_WRW_FIRE_STANDING);
			return BEHAVIOR_EVALUATING;
			}
			
		stateResult = evaluateStateStanding();

		if ( stateResult == BEHAVIOR_FAILED )
			{
			transitionToState( COVERCOMBAT_WRW_FAILED );			
			_fireWeapon.End( *_self );
			setTorsoAnim();
			}

		if ( stateResult == BEHAVIOR_SUCCESS )
			{
			transitionToState( COVERCOMBAT_WRW_CHANGE_POSTURE_DUCK );
			_fireWeapon.End( *_self );
			setTorsoAnim();
			}

	break;

	case COVERCOMBAT_WRW_DUCKED:
		if ( _spotted )
			{
			transitionToState( COVERCOMBAT_WRW_SPOTTED );
			( void )BEHAVIOR_EVALUATING;
			}

		stateResult = evaluateStateDucked();

		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( COVERCOMBAT_WRW_FAILED );

		if ( stateResult == BEHAVIOR_SUCCESS )
			transitionToState( COVERCOMBAT_WRW_CHANGE_POSTURE_STAND );
	break;

	case COVERCOMBAT_WRW_STANDING:
		if ( _spotted )
			{
			transitionToState( COVERCOMBAT_WRW_SPOTTED );
			( void )BEHAVIOR_EVALUATING;
			}

		stateResult = evaluateRotate();
		if ( stateResult == BEHAVIOR_SUCCESS )
			transitionToState( COVERCOMBAT_WRW_FIRE_STANDING );			
			

	break;

	case COVERCOMBAT_WRW_SPOTTED:
		stateResult = evaluateStateSpotted();

		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( COVERCOMBAT_WRW_FAILED );

		if ( stateResult == BEHAVIOR_SUCCESS )
			transitionToState( COVERCOMBAT_WRW_FIND_BETTER_COVER );
	break;

	case COVERCOMBAT_WRW_SUCCESS:
		return BEHAVIOR_SUCCESS;

	break;


	case COVERCOMBAT_WRW_FAILED:
		return BEHAVIOR_FAILED;

	break;
	}

	return BEHAVIOR_EVALUATING;   

}



void CoverCombatWithRangedWeapon::End(Actor &self)
{   
	if ( !_self )
		return;

	_self->movementSubsystem->setMovingBackwards( false );
	_fireWeapon.End(*_self);
}



void CoverCombatWithRangedWeapon::transitionToState( coverCombatStates_t state )
{
	switch ( state )
	{
	case COVERCOMBAT_WRW_FIND_COVER:
		setupStateFindCover();	
		setInternalState( state , "COVERCOMBAT_WRW_FIND_COVER" );
	break;

	case COVERCOMBAT_WRW_FIND_BETTER_COVER:
		setupStateFindBetterCover();	
		setInternalState( state , "COVERCOMBAT_WRW_FIND_BETTER_COVER" );
	break;

	case COVERCOMBAT_WRW_MOVE_TO_COVER:
		setupStateMoveToCover();
		setInternalState( state , "COVERCOMBAT_WRW_MOVE_TO_COVER" );
	break;

	case COVERCOMBAT_WRW_CHANGE_POSTURE_DUCK:
		setupStateChangePostureDuck();
		setInternalState( state , "COVERCOMBAT_WRW_CHANGE_POSTURE_DUCK" );
	break;

	case COVERCOMBAT_WRW_CHANGE_POSTURE_STAND:
		setupStateChangePostureStand();
		setInternalState( state , "COVERCOMBAT_WRW_CHANGE_POSTURE_DUCK" );
	break;

	case COVERCOMBAT_WRW_FIRE_STANDING:
		setupStateFireStanding();
		setInternalState( state , "COVERCOMBAT_WRW_FIRE_STANDING" );
	break;

	case COVERCOMBAT_WRW_FIRE_PAUSE_STANDING:
		setupStateFirePauseStanding();
		setInternalState( state , "COVERCOMBAT_WRW_FIRE_PAUSE_STANDING" );
	break;

	case COVERCOMBAT_WRW_DUCKED:
		setupStateDucked();
		setInternalState( state , "COVERCOMBAT_WRW_DUCKED" );
	break;

	case COVERCOMBAT_WRW_STANDING:
		setupStateStanding();
		setInternalState( state , "COVERCOMBAT_WRW_STANDING" );
	break;

	case COVERCOMBAT_WRW_SPOTTED:
		setupStateSpotted();
		setInternalState( state , "COVERCOMBAT_WRW_SPOTTED" );
	break;

	case COVERCOMBAT_WRW_SUCCESS:
		setInternalState( state , "COVERCOMBAT_WRW_SUCCESS" );
	break;

	case COVERCOMBAT_WRW_FAILED:
		setInternalState( state , "COVERCOMBAT_WRW_FAILED" );
	break;
	}
}


void CoverCombatWithRangedWeapon::setInternalState( coverCombatStates_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );
}

void CoverCombatWithRangedWeapon::init( Actor &self )
{
	_self = &self;
	transitionToState(COVERCOMBAT_WRW_FIND_COVER);
	_finishedPostureTransition = true;
	_nextStandTime = 0.0f;
	_nextDuckTime = 0.0f;
	_nextSpotCheck = 0.0f;	
	updateEnemy();	
}

void CoverCombatWithRangedWeapon::think()
{
	trace_t trace;

	if ( level.time < _nextSpotCheck )
		return;

	if ( !_currentEnemy || !_node )
		return;

	updateEnemy();
	trace = G_Trace( _node->origin , vec_zero , vec_zero , _currentEnemy->origin , NULL , MASK_SOLID, false, "CoverCombatWithRangedWeapon::think");

	if ( trace.fraction >= .95 )
		_spotted = true;
	else
		_spotted = false;

	_nextSpotCheck = level.time + G_Random(0.5);
	
}


void CoverCombatWithRangedWeapon::updateEnemy()
{
	Entity *currentEnemy = _self->enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		{
		_self->enemyManager->FindHighestHateEnemy();
		currentEnemy = _self->enemyManager->GetCurrentEnemy();
		if ( !currentEnemy )
			{
			SetFailureReason( "CoverCombatWithRangedWeapon::updateEnemy -- No Enemy" );
			transitionToState( COVERCOMBAT_WRW_FAILED );
			}
			
		}

	_currentEnemy = currentEnemy;
	setupRotate();
}

void CoverCombatWithRangedWeapon::setupStateFindCover()
{
}


BehaviorReturnCode_t CoverCombatWithRangedWeapon::evaluateStateFindCover()
{

	if ( !_node )
		{		
		failureStateFindCover( "CoverCombatWithRangedWeapon::evaluateStateFindCover() -- Cannot Find Suitable Cover Node" );
		return BEHAVIOR_FAILED;
		}

	return BEHAVIOR_SUCCESS;
}

void CoverCombatWithRangedWeapon::failureStateFindCover(const str& failureReason)
{
	SetFailureReason( failureReason );
}



void CoverCombatWithRangedWeapon::setupStateMoveToCover()
{
	_gotoPoint.SetAnim( _movementAnim );
	_gotoPoint.SetDistance( NODE_RADIUS );
	_gotoPoint.SetPoint( _node->origin );
	_gotoPoint.Begin( *_self );
}


BehaviorReturnCode_t CoverCombatWithRangedWeapon::evaluateStateMoveToCover()
{
	BehaviorReturnCode_t result = _gotoPoint.Evaluate( *_self );
	str failureReason = "CoverCombatWithRangedWeapon::evaluateStateMoveToCover -- _gotoPoint component returned: " + _gotoPoint.GetFailureReason();
	
	if ( result == BEHAVIOR_FAILED )
		failureStateMoveToCover( failureReason );

	return result;		
	
}


void CoverCombatWithRangedWeapon::failureStateMoveToCover( const str &failureReason )
{
}


void CoverCombatWithRangedWeapon::setupStateChangePostureDuck()
{
	bool canChange = _self->postureController->requestPosture( "DUCK" , this );

	if ( !canChange )
		{
		failureStateChangePostureDuck ( "Requested Posture is unavailable" );
		return;
		}

	_finishedPostureTransition = false;
}

BehaviorReturnCode_t CoverCombatWithRangedWeapon::evaluateStateChangePostureDuck()
{
	if ( _finishedPostureTransition )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;

}

void CoverCombatWithRangedWeapon::failureStateChangePostureDuck( const str &failureReason )
{
}

void CoverCombatWithRangedWeapon::setupStateChangePostureStand()
{
	bool canChange = _self->postureController->requestPosture( "STAND" , this );

	if ( !canChange )
		{
		failureStateChangePostureStand ( "Requested Posture is unavailable" );
		return;
		}

	_finishedPostureTransition = false;
}

BehaviorReturnCode_t CoverCombatWithRangedWeapon::evaluateStateChangePostureStand()
{
	if ( _finishedPostureTransition )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;

}

void CoverCombatWithRangedWeapon::failureStateChangePostureStand( const str &failureReason )
{
}

void CoverCombatWithRangedWeapon::setupStateSpotted()
{
	bool canChange = _self->postureController->requestPosture( "STAND" , this );

	if ( !canChange )
		{
		failureStateChangePostureStand ( "Requested Posture is unavailable" );
		return;
		}

	_finishedPostureTransition = false;
}

BehaviorReturnCode_t CoverCombatWithRangedWeapon::evaluateStateSpotted()
{
	if ( _finishedPostureTransition )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void CoverCombatWithRangedWeapon::failureStateSpotted( const str &failureReason )
{
}

void CoverCombatWithRangedWeapon::setupStateDucked()
{
	_nextStandTime = level.time + G_Random() + 3.0f;
}

BehaviorReturnCode_t CoverCombatWithRangedWeapon::evaluateStateDucked()
{
	if ( level.time > _nextStandTime )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void CoverCombatWithRangedWeapon::failureStateDucked( const str &failureReason )
{
}

void CoverCombatWithRangedWeapon::setupStateStanding()
{
	_nextDuckTime = level.time + G_Random() + 5.0f;
	_nextFireTime = 0.0f;
	_nextPauseTime = level.time + G_Random(_pauseTimeMax ) + _pauseTimeMin;
}

BehaviorReturnCode_t CoverCombatWithRangedWeapon::evaluateStateStanding()
{
	if ( level.time > _nextDuckTime )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void CoverCombatWithRangedWeapon::failureStateStanding( const str &failureReason )
{
}



void CoverCombatWithRangedWeapon::setupStateFindBetterCover()
{
}


BehaviorReturnCode_t CoverCombatWithRangedWeapon::evaluateStateFindBetterCover()
{

	if ( !_node )
		{		
		failureStateFindCover( "CoverCombatWithRangedWeapon::evaluateStateFindBetterCover() -- Cannot Find Suitable Cover Node" );
		return BEHAVIOR_FAILED;
		}

	return BEHAVIOR_SUCCESS;
}

void CoverCombatWithRangedWeapon::failureStateFindBetterCover(const str& failureReason)
{
	SetFailureReason( failureReason );
}

void CoverCombatWithRangedWeapon::setupRotate()
{
	_rotate.SetEntity( _currentEnemy ); 
	_rotate.Begin( *_self );
}


BehaviorReturnCode_t CoverCombatWithRangedWeapon::evaluateRotate()
{
	return _rotate.Evaluate( *_self );
}

void CoverCombatWithRangedWeapon::setupStateFireStanding()
{
	_nextFireTime = level.time + G_Random( _fireTimeMax ) + _fireTimeMin;
	if ( _self->combatSubsystem->CanAttackEnemy() )
		{
		_fireWeapon.SetAnim( _fireAnim ); 
		_fireWeapon.Begin( *_self );
		}
}

BehaviorReturnCode_t CoverCombatWithRangedWeapon::evaluateStateFireStanding()
{
	_fireWeapon.Evaluate( *_self );
	_self->combatSubsystem->AimWeaponTag(_currentEnemy);	

	if ( level.time > _nextFireTime )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void CoverCombatWithRangedWeapon::failureStateFireStanding( const str &failureReason )
{

}

void CoverCombatWithRangedWeapon::setupStateFirePauseStanding()
{
	_nextPauseTime = level.time + G_Random( _pauseTimeMax ) + _pauseTimeMin;
	setTorsoAnim();
}

BehaviorReturnCode_t CoverCombatWithRangedWeapon::evaluateStateFirePauseStanding()
{
	if ( level.time > _nextPauseTime )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void CoverCombatWithRangedWeapon::failureStateFirePauseStanding( const str &failureReason )
{
}


void CoverCombatWithRangedWeapon::setTorsoAnim()
{
	_self->SetAnim( _torsoAnim , NULL , torso );
}

bool CoverCombatWithRangedWeapon::CanExecute( Actor &self , float maxDistance )
{
	Entity* currentEnemy;

	currentEnemy = self.enemyManager->GetCurrentEnemy();

	if ( !currentEnemy )
		return false;
	


	return false;
}






