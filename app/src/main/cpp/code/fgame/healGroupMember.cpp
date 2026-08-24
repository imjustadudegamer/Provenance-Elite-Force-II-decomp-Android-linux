
#include "actor.h"
#include "groupcoordinator.h" 
#include "healGroupMember.hpp"

extern Event EV_Sentient_HealOverTime;
CLASS_DECLARATION( Behavior, HealGroupMember, NULL )
	{
		{ &EV_Behavior_Args,		&HealGroupMember::SetArgs	},
		{ &EV_Behavior_AnimDone,	&HealGroupMember::AnimDone	}, 
		{ NULL, NULL }
	};


HealGroupMember::HealGroupMember()
{
	_nextTriageUpdate = 0.0f;
}

HealGroupMember::~HealGroupMember()
{
	triageEntry_t* checkEntry;

	for ( int i = _triageList.NumObjects() ; i > 0 ; i-- )
		{
		checkEntry = _triageList.ObjectAt( i );
		delete checkEntry;
		checkEntry = NULL;
		_triageList.RemoveObjectAt( i );
		}
}

void HealGroupMember::SetArgs( Event *ev )
{
	_anim					= ev->GetString( 1 );	
	_healDistance			= ev->GetFloat ( 2 );
	_maxDistance			= ev->GetFloat ( 3 );
	_initialHealPercentage	= ev->GetFloat ( 4 );
	_regenHealPercentage	= ev->GetFloat ( 5 );
	_regenInterval			= ev->GetFloat ( 6 );
	_maxPercentage			= ev->GetFloat ( 7 );
}



void HealGroupMember::AnimDone( Event *ev )
{
}



void HealGroupMember::Begin( Actor &self )
{       
	init( self );
}



void HealGroupMember::init( Actor &self )
{	
	updateTriageList( self );
	_currentPatient = findHighestPriorityPatient( self );
	setupGotoEntity( self );
	_state = HGM_STATE_GOTO_ENTITY;
	updateTriageList( self );
	
}



BehaviorReturnCode_t HealGroupMember::Evaluate( Actor &self )
{  
	Event *injuredEvent;

	if ( !_currentPatient )
		{
		SetFailureReason( "No Group Member To Heal" );
		_state = HGM_STATE_FAILED;
		}

	switch ( _state )
		{
		case HGM_STATE_GOTO_ENTITY:
			doGotoEntity( self );
		break;

		case HGM_STATE_FACE_TARGET:
			doRotateToEntity( self );
		break;

		case HGM_STATE_ANIMATE:
			doAnimate( self );
		break;

		case HGM_STATE_HEAL:
			doHeal( self );
		break;

		case HGM_STATE_FAILED:
			return BEHAVIOR_FAILED;
		break;

		case HGM_STATE_SUCCESS:
			if ( allPatientsTreated( self ) )
				{
				injuredEvent = new Event ( EV_Sentient_GroupMemberInjured );
				injuredEvent ->AddInteger( 0 );
				groupcoordinator->SendEventToGroup( injuredEvent , self.GetGroupID() );
				return BEHAVIOR_SUCCESS;
				}				
			else
				{
				init( self );
				}
		break;

		}

	return BEHAVIOR_EVALUATING;   
}



void HealGroupMember::End(Actor &self)
{   
}

void HealGroupMember::setupGotoEntity( Actor &self )
{
	_gotoEntity.SetEntity	( self, _currentPatient );
	_gotoEntity.SetDistance ( _healDistance );
	_gotoEntity.SetAnim		( "run"			);
	_gotoEntity.Begin		( self			);
	
}

void HealGroupMember::doGotoEntity( Actor &self )
{
	BehaviorReturnCode_t result;
	result = _gotoEntity.Evaluate( self );
	
	if ( level.time > _nextTriageUpdate )
		{
		EntityPtr lastPatient;
		lastPatient = _currentPatient;

		updateTriageList( self );
		_currentPatient = findHighestPriorityPatient( self );

		if ( _currentPatient != lastPatient )			
			setupGotoEntity( self );
						
		}

	if ( self.WithinDistance( _currentPatient , _healDistance ) || result == BEHAVIOR_SUCCESS )
		{
		_gotoEntity.End( self );
		_state = HGM_STATE_FACE_TARGET;
		setupRotateToEntity( self );		
		return;
		}

	if ( result != BEHAVIOR_EVALUATING )
		{		
		gotoEntityFailed( self );
		}
}

void HealGroupMember::gotoEntityFailed( Actor &self )
{
	str FailureReason;
	FailureReason = "GotoEntity Component Failed:  " + _gotoEntity.GetFailureReason() + "\n";	
	SetFailureReason( FailureReason );

	_gotoEntity.End( self );
	_state = HGM_STATE_FAILED;
}

void HealGroupMember::doHeal( Actor &self )
{
	Event *regenEvent;

	regenEvent = new Event ( EV_Sentient_HealOverTime );
	
	if ( !regenEvent)
		{
		assert(regenEvent);
		return;
		}

	regenEvent->AddFloat( _initialHealPercentage	);
	regenEvent->AddFloat( _regenHealPercentage		);
	regenEvent->AddFloat( _regenInterval			);
	regenEvent->AddFloat( _maxPercentage			);

	_currentPatient->ProcessEvent( regenEvent );
	treatedPatient( self, _currentPatient );

	_state = HGM_STATE_SUCCESS;

}

