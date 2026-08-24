

#include "actor.h"
#include "suppressionFireCombat.hpp"

CLASS_DECLARATION( Behavior, SuppressionFireCombat, NULL )
	{
		{ &EV_Behavior_Args,		&SuppressionFireCombat::SetArgs		},
		{ &EV_Behavior_AnimDone,	&SuppressionFireCombat::AnimDone	}, 
		{ NULL, NULL }
	};


SuppressionFireCombat::SuppressionFireCombat()
{	
	_nextMoveAttempt	= 0.0f;
	_endFireTime		= 0.0f;
	_endPauseTime		= 0.0f;
}

SuppressionFireCombat::~SuppressionFireCombat()
{
}

void SuppressionFireCombat::SetArgs( Event *ev )
{		
	_movementAnim		= ev->GetString( 1 );
	_torsoIdleAnim		= ev->GetString( 2 );
	_torsoAttackAnim	= ev->GetString( 3 );
	_maxDistance		= ev->GetFloat ( 4 );
	_pauseTimeMin		= ev->GetFloat ( 5 );
	_pauseTimeMax		= ev->GetFloat ( 6 );
	_fireTimeMin		= ev->GetFloat ( 7 );
	_fireTimeMax		= ev->GetFloat ( 8 );

}



void SuppressionFireCombat::AnimDone( Event *ev )
{
}



void SuppressionFireCombat::Begin( Actor &self )
{       
	init( self );
	transitionToState ( SUPPRESSION_FIRE_FIND_NODE );
}


BehaviorReturnCode_t SuppressionFireCombat::Evaluate( Actor &self )
{
	BehaviorReturnCode_t stateResult;

	think();
	
	switch ( _state )
		{
		case SUPPRESSION_FIRE_FIND_NODE:
			stateResult = evaluateStateFindNode();

			if ( stateResult == BEHAVIOR_FAILED )
				transitionToState( SUPPRESSION_FIRE_FAILED );
			else
				transitionToState( SUPPRESSION_FIRE_MOVE_TO_NODE );
		break;

		case SUPPRESSION_FIRE_MOVE_TO_NODE:
			stateResult = evaluateStateMoveToNode();

			if ( stateResult == BEHAVIOR_FAILED )
				{
				_nextMoveAttempt = level.time + G_Random() + 1.0f;				
				transitionToState( SUPPRESSION_FIRE_ATTACK );
				}

			if ( stateResult == BEHAVIOR_SUCCESS )
				{
				_atNode = true;
				transitionToState( SUPPRESSION_FIRE_ATTACK );
				_self->SetAnim ( "idle" , NULL , legs );
				_self->SetAnim ( _torsoIdleAnim , NULL , torso );
				}

		break;

		case SUPPRESSION_FIRE_ATTACK:
			if ( !_atNode && level.time > _nextMoveAttempt )
				{
				_fireWeapon.End( *_self );
				transitionToState( SUPPRESSION_FIRE_FIND_NODE );				
				}

			stateResult = evaluateStateAttack();

			if ( stateResult != BEHAVIOR_EVALUATING )
				{
				_fireWeapon.End( *_self );
				transitionToState( SUPPRESSION_FIRE_PAUSE );
				}
		break;


		case SUPPRESSION_FIRE_PAUSE:
			stateResult = evaluateStatePause();

			if ( stateResult == BEHAVIOR_SUCCESS )
				{
				updateEnemy();
				transitionToState( SUPPRESSION_FIRE_ATTACK );
				}
				
		break;

		case SUPPRESSION_FIRE_SUCCESS:
			return BEHAVIOR_SUCCESS;

		break;


		case SUPPRESSION_FIRE_FAILED:
			return BEHAVIOR_FAILED;

		break;


		}
	

	return BEHAVIOR_EVALUATING;   

}



void SuppressionFireCombat::End(Actor &self)
{   
}



void SuppressionFireCombat::transitionToState( SuppressionFireStates_t state )
{	
	switch ( state )
		{	
		case SUPPRESSION_FIRE_FIND_NODE:
			setupStateFindNode();
			setInternalState( state , "SUPPRESSION_FIRE_FIND_NODE" );
		break;

		case SUPPRESSION_FIRE_MOVE_TO_NODE:
			setupStateMoveToNode();
			setInternalState( state , "SUPPRESSION_FIRE_MOVE_TO_NODE" );
		break;

		case SUPPRESSION_FIRE_ATTACK:
			setupStateAttack();
			setInternalState( state , "SUPPRESSION_FIRE_ATTACK" );
		break;

		case SUPPRESSION_FIRE_PAUSE:
			setupStatePause();
			setInternalState( state , "SUPPRESSION_FIRE_PAUSE" );
		break;

		case SUPPRESSION_FIRE_SUCCESS:
			setInternalState( state , "SUPPRESSION_FIRE_SUCCESS" );
		break;

		case SUPPRESSION_FIRE_FAILED:
			setInternalState( state , "SUPPRESSION_FIRE_FAILED" );
		break;
		}
	
}


