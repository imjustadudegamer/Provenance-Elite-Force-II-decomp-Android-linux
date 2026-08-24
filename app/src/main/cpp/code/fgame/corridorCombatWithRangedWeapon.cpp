
#include "actor.h"
#include "corridorCombatWithRangedWeapon.hpp"
#include "actor_posturecontroller.h"

extern Event EV_PostureChanged_Completed;

const float CorridorCombatWithRangedWeapon::NODE_RADIUS = 32.0f;


CLASS_DECLARATION( Behavior, CorridorCombatWithRangedWeapon, NULL )
	{
		{ &EV_Behavior_Args,		&CorridorCombatWithRangedWeapon::SetArgs	},
		{ &EV_Behavior_AnimDone,	&CorridorCombatWithRangedWeapon::AnimDone	}, 
		{ &EV_PostureChanged_Completed, &CorridorCombatWithRangedWeapon::PostureDone	},
		{ NULL, NULL }
	};


CorridorCombatWithRangedWeapon::CorridorCombatWithRangedWeapon()
{	
	_self = NULL;
	_movementAnim			= "idle";
	_torsoAnim				= "idle";
	_fireAnim				= "idle";
	_preFireAnim			= "idle";
	_postFireAnim			= "idle";
	_postureChangeChance	= 0.0f;
	_maxDistance			= 0.0f;
	_retreatDistance		= 0.0f;
	_threatDistance			= 0.0f;
	_fireTimeMin			= 0.0f;
	_fireTimeMax			= 0.0f;
	_pauseTimeMin			= 0.0f;
	_pauseTimeMax			= 0.0f;

}

CorridorCombatWithRangedWeapon::~CorridorCombatWithRangedWeapon()
{
}

void CorridorCombatWithRangedWeapon::SetArgs( Event *ev )
{
	int num = ev->NumArgs();
	if ( num >  0 ) _movementAnim			= ev->GetString(  1 );
	if ( num >  1 ) _torsoAnim				= ev->GetString(  2 );
	if ( num >  2 ) _fireAnim				= ev->GetString(  3 );
	if ( num >  3 ) _preFireAnim			= ev->GetString(  4 );
	if ( num >  4 ) _postFireAnim			= ev->GetString(  5 );
	if ( num >  5 ) _postureChangeChance	= ev->GetFloat (  6 );
	if ( num >  6 ) _maxDistance			= ev->GetFloat (  7 );
	if ( num >  7 ) _retreatDistance		= ev->GetFloat (  8 );
	if ( num >  8 ) _threatDistance			= ev->GetFloat (  9 );
	if ( num >  9 ) _fireTimeMin			= ev->GetFloat ( 10 );
	if ( num > 10 ) _fireTimeMax			= ev->GetFloat ( 11 );
	if ( num > 11 ) _pauseTimeMin			= ev->GetFloat ( 12 );
	if ( num > 12 ) _pauseTimeMax			= ev->GetFloat ( 13 );	

}



void CorridorCombatWithRangedWeapon::AnimDone( Event *ev )
{
}


void CorridorCombatWithRangedWeapon::PostureDone( Event *ev )
{
	_finishedPostureTransition = true;
}


void CorridorCombatWithRangedWeapon::Begin( Actor &self )
{       
	init( self );
}


