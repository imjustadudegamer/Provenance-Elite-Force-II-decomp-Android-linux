
#include "server.h"

#ifdef USE_VOIP
#endif

serverStatic_t	svs;				
server_t		sv;					
game_export_t	*ge = NULL;

extern cvar_t	*com_canAutoSaveNow;

qboolean sv_bAutoSaveRequested = qfalse;

cvar_t	*sv_fps;				
cvar_t	*sv_timeout;			
cvar_t	*sv_zombietime;			
cvar_t	*sv_rconPassword;		
cvar_t	*sv_privatePassword;	
cvar_t	*sv_allowDownload;
cvar_t	*sv_maxclients;

cvar_t	*sv_privateClients;		
cvar_t	*sv_hostname;
cvar_t	*sv_master[ MAX_MASTER_SERVERS ];		
cvar_t	*sv_reconnectlimit;		
cvar_t	*sv_padPackets;			
cvar_t	*sv_killserver;			
cvar_t	*sv_mapname;
cvar_t	*sv_mapChecksum;
cvar_t	*sv_serverid;
cvar_t	*sv_maxRate;
cvar_t	*sv_pure;
cvar_t	*sv_floodProtect;
cvar_t	*g_gametype;
cvar_t	*mp_gametype;  




char	*SV_ExpandNewlines( char *in ) {
	static	char	string[1024];
	int		l;

	l = 0;
	while ( *in && l < sizeof(string) - 3 ) {
		if ( *in == '\n' ) {
			string[l++] = '\\';
			string[l++] = 'n';
		} else {
			string[l++] = *in;
		}
		in++;
	}
	string[l] = 0;

	return string;
}


void SV_AddServerCommand( client_t *client, const char *cmd ) {
	client->reliableSequence++;
	if ( client->reliableSequence - client->reliableAcknowledge == MAX_RELIABLE_COMMANDS + 1 ) {
		SV_DropClientNextFrame( client, "$$ServerCommandOverflow$$" );
		return;
	}
	Q_strncpyz( client->reliableCommands[ client->reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 ) ],
				cmd, sizeof( client->reliableCommands[0] ) );
}


void SV_DropClientNextFrame( client_t *client, const char *reason ) {
	client->dropNextFrame = 1;
	strncpy( client->dropReason, reason, sizeof( client->dropReason ) );
}


void QDECL SV_SendServerCommand(client_t *cl, const char *fmt, ...) {
	va_list		argptr;
	char		message[MAX_MSGLEN];
	char		expanded[1024];
	client_t	*client;
	int			j;
	int			n;
	const char	*p;
	char		*e;

	if (!svs.clients) {
		return;
	}

	va_start (argptr,fmt);
	Q_vsnprintf (message, sizeof(message), fmt,argptr);
	va_end (argptr);

	if ( cl != NULL ) {
		SV_AddServerCommand( cl, message );
		return;
	}

	if ( com_dedicated->integer && !strncmp( message, "print", 5 ) ) {
		p = message;
		e = expanded;
		n = 0;
		while ( *p && n < 0x3FD ) {
			if ( *p == '\n' ) {
				*e++ = '\\';
				*e++ = 'n';
				n += 2;
			} else {
				*e++ = *p;
				n++;
			}
			p++;
		}
		*e = 0;
		Com_Printf( "broadcast: %s\n", expanded );
	}

	for (j = 0, client = svs.clients; j < sv_maxclients->integer ; j++, client++) {
		if ( client->state >= CS_PRIMED ) {
			SV_AddServerCommand( client, message );
		}
	}
}



void SV_MasterHeartbeat( const char *message ) {
	int			i;
	cvar_t		*master;
	static netadr_t	adr[ MAX_MASTER_SERVERS ];

	if ( !com_dedicated->integer || com_dedicated->integer != 2 ) {
		return;
	}

	if ( svs.time < svs.nextHeartbeatTime ) {
		return;
	}

	svs.nextHeartbeatTime = svs.time + 75000;

	for ( i = 0 ; i < MAX_MASTER_SERVERS ; i++ ) {
		master = sv_master[i];
		if ( !master || !master->string[0] ) {
			continue;
		}

		if ( master->modified ) {
			master->modified = qfalse;
			Com_Printf( "Resolving %s\n", master->string );
			if ( !NET_StringToAdr( master->string, &adr[ i ] ) ) {
				Com_Printf( "Couldn't resolve address\n" );
				continue;
			}
			if ( !strstr( master->string, ":" ) ) {
				adr[ i ].port = BigShort( 29250 );
			}
			Com_Printf( "%s resolved to port %i\n",
				master->string,
				BigShort( adr[ i ].port ) );
		}

		Com_Printf( "Sending heartbeat to %s\n", master->string );
		NET_OutOfBandPrint( NS_SERVER, adr[ i ], "heartbeat %s\n", message );
	}
}

