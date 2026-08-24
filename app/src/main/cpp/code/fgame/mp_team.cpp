//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/mp_team.cpp                                    $
// $Revision:: 8                                                              $
//   $Author:: Steven                                                         $
//     $Date:: 5/20/03 11:39a                                                 $
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
#include "mp_team.hpp"


CLASS_DECLARATION( Class, Team, NULL )
{
	{ NULL, NULL }
};

Team::Team( const str& name ) :	
	_name(name)
{
	_startingHealth = 0;
	_maxPlayers = 5;
	_wins = 0;
	_losses = 0;
	_kills = 0;
	_deaths = 0;
	_points = 0;
	_isFighting = false;
}


Team::~Team()
{
}

void Team::AddDeath( Player* victim, Player* )
{
	assert(victim);
	if (!victim)
	{
		warning("Team::AddDeath", "NULL Player\n");
		return ;
	}

	if (!_playerList.ObjectInList(victim))
	{
		warning("Team::AddDeath", "%s", va("%s is not a member of %s", victim->client->pers.netname, getName().c_str() ) );
		return ;
	}

	_deaths++ ;		
}

void Team::AddKill( Player *killer, Player * )
{
	assert(killer);
	if (!killer)
	{
		warning("Team::AddKill", "NULL Player\n");
		return ;
	}

	if (!_playerList.ObjectInList(killer))
	{
		warning("Team::AddKill", "%s", va("%s is not a member of %s", killer->client->pers.netname, getName().c_str() ) );
		return ;
	}

	_kills++ ;
}

void Team::addPoints( Player *killer, int points )
{
	int oldPoints;


	oldPoints = _points;

	_points += points;

	multiplayerManager.teamPointsChanged( this, oldPoints, _points );
}

void Team::Win()
{
	_wins++ ;
}



void Team::Lose()
{
	_losses++ ;
}

void Team::BeginMatch()
{
	for (int idx = 1; idx <= _playerList.NumObjects(); idx++)
	{
		Player *player = _playerList.ObjectAt(idx);
		assert(player);
		if (!player)
		{
			warning("Team::endMatch", "NULL Player in team list\n");
			continue ;
		}
		_activatePlayer(player);
	}
}

void Team::EndMatch()
{
	for (int idx = 1; idx <= _playerList.NumObjects(); idx++)
	{
		Player *player = _playerList.ObjectAt(idx);
		assert(player);
		if (!player)
		{
			warning("Team::endMatch", "NULL Player in team list\n");
			continue ;
		}
		_deactivatePlayer(player);
	}
}

void Team::AddPlayer( Player* player )
{
	assert(player);
	if (!player)
	{
		warning("Team::AddPlayer", "%s", va("Attempt to add NULL player to %s\n", getName().c_str() ) );
		return ;
	}

	if ( ( unsigned int )_playerList.NumObjects() == _maxPlayers )
	{
		warning("Team::AddPlayer", "%s", va("The %s team is full!\n", getName().c_str() ) );
		return ;
	}
	_playerList.AddObject(player);
	
	if (isFighting())
	{
		_activatePlayer( player );
	}
}


void Team::RemovePlayer( Player *player )
{
	assert(player);
	if (!player)
	{
		warning("Team::RemovePlayer", "%s", va("Attempt to add NULL player to %s\n", getName().c_str() ) );
		return ;
	}

	if ( !_playerList.ObjectInList( player ) )
	{
		warning("Team::RemovePlayer", "%s", va("Player %s is not on team %s\n", player->client->pers.netname, getName().c_str() ) );
		return ;
	}
	_playerList.RemoveObject(player);
}


void Team::AddStartingAmmo( const SimpleAmmoType &ammo )
{
	_ammoList.AddObject(ammo);
}


void Team::AddStartingWeapon( const str& weaponName )
{
	_weaponList.AddObject(weaponName);
}



void Team::_giveInitialConditions( Player *player )
{
	assert(player);
	if (!player)
	{
		warning("Team::_giveWeaponsAndAmmo", "NULL Player passed\n");
		return ;
	}

	if (isStartingHealth())
	{
	}

	int idx = 0;
	for (idx=1; idx <= _weaponList.NumObjects(); idx++)
	{
		multiplayerManager.givePlayerItem( player->entnum, _weaponList.ObjectAt(idx) );
	}

	for (idx=1; idx <= _ammoList.NumObjects(); idx++)
	{
		SimpleAmmoType& ammo = _ammoList.ObjectAt(idx);
		player->giveItem( ammo.type, ammo.amount );
	}
}


void Team::_activatePlayer( Player *player )
{
	_giveInitialConditions(player);
	multiplayerManager.allowFighting( true );
	player->takedamage = DAMAGE_YES ;
}	



void Team::_deactivatePlayer( Player *player )
{
	assert(player);
	if (!player)
	{
		warning("Team::_deactivatePlayer", "%s", va("NULL Player passed to %s team\n", getName().c_str() ) );
		return ;
	}

	multiplayerManager.allowFighting( false );
	player->takedamage = DAMAGE_NO ;
}
