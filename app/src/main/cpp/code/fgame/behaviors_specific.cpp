//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/behaviors_specific.cpp                         $
// $Revision:: 18                                                             $
//   $Author:: Steven                                                         $
//     $Date:: 10/13/03 8:53a                                                 $
//
// Copyright (C) 2001 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// 
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).
#include "_pch_cpp.h"
#include "behavior.h"
#include "actor.h"
#include "doors.h"
#include "object.h"
#include "explosion.h"
#include "weaputils.h"
#include "player.h"
#include "behaviors_specific.h"
#include "rageai.h"  



const float PatrolWorkHibernate::MIN_DISTANCE_TO_PATROL_NODE    = 512.0f;
const float PatrolWorkHibernate::MIN_DISTANCE_TO_WORK_NODE      = 512.0f;
const float PatrolWorkHibernate::MIN_DISTANCE_TO_HIBERNATE_NODE = 96.0f;

CLASS_DECLARATION( Behavior, PatrolWorkHibernate, NULL )
	{
		{ &EV_Behavior_Args,			&PatrolWorkHibernate::SetArgs	},
		{ &EV_Behavior_AnimDone,		&PatrolWorkHibernate::AnimDone	}, 
		{ NULL, NULL }
	};








void PatrolWorkHibernate::Begin( Actor &self )
{
	_init( self );
	if ( self.GetActorFlag ( ACTOR_FLAG_IN_ALCOVE ) )
		_state = PATROLWORKHIBERNATE_HIBERNATE_HOLD;
}









void PatrolWorkHibernate::End ( Actor &self	)
{
}


void PatrolWorkHibernate::_setupPatrol( Actor &self )
{
	_patrolComponent.SetInitialNode( self.currentHelperNode.node );
	_patrolComponent.Begin( self );
}


void PatrolWorkHibernate::_patrol( Actor &self )
{
	if ( _wantsToHibernate( self ) )
		{
		_state = PATROLWORKHIBERNATE_HIBERNATE;
		_setupHibernate( self );
		_patrolComponent.End( self);
		return;
		}

	if ( _wantsToWork( self ) )
		{
		_state = PATROLWORKHIBERNATE_WORK;
		_setupWork( self );
		_patrolComponent.End( self);
		return;
		}


	BehaviorReturnCode_t patrolResult;
	patrolResult = _patrolComponent.Evaluate( self );

	if ( patrolResult == BEHAVIOR_SUCCESS )
		_state = PATROLWORKHIBERNATE_SELECT_STATE;

	if ( patrolResult == BEHAVIOR_FAILED )
		_state = PATROLWORKHIBERNATE_FAILED;
}



void PatrolWorkHibernate::_setupHibernate( Actor &self )
{
	_hibernateComponent.Begin( self );
}





void PatrolWorkHibernate::_setupWork( Actor &self )
{	
	_workComponent.SetNode( self.currentHelperNode.node );
	_workComponent.Begin( self );
}


void PatrolWorkHibernate::_work( Actor &self )
{
	BehaviorReturnCode_t workResult;
	workResult = _workComponent.Evaluate( self );

	if ( workResult == BEHAVIOR_SUCCESS )
		_state = PATROLWORKHIBERNATE_SELECT_STATE;

	if ( workResult == BEHAVIOR_FAILED )
		_state = PATROLWORKHIBERNATE_FAILED;
}


void PatrolWorkHibernate::_setupHold( Actor &self )
{
	self.SetAnim( "idle" );
	_nextStateCheck = level.time + G_Random() + 1.0f;   
}

void PatrolWorkHibernate::_hold( Actor &self )
{
	if ( level.time >= _nextStateCheck )
		_state = PATROLWORKHIBERNATE_SELECT_STATE;
}

bool PatrolWorkHibernate::_wantsToHibernate( Actor &self )
{
	float tendencyToHibernate;

	if ( level.time > _nextHibernateCheck )
		{
		_nextHibernateCheck = level.time + G_Random() + 0.25f;

		HelperNode* hibNode;
		hibNode = HelperNode::FindClosestHelperNode( self , "hibernate" , MIN_DISTANCE_TO_HIBERNATE_NODE );

		if ( !hibNode )
			return false;

		if ( !self.WithinDistance( hibNode->origin , MIN_DISTANCE_TO_HIBERNATE_NODE ) )
			return false;

		if ( hibNode == _lastHibernateNodeChecked )
			return false;

		_lastHibernateNodeChecked = hibNode;

		tendencyToHibernate = self.personality->GetTendency( "hibernate" );

		if ( G_Random() <= tendencyToHibernate )
			return true;         

		}

	return false;
}


