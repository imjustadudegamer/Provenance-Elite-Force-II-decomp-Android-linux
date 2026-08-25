
#pragma once

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../fgame/bg_public.h"
#include "../fgame/g_public.h"
#ifndef DEDICATED
#  include "../client/snd_local.h"
#endif


#define	PERS_SCORE				0		// !!! MUST NOT CHANGE, SERVER AND

#define	MAX_ENT_CLUSTERS	16

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_VOIP
#endif

typedef struct svEntity_s {
	struct worldSector_s *worldSector;
	struct svEntity_s *nextEntityInWorldSector;
	
	entityState_t	baseline;		
	int			numClusters;		
	int			clusternums[MAX_ENT_CLUSTERS];
	int			lastCluster;		
	int			areanum, areanum2;
	int			snapshotCounter;	
} svEntity_t;

// SS_LOADING2 is MOHAA's extra phase and does not exist in retail -- retail's
typedef enum {
	SS_DEAD,			
	SS_LOADING,			
	SS_GAME				
} serverState_t;

typedef struct {
	serverState_t	state;
	qboolean		restarting;			
	int				serverId;			
	int				restartedServerId;	
	int				checksumFeed;		
	int       checksumFeedServerId;	
	int				snapshotCounter;	
	int				timeResidual;		
	int				nextFrameTime;		
	float			frameTime;
	struct cmodel_s	*models[MAX_MODELS];
	char			*configstrings[MAX_CONFIGSTRINGS];
	svEntity_t		svEntities[MAX_GENTITIES];

	int				farplane;
	int				farplane_cull;
	qboolean		skyportal;

	worldState_t	worldState;

	char			*entityParsePoint;	

	gentity_t		*gentities;
	int				gentitySize;
	int				num_entities;		

	playerState_t	*gameClients;
	int				gameClientSize;		
} server_t;





#define	MAX_CLIENT_STRING_CHARS	1024

typedef struct {
	int				areabytes;						 
	byte			areabits[MAX_MAP_AREA_BYTES];	 
	playerState_t	ps;								 
	int				num_entities;					 
	int				first_entity;	 
	int				messageSent;	 
	int				messageAcked;	 
	int				messageSize;	 
} clientSnapshot_t;

#define	ALGN_UP(T, x)	(((x) + __alignof__(T) - 1) & ~(__alignof__(T) - 1))

static_assert(offsetof(clientSnapshot_t, areabits) == 4, "");
static_assert(offsetof(clientSnapshot_t, ps) >= 4 + MAX_MAP_AREA_BYTES, "");
static_assert(offsetof(clientSnapshot_t, num_entities)
		== offsetof(clientSnapshot_t, ps) + sizeof(playerState_t), "playerState_t must be the last thing before the bookkeeping ints");
static_assert(offsetof(clientSnapshot_t, first_entity) == offsetof(clientSnapshot_t, num_entities) + 4, "");
static_assert(offsetof(clientSnapshot_t, messageSent) == offsetof(clientSnapshot_t, first_entity) + 4, "");
static_assert(offsetof(clientSnapshot_t, messageAcked) == offsetof(clientSnapshot_t, messageSent) + 4, "");
static_assert(offsetof(clientSnapshot_t, messageSize) == offsetof(clientSnapshot_t, messageAcked) + 4, "");
static_assert(sizeof(clientSnapshot_t)
	== ALGN_UP(clientSnapshot_t, offsetof(clientSnapshot_t, messageSize) + 4), "messageSize must be clientSnapshot_t's last field");

typedef enum {
	CS_FREE,		
	CS_ZOMBIE,		
	CS_CONNECTED,	
	CS_PRIMED,		
	CS_ACTIVE		
} clientState_t;

typedef struct client_s {
	clientState_t	state;										
	char			userinfo[MAX_CLIENT_STRING_CHARS];			

	char			reliableCommands[MAX_RELIABLE_COMMANDS][MAX_CLIENT_STRING_CHARS];	
	int				reliableSequence;		
	int				reliableAcknowledge;	
	int				messageAcknowledge;		

	int				gamestateMessageNum;	
	int				challenge;				

	usercmd_t		lastUsercmd;			

	byte			unused_132180[4];		 

	int				lastClientCommand;		
	char			lastClientCommandString[MAX_CLIENT_STRING_CHARS];	
	gentity_t		*gentity;				
	char			name[MAX_NAME_LENGTH];	


	byte			unused_133248[12];		 

	int				deltaMessage;			
	int				nextReliableTime;		
	int				nextReliableTimeChat;	
	int				lastPacketTime;			
	int				lastConnectTime;		
	int				nextSnapshotTime;		
	int				nextFragmentTime;		
	qboolean		rateDelayed;			
	int				timeoutCount;			
	clientSnapshot_t	frames[PACKET_BACKUP];	
	int				ping;					
	int				rate;					
	int				snapshotMsec;			
	netchan_t		netchan;				

	int				gamestateTime;			

	server_sound_t server_sounds[ MAX_SERVER_SOUNDS ];	
	int number_of_server_sounds;			
	worldState_t ws;						
	qboolean locprint;						
	int XOffset;							
	int YOffset;							
	char stringToPrint[256];				
	int  currentImportance;					

	// 4,802-function Mac decompile -- retail stores them and never consumes them.
	int				dropNextFrame;			
	char			dropReason[MAX_CLIENT_STRING_CHARS];	

	int				pureAuthentic;			
} client_t;

