//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/behaviors_general.cpp                          $
// $Revision:: 190                                                            $
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
#include "actor_posturecontroller.h"
#include "behavior.h"
#include "actor.h"
#include "doors.h"
#include "object.h"
#include "explosion.h"
#include "weaputils.h"
#include "player.h"
#include "behaviors_general.h"
#include "rageai.h" 

extern Event EV_Contents;
extern Event EV_HelperNodeCommand;




#define YAW_MAXIMUM                100.0f
#define TURN_THRESHOLD_MAJOR        12.0f
#define TURN_THRESHOLD_MINOR         3.0f
#define CONE_OF_FIRE                14.0f
#define MAJOR_YAW_CHANGE            10.0f
#define MAJOR_PITCH_CHANGE           8.0f



#define WORK_NODE_OCCUPIED_TIME     16.5f

#define DUCK_DISTANCE              350.0f
#define RUN_DISTANCE               90.0f

#define POINT_BLANK                 50.0f

#define MIN_POSITION_NODE_DIST      50.0f
#define MAX_POSITION_NODE_DIST     256.0f


#define WORK_HELPER_NODE_STATE_TURN_TO_WORK       1
#define WORK_HELPER_NODE_STATE_START_ANIM         2
#define WORK_HELPER_NODE_STATE_PLAY_ANIM          3
#define WORK_HELPER_NODE_STATE_WAITING_FOR_ANIM   4
#define WORK_HELPER_NODE_STATE_FINISH             5

#define PATROL_HELPER_NODE_STATE_GET_NEXT_NODE    1
#define PATROL_HELPER_NODE_STATE_MOVING           2
#define PATROL_HELPER_NODE_STATE_AT_NODE          3
#define PATROL_HELPER_NODE_STATE_WAITING          4
#define PATROL_HELPER_NODE_STATE_WAITING_FOR_ANIM 5
#define PATROL_HELPER_NODE_STATE_FINISHED         6







CLASS_DECLARATION( Behavior, WarpToPosition, NULL )
	{
		{ &EV_Behavior_Args,			&WarpToPosition::SetArgs		},
		{ NULL,							NULL		}
	};


void WarpToPosition::SetArgs ( Event *ev)
	{
	_position = ev->GetVector( 1 );
	}


void WarpToPosition::Begin( Actor &self )
	{	
	_state = WARP_TO_POSITION_CHECK_POSITION;	
	}


BehaviorReturnCode_t WarpToPosition::Evaluate ( Actor &self )
	{	
	switch ( _state )
		{
		case WARP_TO_POSITION_CHECK_POSITION:
			checkPosition( self );
		break;

		case WARP_TO_POSITION_WARP:
			warpToPosition( self );
		break;

		case WARP_TO_POSITION_FAILED:
			return BEHAVIOR_FAILED;
		break;

		case WARP_TO_POSITION_SUCCESS:
			return BEHAVIOR_SUCCESS;
		break;
		}

	return BEHAVIOR_EVALUATING;	
	}


void WarpToPosition::End ( Actor &self	)
	{
	}

void WarpToPosition::checkPosition( Actor &self )
{
	trace_t trace;
	trace = G_Trace( _position + Vector( "0 0 64" ), self.mins, self.maxs, _position - Vector( "0 0 128" ), &self, self.edict->clipmask, false, "WarpToPosition" );
	_position = trace.endpos;

	if ( trace.allsolid )
		checkPositionFailed( self );		
	else
		_state = WARP_TO_POSITION_WARP;
}

void WarpToPosition::checkPositionFailed( Actor &self )
{
	self.SetAnim( "idle" );
	_state = WARP_TO_POSITION_FAILED;
}

void WarpToPosition::warpToPosition( Actor &self )
{
	self.setOrigin( _position );
	self.NoLerpThisFrame();
	_state = WARP_TO_POSITION_SUCCESS;
}






const float WarpToEntity::DIST_TO_ENTITY = 64.0f;

CLASS_DECLARATION( Behavior, WarpToEntity, NULL )
	{
		{ &EV_Behavior_Args,			&WarpToEntity::SetArgs		},
		{ NULL,							NULL		}
	};


void WarpToEntity::SetArgs ( Event *ev)
{
	_entity = ev->GetEntity( 1 );
}


void WarpToEntity::Begin( Actor &self )
{	
	_state = WARP_TO_ENTITY_SELECT_POSITION;
	_position = POSITION_REAR;
}


BehaviorReturnCode_t WarpToEntity::Evaluate ( Actor &self )
{	
	switch ( _state )
	{
	case WARP_TO_ENTITY_SELECT_POSITION:
		selectPosition( self );
	break;

	case WARP_TO_ENTITY_WARP:
		warpToPosition( self );
	break;

	case WARP_TO_ENTITY_FAILED:
		return BEHAVIOR_FAILED;
	break;

	case WARP_TO_ENTITY_SUCCESS:
		return BEHAVIOR_SUCCESS;
	break;
	}

	return BEHAVIOR_EVALUATING;	
}


void WarpToEntity::End ( Actor &self	)
{
}

void WarpToEntity::selectPosition( Actor &self )
{
	Vector entAngles;
	Vector f,l,u;

	entAngles = _entity->angles;
	entAngles.AngleVectors( &f, &l, &u );			

	switch ( _position )
	{
	case POSITION_REAR:
		_destination = ( f * -1 ) * 64.0f;		
	break;

	case POSITION_LEFT:
		_destination = l * 64.0f;		
	break;

	case POSITION_RIGHT:
		_destination = ( l * -1 ) * 64.0f;
	break;

	case POSITION_FRONT:
		_destination = f * 64.0f;
	break;
	}

	_destination += _entity->origin;

	setupWarp( self );
	_state = WARP_TO_ENTITY_WARP;
	
}

void WarpToEntity::setupWarp( Actor &self )
{
	_warp.SetPosition( _destination );
	_warp.Begin( self );
}

void WarpToEntity::warpToPosition( Actor &self )
{
	BehaviorReturnCode_t result;
	result = _warp.Evaluate( self );

	if ( result == BEHAVIOR_SUCCESS )
		{
		_state = WARP_TO_ENTITY_SUCCESS;
		return;
		}

	if ( result != BEHAVIOR_EVALUATING )
		{
		warpToPositionFailed( self );
		return;
		}
}


void WarpToEntity::warpToPositionFailed( Actor &self )
{
	_position++;

	if ( _position >= POSITION_NUMBER_OF_POSITIONS )
		{
		_state = WARP_TO_ENTITY_FAILED;
		return;
		}

	_state = WARP_TO_ENTITY_SELECT_POSITION;
}



CLASS_DECLARATION( Behavior, GotoEntity, NULL )
	{
		{ &EV_Behavior_Args,			&GotoEntity::SetArgs },      
      { NULL, NULL }
	};



GotoEntity::GotoEntity()
{
	_dist = 96.0f;
}

void GotoEntity::SetArgs( Event *ev )
   {
   _dist = 96.0f;
   
   _anim = ev->GetString( 1 );

   if ( ev->NumArgs() > 1 )
      _dist = ev->GetFloat( 2 );
   
   }



void GotoEntity::Begin( Actor &self )
	{       
    if ( _entity )
		{
		_chase.SetGoal( _entity, _dist,  self );
		_chase.Begin( self );
		}

	self.SetAnim( _anim );
	}



BehaviorReturnCode_t	GotoEntity::Evaluate( Actor &self )
	{  
   unsigned int chaseResult;

   if ( !_entity )
      return BEHAVIOR_FAILED;

   
   chaseResult = _chase.Evaluate( self );


   switch ( chaseResult )
      {
      case Steering::SUCCESS: 
         return BEHAVIOR_SUCCESS;
      break;

      case Steering::EVALUATING: 
         return BEHAVIOR_EVALUATING;
      break;

      case Steering::FAILED: 
		 self.SetAnim( "idle" );
		 SetFailureReason( "Steering returned FAILED" );
		 self.AddStateFlag( STATE_FLAG_STEERING_FAILED );		 		 
         return BEHAVIOR_FAILED;
      break;

      case Steering::FAILED_BLOCKED_BY_ENEMY: 
		  self.SetAnim( "idle" );
		  SetFailureReason( "Steering returned BLOCKED_BY_ENEMY" );
		  self.AddStateFlag( STATE_FLAG_STEERING_FAILED );		 
         return BEHAVIOR_FAILED_STEERING_BLOCKED_BY_ENEMY;
      break;

      case Steering::FAILED_BLOCKED_BY_CIVILIAN:         
		  self.SetAnim( "idle" );
		  SetFailureReason( "Steering returned BLOCKED_BY_CIVILIAN" );
		  self.AddStateFlag( STATE_FLAG_STEERING_FAILED );		 
         return BEHAVIOR_FAILED_STEERING_BLOCKED_BY_CIVILIAN;
      break;

      case Steering::FAILED_BLOCKED_BY_FRIEND: 		  
		  self.SetAnim( "idle" );
		  SetFailureReason( "Steering returned BLOCKED_BY_FRIEND" );
		  self.AddStateFlag( STATE_FLAG_STEERING_FAILED );		 
         return BEHAVIOR_FAILED_STEERING_BLOCKED_BY_FRIEND;
      break;

      case Steering::FAILED_BLOCKED_BY_TEAMMATE: 
		  self.SetAnim( "idle" );
		  SetFailureReason( "Steering returned BLOCKED_BY_TEAMMATE" );
		  self.AddStateFlag( STATE_FLAG_STEERING_FAILED );		 
         return BEHAVIOR_FAILED_STEERING_BLOCKED_BY_TEAMMATE;
      break;

      case Steering::FAILED_BLOCKED_BY_WORLD:
		  self.SetAnim( "idle" );
		  SetFailureReason( "Steering returned BLOCKED_BY_WORLD" );
		  self.AddStateFlag( STATE_FLAG_STEERING_FAILED );		 
         return BEHAVIOR_FAILED_STEERING_BLOCKED_BY_WORLD;
      break;

      case Steering::FAILED_BLOCKED_BY_DOOR: 
		  self.SetAnim( "idle" );
		  SetFailureReason( "Steering returned BLOCKED_BY_DOOR" );
		  self.AddStateFlag( STATE_FLAG_STEERING_FAILED );		 
         return BEHAVIOR_FAILED_STEERING_BLOCKED_BY_DOOR;
      break;

      case Steering::FAILED_CANNOT_GET_TO_PATH: 
		  self.AddStateFlag( STATE_FLAG_NO_PATH );
		  self.SetAnim( "idle" );
		  SetFailureReason( "Steering returned CANNOT_GET_TO_PATH" );
		  self.AddStateFlag( STATE_FLAG_STEERING_FAILED );		 
         return BEHAVIOR_FAILED_STEERING_CANNOT_GET_TO_PATH;
      break;

      case Steering::FAILED_NO_PATH: 
		  self.AddStateFlag( STATE_FLAG_NO_PATH );

		  self.SetAnim( "idle" );
		  SetFailureReason( "Steering returned NO_PATH" );
		  self.AddStateFlag( STATE_FLAG_STEERING_FAILED );		 
         return BEHAVIOR_FAILED_STEERING_NO_PATH;
      break;

      case Steering::ERROR: 
		  self.SetAnim( "idle" );
         gi.Error( ERR_DROP, "Steering Error");         
      break;

      }

   return BEHAVIOR_EVALUATING;   
   }



void GotoEntity::End(Actor &self)
	{   
   _chase.End( self );
	}









CLASS_DECLARATION( Behavior, GotoPoint, NULL )
	{
		{ &EV_Behavior_Args,			&GotoPoint::SetArgs },      
      { NULL, NULL }
	};



void GotoPoint::SetArgs( Event *ev )
   {
   dist = 96.0f;
   
   anim = ev->GetString( 1 );

   if ( ev->NumArgs() > 1 )
      dist = ev->GetFloat( 2 );
   
   }



void GotoPoint::Begin( Actor &self )
	{       
	_chaseFailed = false;
   _chase.SetGoal( point, dist,  self );

   unsigned int testResult;
   testResult = _chase.Evaluate( self );

   if ( testResult == Steering::EVALUATING )
		self.SetAnim( anim );
   else
		{
		self.SetAnim ( "idle" );
		_chaseFailed = true;
		}
	}



BehaviorReturnCode_t	GotoPoint::Evaluate( Actor &self )
	{   
    unsigned int chaseResult;

	if ( self.state_flags & STATE_FLAG_STUCK )
		{
		 self.SetAnim( "idle" );
		 SetFailureReason( "I'm stuck!!!!!!" );
         return BEHAVIOR_FAILED;
		}

   chaseResult = _chase.Evaluate( self );


   switch ( chaseResult )
      {
      case Steering::SUCCESS: 
         return BEHAVIOR_SUCCESS;
      break;

      case Steering::EVALUATING: 
		  if ( _chaseFailed )
			{
			self.SetAnim( anim );
			_chaseFailed = false;
			}	

         return BEHAVIOR_EVALUATING;
      break;

      case Steering::FAILED: 
		  self.SetAnim( "idle" );
         return BEHAVIOR_FAILED;
      break;

      case Steering::FAILED_BLOCKED_BY_ENEMY: 
		  self.SetAnim( "idle" );
         return BEHAVIOR_FAILED_STEERING_BLOCKED_BY_ENEMY;
      break;

      case Steering::FAILED_BLOCKED_BY_CIVILIAN:         
		  self.SetAnim( "idle" );
         return BEHAVIOR_FAILED_STEERING_BLOCKED_BY_CIVILIAN;
      break;

      case Steering::FAILED_BLOCKED_BY_FRIEND: 
		  self.SetAnim( "idle" );
         return BEHAVIOR_FAILED_STEERING_BLOCKED_BY_FRIEND;
      break;

      case Steering::FAILED_BLOCKED_BY_TEAMMATE: 
		  self.SetAnim( "idle" );
         return BEHAVIOR_FAILED_STEERING_BLOCKED_BY_TEAMMATE;
      break;

      case Steering::FAILED_BLOCKED_BY_WORLD:
		  self.SetAnim( "idle" );
         return BEHAVIOR_FAILED_STEERING_BLOCKED_BY_WORLD;
      break;

      case Steering::FAILED_BLOCKED_BY_DOOR: 
		  self.SetAnim( "idle" );
         return BEHAVIOR_FAILED_STEERING_BLOCKED_BY_DOOR;
      break;

      case Steering::FAILED_CANNOT_GET_TO_PATH: 
		  self.AddStateFlag( STATE_FLAG_NO_PATH );
		  self.SetAnim( "idle" );
         return BEHAVIOR_FAILED_STEERING_CANNOT_GET_TO_PATH;
      break;

      case Steering::FAILED_NO_PATH: 
		  self.AddStateFlag( STATE_FLAG_NO_PATH );
		  self.SetAnim( "idle" );
         return BEHAVIOR_FAILED_STEERING_NO_PATH;
      break;

      case Steering::ERROR: 
		  self.SetAnim( "idle" );
         gi.Error( ERR_DROP, "Steering Error");         
      break;

      }


   return BEHAVIOR_EVALUATING;   
   }



void GotoPoint::End(Actor &self)
	{   
   _chase.End( self );
	}



void GotoPoint::SetPoint( const Vector &position )
   {
   point = position;
   }



void GotoPoint::SetAnim( const str &animName )
   {
   anim = animName;   
   }



void GotoPoint::SetDistance( float distance )
   {
   dist = distance;
   }




CLASS_DECLARATION( Behavior, MoveDirectlyToPoint, NULL )
{
	{ &EV_Behavior_Args,			&MoveDirectlyToPoint::SetArgs },      
	{ NULL, NULL }
};

