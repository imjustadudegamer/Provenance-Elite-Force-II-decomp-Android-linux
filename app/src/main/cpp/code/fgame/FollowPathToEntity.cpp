
#include "_pch_cpp.h"
#include "actor.h"
#include "FollowPathToEntity.h"

CLASS_DECLARATION( FollowPath, FollowPathToEntity, NULL )
{
	{ NULL, NULL }
};

FollowPathToEntity::FollowPathToEntity():
	_targetEntity(NULL),
	_oldGoalPosition(0,0,0)
{
}

void FollowPathToEntity::SetGoal(Entity *entity, const float radius, Actor &self)
{
	SetRadius(radius);
	if (
			( _targetEntity == NULL ) ||
			( _targetEntity->entnum != entity->entnum)
		)
	{
		_targetEntity = entity;
		_oldGoalPosition = GetGoalPosition();
		GetPath().SetPath(self, self.origin, GetGoalPosition());
	}
}

const bool FollowPathToEntity::ClearTraceToGoal( Actor &self, const trace_t &traceToGoal, const float radius ) const
{
	assert (_targetEntity != NULL);

	bool traceHitGoal = (
						(_targetEntity->entnum == traceToGoal.entityNum) && 
						(traceToGoal.entityNum != ENTITYNUM_NONE)
						);	
	

	return ( FollowPath::ClearTraceToGoal( self, traceToGoal, radius ) || (!traceToGoal.startsolid && traceHitGoal ) );
}

const Steering::ReturnValue FollowPathToEntity::Evaluate( Actor &self)
{
	if (Vector::Distance( _oldGoalPosition, GetGoalPosition()) > GetAvoidanceDistance() )
	{
		_oldGoalPosition = GetGoalPosition();
		GetPath().SetPath(self, self.origin, GetGoalPosition() );
	}


	return FollowPath::Evaluate( self);
}