// The port is x64, so raw retail offsets are not portable past the gentity pointer
#define	CLI_FIRST(a, b)		(offsetof(client_t, a) < offsetof(client_t, b))
#define	CLI_ADJACENT(a, b)	(offsetof(client_t, b) == offsetof(client_t, a) + sizeof(((client_t *)0)->a))

static_assert(CLI_ADJACENT(state, userinfo), "");
static_assert(sizeof(((client_t *)0)->userinfo) == 1024, "retail userinfo is 1024, not MAX_INFO_STRING");
static_assert(CLI_ADJACENT(userinfo, reliableCommands), "");
static_assert(sizeof(((client_t *)0)->reliableCommands) == MAX_RELIABLE_COMMANDS * 1024, "");
static_assert(offsetof(client_t, reliableSequence) == 132100, "the reliable ring must end where retail's does");
static_assert(offsetof(client_t, reliableAcknowledge) == 132104, "");
static_assert(offsetof(client_t, messageAcknowledge) == 132108, "");
static_assert(offsetof(client_t, gamestateMessageNum) == 132112, "");
static_assert(offsetof(client_t, challenge) == 132116, "");
static_assert(offsetof(client_t, lastUsercmd) == 132120, "retail +132120, a 60-byte usercmd_t");
static_assert(sizeof(usercmd_t) == 60, "");
static_assert(offsetof(client_t, unused_132180) == 132180, "");
static_assert(offsetof(client_t, lastClientCommand) == 132184, "");
static_assert(offsetof(client_t, lastClientCommandString) == 132188, "");
static_assert(sizeof(((client_t *)0)->lastClientCommandString) == 1024, "");
static_assert(offsetof(client_t, gentity) == ALGN_UP(client_t, 133212), "");

static_assert(CLI_ADJACENT(gentity, name), "");
static_assert(sizeof(((client_t *)0)->gentity) == sizeof(gentity_t *), "");
static_assert(sizeof(((client_t *)0)->name) == MAX_NAME_LENGTH, "");
static_assert(CLI_ADJACENT(name, unused_133248), "");
static_assert(sizeof(((client_t *)0)->unused_133248) == 12, "");
static_assert(CLI_ADJACENT(unused_133248, deltaMessage), "");

static_assert(CLI_ADJACENT(deltaMessage, nextReliableTime), "");
static_assert(CLI_ADJACENT(nextReliableTime, nextReliableTimeChat), "");
static_assert(CLI_ADJACENT(nextReliableTimeChat, lastPacketTime), "");
static_assert(CLI_ADJACENT(lastPacketTime, lastConnectTime), "");
static_assert(CLI_ADJACENT(lastConnectTime, nextSnapshotTime), "");
static_assert(CLI_ADJACENT(nextSnapshotTime, nextFragmentTime), "");
static_assert(CLI_ADJACENT(nextFragmentTime, rateDelayed), "");
static_assert(CLI_ADJACENT(rateDelayed, timeoutCount), "");
static_assert(CLI_ADJACENT(timeoutCount, frames), "");
static_assert(offsetof(client_t, ping) == offsetof(client_t, frames) + PACKET_BACKUP * sizeof(clientSnapshot_t), "");
static_assert(CLI_ADJACENT(ping, rate), "");
static_assert(CLI_ADJACENT(rate, snapshotMsec), "");
static_assert(CLI_ADJACENT(snapshotMsec, netchan), "");

