
#include "actor.h"
#include "patrol.hpp"

CLASS_DECLARATION( Behavior, Patrol, NULL )
	{
		{ &EV_Behavior_Args,			&Patrol::SetArgs	},
		{ &EV_Behavior_AnimDone,		&Patrol::AnimDone	}, 
		{ NULL, NULL }
	};


Patrol::Patrol()
{
	_anim = "walk";
}

Patrol::~Patrol()
{
}

void Patrol::SetArgs ( Event *ev)
{
	_anim = ev->GetString( 1 );
}



void Patrol::AnimDone( Event *ev )
{
	if ( _state == PATROL_WAITING_AT_NODE_FOR_ANIM )
		{
		_state = PATROL_FIND_NEXT_PATROL_NODE;
		}
}



void Patrol::Begin( Actor &self )
{
	_init( self );
}



void Patrol::_init( Actor &self )
{
	_nextMoveAttempt = 0.0f;
	_waitTime        = 0.0f;
	_moveFailures     = 0;

	_torsoAnim = self.combatSubsystem->GetAnimForMyWeapon( "Idle" );
	if ( _torsoAnim.length() )
		{
		self.SetAnim( _torsoAnim, NULL , torso );
		}

	_state = PATROL_FIND_CLOSEST_PATROL_NODE;
	if ( !_setupFindClosestPatrolNode( self ) )
		_setupFindClosestPatrolNodeFailed( self );
}



BehaviorReturnCode_t	Patrol::Evaluate ( Actor &self )
{      
	switch ( _state )
	{
		case PATROL_FIND_CLOSEST_PATROL_NODE:
			_findClosestPatrolNode( self );
		break;

		case PATROL_MOVING_TO_PATROL_NODE:
			_moveToPatrolNode( self );
		break;

		case PATROL_AT_PATROL_NODE:
			_atPatrolNode( self );
		break;

		case PATROL_WAITING_AT_NODE:
			_waitAtPatrolNode( self );
		break;

		case PATROL_WAITING_AT_NODE_FOR_ANIM:
			_waitAtPatrolNodeForAnim( self );
		break;

		case PATROL_FIND_NEXT_PATROL_NODE:
			_findNextPatrolNode( self );
		break;

		case PATROL_HOLD:
			_hold( self );
		break;

		case PATROL_FAILED:
			return BEHAVIOR_FAILED;
		break;

		case PATROL_SUCCESSFUL:
			return BEHAVIOR_SUCCESS;
		break;
		}

	return BEHAVIOR_EVALUATING; 
}




void Patrol::End ( Actor &self	)
{   
}



bool Patrol::_setupFindClosestPatrolNode( Actor &self )
{
	_node = NULL;
	return true;
}



void Patrol::_setupFindClosestPatrolNodeFailed( Actor &self )
{
	_state = PATROL_FAILED;
}



void Patrol::_findClosestPatrolNode( Actor &self )
{
	if ( _node )
		{
		_state = PATROL_MOVING_TO_PATROL_NODE;
		if (!_setupMovingToPatrolNode( self ) )
			_setupMovingToPatrolNodeFailed( self );
		return;
		}

	_node = HelperNode::FindClosestHelperNode( self , NODETYPE_PATROL , 512.0f, 0 );   

	if ( !_node )
		{	   
		_findClosestPatrolNodeFailed(self);
		return;
		}

	str nextNodeTargetName;
	nextNodeTargetName = _node->target;

	if ( nextNodeTargetName.length() )
		{
		HelperNode* testNode;
		testNode = HelperNode::GetTargetedHelperNode( nextNodeTargetName );
		if ( testNode )
			{
			Vector selfToTestNode = testNode->origin - self.origin;
			float selfToTestNodeDist = selfToTestNode.length();

			Vector currentNodeToTestNode = testNode->origin - _node->origin;
			float currentNodeToTestNodeDist = currentNodeToTestNode.length();

			if ( currentNodeToTestNodeDist > selfToTestNodeDist )
				_node = testNode;
			}
			
		}



	_state = PATROL_MOVING_TO_PATROL_NODE;
	if (!_setupMovingToPatrolNode( self ) )
		_setupMovingToPatrolNodeFailed( self );

}


void Patrol::_findClosestPatrolNodeFailed( Actor &self )
{
	SetFailureReason( "Unable to find a patrol node within 512.0 units" );
		_state = PATROL_FAILED;
}



bool Patrol::_setupMovingToPatrolNode( Actor &self )
{
	if ( _lastNode )
		_lastNode->RunExitThread();
  
	_torsoAnim = self.combatSubsystem->GetAnimForMyWeapon( "Idle" );
	if ( _torsoAnim.length() )
		{
		self.SetAnim( _torsoAnim, NULL , torso );
		}

	_gotoHelperNode.SetDistance( 34.0f );
	_gotoHelperNode.SetAnim( _anim );
	_gotoHelperNode.SetPoint( _node->origin );
	_gotoHelperNode.Begin( self );

	return true;
}



