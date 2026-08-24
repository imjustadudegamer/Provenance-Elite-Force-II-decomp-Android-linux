
#include "actor.h"
#include "changePosture.hpp"
#include "actor_posturecontroller.h"

CLASS_DECLARATION( Behavior, ChangePosture, NULL )
	{
		{ &EV_Behavior_Args,		&ChangePosture::SetArgs},  
		{ &EV_PostureChanged_Completed, &ChangePosture::PostureDone },
		{ NULL, NULL }
	};


ChangePosture::ChangePosture()
{
	_posture = "";	
}

ChangePosture::~ChangePosture()
{
}


void ChangePosture::SetArgs( Event *ev )
{
	_posture = ev->GetString( 1 );
}

void ChangePosture::PostureDone( Event *ev )
{
	_postureDone = true;
}

void ChangePosture::Begin( Actor &self )
{          
	init();
}



BehaviorReturnCode_t	ChangePosture::Evaluate( Actor &self )
{

	BehaviorReturnCode_t stateResult;


	think();

	switch ( _state )
		{

		case CHANGE_POSTURE_SETUP:
			stateResult = evaluateStateSetup();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( CHANGE_POSTURE_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( CHANGE_POSTURE_CHANGE );
		break;

		case CHANGE_POSTURE_CHANGE:
			stateResult = evaluateStateChange();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( CHANGE_POSTURE_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( CHANGE_POSTURE_SUCCESS );
		break;

		case CHANGE_POSTURE_SUCCESS:
			return BEHAVIOR_SUCCESS;
		break;

		case CHANGE_POSTURE_FAILED:
			return BEHAVIOR_FAILED;
		break;
		}
	
	return BEHAVIOR_EVALUATING;
}



void ChangePosture::End(Actor &self)
{   
}


void ChangePosture::transitionToState( changePostureStates_t state )
{
	switch( state )
		{
		case CHANGE_POSTURE_SETUP:
			setupStateSetup();
			setInternalState( state , "CHANGE_POSTURE_SETUP" );
		break;

		case CHANGE_POSTURE_CHANGE:
			setupStateChange();
			setInternalState( state , "CHANGE_POSTURE_CHANGE" );
		break;

		case CHANGE_POSTURE_SUCCESS:
			setInternalState( state , "CHANGE_POSTURE_SUCCESS" );
		break;

		case CHANGE_POSTURE_FAILED:
			setInternalState( state , "CHANGE_POSTURE_FAILED" );
		break;
		}
}


void ChangePosture::setInternalState( changePostureStates_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );	
}

void ChangePosture::init()
{
	transitionToState(CHANGE_POSTURE_SETUP);
	_canChange = true;
	_postureDone = false;
}

void ChangePosture::think()
{	
}


void ChangePosture::setupStateSetup()
{
	_canChange = GetSelf()->postureController->requestPosture( _posture , this );
}

BehaviorReturnCode_t ChangePosture::evaluateStateSetup()
{
	if ( !_canChange )
		return BEHAVIOR_FAILED;

	return BEHAVIOR_SUCCESS;
}

void ChangePosture::failureStateSetup( const str& failureReason )
{
}

void ChangePosture::setupStateChange()
{
}

BehaviorReturnCode_t ChangePosture::evaluateStateChange()
{	
	if ( _postureDone )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void ChangePosture::failureStateChange( const str& failureReason )
{
}
