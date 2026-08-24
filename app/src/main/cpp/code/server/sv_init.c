
#include "server.h"
#include "../client/client.h"
#include "../corepp/tiki.h"

#define R120_PROBE_GET(r120_n, r120_d, r120_f) Cvar_Get(r120_n, r120_d, r120_f)
#define R120_PROBE_PRINT                       Com_Printf
#include "../qcommon/qa_r120.h"

static char last_mapname[ MAX_QPATH ];
static int g_iSvsTimeFixupCount;
static int *g_piSvsTimeFixups[ 2048 ];

void SV_ClearSvsTimeFixups( void )
{
	g_iSvsTimeFixupCount = 0;
}

void SV_FinishSvsTimeFixups( void )
{
	int i;
	for( i = 0; i < g_iSvsTimeFixupCount; i++ )
	{
		*g_piSvsTimeFixups[ i ] = svs.time;
	}
	SV_ClearSvsTimeFixups();
}

void SV_AddSvsTimeFixup( int *piTime )
{
	g_piSvsTimeFixups[ g_iSvsTimeFixupCount++ ] = piTime;
}

void SV_SetConfigstring (int index, const char *val) {
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error (ERR_DROP, "SV_SetConfigstring: bad index %i\n", index);
	}

	if ( !val ) {
		val = "";
	}

	if ( sv.configstrings[ index ] ) {
		if ( strcmp( val, sv.configstrings[ index ] ) ) {
			Z_Free( sv.configstrings[index] );
			sv.configstrings[index] = CopyString( val );

			if ( sv.state == SS_GAME || sv.restarting ) {
				SV_SendServerCommand( NULL, "cs %i \"%s\"\n", index, val );
			}
		}
	}
}

char *SV_GetConfigstring( int index )
{
	char *buffer;

	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error (ERR_DROP, "SV_GetConfigstring: bad index %i\n", index);
	}
	if ( !sv.configstrings[index] ) {
		return NULL;
	}

	buffer = Hunk_AllocateTempMemory( strlen( sv.configstrings[ index ] ) + 1 );

	strcpy( buffer, sv.configstrings[index] );

	return buffer;
}

int SV_FindIndex( const char *name, int start, int max, qboolean create ) {
	int		i, j;
	char	clean[ 64 ];
	const char	*s;

	if( !name || !name[ 0 ] ) {
		return 0;
	}

	if( strlen( name ) >= sizeof( clean ) ) {
		Com_Printf( "SV_FindIndex: name too long: %s\n", name );
		Com_Printf( "SV_FindIndex: %s\n", name );
		Com_Printf( "SV_FindIndex: overflow\n" );
		return 0;
	}

	if( start >= MAX_CONFIGSTRINGS ) {
		Com_Error( 1, "SV_FindIndex: bad start index %i\n", start );
	}

	if( max >= MAX_CONFIGSTRINGS ) {
		Com_Error( 1, "SV_FindIndex: bad max index %i\n", max );
	}

	strcpy( clean, name );
	for( i = 0; name[ i ]; i++ ) {
		if( clean[ i ] == '\\' ) {
			clean[ i ] = '/';
		}
	}

	for( j = 1; j < max; j++ ) {
		s = sv.configstrings[ start + j ];

		if( !s[ 0 ] ) {
			break;
		}

		if( !Q_stricmp( s, clean ) ) {
			return j;
		}
	}

	if( !create ) {
		return 0;
	}

	if( j == max ) {
		Com_Error( 1, "SV_FindIndex: overflow  max%d create%d  name %s", max, create, clean );
	}

	SV_SetConfigstring( start + j, clean );
	return j;
}

int SV_ModelIndex( const char *name )
{
	return SV_FindIndex( name, CS_MODELS, MAX_MODELS, qtrue );
}

int SV_SoundIndex( const char *name )
{

	return SV_FindIndex( name, CS_SOUNDS, MAX_SOUNDS, qtrue );
}

int SV_ImageIndex( const char *name )
{
	return SV_FindIndex( name, CS_IMAGES, MAX_IMAGES, qtrue );
}

int SV_ItemIndex( const char *name )
{
	return SV_FindIndex( name, CS_ITEMS, MAX_WEAPONS, qtrue );
}

void SV_SetLightStyle( int index, const char *data )
{
	SV_SetConfigstring( CS_LIGHTSTYLES + index, data );
}

