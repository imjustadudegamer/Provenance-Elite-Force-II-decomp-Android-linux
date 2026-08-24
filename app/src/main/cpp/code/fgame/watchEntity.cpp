
#include "actor.h"
#include "watchEntity.hpp"

CLASS_DECLARATION( Behavior, WatchEntity, NULL )
{
	{ &EV_Behavior_Args,		&WatchEntity::SetArgs	},
	{ &EV_Behavior_AnimDone,	&WatchEntity::AnimDone	}, 
	{ NULL, NULL }
};


WatchEntity::WatchEntity()
{
	_anim = "";
	_turnspeed = 10.0f;
	_ent = NULL;	
	_waitForAnim = false;
	_animDone = false;
	_holdAnim = "idle";
}

WatchEntity::~WatchEntity()
{
}

void WatchEntity::SetArgs( Event *ev )
{
	_time = ev->GetFloat( 1 );

	if ( ev->NumArgs() > 1 )
		_turnspeed = ev->GetFloat( 2 );
	if ( ev->NumArgs() > 2 )
		_anim = ev->GetString( 3 );
	if ( ev->NumArgs() > 3 )
		_waitForAnim = ev->GetInteger( 4 );
	if ( ev->NumArgs() > 4 )
		_holdAnim = ev->GetString( 5 );
	
	_forcePlayer = false;
	if ( ev->NumArgs() > 5 )
		_forcePlayer = ev->GetBoolean( 6 );


}



void WatchEntity::AnimDone( Event *ev )
{
	_animDone = true;
}



void WatchEntity::Begin( Actor &self )
{       
	init( self );
}


BehaviorReturnCode_t WatchEntity::Evaluate( Actor &self )
{  

	BehaviorReturnCode_t stateResult;

	think();

	switch ( _state )
		{
		case WATCH_HOLD:
			stateResult = evaluateStateHold(self);
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( WATCH_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( WATCH_ROTATE ); 
		break;
		

		case WATCH_ROTATE:
			stateResult = evaluateStateRotate(self);
			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( WATCH_FAILED );

			if ( stateResult == BEHAVIOR_SUCCESS )
				transitionToState( WATCH_HOLD ); 

		break;

		case WATCH_SUCCESS:
			return BEHAVIOR_SUCCESS;
		break;

		case WATCH_FAILED:
			return BEHAVIOR_FAILED;
		break;

		}

   return BEHAVIOR_EVALUATING; 

}



void WatchEntity::End(Actor &self)
{
}

void WatchEntity::transitionToState( watchEntityStates_t state )
{
	switch ( state )
		{
		case WATCH_HOLD:
			setupStateHold();
			setInternalState( state , "WATCH_HOLD" );
		break;

		case WATCH_ROTATE:
			setupStateRotate();
			setInternalState( state , "WATCH_ROTATE" );
		break;

		case WATCH_SUCCESS:			
			setInternalState( state , "WATCH_SUCCESS" );
		break;

		case WATCH_FAILED:
			setInternalState( state , "WATCH_FAILED" );
		break;		
		}
}

void WatchEntity::setInternalState( watchEntityStates_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );
}

void WatchEntity::init( Actor &self )
{
	SetSelf(&self);

	if ( _time > 0 )
		_time = level.time + _time;

	_oldTurnSpeed  = self.movementSubsystem->getTurnSpeed();

	if ( !_ent )
		{
		self.enemyManager->FindHighestHateEnemy();
		_ent = self.enemyManager->GetCurrentEnemy();

		if ( !_ent )
			{

			if ( _forcePlayer )
				_ent = (Entity*)GetPlayer( 0 );

			if ( !_ent )
				{
				transitionToState(WATCH_FAILED);			
				return;
				}
			}
		}

	transitionToState(WATCH_HOLD);	
}

void WatchEntity::think()
{
	 if ( _time > 0 && level.time >= _time )
		transitionToState(WATCH_SUCCESS);

	 if ( _waitForAnim && _animDone )
		transitionToState(WATCH_SUCCESS);
}

void WatchEntity::setupStateRotate()
{
	_animDone = false;
	GetSelf()->SetAnim( _anim , EV_Actor_NotifyBehavior , legs );
}

BehaviorReturnCode_t WatchEntity::evaluateStateRotate( Actor &self )
{
	Vector entityPos, selfToEntity, dir;

	if ( !_ent ) 
		{
		return BEHAVIOR_FAILED;	
		}

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

	float yawDiff = selfToEntity[YAW] - self.angles[YAW];

	yawDiff = AngleNormalize180(yawDiff);
	if ( yawDiff > -1.5 && yawDiff < 1.5 )
		{
		if ( _waitForAnim )
			{
			if ( _animDone ) return BEHAVIOR_SUCCESS;
			}

		return BEHAVIOR_SUCCESS;
		}
		

	return BEHAVIOR_EVALUATING;
}

void WatchEntity::rotateFailed( Actor &self )
{
}

void WatchEntity::setupStateHold()
{
	_animDone = false;
	GetSelf()->SetAnim( _holdAnim , EV_Actor_NotifyBehavior , legs );
}

BehaviorReturnCode_t WatchEntity::evaluateStateHold( Actor &self )
{
	Vector entityPos , selfToEntity;
	if ( !_ent ) 
		{
		return BEHAVIOR_FAILED;	
		}

	entityPos = _ent->centroid;
	entityPos.z = self.centroid.z;

	selfToEntity = entityPos - self.centroid;
	selfToEntity = selfToEntity.toAngles();	

	float yawDiff = selfToEntity[YAW] - self.angles[YAW];

	if ( yawDiff > 5 || yawDiff < -5 )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void WatchEntity::holdFailed( Actor &self )
{
}

