//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/puzzleobject.cpp                           $
// $Revision:: 28                                                             $
//     $Date:: 9/26/03 2:36p                                                  $
//
// Copyright (C) 1999 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "glb_local.h"
#include "puzzleobject.hpp"
#include "sentient.h"
#include "player.h"
#include "equipment.h"
#include "gamecmds.h"


Event EV_PuzzleObject_SetOpenDistance
(
	"puzzleobject_opendistance",
	EV_DEFAULT,
	"f",
	"openDistance",
	"Sets the open distance from player that the puzzle object will open or close"
);
Event EV_PuzzleObject_AnimationDone
(
	"puzzleobject_animdone",
	EV_CODEONLY,
	NULL,
	NULL,
	"Called when the puzzle object's animation is done."
);
Event EV_PuzzleObject_SetItemToUse
(
	"puzzleobject_itemtouse",
	EV_DEFAULT,
	"s",
	"item",
	"The item to use on the puzzle"
);
Event EV_PuzzleObject_SetItemUsedThread
(
	"puzzleobject_itemusedthread",
	EV_DEFAULT,
	"s",
	"threadname",
	"The thread to call when the item is used"
);
Event EV_PuzzleObject_SetFailedThread
(
	"puzzleobject_failedthread",
	EV_DEFAULT,
	"s",
	"threadname",
	"The thread to call when the puzzle fails"
);
Event EV_PuzzleObject_SetSolvedThread
(
	"puzzleobject_solvedthread",
	EV_DEFAULT,
	"s",
	"threadname",
	"The thread to call when the puzzle is solved"
);
Event EV_PuzzleObject_SetCanceledThread
(
	"puzzleobject_canceledthread",
	EV_DEFAULT,
	"s",
	"threadname",
	"The thread to call when the puzzle is canceled"
);
Event EV_PuzzleObject_Failed
(
	"puzzleobject_failed",
	EV_DEFAULT,
	NULL,
	NULL,
	"Received when the puzzle fails"
);
Event EV_PuzzleObject_Canceled
(
	"puzzleobject_canceled",
	EV_DEFAULT,
	NULL,
	NULL,
	"Received when the puzzle is canceled"
);
Event EV_PuzzleObject_Solved
(
	"puzzleobject_solved",
	EV_DEFAULT,
	NULL,
	NULL,
	"Received when the puzzle is solved"
);
Event EV_PuzzleObject_Reset
(
	"puzzleobject_reset",
	EV_DEFAULT,
	NULL,
	NULL,
	"Resets a previously solved puzzle so that it can be triggered again"
);
Event EV_PuzzleObject_TimeToUse
(
	"puzzleobject_timeToUse",
	EV_DEFAULT,
	"f",
	"timeToUse",
	"Makes the puzzle object solved after the operator has used it for long enough."
);
Event EV_PuzzleObject_Activate
(
	"puzzleobject_activate",
	EV_DEFAULT,
	NULL,
	NULL,
	"Lets the object think and respond to the player again.  DOES NOT affect any change in animation."
);
Event EV_PuzzleObject_DeActivate
(
	"puzzleobject_deactivate",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the object unresponsive to input and the player.  DOES NOT affect any change in animation."
);
Event EV_PuzzleObject_TimerHudName
(
	"puzzleobject_timerHudName",
	EV_DEFAULT,
	"s",
	"hudName",
	"Sets the hud name to use for the timer hud."
);
Event EV_PuzzleObject_BecomeModBarInSkill
(
	"becomeModBarInSkill",
	EV_DEFAULT,
	"f",
	"skill",
	"Tells a puzzleobject to just display a timed modulation bar in any skill less than or equal to the specified one."
);

CLASS_DECLARATION( Entity, PuzzleObject, "puzzle_object" )
{
	{ &EV_PuzzleObject_SetOpenDistance,		&PuzzleObject::setOpenDistance		},
	{ &EV_PuzzleObject_AnimationDone,		&PuzzleObject::animationDone		},
	{ &EV_PuzzleObject_SetItemToUse,		&PuzzleObject::setItemToUse		},
	{ &EV_PuzzleObject_SetItemUsedThread,	&PuzzleObject::setItemUsedThread	},
	{ &EV_PuzzleObject_SetFailedThread,		&PuzzleObject::setFailedThread		},
	{ &EV_PuzzleObject_SetSolvedThread,		&PuzzleObject::setSolvedThread		},
	{ &EV_PuzzleObject_SetCanceledThread,	&PuzzleObject::setCanceledThread	},
	{ &EV_PuzzleObject_Failed,				&PuzzleObject::failed				},
	{ &EV_PuzzleObject_Canceled,			&PuzzleObject::canceled			},
	{ &EV_PuzzleObject_Solved,				&PuzzleObject::solved				},
	{ &EV_PuzzleObject_Reset,				&PuzzleObject::reset				},
	{ &EV_PuzzleObject_TimeToUse,			&PuzzleObject::setTimeToUse		},
	{ &EV_Use,								&PuzzleObject::useEvent			},
	{ &EV_PuzzleObject_Activate,			&PuzzleObject::activate			},
	{ &EV_PuzzleObject_DeActivate,			&PuzzleObject::deActivate			},
	{ &EV_PuzzleObject_TimerHudName,		&PuzzleObject::setTimerHudName		},
	{ &EV_PuzzleObject_BecomeModBarInSkill, &PuzzleObject::becomeModBarInSkill },

	{NULL, NULL}
};