BehaviorReturnCode_t CorridorCombatWithRangedWeapon::Evaluate( Actor &self )
{
	BehaviorReturnCode_t stateResult;


	think();

	switch ( _state )
	{
	case CORRIDORCOMBAT_WRW_FINDNODE:
		stateResult = evaluateStateFindNode();
		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( CORRIDORCOMBAT_WRW_STAND );

		if ( stateResult == BEHAVIOR_SUCCESS )
			transitionToState( CORRIDORCOMBAT_WRW_MOVETONODE );
	break;

	case CORRIDORCOMBAT_WRW_MOVETONODE:
		stateResult = evaluateStateMoveToNode();
	
		if ( stateResult == BEHAVIOR_FAILED )
			{		
			_self->SetAnim( "idle" , NULL , legs );
			transitionToState( CORRIDORCOMBAT_WRW_STAND );			
			}

		if ( stateResult == BEHAVIOR_SUCCESS )
			{
			_self->SetAnim( "idle" , NULL , legs );
			stateResult = evaluateRotate();

			if ( stateResult == BEHAVIOR_SUCCESS )
				{
				if ( checkShouldDuck() )
					transitionToState( CORRIDORCOMBAT_WRW_CHANGEPOSTURE_DUCK );
				else
					transitionToState( CORRIDORCOMBAT_WRW_STAND );
				}
			
			}

	break;

	case CORRIDORCOMBAT_WRW_BACKPEDAL:
		stateResult = evaluateStateBackPedal();

		if ( stateResult == BEHAVIOR_FAILED )
			{
			_self->movementSubsystem->setMovingBackwards( false );
			_self->SetAnim( "idle" , NULL , legs );
			_holdPositionTime = level.time + G_Random(2.0) + 2.0f;

			transitionToState( CORRIDORCOMBAT_WRW_STAND );			
			return BEHAVIOR_EVALUATING;
			}
		
		if ( stateResult == BEHAVIOR_SUCCESS )
			{
			_self->movementSubsystem->setMovingBackwards( false );
			_self->SetAnim( "idle" , NULL , legs );
			transitionToState( CORRIDORCOMBAT_WRW_FINDBETTERNODE );			
			}
	break;

	case CORRIDORCOMBAT_WRW_FINDBETTERNODE:
		stateResult = evaluateStateFindBetterNode();
		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( CORRIDORCOMBAT_WRW_STAND );

		if ( stateResult == BEHAVIOR_SUCCESS )
			transitionToState( CORRIDORCOMBAT_WRW_MOVETOBETTERNODE );			

	break;

	case CORRIDORCOMBAT_WRW_MOVETOBETTERNODE:
		stateResult = evaluateStateMoveToBetterNode();		

		if ( stateResult == BEHAVIOR_FAILED )
			{
			_self->SetAnim( "idle" , NULL , legs );
			transitionToState( CORRIDORCOMBAT_WRW_FAILED );			
			}

		if ( stateResult == BEHAVIOR_SUCCESS )
			{
			_self->SetAnim( "idle" , NULL , legs );
			stateResult = evaluateRotate();

			if ( stateResult == BEHAVIOR_SUCCESS )
				{
				if ( checkShouldDuck() )
					transitionToState( CORRIDORCOMBAT_WRW_CHANGEPOSTURE_DUCK );
				else
					transitionToState( CORRIDORCOMBAT_WRW_STAND );
				}
			
			}
	break;

	case CORRIDORCOMBAT_WRW_CHANGEPOSTURE_DUCK:
		stateResult = evaluateStateChangePostureDuck();
		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( CORRIDORCOMBAT_WRW_FAILED );

		if ( stateResult == BEHAVIOR_SUCCESS )
			transitionToState( CORRIDORCOMBAT_WRW_DUCKED );

	break;

	case CORRIDORCOMBAT_WRW_CHANGEPOSTURE_STAND:
		stateResult = evaluateStateChangePostureStand();
		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( CORRIDORCOMBAT_WRW_FAILED );

		if ( stateResult == BEHAVIOR_SUCCESS )
			transitionToState( CORRIDORCOMBAT_WRW_STAND );

	break;

	case CORRIDORCOMBAT_WRW_DUCKED:
		if ( checkShouldStand() )	
			{
			transitionToState( CORRIDORCOMBAT_WRW_CHANGEPOSTURE_STAND );
			return BEHAVIOR_EVALUATING;
			}

		stateResult = evaluateRotate();
		stateResult = evaluateStateDucked();
		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( CORRIDORCOMBAT_WRW_FAILED );

		if ( stateResult == BEHAVIOR_SUCCESS )
			transitionToState( CORRIDORCOMBAT_WRW_DUCKED_FIRING );
	break;

	case CORRIDORCOMBAT_WRW_DUCKED_FIRING:
		if ( checkShouldStand() )	
			{
			transitionToState( CORRIDORCOMBAT_WRW_CHANGEPOSTURE_STAND );
			_fireWeapon.End(*_self);
			return BEHAVIOR_EVALUATING;
			}

		stateResult = evaluateRotate();
		if ( stateResult != BEHAVIOR_SUCCESS )
			return BEHAVIOR_EVALUATING;


		stateResult = evaluateStateFireDucked();
		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( CORRIDORCOMBAT_WRW_FAILED );

		if ( stateResult == BEHAVIOR_SUCCESS )
			transitionToState( CORRIDORCOMBAT_WRW_DUCKED );

	break;

	case CORRIDORCOMBAT_WRW_STAND:
		if ( checkShouldRetreat() )
			{
			transitionToState( CORRIDORCOMBAT_WRW_BACKPEDAL );
			return BEHAVIOR_EVALUATING;
			}

		stateResult = evaluateRotate();
		stateResult = evaluateStateStanding();
		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( CORRIDORCOMBAT_WRW_FAILED );

		if ( stateResult == BEHAVIOR_SUCCESS )
			transitionToState( CORRIDORCOMBAT_WRW_STAND_FIRING );
	break;

	case CORRIDORCOMBAT_WRW_STAND_FIRING:
		if ( checkShouldRetreat() )
			{
			transitionToState( CORRIDORCOMBAT_WRW_BACKPEDAL );
			_fireWeapon.End(*_self);
			return BEHAVIOR_EVALUATING;
			}

		stateResult = evaluateRotate();
		if ( stateResult != BEHAVIOR_SUCCESS )
			return BEHAVIOR_EVALUATING;

		stateResult = evaluateStateFireStanding();
		if ( stateResult == BEHAVIOR_FAILED )
			{
			_fireWeapon.End(*_self);
			transitionToState( CORRIDORCOMBAT_WRW_FAILED );
			}

		if ( stateResult == BEHAVIOR_SUCCESS )
			{
			_fireWeapon.End(*_self);
			transitionToState( CORRIDORCOMBAT_WRW_STAND );
			}

	break;

	case CORRIDORCOMBAT_WRW_HOLD_POSITION:
		stateResult = evaluateStateHoldPosition();

		if ( stateResult == BEHAVIOR_FAILED )
			transitionToState( CORRIDORCOMBAT_WRW_FAILED );

		if ( stateResult == BEHAVIOR_SUCCESS )
			transitionToState(CORRIDORCOMBAT_WRW_FINDBETTERNODE);
	break;

	case CORRIDORCOMBAT_WRW_SUCCESS:
		return BEHAVIOR_SUCCESS;

	break;


	case CORRIDORCOMBAT_WRW_FAILED:
		return BEHAVIOR_FAILED;

	break;
	}

	return BEHAVIOR_EVALUATING;   

}