void SV_SetUserinfo( int index, const char *val ) {
	if ( index < 0 || index >= sv_maxclients->integer ) {
		Com_Error (ERR_DROP, "SV_SetUserinfo: bad index %i\n", index);
	}

	if ( !val ) {
		val = "";
	}

	Q_strncpyz( svs.clients[index].userinfo, val, sizeof( svs.clients[ index ].userinfo ) );
	Q_strncpyz( svs.clients[index].name, Info_ValueForKey( val, "name" ), sizeof(svs.clients[index].name) );
}



void SV_GetUserinfo( int index, char *buffer, int bufferSize ) {
	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetUserinfo: bufferSize == %i", bufferSize );
	}
	if ( index < 0 || index >= sv_maxclients->integer ) {
		Com_Error (ERR_DROP, "SV_GetUserinfo: bad index %i\n", index);
	}
	Q_strncpyz( buffer, svs.clients[ index ].userinfo, bufferSize );
}


void SV_CreateBaseline( void ) {
	gentity_t		*ent;
	int				entnum;	

	for ( entnum = 1; entnum < sv.num_entities ; entnum++ ) {
		ent = SV_GentityNum(entnum);
		if (!ent->linked) {
			continue;
		}
		ent->s.number = entnum;

		sv.svEntities[entnum].baseline = ent->s;
	}
}

void SV_BoundMaxClients( int minimum ) {
	Cvar_Get( "sv_maxclients", "8", 0 );

	sv_maxclients->modified = qfalse;

	if ( sv_maxclients->integer < minimum ) {
		Cvar_Set( "sv_maxclients", va("%i", minimum) );
	} else if ( sv_maxclients->integer > MAX_CLIENTS ) {
		Cvar_Set( "sv_maxclients", va("%i", MAX_CLIENTS) );
	}
}

void SV_ClientsAlloc( void )
{
	svs.clients = Z_Malloc( sv_maxclients->integer * sizeof( client_t ) );
	Com_Memset( svs.clients, 0, sv_maxclients->integer * sizeof( client_t ) );
	if( g_gametype->integer != GT_SINGLE_PLAYER ) {
		svs.numSnapshotEntities = sv_maxclients->integer * PACKET_BACKUP * MAX_CLIENTS;
	} else {
		svs.numSnapshotEntities = 1 * PACKET_BACKUP * MAX_CLIENTS;
	}

	SV_InitAllCGMessages();
}


void SV_Startup( void ) {
	if ( svs.initialized ) {
		Com_Error( ERR_FATAL, "SV_Startup: svs.initialized already set" );
	}
	SV_BoundMaxClients( 1 );
	SV_ClientsAlloc();

	svs.initialized = qtrue;
	memset( last_mapname, 0, sizeof( last_mapname ) );

	Cvar_Set( "sv_running", "1" );
}

int SV_NumClients(void) {
	int i;
	int numClients = 0;

	for (i = 0; i < sv_maxclients->integer; i++) {
		if (svs.clients[i].state > CS_FREE) {
			++numClients;
		}
	}

	return numClients;
}

void SV_ChangeMaxClients( void ) {
	int		oldMaxClients;
	int		i;
	client_t	*oldClients;
	int		count;

	count = 0;
	for ( i = 0 ; i < sv_maxclients->integer ; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			if (i > count)
				count = i;
		}
	}
	count++;

	oldMaxClients = sv_maxclients->integer;
	SV_BoundMaxClients( count );
	if ( sv_maxclients->integer == oldMaxClients ) {
		return;
	}

	oldClients = svs.clients;
	SV_ClientsAlloc();

	for ( i = 0 ; i < count ; i++ ) {
		if( oldClients[ i ].state >= CS_CONNECTED ) {
			svs.clients[ i ] = oldClients[ i ];
		}
	}

	Z_Free( oldClients );

	if( g_gametype->integer != GT_SINGLE_PLAYER ) {
		svs.numSnapshotEntities = sv_maxclients->integer * PACKET_BACKUP * MAX_CLIENTS;
	} else {
		svs.numSnapshotEntities = 1 * PACKET_BACKUP * MAX_CLIENTS;
	}
}

void SV_ClearServer(void) {
	int i;

	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		if ( sv.configstrings[i] ) {
			Z_Free( sv.configstrings[i] );
		}
	}
	Com_Memset (&sv, 0, sizeof(sv));
	sv.frameTime = 1.0 / sv_fps->value;
}

