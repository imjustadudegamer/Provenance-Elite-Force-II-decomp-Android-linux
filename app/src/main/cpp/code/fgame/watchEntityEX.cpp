
#include "actor.h"
#include "watchEntityEX.hpp"
#include "../qcommon/gameplaymanager.h"

CLASS_DECLARATION( Behavior, WatchEntityEX, NULL )
{
	{ &EV_Behavior_Args,		&WatchEntityEX::SetArgs		},
	{ &EV_Behavior_AnimDone,	&WatchEntityEX::AnimDone	}, 
	{ NULL, NULL }
};


WatchEntityEX::WatchEntityEX()
{
	_stance			= "";
	_shuffleAnim	= "";
	_torsoAnim		= "";
	_ent			= NULL;
	_time			= 0.0;
	_turnspeed		= 0.0;	
}

WatchEntityEX::~WatchEntityEX()
{
}

void WatchEntityEX::SetArgs( Event *ev )
{
	int numargs = ev->NumArgs();

	if ( numargs > 0 ) _stance		= ev->GetString( 1 );
	if ( numargs > 1 ) _torsoAnim	= ev->GetString( 2 );
	if ( numargs > 2 ) _shuffleAnim	= ev->GetString( 3 );
	if ( numargs > 3 ) _turnspeed	= ev->GetFloat ( 4 );
	if ( numargs > 4 ) _time		= ev->GetFloat ( 5 );
}



void WatchEntityEX::AnimDone( Event *ev )
{
}



void WatchEntityEX::Begin( Actor &self )
{       
	init( self );
}


BehaviorReturnCode_t WatchEntityEX::Evaluate( Actor &self )
{  
	BehaviorReturnCode_t stateResult;

	think();
	
	switch ( _state )
		{
		case WATCH_ENTITY_EX_SETUP:
			stateResult = evaluateStateSetup();

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( WATCH_ENTITY_EX_ROTATE );

			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( WATCH_ENTITY_EX_FAILED );
		break;

		case WATCH_ENTITY_EX_ROTATE:
			stateResult = evaluateStateRotate();

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( WATCH_ENTITY_EX_HOLD );
			
			if ( stateResult == BEHAVIOR_FAILED)
				transitionToState( WATCH_ENTITY_EX_FAILED );
		break;


		case WATCH_ENTITY_EX_HOLD:
			stateResult = evaluateStateHold();

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( WATCH_ENTITY_EX_ROTATE );

			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( WATCH_ENTITY_EX_FAILED );
		break;


		case WATCH_ENTITY_EX_SUCCESS:
			return BEHAVIOR_SUCCESS;

		break;


		case WATCH_ENTITY_EX_FAILED:
			return BEHAVIOR_FAILED;

		break;


		}
	

	return BEHAVIOR_EVALUATING;   

}



void WatchEntityEX::End(Actor &self)
{   
}


void WatchEntityEX::init( Actor &self )
{	
	if ( _time > 0 )
		_time = level.time + _time;

	if ( !_ent )
		{
		self.enemyManager->FindHighestHateEnemy();
		_ent = self.enemyManager->GetCurrentEnemy();
		}

	_stance = self.GetStateVar( _stance );

	if ( !_stance.length() )
		_stance = "idle";

	transitionToState( WATCH_ENTITY_EX_SETUP );

}


void WatchEntityEX::transitionToState( watchEntityStates_t state )
{
	switch ( state )
		{	
		case WATCH_ENTITY_EX_SETUP:
			setupStateSetup();
			setInternalState( state , "WATCH_ENTITY_EX_SETUP" );
		break;

		case WATCH_ENTITY_EX_ROTATE:
			setupStateRotate();
			setInternalState( state , "WATCH_ENTITY_EX_ROTATE" );
		break;

		case WATCH_ENTITY_EX_HOLD:
			setupStateHold();
			setInternalState( state , "WATCH_ENTITY_EX_HOLD" );
		break;

		case WATCH_ENTITY_EX_SUCCESS:
			setInternalState( state , "WATCH_ENTITY_EX_SUCCESS" );
		break;

		case WATCH_ENTITY_EX_FAILED:
			setInternalState( state , "WATCH_ENTITY_EX_FAILED" );
		break;
		}
}

void WatchEntityEX::setInternalState( watchEntityStates_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );
}


void WatchEntityEX::think()
{
	if ( _time > 0 && level.time >= _time )
		transitionToState( WATCH_ENTITY_EX_SUCCESS );
}



void WatchEntityEX::setupStateSetup()
{	
}

BehaviorReturnCode_t WatchEntityEX::evaluateStateSetup()
{
	if ( !_ent )
		{
		failureStateSetup("No Entity To Rotate To" );
		return BEHAVIOR_FAILED;
		}

	if ( _torsoAnim.length() )
		GetSelf()->SetAnim( _torsoAnim , NULL , torso );

	return BEHAVIOR_SUCCESS;   
}

void WatchEntityEX::failureStateSetup( const str& failureReason )
{
}



void WatchEntityEX::setupStateRotate()
{
	_rotateToEntity.SetEntity( _ent );
	_rotateToEntity.SetAnim( _shuffleAnim );
	_rotateToEntity.SetTurnSpeed( _turnspeed );	

	_rotateToEntity.Begin( *GetSelf() );
}

BehaviorReturnCode_t WatchEntityEX::evaluateStateRotate()
{
	BehaviorReturnCode_t result;
	result = _rotateToEntity.Evaluate( *GetSelf() );
	
	if ( result == BEHAVIOR_SUCCESS )
		{
		_rotateToEntity.End( *GetSelf() );				
		return BEHAVIOR_SUCCESS;
		}
	
	if ( result != BEHAVIOR_EVALUATING )
		{
		failureStateRotate( "Rotate Component Failed" );
		return BEHAVIOR_FAILED;
		}

	return BEHAVIOR_EVALUATING;   
}

void WatchEntityEX::failureStateRotate( const str& failureReason )
{
}



void WatchEntityEX::setupStateHold()
{
	GetSelf()->SetAnim( _stance , NULL , legs );
}

BehaviorReturnCode_t WatchEntityEX::evaluateStateHold()
{
	Vector selfToEntity = _ent->centroid - GetSelf()->centroid;
	selfToEntity = selfToEntity.toAngles();	

	float yawDiff = selfToEntity[YAW] - GetSelf()->angles[YAW];

	if ( yawDiff > -5.5 && yawDiff < 5.5 )
		return BEHAVIOR_EVALUATING;

	//if Not facing enemy
	return BEHAVIOR_SUCCESS;   
}

void WatchEntityEX::failureStateHold( const str& failureReason )
{
}





