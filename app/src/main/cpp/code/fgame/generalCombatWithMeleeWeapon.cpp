
#include "actor.h"
#include "generalCombatWithMeleeWeapon.hpp"

CLASS_DECLARATION( Behavior, GeneralCombatWithMeleeWeapon, NULL )
	{
		{ &EV_Behavior_Args,			&GeneralCombatWithMeleeWeapon::SetArgs  },
		{ &EV_Behavior_AnimDone,		&GeneralCombatWithMeleeWeapon::AnimDone }, 
		{ NULL, NULL }
	};


GeneralCombatWithMeleeWeapon::GeneralCombatWithMeleeWeapon()
{
	_primaryTorsoAnim		= "idle";      
	_attackAnim				= "idle";
	_rotationAnim           = "rotate";
	_rushAnim				= "run";
	_maxDistanceToEngage	= 0.0f;
	_meleeDistance			= 128.0f;
	_strafeChance			= 0.50f;
	_blockChance			= 0.50f;
	_attackChance			= 0.85f;
	_postureChangeChance	= 0.0f;
	_allowRushFailure		= false;
}

GeneralCombatWithMeleeWeapon::~GeneralCombatWithMeleeWeapon()
{
}

void GeneralCombatWithMeleeWeapon::SetArgs ( Event *ev)
{
	_primaryTorsoAnim = ev->GetString( 1 );
	_attackAnim       = ev->GetString( 2 );

	if ( ev->NumArgs() > 2 )
		_maxDistanceToEngage = ev->GetFloat( 3 );

	if ( ev->NumArgs() > 3 )
		_meleeDistance = ev->GetFloat( 4 );

	if ( ev->NumArgs() > 4 )
		_rushAnim = ev->GetString( 5 );

	if ( ev->NumArgs() > 5 )
		_allowRushFailure = ev->GetBoolean( 6 );

	if ( ev->NumArgs() > 6 ) 
		_rotationAnim = ev->GetString( 7 );

	if ( ev->NumArgs() > 7 )
		_strafeChance = ev->GetFloat( 8 );

	if ( ev->NumArgs() > 8 )
		_blockChance = ev->GetFloat( 9 );

	if ( ev->NumArgs() > 9 )
		_attackChance = ev->GetFloat( 10 );

	if ( ev->NumArgs() > 10 )
		_postureChangeChance = ev->GetFloat( 11 );


}



void GeneralCombatWithMeleeWeapon::AnimDone( Event *ev )
{
	switch ( _state )
		{
		case GENERAL_COMBAT_MELEE_STRAFE:
			_strafeComponent.AnimDone( ev );         
			_nextRushAttemptTime = 0.0f;
		break;

		case GENERAL_COMBAT_MELEE_ATTACK:
			_self->SetAnim( _primaryTorsoAnim );
			_state = GENERAL_COMBAT_MELEE_SELECT_STATE;
			_nextRushAttemptTime = 0.0f;
		break;


		}
}



void GeneralCombatWithMeleeWeapon::Begin( Actor &self )
{
	init( self );

	setUpRotate( self );  
	setHeadWatchTarget( self );
}



void GeneralCombatWithMeleeWeapon::init( Actor &self )
{
	_self = &self;
	_state = GENERAL_COMBAT_MELEE_SELECT_STATE;

	_nextRotateTime			= 0.0f;
	_nextStrafeAttemptTime	= 0.0f;
	_nextRushAttemptTime	= 0.0f;
	_exitHoldTime			= 0.0f;
	_exitBlockTime			= 0.0f;
	_nextPostureChangeTime	= 0.0f;	
}



