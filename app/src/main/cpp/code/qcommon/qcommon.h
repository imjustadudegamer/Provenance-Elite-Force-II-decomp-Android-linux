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
#ifndef _QCOMMON_H_
#define _QCOMMON_H_

#include "cm_public.h"
#include "alias.h"

#ifdef __cplusplus
extern "C" {
#endif



typedef struct msg_s {
	qboolean	allowoverflow;	
	qboolean	overflowed;		
	qboolean	oob;			
	byte	*data;
	size_t	maxsize;
	size_t	cursize;
	int		readcount;
	int		bit;				
} msg_t;

void MSG_Init (msg_t *buf, byte *data, size_t length);
void MSG_InitOOB( msg_t *buf, byte *data, size_t length );
void MSG_Clear (msg_t *buf);
void MSG_WriteData (msg_t *buf, const void *data, size_t length);
void MSG_Bitstream(msg_t* buf);

void MSG_Copy(msg_t *buf, byte *data, int length, msg_t *src);

struct usercmd_s;
struct entityState_s;
struct playerState_s;

void MSG_WriteBits( msg_t *msg, int value, int bits );

void MSG_WriteChar (msg_t *sb, int c);
void MSG_WriteByte (msg_t *sb, int c);
void MSG_WriteShort (msg_t *sb, int c);

void MSG_WriteSVC( msg_t *sb, int c );

void MSG_WriteLong (msg_t *sb, int c);
void MSG_WriteFloat (msg_t *sb, float f);
void MSG_WriteString (msg_t *sb, const char *s);
void MSG_WriteBigString (msg_t *sb, const char *s);
void MSG_WriteAngle16 (msg_t *sb, float f);
int MSG_HashKey(const char *string, int maxlen);
void MSG_WriteEntityNum(msg_t* sb, short number);

void	MSG_BeginReading (msg_t *sb);
void	MSG_BeginReadingOOB(msg_t *sb);

int		MSG_ReadBits( msg_t *msg, int bits );

int		MSG_ReadChar (msg_t *sb);
int		MSG_ReadByte (msg_t *sb);
int		MSG_ReadSVC (msg_t *sb);
int		MSG_ReadShort (msg_t *sb);
int		MSG_ReadLong (msg_t *sb);

void MSG_ReadDir( msg_t *msg, vec3_t dir );
float MSG_ReadCoord( msg_t *msg );
void MSG_GetNullEntityState(entityState_t *nullState);

float	MSG_ReadFloat (msg_t *sb);
char	*MSG_ReadString (msg_t *sb);
char	*MSG_ReadBigString (msg_t *sb);
char	*MSG_ReadStringLine( msg_t *sb );
float	MSG_ReadAngle8( msg_t *sb );
float	MSG_ReadAngle16 (msg_t *sb);
void	MSG_ReadData (msg_t *sb, void *buffer, int size);
unsigned short MSG_ReadEntityNum(msg_t* sb);


void MSG_WriteDeltaUsercmdKey( msg_t *msg, int key, usercmd_t *from, usercmd_t *to, qboolean extended );
void MSG_ReadDeltaUsercmdKey( msg_t *msg, int key, usercmd_t *from, usercmd_t *to, qboolean extended );

void MSG_WriteDeltaEntity( msg_t *msg, struct entityState_s *from, struct entityState_s *to,
						   qboolean force);

void MSG_ReadSounds (msg_t *msg, server_sound_t *sounds, int *snapshot_number_of_sounds);
void MSG_WriteSounds (msg_t *msg, server_sound_t *sounds, int snapshot_number_of_sounds);
void MSG_WriteWorldInfo (msg_t *msg, worldState_t *from, worldState_t *to);
void MSG_ReadWorldInfo (msg_t *msg, worldState_t *from, worldState_t *to);

void MSG_ReadDeltaEntity(msg_t* msg, entityState_t* from, entityState_t* to,
	int number);
void MSG_SetupClientEntity(entityState_t* to);

void MSG_WriteDeltaPlayerstate(msg_t* msg, struct playerState_s* from, struct playerState_s* to);
void MSG_ReadDeltaPlayerstate(msg_t* msg, struct playerState_s* from, struct playerState_s* to);


void MSG_ReportChangeVectors_f( void );
void MSG_PlayerReportChangeVectors_f( void );





#define	PACKET_BACKUP	32	
#define	PACKET_MASK		(PACKET_BACKUP-1)

#define	MAX_PACKET_USERCMDS		32		

#define	PORT_ANY			-1

#define	MAX_RELIABLE_COMMANDS	128			

typedef enum {
	NA_BOT,					 
	NA_BAD,					 
	NA_LOOPBACK,			 
	NA_BROADCAST,			 
	NA_IP,					 
	NA_IPX,					 
	NA_BROADCAST_IPX		 
} netadrtype_t;

typedef enum {
	NS_CLIENT,
	NS_SERVER
} netsrc_t;

#define NET_ADDRSTRMAXLEN 64

typedef struct {
	netadrtype_t	type;

	byte	ip[4];
	byte	ipx[10];

	unsigned short	port;
} netadr_t;

static_assert(sizeof(netadr_t) == 20, "netadr_t must stay the retail 20 bytes");
static_assert(offsetof(netadr_t, ip) == 4, "");
static_assert(offsetof(netadr_t, ipx) == 8, "");
static_assert(offsetof(netadr_t, port) == 18, "");

void		NET_Init( void );
void		NET_Shutdown( void );
void		NET_Restart_f( void );
void		NET_Config( qboolean enableNetworking );
void		NET_SendPacket (netsrc_t sock, size_t length, const void *data, netadr_t to);
void		QDECL NET_OutOfBandPrint( netsrc_t net_socket, netadr_t adr, const char *format, ...) Q_PRINTF_FUNC(3, 4);
void		QDECL NET_OutOfBandData( netsrc_t sock, netadr_t adr, byte *format, int len );

qboolean	NET_CompareAdr (netadr_t a, netadr_t b);
qboolean	NET_CompareBaseAdrMask(netadr_t a, netadr_t b, int netmask);
qboolean	NET_CompareBaseAdr (netadr_t a, netadr_t b);
qboolean	NET_IsLocalAddress (netadr_t adr);
const char	*NET_AdrToString (netadr_t a);
const char	*NET_AdrToStringwPort (netadr_t a);
const char	*NET_AdrToLogString (netadr_t a);
int		NET_StringToAdr ( const char *s, netadr_t *a);
qboolean	NET_GetLoopPacket (netsrc_t sock, netadr_t *net_from, msg_t *net_message);
void		NET_Sleep(int msec);


#define	MAX_MSGLEN				49152		

#define NETCHAN_GENCHECKSUM(challenge, sequence) ((challenge) ^ ((sequence) * (challenge)))
 


typedef struct {
	netsrc_t	sock;

	int			dropped;			

	netadr_t	remoteAddress;
	int			qport;				

	int			incomingSequence;
	int			outgoingSequence;

	int			fragmentSequence;
	int			fragmentLength;	
	byte		fragmentBuffer[MAX_MSGLEN];

	// The three unsent* fields are retail 32-bit ints, not size_t: Netchan_Setup
	qboolean	unsentFragments;
	int			unsentFragmentStart;
	int			unsentLength;
	byte		unsentBuffer[MAX_MSGLEN];

} netchan_t;

static_assert(sizeof(netchan_t) == 98364, "netchan_t must stay the retail 98364 bytes");
static_assert(offsetof(netchan_t, remoteAddress) == 8, "");
static_assert(offsetof(netchan_t, qport) == 28, "");
static_assert(offsetof(netchan_t, outgoingSequence) == 36, "");
static_assert(offsetof(netchan_t, unsentFragments) == 49200, "");
static_assert(offsetof(netchan_t, unsentFragmentStart) == 49204, "");
static_assert(offsetof(netchan_t, unsentLength) == 49208, "");
static_assert(offsetof(netchan_t, unsentBuffer) == 49212, "");

void Netchan_Init( int qport );
void Netchan_Setup(netsrc_t sock, netchan_t *chan, netadr_t adr, int qport);

int Netchan_Transmit( netchan_t *chan, size_t length, const byte *data );
int Netchan_TransmitNextFragment( netchan_t *chan );

qboolean Netchan_Process( netchan_t *chan, msg_t *msg );


#define	PROTOCOL_VERSION			66


#define	UPDATE_SERVER_NAME	"updatemoh.2015.com"
#ifndef MASTER_SERVER_NAME
#define MASTER_SERVER_NAME	"master.2015.com"
#endif
#ifndef AUTHORIZE_SERVER_NAME
#define	AUTHORIZE_SERVER_NAME	"ef2auth.activision.com"
#endif

#define	PORT_MASTER			27950
#define	PORT_UPDATE			27951
#ifndef PORT_AUTHORIZE
#define	PORT_AUTHORIZE		29252
#endif
#define	PORT_SERVER			12203
#define	NUM_SERVER_PORTS	4		


enum svc_ops_e {
	svc_bad,
	svc_nop,
	svc_gamestate,
	svc_configstring,			
	svc_baseline,				
	svc_serverCommand,			
	svc_download,				
	svc_snapshot,
	svc_centerprint,
	svc_locprint,
	svc_EOF,

	svc_voipSpeex,     
	svc_voipOpus,      
};

enum clc_ops_e {
	clc_bad,
	clc_nop, 		
	clc_move,				
	clc_moveNoDelta,		
	clc_clientCommand,		
	clc_EOF,

	clc_voipSpeex,   
	clc_voipOpus,    
};


typedef struct vm_s vm_t;

typedef enum {
	VMI_NATIVE,
	VMI_BYTECODE,
	VMI_COMPILED
} vmInterpret_t;

typedef enum {
	TRAP_MEMSET = 100,
	TRAP_MEMCPY,
	TRAP_STRNCPY,
	TRAP_SIN,
	TRAP_COS,
	TRAP_ATAN2,
	TRAP_SQRT,
	TRAP_MATRIXMULTIPLY,
	TRAP_ANGLEVECTORS,
	TRAP_PERPENDICULARVECTOR,
	TRAP_FLOOR,
	TRAP_CEIL,

	TRAP_TESTPRINTINT,
	TRAP_TESTPRINTFLOAT
} sharedTraps_t;

typedef intptr_t (QDECL *vmMainProc)(int callNum, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11);

void	VM_Init( void );
vm_t	*VM_Create( const char *module, intptr_t (*systemCalls)(intptr_t *), 
				   vmInterpret_t interpret );

void	VM_Free( vm_t *vm );
void	VM_Clear(void);
void	VM_Forced_Unload_Start(void);
void	VM_Forced_Unload_Done(void);
vm_t	*VM_Restart(vm_t *vm, qboolean unpure);

intptr_t		QDECL VM_Call( vm_t *vm, int callNum, ... );

void	VM_Debug( int level );

void	*VM_ArgPtr( intptr_t intValue );
void	*VM_ExplicitArgPtr( vm_t *vm, intptr_t intValue );

#define	VMA(x) VM_ArgPtr(args[x])
static ID_INLINE float _vmf(intptr_t x)
{
    floatint_t fi;
    fi.i = (int)x;
    return fi.f;
}
#define	VMF(x)	_vmf(args[x])




void Cbuf_Init (void);

void Cbuf_AddText( const char *text );

void Cbuf_ExecuteText(int exec_when, const char *text );

void Cbuf_Execute (int msec);

void Cmd_WriteAliases( fileHandle_t f );



typedef void (*xcommand_t) (void);

void	Cmd_Init (void);

void	Cmd_AddCommand( const char *cmd_name, xcommand_t function );

void	Cmd_RemoveCommand( const char *cmd_name );

void	Cmd_RemoveCommandSafe( const char *cmd_name );

void	Cmd_CommandCompletion( void(*callback)(const char *s) );
const char* Cmd_CompleteCommandByNumber(const char* partial, int number);

int		Cmd_Argc (void);
char	*Cmd_Argv (int arg);
void	Cmd_ArgvBuffer( int arg, char *buffer, int bufferLength );
char	*Cmd_Args (void);
char	*Cmd_ArgsFrom( int arg );
void	Cmd_ArgsBuffer( char *buffer, int bufferLength );
char	*Cmd_Cmd (void);
void	Cmd_Args_Sanitize( void );

void	Cmd_TokenizeString( const char *text );
void	Cmd_TokenizeStringIgnoreQuotes( const char *text_in );

void	Cmd_ExecuteString( const char *text );
void	Cmd_Alias( const char *alias, const char *cmd );




void Cvar_SetDefault( cvar_t *var, const char *var_value );

cvar_t *Cvar_Get( const char *var_name, const char *value, int flags );

void	Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );

