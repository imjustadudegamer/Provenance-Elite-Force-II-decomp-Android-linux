
#include "server.h"

char *Com_GetGameTypeName( int gametype, char *buf, int bufsize );

#ifndef DEDICATED
#    include "../client/client.h"
#    include "../client/snd_local.h"
#    include "../uilib/ui_public.h"
#endif



static int sv_saveStatus = 2;    
static int sv_loadStatus = 2;    


static client_t *SV_SetPlayer( void ) {
	client_t	*client;
	int			i;
	char		*s;
	char		cleanName[64];

	if ( !com_sv_running->integer ) {
		return NULL;
	}

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "No player specified.\n" );
		return NULL;
	}

	s = Cmd_Argv(1);

	for ( i=0, client=svs.clients ; i < sv_maxclients->integer ; i++,client++ ) {
		if ( !client->state ) {
			continue;
		}
		if ( !Q_stricmp( client->name, s ) ) {
			return client;
		}

		Q_strncpyz( cleanName, client->name, sizeof(cleanName) );
		Q_CleanStr( cleanName );
		if ( !Q_stricmp( cleanName, s ) ) {
			return client;
		}
	}

	if ( s[0] < '0' || s[0] > '9' ) {
		Com_Printf( "User %s is not on the server\n", s );
		return NULL;
	}

	i = atoi( s );

	if ( i < 0 || i >= sv_maxclients->integer ) {
		Com_Printf( "Bad client slot: %i\n", i );
		return NULL;
	}

	client = &svs.clients[i];

	if ( !client->state ) {
		Com_Printf( "Client %i is not active\n", i );
		return NULL;
	}

	return client;
}

static void SV_Map_f( void ) {
	char		expanded[ 64 ];
	char		map[ 64 ];
	char		mapname[ 68 ];
	const char	*hash;
	const char	*dollar;

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "USAGE: map <map>\n" );
		return;
	}

	if ( developer->integer && *fs_mapdir->string ) {
		Com_sprintf( map, sizeof( map ), "%s/%s", fs_mapdir->string, Cmd_Argv( 1 ) );
	} else {
		Com_sprintf( map, sizeof( map ), "%s", Cmd_Argv( 1 ) );
	}

	Com_BackslashToSlash( map );

	dollar = strchr( map, '$' );
	hash   = strchr( map, '#' );
	if ( dollar ) {
		Q_strncpyz( mapname, map, dollar - map + 1 );
	} else if ( hash ) {
		Q_strncpyz( mapname, map, hash - map + 1 );
	} else {
		Q_strncpyz( mapname, map, sizeof( mapname ) );
	}

	Com_sprintf( expanded, sizeof( expanded ), "maps/%s.bsp", mapname );
	if ( FS_ReadFile( expanded, NULL ) == -1 ) {
		Com_WPrintf( "Can't find map %s\n", expanded );
		return;
	}

	Cvar_Get( "g_gametype", "0", CVAR_LATCH | CVAR_SERVERINFO );

	if ( !developer->integer ) {
		if ( !strcmp( Cmd_Argv( 0 ), "devmap" ) ) {
			Cvar_Set( "cheats", "1" );
		} else {
			Cvar_Set( "cheats", "0" );
		}
	}

	Com_WipeSavegame( "current", qtrue );

	SV_DeleteTempSublevelFiles();

	SV_SpawnServer( map, qfalse, qfalse, qfalse, qfalse );
}

