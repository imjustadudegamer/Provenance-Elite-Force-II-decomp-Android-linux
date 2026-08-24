
#include "_pch_cpp.h"
#include "actor.h"
#include "actor_locomotion.h"
#include "GoDirectlyToPoint.h"


CLASS_DECLARATION( Steering, GoDirectlyToPoint, NULL )
{
	{ NULL, NULL }
};

GoDirectlyToPoint::GoDirectlyToPoint()
{
}

void GoDirectlyToPoint::Begin(Actor &self)
{
}

const Steering::ReturnValue GoDirectlyToPoint::Evaluate( Actor &self )
{
	ResetForces();

	if (AtDestination( self ))
	{
		return Steering::SUCCESS;
	}
	
	Vector newSteeringForce = self.movementSubsystem->SteerTowardsPoint
											( 
											_destination,			
											vec_zero,											
											self.movementSubsystem->getMoveDir(),		
											self.movementSubsystem->getMoveSpeed()		
											);
	self.movementSubsystem->Accelerate( newSteeringForce );
	
	return Steering::EVALUATING ;
}

void GoDirectlyToPoint::ShowInfo( Actor &self )
{
	Steering::ShowInfo( self );
	
	gi.Printf( "\n destination : ( %f, %f, %f ) \n", _destination.x, _destination.y, _destination.z );
}



const bool GoDirectlyToPoint::AtDestination(const Actor &self) const
{
	if ((Vector::DistanceXY( _destination, self.origin ) ) <= _radius )
	{
		return true;
	}

	return false;
}
