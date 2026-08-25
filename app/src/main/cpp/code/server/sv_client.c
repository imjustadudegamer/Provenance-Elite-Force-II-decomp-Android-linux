
#include "server.h"

#define PROBE_GET(probe_n, probe_d, probe_f) Cvar_Get(probe_n, probe_d, probe_f)
#define PROBE_PRINT                       Com_Printf
#include "../qcommon/qa_probe.h"


// exact retail buffer length is not recoverable from the 0x470 frame, so this
#define	SV_FS_GAME_BUFSIZE		64

void SV_GetChallenge(netadr_t from)
{
	int			i;
	int			oldest;
	int			oldestTime;
	challenge_t	*challenge;
	char		fs_game[SV_FS_GAME_BUFSIZE];
	cvar_t		*cv;

	if ( Cvar_VariableValue( "g_gametype" ) == GT_SINGLE_PLAYER ||
		Cvar_VariableValue( "g_gametype" ) == GT_BOT_SINGLE_PLAYER ) {
		return;
	}


	oldest = 0;
	oldestTime = 0x7fffffff;

	challenge = &svs.challenges[0];
	for ( i = 0 ; i < MAX_CHALLENGES ; i++, challenge++ ) {
		if ( !challenge->connected && NET_CompareAdr( from, challenge->adr ) ) {
			Com_DPrintf( "Challenge found: %d\n", challenge->challenge );
			break;
		}
		if ( challenge->time < oldestTime ) {
			oldestTime = challenge->time;
			oldest = i;
		}
	}

	if ( i == MAX_CHALLENGES ) {
		challenge = &svs.challenges[oldest];

		challenge->challenge = ( (rand() << 16) ^ rand() ) ^ svs.time;
		challenge->adr = from;
		challenge->firstTime = svs.time;
		challenge->firstPing = 0;
		challenge->time = svs.time;
		challenge->connected = qfalse;
		Com_Printf( "SV_GetChallenge: New challenge added : %d\n", challenge->challenge );
	}

	if ( Sys_IsLANAddress( from ) ) {
		Com_DPrintf( "Sending challenge response directly\n" );
		challenge->pingTime = svs.time;
		NET_OutOfBandPrint( NS_SERVER, from, "challengeResponse %i", challenge->challenge );
		return;
	}

	if ( !svs.authorizeAddress.ip[0] && svs.authorizeAddress.type != NA_BAD ) {
		Com_Printf( "Resolving %s\n", AUTHORIZE_SERVER_NAME );
		if ( !NET_StringToAdr( AUTHORIZE_SERVER_NAME, &svs.authorizeAddress ) ) {
			Com_Printf( "Couldn't resolve address\n" );
			return;
		}
		svs.authorizeAddress.port = BigShort( PORT_AUTHORIZE );
		Com_Printf( "%s resolved to port %i\n", AUTHORIZE_SERVER_NAME,
			BigShort( svs.authorizeAddress.port ) );
	}

	if ( svs.time - challenge->firstTime > AUTHORIZE_TIMEOUT ) {
		Com_DPrintf( "SV_GetChallenge: Authorize server timed out, challenge: %d\n", challenge->challenge );
		challenge->pingTime = svs.time;
		NET_OutOfBandPrint( NS_SERVER, challenge->adr, "challengeResponse %i", challenge->challenge );
		return;
	}

	if ( svs.authorizeAddress.type == NA_BAD ) {
		return;
	}

	fs_game[0] = 0;
	cv = Cvar_Get( "fs_game", "", CVAR_SYSTEMINFO | CVAR_INIT );
	if ( cv && cv->string[0] ) {
		strcpy( fs_game, cv->string );
	}

	Com_DPrintf( "sending getIpAuthorize for %s challenge %d\n",
		NET_AdrToLogString( from ), challenge->challenge );

	cv = Cvar_Get( "sv_allowAnonymous", "0", CVAR_SERVERINFO );

	NET_OutOfBandPrint( NS_SERVER, svs.authorizeAddress,
		"getIpAuthorize %i %i.%i.%i.%i %s %i", challenge->challenge,
		from.ip[0], from.ip[1], from.ip[2], from.ip[3], fs_game,
		cv->integer );
}