static Q_UNUSED_VAR void SV_TouchFile( const char *filename ) {
	fileHandle_t	f;

	FS_FOpenFileRead( filename, &f, qfalse, qtrue );
	if ( f ) {
		FS_FCloseFile( f );
	}
}

static int sv_stickyChecksumFeed = 0;

void SV_SpawnServer( const char *server, qboolean loadgame, qboolean restart,
					 qboolean sublevelLoad, qboolean areSublevels ) {
	int			i;
	int			checksum;
	char		systemInfo[ 1024 ];
	char		map[ 64 ];
	char		spawnpos[ 64 ];
	char		filename[ 64 ];
	char		levelExt[ 64 ];
	char		serverExt[ 64 ];
	const char	*p;
	char		*hash;
	char		*dollar;
	qboolean	differentmap;
	cvar_t		*cheatVar;


	Com_Unpause();

	cheatVar = Cvar_Get( "cheats", "0", CVAR_LATCH );
	if ( cheatVar->latchedString && *cheatVar->latchedString
		 && strcmp( cheatVar->string, cheatVar->latchedString ) ) {
		Cvar_Set( "cheats", cheatVar->latchedString );
	}

	differentmap = qtrue;

	R201_LOAD_BEGIN( spawn );

	hash = strchr( server, '#' );
	if ( hash ) {
		Q_strncpyz( svs.rawServerName, server, hash - server + 1 );
	} else {
		Q_strncpyz( svs.rawServerName, server, sizeof( svs.rawServerName ) );
	}

	dollar = strchr( server, '$' );
	if ( dollar ) {
		Q_strncpyz( map, server, dollar - server + 1 );
		Q_strncpyz( spawnpos, dollar + 1, sizeof( spawnpos ) );
	} else if ( hash ) {
		Q_strncpyz( map, server, hash - server + 1 );
		spawnpos[ 0 ] = 0;
	} else {
		Q_strncpyz( map, server, sizeof( map ) );
		spawnpos[ 0 ] = 0;
	}

	Q_strncpyz( svs.mapName, map, sizeof( svs.mapName ) );

	if ( ge && !Q_stricmp( last_mapname, map ) && ( restart || loadgame ) ) {
		differentmap = qfalse;
	}

	Com_Printf( "------ Server Initialization ------\n" );
	Com_Printf( "Server: %s\n", server );

	if ( svs.initialized ) {
		if ( sv_maxclients->modified ) {
			SV_ChangeMaxClients();
		}
	} else {
		restart = qfalse;
		differentmap = qtrue;

		Cvar_Get( "sv_maxclients", "8", 0 );
		svs.nextSnapshotEntities = 0;
		if ( sv_maxclients->integer < 1 ) {
			Cvar_Set( "sv_maxclients", va( "%i", 1 ) );
		} else if ( sv_maxclients->integer > 128 ) {
			Cvar_Set( "sv_maxclients", va( "%i", 128 ) );
		}

		svs.clients = Z_Malloc( sizeof( client_t ) * sv_maxclients->integer );
		svs.numSnapshotEntities = sv_maxclients->integer << 11;
		svs.initialized = qtrue;
		memset( last_mapname, 0, sizeof( last_mapname ) );
		Cvar_Set( "sv_running", "1" );
	}

	Q_strncpyz( last_mapname, map, sizeof( last_mapname ) );

	CL_MapLoading( differentmap, map, strchr( server, '#' ), areSublevels );

	if ( ge ) {
		ge->Cleanup( restart );
	}

	if ( differentmap ) {
		R201_LOAD_BEGIN( hunkclr );
		Hunk_Clear();
		CM_ClearMap();
		R201_LOAD_END( hunkclr );

		svs.snapshotEntities = Hunk_Alloc( sizeof( entityState_t ) * svs.numSnapshotEntities, h_dontcare );

		for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
			if ( sv.configstrings[ i ] ) {
				Z_Free( sv.configstrings[ i ] );
			}
		}
		memset( &sv, 0, sizeof( sv ) );
		for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
			sv.configstrings[ i ] = CopyString( "" );
		}
	}

	if ( differentmap ) {
		// 0x001DEC10.  Retail does NOT unload the game module here -- that is the
		SV_ClearGameInterface();

		Com_sprintf( filename, sizeof( filename ), "maps/%s.bsp", map );
		R201_LOAD_BEGIN( cmap );
		CM_LoadMap( filename, qfalse, &checksum );
		R201_LOAD_END( cmap );

		Cvar_Set( "sv_mapChecksum", va( "%i", checksum ) );
		Cvar_Set( "mapname", svs.rawServerName );
	}

	if ( differentmap || loadgame ) {
		sv.serverId = com_frameTime;
		sv.restartedServerId = com_frameTime;
	} else {
		sv.restartedServerId = sv.serverId;
		sv.serverId = com_frameTime;
	}

	Cvar_Set( "sv_serverid", va( "%i", sv.serverId ) );

	// EF2 PARITY (ledger T4.14, the "never calls FS_Restart" half).
	if ( sv_stickyChecksumFeed ) {
		sv.checksumFeed = sv_stickyChecksumFeed;
	} else if ( restart ) {
		sv.checksumFeed = sv_stickyChecksumFeed;
	} else {
		srand( Com_Milliseconds() );
		sv.checksumFeed = ( ( rand() << 16 ) ^ rand() ) ^ Com_Milliseconds();
		FS_Restart( sv.checksumFeed );
		sv_stickyChecksumFeed = sv.checksumFeed;
	}

	svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

	CM_ResetAreaPortals();

	if ( !loadgame ) {
		SV_SetConfigstring( CS_SOUNDTRACK, "none" );
	}

	SV_ClearWorld();

	TIKI_FreeLevelData();

	sv.state = SS_LOADING;
	sv.restarting = restart;

	if ( loadgame ) {
		R131_PROBE("svstime", "pre-archive: svs.time=%d map=%s\n", svs.time, svs.mapName);

		if ( sublevelLoad ) {
			Q_strncpyz( levelExt, "sub", sizeof( levelExt ) );
			Q_strncpyz( serverExt, "ssb", sizeof( serverExt ) );
		} else {
			Q_strncpyz( levelExt, "sav", sizeof( levelExt ) );
			Q_strncpyz( serverExt, "ssv", sizeof( serverExt ) );
		}

		if ( !SV_ArchiveLevelFile( levelExt, serverExt, qtrue ) ) {
			loadgame = qfalse;
			sv.restarting = qfalse;
		}

		R131_PROBE("svstime", "after archive restore: svs.time=%d map=%s loadgame=%d\n",
		           svs.time, svs.mapName, loadgame);

		if ( sublevelLoad ) {
			Q_strncpyz( svs.gameName, "current", sizeof( svs.gameName ) );
			SV_ArchivePersistantFile( qtrue, qtrue );
			ge->PostSublevelLoad( spawnpos );
		}

		ge->PostLoad();
	}

	if ( !ge ) {
		R201_LOAD_BEGIN( svinit );
		SV_InitGameProgs();
		R201_LOAD_END( svinit );
	}

	// argument, so a load NEVER zeroes it.  The whole branch reads:
	// MOVED, not just documented: the assignment now sits where retail's does, as the
	if ( !loadgame ) {
		svs.time = 0;

		R201_LOAD_BEGIN( spawnent );
		ge->SpawnEntities( map, CM_EntityString(), svs.time );
		R201_LOAD_END( spawnent );

		p = ge->error_message;
		if ( p ) {
			ge->error_message = NULL;
			Com_Error( ERR_DROP, "%s", p );
		}

		SV_ArchivePersistantFile( qtrue, qfalse );

		g_gametype->modified = qfalse;

		CL_ProcessLoadingScreen( "$$RunningFirstGameFrames$$" );

		for ( i = 0; i < 3; i++ ) {
			ge->RunFrame( svs.time, 100 );

			p = ge->error_message;
			if ( p ) {
				ge->error_message = NULL;
				Com_Error( ERR_DROP, "%s", p );
			}

			svs.time += 100;
		}
	}

	for ( i = 0; i < 3; i++ ) {
		ge->RunFrame( svs.time, 100 );
		SV_BotFrame( svs.time );
		svs.time += 100;
	}

	if ( differentmap ) {
		SV_CreateBaseline();

		for ( i = 0; i < sv_maxclients->integer; i++ ) {
			if ( svs.clients[ i ].state >= CS_CONNECTED ) {
				const char	*denied;

				if ( svs.clients[ i ].netchan.remoteAddress.type != NA_BOT ) {
					denied = ge->ClientConnect( i, qfalse, qfalse, qtrue );

					p = ge->error_message;
					if ( p ) {
						ge->error_message = NULL;
						Com_Error( ERR_DROP, "%s", p );
					}

					if ( denied ) {
						SV_DropClient( &svs.clients[ i ], denied, qfalse );
					} else {
						svs.clients[ i ].state = CS_CONNECTED;

						svs.clients[ i ].nextReliableTime     = 0;
						svs.clients[ i ].nextReliableTimeChat = 0;
						svs.clients[ i ].gamestateTime        = 0;
					}
				} else {
					SV_DropClient( &svs.clients[ i ], "", qtrue );
				}
			}
		}

		if ( sv_pure->integer ) {
			p = FS_LoadedPakChecksums();
			Cvar_Set( "sv_paks", p );
			if ( !strlen( p ) ) {
				Com_Printf( "WARNING: sv_pure set but no PK3 files loaded\n" );
			}
			p = FS_LoadedPakNames();
			Cvar_Set( "sv_pakNames", p );
		} else {
			Cvar_Set( "sv_paks", "" );
			Cvar_Set( "sv_pakNames", "" );
		}

		// Cvar_InfoString (0x001DF14C / 0x001DF184), not the _Big variant, and
		Q_strncpyz( systemInfo, Cvar_InfoString( CVAR_SYSTEMINFO ), sizeof( systemInfo ) );
		cvar_modifiedFlags &= ~CVAR_SYSTEMINFO;
		SV_SetConfigstring( CS_SYSTEMINFO, systemInfo );

		SV_SetConfigstring( CS_SERVERINFO, Cvar_InfoString( CVAR_SERVERINFO ) );
		cvar_modifiedFlags &= ~CVAR_SERVERINFO;

		sv.state = SS_GAME;

		SV_Heartbeat_f();

		Hunk_SetMark();
	} else {
		sv.restarting = qfalse;
		sv.state = SS_GAME;

		for ( i = 0; i < sv_maxclients->integer; i++ ) {
			if ( svs.clients[ i ].state >= CS_CONNECTED ) {
				const char	*denied;

				denied = ge->ClientConnect( i, qfalse, qfalse, qtrue );

				p = ge->error_message;
				if ( p ) {
					ge->error_message = NULL;
					Com_Error( ERR_DROP, "%s", p );
				}

				if ( denied ) {
					SV_DropClient( &svs.clients[ i ], denied, qfalse );
				} else {
					svs.clients[ i ].nextReliableTime     = 0;
					svs.clients[ i ].nextReliableTimeChat = 0;
					svs.clients[ i ].gamestateTime        = 0;

					SV_ClientEnterWorld( &svs.clients[ i ], &svs.clients[ i ].lastUsercmd );
				}
			}
		}
	}

	ge->RunFrame( svs.time, 100 );
	SV_BotFrame( svs.time );
	svs.time += 100;

	if ( sv_pure->integer ) {
		p = FS_LoadedPakChecksums();
		Cvar_Set( "sv_paks", p );
		if ( !strlen( p ) ) {
			Com_Printf( "WARNING: sv_pure set but no PK3 files loaded\n" );
		}
		p = FS_LoadedPakNames();
		Cvar_Set( "sv_pakNames", p );
	} else {
		Cvar_Set( "sv_paks", "" );
		Cvar_Set( "sv_pakNames", "" );
	}

	Q_strncpyz( svs.gameName, "current", sizeof( svs.gameName ) );

	Com_Printf( "-----------------------------------\n" );

	R201_LOAD_END( spawn );
}