void MoveDirectlyToPoint::SetArgs( Event *ev )
{
	_dist = 96.0f;
	
	_anim = ev->GetString( 1 );
	
	if ( ev->NumArgs() > 1 )
		_dist = ev->GetFloat( 2 );
}

void MoveDirectlyToPoint::Begin( Actor &self )
{       
	_dist = 16.0f;
	_motion.SetRadius( _dist );
}

BehaviorReturnCode_t	MoveDirectlyToPoint::Evaluate( Actor &self )
{   
	self.SetAnim( _anim );
	
	unsigned int motionResult = _motion.Evaluate( self );
	
	
	switch ( motionResult )
	{
	case Steering::SUCCESS: 
		return BEHAVIOR_SUCCESS;
		break;
		
	case Steering::EVALUATING: 
		return BEHAVIOR_EVALUATING;
		break;
	}
	
	
	return BEHAVIOR_EVALUATING;   
}

void MoveDirectlyToPoint::End( Actor &self )
{   
	_motion.End( self );
}

void MoveDirectlyToPoint::SetAnim( const str &animName )
{
	_anim = animName;   
}

void MoveDirectlyToPoint::SetPoint( const Vector &position )
{
	_motion.SetDestination( position );
}

void MoveDirectlyToPoint::SetDistance( const float distance )
{
	_dist = distance;
}



const float GotoSpecified::DIST_TO_TARGET_POSITION = 16.0f;
const float GotoSpecified::DIST_TO_TARGET_ENTITY   = 64.0f;


CLASS_DECLARATION( Behavior, GotoSpecified, NULL )
	{
		{ &EV_Behavior_Args,			&GotoSpecified::SetArgs		},
		{ &EV_Behavior_AnimDone,		&GotoSpecified::AnimDone	},
		{ NULL,							NULL		}
	};


void GotoSpecified::SetArgs ( Event *ev)
{
	TargetList *tlist;
	PathNode   *node;
	str        parm_shadow1;

	_state = GOTO_SPEC_FAILED;
	_turnAtEnd = false ;
	_maxFailures = 5;

	_anim = ev->GetString( 1 );

	
	if ( ev->IsVectorAt( 2 ) )
		{
		_targetPosition = ev->GetVector( 2 );
		_state = GOTO_SPEC_CHASE_TARGET;
		_mode = GOTO_SPEC_CHASE_POSITION;
		}	
	else if ( ev->IsEntityAt( 2 ) )
		{		
		_targetEntity = ev->GetEntity( 2 );
		_state = GOTO_SPEC_CHASE_TARGET;
		_mode = GOTO_SPEC_CHASE_ENTITY;
		}	
	else 
		{	
		parm_shadow1 = ev->GetString( 2 );
		node = thePathManager.FindNode( parm_shadow1.c_str() );

		if ( node )
			{
			_targetPosition = node->origin;
			_endAngles = node->angles;
			_state = GOTO_SPEC_CHASE_TARGET;
			_mode = GOTO_SPEC_CHASE_POSITION;
			_turnAtEnd = true ;
			}
		else
			{
			tlist = world->GetTargetList( parm_shadow1 );
			if (tlist->list.NumObjects() > 0 )
				{
				_targetEntity = tlist->list.ObjectAt( 1 );
				_state = GOTO_SPEC_CHASE_TARGET;
				_mode = GOTO_SPEC_CHASE_ENTITY;
				}
			else
				{
				HelperNode *helperNode;


				helperNode = HelperNode::GetTargetedHelperNode( parm_shadow1 );

				if ( helperNode )
					{
					_targetPosition = helperNode->origin;
					_endAngles = helperNode->angles;
					_state = GOTO_SPEC_CHASE_TARGET;
					_mode = GOTO_SPEC_CHASE_POSITION;
					_turnAtEnd = true;
					}
				}
			}
		}

	if ( ev->NumArgs() > 2 )
		{
		if ( ev->IsEntityAt( 3 ) )
			_headwatchTarget = ev->GetEntity( 3 );
		else
			{
			parm_shadow1 = ev->GetString( 3 );
			if ( stricmp( parm_shadow1.c_str() , "none" ) )
				{
				tlist = world->GetTargetList( parm_shadow1 );
				if (tlist->list.NumObjects() > 0 )
					{
					_headwatchTarget = tlist->list.ObjectAt( 1 );
					}
				}
			}

		}

	if ( ev->NumArgs() > 3 )
		_forceToTarget = ev->GetBoolean( 4 );
	else
		_forceToTarget = false;

	if ( ev->NumArgs() > 4 )
		_maxFailures = ev->GetInteger( 5 );

	assert ( _state != GOTO_SPEC_FAILED );

}


void GotoSpecified::AnimDone ( Event *ev )
{
}

void GotoSpecified::Begin( Actor &self )
{
	if ( _mode == GOTO_SPEC_CHASE_POSITION )
		setupChasePosition( self );
	else if ( _mode == GOTO_SPEC_CHASE_ENTITY )
		setupChaseEntity( self );

	_moveFailures = 0;
	_holdTime = 0.0f;

	if ( _headwatchTarget )
		self.SetHeadWatchTarget((Entity *)_headwatchTarget);  

	_attemptedPathWarp = false;

	str torsoAnim;
	self.ClearTorsoAnim();
	if ( self.combatSubsystem->HaveWeapon() )
		{
		if ( self.enemyManager->HasEnemy() )
			torsoAnim = self.combatSubsystem->GetAnimForMyWeapon( "CombatGunIdle" );
		else
			torsoAnim = self.combatSubsystem->GetAnimForMyWeapon( "IdleGunIdle" );
		}

	if ( torsoAnim.length() )
		{
		self.ClearTorsoAnim();
		self.SetAnim( torsoAnim, NULL , torso );
		}


}


BehaviorReturnCode_t GotoSpecified::Evaluate ( Actor &self )
{	
	switch ( _state )
	{
	case GOTO_SPEC_CHASE_TARGET:
		if ( _mode == GOTO_SPEC_CHASE_POSITION )
			chasePosition( self );
		else
			chaseEntity( self );
	break;

	case GOTO_SPEC_HOLD:
		hold( self );
	break;

	case GOTO_SPEC_WARP_TO_PATH:
		warpToNearestPathNode( self );
	break;

	case GOTO_SPEC_WARP_TO_DESTINATION:
		warpToDestination( self );
	break;

	case GOTO_SPEC_SUCCESS:
		if ( _turnAtEnd )
		{
			setAngles( self );
		}
		self.SetAnim( "idle" );
		return BEHAVIOR_SUCCESS;
	break;
		
	case GOTO_SPEC_FAILED:
		self.SetAnim( "idle" );
		return BEHAVIOR_FAILED;
	break;
	}

	return BEHAVIOR_EVALUATING;	
}


void GotoSpecified::End ( Actor &self	)
{
}

void GotoSpecified::setupChaseEntity( Actor &self )
{
	_chaseEntity.SetAnim( _anim );
	_chaseEntity.SetDistance( DIST_TO_TARGET_ENTITY );
	_chaseEntity.SetEntity( self, _targetEntity );
	_chaseEntity.Begin( self );
}

void GotoSpecified::setupChasePosition( Actor &self )
{
	_chasePosition.SetAnim( _anim );
	_chasePosition.SetDistance( DIST_TO_TARGET_POSITION );
	_chasePosition.SetPoint( _targetPosition );
	_chasePosition.Begin( self );
}

void GotoSpecified::setupWarpToPathNode( Actor &self )
{
	PathNode *goalNode = thePathManager.NearestNode( _targetPosition );
	if ( !goalNode )
		{
		_state = GOTO_SPEC_WARP_TO_DESTINATION;
		setupWarpToDestination( self );
		return;
		}
	
	_warpToPosition.SetPosition( goalNode->origin );
	_warpToPosition.Begin( self );
		
}

void GotoSpecified::setupWarpToDestination( Actor &self )
{
	if ( _mode == GOTO_SPEC_CHASE_POSITION )
		{
		_warpToPosition.SetPosition( _targetPosition );
		_warpToPosition.Begin( self );
		return;
		}
	else 
		{
		_warpToEntity.SetEntity( _targetEntity );
		_warpToPosition.Begin( self );
		return;
		}

}

void GotoSpecified::setupHold( Actor &self )
{
	self.SetAnim( "idle" );
	_holdTime = level.time + G_Random() + .5f;	
}

void GotoSpecified::chaseEntity( Actor &self )
{
	BehaviorReturnCode_t result;
	result = _chaseEntity.Evaluate( self );

	switch ( result )
		{
		case BEHAVIOR_SUCCESS:
			_state = GOTO_SPEC_SUCCESS;
			return;
		break;

		case BEHAVIOR_FAILED:
			chaseFailed( self );
			SetFailureReason("_chaseEntity returned BEHAVIOR_FAILED" );
			return;
		break;

		case BEHAVIOR_FAILED_STEERING_BLOCKED_BY_ENEMY:
			chaseFailed( self );
			SetFailureReason("_chaseEntity returned BEHAVIOR_FAILED_STEERING_BLOCKED_BY_ENEMY");
			return;
		break;

		case BEHAVIOR_FAILED_STEERING_BLOCKED_BY_CIVILIAN:
			chaseFailed( self );
			SetFailureReason("_chaseEntity returned BEHAVIOR_FAILED_STEERING_BLOCKED_BY_CIVILIAN");
			return;
		break;

		case BEHAVIOR_FAILED_STEERING_BLOCKED_BY_FRIEND:
			chaseFailed( self );
			SetFailureReason("_chaseEntity returned BEHAVIOR_FAILED_STEERING_BLOCKED_BY_FRIEND");
			return;
		break;

		case BEHAVIOR_FAILED_STEERING_BLOCKED_BY_TEAMMATE:
			chaseFailed( self );
			SetFailureReason( "_chaseEntity returned BEHAVIOR_FAILED_STEERING_BLOCKED_BY_TEAMMATE");
			return;
		break;

		case BEHAVIOR_FAILED_STEERING_BLOCKED_BY_WORLD:
			chaseFailed( self );
			SetFailureReason("_chaseEntity returned BEHAVIOR_FAILED_STEERING_BLOCKED_BY_WORLD");
			return;
		break;

		case BEHAVIOR_FAILED_STEERING_BLOCKED_BY_DOOR:
			chaseFailed( self );
			SetFailureReason("_chaseEntity returned BEHAVIOR_FAILED_STEERING_BLOCKED_BY_DOOR");
			return;
		break;

		case BEHAVIOR_FAILED_STEERING_CANNOT_GET_TO_PATH:
			chaseFailed( self );
			SetFailureReason("_chaseEntity returned BEHAVIOR_FAILED_STEERING_CANNOT_GET_TO_PATH");
			return;
		break;

		case BEHAVIOR_FAILED_STEERING_NO_PATH:
			chaseFailed( self );
			SetFailureReason("_chaseEntity returned BEHAVIOR_FAILED_STEERING_NO_PATH");
			return;
		break;

		case BEHAVIOR_INVALID:
			chaseFailed( self );
			SetFailureReason("_chaseEntity returned BEHAVIOR_INVALID");
			return;
		break;
		default:
		break;
		}

	_moveFailures = 0;
}

void GotoSpecified::chasePosition( Actor &self )
{
	BehaviorReturnCode_t result;
	result = _chasePosition.Evaluate( self );

	switch ( result )
		{
		case BEHAVIOR_SUCCESS:
			_state = GOTO_SPEC_SUCCESS;
			return;
		break;

		case BEHAVIOR_FAILED:
			chaseFailed( self );
			SetFailureReason("_chasePosition returned BEHAVIOR_FAILED" );
			return;
		break;

		case BEHAVIOR_FAILED_STEERING_BLOCKED_BY_ENEMY:
			chaseFailed( self );
			SetFailureReason("_chasePosition returned BEHAVIOR_FAILED_STEERING_BLOCKED_BY_ENEMY");
			return;
		break;

		case BEHAVIOR_FAILED_STEERING_BLOCKED_BY_CIVILIAN:
			chaseFailed( self );
			SetFailureReason("_chasePosition returned BEHAVIOR_FAILED_STEERING_BLOCKED_BY_CIVILIAN");
			return;
		break;

		case BEHAVIOR_FAILED_STEERING_BLOCKED_BY_FRIEND:
			chaseFailed( self );
			SetFailureReason("_chasePosition returned BEHAVIOR_FAILED_STEERING_BLOCKED_BY_FRIEND");
			return;
		break;

		case BEHAVIOR_FAILED_STEERING_BLOCKED_BY_TEAMMATE:
			chaseFailed( self );
			SetFailureReason( "_chasePosition returned BEHAVIOR_FAILED_STEERING_BLOCKED_BY_TEAMMATE");
			return;
		break;

		case BEHAVIOR_FAILED_STEERING_BLOCKED_BY_WORLD:
			chaseFailed( self );
			SetFailureReason("_chasePosition returned BEHAVIOR_FAILED_STEERING_BLOCKED_BY_WORLD");
			return;
		break;

		case BEHAVIOR_FAILED_STEERING_BLOCKED_BY_DOOR:
			chaseFailed( self );
			SetFailureReason("_chasePosition returned BEHAVIOR_FAILED_STEERING_BLOCKED_BY_DOOR");
			return;
		break;

		case BEHAVIOR_FAILED_STEERING_CANNOT_GET_TO_PATH:
			chaseFailed( self );
			SetFailureReason("_chasePosition returned BEHAVIOR_FAILED_STEERING_CANNOT_GET_TO_PATH");
			return;
		break;

		case BEHAVIOR_FAILED_STEERING_NO_PATH:
			chaseFailed( self );
			SetFailureReason("_chasePosition returned BEHAVIOR_FAILED_STEERING_NO_PATH");
			return;
		break;

		case BEHAVIOR_INVALID:
			chaseFailed( self );
			SetFailureReason("_chasePosition returned BEHAVIOR_INVALID");
			return;
		break;
		default:
		break;
		}

	_moveFailures = 0;
}



void GotoSpecified::warpToNearestPathNode( Actor &self )
{
	BehaviorReturnCode_t result;
	result = _warpToPosition.Evaluate( self );

	_attemptedPathWarp = true;
	_moveFailures = 0;

	if ( result != BEHAVIOR_EVALUATING )
		{
		str tname;
		tname = self.TargetName();

		gi.WDPrintf( "=============================================================\n" );
		gi.WDPrintf( "Actor %s is failing to reach destination\n" , tname.c_str() );
		gi.WDPrintf( "=============================================================\n" );
		gi.WDPrintf( "Reported Reason\n" );
		gi.WDPrintf( "%s\n" , GetFailureReason().c_str() );
		gi.WDPrintf( "=============================================================\n" );		

		_state = GOTO_SPEC_CHASE_TARGET;
		if ( _mode == GOTO_SPEC_CHASE_POSITION )
			{
			setupChasePosition( self );			
			}
		else
			{
			setupChaseEntity( self );
			}
		}

}


void GotoSpecified::warpToDestination( Actor &self )
{
	BehaviorReturnCode_t result;
	result = BEHAVIOR_EVALUATING;

	if ( _mode == GOTO_SPEC_CHASE_POSITION )
		result = _warpToPosition.Evaluate( self );
	
	if ( _mode == GOTO_SPEC_CHASE_ENTITY )
		result = _warpToEntity.Evaluate( self );


	if ( result == BEHAVIOR_SUCCESS )
		{
		_state = GOTO_SPEC_SUCCESS;
		return;
		}

	if ( result != BEHAVIOR_EVALUATING )
		{
		_state = GOTO_SPEC_FAILED;
		return;
		}

}

void GotoSpecified::hold( Actor &self )
{
	if ( level.time >= _holdTime )
		{
		if ( _mode == GOTO_SPEC_CHASE_POSITION )
			{
			setupChasePosition( self );
			_state = GOTO_SPEC_CHASE_TARGET ;
			}
		else
			{
			setupChaseEntity( self );	
			_state = GOTO_SPEC_CHASE_TARGET ;		
			}
		}
}


