
#include "_pch_cpp.h"

#include "mp_manager.hpp"
#include "mp_modeDm.hpp"


CLASS_DECLARATION( MultiplayerModeBase, ModeDeathmatch, NULL )
{
	{ NULL, NULL }
};

ModeDeathmatch::ModeDeathmatch()
{
}

ModeDeathmatch::~ModeDeathmatch()
{
}

void ModeDeathmatch::respawnPlayer( Player *player )
{
	Entity *spawnPoint;

	assert(player);

	if ( !player )
	{
		warning("ModeDeathmatch::respawnPlayer", "NULL player\n");
		return ;
	}


	if ( !multiplayerManager.checkRule( "respawnPlayer", true, player ) )
	{
		if ( multiplayerManager.checkRule( "allowSpectator", true ) )
		{
			multiplayerManager.makePlayerSpectator( player );
		}

		return;
	}


	MultiplayerModeBase::respawnPlayer(player);

	multiplayerManager.initPlayer( player );


	spawnPoint = getSpawnPoint( player );

	if ( spawnPoint )
	{
		player->WarpToPoint( spawnPoint );
	}

	KillBox( player );

	ActivatePlayer(player);
}


void ModeDeathmatch::AddPlayer( Player *player )
{
	Entity *spawnPoint;

	assert(player);

	if ( !player )
	{
		warning("ModeDeathmatch::AddPlayer", "NULL Player\n");
		return ; 
	}

	if ( !needToAddPlayer( player ) )
		return;

	MultiplayerModeBase::AddPlayer(player);

	if ( !multiplayerManager.checkRule( "spawnPlayer", true, player ) )
	{
		if ( multiplayerManager.checkRule( "allowSpectator", true ) )
		{
			multiplayerManager.makePlayerSpectator( player );
		}

		return;
	}


	spawnPoint = getSpawnPoint( player );

	if ( spawnPoint )
	{
		player->WarpToPoint( spawnPoint );
	}

	KillBox( player );

	ActivatePlayer(player);


	if ( !_gameStarted )
	{
		multiplayerManager.makePlayerSpectator( player );
	}
}

void ModeDeathmatch::init( int maxPlayers )
{
	MultiplayerModeBase::init( maxPlayers );

	readMultiplayerConfig( "global/mp_dm.cfg" );

	multiplayerManager.cacheMultiplayerFiles( "mp_dm" );
}

bool ModeDeathmatch::checkGameType( const char *gameType )
{
	if ( stricmp( gameType, "dm" ) == 0 )
		return true;
	else
		return false;
}

int ModeDeathmatch::getHighestPoints( int entnum )
{
	int i;
	int highestPoints = -999999999;

	for ( i = 0 ; i < ( int )_maxPlayers ; i++ )
	{
		if ( _playerGameData[ i ]._playing && ( _playerGameData[ i ]._entnum != entnum ) )
		{
			if ( _playerGameData[ i ]._points > highestPoints )
			{
				highestPoints = _playerGameData[ i ]._points;
			}
		}
	}

	return highestPoints;
}

bool ModeDeathmatch::checkRule( const char *rule, bool defaultValue, Player *player )
{
	if ( stricmp( rule, "usingIndividualScore" ) == 0 )
		return true;
	else
		return defaultValue;
}

void ModeDeathmatch::update( float frameTime )
{
	int i;
	Player *player;
	int place;
	bool tied;

	MultiplayerModeBase::update( frameTime );


	if ( _gameStarted )
	{
		for ( i = 0 ; i < ( int )_maxPlayers ; i++ )
		{
			player = multiplayerManager.getPlayer( i );

			if ( !player )
				continue;

			place = getPlace( player, &tied );

			if ( ( place != _playerGameData[ i ]._lastPlace ) || ( tied != _playerGameData[ i ]._lastTied ) )
			{
				if ( _playerGameData[ i ]._lastPlace != -1 )
				{
					if ( ( place == 1 ) && ( !tied ) )
					{
						multiplayerManager.playerSound( _playerGameData[ i ]._entnum, "localization/sound/dialog/dm/comp_1stplace.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, 1.0f );
					}
					else if ( ( place == 1 ) && ( tied ) )
					{
						multiplayerManager.playerSound( _playerGameData[ i ]._entnum, "localization/sound/dialog/dm/comp_tiedlead.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, 1.0f );
					}
					else if ( ( _playerGameData[ i ]._lastPlace == 1 ) && ( ( place != 1 ) || ( tied ) ) )
					{
						multiplayerManager.playerSound( _playerGameData[ i ]._entnum, "localization/sound/dialog/dm/comp_nolead.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, 1.0f );
					}
				}

				_playerGameData[ i ]._lastPlace = place;
				_playerGameData[ i ]._lastTied  = tied;
			}
		}
	}
}
