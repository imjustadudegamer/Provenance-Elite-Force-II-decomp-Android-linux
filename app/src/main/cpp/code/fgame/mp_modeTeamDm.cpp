
#include "_pch_cpp.h"
#include "mp_modeTeamDm.hpp"
#include "mp_manager.hpp"

CLASS_DECLARATION( ModeTeamBase, ModeTeamDeathmatch, NULL )
{
	{ NULL, NULL }
};

ModeTeamDeathmatch::ModeTeamDeathmatch()
{
	_redTeam  = AddTeam("Red");
	_blueTeam = AddTeam("Blue");
}

ModeTeamDeathmatch::~ModeTeamDeathmatch()
{
}

void ModeTeamDeathmatch::playerKilled( Player *killedPlayer, Player *attackingPlayer, Entity *inflictor, int meansOfDeath )
{
	Team *team;
	int points;
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

	if ( team )
	{
		if ( multiplayerManager.checkRule( "team-pointsForKills", true, attackingPlayer ) )
		{

			if ( _playerGameData[ killedPlayer->entnum ]._currentTeam != _playerGameData[ attackingPlayer->entnum ]._currentTeam )
			{
				points = _defaultPointsPerKill;


				points = multiplayerManager.getPointsForKill( killedPlayer, attackingPlayer, inflictor, meansOfDeath, points );


				team->addPoints( attackingPlayer, points );
			}
		}
	}
}

bool ModeTeamDeathmatch::isEndOfMatch( void )
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

int ModeTeamDeathmatch::getTeamPoints( Player *player )
{
	Team *team;


	team = _playerGameData[ player->entnum ]._currentTeam;

	if ( team )
		return team->getPoints();
	else
		return 0;
}

void ModeTeamDeathmatch::setupMultiplayerUI( Player *player )
{

	gi.SendServerCommand( player->entnum, "stufftext \"ui_removehuds all\"\n" );
	gi.SendServerCommand( player->entnum, "stufftext \"ui_addhud mp_console\"\n" );
	gi.SendServerCommand( player->entnum, "stufftext \"ui_addhud mp_teamhud\"\n" );
	
	if(mp_timelimit->integer)
	{
		gi.SendServerCommand( player->entnum, "stufftext \"globalwidgetcommand dmTimer enable\"\n");
	}
	else
	{
		gi.SendServerCommand( player->entnum, "stufftext \"globalwidgetcommand dmTimer disable\"\n");
	}
}

void ModeTeamDeathmatch::init( int maxPlayers )
{
	ModeTeamBase::init( maxPlayers );

	readMultiplayerConfig( "global/mp_team.cfg" );
}

void ModeTeamDeathmatch::score( const Player *player )
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
				(int)(multiplayerManager.getTime() - _playerGameData[ i ]._startTime ), 
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

	gi.SendServerCommand( player->edict-g_entities, "scores 1 %i %d %d %s", count, redScore, blueScore, string );
}

bool ModeTeamDeathmatch::checkGameType( const char *gameType )
{
	if ( stricmp( gameType, "teamdm" ) == 0 )
		return true;
	else
		return false;
}
