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

#include "client.h"
#include "../gamespy/gcdkey/gcdkeyc.h"
#include "../server/server.h"
#include "cl_ui.h"
#include "cl_cin.h"
#include "../corepp/tiki.h"
#include "../tiki/tiki_skan.h"
#include "../qcommon/gameplaymanager.h"
#include "../sys/sys_local.h"
#include "../qcommon/stringresource.h" 
#include "cl_systeminfo.h"
#include "../qcommon/q_jobs.h" 
#include "../uilib/limit_uiprobe.h" 

#define PROBE_GET(name, def, flags) Cvar_Get(name, def, flags)
#define PROBE_PRINT                 Com_Printf
#include "../qcommon/qa_probe.h"

extern "C" {
	#include "../sys/sys_loadlib.h"
}


#include <climits>

#ifdef USE_RENDERER_DLOPEN
cvar_t* cl_renderer;
#endif

cvar_t	*cl_nodelta;
cvar_t	*cl_debugMove;

cvar_t	*cl_motd;

cvar_t	*rcon_client_password;
cvar_t	*rconAddress;

cvar_t	*cl_timeout;
cvar_t	*cl_maxpackets;
cvar_t	*cl_packetdup;
cvar_t	*cl_timeNudge;
cvar_t	*cl_showTimeDelta;
cvar_t	*cl_freezeDemo;

cvar_t	*cl_shownet;
cvar_t	*cl_showSend;
cvar_t	*cl_timedemo;

cvar_t	*cl_freelook;	 
cvar_t	*cl_sensitivity;

cvar_t	*cl_mouseAccel;
cvar_t	*cl_showMouseRate;

cvar_t	*cl_altbindings;
cvar_t	*cl_ctrlbindings;

cvar_t	*m_pitch;
cvar_t	*m_yaw;
cvar_t	*m_forward;
cvar_t	*m_side;
cvar_t	*m_filter;

cvar_t	*j_pitch;
cvar_t	*j_yaw;
cvar_t	*j_forward;
cvar_t	*j_side;
cvar_t	*j_up;
cvar_t	*j_pitch_axis;
cvar_t	*j_yaw_axis;
cvar_t	*j_forward_axis;
cvar_t	*j_side_axis;
cvar_t	*j_up_axis;

cvar_t	*cl_activeAction;


cvar_t	*cg_gametype;
cvar_t	*cl_inGameVideo;
cvar_t	*cin_keepAspect;
cvar_t	*android_aspect;     

cvar_t	*cl_trn;



cvar_t	*cl_r_fullscreen;

cvar_t	*name;
cvar_t	*cl_rate;

// Retail CL_Init cvars (pool 0x25C828) the port had never registered.  com_blood
cvar_t	*mp_savingDemo;
cvar_t	*cl_systemsetup;
cvar_t	*cl_updateInfoString;
cvar_t	*cl_timeNudgeSP;
cvar_t	*cl_avidemo;
cvar_t	*cl_alternateMouseMove;
cvar_t	*m_up;
cvar_t	*dm_morph_c1;
cvar_t	*cl_disableloadinginbackground;
cvar_t	*cl_chbottom;
cvar_t	*cl_chtop;
cvar_t	*cl_chleft;
cvar_t	*cl_chright;
cvar_t	*userFov;

clientActive_t		cl;
clientConnection_t	clc;
clientStatic_t		cls;
clientGameExport_t	*cge;

char				cl_reconnectArgs[MAX_OSPATH];

// Retail off_22BC14 (@0x0022BC14) -- "the list needs a refresh" latch, set by
static int s_serverListNeedsRefresh;

refexport_t	re;
#ifdef USE_RENDERER_DLOPEN
static void	*rendererLib = NULL;
#endif

qboolean camera_reset;
vec3_t camera_offset;

ping_t	cl_pinglist[MAX_PINGREQUESTS];

typedef struct serverStatus_s
{
	char string[BIG_INFO_STRING];
	netadr_t address;
	int time, startTime;
	qboolean pending;
	qboolean print;
	qboolean retrieved;
} serverStatus_t;

serverStatus_t cl_serverStatusList[MAX_SERVERSTATUSREQUESTS];
int serverStatusCount;

#if defined __USEA3D && defined __A3D_GEOM
	void hA3Dg_ExportRenderGeom (refexport_t *incoming_re);
#endif

extern void SV_BotFrame( int time );
void CL_CheckForResend( void );
void CL_ServerStatus_f(void);
void CL_ServerStatusResponse( netadr_t from, msg_t *msg );

void UI_UpdateServerList( void );
void CL_RefreshServerList_f( void );
void CL_SetPingSource_f( void );
extern cvar_t *cl_updatesource;
extern cvar_t *cl_showemptyservers;
extern cvar_t *cl_showfullservers;

static qboolean cl_bCLSystemStarted = qfalse;

static char s_defaultSoundTrack[128] = "music/intro";

void setDefaultSoundTrack_f( void ) {
	strcpy( s_defaultSoundTrack, Cmd_Argv( 1 ) );
}

qboolean CL_UseLargeLightmap(const char* mapName) {
	return qtrue;
}

void CL_CDDialog( void ) {
	Cbuf_ExecuteText( EXEC_NOW, va( "showmenu insertcd\n" ) );
}

float *get_camera_offset( qboolean *lookactive, qboolean *resetview )
{
	*resetview = camera_reset;
	camera_reset = qfalse;
	*lookactive = in_cameralook.active;
	return camera_offset;
}



void CL_AddReliableCommand( const char *cmd, qboolean isDisconnectCmd ) {
	int		index;

	if ( clc.reliableSequence - clc.reliableAcknowledge > MAX_RELIABLE_COMMANDS ) {
		Com_Error( ERR_DROP, "Client command overflow" );
	}
	clc.reliableSequence++;
	index = clc.reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 );
	Q_strncpyz( clc.reliableCommands[ index ], cmd, sizeof( clc.reliableCommands[ index ] ) );
}

void CL_ChangeReliableCommand( void ) {
	size_t r, index, l;

	r = clc.reliableSequence - (random() * 5);
	(void)r;
	index = clc.reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 );
	l = strlen(clc.reliableCommands[ index ]);
	if ( l >= sizeof( clc.reliableCommands[ index ] ) - 1 ) {
		l = sizeof( clc.reliableCommands[ index ] ) - 2;
	}
	clc.reliableCommands[ index ][ l ] = '\n';
	clc.reliableCommands[ index ][ l+1 ] = '\0';
}


void CL_WriteDemoMessage ( msg_t *msg, int headerBytes ) {
	size_t		len, swlen;

	len = clc.serverMessageSequence;
	swlen = LittleLong( len );
	FS_Write (&swlen, 4, clc.demofile);

	len = msg->cursize - headerBytes;
	swlen = LittleLong(len);
	FS_Write (&swlen, 4, clc.demofile);
	FS_Write ( msg->data + headerBytes, len, clc.demofile );
}


void CL_StopRecord_f( void ) {
	int		len;

	if ( !clc.demorecording ) {
		Com_Printf ("Not recording a demo.\n");
		return;
	}

	len = -1;
	FS_Write (&len, 4, clc.demofile);
	FS_Write (&len, 4, clc.demofile);
	FS_FCloseFile (clc.demofile);
	clc.demofile = 0;
	clc.demorecording = qfalse;
	Com_Printf ("Stopped demo.\n");
	Cvar_Set( "mp_savingDemo", "0" );	 
}

void CL_DemoFilename( int number, char *fileName ) {
	int		a,b,c,d;

	if(number < 0 || number > 9999)
		number = 9999;

	a = number / 1000;
	number -= a*1000;
	b = number / 100;
	number -= b*100;
	c = number / 10;
	number -= c*10;
	d = number;

	Com_sprintf( fileName, MAX_OSPATH, "demo%i%i%i%i"
		, a, b, c, d );
}

static char		demoName[MAX_QPATH];	
void CL_Record_f( void ) {
	char		demoFileName[MAX_OSPATH];
	byte		bufData[MAX_MSGLEN];
	msg_t	buf;
	int			i;
	size_t			len;
	entityState_t	*ent;
	entityState_t	nullstate;
	char		*s;

	if ( Cmd_Argc() > 2 ) {
		Com_Printf ("record <demoname>\n");
		return;
	}

	if ( clc.demorecording ) {
		Com_Printf ("Already recording.\n");
		return;
	}

	if ( cls.state != CA_ACTIVE ) {
		Com_Printf ("You must be in a level to record.\n");
		return;
	}

	if ( Cvar_VariableValue( "g_syncronousClients" ) == 0.0f ) {
		Com_Printf( "The server must have 'g_syncronousClients 1' set for demos\n" );
		return;
	}

	if ( Cvar_VariableValue( "com_passthrough" ) != 0.0f ) {
		Com_Printf( "The server must have 'com_passthrough 0' set for demos\n" );
		return;
	}

	if ( Cmd_Argc() == 2 ) {
		s = Cmd_Argv(1);
		Q_strncpyz( demoName, s, sizeof( demoName ) );
		Com_sprintf (demoFileName, sizeof(demoFileName), "demos/%s.dm3", demoName );
	} else {
		int		number;

		for ( number = 0 ; number <= 9999 ; number++ ) {
			CL_DemoFilename( number, demoName );
			Com_sprintf (demoFileName, sizeof(demoFileName), "demos/%s.dm3", demoName );

			if (!FS_FileExists_HomeData(demoFileName))
				break;	
		}
	}


	Com_Printf ("recording to %s.\n", demoFileName);
	clc.demofile = FS_FOpenFileWrite_HomeData( demoFileName );
	if ( !clc.demofile ) {
		Com_Printf ("ERROR: couldn't open.\n");
		return;
	}
	clc.demorecording = qtrue;


	Q_strncpyz( clc.demoName, demoName, sizeof( clc.demoName ) );

	clc.demowaiting = qtrue;

	MSG_Init (&buf, bufData, sizeof(bufData));
	MSG_Bitstream(&buf);

	MSG_WriteLong( &buf, clc.reliableSequence );

	MSG_WriteByte ( &buf, svc_gamestate );
	MSG_WriteLong ( &buf, clc.serverCommandSequence );

	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		if ( !cl.gameState.stringOffsets[i] ) {
			continue;
		}
		s = cl.gameState.stringData + cl.gameState.stringOffsets[i];
		MSG_WriteByte ( &buf, svc_configstring );
		MSG_WriteShort ( &buf, i );
		MSG_WriteBigString ( &buf, s );
	}

	MSG_GetNullEntityState(&nullstate);
	for ( i = 0; i < MAX_GENTITIES ; i++ ) {
		ent = &cl.entityBaselines[i];
		if ( !ent->number ) {
			continue;
		}
		VectorCopy( ent->origin, ent->netorigin );
		VectorCopy( ent->angles, ent->netangles );
		MSG_WriteByte ( &buf, svc_baseline );
		MSG_WriteDeltaEntity ( &buf, &nullstate, ent, qtrue );
	}

	MSG_WriteByte( &buf, svc_EOF );


	MSG_WriteLong(&buf, clc.clientNum);
    MSG_WriteLong(&buf, clc.checksumFeed);

	MSG_WriteByte( &buf, svc_EOF );

	len = LittleLong( clc.serverMessageSequence - 1 );
	FS_Write (&len, 4, clc.demofile);

	len = LittleLong (buf.cursize);
	FS_Write (&len, 4, clc.demofile);
	FS_Write (buf.data, buf.cursize, clc.demofile);

}


// min/avg/max/stddev tail of the timedemo print, which is not retail's

void CL_DemoCompleted( void )
{
	if( cl_timedemo && cl_timedemo->integer )
	{
		int	time;

		time = Sys_Milliseconds() - clc.timeDemoStart;
		if( time > 0 )
		{
			Com_Printf( "%i frames, %3.1f seconds: %3.1f fps\n",
					clc.timeDemoFrames,
					time/1000.0,
					clc.timeDemoFrames*1000.0 / time );
		}
	}

	CL_Disconnect(qtrue);	 
	CL_NextDemo();
}

void CL_ReadDemoMessage( void ) {
	size_t		r;
	msg_t		buf;
	byte		bufData[ MAX_MSGLEN ];
	int			s;

	if ( !clc.demofile ) {
		CL_DemoCompleted ();
		return;
	}

	r = FS_Read( &s, 4, clc.demofile);
	if ( r != 4 ) {
		CL_DemoCompleted ();
		return;
	}
	clc.serverMessageSequence = LittleLong( s );

	MSG_Init( &buf, bufData, sizeof( bufData ) );

	r = FS_Read (&buf.cursize, 4, clc.demofile);
	if ( r != 4 ) {
		CL_DemoCompleted ();
		return;
	}
	buf.cursize = LittleLong( buf.cursize );
	if ( buf.cursize == (uint32_t)-1) {
		CL_DemoCompleted ();
		return;
	}
	if ( buf.cursize > buf.maxsize ) {
		Com_Error (ERR_DROP, "CL_ReadDemoMessage: demoMsglen > MAX_MSGLEN");
	}
	r = FS_Read( buf.data, buf.cursize, clc.demofile );
	if ( r != buf.cursize ) {
		Com_Printf( "Demo file was truncated.\n");
		CL_DemoCompleted ();
		return;
	}

	clc.lastPacketTime = cls.realtime;
	buf.readcount = 0;
	CL_ParseServerMessage( &buf );
}

void CL_PlayDemo_f( void ) {
	char		demoFileName[MAX_OSPATH];
	char		*arg;

	if (Cmd_Argc() != 2) {
		Com_Printf ("playdemo <demoname>\n");
		return;
	}

	Cvar_Set( "sv_killserver", "1" );	 

	CL_Disconnect(qfalse);	 
	CL_FlushMemory();		 

	arg = Cmd_Argv(1);

	if (strstr(arg, ".dm3")) {
		Com_sprintf (demoFileName, sizeof(demoFileName), "demos/%s", arg);
	} else {
		Com_sprintf (demoFileName, sizeof(demoFileName), "demos/%s.dm3", arg);
	}

	FS_FOpenFileRead( demoFileName, &clc.demofile, qtrue, qfalse );	 

	if (!clc.demofile) {
		Com_Error( ERR_DROP, "couldn't open %s", demoFileName);
		return;
	}
	Q_strncpyz( clc.demoName, Cmd_Argv(1), sizeof( clc.demoName ) );

    UI_CloseConsole();

	cls.state = CA_CONNECTED;
	clc.demoplaying = qtrue;
	Q_strncpyz( cls.servername, Cmd_Argv(1), sizeof( cls.servername ) );

	while ( cls.state >= CA_CONNECTED && cls.state < CA_PRIMED ) {
		CL_ReadDemoMessage();
	}
	clc.firstDemoFrameSkipped = qfalse;
}