#ifndef STANDALONE
void SV_AuthorizeIpPacket( netadr_t from ) {
	int		challenge;
	int		i;
	char	*s;
	char	*r;
	challenge_t *challengeptr;

	if ( !NET_CompareBaseAdr( from, svs.authorizeAddress ) ) {
		Com_Printf( "SV_AuthorizeIpPacket: not from authorize server\n" );
		return;
	}

	challenge = atoi( Cmd_Argv( 1 ) );

	for (i = 0 ; i < MAX_CHALLENGES ; i++) {
		if ( svs.challenges[i].challenge == challenge ) {
			break;
		}
	}
	if ( i == MAX_CHALLENGES ) {
		Com_Printf( "SV_AuthorizeIpPacket: challenge not found\n" );
		return;
	}
	
	challengeptr = &svs.challenges[i];

	challengeptr->pingTime = svs.time;
	s = Cmd_Argv( 2 );
	r = Cmd_Argv( 3 );			

	if ( !Q_stricmp( s, "demo" ) ) {
		NET_OutOfBandPrint( NS_SERVER, challengeptr->adr, "print\nServer is not a demo server\n" );
		Com_Memset( challengeptr, 0, sizeof( *challengeptr ) );
		return;
	}
	if ( !Q_stricmp( s, "accept" ) ) {
		// MOHAA/ioq3 added those, and retail's protocol is not negotiated (see
		NET_OutOfBandPrint( NS_SERVER, challengeptr->adr,
			"challengeResponse %d", challengeptr->challenge);
		return;
	}
	if ( !Q_stricmp( s, "unknown" ) ) {
		if (!r) {
			NET_OutOfBandPrint( NS_SERVER, challengeptr->adr, "print\nAwaiting CD key authorization\n" );
		} else {
			NET_OutOfBandPrint( NS_SERVER, challengeptr->adr, "print\n%s\n", r);
		}
		Com_Memset( challengeptr, 0, sizeof( *challengeptr ) );
		return;
	}

	if (!r) {
		NET_OutOfBandPrint( NS_SERVER, challengeptr->adr, "print\nSomeone is using this CD Key\n" );
	} else {
		NET_OutOfBandPrint( NS_SERVER, challengeptr->adr, "print\n%s\n", r );
	}

	Com_Memset( challengeptr, 0, sizeof(*challengeptr) );
}
#endif

challenge_t* FindExistingChallenge(netadr_t from) {
	int i;

	for (i=0; i<MAX_CHALLENGES; i++)
	{
		if (NET_CompareAdr(from, svs.challenges[i].adr))
		{
			return &svs.challenges[i];
		}
	}

	return NULL;
}


