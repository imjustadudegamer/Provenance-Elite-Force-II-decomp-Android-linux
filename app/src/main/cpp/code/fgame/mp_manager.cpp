//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/mp_manager.cpp                                $
// $Revision:: 143                                                            $
//   $Author:: Singlis                                                        $
//     $Date:: 9/26/03 3:53p                                                  $
//
// Copyright (C) 2002 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// DESCRIPTION:
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "_pch_cpp.h"
#include "scriptthread.h"

#include "player.h"
#include "mp_manager.hpp"
#include "mp_modeDm.hpp"
#include "mp_modeTeamDm.hpp"
#include "mp_modeCtf.hpp"
#include "powerups.h"
#include "../corepp/tiki.h" 

#define PROBE_GET(name, def, flags) gi.cvar(name, def, flags)
#define PROBE_PRINT                 gi.Printf
#include "../qcommon/qa_probe.h"


extern int gametype;


MultiplayerManager multiplayerManager;

str playersLastTeam[ MAX_CLIENTS ];


const int	MultiplayerManager::_playerFreezeTime = 100;
const float	MultiplayerManager::_maxVoteTime = 30.0f;
const int	MultiplayerManager::_maxSayStringLength = 150;
const float	MultiplayerManager::_inBetweenMatchTime = 3.0f;

const int	MultiplayerPlayerData::_maxDialogs = 16;

MultiplayerPlayerData::MultiplayerPlayerData()
{
	reset();

	_dialogData = new MultiplayerDialogData[ _maxDialogs ];
}

MultiplayerPlayerData::~MultiplayerPlayerData()
{
	delete [] _dialogData;
}

void MultiplayerPlayerData::reset( void )
{
	_votecount = 0;
	_voted = false;
	_spectator = false;
	_spectatorByChoice = false;
	_valid = false;
	_waitingForRespawn = false;
	_teamHud = "";
	_named = false;

	_nextDialogSendSpot = 0;
	_nextDialogAddSpot = 0;
	_nextDialogSendTime = 0.0f;
	_nextTauntTime = 0.0f;
}

CLASS_DECLARATION( Class, MultiplayerManager, NULL )
{
	{ NULL, NULL }
};

MultiplayerManager::MultiplayerManager()
{
	_inMultiplayerGame = false;

	_multiplayerGame = NULL;

	_playerData = NULL;

	_awardSystem = NULL;

	_gameStarted = false;
	_gameOver    = false;

	_needToAddBots = true;

	_inMatch = false;

	_talkingIconIndex = 0;
	_waitingToRespawnIconIndex = 0;

	_restartMatchTime = 0.0f;

	_voteTime = 0.0f;
}

MultiplayerManager::~MultiplayerManager()
{
	cleanup( false );
}


void MultiplayerManager::cleanup( qboolean restart )
{
	_inMultiplayerGame = false;

	_gameStarted = false;
	_gameOver    = false;

	if ( !restart )
	{
		_needToAddBots = true;
	}


	delete _multiplayerGame;
	_multiplayerGame = NULL;


	delete [] _playerData;
	_playerData = NULL;


	delete _awardSystem;
	_awardSystem = NULL;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		delete modifier;
	}

	_modifiers.FreeObjectList();

	_voteTime = 0.0f;
}

void MultiplayerManager::init( void )
{
	_inMultiplayerGame = false;

	resetRespawnTime();

	checkModifiedCvars( false );
}

void MultiplayerManager::start( void )
{
	if ( !_inMultiplayerGame )
		return;

	_gameStarted = true;
	_gameOver    = false;

	_talkingIconIndex = gi.imageindex( "sysimg/icons/mp/talking" );
	_waitingToRespawnIconIndex = gi.imageindex( "sysimg/icons/mp/elimination_eliminated" );


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
			modifier->start();
	}


	_multiplayerGame->start();
}

void MultiplayerManager::update( float frameTime )
{
	int i;


	if ( !_inMultiplayerGame )
		return;


	if ( !_inMatch && !_gameOver && ( _restartMatchTime > 0.0f ) && ( _restartMatchTime < getTime() ) )
	{
		restartMatch();
	}


	if ( mp_respawnTime->modified )
	{
		if ( mp_respawnTime->value >= 0.0f )
		{
			setRespawnTime( mp_respawnTime->value );
		}

		mp_respawnTime->modified = false;
	}


	if ( !_gameStarted )
		start();

	if ( _needToAddBots )
		addBots();


	if ( _multiplayerGame->inMatch() )
	{
		for ( i = 1 ; i <= _modifiers.NumObjects() ; i++ )
		{
			MultiplayerModifier *modifier;

			modifier = _modifiers.ObjectAt( i );

			if ( modifier )
				modifier->update( frameTime );
		}
	}


	_multiplayerGame->update( frameTime );


	checkVote();


	checkModifiedCvars( true );


	if ( strlen( password->string ) )
	{
		if ( !g_needpass->integer )
		{
			gi.cvar_set( "g_needpass", "1" );
		}
	}
	else
	{
		if ( g_needpass->integer )
		{
			gi.cvar_set( "g_needpass", "0" );
		}
	}


	for ( i = 0 ; i < maxclients->integer ; i++ )
	{
		Player *player;


		player = getPlayer( i );

		if ( !player )
			continue;

		sendNextPlayerSound( player );
	}


	if ( level.intermissiontime )
	{
		G_CheckIntermissionExit();

		if ( !_declaredWinner && ( _declareWinnerTime < getTime() ) )
		{
			_multiplayerGame->declareWinner();

			_declaredWinner = true;
		}
		return;
	}


	if ( _multiplayerGame->isEndOfMatch() )
	{
		_gameOver = true;

		centerPrintAllClients( "$$MatchOver$$\n", CENTERPRINT_IMPORTANCE_NORMAL );
		multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_matover.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.2f );

		_multiplayerGame->EndMatch();
		G_BeginIntermission2();
		matchOver();

		_declareWinnerTime = getTime() + 2.0f;
		_declaredWinner = false;

		setNextMap();
	}


	for ( i = 0 ; i < maxclients->integer ; i++ )
	{
		Player *player;
		Player *playerFollowing;


		player = getPlayer( i );

		if ( !player )
			continue;

		if ( isPlayerSpectator( player, SPECTATOR_TYPE_FOLLOW ) )
		{
			playerFollowing = getPlayer( _playerData[ player->entnum ]._spectatorPlayerNum );

			if ( !playerFollowing || playerFollowing->deadflag == DEAD_DEAD )
			{
				makePlayerSpectateNextPlayer( player );

				if ( !isPlayerSpectator( player, SPECTATOR_TYPE_FOLLOW ) )
					continue;

				playerFollowing = getPlayer( _playerData[ player->entnum ]._spectatorPlayerNum );
			}

			if ( playerFollowing )
			{
				player->setOrigin( playerFollowing->origin );
				player->setAngles( playerFollowing->angles );
				player->SetViewAngles( playerFollowing->getViewAngles() );
			}
		}

		if ( _playerData[ player->entnum ]._waitingForRespawn )
		{
			int lastTimeLeft;
			int timeLeft;
			str printString;

			timeLeft = (int)( _playerData[ player->entnum ]._respawnTime - getTime() + 1.0f + ( frameTime / 2.0f ) );
			lastTimeLeft = (int)( _playerData[ player->entnum ]._respawnTime - getTime() + frameTime + 1.0f + ( frameTime / 2.0f ) );

			if ( ( lastTimeLeft != timeLeft ) && ( timeLeft > 0 ) )
			{
				printString = "$$RespawnIn$$ ";
				printString += timeLeft;

				centerPrint( player->entnum, printString, CENTERPRINT_IMPORTANCE_HIGH );
			}

			if ( _playerData[ player->entnum ]._respawnTime <= getTime() )
			{
				respawnPlayer( player, true );
			}
		}
	}
}

void MultiplayerManager::addBots( void )
{
	cvar_t *mp_botcommands;


	if ( !dedicated->integer && ( !g_entities[ 0 ].inuse || !g_entities[ 0 ].client || !g_entities[ 0 ].entity ) )
	{
		PROBE(
			"npc:addbots-gate",
			"waiting: dedicated=%d inuse=%d client=%d entity=%p",
			dedicated->integer,
			g_entities[ 0 ].inuse,
			g_entities[ 0 ].client ? 1 : 0,
			g_entities[ 0 ].entity
		);
		return;
	}


	mp_botcommands = gi.cvar( "mp_botcommands", "", 0 );

	PROBE("npc:addbots", "mp_botcommands='%s' len=%d", mp_botcommands->string, (int)strlen(mp_botcommands->string));

	if ( strlen( mp_botcommands->string ) > 0 )
	{
		gi.SendConsoleCommand( mp_botcommands->string );
	}

	_needToAddBots = false;
}

extern int gametype; 
void MultiplayerManager::initMultiplayerGame( void )
{
	cvar_t *mp_modifier_Destruction;
	int i;

	PROBE(
		"npc:initmp",
		"mp_gametype=%d mp_botcommands-present=%d warmUpTime=%g minPlayers=%d",
		mp_gametype->integer,
		gi.cvar("mp_botcommands", "", 0) ? (int)strlen(gi.cvar("mp_botcommands", "", 0)->string) : -1,
		gi.cvar("mp_warmUpTime", "10", 0)->value,
		gi.cvar("mp_minPlayers", "0", 0)->integer
	);


	gi.cvar_set( "mp_modifier_diffusion", "0" );


	switch ( mp_gametype->integer ) 
	{
		case 0 : 
			_multiplayerGame = new ModeDeathmatch(); 
			gametype = GT_FFA;
			break;
		case 1 :  
			_multiplayerGame = new ModeTeamDeathmatch(); 
			gametype = GT_TEAM;
			break;
		case 2 :  
			_multiplayerGame = new ModeCaptureTheFlag(); 
			gametype = GT_CTF;
			break ;
		case 3 :  

			_multiplayerGame = new ModeTeamDeathmatch(); 

			gi.cvar_set( "mp_modifier_diffusion", "1" );


			gametype = GT_TEAM;
			break ;

		default: 
			_multiplayerGame = new ModeDeathmatch(); 
			gametype = GT_FFA;
			break ;
	}


	mp_modifier_Destruction = gi.cvar( "mp_modifier_Destruction", "0", 0 );

	if ( mp_modifier_Destruction->integer )
	{
		gametype = GT_OBELISK;
	}

	if ( !_multiplayerGame )
		return;

	


	_playerData = new MultiplayerPlayerData[ EF2_AllocCount( maxclients->integer ) ];


	_multiplayerGame->init( maxclients->integer );

	_multiplayerGame->setPointLimit( mp_pointlimit->integer );
	_multiplayerGame->setTimeLimit( mp_timelimit->value * 60.0f );

	_inMultiplayerGame = true;


	addModifiers();


	for ( i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
			modifier->init( maxclients->integer );
	}


	_awardSystem = new AwardSystem;
	_awardSystem->init( maxclients->integer );



	Event *event = new Event( "addAvailableViewMode" );
	event->AddString( "BogusMode" );
	world->ProcessEvent( event );
}