static_assert(CLI_ADJACENT(netchan, gamestateTime), "");
static_assert(CLI_ADJACENT(gamestateTime, server_sounds), "");
static_assert(sizeof(((client_t *)0)->server_sounds) == MAX_SERVER_SOUNDS * sizeof(server_sound_t), "");
static_assert(CLI_ADJACENT(server_sounds, number_of_server_sounds), "");
static_assert(CLI_ADJACENT(number_of_server_sounds, ws), "");
static_assert(sizeof(worldState_t) == 92, "");
static_assert(CLI_ADJACENT(ws, locprint), "");
static_assert(CLI_ADJACENT(locprint, XOffset), "");
static_assert(CLI_ADJACENT(XOffset, YOffset), "");
static_assert(CLI_ADJACENT(YOffset, stringToPrint), "");
static_assert(sizeof(((client_t *)0)->stringToPrint) == 256, "");
static_assert(CLI_ADJACENT(stringToPrint, currentImportance), "");
static_assert(CLI_ADJACENT(currentImportance, dropNextFrame), "");
static_assert(CLI_ADJACENT(dropNextFrame, dropReason), "");
static_assert(sizeof(((client_t *)0)->dropReason) == 1024, "");
static_assert(CLI_ADJACENT(dropReason, pureAuthentic), "");
static_assert(sizeof(client_t)
	== ((offsetof(client_t, pureAuthentic) + sizeof(int) + __alignof__(client_t) - 1)
		& ~(__alignof__(client_t) - 1)), "pureAuthentic must be client_t's last field");



#define	MAX_CHALLENGES	1024

#define	AUTHORIZE_TIMEOUT	10000

typedef enum {
    CDKS_NONE,
    CDKS_AUTHENTICATING,
    CDKS_AUTHENTICATED,
    CDKS_FAILED
} cdKeyState_e;

typedef struct {
	netadr_t	adr;
	int			challenge;
	int			time;				
	int			pingTime;			
	int			firstTime;			
	int			firstPing;			
	qboolean	connected;

} challenge_t;

static_assert(sizeof(challenge_t) == 44, "challenge_t must stay the retail 44 bytes");

typedef struct {
	qboolean	initialized;				
	int			snapFlagServerBit;			
	int			time;						
	client_t	*clients;					
	int			numSnapshotEntities;		
	int			nextSnapshotEntities;		
	entityState_t	*snapshotEntities;		
	int			nextHeartbeatTime;
	challenge_t	challenges[MAX_CHALLENGES];	
	netadr_t	redirectAddress;			
#ifndef STANDALONE
	netadr_t	authorizeAddress;			
#endif
	char		gameName[64];
	char		mapName[64];
	char		rawServerName[64];
} serverStatic_t;



extern	serverStatic_t	svs;				
extern	server_t		sv;					
extern	game_export_t	*ge;				

extern	cvar_t	*sv_fps;
extern	cvar_t	*sv_timeout;
extern	cvar_t	*sv_zombietime;
extern	cvar_t	*sv_rconPassword;
extern	cvar_t	*sv_privatePassword;
extern	cvar_t	*sv_allowDownload;
extern	cvar_t	*sv_maxclients;

extern	cvar_t	*sv_privateClients;
extern	cvar_t	*sv_hostname;
extern	cvar_t	*sv_master[MAX_MASTER_SERVERS];
extern	cvar_t	*sv_reconnectlimit;
extern	cvar_t	*sv_padPackets;
extern	cvar_t	*sv_killserver;
extern	cvar_t	*sv_mapname;
extern	cvar_t	*sv_mapChecksum;
extern	cvar_t	*sv_serverid;
extern	cvar_t	*sv_maxRate;
extern	cvar_t	*g_gametype;
extern	cvar_t	*mp_gametype;
extern	cvar_t	*sv_pure;
extern	cvar_t	*sv_floodProtect;
extern	cvar_t	*sv_gamename;
extern	cvar_t	*sv_location;


#ifdef USE_VOIP
#endif




void QDECL SV_SendServerCommand( client_t *cl, const char *fmt, ...) Q_PRINTF_FUNC(2, 3);


void SV_AddOperatorCommands (void);
void SV_RemoveOperatorCommands (void);


void SV_MasterHeartbeat (const char *message);
void SV_MasterShutdown (void);
int SV_RateMsec(client_t *client, int messageSize);
void SV_DropClientNextFrame(client_t *client, const char *reason);


qboolean SV_ArchivePersistantFile( qboolean loading, qboolean sublevelTransition );
qboolean SV_ArchiveLevelFile( const char *levelExt, const char *serverExt, qboolean loading );
void SV_CopySublevelFiles( qboolean slotToLive );
void S_Save( fileHandle_t f );
void S_Load( fileHandle_t f );
qboolean SV_ArchiveServerFile( const char *serverExt, qboolean loading, qboolean autosave );
void SV_Loadgame_f( void );
void SV_SavegameFilename( const char *name, char *fileName, int length );
void SV_UpdateLoadSaveStatus( void );
qboolean SV_DoSaveGame(void);
void SV_SaveGame( const char *gamename, qboolean autosave );
void SV_Savegame_f( void );