void SV_DirectConnect( netadr_t from ) {
	char		userinfo[MAX_INFO_STRING];
	int			i;
	client_t	*cl, *newcl;
	static client_t	temp; 
	gentity_t	*ent;
	int			clientNum;
	int			version;
	int			qport;
	int			challenge;
	char		*password;
	int			startIndex;
	const char	*denied;
	char		*ip;
	int			count;

	Com_DPrintf ("SVC_DirectConnect ()\n");

	Q_strncpyz( userinfo, Cmd_Argv(1), sizeof(userinfo) );

	version = atoi(Info_ValueForKey(userinfo, "protocol"));
	if (version != 66) {
		NET_OutOfBandPrint( NS_SERVER, from, "print\nYour version of EF2 is outdated.  Download the latest update.\n" );
		Com_DPrintf("    rejected connect from version %i\n", version);
		return;
	}

	qport = atoi( Info_ValueForKey( userinfo, "qport" ) );
	challenge = atoi( Info_ValueForKey( userinfo, "challenge" ) );

	if ( NET_IsLocalAddress (from) ) {
		ip = "localhost";
	} else {
		for (i=0; i<MAX_CHALLENGES; i++) {
			if (NET_CompareAdr(from, svs.challenges[i].adr)) {
				break;
			}
		}
		if (i == MAX_CHALLENGES) {
			NET_OutOfBandPrint( NS_SERVER, from, "print\nNo challenge for address.\n" );
			return;
		}
		if (challenge != svs.challenges[i].challenge) {
			NET_OutOfBandPrint( NS_SERVER, from, "print\nBad challenge.\n" );
			return;
		}
		ip = (char *)NET_AdrToString( from );
	}
	Info_SetValueForKey( userinfo, "ip", ip );

	for (i=0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++) {
		if ( cl->state == CS_FREE ) {
			continue;
		}
		if ( NET_CompareBaseAdr( from, cl->netchan.remoteAddress )
			&& ( cl->netchan.qport == qport
			|| from.port == cl->netchan.remoteAddress.port ) ) {
			if (( svs.time - cl->lastConnectTime)
				< (sv_reconnectlimit->integer * 1000)) {
				Com_DPrintf ("%s:reconnect rejected : too soon\n", NET_AdrToLogString (from));
				return;
			}
			break;
		}
	}

	newcl = &temp;
	Com_Memset (newcl, 0, sizeof(client_t));

	for (i=0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++) {
		if ( cl->state == CS_FREE ) {
			continue;
		}
		if ( NET_CompareBaseAdr( from, cl->netchan.remoteAddress )
			&& ( cl->netchan.qport == qport
			|| from.port == cl->netchan.remoteAddress.port ) ) {
			Com_Printf ("%s:reconnect\n", NET_AdrToLogString (from));
			newcl = cl;
			goto gotnewcl;
		}
	}

	password = Info_ValueForKey( userinfo, "password" );
	if ( *password && !strcmp( password, sv_privatePassword->string ) ) {
		startIndex = 0;
	} else {
		startIndex = sv_privateClients->integer;
	}

	newcl = NULL;
	for ( i = startIndex; i < sv_maxclients->integer ; i++ ) {
		cl = &svs.clients[i];
		if (cl->state == CS_FREE) {
			newcl = cl;
			break;
		}
	}

	if ( !newcl ) {
		NET_OutOfBandPrint( NS_SERVER, from, "print\nServer is full.\n" );
		Com_DPrintf ("Rejected a connection.\n");
		return;
	}

gotnewcl:
	*newcl = temp;
	clientNum = newcl - svs.clients;
	ent = SV_GentityNum( clientNum );
	newcl->gentity = ent;
	newcl->number_of_server_sounds = 0;

	newcl->challenge = challenge;

	Netchan_Setup(NS_SERVER, &newcl->netchan, from, qport);

	Q_strncpyz( newcl->userinfo, userinfo, sizeof( newcl->userinfo ) );

	denied = ge->ClientConnect( clientNum, qtrue, qfalse, qtrue );
	if ( denied ) {
		NET_OutOfBandPrint( NS_SERVER, from, "print\n%s\n", denied );
		Com_DPrintf ( "Game rejected a connection: %s.\n", denied );
		return;
	}

	SV_UserinfoChanged( newcl );

	NET_OutOfBandPrint( NS_SERVER, from, "connectResponse" );

	Com_DPrintf( "Going from CS_FREE to CS_CONNECTED for %s\n", newcl->name );

	newcl->state = CS_CONNECTED;
	newcl->nextSnapshotTime = svs.time + 800;
	newcl->lastPacketTime = svs.time + 800;
	newcl->lastConnectTime = svs.time + 800;
	newcl->gamestateTime = 0;

	newcl->gamestateMessageNum = -1;

	count = 0;
	for (i=0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			count++;
		}
	}
	if ( count == 1 || count == sv_maxclients->integer ) {
		SV_Heartbeat_f();
	}
}


void SV_FreeClient(client_t *client)
{
#ifdef USE_VOIP
#endif

}

void SV_DropClient( client_t *drop, const char *reason, qboolean freeClient ) {
	int		i;
	int		clientNum;

	if ( drop->state == CS_ZOMBIE ) {
		return;		
	}

	clientNum = drop - svs.clients;

	Com_DPrintf( "Dropping client (%s) - %s\n", drop->name, reason );

	for ( i = 0 ; i < 2 ; i++ ) {
		if ( drop->state >= CS_CONNECTED ) {
			if ( drop->netchan.remoteAddress.type != NA_LOOPBACK ) {
				SV_SendServerCommand( drop, "disconnect \"%s\"", reason );
			}
			drop->nextSnapshotTime = -1;
			SV_SendClientSnapshot( drop );
		}
	}

	Com_DPrintf( "Going to CS_ZOMBIE for %s\n", drop->name );

	if ( freeClient ) {
		drop->state = CS_FREE;		
	} else {
		drop->state = CS_ZOMBIE;	
	}

	SV_FreeClient(drop);

	for ( i = 0 ; i < MAX_CLIENTS ; i++ ) {
		if ( !stricmp( Info_ValueForKey( SV_GetConfigstring( CS_PLAYERS + i ), "name" ), drop->name ) ) {
			SV_SetConfigstring( CS_PLAYERS + i, "" );
			break;
		}
	}

	SV_SetUserinfo( clientNum, "" );

	ge->ClientDisconnect( ( gentity_t * )SV_GentityNum( clientNum ) );

	{
		const char *err = ge->error_message;
		if ( err ) {
			ge->error_message = NULL;
			Com_Error( ERR_DROP, "%s", err );
		}
	}

	// 7 x 1024 == 7168 in that run.  Retail never clears a challenge here, so
	for ( i = 0 ; i < sv_maxclients->integer ; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			break;
		}
	}
	if ( i == sv_maxclients->integer ) {
		SV_Heartbeat_f();
	}

}

