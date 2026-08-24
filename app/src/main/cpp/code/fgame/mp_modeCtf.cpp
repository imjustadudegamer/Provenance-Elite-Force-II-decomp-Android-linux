//------------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/mp_modeCtf.cpp                              $
// $Revision:: 60                                                              $
//   $Author:: Singlis                                                         $
//     $Date:: 9/26/03 2:36p                                                   $
//
// Copyright (C) 2002 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// Description:
//    Implementation of Team and TeamArena classes.  These classes provide
//    Team-based multiplayer support.
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "_pch_cpp.h"
#include "mp_modeCtf.hpp"
#include "mp_manager.hpp"




const float	ModeCaptureTheFlag::_maxGuardingDist = 1000.0f;
const float	ModeCaptureTheFlag::_ctfCapturePoints = 100.0f;
const float	ModeCaptureTheFlag::_maxDroppedFlagTime = 15.0f;

const int ModeCaptureTheFlag::_pointsForFlagReturn			=  5;
const int ModeCaptureTheFlag::_pointsForKillingFlagCarrier	= 20;
const int ModeCaptureTheFlag::_pointsForAssist				= 15;
const int ModeCaptureTheFlag::_pointsForDefense				= 15;
const int ModeCaptureTheFlag::_pointsForTakingTheFlag		= 15;
const int ModeCaptureTheFlag::_pointsForCapturingTheFlag	= 85;


CLASS_DECLARATION( ModeTeamBase, ModeCaptureTheFlag, NULL )
{
	{ NULL, NULL }
};


ModeCaptureTheFlag::ModeCaptureTheFlag()
{
	_redTeam  = AddTeam("Red");
	_blueTeam = AddTeam("Blue");

	_playerCtfData = NULL;

	_useTeamSpawnpoints = true;

	_flagCarrierIconIndex = gi.imageindex( "sysimg/icons/mp/ctf_flagcarrier" );

	_oneFlagTakenIconIndex   = gi.imageindex( "sysimg/icons/mp/ctf_oneFlagTaken" );
	_oneFlagMissingIconIndex = gi.imageindex( "sysimg/icons/mp/ctf_oneFlagMissing" );
	_oneFlagInBaseIconIndex  = gi.imageindex( "sysimg/icons/mp/ctf_oneFlagInBase" );

	_redFlagTakenIconIndex   = gi.imageindex( "sysimg/icons/mp/ctf_redFlagTaken" );
	_redFlagMissingIconIndex = gi.imageindex( "sysimg/icons/mp/ctf_redFlagMissing" );
	_redFlagInBaseIconIndex  = gi.imageindex( "sysimg/icons/mp/ctf_redFlagInBase" );

	_blueFlagTakenIconIndex   = gi.imageindex( "sysimg/icons/mp/ctf_blueFlagTaken" );
	_blueFlagMissingIconIndex = gi.imageindex( "sysimg/icons/mp/ctf_blueFlagMissing" );
	_blueFlagInBaseIconIndex  = gi.imageindex( "sysimg/icons/mp/ctf_blueFlagInBase" );
}


ModeCaptureTheFlag::~ModeCaptureTheFlag()
{
	delete [] _playerCtfData;
	_playerCtfData = NULL;

	_flags.FreeObjectList();
}

void ModeCaptureTheFlag::init( int maxPlayers )
{

	ModeTeamBase::init( maxPlayers );

	_playerCtfData = new MultiplayerPlayerCtfData[ _maxPlayers ];

	_flags.FreeObjectList();


	readMultiplayerConfig( "global/mp_ctf.cfg" );

	multiplayerManager.cacheMultiplayerFiles( "mp_ctf" );
}

bool ModeCaptureTheFlag::shouldKeepItem( MultiplayerItem *item )
{

	if ( strnicmp( item->getName().c_str(), "ctfflag", 7 ) == 0 )
	{
		if ( !multiplayerManager.checkRule ( "keepflags", true ) )
			return false;

		if ( ( stricmp( item->getName().c_str() + 8, "red" ) == 0 ) ||
			 ( stricmp( item->getName().c_str() + 8, "blue" ) == 0 ) || 
			 ( stricmp( item->getName().c_str() + 8, "base" ) == 0 ) )
		{

			return true;
		}
	}

	return false;
}