void CorridorCombatWithRangedWeapon::End(Actor &self)
{   
	if ( !_self )
		return;

	_self->movementSubsystem->setMovingBackwards( false );
	_fireWeapon.End(*_self);
}



void CorridorCombatWithRangedWeapon::transitionToState( corridorCombatStates_t state )
{
	switch ( state )
	{
	case CORRIDORCOMBAT_WRW_FINDNODE:
		setupStateFindNode();	
		setInternalState( state , "CORRIDORCOMBAT_WRW_FINDNODE" );
	break;

	case CORRIDORCOMBAT_WRW_MOVETONODE:
		setupStateMoveToNode();	
		setInternalState( state , "CORRIDORCOMBAT_WRW_MOVETONODE" );
	break;

	case CORRIDORCOMBAT_WRW_BACKPEDAL:
		setupStateBackPedal();
		setInternalState( state , "CORRIDORCOMBAT_WRW_BACKPEDAL" );
	break;

	case CORRIDORCOMBAT_WRW_FINDBETTERNODE:
		setupStateFindBetterNode();
		setInternalState( state , "CORRIDORCOMBAT_WRW_FINDBETTERNODE" );
	break;

	case CORRIDORCOMBAT_WRW_MOVETOBETTERNODE:
		setupStateMoveToBetterNode();
		setInternalState( state , "CORRIDORCOMBAT_WRW_MOVETOBETTERNODE" );
	break;

	case CORRIDORCOMBAT_WRW_CHANGEPOSTURE_DUCK:
		setupStateChangePostureDuck();
		setInternalState( state , "CORRIDORCOMBAT_WRW_CHANGEPOSTURE_DUCK" );
	break;

	case CORRIDORCOMBAT_WRW_CHANGEPOSTURE_STAND:
		setupStateChangePostureStand();
		setInternalState( state , "CORRIDORCOMBAT_WRW_CHANGEPOSTURE_STAND" );
	break;

	case CORRIDORCOMBAT_WRW_DUCKED:
		setupStateDucked();
		setInternalState( state , "CORRIDORCOMBAT_WRW_DUCKED" );
	break;

	case CORRIDORCOMBAT_WRW_DUCKED_FIRING:
		setupStateFireDucked();
		setInternalState( state , "CORRIDORCOMBAT_WRW_DUCKED_FIRING" );
	break;

	case CORRIDORCOMBAT_WRW_STAND:
		setupStateStanding();
		setInternalState( state , "CORRIDORCOMBAT_WRW_STAND" );
	break;

	case CORRIDORCOMBAT_WRW_STAND_FIRING:
		setupStateFireStanding();
		setInternalState( state , "CORRIDORCOMBAT_WRW_STAND_FIRING" );
	break;

	case CORRIDORCOMBAT_WRW_HOLD_POSITION:
		setupStateHoldPosition();
		setInternalState( state , "CORRIDORCOMBAT_WRW_HOLD_POSITION" );
	break;

	case CORRIDORCOMBAT_WRW_SUCCESS:
		setInternalState( state , "CORRIDORCOMBAT_WRW_SUCCESS" );
	break;

	case CORRIDORCOMBAT_WRW_FAILED:
		setInternalState( state , "CORRIDORCOMBAT_WRW_FAILED" );
	break;
	}
}