void SV_MasterShutdown( void ) {
	svs.nextHeartbeatTime = -9999;
	SV_MasterHeartbeat( "TikiServer-1" );

	svs.nextHeartbeatTime = -9999;
	SV_MasterHeartbeat( "TikiServer-1" );

}




void SVC_Status( netadr_t from ) {
	char	player[1024];
	char	status[MAX_MSGLEN];
	int		i;
	client_t	*cl;
	size_t	statusLength;
	size_t	playerLength;
	char	infostring[MAX_INFO_STRING];

	if ( Cvar_VariableValue( "g_gametype" ) == GT_SINGLE_PLAYER
	     || Cvar_VariableValue( "g_gametype" ) == GT_BOT_SINGLE_PLAYER ) {
		return;
	}

	infostring[0] = 0;

	Info_SetValueForKey( infostring, "challenge", Cmd_Argv(1) );

	strcat( infostring, Cvar_InfoString( CVAR_SERVERINFO ) );

	if (Cvar_VariableValue("fs_restrict") != 0.0f) {
		char keywords[MAX_INFO_STRING];

		Com_sprintf(keywords, sizeof(keywords), "demo %s", Info_ValueForKey(infostring, "sv_keywords"));
		Info_SetValueForKey(infostring, "sv_keywords", keywords);
	}

	// STRING, not the cvar name.  0x1DFFB0 `lwz r5,(r29)` (r29 = off_229078 ->
	Info_SetValueForKey( infostring, "gametype", Cvar_VariableString( "mp_gametype" ) );
	Info_SetValueForKey( infostring, "maxplayers", sv_maxclients->string );

	status[0] = 0;
	statusLength = 0;

	for (i=0 ; i < sv_maxclients->integer ; i++) {
		cl = &svs.clients[i];
		if ( cl->state >= CS_CONNECTED ) {
			playerState_t *ps = SV_GameClientNum( i );

			Com_sprintf (player, sizeof(player), "%i %i \"%s\"\n",
				ps->stats[STAT_SCORE], cl->ping, cl->name);
			playerLength = strlen(player);
			if (statusLength + playerLength >= sizeof(status) ) {
				break;		
			}
			Q_strncpyz (status + statusLength, player, sizeof(status) - statusLength);
			statusLength += playerLength;
		}
	}

	NET_OutOfBandPrint( NS_SERVER, from, "statusResponse\n%s\n%s", infostring, status );
}

void SVC_Info( netadr_t from ) {
	int		i, count;
	char	infostring[MAX_INFO_STRING];

	// (0.0 and 2.0, raw 0x1E0110/0x1E0128).  Retail never reads
	if ( Cvar_VariableValue( "g_gametype" ) == GT_SINGLE_PLAYER
	     || Cvar_VariableValue( "g_gametype" ) == GT_BOT_SINGLE_PLAYER ) {
		return;
	}

	count = 0;
	for ( i = sv_privateClients->integer ; i < sv_maxclients->integer ; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			count++;
		}
	}

	infostring[0] = 0;

	Info_SetValueForKey( infostring, "challenge", Cmd_Argv(1) );

	Info_SetValueForKey( infostring, "protocol", va("%i", 66) );
	Info_SetValueForKey( infostring, "version", com_version->string );
	Info_SetValueForKey( infostring, "hostname", sv_hostname->string );
	Info_SetValueForKey( infostring, "mapname", sv_mapname->string );
	Info_SetValueForKey( infostring, "clients", va("%i", count) );
	Info_SetValueForKey( infostring, "gametype", va("%i", g_gametype->integer) );
	Info_SetValueForKey( infostring, "mp_gametype", va("%i", mp_gametype->integer) );
	Info_SetValueForKey( infostring, "pure", va("%i", sv_pure->integer) );
	Info_SetValueForKey( infostring, "sv_privateclients", va("%i", sv_privateClients->integer) );
	Info_SetValueForKey( infostring, "sv_maxclients",
		va("%i", sv_maxclients->integer - sv_privateClients->integer) );

	NET_OutOfBandPrint( NS_SERVER, from, "infoResponse\n%s", infostring );
}

