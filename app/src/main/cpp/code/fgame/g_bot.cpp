//-----------------------------------------------------------------------------
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//-----------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "g_local.h"
#include "../qcommon/qcommon.h"
#include "botlib.h"		

#include "be_aas.h"
#include "be_ea.h"
#include "be_ai_char.h"
#include "be_ai_chat.h"
#include "be_ai_gen.h"
#include "be_ai_goal.h"
#include "be_ai_move.h"
#include "be_ai_weap.h"
#include "ai_main.h"

#include "_pch_cpp.h"

#include "mp_manager.hpp"

#define PROBE_GET(name, def, flags) gi.cvar(name, def, flags)
#define PROBE_PRINT                 gi.Printf
#include "../qcommon/qa_probe.h"

#define	MAX_ARENAS			1024
#define	MAX_ARENAS_TEXT		8192
#define MAX_BOTS			1024
#define MAX_BOTS_TEXT		8192

static int		g_numBots;
static char		*g_botInfos[MAX_BOTS];


int				g_numArenas;
static char		*g_arenaInfos[MAX_ARENAS];


#define BOT_BEGIN_DELAY_BASE		2000
#define BOT_BEGIN_DELAY_INCREMENT	1500

#define BOT_SPAWN_QUEUE_DEPTH	16

typedef struct {
	int		clientNum;
	int		spawnTime;
} botSpawnQueue_t;

static botSpawnQueue_t	botSpawnQueue[BOT_SPAWN_QUEUE_DEPTH];

vmCvar_t bot_minplayers;

extern gentity_t	*podium1;
extern gentity_t	*podium2;
extern gentity_t	*podium3;


float Cvar_VariableValue( const char *var_name ) {
	char buf[128];

	gi.Cvar_VariableStringBuffer(var_name, buf, sizeof(buf));
	return atof(buf);
}



int G_ParseInfos( const char *buf, int max, char *infos[] ) {
	char	*token;
	int		count;
	char	key[MAX_TOKEN_CHARS];
	char	info[MAX_INFO_STRING];

	count = 0;

	while ( 1 ) {
		token = (char *)COM_Parse( (char **)&buf );
		if ( !token[0] ) {
			break;
		}
		if ( strcmp( token, "{" ) ) {
			Com_Printf( "Missing { in info file\n" );
			break;
		}

		if ( count == max ) {
			Com_Printf( "Max infos exceeded\n" );
			break;
		}

		info[0] = '\0';
		while ( 1 ) {
			token = COM_ParseExt( (char **)&buf, qtrue );
			if ( !token[0] ) {
				Com_Printf( "Unexpected end of info file\n" );
				break;
			}
			if ( !strcmp( token, "}" ) ) {
				break;
			}
			Q_strncpyz( key, token, sizeof( key ) );

			token = COM_ParseExt( (char **)&buf, qfalse );
			if ( !token[0] ) {
				strcpy( token, "<NULL>" );
			}
			Info_SetValueForKey( info, key, token );
		}
		infos[count] = (char *)gi.Malloc(strlen(info) + strlen("\\num\\") + strlen(va("%d", MAX_ARENAS)) + 1);
		if (infos[count]) {
			strcpy(infos[count], info);
			count++;
		}
	}
	return count;
}

static void G_LoadArenasFromFile( char *filename ) {
	return;

}