void CL_StartDemoLoop( void ) {
	Cbuf_AddText ("d1\n");
	Key_SetCatcher( 0 );
}

void CL_NextDemo( void ) {
	char	v[MAX_STRING_CHARS];

	Q_strncpyz( v, Cvar_VariableString ("nextdemo"), sizeof(v) );
	v[MAX_STRING_CHARS-1] = 0;
	Com_DPrintf("CL_NextDemo: %s\n", v );
	if (!v[0]) {
		return;
	}

	Cvar_Set ("nextdemo","");
	Cbuf_AddText (v);
	Cbuf_AddText ("\n");
	Cbuf_Execute(0);
}



void CL_ShutdownAll(qboolean shutdownRef) {

#ifdef USE_CURL
#endif
#if defined(NO_MODERN_DMA) && NO_MODERN_DMA
	S_DisableSounds();
#endif
	CL_ShutdownCGame();
	CL_ShutdownUI();

	TIKI_FreeAll();

	if(shutdownRef)
		CL_ShutdownRef();
	else if(re.Shutdown)
		re.Shutdown(qfalse);		

	cls.uiStarted = qfalse;
	cls.cgameStarted = qfalse;
	cls.rendererRegistered = qfalse;
}

void CL_FlushMemory( void ) {

	CL_ShutdownCGame();
	S_StopAllSounds2( qtrue );
	UI_ClearState();
	cls.rendererRegistered = qfalse;
}

void CL_MapLoading( qboolean flush, const char *pszMapName, const char *pszCinematic, int stage ) {
	if ( !com_cl_running->integer ) {
		return;
	}

	UI_Disconnected();
	UI_ServerLoading( pszMapName, stage );
	UI_CloseConsole();
	UI_ForceMenuOff( true );

	if ( !flush ) {
		return;
	}

	SMP_LOAD_BEGIN( clmap );

	if ( cls.state < CA_CONNECTED || Q_stricmp( cls.servername, "localhost" ) ) {
		Cvar_Set( "nextmap", "" );
		CL_Disconnect( qfalse );
		Q_strncpyz( cls.servername, "localhost", sizeof( cls.servername ) );
		cls.state = CA_CHALLENGING;		
		clc.connectTime = -3000;		 
		NET_StringToAdr( cls.servername, &clc.serverAddress);

		CL_CheckForResend();
	}
	else
	{
		Com_Memset( cls.updateInfoString, 0, sizeof( cls.updateInfoString ) );
		Com_Memset( clc.serverMessage, 0, 256 );
		Com_Memset( &cl.gameState, 0, sizeof( cl.gameState ) );
		cls.state = CA_CONNECTED;		
		clc.lastPacketSentTime = -9999;
	}

	CL_ShutdownCGame();
	S_StopAllSounds( qtrue );
	UI_ClearState();
	cls.rendererRegistered = qfalse;
	CL_StartHunkUsers( qfalse );

	if ( pszCinematic ) {
		StartCinematic( pszCinematic );
	}
	else
	{
		ShutdownCinematicSystem();
		StartLoadingScreen();
	}

	SMP_LOAD_END( clmap );
}

void CL_MapLoadingClientOnly( const char *pszMapName ) {
	if ( com_sv_running->integer ) {
		return;
	}

	if ( !com_cl_running->integer ) {
		return;
	}

	UI_ServerLoading( pszMapName, 0 );
	UI_CloseConsole();
	UI_ForceMenuOff( true );
	StartLoadingScreen();
}

void CL_CleanupUserInfo()
{
    cvar_t *cv;
    cvar_t *next;

    for(cv = Cvar_Next(NULL), next = cv->next; cv; cv = next, next = Cvar_Next(cv)) {
        if ((cv->flags & CVAR_USER_CREATED) && (cv->flags & CVAR_USERINFO)) {
            Cvar_Unset(cv);
        }
    }
}

void CL_ClearState( void )
{
	CL_ShutdownCGame();

	S_StopAllSounds( qtrue );

	UI_ClearState();

	Com_Memset( &cl, 0, sizeof( clientActive_t ) );
}

void CL_Disconnect( qboolean showMenu ) {
	connstate_t state = cls.state;

	if ( !com_cl_running || !com_cl_running->integer ) {
		return;
	}

	if ( clc.demorecording ) {
		if ( clc.demofile ) {
			int endFrame = -1;

			FS_Write( &endFrame, 4, clc.demofile );
			FS_Write( &endFrame, 4, clc.demofile );
			FS_FCloseFile( clc.demofile );
			clc.demofile = 0;
		}
		clc.demorecording = qfalse;
		Com_Printf( "Stopped demo.\n" );
		Cvar_Set( "mp_savingDemo", "0" );
	}

	if ( clc.demofile ) {
		FS_FCloseFile( clc.demofile );
		clc.demofile = 0;
	}

	if ( state >= CA_CONNECTED ) {
		CL_AddReliableCommand( "disconnect", qtrue );
		CL_WritePacket();
		CL_WritePacket();
		CL_WritePacket();
	}

	CL_ShutdownCGame();
	S_StopAllSounds( qtrue );
	UI_ClearState();

	Com_Memset( &cl, 0, sizeof( clientActive_t ) );
	Com_Memset( &clc, 0, sizeof( clientConnection_t ) );

	if ( state >= CA_CONNECTED && showMenu ) {
		UI_ForceMenuOff( true );
		UI_ForceMenu( "main" );
		Cvar_Set2( "g_gametype", "0", qtrue );
	}

	cls.state = CA_DISCONNECTED;
	UI_Disconnected();
	Com_UnPause_f();

	// SCR_UpdateScreen() returns immediately and the RENDERER NEVER RUNS AGAIN: the
	SCR_ResetUpdateGuard();
}

void CL_AbnormalDisconnect( const char *message ) {
    // buffer, not Q_strncpyz'd -- retail's copy is not guaranteed terminated
    char reason[256];

    strncpy( reason, message, sizeof( reason ) );	 
    CL_Disconnect( qtrue );							 
    UI_StopLoading();								 
    UI_ForceMenuOff( qtrue );						 
    UI_ForceMenu( "main" );							 
    UI_ShowDisconnectMessage( reason );				 

	if (developer->integer) {
		UI_OpenConsole();							 
	}
}

void CL_ForwardCommandToServer( const char *string ) {
	char	local[MAX_CLIENT_STRING_CHARS];
	char	*cmd;

	cmd = Cmd_Argv(0);

	if ( cmd[0] == '-' ) {
		return;
	}

	if ( clc.demoplaying || cls.state != CA_ACTIVE || cmd[0] == '+' ) {
		Com_WPrintf ("Unknown server command \"%s\"\n", cmd);
		return;
	}

	if ( Cmd_Argc() > 1 ) {
		Com_sprintf( local, sizeof( local ), "%s %s", cmd, Cmd_Args() );
		CL_AddReliableCommand( local, qfalse );
	} else {
		CL_AddReliableCommand( cmd, qfalse );
	}
}

void CL_RequestMotd( void ) {
	char		info[MAX_INFO_STRING];

	if ( !cl_motd->integer ) {
		return;
	}
	Com_Printf( "Resolving %s\n", UPDATE_SERVER_NAME );
	if ( !NET_StringToAdr( UPDATE_SERVER_NAME, &cls.updateServer) ) {
		Com_Printf( "Couldn't resolve address\n" );
		return;
	}
	cls.updateServer.port = BigShort( PORT_UPDATE );
	Com_Printf( "%s resolved to port %i\n", UPDATE_SERVER_NAME,
		BigShort( cls.updateServer.port ) );

	info[0] = 0;
  // NOTE TTimo xoring against Com_Milliseconds, otherwise we may not have a true randomization
	Com_sprintf( cls.updateChallenge, sizeof( cls.updateChallenge ), "%i", ((rand() << 16) ^ rand()) ^ Com_Milliseconds());

	Info_SetValueForKey( info, "challenge", cls.updateChallenge );
	Info_SetValueForKey( info, "renderer", cls.glconfig.renderer_string );
	Info_SetValueForKey( info, "version", com_version->string );

	CL_NET_OutOfBandPrint( cls.updateServer, "getmotd \"%s\"\n", info );
}

void CL_RequestAuthorization( void ) {
    char nums[64];
    int  i, j;
    int  l;

    if (!cls.authorizeServer.port) {
        Com_Printf("Resolving %s\n", "ef2auth.activision.com");
        if (!NET_StringToAdr("ef2auth.activision.com", &cls.authorizeServer)) {
            Com_Printf("Couldn't resolve address\n");
            return;
        }

        cls.authorizeServer.port = BigShort(PORT_AUTHORIZE);
        Com_Printf(
            "%s resolved to port %i\n",
            "ef2auth.activision.com",
            BigShort(cls.authorizeServer.port));
    }

    if (cls.authorizeServer.type == NA_BAD) {
        return;
    }

    if (Cvar_VariableValue("fs_restrict") == 0.0f) {
        j = 0;
        l = strlen(cl_cdkey);
        if (l > 32) {
            l = 32;
        }
        for (i = 0; i < l; i++) {
            if ((cl_cdkey[i] >= '0' && cl_cdkey[i] <= '9') || (cl_cdkey[i] >= 'A' && cl_cdkey[i] <= 'Z')) {
                nums[j] = cl_cdkey[i];
                j++;
            }
        }
        nums[j] = 0;
    } else {
        Q_strncpyz(nums, "demo", sizeof(nums));
    }

    CL_NET_OutOfBandPrint(cls.authorizeServer, "getKeyAuthorize 999 %s", nums);
}


void CL_ForwardToServer_f( void ) {
	if ( cls.state != CA_ACTIVE ) {
		Com_Printf( "Not connected to a server.\n" );
		return;
	}

	if ( Cmd_Argc() > 1 ) {
		CL_AddReliableCommand( Cmd_Args(), qfalse );
	}
}

void CL_Setenv_f( void ) {
	int argc = Cmd_Argc();

	if ( argc > 2 ) {
		char buffer[1024];
		int i;

		Q_strncpyz( buffer, Cmd_Argv(1), sizeof( buffer ) );
		Q_strcat( buffer, sizeof( buffer ), "=" );

		for ( i = 2; i < argc; i++ ) {
			Q_strcat( buffer, sizeof( buffer ), Cmd_Argv( i ) );
			Q_strcat( buffer, sizeof( buffer ), " " );
		}

		// retail @0x0017FE70 builds the string and never hands it to putenv(3);
	} else if ( argc == 2 ) {
		char *env = getenv( Cmd_Argv(1) );

		if ( env ) {
			Com_Printf( "%s=%s\n", Cmd_Argv(1), env );
		} else {
			Com_Printf( "%s undefined\n", Cmd_Argv(1));
		}
	}
}


void CL_Disconnect_f( void ) {
	if ( cls.state != CA_DISCONNECTED ) {
		Com_Error( ERR_DISCONNECT, "" );
	}
}


void CL_Reconnect_f( void ) {
	if ( !strlen( cls.servername ) || !strcmp( cls.servername, "localhost" ) ) {
		Com_Printf( "Can't reconnect to localhost.\n" );
		return;
	}

	Cbuf_AddText( va( "connect %s\n", cls.servername ) );
}

void CL_Connect_f(void) {
	char	server[256];

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "usage: connect [server]\n" );
		return;
	}

	clc.serverMessage[ 0 ] = 0;
	s_serverListNeedsRefresh = 0;

	Q_strncpyz( server, Cmd_Argv( 1 ), sizeof( server ) );

	if ( com_sv_running->integer && !strcmp( server, "localhost" ) ) {
		SV_Shutdown( "$$ServerQuit$$\n" );
	}

	Cvar_Set( "sv_killserver", "1" );
	SV_Frame( 0 );

	CL_Disconnect( qfalse );
	UI_CloseConsole();
	UI_ForceMenuOff( qtrue );
	CL_ShutdownCGame();
	S_StopAllSounds( qtrue );
	UI_ClearState();
	cls.rendererRegistered = qfalse;

	Q_strncpyz( cls.servername, server, sizeof( cls.servername ) );

	if ( NET_StringToAdr( cls.servername, &clc.serverAddress ) ) {
		if ( !clc.serverAddress.port ) {
			clc.serverAddress.port = BigShort( PORT_SERVER );
		}

		Com_Printf( "%s resolved to port %i\n", cls.servername,
			BigShort( clc.serverAddress.port ) );

		if ( clc.serverAddress.type == NA_LOOPBACK ) {
			cls.state = CA_CHALLENGING;
		} else {
			// A remote server is the only thing that needs the real UDP socket.
			// Without it there is no challenge to wait for, so stop here rather
			// than sit in CA_CONNECTING retransmitting into a closed socket.
			if ( !NET_RemoteAvailable() ) {
				Com_Printf( "Connect to %s failed: network unavailable\n", cls.servername );
				CL_Disconnect( qfalse );
				UI_StopLoading();
				UI_ForceMenuOff( qtrue );
				UI_ForceMenu( "main" );
				UI_ShowMenu( "okdialog", qfalse );
				// A literal, not a $$token$$: the retail string table carries no
				// row for this, and an unresolved token would print as itself.
				Cbuf_ExecuteText( EXEC_NOW, "globalwidgetcommand OkDialogTitle title "
					"\"Network unavailable -- this install has no network permission\"\n" );
				return;
			}
			cls.state = CA_CONNECTING;
		}

		clc.connectTime = -99999;	
		clc.connectPacketCount = 0;

		Cvar_Set( "cl_currentServerAddress", server );
	} else {
		Com_Printf( "Bad server address\n" );
		cls.state = CA_DISCONNECTED;
	}
}

#define MAX_RCON_MESSAGE 1024