void HealGroupMember::setupRotateToEntity( Actor &self )
{	
	_rotateToEntity.SetEntity( _currentPatient );
	_rotateToEntity.Begin( self );
}

void HealGroupMember::doRotateToEntity( Actor &self )
{
	BehaviorReturnCode_t result;
	result = _rotateToEntity.Evaluate( self );

	if ( result == BEHAVIOR_SUCCESS )
		{
		_rotateToEntity.End( self );
		_state = HGM_STATE_ANIMATE;
		setupAnimate( self );
		return;
		}
	
	if ( result != BEHAVIOR_EVALUATING )
		{
		rotateToEntityFailed( self );
		}

}

void HealGroupMember::rotateToEntityFailed( Actor &self )
{
	str FailureReason;
	FailureReason = "RotateToEntity Component Failed:  " + _rotateToEntity.GetFailureReason() + "\n";	
	SetFailureReason( FailureReason );

	_rotateToEntity.End( self );
	_state = HGM_STATE_FAILED;
}

void HealGroupMember::setupAnimate( Actor &self )
{

}

void HealGroupMember::doAnimate( Actor &self )
{
	self.SetAnim( _legAnim );
	self.SetAnim( _anim , NULL , torso );

	_state = HGM_STATE_HEAL;
	
}

void HealGroupMember::animateFailed( Actor &self )
{
	str FailureReason;
	FailureReason = "Animation Not Found\n";
	SetFailureReason ( FailureReason );

	_state = HGM_STATE_FAILED;
}

void HealGroupMember::treatedPatient( Actor &self , Entity *ent )
{
	triageEntry_t* checkEntry;

	for ( int i = _triageList.NumObjects() ; i > 0 ; i-- )
		{	
		checkEntry = _triageList.ObjectAt( i );
		if ( checkEntry->ent == ent )
			{
			checkEntry->treated = true;
			}
		}	
}

Actor* HealGroupMember::findHighestPriorityPatient( Actor &self )
{
	triageEntry_t* checkEntry;
	Actor *highestPriorityPatient;

	float lowestHealth;

	lowestHealth = 9999999.9f;
	highestPriorityPatient = 0;

	for ( int i = _triageList.NumObjects() ; i > 0 ; i-- )
		{	
		checkEntry = _triageList.ObjectAt( i );
		if ( checkEntry->health < lowestHealth && checkEntry->dist <= _maxDistance )
			{
			lowestHealth = checkEntry->health;
			highestPriorityPatient = (Actor*)(Entity*)checkEntry->ent;
			}
		}

	return highestPriorityPatient;
}

bool HealGroupMember::allPatientsTreated( Actor &self )
{
	triageEntry_t* checkEntry;

	for ( int i = _triageList.NumObjects() ; i > 0 ; i-- )
		{	
		checkEntry = _triageList.ObjectAt( i );
		if (!checkEntry->treated )
				return false;		
		}

	return true;
}

bool HealGroupMember::patientTreated( Actor &self , Entity *ent )
{
	triageEntry_t* checkEntry;

	for ( int i = _triageList.NumObjects() ; i > 0 ; i-- )
		{	
		checkEntry = _triageList.ObjectAt( i );
		if ( checkEntry->ent == ent )
			{
			return checkEntry->treated;
			}
		}

	return false;
}

void HealGroupMember::updateTriageList( Actor &self )
{
	Group *group;
	Entity *groupEnt;
	float health , maxHealth;

	groupEnt = NULL;

	group = groupcoordinator->GetGroup( self.GetGroupID() );	

	groupEnt = group->GetNextMember( groupEnt );

	while ( groupEnt != NULL )
		{
		health = groupEnt->getHealth();
		maxHealth = groupEnt->getMaxHealth();
		
		if ( ( health / maxHealth ) < .5 )
			updateTriageEntry( self , groupEnt );		
			
		groupEnt = group->GetNextMember( groupEnt );
		}

	_nextTriageUpdate = level.time + G_Random() + 2.5f;
}

void HealGroupMember::updateTriageEntry( Actor &self , Entity *ent)
{
	float entHealth;
	float entDist;
	triageEntry_t* checkEntry;

	entHealth = ent->getHealth();
	entDist = Vector::Distance( self.origin , ent->origin );

	for ( int i = _triageList.NumObjects() ; i > 0 ; i-- )
		{	
		checkEntry = _triageList.ObjectAt( i );
		if ( checkEntry->ent == ent )
			{
			checkEntry->health = entHealth;
			checkEntry->dist = entDist;
			return;
			}
		}

	checkEntry = new triageEntry_t;
	checkEntry->ent = ent;
	checkEntry->health = entHealth;
	checkEntry->dist = entDist;
	checkEntry->treated = false;

	_triageList.AddObject(checkEntry);
}
