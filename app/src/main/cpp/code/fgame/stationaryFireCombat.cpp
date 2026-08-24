
#include "actor.h"
#include "stationaryFireCombat.hpp"

CLASS_DECLARATION( Behavior, StationaryFireCombat, NULL )
	{
		{ &EV_Behavior_Args,		&StationaryFireCombat::SetArgs	},
		{ &EV_Behavior_AnimDone,	&StationaryFireCombat::AnimDone	}, 
		{ NULL, NULL }
	};


StationaryFireCombat::StationaryFireCombat()
{	
	_endFireTime		= 0.0f;
	_endAimTime			= 0.0f;
}	

StationaryFireCombat::~StationaryFireCombat()
{
}

void StationaryFireCombat::SetArgs( Event *ev )
{		
	_aimAnim		= ev->GetString( 1 );
	_preFireAnim	= ev->GetString( 2 );
	_fireAnim		= ev->GetString( 3 );	
	_postFireAnim	= ev->GetString( 4 );
	_stance			= ev->GetString( 5 );
	_aimTimeMin		= ev->GetFloat( 6 );
	_aimTimeMax		= ev->GetFloat( 7 );
	_fireTimeMin	= ev->GetFloat( 8 );
	_fireTimeMax	= ev->GetFloat( 9 );	
}



void StationaryFireCombat::AnimDone( Event *ev )
{
	_animDone = true;
}



void StationaryFireCombat::Begin( Actor &self )
{       
	init( self );
	transitionToState ( STATIONARY_FIRE_AIM );
}


BehaviorReturnCode_t StationaryFireCombat::Evaluate( Actor &self )
{
	BehaviorReturnCode_t stateResult;

	think();
	
	switch ( _state )
		{
		case STATIONARY_FIRE_AIM:
			stateResult = evaluateStateAim();

			if ( stateResult == BEHAVIOR_SUCCESS )
				{
				updateEnemy();
				if ( _preFireAnim.length() )
					transitionToState( STATIONARY_FIRE_PRE_FIRE );
				else
					transitionToState( STATIONARY_FIRE_ATTACK );
				}
				
		break;

		case STATIONARY_FIRE_PRE_FIRE:
			stateResult = evaluateStatePreFire();

			if ( stateResult == BEHAVIOR_SUCCESS )
				{
					transitionToState( STATIONARY_FIRE_ATTACK );	
				}

			if ( stateResult == BEHAVIOR_FAILED )
				{
				transitionToState( STATIONARY_FIRE_FAILED );
				}
			
		break;

		case STATIONARY_FIRE_ATTACK:
			stateResult = evaluateStateAttack();

			if ( stateResult == BEHAVIOR_SUCCESS )
				{
				_fireWeapon.End( *_self );

				if ( _postFireAnim.length() )
					transitionToState( STATIONARY_FIRE_POST_FIRE );
				else
					transitionToState( STATIONARY_FIRE_SUCCESS );
				}

			if ( stateResult == BEHAVIOR_FAILED )
				{
				_fireWeapon.End( *_self );

				if ( _postFireAnim.length() )
					transitionToState( STATIONARY_FIRE_POST_FIRE );
				else
					transitionToState( STATIONARY_FIRE_SUCCESS );
				}

		break;


		case STATIONARY_FIRE_POST_FIRE:
			stateResult = evaluateStatePostFire();

			if ( stateResult == BEHAVIOR_SUCCESS )
				{
					transitionToState( STATIONARY_FIRE_SUCCESS );	
				}
		break;

		case STATIONARY_FIRE_SUCCESS:
			return BEHAVIOR_SUCCESS;

		break;


		case STATIONARY_FIRE_FAILED:
			return BEHAVIOR_FAILED;

		break;


		}
	

	return BEHAVIOR_EVALUATING;   

}



void StationaryFireCombat::End(Actor &self)
{ 
	if ( !_self )
		return;

	_fireWeapon.End(*_self);
	_self->SetAnim( _aimAnim , NULL , torso );

	_self->ClearTorsoAnim();
}