bool PatrolWorkHibernate::_wantsToWork( Actor &self )
{
	float tendencyToWork;

	if ( level.time > _nextWorkCheck )
		{
		_nextWorkCheck = level.time + G_Random() + 0.25f;

		tendencyToWork = self.personality->GetTendency( "work" );
		if ( G_Random() > tendencyToWork )
			return false;

		HelperNode* workNode = HelperNode::FindClosestHelperNode( self , NODETYPE_WORK , MIN_DISTANCE_TO_WORK_NODE  );
	
		if ( !workNode )
			return false;

		if ( !self.WithinDistance( workNode->origin , MIN_DISTANCE_TO_WORK_NODE ) )
			return false;

		if ( workNode == _lastWorkNodeChecked )
			{
			self.currentHelperNode.node->UnreserveNode();
			return false;
			}

		_lastWorkNodeChecked = workNode;
	
		self.currentHelperNode.node->UnreserveNode();
		return true;         
		}

	return false;
}

void PatrolWorkHibernate::_selectState( Actor &self )
{
	HelperNode* patrolNode;
	patrolNode = HelperNode::FindClosestHelperNode( self , NODETYPE_PATROL , MIN_DISTANCE_TO_PATROL_NODE );

	if ( patrolNode && self.WithinDistance( patrolNode->origin , MIN_DISTANCE_TO_PATROL_NODE ) )
		{
		_state = PATROLWORKHIBERNATE_PATROL;
		_setupPatrol( self );
		return;
		}

	if ( _wantsToHibernate( self ) )
		{
		_state = PATROLWORKHIBERNATE_HIBERNATE;
		_setupHibernate( self );   
		return;
		}

	if ( _wantsToWork( self ) )
		{
		_state = PATROLWORKHIBERNATE_WORK;
		_setupWork( self );      
		return;
		}

	_state = PATROLWORKHIBERNATE_HOLD;
	_setupHold( self );

}






const float PatrolWorkWander::MIN_DISTANCE_TO_PATROL_NODE    = 512.0f;
const float PatrolWorkWander::MIN_DISTANCE_TO_WORK_NODE      = 512.0f;


CLASS_DECLARATION( Behavior, PatrolWorkWander, NULL )
	{
		{ &EV_Behavior_Args,		&PatrolWorkWander::SetArgs	},
		{ &EV_Behavior_AnimDone,	&PatrolWorkWander::AnimDone	}, 
		{ NULL, NULL							}
	};


void PatrolWorkWander::SetArgs ( Event *ev)
{
}



void PatrolWorkWander::AnimDone( Event *ev )
{
	switch ( _state )
	{
	   
	case PATROLWORKWANDER_PATROL:
		_patrolComponent.AnimDone( ev );
	break;

    case PATROLWORKWANDER_WORK:
		_workComponent.AnimDone( ev );
	break;
  
	}
}



void PatrolWorkWander::Begin( Actor &self )
{
   _init( self );
}



void PatrolWorkWander::_init( Actor &self )
{
   _nextStateCheck     = 0.0f;
   _nextWorkCheck      = 0.0f; 
   _nextPatrolCheck    = 0.0f;
   
   _wanderFailures = 0;
   _patrolFailures = 0;
   _workFailures   = 0;
   

   _state = PATROLWORKWANDER_SELECT_STATE;
}



BehaviorReturnCode_t PatrolWorkWander::Evaluate ( Actor &self )
{      
	switch ( _state )
		{
		case PATROLWORKWANDER_SELECT_STATE:
			_selectState( self );
		break;

		case PATROLWORKWANDER_PATROL:
			_patrol ( self );
		break;

		case PATROLWORKWANDER_WORK:
			_work( self );
		break;

		case PATROLWORKWANDER_WANDER:
			_wander( self );
		break;

		case PATROLWORKWANDER_HOLD:
			_hold( self );
		break;

		case PATROLWORKWANDER_FAILED:
			return BEHAVIOR_FAILED;
		break;
		}

	return BEHAVIOR_EVALUATING; 
}



void PatrolWorkWander::End ( Actor &self	)
{
   
}


void PatrolWorkWander::_setupWander( Actor &self )
{
	_wanderComponent.SetAnim( "walk" );
	_wanderComponent.SetDistance( 256.0f );
	_wanderComponent.SetMinDistance( 96.0f );
	_wanderComponent.SetMode( MoveRandomDirection::RANDOM_MOVE_ANYWHERE );
	_wanderComponent.Begin( self );
}


