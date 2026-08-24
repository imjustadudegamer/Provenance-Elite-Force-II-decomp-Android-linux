//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/useAlarm.cpp            $
// $Revision:: 10                                                             $
//   $Author:: Singlis                                                        $
//     $Date:: 9/26/03 2:36p                                                  $
//
// Copyright (C) 2002 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
//	CorridorCombatWithRangedWeapon Implementation
// 
// PARAMETERS:
//		str _movementAnim -- The animation to play while moving to the cover node
//
// ANIMATIONS:
//		_movementAnim : PARAMETER
//--------------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "actor.h"
#include "useAlarm.hpp"

CLASS_DECLARATION( Behavior, UseAlarm, NULL )
{
	{ &EV_Behavior_Args,		&UseAlarm::SetArgs				},
	{ &EV_Behavior_AnimDone,	&UseAlarm::AnimDone				}, 
	{ &EV_HelperNodeCommand,	&UseAlarm::HandleNodeCommand	},
	{ NULL, NULL }
};

UseAlarm::UseAlarm()
{
	_movementAnimName = "walk";
	_maxDistance	  = 1024.0f;
}

UseAlarm::~UseAlarm()
{
}

void UseAlarm::SetArgs ( Event *ev)
{
	if (ev->NumArgs() == 1)
	{
		_movementAnimName = ev->GetString( 1 );
	}
}



void UseAlarm::AnimDone( Event *ev )
{
	switch( _state )
		{
		case USE_ALARM_WAIT_ON_ANIM:
			_animDone = true;
		break;
		}

}

void UseAlarm::HandleNodeCommand( Event *ev )
{
}

void UseAlarm::Begin( Actor &self )
{
	init( self );
}

BehaviorReturnCode_t UseAlarm::Evaluate ( Actor &self )
{      
	BehaviorReturnCode_t stateResult;

	think();

	switch ( _state )
		{
		case USE_ALARM_FIND_NODE:
			stateResult = evaluateStateFindNode();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( USE_ALARM_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( USE_ALARM_MOVE_TO_NODE ); 
		break;
		

		case USE_ALARM_MOVE_TO_NODE:
			stateResult = evaluateStateMoveToNode();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( USE_ALARM_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( USE_ALARM_AT_NODE ); 

		break;

		case USE_ALARM_AT_NODE:
			stateResult = evaluateStateAtNode();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( USE_ALARM_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( USE_ALARM_WAIT_ON_ANIM ); 

		break;

		case USE_ALARM_WAIT_ON_ANIM:
			stateResult = evaluateStateWaitOnAnim();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( USE_ALARM_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( USE_ALARM_ROTATE_TO_ENEMY ); 
		break;

		case USE_ALARM_ROTATE_TO_ENEMY:
			stateResult = evaluateStateRotateToEnemy();
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( USE_ALARM_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( USE_ALARM_SUCCESSFUL ); 

		break;

		case USE_ALARM_SUCCESSFUL:
			_node->RunExitThread();
			return BEHAVIOR_SUCCESS;
		break;

		case USE_ALARM_FAILED:
			return BEHAVIOR_FAILED;
		break;

		}

   return BEHAVIOR_EVALUATING; 
}



void UseAlarm::End ( Actor &self	)
{
	if ( _node )
	{
		_node->SetUser( NULL );
	}
}

void UseAlarm::transitionToState ( useAlarmStates_t state )
{
	switch ( state )
		{
		case USE_ALARM_FIND_NODE:
			setupStateFindNode();
			setInternalState( state , "USE_ALARM_FIND_NODE" );
		break;

		case USE_ALARM_MOVE_TO_NODE:
			setupStateMoveToNode();
			setInternalState( state , "USE_ALARM_MOVE_TO_NODE" );
		break;

		case USE_ALARM_AT_NODE:
			setupStateAtNode();
			setInternalState( state , "USE_ALARM_AT_NODE" );
		break;

		case USE_ALARM_WAIT_ON_ANIM:
			setupStateWaitOnAnim();
			setInternalState( state , "WORK_ANIMATE_WAIT_ON_ANIM" );
		break;

		case USE_ALARM_ROTATE_TO_ENEMY:
			setupStateRotateToEnemy();
			setInternalState( state, "USE_ALARM_ROTATE_TO_ENEMY" );
		break;

		case USE_ALARM_SUCCESSFUL:			
			setInternalState( state , "WORK_SUCCESSFUL" );
		break;

		case USE_ALARM_FAILED:
			setInternalState( state , "WORK_FAILED" );
		break;		
		}

}

void UseAlarm::setInternalState ( useAlarmStates_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );
}



void UseAlarm::init( Actor &self )
{
	_self			= &self;
	_animDone		= false;	
	transitionToState( USE_ALARM_FIND_NODE );
}

void UseAlarm::think()
{
}

void UseAlarm::setupStateFindNode()
{
	str currentNodeCustomType;
	if ( _self->currentHelperNode.node )
		currentNodeCustomType = _self->currentHelperNode.node->GetCustomType();

	if ( currentNodeCustomType == "alarm" )
		_node = &(*_self->currentHelperNode.node);
	else
		_node = HelperNode::FindClosestHelperNode( *_self , "alarm" , _maxDistance );	
}

BehaviorReturnCode_t UseAlarm::evaluateStateFindNode()
{
	if ( !_node )
		return BEHAVIOR_FAILED;

	return BEHAVIOR_SUCCESS;
}

void UseAlarm::failureStateFindNode( const str& failureReason )
{
}



void UseAlarm::setupStateMoveToNode()
{
	_gotoHelperNode.SetNode( _node );
	_gotoHelperNode.SetMovementAnim ( _movementAnimName );
	_gotoHelperNode.Begin( *_self );
}

BehaviorReturnCode_t UseAlarm::evaluateStateMoveToNode()
{
	return _gotoHelperNode.Evaluate( *_self );
}

void UseAlarm::failureStateMoveToNode( const str& failureReason )
{
}

void UseAlarm::setupStateAtNode()
{
	_self->SetAnim( "idle" );
	_self->RunThread( _node->GetEntryThread() );
}

BehaviorReturnCode_t UseAlarm::evaluateStateAtNode()
{
	Vector nodeAngles = _node->angles;
	
	_self->movementSubsystem->setAnimDir( nodeAngles );					
	_self->setAngles( nodeAngles );

	_node->SetUser( this );
	return BEHAVIOR_SUCCESS;
}

void UseAlarm::failureStateAtNode( const str& failureReason )
{
}



void UseAlarm::setupStateWaitOnAnim()
{
	if( ! _self->SetAnim( _node->GetAnim() , EV_Actor_NotifyBehavior , legs ) )
	{
		_animDone = true;
	}
	else
	{
		_animDone = false;
	}
}

BehaviorReturnCode_t UseAlarm::evaluateStateWaitOnAnim()
{
	if ( _animDone )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void UseAlarm::failureStateWaitOnAnim( const str& failureReason )
{
}

void UseAlarm::setupStateRotateToEnemy()
{
	Entity *enemy = _self->enemyManager->GetCurrentEnemy();

	if ( !enemy )
		return;

	_rotateToEntity.SetAnim( "idle" );
	_rotateToEntity.SetEntity( enemy );
	_rotateToEntity.SetTurnSpeed( 25.0f );
	_rotateToEntity.Begin( *_self );

}

BehaviorReturnCode_t UseAlarm::evaluateStateRotateToEnemy()
{
	return _rotateToEntity.Evaluate( *_self );
}

void UseAlarm::failureStateRotateToEnemy( const str &failureReason )
{

}

