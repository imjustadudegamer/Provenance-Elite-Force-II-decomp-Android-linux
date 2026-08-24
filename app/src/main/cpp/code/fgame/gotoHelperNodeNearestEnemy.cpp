//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/gotoHelperNodeNearestEnemy.cpp             $
// $Revision:: 6                                                              $
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
//	CoverCombatWithRangedWeapon Implementation
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
#include "gotoHelperNodeNearestEnemy.hpp"

CLASS_DECLARATION( Behavior, GotoHelperNodeNearestEnemy, NULL )
	{
		{ &EV_Behavior_Args,		&GotoHelperNodeNearestEnemy::SetArgs	},
		{ &EV_Behavior_AnimDone,	&GotoHelperNodeNearestEnemy::AnimDone	}, 
		{ NULL, NULL }
	};


GotoHelperNodeNearestEnemy::GotoHelperNodeNearestEnemy()
{	
}

GotoHelperNodeNearestEnemy::~GotoHelperNodeNearestEnemy()
{
}

void GotoHelperNodeNearestEnemy::SetArgs( Event *ev )
{
	_nodeType = ev->GetString( 1 );
	_movementAnim = ev->GetString( 2 );
	_maxDistance = ev->GetFloat( 3 );
		
}



void GotoHelperNodeNearestEnemy::AnimDone( Event *ev )
{
}



void GotoHelperNodeNearestEnemy::Begin( Actor &self )
	{       
	init( self );
	transitionToState ( GOTO_HNODE_FIND_NODE );
	}


BehaviorReturnCode_t GotoHelperNodeNearestEnemy::Evaluate( Actor &self )
{
	BehaviorReturnCode_t stateResult;

	think();
	
	switch ( _state )
		{
		case GOTO_HNODE_FIND_NODE:
			stateResult = evaluateStateFindNode();

			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( GOTO_HNODE_FAILED );
			else
				transitionToState( GOTO_HNODE_MOVE_TO_NODE );

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



void GotoHelperNodeNearestEnemy::End(Actor &self)
{   
}



void GotoHelperNodeNearestEnemy::transitionToState( GotoHelperNodeStates_t state )
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


void GotoHelperNodeNearestEnemy::setInternalState( GotoHelperNodeStates_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );
}

void GotoHelperNodeNearestEnemy::init( Actor &self )
{
	_self = &self;	
	updateEnemy();
}

void GotoHelperNodeNearestEnemy::think()
{
}

void GotoHelperNodeNearestEnemy::updateEnemy()
{
	Entity *currentEnemy = _self->enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		{
		_self->enemyManager->FindHighestHateEnemy();
		currentEnemy = _self->enemyManager->GetCurrentEnemy();
		if ( !currentEnemy )
			{
			SetFailureReason( "GotoHelperNodeNearestEnemy::updateEnemy -- No Enemy" );
			transitionToState( GOTO_HNODE_FAILED );
			return;
			}
			
		}

	_currentEnemy = currentEnemy;
}


void GotoHelperNodeNearestEnemy::setupStateFindNode()
{
	unsigned int NodeMask = HelperNode::GetHelperNodeMask( _nodeType );

	if ( !_currentEnemy )
		return;

	_node = HelperNode::FindHelperNodeClosestTo( *_self , _currentEnemy , NodeMask , _maxDistance );

	if ( !_node )
		_node = HelperNode::FindHelperNodeClosestToWithoutPathing( *_self , _currentEnemy , NodeMask , _maxDistance );
				
}

BehaviorReturnCode_t GotoHelperNodeNearestEnemy::evaluateStateFindNode()
{
	if ( !_node )
		{
		str failureMsg = "No Node of Type: " + _nodeType + " in range";
		failureStateFindNode( failureMsg );
		return BEHAVIOR_FAILED;
		}

	return BEHAVIOR_SUCCESS;
}

void GotoHelperNodeNearestEnemy::failureStateFindNode(const str &failureReason )
{
	SetFailureReason( failureReason );
}

void GotoHelperNodeNearestEnemy::setupStateMoveToNode()
{
	_gotoHNode.SetNode( _node );
	_gotoHNode.SetMovementAnim( _movementAnim );
	_gotoHNode.Begin( *_self );
}


BehaviorReturnCode_t GotoHelperNodeNearestEnemy::evaluateStateMoveToNode()
{
	BehaviorReturnCode_t result = _gotoHNode.Evaluate( *_self );	
	
	if ( result == BEHAVIOR_FAILED )
		{
		str failureReason = "GotoHelperNodeNearestEnemy::evaluateStateMoveToNode -- _gotoHNode component returned: " + _gotoHNode.GetFailureReason();
		failureStateMoveToNode( failureReason );
		}	

	return result;		
	
}


void GotoHelperNodeNearestEnemy::failureStateMoveToNode( const str &failureReason )
{
	SetFailureReason( failureReason );
}


























