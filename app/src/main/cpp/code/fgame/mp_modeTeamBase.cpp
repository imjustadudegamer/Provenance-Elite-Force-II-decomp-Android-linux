//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/mp_modeTeamBase.cpp                        $
// $Revision:: 59                                                             $
//   $Author:: Singlis                                                        $
//     $Date:: 9/26/03 2:36p                                                  $
//
// Copyright (C) 2002 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// Description:
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "_pch_cpp.h"

#include "mp_manager.hpp"
#include "mp_modeBase.hpp"
#include "mp_modeTeamBase.hpp"


CLASS_DECLARATION( MultiplayerModeBase, ModeTeamBase, NULL )
{
	{ NULL, NULL }
};



ModeTeamBase::ModeTeamBase()
{
	_maxTeams = 2;

	_leadTeam = NULL;

	_useTeamSpawnpoints = false;

	_redTeamIconIndex	= gi.imageindex( "sysimg/icons/mp/team_red" );
	_blueTeamIconIndex	= gi.imageindex( "sysimg/icons/mp/team_blue" );

	_redTeamHudIconIndex	= gi.imageindex( "sysimg/icons/mp/team_red_hud" );
	_blueTeamHudIconIndex	= gi.imageindex( "sysimg/icons/mp/team_blue_hud" );

	_redTeamSpectatorHudIconIndex	= gi.imageindex( "sysimg/icons/mp/team_red_spectator" );
	_blueTeamSpectatorHudIconIndex	= gi.imageindex( "sysimg/icons/mp/team_blue_spectator" );
}



ModeTeamBase::~ModeTeamBase()
{
	_deleteTeams();
}

void ModeTeamBase::init( int maxPlayers )
{
	MultiplayerModeBase::init( maxPlayers );

	multiplayerManager.cacheMultiplayerFiles( "mp_teamBase" );
}

bool ModeTeamBase::isEndOfMatch( void )
{
	if (!getPointLimit())
	{
		return false ;
	}

	for (int idx=1; idx <= _teamList.NumObjects(); idx++) 
	{
		Team* team = _teamList.ObjectAt(idx);
		if (( int )team->getDeaths() > getPointLimit())
		{
			multiplayerManager.centerPrintAllClients(va("$$%s$$ $$TeamLoses$$\n", team->getName().c_str() ), CENTERPRINT_IMPORTANCE_NORMAL );

			if ( team->getName() == "Red" )
				multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_redtlose.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
			else
				multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_bltlose.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
		
			return true ;
		}
	}
	return false ;
}

void ModeTeamBase::AddPlayer( Player *player )
{	

	assert(player);

	if ( !player )
	{
		warning("ModeDeathmatch::AddPlayer", "NULL Player\n");
		return ; 
	}


	if ( !needToAddPlayer( player ) )
		return;

	MultiplayerModeBase::AddPlayer(player);	


	if ( playersLastTeam[ player->entnum ].length() )
	{
		addPlayerToTeam( player, getTeam( playersLastTeam[ player->entnum ] ) );
	}
	else if ( !multiplayerManager.checkFlag( MP_FLAG_NO_AUTO_JOIN_TEAM ) || multiplayerManager.checkFlag( MP_FLAG_AUTO_BALANCE_TEAMS ) || ( player->edict->svflags & SVF_BOT ) )
	{
		Team* pickedTeam   = 0 ;
		Team* team         = 0 ;
		int minimumPlayers = 0 ;


		for (int idx = 1; idx <= _teamList.NumObjects(); idx++)
		{
			team    = _teamList.ObjectAt(idx);
			
			assert(team);
			if (!team)
			{
				continue ;
			}
			
			int players = team->getActivePlayers();
			if ( (players < minimumPlayers) || (!pickedTeam) )
			{
				pickedTeam     = team ;
				minimumPlayers = players ;
			}
		}

		if (pickedTeam)
		{
			addPlayerToTeam( player, pickedTeam );
		}
	}
	else
	{

		addPlayerToTeam( player, NULL );
	}



	if ( !_gameStarted )
	{
		multiplayerManager.makePlayerSpectator( player );
	}
}