void StationaryFireCombat::transitionToState( StationaryFireStates_t state )
{	
	switch ( state )
		{	
		case STATIONARY_FIRE_AIM:
			setupStateAim();
			setInternalState( state , "STATIONARY_FIRE_AIM" );
		break;

		case STATIONARY_FIRE_PRE_FIRE:
			setupStatePreFire();
			setInternalState( state , "STATIONARY_FIRE_PRE_FIRE" );
		break;

		case STATIONARY_FIRE_ATTACK:
			setupStateAttack();
			setInternalState( state , "STATIONARY_FIRE_ATTACK" );
		break;

		case STATIONARY_FIRE_POST_FIRE:
			setupStatePostFire();
			setInternalState( state , "STATIONARY_FIRE_POST_FIRE" );
		break;

		case STATIONARY_FIRE_SUCCESS:
			setInternalState( state , "STATIONARY_FIRE_SUCCESS" );
		break;

		case STATIONARY_FIRE_FAILED:
			setInternalState( state , "SUPPRESSION_FIRE_FAILED" );
		break;
		}
	
}


void StationaryFireCombat::setInternalState( StationaryFireStates_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );
}

void StationaryFireCombat::init( Actor &self )
{
	_self = &self;	
	_animDone = false;
	_canAttack = true;
	updateEnemy();
}

void StationaryFireCombat::think()
{
}

void StationaryFireCombat::setupStateAttack()
{
	faceEnemy();
	_endFireTime = level.time + G_Random(_fireTimeMax - _fireTimeMin) + _fireTimeMin;

	if ( _self->combatSubsystem->CanAttackEnemy() )
		{	
		_fireWeapon.SetAnim( _fireAnim ); 
		_fireWeapon.Begin( *_self );
		}

}

BehaviorReturnCode_t StationaryFireCombat::evaluateStateAttack()
{
	BehaviorReturnCode_t result;
	_rotateToEntity.Evaluate( *_self );
	
	if ( _self->combatSubsystem->CanAttackEnemy() )
		result = _fireWeapon.Evaluate( *_self );
	else
		{
		_fireWeapon.End( *_self );
		result = BEHAVIOR_FAILED;
		}

	if ( result == BEHAVIOR_FAILED )
		failureStateAttack( "StationaryFireCombat::evaluateStateAttack -- FAILED" );

	if ( level.time > _endFireTime )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void StationaryFireCombat::failureStateAttack( const str &failureReason )
{
	SetFailureReason( failureReason );
	_fireWeapon.End( *_self );
}

void StationaryFireCombat::setupStateAim()
{
	faceEnemy();
	_endAimTime = level.time + G_Random(_aimTimeMax - _aimTimeMin ) + _aimTimeMin;
	_self->SetAnim( _aimAnim , NULL , torso );
	_self->SetAnim( _stance , NULL , legs );

}

BehaviorReturnCode_t StationaryFireCombat::evaluateStateAim()
{
	_rotateToEntity.Evaluate( *_self );
	if ( level.time > _endAimTime )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}


void StationaryFireCombat::failureStateAim( const str &failureReason )
{
	SetFailureReason( failureReason );
}


void StationaryFireCombat::setupStatePreFire()
{
	if ( _self->combatSubsystem->CanAttackEnemy() )
		{
		_animDone = false;
		_self->SetAnim( _preFireAnim , EV_Actor_NotifyBehavior , torso );
		}
	else
		_canAttack = false;
}

BehaviorReturnCode_t StationaryFireCombat::evaluateStatePreFire()
{
	if ( _animDone )
		return BEHAVIOR_SUCCESS;

	if ( !_canAttack )
		return BEHAVIOR_FAILED;

	return BEHAVIOR_EVALUATING;
}

void StationaryFireCombat::failureStatePreFire( const str &failureReason )
{
}

void StationaryFireCombat::setupStatePostFire()
{
	_animDone = false;
	_self->SetAnim( _postFireAnim , EV_Actor_NotifyBehavior , torso );

}

BehaviorReturnCode_t StationaryFireCombat::evaluateStatePostFire()
{
	if ( _animDone )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_FAILED;

}

void StationaryFireCombat::failureStatePostFire( const str &failureReason )
{
}

void StationaryFireCombat::updateEnemy()
{
	Entity *currentEnemy = _self->enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		{
		_self->enemyManager->FindHighestHateEnemy();
		currentEnemy = _self->enemyManager->GetCurrentEnemy();
		if ( !currentEnemy )
			{
			SetFailureReason( "STATIONARY_FIRE_FAILED::updateEnemy -- No Enemy" );
			transitionToState( STATIONARY_FIRE_FAILED );
			return;
			}
			
		}

	_currentEnemy = currentEnemy;
}

void StationaryFireCombat::faceEnemy()
{
	_rotateToEntity.SetEntity( _currentEnemy );
	_rotateToEntity.SetTurnSpeed( 30.0f );
	_rotateToEntity.Begin( *_self );	

}






















