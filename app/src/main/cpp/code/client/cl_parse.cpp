
#include "client.h"
#include "cl_ui.h"
#define PROBE_GET(name, def, flags) Cvar_Get(name, def, flags)
#define PROBE_PRINT                 Com_Printf
#include "../qcommon/qa_probe.h"

const char *svc_strings[256] = {
	"svc_bad",

	"svc_nop",
	"svc_gamestate",
	"svc_configstring",
	"svc_baseline",	
	"svc_serverCommand",
	"svc_download",
	"svc_snapshot",
	"svc_centerprint",
	"svc_locprint"
};

void SHOWNET( msg_t *msg, const char *s) {
	if ( cl_shownet->integer >= 2) {
		Com_Printf ("%3i:%s\n", msg->readcount-1, s);
	}
}



void CL_DeltaEntity (msg_t *msg, clSnapshot_t *frame, int newnum, entityState_t *old, 
					 qboolean unchanged) {
	entityState_t	*state;

	state = &cl.parseEntities[cl.parseEntitiesNum & (MAX_PARSE_ENTITIES-1)];

	if ( unchanged ) {
		*state = *old;
	} else {
		MSG_ReadDeltaEntity( msg, old, state, newnum);
	}

	if ( state->number == (MAX_GENTITIES-1) ) {
		return;		
	}
	cl.parseEntitiesNum++;
	frame->numEntities++;
}