void CorridorCombatWithRangedWeapon::setInternalState( corridorCombatStates_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );	
}

void CorridorCombatWithRangedWeapon::init( Actor &self )
{
	_self = &self;
	transitionToState(CORRIDORCOMBAT_WRW_FINDNODE);
	_finishedPostureTransition = true;		
	_holdPositionTime = 0.0f;
	_enemyUpdateTime = 0.0f;
	updateEnemy();	
}

void CorridorCombatWithRangedWeapon::think()
{	
	updateEnemy();
}


void CorridorCombatWithRangedWeapon::updateEnemy()
{
	if ( level.time < _enemyUpdateTime )
		return;

	Entity *currentEnemy = _self->enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		{
		_self->enemyManager->FindHighestHateEnemy();
		currentEnemy = _self->enemyManager->GetCurrentEnemy();
		if ( !currentEnemy )
			{
			SetFailureReason( "CorridorCombatWithRangedWeapon::updateEnemy -- No Enemy" );
			transitionToState( CORRIDORCOMBAT_WRW_FAILED );
			}
			
		}

	_currentEnemy = currentEnemy;
	_enemyUpdateTime = level.time + G_Random() + 3.0f; 
	setupRotate();
}

void CorridorCombatWithRangedWeapon::setupStateFindNode()
{
}


