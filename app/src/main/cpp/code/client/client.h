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

#pragma once

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../cgame/tr_types.h"
#include "../renderercommon/tr_public.h"
#include "keys.h"
#include "snd_public.h"
#include "../cgame/cg_public.h"
#include "../fgame/bg_public.h"

#ifdef USE_CURL
#endif 

#ifdef USE_VOIP
#endif

#define	RETRANSMIT_TIMEOUT	3000	

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	qboolean		valid;			
	int				snapFlags;		

	int				serverTime;		
int serverTimeResidual;
	int				messageNum;		
	int				deltaNum;		
	int				ping;			
	byte			areamask[MAX_MAP_AREA_BYTES];		

	int				cmdNum;			
	playerState_t	ps;						

	int				numEntities;			
	int				parseEntitiesNum;		

	int				serverCommandNum;		
	int				number_of_sounds;
	server_sound_t	sounds[64];
	worldState_t	ws;
} clSnapshot_t;




typedef struct {
	int		p_cmdNumber;		
	int		p_serverTime;		
	int		p_realtime;			
	usereyes_t p_eyeinfo;		
} outPacket_t;

#define	MAX_PARSE_ENTITIES	2048

extern int g_console_field_width;
extern int g_console_charWidth;
extern int g_console_charHeight;

typedef struct {
	int			timeoutcount;		
	clSnapshot_t	snap;			

	int			serverTime;			
	int			oldServerTime;		
	int			serverStartTime;
	int			oldFrameServerTime;	
	int			serverTimeDelta;	
	qboolean	extrapolatedSnapshot;	
	qboolean	newSnapshots;		

	gameState_t	gameState;			
	char		mapname[MAX_QPATH];	

	int			parseEntitiesNum;	

	int			mouseDx[2], mouseDy[2];	
	int			mouseIndex;

	int				mousex, mousey;
	unsigned int	mouseButtons;

	int			joystickAxis[MAX_JOYSTICK_AXIS];	

	usercmd_t	cmds[CMD_BACKUP];	
	int			cmdNumber;			

	outPacket_t	outPackets[PACKET_BACKUP];	

	vec3_t		viewangles;

	int			serverId;			
	clSnapshot_t	snapshots[PACKET_BACKUP];

	entityState_t	entityBaselines[MAX_GENTITIES];	

	entityState_t	parseEntities[MAX_PARSE_ENTITIES];
} clientActive_t;

extern	clientActive_t		cl;

void CL_AddCommandString(int seq, const char *s);
void CL_CommitSnapshot(clSnapshot_t *newSnap);
void CL_SnapshotWorldState(worldState_t *ws);


#define MAX_TIMEDEMO_DURATIONS	4096

#ifndef MAX_CLIENT_STRING_CHARS
#define	MAX_CLIENT_STRING_CHARS	1024
#endif

typedef struct {

	int			clientNum;
	int			lastPacketSentTime;			
	int			lastPacketTime;				

	netadr_t	serverAddress;
	int			connectTime;				
	int			connectStartTime;
	int			connectPacketCount;			
	char		serverMessage[MAX_STRING_TOKENS];	

	int			challenge;					
	int			checksumFeed;				

	int			reliableSequence;
	int			reliableAcknowledge;		
	char		reliableCommands[MAX_RELIABLE_COMMANDS][MAX_CLIENT_STRING_CHARS];


	int			serverMessageSequence;

	int			serverCommandSequence;
	char		serverCommands[MAX_RELIABLE_COMMANDS][MAX_CLIENT_STRING_CHARS];


	char		demoName[MAX_QPATH];
	qboolean	demorecording;
	qboolean	demoplaying;
	qboolean	demowaiting;	
	qboolean	firstDemoFrameSkipped;
	fileHandle_t	demofile;

	int			timeDemoFrames;		
	int			timeDemoStart;		
	int			timeDemoBaseTime;	
	int			timeDemoLastFrame;
	int			timeDemoMinDuration;	
	int			timeDemoMaxDuration;	
	unsigned char	timeDemoDurations[ MAX_TIMEDEMO_DURATIONS ];	

    float		aviVideoFrameRemainder;

#ifdef USE_VOIP
#endif

	netchan_t	netchan;
} clientConnection_t;

extern	clientConnection_t clc;


typedef struct {
	netadr_t	adr;
	int			start;
	int			time;
	char		info[MAX_INFO_STRING];
} ping_t;


typedef struct {
	netadr_t	adr;
	char		hostName[32];
	char		mapName[32];
	int			gameType;
	int			clients;
	int			maxClients;
	int			pointLimit;
	int			timeLimit;
	int			ping;
	int			netType;	 
	qboolean	visible;	 
} serverInfo_t;

