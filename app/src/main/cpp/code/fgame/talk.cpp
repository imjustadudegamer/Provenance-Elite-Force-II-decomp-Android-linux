
#include "actor.h"
#include "talk.hpp"
#include "actor_headwatcher.h"
#include "actor_enemymanager.h"

extern Event EV_Actor_NotifyBehavior;
extern Event EV_SimplePlayDialog;

Event EV_TalkBehavior_GreetingDone (	"greetingdone",
	EV_DEFAULT,
	NULL,
	NULL,
	"Notifies the talk behavior the greeting dialog is done"
);

CLASS_DECLARATION( Behavior, Talk, NULL )
{
	{ &EV_TalkBehavior_GreetingDone, &Talk::GreetingDone	},
	{ &EV_Behavior_AnimDone,		 &Talk::AnimDone		},
	{ NULL, NULL }
};

void Talk::SetUser
(
	Sentient *user
)

{
	ent_listening = user;
}

void Talk::AnimDone
(
	Event *ev
)

{
	turnto.ProcessEvent( EV_Behavior_AnimDone );
	animDone = true;
}

void Talk::GreetingDone( Event *ev )
{
	mode = TALK_MODE_TURN_TO;
}

void Talk::Begin
(
	Actor &self
)

{
	Vector dir;
	Vector angles;
	[[maybe_unused]] const char *anim_name;
	oldAnimName = "";

	anim_name = self.animname;
	last_headwatch_target = self.headWatcher->GetWatchTarget();
	animDone = true;
	move_allowed = true;
	turnto.SetUseTurnAnim( false );

	if ( self.talkMode == TALK_IGNORE )
	{
		move_allowed = false;
	}

	if ( self.talkMode == TALK_HEADWATCH )
	{
		move_allowed = false;
		oldAnimName = self.animname;
	}

	if ( self.talkMode != TALK_IGNORE )
	{
		if ( ent_listening )
			self.headWatcher->SetWatchTarget( ent_listening );
	}

	mode = TALK_MODE_PLAY_GREETING;

	Entity *currentEnemy = NULL;
	currentEnemy = self.enemyManager->GetCurrentEnemy();

	if ( currentEnemy )
		mode = TALK_MODE_COMBAT;

	original_yaw = self.angles[YAW];

	if ( ent_listening )
	{
		dir    = ent_listening->centroid - self.centroid;
		angles = dir.toAngles();
		yaw    = angles[YAW];
	}
}

BehaviorReturnCode_t Talk::Evaluate
(
	Actor &self
)