static void SV_SendClientGameState( client_t *client ) {
	int			start;
	int			size;
	entityState_t	*base, nullstate;
	msg_t		msg;
	byte		msgBuffer[MAX_MSGLEN];

	Com_DPrintf ("SV_SendClientGameState() for %s\n", client->name);
	Com_DPrintf( "Going from CS_CONNECTED to CS_PRIMED for %s\n", client->name );
	client->state = CS_PRIMED;
	client->pureAuthentic = 0;

	client->gamestateMessageNum = client->netchan.outgoingSequence;

	// EF2 world state: DELIBERATE DIVERGENCE FROM RETAIL, see below.
	// Retail does NOT resynchronise them: SV_SendClientGameState @0x001D9100 never touches
	// case, not the corner case -- it would black out grouped lighting in all 25 shipped
	Com_Memset( &client->ws, 0, sizeof( client->ws ) );

	MSG_Init( &msg, msgBuffer, sizeof( msgBuffer ) );

	MSG_WriteLong( &msg, client->lastClientCommand );

	MSG_WriteSVC( &msg, svc_gamestate );
	MSG_WriteLong( &msg, client->reliableSequence );

	for ( start = 0 ; start < MAX_CONFIGSTRINGS ; start++ ) {
		if (sv.configstrings[start][0]) {
			MSG_WriteSVC( &msg, svc_configstring );
			MSG_WriteShort( &msg, start );
			MSG_WriteBigString( &msg, sv.configstrings[start] );
		}
	}

	MSG_GetNullEntityState( &nullstate );
	for ( start = 0 ; start < MAX_GENTITIES; start++ ) {
		base = &sv.svEntities[start].baseline;
		if ( !base->number ) {
			continue;
		}
		MSG_WriteSVC( &msg, svc_baseline );
		MSG_WriteDeltaEntity(&msg, &nullstate, base, qtrue);
	}

	MSG_WriteByte( &msg, svc_EOF );

	MSG_WriteLong( &msg, client - svs.clients);

	MSG_WriteLong( &msg, sv.checksumFeed);


	size = SV_SendMessageToClient( &msg, client );
	if ( client->netchan.remoteAddress.type == NA_LOOPBACK ) {
		client->nextFragmentTime = 0;
	} else {
		client->nextFragmentTime = svs.time + SV_RateMsec( client, size );
	}
	client->gamestateTime = svs.time;
}


void SV_ClientEnterWorld( client_t *client, usercmd_t *cmd ) {
	int		clientNum;
	gentity_t *ent;

	Com_DPrintf( "Going from CS_PRIMED to CS_ACTIVE for %s\n", client->name );
	client->state = CS_ACTIVE;


	clientNum = client - svs.clients;
	ent = SV_GentityNum( clientNum );
	ent->s.number = clientNum;
	client->gentity = ent;

	client->deltaMessage = -1;
	if ( sv_maxclients->integer == 1 ) {
		client->nextSnapshotTime = svs.time + 500;
	} else {
		client->nextSnapshotTime = svs.time;
	}

	if(cmd)
		memcpy(&client->lastUsercmd, cmd, sizeof(client->lastUsercmd));
	else
		memset(&client->lastUsercmd, '\0', sizeof(client->lastUsercmd));

	ge->ClientBegin( ( gentity_t * )ent, cmd );

}



static void SV_Disconnect_f( client_t *cl ) {
	SV_DropClient( cl, va( "%s $$Disconnected$$", cl->name ), qfalse );
}

