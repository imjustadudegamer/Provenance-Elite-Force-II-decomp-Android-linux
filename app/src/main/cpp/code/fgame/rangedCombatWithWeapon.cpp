
#include "actor_posturecontroller.h"
#include "actor.h"
#include "rangedCombatWithWeapon.hpp"

extern Event EV_Actor_UseWeapon;
extern Event EV_PostureChanged_Completed;

CLASS_DECLARATION( Behavior, RangedCombatWithWeapon, NULL )
	{
		{ &EV_Behavior_Args,			&RangedCombatWithWeapon::SetArgs		},
		{ &EV_Behavior_AnimDone,		&RangedCombatWithWeapon::AnimDone		},
		{ &EV_PostureChanged_Completed, &RangedCombatWithWeapon::PostureDone	},
		{ NULL,							NULL		}
	};


RangedCombatWithWeapon::RangedCombatWithWeapon()
{
	_approachDist			= 384.00f;
	_retreatDist			= 256.00f;
	_strafeChance			=   0.85f;
	_postureChangeChance	=   0.15f;
}

RangedCombatWithWeapon::~RangedCombatWithWeapon()
{
}

void RangedCombatWithWeapon::SetArgs ( Event *ev)
{
	int argCount = ev->NumArgs();

	_aimAnim = ev->GetString( 1 );
	_fireAnim = ev->GetString( 2 );
	
	if ( argCount > 2 )
		_approachDist = ev->GetFloat( 3 );

	if ( argCount > 3 )
		_retreatDist = ev->GetFloat( 4 );

	if ( argCount > 4 )
		_strafeChance = ev->GetFloat( 5 );

	if ( argCount > 5 )
		_postureChangeChance = ev->GetFloat( 6 );

}


void RangedCombatWithWeapon::AnimDone( Event *ev )
{
	switch ( _state )
		{
		case RCWW_CORRIDOR_COMBAT:
			_corridorCombat.AnimDone( ev );
		break;

		case RCWW_GENERAL_COMBAT:
			_generalCombat.AnimDone( ev );
		break;

		case RCWW_COVER_COMBAT:
			_coverCombat.AnimDone( ev );
		break;

		case RCWW_CHANGE_WEAPON:			
			_selectBestWeapon.AnimDone( ev );			
		break;
		}
}

void RangedCombatWithWeapon::PostureDone( Event *ev )
{
	switch ( _state )
		{
		case RCWW_CORRIDOR_COMBAT:
			_corridorCombat.PostureDone( ev );
		break;

		case RCWW_GENERAL_COMBAT:
			_generalCombat.PostureDone( ev );
		break;
		}
}
void RangedCombatWithWeapon::Begin( Actor &self )
{
	init( self );
	updateEnemy();
	transitionToState( RCWW_CHANGE_WEAPON );	
}


BehaviorReturnCode_t RangedCombatWithWeapon::Evaluate ( Actor &self )
{	
	BehaviorReturnCode_t stateResult;
	think();
	
	switch ( _state )
	{		
	case RCWW_EVALUATE_OPTIONS:
		if ( checkShouldCheckWeapon() )
			{
			transitionToState( RCWW_CHANGE_WEAPON );
			return BEHAVIOR_EVALUATING;
			}

		if ( checkShouldDoCorridorCombat() )
			{
			transitionToState( RCWW_CORRIDOR_COMBAT );
			return BEHAVIOR_EVALUATING;
			}

		if ( checkShouldDoCoverCombat() )
			{
			transitionToState( RCWW_COVER_COMBAT );
			return BEHAVIOR_EVALUATING;
			}

		_recheckTime = level.time + G_Random() + 3.0f;
		transitionToState( RCWW_GENERAL_COMBAT );

	break;

	case RCWW_CHANGE_WEAPON:
		stateResult = evaluateStateChangeWeapon();		
		
		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( RCWW_FAILED );

		if ( stateResult == BEHAVIOR_SUCCESS )
			transitionToState( RCWW_EVALUATE_OPTIONS );

	break;

	case RCWW_GENERAL_COMBAT:
		if ( checkShouldCheckWeapon() )
			{
			transitionToState( RCWW_CHANGE_WEAPON );
			return BEHAVIOR_EVALUATING;
			}

		if ( level.time >= _recheckTime )
			{
			transitionToState( RCWW_EVALUATE_OPTIONS );
			return BEHAVIOR_EVALUATING;
			}

		stateResult = evaluateStateGeneralCombat();	

		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( RCWW_FAILED );
	break;

	case RCWW_CORRIDOR_COMBAT:
		if ( checkShouldCheckWeapon() )
			{
			transitionToState( RCWW_CHANGE_WEAPON );
			return BEHAVIOR_EVALUATING;
			}

		stateResult = evaluateStateCorridorCombat();

		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( RCWW_GENERAL_COMBAT );
	break;

	case RCWW_COVER_COMBAT:


		stateResult = evaluateStateCoverCombat();

		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( RCWW_GENERAL_COMBAT );
	break;

	case RCWW_FAILED:
		return BEHAVIOR_FAILED;
	break;
	}

	return BEHAVIOR_EVALUATING;	
}