BehaviorReturnCode_t CorridorCombatWithRangedWeapon::evaluateStateFindNode()
{
	if ( _node )
		_node->UnreserveNode();

	_node = HelperNode::FindClosestHelperNode( *_self , NODETYPE_COMBAT , DESCRIPTOR_CORRIDOR , _maxDistance );	

	if ( !_node )
		{		
		failureStateFindNode( "CorridorCombatWithRangedWeapon::evaluateStateFindCover() -- Cannot Find Suitable Cover Node" );
		return BEHAVIOR_FAILED;
		}

	_node->ReserveNode();
	return BEHAVIOR_SUCCESS;
}

void CorridorCombatWithRangedWeapon::failureStateFindNode(const str& failureReason)
{
	SetFailureReason( failureReason );
}



void CorridorCombatWithRangedWeapon::setupStateMoveToNode()
{
	_gotoPoint.SetAnim( _movementAnim );
	_gotoPoint.SetDistance( NODE_RADIUS );
	_gotoPoint.SetPoint( _node->origin );
	_gotoPoint.Begin( *_self );

	setTorsoAnim();
}


BehaviorReturnCode_t CorridorCombatWithRangedWeapon::evaluateStateMoveToNode()
{
	BehaviorReturnCode_t result = _gotoPoint.Evaluate( *_self );
	str failureReason = "CorridorCombatWithRangedWeapon::evaluateStateMoveToCover -- _gotoPoint component returned: " + _gotoPoint.GetFailureReason();
	
	if ( result == BEHAVIOR_FAILED )
		failureStateMoveToNode( failureReason );

	return result;		
	
}


void CorridorCombatWithRangedWeapon::failureStateMoveToNode( const str &failureReason )
{
	SetFailureReason( failureReason );
}


void CorridorCombatWithRangedWeapon::setupStateBackPedal()
{
	Vector selfToEnemy;	
	float dist;

	dist = G_Random ( 64.0f ) + 96.0f;

	selfToEnemy = _currentEnemy->origin - _self->origin;
	selfToEnemy = selfToEnemy.toAngles();
	selfToEnemy[PITCH] = 0;
	selfToEnemy[ROLL] = 0;
	_self->setAngles( selfToEnemy );
	selfToEnemy.AngleVectors( &selfToEnemy );
	_self->movementSubsystem->setMoveDir( selfToEnemy );
	_self->movementSubsystem->setAnimDir( selfToEnemy );

	_moveRandomDir.SetDistance( dist );
	_moveRandomDir.SetAnim( "backpedal" );
	_moveRandomDir.SetMode(MoveRandomDirection::RANDOM_MOVE_IN_BACK);
	_moveRandomDir.Begin(*_self);
	_moveRandomDir.SetMinDistance( dist * .75 );
	_self->movementSubsystem->setMovingBackwards( true );

}

BehaviorReturnCode_t CorridorCombatWithRangedWeapon::evaluateStateBackPedal()
{
	BehaviorReturnCode_t moveResult = _moveRandomDir.Evaluate( *_self );

	if ( moveResult == BEHAVIOR_SUCCESS )
		{
		return BEHAVIOR_SUCCESS;            
		}

	if ( moveResult == BEHAVIOR_FAILED )
		{
		failureStateBackPedal( _moveRandomDir.GetFailureReason() );           		
		return BEHAVIOR_FAILED;
		}

	return BEHAVIOR_EVALUATING; 
	
}

void CorridorCombatWithRangedWeapon::failureStateBackPedal( const str& failureReason )
{
	SetFailureReason( failureReason );
}


void CorridorCombatWithRangedWeapon::setupStateFindBetterNode()
{
}

BehaviorReturnCode_t CorridorCombatWithRangedWeapon::evaluateStateFindBetterNode()
{
	if ( _node )
		_node->UnreserveNode();

	_node = HelperNode::FindClosestHelperNodeAtDistanceFrom( *_self , _currentEnemy , NODETYPE_COMBAT , DESCRIPTOR_CORRIDOR , _maxDistance , _retreatDistance );

	if ( !_node )
		{		
		failureStateFindBetterNode( "CorridorCombatWithRangedWeapon::evaluateStateFindBetterNode() -- Cannot Find Suitable Cover Node" );
		return BEHAVIOR_FAILED;
		}

	_node->ReserveNode();
	return BEHAVIOR_SUCCESS; 
}

