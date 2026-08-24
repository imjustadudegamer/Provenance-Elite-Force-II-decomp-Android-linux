
#include "actor.h"
#include "snipeEnemy.hpp"
#include "../qcommon/gameplaymanager.h"

CLASS_DECLARATION( Behavior, SnipeEnemy, NULL )
	{
		{ &EV_Behavior_Args,		&SnipeEnemy::SetArgs	},
		{ &EV_Behavior_AnimDone,	&SnipeEnemy::AnimDone	}, 
		{ NULL, NULL }
	};


SnipeEnemy::SnipeEnemy()
{	
	_maxTorsoTurnSpeed	= 15.0f;
	_maxTorsoYaw		= 90.0;
	_maxTorsoPitch		= 40.0;
	_shots				= 1;
	_fireFailed			= false;

}	

SnipeEnemy::~SnipeEnemy()
{
}

void SnipeEnemy::SetArgs( Event *ev )
{	
	_aimTime = ev->GetFloat( 1 );
	_lockDownTime = ev->GetFloat( 2 );

	if ( ev->NumArgs() > 2 )
		{
		_targetSpread.x = ev->GetFloat(3);
		_targetSpread.y	= ev->GetFloat(4);
		_targetSpread.z	= ev->GetFloat(5);
		}
	
}



void SnipeEnemy::AnimDone( Event *ev )
{
	_animDone = true;
}



void SnipeEnemy::Begin( Actor &self )
{       
	init( self );

	if (_preFireAnim.length())
		transitionToState ( SNIPE_AIM_AND_FIRE_PRE_FIRE );
	else
		transitionToState ( SNIPE_AIM_AND_FIRE_AIM );
}


BehaviorReturnCode_t SnipeEnemy::Evaluate( Actor &self )
{
	BehaviorReturnCode_t stateResult;

	think();
	
	switch ( _state )
		{
		case SNIPE_AIM_AND_FIRE_PRE_FIRE:
			stateResult = evaluateStatePreFire();

			if ( stateResult == BEHAVIOR_SUCCESS )
				{
					transitionToState( SNIPE_AIM_AND_FIRE_AIM );	
				}

			if ( stateResult == BEHAVIOR_FAILED )
				{
				transitionToState( SNIPE_AIM_AND_FIRE_FAILED );
				}
		break;

		case SNIPE_AIM_AND_FIRE_AIM:
			stateResult = evaluateStateAim();
						
			if ( stateResult == BEHAVIOR_SUCCESS )
				{
				transitionToState( SNIPE_AIM_AND_FIRE_ATTACK );
				}
			
			if ( stateResult == BEHAVIOR_FAILED )
				{
				transitionToState( SNIPE_AIM_AND_FIRE_FAILED );
				}
		break;


		case SNIPE_AIM_AND_FIRE_ATTACK:
			stateResult = evaluateStateAttack();

			if ( stateResult == BEHAVIOR_FAILED )
				{
				_fireWeapon.End( *_self );

				if ( _postFireAnim.length() )
					transitionToState( SNIPE_AIM_AND_FIRE_POST_FIRE );
				else
					transitionToState( SNIPE_AIM_AND_FIRE_FAILED );
				}

			if ( stateResult == BEHAVIOR_SUCCESS )
				{
				_fireWeapon.End( *_self );

				if ( _postFireAnim.length() )
					transitionToState( SNIPE_AIM_AND_FIRE_POST_FIRE );
				else
					transitionToState( SNIPE_AIM_AND_FIRE_SUCCESS );
				}
		break;

		case SNIPE_AIM_AND_FIRE_POST_FIRE:
			stateResult = evaluateStatePostFire();

			if ( stateResult == BEHAVIOR_SUCCESS )
				{
					transitionToState( SNIPE_AIM_AND_FIRE_SUCCESS );	
				}
		break;

		case SNIPE_AIM_AND_FIRE_SUCCESS:
			_self->SetControllerAngles( ACTOR_TORSO_TAG, vec_zero );
			return BEHAVIOR_SUCCESS;

		break;


		case SNIPE_AIM_AND_FIRE_FAILED:
			return BEHAVIOR_FAILED;

		break;


		}
	

	return BEHAVIOR_EVALUATING;   

}