void ModeTeamBase::addPlayerToTeam( Player *player, Team *team )
{
	Team *oldTeam;
	Entity *spawnPoint;


	MultiplayerModeBase::AddPlayer(player);	

	oldTeam = _playerGameData[ player->entnum ]._currentTeam;

	if ( team && ( oldTeam != team ) )
	{

		multiplayerManager.HUDPrintAllClients(va("%s $$Joined$$ $$%s$$ $$Team$$.\n", player->client->pers.netname, team->getName().c_str() ) );
	}

	if ( oldTeam )
	{
		_playerGameData[ player->entnum ]._currentTeam = NULL;

		player->SurfaceCommand( "all", "-skin1" );
		player->SurfaceCommand( "all", "-skin2" );

		oldTeam->RemovePlayer(player);
	}

	if ( team )
	{

		if ( _gameStarted )
			multiplayerManager.playerEnterArena( player->entnum, player->health );
		else
			multiplayerManager.makePlayerSpectator( player );


		team->AddPlayer(player);

		_playerGameData[ player->entnum ]._currentTeam = team;

		updatePlayerSkin( player );

		if ( team->getName() == "Red" )
		{
			if ( multiplayerManager.isPlayerSpectator( player ) )
				multiplayerManager.setTeamHud( player, "mp_teamredspec" );
			else
				multiplayerManager.setTeamHud( player, "mp_teamred" );
		}
		else
		{
			if ( multiplayerManager.isPlayerSpectator( player ) )
				multiplayerManager.setTeamHud( player, "mp_teambluespec" );
			else
				multiplayerManager.setTeamHud( player, "mp_teamblue" );
		}

		multiplayerManager.playerSpawned( player );

		playersLastTeam[ player->entnum ] = team->getName();
	}
	else
	{

		multiplayerManager.makePlayerSpectator( player, SPECTATOR_TYPE_FOLLOW, true );

	}


	spawnPoint = getSpawnPoint( player );

	if ( spawnPoint )
	{
		player->WarpToPoint( spawnPoint );
	}

	if ( team && _gameStarted )
	{
		KillBox( player );

		ActivatePlayer( player );
	}
}

void ModeTeamBase::changeTeams( Player *player, Team *team )
{
	RemovePlayer( player );
	addPlayerToTeam( player, team );
}

void ModeTeamBase::respawnPlayer( Player *player )
{
	Entity *spawnPoint;

	if ( !player )
	{
		assert(player);
		warning("ModeDeathmatch::respawnPlayer", "NULL player\n");
		return;
	}


	if ( !multiplayerManager.checkRule( "respawnPlayer", true, player ) )
	{
		if ( multiplayerManager.checkRule( "allowSpectator", true ) )
		{
			multiplayerManager.makePlayerSpectator( player );
		}

		return;
	}

	MultiplayerModeBase::respawnPlayer( player );

	multiplayerManager.initPlayer( player );

	spawnPoint = getSpawnPoint( player );

	if ( spawnPoint )
	{
		player->WarpToPoint( spawnPoint );
	}

	KillBox( player );

	ActivatePlayer( player );
}

Entity *ModeTeamBase::getSpawnPoint( Player *player )
{
	Entity *spawnPoint = NULL;
	int i;
	int numSpawnPoints = 0;
	str spawnpointName;
	int randomStartingSpot;
	int spawnPointIndex;



	if ( multiplayerManager.checkRule( "spawnpoints-special", false, player ) )
	{
		spawnpointName = multiplayerManager.getSpawnPointType( player );

		numSpawnPoints = getNumNamedSpawnpoints( spawnpointName );
	}

	if ( !numSpawnPoints && multiplayerManager.checkRule( "spawnpoints-team", _useTeamSpawnpoints ) && _playerGameData[ player->entnum ]._currentTeam )
	{
		if ( stricmp( _playerGameData[ player->entnum ]._currentTeam->getName(), "red" ) == 0 )
			spawnpointName = "red";
		else
			spawnpointName = "blue";

		numSpawnPoints = getNumNamedSpawnpoints( spawnpointName );
	}

	if ( !numSpawnPoints )
	{
		spawnpointName = "";

		numSpawnPoints = getNumNamedSpawnpoints( spawnpointName );
	}

	randomStartingSpot = ( (int)( G_Random() * numSpawnPoints ) );

	for( i = 0 ; i < numSpawnPoints ; i++ )
	{
		spawnPointIndex = ( randomStartingSpot + i ) % numSpawnPoints;

		spawnPoint = getNamedSpawnpointbyIndex( spawnpointName, spawnPointIndex );


		if ( spawnPoint )
		{
			int j;
			int num;
			int touch[ MAX_GENTITIES ];
			gentity_t *hit;
			Vector min;	
			Vector max;
			bool badSpot;
	
			min = spawnPoint->origin + player->mins + Vector( 0, 0, 1 );
			max = spawnPoint->origin + player->maxs + Vector( 0, 0, 1 );
	
			num = gi.AreaEntities(min, max, touch, MAX_GENTITIES, qfalse);

			badSpot = false;
	
			for( j = 0 ; j < num ; j++ )
			{
				hit = &g_entities[ touch[ j ] ];
		
				if ( !hit->inuse || ( hit->entity == player ) || !hit->entity || ( hit->entity == world ) || ( !hit->entity->edict->solid ) )
				{
					continue;
				}

				if ( hit->entity->isSubclassOf( Player ) )
				{
					badSpot = true;
					break;
				}
			}

			if ( badSpot )
			{
				continue;
			}
		}


		return spawnPoint;
	}


	return spawnPoint;
}