void GotoSpecified::setAngles( Actor &self )
{
	Vector angles;
	Vector animDir;

	_endAngles.AngleVectors( &animDir );

	if ( _mode == GOTO_SPEC_CHASE_POSITION )
		{
		self.setAngles( _endAngles );
		self.movementSubsystem->setAnimDir( animDir );
		}
}

void GotoSpecified::chaseFailed( Actor &self )
{
	str tname;
	tname = self.TargetName();

	if ( _mode == GOTO_SPEC_CHASE_POSITION )
		_chasePosition.End( self );
	else
		_chaseEntity.End( self );

	if ( _maxFailures > 0 && _moveFailures > _maxFailures )
		{
		if ( !_forceToTarget )
			{
			gi.WDPrintf( "=============================================================\n" );
			gi.WDPrintf( "Actor %s is failing to reach destination\n" , tname.c_str() );
			gi.WDPrintf( "=============================================================\n" );
			gi.WDPrintf( "Reported Reason\n" );
			gi.WDPrintf( "%s\n" , GetFailureReason().c_str() );
			gi.WDPrintf( "=============================================================\n" );		
			_state = GOTO_SPEC_FAILED;
			return;
			}

		setupWarpToPathNode( self );
		_state = GOTO_SPEC_WARP_TO_PATH;
		return;
		}

	_moveFailures++;	
	setupHold( self );
	_state = GOTO_SPEC_HOLD;
	return;	

}






const float MoveFromConeOfFire::CONE_OF_FIRE_RADIUS = 500.0f;



CLASS_DECLARATION( Behavior, MoveFromConeOfFire, NULL )
	{
		{ &EV_Behavior_Args,			&MoveFromConeOfFire::SetArgs },
		{ NULL,							NULL	}
	};


void MoveFromConeOfFire::SetArgs ( Event *ev)
	{
	if ( ev->NumArgs() > 0 )
		_anim = ev->GetString( 1 );   
	}


void MoveFromConeOfFire::Begin( Actor &self )
	{	
	Vector dir;

	movegoal = NULL;	
	
	dir = self.movementSubsystem->getAnimDir();
	dir = dir.toAngles();
	self.setAngles( dir );
   
	_nextsearch = 0.0f;
	_torsoAnim = "";

	_state = MOVE_FCOF_SEARCHING_FOR_SPOT;
	_stuckOnPlayer = false;
	_nextToObstacle = false;

	_chase.SetAnim( _anim );
	_chase.SetDistance( 16.0f );   
	_oldTurnSpeed = self.movementSubsystem->getTurnSpeed();
	self.movementSubsystem->setTurnSpeed( 360.0f );
	}


BehaviorReturnCode_t MoveFromConeOfFire::Evaluate ( Actor &self )
	{	
	if ( _stuckOnPlayer )
		return BEHAVIOR_FAILED;

	switch ( _state )
		{
		case MOVE_FCOF_SEARCHING_FOR_SPOT:
			_setDirectionVectors(self);                     
		break;

		case MOVE_FCOF_STATE_FOUND_SPOT:
			_foundDestination( self );
		break;

		case MOVE_FCOF_STATE_NO_SPOT:			
			_noDestination( self );
		break;

		case MOVE_FCOF_STATE_SEARCHING_FOR_NODE:
			_searchForNode( self );
		break;

		case MOVE_FCOF_STATE_FOUND_NODE :
			_foundDestination( self );
		break;

		case MOVE_FCOF_SUCCESS:
			return BEHAVIOR_SUCCESS;
		break;

		case MOVE_FCOF_FAILED:
			return BEHAVIOR_FAILED;
		break;
		}

	return BEHAVIOR_EVALUATING;
	
	}


void MoveFromConeOfFire::End ( Actor &self	)
	{
	_chase.End( self );
	self.movementSubsystem->setTurnSpeed( _oldTurnSpeed ); 
	self.movementSubsystem->setMovingBackwards( false );
	self.movementSubsystem->setAdjustAnimDir( true );
	}


void MoveFromConeOfFire::_setDirectionVectors(Actor &self)
   {
	float	dot;
	Vector	selfToPlayer;
	Vector	playerAngles;   
	Player *player;   
   
	player = GetPlayer( 0 );
	assert( player );
   
	selfToPlayer = self.origin - player->origin;
	playerAngles = player->GetVAngles();
   
	playerAngles[PITCH] = 0.0f;
	playerAngles[ROLL]  = 0.0f;
	playerAngles.AngleVectors( NULL , &_left , NULL );
	assert( fSmallEnough( _left.z, fEpsilon()) );

	const Vector startPos = self.origin;

	dot = DotProduct( selfToPlayer, _left );

	_right = _left * -1;

	_right *= 75.0f;
	_right += startPos;

	_left *= 75.0f;
	_left += startPos;

	_right.z = startPos.z;
	_left.z = startPos.z;

     
	if ( dot <= 0 )
		{
		if ( !_checkDesiredMovement( self , startPos , _right ) )
			{
			gi.WDPrintf( "=============================\n" );
			gi.WDPrintf( "Right Failed, trying Left\n" );
			gi.WDPrintf( "Anim = %s\n" , _anim.c_str() );
			gi.WDPrintf( "=============================\n" );

			if ( _nextToObstacle )
				{
				_state = MOVE_FCOF_FAILED;
				return; 
				}

			
			_state = MOVE_FCOF_FAILED;
			return; 

			}
		}
	else
		{
		if ( !_checkDesiredMovement( self , startPos , _left ) )
			{
			gi.WDPrintf( "=============================\n" );
			gi.WDPrintf( "Left Failed, trying Right\n" );
			gi.WDPrintf( "Anim = %s\n" , _anim.c_str() );
			gi.WDPrintf( "=============================\n" );

			if ( _nextToObstacle )
				{
				_state = MOVE_FCOF_FAILED;
				return; 
				}


			_state = MOVE_FCOF_FAILED;
			return; 
			}  
		}

	_chase.Begin( self );
   }


bool MoveFromConeOfFire::_checkDesiredMovement( Actor &self , const Vector &startPos , const Vector &endPos )
	{
	trace_t trace;

	trace = G_Trace( startPos, self.mins, self.maxs, endPos, &self, self.edict->clipmask, false, "MoveFromConeOfFire: Right Direction Test" );      

	if ( trace.entityNum == 0 )
             _stuckOnPlayer = true;
	
	if ( trace.fraction < .35 )
		return false;

	if ( trace.fraction < .20 )
	{
		_nextToObstacle = true;
		return false;
	}

	_nextsearch = level.time + 0.25f;

	_destination = trace.endpos;

	Vector selfToDestination;
	selfToDestination = trace.endpos - startPos;
	selfToDestination *= .80f;
	_destination = selfToDestination + startPos;
		

	Vector selfToDestinationAngles = _destination - self.origin;
	Vector animAngles = self.movementSubsystem->getAnimDir();
	float yawDiff;

	selfToDestinationAngles = selfToDestinationAngles.toAngles();
	animAngles = animAngles.toAngles();
	yawDiff = AngleNormalize180(selfToDestinationAngles[YAW] - animAngles[YAW] );

	
	if ( yawDiff >= -45.0 && yawDiff <= 45.0 )
		_anim = "walk";

	if ( yawDiff >= -135.0 && yawDiff <= -45.0 )
		{
		_anim = "strafe_left_clear";		
		self.movementSubsystem->setAdjustAnimDir( false );
		}

	if ( yawDiff >= 45.0 && yawDiff <= 135.0f )
		{
		_anim = "strafe_right_clear";		
		self.movementSubsystem->setAdjustAnimDir( false );
		}

	if ( yawDiff >= 135.0 && yawDiff <= 180.0f )
		{
		_anim = "backpedal";
		self.movementSubsystem->setMovingBackwards( true );
		}

	if ( yawDiff <= -135.0 && yawDiff >= -180.0 )
		{
		_anim = "backpedal";
		self.movementSubsystem->setMovingBackwards( true );
		}

	if ( !self.torsoBehavior )
		{
		_torsoAnim = self.combatSubsystem->GetAnimForMyWeapon( "Idle" );
			if ( _torsoAnim.length() )
			{
			self.SetAnim( _torsoAnim, NULL , torso );
			}
		}

	_chase.SetDistance( 32.0f );
	_chase.SetPoint( _destination );
	_chase.SetAnim( _anim );

	_state = MOVE_FCOF_STATE_FOUND_SPOT;   
   	
	return true;
	}


void MoveFromConeOfFire::_foundDestination( Actor &self )
	{
	BehaviorReturnCode_t result;


	result = _chase.Evaluate( self );
   
	if ( result == BEHAVIOR_EVALUATING )
		return;

	if ( result != BEHAVIOR_SUCCESS ) 
		{
		_state = MOVE_FCOF_FAILED;
		return;
		}

	_chase.End( self );			               
	self.SetAnim( "idle" );               
	_state = MOVE_FCOF_SUCCESS;	
	}


void MoveFromConeOfFire::_noDestination( Actor &self )
	{
	_state = MOVE_FCOF_STATE_SEARCHING_FOR_NODE; 
	_nextsearch = 0.0f;	
	}


void MoveFromConeOfFire::_searchForNode( Actor &self )
	{
	if ( level.time > _nextsearch )
		movegoal = _FindNode( self );

	if ( !movegoal )
		{
		self.SetAnim( "idle" );
		return;
		}

	_state = MOVE_FCOF_STATE_FOUND_NODE;

	_nextsearch = level.time + 1.0f;	
	}


PathNode *MoveFromConeOfFire::_FindNode( Actor &self )
	{
	int			 i;
	float		 dot;
	float		 bestdot;
	float		 testdot;
	float		 newDot;
	Vector		 delta;
	Vector		 distanceToNode;
	Vector		 playerToSelf;
	Vector		 nodeToSelf;
	Vector       l;
	PathNode	*bestnode;
	PathNode	*node;	   
	Player		*player;

   
	bestdot  = 0.0f;
	testdot  = 0.0f;
	bestnode = NULL;

	player = GetPlayer( 0 );
	playerToSelf = self.origin - player->origin;

	for ( i = 0 ; i < thePathManager.NumNodes() ; i++ )   
		{
		node = thePathManager.GetNode( i );
      
		if ( !node )
			return NULL;

		distanceToNode = node->origin - self.origin;
		if ( distanceToNode.length() > CONE_OF_FIRE_RADIUS )
			continue;

		nodeToSelf = self.origin - node->origin;
      
		dot = DotProduct( playerToSelf, nodeToSelf ) ;
		if ( dot < 0 )
			{
			playerToSelf = playerToSelf.toAngles();
			playerToSelf.AngleVectors(NULL, &l, NULL );
			
			newDot = DotProduct ( l , nodeToSelf );
			testdot = abs( (int)newDot );
         
			if ( testdot > bestdot )
				{
				bestnode = node;
				bestdot = testdot;
				}
			}

		}


	if ( bestnode )
		{
		bestnode->occupiedTime = level.time + 1.5f;
		bestnode->entnum = self.entnum;

		_chase.SetPoint( bestnode->origin );
		_chase.Begin( self );

		return bestnode;

		}

	return NULL;
	}



CLASS_DECLARATION( Behavior, Strafe, NULL )
	{
		{ &EV_Behavior_Args,		&Strafe::SetArgs },
      { &EV_Behavior_AnimDone,		&Strafe::AnimDone }, 
		{ NULL, NULL }
	};



void Strafe::SetArgs ( Event *ev)
	{
   SetMode( ev->GetInteger( 1 ) );
 	}



void Strafe::AnimDone( Event *ev )
   {
   _strafeComplete = true;
   }



void Strafe::Begin( Actor &self )
	{
   _canStrafe     = true;
   _strafeComplete = false;

   _init( self );

	}


void Strafe::_init( Actor &self )
{
   float chance;   
   chance = G_Random();

   if ( mode != STRAFE_RANDOM )
      {
      _setAnim( self );
      return;
      }

   if ( chance < .5 )
   {
   mode = STRAFE_LEFT;
   _setAnim( self );

   if ( !_canStrafe )
      {
      mode = STRAFE_RIGHT;
      _setAnim( self );
      }
   }
   else
   {
   mode = STRAFE_RIGHT;
   _setAnim( self );

   if ( !_canStrafe )
      {
      mode = STRAFE_LEFT;
      _setAnim( self );
      }
   }

}

void Strafe::_setAnim( Actor &self )
   {
   str currentPostureState = self.postureController->getCurrentPostureName();

   switch ( mode )
      {
      case STRAFE_LEFT:
         if ( !self.checkLeftDirectionClear( 64.0f ) )
            {
            _canStrafe = false;
            return;
            }
		
		 if ( currentPostureState.length() )
			{
	         if ( currentPostureState == "STAND" )
		        _anim = "strafe_left";
			 else if ( currentPostureState == "DUCK" )
				_anim = "roll_left";
			}
		 else
			{
			_anim = "strafe_left";
			}
			
         
      break;

      case STRAFE_RIGHT:
         if ( !self.checkRightDirectionClear( 64.0f ) )
            {
            _canStrafe = false;
            return;
            }
		
		 if ( currentPostureState.length() )
			{
	         if ( currentPostureState == "STAND" )
		        _anim = "strafe_right";
			 else if ( currentPostureState == "DUCK" )
				_anim = "roll_right";
			}
		 else
			{
			 _anim = "strafe_right";
			}

      break; 
      }

   	if ( _canStrafe )
		{
		self.SetAnim( _anim , EV_Actor_NotifyBehavior , legs );
		}

   }



BehaviorReturnCode_t	Strafe::Evaluate ( Actor &self )
	{      

   if ( !_canStrafe )
      return BEHAVIOR_FAILED;

   if ( _strafeComplete )
      return BEHAVIOR_SUCCESS;

   return BEHAVIOR_EVALUATING; 
	}



void Strafe::End ( Actor &self	)
	{
   
	}


void Strafe::SetMode( unsigned int strafeMode )
   {
  if ( strafeMode >= STRAFE_NUMBER_OF_MODES )
      gi.Error( ERR_DROP, "Strafe -- Unknown Strafe Mode");

   mode = strafeMode;
   }





CLASS_DECLARATION( Behavior, CircleStrafeEntity, NULL )
	{
		{ &EV_Behavior_Args,			&CircleStrafeEntity::SetArgs },
		{ NULL, NULL }
	};



void CircleStrafeEntity::SetArgs ( Event *ev)
{
	_checkParameters(ev);

	_type			= ev->GetString ( 1 );
	_legAnim		= ev->GetString ( 2 );
	_radius			= ev->GetFloat  ( 3 );
	_clockwise		= ev->GetBoolean( 4 );

	if ( ev->NumArgs() > 4 )
		_testDistance	= ev->GetFloat ( 5 );
	else
		_testDistance = 80.0f;
}


void CircleStrafeEntity::Begin( Actor &self )
{
	_strafeTarget = _getStrafeTarget( self , _type );
	
	self.SetAnim( _legAnim, EV_Actor_NotifyBehavior );

	_failed = false;

	_lastPosition    = self.origin;
	_moveAttempts    = 0;
	_startWanderTime = 0;
	_recheckTime     = -1;
	_init( self );
}



BehaviorReturnCode_t	CircleStrafeEntity::Evaluate ( Actor &self )
{
	Vector Destination;
	Vector SelfToTarget;

	if ( !_strafeTarget )
		return BEHAVIOR_FAILED;

	if ( _failed )
		return BEHAVIOR_FAILED;

	SelfToTarget = _strafeTarget->origin - self.origin;
	SelfToTarget = SelfToTarget.toAngles();

	SelfToTarget[PITCH] = 0;
	SelfToTarget[ROLL]  = 0;
	_init( self );

	_holdAngles.AngleVectors( &Destination );
	
	if ( _checkIfStuck(self) )
		return BEHAVIOR_FAILED;
	else
		{
		self.movementSubsystem->setMoveDir( Destination );
		self.setAngles(SelfToTarget);		
		}

	return BEHAVIOR_EVALUATING;
}


