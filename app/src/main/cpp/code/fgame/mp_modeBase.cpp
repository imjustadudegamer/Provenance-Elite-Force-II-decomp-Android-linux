

#include "_pch_cpp.h"

#include "mp_manager.hpp"
#include "mp_modeBase.hpp"
#include "equipment.h"
#include "powerups.h"
#include "weaputils.h"
extern Event EV_Player_DeadBody;  

#define PROBE_GET(name, def, flags) gi.cvar(name, def, flags)
#define PROBE_PRINT                 gi.Printf
#include "../qcommon/qa_probe.h"

const float	MultiplayerModeBase::_defaultStartinghealth = 100.0f;
const int	MultiplayerModeBase::_defaultPointsPerKill = 1;
const int	MultiplayerModeBase::_defaultPointsPerTakenAwayForSuicide = 1;
const float	MultiplayerModeBase::_spectatorMoveSpeedModifier = 1.5f;

MultiplayerPlayerGameData::MultiplayerPlayerGameData()
{
	init();
}

void MultiplayerPlayerGameData::init( void )
{
	_numDeaths = 0;
	_numKills = 0;
	_points = 0;

	_entnum = 0;

	_playing = false;

	_nextHitSoundTime = 0.0f;

	_currentTeam = NULL;

	_lastKilledByPlayer = -1;
	_lastKillerOfPlayer = -1;

	_lastKilledByPlayerMOD = MOD_NONE;
	_lastKillerOfPlayerMOD = MOD_NONE;

	_lastPlace = -1;
	_lastTied = false;
}

void MultiplayerPlayerGameData::reset( void )
{
	_numDeaths = 0;
	_numKills = 0;
	_points = 0;

	_lastKilledByPlayer = -1;
	_lastKillerOfPlayer = -1;

	_lastKilledByPlayerMOD = MOD_NONE;
	_lastKillerOfPlayerMOD = MOD_NONE;

	_nextHitSoundTime = 0.0f;

	_lastPlace = -1;
	_lastTied = false;
}

CLASS_DECLARATION( Class, MultiplayerModeBase, NULL )
{
	{ NULL, NULL }
};


MultiplayerModeBase::MultiplayerModeBase()
{
	_maxPlayers = 20;
	_startingHealth = (unsigned int) _defaultStartinghealth;

	_fightInProgress = false;

	_activePlayers = 0;
	_spawncounter = 0;

	_pointLimit = 0;
	_timeLimit = 0.0f;

	_playerGameData = NULL;

	_spectatorIconIndex	= gi.imageindex( "sysimg/icons/mp/spectator" );

	_warmUpTextIndex = G_FindConfigstringIndex( "$$WarmUp$$", CS_GENERAL_STRINGS, MAX_GENERAL_STRINGS, true ) + CS_GENERAL_STRINGS;;
	_waitingForMinPlayersTextIndex = G_FindConfigstringIndex( "$$WaitMinPlayers$$", CS_GENERAL_STRINGS, MAX_GENERAL_STRINGS, true ) + CS_GENERAL_STRINGS;;
	_playingTextIndex = G_FindConfigstringIndex( "$$Playing$$", CS_GENERAL_STRINGS, MAX_GENERAL_STRINGS, true ) + CS_GENERAL_STRINGS;;

	_gameStarted = false;
	_lastTimeRemaining = 0;

	_lastHighestPoints = 0;
}

MultiplayerModeBase::~MultiplayerModeBase()
{
	int idx = 0 ;


	for (idx = 1; idx <= _ammoList.NumObjects(); idx++)
	{
		SimpleAmmoType* ammo = _ammoList.ObjectAt(idx);
		delete ammo;
		ammo = NULL;
	}


	delete [] _playerGameData;
	_playerGameData = NULL;
}

void MultiplayerModeBase::init( int maxPlayers )
{
	_maxPlayers = maxPlayers;
	_playerGameData = new MultiplayerPlayerGameData[ _maxPlayers ];

	multiplayerManager.cacheMultiplayerFiles( "mp_general" );
}

void MultiplayerModeBase::initItems( void )
{

	getSpawnpoints();

	resetSpawnpoints();


	_matchStartTime = multiplayerManager.getTime();
	_gameStartTime = multiplayerManager.getTime();

	_played5MinWarning = false;
	_played2MinWarning = false;
	_played1MinWarning = false;
}

int MultiplayerModeBase::findPlayer( const Player *player )
{
	int i;

	for ( i = 0 ; i < ( int )_maxPlayers ; i++ )
	{
		if ( _playerGameData[ i ]._playing && _playerGameData[ i ]._entnum == player->entnum )
			return i;
	}

	return -1;
}

int MultiplayerModeBase::getPoints( Player *player )
{
	int index;

	index = findPlayer( player );

	if ( index >= 0 )
	{
		return _playerGameData[ index ]._points;
	}

	return 0;
}

int MultiplayerModeBase::getKills( Player *player )
{
	int index;

	index = findPlayer( player );

	if ( index >= 0 )
	{
		return _playerGameData[ index ]._numKills;
	}

	return 0;
}

int MultiplayerModeBase::getDeaths( Player *player )
{
	int index;

	index = findPlayer( player );

	if ( index >= 0 )
	{
		return _playerGameData[ index ]._numDeaths;
	}

	return 0;
}

Team* MultiplayerModeBase::getPlayersTeam( const Player *player )
{
	int index;

	index = findPlayer( player );

	if ( index >= 0 )
	{
		return _playerGameData[ index ]._currentTeam;
	}

	return NULL;
}