void MultiplayerManager::initItems( void )
{
	int i;
	int j;
	bool shouldKeep;
	MultiplayerItem *item;
	Item *normalItem;

	if ( !_inMultiplayerGame )
		return;


	_multiplayerGame->initItems();


	for ( i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
			modifier->initItems();
	}


	_awardSystem->initItems();



	for ( i = 0; i < MAX_GENTITIES; i++ )
	{
		if ( g_entities[ i ].inuse && g_entities[ i ].entity )
		{
			if ( g_entities[ i ].entity->isSubclassOf( MultiplayerItem ) )
			{
				item = (MultiplayerItem *)g_entities[ i ].entity;

				shouldKeep = false;


				for ( j = 1 ; j <= _modifiers.NumObjects() ; j++ )
				{
					MultiplayerModifier *modifier;

					modifier = _modifiers.ObjectAt( j );

					if ( modifier )
					{
						if ( modifier->shouldKeepItem( item ) )
							shouldKeep = true;
					}
				}


				if ( _multiplayerGame->shouldKeepItem( item ) )
					shouldKeep = true;


				if ( shouldKeep )
				{

					for ( j = 1 ; j <= _modifiers.NumObjects() ; j++ )
					{
						MultiplayerModifier *modifier;

						modifier = _modifiers.ObjectAt( j );

						if ( modifier )
						{
							modifier->itemKept( item );
						}
					}


					_multiplayerGame->itemKept( item );
				}
				else
				{

					item->PostEvent( EV_Remove, FRAMETIME );
				}
			}	
			else if ( g_entities[ i ].entity->isSubclassOf( Item ) )
			{
				normalItem = (Item *)g_entities[ i ].entity;

				shouldKeep = true;


				for ( j = 1 ; j <= _modifiers.NumObjects() ; j++ )
				{
					MultiplayerModifier *modifier;

					modifier = _modifiers.ObjectAt( j );

					if ( modifier )
					{
						if ( !modifier->shouldKeepNormalItem( normalItem ) )
							shouldKeep = false;
					}
				}


				if ( !_multiplayerGame->shouldKeepNormalItem( normalItem ) )
					shouldKeep = false;


				if ( !shouldKeep )
				{

					normalItem->PostEvent( EV_Remove, FRAMETIME );
				}
			}
		}
	}
}

void MultiplayerManager::resetItems( void )
{
	int i;
	Item *item;


	for ( i = 0; i < MAX_GENTITIES; i++ )
	{
		if ( g_entities[ i ].inuse && g_entities[ i ].entity && g_entities[ i ].entity->isSubclassOf( Item ) )
		{
			item = (Item *)g_entities[ i ].entity;


			if ( item->isSubclassOf( Rune ) || item->isSubclassOf( MultiplayerItem ) )
				continue;

			if ( item->Respawnable() )
			{

				item->CancelEventsOfType( EV_Item_Respawn );
				item->PostEvent( EV_Item_Respawn, 0.0f );
			}
			else
			{

				item->PostEvent( EV_Remove, 0.0f );
			}
		}
	}
}

bool MultiplayerManager::inMultiplayer( void )
{
	return _inMultiplayerGame;
}

bool MultiplayerManager::checkFlag( unsigned int flag )
{
	if ( !_inMultiplayerGame )
		return false;
	

	if ( mp_flags->integer & flag )
		return true;
	else
		return false;
}


bool MultiplayerManager::fullCollision( void )
{
	if ( !_inMultiplayerGame )
		return true;

	if ( checkFlag( MP_FLAG_FULL_COLLISION ) )
		return true;
	else
		return false;
}

bool MultiplayerManager::isFightingAllowed( void )
{
	if ( !_inMultiplayerGame )
		return true;

	if ( !_inMatch )
		return false;

	return _allowFighting;
}

void MultiplayerManager::addPlayer( Player *player )
{
	if ( !_inMultiplayerGame )
		return;

	assert( player );

	if ( !player )
		return;


	if ( _playerData[ player->entnum ]._valid )
		return;


	_playerData[ player->entnum ].reset();
	_playerData[ player->entnum ]._valid = true;


	_multiplayerGame->AddPlayer( player );


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
			modifier->addPlayer( player );
	}


	_awardSystem->addPlayer ( player );


	multiplayerManager.HUDPrintAllClients( va( "%s $$JoinedGame$$\n", player->client->pers.netname ) );
}

void MultiplayerManager::removePlayer( Player *player )
{
	if ( !_inMultiplayerGame )
		return;

	assert( player );

	if ( !player )
		return;

	if ( !_playerData[ player->entnum ]._valid )
		return;


	_multiplayerGame->RemovePlayer( player );


	_playerData[ player->entnum ].reset();


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
			modifier->removePlayer( player );
	}


	_awardSystem->removePlayer ( player );


	multiplayerManager.HUDPrintAllClients( va( "%s $$LeftGame$$\n", player->client->pers.netname ) );
}

void MultiplayerManager::changePlayerModel( Player *player, const char *modelName, bool force )
{
	str modelToUse;

	if ( !_inMultiplayerGame || !player )
		return;

	if ( ( player->model != modelName ) || ( force ) )
	{
		modelToUse = modelName;


		if ( !isValidPlayerModel( player, modelToUse ) )
		{
			centerPrint( player->entnum, va( "%s is not an acceptable model to use", modelName ), CENTERPRINT_IMPORTANCE_NORMAL );
			modelToUse = getDefaultPlayerModel( player );
		}

		if ( checkFlag( MP_FLAG_FORCE_DEFAULT_MODEL ) )
		{
			modelToUse = getDefaultPlayerModel( player );
		}


		player->setBackupModel( getDefaultPlayerModel( player ) );


		player->InitModel( modelToUse );

		player->CancelEventsOfType( EV_ProcessInitCommands );
		player->ProcessInitCommands( player->edict->s.modelindex );

		resetPlayerStateMachine( player );

		if ( multiplayerManager.isPlayerSpectator( player ) )
		{
			player->hideModel();
			player->setSolidType( SOLID_NOT );
		}

		for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
		{
			MultiplayerModifier *modifier;

			modifier = _modifiers.ObjectAt( i );

			if ( modifier )
				modifier->playerChangedModel( player );
		}

		_multiplayerGame->playerChangedModel( player );

		player->modelChanged();
	}
}

void MultiplayerManager::resetPlayerStateMachine( Player *player )
{
	if ( !_inMultiplayerGame )
		return;

	if ( player )
	{
		player->SetAnim( "stand_idle", legs, true );
		player->SetAnim( "stand_idle", torso, true );
		player->LoadStateTable();
	}

}

void MultiplayerManager::changePlayerName( Player *player, const str &playerName )
{
	if ( !_inMultiplayerGame )
		return;

	if ( player )
	{
		if ( _playerData[ player->entnum ]._named && ( _playerData[ player->entnum ]._name != playerName ) )
		{

			multiplayerManager.HUDPrintAllClients( va( "%s $$ChangedName$$ %s\n", _playerData[ player->entnum ]._name.c_str(), playerName.c_str() ) );
		}

		_playerData[ player->entnum ]._named = true;
		_playerData[ player->entnum ]._name = playerName;
	}

}

void MultiplayerManager::playerKilled( Player *killedPlayer, Player *attackingPlayer, Entity *inflictor, int meansOfDeath )
{
	if ( !_inMultiplayerGame )
		return;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
			modifier->playerKilled( killedPlayer, attackingPlayer, inflictor, meansOfDeath );
	}


	_multiplayerGame->playerKilled( killedPlayer, attackingPlayer, inflictor, meansOfDeath );


	_awardSystem->playerKilled( killedPlayer, attackingPlayer, inflictor, meansOfDeath );


	if ( killedPlayer == attackingPlayer || !attackingPlayer )
	{
		centerPrint( killedPlayer->entnum, va( "^%c$$YouKilledYourself$$^%c", COLOR_RED, COLOR_NONE ), CENTERPRINT_IMPORTANCE_NORMAL );
	}
	else
	{
		str printString;

		centerPrint( killedPlayer->entnum, va( "^%c$$KilledByPlayer$$^%c %s", COLOR_RED, COLOR_NONE, attackingPlayer->client->pers.netname ), CENTERPRINT_IMPORTANCE_NORMAL );

		printString = va( "^%c$$KilledPlayer$$^%c %s", COLOR_GREEN, COLOR_NONE, killedPlayer->client->pers.netname );

		if ( checkRule( "usingIndividualScore", false, attackingPlayer ) )
		{
			str placeName = getPlaceName( attackingPlayer );
			int points = getPoints( attackingPlayer );

			if ( points == 1 )
				printString += va( "\n$$PlaceString1$$ ^%c %s^%c $$PlaceString2$$ ^%c %d^%c $$Point$$", COLOR_CYAN, placeName.c_str(), COLOR_NONE, COLOR_CYAN, points, COLOR_NONE );
			else
				printString += va( "\n$$PlaceString1$$ ^%c %s^%c $$PlaceString2$$ ^%c %d^%c $$Points$$", COLOR_CYAN, placeName.c_str(), COLOR_NONE, COLOR_CYAN, points, COLOR_NONE );
		}

		centerPrint( attackingPlayer->entnum, printString, CENTERPRINT_IMPORTANCE_NORMAL );

		multiplayerManager.instantPlayerSound( attackingPlayer->entnum, "snd_mp_killedsomeone", CHAN_COMBAT4 );
	}
}