static void SV_GameMap_f( void ) {
	char		mapname[ 512 ];
	char		mapbase[ 512 ];
	char		levelname[ 512 ];
	const char	*sublevel;
	qboolean	bTransition;

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "USAGE: gamemap <map>\n" );
		return;
	}

	Com_DPrintf( "SV_GameMap(%s)\n", Cmd_Argv( 1 ) );

	Q_strncpyz( svs.gameName, "current", sizeof( svs.gameName ) );

	Q_strncpyz( mapname, Cmd_Argv( 1 ), sizeof( mapname ) );
	SV_ParseMapName( mapname, mapbase, NULL, NULL );

	Q_strncpyz( levelname, svs.mapName, sizeof( levelname ) );
	bTransition = SV_areSublevels( levelname, mapbase );

	if ( mapname[ 0 ] == '*' ) {
		Com_WipeSavegame( "current", qtrue );
	} else if ( sv.state == SS_GAME ) {
		SV_ArchivePersistantFile( qfalse, bTransition );
	}

	if ( bTransition ) {
		PF_GetLevelDefs( levelname, NULL, NULL, &sublevel );
		SV_SaveSublevel( sublevel );

		if ( SV_LoadSublevel( mapname ) ) {
			return;
		}

		Q_strncpyz( svs.gameName, "current", sizeof( svs.gameName ) );
	} else {
		SV_DeleteTempSublevelFiles();
	}

	SV_SpawnServer( mapname, qfalse, qfalse, qfalse, bTransition );

	if ( !com_dedicated->integer ) {
		if ( !g_gametype->integer ) {
			sv_bAutoSaveRequested = qtrue;
			Cvar_Set( "com_canAutoSaveNow", "0" );
		}
	}
}

static void SV_MapRestart_f( void ) {
	if ( com_frameTime == sv.serverId ) {
		return;
	}

	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( SV_LoadSublevel( svs.rawServerName ) ) {
		return;
	}

	Q_strncpyz( svs.gameName, "current", sizeof( svs.gameName ) );
	SV_SpawnServer( svs.rawServerName, qfalse, qtrue, qfalse, qfalse );
}


static void SV_Kick_f( void ) {
	client_t	*client;

	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	//   0x1D5CD8  Com_Printf(pool+294)          "Can not kick host player\n"
	// port-only; retail rejects any argument count but exactly 2, and prints the
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "Usage: kick <user>\n" );
		return;
	}

	client = SV_SetPlayer();
	if ( !client ) {
		return;
	}

	if ( client->netchan.remoteAddress.type == NA_LOOPBACK ) {
		Com_Printf( "Can not kick host player\n" );
		return;
	}

	SV_DropClient( client, va( "%s $$WasKicked$$", client->name ), qfalse );

	client->lastPacketTime = svs.time;	
}
static int SV_Strlen( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
			count++;
			s++;
	}

	return count;
}

static void SV_Status_f(void) {
	int				i;
	size_t			j, l;
	client_t		*client;
	playerState_t	*ps;
	const char*		s;
	const char		*colName[8];
	size_t			len;
	unsigned int	colSize[8];
	int				ping;
	char			padding[64];

	if (!com_sv_running->integer) {
		Com_Printf("Server is not running.\n");
		return;
	}


	colName[0] = "num";
	colSize[0] = strlen(colName[0]);
	colName[1] = "score";
	colSize[1] = strlen(colName[1]);
	colName[2] = "ping";
	colSize[2] = strlen(colName[2]);
	colName[3] = "name";
	colSize[3] = 15;
	colName[4] = "lastmsg";
	colSize[4] = 7;
	colName[5] = "address";
	colSize[5] = 21;
	colName[6] = "qport";
	colSize[6] = 5;
	colName[7] = "rate";
	colSize[7] = 5;

	Com_Printf("map: %s\n", sv_mapname->string);


	for (i = 0; i < 8; i++) {
		len = strlen(colName[i]);
		Com_Printf("%s", colName[i]);

		for (j = 0; j < (ARRAY_LEN(padding) - 1) && (colSize[i] - j >= len); j++) {
			padding[j] = ' ';
		}
		padding[j] = 0;

		if (j > 0) {
			Com_Printf("%s", padding);
		}
	}
	Com_Printf("\n");


	for (i = 0; i < 8; i++) {
		for (j = 0; j < (ARRAY_LEN(padding) - 1) && (j < colSize[i]); j++) {
			padding[j] = '-';
		}
		padding[j] = 0;

		Com_Printf("%s ", padding);
	}
	Com_Printf("\n");


	for (i = 0, client = svs.clients; i < sv_maxclients->integer; i++, client++)
	{
		if (client->state == CS_FREE) {
			continue;
		}

		Com_Printf("%*u ", colSize[0], i);
		ps = SV_GameClientNum(i);
		Com_Printf("%*i ", colSize[1], ps->stats[STAT_SCORE]);

		if (client->state == CS_CONNECTED)
			Com_Printf("CNCT ");
		else if (client->state == CS_ZOMBIE)
			Com_Printf("ZMBI ");
		else
		{
			ping = client->ping < 9999 ? client->ping : 9999;
			Com_Printf("%*u ", colSize[2], ping);
		}

		Com_Printf("%s ", client->name);
		l = SV_Strlen(client->name);
		if (l < colSize[3]) {
			l = colSize[3] - l;
			for (j = 0; j < l; j++) {
				Com_Printf(" ");
			}
		}

		Com_Printf("%*u ", colSize[4], svs.time - client->lastPacketTime);

		s = NET_AdrToLogString(client->netchan.remoteAddress);
		Com_Printf("%s ", s);

		len = strlen(s);
		if (len < colSize[5]) {
			l = colSize[5] - strlen(s);
			for (j = 0; j < l; j++) {
				Com_Printf(" ");
			}
		}

		Com_Printf( "%*u", colSize[6], client->netchan.qport );

		Com_Printf(" %*u", colSize[7], client->rate);

		Com_Printf("\n");
	}
	Com_Printf("\n");
}