static_assert(sizeof(serverInfo_t) == sizeof(netadr_t) + 96, "serverInfo_t must stay retail-shaped");
static_assert(sizeof(serverInfo_t) == 116, "serverInfo_t must be the retail 116 bytes");
static_assert(offsetof(serverInfo_t, adr) == 0, "");
static_assert(offsetof(serverInfo_t, hostName) == sizeof(netadr_t) + 20 - 20, "");
static_assert(offsetof(serverInfo_t, mapName) == sizeof(netadr_t) + 52 - 20, "");
static_assert(offsetof(serverInfo_t, gameType) == sizeof(netadr_t) + 84 - 20, "");
static_assert(offsetof(serverInfo_t, clients) == sizeof(netadr_t) + 88 - 20, "");
static_assert(offsetof(serverInfo_t, maxClients) == sizeof(netadr_t) + 92 - 20, "");
static_assert(offsetof(serverInfo_t, pointLimit) == sizeof(netadr_t) + 96 - 20, "");
static_assert(offsetof(serverInfo_t, timeLimit) == sizeof(netadr_t) + 100 - 20, "");
static_assert(offsetof(serverInfo_t, ping) == sizeof(netadr_t) + 104 - 20, "");
static_assert(offsetof(serverInfo_t, netType) == sizeof(netadr_t) + 108 - 20, "");
static_assert(offsetof(serverInfo_t, visible) == sizeof(netadr_t) + 112 - 20, "");

typedef struct {
	byte	ip[4];
	unsigned short	port;
} serverAddress_t;

typedef enum loadProgress_e {
	LOAD_PROGRESS_FALSE,
	LOAD_PROGRESS_DEFAULT,
	LOAD_PROGRESS_LOAD,
	LOAD_PROGRESS_SAVE
} loadProgress_t;

typedef struct {
	connstate_t		state;				 
	loadProgress_t	loading;
	char			servername[MAX_OSPATH];
	int				keyCatchers;
	qboolean		vid_restart;
	qboolean		cddialog;			
	qboolean		no_menus;
	int				saveShotFrames;		/* frames since the save shot was asked for */
	char			saveShotCmd[MAX_OSPATH];	/* the shot rides the next world frame */
	float			saveShotDrawModel;	/* r_drawviewmodel, held out of the shot */

	qboolean	rendererRegistered;
	qboolean	cgameStarted;
	qboolean	uiStarted;
	qboolean	timeScaled;

	int			framecount;
	int			frametime;			
	float		serverFrameTime;

	int			realtime;			
	int			realFrametime;		

	int				numlocalservers;
	serverInfo_t	localServers[ MAX_OTHER_SERVERS ];
	qboolean		bNewLocalServerInfo;

	int			numglobalservers;
	serverInfo_t  globalServers[MAX_GLOBAL_SERVERS];
	int			numGlobalServerAddresses;
	netadr_t	globalServerAddresses[MAX_GLOBAL_SERVERS];

	int			numfavoriteservers;
	serverInfo_t	favoriteServers[MAX_OTHER_SERVERS];

	int			nummplayerservers;
	serverInfo_t	mplayerServers[MAX_OTHER_SERVERS];

	int pingUpdateSource;		

	int masterNum;

	netadr_t	updateServer;
	char		updateChallenge[MAX_TOKEN_CHARS];
	char		updateInfoString[MAX_INFO_STRING];

	netadr_t	authorizeServer;

	glconfig_t	glconfig;
	int total_tris;
	int total_verts;
	int total_texels;
	int world_tris;
	int world_verts;
	int character_lights;
	clientAnim_t anim;
	stopWatch_t stopwatch;
	char gcdResponse[73];
	qhandle_t	whiteShader;

	int refSequence;
} clientStatic_t;

extern clientStatic_t	cls;


extern	clientGameExport_t	*cge;
extern	refexport_t		re;		


extern qboolean	scr_initialized;

extern	cvar_t	*cl_nodelta;
extern	cvar_t	*cl_debugMove;
extern	cvar_t	*cl_timegraph;
extern	cvar_t	*cl_debuggraph;
extern	cvar_t	*cl_timeout;
extern	cvar_t	*cl_maxpackets;
extern	cvar_t	*cl_packetdup;
extern	cvar_t	*cl_shownet;
extern	cvar_t	*cl_showSend;
extern	cvar_t	*cl_timeNudge;
extern	cvar_t	*cl_showTimeDelta;
extern	cvar_t	*cl_freezeDemo;

