
#include "_pch_cpp.h"
#include "FollowPathToPoint.h"
#include "actor.h"

CLASS_DECLARATION( FollowPath, FollowPathToPoint, NULL )
{
	{ NULL, NULL }
};

FollowPathToPoint::FollowPathToPoint():_targetPoint(0,0,0)
{
}

void FollowPathToPoint::SetGoal(const Vector &targetPoint, const float radius, Actor &self)
{
	SetRadius(radius);
	if (!Vector::CloseEnough(_targetPoint, targetPoint, 1.0f))
	{
		_targetPoint = targetPoint;
		GetPath().SetPath(self, self.origin, GetGoalPosition());
	}
}