static void SV_VerifyPaks_f( client_t *cl ) {
	int nChkSum1, nChkSum2, nClientPaks, nServerPaks, i, j, nCurArg;
	int nClientChkSum[1024];
	int nServerChkSum[1024];
	const char *pPaks;
	qboolean bGood = qtrue;

	if ( sv_pure->integer != 0 ) {

		nChkSum1 = nChkSum2 = 0;

		nClientPaks = Cmd_Argc();

		nCurArg = 1;
	
		while (bGood) {

			for (i = 0; nCurArg < nClientPaks; i++) {
				nClientChkSum[i] = atoi(Cmd_Argv(nCurArg++));
			}

			nClientPaks = i - 1;

			for (i = 0; i < nClientPaks; i++) {
				for (j = 0; j < nClientPaks; j++) {
					if (i == j)
						continue;
					if (nClientChkSum[i] == nClientChkSum[j]) {
						bGood = qfalse;
						break;
					}
				}
				if (bGood == qfalse)
					break;
			}
			if (bGood == qfalse)
				break;

			pPaks = FS_LoadedPakPureChecksums();
			Cmd_TokenizeString( pPaks );
			nServerPaks = Cmd_Argc();
			if (nServerPaks > 1024)
				nServerPaks = 1024;

			for (i = 0; i < nServerPaks; i++) {
				nServerChkSum[i] = atoi(Cmd_Argv(i));
			}

			for (i = 0; i < nClientPaks; i++) {
				for (j = 0; j < nServerPaks; j++) {
					if (nClientChkSum[i] == nServerChkSum[j]) {
						break;
					}
				}
				if (j >= nServerPaks) {
					bGood = qfalse;
					break;
				}
			}
			if ( bGood == qfalse ) {
				break;
			}

			nChkSum1 = sv.checksumFeed;
			for (i = 0; i < nClientPaks; i++) {
				nChkSum1 ^= nClientChkSum[i];
			}
			nChkSum1 ^= nClientPaks;
			if (nChkSum1 != nClientChkSum[nClientPaks]) {
				bGood = qfalse;
				break;
			}

			break;
		}

		if (bGood) {
			cl->pureAuthentic = 1;
		}
		else {
			cl->pureAuthentic = 0;
			cl->nextSnapshotTime = 0;
			cl->state = CS_ACTIVE;
			SV_SendClientSnapshot( cl );
			SV_DropClient( cl, "$$UnpureClient$$", qtrue );
		}
	}
}

static void SV_ResetPureClient_f( client_t *cl ) {
	cl->pureAuthentic = 0;
}

void SV_UserinfoChanged( client_t *cl ) {
	char	*val;
	int		i;

	Q_strncpyz( cl->name, Info_ValueForKey (cl->userinfo, "name"), sizeof(cl->name) );


	if ( Sys_IsLANAddress( cl->netchan.remoteAddress ) && com_dedicated->integer != 2 ) {
		cl->rate = 99999;	
	} else {
		val = Info_ValueForKey (cl->userinfo, "rate");
		if (strlen(val)) {
			i = atoi(val);
			cl->rate = i;
			if (cl->rate < 1000) {
				cl->rate = 1000;
			} else if (cl->rate > 90000) {
				cl->rate = 90000;
			}
		} else {
			cl->rate = 3000;
		}
	}

	val = Info_ValueForKey (cl->userinfo, "snaps");
	if (strlen(val)) {
		i = atoi(val);
		if (i < 1) {
			i = 1;
		} else if (i > 30) {
			i = 30;
		}
		cl->snapshotMsec = 1000 / i;
	} else {
		cl->snapshotMsec = 50;
	}
}


static void SV_UpdateUserinfo_f( client_t *cl ) {
	Q_strncpyz( cl->userinfo, Cmd_Argv(1), sizeof(cl->userinfo) );

	SV_UserinfoChanged( cl );
	ge->ClientUserinfoChanged( ( gentity_t * )SV_GentityNum( cl - svs.clients ), cl->userinfo );
}


#ifdef USE_VOIP
#endif


typedef struct {
	char	*name;
	void	(*func)( client_t *cl );
} ucmd_t;

static ucmd_t ucmds[] = {
	{"userinfo", SV_UpdateUserinfo_f},
	{"disconnect", SV_Disconnect_f},
	{"cp", SV_VerifyPaks_f},
	{"vdr", SV_ResetPureClient_f},

#ifdef USE_VOIP
#endif

	{NULL, NULL}
};