void MultiplayerManager::matchOver( void )
{
	if ( !_inMultiplayerGame )
		return;

	int i;


	for ( i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
			modifier->matchOver();
	}


	_multiplayerGame->matchOver();


	_awardSystem->matchOver();

	_inMatch = false;


	cvar_t *mp_solomatch;
	cvar_t *mp_mapAward;
	cvar_t *mp_botskilllevel;
	str cvarName;


	cvarName = level.mapname + "_award";

	mp_solomatch     = gi.cvar( "mp_solomatch",     "0", 0 );
	mp_mapAward      = gi.cvar( cvarName.c_str(),   "0", CVAR_ARCHIVE );
	mp_botskilllevel = gi.cvar( "mp_botskilllevel", "0", CVAR_ARCHIVE );


	if ( mp_solomatch->integer )
	{
		int playersScore;
		int botsScore;
		bool wonMatch = true;
		Player *player;


		player = getPlayer( 0 );
		playersScore = getPoints( player );

		for ( i = 1 ; i < maxclients->integer ; i++ )
		{
			player = getPlayer( i );

			if ( player )
			{
				botsScore = getPoints( player );

				if ( botsScore >= playersScore )
				{
					wonMatch = false;
					break;
				}
			}
		}

		if ( wonMatch )
		{

			if ( mp_mapAward->integer < mp_botskilllevel->integer )
			{

				gi.cvar_set( cvarName.c_str(), mp_botskilllevel->string );
			}
		}
	}
}

int MultiplayerManager::getScoreIcon( Player *player, int index )
{
	int icon = 0;

	if ( _inMatch )
	{
		for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
		{
			MultiplayerModifier *modifier;

			modifier = _modifiers.ObjectAt( i );

			if ( modifier )
			{
				icon = modifier->getScoreIcon( player, index, icon );
			}
		}


		icon = _multiplayerGame->getScoreIcon( player, index, icon );
	}
	else
	{
		icon =  _awardSystem->getAfterMatchAward( player, index );
	}

	if ( ( index == SCOREICON3 ) && ( icon == 0 ) && _playerData[ player->entnum ]._waitingForRespawn )
	{
		icon = _waitingToRespawnIconIndex;
	}

	return icon;
}

int MultiplayerManager::getAfterMatchAward( Player *player, int index )
{
	return _awardSystem->getAfterMatchAward( player, index );
}

void MultiplayerManager::playerDead( Player *player )
{
	assert(player);
	
	if ( !player )
		return;

	TIKIANIM_PROBE("playerdead", "ent=%d -> mode->playerDead", player->entnum);


	_multiplayerGame->playerDead( player );
}

void MultiplayerManager::applySpeedModifiers( Player *player,  int *moveSpeed )
{
	if ( !_inMultiplayerGame )
		return;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
			modifier->applySpeedModifiers( player, moveSpeed );
	}


	_multiplayerGame->applySpeedModifiers( player, moveSpeed );
}

void MultiplayerManager::applyJumpModifiers( Player *player,  int *jumpSpeed )
{
	if ( !_inMultiplayerGame )
		return;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
			modifier->applyJumpModifiers( player, jumpSpeed );
	}


	_multiplayerGame->applyJumpModifiers( player, jumpSpeed );
}

void MultiplayerManager::applyAirAccelerationModifiers( Player *player,  int *airAcceleration )
{
	if ( !_inMultiplayerGame )
		return;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
			modifier->applyAirAccelerationModifiers( player, airAcceleration );
	}


	_multiplayerGame->applyAirAccelerationModifiers( player, airAcceleration );
}

bool MultiplayerManager::canPickup( Player *player, MultiplayerItemType itemType, const char *item_name )
{
	if ( !_inMultiplayerGame )
		return true;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			if ( !modifier->canPickup( player, itemType, item_name ) )
				return false;
		}
	}


	if ( !_multiplayerGame->canPickup( player, itemType, item_name ) )
		return false;


	return true;
}

void MultiplayerManager::pickedupItem( Player *player, MultiplayerItemType itemType, const char *itemName )
{
	if ( !_inMultiplayerGame )
		return;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			modifier->pickedupItem( player, itemType, itemName );
		}
	}


	_multiplayerGame->pickedupItem( player, itemType, itemName );


	_awardSystem->pickedupItem( player, itemType, itemName );
}

int MultiplayerManager::getPointsForKill( Player *killedPlayer, Player *attackingPlayer, Entity *inflictor, int meansOfDeath, int points )
{
	int realPoints;

	realPoints = points;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
			realPoints = modifier->getPointsForKill( killedPlayer, attackingPlayer, inflictor, meansOfDeath, realPoints );
	}

	return realPoints;
}

void MultiplayerManager::respawnPlayer( Player *player, bool forced )
{
	if ( !_inMultiplayerGame )
		return;

	assert(player);
	
	if ( !player )
		return;

	if ( multiplayerManager.isPlayerSpectator( player ) && multiplayerManager.isPlayerSpectatorByChoice( player ) )
		return;

	if ( getRespawnTime() > 0.0f && !forced )
	{
		TIKIANIM_PROBE("respawn:deferred", "ent=%d mp_respawnTime=%.2f -> _waitingForRespawn=true, respawn at %.2f",
		           player->entnum, getRespawnTime(), (float)(getTime() + getRespawnTime()));
		if ( !_playerData[ player->entnum ]._waitingForRespawn )
		{
			_playerData[ player->entnum ]._waitingForRespawn = true;
			_playerData[ player->entnum ]._respawnTime = getTime() + getRespawnTime();
		}
	}
	else
	{
		TIKIANIM_PROBE("respawn:immediate", "ent=%d forced=%d mp_respawnTime=%.2f -> mode->respawnPlayer",
		           player->entnum, forced ? 1 : 0, getRespawnTime());
		_playerData[ player->entnum ]._waitingForRespawn = false;
		_multiplayerGame->respawnPlayer( player );
	}
}

void MultiplayerManager::respawnAllPlayers( void )
{
	int i;
	Player *player;

	if ( !_inMultiplayerGame )
		return;

	for( i = 0 ; i < maxclients->integer ; i++ )
	{
		player = getPlayer( i );

		if ( !player )
			continue;

		if ( multiplayerManager.isPlayerSpectatorByChoice( player ) && !( player->edict->svflags & SVF_BOT ) )
			continue;

		respawnPlayer( player, true );
	}
}

Entity *MultiplayerManager::getSpawnPoint( Player *player )
{
	if ( !_inMultiplayerGame )
		return NULL;

	assert( player );
	
	if ( !player )
		return NULL;


	return _multiplayerGame->getSpawnPoint( player );
}

float MultiplayerManager::playerDamaged( Player *damagedPlayer, Player *attackingPlayer, float damage, int meansOfDeath )
{
	float realDamage;

	realDamage = damage;

	if ( !_inMultiplayerGame )
		return realDamage;


	realDamage *= mp_damageMultiplier->value;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
			realDamage = modifier->playerDamaged( damagedPlayer, attackingPlayer, realDamage, meansOfDeath );
	}


	realDamage = _multiplayerGame->playerDamaged( damagedPlayer, attackingPlayer, realDamage, meansOfDeath );


	_awardSystem->playerDamaged( damagedPlayer, attackingPlayer, realDamage, meansOfDeath );

	return realDamage;
}

void MultiplayerManager::playerTookDamage( Player *damagedPlayer, Player *attackingPlayer, float damage, int meansOfDeath )
{
	_multiplayerGame->playerTookDamage( damagedPlayer, attackingPlayer, damage, meansOfDeath );
}

void MultiplayerManager::playerFired( Player *attackingPlayer )
{
	if ( !_inMultiplayerGame )
		return;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
			modifier->playerFired( attackingPlayer );
	}


	_multiplayerGame->playerFired( attackingPlayer );


	_awardSystem->playerFired( attackingPlayer );
}

float MultiplayerManager::getModifiedKnockback( Player *damagedPlayer, Player *attackingPlayer, float knockback )
{
	if ( !_inMultiplayerGame )
		return knockback;


	return knockback * mp_knockbackMultiplier->value;
}

void MultiplayerManager::itemTouched( Player *player, MultiplayerItem *item )
{
	if ( !_inMultiplayerGame )
		return;


	if ( isPlayerSpectator( player ) || ( player->deadflag != DEAD_NO ) || ( player->getHealth() <= 0.0f ) )
		return;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
			modifier->itemTouched( player, item );
	}


	_multiplayerGame->itemTouched( player, item );
}

float MultiplayerManager::itemDamaged( MultiplayerItem *item, Player *attackingPlayer, float damage, int meansOfDeath )
{
	float realDamage;

	if ( !_inMultiplayerGame )
		return damage;

	realDamage = damage;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			realDamage = modifier->itemDamaged( item, attackingPlayer, realDamage, meansOfDeath );
		}
	}


	return _multiplayerGame->itemDamaged( item, attackingPlayer, realDamage, meansOfDeath );
}

void MultiplayerManager::itemDestroyed( Player *player, MultiplayerItem *item )
{
	if ( !_inMultiplayerGame )
		return;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
			modifier->itemDestroyed( player, item );
	}


	_multiplayerGame->itemDestroyed( player, item );
}

void MultiplayerManager::itemUsed( Entity *entity, MultiplayerItem *item )
{
	if ( !_inMultiplayerGame )
		return;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
			modifier->itemUsed( entity, item );
	}


	_multiplayerGame->itemUsed( entity, item );
}

void MultiplayerManager::playerUsed( Player *usedPlayer, Player *usingPlayer, Equipment *equipment )
{
	if ( !_inMultiplayerGame )
		return;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
			modifier->playerUsed( usedPlayer, usingPlayer, equipment );
	}


	_multiplayerGame->playerUsed( usedPlayer, usingPlayer, equipment );
}

bool MultiplayerManager::checkRule( const char *rule, bool defaultValue, Player *player )
{
	bool value;

	if ( !_inMultiplayerGame )
		return defaultValue;


	value = defaultValue;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			value = modifier->checkRule( rule, value, player );
		}
	}


	value = _multiplayerGame->checkRule( rule, value, player );

	return value;
}

bool MultiplayerManager::checkGameType( const char *gameType )
{
	if ( !_inMultiplayerGame )
		return false;


	if ( _multiplayerGame->checkGameType( gameType ) )
		return true;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			if ( modifier->checkGameType( gameType ) )
				return true;
		}
	}

	return false;
}

