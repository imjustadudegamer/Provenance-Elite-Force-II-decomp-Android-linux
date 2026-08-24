
#include "actor.h"
#include "PlayAnim.hpp"

CLASS_DECLARATION( Behavior, PlayAnim, NULL )
	{
		{ &EV_Behavior_Args,			&PlayAnim::SetArgs		},   		
		{ NULL, NULL }
	};


PlayAnim::PlayAnim()
{
	_legAnim = "idle";
	_torsoAnim = "";
	_minTime = 0.0f;
	_maxTime = 0.0f;
	_endTime = 0.0f;
}

PlayAnim::~PlayAnim()
{
}


void PlayAnim::SetArgs( Event *ev )
{
	_legAnim = ev->GetString( 1 );
	if ( ev->NumArgs() > 1 ) _torsoAnim = ev->GetString( 2 );
	if ( ev->NumArgs() > 2 ) _minTime = ev->GetFloat( 3 );
	if ( ev->NumArgs() > 3 ) _maxTime = ev->GetFloat( 4 );	

}


void PlayAnim::Begin( Actor &self )
{          
	init( self );
}



BehaviorReturnCode_t	PlayAnim::Evaluate( Actor &self )
{

	BehaviorReturnCode_t stateResult;


	think();

	switch ( _state )
		{
		case PLAYANIM_SETUP:
			stateResult = evaluateStateSetup();

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( PLAYANIM_ANIMATE );
						
		break;

		case PLAYANIM_ANIMATE:
			stateResult = evaluateStateAnimate();

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( PLAYANIM_SUCCESS );
		break;
			
		case PLAYANIM_SUCCESS:
			return BEHAVIOR_SUCCESS;
		break;

		case PLAYANIM_FAILED:
			return BEHAVIOR_FAILED;
		break;
		}
	
	return BEHAVIOR_EVALUATING;
}



void PlayAnim::End(Actor &self)
{   
	self.RemoveAnimDoneEvent();
}


void PlayAnim::transitionToState( PlayAnimStates_t state )
{
	switch( state )
		{
		case PLAYANIM_SETUP:
			setInternalState( state , "PLAYANIM_SETUP" );
			setupStateSetup();
		break;

		case PLAYANIM_ANIMATE:
			setInternalState( state , "PLAYANIM_ANIMATE" );
			setupStateAnimate();
		break;

		case PLAYANIM_SUCCESS:
			setInternalState( state , "PLAYANIM_SUCCESS" );
		break;

		case PLAYANIM_FAILED:
			setInternalState( state , "PLAYANIM_FAILED" );
		break;
		}
}


void PlayAnim::setInternalState( PlayAnimStates_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );	
}

void PlayAnim::init( Actor &self )
{
	str legAnim, torsoAnim;
	ScriptVariable *var = NULL;

	legAnim = self.GetStateVar( _legAnim );
	if ( legAnim.length() )
		_legAnim = legAnim;

	
	torsoAnim = self.GetStateVar( _torsoAnim );
	if ( torsoAnim.length() )
		_torsoAnim = torsoAnim;


	var = self.entityVars.GetVariable( _legAnim );
	if ( var )
		{
		legAnim = var->stringValue();
		if ( legAnim.length() )
			_legAnim = legAnim;
		}


	var = self.entityVars.GetVariable( _torsoAnim );
	if ( var )
		{
		torsoAnim = var->stringValue();
		if ( torsoAnim.length() )
			_torsoAnim = torsoAnim;
		}


	transitionToState(PLAYANIM_SETUP);	
}

void PlayAnim::think()
{	
}


void PlayAnim::setupStateSetup()
{
	if ( _minTime )
		_endTime = level.time + _minTime;

	if ( _maxTime )
		_endTime = _endTime + G_Random( _maxTime - _minTime );

 	if ( _legAnim.length() )
		{
		if ( !_endTime )
			{
			if ( !GetSelf()->SetAnim( _legAnim, EV_Actor_EndBehavior ) )
				{
				GetSelf()->PostEvent( EV_Actor_EndBehavior, 0.0f );
				}
			}
		else
			{	
			if ( !GetSelf()->SetAnim( _legAnim, NULL ) )
				{
				GetSelf()->PostEvent( EV_Actor_EndBehavior, 0.0f );
				}
			}

		GetSelf()->ClearTorsoAnim();
		}

	if ( _torsoAnim.length() )
		{
		if ( !_endTime )
			{
			if ( !GetSelf()->SetAnim( _torsoAnim, EV_Actor_EndBehavior ) )
				{
				GetSelf()->PostEvent( EV_Actor_EndBehavior, 0.0f );
				}
			}
		else
			{	
			if ( !GetSelf()->SetAnim( _torsoAnim, NULL ) )
				{
				GetSelf()->PostEvent( EV_Actor_EndBehavior, 0.0f );
				}
			}
		}
}

BehaviorReturnCode_t PlayAnim::evaluateStateSetup()
{
	return BEHAVIOR_SUCCESS;
}

void PlayAnim::failureStateSetup( const str& failureReason )
{
}


void PlayAnim::setupStateAnimate()
{ 
}

BehaviorReturnCode_t PlayAnim::evaluateStateAnimate()
{
	if ( _endTime )
		{
		if ( level.time >= _endTime )
			return BEHAVIOR_SUCCESS;
		}
	

	return BEHAVIOR_EVALUATING;
}

void PlayAnim::failureStateAnimate( const str& failureReason )
{
}