void MultiplayerModeBase::update( float frameTime )
{
	if ( !_gameStarted )
	{
		if ( shouldStartMatch() )
		{
			multiplayerManager.startMatch();
		}
	}


	if ( getTimeLimit() > 0.0f )
	{
		float timeLimit;
		float timeRemaining;
		float fiveMinutes = 5 * 60;
		float twoMinutes = 2 * 60;
		float oneMinute = 1 * 60;

		timeLimit = getTimeLimit();
		timeRemaining = getTimeLimit() - ( multiplayerManager.getTime() - _gameStartTime );

		if ( ( timeLimit > fiveMinutes ) && ( timeRemaining < fiveMinutes ) && ( !_played5MinWarning ) )
		{
			multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_5mins.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
			_played5MinWarning = true;
		}
		else if ( ( timeLimit > twoMinutes ) && ( timeRemaining < twoMinutes ) && ( !_played2MinWarning ) )
		{
			multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_2mins.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
			_played2MinWarning = true;
		}
		else if ( ( timeLimit > oneMinute ) && ( timeRemaining < oneMinute ) && ( !_played1MinWarning ) )
		{
			multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_1mins.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
			_played1MinWarning = true;
		}
	}


	if ( ( getPointLimit() > 0 ) && ( !isEndOfMatch() ) )
	{
		int pointLimit;
		int pointsRemaining;
		int highestPoints;
		int lastPointsRemaining;

		highestPoints = getHighestPoints();

		if ( highestPoints > _lastHighestPoints )
		{
			pointLimit = getPointLimit();
			pointsRemaining = pointLimit - highestPoints;
			lastPointsRemaining = pointLimit - _lastHighestPoints;

			if ( ( pointLimit > 1 ) && ( pointsRemaining <= 1 ) && ( lastPointsRemaining > 1 ) )
				multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_1pointsleft.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
			else if ( ( pointLimit > 2 ) && ( pointsRemaining <= 2 ) && ( lastPointsRemaining > 2 ) )
				multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_2pointsleft.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
			else if ( ( pointLimit > 3 ) && ( pointsRemaining <= 3 ) && ( lastPointsRemaining > 3 ) )
				multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_3pointsleft.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
			else if ( ( pointLimit > 4 ) && ( pointsRemaining <= 4 ) && ( lastPointsRemaining > 4 ) )
				multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_4pointsleft.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
			else if ( ( pointLimit > 5 ) && ( pointsRemaining <= 5 ) && ( lastPointsRemaining > 5 ) )
				multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_5pointsleft.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
			else if ( ( pointLimit > 10 ) && ( pointsRemaining <= 10 ) && ( lastPointsRemaining > 10 ) )
				multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_10pointsleft.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
			else if ( ( pointLimit > 25 ) && ( pointsRemaining <= 25 ) && ( lastPointsRemaining > 25 ) )
				multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_25pointsleft.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
			else if ( ( pointLimit > 100 ) && ( pointsRemaining <= 100 ) && ( lastPointsRemaining > 100 ) )
				multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_100pointsleft.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
			else if ( ( pointLimit > 500 ) && ( pointsRemaining <= 500 ) && ( lastPointsRemaining > 500 ) )
				multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_500pointsleft.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
		}

		_lastHighestPoints = highestPoints;
	}
}

bool MultiplayerModeBase::isEndOfMatch( void )
{
	int i;


	if ( getPointLimit() > 0 ) 
	{
		for ( i = 0 ; i < ( int )_maxPlayers ; i++ )
		{
			if ( _playerGameData[ i ]._playing )
			{
				if ( _playerGameData[ i ]._points >= getPointLimit() )
					return true;
			}
		}
	}


	if ( ( getTimeLimit() > 0.0f ) && ( multiplayerManager.getTime() - _gameStartTime > getTimeLimit() ) ) 
		return true;

	return false;
}

void MultiplayerModeBase::playerKilled( Player *killedPlayer, Player *attackingPlayer, Entity *inflictor, int meansOfDeath )
{
	bool goodKill;

	if ( attackingPlayer && ( killedPlayer != attackingPlayer ) )
		goodKill = true;
	else
		goodKill = false;

	handleKill( killedPlayer, attackingPlayer, inflictor, meansOfDeath, goodKill );
}

void MultiplayerModeBase::handleKill( Player *killedPlayer, Player *attackingPlayer, Entity *inflictor, int meansOfDeath, bool goodKill )
{

	_playerGameData[ killedPlayer->entnum ]._numDeaths++;
	_playerGameData[ killedPlayer->entnum ]._lastKillerOfPlayerMOD = meansOfDeath;


	if ( goodKill )
	{

		_playerGameData[ attackingPlayer->entnum ]._numKills++;

		addPoints( attackingPlayer->entnum, multiplayerManager.getPointsForKill( killedPlayer, attackingPlayer, 
				inflictor, meansOfDeath, _defaultPointsPerKill ) );

		_playerGameData[ killedPlayer->entnum ]._lastKillerOfPlayer = attackingPlayer->entnum;
	}
	else
	{

		if ( attackingPlayer )
		{

			addPoints( attackingPlayer->entnum, -_defaultPointsPerTakenAwayForSuicide );
		}
		else
		{

			addPoints( killedPlayer->entnum, -_defaultPointsPerTakenAwayForSuicide );
		}

		_playerGameData[ killedPlayer->entnum ]._lastKillerOfPlayer = killedPlayer->entnum;
	}


	obituary( killedPlayer, attackingPlayer, meansOfDeath );


	if ( attackingPlayer )
	{
		_playerGameData[ attackingPlayer->entnum ]._lastKilledByPlayer = killedPlayer->entnum;
		_playerGameData[ attackingPlayer->entnum ]._lastKilledByPlayerMOD = meansOfDeath;
	}
}