bool MultiplayerManager::doesPlayerHaveItem( Player *player, const char *itemName )
{
	if ( !_inMultiplayerGame )
		return false;

	if ( !player )
		return false;


	if ( _multiplayerGame->doesPlayerHaveItem( player, itemName ) )
		return true;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			if ( modifier->doesPlayerHaveItem( player, itemName ) )
				return true;
		}
	}

	return false;
}

void MultiplayerManager::score( Player *player )
{
	assert(player);
	
	if ( !player )
		return;

	if ( !_inMultiplayerGame )
		return;


	_multiplayerGame->score( player );
}

int MultiplayerManager::getPoints( Player *player )
{
	assert(player);
	
	if ( !player )
		return 0;

	if ( !_inMultiplayerGame )
		return 0;


	return _multiplayerGame->getPoints( player );
}

int MultiplayerManager::getKills( Player *player )
{
	assert(player);
	
	if ( !player )
		return 0;

	if ( !_inMultiplayerGame )
		return 0;


	return _multiplayerGame->getKills( player );
}

int MultiplayerManager::getDeaths( Player *player )
{
	assert(player);
	
	if ( !player )
		return 0;

	if ( !_inMultiplayerGame )
		return 0;


	return _multiplayerGame->getDeaths( player );
}

int MultiplayerManager::getTeamPoints( Player *player )
{
	assert(player);
	
	if ( !player )
		return 0;

	if ( !_inMultiplayerGame )
		return 0;


	return _multiplayerGame->getTeamPoints( player );
}

int MultiplayerManager::getTeamPoints( const str &teamName )
{
	if ( !_inMultiplayerGame )
		return 0;


	return _multiplayerGame->getTeamPoints( teamName );
}

void MultiplayerManager::addTeamPoints( const str &teamName, int points )
{
	if ( !_inMultiplayerGame )
		return;


	_multiplayerGame->addTeamPoints( teamName, points );
}

int MultiplayerManager::getStat( Player *player, int statNum )
{
	int value = 0;

	assert(player);
	
	if ( !player )
		return 0;

	if ( !_inMultiplayerGame )
		return 0;


	if ( statNum == STAT_TIMELEFT_SECONDS )
	{
		value = 0;

		if ( mp_timelimit->integer )
		{
			value = (int) (mp_timelimit->integer * 60.0f - level.time);
		}
	}

	if ( statNum == STAT_MP_SPECTATING_ENTNUM )
	{
		value = _playerData[ player->entnum ]._spectatorPlayerNum;
	}


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			value = modifier->getStat( player, statNum, value );
		}
	}


	value = _multiplayerGame->getStat( player, statNum, value );

	value = _awardSystem->getStat( player, statNum, value );

	return value;
}

int MultiplayerManager::getIcon( Player *player, int statNum )
{
	int value = -1;

	assert(player);
	
	if ( !player )
		return -1;

	if ( !_inMultiplayerGame )
		return -1;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			value = modifier->getIcon( player, statNum, value );
		}
	}


	value = _multiplayerGame->getIcon( player, statNum, value );

	value = _awardSystem->getIcon( player, statNum, value );

	return value;
}

Team* MultiplayerManager::getPlayersTeam( const Player *player )
{
	assert(player);
	
	if ( !player )
		return NULL;

	if ( !_inMultiplayerGame )
		return NULL;

	return _multiplayerGame->getPlayersTeam( player );
}


void MultiplayerManager::callVote( Player *player, const str &command, const str &arg )
{
	int i;
	int count;


	if ( !_inMultiplayerGame )
		return;


	if ( checkFlag( MP_FLAG_DONT_ALLOW_VOTE ) )
	{
		HUDPrint( player->entnum, "$$VotingNotAllowed$$.\n" );
		return;
	}


	if ( _voteTime )
	{
		HUDPrint( player->entnum, "$$AlreadyVoting$$.\n" );
		return;
	}


	if ( _playerData[ player->entnum ]._votecount >= mp_maxVotes->integer )
	{
		HUDPrint( player->entnum, va( "$$MaxVotes$$ (%d).\n", mp_maxVotes->integer ) );
		return;
	}


	if( strchr( command.c_str(), ';' ) || strchr( arg.c_str(), ';' ) )
	{
		HUDPrint( player->entnum, "$$InvalidVote$$\n" );
		return;
	}

	if ( ( stricmp( command.c_str(), "restart" )    != 0 ) &&
		 ( stricmp( command.c_str(), "nextmap" )    != 0 ) &&
		 ( stricmp( command.c_str(), "map" )        != 0 ) &&
		 ( stricmp( command.c_str(), "g_gametype" ) != 0 ) &&
		 ( stricmp( command.c_str(), "kick" )       != 0 ) )
	{
		HUDPrint( player->entnum, "$$InvalidVote$$\n" );
		HUDPrint( player->entnum, "$$VoteCommands$$: restart, nextmap, map <mapname>, g_gametype <n> and kick <player>.\n" );
		return;
	}


	if ( stricmp( command.c_str(), "map" ) == 0 )
	{
		str fullMapName;
		str printString;

		fullMapName = "maps/";
		fullMapName += arg;
		fullMapName += ".bsp";

		if ( !gi.FS_Exists( fullMapName.c_str() ) )
		{
			printString = fullMapName + " $$NotFoundOnServer$$\n";
			HUDPrint( player->entnum, printString.c_str() );
			return;
		}
	}


	if ( stricmp( command.c_str(), "map" ) == 0 )
	{
		if ( strlen( sv_nextmap->string ) )
		{
			_voteString = va( "%s %s; set nextmap \"%s\"", command.c_str(), arg.c_str(), sv_nextmap->string );
		}
		else
		{
			_voteString = va( "%s %s", command.c_str(), arg.c_str() );
		}
	}
	else
	{
		_voteString = va( "%s %s", command.c_str(), arg.c_str() );
	}


	multiplayerManager.HUDPrintAllClients( va( "%s ^4$$CalledVote$$:^8 %s\n", player->client->pers.netname, _voteString.c_str() ) );


	broadcastInstantSound( "sound/ships/attrexian/att-beepreject.wav" );


	_voteTime = level.time;
	_voteYes  = 1;
	_voteNo   = 0;

	_playerData[ player->entnum ]._voted = true;
	_playerData[ player->entnum ]._votecount++;


	count = 1;

	for( i = 0; i < maxclients->integer; i++ )
	{
		Player *currentPlayer;
		gentity_t *ent = g_entities + i;

		if ( !ent->inuse || !ent->client || !ent->entity )
			continue;

		if ( ent->svflags & SVF_BOT )
			continue;

		if ( i == player->entnum )
			continue;

		_playerData[ i ]._voted = false;

		currentPlayer = getPlayer( i );

		if ( currentPlayer )
		{
			currentPlayer->setVoteText( va( "$$NewVote$$: %s (F1 = $$Yes$$, F2 = $$No$$)\n", _voteString.c_str() ) );
		}

		count++;
	}

	_numVoters = count;
}

void MultiplayerManager::vote( Player *player, const str &vote )
{


	if ( !_voteTime )
	{
		HUDPrint( player->entnum, "$$NoVote$$\n" );
		return;
	}


	if ( _playerData[ player->entnum ]._voted )
	{
		HUDPrint( player->entnum, "$$VoteAlreadyCast$$\n" );
		return;
	}


	if ( vote.length() < 1 )
		return;


	_playerData[ player->entnum ]._voted = true;

	player->clearVoteText();

	if ( ( vote[0] == 'y' ) || ( vote[0] == 'Y' ) || ( vote[0] == '1' ) )
	{
		_voteYes++;

		HUDPrint( player->entnum, "$$VoteCast$$ - $$Yes$$\n" );
	}
	else
	{
		_voteNo++;

		HUDPrint( player->entnum, "$$VoteCast$$ - $$No$$\n" );
	}

}

void MultiplayerManager::checkVote( void )
{
	int i;


	if ( !_voteTime )
	{
		return;
	}


	if ( level.time - _voteTime >= _maxVoteTime )
	{
		multiplayerManager.HUDPrintAllClients( "$$VoteFailed$$\n" );
	}
	else
	{

		if ( _voteYes > ( _numVoters / 2.0f ) )
		{

			multiplayerManager.HUDPrintAllClients( "$$VotePassed$$\n" );
			gi.SendConsoleCommand( va("%s\n", _voteString.c_str() ) );
		}
		else if ( _voteNo >= ( _numVoters / 2.0f ) )
		{
			multiplayerManager.HUDPrintAllClients( "$$VoteFailed$$\n" );
		}
		else
		{
			return;
		}
	}

	_voteTime = 0.0f;


	for( i = 0; i < maxclients->integer; i++ )
	{
		Player *currentPlayer;

		currentPlayer = getPlayer( i );

		if ( currentPlayer )
		{
			currentPlayer->clearVoteText();
		}
	}
}

void MultiplayerManager::joinTeam( Player *player, const str &teamName )
{
	str realTeamName;

	int i;


	if ( !player || !_inMultiplayerGame )
		return;


	if ( stricmp( teamName.c_str(), "Blue" ) == 0 )
		realTeamName = "Blue";
	else if ( stricmp( teamName.c_str(), "Red" ) == 0 )
		realTeamName = "Red";
	else
		realTeamName = teamName;

	if ( ( realTeamName == "Red" ) || ( realTeamName == "Blue" ) || ( realTeamName == "normal" ) )
	{
		_playerData[ player->entnum ]._spectatorByChoice = false;
	}


	if ( !_multiplayerGame->canJoinTeam( player, realTeamName ) )
		return;


	for ( i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			modifier->removePlayer( player );
		}
	}


	_multiplayerGame->joinTeam( player, realTeamName );


	for ( i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			modifier->addPlayer( player );

			modifier->joinedTeam( player, realTeamName );
		}
	}
}