void ModeCaptureTheFlag::itemKept( MultiplayerItem *item )
{

	if ( strnicmp( item->getName().c_str(), "ctfflag", 7 ) == 0 )
	{
		CtfFlag ctfFlag;


		ctfFlag._realFlag = item;
		ctfFlag._tempFlag = NULL;

		if ( strnicmp( item->getName().c_str() + 8, "red", 3 ) == 0 )
		{
			ctfFlag._teamName = "Red";
		}
		else if ( strnicmp( item->getName().c_str() + 8, "blue", 4 ) == 0 )
		{
			ctfFlag._teamName = "Blue";
		}

		_flags.AddObject ( ctfFlag );
	}
}

void ModeCaptureTheFlag::addPlayerToTeam( Player *player, Team *team )
{
	ModeTeamBase::addPlayerToTeam( player, team );

	_playerCtfData[ player->entnum ].init();
}

void ModeCaptureTheFlag::RemovePlayer( Player *player )
{
	ModeTeamBase::RemovePlayer( player );

	putFlagBack( player );
}

void ModeCaptureTheFlag::setupMultiplayerUI( Player *player )
{

	gi.SendServerCommand( player->entnum, "stufftext \"ui_removehuds all\"\n" );
	gi.SendServerCommand( player->entnum, "stufftext \"ui_addhud mp_console\"\n" );
	gi.SendServerCommand( player->entnum, "stufftext \"ui_addhud mp_teamhud\"\n" );

	if ( multiplayerManager.checkRule ( "flagpickup-enemyflag", true ) )
	{
		gi.SendServerCommand( player->entnum, "stufftext \"ui_addhud mp_flagstatus\"\n" );
	}
	
	if(mp_timelimit->integer)
	{
		gi.SendServerCommand( player->entnum, "stufftext \"globalwidgetcommand dmTimer enable\"\n");
	}
	else
	{
		gi.SendServerCommand( player->entnum, "stufftext \"globalwidgetcommand dmTimer disable\"\n");
	}
}