static void SV_ConSay_f(void) {
	char	*p;
	char	text[1024];

	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc () < 2 ) {
		return;
	}

	Q_strncpyz (text, "console: ", sizeof(text));
	p = Cmd_Args();

	if ( *p == '"' ) {
		p++;
		p[strlen(p)-1] = 0;
	}

	strcat(text, p);

	Com_Printf("%s\n", text);
	SV_SendServerCommand(NULL, "print \"" HUD_MESSAGE_CHAT_WHITE "%s\n\"", text);
}

void SV_Nextgametype_f( void ) {
	char	name[256];
	int		gametype;

	gametype = mp_gametype->integer + 1;
	if ( gametype > 3 ) {
		gametype = 0;
	}

	Cvar_Set( "mp_gametype", va( "%d", gametype ) );
	Com_GetGameTypeName( gametype, name, sizeof( name ) );
	Cvar_Set( "mp_gametypename", name );
}

void SV_Setgametype_f( void ) {
	char	name[256];
	int		gametype;

	gametype = atoi( Cmd_Argv( 1 ) );
	if ( gametype > 3 ) {
		gametype = 0;
	}

	Cvar_Set( "mp_gametype", va( "%d", gametype ) );
	Com_GetGameTypeName( gametype, name, sizeof( name ) );
	Cvar_Set( "mp_gametypename", name );
}

void SV_SetTempGameType_f( void ) {
	char	name[256];
	int		gametype;

	gametype = atoi( Cmd_Argv( 1 ) );
	if ( gametype > 3 ) {
		gametype = 0;
	}

	Cvar_Set( "temp_mp_gametype", va( "%d", gametype ) );
	Com_GetGameTypeName( gametype, name, sizeof( name ) );
	Cvar_Set( "temp_mp_gametypename", name );
}

void SV_AddBot_f( void ) {
	if ( !ge ) {
		return;
	}

	if ( !com_sv_running->integer ) {
		return;
	}

	if ( ge->AddBot_f ) {
		ge->AddBot_f();
	}
}

void SV_Heartbeat_f( void ) {
	svs.nextHeartbeatTime = -9999999;
}


static void SV_Serverinfo_f( void ) {
	Com_Printf ("Server info settings:\n");
	Info_Print ( Cvar_InfoString( CVAR_SERVERINFO ) );
}


static void SV_Systeminfo_f( void ) {
	Com_Printf ("System info settings:\n");
	Info_Print ( Cvar_InfoString( CVAR_SYSTEMINFO ) );
}


static void SV_DumpUser_f( void ) {
	client_t	*client;

	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: info <userid>\n");
		return;
	}

	client = SV_SetPlayer();
	if ( !client ) {
		return;
	}

	Com_Printf( "userinfo\n" );
	Com_Printf( "--------\n" );
	Info_Print( client->userinfo );
}

static void SV_KillServer_f( void ) {
	SV_Shutdown( "killserver" );
}




static Q_UNUSED_VAR void SV_CompleteMapName( char *args, int argNum ) {
	if( argNum == 2 ) {
		Field_CompleteFilename( "maps", "bsp", qtrue, qfalse );
	}
}

#if 0