BehaviorReturnCode_t GeneralCombatWithMeleeWeapon::Evaluate ( Actor &self )
{      
	Entity *currentEnemy;

	if ( _state != GENERAL_COMBAT_MELEE_ATTACK && _state != GENERAL_COMBAT_MELEE_CHANGE_POSTURE )
		setUpRotate( self );        
		
	faceEnemy( self );
	setHeadWatchTarget( self );


	currentEnemy = self.enemyManager->GetCurrentEnemy();

	if (currentEnemy && _maxDistanceToEngage && !self.WithinDistance( currentEnemy, _maxDistanceToEngage))
		{
		self.enemyManager->ClearCurrentEnemy();
		SetFailureReason( "My Current Enemy is farther away than my _maxDistanceToEngage\n" );
		return BEHAVIOR_FAILED; 
		}
 
	if ( _state == GENERAL_COMBAT_MELEE_FAILED )
		return BEHAVIOR_FAILED;

	if ( currentEnemy && !self.WithinDistance( currentEnemy , _meleeDistance ) && level.time > _nextRushAttemptTime )
		{
		setupRushEnemy( self );
		_state = GENERAL_COMBAT_MELEE_RUSH_ENEMY;      
		}

	switch ( _state )
		{
		case GENERAL_COMBAT_MELEE_SELECT_STATE:
			selectState( self );
		break;

		case GENERAL_COMBAT_MELEE_RUSH_ENEMY:
			rushEnemy( self );
		break;

		case GENERAL_COMBAT_MELEE_STRAFE:
			setTorsoAnim( self );
			strafe(self);
			setTorsoAnim( self );
		break;

		case GENERAL_COMBAT_MELEE_ATTACK:      
			attack( self );
		break;

		case GENERAL_COMBAT_MELEE_BLOCK:
			block( self );
		break;

		case GENERAL_COMBAT_MELEE_CHANGE_POSTURE:
			changePosture(self);
		break;

		case GENERAL_COMBAT_MELEE_HOLD:
			setTorsoAnim( self );
			hold( self );
		break;

		case GENERAL_COMBAT_MELEE_FAILED:
			return BEHAVIOR_FAILED;
		}

	return BEHAVIOR_EVALUATING; 
}



void GeneralCombatWithMeleeWeapon::End ( Actor &self	)
{
}



void GeneralCombatWithMeleeWeapon::setUpRotate( Actor &self )
{
	Entity *currentEnemy;
	currentEnemy = NULL;

	currentEnemy = self.enemyManager->GetCurrentEnemy();

	if ( !currentEnemy ) 
		return;

	_rotate.SetEntity( currentEnemy );  
}



void GeneralCombatWithMeleeWeapon::faceEnemy( Actor &self )
{
	BehaviorReturnCode_t rotateResult;	

	if ( level.time < _nextRotateTime )
		return;

	_rotate.Begin( self );
	if ( _state == GENERAL_COMBAT_MELEE_ATTACK )
		_rotate.SetAnim( _rotationAnim );      
		
	rotateResult = _rotate.Evaluate( self );

	if ( rotateResult == BEHAVIOR_SUCCESS )
		_nextRotateTime = level.time + .75f;
}



void GeneralCombatWithMeleeWeapon::setTorsoAnim( Actor &self )
{
	Event *torsoAnimEvent;
	torsoAnimEvent = new Event( EV_Torso_Anim_Done );
 
	self.SetAnim( _primaryTorsoAnim , torsoAnimEvent , torso );

}



void GeneralCombatWithMeleeWeapon::setHeadWatchTarget( Actor &self )
{
	Entity *currentEnemy;
	currentEnemy = NULL;

	currentEnemy = self.enemyManager->GetCurrentEnemy();

	if ( !currentEnemy ) 
		return;
   
	self.SetHeadWatchTarget( currentEnemy );
}



bool GeneralCombatWithMeleeWeapon::setupStrafe( Actor &self )
{
	_strafeComponent.SetMode( Strafe::STRAFE_RANDOM );
	_strafeComponent.Begin( self );

	return true;
}



void GeneralCombatWithMeleeWeapon::strafe( Actor &self )
{
	BehaviorReturnCode_t result;

	result = _strafeComponent.Evaluate( self );

	if ( result == BEHAVIOR_FAILED )
		strafeFailed( self );
	else if ( result == BEHAVIOR_SUCCESS )
		_state = GENERAL_COMBAT_MELEE_SELECT_STATE;
}



void GeneralCombatWithMeleeWeapon::strafeFailed( Actor &self )
{
	_nextStrafeAttemptTime = level.time + G_Random( 1.0 ) + 3.0f;
	_state = GENERAL_COMBAT_MELEE_SELECT_STATE;
}



bool GeneralCombatWithMeleeWeapon::setupRushEnemy( Actor &self )
{
	Entity *currentEnemy;
	currentEnemy = self.enemyManager->GetCurrentEnemy();

	if ( !currentEnemy )
		return false;

	_nextRushAttemptTime = level.time + G_Random( 0.25 ) + 1.25f;
	_rush.SetAnim ( _rushAnim );
	_rush.SetEntity( self, currentEnemy );
	_rush.SetDistance ( 96.0f );
	_rush.Begin( self );


	return true;
}



void GeneralCombatWithMeleeWeapon::setupRushEnemyFailed ( Actor &self )
{
 	if ( !_allowRushFailure )
		{
		SetFailureReason( "RushToEnemy Failed" );
		_state = GENERAL_COMBAT_MELEE_FAILED;
		return;
		}

	_nextRushAttemptTime = level.time + G_Random( 0.25 ) + 1.25f;
	_state = GENERAL_COMBAT_MELEE_SELECT_STATE;   
}