void ModeCaptureTheFlag::playerKilled( Player *killedPlayer, Player *attackingPlayer, Entity *inflictor, int meansOfDeath )
{
	Team *team;
	float distance;
	str printString;
	bool goodKill;

	if ( attackingPlayer && ( killedPlayer != attackingPlayer ) && 
		 ( _playerGameData[ attackingPlayer->entnum ]._currentTeam != _playerGameData[ killedPlayer->entnum ]._currentTeam ) )
		goodKill = true;
	else
		goodKill = false;

	handleKill( killedPlayer, attackingPlayer, inflictor, meansOfDeath, goodKill );

	if ( !attackingPlayer )
		attackingPlayer = killedPlayer;

	team = _playerGameData[ attackingPlayer->entnum ]._currentTeam;



	if ( _playerCtfData[ killedPlayer->entnum ]._hasFlag )
	{
		if ( _playerGameData[ killedPlayer->entnum ]._currentTeam != _playerGameData[ attackingPlayer->entnum ]._currentTeam )
		{
			CtfFlag *ctfFlag;
			str printString_shadow1;

			printString_shadow1 = "$$FlagCarrierKilled$$ ";
			printString_shadow1 += attackingPlayer->client->pers.netname;
			printString_shadow1 += " ($$";
			printString_shadow1 += _playerGameData[ attackingPlayer->entnum ]._currentTeam->getName();
			printString_shadow1 += "$$ $$Team$$)!";

			multiplayerManager.centerPrintAllClients( printString_shadow1, CENTERPRINT_IMPORTANCE_NORMAL );

			ctfFlag = findFlag( _playerCtfData[ killedPlayer->entnum ]._flag );

			if ( ctfFlag )
			{
				if ( ctfFlag->_teamName == "Red" )
					multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_rfk.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
				else if ( ctfFlag->_teamName == "Blue" )
					multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_bfk.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
				else
					multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_flagcarrierkilled.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
			}


			multiplayerManager.addPoints( attackingPlayer->entnum, _pointsForKillingFlagCarrier );
		}

		dropFlag( killedPlayer );
	}


	if ( _playerGameData[ killedPlayer->entnum ]._currentTeam != _playerGameData[ attackingPlayer->entnum ]._currentTeam )
	{
		bool flagGuarded;

		distance = findNearestTeamFlagDist( team->getName(), attackingPlayer->origin );

		flagGuarded = false;

		if ( ( distance > 0 ) && ( distance < _maxGuardingDist ) )
		{
			flagGuarded = true;
		}
		else
		{
			distance = findNearestTeamFlagDist( team->getName(), killedPlayer->origin );

			if ( ( distance > 0 ) && ( distance < _maxGuardingDist ) )
			{
				flagGuarded = true;
			}
		}

		if ( flagGuarded )
		{
			multiplayerManager.playerEventNotification( "flag-guarded", "", attackingPlayer );
			
			printString = "$$FlagGuarded$$ ";
			printString += attackingPlayer->client->pers.netname;

			multiplayerManager.playerSound( attackingPlayer->entnum, "localization/sound/dialog/dm/comp_flagguarded.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, 1.0f );

			multiplayerManager.centerPrintTeamClients( attackingPlayer, printString, CENTERPRINT_IMPORTANCE_HIGH );


			multiplayerManager.addPoints( attackingPlayer->entnum, _pointsForDefense );
		}
	}


	if ( ( _playerGameData[ killedPlayer->entnum ]._currentTeam != _playerGameData[ attackingPlayer->entnum ]._currentTeam ) &&
		 !_playerCtfData[ attackingPlayer->entnum ]._hasFlag )
	{
		bool flagCarrierGuarded;

		distance = findNearestTeamFlagCarrierDist( team->getName(), attackingPlayer->origin );

		flagCarrierGuarded = false;

		if ( ( distance > 0 ) && ( distance < _maxGuardingDist ) )
		{
			flagCarrierGuarded = true;
		}
		else
		{
			distance = findNearestTeamFlagCarrierDist( team->getName(), killedPlayer->origin );

			if ( ( distance > 0 ) && ( distance < _maxGuardingDist ) )
			{
				flagCarrierGuarded = true;
			}
		}

		if ( flagCarrierGuarded )
		{
			
			printString = "$$FlagAssist$$ ";
			printString += attackingPlayer->client->pers.netname;

			multiplayerManager.playerSound( attackingPlayer->entnum, "localization/sound/dialog/dm/comp_flagassist.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, 1.0f );

			multiplayerManager.centerPrintTeamClients( attackingPlayer, printString, CENTERPRINT_IMPORTANCE_HIGH );


			multiplayerManager.addPoints( attackingPlayer->entnum, _pointsForAssist );
		}
	}
}

CtfFlag *ModeCaptureTheFlag::findFlag( MultiplayerItem *item )
{
	int i;
	CtfFlag *ctfFlag;

	for ( i = 1 ; i <= _flags.NumObjects() ; i++ )
	{
		ctfFlag = &_flags.ObjectAt( i );

		if ( ( ctfFlag->_realFlag == item ) || ( ctfFlag->_tempFlag == item ) )
			return ctfFlag;
	}

	return NULL;
}

float ModeCaptureTheFlag::findNearestTeamFlagDist( const str &teamName, const Vector &position )
{
	int i;
	CtfFlag *ctfFlag;
	CtfFlag *nearestCtfFlag = NULL;
	float nearestDistance = -1.0f;
	Vector diff;
	float distance;


	for ( i = 1 ; i <= _flags.NumObjects() ; i++ )
	{
		ctfFlag = &_flags.ObjectAt( i );


		if ( !ctfFlag->_realFlag )
			continue;

		if ( ctfFlag->_teamName != teamName )
			continue;


		diff = position - ctfFlag->_realFlag->origin;
		distance = diff.length();


		if ( !nearestCtfFlag || ( distance < nearestDistance ) )
		{
			nearestCtfFlag = ctfFlag;
			nearestDistance = distance;
		}
	}

	return nearestDistance;
}

float ModeCaptureTheFlag::findNearestTeamFlagCarrierDist( const str &teamName, const Vector &position )
{
	int i;
	Player *currentPlayer;
	Player *nearestFlagCarrier = NULL;
	float nearestDistance = -1.0f;
	Team *team;
	Vector diff;
	float distance;


	for ( i = 0 ; i < ( int )_maxPlayers ; i++ )
	{
		if ( !_playerGameData[ i ]._playing )
			continue;

		currentPlayer = (Player *)g_entities[ _playerGameData[ i ]._entnum ].entity;

		team = multiplayerManager.getPlayersTeam( currentPlayer );


		if ( !team )
			continue;

		if ( team->getName() != teamName )
			continue;

		if ( !_playerCtfData[ currentPlayer->entnum ]._hasFlag )
			continue;


		diff = position - currentPlayer->origin;
		distance = diff.length();


		if ( !nearestFlagCarrier || ( distance < nearestDistance ) )
		{
			nearestFlagCarrier = currentPlayer;
			nearestDistance = distance;
		}
	}

	return nearestDistance;
}

void ModeCaptureTheFlag::itemTouched( Player *player, MultiplayerItem *item )
{
	CtfFlag *ctfFlag;
	bool myFlag;
	bool teamFlag;
	bool tempFlag;


	ctfFlag = findFlag( item );

	if ( !ctfFlag )
		return;


	if ( !_playerGameData[ player->entnum ]._currentTeam )
		return;

	if ( ( ( ctfFlag->_teamName == "Red" ) && ( _playerGameData[ player->entnum ]._currentTeam == _redTeam ) ) ||
		 ( ( ctfFlag->_teamName == "Blue" ) && ( _playerGameData[ player->entnum ]._currentTeam == _blueTeam ) ) )
		myFlag = true;
	else
		myFlag = false;


	if ( ( ctfFlag->_teamName == "Red" ) || ( ctfFlag->_teamName == "Blue" ) )
		teamFlag = true;
	else
		teamFlag = false;


	if ( item == ctfFlag->_tempFlag )
		tempFlag = true;
	else
		tempFlag = false;


	if ( myFlag && teamFlag && tempFlag && multiplayerManager.checkRule ( "flagreturn-teamflag", true ) )
	{

		returnFlag( item, player );
	}
	else if ( _playerCtfData[ player->entnum ]._hasFlag )
	{

		if ( !tempFlag && myFlag && multiplayerManager.checkRule ( "flagscore-teamflag", true ) )
		{
			score( player );
		}
		else if ( !tempFlag && !myFlag && teamFlag && multiplayerManager.checkRule ( "flagscore-enemyflag", false ) )
		{
			score( player );
		}
		else if ( !tempFlag && !teamFlag )
		{
			str rule;

			rule = "flagscore-otherflag-";
			rule += item->getName();

			if ( multiplayerManager.checkRule ( rule.c_str(), false ) )
			{
				score( player );
			}
		}
	}
	else
	{

		if ( !myFlag && teamFlag && multiplayerManager.checkRule ( "flagpickup-enemyflag", true ) )
		{
			grabTheFlag( player, item );
		}
		else if ( myFlag && teamFlag && multiplayerManager.checkRule ( "flagpickup-teamflag", false ) )
		{
			grabTheFlag( player, item );
		}
		else if ( !teamFlag )
		{
			str rule;

			rule = "flagpickup-otherflag-";
			rule += ctfFlag->_realFlag->getName();

			if ( multiplayerManager.checkRule ( rule.c_str(), false ) )
			{
				grabTheFlag( player, item );
			}
		}
	}
}

void ModeCaptureTheFlag::score( Player *player )
{
	Team *team;
	str printString;
	CtfFlag *ctfFlag;

	ctfFlag = findFlag( _playerCtfData[ player->entnum ]._flag );

	if ( !ctfFlag )
		return;


	putFlagBack( player );



	team = _playerGameData[ player->entnum ]._currentTeam;


	printString = "$$FlagCaptured$$ ";
	printString += player->client->pers.netname;
	printString += " ($$";
	printString += team->getName();
	printString += "$$ $$Team$$)!";

	multiplayerManager.centerPrintAllClients( printString, CENTERPRINT_IMPORTANCE_NORMAL );

	if ( strnicmp( ctfFlag->_realFlag->getName().c_str(), "ctfflag-red", strlen( "ctfflag-red" ) ) == 0 )
		multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_frc.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
	else if ( strnicmp( ctfFlag->_realFlag->getName().c_str(), "ctfflag-blue", strlen( "ctfflag-blue" ) ) == 0 )
		multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_bfc.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
	else
	{
		if ( team->getName() == "Red" )
			multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_fcrt.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
		else
			multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_fcbt.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
	}


	multiplayerManager.playerEventNotification( "flag-captured", ctfFlag->_realFlag->getName().c_str(), player );


	team->addPoints( player,(int) _ctfCapturePoints );

	multiplayerManager.addPoints( player->entnum, _pointsForCapturingTheFlag );
}

void ModeCaptureTheFlag::dropFlag( Player *player )
{
	CtfFlag *ctfFlag;
	MultiplayerItem *newFlag;


	if ( !_playerCtfData[ player->entnum ]._hasFlag )
		return;

	ctfFlag = findFlag( _playerCtfData[ player->entnum ]._flag );

	if ( !ctfFlag )
		return;


	newFlag = new MultiplayerItem;

	newFlag->setModel( ctfFlag->_realFlag->model );

	newFlag->angles = player->angles;
	newFlag->setAngles();

	newFlag->setOrigin( player->origin );

	newFlag->CancelEventsOfType( EV_ProcessInitCommands );
	newFlag->ProcessInitCommands( newFlag->edict->s.modelindex );

	newFlag->setName( "ctfflag-temp" );


	ctfFlag->_tempFlag = newFlag;
	ctfFlag->_tempFlagTime = multiplayerManager.getTime();


	if ( _playerCtfData[ player->entnum ]._carriedFlag )
	{
		_playerCtfData[ player->entnum ]._carriedFlag->PostEvent( EV_Detach, 0.0f );
		_playerCtfData[ player->entnum ]._carriedFlag->PostEvent( EV_Remove, FRAMETIME );
		_playerCtfData[ player->entnum ]._carriedFlag = NULL;
	}

	_playerCtfData[ player->entnum ]._hasFlag = false;

	multiplayerManager.playerEventNotification( "flag-dropped", ctfFlag->_realFlag->getName().c_str(), player );
}

void ModeCaptureTheFlag::returnFlag( MultiplayerItem *item, Player *player )
{
	CtfFlag *ctfFlag;
	str printString;

	ctfFlag = findFlag( item );

	if ( !ctfFlag )
		return;


	if ( ctfFlag->_tempFlag )
	{
		ctfFlag->_tempFlag->PostEvent( EV_Remove, FRAMETIME );

		ctfFlag->_tempFlag = NULL;
	}


	if ( ctfFlag->_realFlag )
	{
		ctfFlag->_realFlag->showModel();
		ctfFlag->_realFlag->setSolidType( SOLID_TRIGGER );
	}

	if ( player )
	{

		printString = "$$FlagReturnedBy$$ ";
		printString += player->client->pers.netname;
		printString += " ($$";
		printString += _playerGameData[ player->entnum ]._currentTeam->getName();
		printString += "$$ $$Team$$)!";

		multiplayerManager.centerPrintAllClients( printString, CENTERPRINT_IMPORTANCE_NORMAL );

		if ( strnicmp( ctfFlag->_realFlag->getName().c_str(), "ctfflag-red", strlen( "ctfflag-red" ) ) == 0 )
			multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_rfr.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.2f );
		else if ( strnicmp( ctfFlag->_realFlag->getName().c_str(), "ctfflag-blue", strlen( "ctfflag-blue" ) ) == 0 )
			multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_bfr.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.2f );
		else
		{
			if ( _playerGameData[ player->entnum ]._currentTeam->getName() == "Red" )
				multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_frbrt.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.2f );
			else
				multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_frbbt.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.2f );
		}


		multiplayerManager.playerEventNotification( "flag-returned", ctfFlag->_realFlag->getName().c_str(), player );


		multiplayerManager.addPoints( player->entnum, _pointsForFlagReturn );
	}
	else
	{
		printString = "$$FlagReturned$$";

		multiplayerManager.centerPrintAllClients( printString, CENTERPRINT_IMPORTANCE_NORMAL );

		if ( strnicmp( ctfFlag->_realFlag->getName().c_str(), "ctfflag-red", strlen( "ctfflag-red" ) ) == 0 )
			multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_rfr.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.2f );
		else if ( strnicmp( ctfFlag->_realFlag->getName().c_str(), "ctfflag-blue", strlen( "ctfflag-blue" ) ) == 0 )
			multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_bfr.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.2f );
		
	}
}