void MultiplayerManager::say( Player *player, const str &text, bool team )
{
	str realText;
	str tempText;
	int i;
	str name;


	if ( player )
	{
		name = player->client->pers.netname;
	}
	else
	{
		name = "$$ServerName$$";
	}

	realText = name;
	realText += ":";


	if ( player )
	{
		realText += "^";

		if ( team )
			realText += COLOR_YELLOW;
		else
			realText += COLOR_CYAN;

		realText += " ";
	}


	tempText = text;

	for ( i = 0 ; i < tempText.length() ; i++ )
	{
		if ( tempText[ i ] == '^' )
		{
			tempText[ i ] = '*';
		}
	}


	realText += tempText;


	if ( player )
	{
		realText += "^";
		realText += COLOR_NONE;
	}

	realText += "\n";


	for ( i = 0 ; i < realText.length() ; i++ )
	{
		if ( realText[ i ] == '%' )
		{
			realText[ i ] = '.';
		}
	}


	if ( realText.length() > _maxSayStringLength )
	{
		HUDPrint( player->entnum, "$$SayTooLong$$\n" );
		return;
	}


	if ( dedicated->integer )
	{
		gi.Printf( "%s: %s\n", name.c_str(), text.c_str() );
	}


	if ( player && isPlayerSpectator( player ) )
		HUDPrintSpectators( player, realText, team );
	else if ( player && team )
		HUDPrintTeamClients( player, realText );
	else
		HUDPrintAllClients( realText );
}

void MultiplayerManager::tell( Player *player, const str &text, int entnum )
{
	str realText;
	str tempText;
	int i;
	Player *otherPlayer;


	realText = player->client->pers.netname;
	realText += ":";


	realText += "^";
	realText += COLOR_MAGENTA;
	realText += " ";


	tempText = text;

	for ( i = 0 ; i < tempText.length() ; i++ )
	{
		if ( tempText[ i ] == '^' )
		{
			tempText[ i ] = '*';
		}
	}


	realText += tempText;


	realText += "^";
	realText += COLOR_NONE;
	realText += "\n";


	if ( realText.length() > _maxSayStringLength )
	{
		HUDPrint( player->entnum, "$$SayTooLong$$\n" );
		return;
	}


	otherPlayer = getPlayer( entnum );

	if ( !otherPlayer )
		return;



	if ( isPlayerSpectator( player ) && !isPlayerSpectator( otherPlayer ) )
		return;

	HUDSay( otherPlayer->entnum, realText );
}


void MultiplayerManager::addModifiers( void )
{
	tryAddModifier( "InstantKill" );
	tryAddModifier( "ActionHero" );
	tryAddModifier( "AutoHandicap" );
	tryAddModifier( "PointsPerWeapon" );
	tryAddModifier( "ControlPoints" );
	tryAddModifier( "Destruction" );
	tryAddModifier( "OneFlag" );
	tryAddModifier( "Elimination" );
	tryAddModifier( "Diffusion" );
	tryAddModifier( "Specialties" );
}

void MultiplayerManager::tryAddModifier( const str &modifierName )
{
	cvar_t *mp_modifier;
	str cvarName;

	cvarName = "mp_modifier_";
	cvarName += modifierName;

	mp_modifier = gi.cvar( cvarName.c_str(), "0", 0 );

	if ( mp_modifier->integer )
	{
		addModifier( modifierName );
	}
}

void MultiplayerManager::addModifier( const str &modifierName )
{
	MultiplayerModifier *newModifier = NULL;

	if ( modifierName == "InstantKill" )
	{
		newModifier = new ModifierInstantKill;
	}
	else if ( modifierName == "ActionHero" )
	{
		newModifier = new ModifierActionHero;
	}
	else if ( modifierName == "AutoHandicap" )
	{
		newModifier = new ModifierAutoHandicap;
	}
	else if ( modifierName == "PointsPerWeapon" )
	{
		newModifier = new ModifierPointsPerWeapon;
	}
	else if ( modifierName == "ControlPoints" )
	{
		newModifier = new ModifierControlPoints;
	}
	else if ( modifierName == "Destruction" )
	{
		newModifier = new ModifierDestruction;
	}
	else if ( modifierName == "OneFlag" )
	{
		newModifier = new ModifierOneFlag;
	}
	else if ( modifierName == "Elimination" )
	{
		newModifier = new ModifierElimination;
	}
	else if ( modifierName == "Diffusion" )
	{
		newModifier = new ModifierDiffusion;
	}
	else if ( modifierName == "Specialties" )
	{
		newModifier = new ModifierSpecialties;
	}

	if ( newModifier )
	{
		_modifiers.AddObject( newModifier );
	}
}

void MultiplayerManager::addPoints( int entnum, int points )
{
	if ( !_inMultiplayerGame )
		return;

	_multiplayerGame->addPoints( entnum, points );
}


int MultiplayerManager::getClientNum( int entnum )
{
	gclient_s *client;


	client = getClient( entnum );

	if ( client )
		return client->ps.clientNum;
	else
		return -1;
}

int MultiplayerManager::getClientPing( int entnum )
{
	gclient_s *client;


	client = getClient( entnum );

	if ( client )
		return client->ps.ping;
	else
		return -1;
}

float MultiplayerManager::getTime( void )
{
	return level.time;
}

void MultiplayerManager::centerPrint( int entnum, const str &string, CenterPrintImportance importance )
{
	gentity_t *gentity;

	if ( ( entnum >= 0 ) && ( entnum < maxclients->integer ) )
	{
		gentity = &g_entities[ entnum ];

		if ( gentity->inuse && gentity->entity && gentity->client )
		{
			gi.centerprintf( gentity, importance, "%s", string.c_str() );
		}
	}
}

Player *MultiplayerManager::getPlayer( int entnum )
{
	gentity_t *gentity;
	Player *player = NULL;


	if ( ( entnum >= 0 ) && ( entnum < maxclients->integer ) )
	{
		gentity = &g_entities[ entnum ];

		if ( gentity->inuse && gentity->entity && gentity->client && gentity->entity->isSubclassOf( Player ) )
		{
			player = (Player *)gentity->entity;
		}
	}

	return player;
}

int MultiplayerManager::getMaxPlayers( void )
{
	return maxclients->integer;
}

int MultiplayerManager::getTotalPlayers( bool countSpectators )
{
	int i;
	Player *player;
	int numPlayers;

	numPlayers = 0;

	for( i = 0 ; i < maxclients->integer ; i++ )
	{
		player = getPlayer( i );

		if ( player )
		{
			if ( !isPlayerSpectator( player ) || !countSpectators )
			{
				numPlayers++;
			}
		}

	}
	return numPlayers;
}

gclient_s *MultiplayerManager::getClient( int entnum )
{
	gentity_t *gentity;


	if ( ( entnum >= 0 ) && ( entnum < maxclients->integer ) )
	{
		gentity = &g_entities[ entnum ];

		if ( gentity->inuse && gentity->entity && gentity->client && gentity->entity->isSubclassOf( Player ) )
		{
			return gentity->client;
		}
	}

	return NULL;
}

void MultiplayerManager::HUDSay( int entnum, const str &string )
{
	str command;
	Player *player;


	player = getPlayer( entnum );

	if ( player )
	{

		command = "hudsay \"";
		command += string;
		command += "\"\n";

		if ( gi.GetNumFreeReliableServerCommands( player->edict - g_entities ) > 32 )
		{
			gi.SendServerCommand( player->edict - g_entities, "%s", command.c_str() );
		}
	}
}

void MultiplayerManager::HUDPrint( int entnum, const str &string )
{
	Player *player;

	player = getPlayer( entnum );

	if ( player )
	{
		player->hudPrint( string );
	}
}

void MultiplayerManager::statusPrint( int entnum, const str &string )
{
	str command;
	Player *player;


	player = getPlayer( entnum );

	if ( player )
	{

		command = "status \"";
		command += string;
		command += "\"\n";

		gi.SendServerCommand( player->edict - g_entities, "%s", command.c_str() );
	}
}

void MultiplayerManager::playerSound( int entnum, const str &soundName, int channel, float volume, float minDist, float time )
{
	Player *player;


	player = getPlayer( entnum );


	if ( player )
	{
		addSoundToQueue( player, soundName, channel, volume, minDist, time );
	}
}

void MultiplayerManager::instantPlayerSound( int entnum, const str &soundName, int channel, float volume, float minDist )
{
	Player *player;


	player = getPlayer( entnum );


	if ( player )
	{
		player->Sound( soundName, channel, volume, minDist, NULL, 1.0f, true );
	}
}

void MultiplayerManager::broadcastInstantSound( const str &soundName, int channel, float volume, float minDist, Player *except )
{
	int i;
	Player *player;


	for( i = 0; i < maxclients->integer; i++ )
	{

		player = getPlayer( i );


		if ( player && ( player != except ) )
		{
			instantPlayerSound( player->entnum, soundName, channel, volume, minDist );
		}
	}
}

void MultiplayerManager::teamSound( Team *team, const str &soundName, int channel, float volume, float minDist, float time )
{
	int i;
	Player *player;


	for( i = 0; i < maxclients->integer; i++ )
	{

		player = getPlayer( i );


		if ( player && ( getPlayersTeam( player ) == team ) )
		{
			playerSound( player->entnum, soundName, channel, volume, minDist, time );
		}
	}
}

void MultiplayerManager::broadcastSound( const str &soundName, int channel, float volume, float minDist, Player *except, float time )
{
	int i;
	Player *player;


	for( i = 0; i < maxclients->integer; i++ )
	{

		player = getPlayer( i );


		if ( player && ( player != except ) )
		{
			playerSound( player->entnum, soundName, channel, volume, minDist, time );
		}
	}
}

bool MultiplayerManager::isPlayerSpectator( Player *player, SpectatorTypes spectatorType )
{
	if ( !_inMultiplayerGame )
		return false;

	if ( spectatorType == SPECTATOR_TYPE_ANY )
		return _playerData[ player->entnum ]._spectator;
	else if ( spectatorType == SPECTATOR_TYPE_NONE )
		return !_playerData[ player->entnum ]._spectator;
	else
		return ( _playerData[ player->entnum ]._spectator && _playerData[ player->entnum ]._spectatorType == spectatorType );
}

bool MultiplayerManager::isPlayerSpectatorByChoice( Player *player )
{
	if ( !_inMultiplayerGame )
		return false;

	if ( _playerData[ player->entnum ]._spectator &&  _playerData[ player->entnum ]._spectatorByChoice )
		return true;
	else
		return false;
}

Player *MultiplayerManager::getPlayerSpectating( Player *player )
{
	if ( isPlayerSpectator( player, SPECTATOR_TYPE_FOLLOW ) )
	{
		return getPlayer( _playerData[ player->entnum ]._spectatorPlayerNum );
	}

	return NULL;
}

