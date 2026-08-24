
#include "actor.h"
#include "player.h"
#include "teleportToEntity.hpp"

CLASS_DECLARATION( Behavior, AnimatedTeleportToPlayer, NULL )
	{
		{ &EV_Behavior_Args, &AnimatedTeleportToPlayer::SetArgs },
		{ NULL, NULL			     }
	};


void AnimatedTeleportToPlayer::SetArgs( Event *ev )
{
	_startAnim = ev->GetString( 1 );
	_endAnim   = ev->GetString( 2 );
}

void AnimatedTeleportToPlayer::Begin( Actor &self )
{
	_state = ANIM_TELEPORT_BEGIN;
}

BehaviorReturnCode_t AnimatedTeleportToPlayer::Evaluate ( Actor &self )
{
	int current_position;	
	qboolean teleport_position_found;
	Vector new_position;
	int i;
	Vector dir;
	Vector angles;
	
	Player *player = NULL;
	Player *temp_player = NULL;
	for(i = 0; i < game.maxclients; i++)
		{
		player = GetPlayer(i);		
		
		if ( temp_player && !( temp_player->flags & FL_NOTARGET ) )
			{
			player = temp_player;
			break;
			}
		}

	if ( !player )
		return BEHAVIOR_SUCCESS;
	
	switch ( _state )
		{
		case ANIM_TELEPORT_BEGIN:

			_teleportPosition = self.origin;
			teleport_position_found = false;

			current_position = TELEPORT_BEHIND;

			if ( testPosition( self, current_position, new_position, player, true ) )
				{
				_teleportPosition = new_position;
				teleport_position_found = true;
				}
			else
				{
				if ( testPosition( self, current_position, new_position, player, false ) )
					{
					_teleportPosition = new_position;
					teleport_position_found = true;
					}
				}

			if ( !teleport_position_found )
				return BEHAVIOR_FAILED;

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
			_teleportPosition = self.origin;
			teleport_position_found = false;

			current_position = TELEPORT_BEHIND;

			if ( testPosition( self, current_position, new_position, player, true ) )
				{
				_teleportPosition = new_position;
				teleport_position_found = true;
				}
			else
				{
				if ( testPosition( self, current_position, new_position, player, false ) )
					{
					_teleportPosition = new_position;
					teleport_position_found = true;
					}
				}

				if ( !teleport_position_found )
					{
					_state = ANIM_TELEPORT_END_ANIM;
					break;
					}

			self.setOrigin( _teleportPosition );
			self.NoLerpThisFrame();

			dir = player->origin - _teleportPosition;
			angles = dir.toAngles();

			angles[ROLL] = 0.0f;
			angles[PITCH] = 0.0f;

			self.setAngles( angles );
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

void AnimatedTeleportToPlayer::End( Actor &self )
{
}


bool AnimatedTeleportToPlayer::testPosition( Actor &self, int test_pos, Vector &good_position, Entity* player, bool use_player_dir )
{
	Vector test_position;
	Vector player_angles;
	Vector player_forward;
	Vector player_left;
	trace_t trace;


	test_position = player->origin;
	
	if ( use_player_dir )
		{

		player_angles = player->angles;
		player_angles.AngleVectors( &player_forward, &player_left );
		
		test_position -= player_forward * 128.0f;
		}
	else
		{
		test_position += Vector(-128, 0, 0);
		}

	test_position += Vector(0, 0, 64);


	trace = G_Trace( test_position, self.mins, self.maxs, test_position - Vector( "0 0 250" ), &self, self.edict->clipmask, false, "Teleport::TestPosition" );

	if ( trace.allsolid || trace.startsolid )
		return false;

	if ( trace.fraction == 1.0 )
		return false;




	good_position = trace.endpos;
	return true;
}