void G_LoadArenas( void ) {
	int			numdirs;
	vmCvar_t	arenasFile;
	char		filename[128];
	char		dirlist[1024];
	char*		dirptr;
	int			i, n;
	int			dirlen;

	g_numArenas = 0;

	gi.Cvar_Register( &arenasFile, "g_arenasFile", "", CVAR_INIT|CVAR_ROM );
	if( *arenasFile.string ) {
		G_LoadArenasFromFile(arenasFile.string);
	}
	else {
		G_LoadArenasFromFile("docs/scripts/arenas.txt");
	}

	numdirs = gi.FS_GetFileList("scripts", ".arena", dirlist, 1024 );
	dirptr  = dirlist;
	for (i = 0; i < numdirs; i++, dirptr += dirlen+1) {
		dirlen = strlen(dirptr);
		strcpy(filename, "docs/scripts/");
		strcat(filename, dirptr);
		G_LoadArenasFromFile(filename);
	}
	gi.Printf( va( "%i arenas parsed\n", g_numArenas ) );
	
	for( n = 0; n < g_numArenas; n++ ) {
		Info_SetValueForKey( g_arenaInfos[n], "num", va( "%i", n ) );
	}
}


const char *G_GetArenaInfoByMap( const char *map ) {
	int			n;

	for( n = 0; n < g_numArenas; n++ ) {
		if( Q_stricmp( (char *)Info_ValueForKey( g_arenaInfos[n], "map" ), map ) == 0 ) {
			return g_arenaInfos[n];
		}
	}

	return NULL;
}

char* Q_strrchr( const char* string, int c ) 
{
	char cc = c;
	char *s;
	char *sp=(char *)0;

	s = (char*)string;

	while (*s)
	{
		if (*s == cc)
			sp = s;
		s++;
	}
	if (cc == 0)
		sp = s;

	return sp;
}


static void PlayerIntroSound( const char *modelAndSkin ) {
	char	model[MAX_QPATH];
	char	*skin;

	Q_strncpyz( model, modelAndSkin, sizeof(model) );
	skin = Q_strrchr( model, '/' );
	if ( skin ) {
		*skin++ = '\0';
	}
	else {
		skin = model;
	}

	if( Q_stricmp( skin, "default" ) == 0 ) {
		skin = model;
	}

}

void G_AddRandomBot( int team ) {
	int		i, n, num;
	float	skill;
	char	*value, netname[36], *teamstr;
	gclient_t	*cl;

	num = 0;
	for ( n = 0; n < g_numBots ; n++ ) {
		value = (char *)Info_ValueForKey( g_botInfos[n], "name" );
		for ( i=0 ; i< maxclients->integer ; i++ ) {
			cl = game.clients + i;
			if ( !(g_entities[cl->ps.clientNum].svflags & SVF_BOT) ) {
				continue;
			}
			if ( !Q_stricmp( value, cl->pers.netname ) ) {
				break;
			}
		}
		if (i >= maxclients->integer) {
			num++;
		}
	}
	num = (int) random() * num;
	for ( n = 0; n < g_numBots ; n++ ) {
		value = (char *)Info_ValueForKey( g_botInfos[n], "name" );
		for ( i=0 ; i< maxclients->integer ; i++ ) {
			cl = game.clients + i;
			if ( !(g_entities[cl->ps.clientNum].svflags & SVF_BOT) ) {
				continue;
			}
			if ( !Q_stricmp( value, cl->pers.netname ) ) {
				break;
			}
		}
		if (i >= maxclients->integer) {
			num--;
			if (num <= 0) {
				skill = gi.Cvar_VariableValue( "g_spSkill" );
				if (team == TEAM_RED) teamstr = "red";
				else if (team == TEAM_BLUE) teamstr = "blue";
				else teamstr = "";
				Q_strncpyz(netname, value, sizeof(netname)-1);
				netname[sizeof(netname)-1] = '\0';
				Q_CleanStr(netname);
				gi.SendConsoleCommand( va("addbot %s %f %s %i\n", netname, skill, teamstr, 0) ); 
				return;
			}
		}
	}
}

int G_RemoveRandomBot( int team ) {
	int i;
	char netname[36];
	gclient_t	*cl;

	for ( i=0 ; i< maxclients->integer ; i++ ) {
		cl = game.clients + i;
		if ( !(g_entities[cl->ps.clientNum].svflags & SVF_BOT) ) {
			continue;
		}
		strcpy(netname, cl->pers.netname);
		Q_CleanStr(netname);
		gi.SendConsoleCommand(va("kick %s\n", netname) ); 
		return qtrue;
	}
	return qfalse;
}