void CL_Rcon_f( void ) {
	char	message[MAX_RCON_MESSAGE];
	netadr_t	to;
	int		i;

	if ( !rcon_client_password->string ) {
		Com_Printf ("You must set 'rconpassword' before\n"
					"issuing an rcon command.\n");
		return;
	}

	message[0] = -1;
	message[1] = -1;
	message[2] = -1;
	message[3] = -1;
	message[4] = 0;	 

	Q_strcat (message, MAX_RCON_MESSAGE, "rcon ");

	Q_strcat (message, MAX_RCON_MESSAGE, rcon_client_password->string);
	Q_strcat (message, MAX_RCON_MESSAGE, " ");

	for ( i = 1 ; i < Cmd_Argc() ; i++ ) {
		Q_strcat (message, MAX_RCON_MESSAGE, Cmd_Argv( i ));
		Q_strcat (message, MAX_RCON_MESSAGE, " ");
	}

	if ( cls.state >= CA_CONNECTED ) {
		to = clc.netchan.remoteAddress;
	} else {
		if (!strlen(rconAddress->string)) {
			Com_Printf ("You must either be connected,\n"
						"or set the 'rconAddress' cvar\n"
						"to issue rcon commands\n");

			return;
		}
		NET_StringToAdr (rconAddress->string, &to);
		if (to.port == 0) {
			to.port = BigShort (PORT_SERVER);
		}
	}

	NET_SendPacket (NS_CLIENT, strlen(message) + 1, message, to);
}

void CL_SendPureChecksums( void ) {
	char cMsg[MAX_CLIENT_STRING_CHARS];

	Com_sprintf( cMsg, sizeof( cMsg ), "Yf " );
	Q_strcat( cMsg, sizeof( cMsg ), FS_ReferencedPakPureChecksums() );

	CL_AddReliableCommand( cMsg, qfalse );
}

void CL_ResetPureClientAtServer( void ) {
	CL_AddReliableCommand( va("vdr"), qfalse );
}

qboolean CL_Allowed_Vid_Restart() {
	return cls.state == CA_DISCONNECTED || cls.state == CA_ACTIVE;
}

void CL_Vid_Restart_f( void ) {
    soundsystemsavegame_t save;
    char tm_filename[MAX_RES_NAME];
    int tm_loopcount;
    int tm_offset;
    char current_soundtrack[128];

	if (!CL_Allowed_Vid_Restart()) {
		return;
	}

	Com_Printf("-------- CL_Vid_Restart_f --------\n");
	cls.vid_restart = qtrue;

    Q_strncpyz(tm_filename, S_GetMusicFilename(), sizeof(tm_filename));
    tm_loopcount = S_GetMusicLoopCount();
    tm_offset = S_GetMusicOffset();

	SV_ClearSvsTimeFixups();

    S_SaveData(&save);
    Q_strncpyz(current_soundtrack, S_CurrentSoundtrack(), sizeof(current_soundtrack));

	S_BeginRegistration();

    CL_ShutdownRef();

    cls.rendererRegistered = qfalse;

	CL_ShutdownCGame();
	CL_InitRef();
	Alias_Clear();

	Com_Unpause();

	CL_StartHunkUsers(qfalse);

	UI_ResolutionChange();

#if !defined(NO_MODERN_DMA) || !NO_MODERN_DMA
    s_bSoundPaused = true;
    S_LoadData(&save);
#else
    S_Init();
#endif

	SV_FinishSvsTimeFixups();

    S_ReLoad(&save);

    if (tm_filename[0]) {
        S_TriggeredMusic_SetupHandle(tm_filename, tm_loopcount, tm_offset, 0);
    }

    MUSIC_NewSoundtrack(current_soundtrack);

	if (cls.state > CA_CONNECTED && cls.state != CA_CINEMATIC)
	{
		CL_SendPureChecksums();
	}

	cls.vid_restart = qfalse;
	Com_Printf("----- finished CL_Vid_Restart_f ----\n");
}

void CL_Snd_Restart_f( void ) {
#if defined(NO_MODERN_DMA) && NO_MODERN_DMA
	S_Shutdown();
	S_Init();
    CL_Vid_Restart_f();
#else
	qboolean full;
	soundsystemsavegame_t save;
    char tm_filename[MAX_RES_NAME];
	int tm_loopcount;
	int tm_offset;
	char current_soundtrack[128];

	full = S_NeedFullRestart();

    Q_strncpyz(tm_filename, S_GetMusicFilename(), sizeof(tm_filename));
    tm_loopcount = S_GetMusicLoopCount();
    tm_offset = S_GetMusicOffset();

	SV_ClearSvsTimeFixups();

	S_SaveData(&save);
	Q_strncpyz(current_soundtrack, S_CurrentSoundtrack(), sizeof(current_soundtrack));

	S_Shutdown(qfalse);
	S_Init(qfalse);

	s_bSoundPaused = true;

	S_LoadData(&save);

	SV_FinishSvsTimeFixups();
	S_ReLoad(&save);

	if (tm_filename[0]) {
		S_TriggeredMusic_SetupHandle(tm_filename, tm_loopcount, tm_offset, 0);
	}

	MUSIC_NewSoundtrack(current_soundtrack);

	if (full) {
		CL_Vid_Restart_f();
	}
#endif
}


const char *CL_ConfigString( int index ) {
	if( index > MAX_CONFIGSTRINGS ) {
		Com_Error( ERR_DROP, "CL_ConfigString: bad index: %i", index );
	}
	return &cl.gameState.stringData[ cl.gameState.stringOffsets[ index ] ];
}

void CL_Configstrings_f( void ) {
	int		i;
	int		ofs;

	if ( cls.state != CA_ACTIVE ) {
		Com_Printf( "Not connected to a server.\n");
		return;
	}

	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		ofs = cl.gameState.stringOffsets[ i ];
		if ( !ofs ) {
			continue;
		}
		Com_Printf( "%4i: %s\n", i, cl.gameState.stringData + ofs );
	}
}

void CL_Clientinfo_f( void ) {
	Com_Printf( "--------- Client Information ---------\n" );
	Com_Printf( "state: %i\n", cls.state );
	Com_Printf( "Server: %s\n", cls.servername );
	Com_Printf ("User info settings:\n");
	Info_Print( Cvar_InfoString( CVAR_USERINFO ) );
	Com_Printf( "--------------------------------------\n" );
}




void CL_CheckForResend( void ) {
	int		port, i;
	char	info[MAX_INFO_STRING];
	char	data[MAX_INFO_STRING];

	if ( clc.demoplaying ) {
		return;
	}

	if ( cls.state != CA_CONNECTING && cls.state != CA_CHALLENGING ) {
		return;
	}

	if ( cls.realtime - clc.connectTime < RETRANSMIT_TIMEOUT ) {
		return;
	}

	clc.connectTime = cls.realtime;	
	clc.connectPacketCount++;


	switch ( cls.state ) {
	case CA_CONNECTING:
		if (!Sys_IsLANAddress( clc.serverAddress ) ) {
			CL_RequestAuthorization();
		}
		CL_NET_OutOfBandPrint(clc.serverAddress, "getchallenge");
		break;
	case CA_AUTHORIZING:
		gcd_compute_response(cl_cdkey, Cmd_Argv(1), cls.gcdResponse, CDResponseMethod_REAUTH);
		CL_NET_OutOfBandPrint(clc.serverAddress, "authorizeThis %s", cls.gcdResponse);
		break;
	case CA_CHALLENGING:

		port = Cvar_VariableIntegerValue ("net_qport");

	    char szSanitizedName[MAX_NAME_LENGTH];
		if (Com_SanitizeName(name->string, szSanitizedName, sizeof(szSanitizedName))) {
			Cvar_Set("name", szSanitizedName);
		}

		Q_strncpyz( info, Cvar_InfoString( CVAR_USERINFO ), sizeof( info ) );

		Info_SetValueForKey(info, "protocol", va("%i", PROTOCOL_VERSION));
		Info_SetValueForKey(info, "qport", va("%i", port));
		Info_SetValueForKey(info, "challenge", va("%i", clc.challenge));

		Q_strncpyz(data, "connect ", sizeof(data));
    data[8] = '"';

		for(i=0;i<(int)strlen(info);i++) {
			data[9+i] = info[i];	
		}
    data[9+i] = '"';
		data[10+i] = 0;

		NET_OutOfBandData( NS_CLIENT, clc.serverAddress, (byte *) &data[0], i+10 );
		cvar_modifiedFlags &= ~CVAR_USERINFO;
		break;

	default:
		Com_Error( ERR_FATAL, "CL_CHeckForResend: bad cls.state" );
	}
}

void CL_DisconnectPacket( netadr_t from ) {
	if ( cls.state != CA_ACTIVE ) {
		return;
	}

	if ( !NET_CompareAdr( from, clc.serverAddress ) ) {
		return;
	}

	if ( cls.realtime - clc.lastPacketTime < 3000 ) {
		return;
	}

	Com_Printf( "Server disconnected for unknown reason\n" );
	CL_Disconnect( qtrue );
}


void CL_MotdPacket( netadr_t from ) {
	char	*challenge;
	char	*info;

	if ( !NET_CompareAdr( from, cls.updateServer ) ) {
		return;
	}

	info = Cmd_Argv(1);

	challenge = Info_ValueForKey( info, "challenge" );
	if ( strcmp( challenge, cls.updateChallenge ) ) {
		return;
	}

	challenge = Info_ValueForKey( info, "motd" );

	Q_strncpyz( cls.updateInfoString, info, sizeof( cls.updateInfoString ) );
}

void CL_InitServerInfo( serverInfo_t *server, netadr_t *address ) {
	server->adr = *address;
	server->hostName[0] = '\0';
	server->gameType = 0;
	server->clients = 0;
	server->maxClients = 0;
	server->pointLimit = 0;
	server->timeLimit = 0;
	server->ping = -1;
	server->netType = 0;
	server->visible = qfalse;
}

#define MAX_SERVERSPERPACKET	256


static int CL_ServersResponseNibble( int c ) {
	if ( c >= '0' && c <= '9' ) {
		return c - '0';
	}
	if ( c >= 'a' && c <= 'f' ) {
		return c - 'W';
	}
	if ( c >= 'A' && c <= 'F' ) {
		return c - '@';
	}
	return 0;
}

void CL_ServersResponsePacket( const netadr_t* from, msg_t *msg ) {
	byte			ips[MAX_SERVERSPERPACKET][4];
	unsigned short	ports[MAX_SERVERSPERPACKET];
	int				numservers;
	int				i;
	byte*			buffptr;
	byte*			buffend;
	int				c;

	Com_Printf("CL_ServersResponsePacket\n");

	if (cls.numglobalservers == -1) {
		cls.numglobalservers = 0;
	}

	numservers = 0;
	buffptr    = msg->data;
	buffend    = buffptr + msg->cursize;

	do {
		int		hi, lo, port;

		if ( buffptr + 1 >= buffend ) {
			break;
		}

		do {
			c = *buffptr++;
		} while ( c != '\\' && buffptr < buffend );

		hi = CL_ServersResponseNibble( buffptr[0] );
		lo = CL_ServersResponseNibble( buffptr[1] );
		ips[numservers][0] = ( hi << 4 ) + lo;

		hi = CL_ServersResponseNibble( buffptr[2] );
		lo = CL_ServersResponseNibble( buffptr[3] );
		ips[numservers][1] = ( hi << 4 ) + lo;

		hi = CL_ServersResponseNibble( buffptr[4] );
		lo = CL_ServersResponseNibble( buffptr[5] );
		ips[numservers][2] = ( hi << 4 ) + lo;

		hi = CL_ServersResponseNibble( buffptr[6] );
		lo = CL_ServersResponseNibble( buffptr[7] );
		ips[numservers][3] = ( hi << 4 ) + lo;

		hi = ( CL_ServersResponseNibble( buffptr[8] ) << 12 )
			+ ( CL_ServersResponseNibble( buffptr[9] ) << 8 );
		lo = ( CL_ServersResponseNibble( buffptr[10] ) << 4 )
			+ CL_ServersResponseNibble( buffptr[11] );
		port = hi + lo;

		buffptr += 12;

		if ( *buffptr != '\\' ) {
			break;
		}

		Com_DPrintf( "server: %d port: %d\n", numservers, port );

		ports[numservers] = port;
		numservers++;
		if ( numservers >= MAX_SERVERSPERPACKET ) {
			break;
		}

	} while ( buffptr[1] != 'E' || buffptr[2] != 'O' || buffptr[3] != 'T' );

	Com_Printf("%d servers parsed\n", numservers);

	for (i = 0; i < numservers && cls.numglobalservers < MAX_GLOBAL_SERVERS; i++) {
		serverInfo_t *server = &cls.globalServers[cls.numglobalservers];

		server->adr.ip[0] = ips[i][0];
		server->adr.ip[1] = ips[i][1];
		server->adr.ip[2] = ips[i][2];
		server->adr.ip[3] = ips[i][3];
		server->adr.port = BigShort( ports[i] );

		cls.numglobalservers++;
	}
}