void ModeCaptureTheFlag::putFlagBack( Player *player )
{
	CtfFlag *ctfFlag;
	MultiplayerItem *flag;


	if ( !_playerCtfData[ player->entnum ]._hasFlag )
		return;

	ctfFlag = findFlag( _playerCtfData[ player->entnum ]._flag );

	if ( !ctfFlag )
		return;

	flag = ctfFlag->_realFlag;


	if ( flag )
	{
		flag->showModel();
		flag->setSolidType( SOLID_TRIGGER );
	}


	if ( _playerCtfData[ player->entnum ]._carriedFlag )
	{
		_playerCtfData[ player->entnum ]._carriedFlag->PostEvent( EV_Detach, 0.0f );
		_playerCtfData[ player->entnum ]._carriedFlag->PostEvent( EV_Remove, FRAMETIME );
		_playerCtfData[ player->entnum ]._carriedFlag = NULL;
	}

	_playerCtfData[ player->entnum ]._hasFlag = false;
}

void ModeCaptureTheFlag::grabTheFlag( Player *player, MultiplayerItem *item )
{
	CtfFlag *ctfFlag;
	Entity *obj;
	str printString;
	Team *team;
	MultiplayerItem *flag;
	bool tempFlag;

	ctfFlag = findFlag( item );

	flag = ctfFlag->_realFlag;


	if ( ctfFlag->_tempFlag )
	{
		ctfFlag->_tempFlag->PostEvent( EV_Remove, FRAMETIME );

		ctfFlag->_tempFlag = NULL;

		tempFlag = true;
	}
	else
	{
		tempFlag = false;
	}


	obj = new Entity( ENTITY_CREATE_FLAG_ANIMATE );

	obj->setModel( flag->model );

	obj->animate->RandomAnimate( "idle" );

	attachFlag( player, obj );

	_playerCtfData[ player->entnum ]._hasFlag = true;
	_playerCtfData[ player->entnum ]._carriedFlag = obj;
	_playerCtfData[ player->entnum ]._flag = flag;


	flag->setSolidType( SOLID_NOT );
	flag->hideModel();


	if ( !tempFlag )
	{
		multiplayerManager.addPoints( player->entnum, _pointsForTakingTheFlag );
	}


	team = _playerGameData[ player->entnum ]._currentTeam;

	printString = "$$FlagTaken$$ ";
	printString += player->client->pers.netname;
	printString += " ($$";
	printString += team->getName();
	printString += "$$ $$Team$$)!";

	multiplayerManager.centerPrintAllClients( printString, CENTERPRINT_IMPORTANCE_NORMAL );

	if ( strnicmp( ctfFlag->_realFlag->getName().c_str(), "ctfflag-red", strlen( "ctfflag-red" ) ) == 0 )
		multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_rft.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.2f );
	else if ( strnicmp( ctfFlag->_realFlag->getName().c_str(), "ctfflag-blue", strlen( "ctfflag-blue" ) ) == 0 )
		multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_bft.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.2f );
	else
	{
		if ( team->getName() == "Red" )
			multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_ftbrt.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.2f );
		else
			multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_ftbbt.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.2f );
	}

	multiplayerManager.playerEventNotification( "flag-taken", ctfFlag->_realFlag->getName().c_str(), player );
}