void MultiplayerModeBase::addPoints( int entnum, int points )
{
	_playerGameData[ entnum ]._points += points;
}

void MultiplayerModeBase::obituary( Player *killedPlayer, Player *attackingPlayer, int meansOfDeath )
{
	const char *s1=NULL, *s2=NULL;
	str printString;
	bool suicide;
	bool printSomething;
	int i;
	char color;
	bool sameTeam;


	suicide = false;
	printSomething = false;

	sameTeam = false;

	if ( attackingPlayer && ( killedPlayer != attackingPlayer ) )
	{
		Team *killedPlayersTeam;
		Team *attackingPlayersTeam;

		killedPlayersTeam = multiplayerManager.getPlayersTeam( killedPlayer );

		attackingPlayersTeam = multiplayerManager.getPlayersTeam( attackingPlayer );

		if ( killedPlayersTeam && attackingPlayersTeam && ( killedPlayersTeam == attackingPlayersTeam ) )
		{
			sameTeam = true;
		}
	}

	if ( killedPlayer == attackingPlayer || !attackingPlayer )
	{
		suicide = true;
		printSomething = true;

		switch( meansOfDeath )
		{
			case MOD_SUICIDE:
				s1 = "$$MOD_SUICIDE$$";
				break;
			case MOD_DROWN:
				s1 = "$$MOD_DROWN$$";
				break;
			case MOD_LAVA:
				s1 = "$$MOD_LAVA$$";
				break;
			case MOD_SLIME:
				s1 = "$$MOD_SLIME$$";
				break;
			case MOD_FALLING:
				s1 = "$$MOD_FALLING$$";
				break;
			default:
				s1 = "$$MOD_SUICIDE$$";
				break;
		}
	}


	if ( attackingPlayer && attackingPlayer->isClient() && ( killedPlayer != attackingPlayer ) )
	{
		printSomething = true;

		switch( meansOfDeath )
		{
			case MOD_CRUSH:
			case MOD_CRUSH_EVERY_FRAME:
				s1 = "$$MOD_CRUSH$$";
				break;
			case MOD_TELEFRAG:
				s1 = "$$MOD_TELEFRAG$$";
				break;
			case MOD_EXPLODEWALL:
			case MOD_EXPLOSION:
			case MOD_POO_EXPLOSION:
				s1 = "$$MOD_EXPLOSION$$";
				break;
			case MOD_ELECTRICWATER:
			case MOD_ELECTRIC:
			case MOD_CIRCLEOFPROTECTION:
				s1 = "$$MOD_ELECTRIC$$";
				break;
			case MOD_IMPACT:
			case MOD_THROWNOBJECT:
				s1 = "$$MOD_IMPACT$$";
				s2 = "$$MOD_IMPACT2$$";
				break;
			case MOD_BEAM:
				s1 = "$$MOD_BEAM$$";
				break;
			case MOD_ROCKET:
				s1 = "$$MOD_ROCKET$$";
				s2 = "$$MOD_ROCKET2$$";
				break;
			case MOD_GAS_BLOCKABLE:
			case MOD_GAS:
				s1 = "$$MOD_GAS$$";
				break;
			case MOD_ACID:
				s1 = "$$MOD_ACID$$";
				break;
			case MOD_SWORD:
				s1 = "$$MOD_SWORD$$";
				break;
			case MOD_PLASMA:
			case MOD_PLASMABEAM:
			case MOD_PLASMASHOTGUN:
				s1 = "$$MOD_ASSULT_RIFLE$$";
				break;
			case MOD_RADIATION:
				s1 = "$$MOD_PLASMA$$";
				break;
			case MOD_STING:
			case MOD_STING2:
				s1 = "$$MOD_STING$$";
				break;
			case MOD_BULLET:
			case MOD_FAST_BULLET:
				s1 = "$$MOD_BULLET$$";
				break;
			case MOD_VEHICLE:
				s1 = "$$MOD_VEHICLE$$";
				break;
			case MOD_FIRE:
			case MOD_FIRE_BLOCKABLE:
			case MOD_ON_FIRE:
				s1 = "$$MOD_FIRE$$";
				break;
			case MOD_LIFEDRAIN:
				s1 = "$$MOD_LIFEDRAIN$$";
				break;
			case MOD_FLASHBANG:
				s1 = "$$MOD_FLASHBANG$$";
				break;
			case MOD_AXE:
				s1 = "$$MOD_AXE$$";
				s2 = "$$MOD_AXE2$$";
				break;
			case MOD_CHAINSWORD:
				s1 = "$$MOD_CHAINSWORD$$";
				break;
			case MOD_FIRESWORD:
				s1 = "$$MOD_FIRESWORD$$";
				break;
			case MOD_ELECTRICSWORD:
				s1 = "$$MOD_ELECTRICSWORD$$";
				s2 = "$$MOD_ELECTRICSWORD2$$";
				break;
			case MOD_LIGHTSWORD:
				s1 = "$$MOD_LIGHTSWORD$$";
				s2 = "$$MOD_LIGHTSWORD2$$";
				break;
			case MOD_IMPALE:
				s1 = "$$MOD_IMPALE$$";
				break;
			case MOD_UPPERCUT:
				s1 = "$$MOD_UPPERCUT$$";
				break;
			case MOD_POISON:
				s1 = "$$MOD_POISON$$";
				break;
			case MOD_PHASER:
				s1 = "$$MOD_PHASER$$";
				break;
			case MOD_COMP_RIFLE:
				s1 = "$$MOD_COMP_RIFLE$$";
				break;

			
			case MOD_VAPORIZE:
			case MOD_VAPORIZE_COMP:
			case MOD_VAPORIZE_DISRUPTOR:
			case MOD_VAPORIZE_PHOTON:
				s1 = "$$MOD_VAPORIZE$$";
				break;
			default:
				s1 = "$$MOD_DEFAULT$$";
				break;
		}
	}

	if ( printSomething )
	{
		Player *currentPlayer;


		if ( dedicated->integer )
		{
			if ( suicide )
			{
				printString = va( "%s %s", killedPlayer->client->pers.netname, s1 );
			}
			else if ( s2 )
			{
				printString = va( "%s %s %s %s", killedPlayer->client->pers.netname, s1, attackingPlayer->client->pers.netname, s2 );
			}
			else
			{
				printString = va( "%s %s %s", killedPlayer->client->pers.netname, s1, attackingPlayer->client->pers.netname );
			}

			if ( sameTeam )
			{
				printString += " ($$SameTeam$$)";
			}

			printString += "\n";

			gi.Printf( "%s", printString.c_str() );
		}


		for ( i = 0 ; i < ( int )_maxPlayers ; i++ )
		{
			currentPlayer = multiplayerManager.getPlayer( i );

			if ( !currentPlayer )
				continue;


			if ( killedPlayer && killedPlayer->entnum == i )
				color = COLOR_RED;
			else if ( attackingPlayer && attackingPlayer->entnum == i )
				color = COLOR_GREEN;
			else 
				color = COLOR_NONE;


			if ( suicide )
			{
				printString = va( "%s ^%c%s^8", killedPlayer->client->pers.netname, color, s1 );
			}
			else if ( s2 )
			{
				printString = va( "%s ^%c%s^8 %s ^%c%s^8", killedPlayer->client->pers.netname, color, s1, attackingPlayer->client->pers.netname, color, s2 );
			}
			else
			{
				printString = va( "%s ^%c%s^8 %s", killedPlayer->client->pers.netname, color, s1, attackingPlayer->client->pers.netname );
			}

			if ( sameTeam )
			{
				printString += " (^";
				printString += COLOR_RED;
				printString += "$$SameTeam$$^8)";
			}

			printString += "\n";

				
			if ( gi.GetNumFreeReliableServerCommands( currentPlayer->edict - g_entities ) > 32 )
			{
				multiplayerManager.HUDPrint( currentPlayer->entnum, printString.c_str() );
			}

		}
	}
}