static void SV_TIKI_f( void ) {
	char		*fname;
	fname = Cmd_Argv(1);
	if(TIKI_RegisterModel(fname)==0)
	{
		char tmp[128];
		Q_strncpyz(tmp,"models/",sizeof(tmp));
		Q_strcat(tmp,sizeof(tmp),fname);
		TIKI_RegisterModel(tmp);
	}
}

static void SV_TIKI_DumpBones_f( void ) {
	char		*fname;
	tiki_t		*tiki;
	int i;
	int filter = -1;

	fname = Cmd_Argv(1);
	tiki = TIKI_RegisterModel(fname);
	if(tiki==0)
	{
		char tmp[128];
		Q_strncpyz(tmp,"models/",sizeof(tmp));
		Q_strcat(tmp,sizeof(tmp),fname);
		tiki = TIKI_RegisterModel(tmp);
		if(!tiki)
			return;
	}
	if(Cmd_Argc() > 2) {
		filter = atoi(Cmd_Argv(2));
		if(filter < 0 || filter > 6) {
			filter = -1;
		}
	}
	for(i = 0; i < tiki->numBones; i++) {
		if(filter != -1 && *((int*)tiki->bones[i]) != filter)
			continue;
		Com_Printf("Bone %i of %i ",i,tiki->numBones);
		TIKI_PrintBoneInfo(tiki,i);
	}
}

#endif


void SV_AddOperatorCommands(void) {
	static qboolean	initialized;

	if (initialized) {
		return;
	}
	initialized = qtrue;

	Cmd_AddCommand("killserver", SV_KillServer_f);
	Cmd_AddCommand("heartbeat", SV_Heartbeat_f);
	Cmd_AddCommand("kick", SV_Kick_f);
	Cmd_AddCommand("status", SV_Status_f);
	Cmd_AddCommand("serverinfo", SV_Serverinfo_f);
	Cmd_AddCommand("systeminfo", SV_Systeminfo_f);
	Cmd_AddCommand("dumpuser", SV_DumpUser_f);
	Cmd_AddCommand("restart", SV_MapRestart_f);
	Cmd_AddCommand("sectorlist", SV_SectorList_f);
	Cmd_AddCommand("map", SV_Map_f);
	Cmd_AddCommand("devmap", SV_Map_f);
	Cmd_AddCommand("gamemap", SV_GameMap_f);
	Cmd_AddCommand("savegame", SV_Savegame_f);
	Cmd_AddCommand("loadgame", SV_Loadgame_f);
	Cmd_AddCommand("loadbrokengame", SV_Loadbrokengame_f);
	Cmd_AddCommand("nextgametype", SV_Nextgametype_f);
	Cmd_AddCommand("setgametype", SV_Setgametype_f);
	Cmd_AddCommand("settempgametype", SV_SetTempGameType_f);
	Cmd_AddCommand("addbot", SV_AddBot_f);

	if (com_dedicated->integer) {
		Cmd_AddCommand("say", SV_ConSay_f);
	}
}

void SV_RemoveOperatorCommands(void) {
	// the MOHAA commands `banUser' / `banClient' / `map_restart' that retail never registers
}

qboolean SV_ArchivePersistantFile(qboolean loading, qboolean sublevelTransition)
{
	const char *name;

	// It is also not retail.  Retail EF2 ships ONE binary (EF2.exe); there is no
	Com_DPrintf("SV_ArchivePersistantFile()\n");
	name = Com_GetArchiveFileName(svs.gameName, svs.gameName, "spv", qtrue);

	if (loading) {
		return ge->ReadPersistant(name, sublevelTransition);
	}

	ge->WritePersistant(name, sublevelTransition);
	return qtrue;
}

qboolean SV_ArchiveLevelFile( const char *levelExt, const char *serverExt, qboolean loading )
{
	const char      *name;
	fileHandle_t     f;
	savegamestruct_t save;
	qboolean         tempFile;

	Com_DPrintf( "SV_ArchiveLevelFile()\n" );

	tempFile = Q_stricmp( serverExt, "ssv" ) != 0;
	name     = Com_GetArchiveFileName( svs.gameName, svs.gameName, levelExt, tempFile );

	if ( loading ) {
		if ( !ge->ReadLevel( name ) ) {
			return qfalse;
		}

		name = Com_GetArchiveFileName( svs.gameName, svs.gameName, serverExt, tempFile );
		FS_FOpenFileRead( name, &f, qfalse, qtrue );
		if ( f ) {
			FS_Read( &save, sizeof( savegamestruct_t ), f );
			CM_ReadPortalState( f );
			FS_FCloseFile( f );
		}

		return qtrue;
	}

	ge->WriteLevel( name, qfalse );

	return qtrue;
}