void CorridorCombatWithRangedWeapon::failureStateFindBetterNode( const str& failureReason )
{
	SetFailureReason( failureReason );
}

void CorridorCombatWithRangedWeapon::setupStateMoveToBetterNode()
{
	_gotoPoint.SetAnim( _movementAnim );
	_gotoPoint.SetDistance( NODE_RADIUS );
	_gotoPoint.SetPoint( _node->origin );
	_gotoPoint.Begin( *_self );
}

BehaviorReturnCode_t CorridorCombatWithRangedWeapon::evaluateStateMoveToBetterNode()
{
	BehaviorReturnCode_t result = _gotoPoint.Evaluate( *_self );
	str failureReason = "CorridorCombatWithRangedWeapon::evaluateStateMoveToBetterNode -- _gotoPoint component returned: " + _gotoPoint.GetFailureReason();
	
	if ( result == BEHAVIOR_FAILED )
		failureStateMoveToBetterNode( failureReason );

	return result;		
}

void CorridorCombatWithRangedWeapon::failureStateMoveToBetterNode( const str& failureReason )
{
	SetFailureReason( failureReason );
}


void CorridorCombatWithRangedWeapon::setupStateChangePostureDuck()
{
	bool canChange = _self->postureController->requestPosture( "DUCK" , this );

	if ( !canChange )
		{
		failureStateChangePostureDuck ( "Requested Posture is unavailable" );
		return;
		}

	_finishedPostureTransition = false;
}

BehaviorReturnCode_t CorridorCombatWithRangedWeapon::evaluateStateChangePostureDuck()
{
	if ( _finishedPostureTransition )
		return BEHAVIOR_SUCCESS;


	return BEHAVIOR_EVALUATING;

}

void CorridorCombatWithRangedWeapon::failureStateChangePostureDuck( const str &failureReason )
{
	SetFailureReason( failureReason );
}

void CorridorCombatWithRangedWeapon::setupStateChangePostureStand()
{
	bool canChange = _self->postureController->requestPosture( "STAND" , this );

	if ( !canChange )
		{
		failureStateChangePostureStand ( "Requested Posture is unavailable" );
		return;
		}

	_finishedPostureTransition = false;
}

BehaviorReturnCode_t CorridorCombatWithRangedWeapon::evaluateStateChangePostureStand()
{
	if ( _finishedPostureTransition )
		return BEHAVIOR_SUCCESS;
	
	return BEHAVIOR_EVALUATING;

}

void CorridorCombatWithRangedWeapon::failureStateChangePostureStand( const str &failureReason )
{
	SetFailureReason( failureReason );
}

void CorridorCombatWithRangedWeapon::setupStateDucked()
{	
	_nextPauseTime = level.time + G_Random( _pauseTimeMax ) + _pauseTimeMin;
	setTorsoAnim();
	_self->SetAnim( "duck" , NULL , legs );
	_fireWeapon.End(*_self);
}

BehaviorReturnCode_t CorridorCombatWithRangedWeapon::evaluateStateDucked()
{
	if ( level.time > _nextPauseTime )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;	

}

void CorridorCombatWithRangedWeapon::failureStateDucked( const str &failureReason )
{
	SetFailureReason( failureReason );
}

void CorridorCombatWithRangedWeapon::setupStateFireDucked()
{
	_nextFireTime = level.time + G_Random( _fireTimeMax ) + _fireTimeMin;
	if ( _self->combatSubsystem->CanAttackEnemy() )
		{
		_fireWeapon.SetAnim( _fireAnim ); 
		_fireWeapon.Begin( *_self );
		}
}