void Patrol::_setupMovingToPatrolNodeFailed( Actor &self )
{
	_state = PATROL_FAILED;
}



void Patrol::_moveToPatrolNode( Actor &self )
{
	BehaviorReturnCode_t moveResult;

	moveResult = _gotoHelperNode.Evaluate( self );
   
	if ( moveResult == BEHAVIOR_SUCCESS )
		{
		_nextMoveAttempt = 0.0f;
		_moveFailures    = 0;
		_state = PATROL_AT_PATROL_NODE;
		if ( !_setupAtPatrolNode( self ) )
			_setupAtPatrolNodeFailed( self );
		}  
   else if ( moveResult == BEHAVIOR_FAILED )
	{
	_moveToPatrolNodeFailed( self );
	}

}



void Patrol::_moveToPatrolNodeFailed( Actor &self )
{
	_moveFailures++;

		_state = PATROL_FAILED;

}



bool Patrol::_setupAtPatrolNode( Actor &self )
{
	return true;
}


void Patrol::_setupAtPatrolNodeFailed( Actor &self )
{
	_state = PATROL_FAILED;
}



void Patrol::_atPatrolNode( Actor &self )
{
	str nodeAnim;
	nodeAnim = _node->GetAnim();


	_node->RunEntryThread();

	_waitTime = _node->GetWaitTime() + level.time;

	if ( _node->isWaitRandom() )
		_waitTime = level.time + G_Random( _waitTime ) + 1;

	if ( _waitTime > level.time )
		{
		if ( nodeAnim.length() && self.animate->HasAnim( nodeAnim.c_str() ) )
			{
			self.ClearLegAnim();
			self.ClearTorsoAnim();
			self.SetAnim( nodeAnim );
			}
		else 
			self.SetAnim( "idle" );

		_state = PATROL_WAITING_AT_NODE;
		if ( !_setupWaitingAtPatrolNode( self ) )
			_setupWaitingAtPatrolNodeFailed( self );

		return;
		}

	if ( _node->isWaitForAnim() && nodeAnim.length() )
		{
		self.SetAnim( nodeAnim , EV_Actor_NotifyBehavior);
		_state = PATROL_WAITING_AT_NODE_FOR_ANIM;
		if ( !_setupWaitingAtPatrolNodeForAnim( self ) )
			_setupWaitingAtPatrolNodeForAnimFailed( self );

		return;
		}

	_state = PATROL_FIND_NEXT_PATROL_NODE;
	if ( !_setupFindNextPatrolNode( self ) )
		_setupFindNextPatrolNodeFailed( self );
}




bool Patrol::_setupWaitingAtPatrolNode( Actor &self )
{
	return true;
}



void Patrol::_setupWaitingAtPatrolNodeFailed(Actor &self )
{
	_state = PATROL_FAILED;
}



void Patrol::_waitAtPatrolNode(Actor &self )
{
	if ( level.time > _waitTime )
		{
		_state = PATROL_FIND_NEXT_PATROL_NODE;
		if ( !_setupFindNextPatrolNode( self ) )
			_setupFindNextPatrolNodeFailed( self );
		}
}



bool Patrol::_setupWaitingAtPatrolNodeForAnim( Actor &self )
{
	self.setAngles(_node->angles);
	return true;
}



void Patrol::_setupWaitingAtPatrolNodeForAnimFailed( Actor &self )
{
	_state = PATROL_FAILED;
}



void Patrol::_waitAtPatrolNodeForAnim( Actor &self )
{
}



bool Patrol::_setupFindNextPatrolNode( Actor &self )
{
	return true;
}



void Patrol::_setupFindNextPatrolNodeFailed( Actor &self )
{
	SetFailureReason( "Unable to find the next Node in the patrol path" );
	_state = PATROL_FAILED;
}



void Patrol::_findNextPatrolNode( Actor &self )
{
	str nextNodeTargetName;
	nextNodeTargetName = _node->target;

	if ( !nextNodeTargetName.length() )
		{
		_state = PATROL_SUCCESSFUL;
		return;
		}

	_lastNode = _node;
	_node = HelperNode::GetTargetedHelperNode( nextNodeTargetName );

	if ( !_node )
		{
		_state = PATROL_FAILED;
		return;
		}

	_state = PATROL_MOVING_TO_PATROL_NODE;
	if ( !_setupMovingToPatrolNode( self ) )
		_setupMovingToPatrolNodeFailed( self );
}  



bool Patrol::_setupHold( Actor &self )
{
	_nextMoveAttempt = level.time + 0.5f + G_Random();
	return true;
}



void Patrol::_setupHoldFailed( Actor &self )
{
	_state = PATROL_FAILED;
}



void Patrol::_hold( Actor &self )
{
	if ( level.time > _nextMoveAttempt )
		{
		_state = PATROL_MOVING_TO_PATROL_NODE;
		if ( !_setupMovingToPatrolNode( self ) )
			_setupMovingToPatrolNodeFailed( self );
		}
}
