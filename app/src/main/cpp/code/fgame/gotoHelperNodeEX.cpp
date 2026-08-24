//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/gotoHelperNodeEX.cpp                         $
// $Revision:: 3                                                              $
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
//		str _nodeType	   -- The name of the node to move to
//		str _movementAnim  -- The animation to play while moving to the node
//		float _maxDistance -- The maximum distance to look for a node
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
#include "gotoHelperNodeEX.hpp"
#include "../qcommon/gameplaymanager.h"

const float GotoHelperNodeEX::NODE_RADIUS = 32.0f;

CLASS_DECLARATION( Behavior, GotoHelperNodeEX, NULL )
	{
		{ &EV_Behavior_Args,		&GotoHelperNodeEX::SetArgs		},
		{ &EV_Behavior_AnimDone,	&GotoHelperNodeEX::AnimDone		}, 
		{ NULL, NULL }
	};


GotoHelperNodeEX::GotoHelperNodeEX()
{
	_movementAnim = "";
	_nodeType = "" ;
	_maxDistance = 256;
}

GotoHelperNodeEX::~GotoHelperNodeEX()
{
}

void GotoHelperNodeEX::SetArgs( Event *ev )
{
	_nodeType	  = ev->GetString( 1 );
	_maxDistance  = ev->GetFloat ( 2 );
}


void GotoHelperNodeEX::AnimDone( Event *ev )
{
}



void GotoHelperNodeEX::Begin( Actor &self )
	{       
	init( self );
	transitionToState ( GOTO_HNODE_FIND_NODE );
	}


BehaviorReturnCode_t GotoHelperNodeEX::Evaluate( Actor &self )
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



void GotoHelperNodeEX::End(Actor &self)
{   
}



void GotoHelperNodeEX::transitionToState( GotoHelperNodeStates_t state )
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


void GotoHelperNodeEX::setInternalState( GotoHelperNodeStates_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );
}

void GotoHelperNodeEX::init( Actor &self )
{
	str objname;
	str movementMode;
	str stateName;
	GameplayManager *gpm;

	gpm = GameplayManager::getTheGameplayManager();
	if ( !gpm->hasObject(self.getArchetype()) )
		{
		_movementAnim = "run";
		return;
		}
	
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
		else
			_movementAnim = "run";

		return;		
		}

	if ( movementMode == "WALK" )
		{
		if ( gpm->hasProperty(objname, "Walk") )
			_movementAnim = gpm->getStringValue( objname , "Walk" );
		else
			_movementAnim = "walk";

		return;				
		}
	
	_movementAnim = "run";

}

void GotoHelperNodeEX::think()
{
}



void GotoHelperNodeEX::setupStateMoveToNode()
{
	_gotoPoint.SetAnim( _movementAnim );
	_gotoPoint.SetDistance( NODE_RADIUS );
	_gotoPoint.SetPoint( _node->origin );
	_gotoPoint.Begin( *GetSelf() );
}


BehaviorReturnCode_t GotoHelperNodeEX::evaluateStateMoveToNode()
{
	BehaviorReturnCode_t result = _gotoPoint.Evaluate( *GetSelf() );
	str failureReason = "GotoHelperNode::evaluateStateMoveToNode -- _gotoPoint component returned: " + _gotoPoint.GetFailureReason();
	
	if ( result == BEHAVIOR_FAILED )
		failureStateMoveToNode( failureReason );

	return result;	
}



BehaviorReturnCode_t GotoHelperNodeEX::evaluateStateFindNode()
{
	BehaviorReturnCode_t result = BEHAVIOR_SUCCESS;
	str failureReason = "GotoHelperNode::evaluateStateFindNode -- could not find an appropriate node";

	unsigned int NodeMask = HelperNode::GetHelperNodeMask( _nodeType );

	_node = HelperNode::FindClosestHelperNode( *GetSelf() , NodeMask , _maxDistance );

	if ( !_node )
	{
		failureStateFindNode( failureReason );
		result = BEHAVIOR_FAILED;
	}


	return result;
}

void GotoHelperNodeEX::setupStateFindNode()
{
}


void GotoHelperNodeEX::failureStateMoveToNode( const str &failureReason )
{
	SetFailureReason( failureReason );
}


void GotoHelperNodeEX::failureStateFindNode( const str &failureReason )
{
	SetFailureReason( failureReason );
}