{
	Vector dir;
	Vector angles;
	str greetingDialog;
	str combatDialog;
	float greetingDialogLength;
	Event *greetingEvent;
	char localizedDialogName[MAX_QPATH];

	if ( !ent_listening )
		mode = TALK_MODE_TURN_BACK;

	if ( !self.GetActorFlag(ACTOR_FLAG_PLAYING_DIALOG_ANIM) && animDone && self.GetActorFlag(ACTOR_FLAG_DIALOG_PLAYING) )
	{
		if ( self.useConvAnims )
		{
			self.SetAnim( str("talk"), EV_Actor_NotifyBehavior );
			self.SetActorFlag( ACTOR_FLAG_PLAYING_DIALOG_ANIM , true );
			self.SetActorFlag(ACTOR_FLAG_CAN_CHANGE_ANIM , false );
			animDone = false;
		}
	}

	if ( self.useConvAnims && animDone && mode != TALK_MODE_WAIT && mode != TALK_MODE_TURN_TO )
	{
		self.SetActorFlag(ACTOR_FLAG_CAN_CHANGE_ANIM , true);
		self.SetAnim( str("conv-idle"), EV_Actor_NotifyBehavior );
		animDone = false;
	}

	if ( self.useConvAnims && mode == TALK_MODE_WAIT )
	{
		self.SetActorFlag(ACTOR_FLAG_CAN_CHANGE_ANIM , true);
		if ( oldAnimName.length() )
			self.SetAnim( oldAnimName, EV_Actor_NotifyBehavior );
		else
			self.SetAnim( str("idle"), EV_Actor_NotifyBehavior );
		animDone = false;
	}

	if ( !self.GetActorFlag(ACTOR_FLAG_PLAYING_DIALOG_ANIM) && !self.GetActorFlag(ACTOR_FLAG_DIALOG_PLAYING) && ( ( !self.talkMode ) == TALK_HEADWATCH ) )
	{
		self.SetAnim( str("idle"), EV_Actor_NotifyBehavior );
		animDone = true;
	}

	if ( self.GetActorFlag(ACTOR_FLAG_PLAYING_DIALOG_ANIM) && animDone )
	{
		if ( oldAnimName.length() )
		{
			self.SetActorFlag(ACTOR_FLAG_CAN_CHANGE_ANIM , true );
			self.SetAnim( oldAnimName, EV_Actor_NotifyBehavior );
			self.SetActorFlag(ACTOR_FLAG_CAN_CHANGE_ANIM , false );
			if ( mode != TALK_MODE_TURN_BACK )
				mode = TALK_MODE_WAIT;
		}
	}

	switch( mode )
	{
	case TALK_MODE_COMBAT:
		combatDialog = self.FindDialog( ent_listening, DIALOG_TYPE_COMBAT );
		if ( !combatDialog.length() )
			return BEHAVIOR_SUCCESS;

		self.PlayDialog( ent_listening, -1.0f, -1.0f, combatDialog.c_str() );  
		return BEHAVIOR_SUCCESS;
		break;

	case TALK_MODE_PLAY_GREETING:
		greetingDialog = self.FindDialog( ent_listening, DIALOG_TYPE_GREETING );
		if ( !greetingDialog.length() )
		{
			mode = TALK_MODE_TURN_TO;
			return BEHAVIOR_EVALUATING;
		}

		gi.LocalizeFilePath( greetingDialog.c_str(), localizedDialogName );
		greetingDialogLength = gi.SoundLength( localizedDialogName );

		if ( greetingDialogLength > 0 )
		{
			greetingEvent = new Event(EV_TalkBehavior_GreetingDone);
			PostEvent(greetingEvent , greetingDialogLength );

			Event *dialogEvent = new Event( EV_SimplePlayDialog );
			dialogEvent->AddString( greetingDialog.c_str() );
			ent_listening->ProcessEvent( dialogEvent );

			mode = TALK_MODE_WAIT_FOR_GREETING;
		}
		else
		{
			mode = TALK_MODE_TURN_TO;
		}
		break;

	case TALK_MODE_WAIT_FOR_GREETING:
		break;

	case TALK_MODE_TURN_TO :
		if ( move_allowed )
		{
			turnto.SetDirection( yaw );

			if ( !turnto.Evaluate( self ) )
			{
				mode = TALK_MODE_TALK;
				self.PlayDialog( ent_listening );  
			}
		}
		else
		{
			mode = TALK_MODE_TALK;
			self.PlayDialog( ent_listening );  
		}
		break;

	case TALK_MODE_TALK :
		if ( move_allowed )
		{
			dir    = ent_listening->centroid - self.centroid;
			angles = dir.toAngles();
			turnto.SetDirection( angles[YAW] );
			turnto.Evaluate( self );
		}

		if ( !self.GetActorFlag( ACTOR_FLAG_DIALOG_PLAYING ) )
		{
			mode = TALK_MODE_WAIT;
			self.state_flags &= ~STATE_FLAG_USED;
		}
		else if ( !self.GetActorFlag( ACTOR_FLAG_RADIUS_DIALOG_PLAYING ) )
		{
			if ( !self.WithinDistance( ent_listening, self.radiusDialogRange  ) )
			{
				self.PlayRadiusDialog( ent_listening );
			}
		}
		break;

	case TALK_MODE_WAIT :
		if ( move_allowed )
		{
			dir    = ent_listening->centroid - self.centroid;
			angles = dir.toAngles();
			turnto.SetDirection( angles[YAW] );
			turnto.Evaluate( self );
		}

		if ( !self.WithinDistance( ent_listening, 100.0f ) )
			mode = TALK_MODE_TURN_BACK;

		if ( self.state_flags & STATE_FLAG_USED )
		{
			mode = TALK_MODE_TURN_TO;
			self.SetActorFlag(ACTOR_FLAG_PLAYING_DIALOG_ANIM, false );

			dir    = ent_listening->centroid - self.centroid;
			angles = dir.toAngles();
			yaw    = angles[YAW];

			self.state_flags &= ~STATE_FLAG_USED;
		}
		break;

	case TALK_MODE_TURN_BACK :
		if ( move_allowed )
		{
			turnto.SetDirection( original_yaw );

			if ( !turnto.Evaluate( self ) )
				return BEHAVIOR_SUCCESS;
		}
		else
		{
			return BEHAVIOR_SUCCESS;
		}
		break;
	}

	return BEHAVIOR_EVALUATING;
}

void Talk::End( Actor &self )
{
	self.SetActorFlag( ACTOR_FLAG_PLAYING_DIALOG_ANIM , false );
	self.SetActorFlag(ACTOR_FLAG_CAN_CHANGE_ANIM , true );

	self.ClearLegAnim();
	self.ClearTorsoAnim();

	if ( oldAnimName.length() )
	{
		self.SetAnim( oldAnimName, EV_Actor_NotifyBehavior );
	}
	else
	{
		self.SetAnim( str("idle"), EV_Actor_NotifyBehavior );
	}

	if ( last_headwatch_target )
		self.headWatcher->SetWatchTarget( last_headwatch_target );
	else
		self.headWatcher->SetWatchTarget( NULL );
}