void	Cvar_Update( vmCvar_t *vmCvar );

void 	Cvar_Set( const char *var_name, const char *value );

cvar_t	*Cvar_Set2( const char *var_name, const char *value, qboolean force );

cvar_t	*Cvar_Next( cvar_t *var );

void Cvar_SetLatched( const char *var_name, const char *value);

void	Cvar_SetValue( const char *var_name, float value );

cvar_t	*Cvar_FindVar( const char *var_name );
float	Cvar_VariableValue( const char *var_name );
int		Cvar_VariableIntegerValue(const char* var_name);
float	Cvar_VariableFloatValue(const char* var_name);

char	*Cvar_VariableString( const char *var_name );
void	Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );

const char *Cvar_CompleteVariable( const char *partial );
const char *Cvar_CompleteVariableByNumber( const char *partial, int number );

int	Cvar_Flags(const char *var_name);

void	Cvar_CommandCompletion( void(*callback)(const char *s) );

void 	Cvar_Reset( const char *var_name );
void 	Cvar_ForceReset(const char *var_name);

void	Cvar_SetCheatState( void );

qboolean Cvar_Command( void );
void	Cvar_SetD_f( void );

void 	Cvar_WriteVariables( fileHandle_t f );

void	Cvar_Init( void );

char	*Cvar_InfoString( int bit );
char	*Cvar_InfoString_Big( int bit );
void	Cvar_InfoStringBuffer( int bit, char *buff, int buffsize );
void Cvar_CheckRange( cvar_t *cv, float minVal, float maxVal, qboolean shouldBeIntegral );
void Cvar_SetDescription( cvar_t *var, const char *var_description );