void SV_CopyArchiveFile( const char *src, const char *dst )
{
	void *buf;
	long  len;

	if ( !src || !dst || !src[0] || !dst[0] ) {
		return;
	}

	len = FS_ReadFileEx( src, &buf, qtrue );
	if ( len <= 0 || !buf ) {
		return;
	}

	FS_WriteFile( dst, buf, len );
	FS_FreeFile( buf );
}

void SV_CopySublevelFiles( qboolean slotToLive )
{
	const char **list;
	const char  *folder;
	char         src[ 1024 ];
	void        *buf;
	long         len;
	int          numfiles;
	int          i;

	folder = Com_GetArchiveFileName( svs.gameName, NULL, NULL, slotToLive ? qfalse : qtrue );
	list   = (const char **)FS_ListFiles( folder, "sub", qfalse, &numfiles );

	for ( i = 0; i < numfiles; i++ ) {
		COM_StripExtension( list[ i ], src, sizeof( src ) );

		len = FS_ReadFileEx( Com_GetArchiveFileName( svs.gameName, src, "sub", slotToLive ? qfalse : qtrue ),
		                     &buf, qtrue );
		if ( len > 0 && buf ) {
			FS_WriteFile( Com_GetArchiveFileName( svs.gameName, src, "sub", slotToLive ? qtrue : qfalse ),
			              buf, len );
			FS_FreeFile( buf );
		}

		len = FS_ReadFileEx( Com_GetArchiveFileName( svs.gameName, src, "ssb", slotToLive ? qfalse : qtrue ),
		                     &buf, qtrue );
		if ( len > 0 && buf ) {
			FS_WriteFile( Com_GetArchiveFileName( svs.gameName, src, "ssb", slotToLive ? qtrue : qfalse ),
			              buf, len );
			FS_FreeFile( buf );
		}

		len = FS_ReadFileEx( Com_GetArchiveFileName( svs.gameName, src, "tga", slotToLive ? qfalse : qtrue ),
		                     &buf, qtrue );
		if ( len > 0 && buf ) {
			FS_WriteFile( Com_GetArchiveFileName( svs.gameName, src, "tga", slotToLive ? qtrue : qfalse ),
			              buf, len );
			FS_FreeFile( buf );
		}
	}

	FS_FreeFileList( (char **)list );
}

void SV_ParseMapName( const char *name, char *base, char *sublevel, char *rest )
{
	char         folded[ 64 ];
	const char  *hash;
	const char  *dollar;
	char        *slash;
	int          i;

	Q_strncpyz( folded, name ? name : "", sizeof( folded ) );
	for ( slash = folded; *slash; slash++ ) {
		if ( *slash == '\\' ) {
			*slash = '/';
		}
	}

	hash   = strchr( folded, '#' );
	dollar = strchr( folded, '$' );

	if ( hash ) {
		if ( rest ) {
			strcpy( rest, hash );
		}
	} else if ( rest ) {
		strcpy( rest, "0" );
	}

	if ( dollar ) {
		i = dollar - folded;
		if ( i < 1 ) {
			Com_Error( ERR_DROP, "SV_ParseMapName: bad map name %s", name );
		}
		strncpy( base, folded, i );
		base[ i ] = 0;
	} else if ( hash ) {
		i = hash - folded;
		if ( i < 1 ) {
			Com_Error( ERR_DROP, "SV_ParseMapName: bad map name %s", name );
		}
		strncpy( base, folded, i );
		base[ i ] = 0;
	} else {
		strcpy( base, folded );
	}

	if ( sublevel ) {
		if ( dollar ) {
			if ( hash ) {
				i = hash - dollar;
				strncpy( sublevel, dollar, i );
				sublevel[ i ] = 0;
			} else {
				strcpy( sublevel, dollar );
			}
		} else {
			strcpy( sublevel, "0" );
		}
	}
}