BehaviorReturnCode_t CorridorCombatWithRangedWeapon::evaluateStateFireDucked()
{
	_fireWeapon.Evaluate( *_self );
	_self->combatSubsystem->AimWeaponTag(_currentEnemy);	
	
	if ( level.time > _nextFireTime )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void CorridorCombatWithRangedWeapon::failureStateFireDucked( const str& failureReason )
{
	SetFailureReason( failureReason );
}



void CorridorCombatWithRangedWeapon::setupStateStanding()
{	
	_self->SetAnim( "idle" , NULL , legs);
	_nextPauseTime = level.time + G_Random( _pauseTimeMax ) + _pauseTimeMin;
	setTorsoAnim();
}

BehaviorReturnCode_t CorridorCombatWithRangedWeapon::evaluateStateStanding()
{
	if ( level.time > _nextPauseTime )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void CorridorCombatWithRangedWeapon::failureStateStanding( const str &failureReason )
{
	SetFailureReason( failureReason );
}


void CorridorCombatWithRangedWeapon::setupStateFireStanding()
{
	_self->SetAnim( "idle" , NULL , legs);
	_nextFireTime = level.time + G_Random( _fireTimeMax ) + _fireTimeMin;
	if ( _self->combatSubsystem->CanAttackTarget( _currentEnemy ) )
		{
		_fireWeapon.SetAnim( _fireAnim ); 
		_fireWeapon.Begin( *_self );
		}
}

BehaviorReturnCode_t CorridorCombatWithRangedWeapon::evaluateStateFireStanding()
{
	_fireWeapon.Evaluate( *_self );
	_self->combatSubsystem->AimWeaponTag(_currentEnemy);	

	if ( level.time > _nextFireTime )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void CorridorCombatWithRangedWeapon::failureStateFireStanding( const str &failureReason )
{
	SetFailureReason( failureReason );
}

void CorridorCombatWithRangedWeapon::setupStateHoldPosition()
{
	_self->SetAnim( "idle" );
	setTorsoAnim();
	_holdPositionTime = level.time + G_Random() + 1.0f;
}

BehaviorReturnCode_t CorridorCombatWithRangedWeapon::evaluateStateHoldPosition()
{
	if ( level.time >= _holdPositionTime )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void CorridorCombatWithRangedWeapon::failureStateHoldPosition( const str& failureReason )
{
	SetFailureReason( failureReason );
}


void CorridorCombatWithRangedWeapon::setupRotate()
{
	_rotate.SetTurnSpeed( 30.0f );
	_rotate.SetEntity( _currentEnemy ); 
	_rotate.Begin( *_self );
}


BehaviorReturnCode_t CorridorCombatWithRangedWeapon::evaluateRotate()
{
	return _rotate.Evaluate( *_self );
}


void CorridorCombatWithRangedWeapon::setTorsoAnim()
{
	_self->SetAnim( _torsoAnim , NULL , torso );
}

bool CorridorCombatWithRangedWeapon::checkShouldDuck()
{
	if ( _self->WithinDistanceXY( _currentEnemy , _threatDistance ) )
		return false;

	if ( G_Random() <= _postureChangeChance )
		return true;

	return false;
}

bool CorridorCombatWithRangedWeapon::checkShouldStand()
{
	if ( _self->WithinDistanceXY( _currentEnemy , _threatDistance ) )
		return true;

	return false;
}

bool CorridorCombatWithRangedWeapon::checkShouldRetreat()
{
	if ( level.time >= _holdPositionTime && _self->WithinDistanceXY( _currentEnemy , _retreatDistance ) )
		return true;

	return false;
	
}

bool CorridorCombatWithRangedWeapon::CanExecute( float maxDistance , Actor &self )
{
	HelperNodePtr node;
	node = HelperNode::FindClosestHelperNode( self , NODETYPE_COMBAT , DESCRIPTOR_CORRIDOR , maxDistance );	

	if ( node )
		return true;

	return false;
}