void SuppressionFireCombat::setInternalState( SuppressionFireStates_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );
}

void SuppressionFireCombat::init( Actor &self )
{
	_self = &self;		
	updateEnemy();
}

void SuppressionFireCombat::think()
{
}

void SuppressionFireCombat::setupStateFindNode()
{
	_node = HelperNode::FindHelperNodeClosestTo( *_self , _currentEnemy , NODETYPE_COMBAT , _maxDistance );

	if ( !_node )
		return;
}

BehaviorReturnCode_t SuppressionFireCombat::evaluateStateFindNode()
{
	if ( !_node )
		{
		str failureMsg = "No Node of Type: COMBAT in range";
		failureStateFindNode( failureMsg );
		return BEHAVIOR_FAILED;
		}

	return BEHAVIOR_SUCCESS;
}

void SuppressionFireCombat::failureStateFindNode( const str& failureReason )
{
	SetFailureReason( failureReason );
}

void SuppressionFireCombat::setupStateMoveToNode()
{	
	_gotoHNode.SetNode( _node );
	_gotoHNode.SetMovementAnim( _movementAnim );
	_gotoHNode.Begin( *_self );
}


BehaviorReturnCode_t SuppressionFireCombat::evaluateStateMoveToNode()
{
	BehaviorReturnCode_t result = _gotoHNode.Evaluate( *_self );	
	
	if ( result == BEHAVIOR_FAILED )
		{
		str failureReason = "GotoHelperNodeNearestEnemy::evaluateStateMoveToNode -- _gotoHNode component returned: " + _gotoHNode.GetFailureReason();
		failureStateMoveToNode( failureReason );
		}	

	return result;		
	
}


void SuppressionFireCombat::failureStateMoveToNode( const str &failureReason )
{
	SetFailureReason( failureReason );
}


void SuppressionFireCombat::setupStateAttack()
{
	faceEnemy();
	_endFireTime = level.time + G_Random(_fireTimeMax - _fireTimeMin) + _fireTimeMin;

	_fireWeapon.SetAnim( _torsoAttackAnim );
	_fireWeapon.Begin( *_self );	

}

BehaviorReturnCode_t SuppressionFireCombat::evaluateStateAttack()
{
	_rotateToEntity.Evaluate( *_self );
	BehaviorReturnCode_t result = _fireWeapon.Evaluate( *_self );

	if ( result == BEHAVIOR_FAILED )
		failureStateAttack( "SuppressionFireCombat::evaluateStateAttack -- FAILED" );

	if ( level.time > _endFireTime )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void SuppressionFireCombat::failureStateAttack( const str &failureReason )
{
	SetFailureReason( failureReason );
	_fireWeapon.End( *_self );
}

void SuppressionFireCombat::setupStatePause()
{
	_endPauseTime = level.time + G_Random(_pauseTimeMax - _pauseTimeMin ) + _pauseTimeMin;
	_self->SetAnim( "idle" , NULL , legs );
	_self->SetAnim( _torsoIdleAnim , NULL , torso );

}

BehaviorReturnCode_t SuppressionFireCombat::evaluateStatePause()
{
	if ( level.time > _endPauseTime )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}


void SuppressionFireCombat::failureStatePause( const str &failureReason )
{
	SetFailureReason( failureReason );
}


void SuppressionFireCombat::updateEnemy()
{
	Entity *currentEnemy = _self->enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		{
		_self->enemyManager->FindHighestHateEnemy();
		currentEnemy = _self->enemyManager->GetCurrentEnemy();
		if ( !currentEnemy )
			{
			SetFailureReason( "SUPPRESSION_FIRE_FAILED::updateEnemy -- No Enemy" );
			transitionToState( SUPPRESSION_FIRE_FAILED );
			return;
			}
			
		}

	_currentEnemy = currentEnemy;
}

void SuppressionFireCombat::faceEnemy()
{
	_rotateToEntity.SetEntity( _currentEnemy );
	_rotateToEntity.SetTurnSpeed( 30.0f );
	_rotateToEntity.Begin( *_self );	

}






