bool MultiplayerModeBase::needToAddPlayer( Player *player )
{
	MultiplayerPlayerGameData *playerGameData;

	playerGameData = &_playerGameData[ player->entnum ];

	if ( playerGameData->_playing )
		return false;

	return true;
}


void MultiplayerModeBase::AddPlayer( Player *player )
{
	MultiplayerPlayerGameData *playerGameData;


	if ( !needToAddPlayer( player ) )
		return;


	playerGameData = &_playerGameData[ player->entnum ];

	playerGameData->reset();
	playerGameData->_entnum = player->entnum;
	playerGameData->_playing = true;
	playerGameData->_startTime = multiplayerManager.getTime();


	setupMultiplayerUI( player );
}

bool MultiplayerModeBase::canJoinTeam( Player *player, const str &teamName )
{
	if ( teamName == "normal" && multiplayerManager.isPlayerSpectator( player ) )
		return true;
	else if ( teamName == "spectator" )
		return true;
	else
		return false;
}

void MultiplayerModeBase::joinTeam( Player *player, const str &teamName )
{
	if ( teamName == "normal" )
	{
		multiplayerManager.setTeamHud( player, "" );
		RemovePlayer( player );
		AddPlayer( player  );
	}
	else if ( teamName == "spectator" )
	{
		multiplayerManager.makePlayerSpectator( player, SPECTATOR_TYPE_FOLLOW, true );
	}
}

void MultiplayerModeBase::setupMultiplayerUI( Player *player )
{
	gi.SendServerCommand( player->entnum, "stufftext \"ui_removehuds all\"\n" );
	gi.SendServerCommand( player->entnum, "stufftext \"ui_addhud mp_console\"\n" );


	if(mp_timelimit->integer)
	{
		gi.SendServerCommand( player->entnum, "stufftext \"globalwidgetcommand dmTimer enable\"\n");
	}
	else
	{
		gi.SendServerCommand( player->entnum, "stufftext \"globalwidgetcommand dmTimer disable\"\n");
	}
}

void MultiplayerModeBase::RemovePlayer( Player *player )
{
	_playerGameData[ player->entnum ]._playing = false;
}

float MultiplayerModeBase::playerDamaged( Player *damagedPlayer, Player *attackingPlayer, float damage, int meansOfDeath )
{
	return damage;
}

void MultiplayerModeBase::playerTookDamage( Player *damagedPlayer, Player *attackingPlayer, float damage, int meansOfDeath )
{
	if ( attackingPlayer && ( attackingPlayer != damagedPlayer ) )
	{

		if ( multiplayerManager.getTime() >= _playerGameData[ attackingPlayer->entnum ]._nextHitSoundTime && damagedPlayer->health > 0.0f )
		{
			multiplayerManager.instantPlayerSound( attackingPlayer->entnum, "snd_mp_hurtsomeone", CHAN_COMBAT4 );

			_playerGameData[ attackingPlayer->entnum ]._nextHitSoundTime = level.time + 0.25;
		}
	}
}

void MultiplayerModeBase::readMultiplayerConfig( const char *configName )
{
	Script buffer;
	const char *token;


	if ( !gi.FS_Exists( configName ) )
		return;


	buffer.LoadFile( configName );


	while ( buffer.TokenAvailable( true ) )
	{
		token = buffer.GetToken( true );


		if ( !parseConfigToken( token, &buffer ) )
		{
			gi.DPrintf( "Token %s from %s not handled by anyone\n", token, configName );
		}
	}
}