void CL_ConnectionlessPacket( netadr_t from, msg_t *msg ) {
	char	*s;
	char	*c;
	
	MSG_BeginReadingOOB( msg );
	MSG_ReadLong( msg );	


	s = MSG_ReadStringLine( msg );

	Cmd_TokenizeString( s );

	c = Cmd_Argv(0);

	Com_DPrintf ("CL packet %s: %s\n", NET_AdrToLogString(from), c);

	if ( !Q_stricmp(c, "challengeResponse") ) {
		if ( cls.state != CA_CONNECTING ) {
			Com_Printf( "Unwanted challenge response received.  Ignored.\n" );
		} else {
			clc.challenge = atoi(Cmd_Argv(1));
			cls.state = CA_CHALLENGING;
			clc.connectPacketCount = 0;
			clc.connectTime = -99999;

			clc.serverAddress = from;
		}
		return;
	}

	if ( !Q_stricmp(c, "connectResponse") ) {
		if ( cls.state >= CA_CONNECTED ) {
			Com_Printf ("Dup connect received.  Ignored.\n");
			return;
		}
		if ( cls.state != CA_CHALLENGING ) {
			Com_Printf ("connectResponse packet while not connecting.  Ignored.\n");
			return;
		}
		if ( !NET_CompareBaseAdr( from, clc.serverAddress ) ) {
			Com_Printf( "connectResponse from a different address.  Ignored.\n" );
			Com_Printf( "%s should have been %s\n", NET_AdrToLogString( from ),
				NET_AdrToLogString( clc.serverAddress ) );
			return;
		}

		Netchan_Setup(NS_CLIENT, &clc.netchan, from, Cvar_VariableValue("net_qport"));

		cls.state = CA_CONNECTED;
		clc.lastPacketSentTime = -9999;		
		return;
	}

	if ( !Q_stricmp(c, "infoResponse") ) {
		CL_ServerInfoPacket( from, msg );
		return;
	}

	if ( !Q_stricmp(c, "statusResponse") ) {
		CL_ServerStatusResponse( from, msg );
		return;
	}

	if (!Q_stricmp(c, "disconnect")) {
		CL_DisconnectPacket( from );
		return;
	}

	// Retail compares this one with strcmp, not Q_stricmp (@0x001817C0).
	if ( !strcmp(c, "echo") ) {
		CL_NET_OutOfBandPrint(from, "%s", Cmd_Argv(1) );
		return;
	}

	if ( !Q_stricmp(c, "keyAuthorize") ) {
		s = MSG_ReadString( msg );
		Q_strncpyz( clc.serverMessage, s, 256 );
		Com_Printf( "%s", s );
		return;
	}

	if ( !Q_stricmp(c, "motd") ) {
		CL_MotdPacket( from );
		return;
	}

	if ( !Q_stricmp(c, "print") ) {
		s = MSG_ReadString( msg );
		Q_strncpyz( clc.serverMessage, s, sizeof( clc.serverMessage ) );
		Com_Printf( "%s", s );
		return;
	}

	if ( !Q_strncmp(c, "getserversResponse", 18) ) {
		CL_ServersResponsePacket( &from, msg );
		return;
	}

	if ( !Q_stricmp(c, "getinfo") ) {
		return;
	}

	Com_DPrintf ("Unknown connectionless packet command.\n");
}


void CL_PacketEvent( netadr_t from, msg_t *msg ) {
	int		headerBytes;

	clc.lastPacketTime = cls.realtime;

	if ( msg->cursize >= 4 && *(int *)msg->data == -1 ) {
		CL_ConnectionlessPacket( from, msg );
		return;
	}

	if ( cls.state < CA_CONNECTED ) {
		return;		
	}

	if ( msg->cursize < 4 ) {
		Com_Printf ("%s: Runt packet\n",NET_AdrToLogString( from ));
		return;
	}

	if ( !NET_CompareAdr( from, clc.netchan.remoteAddress ) ) {
		Com_DPrintf ("%s:sequenced packet without connection\n"
			,NET_AdrToLogString( from ) );
		return;
	}

	if (!CL_Netchan_Process( &clc.netchan, msg) ) {
		return;		
	}

	headerBytes = msg->readcount;

	clc.serverMessageSequence = LittleLong( *(int *)msg->data );

	clc.lastPacketTime = cls.realtime;
	CL_ParseServerMessage( msg );

	if ( clc.demorecording && !clc.demowaiting ) {
		CL_WriteDemoMessage( msg, headerBytes );
	}
}

void CL_CheckTimeout( void ) {
	char buffer[256];

	if ( com_sv_running->integer
		|| cls.state < CA_CONNECTED
		|| cls.state == CA_CINEMATIC
		|| ( float )( cls.realtime - clc.lastPacketTime ) <= 1000.0f * cl_timeout->value ) {
		cl.timeoutcount = 0;
		return;
	}

	if ( ++cl.timeoutcount <= 5 ) {
		return;
	}

	if ( cg_gametype->integer != 1 ) {
		return;
	}

	Com_Printf( "\nServer connection timed out.\n" );
	strncpy( buffer, "$$ServerConnectionTimedOut$$\n", sizeof( buffer ) );
	CL_Disconnect( qtrue );		 
	UI_StopLoading();
	UI_ForceMenuOff( qtrue );
	UI_ForceMenu( "main" );

	UI_ShowMenu( "okdialog", qfalse );
	Cbuf_ExecuteText( EXEC_NOW, va( "globalwidgetcommand OkDialogTitle title \"%s\"", buffer ) );

	if ( developer->integer ) {
		UI_OpenConsole();
	}
}

qboolean CL_CheckPaused(void)
{
	if(paused->integer)
		return qtrue;

	return qfalse;
}


void CL_CheckUserinfo( void ) {
	char szSanitizedName[MAX_NAME_LENGTH];

	if(cls.state < CA_CHALLENGING)
		return;

	if(CL_CheckPaused())
		return;

	if(cvar_modifiedFlags & CVAR_USERINFO)
	{
		if (Com_SanitizeName(name->string, szSanitizedName, sizeof(szSanitizedName))) {
			Cvar_Set("name", szSanitizedName);
		}

		cvar_modifiedFlags &= ~CVAR_USERINFO;
		CL_AddReliableCommand( va("userinfo \"%s\"", Cvar_InfoString( CVAR_USERINFO ) ), qfalse );
	}
}

void CL_SetFrameNumber(int frameNumber) {
	if (!re.SetFrameNumber) {
		return;
	}

	re.SetFrameNumber(frameNumber);
}

void CL_Frame ( int msec ) {
	if ( !com_cl_running->integer ) {
		return;
	}

#ifdef USE_CURL
#endif

	if (cls.state == CA_DISCONNECTED) {
		MUSIC_NewSoundtrack(s_defaultSoundTrack);
	}

	if ( GetBackgroundMode() == 4 ) {
		GetRenderingContext();
		SetBackgroundMode( 0 );
		Com_UnPause_f();
	}

	if ( GetBackgroundMode() && cls.state == CA_ACTIVE ) {
		if ( !com_sv_running->integer ) {
			Com_Pause_f();
		}
		Sys_Sleep( 50 );
	}

	CL_StartHunkUsers( qfalse );

	cls.realFrametime = msec;

	cls.frametime = msec;

	cls.realtime += cls.frametime;

	if ( cl_timegraph->integer ) {
		SCR_DebugGraph ( cls.realFrametime * 0.25 );
	}

	cls.timeScaled = com_timescale->integer != 1;

	CL_CheckUserinfo();

	CL_CheckTimeout();

	CL_SendCmd();

	CL_CheckForResend();

	{
		static int s_loadstallParkMs = -100000;

		if ( cls.state >= CA_CONNECTED && cls.state != CA_ACTIVE
		     && cls.realtime >= s_loadstallParkMs + 1000 ) {
			s_loadstallParkMs = cls.realtime;
			LOADSTALL_PROBE( "park",
			            "t=%d cls=%d clSid=%d svSid=%d msgSeq=%d cmdSeq=%d lastPkt=%d txSeq=%d paused=%d\n",
			            cls.realtime, cls.state, cl.serverId, sv.serverId,
			            clc.serverMessageSequence, clc.serverCommandSequence, clc.lastPacketTime,
			            clc.netchan.outgoingSequence, paused->integer );
		}
	}

	CL_SetCGameTime();

	if( SV_DoSaveGame() )
	{
		if( cl.serverTime < svs.time ) {
			cl.serverTime = svs.time;
		}
	}

	L_ProcessPendingEvents();

	SCR_UpdateScreen();

	{
		int smpAudioStart = Sys_Milliseconds();

		S_Update();

		SMP_BudgetAdd( SMP_ZONE_AUDIO, Sys_Milliseconds() - smpAudioStart );
	}

	ProcessCinematic();

	CL_UpdateMissionObjectives();

	cls.framecount++;
}



static Q_PRINTF_FUNC(2, 3) void QDECL CL_RefPrintf( int print_level, const char *fmt, ...) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	if (print_level == PRINT_ALL) {
		Com_Printf("%s", msg);
	}
	else if (print_level == PRINT_WARNING) {
		Com_WPrintf("%s", msg);
	}
	else if (print_level == PRINT_DEVELOPER) {
		Com_DPrintf("%s", msg);
	}
	else if (print_level == PRINT_DEVELOPER_2 && developer->integer) {
		Com_DPrintf("%s", msg);
	}
}

void CL_RefSetPerformanceCounters( int total_tris, int total_verts, int total_texels, int world_tris, int world_verts, int character_lights )
{
	cls.total_tris = total_tris;
	cls.total_verts = total_verts;
	cls.total_texels = total_texels;
	cls.world_tris = world_tris;
	cls.world_verts = world_verts;
	cls.character_lights = character_lights;
}

void CL_ShutdownRef( void ) {
	if ( re.Shutdown ) {
		re.Shutdown( qtrue );
	}

	Com_Memset( &re, 0, sizeof( re ) );

#ifdef USE_RENDERER_DLOPEN
	if ( rendererLib ) {
		Sys_UnloadLibrary( rendererLib );
		rendererLib = NULL;
	}
#endif
}

void CL_StartHunkUsers( qboolean rendererOnly ) {
	if( !com_cl_running ) {
		return;
	}

	if ( !com_cl_running->integer ) {
		return;
	}

	SMP_LOAD_BEGIN( hunkusr );

	if ( !cls.rendererRegistered ) {
		cls.rendererRegistered = qtrue;
		CL_BeginRegistration();
	}

	if( !cls.cgameStarted ) {
		if( cls.state >= CA_LOADING && cls.state != CA_CINEMATIC ) {
			CL_InitCGame();
			CL_EndRegistration();	 
		}
	}

	if ( !cls.uiStarted ) {
		CL_InitializeUI();
	}

	SMP_LOAD_END( hunkusr );
}

void *CL_RefMalloc(int size) {
	return Z_TagMalloc( size, TAG_RENDERER );
}

void CL_RefFree(void* ptr) {
	Z_Free(ptr);
}

void CL_RefClear( void )
{
	Z_FreeTags( TAG_RENDERER );
}

void* CL_RefStaticMallocDebug(size_t size, const char* label, const char* file, int line) {
    if (size < 1) {
        size = 1;
    }
    void* ptr = Z_TagMalloc((int)size, TAG_STATIC_RENDERER);
    Com_Memset(ptr, 0, size);
    return ptr;
}

void *CL_RefStaticMalloc(int size, ha_pref preference ) {
	if (size < 1) {
		size = 1;
	}
	void *ptr = Z_TagMalloc( size, TAG_STATIC_RENDERER );
	Com_Memset( ptr, 0, size );
	return ptr;
}

void* CL_RefStaticMallocTemp(int size) {
	return Hunk_AllocateTempMemory(size);
}

static void (*s_refHunkClearNotify)( void );

void CL_RefHunkClearNotify( void (*fn)( void ) ) {
	s_refHunkClearNotify = fn;
}

void CL_RefStaticClear( void ) {
	if ( s_refHunkClearNotify ) {
		s_refHunkClearNotify();
	}
	Z_FreeTags( TAG_STATIC_RENDERER );
}

// CG_PlaceCloudFX @0x03A96C -- never from the renderer.  The engine therefore has no

extern "C"
int CL_ScaledMilliseconds(void) {
	return Sys_Milliseconds()*com_timescale->value;
}

extern "C"
void CL_RefNoteBlockedMsec( int msec ) {
	SMP_BudgetAdd( SMP_ZONE_SWAP, msec );
}

void CL_RefFS_WriteFile(const char* qpath, const void* buffer, int size) {
	FS_WriteFile(qpath, buffer, size);
}

char** CL_RefFS_ListFiles(const char* dirPath, const char* extension, int* numfilesfound) {
	return FS_ListFiles(dirPath, extension, qtrue, numfilesfound);
}

void CL_RefCIN_UploadCinematic(int handle) {
}

int CL_RefTIKI_GetNumChannels(dtiki_t* tiki) {
	return tiki->m_boneList.NumChannels();
}

int CL_RefTIKI_GetLocalChannel(dtiki_t* tiki, int channel) {
	return tiki->m_boneList.LocalChannel(channel);
}

int CL_RefTIKI_GetLocalFromGlobal(dtiki_t* tiki, int channel) {
    return tiki->m_boneList.GetLocalFromGlobal(channel);
}

int CL_RefSKEL_GetMorphWeightFrame(void* skeletor, int index, float time, int* data) {
    return ((skeletor_c*)skeletor)->GetMorphWeightFrame(index, time, data);
}

void CL_RefSKEL_SetExternalMorphControllers(void* skeletor, const morph_controller_t* controllers, int count) {
    ((skeletor_c*)skeletor)->SetExternalMorphControllers(controllers, count);
}

int CL_RefSKEL_GetBoneParent(void* skeletor, int boneIndex) {
	return ((skeletor_c*)skeletor)->GetBoneParent(boneIndex);
}

int CL_GetRefSequence(void) {
	return cls.refSequence;
}

qboolean CL_IsRendererLoaded(void) {
	return re.Shutdown != NULL;
}

