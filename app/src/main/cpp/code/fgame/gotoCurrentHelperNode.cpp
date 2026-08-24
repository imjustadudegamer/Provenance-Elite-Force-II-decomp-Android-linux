
#include "actor.h"
#include "gotoCurrentHelperNode.hpp"
#include "../qcommon/gameplaymanager.h"

const float GotoCurrentHelperNode::NODE_RADIUS = 32.0f;

CLASS_DECLARATION( Behavior, GotoCurrentHelperNode, NULL )
	{
		{ &EV_Behavior_Args,		&GotoCurrentHelperNode::SetArgs		},
		{ &EV_Behavior_AnimDone,	&GotoCurrentHelperNode::AnimDone	}, 
		{ NULL, NULL }
	};


GotoCurrentHelperNode::GotoCurrentHelperNode()
{
	_movementAnim = "run";
	_faceEnemy = false;
}

GotoCurrentHelperNode::~GotoCurrentHelperNode()
{
}

void GotoCurrentHelperNode::SetArgs( Event *ev )
{
	_movementAnim	  = ev->GetString( 1 );
	if ( ev->NumArgs() > 1 )
		_faceEnemy = ev->GetBoolean( 2 );

}


void GotoCurrentHelperNode::AnimDone( Event *ev )
{
}



void GotoCurrentHelperNode::Begin( Actor &self )
{       
	init( self );
	transitionToState ( GOTO_HNODE_FIND_NODE );
}


BehaviorReturnCode_t GotoCurrentHelperNode::Evaluate( Actor &self )
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



void GotoCurrentHelperNode::End(Actor &self)
{
	if ( _faceEnemy )
			self.movementSubsystem->setFaceEnemy( false );
}



void GotoCurrentHelperNode::transitionToState( GotoHelperNodeStates_t state )
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


void GotoCurrentHelperNode::setInternalState( GotoHelperNodeStates_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );
}

void GotoCurrentHelperNode::init( Actor &self )
{
	str objname;
	str movementMode;
	str stateName;
	GameplayManager *gpm;

	self.movementSubsystem->setFaceEnemy( _faceEnemy );

	gpm = GameplayManager::getTheGameplayManager();
	if ( gpm->hasObject(self.getArchetype()) )
		{
			objname = self.getArchetype();
			stateName = self.currentState->getName();
			objname = objname + "." + stateName;
			
			if ( gpm->hasProperty( objname , "MovementMode" ) )
				movementMode = gpm->getStringValue( objname , "MovementMode" );
			else
				movementMode = "RUN";


			objname = self.getArchetype();
			objname = objname + "." + self.GetAnimSet();

			if ( movementMode == "RUN" )
				{
				if ( gpm->hasProperty(objname, "Run") )
					_movementAnim = gpm->getStringValue( objname , "Run" );

				return;		
				}

			if ( movementMode == "WALK" )
				{
				if ( gpm->hasProperty(objname, "Walk") )
					_movementAnim = gpm->getStringValue( objname , "Walk" );
				return;				
				}

			}

	str movementAnim = self.GetStateVar( _movementAnim );

	if ( movementAnim.length() )
		_movementAnim = movementAnim;	

}

void GotoCurrentHelperNode::think()
{
}



void GotoCurrentHelperNode::setupStateMoveToNode()
{
	_gotoPoint.SetAnim( _movementAnim );
	_gotoPoint.SetDistance( NODE_RADIUS );
	_gotoPoint.SetPoint( _node->origin );
	_gotoPoint.Begin( *GetSelf() );
}


BehaviorReturnCode_t GotoCurrentHelperNode::evaluateStateMoveToNode()
{
	BehaviorReturnCode_t result = _gotoPoint.Evaluate( *GetSelf() );
	str failureReason = "GotoCurrentHelperNode::evaluateStateMoveToNode -- _gotoPoint component returned: " + _gotoPoint.GetFailureReason();
	
	if ( result == BEHAVIOR_FAILED )
		failureStateMoveToNode( failureReason );

	return result;	
}



BehaviorReturnCode_t GotoCurrentHelperNode::evaluateStateFindNode()
{
	BehaviorReturnCode_t result = BEHAVIOR_SUCCESS;
	str failureReason = "GotoCurrentHelperNode::evaluateStateFindNode -- could not find an appropriate node";

	if ( !_node )
	{
		failureStateFindNode( failureReason );
		result = BEHAVIOR_FAILED;
	}


	return result;
}

void GotoCurrentHelperNode::setupStateFindNode()
{
	_node = GetSelf()->currentHelperNode.node;

}


void GotoCurrentHelperNode::failureStateMoveToNode( const str &failureReason )
{
	SetFailureReason( failureReason );
}


void GotoCurrentHelperNode::failureStateFindNode( const str &failureReason )
{
	SetFailureReason( failureReason );
}