void	Cvar_Restart(qboolean unsetVM);
void	Cvar_Restart_f( void );
void	Cvar_SaveGameRestart_f( void );

void	Cvar_SetH_f( void );
void	Cvar_SetL_f( void );

cvar_t  *Cvar_Unset(cvar_t *cv);

extern	int			cvar_modifiedFlags;
extern	qboolean	cvar_global_force;


#define FS_GENERAL_REF	0x01
#define NUM_ID_PAKS		5

#define	MAX_FILE_HANDLES	9    

#ifdef DEDICATED
#	define EF2_CONFIG_FILE "config_server.cfg"
#else
#	define EF2_CONFIG_FILE "config.cfg"
#endif
#define Q3CONFIG_CFG EF2_CONFIG_FILE

qboolean FS_Initialized( void );

void	FS_InitFilesystem( void );
void	FS_Shutdown( qboolean closemfp );

qboolean	FS_ConditionalRestart( int checksumFeed, qboolean disconnect );
void	FS_Restart( int checksumFeed );

char	**FS_ListFilteredFiles( const char *path, const char *extension, const char *filter, qboolean wantSubs, int *numfiles, qboolean allowNonPureFilesOnDisk );
char	**FS_ListFiles( const char *directory, const char *extension, qboolean wantSubs, int *numfiles );