void SnipeEnemy::End(Actor &self)
{ 
	if ( !_self )
		return;


	_fireWeapon.End(*_self);
	_self->SetControllerAngles( ACTOR_TORSO_TAG, vec_zero );	
	_self->ClearTorsoAnim();
	_self->SetEnemyTargeted( false );
}



void SnipeEnemy::transitionToState( SnipeAimAndFireStates_t state )
{	
	switch ( state )
		{	
		case SNIPE_AIM_AND_FIRE_AIM:
			setupStateAim();
			setInternalState( state , "SNIPE_AIM_AND_FIRE_AIM" );
		break;

		case SNIPE_AIM_AND_FIRE_PRE_FIRE:
			setupStatePreFire();
			setInternalState( state , "SNIPE_AIM_AND_FIRE_PRE_FIRE" );
		break;

		case SNIPE_AIM_AND_FIRE_ATTACK:
			setupStateAttack();
			setInternalState( state , "SNIPE_AIM_AND_FIRE_ATTACK" );
		break;

		case SNIPE_AIM_AND_FIRE_POST_FIRE:
			setupStatePostFire();
			setInternalState( state , "SNIPE_AIM_AND_FIRE_POST_FIRE" );
		break;

		case SNIPE_AIM_AND_FIRE_SUCCESS:
			setInternalState( state , "SNIPE_AIM_AND_FIRE_SUCCESS" );
		break;

		case SNIPE_AIM_AND_FIRE_FAILED:
			setInternalState( state , "SNIPE_AIM_AND_FIRE_FAILED" );
		break;
		}
	
}


void SnipeEnemy::setInternalState( SnipeAimAndFireStates_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );
}

void SnipeEnemy::init( Actor &self )
{
	_self = &self;

	self.SetControllerTag( ACTOR_TORSO_TAG, gi.Tag_NumForName( self.edict->s.modelindex, "Bip01 Spine1" ) );
	_currentTorsoAngles = self.GetControllerAngles( ACTOR_TORSO_TAG );

	_animDone = false;
	_canAttack = true;

	_aimAnim		= "idle";
	_preFireAnim	= "idle";
	_fireAnim		= "idle";
	_postFireAnim	= "idle";

	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	if ( !gpm->hasObject(self.getArchetype()) )
		return;
		
	str objname = self.combatSubsystem->GetActiveWeaponArchetype();	
	objname = "Hold" + objname;

	if ( gpm->hasProperty(objname, "Aim") )
		_aimAnim = gpm->getStringValue( objname , "Aim" );
	
	if ( gpm->hasProperty( objname , "PreFire" ) )
		_preFireAnim = gpm->getStringValue( objname , "PreFire" );

	if ( gpm->hasProperty( objname , "Fire" ) )
		_fireAnim = gpm->getStringValue( objname , "Fire" );

	if ( gpm->hasProperty( objname , "PostFire" ) )
		_postFireAnim = gpm->getStringValue( objname , "PostFire" );

	if ( gpm->hasProperty( objname , "ShotCount" ) )
		_shots = (int)gpm->getFloatValue( objname , "ShotCount" );

	float spreadX, spreadY;
	spreadX = self.combatSubsystem->GetDataForMyWeapon( "spreadx" );
	spreadY = self.combatSubsystem->GetDataForMyWeapon( "spready" );
	self.combatSubsystem->OverrideSpread( spreadX , spreadY );


	_self->shotsFiredThisVolley = 0;

	updateEnemy();
	
}