extern	cvar_t	*cl_yawspeed;
extern	cvar_t	*cl_pitchspeed;
extern	cvar_t	*cl_run;
extern	cvar_t	*cl_anglespeedkey;

extern	cvar_t	*cl_sensitivity;
extern	cvar_t	*cl_freelook;

extern	cvar_t	*cl_mouseAccel;
extern	cvar_t	*cl_showMouseRate;

extern	cvar_t	*cl_altbindings;
extern	cvar_t	*cl_ctrlbindings;

extern	cvar_t	*m_pitch;
extern	cvar_t	*m_yaw;
extern	cvar_t	*m_forward;
extern	cvar_t	*m_up;
extern	cvar_t	*m_side;
extern	cvar_t	*m_filter;
extern	cvar_t	*cl_alternateMouseMove;

extern	cvar_t	*j_pitch;
extern	cvar_t	*j_yaw;
extern	cvar_t	*j_forward;
extern	cvar_t	*j_side;
extern	cvar_t	*j_up;
extern	cvar_t	*j_pitch_axis;
extern	cvar_t	*j_yaw_axis;
extern	cvar_t	*j_forward_axis;
extern	cvar_t	*j_side_axis;
extern	cvar_t	*j_up_axis;

extern	cvar_t	*cl_timedemo;

extern	cvar_t	*cl_activeAction;

extern	cvar_t	*cl_inGameVideo;

extern	cvar_t	*cin_keepAspect;

extern	cvar_t	*android_aspect;


extern	cvar_t	*cl_r_fullscreen;


#ifdef USE_MUMBLE
#endif

#ifdef USE_VOIP
#endif

extern	cvar_t	*cg_gametype;

extern	cvar_t* j_pitch;
extern	cvar_t* j_yaw;
extern	cvar_t* j_forward;
extern	cvar_t* j_side;
extern	cvar_t* j_up;
extern	cvar_t* j_pitch_axis;
extern	cvar_t* j_yaw_axis;
extern	cvar_t* j_forward_axis;
extern	cvar_t* j_side_axis;
extern	cvar_t* j_up_axis;



void CL_Init (void);
void CL_InitClientSavedData(void);
void CL_AddReliableCommand(const char *cmd, qboolean isDisconnectCmd);

void CL_StartHunkUsers( qboolean rendererOnly );


void CL_Disconnect_f (void);
void CL_GetChallengePacket (void);
void CL_Vid_Restart_f( void );
void CL_Snd_Restart_f (void);
const char *CL_ConfigString( int index );
void CL_StartDemoLoop( void );
void CL_NextDemo( void );
void CL_ReadDemoMessage( void );
void CL_StopRecord_f(void);

void CL_GetPing( int n, char *buf, int buflen, int *pingtime );
void CL_GetPingInfo( int n, char *buf, int buflen );
void CL_ClearPing( int n );
int CL_GetPingQueueCount( void );

void CL_ShutdownRef( void );
void CL_InitRef( void );
qboolean CL_CDKeyValidate( const char *key, const char *checksum );
int CL_ServerStatus( const char *serverAddress, char *serverStatusString, int maxLen );
void UI_ClearResource( void );
void UI_LoadResource( const char *name );

qboolean CL_CheckPaused(void);

int CL_GetRefSequence(void);
qboolean CL_IsRendererLoaded(void);

typedef struct {
	int			down[2];		
	unsigned	downtime;		
	unsigned	msec;			
	qboolean	active;			
	qboolean	wasPressed;		
} kbutton_t;

extern	kbutton_t	in_mlook, in_klook;
extern 	kbutton_t 	in_strafe;
extern 	kbutton_t 	in_speed;
extern	qboolean	in_guimouse;

void IN_ToggleMouse( void );
void IN_MouseOn( void );
void IN_MouseOff( void );

qboolean UI_IsHudActive( void );

// raises -- written by retail and never read.
extern	int		in_objectivesHud;
extern	int		in_scoreboardShown;
extern	int		in_missionStatus;

void IN_Objectives_ScoreDown( void );
void IN_Objectives_ScoreUp( void );
void IN_MissionStatusDown( void );
void IN_MissionStatusUp( void );

void CL_UpdateMissionObjectives( void );	 

void GetViewTrace( trace_t *trace );			 
void CL_KeyMoveAlternate( usercmd_t *cmd );
void CL_JoystickMove( usercmd_t *cmd );

extern	vec3_t	cl_crosshairoffset;

float *get_crosshairoffset( void );

