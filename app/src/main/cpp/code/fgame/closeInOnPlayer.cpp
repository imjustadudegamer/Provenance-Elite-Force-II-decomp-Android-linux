
#include "actor.h"
#include "closeInOnPlayer.hpp"

CLASS_DECLARATION( Behavior, CloseInOnPlayer, NULL )
	{
		{ &EV_Behavior_Args,			&CloseInOnPlayer::SetArgs },      
		{ NULL, NULL }
	};


CloseInOnPlayer::CloseInOnPlayer()
{
	_anim = "";
	_torsoAnim = "";
	_dist = 64.0f;
}

CloseInOnPlayer::~CloseInOnPlayer()
{
}


void CloseInOnPlayer::SetArgs( Event *ev )
{
	_anim = ev->GetString( 1 );

	if ( ev->NumArgs() > 1 )
		_dist = ev->GetFloat( 2 );
 
	if ( ev->NumArgs() > 2 )
		_torsoAnim = ev->GetString( 3 );
}



void CloseInOnPlayer::Begin( Actor &self )
{          
	init( self );
}



BehaviorReturnCode_t	CloseInOnPlayer::Evaluate( Actor &self )
{

	BehaviorReturnCode_t stateResult;


	think();

	switch ( _state )
		{
		case CLOSE_IN_ON_PLAYER_APPROACH:
			stateResult = evaluateStateApproach();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( CLOSE_IN_ON_PLAYER_FAILED );

			if ( stateResult == BEHAVIOR_FAILED_STEERING_NO_PATH )
				transitionToState( CLOSE_IN_ON_PLAYER_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( CLOSE_IN_ON_PLAYER_SUCCESS );
		break;

		case CLOSE_IN_ON_PLAYER_SUCCESS:
			return BEHAVIOR_SUCCESS;
		break;

		case CLOSE_IN_ON_PLAYER_FAILED:
			return BEHAVIOR_FAILED;
		break;
		}
	
	return BEHAVIOR_EVALUATING;
}



void CloseInOnPlayer::End(Actor &self)
{   
	_chase.End( self );
}


void CloseInOnPlayer::transitionToState( closeInOnPlayerStates_t state )
{
	switch( state )
		{
		case CLOSE_IN_ON_PLAYER_APPROACH:
			setupStateApproach();
			setInternalState( state , "CLOSE_IN_ON_PLAYER_APPROACH" );
		break;

		case CLOSE_IN_ON_PLAYER_SUCCESS:
			setInternalState( state , "CLOSE_IN_ON_PLAYER_SUCCESS" );
		break;

		case CLOSE_IN_ON_PLAYER_FAILED:
			setInternalState( state , "CLOSE_IN_ON_PLAYER_FAILED" );
		break;
		}
}


void CloseInOnPlayer::setInternalState( closeInOnPlayerStates_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );	
}

void CloseInOnPlayer::init( Actor &self )
{
	_self = &self;
	findPlayer();	
	transitionToState(CLOSE_IN_ON_PLAYER_APPROACH);
}

void CloseInOnPlayer::think()
{	
}


void CloseInOnPlayer::findPlayer()
{
	Entity *player = (Entity*)GetPlayer(0);

	if ( !player || player->flags & FL_NOTARGET )
		{
		SetFailureReason( "CloseInOnPlayer::findPlayer -- No Player" );
		transitionToState( CLOSE_IN_ON_PLAYER_FAILED );
		return;
		}

	_player = player;

}

void CloseInOnPlayer::setTorsoAnim()
{
	_self->SetAnim( _torsoAnim , NULL , torso );
}

void CloseInOnPlayer::setupStateApproach()
{
   _chase.SetAnim( _anim );
   _chase.SetDistance( _dist );   
   _chase.SetEntity( *_self, _player );

   if ( _torsoAnim.length() > 0 )
	   setTorsoAnim();

   _chase.Begin( *_self );
}

BehaviorReturnCode_t CloseInOnPlayer::evaluateStateApproach()
{
	return _chase.Evaluate( *_self );
}

void CloseInOnPlayer::failureStateApproach( const str& failureReason )
{
}