qboolean SV_areSublevels( const char *oldMapName, const char *newMapName )
{
	char        oldBase[ 64 ];
	char        newBase[ 64 ];
	const char *oldEnv, *oldLevel, *oldSub;
	const char *newEnv, *newLevel, *newSub;

	SV_ParseMapName( oldMapName, oldBase, NULL, NULL );
	SV_ParseMapName( newMapName, newBase, NULL, NULL );

	PF_GetLevelDefs( oldBase, &oldEnv, &oldLevel, &oldSub );
	PF_GetLevelDefs( newBase, &newEnv, &newLevel, &newSub );

	return !Q_stricmp( oldEnv, newEnv ) && !Q_stricmp( oldLevel, newLevel )
	       && Q_stricmp( oldSub, newSub ) != 0;
}

void SV_SaveSublevel( const char *sublevelName )
{
	char name[ 512 ];

	if ( Cvar_VariableValue( "deathmatch" ) != 0 ) {
		Com_Printf( "Can't savegame in a deathmatch\n" );
		return;
	}

	if ( !svs.clients || !svs.clients->gentity || !svs.clients->gentity->client
	     || !svs.clients->gentity->client->ps.stats[ 0 ] ) {
		Com_Printf( "Can't savegame when dead\n" );
		return;
	}

	if ( !sublevelName || !sublevelName[ 0 ] ) {
		return;
	}

	Com_sprintf( name, sizeof( name ), "%s", sublevelName );
	Com_DPrintf( "Saving sublevel...%s\n", name );

	Q_strncpyz( svs.gameName, name, sizeof( svs.gameName ) );

	SV_ArchiveLevelFile( "sub", "ssb", qfalse );
	SV_ArchiveServerFile( "ssb", qfalse, qfalse );

	Com_Printf( "Done.\n" );

	Q_strncpyz( svs.gameName, "current", sizeof( svs.gameName ) );
}

qboolean SV_LoadSublevel( const char *mapname )
{
	char         base[ 512 ];
	const char  *name;

	if ( sv_maxclients->integer > 1 ) {
		return qfalse;
	}

	if ( !mapname || !mapname[ 0 ] ) {
		return qfalse;
	}

	SV_ParseMapName( mapname, base, NULL, NULL );

	Com_Printf( "Loading sublevel...%s\n", mapname );

	Q_strncpyz( svs.gameName, base, sizeof( svs.gameName ) );

	name = Com_GetArchiveFileName( base, base, "sub", qtrue );
	if ( FS_ReadFileEx( name, NULL, qtrue ) == -1 ) {
		return qfalse;
	}

	if ( !SV_ArchiveServerFile( "ssb", qtrue, qfalse ) ) {
		return qfalse;
	}

	Q_strncpyz( svs.mapName, mapname, sizeof( svs.mapName ) );
	SV_SpawnServer( svs.mapName, qtrue, qfalse, qtrue, qtrue );

	return qtrue;
}


// engine symbol table (the retail sound system is MILES, not this OpenAL layer).