void	FS_FreeFileList( char **list );

qboolean FS_FileExists_HomeConfig( const char *file );
qboolean FS_FileExists_HomeData( const char *file );
qboolean FS_FileExists_HomeState( const char *file );

void FS_CorrectCase(char* path);
qboolean FS_CreatePath (const char *OSPath);

char	*FS_BaseDir_BuildOSPath( const char *base, const char *qpath );
char	*FS_BuildOSPath( const char *base, const char *game, const char *qpath );
void	 FS_CopyFile( const char *from, const char *to );    

int		FS_LoadStack( void );

int		FS_GetFileList(  const char *path, const char *extension, char *listbuf, int bufsize );
int		FS_GetModList(  char *listbuf, int bufsize );

void	FS_GetModDescription(const char* modDir, char* description, int descriptionLen);

fileHandle_t	FS_FOpenFileWrite_HomeConfig( const char *filename );
fileHandle_t	FS_FOpenFileWrite_HomeData( const char *filename );
fileHandle_t	FS_FOpenFileWrite_HomeState( const char *filename );
fileHandle_t	FS_FOpenFileAppend_HomeData( const char *filename );
fileHandle_t	FS_FOpenTextFileWrite_HomeConfig(const char* qpath);
fileHandle_t	FS_FOpenTextFileWrite_HomeData(const char* qpath);
fileHandle_t	FS_FOpenTextFileWrite_HomeState(const char* qpath);

fileHandle_t FS_BaseDir_FOpenFileWrite_HomeConfig( const char *filename );
fileHandle_t FS_BaseDir_FOpenFileWrite_HomeData( const char *filename );
fileHandle_t FS_BaseDir_FOpenFileWrite_HomeState( const char *filename );
long		FS_BaseDir_FOpenFileRead( const char *filename, fileHandle_t *fp );
void	FS_BaseDir_Rename_HomeData( const char *from, const char *to, qboolean safe );
void	FS_CanonicalFilename( char *filename );

long	FS_FOpenFileRead(const char* filename, fileHandle_t* file, qboolean uniqueFILE, qboolean quiet);

int		FS_FileIsInPAK(const char *filename, int *pChecksum );

size_t	FS_Write( const void *buffer, size_t len, fileHandle_t f );

size_t	FS_Read( void *buffer, size_t len, fileHandle_t f );

void	FS_FCloseFile( fileHandle_t f );

long		FS_ReadFileEx(const char* qpath, void** buffer, qboolean quiet);
void		FS_GetDirectoryNames(const char* dirname, void* buf, int len);
long		FS_ReadFile(const char* qpath, void** buffer);
qboolean	FS_Exists( const char *qpath );
const char	*FS_Gamedir( void );
void		Com_LocalizeFilePath( const char *path, char *localizedPath );

void	FS_ForceFlush( fileHandle_t f );

void	FS_FreeFile( void *buffer );


const char	*FS_PrepFileWrite( const char *filename );

int     FS_WriteFile( const char *qpath, const void *buffer, int size );

void	FS_WriteTextFile( const char *qpath, const void *buffer, int size );

long FS_filelength(fileHandle_t f);

int		FS_FTell( fileHandle_t f );

void	FS_Flush( fileHandle_t f );

void 	QDECL FS_Printf( fileHandle_t f, const char *fmt, ... ) Q_PRINTF_FUNC(2, 3);

int		FS_FOpenFileByMode( const char *qpath, fileHandle_t *f, fsMode_t mode );

int		FS_Seek( fileHandle_t f, long offset, int origin );

qboolean FS_FilenameCompare( const char *s1, const char *s2 );
qboolean FS_FileNewer( const char *source, const char *destination );

const char *FS_LoadedPakNames( void );
const char *FS_LoadedPakChecksums( void );
const char *FS_LoadedPakPureChecksums( void );

const char *FS_ReferencedPakPureChecksums( void );


void FS_PureServerSetReferencedPaks( const char *pakSums, const char *pakNames );
void FS_PureServerSetLoadedPaks( const char *pakSums, const char *pakNames );

qboolean FS_CheckDirTraversal(const char *checkdir);
qboolean FS_idPak(const char *pak, const char *base);
qboolean FS_ComparePaks( char *neededpaks, int len, qboolean dlstring );

void FS_Remove( const char *osPath );
void FS_Remove_HomeData( const char *homePath );

void	FS_FilenameCompletion( const char *dir, const char *ext,
		qboolean stripExt, void(*callback)(const char *s), qboolean allowNonPureFilesOnDisk );

const char* FS_GetCurrentGameDir(void);
void	FS_GetRelativeFilename( const char *currentDirectory, const char *absoluteFilename, char *out, size_t destlen );

void FS_FileTime(const char* filename, char* date, char* size);

void FS_CorrectCase(char* path);