void PatrolWorkWander::_wander( Actor &self )
{
   if ( _wantsToWork( self ) )
      {
      _state = PATROLWORKWANDER_WORK;
      _setupWork( self );
      _wanderComponent.End( self);
      return;
      }

   BehaviorReturnCode_t wanderResult;
   wanderResult = _wanderComponent.Evaluate( self );

   if ( wanderResult == BEHAVIOR_SUCCESS )
		{
		_wanderFailures = 0;
		_setupHold( self );
		_state = PATROLWORKWANDER_HOLD;
		return;
		}

   if ( wanderResult == BEHAVIOR_FAILED ) 
		{
		_wanderFailed( self );
		return;
		}

}


void PatrolWorkWander::_wanderFailed( Actor &self )
{
	_wanderFailures++;
	
	_setupHold( self );
	_state = PATROLWORKWANDER_HOLD;
}

void PatrolWorkWander::_setupPatrol( Actor &self )
{
	_patrolComponent.Begin( self );
}


void PatrolWorkWander::_patrol( Actor &self )
{
   if ( _wantsToWork( self ) )
      {
      _state = PATROLWORKWANDER_WORK;
      _setupWork( self );
      _patrolComponent.End( self);
      return;
      }


   BehaviorReturnCode_t patrolResult;
   patrolResult = _patrolComponent.Evaluate( self );

   if ( patrolResult == BEHAVIOR_SUCCESS )
		{
		_patrolFailures = 0;
		_setupHold( self );
		_state = PATROLWORKWANDER_HOLD;
		return;
		}
      

   if ( patrolResult == BEHAVIOR_FAILED )
		{
		_patrolFailed( self );
		return;
		}
	
}

void PatrolWorkWander::_patrolFailed( Actor &self )
{
	_patrolFailures++;

	_setupHold( self );
	_state = PATROLWORKWANDER_HOLD;
}

void PatrolWorkWander::_setupWork( Actor &self )
{
   _workComponent.Begin( self );
}


void PatrolWorkWander::_work( Actor &self )
{
   BehaviorReturnCode_t workResult;
   workResult = _workComponent.Evaluate( self );

   if ( workResult == BEHAVIOR_SUCCESS )
		{
		_workFailures = 0;
		_setupHold( self );
		_state = PATROLWORKWANDER_HOLD;
		return;
		}
      

   if ( workResult == BEHAVIOR_FAILED )
		{
		_workFailed( self );
		return;
		}
   
}

void PatrolWorkWander::_workFailed( Actor &self )
{
	_workFailures++;

	_setupHold( self );
	_state = PATROLWORKWANDER_HOLD;
}

void PatrolWorkWander::_setupHold( Actor &self )
{
   self.SetAnim( "idle" );
   _nextStateCheck = level.time + G_Random( 5.0f ) + 1.0f;   
}

void PatrolWorkWander::_hold( Actor &self )
{
   if ( level.time >= _nextStateCheck )
      _state = PATROLWORKWANDER_SELECT_STATE;
}

bool PatrolWorkWander::_wantsToWork( Actor &self )
{
   float tendencyToWork;
   
	if ( _workFailures > 5 )
		return false;

   if ( level.time > _nextWorkCheck )
      {
      _nextWorkCheck = level.time + G_Random() + 0.25f;

      HelperNode* workNode;
      workNode = HelperNode::FindClosestHelperNode( self , NODETYPE_WORK , MIN_DISTANCE_TO_WORK_NODE  );
      
      if ( !workNode )
         return false;

      if ( !self.WithinDistance( workNode->origin , MIN_DISTANCE_TO_WORK_NODE ) )
         return false;

      if ( workNode == _lastWorkNodeChecked )
         return false;

      _lastWorkNodeChecked = workNode;

      tendencyToWork = self.personality->GetTendency( "work" );
      
      if ( G_Random() <= tendencyToWork )
		{
       	   self.currentHelperNode.node->UnreserveNode();
		   return true;         
		}

	 

      }

   return false;
}