// string "client text" does not exist in the retail engine image.
void SV_ExecuteClientCommand( client_t *cl, const char *s ) {
	ucmd_t	*u;
	
	Cmd_TokenizeString( s );

	for (u=ucmds ; u->name ; u++) {
		if (!strcmp (Cmd_Argv(0), u->name) ) {
			u->func( cl );
			break;
		}
	}

	if (!u->name && sv.state == SS_GAME) {
		const char *err;

		ge->ClientCommand( cl->gentity );

		err = ge->error_message;
		if (err)
		{
			ge->error_message = NULL;
			Com_Error(ERR_DROP, "%s", err);
		}
	}
}

static qboolean SV_ClientCommand( client_t *cl, msg_t *msg ) {
	int		seq;
	const char	*s;
	qboolean stalled = qfalse;

	seq = MSG_ReadLong( msg );
	s = MSG_ReadString( msg );

	if ( cl->lastClientCommand >= seq ) {
		return qtrue;
	}

	if( developer->integer == 2 ) {
		Com_DPrintf( "clientCommand: %s : %i : %s\n", cl->name, seq, s );
	}

	if ( seq > cl->lastClientCommand + 1 ) {
		Com_Printf( "Client %s lost %i clientCommands\n", cl->name, 
			seq - cl->lastClientCommand + 1 );
		SV_DropClient( cl, "Lost reliable commands", qfalse );
		return qfalse;
	}

	// Retail SV_ClientCommand @0x001D9CE0 keeps TWO deadlines, not the one
	{
		qboolean	chat = qfalse;

		if ( sv_floodProtect->integer ) {
			if ( !Q_stricmpn( s, "say", 3 ) || !Q_stricmpn( s, "tsay", 4 ) || !Q_stricmpn( s, "tell", 4 ) ) {
				chat = qtrue;
			}

			stalled = ( chat && svs.time < cl->nextReliableTimeChat )
					|| ( !chat && svs.time < cl->nextReliableTime
						&& Q_stricmpn( s, "use", 3 ) && Q_stricmpn( s, "cp", 2 ) );
			if ( stalled ) {
				Com_DPrintf( "stalled clientCommand (%s) for %s\n", s, cl->name );
			}
		}

		if ( !stalled ) {
			if ( chat ) {
				cl->nextReliableTimeChat = svs.time + 500;
			} else {
				cl->nextReliableTime = svs.time + 1000;
			}
		}
	}

	if ( !stalled ) {
		SV_ExecuteClientCommand( cl, s );
	}

	cl->lastClientCommand = seq;
	Com_sprintf(cl->lastClientCommandString, sizeof(cl->lastClientCommandString), "%s", s);

	return qtrue;		
}




void SV_ClientThink (client_t *cl, usercmd_t *cmd) {
	const char *err;

	cl->lastUsercmd = *cmd;

	if ( cl->state != CS_ACTIVE ) {
		return;		
	}

	ge->ClientThink( ( gentity_t * )SV_GentityNum( cl - svs.clients ), cmd );  

	err = ge->error_message;
	if( err )
	{
		ge->error_message = NULL;
		Com_Error(ERR_DROP, "%s", err);
	}
}

static void SV_UserMove( client_t *cl, msg_t *msg, qboolean delta ) {
	int			i, key;
	int			cmdCount;
	usercmd_t	nullcmd;
	usercmd_t	cmds[MAX_PACKET_USERCMDS];
	usercmd_t	*cmd, *oldcmd;
	qboolean	extended;

	if ( delta ) {
		cl->deltaMessage = cl->messageAcknowledge;
	} else {
		cl->deltaMessage = -1;
	}

	cmdCount = MSG_ReadByte( msg );

	if ( cmdCount < 1 ) {
		Com_Printf( "cmdCount < 1\n" );
		return;
	}

	if ( cmdCount > MAX_PACKET_USERCMDS ) {
		Com_Printf( "cmdCount > MAX_PACKET_USERCMDS\n" );
		return;
	}

	key = sv.checksumFeed;
	key ^= cl->messageAcknowledge;
	key ^= MSG_HashKey(cl->reliableCommands[ cl->reliableAcknowledge & (MAX_RELIABLE_COMMANDS-1) ], 32);


	extended = (qboolean)( Cvar_VariableIntegerValue( "g_gametype" ) == 0 );

	Com_Memset( &nullcmd, 0, sizeof(nullcmd) );
	oldcmd = &nullcmd;
	for ( i = 0 ; i < cmdCount ; i++ ) {
		cmd = &cmds[i];
		MSG_ReadDeltaUsercmdKey( msg, key, oldcmd, cmd, extended );
		oldcmd = cmd;
	}

	cl->frames[ cl->messageAcknowledge & PACKET_MASK ].messageAcked = svs.time;

	if ( cl->state == CS_PRIMED ) {
		SV_ClientEnterWorld( cl, &cmds[0] );
	}

	if (sv_pure->integer != 0 && cl->pureAuthentic == 0) {
		SV_DropClient( cl, "$$CannotValidatePureClient$$", qtrue );
		return;
	}

	if ( cl->state != CS_ACTIVE ) {
		cl->deltaMessage = -1;
		return;
	}

	for ( i =  0 ; i < cmdCount ; i++ ) {
		if ( cmds[i].serverTime > cmds[cmdCount-1].serverTime ) {
			continue;
		}
		SV_ClientThink (cl, &cmds[ i ]);
	}
}