void GeneralCombatWithMeleeWeapon::rushEnemy ( Actor &self )
{
	BehaviorReturnCode_t rushResult;

	rushResult = _rush.Evaluate( self );

	Entity* currentEnemy;
	currentEnemy = self.enemyManager->GetCurrentEnemy();
	if ( self.WithinDistance( currentEnemy , _meleeDistance ) )
		rushResult = BEHAVIOR_SUCCESS;

	if ( rushResult == BEHAVIOR_SUCCESS )
		{
		_state = GENERAL_COMBAT_MELEE_SELECT_STATE;   
		return;
		}
   
	if ( rushResult == BEHAVIOR_FAILED )
		setupRushEnemyFailed( self );

}



void GeneralCombatWithMeleeWeapon::selectState( Actor &self )
{
	float chance;
	Entity* currentEnemy;
	currentEnemy = self.enemyManager->GetCurrentEnemy();

	if ( !currentEnemy )
		self.enemyManager->FindHighestHateEnemy();

	if ( !currentEnemy )
		{
		if ( !setupHold( self ))
			return;

		_state = GENERAL_COMBAT_MELEE_HOLD;
		return;
		}

	chance = G_Random();
	if ( self.checkincomingmeleeattack() && chance < _blockChance )
		{
		_state = GENERAL_COMBAT_MELEE_BLOCK;

		if ( !setupBlock( self ) )
			setupBlockFailed( self );
		return;
		}

	chance = G_Random();
	if ( currentEnemy && self.enemyManager->InEnemyLineOfFire() && chance < _strafeChance && self.WithinDistance( currentEnemy , _meleeDistance ) )
		{
		_state = GENERAL_COMBAT_MELEE_STRAFE;

		if ( !setupStrafe( self ) )
			strafeFailed(self);

		return;      
		}





	chance = G_Random();
	if ( currentEnemy && chance < _attackChance && self.WithinDistance( currentEnemy , _meleeDistance ) )
		{
		_state = GENERAL_COMBAT_MELEE_ATTACK;

		if (!setupAttack( self ))
			setupAttackFailed( self );

		return;
		}

	setupHold( self );
	_state = GENERAL_COMBAT_MELEE_HOLD;

}



bool GeneralCombatWithMeleeWeapon::setupAttack( Actor &self )
{   
	self.animate->ClearTorsoAnim();
	self.SetAnim( _attackAnim , EV_Actor_NotifyBehavior );
	return true;
}



void GeneralCombatWithMeleeWeapon::setupAttackFailed( Actor &self )
{
	_state = GENERAL_COMBAT_MELEE_SELECT_STATE;   
}



void GeneralCombatWithMeleeWeapon::attack( Actor &self )
{
}



bool GeneralCombatWithMeleeWeapon::setupHold( Actor &self )
{
	self.SetAnim( _primaryTorsoAnim );   
	_exitHoldTime = level.time + G_Random ( 0.15f ) + 0.25f;   

	return true;
}



void GeneralCombatWithMeleeWeapon::hold( Actor &self )
	{
	if ( level.time > _exitHoldTime )
		_state = GENERAL_COMBAT_MELEE_SELECT_STATE;
	}



void GeneralCombatWithMeleeWeapon::setupHoldFailed( Actor &self )
{
	self.SetAnim( _primaryTorsoAnim );
	_state = GENERAL_COMBAT_MELEE_SELECT_STATE;
}



bool GeneralCombatWithMeleeWeapon::setupBlock( Actor &self )
{
	self.SetAnim( "block" );
	_exitBlockTime = level.time + G_Random ( 1.00f ) + 0.25f;  
	return true;
}



void GeneralCombatWithMeleeWeapon::setupBlockFailed( Actor &self )
{
	self.SetAnim( _primaryTorsoAnim );
	_state = GENERAL_COMBAT_MELEE_SELECT_STATE;   
}



void GeneralCombatWithMeleeWeapon::block( Actor &self )
{
	if ( level.time > _exitBlockTime )
		_state = GENERAL_COMBAT_MELEE_SELECT_STATE; 
}



bool GeneralCombatWithMeleeWeapon::setupChangePosture( Actor &self )
{
	[[maybe_unused]] float chance;
	chance = G_Random();

	return true;
}



void GeneralCombatWithMeleeWeapon::changePosture( Actor &self )
{
}



void GeneralCombatWithMeleeWeapon::setupChangePostureFailed( Actor &self )
{
	 self.SetAnim( _primaryTorsoAnim );
	 _state = GENERAL_COMBAT_MELEE_SELECT_STATE;   
}

