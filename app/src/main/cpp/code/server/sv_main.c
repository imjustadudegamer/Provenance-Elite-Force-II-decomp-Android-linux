/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later

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

#define SMP_SV_WINDOW_MS   1000
#define SMP_SV_MAX_FRAMES  3600
#define SMP_SV_MAX_ZONES   8

static void SMP_SVFrameBudget( int totalMsec, int botMsec, int pingMsec, int gameMsec,
								int timeoutMsec, int sendMsec, int hbMsec ) {
	static cvar_t	*smp_serverStats;
	static int		smp_svTotal, smp_svTotalMax;
	static int		smp_svBot,   smp_svBotMax;
	static int		smp_svPing,  smp_svPingMax;
	static int		smp_svGame,  smp_svGameMax;
	static int		smp_svTmo,   smp_svTmoMax;
	static int		smp_svSend,  smp_svSendMax;
	static int		smp_svHb,    smp_svHbMax;
	static int		smp_svOther, smp_svOtherMax;
	static int		smp_svFrames;
	static int		smp_svWindowStart;

	int		otherMsec, window, frames, i, mean10;
	char	line[1024], part[64];
	struct { const char *name; int sum; int max; } zone[SMP_SV_MAX_ZONES];

	if ( !smp_serverStats ) {
		smp_serverStats = Cvar_Get( "smp_serverStats", "1", 0 );
	}

	if ( !smp_serverStats->integer || !Cvar_VariableIntegerValue( "qa_probe" ) ) {
		smp_svFrames = 0;
		smp_svWindowStart = 0;
		smp_svTotal = smp_svTotalMax = 0;
		smp_svBot   = smp_svBotMax   = 0;
		smp_svPing  = smp_svPingMax  = 0;
		smp_svGame  = smp_svGameMax  = 0;
		smp_svTmo   = smp_svTmoMax   = 0;
		smp_svSend  = smp_svSendMax  = 0;
		smp_svHb    = smp_svHbMax    = 0;
		smp_svOther = smp_svOtherMax = 0;
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

	smp_svTotal += totalMsec;	if ( totalMsec  > smp_svTotalMax ) smp_svTotalMax = totalMsec;
	smp_svBot   += botMsec;	if ( botMsec    > smp_svBotMax   ) smp_svBotMax   = botMsec;
	smp_svPing  += pingMsec;	if ( pingMsec   > smp_svPingMax  ) smp_svPingMax  = pingMsec;
	smp_svGame  += gameMsec;	if ( gameMsec   > smp_svGameMax  ) smp_svGameMax  = gameMsec;
	smp_svTmo   += timeoutMsec;if ( timeoutMsec> smp_svTmoMax   ) smp_svTmoMax   = timeoutMsec;
	smp_svSend  += sendMsec;	if ( sendMsec   > smp_svSendMax  ) smp_svSendMax  = sendMsec;
	smp_svHb    += hbMsec;		if ( hbMsec     > smp_svHbMax    ) smp_svHbMax    = hbMsec;
	smp_svOther += otherMsec;	if ( otherMsec  > smp_svOtherMax ) smp_svOtherMax = otherMsec;

	smp_svFrames++;

	if ( !smp_svWindowStart ) {
		smp_svWindowStart = Sys_Milliseconds();
	}

	window = Sys_Milliseconds() - smp_svWindowStart;
	if ( window < SMP_SV_WINDOW_MS && smp_svFrames < SMP_SV_MAX_FRAMES ) {
		return;
	}

	frames = smp_svFrames;
	if ( frames < 1 ) {
		frames = 1;
	}

	zone[0].name = "bot";		zone[0].sum = smp_svBot;	zone[0].max = smp_svBotMax;
	zone[1].name = "ping";		zone[1].sum = smp_svPing;	zone[1].max = smp_svPingMax;
	zone[2].name = "game";		zone[2].sum = smp_svGame;	zone[2].max = smp_svGameMax;
	zone[3].name = "timeouts";	zone[3].sum = smp_svTmo;	zone[3].max = smp_svTmoMax;
	zone[4].name = "send";		zone[4].sum = smp_svSend;	zone[4].max = smp_svSendMax;
	zone[5].name = "hb";		zone[5].sum = smp_svHb;	zone[5].max = smp_svHbMax;
	zone[6].name = "other";		zone[6].sum = smp_svOther;	zone[6].max = smp_svOtherMax;
	zone[7].name = "total";		zone[7].sum = smp_svTotal;	zone[7].max = smp_svTotalMax;

	line[0] = '\0';
	for ( i = 0; i < SMP_SV_MAX_ZONES; i++ ) {
		mean10 = (int)( ( (long long)zone[i].sum * 10 + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d",
					 zone[i].name, mean10 / 10, mean10 % 10, zone[i].max );
		Q_strcat( line, sizeof( line ), part );
	}

	Com_Printf( "[SMP:server] frames=%d window=%dms%s\n", frames, window, line );

	smp_svFrames = 0;
	smp_svWindowStart = Sys_Milliseconds();
	smp_svTotal = smp_svTotalMax = 0;
	smp_svBot   = smp_svBotMax   = 0;
	smp_svPing  = smp_svPingMax  = 0;
	smp_svGame  = smp_svGameMax  = 0;
	smp_svTmo   = smp_svTmoMax   = 0;
	smp_svSend  = smp_svSendMax  = 0;
	smp_svHb    = smp_svHbMax    = 0;
	smp_svOther = smp_svOtherMax = 0;
}

void SV_Frame( int msec ) {
	int		frameMsec;
	int		startTime;
	int		smp_frameStart, smp_t0;
	int		smp_bot = 0, smp_ping = 0, smp_game = 0, smp_tmo = 0, smp_send = 0, smp_hb = 0;

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

	smp_frameStart = Sys_Milliseconds();

	if ( sv_bAutoSaveRequested && com_canAutoSaveNow->integer ) {
		Cbuf_AddText( "pause ; ui_autosavegame ; pause\n" );
		sv_bAutoSaveRequested = qfalse;
	}

	if ( sv_fps->integer < 1 ) {
		Cvar_Set( "sv_fps", "10" );
	}

	frameMsec = 1000 / sv_fps->integer;

	sv.timeResidual = (int)( (float)msec * sv.worldState.time_scale + (float)sv.timeResidual );

	// frames, sv_init.c) and then NEVER AGAIN -- which is exactly the BOT run:
	if ( !com_dedicated->integer ) {
		smp_t0 = Sys_Milliseconds();
		SV_BotFrame( svs.time );
		smp_bot += Sys_Milliseconds() - smp_t0;
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

	smp_t0 = Sys_Milliseconds();
	SV_CalcPings();
	smp_ping += Sys_Milliseconds() - smp_t0;

	if ( com_dedicated->integer ) {
		smp_t0 = Sys_Milliseconds();
		SV_BotFrame( svs.time );
		smp_bot += Sys_Milliseconds() - smp_t0;
	}

	smp_t0 = Sys_Milliseconds();
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
	smp_game += Sys_Milliseconds() - smp_t0;

	if ( com_speeds->integer ) {
		time_game = Sys_Milliseconds() - startTime;
	}

	smp_t0 = Sys_Milliseconds();
	SV_CheckTimeouts();
	smp_tmo += Sys_Milliseconds() - smp_t0;

	smp_t0 = Sys_Milliseconds();
	SV_SendClientMessages();
	smp_send += Sys_Milliseconds() - smp_t0;

	smp_t0 = Sys_Milliseconds();
	SV_MasterHeartbeat( "TikiServer-1" );
	smp_hb += Sys_Milliseconds() - smp_t0;

	SMP_SVFrameBudget( Sys_Milliseconds() - smp_frameStart,
						smp_bot, smp_ping, smp_game, smp_tmo, smp_send, smp_hb );
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

	SetBelowNormalThreadPriority();

	Cvar_Set( "paused", "1" );
	Cbuf_ExecuteText( EXEC_NOW, "ui_addhud pausehud 1\n" );
}

void Com_UnPause_f(void)
{
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