void RangedCombatWithWeapon::End ( Actor &self	)
{
	_generalCombat.End( self );
	_corridorCombat.End( self );
	_coverCombat.End( self );
	self.postureController->setPostureState( "STAND" , "STAND" );	
}



void RangedCombatWithWeapon::init(Actor &self)
{
	_self = &self;
	_nextSelectWeaponTime = 0.0f;
	_recheckTime = -1.0f;
}

void RangedCombatWithWeapon::think()
{
}

void RangedCombatWithWeapon::updateEnemy()
{
	_currentEnemy = _self->enemyManager->GetCurrentEnemy();
	if ( !_currentEnemy )
		{
		_self->enemyManager->FindHighestHateEnemy();
		_currentEnemy = _self->enemyManager->GetCurrentEnemy();
		if ( !_currentEnemy )
			{
			SetFailureReason( "RangedCombatWithWeapon::updateEnemy -- No Enemy" );
			transitionToState( RCWW_FAILED );
			}
			
		}
}

void RangedCombatWithWeapon::transitionToState( RangedCombatWithWeapon_t state )
{
	switch ( state )
	{	
	case RCWW_EVALUATE_OPTIONS:
		setupStateEvaluateOptions();
		setInternalState( state , "RCWW_EVALUATE_OPTIONS" );
	break;

	case RCWW_CHANGE_WEAPON:
		setupStateChangeWeapon();
		setInternalState( state , "RCWW_CHANGE_WEAPON" );
	break;

	case RCWW_GENERAL_COMBAT:
		setupStateGeneralCombat();
		setInternalState( state , "RCWW_GENERAL_COMBAT" );
	break;
	
	case RCWW_CORRIDOR_COMBAT:
		setupStateCorridorCombat();
		setInternalState( state , "RCWW_CORRIDOR_COMBAT" );
	break;

	case RCWW_COVER_COMBAT:
		setupStateCoverCombat();
		setInternalState( state , "RCWW_COVER_COMBAT" );
	break;

	case RCWW_FAILED:
		setInternalState( state , "RCWW_FAILED" );
	break;
	}
}


void RangedCombatWithWeapon::setInternalState( RangedCombatWithWeapon_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );
}

void RangedCombatWithWeapon::setupStateChangeWeapon()
{
	_nextSelectWeaponTime = level.time + G_Random() + 5.0f;
	_selectBestWeapon.SetCurrentEnemy( _currentEnemy );
	_selectBestWeapon.Begin( *_self );
}

BehaviorReturnCode_t RangedCombatWithWeapon::evaluateStateChangeWeapon()
{
	return _selectBestWeapon.Evaluate( *_self );
}

void RangedCombatWithWeapon::failureStateChangeWeapon()
{
}

void RangedCombatWithWeapon::setupStateEvaluateOptions()
{
}

BehaviorReturnCode_t RangedCombatWithWeapon::evaluateStateEvaluateOptions()
{
	return BEHAVIOR_EVALUATING;
}