bool MultiplayerModeBase::parseConfigToken( const char *key, Script *buffer )
{
	const char *token;

	if ( stricmp( key, "giveWeapon" ) == 0 )
	{
		if ( buffer->TokenAvailable( false ) )
		{
			token = buffer->GetToken( false );

			AddStartingWeapon( token );

			return true;
		}
	}
	else if ( stricmp( key, "startingWeapon" ) == 0 )
	{
		if ( buffer->TokenAvailable( false ) )
		{
			token = buffer->GetToken( false );

			SetStartingWeapon( token );

			return true;
		}
	}

	return false;	
}

int MultiplayerModeBase::getIcon( Player *player, int statNum, int value )
{
		return value;
}

bool MultiplayerModeBase::shouldStartMatch( void )
{
	int timeRemaining;
	int numPlayers;
	int i;

	if ( _gameStarted )
		return false;

	if ( isEndOfMatch() )
		return false;


	timeRemaining = (int)(_matchStartTime + mp_warmUpTime->value - multiplayerManager.getTime() + 1.0f);

	if ( ( timeRemaining > 0 ) && ( timeRemaining < 6 ) && ( timeRemaining != _lastTimeRemaining ) )
	{
		_lastTimeRemaining = timeRemaining;

		multiplayerManager.centerPrintAllClients( va( "%d", _lastTimeRemaining ), CENTERPRINT_IMPORTANCE_NORMAL );

		switch( timeRemaining )
		{
		case 1 :
			multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_1.mp3", CHAN_AUTO, DEFAULT_VOL, 
					DEFAULT_MIN_DIST, NULL, 0.5f  );
			break;
		case 2 :
			multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_2.mp3", CHAN_AUTO, DEFAULT_VOL, 
					DEFAULT_MIN_DIST, NULL, 0.5f  );
			break;
		case 3 :
			multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_3.mp3", CHAN_AUTO, DEFAULT_VOL, 
					DEFAULT_MIN_DIST, NULL, 0.5f  );
			break;
		case 4 :
			multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_4.mp3", CHAN_AUTO, DEFAULT_VOL, 
					DEFAULT_MIN_DIST, NULL, 0.5f  );
			break;
		case 5 :
			multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_5.mp3", CHAN_AUTO, DEFAULT_VOL, 
					DEFAULT_MIN_DIST, NULL, 0.5f  );
			break;
		}
	}


	if ( multiplayerManager.getTime() < _matchStartTime + mp_warmUpTime->value )
		return false;


	numPlayers = 0;

	for ( i = 0 ; i < ( int )_maxPlayers ; i++ )
	{
		if ( _playerGameData[ i ]._playing )
		{
			numPlayers++;
		}
	}

	if ( numPlayers < mp_minPlayers->integer )
	{
		{
			static int n = 0;
			if (PROBE_Throttle(&n, 4, 500)) {
				PROBE(
					"npc:warmup",
					"BLOCKED minPlayers: numPlayers=%d mp_minPlayers=%d warmupElapsed=%d",
					numPlayers,
					mp_minPlayers->integer,
					(int)(multiplayerManager.getTime() - (_matchStartTime + mp_warmUpTime->value))
				);
			}
		}
		return false;
	}

	PROBE(
		"npc:warmup",
		"WARMUP DONE -> startMatch: numPlayers=%d maxPlayers=%d",
		numPlayers,
		(int)_maxPlayers
	);
	return true;
}

int MultiplayerModeBase::getStat( Player *player, int statNum, int value )
{
	if ( statNum == STAT_MP_STATE )
	{
		int numPlayers;
		int i;
		Player *player_shadow1;

		if ( _gameStarted )
		{
			if ( !value )
				return _playingTextIndex;
			else
				return value;
		}

		if ( multiplayerManager.getTime() < _matchStartTime + mp_warmUpTime->value )
			return _warmUpTextIndex;


		numPlayers = 0;

		for ( i = 0 ; i < ( int )_maxPlayers ; i++ )
		{
			player_shadow1 = multiplayerManager.getPlayer( i );

			if ( player_shadow1 && !multiplayerManager.isPlayerSpectatorByChoice( player_shadow1 ) )
			{
				numPlayers++;
			}
		}

		if ( numPlayers < mp_minPlayers->integer )
			return _waitingForMinPlayersTextIndex;

	}

	return value;
}