void CircleStrafeEntity::_init( Actor &self )
{
	Vector SelfToTarget;	
	trace_t tracecw;
	float StrafeAngle;
	float FallbackAngle;
	float EmergencyAngle;

	if ( !_strafeTarget )
		{
		_failed = true;
		return;
		}
		
	SelfToTarget = _strafeTarget->origin - self.origin;
	_holdAngles = SelfToTarget.toAngles();
	_holdAngles.z = 0;
	_holdAngles.EulerNormalize();

	if ( self.WithinDistance( _strafeTarget , _radius ) )
		StrafeAngle = 90;
	else
		StrafeAngle = 45 + G_Random ( 10 );

	FallbackAngle = 80 + G_Random ( 10 );
	EmergencyAngle = 120 + G_Random ( 10 );
	if ( !_clockwise )
		{
		StrafeAngle *= -1;
		FallbackAngle *= -1;
		EmergencyAngle *= -1;
		}

   
	tracecw = self.Trace(StrafeAngle, _testDistance, "CircleStrafe Avoid Trace");

	if ( tracecw.fraction < 1.0 )
		{
			tracecw = self.Trace(FallbackAngle, _testDistance, "CircleStrafe Avoid Trace");	

			if ( tracecw.fraction < 1.0 )
				{
				tracecw = self.Trace(EmergencyAngle, _testDistance, "CircleStrafe Avoid Trace");	
				
				if ( tracecw.fraction < 1.0 )
					{
					_failed = true;
					return;
					}
				else
					{
					_holdAngles[YAW] += FallbackAngle;
					return;
					}
				}
			else
				{
				_holdAngles[YAW] += FallbackAngle;
				return;
				}
		}
	else
		{
		_holdAngles[YAW] += StrafeAngle;
		}


}

void CircleStrafeEntity::End ( Actor &self	)
	{
	_wander.End( self );
	}



Entity* CircleStrafeEntity::_getStrafeTarget( Actor &self, const str &target )
	{
	Entity *ent = NULL;
	
	if ( target == "player" )
		ent = GetPlayer( 0 );
	else if ( target == "enemy" )
		ent = self.enemyManager->GetCurrentEnemy();

	return ent;
	}


void CircleStrafeEntity::_checkParameters( Event *ev )
	{
	if ( ev->NumArgs() < 4 )
		gi.Error( ERR_DROP, "CircleStrafeEntity::_checkParameters -- Wrong Parameter Count");
	
	}



qboolean CircleStrafeEntity::_checkIfStuck(Actor &self)
	{
	Vector checkDistance = _lastPosition - self.origin;
	if ( checkDistance.length() < 1.5f )
		_moveAttempts++;		
	else
		{
		_moveAttempts = 0;
		_lastPosition = self.origin;
		}

	if ( _moveAttempts >= 3 )
		return true;
	else
		return false;
	}



CLASS_DECLARATION( Behavior, FollowInFormation, NULL )
	{
		{ &EV_Behavior_Args,			&FollowInFormation::SetArgs },
		{ NULL, NULL }
	};

void FollowInFormation::SetArgs ( Event *ev)
{
	_anim = ev->GetString ( 1 );

	if ( ev->NumArgs() > 1 )
		_emergencyDistance = ev->GetFloat( 2 );
	else
		_emergencyDistance = 0;

	if ( ev->NumArgs() > 2 )
		_catchupSpeed = ev->GetFloat( 3 );
	else
		_catchupSpeed = 10.0f;
}

void FollowInFormation::Begin( Actor &self )
{
 
	self.SetActorFlag( ACTOR_FLAG_FOLLOWING_IN_FORMATION , true );

	_selectedFollowTarget = false;

	_followDist = self.followTarget.maxRangeIdle;
	_followDistMin = _followDist * 0.75f;

	if ( _followDistMin < 96.0f )
		_followDistMin = 96.0f;

	SetDefaultFollowTarget( self );


	_state = FOLLOW_TARGET_STATE_SELECT_STATE;

      
	_nextFollowAttemptTime = 0.0f;
	_nextTargetCheckTime   = 0.0f;
	_endHold               = 0.0f;
	_followFailureTime     = 0.0f;
	_setFollowFailureTime  = false;
	_attemptedWarpToPath   = false;
	_codeDriven = false;
	_oldForwardSpeed = self.movementSubsystem->getForwardSpeed();
	_oldTurnSpeed = self.movementSubsystem->getTurnSpeed();

}


BehaviorReturnCode_t FollowInFormation::Evaluate ( Actor &self )
{      

	switch ( _state )
		{
		case FOLLOW_TARGET_STATE_SELECT_STATE:
			selectState( self );
		break;

		case FOLLOW_TARGET_STATE_FOLLOW_TARGET:
			follow( self );
		break;

		case FOLLOW_TARGET_STATE_FOLLOW_HOLD:
			hold ( self );
		break;

		case FOLLOW_TARGET_STATE_FOLLOW_WARP_TO_NEAREST_PATHNODE:
			warpToPathNode( self );
		break;

		case FOLLOW_TARGET_STATE_FOLLOW_WARP_TO_FOLLOW_TARGET:
			warpToTarget( self );
		break;

		case FOLLOW_TARGET_STATE_FIND_FOLLOW_TARGET:
			findFollowTarget( self );
		break;

		case FOLLOW_TARGET_STATE_FOLLOW_FAILED:
			return BEHAVIOR_FAILED;
		break;

		case FOLLOW_TARGET_STATE_FOLLOW_NO_TARGET:
			return BEHAVIOR_FAILED;

		case FOLLOW_TARGET_STATE_FOLLOW_SUCCESS:
			return BEHAVIOR_SUCCESS;
		break;
		}

	return BEHAVIOR_EVALUATING;
}


void FollowInFormation::End ( Actor &self	)
{
	self.SetActorFlag( ACTOR_FLAG_FOLLOWING_IN_FORMATION , false );   
	self.movementSubsystem->setForwardSpeed( _oldForwardSpeed );
	self.movementSubsystem->setTurnSpeed( _oldTurnSpeed );
}


void FollowInFormation::SetDefaultFollowTarget( Actor &self )
{
	if ( !self.followTarget.specifiedFollowTarget )
		{
		Player *player;
		player = GetPlayer( 0 );

		if ( !player )
			{
			_state = FOLLOW_TARGET_STATE_FOLLOW_NO_TARGET;   
			return;
			}
      
		self.followTarget.specifiedFollowTarget = player;
		}	
}


void FollowInFormation::findFollowTarget( Actor &self )
{
	Vector selfToFollowTarget;
	Vector actToFollowTarget;
	Vector selfToAct;

	float actDistToFollowTarget;
	float distToFollowTarget;
	float distToAct;
	float bestDist;

	Actor *act;		 


	if ( level.time < _nextTargetCheckTime || !self.followTarget.specifiedFollowTarget )
	  return;


	self.followTarget.currentFollowTarget = self.followTarget.specifiedFollowTarget;

	selfToFollowTarget = self.followTarget.currentFollowTarget->origin - self.origin;

	distToFollowTarget     = selfToFollowTarget.length();
	bestDist               = 9999999.9f;
   

	for( int i = 1; i <= ActiveList.NumObjects(); i++ )   
	  {
	  act = ActiveList.ObjectAt( i );
	  if ( act && act->entnum != self.entnum && act->GetGroupID() == self.GetGroupID() )
		 {
		 if ( !act->GetActorFlag(ACTOR_FLAG_FOLLOWING_IN_FORMATION) ||
			act->followTarget.specifiedFollowTarget != self.followTarget.specifiedFollowTarget )
			continue;

		 actToFollowTarget = self.followTarget.currentFollowTarget->origin - act->origin;
		 actDistToFollowTarget = actToFollowTarget.length();

		 if ( actDistToFollowTarget < distToFollowTarget && actDistToFollowTarget < bestDist )
			{
			selfToAct = act->origin - self.origin;
			distToAct = selfToAct.length();
			if ( distToAct < bestDist )
			   {
			   self.followTarget.currentFollowTarget = act;
			   bestDist = distToAct;
			   }
			}

		 }

	  }   

	_selectedFollowTarget = true;

	_followEntity.SetAnim( _anim );
	_followEntity.SetEntity( self, self.followTarget.currentFollowTarget );

	if ( self.followTarget.currentFollowTarget != self.followTarget.specifiedFollowTarget )
		_followEntity.SetDistance( _followDistMin );   
	else
		_followEntity.SetDistance( _followDist );   

	_followEntity.Begin( self );
	_state = FOLLOW_TARGET_STATE_SELECT_STATE;

	_nextTargetCheckTime = level.time + G_Random();
}


void FollowInFormation::selectState( Actor &self )
{
	if ( !_selectedFollowTarget )
      {
      _state = FOLLOW_TARGET_STATE_FIND_FOLLOW_TARGET;
      return;
      }

   if ( !self.WithinDistanceXY( self.followTarget.currentFollowTarget , _followDist ) )   
      {
      setupFollow( self );
      _state = FOLLOW_TARGET_STATE_FOLLOW_TARGET;
	  return;
      }

   _state = FOLLOW_TARGET_STATE_FOLLOW_HOLD;      
   setupHold( self );
}


void FollowInFormation::follow( Actor &self )
{
	findFollowTarget( self );
	BehaviorReturnCode_t gotoEntityResult;


	if ( !self.followTarget.currentFollowTarget )
		return;

	if ( self.WithinDistanceXY(self.followTarget.currentFollowTarget , _followDist + 96.0f ) )
		{
		_followEntity.SetAnim( "walk" );
		}


	gotoEntityResult = _followEntity.Evaluate( self );


	if ( gotoEntityResult == BEHAVIOR_SUCCESS )
	  {
	  _followEntity.End( self );
	  _nextFollowAttemptTime = 0.0f;	  
	  _state = FOLLOW_TARGET_STATE_SELECT_STATE;
	  return;
	  }

	if ( gotoEntityResult != BEHAVIOR_EVALUATING )
		{
		_state = FOLLOW_TARGET_STATE_FOLLOW_FAILED;
		return;
		}
  
}


void FollowInFormation::followFailed( Actor &self )
{
	if ( !_setFollowFailureTime )
		{
		_followFailureTime = level.time + 1.5;
		_setFollowFailureTime = true;
		return;
		}

	if ( level.time >= _followFailureTime )
		{
		_followFailureTime = 0.0f;
		_setFollowFailureTime = false;

		if ( !_attemptedWarpToPath )
			{
			_state = FOLLOW_TARGET_STATE_FOLLOW_WARP_TO_NEAREST_PATHNODE;
			setupWarpToPathNode( self );
			return;
			}
		else
			{
			_state = FOLLOW_TARGET_STATE_FOLLOW_WARP_TO_FOLLOW_TARGET;
			setupWarpToTarget( self );
			return;
			}
			
		}
	else
	  _state = FOLLOW_TARGET_STATE_SELECT_STATE;

	_nextFollowAttemptTime = level.time + G_Random( 1.0f );	
}


void FollowInFormation::setupWarpToPathNode( Actor &self )
{
	PathNode *goalNode = thePathManager.NearestNode( self.origin , NULL , true , false );
	if ( !goalNode )
		{
		_state = FOLLOW_TARGET_STATE_FOLLOW_WARP_TO_FOLLOW_TARGET;
		setupWarpToTarget( self );
		return;
		}

	_warpToPosition.SetPosition( goalNode->origin );
	_warpToPosition.Begin( self );
}


void FollowInFormation::warpToPathNode( Actor &self )
{
	BehaviorReturnCode_t result;

	_attemptedWarpToPath = true;
	
	result = _warpToPosition.Evaluate( self );	
	if ( result != BEHAVIOR_EVALUATING )
		{
		setupFollow( self );
		_state = FOLLOW_TARGET_STATE_FOLLOW_TARGET;
		}

}


void FollowInFormation::setupWarpToTarget( Actor &self )
{
	_warpToEntity.SetEntity( self.followTarget.currentFollowTarget );
	_warpToEntity.Begin( self );
}


void FollowInFormation::warpToTarget( Actor &self )
{
	BehaviorReturnCode_t result;

	result = _warpToEntity.Evaluate( self );
	if ( result != BEHAVIOR_EVALUATING )
		{
		setupFollow( self );
		_state = FOLLOW_TARGET_STATE_FOLLOW_TARGET;
		}

}


void FollowInFormation::setupFollow( Actor &self )
{
	_followEntity.Begin( self );
}


void FollowInFormation::setupHold( Actor &self )
{   
	_endHold = level.time + .75;
	self.SetAnim( "idle" );
}


void FollowInFormation::hold( Actor &self )
{
	if ( !self.followTarget.currentFollowTarget )
	  {
	  _state = FOLLOW_TARGET_STATE_FOLLOW_NO_TARGET;
	  return;
	  }

	findFollowTarget( self );

	if ( level.time > _endHold )
		{
		_state = FOLLOW_TARGET_STATE_SELECT_STATE;
		return;
		}
	
}


void FollowInFormation::checkSpeed( Actor &self )
{
	if ( _emergencyDistance < 1 )
		return;

	if ( !self.WithinDistanceXY( self.followTarget.currentFollowTarget , _emergencyDistance ) )
		{
		_followEntity.SetAnim( "run_codedriven" );				
		self.movementSubsystem->setForwardSpeed( _catchupSpeed );
		self.movementSubsystem->setTurnSpeed( 360.0f );
		_codeDriven = true;
		}
	else if ( _codeDriven )
		{
		self.movementSubsystem->setForwardSpeed( 0.0 ); 
		self.movementSubsystem->setTurnSpeed( 25.0 ); 
		_codeDriven = false;
		}
}



const float GroupFollow::_minRangeMultiplier = 0.9f;
const float GroupFollow::_maxRangeMultiplier = 1.1f;

CLASS_DECLARATION( Behavior, GroupFollow, NULL )
{
	{ &EV_Behavior_Args,			&GroupFollow::SetArgs },
	{ &EV_Behavior_AnimDone,		&GroupFollow::AnimDone	},
	{ NULL, NULL }
};

float GetAnimationRate( Entity &entity, const int animation )
{
	const float time = gi.Anim_Time( entity.edict->s.modelindex, animation );
	assert( time != 0.0f );

	if ( time == 0.0f )
	{
		gi.DPrintf( "Invalid animation for %d\n", entity.entnum );
		return 0.0f;
	}

	Vector myNewAnimationTotalDelta;
	gi.Anim_Delta( entity.edict->s.modelindex, animation, myNewAnimationTotalDelta );
	
	return myNewAnimationTotalDelta.length() / time;
}

float GetAnimationRate( Entity &entity, const str &animationName )
{
	const int animation = gi.Anim_Random( entity.edict->s.modelindex, animationName );
	
	return GetAnimationRate( entity, animation );
}

void GroupFollow::SetArgs ( Event *ev)
{
	_stopDistance = ev->GetFloat( 1 );
	_paceDistance = ev->GetFloat( 2 );
	_idleAnimation = "idle";
	_paceAnimation = "walk";
	_closeAnimation = "run";
	if (ev->NumArgs() > 2 )
	{
		_idleAnimation = ev->GetString( 3 );
		if (ev->NumArgs() > 3 )
		{
			_paceAnimation = ev->GetString( 4 );
			if (ev->NumArgs() > 4 )
			{
				_closeAnimation = ev->GetString( 5 );
			}
		}
	}
}