int G_CountHumanPlayers( int team ) {
	int i, num;
	gclient_t	*cl;

	num = 0;
	for ( i=0 ; i< maxclients->integer ; i++ ) {
		cl = game.clients + i;
		if ( g_entities[cl->ps.clientNum].svflags & SVF_BOT ) {
			continue;
		}
		num++;
	}
	return num;
}

int G_CountBotPlayers( int team ) {
	int i, n, num;
	gclient_t	*cl;

	num = 0;
	for ( i=0 ; i< maxclients->integer ; i++ ) {
		cl = game.clients + i;
		if ( !(g_entities[cl->ps.clientNum].svflags & SVF_BOT) ) {
			continue;
		}
		num++;
	}
	for( n = 0; n < BOT_SPAWN_QUEUE_DEPTH; n++ ) {
		if( !botSpawnQueue[n].spawnTime ) {
			continue;
		}
		if ( botSpawnQueue[n].spawnTime > level.time ) {
			continue;
		}
		num++;
	}
	return num;
}

void G_CheckMinimumPlayers( void ) {
	int minplayers;
	int humanplayers, botplayers;
	static int checkminimumplayers_time;

	if (level.intermissiontime) return;
	if (checkminimumplayers_time > level.time - 10000) {
		return;
	}
	checkminimumplayers_time = (int) level.time;
	gi.Cvar_Update(&bot_minplayers);
	minplayers = bot_minplayers.integer;
	if (minplayers <= 0) return;

	if (g_gametype->integer >= GT_TEAM) {
		if (minplayers >= maxclients->integer / 2) {
			minplayers = (maxclients->integer / 2) -1;
		}

		humanplayers = G_CountHumanPlayers( TEAM_RED );
		botplayers = G_CountBotPlayers(	TEAM_RED );
		if (humanplayers + botplayers < minplayers) {
			G_AddRandomBot( TEAM_RED );
		} else if (humanplayers + botplayers > minplayers && botplayers) {
			G_RemoveRandomBot( TEAM_RED );
		}
		humanplayers = G_CountHumanPlayers( TEAM_BLUE );
		botplayers = G_CountBotPlayers( TEAM_BLUE );
		if (humanplayers + botplayers < minplayers) {
			G_AddRandomBot( TEAM_BLUE );
		} else if (humanplayers + botplayers > minplayers && botplayers) {
			G_RemoveRandomBot( TEAM_BLUE );
		}
	}
	else if (g_gametype->integer == GT_TOURNAMENT ) {
		if (minplayers >= maxclients->integer) {
			minplayers = maxclients->integer-1;
		}
		humanplayers = G_CountHumanPlayers( -1 );
		botplayers = G_CountBotPlayers( -1 );
		if (humanplayers + botplayers < minplayers) {
			G_AddRandomBot( TEAM_FREE );
		} else if (humanplayers + botplayers > minplayers && botplayers) {
			if (!G_RemoveRandomBot( TEAM_SPECTATOR )) {
				G_RemoveRandomBot( -1 );
			}
		}
	}
	else if (g_gametype->integer == GT_FFA) {
		if (minplayers >= maxclients->integer) {
			minplayers = maxclients->integer-1;
		}
		humanplayers = G_CountHumanPlayers( TEAM_FREE );
		botplayers = G_CountBotPlayers( TEAM_FREE );
		if (humanplayers + botplayers < minplayers) {
			G_AddRandomBot( TEAM_FREE );
		} else if (humanplayers + botplayers > minplayers && botplayers) {
			G_RemoveRandomBot( TEAM_FREE );
		}
	}
}