#ifdef USE_VOIP
#endif




void SV_ExecuteClientMessage( client_t *cl, msg_t *msg ) {
	int			c;
	int			serverId;

	MSG_Bitstream(msg);

	serverId = MSG_ReadLong( msg );
	cl->messageAcknowledge = MSG_ReadLong( msg );

	if ( cl->messageAcknowledge >= 0 ) {
		cl->reliableAcknowledge = MSG_ReadLong( msg );

		if ( cl->reliableSequence - cl->reliableAcknowledge <= MAX_RELIABLE_COMMANDS ) {

			if ( serverId == sv.serverId ) {

				do {
					c = MSG_ReadByte( msg );

					if ( c == clc_EOF ) {
						break;
					}

					if ( c != clc_clientCommand ) {
						break;
					}
					if ( !SV_ClientCommand( cl, msg ) ) {
						return;	
					}
					if (cl->state == CS_ZOMBIE) {
						return;	
					}
				} while ( 1 );

				if ( c == clc_move ) {
					SV_UserMove( cl, msg, qtrue );
				} else if ( c == clc_moveNoDelta ) {
					SV_UserMove( cl, msg, qfalse );
				} else if ( c != clc_EOF ) {
					Com_Printf( "WARNING: bad command byte for client %i\n", (int) (cl - svs.clients) );
				}
			}
			else if ( serverId != sv.restartedServerId ) {
				if ( !cl->gamestateTime || cl->gamestateTime + 5000 < svs.time ) {
					LOADHANG_PROBE( "gssend",
					            "num=%d serverId=%d svSid=%d rsSid=%d gsTime=%d svs.time=%d cs=%d\n",
					            (int)( cl - svs.clients ), serverId, sv.serverId, sv.restartedServerId,
					            cl->gamestateTime, svs.time, cl->state );
					Com_DPrintf( "Sending gamestate to %s\n", cl->name );
					if ( cvar_modifiedFlags & CVAR_SERVERINFO ) {
						SV_SetConfigstring( CS_SERVERINFO, Cvar_InfoString( CVAR_SERVERINFO ) );
						cvar_modifiedFlags &= ~CVAR_SERVERINFO;
					}
					if ( cvar_modifiedFlags & CVAR_SYSTEMINFO ) {
						SV_SetConfigstring( CS_SYSTEMINFO, Cvar_InfoString_Big( CVAR_SYSTEMINFO ) );
						cvar_modifiedFlags &= ~CVAR_SYSTEMINFO;
					}
					SV_AddServerCommand( cl, "" );
					SV_SendClientGameState( cl );
				} else {
					LOADHANG_PROBE( "gate",
					            "BLOCKED num=%d serverId=%d svSid=%d rsSid=%d gsTime=%d svs.time=%d shutms=%d cs=%d\n",
					            (int)( cl - svs.clients ), serverId, sv.serverId, sv.restartedServerId,
					            cl->gamestateTime, svs.time,
					            cl->gamestateTime + 5000 - svs.time, cl->state );
				}
			}
			else {
				cl->reliableAcknowledge = cl->reliableSequence;
			}
		}
	}
}


unsigned int SV_Client_GetNumPendingCommands(client_t *cl)
{
	return cl->reliableSequence - cl->reliableAcknowledge;
}

unsigned int SV_Client_GetMaxPendingCommands(client_t *cl)
{
	return MAX_RELIABLE_COMMANDS;
}