void GroupFollow::AnimDone( Event *ev )
{
	_animationRateNeedsUpdate = true;
}

void GroupFollow::SetArgs ( const float stopDistance, const float paceDistance )
{
	_stopDistance = stopDistance;
	_paceDistance = paceDistance;

}

void GroupFollow::Begin( Actor &self )
{
	_nextFindFollowTime = 0.0f;
	_nextPathLenCheckTime = 0.0f;
	_nextPathLenCheckTime2 = 0.0f;

	if ( !self.followTarget.specifiedFollowTarget )
		{
		self.followTarget.specifiedFollowTarget = GetPlayer( 0 );
		assert( self.followTarget.specifiedFollowTarget );
		}

	self.followTarget.currentFollowTarget = self.followTarget.specifiedFollowTarget;

	self.SetActorFlag( ACTOR_FLAG_FOLLOWING_IN_FORMATION , true );
	_animationRateNeedsUpdate = false;
	_follow.Begin( self );
	GotoHoldState( self );

	_torsoAnimation = self.combatSubsystem->GetAnimForMyWeapon( "Idle" );
	if ( _torsoAnimation.length() )
		{
		self.SetAnim( _torsoAnimation, NULL , torso );
		}

	_endHold = 0.0f;
	_oldForwardSpeed = self.movementSubsystem->getForwardSpeed();
	
}


BehaviorReturnCode_t GroupFollow::Evaluate ( Actor &self )
{      
	BehaviorReturnCode_t returnCode = BEHAVIOR_INVALID;

	FindFollowTarget( self );

	switch ( _state )
	{
	case CLOSE_WITH_TARGET:
		returnCode = CloseWithTarget( self );
		break;

	case PACE_TARGET:
		returnCode = PaceTarget( self );
		break;
		
	case HOLD:
		returnCode = Hold( self );
		break;
	
	case WANDER:
		returnCode = Wander(self);
		break;

	default:
		assert( false ); 
		break;
	}
	

	return returnCode;
}


void GroupFollow::End ( Actor &self	)
{
	self.SetActorFlag( ACTOR_FLAG_FOLLOWING_IN_FORMATION , false );   
	self.movementSubsystem->setForwardSpeed( _oldForwardSpeed );
}


BehaviorReturnCode_t GroupFollow::FindFollowTarget( Actor &self )
{

	if ( level.time <= _nextFindFollowTime ) return BEHAVIOR_EVALUATING;

	_nextFindFollowTime = level.time + (G_Random() + 0.25 );


	FindMovementPath	find;
	Path				*path;
	float distanceFromCurrentActorToTarget;
	float distanceFromGroupMemberToTarget;

	find.heuristic.self = &self;
	find.heuristic.setSize( self.size );
	find.heuristic.entnum = self.entnum;

	path = find.FindPath( self.followTarget.specifiedFollowTarget->origin, self.origin );
			
    if ( path )
		{
		distanceFromCurrentActorToTarget = path->Length();
		delete path;
		path = NULL;
		}
	else
		{
		distanceFromCurrentActorToTarget = Vector::Distance( self.followTarget.specifiedFollowTarget->origin, self.origin );
		}

	Entity* currentEnemy = NULL;
	float maxDistance;
	currentEnemy = self.enemyManager->GetCurrentEnemy();

	if ( currentEnemy )
		{
		maxDistance = self.followTarget.maxRangeCombat;
		}
	else
		{
		maxDistance = self.followTarget.maxRangeIdle;
		}

	if ( distanceFromCurrentActorToTarget > ( maxDistance * 3 ) )
		{
		_follow.SetEntity( self, self.followTarget.specifiedFollowTarget );
		return BEHAVIOR_EVALUATING;
		}


	Container< Actor * > groupList;
	for( int i = 1; i <= ActiveList.NumObjects(); i++ )   
	{
		Actor &currentActor = *ActiveList.ObjectAt( i );
		if ( currentActor.GetGroupID() == self.GetGroupID() )
		{
			if ( 
				( currentActor.GetActorFlag(ACTOR_FLAG_FOLLOWING_IN_FORMATION ) ) && 
				( currentActor.followTarget.specifiedFollowTarget == self.followTarget.specifiedFollowTarget )
				)
			{	
			path = NULL;
			if ( level.time >= _nextPathLenCheckTime )
				{
				if ( sv_traceinfo->integer )
					gi.WDPrintf( "Pathing To FollowTarget1" );

				path = find.FindPath( self.followTarget.specifiedFollowTarget->origin, currentActor.origin );
				_nextPathLenCheckTime = level.time + (G_Random() + 1.0);
				}
						
            if ( path )
            {
                distanceFromCurrentActorToTarget = path->Length();
                delete path;
                path = NULL;
            }
            else
                distanceFromCurrentActorToTarget = Vector::Distance( self.followTarget.specifiedFollowTarget->origin, currentActor.origin );
               				
				int j;
				for ( j = 1; j <= groupList.NumObjects(); j++ )
				{
					path = NULL;
					const Actor &currentGroupMember = *groupList.ObjectAt( j );
					if ( level.time >= _nextPathLenCheckTime2 )
						{
						if ( sv_traceinfo->integer )
							gi.WDPrintf( "Pathing To FollowTarget2" );

						path = find.FindPath( self.followTarget.specifiedFollowTarget->origin, currentGroupMember.origin );
						_nextPathLenCheckTime2 = level.time + (G_Random() + 1.0);
						}					
					
					if ( path )
						{
						distanceFromGroupMemberToTarget = path->Length();
						delete path;
						path = NULL;
						}
					else
						distanceFromGroupMemberToTarget = Vector::Distance( self.followTarget.specifiedFollowTarget->origin, currentGroupMember.origin );
					
					if ( distanceFromCurrentActorToTarget < distanceFromGroupMemberToTarget )
					{
						break;
					}
				}
				if ( j > groupList.NumObjects() )
				{
					groupList.AddObject( &currentActor );
				}
				else
				{
					groupList.InsertObjectAt( j, &currentActor );
				}
			}			
		}
	}

	int j;
	for ( j = 1; j <= groupList.NumObjects(); j++ )
	{
		if ( groupList.ObjectAt( j )->entnum == self.entnum )
		{
			break;
		}
	}

	Entity *currentFollowEntity;
	currentFollowEntity = _follow.GetEntity();
	if ( !currentFollowEntity )
		{
		_follow.SetEntity( self, self.followTarget.currentFollowTarget );
		}

	if ( j == 1 )
	{
		if ( self.followTarget.currentFollowTarget != self.followTarget.specifiedFollowTarget )
			{
			self.followTarget.currentFollowTarget = self.followTarget.specifiedFollowTarget;
			
			currentFollowEntity = _follow.GetEntity();

			if ( !currentFollowEntity )
				{
				_follow.SetEntity( self, self.followTarget.currentFollowTarget );
				}
			else if ( currentFollowEntity != self.followTarget.currentFollowTarget )
				{
				_follow.SetEntity( self, self.followTarget.currentFollowTarget );
				}
			
			}		
	}
	else
	{
		if ( self.followTarget.currentFollowTarget != groupList.ObjectAt( j - 1 ) )
			{
			self.followTarget.currentFollowTarget = groupList.ObjectAt( j - 1 );
			
			currentFollowEntity = _follow.GetEntity();

			if ( !currentFollowEntity )
				{
				_follow.SetEntity( self, self.followTarget.currentFollowTarget );
				}
			else if ( currentFollowEntity != self.followTarget.currentFollowTarget )
				{
				_follow.SetEntity( self, self.followTarget.currentFollowTarget );
				}
			}		
	}

	return BEHAVIOR_EVALUATING;
}

BehaviorReturnCode_t GroupFollow::Hold( Actor &self )
{
	assert( _stopDistance * _maxRangeMultiplier < _paceDistance * _minRangeMultiplier );

	float distanceToTarget = Vector::Distance( self.origin, self.followTarget.currentFollowTarget->origin );

	if ( level.time > _endHold )
	{
		if ( distanceToTarget > _stopDistance * _maxRangeMultiplier)
		{
			GotoPaceTargetState( self );
		}
		else
		{
			return BEHAVIOR_SUCCESS;			
		}
	}
	return BEHAVIOR_EVALUATING;
}


BehaviorReturnCode_t GroupFollow::PaceTarget( Actor &self )
{

	if ( _animationRateNeedsUpdate )
	{
		const float animationRate = ComputeAnimationRate( self, _paceAnimation, ComputePaceAnimationRateMultiplier( self ) );
		if ( !fSmallEnough( animationRate, fEpsilon() ) )
		{
			if ( !fCloseEnough( animationRate, self.edict->s.animationRate, 0.5f) )
			{
				self.SetAnim( _paceAnimation, EV_Actor_NotifyBehavior, legs, animationRate );
				_follow.Begin( self );
			}
		}
		else
		{
			GotoHoldState( self );
			return BEHAVIOR_EVALUATING;
		}
		_animationRateNeedsUpdate = false;
	}

	
	BehaviorReturnCode_t gotoEntityResult = _follow.Evaluate( self );

	if ( gotoEntityResult != BEHAVIOR_EVALUATING )
	{
		_follow.End( self );

		if ( gotoEntityResult == BEHAVIOR_FAILED )
			GotoWanderState(self);
		else
			GotoHoldState( self );
	}
	else
	{
		assert( _stopDistance * _maxRangeMultiplier < _paceDistance * _minRangeMultiplier );

		float distanceToTarget = Vector::Distance( self.origin, self.followTarget.currentFollowTarget->origin );

		if ( distanceToTarget > _paceDistance * _maxRangeMultiplier )
		{
			GotoCloseWithTargetState( self );
		}
		else if ( distanceToTarget < _stopDistance * _minRangeMultiplier )
		{
			GotoHoldState( self );
		}
	}
	return BEHAVIOR_EVALUATING;
}


BehaviorReturnCode_t GroupFollow::CloseWithTarget( Actor &self )
{
	if ( _animationRateNeedsUpdate )
	{
		const float animationRate = ComputeAnimationRate( self, _closeAnimation, 1.75f );
		if ( !fCloseEnough( animationRate, self.edict->s.animationRate, 0.5f) )
		{
			self.SetAnim( _closeAnimation, EV_Actor_NotifyBehavior, legs, animationRate );
			_follow.Begin( self );
			_animationRateNeedsUpdate = false;
		}
	}

	BehaviorReturnCode_t gotoEntityResult = _follow.Evaluate( self );


	if ( gotoEntityResult != BEHAVIOR_EVALUATING )
	{
		_follow.End( self );
		GotoHoldState( self );
	}


	assert( _stopDistance * _maxRangeMultiplier < _paceDistance * _minRangeMultiplier );

	float distanceToTarget = Vector::DistanceXY( self.origin, self.followTarget.currentFollowTarget->origin );

	if ( distanceToTarget < _paceDistance * _minRangeMultiplier && self.sensoryPerception->CanSeeEntity( &self,self.followTarget.currentFollowTarget , false , false )  )
	{
		GotoPaceTargetState( self );
	}

	return BEHAVIOR_EVALUATING;
}


void GroupFollow::GotoHoldState( Actor &self )
{   
	_follow.End( self );
	_state = HOLD;      
	_endHold = level.time + 0.75f;
	self.SetAnim( _idleAnimation, EV_Actor_NotifyBehavior, legs, 1.0f );
}

float GroupFollow::ComputePaceAnimationRateMultiplier( Actor &self )
{
	const float distanceToTarget = Vector::DistanceXY( self.origin, self.followTarget.currentFollowTarget->origin );

	float animationRateMultiplier = 1.0f;

	if ( distanceToTarget < _stopDistance )
	{
		animationRateMultiplier = 0.0f;
	}
	else if ( distanceToTarget < _paceDistance )
	{
		animationRateMultiplier = 0.0f + ( ( distanceToTarget - _stopDistance) / ( _paceDistance - _stopDistance) ) * 1.0f;
	}

	return animationRateMultiplier;
}


void GroupFollow::GotoPaceTargetState( Actor &self )
{   

	float animationRateMultiplier = ComputePaceAnimationRateMultiplier( self );

	_follow.End( self );
	_state = PACE_TARGET;

	const float animationRate = ComputeAnimationRate( self, _paceAnimation, animationRateMultiplier );

	if ( !fSmallEnough( animationRate, fEpsilon() ) )
	{
		self.SetAnim( _paceAnimation, EV_Actor_NotifyBehavior, legs, animationRate );
		_follow.Begin( self );
	}
	else
	{
		GotoHoldState( self );
	}

}

float GroupFollow::ComputeAnimationRate( Actor &self, const str &animationName, const float scale )
{
	const float myNewAnimationSpeed = GetAnimationRate( self, animationName);
	
	if ( !fSmallEnough( myNewAnimationSpeed, fEpsilon() )  )
	{
		const float targetAnimationSpeed = GetAnimationRate( *self.followTarget.currentFollowTarget, self.followTarget.currentFollowTarget->CurrentAnim( legs ) );
		if ( !fSmallEnough( targetAnimationSpeed, fEpsilon() ) )
		{
			const float animationRate = scale * targetAnimationSpeed / myNewAnimationSpeed;
			if ( animationRate > 1.0f )
			{
				return animationRate;
			}
		}
	}
	return 1.0f;
}
void GroupFollow::GotoCloseWithTargetState( Actor &self )
{   
	_follow.End( self );
	_state = CLOSE_WITH_TARGET;

	const float animationRate = ComputeAnimationRate( self, _closeAnimation, 1.1f );
	self.SetAnim( _closeAnimation, EV_Actor_NotifyBehavior, legs, animationRate );
	_follow.Begin( self );
}


void GroupFollow::GotoWanderState( Actor &self )
{
	_wander.SetAnim( "walk" );
	_wander.SetMinDistance( 32 );
	_wander.SetDistance( 48 );
	_wander.Begin(self);
	_state = WANDER;
}

BehaviorReturnCode_t GroupFollow::Wander( Actor &self )
{
	BehaviorReturnCode_t result;
	result = _wander.Evaluate( self );

	if ( result == BEHAVIOR_FAILED )
	{
		GotoHoldState(self);
	}

	if ( result == BEHAVIOR_SUCCESS )
	{
		GotoCloseWithTargetState(self);
	}

	return BEHAVIOR_EVALUATING;
}

CLASS_DECLARATION( Behavior, MoveToDistanceFromEnemy, NULL )
	{
		{ &EV_Behavior_Args,			&MoveToDistanceFromEnemy::SetArgs },
		{ NULL, NULL }
	};


void MoveToDistanceFromEnemy::SetArgs ( Event *ev)
	{
	_checkParameters(ev);

	_anim = ev->GetString( 1 );
   _distance = ev->GetFloat( 2 );
	}



void MoveToDistanceFromEnemy::Begin( Actor &self )
	{	
	movegoal = NULL;
	_state = MOVE_TO_DISTANCE_STATE_SEARCHING_FOR_NODE;
	

	Vector dir;
	dir = self.movementSubsystem->getAnimDir();
	dir = dir.toAngles();
	self.setAngles( dir );

   self.testing = true;
   self.SetActorFlag(ACTOR_FLAG_RETREATING , true );

   _away = self.enemyManager->GetAwayFromEnemies();
   
   _away = _away * _distance;
   _away = _away + self.origin; 

	}