void G_CheckBotSpawn( void ) {
	int		n;
	char	userinfo[MAX_INFO_VALUE];

	G_CheckMinimumPlayers();

	for( n = 0; n < BOT_SPAWN_QUEUE_DEPTH; n++ ) {
		if( !botSpawnQueue[n].spawnTime ) {
			continue;
		}
		if ( botSpawnQueue[n].spawnTime > level.time ) {
			continue;
		}
		G_ClientBegin( &g_entities[botSpawnQueue[n].clientNum], NULL );
		botSpawnQueue[n].spawnTime = 0;

		if( g_gametype->integer == GT_SINGLE_PLAYER ) {
			gi.getUserinfo( botSpawnQueue[n].clientNum, userinfo, sizeof(userinfo) );
			PlayerIntroSound( (char *)Info_ValueForKey (userinfo, "model") );
		}
	}
}

void G_RemoveQueuedBotBegin( int clientNum ) {
	int		n;

	for( n = 0; n < BOT_SPAWN_QUEUE_DEPTH; n++ ) {
		if( botSpawnQueue[n].clientNum == clientNum ) {
			botSpawnQueue[n].spawnTime = 0;
			return;
		}
	}
}

int BotAISetupClient(int client, struct bot_settings_s *settings, qboolean restart);

qboolean G_BotConnect( int clientNum, qboolean restart ) {
	bot_settings_t	settings;
	char			userinfo[MAX_INFO_STRING];

	gi.getUserinfo( clientNum, userinfo, sizeof(userinfo) );

	Q_strncpyz( settings.characterfile, (char *)Info_ValueForKey( userinfo, "characterfile" ), sizeof(settings.characterfile) );
	settings.skill = atof( (char *)Info_ValueForKey( userinfo, "skill" ) );
	Q_strncpyz( settings.team, (char *)Info_ValueForKey( userinfo, "team" ), sizeof(settings.team) );

	if (!BotAISetupClient( clientNum, &settings, restart )) {
		gi.DropClient( clientNum, "BotAISetupClient failed" );
		return qfalse;
	}

	return qtrue;
}

char *G_GetBotInfoByName( char *name );