qboolean SV_ArchiveServerFile( const char *serverExt, qboolean loading, qboolean autosave )
{
	fileHandle_t     f;
	const char      *name;
	char             comment[64];
	savegamestruct_t save;
	char             cmdString[256];
	time_t           aclock;
	qboolean         tempFile;

	Com_DPrintf( "SV_ArchiveServerFile(%s)\n", autosave ? "true" : "false" );
	memset( &save, 0, sizeof( save ) );

	tempFile = Q_stricmp( serverExt, "ssv" ) != 0;
	name     = Com_GetArchiveFileName( svs.gameName, svs.gameName, serverExt, tempFile );

	if ( !loading ) {
		f = FS_FOpenFileWrite_HomeData( name );
		if ( !f ) {
			Com_WPrintf( "Couldn't write %s\n", name );
			return qfalse;
		}

		time( &aclock );

		// EF2 does not have -- retail carries the name in the `savegamename` cvar the
		if ( autosave ) {
			Com_sprintf( comment, sizeof( comment ), "%s %s", "Starting", sv.configstrings[ CS_NAME ] );
		} else {
			const char *savename = Cvar_VariableString( "savegamename" );

			if ( savename && savename[ 0 ] ) {
				Com_sprintf( comment, sizeof( comment ), "%s", savename );
				Cvar_Set( "savegamename", "" );
			} else {
				Com_sprintf( comment, sizeof( comment ), "%s", sv.configstrings[ CS_NAME ] );
			}
		}

		Q_strncpyz( save.comment, comment, sizeof( save.comment ) );

		save.time = aclock;
		save.svsTime = svs.time;
		save.secondsPlayed = (int) ( (float) ge->GetTotalGameFrames() * 0.05f );
		Q_strncpyz( save.mapName, svs.mapName, sizeof( save.mapName ) );
		Q_strncpyz( save.saveName, svs.gameName, sizeof( save.saveName ) );

		FS_Write( &save, sizeof( savegamestruct_t ), f );
		CM_WritePortalState( f );
		FS_FCloseFile( f );

		name = Com_GetArchiveFileName( svs.gameName, svs.gameName, "tga", tempFile );
		Com_sprintf( cmdString, sizeof( cmdString ), "saveshot %s 256 256\n", name );
		Cbuf_ExecuteText( EXEC_NOW, cmdString );
	} else {
		FS_FOpenFileRead( name, &f, qfalse, qtrue );
		if ( !f ) {
			Com_WPrintf( "Couldn't read %s\n", name );
			return qfalse;
		}

		// Retail's read path restores exactly two fields and does not touch the
		FS_Read( &save, sizeof( savegamestruct_t ), f );
		Q_strncpyz( svs.mapName, save.mapName, sizeof( svs.mapName ) );
		svs.time = save.svsTime;
		FS_FCloseFile( f );
	}

	return qtrue;
}

void SV_Loadgame_f( void )
{
	int         length;
	const char *name;
	const char *archive_name;
	char        cmdString[ 1024 ];

	// loading" do not exist anywhere in the retail engine data image, and retail
	if ( g_gametype->integer != GT_SINGLE_PLAYER ) {
		return;
	}

	if ( svs.clients && svs.clients->state == CS_PRIMED ) {
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "USAGE: loadgame <name>\n" );
		return;
	}

	if ( sv_loadStatus != 2 ) {
		return;
	}
	sv_loadStatus = 0;

	name = Cmd_Argv( 1 );
	Com_Printf( "Loading game...%s\n", name );
	if ( strstr( name, ".." ) || strstr( name, "/" ) || strstr( name, "\\" ) ) {
		Com_WPrintf( "Bad savedir.\n" );
	}

	if ( !ge ) {
		SV_InitGameProgs();
	}

	Q_strncpyz( svs.gameName, name, sizeof( svs.gameName ) );
	archive_name = Com_GetArchiveFileName( name, name, "sav", qfalse );

	length = FS_ReadFileEx( archive_name, NULL, qtrue );
	if ( length == -1 ) {
		Com_WPrintf( "Savegame not found.\n" );
		return;
	}

	if ( ge->LevelArchiveValid( archive_name ) ) {
		if ( SV_ArchiveServerFile( "ssv", qtrue, qfalse ) ) {
			SV_DeleteTempSublevelFiles();

			SV_CopyArchiveFile( Com_GetArchiveFileName( name, name, "spv", qfalse ),
			                    Com_GetArchiveFileName( NULL, "current", "spv", qtrue ) );

			SV_CopySublevelFiles( qtrue );

			SV_SpawnServer( svs.mapName, qtrue, qfalse, qfalse, qfalse );
		}
	} else {
		Com_sprintf( cmdString, sizeof( cmdString ),
		             "dialog \"\" \"\" \"loadbrokengame %s\" 260 150 textures\\menu\\invalid "
		             "textures\\menu\\okbutton textures\\menu\\cancelbutton\n", name );
		Cbuf_ExecuteText( EXEC_NOW, cmdString );
	}
}

