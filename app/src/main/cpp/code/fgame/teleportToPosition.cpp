
#include "actor.h"
#include "player.h"
#include "weaputils.h"
#include "teleportToPosition.hpp"

CLASS_DECLARATION( Behavior, AnimatedTeleportToPosition, NULL )
	{
		{ &EV_Behavior_Args,			&AnimatedTeleportToPosition::SetArgs },
		{ NULL, NULL }
	};




BehaviorReturnCode_t AnimatedTeleportToPosition::Evaluate( Actor &self )
{
	Vector dir;
	Vector angles;
	trace_t trace;
	str pathnode_name;	
	Vector teleport_position;
	Vector attack_position;
	float half_height;	

	Entity *currentEnemy;
	currentEnemy = self.enemyManager->GetCurrentEnemy();

	switch ( _state )
		{
		case ANIM_TELEPORT_BEGIN:

			pathnode_name = _teleportPositionName;
			pathnode_name += (int)G_Random( (float)_numberOfTeleportPositions ) + 1 ;

			_goal = thePathManager.FindNode( pathnode_name );

			if ( !_goal )
				{
				gi.WDPrintf( "Can't find position %s\n", pathnode_name.c_str() );
				SetFailureReason( "Unable to find a teleport position" );
				return BEHAVIOR_FAILED;
				}

			_state = ANIM_TELEPORT_START_ANIM;
		break;

		case ANIM_TELEPORT_START_ANIM:
			self.SetAnim( _startAnim , EV_Anim_Done , legs );
			_state = ANIM_TELEPORT_START_ANIMATING;
		break;

		case ANIM_TELEPORT_START_ANIMATING:
			if ( self.GetActorFlag( ACTOR_FLAG_ANIM_DONE ) )
				_state = ANIM_TELEPORT_TELEPORT;
		break;

		case ANIM_TELEPORT_TELEPORT:

			teleport_position = _goal->origin;

			half_height = self.maxs.z / 2.0f;
			attack_position = teleport_position;
			attack_position.z += half_height;

			MeleeAttack( attack_position, attack_position, 10000.0f, &self, MOD_TELEFRAG, self.maxs.x, -half_height, half_height, 0 );

			trace = G_Trace( teleport_position + Vector( "0 0 64" ), self.mins, self.maxs, teleport_position - Vector( "0 0 128" ), &self, MASK_PATHSOLID, false, "TeleportToPosition" );	

			if ( trace.allsolid )
				{
				gi.WDPrintf( "Failed to teleport to %s\n", _goal->targetname.c_str() );
				SetFailureReason( "Trace at my designated teleport position return All Solid" );
				return BEHAVIOR_FAILED;
				}

			teleport_position = trace.endpos;
			self.setOrigin( teleport_position );
			self.NoLerpThisFrame();

			if ( currentEnemy )
				{
				dir = currentEnemy->origin - teleport_position;
				angles = dir.toAngles();

				angles[ROLL] = 0.0f;
				angles[PITCH] = 0.0f;

				self.setAngles( angles );
				}

			_state = ANIM_TELEPORT_END_ANIM;
		break;

		case ANIM_TELEPORT_END_ANIM:
			self.SetAnim( _endAnim , EV_Anim_Done , legs );
			_state = ANIM_TELEPORT_END_ANIMATING;
		break;

		case ANIM_TELEPORT_END_ANIMATING:
			if ( self.GetActorFlag( ACTOR_FLAG_ANIM_DONE ) )
			return BEHAVIOR_SUCCESS;
		break;
	}

	return BEHAVIOR_EVALUATING;
}





void AnimatedTeleportToPosition::Begin( Actor &self )
{
	_state = ANIM_TELEPORT_BEGIN;
}

void AnimatedTeleportToPosition::End( Actor &self )
{
}

void AnimatedTeleportToPosition::SetArgs( Event *ev )
{
	_teleportPositionName		 = ev->GetString( 1 );
	_numberOfTeleportPositions	 = ev->GetInteger( 2 );
	_startAnim					 = ev->GetString( 3 );
	_endAnim					 = ev->GetString( 4 );
}