static void SV_FlushRedirect( char *outputbuf ) {
	NET_OutOfBandPrint( NS_SERVER, svs.redirectAddress, "print\n%s", outputbuf );
}

static void SVC_RemoteCommand( netadr_t from, msg_t *msg ) {
	qboolean	valid;
	char		remaining[1024];
	// `0xBFF0' (49136) to Com_BeginRedirect.  ioq3's 8192-16 was port-only.
#define SV_OUTPUTBUF_LENGTH (0xC000 - 16)
	char		sv_outputbuf[SV_OUTPUTBUF_LENGTH];
	int			i;
	static int	lastRconTime;

	// port-only: SVC_RateLimit / SVC_RateLimitAddress have NO Mac symbol at all
	if ( (int)Com_Milliseconds() < lastRconTime + 500 ) {
		return;
	}
	lastRconTime = (int)Com_Milliseconds();

	if ( !strlen( sv_rconPassword->string ) ||
		strcmp (Cmd_Argv(1), sv_rconPassword->string) ) {
		valid = qfalse;
		Com_Printf ("Bad rcon from %s:\n%s\n", NET_AdrToLogString (from), Cmd_Argv(2) );
	} else {
		valid = qtrue;
		Com_Printf ("Rcon from %s:\n%s\n", NET_AdrToLogString (from), Cmd_Argv(2) );
	}

	svs.redirectAddress = from;
	Com_BeginRedirect (sv_outputbuf, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect);

	if ( !strlen( sv_rconPassword->string ) ) {
		Com_Printf ("No rconpassword set on the server.\n");
	} else if ( !valid ) {
		Com_Printf ("Bad rconpassword.\n");
	} else {
		remaining[0] = 0;

		for ( i = 2; i < Cmd_Argc(); i++ ) {
			Q_strcat( remaining, sizeof(remaining), Cmd_Argv(i) );
			Q_strcat( remaining, sizeof(remaining), " " );
		}

		Cmd_ExecuteString (remaining);
	}

	Com_EndRedirect ();
}

void SV_ConnectionlessPacket( netadr_t from, msg_t *msg ) {
	char	*s;
	char	*c;

	MSG_BeginReadingOOB( msg );
	MSG_ReadLong( msg );		


	if (!Q_strncmp("connect", (char *) &msg->data[4], 7)) {
		Huff_Decompress(msg, 12);
	}

	s = MSG_ReadStringLine( msg );
	Cmd_TokenizeString( s );

	c = Cmd_Argv(0);
	Com_DPrintf ("SV packet %s : %s\n", NET_AdrToLogString(from), c);

	if (!Q_stricmp(c, "getstatus")) {
		SVC_Status( from );
  } else if (!Q_stricmp(c, "getinfo")) {
		SVC_Info( from );
	} else if (!Q_stricmp(c, "getchallenge")) {
		SV_GetChallenge(from);
	} else if (!Q_stricmp(c, "connect")) {
		SV_DirectConnect( from );
	} else if (!Q_stricmp(c, "ipAuthorize")) {
		SV_AuthorizeIpPacket( from );
	} else if (!Q_stricmp(c, "rcon")) {
		SVC_RemoteCommand( from, msg );
	} else if (!Q_stricmp(c, "disconnect")) {
	} else {
		Com_DPrintf ("bad connectionless packet from %s:\n%s\n",
			NET_AdrToLogString (from), s);
	}
}