void CL_InitRef( void ) {
	refimport_t	ri;

	memset(&ri, 0, sizeof(ri));
	refexport_t	*ret;
#ifdef USE_RENDERER_DLOPEN
	GetRefAPI_t		GetRefAPI;
	char			dllName[MAX_OSPATH];
#endif

	Com_Printf( "----- Initializing Renderer ----\n" );

#define RENDERER_ARCH_DLL_EXT DLL_SUFFIX DLL_EXT

#ifdef USE_RENDERER_DLOPEN
	// SANCTIONED PLATFORM GLUE (STANDING_RULES §2.2): `cl_renderer` has NO retail
	// The name/default/flags are ioq3's dynamic-loader convention, not retail's, so
#ifndef EF2_DEFAULT_RENDERER
	/* cmake/client.cmake always supplies this (vulkan, the operator's shipping
	   renderer on every platform); the fallback only fires in a misconfigured
	   build and must agree with that, not with the reference twin. */
	#define EF2_DEFAULT_RENDERER "vulkan"
#endif
	cl_renderer = Cvar_Get("cl_renderer", EF2_DEFAULT_RENDERER, CVAR_ARCHIVE | CVAR_LATCH);

	Com_sprintf(dllName, sizeof(dllName), "renderer_%s" RENDERER_ARCH_DLL_EXT, cl_renderer->string);

	if(!(rendererLib = Sys_LoadDll(dllName, qfalse)) && strcmp(cl_renderer->string, cl_renderer->resetString))
	{
		Com_Printf("failed:\n\"%s\"\n", Sys_LibraryError());
		Cvar_ForceReset("cl_renderer");

		Com_sprintf(dllName, sizeof(dllName), "renderer_%s" RENDERER_ARCH_DLL_EXT, cl_renderer->resetString);
		rendererLib = Sys_LoadDll(dllName, qfalse);
	}

	if(!rendererLib)
	{
		Com_Printf("failed:\n\"%s\"\n", Sys_LibraryError());
		Com_Error(ERR_FATAL, "Failed to load renderer");
	}

	GetRefAPI = (GetRefAPI_t)Sys_LoadFunction(rendererLib, "GetRefAPI");
	if(!GetRefAPI)
	{
		Com_Error(ERR_FATAL, "Can't load symbol GetRefAPI: '%s'",  Sys_LibraryError());
	}
#endif

	ri.Cmd_AddCommand = Cmd_AddCommand;
	ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
	ri.Cmd_Argc = Cmd_Argc;
	ri.Cmd_Argv = Cmd_Argv;
	ri.Cmd_ExecuteText = Cbuf_ExecuteText;
	ri.Printf = CL_RefPrintf;
	ri.Error = Com_Error;
	ri.Milliseconds = CL_ScaledMilliseconds;
	ri.NoteBlockedMsec = CL_RefNoteBlockedMsec;
	ri.FS_RemoveHomeData = FS_Remove_HomeData;
	ri.Malloc = CL_RefMalloc;
	ri.Free = CL_RefFree;
	ri.Clear = CL_RefClear;
    ri.Hunk_Clear = CL_RefStaticClear;
    ri.Hunk_ClearNotify = CL_RefHunkClearNotify;
#ifdef HUNK_DEBUG
    ri.Hunk_AllocDebug = CL_RefStaticMallocDebug;
#else
    ri.Hunk_Alloc = CL_RefStaticMalloc;
#endif
	ri.Hunk_AllocateTempMemory = CL_RefStaticMallocTemp;
	ri.Hunk_FreeTempMemory = Hunk_FreeTempMemory;
	ri.CM_DrawDebugSurface = CM_DrawDebugSurface;

	ri.FS_OpenFile = FS_FOpenFileRead;
	ri.FS_OpenFileWrite = FS_FOpenFileWrite_HomeData;
	ri.FS_CloseFile = FS_FCloseFile;
	ri.FS_Read = FS_Read;
	ri.FS_Write = FS_Write;
	ri.FS_Seek = FS_Seek;
	ri.FS_ReadFile = FS_ReadFile;
	ri.FS_ReadFileEx = FS_ReadFileEx;
	ri.FS_FreeFile = FS_FreeFile;
	ri.FS_WriteFile = CL_RefFS_WriteFile;
	ri.FS_FreeFileList = FS_FreeFileList;
	ri.FS_ListFiles = CL_RefFS_ListFiles;
	ri.FS_FileIsInPAK = FS_FileIsInPAK;
	ri.FS_FileExists = FS_FileExists_HomeData;
	ri.FS_CanonicalFilename = FS_CanonicalFilename;
	ri.Cvar_Get = Cvar_Get;
	ri.Cvar_Set = Cvar_Set;
	ri.Cvar_SetValue = Cvar_SetValue;
	ri.Cvar_SetDefault = Cvar_SetDefault;

    ri.CM_EntityString = CM_EntityString;
    ri.CM_MapTime = CM_MapTime;
    ri.CM_BoxTrace = CM_BoxTrace;
	ri.SetPerformanceCounters = CL_RefSetPerformanceCounters;

    ri.DebugLines = &DebugLines;
    ri.numDebugLines = &numDebugLines;
    ri.DebugStrings = &DebugStrings;
    ri.numDebugStrings = &numDebugStrings;

	ri.TIKI_OrientationInternal = TIKI_OrientationInternal;
	ri.TIKI_IsOnGroundInternal = TIKI_IsOnGroundInternal;
	ri.TIKI_SetPoseInternal = TIKI_SetPoseInternal;
	ri.TIKI_Alloc = TIKI_Alloc;
	ri.GetRadiusInternal = TIKI_GetRadiusInternal;
	ri.GetCentroidRadiusInternal = TIKI_GetCentroidRadiusInternal;
	ri.GetFrameInternal = TIKI_GetFrameInternal;

    ri.Cvar_CheckRange = Cvar_CheckRange;
    ri.Cvar_SetDescription = Cvar_SetDescription;
    ri.Cvar_VariableIntegerValue = Cvar_VariableIntegerValue;

    ri.CIN_UploadCinematic = CL_RefCIN_UploadCinematic;
    ri.CIN_PlayCinematic = CIN_PlayCinematic;
    ri.CIN_RunCinematic = CIN_RunCinematic;

    ri.IN_Init = IN_Init;
    ri.IN_Shutdown = IN_Shutdown;
    ri.IN_Restart = IN_Restart;
    ri.ftol = Q_ftol;

    ri.Sys_SetEnv = Sys_SetEnv;
    ri.Sys_GLimpSafeInit = Sys_GLimpSafeInit;
    ri.Sys_GLimpInit = Sys_GLimpInit;
    ri.Sys_LowPhysicalMemory = Sys_LowPhysicalMemory;

	ri.UI_LoadResource = UI_LoadResource;
	ri.CM_PointLeafnum = CM_PointLeafnum;
	ri.CM_LeafCluster = CM_LeafCluster;

    ri.TIKI_CalcLodConsts = TIKI_CalcLodConsts;
    ri.TIKI_CalculateBounds = TIKI_CalculateBounds;
    ri.TIKI_FindTiki = TIKI_FindTiki;
    ri.TIKI_RegisterTikiFlags = TIKI_RegisterTikiFlags;
    ri.TIKI_GetSkeletor = TIKI_GetSkeletor;
    ri.TIKI_GetSkel         = TIKI_GetSkel;
    ri.TIKI_GetBoneMapping   = TIKI_GetBoneMapping;
    ri.TIKI_GetMorphMapping  = TIKI_GetMorphMapping;
    ri.TIKI_GetSkelAnimFrame = TIKI_GetSkelAnimFrame;
    ri.TIKI_GlobalRadius = TIKI_GlobalRadius;
    ri.TIKI_FindSkelByHeader = TIKI_FindSkelByHeader;
	ri.TIKI_GetNumChannels = CL_RefTIKI_GetNumChannels;
    ri.TIKI_GetLocalChannel = CL_RefTIKI_GetLocalChannel;
    ri.TIKI_GetLocalFromGlobal = CL_RefTIKI_GetLocalFromGlobal;
    ri.TIKI_Anim_NumFrames = TIKI_Anim_NumFrames;
    ri.TIKI_Anim_FrameTime = TIKI_Frame_Time;
    ri.TIKI_Anim_TanFrames = TIKI_Anim_TanFrames;
    ri.TIKI_Skan_FindByAnim = TIKI_Skan_FindByAnim;
    ri.TIKI_Skan_SetProbeSubject = TIKI_Skan_SetProbeSubject;
    ri.TIKI_SkelBonesFromRecords = TIKI_SkelBonesFromRecords;
    ri.TIKI_SetPoseChannelsInternal = TIKI_SetPoseChannelsInternal;

	ri.SKEL_GetBoneParent = CL_RefSKEL_GetBoneParent;
	ri.SKEL_GetMorphWeightFrame = CL_RefSKEL_GetMorphWeightFrame;
	ri.SKEL_SetExternalMorphControllers = CL_RefSKEL_SetExternalMorphControllers;

	ret = GetRefAPI( REF_API_VERSION, &ri );

#if defined __USEA3D && defined __A3D_GEOM
	hA3Dg_ExportRenderGeom (ret);
#endif

	Com_Printf( "-------------------------------\n");

	if ( !ret ) {
		Com_Error (ERR_FATAL, "Couldn't initialize refresh" );
	}

	re = *ret;

	CL_FillUIImports();

	// not have (retail links its renderer in).

	cls.refSequence++;
}



qboolean CL_SetVidMode( int mode ) {
	qboolean ret;

	ret = re.SetMode( mode, &cls.glconfig );

	if( cge ) {
		cge->CG_GetRendererConfig();
	}

	UI_ResolutionChange();
	return ret;
}

void CL_SetFullscreen( qboolean fullscreen ) {
	re.SetFullscreen( fullscreen, &cls.glconfig );

	if( cge ) {
		cge->CG_GetRendererConfig();
	}
}

void CL_VidMode_f( void ) {
	char	text[ 16 ];
	int		mode;

	if( Cmd_Argc() != 2 ) {
		Com_Printf( "Usage: vidmode [modenum]\n" );
		return;
	}

	mode = atoi( Cmd_Argv( 1 ) );

	if( CL_SetVidMode( mode ) ) {
		Com_sprintf( text, sizeof( text ), "%d", mode );
		Cvar_Set( "r_mode", text );
	}
}

void CL_CacheTikisCommand_f( void ) {
	TIKI_CacheAll();
}

void CL_TikiListCommand_f( void ) {
	TIKI_ModelList();
}

void CL_TikiInfoCommand_f( void ) {
	dtiki_t		*tiki;
	qhandle_t	hModel;
	const char	*tikiName;
	char		modelname[ 128 ];

	if( Cmd_Argc() != 2 ) {
		Com_Printf( "usage: tiki tikiname\n" );
		return;
	}

	tikiName = Cmd_Argv( 1 );

	if( strchr( tikiName, '/' ) ) {
		Q_strncpyz( modelname, tikiName, sizeof( modelname ) );
	} else {
		Q_strncpyz( modelname, "models/", sizeof( modelname ) );
		Q_strcat( modelname, sizeof( modelname ), tikiName );
	}

	COM_DefaultExtension( modelname, sizeof( modelname ), ".tik" );
	hModel = re.RegisterModel( modelname );
	tiki = re.R_Model_GetHandle( hModel );

	TIKI_ModelInfo( tiki );
}




void CL_UpdateMissionObjectives( void ) {
	const char *objectives;

	objectives = CL_ConfigString( cl.snap.ps.objectiveNameIndex + CS_OBJECTIVE_NAME );
	if ( strlen( objectives ) ) {
		MissionObjectives_Update( objectives );
	}
}

void CL_SetClientVar_f( void ) {
	char	clientVarName[256];

	if ( Cmd_Argc() != 3 ) {
		return;
	}

	if ( strlen( "ClientVar_" ) + strlen( Cmd_Argv(1) ) + 1 > sizeof( clientVarName ) ) {
		Com_Printf( "Variable name too long (%s)\n", Cmd_Argv(1) );
		return;
	}

	Com_sprintf( clientVarName, sizeof( clientVarName ), "ClientVar_%s", Cmd_Argv(1) );
	Cvar_Set( clientVarName, Cmd_Argv(2) );
}

void CL_ChangeLanguage_f( void ) {
	const char *mapname;

	clearAllStrings();
	loadGlobalStrings();

	mapname = Info_ValueForKey(
		cl.gameState.stringData + cl.gameState.stringOffsets[CS_SERVERINFO],
		"mapname"
	);

	loadLevelStrings( mapname );

	UI_ReloadStrings();

	CL_Vid_Restart_f();
}

void CL_ReplaceShader_f( void ) {
	char	*data;
	char	*brace;

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "Syntax: cl_replaceshader <shadername>\n" );
		return;
	}

	data = ToolServerGetData();
	if ( !data || !*data ) {
		data = Sys_GetClipboardData();
	}
	if ( !data || !*data ) {
		return;
	}

	brace = strchr( data, '{' );
	if ( !brace ) {
		Com_Printf( "Data didn't contain a curly brace!\n" );
		return;
	}

	if ( brace == data ) {
		Com_Printf( "Data text must start with the shader name\n" );
		return;
	}

	if ( re.ReplaceShader ) {
		re.ReplaceShader( Cmd_Argv( 1 ), brace );
	}
}

qboolean CL_DoneLoading( void ) {
	return com_cl_running && com_cl_running->integer && cls.state == CA_ACTIVE;
}

static cvar_t *cl_jobs = NULL;

static void CL_JobPoolTest_f( void ) {
	JobPool_SelfTest( Com_Printf );
}

static void CL_JobPoolStats_f( void ) {
	jobpoolStats_t st;

	JobPool_GetStats( &st );
	Com_Printf( "jobpoolstats: %s %i workers | %i batches, %lld elements parallel; "
		"%i inline calls, %lld elements inline; %i ranges refused\n",
		JobPool_Name(), st.workers, st.batches, st.parallelElements,
		st.inlineCalls, st.inlineElements, st.droppedRanges );
}