extern cvar_t* fs_debug;
extern cvar_t* fs_mapdir;


#define	MAX_EDIT_LINE	256
typedef struct {
	int		cursor;
	int		scroll;
	int		widthInChars;
	char	buffer[MAX_EDIT_LINE];
} field_t;

void Field_Clear( field_t *edit );
void Field_AutoComplete( field_t *edit );
void Field_CompleteKeyname( void );
void Field_CompleteFilename( const char *dir,
		const char *ext, qboolean stripExt, qboolean allowNonPureFilesOnDisk );
void Field_CompleteCommand( char *cmd,
		qboolean doCommands, qboolean doCvars );
void Field_CompletePlayerName( const char **names, int count );

#define CPUID_GENERIC			0			

#define CPUID_AXP				0x10

#define CPUID_INTEL_UNSUPPORTED	0x20			
#define CPUID_INTEL_PENTIUM		0x21			
#define CPUID_INTEL_MMX			0x22			
#define CPUID_INTEL_KATMAI		0x23			

#define CPUID_AMD_3DNOW			0x30			


#ifdef _WIN32
#define Q_vsnprintf _vsnprintf
#define Q_snprintf _snprintf
#else
#define Q_vsnprintf vsnprintf
#define Q_snprintf snprintf
#endif

extern char cl_cdkey[34];

typedef enum
{
  CF_RDTSC      = 1 << 0,
  CF_MMX        = 1 << 1,
  CF_MMX_EXT    = 1 << 2,
  CF_3DNOW      = 1 << 3,
  CF_3DNOW_EXT  = 1 << 4,
  CF_SSE        = 1 << 5,
  CF_SSE2       = 1 << 6,
  CF_ALTIVEC    = 1 << 7
} cpuFeatures_t;

#define	MAXPRINTMSG	4096


#define	MAX_RENDER_STRINGS			8
#define	MAX_RENDER_STRING_LENGTH	32

char		*CopyString( const char *in );
void		Info_Print( const char *s );

void		Com_BeginRedirect (char *buffer, int buffersize, void (*flush)(char *));
void		Com_EndRedirect( void );
void 		QDECL Com_Printf( const char *fmt, ... ) Q_PRINTF_FUNC(1, 2);
void 		QDECL Com_WPrintf( const char *fmt, ... ) Q_PRINTF_FUNC(1, 2);
void 		QDECL Com_WDPrintf( const char *fmt, ... ) Q_PRINTF_FUNC(1, 2);

qboolean Com_DoneLoading( void );
qboolean CL_DoneLoading( void );
qboolean SV_DoneLoading( void );   
void 		QDECL Com_DPrintf( const char *fmt, ... ) Q_PRINTF_FUNC(1, 2);
void 		QDECL Com_Error( int code, const char *fmt, ... ) Q_NO_RETURN Q_PRINTF_FUNC(2, 3);
void 		Com_Quit_f( void ) Q_NO_RETURN;

int			Com_Milliseconds( void );	
unsigned	Com_BlockChecksum( const void *buffer, int length );
unsigned 	Com_BlockChecksumKey (const void *buffer, int length, int key);
char		*Com_MD5File(const char *filename, int length, const char *prefix, int prefix_len);
int			Com_HashKey(char *string, int maxlen);
int			Com_Filter(const char *filter, const char *name, int casesensitive);
int			Com_FilterPath(const char *filter, const char *name, int casesensitive);
int			Com_RealTime(qtime_t *qtime);
void		Com_RunAndTimeServerPacket(netadr_t *evFrom, msg_t *buf);

qboolean	Com_IsVoipTarget(uint8_t *voipTargets, int voipTargetsSize, int clientNum);

void		Com_StartupVariable( const char *match );
qboolean		Com_PlayerNameToFieldString( char *str, int length, const char *name );
qboolean		Com_FieldStringToPlayerName( char *name, int length, const char *rawname );
int QDECL	Com_strCompare( const void *a, const void *b );


extern	float currentfps;
extern	cvar_t	*paused;
extern	cvar_t	*config;
extern	cvar_t	*fps;
extern	cvar_t	*developer;
extern	cvar_t	*com_dedicated;
extern	cvar_t	*com_speeds;
extern	cvar_t	*com_timescale;
extern	cvar_t	*com_sv_running;
extern	cvar_t	*com_cl_running;
extern	cvar_t	*com_viewlog;			
extern	cvar_t	*com_shortversion;
extern	cvar_t	*com_version;
extern	cvar_t	*deathmatch;
extern	cvar_t	*com_blood;
extern	cvar_t	*com_buildScript;		
extern	cvar_t	*com_passthrough;		
extern	cvar_t	*com_journal;
extern	cvar_t	*com_cameraMode;
extern	cvar_t	*com_homepath;

extern	cvar_t	*sv_paused;

extern	cvar_t* com_updatecheck_enabled;
extern	cvar_t* com_updatecheck_interval;

extern	int		time_game;
extern	int		time_frontend;
extern	int		time_backend;		

#define SMP_ZONE_AUDIO		0
#define SMP_ZONE_SLEEP		1
#define SMP_ZONE_SWAP		2

