
#include "actor.h"
#include "player.h"
#include "work.hpp"

CLASS_DECLARATION( Behavior, Work, NULL )
{
	{ &EV_Behavior_Args,		&Work::SetArgs				},
	{ &EV_Behavior_AnimDone,	&Work::AnimDone				}, 
	{ &EV_HelperNodeCommand,	&Work::HandleNodeCommand	},

	{ NULL, NULL }
};

Work::Work()
{
	_gotoWorkAnimName = "walk";
	_maxDistance	  = 1024.0f;
}

Work::~Work()
{
}

void Work::SetArgs ( Event *ev)
{
	if (ev->NumArgs() == 1)
	{
		_gotoWorkAnimName = ev->GetString( 1 );
	}
}



void Work::AnimDone( Event *ev )
{
	switch( _state )
		{
		case WORK_ANIMATE_WAIT_ON_ANIM:
			_animDone = true;
		break;

		case WORK_ANIMATE_LIST_WAIT_ON_ANIM:
			_animDone = true;
		break;		
		}

}

void Work::HandleNodeCommand( Event *ev )
{
	switch( _state )
		{
		case WORK_ANIMATE_WAIT_ON_SIGNAL:
			_animDone = true;
		break;

		case WORK_ANIMATE_LIST_WAIT_ON_SIGNAL:
			_animDone = true;
		break;
		}
}

void Work::Begin( Actor &self )
{
	init( self );
}