void SV_PacketEvent( netadr_t from, msg_t *msg ) {
	int			i;
	client_t	*cl;
	int			qport;

	if ( msg->cursize >= 4 && *(int *)msg->data == -1) {
		SV_ConnectionlessPacket( from, msg );
		return;
	}

	MSG_BeginReadingOOB( msg );
	MSG_ReadLong( msg );				
	qport = MSG_ReadShort( msg ) & 0xffff;

	for (i=0, cl=svs.clients ; i < sv_maxclients->integer; i++,cl++) {
		if (cl->state == CS_FREE) {
			continue;
		}
		if ( !NET_CompareBaseAdr( from, cl->netchan.remoteAddress ) ) {
			continue;
		}
		if (cl->netchan.qport != qport) {
			continue;
		}

		if (cl->netchan.remoteAddress.port != from.port) {
			Com_Printf( "SV_PacketEvent: fixing up a translated port\n" );
			cl->netchan.remoteAddress.port = from.port;
		}

		if (SV_Netchan_Process(cl, msg)) {
			if (cl->state != CS_ZOMBIE) {
				cl->lastPacketTime = svs.time;	
				SV_ExecuteClientMessage( cl, msg );
			}
		}
		return;
	}
	
	NET_OutOfBandPrint( NS_SERVER, from, "disconnect" );
}


static void SV_CalcPings( void ) {
	int			i, j;
	client_t	*cl;
	int			total, count;
	int			delta;
	playerState_t	*ps;

	for (i=0 ; i < sv_maxclients->integer ; i++) {
		cl = &svs.clients[i];
		if ( cl->state != CS_ACTIVE ) {
			cl->ping = 999;
			continue;
		}
		if ( !cl->gentity ) {
			cl->ping = 999;
			continue;
		}
		if ( cl->gentity->svflags & SVF_BOT ) {
			cl->ping = 0;
			continue;
		}

		total = 0;
		count = 0;
		for ( j = 0 ; j < PACKET_BACKUP ; j++ ) {
			if ( cl->frames[j].messageAcked <= 0 ) {
				continue;
			}
			delta = cl->frames[j].messageAcked - cl->frames[j].messageSent;
			count++;
			total += delta;
		}
		if (!count) {
			cl->ping = 999;
		} else {
			cl->ping = total/count;
			if ( cl->ping > 999 ) {
				cl->ping = 999;
			}
		}

		ps = SV_GameClientNum( i );
		ps->ping = cl->ping;
	}
}

static void SV_CheckTimeouts( void ) {
	int		i;
	client_t	*cl;
	int			droppoint;
	int			zombiepoint;

	if ( sv_maxclients->integer == 1 ) {
		return;
	}

	droppoint = svs.time - 1000 * sv_timeout->integer;
	zombiepoint = svs.time - 1000 * sv_zombietime->integer;

	for (i=0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++) {
		if (cl->lastPacketTime > svs.time) {
			cl->lastPacketTime = svs.time;
		}

		if (cl->state == CS_ZOMBIE
		&& cl->lastPacketTime < zombiepoint) {
			Com_DPrintf( "Going from CS_ZOMBIE to CS_FREE for %s\n", cl->name );
			cl->state = CS_FREE;	
			continue;
		}
		if ( cl->state < CS_CONNECTED || cl->lastPacketTime >= droppoint ) {
			cl->timeoutCount = 0;
		} else {
			if ( ++cl->timeoutCount > 5 ) {
				SV_DropClient (cl, va( "%s $$TimedOut$$", cl->name ), qtrue );
			}
		}
	}
}


int SV_FrameMsec(void)
{
	if(sv_fps)
	{
		int frameMsec;
		
		frameMsec = 1000.0f / sv_fps->value;
		
		if(frameMsec < sv.timeResidual)
			return 0;
		else
			return frameMsec - sv.timeResidual;
	}
	else
		return 1;
}

#define R201_SV_WINDOW_MS   1000
#define R201_SV_MAX_FRAMES  3600
#define R201_SV_MAX_ZONES   8