void CL_ParsePacketEntities( msg_t *msg, clSnapshot_t *oldframe, clSnapshot_t *newframe) {
	int			newnum;
	entityState_t	*oldstate;
	int			oldindex, oldnum;

	newframe->parseEntitiesNum = cl.parseEntitiesNum;
	newframe->numEntities = 0;

	oldindex = 0;
	oldstate = NULL;
	if (!oldframe) {
		oldnum = 99999;
	} else {
		if ( oldindex >= oldframe->numEntities ) {
			oldnum = 99999;
		} else {
			oldstate = &cl.parseEntities[
				(oldframe->parseEntitiesNum + oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}

	while ( 1 ) {
		newnum = MSG_ReadEntityNum(msg);

		if ( newnum == (MAX_GENTITIES-1) ) {
			break;
		}

		if ( (size_t)msg->readcount > msg->cursize ) {
			Com_Error (ERR_DROP,"CL_ParsePacketEntities: end of message");
		}

		while ( oldnum < newnum ) {
			if ( cl_shownet->integer == 3 ) {
				Com_Printf ("%3i:  unchanged: %i\n", msg->readcount, oldnum);
			}
			CL_DeltaEntity( msg, newframe, oldnum, oldstate, qtrue );
			
			oldindex++;

			if ( oldindex >= oldframe->numEntities ) {
				oldnum = 99999;
			} else {
				oldstate = &cl.parseEntities[
					(oldframe->parseEntitiesNum + oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
		}
		if (oldnum == newnum) {
			if ( cl_shownet->integer == 3 ) {
				Com_Printf ("%3i:  delta: %i\n", msg->readcount, newnum);
			}
			CL_DeltaEntity( msg, newframe, newnum, oldstate, qfalse );

			oldindex++;

			if ( oldindex >= oldframe->numEntities ) {
				oldnum = 99999;
			} else {
				oldstate = &cl.parseEntities[
					(oldframe->parseEntitiesNum + oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
			continue;
		}

		if ( oldnum > newnum ) {
			if ( cl_shownet->integer == 3 ) {
				Com_Printf ("%3i:  baseline: %i\n", msg->readcount, newnum);
			}
			CL_DeltaEntity( msg, newframe, newnum, &cl.entityBaselines[newnum], qfalse );
			continue;
		}

	}

	while ( oldnum != 99999 ) {
		if ( cl_shownet->integer == 3 ) {
			Com_Printf ("%3i:  unchanged: %i\n", msg->readcount, oldnum);
		}
		CL_DeltaEntity( msg, newframe, oldnum, oldstate, qtrue );
		
		oldindex++;

		if ( oldindex >= oldframe->numEntities ) {
			oldnum = 99999;
		} else {
			oldstate = &cl.parseEntities[
				(oldframe->parseEntitiesNum + oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}
}


void CL_ParseSnapshot( msg_t *msg ) {
	int			len;
	clSnapshot_t	*old;
	clSnapshot_t	newSnap;
	int			deltaNum;


	Com_Memset (&newSnap, 0, sizeof(newSnap));

	newSnap.serverCommandNum = clc.serverCommandSequence;

	newSnap.serverTime = MSG_ReadLong( msg );
	newSnap.serverTimeResidual = MSG_ReadByte( msg );

	newSnap.messageNum = clc.serverMessageSequence;

	deltaNum = MSG_ReadByte( msg );
	if ( !deltaNum ) {
		newSnap.deltaNum = -1;
	} else {
		newSnap.deltaNum = newSnap.messageNum - deltaNum;
	}
	newSnap.snapFlags = MSG_ReadByte( msg );

	if ( newSnap.deltaNum <= 0 ) {
		newSnap.valid = qtrue;		
		old = NULL;
		clc.demowaiting = qfalse;	
	} else {
		old = &cl.snapshots[newSnap.deltaNum & PACKET_MASK];
		if ( !old->valid ) {
			Com_WPrintf ("Delta from invalid frame (not supposed to happen!).\n");
		} else if ( old->messageNum != newSnap.deltaNum ) {
			Com_WPrintf ("Delta frame too old.\n");
		} else if ( cl.parseEntitiesNum - old->parseEntitiesNum > MAX_PARSE_ENTITIES-128 ) {
			Com_WPrintf ("Delta parseEntitiesNum too old.\n");
		} else {
			newSnap.valid = qtrue;	
		}
	}

	len = MSG_ReadByte( msg );
	
	if((size_t)len > sizeof(newSnap.areamask))
	{
		Com_Error (ERR_DROP,"CL_ParseSnapshot: Invalid size %d for areamask.", len);
		return;
	}
	
	MSG_ReadData( msg, &newSnap.areamask, len);

    SHOWNET(msg, "playerstate");
	if ( old ) {
		MSG_ReadDeltaPlayerstate( msg, &old->ps, &newSnap.ps );
	} else {
		MSG_ReadDeltaPlayerstate( msg, NULL, &newSnap.ps );
	}

	SHOWNET( msg, "packet entities" );
	CL_ParsePacketEntities( msg, old, &newSnap );

	MSG_ReadSounds( msg, newSnap.sounds, &newSnap.number_of_sounds );

	MSG_ReadWorldInfo( msg, &cl.snap.ws, &newSnap.ws );

	re.SetDynamicLights( newSnap.ws.dynamic_light_default_intensities,
		newSnap.ws.dynamic_light_intensities );
	re.SetWindDirection( newSnap.ws.wind_direction );
	re.SetWindIntensity( newSnap.ws.wind_intensity );
	re.SetTimeScale( newSnap.ws.time_scale );

	if ( !newSnap.valid ) {
		return;
	}

	CL_CommitSnapshot( &newSnap );

}

void CL_SnapshotWorldState( worldState_t *ws ) {
	re.SetDynamicLights( ws->dynamic_light_default_intensities, ws->dynamic_light_intensities );
	re.SetWindDirection( ws->wind_direction );
	re.SetWindIntensity( ws->wind_intensity );
	re.SetTimeScale( ws->time_scale );
}

void CL_CommitSnapshot( clSnapshot_t *newSnap ) {
	int			oldMessageNum;
	int			i, packetNum;

	oldMessageNum = cl.snap.messageNum + 1;

	if ( newSnap->messageNum - oldMessageNum >= PACKET_BACKUP ) {
		oldMessageNum = newSnap->messageNum - ( PACKET_BACKUP - 1 );
	}
	for ( ; oldMessageNum < newSnap->messageNum ; oldMessageNum++ ) {
		cl.snapshots[oldMessageNum & PACKET_MASK].valid = qfalse;
	}

	if( cl.snap.valid && ( newSnap->snapFlags ^ cl.snap.snapFlags ) & SNAPFLAG_SERVERCOUNT ) {
		cl.serverStartTime = newSnap->serverTime;
	}

	cl.snap = *newSnap;
	cl.snap.ping = 999;
	for ( i = 0 ; i < PACKET_BACKUP ; i++ ) {
		packetNum = ( clc.netchan.outgoingSequence - 1 - i ) & PACKET_MASK;
		if ( cl.snap.ps.commandTime == cl.outPackets[ packetNum ].p_serverTime ) {
			cl.snap.ping = cls.realtime - cl.outPackets[ packetNum ].p_realtime;
			break;
		}
	}
	cl.snapshots[cl.snap.messageNum & PACKET_MASK] = cl.snap;

	{
		static int widget_m1_lastFlags = -1;

		if ( (int)newSnap->snapFlags != widget_m1_lastFlags ) {
			widget_m1_lastFlags = newSnap->snapFlags;
			WIDGETM1_PROBE( "commit", "state=%d snapFlags=%d msgNum=%d snapTime=%d cmdNum=%d serverCmdSeq=%d",
				cls.state, newSnap->snapFlags, newSnap->messageNum, newSnap->serverTime,
				newSnap->cmdNum, newSnap->serverCommandNum );
		}
	}

	if (cl_shownet->integer == 3) {
		Com_Printf( "   snapshot:%i  delta:%i  ping:%i\n", cl.snap.messageNum,
		cl.snap.deltaNum, cl.snap.ping );
	}

	cl.newSnapshots = qtrue;
}



int cl_connectedToPureServer;
int cl_connectedToCheatServer;

void CL_SystemInfoChanged( void ) {
	char			*systemInfo;
	const char		*s, *t;
	char			key[BIG_INFO_KEY];
	char			value[BIG_INFO_VALUE];
	qboolean		gameSet;

	systemInfo = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SYSTEMINFO ];
	cl.serverId = atoi( Info_ValueForKey( systemInfo, "sv_serverid" ) );

	if ( clc.demoplaying ) {
		return;
	}

	s = Info_ValueForKey( systemInfo, "cheats" );
	cl_connectedToCheatServer = atoi( s );
	if ( !cl_connectedToCheatServer ) {
		Cvar_SetCheatState();
	}

	s = Info_ValueForKey( systemInfo, "sv_paks" );
	t = Info_ValueForKey( systemInfo, "sv_pakNames" );
	FS_PureServerSetLoadedPaks( s, t );

	s = Info_ValueForKey( systemInfo, "sv_referencedPaks" );
	t = Info_ValueForKey( systemInfo, "sv_referencedPakNames" );
	FS_PureServerSetReferencedPaks( s, t );

	gameSet = qfalse;
	s = systemInfo;
	while ( s ) {
		int cvar_flags;
		
		Info_NextPair( &s, key, value );
		if ( !key[0] ) {
			break;
		}
		
		if (!Q_stricmp(key, "fs_game"))
		{
			if(FS_CheckDirTraversal(value))
			{
				Com_Printf(S_COLOR_YELLOW "WARNING: Server sent invalid fs_game value %s\n", value);
				continue;
			}
				
			gameSet = qtrue;
		}

		if((unsigned int)(cvar_flags = Cvar_Flags(key)) == CVAR_NONEXISTENT)
			Cvar_Get(key, value, CVAR_SERVER_CREATED | CVAR_ROM);
		else
		{
			if(!(cvar_flags & (CVAR_SYSTEMINFO | CVAR_SERVER_CREATED)))
			{
				Com_Printf(S_COLOR_YELLOW "WARNING: server is not allowed to set %s=%s\n", key, value);
				continue;
			}

			Cvar_Set(key, value);
		}
	}
	if ( !gameSet && *Cvar_VariableString("fs_game") ) {
		Cvar_Set( "fs_game", "" );
	}
	cl_connectedToPureServer = 0;
}

void CL_ParseGamestate( msg_t *msg ) {
	int				i, csNum;
	entityState_t	*es;
	int				newnum;
	entityState_t	nullstate;
	int				cmd;
	char			*s;

	WIDGETM1_PROBE( "gs", "PREV state=%d serverId=%d checksumFeed=%d", cls.state, cl.serverId, clc.checksumFeed );

	LOADHANG_PROBE( "gsparse", "state=%d serverId=%d checksumFeed=%d realtime=%d\n",
	            cls.state, cl.serverId, clc.checksumFeed, cls.realtime );

	clc.connectPacketCount = 0;
	if (cls.cgameStarted) {					 
		CL_FlushMemory();
	}

	CL_ClearState();

	MSG_GetNullEntityState( &nullstate );
	for ( i = 0; i < MAX_GENTITIES; i++ ) {
		cl.entityBaselines[ i ] = nullstate;
	}

	clc.serverCommandSequence = MSG_ReadLong( msg );

	clc.serverCommands[ clc.serverCommandSequence & ( MAX_RELIABLE_COMMANDS - 1 ) ][ 0 ] = 0;

	cl.gameState.dataCount = 1;	
	while ( 1 ) {
		cmd = MSG_ReadByte( msg );

		if ( cmd == svc_EOF ) {
			break;
		}
		
		if ( cmd == svc_configstring ) {
            int		len;

            i = MSG_ReadShort(msg);
            csNum = i;
            if (csNum < 0 || csNum >= MAX_CONFIGSTRINGS) {
                Com_Error(ERR_DROP, "configstring > MAX_CONFIGSTRINGS");
				return;
            }
            s = MSG_ReadBigString(msg);
            len = strlen(s);

            if (len + 1 + cl.gameState.dataCount > MAX_GAMESTATE_CHARS) {
                Com_Error(ERR_DROP, "MAX_GAMESTATE_CHARS exceeded");
            }

            cl.gameState.stringOffsets[csNum] = cl.gameState.dataCount;
            Com_Memcpy(cl.gameState.stringData + cl.gameState.dataCount, s, len + 1);
            cl.gameState.dataCount += len + 1;
		} else if ( cmd == svc_baseline ) {
			newnum = MSG_ReadEntityNum(msg);
			if ( newnum < 0 || newnum >= MAX_GENTITIES ) {
				Com_Error( ERR_DROP, "Baseline number out of range: %i", newnum );
			}
			MSG_GetNullEntityState(&nullstate);
			es = &cl.entityBaselines[ newnum ];
			MSG_ReadDeltaEntity( msg, &nullstate, es, newnum);
		} else {
			Com_Error( ERR_DROP, "CL_ParseGamestate: bad command byte %i", cmd );
		}
	}

	clc.clientNum = MSG_ReadLong(msg);
	clc.checksumFeed = MSG_ReadLong( msg );

	CL_SystemInfoChanged();

	FS_ConditionalRestart(clc.checksumFeed, qfalse);    

	cls.state = CA_LOADING;

	CL_StartHunkUsers(qfalse);

	Com_UnPause_f();
}


#ifdef USE_VOIP
	#if 0
	static FILE *encio = NULL;
	if (encio == NULL) encio = fopen("voip-incoming-encoded.bin", "wb");
	if (encio != NULL) { fwrite(encoded, packetsize, 1, encio); fflush(encio); }
	static FILE *decio = NULL;
	if (decio == NULL) decio = fopen("voip-incoming-decoded.bin", "wb");
	if (decio != NULL) { fwrite(decoded+written, numSamples*2, 1, decio); fflush(decio); }
	#endif
#endif


void CL_AddCommandString( int seq, const char *s ) {
	int		index;

	if ( clc.serverCommandSequence >= seq ) {
		return;
	}
	clc.serverCommandSequence = seq;

	index = seq & (MAX_RELIABLE_COMMANDS-1);
	Q_strncpyz( clc.serverCommands[ index ], s, sizeof( clc.serverCommands[ index ] ) );
}

void CL_ParseCommandString( msg_t *msg ) {
	int		seq;
	char	*s;

	seq = MSG_ReadLong( msg );
	s = MSG_ReadString( msg );

	CL_AddCommandString( seq, s );
}

void CL_ParseLocationprint( msg_t *msg ) {
	int x, y;
	char *string;

	x = MSG_ReadShort( msg );
	y = MSG_ReadShort( msg );
	string = MSG_ReadString(msg);

	UI_UpdateLocationPrint( x, y, string, 1.0 );
}

void CL_ParseCenterprint( msg_t *msg ) {
	char *string;

	string = MSG_ReadString( msg );

	UI_UpdateCenterPrint( string, 1.0 );
}

void CL_ParseServerMessage( msg_t *msg ) {
	int			cmd;
	if ( cl_shownet->integer == 1 ) {
		Com_Printf ("%zu ",msg->cursize);
	} else if ( cl_shownet->integer >= 2 ) {
		Com_Printf ("------------------\n");
	}

	MSG_Bitstream(msg);

	clc.reliableAcknowledge = MSG_ReadLong( msg );
	if ( clc.reliableAcknowledge < clc.reliableSequence - MAX_RELIABLE_COMMANDS ) {
		clc.reliableAcknowledge = clc.reliableSequence;
	}

	while ( 1 ) {
		if ( (size_t)msg->readcount > msg->cursize ) {
			Com_Error (ERR_DROP,"CL_ParseServerMessage: read past end of server message");
			break;
		}

		cmd = MSG_ReadByte( msg );
		if ( cmd == svc_EOF) {
			SHOWNET( msg, "END OF MESSAGE" );
			break;
		}

		if ( cl_shownet->integer >= 2 ) {
			if ( !svc_strings[cmd] ) {
				Com_Printf( "%3i:BAD CMD %i\n", msg->readcount-1, cmd );
			} else {
				SHOWNET( msg, svc_strings[cmd] );
			}
		}
	
		switch ( cmd ) {
		default:
			Com_Error (ERR_DROP,"CL_ParseServerMessage: Illegible server message\n");
			break;			
		case svc_nop:
			break;
		case svc_serverCommand:
			CL_ParseCommandString( msg );
			break;
		case svc_gamestate:
			CL_ParseGamestate( msg );
			break;
		case svc_snapshot:
			CL_ParseSnapshot( msg );
			break;
		// The enum slot itself must stay -- retail's svc_strings table (data
		case svc_centerprint:
			CL_ParseCenterprint( msg );
			break;
		case svc_locprint:
			CL_ParseLocationprint( msg );
			break;
		case svc_voipSpeex:
#ifdef USE_VOIP
#endif
			break;
		case svc_voipOpus:
#ifdef USE_VOIP
#endif
			break;
		}
	}
}