BehaviorReturnCode_t	MoveToDistanceFromEnemy::Evaluate ( Actor &self )
	{

   Entity *currentEnemy = NULL;
   currentEnemy = self.enemyManager->GetCurrentEnemy();

   if ( !currentEnemy )
      return BEHAVIOR_FAILED;

   Vector startPos;
   Vector endPos;
   startPos = self.origin;
   endPos = _away;

   startPos.z += 25;
   endPos.z += 25;



   if ( !movegoal )
		_state = MOVE_TO_DISTANCE_STATE_SEARCHING_FOR_NODE;

	switch( _state )
		{
		case MOVE_TO_DISTANCE_STATE_SEARCHING_FOR_NODE :
         

			_chase.Begin( self );
			movegoal = _FindNode( self );

 			if ( !movegoal )
				return BEHAVIOR_SUCCESS;

			self.SetAnim( _anim );
			_state = MOVE_TO_DISTANCE_STATE_FOUND_NODE;
			_nextsearch = level.time + 1.25f;

		case MOVE_TO_DISTANCE_STATE_FOUND_NODE :
         Vector distToGoal;
         distToGoal = movegoal->origin - _away;
         float dist;
         dist = distToGoal.length();
         

         if ( dist >= 400 )
            {
            return BEHAVIOR_FAILED;            
            }
         


			if ( _chase.Evaluate( self ) == Steering::EVALUATING )
				{
				if ( _nextsearch < level.time )
					_state = MOVE_TO_DISTANCE_STATE_SEARCHING_FOR_NODE;

				return BEHAVIOR_EVALUATING;
				}
			
         _chase.End( self );			               
         self.SetAnim( "idle" );
         return BEHAVIOR_SUCCESS;
         
			break;
		}

	return BEHAVIOR_EVALUATING;
	
	}



void MoveToDistanceFromEnemy::End ( Actor &self	)
	{
	self.testing = false;
   self.SetActorFlag(ACTOR_FLAG_RETREATING , false );
   _chase.End( self );
	}



PathNode *MoveToDistanceFromEnemy::_FindNode( Actor &self )
	{
   int i;
	PathNode	*bestnode;
	PathNode *node;
	FindCoverPath find;
	Vector	delta;	
	Vector	pos;
   
   pos = _away;
	bestnode = NULL;
   node = NULL;

   float bestdist;
   float dist;


	Entity *currentEnemy;
	currentEnemy = self.enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return NULL;
   
   bestdist = 9999999999.9f;   
   for( i = 0; i <= thePathManager.NumNodes(); i++ )
   	{
      node = thePathManager.GetNode( i );

		if ( node &&  (node->occupiedTime <= level.time) )
			{
         delta = node->origin - pos;
         dist = delta.length();
              
         if ( dist < bestdist )
            {
            bestnode = node;         
            bestdist = dist;
            }
            

			}
		}

	if ( bestnode )
      {
		bestnode->occupiedTime = level.time + 1.5f;
		bestnode->entnum = self.entnum;

		float radius=16.0f;
		_chase.SetGoal( bestnode->origin, radius, self );
      return bestnode;

      }

	return NULL;
   
	}



void MoveToDistanceFromEnemy::_checkParameters( Event *ev )
	{
	if ( ev->NumArgs() != 2 )
		gi.Error( ERR_DROP, "FindWork::FindWork -- Wrong Parameter Count");
	
	if ( ev->IsNumericAt( 1 ) )
		gi.Error( ERR_DROP, "FindWork::_checkParameters -- Parameter Mismatch");
	
	}



void MoveToDistanceFromEnemy::SetAnim( const str &anim )
   {
   _anim = anim;
   }



void MoveToDistanceFromEnemy::SetDistance( float distance )
   {
   _distance = distance;
   }












CLASS_DECLARATION( Behavior, BackAwayFromEnemy, NULL )
	{
		{ &EV_Behavior_Args,			&BackAwayFromEnemy::SetArgs		},
		{ &EV_Behavior_AnimDone,		&BackAwayFromEnemy::AnimDone	},
		{ NULL,							NULL		}
	};


void BackAwayFromEnemy::SetArgs ( Event *ev)
	{
	_anim = ev->GetString( 1 );
	_dist = ev->GetFloat( 2 );
	_minDist = ev->GetFloat( 3 );
	}

void BackAwayFromEnemy::AnimDone( Event *ev )
{
}

void BackAwayFromEnemy::Begin( Actor &self )
	{
	_state = BAFE_SELECT_STATE;
	}


BehaviorReturnCode_t BackAwayFromEnemy::Evaluate ( Actor &self )
	{	
	switch ( _state )
		{
		case BAFE_SELECT_STATE:
			selectState( self );
		break;

		case BAFE_BACK_AWAY:
			moveRandom( self );
		break;

		case BAFE_BACK_AWAY_FAILED:
			return BEHAVIOR_FAILED;
		break;

		case BAFE_BACK_AWAY_SUCCESS:
			return BEHAVIOR_SUCCESS;
		break;
		}

	return BEHAVIOR_EVALUATING;	
	}


void BackAwayFromEnemy::End ( Actor &self	)
{
	self.movementSubsystem->setMovingBackwards( false );
}


void BackAwayFromEnemy::selectState( Actor &self )
{
	setupMoveRandom( self );
	_state = BAFE_BACK_AWAY;
}


void BackAwayFromEnemy::setupMoveRandom( Actor &self )
{
	self.movementSubsystem->setMovingBackwards( true );
	_moveRandom.SetMode( MoveRandomDirection::RANDOM_MOVE_IN_BACK );
	_moveRandom.SetAnim( _anim );
	_moveRandom.SetDistance( _dist );
	_moveRandom.SetMinDistance( _minDist );
	_moveRandom.Begin( self );
}

void BackAwayFromEnemy::moveRandom( Actor &self )
{
	BehaviorReturnCode_t result;

	result = _moveRandom.Evaluate( self );

	if ( result == BEHAVIOR_SUCCESS )
		{
		_state = BAFE_BACK_AWAY_SUCCESS;
		self.movementSubsystem->setMovingBackwards( false );
		return;
		}

	if ( result != BEHAVIOR_EVALUATING )
		moveRandomFailed( self );
}


void BackAwayFromEnemy::moveRandomFailed( Actor &self )
{
	_state = BAFE_BACK_AWAY_FAILED;
	self.movementSubsystem->setMovingBackwards( false );
}

















CLASS_DECLARATION( Behavior, AlertIdle, NULL )
	{
		{ &EV_Behavior_Args,		&AlertIdle::SetArgs },
      { &EV_Behavior_AnimDone,		&AlertIdle::AnimDone }, 
		{ NULL, NULL }
	};



void AlertIdle::SetArgs ( Event *ev)
	{
	_followAnim   = ev->GetString( 1 );
	_torsoAnim	  = ev->GetString( 2 );
	_baseIdleTime = ev->GetFloat( 3 );

	if ( ev->NumArgs() > 3 )		
		_emergencyDist = ev->GetFloat( 4 );				
	else
		_emergencyDist = 600.0f;

	if ( ev->NumArgs() > 4 )
		_followDist = ev->GetFloat( 5 );
	else
		_followDist = 176.0f;

	if ( ev->NumArgs() > 5 )
		_wanderDist = ev->GetFloat( 6 );
	else
		_wanderDist = 328.0f;

	if ( !_torsoAnim.length() )
		_useTorsoAnim = false;
	else
		_useTorsoAnim = true;
	


	}



void AlertIdle::AnimDone( Event *ev )
   {

   }



void AlertIdle::Begin( Actor &self )
	{
   _init( self );
	}



void AlertIdle::_init( Actor &self )
   {
	_setupFollow( self );

	_nextFollowAttempt = 0.0f;
	_nextWanderTime    = -1.0f;
	_self = &self;
	_unableToFollow = false;
	self.postureController->setPostureState( "STAND" , "STAND" );	
   _state = ALERT_IDLE_SELECT_STATE;
   }



BehaviorReturnCode_t	AlertIdle::Evaluate ( Actor &self )
	{      
	_setTorsoAnim( self );
	switch ( _state )
		{
		case ALERT_IDLE_SELECT_STATE:
			_selectState( self );
		break;

		case ALERT_IDLE_IN_THE_WAY:
			_doGetOutOfTheWay( self );
		break;

		case ALERT_IDLE_FOLLOW:			
			_doFollow( self );
		break;

		case ALERT_IDLE_WANDER:			
			_doWander( self );
		break;

		case ALERT_IDLE_HOLD:
			_hold ( self );
		break;
		}

	return BEHAVIOR_EVALUATING; 
	}



void AlertIdle::End ( Actor &self	)
	{
	self.SetActorFlag( ACTOR_FLAG_FOLLOWING_IN_FORMATION , false );
	}


void AlertIdle::_selectState( Actor &self )
   {
	
	if ( level.time > _nextFollowAttempt )
		{
		_setupFollow( self );
		_state = ALERT_IDLE_FOLLOW;
		return;
		}

	_setupHold( self );
	_state = ALERT_IDLE_HOLD;

   }


void AlertIdle::_setupGetOutOfTheWay( Actor &self )
	{
    self.SetAnim( "idle" );
	if ( self.checkplayerranged() )
		_getOutOfTheWay.SetAnim( "run" );
	else
		_getOutOfTheWay.SetAnim( "walk" );

	_getOutOfTheWay.Begin( self );
	}


void AlertIdle::_doGetOutOfTheWay( Actor &self )
	{
	BehaviorReturnCode_t result;
	result = _getOutOfTheWay.Evaluate( self );

	if ( result == BEHAVIOR_SUCCESS )
		{
		self.ClearStateFlags();
		_getOutOfTheWay.End( self );
		_state = ALERT_IDLE_SELECT_STATE;
		return;
		}
	
	if ( result != BEHAVIOR_EVALUATING )
		{
		self.ClearStateFlags();
		_getOutOfTheWay.End( self );
		
		self.SetAnim( "idle" );
		_setupGetOutOfTheWay( self );
		_state = ALERT_IDLE_SELECT_STATE;
		}

	}


void AlertIdle::_setupHold( Actor &self )
	{
	self.SetAnim( "idle" );
	_setNextWanderTime( self );
	}


void AlertIdle::_hold( Actor &self )
	{
	

	if ( self.checktouchedbyplayer() )
		{
		_state = ALERT_IDLE_IN_THE_WAY;
		_setupGetOutOfTheWay( self );		
		return;
		}	

	if ( !self.WithinDistance( self.followTarget.specifiedFollowTarget , _followDist ) )
		{	
		_state = ALERT_IDLE_SELECT_STATE;
		return;
		}

	if ( level.time > _nextWanderTime )
		{
		_setupWander( self );
		_state = ALERT_IDLE_WANDER;
		return;
		}

	}


void AlertIdle::_setupFollow( Actor &self )
	{
	_setTorsoAnim( self );

	_nextWanderTime    = -1.0f;
	_follow.SetAnim( _followAnim );
	_follow.SetEmergencyDistance( _emergencyDist );
	_follow.SetCatchupSpeed( 25.0f );

	_follow.Begin( self );
	}


void AlertIdle::_doFollow( Actor &self )
	{
	BehaviorReturnCode_t result;

	if ( _checkInTheWay( self ) )
		return;

	
	if ( _follow.GetState() == FollowInFormation::FOLLOW_TARGET_STATE_FOLLOW_HOLD )
		{
		if ( _tryWander( self ) )
			return;
		}

	result = _follow.Evaluate( self );
	
	if ( result == BEHAVIOR_SUCCESS )
		{
		self.SetAnim ( "idle" );
		_follow.End( self );				
		_state = ALERT_IDLE_SELECT_STATE;
		return;
		}

	if ( result != BEHAVIOR_EVALUATING )
		{
		_nextFollowAttempt = level.time + 0.25f;
		_unableToFollow = true;
		_setupWander( self );
		_state = ALERT_IDLE_WANDER;

		
		}
	else
		_unableToFollow = false;

	}


bool AlertIdle::_tryWander( Actor &self )
	{
	if ( _nextWanderTime < 0 )
		_setNextWanderTime( self );

	if ( !self.WithinDistance( self.followTarget.specifiedFollowTarget , _wanderDist ) )
		return false;

	if ( ( _nextWanderTime > 0 ) && ( level.time > _nextWanderTime ) )
		{
		_setupWander( self );
		_state = ALERT_IDLE_WANDER;
		return true;
		}

	return false;
	}


void AlertIdle::_setupWander( Actor &self )
	{
	_follow.End( self );
	_wander.SetAnim( "walk" );
	_wander.SetDistance( 128.0f );
	_wander.SetMinDistance( 96.0f );
	_wander.SetMode( MoveRandomDirection::RANDOM_MOVE_ANYWHERE );
	_wander.Begin( self );	
	}


void AlertIdle::_doWander( Actor &self )
	{
	BehaviorReturnCode_t result;	


	if ( _checkInTheWay( self ) )
		return;

	result = _wander.Evaluate( self );

	if ( !self.WithinDistance( self.followTarget.specifiedFollowTarget , _followDist ) && !_unableToFollow)
		{	
		_state = ALERT_IDLE_SELECT_STATE;
		return;
		}


	if ( result != BEHAVIOR_EVALUATING )
		{
		self.SetAnim ( "idle" );
		_wander.End( self );			
		_setNextWanderTime( self );
		_state = ALERT_IDLE_HOLD;		
		return;
		}
		
	}


void AlertIdle::_setNextWanderTime( Actor &self )
	{
	_nextWanderTime = level.time + G_Random( 3.0 ) + _baseIdleTime;
	}


bool AlertIdle::_checkInTheWay( Actor &self )
	{
	if ( self.checktouchedbyplayer() )
		{
		_state = ALERT_IDLE_IN_THE_WAY;
		_setupGetOutOfTheWay( self );		
		return true;
		}

	return false;
	}


void AlertIdle::_setTorsoAnim( Actor &self )
	{
	if ( _useTorsoAnim )
		self.SetAnim( _torsoAnim , NULL , torso );
	}













const float DoBeamAttack::BEAMATTACK_SPREADFACTOR = 2.0f;

CLASS_DECLARATION( Behavior, DoBeamAttack, NULL )
	{
		{ &EV_Behavior_Args,			&DoBeamAttack::SetArgs },
      { &EV_Behavior_AnimDone,			&DoBeamAttack::AnimDone }, 
      { NULL, NULL }
	};



void DoBeamAttack::SetArgs( Event *ev )
   {
   tagName     = ev->GetString  ( 1 );
   beamShader  = ev->GetString  ( 2 );
   impactModel = ev->GetString  ( 3 );
   flashModel  = ev->GetString  ( 4 );
   anim        = ev->GetString  ( 5 );
   damage      = ev->GetFloat   ( 6 );
   time        = ev->GetFloat   ( 7 );
   turnspeed   = ev->GetFloat   ( 8 );
   trackEnemy  = ev->GetBoolean ( 9 );
   if ( ev->NumArgs() > 9 )
	   beamCount = ev->GetInteger( 10 );
   else
	   beamCount = 1;

   if ( ev->NumArgs() > 10 )
	   useRotation = ev->GetBoolean( 11 );
   else
	   useRotation = false;

   }



void DoBeamAttack::AnimDone( Event *ev )
   {
   if ( _state == BEAMATTACK_START_ANIM )
      _state = BEAMATTACK_START_ATTACK;
   }



void DoBeamAttack::Begin( Actor &self )
	{       
   _initialRotationComplete = false;
   _state = BEAMATTACK_SETUP;   
	}



BehaviorReturnCode_t	DoBeamAttack::Evaluate( Actor &self )
	{
   if ( _initialRotationComplete && trackEnemy )
      _rotate( self );

   switch ( _state )
      {
      case BEAMATTACK_SETUP:
         _setupRotate( self );
      break;

      case BEAMATTACK_ROTATE:
         _rotate( self );
      break;

      case BEAMATTACK_START_ANIM:
         _playAttackAnim( self );
      break;

      case BEAMATTACK_START_ATTACK:
         _createBeam( self );
      break;

      case BEAMATTACK_ATTACKING:
         _updateBeam( self );
      break;

      case BEAMATTACK_COMPLETE:
         return BEHAVIOR_SUCCESS;
      break;

      case BEAMATTACK_FAILED:
         return BEHAVIOR_FAILED;
      break;
      }

   return BEHAVIOR_EVALUATING;   
   }