void MultiplayerManager::makePlayerSpectator( Player *player, SpectatorTypes spectatorType, bool byChoice )
{
	Team *team;


	if ( player )
	{
		initPlayer( player );

		_playerData[ player->entnum ]._spectator = true;
		_playerData[ player->entnum ]._spectatorType = spectatorType;
		_playerData[ player->entnum ]._spectatorTime = getTime();

		if ( byChoice )
		{
			_playerData[ player->entnum ]._spectatorByChoice = true;
		}

		_playerData[ player->entnum ]._spectatorPlayerNum = 0;


		if ( spectatorType == SPECTATOR_TYPE_FOLLOW )
			centerPrint( player->entnum, "$$SpectatorFollow$$", CENTERPRINT_IMPORTANCE_NORMAL );
		else if ( spectatorType == SPECTATOR_TYPE_FREEFORM )
			centerPrint( player->entnum, "$$SpectatorFreeForm$$", CENTERPRINT_IMPORTANCE_NORMAL );
		else
			centerPrint( player->entnum, "$$SpectatorNormal$$", CENTERPRINT_IMPORTANCE_NORMAL );

		if ( spectatorType == SPECTATOR_TYPE_FOLLOW )
		{
			Player *playerToSpectate = getLastKillerOfPlayer( player );

			if ( playerToSpectate )
				makePlayerSpectatePlayer( player, playerToSpectate );
			else
				makePlayerSpectateNextPlayer( player );
		}

		player->takedamage              = DAMAGE_NO;
		player->client->ps.feetfalling  = false;
		player->deadflag				= DEAD_NO;

		player->flags &= ~FL_IMMOBILE;
		player->flags &= ~FL_STUNNED;



		player->hideModel();


		player->SetState( "STAND", "STAND" );


		player->setSolidType( SOLID_NOT );


		player->setMoveType( MOVETYPE_WALK );

		
		player->FreeInventory();
		player->disableInventory();

		team = getPlayersTeam( player );

		if ( !team )
			setTeamHud( player, "mp_teamspec" );
		else if ( team->getName() == "Red" )
			setTeamHud( player, "mp_teamredspec" );
		else
			setTeamHud( player, "mp_teambluespec" );

		player->clearItemText();
	}
}

void MultiplayerManager::makePlayerSpectateNextPlayer( Player *player )
{
	int startIndex;
	int currentIndex;
	Player *testPlayer;

	if ( !isPlayerSpectator( player, SPECTATOR_TYPE_FOLLOW ) )
		return;
	
	startIndex = _playerData[ player->entnum ]._spectatorPlayerNum;

	currentIndex = startIndex + 1;

	if ( currentIndex >= maxclients->integer )
		currentIndex = 0;

	while ( currentIndex != startIndex )
	{
		testPlayer = getPlayer( currentIndex );

		if ( testPlayer && !isPlayerSpectator( testPlayer ) )
		{
			_playerData[ player->entnum ]._spectatorPlayerNum = currentIndex;
			return;
		}

		currentIndex++;

		if ( currentIndex >= maxclients->integer )
			currentIndex = 0;
	}

	testPlayer = getPlayer( currentIndex );

	if ( testPlayer && !isPlayerSpectator( testPlayer ) )
	{
		_playerData[ player->entnum ]._spectatorPlayerNum = currentIndex;
		return;
	}
	else
	{

		makePlayerSpectator( player, SPECTATOR_TYPE_NORMAL );
	}
}

void MultiplayerManager::makePlayerSpectatePrevPlayer( Player *player )
{
	int startIndex;
	int currentIndex;
	Player *testPlayer;

	if ( !isPlayerSpectator( player, SPECTATOR_TYPE_FOLLOW ) )
		return;
	
	startIndex = _playerData[ player->entnum ]._spectatorPlayerNum;

	currentIndex = startIndex - 1;

	if ( currentIndex < 0 )
		currentIndex = maxclients->integer - 1;

	while ( currentIndex != startIndex )
	{
		testPlayer = getPlayer( currentIndex );

		if ( testPlayer && !isPlayerSpectator( testPlayer ) )
		{
			_playerData[ player->entnum ]._spectatorPlayerNum = currentIndex;
			return;
		}

		currentIndex--;

		if ( currentIndex < 0 )
			currentIndex = maxclients->integer - 1;
	}


	makePlayerSpectator( player, SPECTATOR_TYPE_NORMAL );
}

void MultiplayerManager::makePlayerSpectatePlayer( Player *player, Player *playerToSpectate )
{
	if ( !isPlayerSpectator( player, SPECTATOR_TYPE_FOLLOW ) )
		return;
	
	if ( playerToSpectate && !isPlayerSpectator( playerToSpectate ) )
	{
		_playerData[ player->entnum ]._spectatorPlayerNum = playerToSpectate->entnum;
		return;
	}


	makePlayerSpectateNextPlayer( player );
}

void MultiplayerManager::playerEnterArena( int entnum, float health )
{
	Player *player;
	Team *team;

	player = getPlayer( entnum );

	if ( !player )
		return;

	_playerData[ entnum ]._spectator = false;
	_playerData[ entnum ]._spectatorByChoice = false;


	player->takedamage = DAMAGE_YES;
	player->setSolidType( SOLID_BBOX );


	player->showModel();


	player->FreeInventory();

	player->dropRune();
	player->dropPowerup();
	player->removeHoldableItem();

	player->ProcessEvent( EV_Sentient_StopOnFire );
	player->stopStasis();

	player->removeAttachedModelByTargetname( "attachedSpecialityBackpack" );
	player->removeAttachedModelByTargetname( "attachedDiffusionBomb" );
	player->removeAttachedModelByTargetname( "actionHero" );

	player->clearTempAttachments();

	player->enableInventory();



	player->health = health;

	player->client->ps.pm_time = 100;
	player->client->ps.pm_flags |= PMF_TIME_TELEPORT;

	Event *newEvent = new Event( EV_DisplayEffect );
	newEvent->AddString( "TransportIn" );
	newEvent->AddString( "Multiplayer" );
	player->PostEvent( newEvent, 0.0f );

	changePlayerModel( player, player->model, true );

	team = getPlayersTeam( player );

	if ( !team )
		setTeamHud( player, "" );
	else if ( team->getName() == "Red" )
		setTeamHud( player, "mp_teamred" );
	else
		setTeamHud( player, "mp_teamblue" );
}

void MultiplayerManager::playerSpawned( Player *player )
{

	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			modifier->playerSpawned( player );
		}
	}
}

void MultiplayerManager::allowFighting( bool allowFighting )
{
	_allowFighting = allowFighting;
}

void MultiplayerManager::centerPrintAllClients( const str &string, CenterPrintImportance importance )
{
	int i;
	gentity_t *ent;


	for( i = 0; i < maxclients->integer; i++ )
	{
		ent = g_entities + i;


		if ( !ent->inuse || !ent->client || !ent->entity )
			continue;


		centerPrint( ent->entity->entnum, string, importance );
	}
}

void MultiplayerManager::HUDPrintAllClients( const str &string )
{
	int i;
	gentity_t *ent;


	for( i = 0; i < maxclients->integer; i++ )
	{
		ent = g_entities + i;


		if ( !ent->inuse || !ent->client || !ent->entity )
			continue;


		HUDSay( ent->entity->entnum, string );
	}
}

void MultiplayerManager::centerPrintTeamClients( Player *player, const str &string, CenterPrintImportance importance )
{
	int i;
	gentity_t *ent;
	Player *otherPlayer;


	for( i = 0; i < maxclients->integer; i++ )
	{
		ent = g_entities + i;


		if ( !ent->inuse || !ent->client || !ent->entity )
			continue;

		if ( !ent->entity->isSubclassOf( Player ) )
			continue;

		otherPlayer = (Player *)ent->entity;


		if ( getPlayersTeam( player ) == getPlayersTeam( otherPlayer ) )
		{

			centerPrint( otherPlayer->entnum, string, importance );
		}
	}
}

void MultiplayerManager::HUDPrintTeamClients( Player *player, const str &string )
{
	int i;
	gentity_t *ent;
	Player *otherPlayer;


	for( i = 0; i < maxclients->integer; i++ )
	{
		ent = g_entities + i;



		if ( !ent->inuse || !ent->client || !ent->entity )
			continue;

		if ( !ent->entity->isSubclassOf( Player ) )
			continue;

		otherPlayer = (Player *)ent->entity;


		if ( getPlayersTeam( player ) == getPlayersTeam( otherPlayer ) )
		{

			HUDSay( otherPlayer->entnum, string );
		}
	}
}

void MultiplayerManager::HUDPrintSpectators( Player *player, const str &string, bool team )
{
	int i;
	gentity_t *ent;
	Player *otherPlayer;


	for( i = 0 ; i < maxclients->integer ; i++ )
	{
		ent = g_entities + i;


		if ( !ent->inuse || !ent->client || !ent->entity )
			continue;

		if ( !ent->entity->isSubclassOf( Player ) )
			continue;

		otherPlayer = (Player *)ent->entity;


		if ( !isPlayerSpectator( otherPlayer ) )
			continue;


		if ( team && ( getPlayersTeam( player ) != getPlayersTeam( otherPlayer ) ) )
			continue;


		HUDSay( otherPlayer->entnum, string );
	}
}

void MultiplayerManager::addPlayerHealth( int entnum, float healthToAdd ) 
{
	Player *player;

	player = getPlayer( entnum );

	if ( player )
	{
		player->AddHealth( healthToAdd );
	}
}

void MultiplayerManager::givePlayerItem( int entnum, const str &itemName )
{
	Player *player;
	Event *event;
	bool canGive;


	if ( !_inMatch )
		return;

	player = getPlayer( entnum );

	if ( !player )
		return;

	canGive = canGivePlayerItem( entnum, itemName );

	if ( canGive )
	{
		event = new Event( EV_Player_GiveCheat );
		event->AddString( itemName );
		player->ProcessEvent( event );
	}
}

void MultiplayerManager::usePlayerItem( int entnum, const str &itemName )
{
	Player *player;

	player = getPlayer( entnum );

	if ( !player )
		return;


	Event *ev = new Event( "use" );
	ev->AddString( itemName );
	player->ProcessEvent( ev );
}

bool MultiplayerManager::canGivePlayerItem( int entnum, const str &itemName )
{
	int i;
	Player *player;

	player = getPlayer( entnum );

	if ( !player )
		return false;

	if ( player->FindItem( itemName ) )
		return false;


	for ( i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			if ( !modifier->canGivePlayerItem( entnum, itemName ) )
				return false;
		}
	}


	if ( !_multiplayerGame->canGivePlayerItem( entnum, itemName ) )
		return false;

	return true;
}