PuzzleObject::PuzzleObject() 
{
	animate = new Animate( this );

	_saveState = PUZZLE_STATE_IDLE;
	_puzzleState = PUZZLE_STATE_IDLE;
	animate->RandomAnimate( "puzzle_idle" );

	edict->s.eType = ET_MODELANIM;
	_openDistance = 200.0f;
	
	if( model.length() == 0 )
	{
		Vector mins_shadow1( -16, -16, 0 );
		Vector maxs_shadow1( 16, 16, 32 );
		this->setSize( mins_shadow1, maxs_shadow1 );
	}

	_timed = false;
	_timeToUse = 0.0f;
	_hudOn = false;
	_lastTimeUsed = 0.0f;
	_usedTime = 0.0f;

	_nextNeedToUseTime = 0.0f;

	turnThinkOn();

	_hudName = "timerhud";
	_minSkill = -1;
}



PuzzleObject::~PuzzleObject()
{

}


void PuzzleObject::Think( void )
{
	bool playerNearby = false;
	Player *player;
	Vector dir;
	float distance;

	if( (_puzzleState == PUZZLE_STATE_IDLE) || (_puzzleState == PUZZLE_STATE_IDLE_OPEN) )
	{
		player = (Player *)g_entities[ 0 ].entity;
		if ( player )
		{
			dir = player->origin - origin;
			distance = dir.length();

			if ( distance < _openDistance )
			{
				playerNearby = true;
			}
		}

		if( (_puzzleState == PUZZLE_STATE_IDLE) && (playerNearby == true) )
		{
			_puzzleState = PUZZLE_STATE_OPENING;
			animate->RandomAnimate( "puzzle_opening", EV_PuzzleObject_AnimationDone );
		}

		if( (_puzzleState == PUZZLE_STATE_IDLE_OPEN) && (playerNearby == false) )
		{
			_puzzleState = PUZZLE_STATE_CLOSING;
			animate->RandomAnimate("puzzle_closing", EV_PuzzleObject_AnimationDone );
		}
	}

	if ( ( _timed || (level.getSkill() <= _minSkill) ) 
		&& _hudOn && ( _lastTimeUsed + 0.25 < level.time ) )
	{
		_lastTimeUsed = 0.0f;
		_usedTime = 0.0f;

		timedPuzzleCanceled();
	}
}


void PuzzleObject::setOpenDistance(Event* event)
{
	_openDistance = event->GetFloat(1);
}

void PuzzleObject::setItemToUse(Event* event)
{
	_itemToUse = event->GetString(1);
}
void PuzzleObject::setItemUsedThread(Event* event)
{
	_itemUsedThread = event->GetString(1);
}


void PuzzleObject::setFailedThread(Event* event)
{
	_failedThread = event->GetString(1);
}


void PuzzleObject::setSolvedThread(Event* event)
{
	_solvedThread = event->GetString(1);
}


void PuzzleObject::setCanceledThread(Event* event)
{
	_canceledThread = event->GetString(1);
}


void PuzzleObject::failed(Event* event)
{
	_puzzleState = PUZZLE_STATE_CLOSING_LOCKED;
	animate->RandomAnimate( "puzzle_closing", EV_PuzzleObject_AnimationDone );
	
	if(_failedThread.length() != 0)
	{
		ExecuteThread(_failedThread, true, this);
	}
	else
	{
		PostEvent( EV_PuzzleObject_Reset, 1 );
	}

}


void PuzzleObject::canceled(Event* event)
{
	_puzzleState = PUZZLE_STATE_CLOSING;
	animate->RandomAnimate( "puzzle_closing", EV_PuzzleObject_AnimationDone );

	if(_canceledThread.length() != 0)
	{
		ExecuteThread( _canceledThread, true, this);
	}
	else
	{
		PostEvent( EV_PuzzleObject_Reset, 1 );
	}

}


void PuzzleObject::solved(Event* event)
{
	_puzzleState = PUZZLE_STATE_CLOSING_SOLVED;
	animate->RandomAnimate( "puzzle_closing", EV_PuzzleObject_AnimationDone );

	if(_solvedThread.length() != 0)
	{
		ExecuteThread(_solvedThread, true, this);
	}

}