void SnipeEnemy::think()
{
	int    tagNum;	
	Vector tagPos;	
	Vector watchPosition;
	Actor *actTarget;
   actTarget = NULL;
   str targetBone;


  	tagNum = gi.Tag_NumForName( _self->edict->s.modelindex, "Bip01 Spine1" );

	if ( tagNum < 0 )
		return;

	_self->GetTag( "Bip01 Spine1", &tagPos );

   if ( !_currentEnemy )
      {
      LerpTorsoBySpeed( vec_zero );      
      return;
      }

	if ( _currentEnemy->isSubclassOf( Actor ) )
		{
		actTarget = (Actor *)(Entity *)_currentEnemy;

		if ( !actTarget->isThinkOn() )
			{
			_currentEnemy = NULL;
			actTarget = NULL;
			return;
			}
		}

	targetBone = _currentEnemy->getTargetPos();

	watchPosition = _currentEnemy->centroid;

	if ( targetBone.length() )
		{
		if ( gi.Tag_NumForName( _currentEnemy->edict->s.modelindex , targetBone ) > 0 )
			{
			_currentEnemy->GetTag( targetBone.c_str() , &watchPosition , NULL , NULL , NULL );
			}		
		}

	


   AdjustTorsoAngles( tagPos , watchPosition );

}


void SnipeEnemy::updateEnemy()
{
	
	Entity *currentEnemy = _self->enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		{
		_self->enemyManager->FindHighestHateEnemy();
		currentEnemy = _self->enemyManager->GetCurrentEnemy();
		if ( !currentEnemy )
			{
			[[maybe_unused]] Player* player;
			player = GetPlayer( 0 );
			}
			
		}

	_currentEnemy = currentEnemy;
}

void SnipeEnemy::LerpTorsoBySpeed( const Vector &angleDelta )
   {
   Vector anglesDiff;
   Vector change;
   Vector finalAngles;
   Vector currentTorsoAngles;

   anglesDiff = angleDelta;

   _self->SetControllerTag( ACTOR_TORSO_TAG, gi.Tag_NumForName( _self->edict->s.modelindex, "Bip01 Spine1" ) );


	change = anglesDiff - _currentTorsoAngles;

	if ( change[YAW] > _maxTorsoTurnSpeed )
		anglesDiff[YAW] = _currentTorsoAngles[YAW] + _maxTorsoTurnSpeed;
	else if ( change[YAW] < -_maxTorsoTurnSpeed )
		anglesDiff[YAW] = _currentTorsoAngles[YAW] - _maxTorsoTurnSpeed;

	if ( change[PITCH] > _maxTorsoTurnSpeed )
		anglesDiff[PITCH] = _currentTorsoAngles[PITCH] + _maxTorsoTurnSpeed;
	else if ( change[PITCH] < -_maxTorsoTurnSpeed )
		anglesDiff[PITCH] = _currentTorsoAngles[PITCH] - _maxTorsoTurnSpeed;

   
   finalAngles = anglesDiff;
   
   _self->SetControllerAngles( ACTOR_TORSO_TAG, finalAngles );


	_currentTorsoAngles = anglesDiff;

   }

void SnipeEnemy::AdjustTorsoAngles( const Vector &tagPos , const Vector &watchPosition )
   {
	Vector dir;
	Vector angles;
	Vector anglesDiff;
   [[maybe_unused]] float  yawChange;
   [[maybe_unused]] float  pitchChange;

   
	dir = watchPosition - tagPos;
	angles = dir.toAngles();

	anglesDiff = angles - _self->angles;
		

	anglesDiff[YAW]   = AngleNormalize180( anglesDiff[YAW] );
	anglesDiff[PITCH] = AngleNormalize180( anglesDiff[PITCH] );

	yawChange = anglesDiff[YAW];
	pitchChange = anglesDiff[PITCH];

	if ( anglesDiff[YAW] < -_maxTorsoYaw )
		anglesDiff[YAW] = -_maxTorsoYaw;
	else if ( anglesDiff[YAW] > _maxTorsoYaw )
		anglesDiff[YAW] = _maxTorsoYaw;

	if ( anglesDiff[PITCH] < -_maxTorsoPitch )
		anglesDiff[PITCH] = -_maxTorsoPitch;
	else if ( anglesDiff[PITCH] > _maxTorsoPitch )
		anglesDiff[PITCH] = _maxTorsoPitch;

	anglesDiff[ROLL] = 0.0f;


   LerpTorsoBySpeed( anglesDiff );   

   }