float MultiplayerManager::getItemRespawnMultiplayer( void )
{
	if ( !_inMultiplayerGame )
		return 1.0f;
	else
		return mp_itemRespawnMultiplier->value;
}

float MultiplayerManager::getWeaponRespawnMultiplayer( void )
{
	if ( !_inMultiplayerGame )
		return 1.0f;
	else
		return mp_weaponRespawnMultiplier->value;
}

float MultiplayerManager::getPowerupRespawnMultiplayer( void )
{
	if ( !_inMultiplayerGame )
		return 1.0f;
	else
		return mp_powerupRespawnMultiplier->value;
}

void MultiplayerManager::playerEventNotification( const char *eventName, const char *eventItemName, Player *eventPlayer )
{
	int i;
	gentity_t *ent;
	Player *player;

	if ( !_inMultiplayerGame )
		return;


	for ( i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
			modifier->playerEventNotification( eventName, eventItemName, eventPlayer );
	}


	_awardSystem->playerEventNotification( eventName, eventItemName, eventPlayer );


	for( i = 0; i < maxclients->integer; i++ )
	{
		ent = g_entities + i;


		if ( !ent->inuse || !ent->client || !ent->entity || !ent->entity->isSubclassOf( Player ) )
			continue;

		player = (Player *)ent->entity;

		player->notifyPlayerOfMultiplayerEvent( eventName, eventItemName, eventPlayer );
	}


	_multiplayerGame->playerEventNotification( eventName, eventItemName, eventPlayer );
}

void MultiplayerManager::startMatch( void )
{
	int i;

	_inMatch = true;


	for ( i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			modifier->matchStarting();
		}
	}


	_multiplayerGame->startMatch();


	for ( i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			modifier->matchStarted();
		}
	}
}

void MultiplayerManager::restartMatch( void )
{

	_multiplayerGame->endMatch();


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			modifier->matchEnded();
		}
	}

	resetItems();
	

	_multiplayerGame->restartMatch();



	_inMatch = true;
}

void MultiplayerManager::endMatch( void )
{
	_inMatch = false;

	_restartMatchTime = getTime() + _inBetweenMatchTime;
}

void MultiplayerManager::initPlayer( Player *player )
{
	Vector savedAngles;
	Vector savedViewAngles;
	qboolean saved_teleport_bit;


	player->CancelPendingEvents();


	saved_teleport_bit = player->edict->s.eFlags & EF_TELEPORT_BIT;
	savedAngles = player->client->cmd_angles;
	savedViewAngles = player->getViewAngles();


	player->Init();


	player->edict->s.eFlags |= saved_teleport_bit;
	savedAngles.copyTo( player->client->cmd_angles );
	player->SetViewAngles( savedViewAngles );
}

Player *MultiplayerManager::getLastKilledByPlayer( Player *player, int *meansOfdeath )
{
	return _multiplayerGame->getLastKilledByPlayer( player, meansOfdeath );
}

Player *MultiplayerManager::getLastKillerOfPlayer( Player *player, int *meansOfdeath )
{
	return _multiplayerGame->getLastKillerOfPlayer( player, meansOfdeath );
}

void MultiplayerManager::playerCommand( Player *player, const char *command, const char *parm_shadow1 )
{
	if ( !_inMultiplayerGame )
		return;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			modifier->playerCommand( player, command, parm_shadow1 );
		}
	}


	_multiplayerGame->playerCommand( player, command, parm_shadow1 );
}

void MultiplayerManager::playerInput( Player *player, int newButtons )
{
	if ( isPlayerSpectator( player ) )
	{
		if ( newButtons & BUTTON_USE )
		{

			if ( isPlayerSpectator( player, SPECTATOR_TYPE_FOLLOW ) )
				makePlayerSpectator( player, SPECTATOR_TYPE_NORMAL );
			else if ( isPlayerSpectator( player, SPECTATOR_TYPE_NORMAL ) )
				makePlayerSpectator( player, SPECTATOR_TYPE_FREEFORM );
			else 
				makePlayerSpectator( player, SPECTATOR_TYPE_FOLLOW );
		}

		if ( isPlayerSpectator( player, SPECTATOR_TYPE_FOLLOW ) && ( _playerData[ player->entnum ]._spectatorTime != getTime() ) )
		{
			if ( newButtons & BUTTON_ATTACKRIGHT )
			{

				makePlayerSpectateNextPlayer( player );
			}
		
			if ( newButtons & BUTTON_ATTACKLEFT )
			{

				makePlayerSpectatePrevPlayer( player );
			}
		}
	}
}

int MultiplayerManager::getInfoIcon( Player *player, int buttons )
{
	int icon;


	if ( buttons & BUTTON_TALK )
		return _talkingIconIndex;


	icon = _multiplayerGame->getInfoIcon( player );

	if ( icon > 0 )
		return icon;


	icon = _awardSystem->getInfoIcon( player );

	if ( icon > 0 )
		return icon;


	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			icon = modifier->getInfoIcon( player );

			if ( icon > 0 )
				return icon;
		}
	}


	return 0;
}

void MultiplayerManager::teamPointsChanged( Team *team, int oldPoints, int newPoints )
{
	if ( !_inMultiplayerGame )
		return;


	_multiplayerGame->teamPointsChanged( team, oldPoints, newPoints );
}

void MultiplayerManager::cacheMultiplayerFiles( const str &cacheFileName )
{
	str fileToCache;


	fileToCache = "precache/server/";
	fileToCache += cacheFileName;
	fileToCache += ".txt";


	if ( gi.FS_ReadFile( fileToCache, NULL, true ) != -1 )
	{
		level.consoleThread->Parse( fileToCache );
	}
}

str MultiplayerManager::getPlaceName( Player *player )
{
	int place;
	str placeName;

	place = _multiplayerGame->getPlace( player );

	placeName = "$$Place";
	placeName += place;
	placeName += "$$";

	return placeName;
}

void MultiplayerManager::setTeamHud( Player *player, const str &teamHudName )
{
	str command;

	if ( teamHudName == _playerData[ player->entnum ]._teamHud )
		return;


	if ( _playerData[ player->entnum ]._teamHud.length() > 0 )
	{
		command = va( "stufftext \"ui_removehud %s\"\n", _playerData[ player->entnum ]._teamHud.c_str() );
		gi.SendServerCommand( player->entnum, "%s", command.c_str() );
	}


	if ( teamHudName.length() > 0 )
	{
		command = va( "stufftext \"ui_addhud %s\"\n", teamHudName.c_str() );
		gi.SendServerCommand( player->entnum, "%s", command.c_str() );
	}

	_playerData[ player->entnum ]._teamHud = teamHudName;
}

str MultiplayerManager::getSpawnPointType( Player *player )
{
	str spawnPointName;
	MultiplayerModifier *modifier;
	int i;
	float priority;
	float highestPriority = 0.0f;

	for ( i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			priority = modifier->getSpawnPointPriority( player );

			if ( priority > highestPriority )
			{
				spawnPointName = modifier->getSpawnPointType( player );

				highestPriority = priority;
			}
		}
	}

	return spawnPointName;
}

bool MultiplayerManager::isValidPlayerModel( Player *player, str modelToUse )
{
	MultiplayerModifier *modifier;
	int i;
	bool validPlayerModel = false;
	int modelIndex;
	tiki_cmd_t tikicmds;


	modelIndex = gi.modelindex( modelToUse );

	if ( gi.InitCommands( modelIndex, &tikicmds ) )
	{
		for( i = 0; i < tikicmds.num_cmds; i++ )
		{
			if ( stricmp( tikicmds.cmds[ i ].args[ 0 ], "validPlayerModel" ) == 0 )
			{
				validPlayerModel = true;
				break;
			}
		}
	}


	validPlayerModel = _multiplayerGame->isValidPlayerModel( player, modelToUse, validPlayerModel );


	for ( i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			validPlayerModel = modifier->isValidPlayerModel( player, modelToUse, validPlayerModel );
		}
	}

	return validPlayerModel;
}

str MultiplayerManager::getDefaultPlayerModel( Player *player )
{
	MultiplayerModifier *modifier;
	int i;
	str modelName;


	modelName = _multiplayerGame->getDefaultPlayerModel( player, "models/char/munro.tik" );

	for ( i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			modelName = modifier->getDefaultPlayerModel( player, modelName );
		}
	}

	return modelName;
}

void MultiplayerManager::setNextMap( void )
{
	str nextMapName;
	str fullMapName;

	if ( ( strlen( sv_nextmap->string ) == 0 ) && ( mp_useMapList->integer ) && ( strlen( mp_mapList->string ) > 0 ) )
	{
		nextMapName = getNextMap();

		fullMapName = "maps/";
		fullMapName += nextMapName;
		fullMapName += ".bsp";

		if ( gi.FS_Exists( fullMapName ) )
		{
			gi.cvar_set( "nextmap", nextMapName.c_str() );
		}
		else
		{
			gi.Printf( "%s map not found\n", fullMapName.c_str() );
		}

		gi.cvar_set( "mp_currentPosInMapList", va( "%d", mp_currentPosInMapList->integer + 1 ) );
	
	}
}

str MultiplayerManager::getNextMap( void )
{
	str nextMapName;
	str mapList;
	int numMaps;
	const char *currentPlaceInMapList;
	int realCurrentPos;
	int i;
	str tempString;
	const char *nextSpace;
	int diff;


	mapList = mp_mapList->string;


	numMaps = 0;
	currentPlaceInMapList = mapList.c_str();

	while( 1 )
	{
		currentPlaceInMapList = strstr( currentPlaceInMapList, ";" );

		if ( currentPlaceInMapList )
			numMaps++;
		else
			break;

		currentPlaceInMapList++;
	}

	if ( mapList.length() && ( mapList[ mapList.length() - 1 ] != ';' ) )
		numMaps++;

	if ( numMaps == 0 )
		return "";


	realCurrentPos = mp_currentPosInMapList->integer % numMaps;


	currentPlaceInMapList = mapList.c_str();

	for ( i = 0 ; i < realCurrentPos ; i++ )
	{
		currentPlaceInMapList = strstr( currentPlaceInMapList, ";" );
		currentPlaceInMapList++;
	}

	nextMapName = currentPlaceInMapList;

	currentPlaceInMapList = nextMapName.c_str();

	currentPlaceInMapList = strstr( currentPlaceInMapList, ";" );

	if ( currentPlaceInMapList )
	{
		nextMapName.CapLength( currentPlaceInMapList - nextMapName.c_str() );
	}


	while ( ( nextMapName.length() > 0 ) && ( nextMapName[ 0 ] == ' ' ) )
	{
		tempString = nextMapName.c_str() + 1;
		nextMapName = tempString;
	}


	nextSpace = strstr( nextMapName.c_str(), " " );

	if ( nextSpace )
	{
		diff = nextSpace - nextMapName.c_str();
		nextMapName.CapLength( diff );
	}

	return nextMapName;
}