void CL_Init( void ) {
	int start, end;

	if( cl_bCLSystemStarted ) {
		return;
	}

	cl_bCLSystemStarted = qtrue;

	Com_Printf( "----- Client Initialization -----\n" );

	start = Sys_Milliseconds();

	cl_jobs = Cvar_Get( "cl_jobs", "-1", 0 );
	JobPool_Init( "cljob", cl_jobs->integer );

	CL_ClearState ();
	S_StopAllSounds2( qtrue );

	cls.state = CA_DISCONNECTED;
	Key_SetCatcher(KEYCATCH_UI);
	cls.realtime = 0;
	cls.saveShotCmd[0] = 0;
	cls.saveShotDrawModel = -1.0f;

	CL_InitInput ();

	if( !L_EventSystemStarted() ) {
		L_InitEvents();
	}

	cl_motd = Cvar_Get ("cl_motd", "1", 0);         

	cl_timeout = Cvar_Get ("cl_timeout", "125", 0);         
	
	cl_timeNudge = Cvar_Get ("cl_timeNudge", "0", CVAR_TEMP );
	cl_shownet = Cvar_Get ("cl_shownet", "0", CVAR_TEMP );
	cl_showSend = Cvar_Get ("cl_showSend", "0", CVAR_TEMP );
	cl_showTimeDelta = Cvar_Get ("cl_showTimeDelta", "0", CVAR_TEMP );
	cl_freezeDemo = Cvar_Get ("cl_freezeDemo", "0", CVAR_TEMP );
	rcon_client_password = Cvar_Get ("rconPassword", "", CVAR_TEMP );
	cl_activeAction = Cvar_Get( "activeAction", "", CVAR_TEMP );

	cl_timedemo = Cvar_Get ("timedemo", "0", 0);

	rconAddress = Cvar_Get ("rconAddress", "", 0);

	cl_yawspeed = Cvar_Get ("cl_yawspeed", "140", CVAR_ARCHIVE);
	cl_pitchspeed = Cvar_Get ("cl_pitchspeed", "140", CVAR_ARCHIVE);
	cl_anglespeedkey = Cvar_Get ("cl_anglespeedkey", "1.5", 0);

	cl_maxpackets = Cvar_Get ("cl_maxpackets", "30", 0 );
	cl_packetdup = Cvar_Get ("cl_packetdup", "1", 0 );

	cl_run = Cvar_Get( "cl_run", "0", CVAR_ARCHIVE );         
    cl_freelook = Cvar_Get("freelook", "1", CVAR_ARCHIVE);
	cl_sensitivity = Cvar_Get( "sensitivity", "5", CVAR_ARCHIVE );
    cl_mouseAccel = Cvar_Get("cl_mouseAccel", "0", CVAR_ARCHIVE);
	cl_showMouseRate = Cvar_Get ("cl_showmouserate", "0", 0);

	// cl_ctrlbindings NEVER EXISTED and the pointer aliased cl_altbindings.
	cl_altbindings = Cvar_Get( "cl_altbindings", "0", CVAR_ARCHIVE );
	cl_ctrlbindings = Cvar_Get( "cl_ctrlbindings", "0", CVAR_ARCHIVE );

	cl_r_fullscreen = Cvar_Get( "r_fullscreen", "1", CVAR_ARCHIVE );

	m_pitch = Cvar_Get ("m_pitch", "0.022", CVAR_ARCHIVE);
	m_yaw = Cvar_Get ("m_yaw", "0.022", CVAR_ARCHIVE);
	m_forward = Cvar_Get ("m_forward", "0.25", CVAR_ARCHIVE);
	m_side = Cvar_Get ("m_side", "0.25", CVAR_ARCHIVE);
	m_filter = Cvar_Get ("m_filter", "0", CVAR_ARCHIVE);


	j_pitch =        Cvar_Get ("j_pitch",        "0.022", CVAR_ARCHIVE);
	j_yaw =          Cvar_Get ("j_yaw",          "-0.022", CVAR_ARCHIVE);
	j_forward =      Cvar_Get ("j_forward",      "-0.25", CVAR_ARCHIVE);
	j_side =         Cvar_Get ("j_side",         "0.25", CVAR_ARCHIVE);
	j_up =           Cvar_Get ("j_up",           "0", CVAR_ARCHIVE);

	j_pitch_axis =   Cvar_Get ("j_pitch_axis",   "3", CVAR_ARCHIVE);
	j_yaw_axis =     Cvar_Get ("j_yaw_axis",     "2", CVAR_ARCHIVE);
	j_forward_axis = Cvar_Get ("j_forward_axis", "1", CVAR_ARCHIVE);
	j_side_axis =    Cvar_Get ("j_side_axis",    "0", CVAR_ARCHIVE);
	j_up_axis =      Cvar_Get ("j_up_axis",      "4", CVAR_ARCHIVE);

	Cvar_CheckRange(j_pitch_axis, 0, MAX_JOYSTICK_AXIS-1, qtrue);
	Cvar_CheckRange(j_yaw_axis, 0, MAX_JOYSTICK_AXIS-1, qtrue);
	Cvar_CheckRange(j_forward_axis, 0, MAX_JOYSTICK_AXIS-1, qtrue);
	Cvar_CheckRange(j_side_axis, 0, MAX_JOYSTICK_AXIS-1, qtrue);
	Cvar_CheckRange(j_up_axis, 0, MAX_JOYSTICK_AXIS-1, qtrue);

	cg_gametype = Cvar_Get( "cg_gametype", "0", 0 );

	cl_updatesource                = Cvar_Get( "cl_updatesource", "0", 0 );
	cl_showemptyservers           = Cvar_Get( "cl_showemptyservers", "1", 0 );         
	cl_showfullservers             = Cvar_Get( "cl_showfullservers", "1", 0 );         

	// The seventeen retail CL_Init cvars the port had never registered, in
	mp_savingDemo                 = Cvar_Get( "mp_savingDemo", "0", CVAR_USERINFO );                    
	cl_systemsetup                = Cvar_Get( "cl_systemsetup", "1", CVAR_ARCHIVE );                    
	cl_updateInfoString           = Cvar_Get( "cl_updateInfoString", "", CVAR_ROM );                    
	cl_timeNudgeSP                = Cvar_Get( "cl_timeNudgeSP", "20", CVAR_TEMP );                      
	cl_avidemo                    = Cvar_Get( "cl_avidemo", "0", 0 );                                   
	cl_alternateMouseMove         = Cvar_Get( "cl_alternateMouseMove", "0", 0 );                        
	m_up                          = Cvar_Get( "m_up", "0", CVAR_ARCHIVE );                              
	dm_morph_c1                   = Cvar_Get( "dm_morph_c1", "0", CVAR_USERINFO );        
	cl_disableloadinginbackground = Cvar_Get( "cl_disableloadinginbackground", "1", CVAR_ARCHIVE );      
	cl_chbottom                   = Cvar_Get( "cl_chbottom", "150", CVAR_ARCHIVE );                     
	cl_chtop                      = Cvar_Get( "cl_chtop", "-150", CVAR_ARCHIVE );                       
	cl_chleft                     = Cvar_Get( "cl_chleft", "-100", CVAR_ARCHIVE );                      
	cl_chright                    = Cvar_Get( "cl_chright", "100", CVAR_ARCHIVE );                      
	Cvar_Get( "cg_predictItems", "1", CVAR_ARCHIVE | CVAR_USERINFO );                                   
	Cvar_Get( "mp_lowBandwidth", "0", CVAR_USERINFO );                                   
	userFov                       = Cvar_Get( "userFov", "0", CVAR_ARCHIVE | CVAR_USERINFO );            
	com_blood                     = Cvar_Get( "com_blood", "1", CVAR_ARCHIVE | CVAR_USERINFO | CVAR_SERVERINFO );        


	cl_inGameVideo = Cvar_Get ("r_inGameVideo", "1", CVAR_ARCHIVE);

	// Port-only cvar; 0 = retail's stretch.  Declared default 1.
	cin_keepAspect = Cvar_Get ("cin_keepAspect", "1", CVAR_ARCHIVE);

	// Port-only cvar (retail has no Android renderer).  Registered on every
	android_aspect = Cvar_Get ("android_aspect", "0", CVAR_ARCHIVE);

	Cvar_Get( "cl_maxPing", "800", 0 );

	name = Cvar_Get ("name", "RedShirt", CVAR_USERINFO | CVAR_ARCHIVE);
	cl_rate = Cvar_Get ("rate", "25000", CVAR_USERINFO );
	Cvar_Get ("snaps", "20", CVAR_USERINFO );
	Cvar_Get ("password", "", CVAR_USERINFO);		 
	Cvar_Get ("mp_playermodel", "models/char/mainchar.tik", CVAR_USERINFO | CVAR_ARCHIVE );

	Cvar_Get ("cg_mpgametype", "1", 0);
	Cvar_Get ("viewsize", "100", CVAR_ARCHIVE);
	Cvar_Get ("cg_effectdetail", "1", CVAR_ARCHIVE);

	Cvar_Get("mp_autoSwitchWeapons", "1", CVAR_USERINFO | CVAR_ARCHIVE);

	Cmd_AddCommand ("cmd", CL_ForwardToServer_f);
	Cmd_AddCommand ("configstrings", CL_Configstrings_f);
	Cmd_AddCommand ("clientinfo", CL_Clientinfo_f);
	Cmd_AddCommand ("snd_restart", CL_Snd_Restart_f);
	Cmd_AddCommand ("vid_restart", CL_Vid_Restart_f);
	Cmd_AddCommand ("disconnect", CL_Disconnect_f);
	Cmd_AddCommand ("record", CL_Record_f);
	Cmd_AddCommand ("demo", CL_PlayDemo_f);
	Cmd_AddCommand ("cinematic", CL_PlayCinematic_f);
	Cmd_AddCommand ("aftercinematic", CL_AfterCinematic_f);
	Cmd_AddCommand ("stoprecord", CL_StopRecord_f);
	Cmd_AddCommand ("connect", CL_Connect_f);
	Cmd_AddCommand ("reconnect", CL_Reconnect_f);
	Cmd_AddCommand ("localservers", CL_LocalServers_f);
	Cmd_AddCommand ("globalservers", CL_GlobalServers_f);
	Cmd_AddCommand ("refreshserverlist", CL_RefreshServerList_f);
	Cmd_AddCommand ("serverstatus", CL_ServerStatus_f );
	Cmd_AddCommand ("setpingsource", CL_SetPingSource_f );
	Cmd_AddCommand ("rcon", CL_Rcon_f);
	Cmd_AddCommand ("setenv", CL_Setenv_f );
	Cmd_AddCommand ("ping", CL_Ping_f );
	Cmd_AddCommand ("tikilist", CL_TikiListCommand_f);
	Cmd_AddCommand ("tiki", CL_TikiInfoCommand_f );
	Cmd_AddCommand ("cachealltikis", CL_CacheTikisCommand_f);
	Cmd_AddCommand ("vidmode", CL_VidMode_f );
	Cmd_AddCommand ("saveshot", CL_SaveShot_f );
	Cmd_AddCommand ("dialog", CL_Dialog_f );
	Cmd_AddCommand ("aliasdump", Alias_Dump );
	Cmd_AddCommand ("changelanguage", CL_ChangeLanguage_f);
	Cmd_AddCommand ("setupsystem", CL_SetupSystem_f);
	Cmd_AddCommand ("flushlips", resetAllLips);
	Cmd_AddCommand ("SetClientVar", CL_SetClientVar_f);
	// occurs nowhere in its string pool, so retail never registers it.

	Cmd_AddCommand ("defaultSoundTrack", setDefaultSoundTrack_f);

	// PORT-ONLY DIAGNOSTIC (uivk-mt), no retail counterpart and no game effect:
	Cmd_AddCommand ("jobpooltest", CL_JobPoolTest_f);
	Cmd_AddCommand ("jobpoolstats", CL_JobPoolStats_f);

	CL_InitConsoleCommands();
	CL_DetectSystemInfo();
	CL_SetupSystem();
	CL_InitRef();
	CL_StartHunkUsers(qfalse);

	SCR_Init ();

	Cbuf_Execute (0);

	Cvar_Set( "cl_running", "1" );

#if defined(NO_MODERN_DMA) && NO_MODERN_DMA
	S_Init2();
#else
	S_Init(qtrue);
#endif

	CreateArenaController();

	CreateGameplayManager();

	CL_StartHunkUsers(qfalse);

	end = Sys_Milliseconds();

	Com_Printf( "----- Client Initialization Complete ----- %i ms\n", end - start );
}


static void CL_ShutdownBody( void ) {
	static qboolean recursive = qfalse;

	if(!(com_cl_running && com_cl_running->integer))
		return;
	
	Com_Printf( "----- CL_Shutdown -----\n" );

	if ( recursive ) {
		printf ("recursive shutdown\n");
		return;
	}
    recursive = qtrue;

	ShutdownCinematicSystem();

	DeleteArenaController();

	CL_Disconnect(qfalse);	 

#if defined(NO_MODERN_DMA) && NO_MODERN_DMA
	S_Shutdown();
#else
	S_Shutdown(qtrue);
#endif
	CL_ShutdownRef();

	CL_ShutdownUI();

	Cmd_RemoveCommand ("cmd");
	Cmd_RemoveCommand ("configstrings");
	Cmd_RemoveCommand ("userinfo");
	Cmd_RemoveCommand ("snd_restart");
	Cmd_RemoveCommand ("vid_restart");
	Cmd_RemoveCommand ("disconnect");
	Cmd_RemoveCommand ("record");
	Cmd_RemoveCommand ("demo");
	Cmd_RemoveCommand ("cinematic");
	Cmd_RemoveCommand ("stoprecord");
	Cmd_RemoveCommand ("connect");
	Cmd_RemoveCommand ("localservers");
	Cmd_RemoveCommand ("globalservers");
	Cmd_RemoveCommand ("refreshserverlist");
	Cmd_RemoveCommand ("rcon");
	Cmd_RemoveCommand ("setenv");
	Cmd_RemoveCommand ("ping");
	Cmd_RemoveCommand ("serverstatus");
	Cmd_RemoveCommand ("setpingsource");

	CL_ShutdownInput();

	Cvar_Set( "cl_running", "0" );

	ShutdownGameplayManager();

	recursive = qfalse;

	Cmd_RemoveCommand ("jobpooltest");
	Cmd_RemoveCommand ("jobpoolstats");
	JobPool_Shutdown();

	Com_Printf( "-----------------------\n" );
	Z_FreeTags(TAG_CLIENT);
	cl_bCLSystemStarted = qfalse;

}

void CL_Shutdown( void ) {
	CL_ShutdownBody();
}


static void CL_SetServerInfo(serverInfo_t *server, const char *info, int ping) {
	if (server) {
		if (info) {
			if ( strlen( Info_ValueForKey( info, "hostname" ) ) ) {
				Q_strncpyz( server->hostName, Info_ValueForKey( info, "hostname" ), sizeof( server->hostName ) );
			}
			if ( strlen( Info_ValueForKey( info, "mapname" ) ) ) {
				Q_strncpyz( server->mapName, Info_ValueForKey( info, "mapname" ), sizeof( server->mapName ) );
			}
			server->gameType = atoi( Info_ValueForKey( info, "mp_gametype" ) );
			server->clients = atoi( Info_ValueForKey( info, "clients" ) );
			server->maxClients = atoi( Info_ValueForKey( info, "sv_maxclients" ) );
			server->pointLimit = atoi( Info_ValueForKey( info, "mp_pointlimit" ) );
			server->timeLimit = atoi( Info_ValueForKey( info, "mp_timelimit" ) );
		}
		server->ping = ping;
	}
}

static void CL_SetServerInfoByAddress(netadr_t from, const char *info, int ping) {
	int i;

	for (i = 0; i < MAX_OTHER_SERVERS; i++) {
		if (NET_CompareAdr(from, cls.localServers[i].adr)) {
			CL_SetServerInfo(&cls.localServers[i], info, ping);
		}
	}

	for (i = 0; i < MAX_OTHER_SERVERS; i++) {
		if (NET_CompareAdr(from, cls.mplayerServers[i].adr)) {
			CL_SetServerInfo(&cls.mplayerServers[i], info, ping);
		}
	}

	for (i = 0; i < MAX_GLOBAL_SERVERS; i++) {
		if (NET_CompareAdr(from, cls.globalServers[i].adr)) {
			CL_SetServerInfo(&cls.globalServers[i], info, ping);
		}
	}

	for (i = 0; i < MAX_OTHER_SERVERS; i++) {
		if (NET_CompareAdr(from, cls.favoriteServers[i].adr)) {
			CL_SetServerInfo(&cls.favoriteServers[i], info, ping);
		}
	}

}