void MultiplayerModeBase::startMatch( void )
{
	int i;
	Player *player;

	_gameStarted = true;
	multiplayerManager.allowFighting( true );

	PROBE("npc:matchstart", "startMatch: _maxPlayers=%d", (int)_maxPlayers);


	for ( i = 0 ; i < ( int )_maxPlayers ; i++ )
	{
		player = getPlayer( i );

		if ( player && ( ( player->edict->svflags & SVF_BOT ) || !multiplayerManager.isPlayerSpectatorByChoice( player ) ) )
		{
			multiplayerManager.playerEnterArena( player->entnum, player->health );

			respawnPlayer( player );

			multiplayerManager.playerSpawned( player );
		}
	}


	multiplayerManager.centerPrintAllClients( "$$MatchStarted$$", CENTERPRINT_IMPORTANCE_HIGH );
	multiplayerManager.broadcastSound( "localization/sound/dialog/dm/comp_mats.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.5f );
}

void MultiplayerModeBase::restartMatch( void )
{
	_gameStarted = false;

	_matchStartTime = multiplayerManager.getTime();
	_lastTimeRemaining = 0;
}

void MultiplayerModeBase::endMatch( void )
{
	int i;
	Player *player;

	_gameStarted = false;


	for ( i = 0 ; i < ( int )_maxPlayers ; i++ )
	{
		if ( _playerGameData[ i ]._playing )
		{
			player = getPlayer( i );

			if ( player )
			{
				multiplayerManager.makePlayerSpectator( player );
			}
		}
	}
}

bool MultiplayerModeBase::inMatch( void )
{
	return _gameStarted;
}

Player *MultiplayerModeBase::getPlayer( int entnum )
{

	if ( ( entnum < 0 ) || ( entnum >= ( int )_maxPlayers ) )
		return NULL;

	if ( !g_entities[ entnum ].inuse || !g_entities[ entnum ].entity )
		return NULL;

	if ( !g_entities[ entnum ].entity->isSubclassOf( Player ) )
		return NULL;


	return (Player *)g_entities[ entnum ].entity;
}



































void MultiplayerModeBase::score( const Player *player )
{
	char		   string[1400];
	char		   entry[1024];
	int            i;
	int            tempStringlength;
	int            count        = 0;
	int            stringlength = 0;
	Player			*currentPlayer;
	int				spectator;

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

		if ( multiplayerManager.isPlayerSpectator( currentPlayer ) && multiplayerManager.isPlayerSpectatorByChoice( currentPlayer ) )
			spectator = true;
		else
			spectator = false;

		Com_sprintf( entry, sizeof( entry ), "%i %i %i %i %i %i %i %d %d %d %d %d %d ", 
				multiplayerManager.getClientNum( _playerGameData[ i ]._entnum ),
				_playerGameData[ i ]._points, 
				_playerGameData[ i ]._numKills, 
				_playerGameData[ i ]._numDeaths, 
				spectator,
				(int)(multiplayerManager.getTime() - _playerGameData[ i ]._startTime ), 
				multiplayerManager.getClientPing( _playerGameData[ i ]._entnum ),
				multiplayerManager.getScoreIcon( currentPlayer, SCOREICON1 ), 
				multiplayerManager.getScoreIcon( currentPlayer, SCOREICON2 ), 
				multiplayerManager.getScoreIcon( currentPlayer, SCOREICON3 ), 
				multiplayerManager.getScoreIcon( currentPlayer, SCOREICON4 ), 
				multiplayerManager.getScoreIcon( currentPlayer, SCOREICON5 ), 
				multiplayerManager.getScoreIcon( currentPlayer, SCOREICON6 ) );

		tempStringlength = strlen( entry );


		if ( stringlength + tempStringlength > 1000 )
				break;

		strcpy( string + stringlength, entry );

		stringlength += tempStringlength;
		count++;
	}

	gi.SendServerCommand( player->edict-g_entities, "scores 0 %i %s", count, string );
}

void MultiplayerModeBase::playerDead( Player *player )
{
	assert(player);

	if (!player)
	{
		warning("MultiplayerModeBase::playerDead", "NULL Player\n");
		return ;
	}

	player->ProcessEvent( EV_Player_DeadBody );
	player->hideModel();
}

void MultiplayerModeBase::SetStartingWeapon( const str &weaponName )
{
	_startingWeaponName = weaponName;
}

void MultiplayerModeBase::AddStartingWeapon( const str& weaponViewmodel )
{
	_weaponList.AddObject(weaponViewmodel);
}

void MultiplayerModeBase::ActivatePlayer( Player* player )
{
	assert( player );

	if ( !player )
	{
		warning("MultiplayerModeBase::ActivatePlayer", "NULL Player\n");
		return ;
	}


	if ( !_gameStarted )
	{
		multiplayerManager.makePlayerSpectator( player );
		return;

	}
	
	multiplayerManager.playerEnterArena( player->entnum, _startingHealth );
	
	multiplayerManager.changePlayerModel( player, player->client->pers.mp_playermodel );
	_giveInitialConditions( player );


	if ( mp_respawnInvincibilityTime->value > 0.0f ) 
	{
		Powerup *powerup;
		Event *event;

		powerup = Powerup::CreatePowerup( "ProtectionTemp", "models/item/powerup_protection.tik", player );

		if ( powerup )
		{
			powerup->CancelEventsOfType( EV_ProcessInitCommands );
			powerup->ProcessInitCommands( powerup->edict->s.modelindex );

			event = new Event( EV_Item_SetAmount );
			event->AddFloat( mp_respawnInvincibilityTime->value );
			powerup->ProcessEvent( event );

			player->setPowerup( powerup );
		}
	}

	multiplayerManager.allowFighting( true );
	player->takedamage = DAMAGE_YES;

	multiplayerManager.playerSpawned( player );
}

void MultiplayerModeBase::BeginMatch( void )
{
	_fightInProgress = true ;
	resetSpawnpoints();
	_beginMatch();
}

void MultiplayerModeBase::EndMatch( void )
{
	_fightInProgress = false ;
	resetSpawnpoints();
	_endMatch();
}

void MultiplayerModeBase::resetSpawnpoints( void )
{
	_unusedSpawnpointList.ClearObjectList();

	for( int idx = 1; idx <= _spawnpointList.NumObjects(); idx++ )
	{
		_unusedSpawnpointList.AddObject( _spawnpointList.ObjectAt( idx ) );
	}
}

void MultiplayerModeBase::getSpawnpoints( void )
{
	PlayerDeathmatchStart *deathmatchStart;
		
	deathmatchStart = ( PlayerDeathmatchStart * )G_FindClass( NULL, "info_player_deathmatch" );

	while ( deathmatchStart )
	{
		_spawnpointList.AddObject( deathmatchStart );

		deathmatchStart = ( PlayerDeathmatchStart * )G_FindClass( deathmatchStart, "info_player_deathmatch" );
	}
}