void G_AddBot( char *name, float skill, const char *team, const char *specialty, char *altname) {
	int				clientNum;
	char			*botinfo;
	gentity_t		*bot;
	char			*key;
	char			*s;
	char			*botname;
	char			*model;
	char			*headmodel;
	char			userinfo[MAX_INFO_STRING];

	botinfo = G_GetBotInfoByName( name );
	if ( !botinfo ) {
		gi.Printf( S_COLOR_RED "Error: Bot '%s' not defined\n", name );
		return;
	}

	userinfo[0] = '\0';

	botname = (char *)Info_ValueForKey( botinfo, "funname" );
	if( !botname[0] ) {
		botname = (char *)Info_ValueForKey( botinfo, "name" );
	}
	if (altname && altname[0]) {
		botname = altname;
	}
	Info_SetValueForKey( userinfo, "name", botname );
	Info_SetValueForKey( userinfo, "rate", "25000" );
	Info_SetValueForKey( userinfo, "snaps", "20" );
	Info_SetValueForKey( userinfo, "skill", va("%1.2f", skill) );

	if ( skill >= 1 && skill < 2 ) {
		Info_SetValueForKey( userinfo, "handicap", "50" );
	}
	else if ( skill >= 2 && skill < 3 ) {
		Info_SetValueForKey( userinfo, "handicap", "70" );
	}
	else if ( skill >= 3 && skill < 4 ) {
		Info_SetValueForKey( userinfo, "handicap", "90" );
	}

	key = "mp_playermodel";
	model = (char *)Info_ValueForKey( botinfo, key );
	if ( !*model ) {
		model = "models/char/munro.tik";
	}
	Info_SetValueForKey( userinfo, key, model );
	key = "team_model";
	Info_SetValueForKey( userinfo, key, model );

	key = "headmodel";
	headmodel = (char *)Info_ValueForKey( botinfo, key );
	if ( !*headmodel ) {
		headmodel = model;
	}
	Info_SetValueForKey( userinfo, key, headmodel );
	key = "team_headmodel";
	Info_SetValueForKey( userinfo, key, headmodel );

	key = "gender";
	s = (char *)Info_ValueForKey( botinfo, key );
	if ( !*s ) {
		s = "male";
	}
	Info_SetValueForKey( userinfo, "sex", s );

	key = "color1";
	s = (char *)Info_ValueForKey( botinfo, key );
	if ( !*s ) {
		s = "4";
	}
	Info_SetValueForKey( userinfo, key, s );

	key = "color2";
	s = (char *)Info_ValueForKey( botinfo, key );
	if ( !*s ) {
		s = "5";
	}
	Info_SetValueForKey( userinfo, key, s );

	s = (char *)Info_ValueForKey(botinfo, "aifile");
	if (!*s ) {
		gi.Printf( S_COLOR_RED "Error: bot has no aifile specified\n" );
		return;
	}

	clientNum = gi.BotAllocateClient();
	if ( clientNum == -1 ) {
		gi.Printf( S_COLOR_RED "Unable to add bot.  All player slots are in use.\n" );
		gi.Printf( S_COLOR_RED "Start server with more 'open' slots (or check setting of sv_maxclients cvar).\n" );
		return;
	}

	bot = &g_entities[ clientNum ];

	Info_SetValueForKey( userinfo, "characterfile", (char *)Info_ValueForKey( botinfo, "aifile" ) );
	Info_SetValueForKey( userinfo, "skill", va( "%5.2f", skill ) );
	
	if(strlen(password->string) != 0)
	{
		Info_SetValueForKey( userinfo, "password", password->string );
	}

	gi.setUserinfo( clientNum, userinfo );

if ( G_ClientConnect( clientNum, qtrue, qtrue, qtrue ) ) {
		return;
	}

	G_ClientBegin( &g_entities[clientNum],NULL );

	g_entities[clientNum].svflags |= SVF_BOT;

	Player *plyr = (Player *)g_entities[clientNum].entity;
	char targetname[11];
	sprintf(targetname,"player%d",clientNum);
	plyr->SetTargetName(targetname);

	if ( multiplayerManager.checkGameType("teamdm") || 
		multiplayerManager.checkGameType("destruction") || 
		multiplayerManager.checkGameType("ctf") ) {

		Player *player = (Player *)bot->entity;

		if ((team) && (*team))
			multiplayerManager.joinTeam( player, team );

		char teamchar[4];
		if (multiplayerManager.getPlayersTeam(player)->getName() == "Blue") { 
			team = "blue";
			sprintf(teamchar,"%d",TEAM_BLUE);
		}
		else {
			team = "red";
			sprintf(teamchar,"%d",TEAM_RED);
		}
	
		if ((specialty) && (*specialty)) {
			player->edict->svflags |= SVF_BOT;
			multiplayerManager.playerCommand( player, "setSpecialty", specialty );
		}

		Info_SetValueForKey( userinfo, "team", team );
		Info_SetValueForKey( userinfo, "t", teamchar );
		gi.setUserinfo( clientNum, userinfo );

	}
	
	return;
}