void CL_ServerInfoPacket( netadr_t from, msg_t *msg ) {
	int				i;
	char			*infoString;
	char			info[MAX_INFO_STRING];

	infoString = MSG_ReadString( msg );

	for ( i = 0; i < MAX_PINGREQUESTS; i++ ) {
		if ( cl_pinglist[i].adr.port && !cl_pinglist[i].time && NET_CompareAdr( from, cl_pinglist[i].adr ) ) {
			cl_pinglist[i].time = cls.realtime - cl_pinglist[i].start + 1;
			Com_DPrintf( "ping time %dms from %s\n", cl_pinglist[i].time, NET_AdrToLogString( from ) );

			Q_strncpyz( cl_pinglist[i].info, infoString, sizeof( cl_pinglist[i].info ) );

			return;
		}
	}

	for ( i = 0 ; i < MAX_OTHER_SERVERS ; i++ ) {
		if ( i >= cls.numlocalservers ) {
			break;
		}

		if ( NET_CompareAdr( from, cls.localServers[i].adr ) ) {
			return;
		}
	}

	if ( i == MAX_OTHER_SERVERS ) {
		Com_DPrintf( "MAX_LOCAL_SERVERS hit, dropping infoResponse\n" );
		return;
	}

	cls.numlocalservers = i + 1;
	cls.localServers[i].adr = from;

	Q_strncpyz( info, MSG_ReadString( msg ), MAX_INFO_STRING );
	Com_Printf( "%s: %s", NET_AdrToLogString( from ), info );
}

serverStatus_t *CL_GetServerStatus( netadr_t from ) {
	int i, oldest, oldestTime;

	for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
		if ( NET_CompareAdr( from, cl_serverStatusList[i].address ) ) {
			return &cl_serverStatusList[i];
		}
	}
	for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
		if ( cl_serverStatusList[i].retrieved ) {
			return &cl_serverStatusList[i];
		}
	}
	oldest = -1;
	oldestTime = 0;
	for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
		if (oldest == -1 || cl_serverStatusList[i].startTime < oldestTime) {
			oldest = i;
			oldestTime = cl_serverStatusList[i].startTime;
		}
	}
	return &cl_serverStatusList[oldest];
}

int CL_ServerStatus( const char *serverAddress, char *serverStatusString, int maxLen ) {
	int i;
	netadr_t	to;
	serverStatus_t *serverStatus;

	if ( !serverAddress ) {
		for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
			cl_serverStatusList[i].address.port = 0;
			cl_serverStatusList[i].retrieved = qtrue;
		}
		return qfalse;
	}
	if ( !NET_StringToAdr( serverAddress, &to) ) {
		return qfalse;
	}
	serverStatus = CL_GetServerStatus( to );
	if ( !serverStatusString ) {
		serverStatus->retrieved = qtrue;
		return qfalse;
	}

	if ( NET_CompareAdr( to, serverStatus->address) ) {
		if (!serverStatus->pending) {
			Q_strncpyz(serverStatusString, serverStatus->string, maxLen);
			serverStatus->retrieved = qtrue;
			serverStatus->startTime = 0;
			return qtrue;
		}
		else {
			serverStatus->print = qfalse;
			serverStatus->pending = qtrue;
			serverStatus->retrieved = qfalse;
			serverStatus->time = 0;
			serverStatus->startTime = Com_Milliseconds();
			CL_NET_OutOfBandPrint(to, "getstatus" );
			return qfalse;
		}
	}
	else if ( serverStatus->retrieved ) {
		serverStatus->address = to;
		serverStatus->print = qfalse;
		serverStatus->pending = qtrue;
		serverStatus->retrieved = qfalse;
		serverStatus->startTime = Com_Milliseconds();
		serverStatus->time = 0;
		CL_NET_OutOfBandPrint(to, "getstatus" );
		return qfalse;
	}
	return qfalse;
}

void CL_ServerStatusResponse( netadr_t from, msg_t *msg ) {
	const char	*s;
	char		info[MAX_INFO_STRING];
	int			i, l, ping;
	int			len;
	serverStatus_t *serverStatus;

	serverStatus = NULL;
	for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
		if ( NET_CompareAdr( from, cl_serverStatusList[i].address ) ) {
			serverStatus = &cl_serverStatusList[i];
			break;
		}
	}
	if (!serverStatus) {
		return;
	}

	s = MSG_ReadStringLine( msg );

	len = 0;
	Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "%s", s);

	if (serverStatus->print) {
		Com_Printf("Server settings:\n");
		while (*s) {
			for (i = 0; i < 2 && *s; i++) {
				if (*s == '\\')
					s++;
				l = 0;
				while (*s) {
					info[l++] = *s;
					if (l >= MAX_INFO_STRING-1)
						break;
					s++;
					if (*s == '\\') {
						break;
					}
				}
				info[l] = '\0';
				if (i) {
					Com_Printf("%s\n", info);
				}
				else {
					Com_Printf("%-24s", info);
				}
			}
		}
	}

	len = strlen(serverStatus->string);
	Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "\\");

	if (serverStatus->print) {
		Com_Printf("\nPlayers:\n");
		Com_Printf("num: ping: name:\n");
	}
	for (i = 0, s = MSG_ReadStringLine( msg ); *s; s = MSG_ReadStringLine( msg ), i++) {

		len = strlen(serverStatus->string);
		Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "\\%s", s);

		if (serverStatus->print) {
			ping = 0;
			sscanf(s, "%d", &ping);
			s = strchr(s, ' ');
			if (s)
				s++;
			else
				s = "unknown";
			Com_Printf("%-2d   %-3d   %s\n", i, ping, s );
		}
	}
	len = strlen(serverStatus->string);
	Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "\\");

	serverStatus->time = Com_Milliseconds();
	serverStatus->address = from;
	serverStatus->pending = qfalse;
	if (serverStatus->print) {
		serverStatus->retrieved = qtrue;
	}
}


cvar_t *cl_updatesource;
cvar_t *cl_showemptyservers;
cvar_t *cl_showfullservers;

// Retail off_22BC14 (@0x0022BC14) -- the "the list needs a refresh" latch.  The

static int s_lastPingUpdateTime;


static const float s_browserSourceLocal = 0.0f;     
static float       s_browserRefreshInterval = 1000.0f;  
static const float s_browserSourceFavorite = 2.0f;  
static const float s_browserSourceGlobal = 1.0f;    

int CL_GetServerCount( void ) {
	switch ( cl_updatesource->integer ) {
	case 1:
		return cls.numglobalservers;
	case 2:
		return cls.numfavoriteservers;
	default:
		return cls.numlocalservers;
	}
}

serverInfo_t *CL_GetServers( void ) {
	switch ( cl_updatesource->integer ) {
	case 1:
		return cls.globalServers;
	case 2:
		return cls.favoriteServers;
	default:
		return cls.localServers;
	}
}

void CL_ClearServers( void ) {
	serverInfo_t *servers;
	int           count;
	int           i;

	switch ( cl_updatesource->integer ) {
	case 1:
		count = MAX_GLOBAL_SERVERS;
		servers = cls.globalServers;
		break;
	case 2:
		count = MAX_OTHER_SERVERS;
		servers = cls.favoriteServers;
		break;
	default:
		count = MAX_OTHER_SERVERS;
		servers = cls.localServers;
		break;
	}

	for ( i = 0; i < count; i++ ) {
		Com_Memset( servers[i].adr.ip, 0, sizeof( servers[i].adr.ip ) );
		servers[i].adr.type = NA_IP;
		servers[i].adr.port = 0;
		servers[i].hostName[0] = '\0';
		servers[i].gameType = 0;
		servers[i].clients = 0;
		servers[i].maxClients = 0;
		servers[i].pointLimit = 0;
		servers[i].timeLimit = 0;
		servers[i].ping = -1;
		servers[i].netType = 0;
		servers[i].visible = qfalse;
	}
}

static void CL_SetServerInformation( netadr_t address, const char *info, int ping ) {
	serverInfo_t *servers;
	int           count;
	int           pass;

	for ( pass = 0; pass < 3; pass++ ) {
		switch ( pass ) {
		case 0:
			count = MAX_OTHER_SERVERS;
			servers = cls.localServers;
			break;
		case 1:
			count = MAX_GLOBAL_SERVERS;
			servers = cls.globalServers;
			break;
		default:
			count = MAX_OTHER_SERVERS;
			servers = cls.favoriteServers;
			break;
		}

		for ( count = ( pass == 1 ) ? MAX_GLOBAL_SERVERS : MAX_OTHER_SERVERS; count > 0; count--, servers++ ) {
			if ( !NET_CompareAdr( address, servers->adr ) ) {
				continue;
			}

			servers->ping = ping;
			if ( info ) {
				if ( strlen( Info_ValueForKey( info, "hostname" ) ) ) {
					Q_strncpyz( servers->hostName, Info_ValueForKey( info, "hostname" ), sizeof( servers->hostName ) );
				}
				if ( strlen( Info_ValueForKey( info, "mapname" ) ) ) {
					Q_strncpyz( servers->mapName, Info_ValueForKey( info, "mapname" ), sizeof( servers->mapName ) );
				}
				servers->gameType = atoi( Info_ValueForKey( info, "mp_gametype" ) );
				servers->clients = atoi( Info_ValueForKey( info, "clients" ) );
				servers->maxClients = atoi( Info_ValueForKey( info, "sv_maxclients" ) );
				servers->pointLimit = atoi( Info_ValueForKey( info, "mp_pointlimit" ) );
				servers->timeLimit = atoi( Info_ValueForKey( info, "mp_timelimit" ) );
			}
		}
	}
}

static void CL_AddServerPing( serverInfo_t *server ) {
	int i;

	for ( i = 0; i < MAX_PINGREQUESTS; i++ ) {
		if ( cl_pinglist[i].adr.port && NET_CompareAdr( cl_pinglist[i].adr, server->adr ) ) {
			return;
		}
	}

	for ( i = 0; i < MAX_PINGREQUESTS; i++ ) {
		if ( !cl_pinglist[i].adr.port ) {
			break;
		}
	}

	if ( i == MAX_PINGREQUESTS ) {
		return;
	}

	cl_pinglist[i].adr = server->adr;
	cl_pinglist[i].start = cls.realtime;
	cl_pinglist[i].time = 0;

	NET_OutOfBandPrint( NS_CLIENT, cl_pinglist[i].adr, "getinfo xxx" );
}

qboolean CL_UpdateServerPings( void ) {
	serverInfo_t *servers;
	int           count;
	int           maxPing;
	int           ping;
	int           pending;
	int           i;

	pending = 0;
	for ( i = 0; i < MAX_PINGREQUESTS; i++ ) {
		if ( cl_pinglist[i].adr.port ) {
			pending++;
		}
	}

	count = CL_GetServerCount();
	servers = CL_GetServers();

	if ( pending < MAX_PINGREQUESTS ) {
		for ( i = 0; i < count; i++ ) {
			if ( servers[i].ping == -1 ) {
				CL_AddServerPing( &servers[i] );
			}
		}
	}

	for ( i = 0; i < MAX_PINGREQUESTS; i++ ) {
		if ( !cl_pinglist[i].adr.port ) {
			continue;
		}

		ping = cl_pinglist[i].time;
		if ( !ping ) {
			maxPing = Cvar_VariableIntegerValue( "cl_maxPing" );
			ping = cls.realtime - cl_pinglist[i].start;
			if ( maxPing < 100 ) {
				maxPing = 100;
			}
			if ( ping < maxPing ) {
				ping = 0;
			}
			if ( ping > maxPing ) {
				ping = maxPing;
			}
		}

		if ( ping ) {
			CL_SetServerInformation( cl_pinglist[i].adr, cl_pinglist[i].info, ping );
			cl_pinglist[i].adr.port = 0;
			cl_pinglist[i].info[0] = '\0';
		}
	}

	pending = 0;
	for ( i = 0; i < MAX_PINGREQUESTS; i++ ) {
		if ( cl_pinglist[i].adr.port ) {
			pending++;
		}
	}

	return ( qboolean )( pending != 0 );
}

void CL_ServerRefresh( void ) {
	qboolean emptyList;
	int	limit_z;

	limit_z = LIMIT_Leg();
	if ( !s_serverListNeedsRefresh ) {
		LIMIT_LegEnd( LIMIT_FAM_UIT, LIMIT_UITZ_srvnone, limit_z );
		return;
	}

	emptyList = qfalse;
	if ( cl_updatesource->value != s_browserSourceFavorite ) {
		if ( CL_GetServerCount() <= 0 ) {
			emptyList = qtrue;
		}
	}

	if ( ( float )cls.realtime - ( float )s_lastPingUpdateTime >= s_browserRefreshInterval || !emptyList ) {
		int			limit_zp;
		qboolean	pinged;

		LIMIT_Count( LIMIT_FAM_UIT, LIMIT_UTC_srvrun, 1 );

		limit_zp = LIMIT_Leg();
		pinged  = CL_UpdateServerPings();
		if ( !pinged && !emptyList ) {
			s_serverListNeedsRefresh = 0;
		}
		s_lastPingUpdateTime = cls.realtime;
		LIMIT_LegEnd( LIMIT_FAM_UIT, LIMIT_UITZ_srvping, limit_zp );

		LIMIT_Count( LIMIT_FAM_UIT, LIMIT_UTC_srvn, CL_GetServerCount() );

		limit_zp = LIMIT_Leg();
		UI_UpdateServerList();
		LIMIT_LegEnd( LIMIT_FAM_UIT, LIMIT_UITZ_srvlist, limit_zp );
	}
}

void CL_RefreshServerList_f( void ) {
	serverInfo_t *servers;
	int           count;
	int           i;

	switch ( cl_updatesource->integer ) {
	case 1:
		count = MAX_GLOBAL_SERVERS;
		servers = cls.globalServers;
		break;
	case 2:
		count = MAX_OTHER_SERVERS;
		servers = cls.favoriteServers;
		break;
	default:
		count = MAX_OTHER_SERVERS;
		servers = cls.localServers;
		break;
	}

	for ( i = 0; i < count; i++ ) {
		servers[i].ping = -1;
		servers[i].netType = 0;
		servers[i].visible = qfalse;
	}

	s_serverListNeedsRefresh = 1;
}

