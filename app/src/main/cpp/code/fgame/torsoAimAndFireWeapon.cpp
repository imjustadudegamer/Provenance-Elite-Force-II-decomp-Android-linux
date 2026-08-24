
#include "actor.h"
#include "torsoAimAndFireWeapon.hpp"
#include "../qcommon/gameplaymanager.h"

CLASS_DECLARATION( Behavior, TorsoAimAndFireWeapon, NULL )
	{
		{ &EV_Behavior_Args,		&TorsoAimAndFireWeapon::SetArgs		},
		{ &EV_Behavior_AnimDone,	&TorsoAimAndFireWeapon::AnimDone	}, 
		{ NULL, NULL }
	};


TorsoAimAndFireWeapon::TorsoAimAndFireWeapon()
{	
	_endFireTime		= 0.0f;
	_endAimTime			= 0.0f;
	_maxTorsoTurnSpeed	= 15.0f;
	_maxTorsoYaw		= 90.0;
	_maxTorsoPitch		= 40.0;
	_aimOnly			= false;
	_shots				= 1;
	_fireFailed			= false;
	_repeat				= false;

}	

TorsoAimAndFireWeapon::~TorsoAimAndFireWeapon()
{
}




void TorsoAimAndFireWeapon::AnimDone( Event *ev )
{


	_animDone = true;
}





BehaviorReturnCode_t TorsoAimAndFireWeapon::Evaluate( Actor &self )
{
	BehaviorReturnCode_t stateResult;

	think();
	
	switch ( _state )
		{
		case TORSO_AIM_AND_FIRE_PRE_FIRE:
			stateResult = evaluateStatePreFire();

			if ( stateResult == BEHAVIOR_SUCCESS )
				{
					transitionToState( TORSO_AIM_AND_FIRE_AIM );	
				}

			if ( stateResult == BEHAVIOR_FAILED )
				{
				transitionToState( TORSO_AIM_AND_FIRE_FAILED );
				}
		break;

		case TORSO_AIM_AND_FIRE_AIM:
			stateResult = evaluateStateAim();
						
			if ( stateResult == BEHAVIOR_SUCCESS )
				{
					if ( _aimOnly )
						transitionToState(TORSO_AIM_AND_FIRE_AIM);
					else
						transitionToState( TORSO_AIM_AND_FIRE_ATTACK );
				}
			
			if ( stateResult == BEHAVIOR_FAILED )
				return BEHAVIOR_FAILED;

		break;


		case TORSO_AIM_AND_FIRE_ATTACK:
			stateResult = evaluateStateAttack();

			if ( stateResult == BEHAVIOR_FAILED )
				{
				_fireWeapon.End( *_self );

				if ( _postFireAnim.length() )
					transitionToState( TORSO_AIM_AND_FIRE_POST_FIRE );
				else
					transitionToState( TORSO_AIM_AND_FIRE_FAILED );
				}

			if ( stateResult == BEHAVIOR_SUCCESS )
				{
				_fireWeapon.End( *_self );

				if ( _postFireAnim.length() )
					transitionToState( TORSO_AIM_AND_FIRE_POST_FIRE );
				else
					transitionToState( TORSO_AIM_AND_FIRE_SUCCESS );
				}
		break;

		case TORSO_AIM_AND_FIRE_POST_FIRE:
			stateResult = evaluateStatePostFire();

			if ( stateResult == BEHAVIOR_SUCCESS )
				{
					transitionToState( TORSO_AIM_AND_FIRE_SUCCESS );	
				}
		break;

		case TORSO_AIM_AND_FIRE_SUCCESS:
			_self->SetControllerAngles( ACTOR_TORSO_TAG, vec_zero );

			if ( _repeat )
				{
				Begin(self);
				return BEHAVIOR_EVALUATING;
				}

			return BEHAVIOR_SUCCESS;

		break;


		case TORSO_AIM_AND_FIRE_FAILED:
			if ( _repeat )
				{
				Begin(self);
				return BEHAVIOR_EVALUATING;
				}

			return BEHAVIOR_FAILED;

		break;


		}
	

	return BEHAVIOR_EVALUATING;   

}