void ModeCaptureTheFlag::update( float frameTime )
{
	CtfFlag *ctfFlag;
	int i;

	MultiplayerModeBase::update( frameTime );


	for ( i = 1 ; i <= _flags.NumObjects() ; i++ )
	{
		ctfFlag = &_flags.ObjectAt( i );

		if ( ( ctfFlag->_tempFlag ) && ( ctfFlag->_tempFlagTime + _maxDroppedFlagTime < multiplayerManager.getTime() ) )
		{
			returnFlag( ctfFlag->_tempFlag, NULL );
		}
	}
}

bool ModeCaptureTheFlag::isEndOfMatch( void )
{

	if ( getPointLimit() > 0 ) 
	{

		if ( _redTeam->getPoints() >= getPointLimit() )
			return true;
		else if ( _blueTeam->getPoints() >= getPointLimit() )
			return true;
	}


	if ( ( getTimeLimit() > 0.0f ) && ( multiplayerManager.getTime() - _gameStartTime > getTimeLimit() ) ) 
		return true;

	return false;
}

int ModeCaptureTheFlag::getIcon( Player *player, int statNum, int value )
{
	if ( statNum == STAT_MP_MODE_ICON )
	{
		if ( _playerCtfData[ player->entnum ]._hasFlag )
			return _flagCarrierIconIndex;
		else
			return -1;
	}

	return ModeTeamBase::getIcon( player, statNum, value );
}