void SV_ClearSvsTimeFixups( void );
void SV_FinishSvsTimeFixups( void );
void SV_AddSvsTimeFixup( int *piTime );
void SV_SetConfigstring( int index, const char *val );
char *SV_GetConfigstring( int index );
int SV_FindIndex( const char *name, int start, int max, qboolean create );
int SV_ModelIndex( const char *name );
int SV_SoundIndex( const char *name );
int SV_ImageIndex( const char *name );
int SV_ItemIndex( const char *name );
void SV_SetLightStyle( int index, const char *data );

int SV_GetLightingGroup( const char *group_name );
void SV_SetDynamicLight( int dynamic_light, float intensity );
void SV_SetDynamicLightDefault( int dynamic_light, float intensity );
void SV_SetWindDirection( vec3_t direction );
void SV_SetWindIntensity( float wind_intensity );
void SV_SetWeatherInfo( weather_t type, int intensity );
void SV_SetTimeScale( float time_scale );


#define CS_FAILEDCONDITION 1969

int SV_FailedCondition( const char *name );
qboolean SV_inPVSIgnorePortals( const vec3_t p1, const vec3_t p2 );
qboolean SV_FS_Exists( const char *filename );
int SV_GameGetNumFreeReliableServerCommands( int clientNum );
qboolean SV_IsClientActive( const gentity_t *ent );

void SV_SetUserinfo( int index, const char *val );
void SV_GetUserinfo( int index, char *buffer, int bufferSize );

int SV_NumClients(void);
void SV_ChangeMaxClients( void );
void SV_SpawnServer( const char *server, qboolean loadgame, qboolean restart,
					 qboolean sublevelLoad, qboolean areSublevels );
qboolean SV_DoneLoading( void );

void SV_DeleteTempSublevelFiles(void);
// game_import_t table.  It is NOT a game-module shutdown -- retail never unloads
void SV_ClearGameInterface( void );
void     SV_SaveSublevel( const char *sublevelName );
qboolean SV_LoadSublevel( const char *mapname );
qboolean SV_areSublevels( const char *oldMapName, const char *newMapName );
void SV_ParseMapName( const char *name, char *base, char *sublevel, char *rest );
void PF_GetLevelDefs( const char *name, const char **environmentName, const char **levelName,
					  const char **sublevelName );
void PF_centerprintf( const gentity_t *ent, CenterPrintImportance importance, const char *fmt, ... );
void SV_Loadbrokengame_f( void );
void SV_CopyArchiveFile( const char *src, const char *dst );
extern qboolean sv_bAutoSaveRequested;


challenge_t* FindExistingChallenge(netadr_t from);
void SV_GetChallenge(netadr_t from);

void SV_DirectConnect( netadr_t from );

void SV_AuthorizeIpPacket( netadr_t from );

void SV_ExecuteClientMessage( client_t *cl, msg_t *msg );
void SV_UserinfoChanged( client_t *cl );

void SV_ClientEnterWorld( client_t *client, usercmd_t *cmd );
void SV_FreeClient(client_t *client);
void SV_DropClient( client_t *drop, const char *reason, qboolean freeClient );

void SV_ExecuteClientCommand( client_t *cl, const char *s );  
void SV_ClientThink (client_t *cl, usercmd_t *cmd);

int SV_WriteDownloadToClient(client_t *cl , msg_t *msg);

// port-only, no Mac symbol, no import slot; retail SV_Kick_f sends no print).

unsigned int SV_Client_GetNumPendingCommands(client_t *cl);
unsigned int SV_Client_GetMaxPendingCommands(client_t *cl);


void SV_Heartbeat_f( void );

void SV_AddServerCommand( client_t *client, const char *cmd );
void SV_WriteFrameToClient (client_t *client, msg_t *msg);
int SV_SendMessageToClient( msg_t *msg, client_t *client );
void SV_SendClientMessages( void );
void SV_SendClientSnapshot( client_t *client );

extern debugline_t* DebugLines;
extern int numDebugLines;
extern debugstring_t* DebugStrings;
extern int numDebugStrings;