void	SMP_BudgetAdd( int zone, int msec );

#define SMP_BUDGET_ZONES	11
qboolean SMP_BudgetLastWindow( int *framesOut, int *mean10Out );

#define SMP_LZ_spawn		0
#define SMP_LZ_cmap		1
#define SMP_LZ_hunkclr		2
#define SMP_LZ_svinit		3
#define SMP_LZ_spawnent	4
#define SMP_LZ_clmap		5
#define SMP_LZ_hunkusr		6
#define SMP_LZ_rendreg		7
#define SMP_LZ_initcg		8
#define SMP_LZ_cginit		9
#define SMP_LZ_fs		10
#define SMP_LZ_sndinit		11
#define SMP_LZ_scr		12
#define SMP_LZ_cgame		13
#define SMP_LZ_ui		14
#define SMP_LZ_COUNT		15

extern	int		smp_loadOn;
extern	int		smp_loadInClient;

void	SMP_LoadAdd( int zone, int msec, int depth );

#define SMP_LOAD_BEGIN(z) \
	int smp_ld_##z = ( smp_loadOn ? smp_loadDepth++ : 0 ); \
	int smp_lt_##z = ( smp_loadOn ? Sys_Milliseconds() : 0 )
#define SMP_LOAD_END(z) \
	do { \
		if ( smp_loadOn ) { \
			smp_loadDepth--; \
			SMP_LoadAdd( SMP_LZ_##z, Sys_Milliseconds() - smp_lt_##z, smp_ld_##z ); \
		} \
	} while ( 0 )

extern	int		smp_loadDepth;

extern	int		com_frameTime;
extern	int		com_frameMsec;

extern	qboolean	com_errorEntered;
extern	qboolean	com_fullyInitialized;
extern	qboolean	com_firstConfig;

extern	fileHandle_t	com_journalFile;
extern	fileHandle_t	com_journalDataFile;

typedef enum {
	TAG_FREE,
	TAG_CONST,
	TAG_BOTLIB,
	TAG_RENDERER,				
	TAG_GAME,					
	TAG_CGAME,					
	TAG_CLIENT,					
	TAG_TIKI,					
	TAG_STRINGS_AND_COMMANDS,
	TAG_SOUND,					
	TAG_STATIC,
	TAG_STATIC_RENDERER,
	TAG_TEMP,					
	TAG_NUM_TOTAL_TAGS
} memtag_t;


#if defined(_DEBUG) && !defined(BSPC)
	#define ZONE_DEBUG
#endif

#ifdef ZONE_DEBUG
#define Z_TagMalloc(size, tag)			Z_TagMallocDebug(size, tag, #size, __FILE__, __LINE__)
#define Z_Malloc(size)					Z_MallocDebug(size, #size, __FILE__, __LINE__)
#define S_Malloc(size)					S_MallocDebug(size, #size, __FILE__, __LINE__)
void *Z_TagMallocDebug( int size, int tag, const char *label, const char *file, int line );	
void *Z_MallocDebug( int size, const char *label, const char *file, int line );			
void *S_MallocDebug( int size, const char *label, const char *file, int line );			
#else
void *Z_TagMalloc( int size, int tag );	
void *Z_Malloc( int size );			
void *S_Malloc( int size );			
#endif
void Z_Free( void *ptr );
void Z_FreeTags( int tag );
int Z_AvailableMemory( void );
void Z_LogHeap( void );

void Z_ChurnGet( long long *allocs, long long *frees, long long *bytes );
void Z_ChurnReset( void );
void Z_ChurnMeterFrame( void );

void Hunk_Clear( void );
void Hunk_SetClearNotify( void (*fn)( void ) );
void Hunk_ClearToMark( void );
void Hunk_SetMark( void );
qboolean Hunk_CheckMark( void );
void Hunk_ClearTempMemory( void );
void *Hunk_AllocateTempMemory( int size );
void Hunk_FreeTempMemory( void *buf );
int	Hunk_MemoryRemaining( void );
void Hunk_Log( void);

void Com_TouchMemory( void );

const char* Z_EmptyStringPointer(void);
const char* Z_NumberStringPointer(int iNum);

void Com_Init( char *commandLine );
void Com_Frame( void );
void Com_Shutdown( void );

qboolean Com_SanitizeName( const char *pszOldName, char *pszNewName, size_t bufferSize );
const char *Com_GetArchiveFileName( const char *gameName, const char *filename, const char *extension, qboolean tempFile );
const char *Com_GetArchiveFolder( void );
const char *Com_GetArchiveSlotFolder( const char *gameName );
void Com_WipeSavegame( const char *savename, qboolean deep );
void Com_DeleteSublevelFiles( const char *gameName );
qboolean Com_ShiftedStrStr(const char* shifted, const char* name, int offset);
qboolean COM_IsMapValid(const char* name);



void CL_InitKeyCommands( void );

void CL_Init( void );
void CL_Disconnect(qboolean showMenu);
void CL_AbnormalDisconnect( const char *message );	 
void CL_Shutdown( void );
void StopLoadingScreen(void);
void CL_SetFrameNumber(int frameNumber);
void CL_Frame( int msec );
qboolean CL_GameCommand( void );
void CL_KeyEvent (int key, qboolean down, unsigned time);

void CL_Key_ForceCommand(int key, unsigned int time);

void CL_CharEvent( int key );

void CL_GetMouseState( int *x, int *y, unsigned int *buttons );

void CL_SetMousePos( int x, int y );

void CL_MouseEvent( int dx, int dy, int time );

void CL_ClearButtons( void );

void CL_JoystickEvent( int axis, int value, int time );

void CL_PacketEvent( netadr_t from, msg_t *msg );

void CL_MapLoading( qboolean flush, const char *pszMapName, const char *pszCinematic, int stage );
void CL_MapLoadingClientOnly( const char *pszMapName );

void	CL_ForwardCommandToServer( const char *string );

qboolean CL_UseLargeLightmap(const char* mapName);

void CL_CDDialog( void );

void CL_FlushMemory( void );

void CL_ShutdownAll(qboolean shutdownRef);

void CL_InitRef(void);

void CL_StartHunkUsers( qboolean rendererOnly );

void CL_Snd_Shutdown(void);

void Key_KeynameCompletion( void(*callback)(const char *s) );

void Key_WriteBindings( fileHandle_t f );

void S_ClearSoundBuffer( void );

float *get_camera_offset( qboolean *lookactive, qboolean *resetview );

#define AVI_LINE_PADDING 4

void SV_Init( void );
void SV_Shutdown( const char *finalmsg );
void LoadViewModes( void );
void SV_Frame( int msec );
void SV_PacketEvent( netadr_t from, msg_t *msg );
int SV_FrameMsec(void);
qboolean SV_GameCommand(void);

void IN_Init(void *windowData);
void Sys_SendKeyEvents(void);
void IN_Shutdown(void);
void IN_Restart(void);
void IN_GetMousePosition(int *x, int *y);

void IN_FreeCursor(void);
qboolean IN_IsCursorActive(void);
/* True where a pointer device should put the software cursor on screen.  On
   Android that is only after a genuine (non-touch, non-gamepad) mouse event;
   elsewhere it is true, so the desktop cursor is exactly retail's. */
qboolean IN_HasRealMouse(void);
void     IN_NoteRealMouse(void);

/* The Android display density in dpi, or 0 where the query is unavailable.
   Declared for every platform so callers need no #ifdef. */
int Sys_AndroidDensityDpi(void);

void Com_Pause(void);
void Com_Unpause(void);
void Com_FakePause(void);
void Com_FakeUnpause(void);
void Com_Pause_f(void);
void Com_UnPause_f(void);

int R_CountTextureMemory(void);

typedef enum {
	AXIS_SIDE,
	AXIS_FORWARD,
	AXIS_UP,
	AXIS_ROLL,
	AXIS_YAW,
	AXIS_PITCH,
	MAX_JOYSTICK_AXIS
} joystickAxis_t;

typedef enum {
	SE_NONE = 0,	
	SE_KEY,		
	SE_CHAR,	
	SE_MOUSE,	
	SE_JOYSTICK_AXIS,	
	SE_CONSOLE,	
	SE_PACKET	
} sysEventType_t;

typedef struct {
	int				evTime;
	sysEventType_t	evType;
	int				evValue, evValue2;
	size_t			evPtrLength;	
	void			*evPtr;			
} sysEvent_t;

sysEvent_t	Sys_GetEvent(void);

void		Com_QueueEvent( int time, sysEventType_t type, int value, int value2, size_t ptrLength, void *ptr );
int			Com_EventLoop( void );
sysEvent_t	Com_GetSystemEvent( void );

void	Sys_Init (void);

void	Sys_UnloadDll( void *dllHandle );

qboolean Sys_DllExtension(const char* name);

void	Sys_UnloadGame( void );
void	*Sys_GetGameAPI( void *parms );

void	Sys_UnloadCGame( void );
void	*Sys_GetCGameAPI( void *parms );

void	Sys_UnloadUI( void );
void	*Sys_GetUIAPI( void );

void	Sys_UnloadBotLib( void );
void	*Sys_GetBotLibAPI( void *parms );

char	*Sys_GetCurrentUser( void );

const char *Sys_GetCPUModel( void );
const char *Sys_GetDeviceModel( void );

void	QDECL Sys_Error( const char *error, ...) Q_NO_RETURN Q_PRINTF_FUNC(1, 2);
void	Sys_Quit (void) Q_NO_RETURN;
char	*Sys_GetClipboardData( void );	

void	Sys_Print( const char *msg );

int		Sys_Milliseconds (void);

qboolean Sys_RandomBytes( byte *string, int len );

void	Sys_DisplaySystemConsole( qboolean show );

cpuFeatures_t Sys_GetProcessorFeatures( void );

void	Sys_SetErrorText( const char *text );

void	Sys_SendPacket( int length, const void *data, netadr_t to );

qboolean	Sys_StringToAdr( const char *s, netadr_t *a );
qboolean Sys_GetPacket( netadr_t *net_from, msg_t *net_message );


qboolean	Sys_IsLANAddress (netadr_t adr);
qboolean	NET_IsIPAddressLocal (netadr_t adr);	 
void		Sys_ShowIP(void);

FILE	*Sys_FOpen( const char *ospath, const char *mode );
qboolean Sys_Mkdir( const char *path );
char	*Sys_Cwd( void );
void	Sys_SetDefaultInstallPath(const char *path);
char	*Sys_DefaultInstallPath(void);
char	*Sys_DefaultCDPath(void);       
void	 ToolServerInit( void );               
void	 ToolServerProcessCommands( void );    
char	*ToolServerGetData( void );            
void	 ToolServerShutdown( void );           
char	*Sys_SteamPath(void);
char	*Sys_GogPath(void);
char	*Sys_MicrosoftStorePath(void);


char	*Sys_DefaultHomeConfigPath(void);
char	*Sys_DefaultHomeDataPath(void);
char	*Sys_DefaultHomeStatePath(void);
const char *Sys_Dirname( char *path );
const char *Sys_Basename( char *path );
char *Sys_ConsoleInput( void );

void	SetProgramPath( const char *path );
char	*Sys_DefaultBasePath( void );
char	*Sys_DefaultUserPath( void );
char	*Sys_DefaultOutputPath( void );

char **Sys_ListFiles( const char *directory, const char *extension, const char *filter, int *numfiles, qboolean wantsubs );
void	Sys_FreeFileList( char **list );
void	Sys_Sleep(int msec);

void Sys_InitStreamThread(void);
void Sys_ShutdownStreamThread(void);
void Sys_BeginStreamedFile(fileHandle_t f, int readAhead);
void Sys_EndStreamedFile(fileHandle_t f);
int Sys_StreamedRead(void* buffer, int size, int count, fileHandle_t f);
void Sys_StreamSeek(fileHandle_t f, int offset, int origin);

qboolean Sys_LowPhysicalMemory(void);

void Sys_SetEnv(const char* name, const char* value);

void SetNormalThreadPriority( void );
void SetBelowNormalThreadPriority( void );

void RecoverLostAutodialData( void );

void Sys_CloseMutex( void );
void Sys_ShowConsole( int visLevel, qboolean quitOnClose );
void Sys_DebugPrint(const char* message);
void Sys_PumpMessageLoop( void );

qboolean SaveRegistryInfo( qboolean user, const char *pszName, void *pvBuf, long lSize );
qboolean LoadRegistryInfo( qboolean user, const char *pszName, void *pvBuf, long *plSize );

qboolean IsFirstRun( void );
qboolean IsNewConfig( void );
qboolean IsSafeMode( void );
void ClearNewConfigFlag( void );
const char *Sys_GetWholeClipboard( void );
void Sys_SetClipboard( const char *contents );

typedef enum
{
	DR_YES = 0,
	DR_NO = 1,
	DR_OK = 0,
	DR_CANCEL = 1
} dialogResult_t;

typedef enum
{
	DT_INFO,
	DT_WARNING,
	DT_ERROR,
	DT_YES_NO,
	DT_OK_CANCEL
} dialogType_t;

dialogResult_t Sys_Dialog( dialogType_t type, const char *message, const char *title );
qboolean Sys_OpenFolderInFileManager( const char *path, qboolean create );

void Sys_RemovePIDFile( const char *gamedir );
void Sys_InitPIDFile( const char *gamedir );

void Sys_ProcessBackgroundTasks(void);


#define NYT HMAX					
#define INTERNAL_NODE (HMAX+1)

typedef struct nodetype {
	struct	nodetype *left, *right, *parent;  
	struct	nodetype *next, *prev; 
	struct	nodetype **head; 
	int		weight;
	int		symbol;
} node_t;

#define HMAX 256 

typedef struct {
	int			blocNode;
	int			blocPtrs;

	node_t*		tree;
	node_t*		lhead;
	node_t*		ltail;
	node_t*		loc[HMAX+1];
	node_t**	freelist;

	node_t		nodeList[768];
	node_t*		nodePtrs[768];
} huff_t;

typedef struct {
	huff_t		compressor;
	huff_t		decompressor;
} huffman_t;

void	Huff_Compress(msg_t *buf, int offset);
void	Huff_Decompress(msg_t *buf, int offset);
void	Huff_Init(huffman_t *huff);
void	Huff_addRef(huff_t* huff, byte ch);
int		Huff_Receive (node_t *node, int *ch, byte *fin);
void	Huff_transmit (huff_t *huff, int ch, byte *fout);
void	Huff_offsetReceive (node_t *node, int *ch, byte *fin, int *offset);
void	Huff_offsetTransmit (huff_t *huff, int ch, byte *fout, int *offset);
void	Huff_putBit( int bit, byte *fout, int *offset);
int		Huff_getBit( byte *fout, int *offset);

extern huffman_t clientHuffTables;

#define	SV_ENCODE_START		4
#define SV_DECODE_START		12
#define	CL_ENCODE_START		12
#define CL_DECODE_START		4

float LIP_GetLength(const char *name);
int   load_lip_info(const char *name);
char *GetNextMorphTarget(const char *name, int *index, int *frame, float *amplitude);
void  resetAllLips(void);

#ifdef __cplusplus
}
#endif

#endif 