void CL_InitInput (void);
void CL_ShutdownInput(void);
void CL_SendCmd (void);
void CL_ClearState (void);
void CL_ReadPackets (void);

void CL_WritePacket( void );
void IN_CenterView (void);

extern kbutton_t in_cameralook;
extern qboolean  camera_reset;

void CL_VerifyCode( void );

float CL_KeyState (kbutton_t *key);
const char *Key_KeynumToString( int keynum );
const char *Key_KeynumToBindString( int keynum );
void Key_GetKeysForCommand( const char *command, int *key1, int *key2 );
int Key_GetCatcher( void );
void Key_SetCatcher( int catcher );

void CL_EyeInfo(usereyes_t *info);

extern int cl_connectedToPureServer;
extern int cl_connectedToCheatServer;

#ifdef USE_VOIP
#endif

void CL_SystemInfoChanged( void );
void CL_ParseServerMessage( msg_t *msg );


void	CL_ServerInfoPacket( netadr_t from, msg_t *msg );
void	CL_LocalServers_f( void );
void	CL_GlobalServers_f( void );
void	CL_FavoriteServers_f( void );
void	CL_Ping_f( void );
void	CL_SaveShot_f( void );
void	CL_Dialog_f( void );
void	CL_ServerRestarted( void );
qboolean CL_UpdateVisiblePings_f( int source );


void Con_DrawCharacter (int cx, int line, int num);

void Con_CheckResize (void);
void Con_Init (void);
void Con_Shutdown (void);
void Con_Clear_f (void);
void Con_ToggleConsole_f (void);
void Con_DrawNotify (void);
void Con_ClearNotify (void);
void Con_RunConsole (void);
void Con_DrawConsole (void);
void Con_PageUp( void );
void Con_PageDown( void );
void Con_Top( void );
void Con_Bottom( void );

void	SCR_Init (void);
void	SCR_DrawDebugGraph(void);
void	SCR_DrawScreenField(void);
void	SCR_UpdateScreen (void);
void	SCR_ResetUpdateGuard (void);

void	CL_ProcessLoadingScreen( const char *loadingStatus );
// to UI_UpdateLoadingStatus (0x001A1E30), which is not ported; kept so that a later
extern int cl_loadingScreenStage;

void	SCR_DebugGraph (float value);

void	SCR_AdjustFrom640( float *x, float *y, float *w, float *h );
void	SCR_AndroidDisplayRect( int surfaceW, int surfaceH, int *outX, int *outY, int *outW, int *outH );
void	SCR_DrawAndroidBars( void );
void	SCR_FillRect( float x, float y, float width, float height,
					 const float *color );
void	SCR_DrawPic( float x, float y, float width, float height, qhandle_t hShader );
void	SCR_DrawNamedPic( float x, float y, float width, float height, const char *picname );

void	UpdateStereoSide( stereoFrame_t s );


void CL_PlayCinematic_f( void );
void SCR_DrawCinematic (void);
void ShutdownCinematicSystem(void);
void InitializeCinematicSystem(void);
int  GetBackgroundMode(void);
void SetBackgroundMode(int mode);
void GetRenderingContext(void);
void StopLoadingScreen(void);
void StartLoadingScreen(void);
void StartCinematic(const char *pszName);
int CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits);
e_status CIN_StopCinematic(int handle);
e_status CIN_RunCinematic (int handle);
void CIN_DrawCinematic (int handle);
void CIN_SetExtents (int handle, int x, int y, int w, int h);
void CIN_SetLooping (int handle, qboolean loop);
void CIN_UploadCinematic(int handle);
void CIN_CloseAllVideos(void);

void CL_InitCGame( void );
void CL_InitCGameDLL( void );
void CL_ShutdownCGame( void );
qboolean CL_GameCommand( void );
void CL_CGameRendering( stereoFrame_t stereo );
void CL_CGame2D( stereoFrame_t stereo );
void CL_UpdateSnapFlags( void );
void CL_SetCGameTime( void );
void CL_FirstSnapshot( void );
void CL_ShaderStateChanged( void );
dshader_t *CL_GetShaderPointer( int iShaderNum );



void CL_Netchan_Transmit( netchan_t *chan, msg_t* msg);	
void CL_Netchan_TransmitNextFragment( netchan_t *chan );
qboolean CL_Netchan_Process( netchan_t *chan, msg_t *msg );
void CL_NET_OutOfBandPrint( netadr_t adr, const char* format, ... ) Q_PRINTF_FUNC(2, 3);

void CL_InitConsoleCommands( void );

#ifdef __cplusplus
}
#endif