void SV_AddBot_f( void ) {
	float			skill;
	char			name[MAX_TOKEN_CHARS];
	char			altname[MAX_TOKEN_CHARS];
	char			string[MAX_TOKEN_CHARS];
	char			team[MAX_TOKEN_CHARS];
	char			specialty[MAX_TOKEN_CHARS];

	PROBE(
		"npc:addbot-entry",
		"argc=%d a1='%s' a2='%s' a3='%s' a4='%s' bot_enable=%d",
		gi.argc(),
		gi.argv(1),
		gi.argv(2),
		gi.argv(3),
		gi.argv(4),
		gi.Cvar_VariableIntegerValue("bot_enable")
	);

	if ( !gi.Cvar_VariableIntegerValue( "bot_enable" ) ) {
		PROBE("npc:addbot-blocked", "bot_enable=0 -- bot refused");
		return;
	}

	Q_strncpyz(name, gi.argv( 1), sizeof(name));
	if ( !name[0] ) {
		gi.Printf( "Usage: Addbot <botname> [skill 1-5] [team] [specialty (infiltrator, medic, technician, demolitionist, heavyweapons, sniper)] [altname]\n" );
		return;
	}

	Q_strncpyz(string,gi.argv( 2),sizeof(string));
	if ( !string[0] ) {
		skill = 4;
	}
	else {
		skill = atof( string );
	}

	Q_strncpyz(team,gi.argv( 3),sizeof(team));

	Q_strncpyz(specialty,gi.argv(4),sizeof(specialty));

	Q_strncpyz(altname,gi.argv( 5),sizeof(altname));

	G_AddBot( name, skill, team, specialty, altname );
}

void Svcmd_BotList_f( void ) {
	int i;
	char name[MAX_TOKEN_CHARS];
	char funname[MAX_TOKEN_CHARS];
	char model[MAX_TOKEN_CHARS];
	char aifile[MAX_TOKEN_CHARS];

	gi.Printf("^1name             model            aifile              funname\n");
	for (i = 0; i < g_numBots; i++) {
		strcpy(name, (char *)Info_ValueForKey( g_botInfos[i], "name" ));
		if ( !*name ) {
			strcpy(name, "UnnamedPlayer");
		}
		strcpy(funname, (char *)Info_ValueForKey( g_botInfos[i], "funname" ));
		if ( !*funname ) {
			strcpy(funname, "");
		}
		strcpy(model, (char *)Info_ValueForKey( g_botInfos[i], "model" ));
		if ( !*model ) {
			strcpy(model, "visor/default");
		}
		strcpy(aifile, (char *)Info_ValueForKey( g_botInfos[i], "aifile"));
		if (!*aifile ) {
			strcpy(aifile, "bots/default_c.c");
		}
		gi.Printf(va("%-16s %-16s %-20s %-20s\n", name, model, aifile, funname));
	}
}


void G_SpawnBots( char *botList, int baseDelay ) {
	char		*bot;
	char		*p;
	float		skill;
	int			delay;
	char		bots[MAX_INFO_VALUE];


	skill = gi.Cvar_VariableValue( "g_spSkill" );
	if( skill < 1 ) {
		gi.cvar_set( "g_spSkill", "1" );
		skill = 1;
	}
	else if ( skill > 5 ) {
		gi.cvar_set( "g_spSkill", "5" );
		skill = 5;
	}

	Q_strncpyz( bots, botList, sizeof(bots) );
	p = &bots[0];
	delay = baseDelay;
	while( *p ) {
		while( *p && *p == ' ' ) {
			p++;
		}
		if( !p ) {
			break;
		}

		bot = p;

		while( *p && *p != ' ' ) {
			p++;
		}
		if( *p ) {
			*p++ = 0;
		}

		gi.SendConsoleCommand(va("addbot %s %f free %i\n", bot, skill, delay) ); 

		delay += BOT_BEGIN_DELAY_INCREMENT;
	}
}


void G_LoadBotsFromFile( char *filename ) {
	int				len;
	fileHandle_t	f;
	char			buf[MAX_BOTS_TEXT];

	len = gi.FS_FOpenFile( filename, &f, FS_READ );
	if ( !f ) {
		gi.Printf( va( S_COLOR_RED "file not found: %s\n", filename ) );
		return;
	}
	if ( len >= MAX_BOTS_TEXT ) {
		gi.Printf( va( S_COLOR_RED "file too large: %s is %i, max allowed is %i", filename, len, MAX_BOTS_TEXT ) );
		gi.FS_FCloseFile( f );
		return;
	}

	gi.FS_Read( buf, len, f );
	buf[len] = 0;
	gi.FS_FCloseFile( f );

	g_numBots += G_ParseInfos( buf, MAX_BOTS - g_numBots, &g_botInfos[g_numBots] );
}