int ModeCaptureTheFlag::getScoreIcon( Player *player, int index, int value )
{
	if ( index == SCOREICON1 )
	{
		if ( _playerCtfData[ player->entnum ]._hasFlag )
			return _flagCarrierIconIndex;
		else
			return 0;
	}

	return value;
}

void ModeCaptureTheFlag::score( const Player *player )
{
	char		   string[1400];
	char		   entry[1024];
	int            i;
	int            tempStringlength;
	int            count        = 0;
	int            stringlength = 0;
	Team			*team;
	str				teamName;
	Player			*currentPlayer;
	int				redScore;
	int				blueScore;

	assert( player );
	if ( !player )
	{
		warning( "MultiplayerModeBase::score", "Null Player specified.\n" );
		return;
	}

	string[0]  = 0;
	entry[0]   = 0;


	for ( i = 0 ; i < ( int )_maxPlayers ; i++ )
	{
		currentPlayer = multiplayerManager.getPlayer( i );

		if ( !currentPlayer )
			continue;

		team = multiplayerManager.getPlayersTeam( currentPlayer );

		if ( team )
		{
			teamName = team->getName();
		}
		else
		{
			teamName = "spectator";
		}


		Com_sprintf( entry, sizeof( entry ), "%i %i %i %i %s %i %i %d %d %d %d %d %d ", 
				multiplayerManager.getClientNum( _playerGameData[ i ]._entnum ),
				_playerGameData[ i ]._points,
				_playerGameData[ i ]._numKills, 
				_playerGameData[ i ]._numDeaths, 
				teamName.c_str(),
				(int)(multiplayerManager.getTime() - _playerGameData[i]._startTime), 
				multiplayerManager.getClientPing( _playerGameData[ i ]._entnum ),
				multiplayerManager.getScoreIcon( currentPlayer, SCOREICON1 ), 
				multiplayerManager.getScoreIcon( currentPlayer, SCOREICON2 ), 
				multiplayerManager.getScoreIcon( currentPlayer, SCOREICON3 ), 
				multiplayerManager.getScoreIcon( currentPlayer, SCOREICON4 ), 
				multiplayerManager.getScoreIcon( currentPlayer, SCOREICON5 ), 
				multiplayerManager.getScoreIcon( currentPlayer, SCOREICON6 ) );

		tempStringlength = strlen( entry );


		if ( stringlength + tempStringlength > 975 )
			break;

		strcpy( string + stringlength, entry );

		stringlength += tempStringlength;
		count++;
	}

	redScore  = multiplayerManager.getTeamPoints( "Red" );
	blueScore = multiplayerManager.getTeamPoints( "Blue" );

	gi.SendServerCommand( player->edict-g_entities, "scores 2 %i %d %d %s", count, redScore, blueScore, string );
}