void SV_Loadbrokengame_f( void )
{
	const char *name;

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "USAGE: loadbrokengame <name>\n" );
		return;
	}

	name = Cmd_Argv( 1 );
	Com_Printf( "Loading broken game...%s\n", name );
	if ( strstr( name, ".." ) || strstr( name, "/" ) || strstr( name, "\\" ) ) {
		Com_WPrintf( "Bad savedir.\n" );
	}

	Q_strncpyz( svs.gameName, name, sizeof( svs.gameName ) );

	if ( !SV_ArchiveServerFile( "ssv", qtrue, qfalse ) ) {
		return;
	}

	SV_CopyArchiveFile( Com_GetArchiveFileName( name, name, "spv", qfalse ),
	                    Com_GetArchiveFileName( NULL, "current", "spv", qtrue ) );

	SV_CopySublevelFiles( qtrue );

	SV_SpawnServer( svs.mapName, qtrue, qfalse, qfalse, qfalse );
}

void SV_SavegameFilename(const char *name, char *fileName, int length)
{
#ifndef DEDICATED
	int         num;
	const char *fname;
	int         len;

	// scheme looked for ent-training10000.ssv, a file retail never writes.
	for (num = 0; num < 10000; num++) {
		Com_sprintf(fileName, length, "%s", name);
		fname = Com_GetArchiveFileName(fileName, fileName, "ssv", qfalse);
		len   = FS_ReadFileEx(fname, NULL, qtrue);
		if (len <= 0) {
			break;
		}
	}
#endif
}

void SV_UpdateLoadSaveStatus( void )
{
	if ( sv_saveStatus != 2 ) {
		if ( sv_saveStatus == 1 ) {
			sv_saveStatus = 2;
		} else if ( !sv_saveStatus ) {
			sv_saveStatus = 1;
		}
	}

	if ( sv_loadStatus != 2 ) {
		if ( sv_loadStatus == 1 ) {
			sv_loadStatus = 2;
		} else if ( !sv_loadStatus ) {
			sv_loadStatus = 1;
		}
	}
}

qboolean SV_DoSaveGame(void)
{
	return qfalse;
}

void SV_SaveGame( const char *gamename, qboolean autosave )
{
	char *ptr;
	char  name[ 64 ];
	char  mname[ 64 ];

	if ( !autosave && ( sv_saveStatus != 2 || sv_loadStatus != 2 ) ) {
		return;
	}

	if ( sv.state != SS_GAME ) {
		Com_Printf( "You must be in a game to save.\n" );
		return;
	}

	if ( ge->inMultiplayerGame() ) {
		return;
	}

	if ( !svs.clients || svs.clients->gentity == NULL || svs.clients->gentity->client == NULL
	     || !svs.clients->gentity->client->ps.stats[ 0 ] ) {
		Com_Printf( "Can't savegame when dead\n" );
		return;
	}

	sv_saveStatus = 0;

	if ( gamename ) {
		Q_strncpyz( name, gamename, sizeof( name ) );
	} else {
		Q_strncpyz( mname, svs.mapName, sizeof( mname ) );
		for ( ptr = strchr( mname, '/' ); ptr != NULL; ptr = strchr( mname, '/' ) ) {
			*ptr = '_';
		}
		SV_SavegameFilename( mname, name, sizeof( name ) );
	}

	if ( strstr( name, ".." ) || strchr( name, '/' ) || strchr( name, '\\' ) ) {
		Com_WDPrintf( "Bad savedir.\n" );
		return;
	}

	Com_DPrintf( "Saving game...%s\n", name );

	Q_strncpyz( svs.gameName, name, sizeof( svs.gameName ) );

	SV_CopyArchiveFile( Com_GetArchiveFileName( NULL, "current", "spv", qtrue ),
	                    Com_GetArchiveFileName( svs.gameName, svs.gameName, "spv", qfalse ) );

	Com_DeleteSublevelFiles( svs.gameName );

	SV_CopySublevelFiles( qfalse );

	SV_ArchiveLevelFile( "sav", "ssv", qfalse );

	SV_ArchiveServerFile( "ssv", qfalse, autosave );

	Com_Printf( "Done.\n" );

	PF_centerprintf( svs.clients->gentity, CENTERPRINT_IMPORTANCE_CRITICAL, "$$GameSaved$$\n" );

	Q_strncpyz( svs.gameName, "current", sizeof( svs.gameName ) );
}


void SV_Savegame_f(void)
{
	if ( Cmd_Argc() != 2 ) {
		SV_SaveGame( NULL, qfalse );
		return;
	}

	SV_SaveGame( Cmd_Argv( 1 ), qfalse );
}