void G_LoadBots( void ) {
	vmCvar_t	botsFile;
	int			numdirs;
	char		filename[128];
	char		dirlist[1024];
	char*		dirptr;
	int			i;
	int			dirlen;

	if ( !gi.Cvar_VariableIntegerValue( "bot_enable" ) ) {
		return;
	}

	g_numBots = 0;

	gi.Cvar_Register( &botsFile, "g_botsFile", "", CVAR_INIT|CVAR_ROM );
	if( *botsFile.string ) {
		G_LoadBotsFromFile(botsFile.string);
	}
	else {
		G_LoadBotsFromFile("botfiles/bots.txt");
	}

	numdirs = gi.FS_GetFileList("scripts", ".bot", dirlist, 1024 );
	dirptr  = dirlist;
	for (i = 0; i < numdirs; i++, dirptr += dirlen+1) {
		dirlen = strlen(dirptr);
		strcpy(filename, "docs/scripts/");
		strcat(filename, dirptr);
		G_LoadBotsFromFile(filename);
	}
	gi.Printf( va( "%i bots parsed\n", g_numBots ) );
}



char *G_GetBotInfoByNumber( int num ) {
	if( num < 0 || num >= g_numBots ) {
		gi.Printf( va( S_COLOR_RED "Invalid bot number: %i\n", num ) );
		return NULL;
	}
	return g_botInfos[num];
}


char *G_GetBotInfoByName( char *name ) {
	int		n;
	char	*value;

	for ( n = 0; n < g_numBots ; n++ ) {
		value = (char *)Info_ValueForKey( g_botInfos[n], "name" );
		if ( !Q_stricmp( value, name ) ) {
			return g_botInfos[n];
		}
	}

	return NULL;
}

void G_InitBots( qboolean restart ) {
	int			fragLimit;
	int			timeLimit;
	const char	*arenainfo;
	char		*strValue;
	int			basedelay;
	char		map[MAX_QPATH];
	char		serverinfo[MAX_INFO_STRING];

	G_LoadBots();
	G_LoadArenas();

	gi.Cvar_Register( &bot_minplayers, "bot_minplayers", "0", CVAR_SERVERINFO );

	if( g_gametype->integer == GT_SINGLE_PLAYER ) {
		gi.SV_GetServerinfo( serverinfo, sizeof(serverinfo) );
		Q_strncpyz( map, (char *)Info_ValueForKey( serverinfo, "mapname" ), sizeof(map) );
		arenainfo = G_GetArenaInfoByMap( map );
		if ( !arenainfo ) {
			return;
		}

		strValue = (char *)Info_ValueForKey( arenainfo, "fraglimit" );
		fragLimit = atoi( strValue );
		if ( fragLimit ) {
			gi.cvar_set( "fraglimit", strValue );
		}
		else {
			gi.cvar_set( "fraglimit", "0" );
		}

		strValue = (char *)Info_ValueForKey( arenainfo, "timelimit" );
		timeLimit = atoi( strValue );
		if ( timeLimit ) {
			gi.cvar_set( "timelimit", strValue );
		}
		else {
			gi.cvar_set( "timelimit", "0" );
		}

		if ( !fragLimit && !timeLimit ) {
			gi.cvar_set( "fraglimit", "10" );
			gi.cvar_set( "timelimit", "0" );
		}

		basedelay = BOT_BEGIN_DELAY_BASE;
		strValue = (char *)Info_ValueForKey( arenainfo, "special" );
		if( Q_stricmp( strValue, "training" ) == 0 ) {
			basedelay += 10000;
		}

		if( !restart ) {
			G_SpawnBots( (char *)Info_ValueForKey( arenainfo, "bots" ), basedelay );
		}
	}
}