qboolean SV_DoneLoading( void ) {
	return sv.state == SS_GAME;
}

void SV_DeleteTempSublevelFiles(void)
{
	const char	*folder;
	char		**list;
	int		numfiles;
	int		i;

	folder = Com_GetArchiveFileName( NULL, NULL, NULL, qtrue );
	list = FS_ListFiles( folder, "ssb", qfalse, &numfiles );
	for ( i = 0; i < numfiles; i++ ) {
		FS_Remove_HomeData( Com_GetArchiveFileName( NULL, list[ i ], "ssb", qtrue ) );
		FS_Remove_HomeData( Com_GetArchiveFileName( NULL, list[ i ], "sub", qtrue ) );
		FS_Remove_HomeData( Com_GetArchiveFileName( NULL, list[ i ], "tga", qtrue ) );
	}
	FS_FreeFileList( list );
}

void SV_Init (void)
{
	int index;

	SV_AddOperatorCommands();

	Cvar_Get ("mp_flags", "0", CVAR_SERVERINFO);
	Cvar_Get ("mp_pointlimit", "0", CVAR_SERVERINFO);
	Cvar_Get ("mp_timelimit", "20", CVAR_SERVERINFO);
	Cvar_Get ("mp_gametypename", "Holomatch", 0);
	mp_gametype = Cvar_Get ("mp_gametype", "0", 0);
	g_gametype = Cvar_Get ("g_gametype", "0", CVAR_SERVERINFO | CVAR_LATCH);
	Cvar_Get ("sv_keywords", "", CVAR_SERVERINFO);
	Cvar_Get ("protocol", "66", CVAR_SERVERINFO | CVAR_ROM);
	sv_mapname = Cvar_Get ("mapname", "nomap", CVAR_SERVERINFO | CVAR_ROM);
	sv_privateClients = Cvar_Get ("sv_privateClients", "0", CVAR_SERVERINFO);
	sv_hostname = Cvar_Get ("sv_hostname", "noname", CVAR_SERVERINFO | CVAR_ARCHIVE);
	sv_maxclients = Cvar_Get ("sv_maxclients", "1", CVAR_SERVERINFO | CVAR_LATCH);
	sv_maxRate = Cvar_Get ("sv_maxRate", "0", CVAR_ARCHIVE | CVAR_SERVERINFO);
	sv_floodProtect = Cvar_Get ("sv_floodProtect", "1", CVAR_ARCHIVE | CVAR_SERVERINFO);
	Cvar_Get ("cheats", "0", CVAR_LATCH | CVAR_SYSTEMINFO);
	sv_serverid = Cvar_Get ("sv_serverid", "0", CVAR_SYSTEMINFO | CVAR_ROM);
	sv_pure = Cvar_Get ("sv_pure", "0", CVAR_SYSTEMINFO);
	Cvar_Get ("sv_paks", "", CVAR_SYSTEMINFO | CVAR_ROM);
	Cvar_Get ("sv_pakNames", "", CVAR_SYSTEMINFO | CVAR_ROM);

	sv_rconPassword = Cvar_Get ("rconPassword", "", CVAR_TEMP);
	sv_privatePassword = Cvar_Get ("sv_privatePassword", "", CVAR_TEMP);
	sv_fps = Cvar_Get ("sv_fps", "20", CVAR_TEMP);
	sv_timeout = Cvar_Get ("sv_timeout", "120", CVAR_TEMP);
	sv_zombietime = Cvar_Get ("sv_zombietime", "2", CVAR_TEMP);
	Cvar_Get ("nextmap", "", CVAR_TEMP);

	sv_allowDownload = Cvar_Get ("sv_allowdownload", "1", 0);
	sv_master[0] = Cvar_Get ("sv_master1", "ef2master.activision.com", 0);
	for (index = 1; index < MAX_MASTER_SERVERS; index++) {
		sv_master[index] = Cvar_Get (va ("sv_master%d", index + 1), "", CVAR_ARCHIVE);
	}

	sv_reconnectlimit = Cvar_Get ("sv_reconnectlimit", "3", 0);
	sv_padPackets = Cvar_Get ("sv_padPackets", "0", 0);
	sv_killserver = Cvar_Get ("sv_killserver", "0", 0);
	sv_mapChecksum = Cvar_Get ("sv_mapChecksum", "", CVAR_ROM);

	Q_strncpyz (svs.gameName, "current", sizeof (svs.gameName));
	SV_DeleteTempSublevelFiles();
	SV_BotInitCvars();
	SV_BotInitBotLib();
}



void SV_Shutdown( const char *finalmsg ) {
	if ( !com_sv_running || !com_sv_running->integer ) {
		return;
	}

	Com_Printf( "----- Server Shutdown (%s) -----\n", finalmsg );

	if ( svs.clients && !com_errorEntered ) {
		int i;

		for ( i = 0; i < sv_maxclients->integer; i++ ) {
			SV_DropClient( &svs.clients[i], finalmsg, qfalse );
		}
	}

	SV_RemoveOperatorCommands();
	SV_MasterShutdown();
	SV_ShutdownGameProgs();

	SV_DeleteTempSublevelFiles();

	SV_ClearServer();

	if(svs.clients)
	{
		Z_Free(svs.clients);
		svs.clients = NULL;
	}
	Com_Memset( &svs, 0, sizeof( svs ) );

	Cvar_Set( "sv_running", "0" );

	Com_Printf( "---------------------------\n" );

}