float MultiplayerManager::getRespawnTime( void )
{
	return _respawnTime;
}

void MultiplayerManager::setRespawnTime( float time )
{
	_respawnTime = time;
}

void MultiplayerManager::resetRespawnTime( void )
{
	setRespawnTime( mp_respawnTime->value );
	mp_respawnTime->modified = false;
}

void MultiplayerManager::checkModifiedCvars( bool informPlayers )
{

	if ( informPlayers )
	{
		if ( hasFlagChanged( MP_FLAG_WEAPONS_STAY ) )
			checkCvar( mp_flags, "$$WeaponsStay$$", MP_CVAR_TYPE_BOOL, MP_FLAG_WEAPONS_STAY );
		if ( hasFlagChanged( MP_FLAG_NO_FALLING ) )
			checkCvar( mp_flags, "$$NoFalling$$", MP_CVAR_TYPE_BOOL, MP_FLAG_NO_FALLING );
		if ( hasFlagChanged( MP_FLAG_FRIENDLY_FIRE ) )
			checkCvar( mp_flags, "$$AllowFriendlyFire$$", MP_CVAR_TYPE_BOOL, MP_FLAG_FRIENDLY_FIRE );
		if ( hasFlagChanged( MP_FLAG_FORCE_RESPAWN ) )
			checkCvar( mp_flags, "$$ForceRespawn$$", MP_CVAR_TYPE_BOOL, MP_FLAG_FORCE_RESPAWN );
		if ( hasFlagChanged( MP_FLAG_INFINITE_AMMO ) )
			checkCvar( mp_flags, "$$InfiniteAmmo$$", MP_CVAR_TYPE_BOOL, MP_FLAG_INFINITE_AMMO );
		if ( hasFlagChanged( MP_FLAG_FIXED_FOV ) )
			checkCvar( mp_flags, "$$FixedFOV$$", MP_CVAR_TYPE_BOOL, MP_FLAG_FIXED_FOV );
		if ( hasFlagChanged( MP_FLAG_NO_DROP_WEAPONS ) )
			checkCvar( mp_flags, "$$DontDropWeapons$$", MP_CVAR_TYPE_BOOL, MP_FLAG_NO_DROP_WEAPONS );
		if ( hasFlagChanged( MP_FLAG_NO_FOOTSTEPS ) )
			checkCvar( mp_flags, "$$NoFootstepSounds$$", MP_CVAR_TYPE_BOOL, MP_FLAG_NO_FOOTSTEPS );
		if ( hasFlagChanged( MP_FLAG_DONT_ALLOW_VOTE ) )
			checkCvar( mp_flags, "$$DontAllowVoting$$", MP_CVAR_TYPE_BOOL, MP_FLAG_DONT_ALLOW_VOTE );
		if ( hasFlagChanged( MP_FLAG_FULL_COLLISION ) )
			checkCvar( mp_flags, "$$FullCollision$$", MP_CVAR_TYPE_BOOL, MP_FLAG_FULL_COLLISION );

		checkCvar( mp_pointlimit, "$$PointLimit$$", MP_CVAR_TYPE_INTEGER );
		checkCvar( mp_timelimit, "$$TimeLimit$$", MP_CVAR_TYPE_INTEGER );
		
		checkCvar( mp_itemRespawnMultiplier, "$$ItemRespawnMultiplier$$", MP_CVAR_TYPE_FLOAT );
		checkCvar( mp_weaponRespawnMultiplier, "$$WeaponRespawnMultiplier$$", MP_CVAR_TYPE_FLOAT );
		checkCvar( mp_powerupRespawnMultiplier, "$$PowerupRespawnMultiplier$$", MP_CVAR_TYPE_FLOAT );
		checkCvar( mp_knockbackMultiplier, "$$KnocbackMultiplier$$", MP_CVAR_TYPE_INTEGER );
		checkCvar( mp_damageMultiplier, "$$DamageMultiplier$$", MP_CVAR_TYPE_FLOAT );
		checkCvar( mp_respawnInvincibilityTime, "$$RespawnInvincibilityTime$$", MP_CVAR_TYPE_FLOAT );

		checkCvar( mp_warmUpTime, "$$WarmUpTime$$", MP_CVAR_TYPE_INTEGER );

		checkCvar( sv_maxspeed, "$$PlayerSpeed$$", MP_CVAR_TYPE_INTEGER );

		checkCvar( mp_bigGunMode, "$$OptionBigGunMode$$", MP_CVAR_TYPE_INTEGER );

		checkCvar( mp_respawnTime, "$$OptionRespawnTime$$", MP_CVAR_TYPE_INTEGER );
		checkCvar( mp_bombTime, "$$OptionBombTime$$", MP_CVAR_TYPE_INTEGER );
		checkCvar( mp_maxVotes, "$$OptionMaxVotes$$", MP_CVAR_TYPE_INTEGER );
	}


	mp_flags->modified = false;
	mp_pointlimit->modified = false;
	mp_timelimit->modified = false;
	mp_itemRespawnMultiplier->modified = false;
	mp_weaponRespawnMultiplier->modified = false;
	mp_powerupRespawnMultiplier->modified = false;
	mp_knockbackMultiplier->modified = false;
	mp_damageMultiplier->modified = false;
	mp_respawnInvincibilityTime->modified = false;
	mp_warmUpTime->modified = false;
	sv_maxspeed->modified = false;
	mp_bigGunMode->modified = false;
	mp_respawnTime->modified = false;
	mp_bombTime->modified = false;
	mp_maxVotes->modified = false;


	_oldFlags = mp_flags->integer;
}


void MultiplayerManager::checkCvar( cvar_t *mp_cvarToCheck, str optionName, MPCvarType cvarType, int bitToCheck )
{
	str stringToPrint;

	if ( mp_cvarToCheck->modified )
	{
		stringToPrint = "$$ServerOptionChanged$$ : ";
		stringToPrint += optionName;
		stringToPrint += " ";

		if ( cvarType == MP_CVAR_TYPE_INTEGER )
		{
			stringToPrint += mp_cvarToCheck->integer;
		}
		else if ( cvarType == MP_CVAR_TYPE_FLOAT )
		{
			stringToPrint += mp_cvarToCheck->value;
		}
		else if ( cvarType == MP_CVAR_TYPE_BOOL )
		{
			int value;

			if ( bitToCheck >= 0 )
			{
				value = mp_cvarToCheck->integer & bitToCheck;
			}
			else
			{
				value = mp_cvarToCheck->integer;
			}

			if ( value == 0 )
				stringToPrint += "$$Off$$";
			else
				stringToPrint += "$$On$$";
		}

		stringToPrint += "\n";

		multiplayerManager.HUDPrintAllClients( stringToPrint );
	}
}

bool MultiplayerManager::hasFlagChanged( int bitToCheck )
{
	if ( ( _oldFlags & bitToCheck ) != ( mp_flags->integer & bitToCheck ) )
		return true;
	else
		return false;
}

void MultiplayerManager::addSoundToQueue( Player *player, str soundName, int channel, float volume, float minDist, float time )
{
	MultiplayerDialogData * dialogData;
	int nextDialogAddSpot;
	MultiplayerPlayerData * playerData;


	playerData = &_playerData[ player->entnum ];

	nextDialogAddSpot = playerData->getNextDialogAddSpot();

	dialogData = &playerData->_dialogData[ nextDialogAddSpot ];


	dialogData->_soundName = soundName;
	dialogData->_channel   = channel;
	dialogData->_volume    = volume;
	dialogData->_minDist   = minDist;
	dialogData->_time      = time;


	playerData->_nextDialogAddSpot++;
}

void MultiplayerManager::sendNextPlayerSound( Player *player )
{
	MultiplayerDialogData * dialogData;
	MultiplayerPlayerData * playerData;
	int nextDialogSendSpot;

	playerData = &_playerData[ player->entnum ];


	if ( playerData->_nextDialogSendTime > multiplayerManager.getTime() )
		return;


	if ( playerData->_nextDialogSendSpot >= playerData->_nextDialogAddSpot )
		return;


	nextDialogSendSpot = playerData->getNextDialogSendSpot();

	dialogData = &playerData->_dialogData[ nextDialogSendSpot ];

	player->Sound( dialogData->_soundName, dialogData->_channel, dialogData->_volume, dialogData->_minDist, NULL, 1.0f, true );


	playerData->_nextDialogSendSpot++;


	playerData->_nextDialogSendTime = multiplayerManager.getTime() + dialogData->_time;
}

bool MultiplayerManager::skipWeaponReloads( void )
{
	if ( !_inMultiplayerGame )
		return false;

	if ( mp_skipWeaponReloads->integer )
		return true;

	if ( _multiplayerGame->skipWeaponReloads() )
		return true;

	for ( int i = 1 ; i <= _modifiers.NumObjects() ; i++ )
	{
		MultiplayerModifier *modifier;

		modifier = _modifiers.ObjectAt( i );

		if ( modifier )
		{
			if ( modifier->skipWeaponReloads() )
				return true;
		}
	}

	return false;
}

void MultiplayerManager::playerTaunt( Player *player, const str &tauntName )
{
	if ( !_inMultiplayerGame || !player )
		return;

	if ( _playerData[ player->entnum ]._spectator )
		return;

	if ( _playerData[ player->entnum ]._nextTauntTime > level.time )
		return;

	player->Sound( tauntName, CHAN_TAUNT, DEFAULT_VOL, 250.0f, NULL, 1.0f, false );

	_playerData[ player->entnum ]._nextTauntTime = level.time + mp_minTauntTime->value;
}
