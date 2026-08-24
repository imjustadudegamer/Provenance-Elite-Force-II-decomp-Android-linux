
#include "actor.h"
#include "gotoHelperNode.hpp"

const float GotoHelperNode::NODE_RADIUS = 32.0f;

CLASS_DECLARATION( Behavior, GotoHelperNode, NULL )
	{
		{ &EV_Behavior_Args,		&GotoHelperNode::SetArgs	},
		{ &EV_Behavior_AnimDone,	&GotoHelperNode::AnimDone	}, 
		{ NULL, NULL }
	};


GotoHelperNode::GotoHelperNode()
{
	_movementAnim = "";
	_nodeType = "" ;
	_maxDistance = 256;
}

GotoHelperNode::~GotoHelperNode()
{
}

void GotoHelperNode::SetArgs( Event *ev )
{
	_nodeType	  = ev->GetString( 1 );
	_movementAnim = ev->GetString( 2 );
	_maxDistance  = ev->GetFloat ( 3 );
}


void GotoHelperNode::AnimDone( Event *ev )
{
}



void GotoHelperNode::Begin( Actor &self )
	{       
	init( self );
	transitionToState ( GOTO_HNODE_FIND_NODE );
	}


BehaviorReturnCode_t GotoHelperNode::Evaluate( Actor &self )
{
	BehaviorReturnCode_t stateResult;

	think();
	
	switch ( _state )
		{
		case GOTO_HNODE_FIND_NODE:
			stateResult = evaluateStateFindNode();

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( GOTO_HNODE_MOVE_TO_NODE );
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( GOTO_HNODE_FAILED );
		break;

		case GOTO_HNODE_MOVE_TO_NODE:
			stateResult = evaluateStateMoveToNode();

			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( GOTO_HNODE_FAILED );
			
			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( GOTO_HNODE_SUCCESS );
		break;


		case GOTO_HNODE_SUCCESS:
			return BEHAVIOR_SUCCESS;

		break;


		case GOTO_HNODE_FAILED:
			return BEHAVIOR_FAILED;

		break;


		}
	

	return BEHAVIOR_EVALUATING;   

}



void GotoHelperNode::End(Actor &self)
{   
}



void GotoHelperNode::transitionToState( GotoHelperNodeStates_t state )
{	
	switch ( state )
		{	
		case GOTO_HNODE_FIND_NODE:
			setupStateFindNode();
			setInternalState( state , "GOTO_HNODE_FIND_NODE" );
		break;

		case GOTO_HNODE_MOVE_TO_NODE:
			setupStateMoveToNode();
			setInternalState( state , "GOTO_HNODE_MOVE_TO_NODE" );
		break;

		case GOTO_HNODE_SUCCESS:
			setInternalState( state , "GOTO_HNODE_SUCCESS" );
		break;

		case GOTO_HNODE_FAILED:
			setInternalState( state , "GOTO_HNODE_FAILED" );
		break;
		}
	
}


void GotoHelperNode::setInternalState( GotoHelperNodeStates_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );
}

void GotoHelperNode::init( Actor &self )
{
	_self = &self;	
}

void GotoHelperNode::think()
{
}



void GotoHelperNode::setupStateMoveToNode()
{
	_gotoPoint.SetAnim( _movementAnim );
	_gotoPoint.SetDistance( NODE_RADIUS );
	_gotoPoint.SetPoint( _node->origin );
	_gotoPoint.Begin( *_self );
}


BehaviorReturnCode_t GotoHelperNode::evaluateStateMoveToNode()
{
	BehaviorReturnCode_t result = _gotoPoint.Evaluate( *_self );
	str failureReason = "GotoHelperNode::evaluateStateMoveToNode -- _gotoPoint component returned: " + _gotoPoint.GetFailureReason();
	
	if ( result == BEHAVIOR_FAILED )
		failureStateMoveToNode( failureReason );

	return result;	
}



BehaviorReturnCode_t GotoHelperNode::evaluateStateFindNode()
{
	if ( _node )
		return BEHAVIOR_SUCCESS;

	BehaviorReturnCode_t result = BEHAVIOR_SUCCESS;
	str failureReason = "GotoHelperNode::evaluateStateFindNode -- could not find an appropriate node";

	unsigned int NodeMask = HelperNode::GetHelperNodeMask( _nodeType );

	_node = HelperNode::FindClosestHelperNode( *_self , NodeMask , _maxDistance );

	if ( !_node )
	{
		failureStateFindNode( failureReason );
		result = BEHAVIOR_FAILED;
	}


	return result;
}

void GotoHelperNode::setupStateFindNode()
{
}


void GotoHelperNode::failureStateMoveToNode( const str &failureReason )
{
	SetFailureReason( failureReason );
}


void GotoHelperNode::failureStateFindNode( const str &failureReason )
{
	SetFailureReason( failureReason );
}