static void R201_SVFrameBudget( int totalMsec, int botMsec, int pingMsec, int gameMsec,
								int timeoutMsec, int sendMsec, int hbMsec ) {
	static cvar_t	*r201_serverStats;
	static int		r201_svTotal, r201_svTotalMax;
	static int		r201_svBot,   r201_svBotMax;
	static int		r201_svPing,  r201_svPingMax;
	static int		r201_svGame,  r201_svGameMax;
	static int		r201_svTmo,   r201_svTmoMax;
	static int		r201_svSend,  r201_svSendMax;
	static int		r201_svHb,    r201_svHbMax;
	static int		r201_svOther, r201_svOtherMax;
	static int		r201_svFrames;
	static int		r201_svWindowStart;

	int		otherMsec, window, frames, i, mean10;
	char	line[1024], part[64];
	struct { const char *name; int sum; int max; } zone[R201_SV_MAX_ZONES];

	if ( !r201_serverStats ) {
		r201_serverStats = Cvar_Get( "r201_serverStats", "1", 0 );
	}

	if ( !r201_serverStats->integer ) {
		r201_svFrames = 0;
		r201_svWindowStart = 0;
		r201_svTotal = r201_svTotalMax = 0;
		r201_svBot   = r201_svBotMax   = 0;
		r201_svPing  = r201_svPingMax  = 0;
		r201_svGame  = r201_svGameMax  = 0;
		r201_svTmo   = r201_svTmoMax   = 0;
		r201_svSend  = r201_svSendMax  = 0;
		r201_svHb    = r201_svHbMax    = 0;
		r201_svOther = r201_svOtherMax = 0;
		return;
	}

	if ( totalMsec  < 0 ) totalMsec  = 0;
	if ( botMsec    < 0 ) botMsec    = 0;
	if ( pingMsec   < 0 ) pingMsec   = 0;
	if ( gameMsec   < 0 ) gameMsec   = 0;
	if ( timeoutMsec< 0 ) timeoutMsec= 0;
	if ( sendMsec   < 0 ) sendMsec   = 0;
	if ( hbMsec     < 0 ) hbMsec     = 0;

	otherMsec = totalMsec - botMsec - pingMsec - gameMsec - timeoutMsec - sendMsec - hbMsec;
	if ( otherMsec < 0 ) {
		otherMsec = 0;
	}

	r201_svTotal += totalMsec;	if ( totalMsec  > r201_svTotalMax ) r201_svTotalMax = totalMsec;
	r201_svBot   += botMsec;	if ( botMsec    > r201_svBotMax   ) r201_svBotMax   = botMsec;
	r201_svPing  += pingMsec;	if ( pingMsec   > r201_svPingMax  ) r201_svPingMax  = pingMsec;
	r201_svGame  += gameMsec;	if ( gameMsec   > r201_svGameMax  ) r201_svGameMax  = gameMsec;
	r201_svTmo   += timeoutMsec;if ( timeoutMsec> r201_svTmoMax   ) r201_svTmoMax   = timeoutMsec;
	r201_svSend  += sendMsec;	if ( sendMsec   > r201_svSendMax  ) r201_svSendMax  = sendMsec;
	r201_svHb    += hbMsec;		if ( hbMsec     > r201_svHbMax    ) r201_svHbMax    = hbMsec;
	r201_svOther += otherMsec;	if ( otherMsec  > r201_svOtherMax ) r201_svOtherMax = otherMsec;

	r201_svFrames++;

	if ( !r201_svWindowStart ) {
		r201_svWindowStart = Sys_Milliseconds();
	}

	window = Sys_Milliseconds() - r201_svWindowStart;
	if ( window < R201_SV_WINDOW_MS && r201_svFrames < R201_SV_MAX_FRAMES ) {
		return;
	}

	frames = r201_svFrames;
	if ( frames < 1 ) {
		frames = 1;
	}

	zone[0].name = "bot";		zone[0].sum = r201_svBot;	zone[0].max = r201_svBotMax;
	zone[1].name = "ping";		zone[1].sum = r201_svPing;	zone[1].max = r201_svPingMax;
	zone[2].name = "game";		zone[2].sum = r201_svGame;	zone[2].max = r201_svGameMax;
	zone[3].name = "timeouts";	zone[3].sum = r201_svTmo;	zone[3].max = r201_svTmoMax;
	zone[4].name = "send";		zone[4].sum = r201_svSend;	zone[4].max = r201_svSendMax;
	zone[5].name = "hb";		zone[5].sum = r201_svHb;	zone[5].max = r201_svHbMax;
	zone[6].name = "other";		zone[6].sum = r201_svOther;	zone[6].max = r201_svOtherMax;
	zone[7].name = "total";		zone[7].sum = r201_svTotal;	zone[7].max = r201_svTotalMax;

	line[0] = '\0';
	for ( i = 0; i < R201_SV_MAX_ZONES; i++ ) {
		mean10 = (int)( ( (long long)zone[i].sum * 10 + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d",
					 zone[i].name, mean10 / 10, mean10 % 10, zone[i].max );
		Q_strcat( line, sizeof( line ), part );
	}

	Com_Printf( "[R201:server] frames=%d window=%dms%s\n", frames, window, line );

	r201_svFrames = 0;
	r201_svWindowStart = Sys_Milliseconds();
	r201_svTotal = r201_svTotalMax = 0;
	r201_svBot   = r201_svBotMax   = 0;
	r201_svPing  = r201_svPingMax  = 0;
	r201_svGame  = r201_svGameMax  = 0;
	r201_svTmo   = r201_svTmoMax   = 0;
	r201_svSend  = r201_svSendMax  = 0;
	r201_svHb    = r201_svHbMax    = 0;
	r201_svOther = r201_svOtherMax = 0;
}

void SV_Frame( int msec ) {
	int		frameMsec;
	int		startTime;
	int		r201_frameStart, r201_t0;
	int		r201_bot = 0, r201_ping = 0, r201_game = 0, r201_tmo = 0, r201_send = 0, r201_hb = 0;

	if ( sv_killserver->integer ) {
		SV_Shutdown( "Server was killed.\n" );
		Cvar_Set( "sv_killserver", "0" );
		return;
	}

	if ( !com_sv_running->integer ) {
#ifdef DEDICATED
		Sys_Sleep(-1);
#endif

		return;
	}

	SV_UpdateLoadSaveStatus();

	if ( paused->integer ) {
		return;
	}

	r201_frameStart = Sys_Milliseconds();

	if ( sv_bAutoSaveRequested && com_canAutoSaveNow->integer ) {
		Cbuf_AddText( "pause ; ui_autosavegame ; pause\n" );
		sv_bAutoSaveRequested = qfalse;
	}

	if ( sv_fps->integer < 1 ) {
		Cvar_Set( "sv_fps", "10" );
	}

	frameMsec = 1000 / sv_fps->integer;

	sv.timeResidual = (int)( (float)msec * sv.worldState.time_scale + (float)sv.timeResidual );

	// frames, sv_init.c) and then NEVER AGAIN -- which is exactly the R121 run:
	if ( !com_dedicated->integer ) {
		r201_t0 = Sys_Milliseconds();
		SV_BotFrame( svs.time );
		r201_bot += Sys_Milliseconds() - r201_t0;
	}

	// Retail 0x001E1194: a dedicated server that has not accumulated a full
	if ( com_dedicated->integer ) {
		if ( sv.timeResidual < frameMsec ) {
			NET_Sleep( frameMsec - sv.timeResidual );
			return;
		}
	}

	if ( svs.time > 0x70000000 ) {
		SV_Shutdown( "Restarting server due to time wrapping" );
		Cbuf_AddText( "vstr nextmap\n" );
		return;
	}

	if ( cvar_modifiedFlags & CVAR_SERVERINFO ) {
		SV_SetConfigstring( CS_SERVERINFO, Cvar_InfoString( CVAR_SERVERINFO ) );
		cvar_modifiedFlags &= ~CVAR_SERVERINFO;
	}
	if ( cvar_modifiedFlags & CVAR_SYSTEMINFO ) {
		// Retail 0x001E1230 uses Cvar_InfoString here, not _Big.
		SV_SetConfigstring( CS_SYSTEMINFO, Cvar_InfoString( CVAR_SYSTEMINFO ) );
		cvar_modifiedFlags &= ~CVAR_SYSTEMINFO;
	}

	if ( com_speeds->integer ) {
		startTime = Sys_Milliseconds();
	} else {
		startTime = 0;	
	}

	r201_t0 = Sys_Milliseconds();
	SV_CalcPings();
	r201_ping += Sys_Milliseconds() - r201_t0;

	if ( com_dedicated->integer ) {
		r201_t0 = Sys_Milliseconds();
		SV_BotFrame( svs.time );
		r201_bot += Sys_Milliseconds() - r201_t0;
	}

	r201_t0 = Sys_Milliseconds();
	while ( sv.timeResidual >= frameMsec ) {
		sv.timeResidual -= frameMsec;
		svs.time += frameMsec;

		ge->RunFrame( svs.time, frameMsec );

		if ( ge->error_message ) {
			const char *err = ge->error_message;

			ge->error_message = NULL;
			Com_Error( ERR_DROP, "%s", err );
		}
	}
	r201_game += Sys_Milliseconds() - r201_t0;

	if ( com_speeds->integer ) {
		time_game = Sys_Milliseconds() - startTime;
	}

	r201_t0 = Sys_Milliseconds();
	SV_CheckTimeouts();
	r201_tmo += Sys_Milliseconds() - r201_t0;

	r201_t0 = Sys_Milliseconds();
	SV_SendClientMessages();
	r201_send += Sys_Milliseconds() - r201_t0;

	r201_t0 = Sys_Milliseconds();
	SV_MasterHeartbeat( "TikiServer-1" );
	r201_hb += Sys_Milliseconds() - r201_t0;

	R201_SVFrameBudget( Sys_Milliseconds() - r201_frameStart,
						r201_bot, r201_ping, r201_game, r201_tmo, r201_send, r201_hb );
}

void Com_Pause(void)
{
	if( deathmatch->integer ) {
		return;
	}

	if( !com_sv_running->integer ) {
		return;
	}

	if( sv.state != SS_GAME ) {
		return;
	}

	if( paused->integer == 1 ) {
		return;
	}

	SetBelowNormalThreadPriority();
	Cvar_Set( "paused", "1" );
}

void Com_Unpause(void)
{
	if( paused->integer )
	{
		if( paused->integer == 1 ) {
			SetNormalThreadPriority();
		}

		Cvar_Set( "paused", "0" );
		CL_ClearButtons();
	}
}

void Com_FakePause(void)
{
	if( !paused->integer ) {
		Cvar_Set( "paused", "2" );
	}
}

void Com_FakeUnpause(void)
{
	if( paused->integer >= 2 )
	{
		Cvar_Set( "paused", "0" );
		CL_ClearButtons();
	}
}

static qboolean SV_CanPause( void )
{
	client_t	*client;
	int			i, numclients;

	if ( Cvar_VariableIntegerValue( "g_gametype" ) == 0 ) {
		return qtrue;
	}

	if ( !com_sv_running->integer ) {
		return qfalse;
	}

	numclients = 0;
	for ( i = 0, client = svs.clients; i < sv_maxclients->integer; i++, client++ ) {
		if ( client->state >= CS_CONNECTED
			&& client->netchan.remoteAddress.type != NA_BOT ) {
			numclients++;
		}
	}

	return (qboolean)( numclients == 1 );
}

void Com_Pause_f(void)
{
	if ( paused->integer ) {
		Com_UnPause_f();
		return;
	}

	if ( !SV_CanPause() ) {
		return;
	}

	// SANCTIONED platform glue: the Android sys layer drops the process/thread
	SetBelowNormalThreadPriority();

	Cvar_Set( "paused", "1" );
	Cbuf_ExecuteText( EXEC_NOW, "ui_addhud pausehud 1\n" );
}

void Com_UnPause_f(void)
{
	// SANCTIONED platform glue, symmetric with Com_Pause_f.
	if ( paused->integer == 1 ) {
		SetNormalThreadPriority();
	}

	Cvar_Set( "paused", "0" );
	Cbuf_ExecuteText( EXEC_NOW, "ui_removehud pausehud\n" );
}



#define UDPIP_HEADER_SIZE 48

int SV_RateMsec(client_t *client, int messageSize)
{
	int rate;

	if ( messageSize > 1500 ) {
		messageSize = 1500;
	}

	rate = client->rate;

	if ( sv_maxRate->integer ) {
		if ( sv_maxRate->integer < 1000 ) {
			// the never-registered `sv_MaxRate' -- the clamp is INERT in retail, and the
			Cvar_Set( "sv_MaxRate", "1000" );
		}
		if ( sv_maxRate->integer < rate ) {
			rate = sv_maxRate->integer;
		}
	}

	return 1000 * ( messageSize + UDPIP_HEADER_SIZE ) / rate;
}