void MultiplayerModeBase::_beginMatch()
{
	for(int idx=1; idx <= _playerList.NumObjects(); idx++ )
	{
		Player *player = _playerList.ObjectAt( idx );

		assert( player );
		if ( !player )
		{
				continue;
		}

		multiplayerManager.allowFighting( true );
	}
}

void MultiplayerModeBase::_endMatch()
{
}

void MultiplayerModeBase::declareWinner( void )
{
	int i;
	Player *player;
	int place;
	bool tied;

	for ( i = 0 ; i < ( int )_maxPlayers ; i++ )
	{
		player = getPlayer( i );

		if ( !player )
			continue;

		place = getPlace( player, &tied );

		if ( ( place == 1 ) && tied )
			multiplayerManager.playerSound( player->entnum, "localization/sound/dialog/dm/comp_tiedfirst.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, 1.5f );
		else if ( place == 1 )
			multiplayerManager.playerSound( player->entnum, "localization/sound/dialog/dm/comp_winn.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, 1.5f );
		else if ( place == 2 )
			multiplayerManager.playerSound( player->entnum, "localization/sound/dialog/dm/comp_second.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, 1.5f );
		else if ( place == 3 )
			multiplayerManager.playerSound( player->entnum, "localization/sound/dialog/dm/comp_third.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, 1.5f );
		else
			multiplayerManager.playerSound( player->entnum, "localization/sound/dialog/dm/comp_didnotrank.mp3", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, 1.5f );
	}
}

void MultiplayerModeBase::_giveInitialConditions( Player *player )
{
	int idx ;

	assert( player );

	if ( !player )
	{
		warning( "MultiplayerModeBase::_giveInitialConditions", "NULL player specified.\n" );
		return;
	}


	for ( idx=1; idx <= _weaponList.NumObjects(); idx++ )
	{
		multiplayerManager.givePlayerItem( player->entnum, _weaponList.ObjectAt( idx ) );
	}


	for ( idx=1; idx <= _ammoList.NumObjects(); idx++ )
	{
		Event *ev = new Event( "ammo" );
		ev->AddString( _ammoList.ObjectAt( idx )->type );
		ev->AddInteger( _ammoList.ObjectAt( idx )->amount );
		player->ProcessEvent( ev ); 
	}


	if ( _startingWeaponName.length() )
	{
		Event *ev = new Event( "use" );
		ev->AddString( _startingWeaponName );
		player->ProcessEvent( ev ); 
	}
}

int MultiplayerModeBase::getNumSpawnpoints( void )
{
	return _spawnpointList.NumObjects();
}

Entity* MultiplayerModeBase::getSpawnpointbyIndex( int index )
{
	int numPoints;


	numPoints = getNumSpawnpoints();

	if ( !numPoints )
	{
		warning( "MultiplayerModeBase::getSpawnpointbyIndex", "No spawnpoints found in arena\n" );
		return NULL;
	}

	if ( ( index < 0 ) || ( index >= numPoints ) )
		return NULL;


	return _spawnpointList.ObjectAt( index + 1 );
}

Entity* MultiplayerModeBase::getRandomSpawnpoint( bool useCounter )
{
	Entity *spot = NULL;

	int numPoints = _spawnpointList.NumObjects();


	if ( !numPoints )
	{
		warning( "MultiplayerModeBase::GetRandomSpawnpoint", "No spawnpoints found in arena\n" );
		return NULL;
	}


	if ( useCounter )
	{
		if ( ( int )_spawncounter > numPoints )
		{
			_spawncounter = 1; 
		}

		spot = ( Entity * )_spawnpointList.ObjectAt( _spawncounter );
		_spawncounter++;
		return spot;
	}
			
	int selection = ( (int)( G_Random() * numPoints ) );


	return getSpawnpointbyIndex( selection );
}

int MultiplayerModeBase::getNumNamedSpawnpoints( const str &spawnpointType )
{
	int numPoints;
	int numNamedPoints;
	int i;
	PlayerDeathmatchStart *spawnpoint;

	numPoints = _spawnpointList.NumObjects();

	if ( !numPoints )
	{
		warning( "MultiplayerModeBase::getNumNamedSpawnpoint", "No spawnpoints found in map\n" );
		return 0;
	}


	numNamedPoints = 0;

	for ( i = 1 ; i <= numPoints ; i++ )
	{
		spawnpoint = _spawnpointList.ObjectAt( i );

		if ( spawnpoint->_type == spawnpointType )
		{
			numNamedPoints++;
		}
	}

	return numNamedPoints;
}

Entity* MultiplayerModeBase::getNamedSpawnpointbyIndex( const str &spawnpointType, int index )
{
	int i;
	int numNamedPoints;
	int numPoints;
	PlayerDeathmatchStart *spawnpoint;


	numNamedPoints = getNumNamedSpawnpoints( spawnpointType );


	if ( !numNamedPoints || ( index < 0 ) || ( index >= numNamedPoints ) )
		return NULL;

	numNamedPoints = 0;


	numPoints = getNumSpawnpoints();

	for ( i = 1 ; i <= numPoints ; i++ )
	{
		spawnpoint = _spawnpointList.ObjectAt( i );

		if ( spawnpoint->_type == spawnpointType )
		{
			if ( numNamedPoints == index )
			{
				return spawnpoint;
			}

			numNamedPoints++;
		}
	}

	return NULL;
}

Entity* MultiplayerModeBase::getRandomNamedSpawnpoint( const str &spawnpointType )
{
	int numNamedPoints;
	int selection;

	numNamedPoints = getNumNamedSpawnpoints( spawnpointType );

	selection = ( (int)( G_Random() * numNamedPoints ) );

	return getNamedSpawnpointbyIndex( spawnpointType, selection );
}

Entity* MultiplayerModeBase::getFarthestNamedSpawnpoint( const Vector &origin, const str &spawnpointType )
{
	int i;
	int numNamedPoints;
	int numPoints;
	PlayerDeathmatchStart *spawnpoint;
	PlayerDeathmatchStart *farthestSpawnpoint;
	float farthestDistance;
	Vector dir;
	float dist;


	farthestSpawnpoint = NULL;
	farthestDistance = 0.0f;

	numNamedPoints = getNumNamedSpawnpoints( spawnpointType );


	if ( !numNamedPoints )
		return NULL;


	numPoints = getNumSpawnpoints();

	for ( i = 1 ; i <= numPoints ; i++ )
	{
		spawnpoint = _spawnpointList.ObjectAt( i );

		if ( spawnpoint->_type == spawnpointType )
		{
			dir = origin - spawnpoint->origin;
			dist = dir.length();

			if ( !farthestSpawnpoint || ( dist > farthestDistance ) )
			{
				farthestSpawnpoint = spawnpoint;
				farthestDistance = dist;
			}
		}
	}

	return farthestSpawnpoint;
}

Entity *MultiplayerModeBase::getSpawnPoint( Player *player )
{
	Entity *spawnPoint = NULL;
	int randomStartingSpot;
	int i;
	int spawnPointIndex;
	int numSpawnPoints;
	bool useAnySpawnPoint;

	numSpawnPoints = getNumNamedSpawnpoints( "" );

	if ( numSpawnPoints == 0 )
	{
		useAnySpawnPoint = true;

		numSpawnPoints = getNumSpawnpoints();
	}
	else
	{
		useAnySpawnPoint = false;
	}

	randomStartingSpot = ( (int)( G_Random() * numSpawnPoints ) );

	for( i = 0 ; i < numSpawnPoints ; i++ )
	{
		spawnPointIndex = ( randomStartingSpot + i ) % numSpawnPoints;

		if ( useAnySpawnPoint )
			spawnPoint = getSpawnpointbyIndex( spawnPointIndex );
		else
			spawnPoint = getNamedSpawnpointbyIndex( "", spawnPointIndex );

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

			num = gi.AreaEntities( min, max, touch, MAX_GENTITIES, qfalse );

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
					[[maybe_unused]] Player *hitPlayer;

					hitPlayer = (Player *)hit->entity;

					badSpot = true;
					break;
				}
			}

			if ( !badSpot )
			{
				break;
			}
		}

	}

	return spawnPoint;
}

Player *MultiplayerModeBase::getLastKilledByPlayer( Player *player, int *meansOfDeath )
{
	int entnum;

	entnum = _playerGameData[ player->entnum ]._lastKilledByPlayer;

	if ( entnum < 0 )
		return NULL;

	if ( meansOfDeath )
		*meansOfDeath = _playerGameData[ player->entnum ]._lastKilledByPlayerMOD;

	return GetPlayer( entnum );
}

Player *MultiplayerModeBase::getLastKillerOfPlayer( Player *player, int *meansOfDeath )
{
	int entnum;

	entnum = _playerGameData[ player->entnum ]._lastKillerOfPlayer;

	if ( entnum < 0 )
		return NULL;

	if ( meansOfDeath )
		*meansOfDeath = _playerGameData[ player->entnum ]._lastKillerOfPlayerMOD;

	return GetPlayer( entnum );
}

void MultiplayerModeBase::applySpeedModifiers( Player *player, int *moveSpeed )
{
	if ( multiplayerManager.isPlayerSpectator( player ) )
	{
		*moveSpeed *= (int)_spectatorMoveSpeedModifier;
	}
}

int MultiplayerModeBase::comparePlayerScore( MultiplayerPlayerGameData &player1Data, MultiplayerPlayerGameData &player2Data )
{

	if ( player1Data._points > player2Data._points )
		return 1;
	else if ( player1Data._points < player2Data._points )
		return -1;


	if ( player1Data._numKills > player2Data._numKills )
		return 1;
	else if ( player1Data._numKills < player2Data._numKills )
		return -1;


	if ( player1Data._numDeaths > player2Data._numDeaths )
		return -1;
	else if ( player1Data._numDeaths < player2Data._numDeaths )
		return 1;


	return 0;
}

int MultiplayerModeBase::getPlace( Player *player, bool *tied )
{
	int place = 1;
	bool isTied = false;
	int i;
	Player *currentPlayer;
	int scoreDiff;

	for ( i = 0 ; i < ( int )_maxPlayers ; i++ )
	{

		currentPlayer = getPlayer( i );

		if ( !currentPlayer || ( currentPlayer == player ) )
			continue;


		scoreDiff = comparePlayerScore( _playerGameData[ player->entnum ], _playerGameData[ currentPlayer->entnum ] );

		if ( scoreDiff < 0 )
		{

			place++;
		}
		else if ( scoreDiff == 0 )
		{

			isTied = true;
		}
	}

	if ( tied )
	{
		*tied = isTied;
	}

	return place;
}

int MultiplayerModeBase::getHighestPoints( void )
{
	int i;
	int highestPoints = -999999999;

	for ( i = 0 ; i < ( int )_maxPlayers ; i++ )
	{
		if ( _playerGameData[ i ]._points > highestPoints )
		{
			highestPoints = _playerGameData[ i ]._points;
		}
	}

	return highestPoints;
}

bool MultiplayerModeBase::shouldKeepNormalItem( Item *item )
{
	if ( multiplayerManager.checkFlag( MP_FLAG_NO_POWERUPS ) && item->isSubclassOf( PowerupBase ) )
		return false;

	return true;
}