bool ModeCaptureTheFlag::checkGameType( const char *gameType )
{
	if ( stricmp( gameType, "ctf" ) == 0 )
		return true;
	else
		return false;
}

bool ModeCaptureTheFlag::doesPlayerHaveItem( Player *player, const char *itemName )
{

	if ( strnicmp( itemName, "ctfflag", 7 ) == 0 )
	{

		if ( _playerCtfData[ player->entnum ]._hasFlag )
		{
			CtfFlag *ctfFlag;


			ctfFlag = findFlag( _playerCtfData[ player->entnum ]._flag );

			if ( !ctfFlag )
				return false;

			if ( ctfFlag->_realFlag )
			{

				if ( stricmp( ctfFlag->_realFlag->getName().c_str(), itemName ) == 0 )
				{
					return true;
				}
			}
		}
	}

	return false;
}

int ModeCaptureTheFlag::getStat( Player *player, int statNum, int value )
{
	CtfFlag *ctfFlag;
	int i;
	str flagName;
	bool oneFlag;
	int takenIconIndex;
	int missingIconIndex;
	int inBaseIconIndex;

	if ( ( statNum == STAT_MP_GENERIC5 ) || ( statNum == STAT_MP_GENERIC6 ) )
	{

		oneFlag = !multiplayerManager.checkRule ( "flagpickup-enemyflag", true );

		if ( oneFlag && statNum == STAT_MP_GENERIC6 )
			return value;

		if ( oneFlag )
		{
			flagName = "ctfflag-one";
			takenIconIndex   = _oneFlagTakenIconIndex;
			missingIconIndex = _oneFlagMissingIconIndex;
			inBaseIconIndex  = _oneFlagInBaseIconIndex;
		}
		else if ( statNum == STAT_MP_GENERIC5 )
		{
			flagName = "ctfflag-red";
			takenIconIndex   = _redFlagTakenIconIndex;
			missingIconIndex = _redFlagMissingIconIndex;
			inBaseIconIndex  = _redFlagInBaseIconIndex;
		}
		else
		{
			flagName = "ctfflag-blue";
			takenIconIndex   = _blueFlagTakenIconIndex;
			missingIconIndex = _blueFlagMissingIconIndex;
			inBaseIconIndex  = _blueFlagInBaseIconIndex;
		}

		for ( i = 1 ; i <= _flags.NumObjects() ; i++ )
		{
			ctfFlag = &_flags.ObjectAt( i );

			if ( strnicmp( ctfFlag->_realFlag->getName().c_str(), flagName.c_str(), flagName.length() ) == 0 )
			{
				if ( ctfFlag->_tempFlag )
				{
					return missingIconIndex;
				}
				else if ( ctfFlag->_realFlag->getSolidType() == SOLID_NOT )
				{
					return takenIconIndex;
				}
				else
				{
					return inBaseIconIndex;
				}
			}
		}
	}
	else if ( statNum == STAT_MP_STATE )
	{
		return MultiplayerModeBase::getStat( player, statNum, value );
	}

	return value;
}

