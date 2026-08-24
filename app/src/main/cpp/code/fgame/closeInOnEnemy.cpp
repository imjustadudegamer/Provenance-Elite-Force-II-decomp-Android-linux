
#include "actor.h"
#include "closeInOnEnemy.hpp"

CLASS_DECLARATION( Behavior, CloseInOnEnemy, NULL )
	{
		{ &EV_Behavior_Args,			&CloseInOnEnemy::SetArgs },      
		{ NULL, NULL }
	};


CloseInOnEnemy::CloseInOnEnemy()
{
	_anim = "";
	_torsoAnim = "";
	_dist = 64.0f;
}

CloseInOnEnemy::~CloseInOnEnemy()
{
}


void CloseInOnEnemy::SetArgs( Event *ev )
{
	_anim = ev->GetString( 1 );

	if ( ev->NumArgs() > 1 )
		_dist = ev->GetFloat( 2 );
 
	if ( ev->NumArgs() > 2 )
		_torsoAnim = ev->GetString( 3 );
}



void CloseInOnEnemy::Begin( Actor &self )
{          
	init( self );
}



BehaviorReturnCode_t	CloseInOnEnemy::Evaluate( Actor &self )
{

	BehaviorReturnCode_t stateResult;


	think();

	switch ( _state )
		{
		case CLOSE_IN_ON_ENEMY_APPROACH:
			stateResult = evaluateStateApproach();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( CLOSE_IN_ON_ENEMY_FAILED );

			if ( stateResult == BEHAVIOR_FAILED_STEERING_NO_PATH )
				transitionToState( CLOSE_IN_ON_ENEMY_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( CLOSE_IN_ON_ENEMY_SUCCESS );
		break;

		case CLOSE_IN_ON_ENEMY_SUCCESS:
			return BEHAVIOR_SUCCESS;
		break;

		case CLOSE_IN_ON_ENEMY_FAILED:
			return BEHAVIOR_FAILED;
		break;
		}
	
	return BEHAVIOR_EVALUATING;
}



void CloseInOnEnemy::End(Actor &self)
{   
	_chaseEnemy.End( self );
	self.SetAnim( "idle" );
}


void CloseInOnEnemy::transitionToState( closeInOnEnemyStates_t state )
{
	switch( state )
		{
		case CLOSE_IN_ON_ENEMY_APPROACH:
			setupStateApproach();
			setInternalState( state , "CLOSE_IN_ON_ENEMY_APPROACH" );
		break;

		case CLOSE_IN_ON_ENEMY_SUCCESS:
			setInternalState( state , "CLOSE_IN_ON_ENEMY_SUCCESS" );
		break;

		case CLOSE_IN_ON_ENEMY_FAILED:
			setInternalState( state , "CLOSE_IN_ON_ENEMY_FAILED" );
		break;
		}
}


void CloseInOnEnemy::setInternalState( closeInOnEnemyStates_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );	
}

void CloseInOnEnemy::init( Actor &self )
{
	_self = &self;
	updateEnemy();	
	transitionToState(CLOSE_IN_ON_ENEMY_APPROACH);
}

void CloseInOnEnemy::think()
{	
	if ( !_currentEnemy )
		{
		SetFailureReason( "CloseInOnEnemy::updateEnemy -- No Enemy" );
		transitionToState( CLOSE_IN_ON_ENEMY_FAILED );
		}
}


void CloseInOnEnemy::updateEnemy()
{
	Entity *currentEnemy = _self->enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		{
		_self->enemyManager->FindHighestHateEnemy();
		currentEnemy = _self->enemyManager->GetCurrentEnemy();
		if ( !currentEnemy )
			{
			SetFailureReason( "CloseInOnEnemy::updateEnemy -- No Enemy" );
			transitionToState( CLOSE_IN_ON_ENEMY_FAILED );
			}
			
		}

	_currentEnemy = currentEnemy;	
}

void CloseInOnEnemy::setTorsoAnim()
{
	_self->SetAnim( _torsoAnim , NULL , torso );
}

void CloseInOnEnemy::setupStateApproach()
{
   _chaseEnemy.SetAnim( _anim );
   _chaseEnemy.SetDistance( _dist );   
   _chaseEnemy.SetEntity( *_self, _currentEnemy );

   if ( _torsoAnim.length() > 0 )
	   setTorsoAnim();

   _chaseEnemy.Begin( *_self );
}

BehaviorReturnCode_t CloseInOnEnemy::evaluateStateApproach()
{
	return _chaseEnemy.Evaluate( *_self );
}

void CloseInOnEnemy::failureStateApproach( const str& failureReason )
{
}