void CL_SetPingSource_f( void ) {
	const char *arg;
	float       value;

	if ( Cmd_Argc() < 1 ) {
		Com_Printf( "usage: setpingsource <local,global, or favorite>\n" );
		return;
	}

	arg = Cmd_Argv( 1 );
	if ( !arg || !strlen( arg ) ) {
		return;
	}

	if ( stricmp( arg, "local" ) ) {
		if ( stricmp( arg, "global" ) ) {
			value = s_browserSourceFavorite;
		} else {
			value = s_browserSourceGlobal;
		}
	} else {
		value = s_browserSourceLocal;
	}

	Cvar_SetValue( "cl_updatesource", value );
}

void CL_LocalServers_f( void ) {
	const char *message;
	int         i, j;
	netadr_t    to;

	Com_Printf( "Scanning for servers on the local network...\n" );

	// port-only: nothing in retail reads this cvar (it is not in the retail CL
	CL_ClearServers();
	s_serverListNeedsRefresh = 1;
	cls.numlocalservers = 0;
	cl_updatesource->value = s_browserSourceLocal;

	Com_Memset( &to, 0, sizeof( to ) );

	message = "\377\377\377\377getinfo xxx";

	for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < NUM_SERVER_PORTS; j++ ) {
			to.port = BigShort( ( short )( PORT_SERVER + j ) );

			to.type = NA_BROADCAST;
			NET_SendPacket( NS_CLIENT, strlen( message ), message, to );

			to.type = NA_BROADCAST_IPX;
			NET_SendPacket( NS_CLIENT, strlen( message ), message, to );
		}
	}
}

void CL_GlobalServers_f( void ) {
	netadr_t    to;
	char        command[1024];
	const char *protocol;
	int         master;
	int         count, i;

	if ( ( count = Cmd_Argc() ) < 3 ) {
		Com_Printf( "usage: globalservers <master# 0-1> <protocol> [keywords]\n" );
		return;
	}

	Com_Printf( "Requesting servers from the master...\n" );

	CL_ClearServers();
	s_serverListNeedsRefresh = 1;

	master = atoi( Cmd_Argv( 1 ) );
	cls.masterNum = master;
	cls.numglobalservers = -1;

	if ( master == 1 ) {
		NET_StringToAdr( "master.quake3world.com", &to );
	} else {
		NET_StringToAdr( "ef2master.activision.com", &to );
		cl_updatesource->value = s_browserSourceGlobal;
	}

	to.type = NA_IP;
	to.port = BigShort( PORT_MASTER );

	protocol = Cmd_Argv( 2 );
	sprintf( command, "getservers %s", protocol );

	for ( i = 3; i < count; i++ ) {
		Q_strcat( command, sizeof( command ), " " );
		Q_strcat( command, sizeof( command ), Cmd_Argv( i ) );
	}

	if ( Cvar_VariableValue( "fs_restrict" ) != 0.0f ) {
		Q_strcat( command, sizeof( command ), " demo" );
	}

	NET_OutOfBandPrint( NS_SERVER, to, "%s", command );
}


void CL_GetPing( int n, char *buf, int buflen, int *pingtime )
{
	const char	*str;
	int		time;
	int		maxPing;

	if (n < 0 || n >= MAX_PINGREQUESTS || !cl_pinglist[n].adr.port)
	{
		buf[0]    = '\0';
		*pingtime = 0;
		return;
	}

	str = NET_AdrToLogString( cl_pinglist[n].adr );
	Q_strncpyz( buf, str, buflen );

	time = cl_pinglist[n].time;
	if (!time)
	{
		time = Sys_Milliseconds() - cl_pinglist[n].start;
		maxPing = Cvar_VariableIntegerValue( "cl_maxPing" );
		if( maxPing < 100 ) {
			maxPing = 100;
		}
		if (time < maxPing)
		{
			time = 0;
		}
	}

	CL_SetServerInfoByAddress(cl_pinglist[n].adr, cl_pinglist[n].info, cl_pinglist[n].time);

	*pingtime = time;
}

void CL_UpdateServerInfo( int n )
{
	if (!cl_pinglist[n].adr.port)
	{
		return;
	}

	CL_SetServerInfoByAddress(cl_pinglist[n].adr, cl_pinglist[n].info, cl_pinglist[n].time );
}

void CL_GetPingInfo( int n, char *buf, int buflen )
{
	if (n < 0 || n >= MAX_PINGREQUESTS || !cl_pinglist[n].adr.port)
	{
		if (buflen)
			buf[0] = '\0';
		return;
	}

	Q_strncpyz( buf, cl_pinglist[n].info, buflen );
}

void CL_ClearPing( int n )
{
	if (n < 0 || n >= MAX_PINGREQUESTS)
		return;

	cl_pinglist[n].adr.port = 0;
}

int CL_GetPingQueueCount( void )
{
	int		i;
	int		count;
	ping_t*	pingptr;

	count   = 0;
	pingptr = cl_pinglist;

	for (i=0; i<MAX_PINGREQUESTS; i++, pingptr++ ) {
		if (pingptr->adr.port) {
			count++;
		}
	}

	return (count);
}

ping_t* CL_GetFreePing( void )
{
	ping_t*	pingptr;
	ping_t*	best;	
	int		oldest;
	int		i;
	int		time;

	pingptr = cl_pinglist;
	for (i=0; i<MAX_PINGREQUESTS; i++, pingptr++ )
	{
		if (pingptr->adr.port)
		{
			if (!pingptr->time)
			{
				if (Sys_Milliseconds() - pingptr->start < 500)
				{
					continue;
				}
			}
			else if (pingptr->time < 500)
			{
				continue;
			}
		}

		pingptr->adr.port = 0;
		pingptr->info[0] = 0;
		return (pingptr);
	}

	pingptr = cl_pinglist;
	best    = cl_pinglist;
	oldest  = INT_MIN;
	for (i=0; i<MAX_PINGREQUESTS; i++, pingptr++ )
	{
		time = Sys_Milliseconds() - pingptr->start;
		if (time > oldest)
		{
			oldest = time;
			best   = pingptr;
		}
	}

	return (best);
}

void CL_Ping_f( void ) {
	netadr_t	to;
	ping_t*		pingptr;
	char*		server;
	int			argc;

	argc = Cmd_Argc();

	if ( argc != 2 ) {
		Com_Printf( "usage: ping [server]\n");
		return;
	}

	server = Cmd_Argv(1);

	Com_Memset( &to, 0, sizeof(netadr_t) );

	if ( !NET_StringToAdr( server, &to ) ) {
		return;
	}

	pingptr = CL_GetFreePing();

	memcpy( &pingptr->adr, &to, sizeof (netadr_t) );
	pingptr->start = cls.realtime;	 
	pingptr->time  = 0;

	CL_NET_OutOfBandPrint( to, "getinfo xxx" );
}

void CL_SaveShot_f( void ) {
	int i;
	char expanded[ 1024 ];
	cvar_t *drawviewmodel;

	if( !cls.rendererRegistered ) {
		return;
	}

	memcpy( expanded, "screenshot", 11 );

	for( i = 1; i < Cmd_Argc(); i++ )
	{
		strcat( expanded, " " );
		strcat( expanded, Cmd_Argv( i ) );
	}

	strcat( expanded, "\n" );

	cls.no_menus = qtrue;
	cls.saveShotFrames = 0;

	/* The thumbnail is the world frame the player was looking at, so the request
	   rides the next frame the suppression actually draws instead of firing
	   here: the frames that follow a save are whatever the client is showing --
	   a sublevel's loading screen, a menu, a frame with no world in it. */
	Q_strncpyz( cls.saveShotCmd, expanded, sizeof( cls.saveShotCmd ) );

	drawviewmodel = Cvar_FindVar( "r_drawviewmodel" );
	if ( drawviewmodel ) {
		cls.saveShotDrawModel = drawviewmodel->value;
		Cvar_SetValue( "r_drawviewmodel", 0.0f );
	}

	UpdateStereoSide( STEREO_CENTER );

	/* The UI stays off until the renderer has taken the shot: restoring it here
	   would let the next frame draw the pause menu, and that is the frame the
	   capture reads.  SCR_UpdateScreen releases it. */
}

void CL_Dialog_f( void ) {
	char		title[ 64 ];
	char		cvar[ 64 ];
	char		command[ 64 ];
	char		shader[ 64 ];
	char		okshader[ 64 ];
	char		cancelshader[ 64 ];
	int			width, height;

	if( Cmd_Argc() < 4 ) {
		Com_Printf( "usage: dialog title cvar_name command [width] [height]\n" );
	}

	Q_strncpyz( title, Cmd_Argv( 1 ), sizeof( title ) );
	Q_strncpyz( cvar, Cmd_Argv( 2 ), sizeof( cvar ) );
	Q_strncpyz( command, Cmd_Argv( 3 ), sizeof( command ) );

	if( Cmd_Argc() > 4 ) {
		width = atoi( Cmd_Argv( 4 ) );
	} else {
		width = 300;
	}
	if( Cmd_Argc() > 5 ) {
		height = atoi( Cmd_Argv( 5 ) );
	} else {
		height = 200;
	}

	shader[ 0 ] = 0;
	if( Cmd_Argc() > 6 ) {
		Q_strncpyz( shader, Cmd_Argv( 6 ), sizeof( shader ) );
	}

	okshader[ 0 ] = 0;
	if( Cmd_Argc() > 7 ) {
		Q_strncpyz( okshader, Cmd_Argv( 7 ), sizeof( okshader ) );
	}

	cancelshader[ 0 ] = 0;
	if( Cmd_Argc() > 8 ) {
		Q_strncpyz( cancelshader, Cmd_Argv( 8 ), sizeof( cancelshader ) );
	}

	UI_CreateDialog( title, cvar, command, width, height, shader, okshader, cancelshader );
}

void CL_ServerRestarted( void ) {
	S_StopAllSounds( qfalse );

	UI_ServerLoaded();
	UI_ClearState();

	VectorClear( cl.viewangles );
}

qboolean CL_UpdateVisiblePings_f(int source) {
	int			slots, i;
	char		buff[MAX_STRING_CHARS];
	int			pingTime;
	int			max;
	qboolean status = qfalse;

	if (source < 0 || source > AS_FAVORITES) {
		return qfalse;
	}

	cls.pingUpdateSource = source;

	slots = CL_GetPingQueueCount();
	if (slots < MAX_PINGREQUESTS) {
		serverInfo_t *server = NULL;

		switch (source) {
			case AS_LOCAL :
				server = &cls.localServers[0];
				max = cls.numlocalservers;
			break;
			case AS_GLOBAL :
				server = &cls.globalServers[0];
				max = cls.numglobalservers;
			break;
			case AS_FAVORITES :
				server = &cls.favoriteServers[0];
				max = cls.numfavoriteservers;
			break;
			default:
				return qfalse;
		}
		for (i = 0; i < max; i++) {
			if (server[i].visible) {
				if (server[i].ping == -1) {
					int j;

					if (slots >= MAX_PINGREQUESTS) {
						break;
					}
					for (j = 0; j < MAX_PINGREQUESTS; j++) {
						if (!cl_pinglist[j].adr.port) {
							continue;
						}
						if (NET_CompareAdr( cl_pinglist[j].adr, server[i].adr)) {
							break;
						}
					}
					if (j >= MAX_PINGREQUESTS) {
						status = qtrue;
						for (j = 0; j < MAX_PINGREQUESTS; j++) {
							if (!cl_pinglist[j].adr.port) {
								break;
							}
						}
						memcpy(&cl_pinglist[j].adr, &server[i].adr, sizeof(netadr_t));
						cl_pinglist[j].start = Sys_Milliseconds();
						cl_pinglist[j].time = 0;
						CL_NET_OutOfBandPrint( cl_pinglist[j].adr, "getinfo xxx" );
						slots++;
					}
				}
				else if (server[i].ping == 0) {
					if (source == AS_GLOBAL) {
						if ( cls.numGlobalServerAddresses > 0 ) {
							cls.numGlobalServerAddresses--;
							CL_InitServerInfo(&server[i], &cls.globalServerAddresses[cls.numGlobalServerAddresses]);
						}
					}
				}
			}
		}
	} 

	if (slots) {
		status = qtrue;
	}
	for (i = 0; i < MAX_PINGREQUESTS; i++) {
		if (!cl_pinglist[i].adr.port) {
			continue;
		}
		CL_GetPing( i, buff, MAX_STRING_CHARS, &pingTime );
		if (pingTime != 0) {
			CL_ClearPing(i);
			status = qtrue;
		}
	}

	return status;
}

void CL_ServerStatus_f(void) {
	netadr_t	to;

	if ( Cmd_Argc() < 1 ) {
		Com_Printf( "usage: serverstatus <ip>\n" );
		return;
	}

	if ( !NET_StringToAdr( Cmd_Argv(1), &to ) ) {
		Com_Printf( "CL_GetServerStatus Error: invalid address\n" );
		return;
	}

	CL_NET_OutOfBandPrint( to, "getstatus xxx" );
}

qboolean CL_CDKeyValidate( const char *key, const char *checksum ) {
	char	ch;
	byte	sum;
	char	chs[3];
	int i;
	size_t len;

	len = strlen(key);
	if( len != CDKEY_LEN ) {
		return qfalse;
	}

	if( checksum && strlen( checksum ) != CDCHKSUM_LEN ) {
		return qfalse;
	}

	sum = 0;
	for (i = 0; (size_t)i < len; i++) {
		ch = *key++;
		if (ch>='a' && ch<='z') {
			ch -= 32;
		}
		switch( ch ) {
		case '2':
		case '3':
		case '7':
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'G':
		case 'H':
		case 'J':
		case 'L':
		case 'P':
		case 'R':
		case 'S':
		case 'T':
		case 'W':
			sum += ch;
			continue;
		default:
			return qfalse;
		}
	}

	Com_sprintf(chs, sizeof(chs), "%02x", sum);

	if (checksum && !Q_stricmp(chs, checksum)) {
		return qtrue;
	}

	if (!checksum) {
		return qtrue;
	}

	return qfalse;
}