void ModeCaptureTheFlag::playerEventNotification( const char *eventName, const char *eventItemName, Player *eventPlayer )
{
	if ( stricmp( eventName, "use-HoldableItem" ) == 0 )
	{
		if ( stricmp( eventItemName, "Transporter" ) == 0 )
		{
			dropFlag( eventPlayer );
		}
	}
}

void ModeCaptureTheFlag::playerChangedModel( Player *player )
{
	Entity *flag;

	if ( _playerCtfData[ player->entnum ]._hasFlag && _playerCtfData[ player->entnum ]._carriedFlag )
	{
		flag = _playerCtfData[ player->entnum ]._carriedFlag;

		flag->detach();

		attachFlag( player, flag );
	}

	updatePlayerSkin( player );
}

void ModeCaptureTheFlag::attachFlag( Player *player, Entity *obj )
{
	int tagnum;

	tagnum = gi.Tag_NumForName( player->edict->s.modelindex, "Bip01 spine2" );

	if ( tagnum < 0 )
		tagnum = gi.Tag_NumForName( player->edict->s.modelindex, "Bip01 spine1" );

	if ( tagnum < 0 )
		tagnum = gi.Tag_NumForName( player->edict->s.modelindex, "Bip01 Head" );

	if ( ( tagnum < 0 ) || !obj->attach( player->entnum, tagnum, false, player->getFlagAttachOffset(), player->getFlagAttachAngles() ) )
	{
		warning( "ModeCaptureTheFlag::attachFlag", "Could not attach model %s", obj->model.c_str() );
		delete obj;
		return;
	}
}