bool PatrolWorkWander::_wantsToPatrol( Actor &self )
{
   float tendencyToPatrol;
   
	if ( _patrolFailures > 5 )
		return false;

   if ( level.time >= _nextPatrolCheck )
      {
      _nextPatrolCheck = level.time + G_Random() + 0.25f;

      HelperNode* patrolNode;
      patrolNode = HelperNode::FindClosestHelperNode( self , NODETYPE_PATROL , MIN_DISTANCE_TO_PATROL_NODE  );
      
      if ( !patrolNode )
         return false;

      if ( !self.WithinDistance( patrolNode->origin , MIN_DISTANCE_TO_PATROL_NODE ) )
         return false;

      if ( patrolNode == _lastPatrolNodeChecked )
         return false;

      _lastPatrolNodeChecked = patrolNode;

      tendencyToPatrol = self.personality->GetTendency( "patrol" );
      
      if ( G_Random() <= tendencyToPatrol )
         return true;         

      }

   return false;
}

bool PatrolWorkWander::_wantsToWander( Actor &self )
{
	if ( _wanderFailures > 5 )
		return false;

	return true;
}
void PatrolWorkWander::_selectState( Actor &self )
{

	if ( _wantsToPatrol( self ) )
		{
		_setupPatrol( self );
		_state = PATROLWORKWANDER_PATROL;
		return;
		}

	if ( _wantsToWork( self ) )
		{
		_setupWork( self );
		_state = PATROLWORKWANDER_WORK;
		return;
		}

	if ( _wantsToWander( self ) )
		{
		_setupWander( self );
		_state = PATROLWORKWANDER_WANDER;
		return;
		}

   _state = PATROLWORKWANDER_HOLD;
   _setupHold( self );
}




void PatrolWorkHibernate::AnimDone( Event *ev )
{
	switch ( _state )
	{
	case PATROLWORKHIBERNATE_HIBERNATE:
		_hibernateComponent.AnimDone( ev );
	break;

	case PATROLWORKHIBERNATE_HACK:	   
		_state = PATROLWORKHIBERNATE_SELECT_STATE;
	break;

	case PATROLWORKHIBERNATE_HIBERNATE_HOLD:
		_state = PATROLWORKHIBERNATE_SELECT_STATE;
	break;

	case PATROLWORKHIBERNATE_PATROL:
		_patrolComponent.AnimDone( ev );
	break;

	case PATROLWORKHIBERNATE_WORK:
		_workComponent.AnimDone( ev );
	break;
	}
   
}

BehaviorReturnCode_t	PatrolWorkHibernate::Evaluate ( Actor &self )
{      
	float tendencyToHibernate;

	switch ( _state )
	{
		case PATROLWORKHIBERNATE_SELECT_STATE:
			_selectState( self );
		break;
   
		case PATROLWORKHIBERNATE_PATROL:
			_patrol( self );
		break;

		case PATROLWORKHIBERNATE_WORK:
			_work( self );
		break;

		case PATROLWORKHIBERNATE_HIBERNATE:
			tendencyToHibernate = self.personality->GetTendency( "hibernate" );

			if ( G_Random() > tendencyToHibernate )
				{
				self.SetAnim( "alcove_activate" , EV_Actor_NotifyBehavior );	
				_state = PATROLWORKHIBERNATE_HACK;
				return BEHAVIOR_EVALUATING;
				}
		
			_hibernate( self );
		break;

		case PATROLWORKHIBERNATE_HOLD:
			_hold( self );
		break;

		case PATROLWORKHIBERNATE_HIBERNATE_HOLD:
			self.SetAnim( "alcove_idle" );	
			tendencyToHibernate = self.personality->GetTendency( "hibernate" );

			if ( G_Random() > tendencyToHibernate )
				{
				self.SetAnim( "alcove_activate" , EV_Actor_NotifyBehavior );	
				}

		break;

		case PATROLWORKHIBERNATE_FAILED:
			return BEHAVIOR_FAILED;
		break;      
	}

	return BEHAVIOR_EVALUATING; 
}

void PatrolWorkHibernate::_init( Actor &self )
{
	_nextStateCheck			= 0.0f;
	_nextWorkCheck			= 0.0f;
	_nextHibernateCheck		= 0.0f;

	_state = PATROLWORKHIBERNATE_SELECT_STATE;
}

void PatrolWorkHibernate::SetArgs ( Event *ev)
{

}





void PatrolWorkHibernate::_hibernate( Actor &self )
{
	BehaviorReturnCode_t hibernateResult;
	hibernateResult = _hibernateComponent.Evaluate( self );

	if ( hibernateResult == BEHAVIOR_SUCCESS )
		_state = PATROLWORKHIBERNATE_SELECT_STATE;

	if ( hibernateResult == BEHAVIOR_FAILED )
		_state = PATROLWORKHIBERNATE_FAILED;
}