// NOTE: this is TorsoAimAndFireWeapon::End, not FireWeapon::End -- the two were
void TorsoAimAndFireWeapon::End(Actor &self)
{
	if ( !_self )
		return;

	_fireWeapon.End(*_self);
	_self->SetControllerAngles( ACTOR_TORSO_TAG, vec_zero );
	_self->ClearTorsoAnim();
}



void TorsoAimAndFireWeapon::transitionToState( TorsoAimAndFireStates_t state )
{	
	switch ( state )
		{	
		case TORSO_AIM_AND_FIRE_AIM:
			setupStateAim();
			setInternalState( state , "TORSO_AIM_AND_FIRE_AIM" );
		break;

		case TORSO_AIM_AND_FIRE_PRE_FIRE:
			setupStatePreFire();
			setInternalState( state , "TORSO_AIM_AND_FIRE_PRE_FIRE" );
		break;

		case TORSO_AIM_AND_FIRE_ATTACK:
			setupStateAttack();
			setInternalState( state , "TORSO_AIM_AND_FIRE_ATTACK" );
		break;

		case TORSO_AIM_AND_FIRE_POST_FIRE:
			setupStatePostFire();
			setInternalState( state , "TORSO_AIM_AND_FIRE_POST_FIRE" );
		break;

		case TORSO_AIM_AND_FIRE_SUCCESS:
			setInternalState( state , "TORSO_AIM_AND_FIRE_SUCCESS" );
		break;

		case TORSO_AIM_AND_FIRE_FAILED:
			setInternalState( state , "TORSO_AIM_AND_FIRE_FAILED" );
		break;
		}
	
}


void TorsoAimAndFireWeapon::setInternalState( TorsoAimAndFireStates_t state , const str &stateName )
{
	_state = state;
	SetInternalStateName( stateName );
}

void TorsoAimAndFireWeapon::init( Actor &self )
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

	
	if ( gpm->hasProperty( objname , "AimTimeMin" ) )
		_aimTimeMin = gpm->getFloatValue( objname , "AimTimeMin" );

	if ( gpm->hasProperty( objname , "AimTimeMax" ) )
		_aimTimeMax = gpm->getFloatValue( objname , "AimTimeMax" );

	if ( gpm->hasProperty( objname , "FireTimeMin" ) )
		_fireTimeMin = gpm->getFloatValue( objname , "FireTimeMin" );

	if ( gpm->hasProperty( objname , "FireTimeMax" ) )
		_fireTimeMax = gpm->getFloatValue( objname , "FireTimeMax" );


	if ( gpm->hasProperty( objname , "ShotCount" ) )
		_shots = (int)gpm->getFloatValue( objname , "ShotCount" );

	float spreadX, spreadY;
	spreadX = self.combatSubsystem->GetDataForMyWeapon( "spreadx" );
	spreadY = self.combatSubsystem->GetDataForMyWeapon( "spready" );
	self.combatSubsystem->OverrideSpread( spreadX , spreadY );


	_self->shotsFiredThisVolley = 0;

	updateEnemy();
	
}

void TorsoAimAndFireWeapon::think()
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


void TorsoAimAndFireWeapon::updateEnemy()
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

void TorsoAimAndFireWeapon::LerpTorsoBySpeed( const Vector &angleDelta )
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

void TorsoAimAndFireWeapon::AdjustTorsoAngles( const Vector &tagPos , const Vector &watchPosition )
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


void TorsoAimAndFireWeapon::setupStateAim()
{
	_animDone = false;

	if ( _aimTimeMax + _aimTimeMin > 0 )
		_endAimTime = level.time + G_Random(_aimTimeMax - _aimTimeMin ) + _aimTimeMin;
	else
		_endAimTime = -1.0;
	
	_self->SetAnim( _aimAnim , EV_Actor_NotifyTorsoBehavior , torso );	
}