qboolean PF_IsModel( int index );
struct dtiki_s;
dtiki_t *TIKI_Handle_For_ModelIndex( int modelindex );
void PF_setviewmodel( gentity_t *ent, const char *name );
int	SV_NumForGentity( gentity_t *ent );
gentity_t *SV_GentityNum( int num );
playerState_t *SV_GameClientNum( int num );
svEntity_t	*SV_SvEntityForGentity( gentity_t *gEnt );
gentity_t *SV_GEntityForSvEntity( svEntity_t *svEnt );
void		SV_InitGameProgs ( void );
void		SV_ShutdownGameProgs ( void );
qboolean	SV_inPVS (const vec3_t p1, const vec3_t p2);
void SV_WriteCGMToClient (client_t *client, msg_t *msg);
void SV_InitAllCGMessages (void);

void SV_ClientSound(client_t* client, vec3_t* org, int entnum, int channel, int sound_index, float volume, float mindist, float pitch, float maxdist, qboolean streamed);
void SV_Sound( vec3_t *org, int entnum, int channel, const char *sound_name, float volume, float mindist, float pitch, float maxdist, qboolean streamed );
void SV_ClearSounds( client_t *client );
void SV_StopSound( int entnum, int channel );
float SV_SoundLength( const char *path );


void SV_ClearWorld (void);

void SV_UnlinkEntity( gentity_t *ent );

void SV_LinkEntity( gentity_t *ent );


clipHandle_t SV_ClipHandleForEntity( const gentity_t *ent );


void SV_SectorList_f( void );


int SV_AreaEntities( const vec3_t mins, const vec3_t maxs, int *entityList, int maxcount, qboolean fullTrace );

dshader_t *SV_GetShaderPointer( int iShaderNum );
int SV_PointContents( const vec3_t p, int passEntityNum );

void SV_Trace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask, qboolean cylinder );
void SV_FullTrace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask, qboolean cylinder );





void SV_ClipToEntity( trace_t *trace, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int entityNum, int contentmask );

int SV_Netchan_Transmit( client_t *client, msg_t *msg);
int SV_Netchan_TransmitNextFragment( client_t *client );
qboolean SV_Netchan_Process( client_t *client, msg_t *msg );
void SV_NET_OutOfBandPrint(netadr_t adr, const char* format, ...) Q_PRINTF_FUNC(2, 3);

#ifdef __cplusplus
}
#endif



void SV_BotInitCvars(void);
void SV_BotInitBotLib(void);
int SV_BotLibSetup(void);
int SV_BotLibShutdown(void);
int SV_BotLibVarSet(char *var_name, char *value);
int SV_BotLibVarGet(char *var_name, char *value, int size);
int SV_PC_AddGlobalDefine(char *string);
int SV_PC_LoadSourceHandle(const char *filename);
int SV_PC_FreeSourceHandle(int handle);
int SV_PC_SourceFileAndLine(int handle, char *filename, int *line);
int SV_BotLibStartFrame(float time);
void SV_BotFrame(int time);
extern cvar_t *bot_enable;	 
extern int sv_botEnable;	 
int SV_BotLibLoadMap(const char *mapname);
int SV_BotLibUpdateEntity(int ent, bot_entitystate_t *state);
int SV_BotLibTest(int parm0, char *parm1, vec3_t parm2, vec3_t parm3);
qboolean SV_BotLibReady(void);
int SV_BotAllocateClient(void);
void SV_GetServerinfo(char *buffer, int bufferSize);
int SV_BotGetConsoleMessage(int client, char *buf, int size);
int SV_BotGetSnapshotEntity(int client, int sequence);
void SV_BotUserCommand(int client, usercmd_t *cmd);

void QDECL BotImport_Print(int type, char *fmt, ...);
char *BotImport_BSPEntityData(void);
void *BotImport_HunkAlloc(int size);

int PF_PointBrushNum(const vec3_t point, int passEntityNum);
int SV_ObjectiveNameIndex(const char *name);
void PF_ReloadTiki(const char *modelstr);
void SV_FS_DeleteFile(const char *filename);
// stood here were never defined and have no retail symbol; they are gone.
const char *SurfaceTypeToName(unsigned int surfaceType);

void QDECL BotImport_Print(int type, char *fmt, ...);
int BotImport_PointContents(vec3_t point);
char *BotImport_BSPEntityData(void);
void BotImport_BSPModelMinsMaxsOrigin(int modelnum, vec3_t angles, vec3_t outmins, vec3_t outmaxs, vec3_t origin);
void BotClientCommand(int client, const char *command);
int BotImport_AvailableMemory(void);
void *BotImport_HunkAlloc(int size);
int BotImport_DebugLineCreate(void);
void BotImport_DebugLineDelete(int line);
void BotImport_DebugLineShow(int line, vec3_t start, vec3_t end, int color);
int BotImport_DebugPolygonCreate(int color, int numPoints, vec3_t *points);
void BotImport_DebugPolygonDelete(int id);