void RangedCombatWithWeapon::failureStateEvaluateOptions()
{
}

void RangedCombatWithWeapon::setupStateGeneralCombat()
{
	_generalCombat.SetTorsoAnim( _aimAnim );	
	_generalCombat.SetFireAnim( _fireAnim );
	_generalCombat.SetApproachDist( _approachDist );
	_generalCombat.SetRetreatDist ( _retreatDist );
	_generalCombat.SetStrafeChance ( _strafeChance );
	_generalCombat.SetPostureChangeChance ( _postureChangeChance );
	_generalCombat.SetFireTimeMin( 2.0f );
	_generalCombat.SetFireTimeMax( 2.5f );
	_generalCombat.SetPauseTimeMin( 0.5f );
	_generalCombat.SetPauseTimeMax( 0.75f );
	
	_generalCombat.Begin( *_self );
}

BehaviorReturnCode_t RangedCombatWithWeapon::evaluateStateGeneralCombat()
{
	return _generalCombat.Evaluate( *_self );

}

void RangedCombatWithWeapon::failureStateGeneralCombat()
{
}


void RangedCombatWithWeapon::setupStateCorridorCombat()
{
	_corridorCombat.SetMovementAnim( "walk_rifle" );
	_corridorCombat.SetTorsoAnim( _aimAnim );
	_corridorCombat.SetFireAnim( _fireAnim );
	_corridorCombat.SetPreFireAnim( "" );
	_corridorCombat.SetPostFireAnim( "" );
	_corridorCombat.SetPostureChangeChance( .65 );
	_corridorCombat.SetMaxDistance( 128.0f );
	_corridorCombat.SetRetreatDistance( 96.0f );
	_corridorCombat.SetThreatDistance( 160.0f );
	_corridorCombat.SetFireTimeMin( 2.0f );
	_corridorCombat.SetFireTimeMax( 2.5f );
	_corridorCombat.SetPauseTimeMin( 0.5f );
	_corridorCombat.SetPauseTimeMax( 0.75f );
	
	_corridorCombat.Begin( *_self );
}

BehaviorReturnCode_t RangedCombatWithWeapon::evaluateStateCorridorCombat()
{
	return _corridorCombat.Evaluate( *_self );

}

void RangedCombatWithWeapon::failureStateCorridorCombat()
{
}

void RangedCombatWithWeapon::setupStateCoverCombat()
{
	_coverCombat.SetMovementAnim( "run" );
	_coverCombat.SetTorsoAnim( _aimAnim );
	_coverCombat.SetFireAnim( _fireAnim );
	_coverCombat.SetMaxDistance( 1024.0f );
	_coverCombat.SetFireTimeMin( 2.0f );
	_coverCombat.SetFireTimeMax( 2.5f );
	_coverCombat.SetPauseTimeMin( 0.5f );
	_coverCombat.SetPauseTimeMax( 0.75f );
	_coverCombat.Begin( *_self );
}

BehaviorReturnCode_t RangedCombatWithWeapon::evaluateStateCoverCombat()
{
	return _coverCombat.Evaluate( *_self );
}

void RangedCombatWithWeapon::failureStateCoverCombat()
{
}

bool RangedCombatWithWeapon::checkShouldDoCorridorCombat()
{	
	if ( CorridorCombatWithRangedWeapon::CanExecute( 1024.0f , *_self ) )
		return true;

	return false;
}

bool RangedCombatWithWeapon::checkShouldDoGeneralCombat()
{
	return true;
}

bool RangedCombatWithWeapon::checkShouldCheckWeapon()
{
	if ( (level.time > _nextSelectWeaponTime) && SelectBestWeapon::CanExecute( *_self ) )
		{
		if ( _self->checkHaveBestWeapon() )
			{
			_nextSelectWeaponTime = level.time + G_Random() + 5.0f;
			return false;
			}

		return true;
		}	

	return false;
}

bool RangedCombatWithWeapon::checkShouldDoCoverCombat()
{
	return CoverCombatWithRangedWeapon::CanExecute( *_self , 1024.0f );
		
}

