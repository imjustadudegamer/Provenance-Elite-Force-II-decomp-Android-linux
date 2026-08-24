
#include "actor.h"
#include "rotateToEntity.hpp"

CLASS_DECLARATION( Behavior, RotateToEntity, NULL )
	{
		{ &EV_Behavior_Args,			&RotateToEntity::SetArgs },
		{ NULL, NULL }
	};



RotateToEntity::RotateToEntity()
{
	_anim = "";
	_turnspeed = 10.0f;
	_ent = NULL;
	_entityType = "enemy";
}


RotateToEntity::~RotateToEntity()
{
}

void RotateToEntity::SetArgs( Event *ev )
	{

	int parmCount= ev->NumArgs();

	if ( parmCount > 0 ) 
		_turnspeed = ev->GetFloat( 1 );

	if ( parmCount > 1 ) 
		_anim = ev->GetString( 2 );

	if ( parmCount > 2 ) 
		_entityType = ev->GetString( 3 );

	}



void RotateToEntity::Begin( Actor &self )
	{       
	
	if ( !_ent )
		{
		if ( _entityType == "enemy" )
			{
			self.enemyManager->FindHighestHateEnemy();
			_ent = self.enemyManager->GetCurrentEnemy();
			}
		else
			{
			Entity *player = (Entity*)GetPlayer(0);

			if ( player && !(player->flags & FL_NOTARGET) )
				{
				_ent = player;
				}
			}
		}
  
	if ( _anim.length() && self.animate->HasAnim( _anim.c_str() ) )
		self.SetAnim( _anim );

	_oldTurnSpeed = self.movementSubsystem->getTurnSpeed();
	self.movementSubsystem->setTurnSpeed( _turnspeed );
	
	}


BehaviorReturnCode_t RotateToEntity::Evaluate( Actor &self )
	{
	Vector dir;
	Vector selfToEntity;
	
	float yawDiff;

	if ( !_ent )
		{
		SetFailureReason( "RotateToEntity: Has no Entity to Rotate To" );
		return BEHAVIOR_FAILED;
		}

	Vector entityPos;
	entityPos = _ent->centroid;
	entityPos.z = self.centroid.z;

	if ( self.IsEntityAlive( _ent ) )
		{		
		dir = self.movementSubsystem->getMoveDir();
		self.movementSubsystem->Accelerate( 
			self.movementSubsystem->SteerTowardsPoint( entityPos, vec_zero, dir, 1.0f)
			);
		}

	selfToEntity = entityPos - self.centroid;
	selfToEntity = selfToEntity.toAngles();	

	yawDiff = selfToEntity[YAW] - self.angles[YAW];

	yawDiff = AngleNormalize180(yawDiff);
	if ( yawDiff > -1.5 && yawDiff < 1.5 )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;

     
   }



void RotateToEntity::End(Actor &self)
{
	self.movementSubsystem->setTurnSpeed( _oldTurnSpeed );
}