void DoBeamAttack::End(Actor &self)
	{   

	}



void DoBeamAttack::_setupRotate( Actor &self )
   {
   Entity *currentEnemy;
   currentEnemy = self.enemyManager->GetCurrentEnemy();

   if ( currentEnemy )
      {
      _rotateBehavior.SetEntity( currentEnemy );
      _rotateBehavior.SetTurnSpeed( turnspeed ); 
      _rotateBehavior.Begin( self );
      }
      
	if ( useRotation )
		_state = BEAMATTACK_ROTATE;
	else
		_state = BEAMATTACK_START_ANIM;

   }



void DoBeamAttack::_rotate( Actor &self )
   {  
   BehaviorReturnCode_t result;

   result = _rotateBehavior.Evaluate( self );

   if ( result == BEHAVIOR_SUCCESS )
        _state = BEAMATTACK_START_ANIM;

   }



void DoBeamAttack::_playAttackAnim( Actor &self )
   {
   self.SetAnim( anim , EV_Actor_NotifyBehavior);         
   }

void DoBeamAttack::_createBeam( Actor &self )
   {
	Vector   tagOrig;
   Vector   dir;
	Vector   angles;	
   Vector   spread;
   FuncBeam *beam;
   
	trace_t  trace;
   Entity  *currentEnemy;
	
	currentEnemy = self.enemyManager->GetCurrentEnemy();
	if ( !currentEnemy || !_canAttack( self ) || tagName.length() == 0 )
      {
      _attackFailed( self );
      return;
      }

	self.setOrigin();

	self.GetTag( tagName.c_str(), &tagOrig );

	dir = currentEnemy->centroid - tagOrig;
	angles = dir.toAngles();
   
	for ( int i = 1 ; i <= beamCount ; i++ )
		{
	   spread.x = G_CRandom(BEAMATTACK_SPREADFACTOR);
	   spread.y = G_CRandom(BEAMATTACK_SPREADFACTOR);
	   spread.z = G_CRandom(BEAMATTACK_SPREADFACTOR);
	   angles = angles + spread;

		angles.AngleVectors( &_beamEndPos, NULL, NULL );
	   _beamEndPos *= dir.length();
	   _beamEndPos += tagOrig;
   
		trace = _beamAttackTrace(self , tagOrig );

		if ( ( trace.fraction < 1.0f ) ) 
			{      
			dir = _beamEndPos - tagOrig;
			dir.normalize();
      
			trace.ent->entity->Damage( &self, &self, damage, vec_zero, dir, vec_zero, 0, 0, MOD_ELECTRIC );		
			}

		beam = CreateBeam( NULL, beamShader.c_str(), tagOrig, _beamEndPos, 1, 1.5f, 0.25f );      
		_beamList.AddObject ( beam );
		}	

 
	
   _endTime = level.time + time;

   _state = BEAMATTACK_ATTACKING;
		

   }

trace_t DoBeamAttack::_beamAttackTrace( Actor &self , const Vector &startPos )
   {
   return G_Trace( startPos, Vector (-15.0f, -15.0f, -15.0f), Vector (15.0f, 15.0f, 15.0f), _beamEndPos, &self, MASK_SHOT, false, "doBeamAttack" );  
   }

void DoBeamAttack::_updateBeam( Actor &self )
   {
   Vector   tagOrig;
   trace_t trace;
   EntityPtr beam;

   if ( level.time >= _endTime )
      _state = BEAMATTACK_COMPLETE;

   self.GetTag( tagName.c_str(), &tagOrig );
	
   for ( int i = 1 ; i <= beamCount ; i++ )
	   {
	   beam = _beamList.ObjectAt( i );

	   if ( beam )
		  {
		  beam->setOrigin( tagOrig ); 
      
		  self.Entity::SpawnEffect( str(flashModel), tagOrig, vec_zero, 0.25f ); 
		  trace = _beamAttackTrace( self , tagOrig );

		  if ( trace.fraction < 1.0f )
			 {
			 _beamEndPos = trace.endpos;

			 FuncBeam* _theBeam;
			 _theBeam = (FuncBeam *)(Entity *)beam;

			 _theBeam->SetEndPoint( _beamEndPos );  

			 self.Entity::SpawnEffect(impactModel, _beamEndPos, vec_zero, 0.35f);  
			 }
		  }

		}

   }

void DoBeamAttack::_attackFailed( Actor &self )
   {
   _state = BEAMATTACK_FAILED;
   }

bool DoBeamAttack::_canAttack( Actor &self )
   {  
   Entity *currentEnemy;
   currentEnemy = self.enemyManager->GetCurrentEnemy();

   if ( !currentEnemy)
      return false;
   
   if ( self.combatSubsystem->CanAttackTarget( currentEnemy ) )
      return true;
   else
      return false;
   }







CLASS_DECLARATION( Behavior, FireWeapon, NULL )
	{
		{ &EV_Behavior_Args,			&FireWeapon::SetArgs },
		{ NULL, NULL }
	};


FireWeapon::FireWeapon()
{
	_target = NULL;
	_havePosition = false;
}

void FireWeapon::SetArgs ( Event *ev)
	{
	_anim       = ev->GetString ( 1 );
	}



void FireWeapon::SetAnim(const str &anim )
   {
   _anim = anim;
   }



void FireWeapon::Begin( Actor &self )
	{
	if ( _havePosition )
		self.combatSubsystem->AimWeaponTag( _targetPosition );
	else if ( _target )
		self.combatSubsystem->AimWeaponTag(_target);

	self.SetAnim( _anim , EV_Actor_NotifyTorsoBehavior , torso );
	}



BehaviorReturnCode_t	FireWeapon::Evaluate ( Actor &self )
	{      
	
	if( !self.combatSubsystem->HaveWeapon() )
		return BEHAVIOR_FAILED;

	if ( !_target )
		_target = self.enemyManager->GetCurrentEnemy();

	if( !_target )
		return BEHAVIOR_FAILED;

	if ( _havePosition )
		self.combatSubsystem->AimWeaponTag( _targetPosition );
	else if ( _target )
		self.combatSubsystem->AimWeaponTag(_target);
		
	self.combatSubsystem->AimWeaponTag(_target);

   return BEHAVIOR_EVALUATING; 
	}



void FireWeapon::End ( Actor &self	)
	{
   _stopFire( self );
	}



void FireWeapon::_stopFire( Actor &self )
	{
	Event *StopFireEvent;
	StopFireEvent = new Event( EV_Sentient_StopFire );
	StopFireEvent->AddString ( "dualhand" );

	self.ProcessEvent( StopFireEvent );
	}


void FireWeapon::SetTargetPosition( const Vector &targetPos )
{
	_targetPosition = targetPos;
	_havePosition = true;
}







CLASS_DECLARATION( Behavior, SimpleMelee, NULL )
	{
		{ &EV_Behavior_Args,		&SimpleMelee::SetArgs },
		{ &EV_Behavior_AnimDone,	&SimpleMelee::AnimDone }, 
		{ NULL, NULL }
	};



void SimpleMelee::SetArgs ( Event *ev)
{
	rushAnim   = ev->GetString( 1 );
	attackAnim = ev->GetString( 2 );
	meleeDist  = ev->GetFloat( 3 );
	turnSpeed  = ev->GetFloat( 4 );
}



void SimpleMelee::AnimDone( Event *ev )
{
	if ( _state == SIMPLE_MELEE_ATTACK )
		_attack.AnimDone( ev );
}



void SimpleMelee::Begin( Actor &self )
{
	_init( self );
}



void SimpleMelee::_init( Actor &self )
{
	_self = &self;
	_holdTime = 0.0f;
	_strafeAttempts = 0;
	_nextStrafeTime = 0.0f;
	_nextEnemyCheckTime = 0.0f;
	_holdCount          = 0;
	_state = SIMPLE_MELEE_SELECT_STATE;   
}



BehaviorReturnCode_t	SimpleMelee::Evaluate ( Actor &self )
{      
	if ( level.time > _nextEnemyCheckTime )
		{
		self.enemyManager->FindHighestHateEnemy();
		_nextEnemyCheckTime = level.time + G_Random() + 2.0f;
		}

	switch ( _state )
      {
      case SIMPLE_MELEE_SELECT_STATE:
         _selectState( self );
      break;

      case SIMPLE_MELEE_RUSH_ENEMY:
         _rush( self );
      break;

      case SIMPLE_MELEE_CIRCLE_STRAFE:
         _strafe( self );
      break;

      case SIMPLE_MELEE_ATTACK:
         _meleeAttack( self );
      break;

      case SIMPLE_MELEE_HOLD:
         _hold ( self );
      break;
      }

	return BEHAVIOR_EVALUATING; 
}



void SimpleMelee::End ( Actor &self	)
{   
}


void SimpleMelee::SetRushAnim( const str &anim )
{
	rushAnim = anim;
}

void SimpleMelee::SetAttackAnim( const str &anim )
{
	attackAnim = anim;
}

void SimpleMelee::SetMeleeDist( float dist )
{
	meleeDist = dist;
}

void SimpleMelee::SetTurnSpeed( float turnspeed )
{
	turnSpeed = turnspeed;
}

void SimpleMelee::_setupRush( Actor &self )
{
	_nextStrafeTime = level.time + G_Random(.75);
	_rushEnemy.setAnim( rushAnim  );
	_rushEnemy.setDist( meleeDist );
	_rushEnemy.Begin( self );
}

void SimpleMelee::_rush( Actor &self )
{
	BehaviorReturnCode_t result;
	Entity *currentEnemy;


	result = _rushEnemy.Evaluate( self );

	if ( level.time >= _nextStrafeTime )
		{
		_setupStrafe( self );
		_state = SIMPLE_MELEE_CIRCLE_STRAFE;
		return;
		}

	currentEnemy = self.enemyManager->GetCurrentEnemy();

	if ( !currentEnemy )
		{
		_state = SIMPLE_MELEE_SELECT_STATE;
		return;
		}

	if ( currentEnemy && self.WithinDistance( currentEnemy , meleeDist ) )
		{
		_state = SIMPLE_MELEE_SELECT_STATE;
		return;
		}

	if ( result == BEHAVIOR_SUCCESS )
		{
		_state = SIMPLE_MELEE_SELECT_STATE;      
		_holdCount = 0;
		return;
		}
	
	if ( result != BEHAVIOR_EVALUATING )
		{
		float chance;
		chance = G_Random();

		if ( chance < .25 )
			{
			_setupHold( self );
			_state = SIMPLE_MELEE_HOLD;
			return;
			}
		}
    
	_holdCount = 0;

}


void SimpleMelee::_setupStrafe( Actor &self )
{
	_circleStrafe.SetAnim( rushAnim );
	_circleStrafe.SetRadius ( meleeDist );
	_circleStrafe.SetType( "enemy" );
	_circleStrafe.SetTestDistance( 128.0f );

	float chance;
	chance = G_Random();

	if ( chance < .5 )
		_strafeClockwise = true;
	else
		_strafeClockwise = false;

	_circleStrafe.SetClockwise( _strafeClockwise );
	_circleStrafe.Begin( self );
}

void SimpleMelee::_strafe( Actor &self )
{
	BehaviorReturnCode_t result;

	result = _circleStrafe.Evaluate( self );   

	if ( result == BEHAVIOR_FAILED || self.combatSubsystem->CanAttackTarget( self.enemyManager->GetCurrentEnemy() ))
	{
		_state = SIMPLE_MELEE_SELECT_STATE;		
		return;
	}

}

void SimpleMelee::_setupMeleeAttack( Actor &self )
{
	_attack.SetAnim( attackAnim );   
	_attack.SetTurnSpeed ( turnSpeed );
	_attack.SetForceAttack( true );
	_attack.Begin( self );
}

void SimpleMelee::_meleeAttack( Actor &self )
{
	BehaviorReturnCode_t result;

	result = _attack.Evaluate( self );   

	if ( result == BEHAVIOR_FAILED )
		{
		_setupHold( self );
		_state = SIMPLE_MELEE_HOLD;
		}

	if ( result == BEHAVIOR_SUCCESS )
		_state = SIMPLE_MELEE_SELECT_STATE;


	if ( !self.combatSubsystem->CanAttackTarget( self.enemyManager->GetCurrentEnemy() ) )
		{
		float chance;
		chance = G_Random();

		if ( chance <= .25 )
			{	
			self.enemyManager->FindNextEnemy();			
			_nextEnemyCheckTime = level.time + G_Random() + 3.0f;
			}
		}

}

void SimpleMelee::_setupHold( Actor &self )
{
	_holdCount++;
	_holdTime = level.time + G_Random(0.25f) + 0.50f;
}

void SimpleMelee::_hold( Actor &self )
{	
	self.SetAnim( "idle" );

	if ( level.time >= _holdTime )
		_state = SIMPLE_MELEE_SELECT_STATE;
}

void SimpleMelee::_selectState( Actor &self )
{
	Entity *currentEnemy;
	currentEnemy = self.enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		self.enemyManager->FindHighestHateEnemy();

	if ( !self.WithinDistance( currentEnemy , meleeDist ) )
		{
		_state = SIMPLE_MELEE_RUSH_ENEMY;
		_setupRush( self );
		return;
		}

	_setupMeleeAttack( self );
	_state = SIMPLE_MELEE_ATTACK;
	return;



}








CLASS_DECLARATION( Behavior, FollowPathBlindly, NULL )
{
	{ &EV_Behavior_Args,			&FollowPathBlindly::SetArgs },
	{ NULL, NULL }
};



void FollowPathBlindly::SetArgs ( Event *ev )
{
	if ( ev->NumArgs() > 0 )
	{
		_animName = ev->GetString( 1 );
	}
	if ( ev->NumArgs() > 1 )
	{
		_offset = ev->GetFloat( 2 );
	}
	else
	{
		_offset = 0.0f;
	}
	if ( ev->NumArgs() > 2 )
	{
		_startNodeName = ev->GetString( 3 );
	}
}



void FollowPathBlindly::Begin( Actor &self )
{
	HelperNodePtr startNode=NULL;
	if ( _startNodeName.length() > 0 )
	{
		startNode = HelperNode::GetTargetedHelperNode( _startNodeName );
	}
	else
	{
		startNode = FindNearestNode( self );
	}

	if ( startNode != NULL )
	{
		SetNode( self, startNode );
	}

	_gotoHelperNode.Begin( self );
	_gotoHelperNode.SetDistance( 32.0f );
	_gotoHelperNode.SetAnim( _animName );
}



BehaviorReturnCode_t FollowPathBlindly::Evaluate ( Actor &self )
{  
    if ( _node == NULL )
	{
		_node = FindNearestNode( self );
		if ( _node == NULL )
		{
			return BEHAVIOR_FAILED; 
		}
	}

	assert( _node != NULL );
	BehaviorReturnCode_t moveResult = _gotoHelperNode.Evaluate( self );
	
	if ( moveResult == BEHAVIOR_SUCCESS )
	{
		_node->RunExitThread();
	
		HelperNodePtr lastNode = _node;
		if ( !AdvanceNode( self ) )
		{
			self.SetAnim( "idle" );
			return BEHAVIOR_SUCCESS;
		}
		_gotoHelperNode.SetPoint( ComputeTargetPoint( lastNode, _node, _nextNode ) );
	}  

	return BEHAVIOR_EVALUATING; 
}



void FollowPathBlindly::End ( Actor &self	)
{
}