Team* ModeTeamBase::AddTeam( const str& teamName )
{
	Team* team = _findTeamByName(teamName);

	if (team)
	{
		warning( "ModeTeamBase::AddTeam", "%s", va("Team %s already exists\n", teamName.c_str() ) );
		return NULL ;
	}

	team = new Team(teamName);
	team->setMaxPlayers(getMaxPlayers() / getMaxTeams());
	_teamList.AddObject( team );

	return team ;
}

Team *ModeTeamBase::getTeam( const str & teamName )
{
	int i;
	Team *team;

	for ( i = 1 ; i <= _teamList.NumObjects() ; i++ )
	{
		team = _teamList.ObjectAt( i );

		if ( team->getName() == teamName )
		{
			return team;
		}
	}

	return NULL;
}


void ModeTeamBase::RemoveTeam( const str& teamName )
{
	Team* team = _findTeamByName(teamName);
	_deleteTeam(team);
}



void ModeTeamBase::AddTeamStartingAmmo( const str& teamName, const str& ammoName, int amount )
{
	Team* team = _findTeamByName(teamName);
	SimpleAmmoType ammoType(ammoName, amount);
	_addTeamStartingAmmo(team, ammoType);
}

void ModeTeamBase::AddTeamStartingWeapon( const str& teamName, const str& weaponName )
{
	Team* team = _findTeamByName(teamName);
	_addTeamStartingWeapon(team, weaponName);
}

void ModeTeamBase::SetTeamStartingHealth( const str &teamName, unsigned int startingHealth )
{
	Team* team = _findTeamByName(teamName);
	_setTeamStartingHealth(team, startingHealth);
}



void ModeTeamBase::_addTeamStartingAmmo( Team* team, const SimpleAmmoType &ammo )
{
	assert(team);
	if (!team)
	{
		warning("ModeTeamBase::_addTeamStartingAmmo", "NULL team passed\n");
		return ;
	}

	assert(ammo.type.length());
	if (!ammo.type.length())
	{
		warning("ModeTeamBase::_addTeamStartingAmmo", "No ammo type specified\n");
		return ;
	}

	assert(ammo.amount > 0);
	if (ammo.amount < 0)
	{
		warning("ModeTeamBase::_addTeamStartingAmmo", "Negative ammo amount specified\n");
		return ;
	}

	team->AddStartingAmmo(ammo);
}


void ModeTeamBase::_addTeamStartingWeapon( Team* team, const str& weaponName )
{
	assert(team);
	if (!team)
	{
		warning("ModeTeamBase::_addTeamStartingWeapon", "NULL team passed\n");
		return ;
	}

	assert(weaponName.length());
	if (!weaponName.length())
	{
		warning("ModeTeamBase::_addTeamStartingWeapon", "No weapon specified\n");
		return ;
	}

	team->AddStartingWeapon(weaponName);
}


void ModeTeamBase::_setTeamStartingHealth( Team* team, unsigned int startingHealth )
{
	assert(team);
	if (!team)
	{
		warning("ModeTeamBase::_addTeamStartingWeapon", "NULL team passed\n");
		return ;
	}


	team->setStartingHealth(startingHealth);
}

void ModeTeamBase::_deleteTeams()
{
	for (int idx = 1; idx <= _teamList.NumObjects(); idx++)
	{
		Team* team = _teamList.ObjectAt(idx);
		_deleteTeam(team);
	}
}

void ModeTeamBase::_deleteTeam( Team* team )
{
	assert(team);
	if (!team)
	{
		warning("ModeTeamBase::_removeTeam", "NULL Team passed\n");
		return ;
	}

	_teamList.RemoveObject(team);
	delete team ;
}