BehaviorReturnCode_t	Work::Evaluate ( Actor &self )
{      
	BehaviorReturnCode_t stateResult;

	think();

	switch ( _state )
		{
		case WORK_FIND_NODE:
			stateResult = evaluateStateFindNode();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( WORK_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( WORK_MOVE_TO_NODE ); 
		break;

		case WORK_MOVE_TO_NODE:
			stateResult = evaluateStateMoveToNode();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( WORK_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( WORK_AT_NODE ); 

		break;

		case WORK_AT_NODE:
			stateResult = evaluateStateAtNode();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( WORK_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( WORK_SELECT_ANIM_MODE ); 

		break;

		case WORK_SELECT_ANIM_MODE:
			if ( _node->isUsingAnimList() )
				{
				customAnimListEntry_t* animEntry = _node->GetCurrentAnimEntryFromList();
				if ( !animEntry )
					{
					transitionToState( WORK_FAILED );
					return BEHAVIOR_EVALUATING;
					}

				if ( animEntry->waitType == WAITTYPE_EVENT )
					transitionToState( WORK_ANIMATE_LIST_WAIT_ON_SIGNAL );
					
				if ( animEntry->waitType == WAITTYPE_ANIM )
					transitionToState( WORK_ANIMATE_LIST_WAIT_ON_ANIM );

				if ( animEntry->waitType == WAITTYPE_TIME )
					transitionToState( WORK_ANIMATE_LIST_WAIT_ON_TIME );

				return BEHAVIOR_EVALUATING;					
				}

			if ( _node->isWaitForAnim() )
				{
				transitionToState( WORK_ANIMATE_WAIT_ON_ANIM );
				return BEHAVIOR_EVALUATING;
				}

			if ( _node->GetWaitTime() > 0 )
				{
				transitionToState( WORK_ANIMATE_WAIT_ON_TIME );
				return BEHAVIOR_EVALUATING;
				}

			transitionToState( WORK_ANIMATE_CONSTANT );
			
			
		break;

		case WORK_ANIMATE_WAIT_ON_TIME:
			stateResult = evaluateStateAnimateWaitOnTime();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( WORK_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( WORK_SUCCESSFUL ); 
		break;

		case WORK_ANIMATE_WAIT_ON_ANIM:
			stateResult = evaluateStateAnimateWaitOnAnim();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( WORK_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( WORK_SUCCESSFUL ); 
		break;

		case WORK_ANIMATE_WAIT_ON_SIGNAL:
			stateResult = evaluateStateAnimateWaitOnSignal();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( WORK_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( WORK_SUCCESSFUL ); 
		break;

		case WORK_ANIMATE_CONSTANT:
			stateResult = evaluateStateAnimateConstant();

			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( WORK_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( WORK_SUCCESSFUL ); 
		break;

		case WORK_ANIMATE_LIST_WAIT_ON_TIME:
			stateResult = evaluateStateAnimateListWaitOnTime();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( WORK_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				{
				if (_node->isAnimListFinished() )
					transitionToState( WORK_SUCCESSFUL ); 				
				else
					transitionToState( WORK_SELECT_ANIM_MODE );				
				}
				
		break;

		case WORK_ANIMATE_LIST_WAIT_ON_ANIM:
			stateResult = evaluateStateAnimateListWaitOnAnim();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( WORK_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				{
				if (_node->isAnimListFinished() )
					transitionToState( WORK_SUCCESSFUL ); 				
				else
					transitionToState( WORK_SELECT_ANIM_MODE );				
				}
		break;

		case WORK_ANIMATE_LIST_WAIT_ON_SIGNAL:
			stateResult = evaluateStateAnimateListWaitOnSignal();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( WORK_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				{
				if (_node->isAnimListFinished() )
					transitionToState( WORK_SUCCESSFUL ); 				
				else
					transitionToState( WORK_SELECT_ANIM_MODE );				
				}
		break;

		case WORK_SUCCESSFUL:
			_node->RunExitThread();
			self.ignoreHelperNode.node = _node;
			return BEHAVIOR_SUCCESS;
		break;

		case WORK_FAILED:
			return BEHAVIOR_FAILED;
		break;

		}

   return BEHAVIOR_EVALUATING; 
}



void Work::End ( Actor &self	)
{
	if ( _node )
	{
		_node->SetUser( NULL );
	}
}

void Work::transitionToState ( workStates_t state )
{
	switch ( state )
		{
		case WORK_FIND_NODE:
			setupStateFindNode();
			setInternalState( state , "WORK_FIND_NODE" );
		break;

		case WORK_MOVE_TO_NODE:
			setupStateMoveToNode();
			setInternalState( state , "WORK_MOVE_TO_NODE" );
		break;

		case WORK_AT_NODE:
			setupStateAtNode();
			setInternalState( state , "WORK_AT_NODE" );
		break;

		case WORK_ANIMATE_WAIT_ON_TIME:
			setupStateAnimateWaitOnTime();
			setInternalState( state , "WORK_ANIMATE_WAIT_ON_TIME" );
		break;

		case WORK_ANIMATE_WAIT_ON_ANIM:
			setupStateAnimateWaitOnAnim();
			setInternalState( state , "WORK_ANIMATE_WAIT_ON_ANIM" );
		break;

		case WORK_ANIMATE_WAIT_ON_SIGNAL:
			setupStateAnimateWaitOnSignal();
			setInternalState( state , "WORK_ANIMATE_WAIT_ON_SIGNAL" );
		break;

		case WORK_ANIMATE_CONSTANT:
			setupStateAnimateConstant();
			setInternalState( state , "WORK_ANIMATE_CONSTANT" );
		break;

		case WORK_ANIMATE_LIST_WAIT_ON_TIME:
			setupStateAnimateListWaitOnTime();
			setInternalState( state , "WORK_ANIMATE_LIST_WAIT_ON_TIME" );
		break;

		case WORK_ANIMATE_LIST_WAIT_ON_ANIM:
			setupStateAnimateListWaitOnAnim();
			setInternalState( state , "WORK_ANIMATE_LIST_WAIT_ON_ANIM" );
		break;

		case WORK_ANIMATE_LIST_WAIT_ON_SIGNAL:
			setupStateAnimateListWaitOnSignal();
			setInternalState( state , "WORK_ANIMATE_LIST_WAIT_ON_SIGNAL" );
		break;

		case WORK_SELECT_ANIM_MODE:
			setInternalState( state, "WORK_SELECT_ANIM_MODE," );
		break;

		case WORK_SUCCESSFUL:
			setInternalState( state , "WORK_SUCCESSFUL" );
		break;

		case WORK_FAILED:
			setInternalState( state , "WORK_FAILED" );
		break;
		}

}

void Work::setInternalState ( workStates_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );
}



void Work::init( Actor &self )
{
	_self			= &self;
	_animDone		= false;
	_endTime		= 0.0f;
	_node = self.currentHelperNode.node;
	transitionToState( WORK_FIND_NODE );
		
}

void Work::think()
{
	if ( _node && _node->isChanged() )
		{
		_node->SetCriticalChange( false );      
		}
}

void Work::setupStateFindNode()
{
	if ( !_node )
		_node = HelperNode::FindClosestHelperNode( *_self , NODETYPE_WORK , _maxDistance );
}

BehaviorReturnCode_t Work::evaluateStateFindNode()
{
	if ( !_node )
		return BEHAVIOR_FAILED;

	return BEHAVIOR_SUCCESS;
}

void Work::failureStateFindNode( const str& failureReason )
{
}



void Work::setupStateMoveToNode()
{
	_gotoHelperNode.SetNode( _node );
	_gotoHelperNode.SetMovementAnim ( _gotoWorkAnimName );
	_gotoHelperNode.Begin( *_self );
}

BehaviorReturnCode_t Work::evaluateStateMoveToNode()
{
	return _gotoHelperNode.Evaluate( *_self );
}

void Work::failureStateMoveToNode( const str& failureReason )
{
}



void Work::setupStateAtNode()
{
	_self->RunThread( _node->GetEntryThread() );
}

BehaviorReturnCode_t Work::evaluateStateAtNode()
{
	Vector nodeAngles = _node->angles;
	
	_self->movementSubsystem->setAnimDir( nodeAngles );					
	_self->setAngles( nodeAngles );

	_node->SetUser( this );
	return BEHAVIOR_SUCCESS;
}

void Work::failureStateAtNode( const str& failureReason )
{
}



void Work::setupStateAnimateWaitOnTime()
{
	if ( _node->isWaitRandom() )
		_endTime = level.time + G_Random(_node->GetWaitTime());
	else
		_endTime = level.time + _node->GetWaitTime();

	_self->SetAnim( _node->GetAnim() , EV_Actor_NotifyBehavior , legs );

}

BehaviorReturnCode_t Work::evaluateStateAnimateWaitOnTime()
{
	if ( level.time >= _endTime )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void Work::failureStateAnimateWaitOnTime( const str& failureReason )
{
}



void Work::setupStateAnimateWaitOnAnim()
{
	_animDone = false;
	_self->SetAnim( _node->GetAnim() , EV_Actor_NotifyBehavior , legs );
}

BehaviorReturnCode_t Work::evaluateStateAnimateWaitOnAnim()
{
	if ( _animDone )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void Work::failureStateAnimateWaitOnAnim( const str& failureReason )
{
}



void Work::setupStateAnimateWaitOnSignal()
{
	_animDone = false;
	_self->SetAnim( _node->GetAnim() , EV_Actor_NotifyBehavior , legs );
}

BehaviorReturnCode_t Work::evaluateStateAnimateWaitOnSignal()
{
	if ( _animDone )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void Work::failureStateAnimateWaitOnSignal( const str& failureReason )
{
}



void Work::setupStateAnimateConstant()
{
	_self->SetAnim( _node->GetAnim() , EV_Actor_NotifyBehavior , legs );
}

BehaviorReturnCode_t Work::evaluateStateAnimateConstant()
{
	return BEHAVIOR_EVALUATING;
}

void Work::failureStateAnimateConstant( const str& failureReason )
{
}



void Work::setupStateAnimateListWaitOnTime()
{
	customAnimListEntry_t* animListEntry;
	animListEntry = _node->GetCurrentAnimEntryFromList();

	if ( !animListEntry )
		{
		_endTime = 0.0f;
		return;
		}
		
	_endTime = level.time + animListEntry->time;
	_self->SetAnim( animListEntry->anim , EV_Actor_NotifyBehavior , legs );
}

BehaviorReturnCode_t Work::evaluateStateAnimateListWaitOnTime()
{
	if ( level.time >= _endTime )
		{
		_node->NextAnim();
		return BEHAVIOR_SUCCESS;
		}

	return BEHAVIOR_EVALUATING;
}

void Work::failureStateAnimateListWaitOnTime( const str& failureReason )
{
}



void Work::setupStateAnimateListWaitOnAnim()
{
	customAnimListEntry_t* animListEntry;
	animListEntry = _node->GetCurrentAnimEntryFromList();

	if ( !animListEntry )
		{
		_animDone = true;
		return;
		}

	_animDone = false;
	_self->SetAnim( animListEntry->anim , EV_Actor_NotifyBehavior , legs );
}

BehaviorReturnCode_t Work::evaluateStateAnimateListWaitOnAnim()
{
	if ( _animDone )
		{
		_node->NextAnim();
		return BEHAVIOR_SUCCESS;
		}

	return BEHAVIOR_EVALUATING;
}

void Work::failureStateAnimateListWaitOnAnim( const str& failureReason )
{
}



void Work::setupStateAnimateListWaitOnSignal()
{
	customAnimListEntry_t* animListEntry;
	animListEntry = _node->GetCurrentAnimEntryFromList();

	if ( !animListEntry )
		{
		_animDone = true;
		return;
		}

	_animDone = false;
	_self->SetAnim( animListEntry->anim , EV_Actor_NotifyBehavior , legs );
}

BehaviorReturnCode_t Work::evaluateStateAnimateListWaitOnSignal()
{
	if ( _animDone )
		{
		_node->NextAnim();
		return BEHAVIOR_SUCCESS;
		}

	return BEHAVIOR_EVALUATING;
}

void Work::failureStateAnimateListWaitOnSignal( const str& failureReason )
{
}























