void PuzzleObject::animationDone(Event* event)
{
	if( _puzzleState == PUZZLE_STATE_OPENING )
	{
		_puzzleState = PUZZLE_STATE_IDLE_OPEN;
		animate->RandomAnimate( "puzzle_waitingopen" );
	}
	else if( _puzzleState == PUZZLE_STATE_CLOSING )
	{
		_puzzleState = PUZZLE_STATE_IDLE;
		animate->RandomAnimate( "puzzle_idle" );
	}
	else if( _puzzleState == PUZZLE_STATE_CLOSING_SOLVED )
	{
		_puzzleState = PUZZLE_STATE_IDLE_SOLVED;
		animate->RandomAnimate( "puzzle_idle" );
	}
	else if( _puzzleState == PUZZLE_STATE_CLOSING_LOCKED )
	{
		_puzzleState = PUZZLE_STATE_IDLE_LOCKED;
		animate->RandomAnimate( "puzzle_idle" );
	}
}


void PuzzleObject::useEvent(Event* event)
{
	Entity* entity;

	if ( event->NumArgs() > 0 )
		entity = event->GetEntity( 1 );
	else
		entity = NULL;


	if(	(_puzzleState != PUZZLE_STATE_IDLE_OPEN) )
		return;


	if ( !entity )
		return;

	if ( _itemToUse.length() > 0 )
	{
		Equipment *equipment;


		if ( !entity->isSubclassOf( Equipment ) )
		{
			if ( _nextNeedToUseTime < level.time )
			{
				gi.centerprintf ( entity->edict, CENTERPRINT_IMPORTANCE_NORMAL, "$$NeedToUse$$ %s", _itemToUse.c_str() );

				if ( entity->isSubclassOf( Player ) )
				{
					Player *player = (Player *)entity;

					player->loadUseItem( _itemToUse );
				}

				_nextNeedToUseTime = level.time + 1.0f;
			}

			return;
		}

		equipment = (Equipment*)entity;


		if ( stricmp( equipment->getTypeName().c_str(), _itemToUse.c_str() ) != 0 )
			return;
	}
	else
	{

		if ( !entity->isSubclassOf( Player ) )
		{

			return;
		}
	}

	if ( _timed || (level.getSkill() <= _minSkill) )
		timedUse();
	else
		normalUse();
}

void PuzzleObject::normalUse( void )
{
	if ( !_itemUsedThread.length() )
		return;

	ExecuteThread( _itemUsedThread, true, this );

	_puzzleState = PUZZLE_STATE_ACTIVE_OPEN;
	animate->RandomAnimate( "puzzle_openon" );
}

void PuzzleObject::timedUse( void )
{
	float percent;
	Player *player;
	Event *event;


	if ( _lastTimeUsed >= level.time )
		return;

	player = ( Player * )g_entities[ 0 ].entity;


	if ( !_hudOn )
	{
		showTimerHud();
	}

	_lastTimeUsed = level.time;
	_usedTime += level.frametime;

	percent = _usedTime / _timeToUse * 100.0f;

	event = new Event( EV_Player_SetStat );
	event->AddString( "generic" );
	event->AddInteger( (int)percent );
	player->ProcessEvent( event );

	if ( percent >= 100.0f )
	{
		timedPuzzleSolved();
	}
}

void PuzzleObject::timedPuzzleSolved( void )
{
	hideTimerHud();
	ProcessEvent( EV_PuzzleObject_Solved );
}

void PuzzleObject::timedPuzzleCanceled( void )
{
	hideTimerHud();

	if ( _canceledThread.length() )
	{
		ProcessEvent( EV_PuzzleObject_Canceled );
	}
}

void PuzzleObject::showTimerHud( void )
{
	Player *player;
	str commandString;


	player = ( Player * )g_entities[ 0 ].entity;

	commandString = "pushmenu ";
	commandString += _hudName;
	commandString += "\n";

	G_SendCommandToPlayer( player->edict, commandString.c_str() );

	_hudOn = true;
}

void PuzzleObject::hideTimerHud( void )
{
	Player *player;
	str commandString;

	player = ( Player * )g_entities[ 0 ].entity;


	commandString = "popmenu ";
	commandString += _hudName;
	commandString += " 1\n";

	G_SendCommandToPlayer( player->edict, commandString.c_str() );

	_hudOn = false;
}

void PuzzleObject::reset( Event* event )
{
	_puzzleState = PUZZLE_STATE_IDLE;
	animate->RandomAnimate( "puzzle_idle" );

	_lastTimeUsed = 0.0f;
	_usedTime = 0.0f;
}

void PuzzleObject::activate( Event* event )
{
	_puzzleState = _saveState;
}

void PuzzleObject::deActivate( Event* event )
{
	_saveState = _puzzleState;
	_puzzleState = PUZZLE_STATE_DEACTIVATED;
}

void PuzzleObject::setTimeToUse( Event * ev )
{
	_timeToUse = ev->GetFloat( 1 );

	if ( _timeToUse > 0.0f )
		_timed = true;
}

void PuzzleObject::setTimerHudName( Event * ev )
{
	_hudName = ev->GetString( 1 );
}

void PuzzleObject::becomeModBarInSkill( Event* ev )
{
	_minSkill = (int) ev->GetFloat( 1 );
	if( _timeToUse == 0.0f )
	{
		_timeToUse = 2.0f;
	}
}