void ModeTeamBase::_beginMatch()
{
	for (int idx = 1; idx <= _teamList.NumObjects(); idx++)
	{
		Team* team = _teamList.ObjectAt(idx);
		team->BeginMatch();
	}
}


void ModeTeamBase::_endMatch()
{
	for (int idx = 1; idx <= _teamList.NumObjects(); idx++)
	{
		Team* team = _teamList.ObjectAt(idx);
		team->EndMatch();
	}
}

void ModeTeamBase::declareWinner( void )
{
	int redPoints;
	int bluePoints;

	redPoints  = getTeamPoints( "Red" );
	bluePoints = getTeamPoints( "Blue" );

	if ( redPoints > bluePoints )
	{
		multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_rtwins.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
	}
	else if ( bluePoints > redPoints )
	{
		multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_btwins.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
	}
}


Team* ModeTeamBase::_findTeamByName( const str& teamName )
{
	assert(teamName.length());
	if (!teamName.length())
	{
		warning("ModeTeamBase::_findTeamByName", "Team name not specified\n");
		return NULL ;
	}

	Team* team = NULL ;
	for (int idx = 1; idx <= _teamList.NumObjects(); idx++)
	{
		team = _teamList.ObjectAt(idx);
		if ( stricmp( team->getName().c_str(), teamName.c_str() ) == 0 )
		{
			return team ;
		}
	}

	return NULL ;
}

int ModeTeamBase::getIcon( Player *player, int statNum, int value )
{
	 if ( statNum == STAT_MP_TEAM_ICON )
	{
		Team *team;

		team = _playerGameData[ player->entnum ]._currentTeam;

		if ( team )
		{

			if ( team->getName() == "Red" )
				return _redTeamIconIndex;
			else if ( team->getName() == "Blue" )
				return _blueTeamIconIndex;
		}
	}
	else if ( statNum == STAT_MP_OTHERTEAM_ICON )
	{
		Team *team;

		team = _playerGameData[ player->entnum ]._currentTeam;

		if ( team )
		{

			if ( team->getName() == "Red" )
				return _blueTeamIconIndex;
			else if ( team->getName() == "Blue" )
				return _redTeamIconIndex;
		}
	}


	return MultiplayerModeBase::getIcon( player, statNum, value );
}

int ModeTeamBase::getInfoIcon( Player *player )
{
	Team *team;


	if ( player->_affectingViewModes & gi.GetViewModeMask( "forcevisible" ) )
		return 0;

	team = _playerGameData[ player->entnum ]._currentTeam;

	if ( team )
	{
		if ( team->getName() == "Red" )
			return _redTeamIconIndex;
		else if ( team->getName() == "Blue" )
			return _blueTeamIconIndex;
	}

	return 0;
}

bool ModeTeamBase::canJoinTeam( Player *player, const str &teamName )
{
	Team *team;

	if ( multiplayerManager.checkFlag( MP_FLAG_AUTO_BALANCE_TEAMS ) )
	{
		Team *team_shadow1;
		Team *redTeam;
		Team *blueTeam;
		int redTeamPlayers;
		int blueTeamPlayers;


		team_shadow1 = getTeam( teamName );
		redTeam = getTeam( "Red" );
		blueTeam = getTeam( "Blue" );



		redTeamPlayers  = redTeam->getActivePlayers();
		blueTeamPlayers = blueTeam->getActivePlayers();

		if ( _playerGameData[ player->entnum ]._currentTeam == redTeam )
			redTeamPlayers -= 1;
		else if ( _playerGameData[ player->entnum ]._currentTeam == blueTeam )
			blueTeamPlayers -= 1;


		if ( ( redTeamPlayers > blueTeamPlayers ) && ( team_shadow1 == redTeam ) )
		{
			multiplayerManager.centerPrint( player->entnum, "$$CannotJoinAutoBalanceRed$$", CENTERPRINT_IMPORTANCE_HIGH );
			return false;
		}
		else if ( ( blueTeamPlayers > redTeamPlayers ) && ( team_shadow1 == blueTeam ) )
		{
			multiplayerManager.centerPrint( player->entnum, "$$CannotJoinAutoBalanceBlue$$", CENTERPRINT_IMPORTANCE_HIGH );
			return false;
		}
	}

	if ( !multiplayerManager.checkRule( "respawnPlayer", true, player ) )
	{
		return false;
	}

	team = _findTeamByName( teamName );

	if ( _playerGameData[ player->entnum ]._currentTeam == team )
		return false;
	else
		return true;
}