void SnipeEnemy::setupStateAim()
{
	Weapon			*weapon;
	_animDone = false;
	_endAimTime = level.time + _aimTime;
	_endLockDownTime = level.time + _lockDownTime;
	_self->SetAnim( _aimAnim , EV_Actor_NotifyTorsoBehavior , torso );	
	_self->SetEnemyTargeted( true );

	weapon = _self->GetActiveWeapon( WeaponHandNameToNum( "dual" ) );
	
	if ( weapon )
		weapon->playAnim( "aim" );
}

BehaviorReturnCode_t SnipeEnemy::evaluateStateAim()
{
	str targetBone;

	if ( level.time > _endAimTime )
		{
		_self->SetEnemyTargeted( false );
		return BEHAVIOR_SUCCESS;
		}

	if ( _self->combatSubsystem->CanAttackEnemy() )
		{
		targetBone = _currentEnemy->getTargetPos();
		_lastGoodPosition = _currentEnemy->centroid;

		if ( targetBone.length() )
			{
			if ( gi.Tag_NumForName( _currentEnemy->edict->s.modelindex , targetBone ) > 0 )
				{
				_currentEnemy->GetTag( targetBone.c_str() , &_lastGoodPosition , NULL , NULL , NULL );
				}		
			}
		}
	else
		{
		if ( level.time < _endLockDownTime )
			{
			_self->SetEnemyTargeted( false );
			return BEHAVIOR_FAILED;
			}
		}

	return BEHAVIOR_EVALUATING;
}

void SnipeEnemy::failureStateAim( const str& failureReason )
{
	SetFailureReason( failureReason );
}


void SnipeEnemy::setupStatePreFire()
{
	_animDone = false;
	_self->SetAnim( _preFireAnim , EV_Actor_NotifyTorsoBehavior , torso );
}

BehaviorReturnCode_t SnipeEnemy::evaluateStatePreFire()
{
	if ( _animDone )
		return BEHAVIOR_SUCCESS;

	if ( !_canAttack )
		return BEHAVIOR_FAILED;

	return BEHAVIOR_EVALUATING;
}

void SnipeEnemy::failureStatePreFire( const str& failureReason )
{

}


void SnipeEnemy::setupStateAttack()
{
	float spreadx = G_CRandom(_targetSpread.x ) + _targetSpread.x;
	float spready = G_CRandom(_targetSpread.y ) + _targetSpread.y;
	float spreadz = G_CRandom(_targetSpread.z ) + _targetSpread.z;

	_lastGoodPosition.x = _lastGoodPosition.x + spreadx;
	_lastGoodPosition.y = _lastGoodPosition.y + spready;
	_lastGoodPosition.z = _lastGoodPosition.z + spreadz;
	
	_animDone = false;
	_fireWeapon.SetTargetPosition(_lastGoodPosition);
	_fireWeapon.SetAnim( _fireAnim ); 
	_fireWeapon.Begin( *_self );

}

BehaviorReturnCode_t SnipeEnemy::evaluateStateAttack()
{
	[[maybe_unused]] BehaviorReturnCode_t result;

	if ( _fireFailed )
		return BEHAVIOR_FAILED;

	result = _fireWeapon.Evaluate( *_self );
	if ( _self->shotsFiredThisVolley >= _shots )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void SnipeEnemy::failureStateAttack( const str& failureReason )
{
	SetFailureReason( failureReason );
	_fireWeapon.End( *_self );
}

void SnipeEnemy::setupStatePostFire()
{
	_animDone = false;
	_self->SetAnim( _postFireAnim , EV_Actor_NotifyTorsoBehavior , torso );
}

BehaviorReturnCode_t SnipeEnemy::evaluateStatePostFire()
{
	if ( _animDone )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_FAILED;
}

void SnipeEnemy::failureStatePostFire( const str& failureReason )
{

}