HelperNodePtr FollowPathBlindly::FindNextNode( Actor &self )
{
	if ( _node != NULL)
	{
		str nextNodeTargetName = _node->target;
		if ( nextNodeTargetName.length() )
		{
			return HelperNode::GetTargetedHelperNode( nextNodeTargetName );
		}
	}
	return NULL;
}


HelperNodePtr FollowPathBlindly::FindNearestNode( Actor &self )
{
	return HelperNode::FindClosestHelperNodeWithoutPathing( self, 512.0f );   
}

void FollowPathBlindly::SetNode( Actor &self, HelperNodePtr node )
{
	_node = node;
	_nextNode = FindNextNode( self );
	_gotoHelperNode.SetPoint( ComputeTargetPoint( NULL, _node, _nextNode ) );

}

const bool FollowPathBlindly::AdvanceNode( Actor &self )
{
	_node = _nextNode;
	if ( _node != NULL)
	{
		_nextNode = FindNextNode( self );
		return true;
	}
	return false;
}



const Vector FollowPathBlindly::ComputeTargetPoint( const HelperNodePtr lastNode, HelperNodePtr const currentNode, const HelperNodePtr nextNode ) const
{
	if ( currentNode == NULL )
	{
		return Vector::Identity();
	}

	if ( fSmallEnough(_offset, fEpsilon() ) )
	{
		return ( currentNode->origin );
	}

	Vector averagePerpendicular;
	Vector parallelDirection1;
	if ( lastNode )
	{
		parallelDirection1 = currentNode->origin - lastNode->origin;
		parallelDirection1.normalize();
		averagePerpendicular.CrossProduct( parallelDirection1, Vector( 0, 0, 1 ) );
	}

	Vector parallelDirection2;
	if ( nextNode )
	{
		parallelDirection2 = nextNode->origin - currentNode->origin;
		parallelDirection2.normalize();
		Vector secondPerpendicular;
		secondPerpendicular.CrossProduct( parallelDirection2, Vector( 0, 0, 1 ) );
		if ( lastNode )
		{
			averagePerpendicular += secondPerpendicular;
			averagePerpendicular /= 2.0f;
		}
		else
		{
			averagePerpendicular = secondPerpendicular;
		}
	}

	if ( Vector::SmallEnough( averagePerpendicular ) )
	{
		if ( lastNode )
		{
			averagePerpendicular = parallelDirection1;
		}
		else
		{
			averagePerpendicular = -parallelDirection2;
		}
	}

	return currentNode->origin + ( averagePerpendicular * _offset );
}





CLASS_DECLARATION( Behavior, Hibernate, NULL )
	{
		{ &EV_Behavior_Args,		&Hibernate::SetArgs },
      { &EV_Behavior_AnimDone,		&Hibernate::AnimDone }, 
		{ NULL, NULL }
	};


void Hibernate::SetArgs ( Event *ev)
	{

	}



void Hibernate::AnimDone( Event *ev )
   {
   switch ( _state )
      {
      case HIBERNATE_START_HIBERNATE:
         _state = HIBERNATE_HIBERNATE;
      break;

	  case HIBERNATE_END_HIBERNATE:		  
		  _state = HIBERNATE_SUCCESSFUL;
	  break;

	  case HIBERNATE_WAIT:		  
		  _state = HIBERNATE_SUCCESSFUL;
	  break;

      }
   }



void Hibernate::Begin( Actor &self )
	{
   _init( self );
	}



void Hibernate::_init( Actor &self )
   {
   _nextMoveAttempt = 0.0f; 
   _moveFailures    = 0;
   
   _state = HIBERNATE_FIND_CLOSEST_NODE;
   if ( !_setupFindClosestHibernateNode( self ) )
      _setupFindClosestHibernateNodeFailed( self );
   }



BehaviorReturnCode_t	Hibernate::Evaluate ( Actor &self )
	{      
   switch ( _state )
      {
      case HIBERNATE_FIND_CLOSEST_NODE:
         _findClosestHibernateNode( self );
      break;

      case HIBERNATE_MOVING_TO_NODE:
         _moveToHibernateNode( self );
      break;

      case HIBERNATE_AT_NODE:
         _atHibernateNode( self );
      break;

      case HIBERNATE_START_HIBERNATE:
      break;

      case HIBERNATE_HIBERNATE:
         _hibernate( self );
      break;

      case HIBERNATE_END_HIBERNATE:
         _endHibernate( self );
      break;

      case HIBERNATE_HOLD:
         _hold( self );
      break;

	  case HIBERNATE_WAIT:
	  break;

      case HIBERNATE_SUCCESSFUL:
		  self.SetActorFlag(ACTOR_FLAG_IN_ALCOVE , false );
		  	self.ignoreHelperNode.node = _node;
         return BEHAVIOR_SUCCESS;
      break;

      case HIBERNATE_FAILED:
         _endHibernate( self );
         return BEHAVIOR_FAILED;
      break;
      }

   return BEHAVIOR_EVALUATING; 
	}


void Hibernate::_wait( Actor &self )
{
	self.SetAnim( "idle", EV_Actor_NotifyBehavior );   	
}

void Hibernate::End ( Actor &self	)
	{
   
	}



bool Hibernate::_setupFindClosestHibernateNode( Actor &self )
   {
   _node = self.currentHelperNode.node;
   return true;
   }



void Hibernate::_setupFindClosestHibernateNodeFailed( Actor &self )
   {
   _state = HIBERNATE_FAILED;
   }



void Hibernate::_findClosestHibernateNode( Actor &self )
   {
	if ( !_node )
		_node = HelperNode::FindClosestHelperNode( self , "hibernate" , 512.0f );   

   if ( !_node )
      {
      _findClosestHibernateNodeFailed(self);
      return;
      }

   _state = HIBERNATE_MOVING_TO_NODE;
   if (!_setupMovingToHibernateNode( self ) )
      _setupMovingToHibernateNodeFailed( self );

   }



void Hibernate::_findClosestHibernateNodeFailed( Actor &self )
   {
   _state = HIBERNATE_FAILED;
   }



bool Hibernate::_setupMovingToHibernateNode( Actor &self )
   {
   _gotoHelperNode.SetDistance( 16.0f );
   _gotoHelperNode.SetAnim( "walk" );
   _gotoHelperNode.SetPoint( _node->origin );
   _gotoHelperNode.Begin( self );

   return true;
   }



void Hibernate::_setupMovingToHibernateNodeFailed( Actor &self )
   {
   _state = HIBERNATE_FAILED;
   }



void Hibernate::_moveToHibernateNode( Actor &self )
   {
   BehaviorReturnCode_t moveResult;

   moveResult = _gotoHelperNode.Evaluate( self );
   
   if ( moveResult == BEHAVIOR_SUCCESS )
      {
      _nextMoveAttempt = 0.0f;
      _moveFailures    = 0;
      _state = HIBERNATE_AT_NODE;
      if ( !_setupAtHibernateNode( self ) )
         _setupAtHibernateNodeFailed( self );

      }  
   else if ( moveResult == BEHAVIOR_FAILED )
      {
      _moveToHibernateNodeFailed( self );
      }
   }



void Hibernate::_moveToHibernateNodeFailed( Actor &self )
   {
   _moveFailures++;

   if ( _moveFailures > 10 )
      {
      _state = HIBERNATE_FAILED;
      }
   else
      {
      _state = HIBERNATE_HOLD;
      if ( !_setupHold( self ) )
         _setupHoldFailed( self );
      }

   }



bool Hibernate::_setupAtHibernateNode( Actor &self )
   {
   Vector nodeAngles;

   
   nodeAngles = _node->angles;
	
	self.movementSubsystem->setAnimDir( nodeAngles );					
   self.setAngles( nodeAngles );

   return true;
   }



void Hibernate::_setupAtHibernateNodeFailed( Actor &self )
   {
   _state = HIBERNATE_FAILED;
   }


void Hibernate::_atHibernateNode( Actor &self )
   {
   _node->RunEntryThread();
   _state = HIBERNATE_START_HIBERNATE;

   self.setOrigin( _node->origin );
   _startHibernate( self );
   }



bool Hibernate::_setupHold( Actor &self )
   {
   self.SetAnim( "idle" );
   _nextMoveAttempt = level.time + 0.5f + G_Random();
   return true;
   }



void Hibernate::_setupHoldFailed( Actor &self )
   {
   _state = HIBERNATE_FAILED;
   }



void Hibernate::_hold( Actor &self )
   {
   if ( level.time > _nextMoveAttempt )
      {
      _state = HIBERNATE_MOVING_TO_NODE;
      if ( !_setupMovingToHibernateNode( self ) )
         _setupMovingToHibernateNodeFailed( self );
      }
    
   }

void Hibernate::_startHibernate( Actor &self )
   {
   self.SetAnim( "alcove_deactivate", EV_Actor_NotifyBehavior );
   }

void Hibernate::_hibernate( Actor &self )
   {
	float tendencyToHibernate = self.personality->GetTendency( "hibernate" );
      
    if ( G_Random() > tendencyToHibernate )
		{		
		_state = HIBERNATE_END_HIBERNATE;
		}

   self.SetAnim( "alcove_idle", EV_Actor_NotifyBehavior );
   }

void Hibernate::_endHibernate( Actor &self )
   {
   self.SetAnim( "alcove_activate", EV_Actor_NotifyBehavior );    
   _state = HIBERNATE_WAIT;
   }






CLASS_DECLARATION( Behavior, GotoLiftPosition, NULL )
	{
		{ &EV_Behavior_Args,		&GotoLiftPosition::SetArgs		},
		{ &EV_Behavior_AnimDone,	&GotoLiftPosition::AnimDone	}, 
		{ NULL, NULL }
	};


void GotoLiftPosition::SetArgs ( Event *ev)
	{
	}



void GotoLiftPosition::AnimDone( Event *ev )
   {
   }



void GotoLiftPosition::Begin( Actor &self )
	{
	_init( self );
	}



void GotoLiftPosition::_init( Actor &self )
	{
	_nextMoveAttempt	= 0.0f; 
	_moveFailures		= 0;
	_node				= NULL;

	FindNodes( self );
	_state				= GLP_FIND_NODE;   
	}



BehaviorReturnCode_t	GotoLiftPosition::Evaluate ( Actor &self )
	{      
	switch ( _state )
		{
		case GLP_FIND_NODE:
			_findLiftNode( self );
		break;

		case GLP_MOVING_TO_NODE:
			_moveToLiftNode( self );
		break;

		case GLP_MOVE_FAILED:
			_moveToLiftNodeFailed( self );
		break;

		case GLP_AT_NODE:
			_atLiftNode( self );
		break;

		case GLP_SUCCESSFUL:
			return BEHAVIOR_SUCCESS;
		break;

		case GLP_FAILED:
			return BEHAVIOR_FAILED;
		break;
		}

	return BEHAVIOR_EVALUATING; 
	}



void GotoLiftPosition::End ( Actor &self	)
	{
	if ( _node )
		_node->UnreserveNode();

	_availableNodes.ClearObjectList();
	_attemptedNodes.ClearObjectList();
	}


void GotoLiftPosition::_findLiftNode( Actor &self )
	{
	Player *player;
	player = GetPlayer( 0 );

	if ( !player )
		{
		_state = GLP_FAILED;
		return;
		}

	HelperNode* theNode;
	HelperNode* checkNode;
	HelperNode* liftNode;
	bool alreadyTriedNode;
	int i, j;

	liftNode = NULL;
	for( i = 1 ; i <= _availableNodes.NumObjects(); i++ )
		{
			theNode = _availableNodes.ObjectAt(i);
			
			alreadyTriedNode = false;		
			for ( j = 1 ; j <= _attemptedNodes.NumObjects() ; j++ )
				{
				checkNode = _attemptedNodes.ObjectAt(j);
				if ( checkNode == theNode )
					{
					alreadyTriedNode = true;
					break;
					}
				}

			if ( alreadyTriedNode ) continue;

			liftNode = theNode;
		}
		
	
	if ( !liftNode )
		{
		_state = GLP_FAILED;
		return;
		}

	_node = liftNode;
	_node->ReserveNode();
	_setupMovingToLiftNode( self );
	_state = GLP_MOVING_TO_NODE;
	}

void GotoLiftPosition::_setupMovingToLiftNode( Actor &self )
	{
	_gotoHelperNode.SetAnim( "run" );
	_gotoHelperNode.SetDistance( 16.0f );
	_gotoHelperNode.SetPoint( _node->origin );
	_gotoHelperNode.Begin( self );
	}

void GotoLiftPosition::_moveToLiftNode( Actor &self )
	{
	BehaviorReturnCode_t result;
	result = _gotoHelperNode.Evaluate( self );

	if ( result == BEHAVIOR_FAILED )
		{
		_setupMoveToLiftFailed( self );
		_state = GLP_MOVE_FAILED;		
		return;
		}

	if ( result == BEHAVIOR_SUCCESS )
		{
		_setupAtLiftNode( self );
		_state = GLP_AT_NODE;
		return;
		}

	if ( result == BEHAVIOR_FAILED_STEERING_NO_PATH )
		{
		if ( !self.GetActorFlag(ACTOR_FLAG_IN_CALL_VOLUME) )
			{
			_setupMoveToLiftFailed(self);
			_state = GLP_MOVE_FAILED;
			}			
		}
		
	}

void GotoLiftPosition::_setupMoveToLiftFailed( Actor &self )
	{
	self.SetAnim( "idle" );
	_moveFailures++;
	_nextMoveAttempt = level.time + G_Random() + 0.5f;
	}

void GotoLiftPosition::_moveToLiftNodeFailed( Actor &self )
	{
	if ( level.time > _nextMoveAttempt && !self.GetActorFlag(ACTOR_FLAG_IN_CALL_VOLUME) )
		{

		_moveFailures++;
		if ( _moveFailures > 5 )
			{
			_attemptedNodes.AddObject(_node);
			_findLiftNode(self);

			_setupMovingToLiftNode( self );
			_state = GLP_MOVING_TO_NODE;
			}
		}
	}

void GotoLiftPosition::_setupAtLiftNode( Actor &self )
	{
	self.SetAnim( "idle" );
	_node->RunEntryThread();
	}

void GotoLiftPosition::_atLiftNode( Actor &self )
	{
	if ( self.checkSpecifiedFollowTargetOutOfRange() )
		self.SetActorFlag( ACTOR_FLAG_PLAYER_IN_CALL_VOLUME, false );
	
	if ( !self.GetActorFlag( ACTOR_FLAG_PLAYER_IN_CALL_VOLUME ) )
		_state = GLP_SUCCESSFUL;
	}

void GotoLiftPosition::_warpToLiftNode( Actor &self )
	{
	self.setOrigin( _node->origin );
	_setupAtLiftNode( self );
	_state = GLP_AT_NODE;
	}

void GotoLiftPosition::_warpToPathNode( Actor &self )
	{
	PathNode *goalNode = thePathManager.NearestNode( self.origin );
	if ( !goalNode )
		_warpToLiftNode( self );

	}

void GotoLiftPosition::FindNodes(Actor &self )
{
	Player* player = GetPlayer(0);
	str volumeName = player->GetCurrentCallVolume();
	HelperNode* node;
	int nodeID;
	
	for ( int i = 1 ; i <= HelperNodes.NumObjects() ; i++ )
	{   
		node = NULL;
		node = HelperNodes.ObjectAt( i );
		nodeID = node->GetID();
		
		if ( nodeID != -1 && nodeID != self.currentHelperNode.nodeID )
			continue;
		
		if ( node->isReserved() )
			continue;
		
		if ( node->isOfType(NODETYPE_CUSTOM))
		{
			str type;
			type = node->GetCustomType();
			
			if ( !stricmp(type.c_str() , "lift" ) && !stricmp(node->target, volumeName.c_str() ) )
			{
				_availableNodes.AddObject(node);
			}
			
		}		
	}
}




