void ModeTeamBase::joinTeam( Player *player, const str &teamName )
{
	Team *team;

	team = _findTeamByName( teamName );

	changeTeams( player, team );
}

float ModeTeamBase::playerDamaged( Player *damagedPlayer, Player *attackingPlayer, float damage, int meansOfDeath )
{

	if ( meansOfDeath == MOD_TELEFRAG )
		return damage;


	if ( damagedPlayer == attackingPlayer )
		return damage;


	if ( ( _playerGameData[ damagedPlayer->entnum ]._currentTeam == _playerGameData[ attackingPlayer->entnum ]._currentTeam ) &&
		 ( !multiplayerManager.checkFlag( MP_FLAG_FRIENDLY_FIRE ) ) )
		return 0;
	else
		return damage;
}

int ModeTeamBase::getTeamPoints( Player *player )
{
	Team *team;


	team = _playerGameData[ player->entnum ]._currentTeam;

	if ( team )
		return team->getPoints();
	else
		return 0;
}

int ModeTeamBase::getTeamPoints( const str & teamName )
{
	int i;
	Team *team;


	for ( i = 1 ; i <= _teamList.NumObjects() ; i++ )
	{
		team = _teamList.ObjectAt( i );

		if ( team->getName() == teamName )
		{
			return team->getPoints();
		}
	}

	return 0;
}

void ModeTeamBase::addTeamPoints( const str & teamName, int points )
{
	int i;
	Team *team;


	for ( i = 1 ; i <= _teamList.NumObjects() ; i++ )
	{
		team = _teamList.ObjectAt( i );

		if ( team->getName() == teamName )
		{
			team->addPoints( NULL, points );
		}
	}
}

void ModeTeamBase::teamPointsChanged( Team *team, int oldPoints, int newPoints )
{
	Team *otherTeam;
	int otherTeamPoints;

	if ( team->getName() == "Red" )
		otherTeam = getTeam( "Blue" );
	else
		otherTeam = getTeam( "Red" );


	otherTeamPoints = otherTeam->getPoints();


	if ( ( newPoints == otherTeamPoints ) && ( oldPoints != otherTeamPoints ) )
	{
		multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_teamstied.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.0f );
	}
	else if ( ( newPoints > otherTeamPoints ) && ( oldPoints <= otherTeamPoints ) )
	{
		multiplayerManager.teamSound( team, "localization/sound/dialog/dm/comp_team1stplace.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, 1.0f );

		multiplayerManager.teamSound( otherTeam, "localization/sound/dialog/dm/comp_teamnolead.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, 1.0f );
	}
	else if ( ( newPoints < otherTeamPoints ) && ( oldPoints >= otherTeamPoints ) )
	{
		multiplayerManager.teamSound( team, "localization/sound/dialog/dm/comp_teamnolead.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, 1.0f );

		multiplayerManager.teamSound( otherTeam, "localization/sound/dialog/dm/comp_team1stplace.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, 1.0f );
	}
}

void ModeTeamBase::RemovePlayer( Player *player )
{
	Team *team;

	team = _playerGameData[ player->entnum ]._currentTeam;

	if ( team )
	{
		team->RemovePlayer( player );
	}

	MultiplayerModeBase::RemovePlayer( player );
}

int ModeTeamBase::getHighestPoints( void )
{
	int i;
	int highestPoints = -999999999;
	Team *team;

	for ( i = 1 ; i <= _teamList.NumObjects() ; i++ )
	{
		team = _teamList.ObjectAt( i );

		if ( team->getPoints() > highestPoints )
		{
			highestPoints = team->getPoints();
		}
	}

	return highestPoints;
}

void ModeTeamBase::updatePlayerSkin( Player *player )
{
	Team *team;

	team = getPlayersTeam( player );

	player->SurfaceCommand( "all", "-skin1" );
	player->SurfaceCommand( "all", "-skin2" );

	if ( !team )
		return;

	if ( team->getName() == "Red" )
	{
		player->SurfaceCommand( "all", "+skin1" );
	}
	else
	{
		player->SurfaceCommand( "all", "+skin2" );
	}
}

void ModeTeamBase::playerChangedModel( Player *player )
{
	updatePlayerSkin( player );
}