BehaviorReturnCode_t TorsoAimAndFireWeapon::evaluateStateAim()
{
	bool tryToFire = false;

	if ( _endAimTime > 0 && level.time > _endAimTime )
		tryToFire = true;
		

	if ( _endAimTime < 0 && _animDone )
		tryToFire = true;

	if ( tryToFire )
		{
		if ( _self->combatSubsystem->CanAttackEnemy() || _forceAttack )
			return BEHAVIOR_SUCCESS;	
		
		if ( _endOnAimFail )
			return BEHAVIOR_FAILED;
		}
	
	return BEHAVIOR_EVALUATING;
}

void TorsoAimAndFireWeapon::failureStateAim( const str& failureReason )
{
	SetFailureReason( failureReason );
}


void TorsoAimAndFireWeapon::setupStatePreFire()
{

	_animDone = false;
	_self->SetAnim( _preFireAnim , EV_Actor_NotifyTorsoBehavior , torso );


}

BehaviorReturnCode_t TorsoAimAndFireWeapon::evaluateStatePreFire()
{
	if ( _animDone )
		return BEHAVIOR_SUCCESS;

	if ( !_canAttack )
		return BEHAVIOR_FAILED;

	return BEHAVIOR_EVALUATING;
}

void TorsoAimAndFireWeapon::failureStatePreFire( const str& failureReason )
{

}


void TorsoAimAndFireWeapon::setupStateAttack()
{
	_animDone = false;

	if ( _fireTimeMax + _fireTimeMax > 0 )
		_endFireTime = level.time + G_Random(_fireTimeMax - _fireTimeMax) + _fireTimeMin;
	else
		_endFireTime = -1.0;


	if ( _self->combatSubsystem->CanAttackEnemy() || _forceAttack )
		{	
		_fireWeapon.SetTarget(_currentEnemy);
		_fireWeapon.SetAnim( _fireAnim ); 
		_fireWeapon.Begin( *_self );
		}
	else
		{
		_fireWeapon.End( *_self );
		_fireFailed = true;
		}

}

BehaviorReturnCode_t TorsoAimAndFireWeapon::evaluateStateAttack()
{
	[[maybe_unused]] BehaviorReturnCode_t result;
		

	if ( _fireFailed )
		return BEHAVIOR_FAILED;

	result = _fireWeapon.Evaluate( *_self );
	if ( _self->shotsFiredThisVolley >= _shots )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_EVALUATING;
}

void TorsoAimAndFireWeapon::failureStateAttack( const str& failureReason )
{
	SetFailureReason( failureReason );
	_fireWeapon.End( *_self );
}

void TorsoAimAndFireWeapon::setupStatePostFire()
{
	_animDone = false;

	_self->SetAnim( _postFireAnim , EV_Actor_NotifyTorsoBehavior , torso );
}

BehaviorReturnCode_t TorsoAimAndFireWeapon::evaluateStatePostFire()
{
	if ( _animDone )
		return BEHAVIOR_SUCCESS;

	return BEHAVIOR_FAILED;
}

void TorsoAimAndFireWeapon::failureStatePostFire( const str& failureReason )
{

}












































void TorsoAimAndFireWeapon::Begin( Actor &self )
{       
	init( self );

	if (_preFireAnim.length() && !_aimOnly )
		transitionToState ( TORSO_AIM_AND_FIRE_PRE_FIRE );
	else
		transitionToState ( TORSO_AIM_AND_FIRE_AIM );
}

void TorsoAimAndFireWeapon::SetArgs( Event *ev )
{	
	_forceAttack		= ev->GetBoolean( 1 );
	if ( ev->NumArgs() > 1 )
		_aimOnly = ev->GetBoolean( 2 );

	if ( ev->NumArgs() > 2 )
		_repeat = ev->GetBoolean( 3 );

	if ( ev->NumArgs() > 3 )
		_endOnAimFail = ev->GetBoolean( 4 );
}


