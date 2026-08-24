
#include "q_shared.h"
#include "qcommon.h"

#include "../sys/sys_local.h"

#include "ef2_android_defaults.h"
#include "q_version.h"
#include <setjmp.h>
#ifndef _WIN32
#include <netinet/in.h>
#include <sys/stat.h> 
#else
#include <winsock.h>
#endif

#ifdef __cplusplus
#  include <chrono>
#endif

#include <time.h>

#ifdef __ANDROID__
#include <android/log.h>
void Sys_AndroidLog( int priority, const char *msg );
#endif

#include "../corepp/tiki.h"

#include "../qcommon/stringresource.h"  

#include "qa_r202.h"

#ifndef DEDICATED
#  include "../uilib/ui_public.h"
#endif


void InitializeCinematicSystem(void);

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_NUM_ARGVS	50

int		com_argc;
char	*com_argv[MAX_NUM_ARGVS+1];

jmp_buf abortframe;		


FILE *debuglogfile;
static fileHandle_t logfile;
fileHandle_t	com_journalFile;			
fileHandle_t	com_journalDataFile;		

cvar_t	*paused;
cvar_t	*com_canAutoSaveNow;
cvar_t	*config;
cvar_t	*fps;
cvar_t	*com_speeds;
cvar_t	*developer;
cvar_t	*com_dedicated;
cvar_t	*com_timescale;
cvar_t	*com_fixedtime;
cvar_t	*com_dropsim;		
cvar_t	*com_journal;
cvar_t	*com_maxfps;
cvar_t	*com_timedemo;
static cvar_t	*com_frameWait;
static cvar_t	*com_frameWaitMargin;
static cvar_t	*com_minimizedCvar;
cvar_t	*com_sv_running;
cvar_t	*com_cl_running;
cvar_t	*com_viewlog;
cvar_t	*com_logfile;		
cvar_t	*com_showtrace;
cvar_t	*com_shortversion;
cvar_t	*com_version;
cvar_t	*deathmatch;
cvar_t	*com_blood;
cvar_t	*com_buildScript;	
cvar_t	*com_passthrough;	 
cvar_t	*com_introPlayed;
cvar_t	*com_introplayed;
cvar_t	*com_disablewriteconfig;
cvar_t	*com_toolserver;
cvar_t	*com_printwarnings;
cvar_t	*loadingstatus;
cvar_t	*cg_maxclients;
cvar_t	*com_cameraMode;
cvar_t  *com_homepath;

cvar_t	*precache;

int		time_game;
int		time_frontend;		
int		time_backend;		

int			com_frameTime;
int			com_frameNumber;
int			com_frameMsec;

qboolean	com_errorEntered = qfalse;
qboolean	com_fullyInitialized = qfalse;
qboolean    com_firstConfig = qfalse;

char	com_errorMessage[MAXPRINTMSG];

void Com_WriteConfig_f( void );
void Com_WriteConfigToFile( const char *filename );
void CIN_CloseAllVideos(void);

void Com_InitHunkMemory( void );
void Z_InitMemory( void );


static char	*rd_buffer;
static int	rd_buffersize;
static void	(*rd_flush)( char *buffer );

#define MAX_FPS_TIMES 16

#ifdef __cplusplus
using fps_clock_t = std::chrono::high_resolution_clock;
using fps_time_t = fps_clock_t::time_point;
using fps_delta_t = fps_clock_t::duration;
#else
typedef unsigned long long fps_clock_t;
typedef unsigned long long fps_time_t;
typedef unsigned long long fps_delta_t;
#endif

static int fpsindex;
static fps_delta_t fpstimes[MAX_FPS_TIMES];
static fps_delta_t fpstotal;
static fps_time_t fpslasttime;
float currentfps;

void Com_BeginRedirect (char *buffer, int buffersize, void (*flush)( char *) )
{
	if (!buffer || !buffersize || !flush)
		return;
	rd_buffer = buffer;
	rd_buffersize = buffersize;
	rd_flush = flush;

	*rd_buffer = 0;
}

void Com_EndRedirect (void)
{
	if ( rd_flush ) {
		rd_flush(rd_buffer);
	}

	rd_buffer = NULL;
	rd_buffersize = 0;
	rd_flush = NULL;
}

#ifndef _COM_NOPRINTF

static void Com_RealPrintf( int level, const char *msg );

void QDECL Com_Printf( const char *fmt, ... ) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	Com_RealPrintf( 0, msg );
}

static void Com_RealPrintf( int level, const char *msg ) {
	static qboolean opening_qconsole = qfalse;
	static qboolean recursive_count = qfalse;

	if (recursive_count) {
		return;
	}
	recursive_count = qtrue;

	if ( com_printwarnings && !com_printwarnings->integer && level ) {
		recursive_count--;
		return;
	}

	if ( rd_buffer ) {
		if ((strlen (msg) + strlen(rd_buffer)) > (rd_buffersize - 1)) {
			rd_flush(rd_buffer);
			*rd_buffer = 0;
		}
		Q_strcat(rd_buffer, rd_buffersize, msg);
		recursive_count--;
		return;
	}

#ifndef DEDICATED
	if (com_dedicated && !com_dedicated->integer) {
		UI_PrintConsole(msg, level);
	}
#endif

	//   0x0C7EB0  beq  0xC7ED8          ; not initialised -> Sys_Print(msg) raw
	if (com_fullyInitialized) {
		char resolved[4096];

		convertStringResourceIds(msg, resolved, sizeof(resolved));
		Sys_Print(resolved);
	} else {
		Sys_Print(msg);
	}

	if ( com_logfile &&
		( com_logfile->integer == 1 ||
		  ( com_logfile->integer == 2 && level ) ||
		  com_logfile->integer == 3 ) ) {
		if ( !logfile && FS_Initialized() && !opening_qconsole) {
			struct tm *newtime;
			time_t aclock;

      opening_qconsole = qtrue;

			time( &aclock );
			newtime = localtime( &aclock );

			logfile = FS_FOpenFileWrite_HomeData( "qconsole.log" );

            recursive_count--;
			if(logfile)
			{
				Com_Printf( "logfile opened on %s\n", asctime( newtime ) );

				if ( com_logfile->integer > 2 )
				{
					FS_ForceFlush(logfile);
				}
			}
			else
			{
				Com_Printf("Opening qconsole.log failed!\n");
				Cvar_SetValue("logfile", 0);
			}
            recursive_count++;

      opening_qconsole = qfalse;
		}
		if (logfile && FS_Initialized()) {
			size_t msgLen = strlen(msg);

			if (msgLen > 0) {
				FS_Write(msg, msgLen, logfile);
			}
		}
    }

    recursive_count--;
}


void QDECL Com_DPrintf( const char *fmt, ...) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	if ( !developer || !developer->integer ) {
		return;			
	}

	va_start (argptr,fmt);
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	Com_RealPrintf( 0, msg );
}

void QDECL Com_WPrintf( const char *fmt, ... ) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	Com_RealPrintf( developer && developer->integer ? 1 : 0, msg );
}

void QDECL Com_WDPrintf( const char *fmt, ...) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	if ( !developer || !developer->integer ) {
		return;
	}

	va_start (argptr,fmt);
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	Com_RealPrintf( 1, msg );
}

qboolean Com_DoneLoading( void ) {
	if ( com_cl_running && com_cl_running->integer ) {
		return CL_DoneLoading();
	}

	return SV_DoneLoading();
}

void QDECL Com_DPrintf2( const char *fmt, ...) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	if ( !developer || !developer->integer ) {
		return;			
	}

	va_start (argptr,fmt);
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	Com_Printf ("%s", msg);
}

void QDECL Com_DebugPrintf( const char *fmt, ...) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	if ( !developer || !developer->integer ) {
		return;			
	}

	va_start (argptr,fmt);
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	Sys_DebugPrint (msg);
}

#endif

void QDECL Com_Error( int code, const char *fmt, ... ) {
	va_list		argptr;
	static int	lastErrorTime;
	static int	errorCount;
	int			currentTime;

#ifdef COM_ERROR_DROP_ASSERT
	if (code == ERR_DROP)
	{
		*(int*)0 = 0;
		assert(qfalse);
	}
#endif

	if ( fmt == NULL ) {
		fmt = "";
	}

	va_start (argptr,fmt);
	Q_vsnprintf (com_errorMessage,sizeof(com_errorMessage),fmt,argptr);
	va_end (argptr);

	if ( com_errorMessage[0] == '\0' ) {
		static int s_r209EmptyErrRows;

		if ( s_r209EmptyErrRows < 4 ) {
			s_r209EmptyErrRows++;
			Com_Printf( "Com_Error: code=%d was raised with NO text; the call site follows\n", code );
#ifdef __ANDROID__
			Sys_AndroidLog( ANDROID_LOG_ERROR,
				va( "Com_Error: code=%d was raised with NO text; the call site follows\n", code ) );
#endif
			Sys_PrintBackTrace();
		}

		Q_strncpyz( com_errorMessage,
			va( "Com_Error was raised with code %d and no text from the caller "
				"(a VM/script caller passing an empty message is the usual cause)", code ),
			sizeof( com_errorMessage ) );
	}

#ifdef __ANDROID__
	Sys_AndroidLog( code == ERR_DISCONNECT ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
		va( "ENGINE ERROR code=%d: %s\n", code, com_errorMessage ) );
#endif

	if ( code == ERR_FATAL ) {
		fprintf( stderr, "Com_Error: FATAL: %s\n", com_errorMessage );
		fflush( stderr );
	}

	assert(code != ERR_FATAL);

	if ( com_buildScript && com_buildScript->integer ) {
		code = ERR_FATAL;
	}

	// retail clears it once, at entry, not inside each arm.
	FS_PureServerSetLoadedPaks("", "");

	currentTime = Sys_Milliseconds();
	if ( currentTime - lastErrorTime < 100 ) {
		if ( ++errorCount > 3 ) {
			code = ERR_FATAL;
		}
	} else {
		errorCount = 0;
	}
	lastErrorTime = currentTime;

	if ( com_errorEntered ) {
		Sys_Error( "$$RecursiveErrorAfter$$ %s", com_errorMessage );
	}
	com_errorEntered = qtrue;

	switch (code) {
	case ERR_DISCONNECT:
#ifndef DEDICATED
		if ( strlen( com_errorMessage ) ) {
			CL_AbnormalDisconnect( com_errorMessage );
		} else {
			CL_Disconnect( qtrue );
		}
		CL_FlushMemory();
		CL_StartHunkUsers(qfalse);
#endif
		com_errorEntered = qfalse;
		SV_Shutdown( "$$ServerQuit$$\n" );
		longjmp (abortframe, -1);
	case ERR_DROP:
		Com_WPrintf ("********************\nERROR: %s\n********************\n", com_errorMessage);
		SV_Shutdown(va("$$ServerCrashed$$ %s\n", com_errorMessage));
#ifndef DEDICATED
		CL_AbnormalDisconnect( va("%s\n", com_errorMessage) );
		CL_FlushMemory();
		CL_StartHunkUsers(qfalse);
#endif
		com_errorEntered = qfalse;
		longjmp (abortframe, -1);
	case ERR_NEED_CD:
		SV_Shutdown("$$ServerDidntHaveCD\n");
#ifndef DEDICATED
		if (com_cl_running && com_cl_running->integer) {
			StopLoadingScreen();
			CL_AbnormalDisconnect("$$PleaseInsertCD$$");
			CL_FlushMemory();
			CL_StartHunkUsers(qfalse);
			com_errorEntered = qfalse;
			CL_CDDialog();
		} else
#endif
		{
			Com_WPrintf("$$ServerDidntHaveCD$$\n");
		}
		longjmp (abortframe, -1);
	default:
#ifndef DEDICATED
		StopLoadingScreen();
#endif
		CL_Shutdown ();
		SV_Shutdown (va("$$ServerFatalCrashed$$ %s\n", com_errorMessage));
		break;
	}

	Com_Shutdown ();

	Sys_Error("%s", com_errorMessage);
}


void Com_Quit_f( void ) {
	char *p = Cmd_Args( );
	if ( !com_errorEntered ) {
		VM_Forced_Unload_Start();
		SV_Shutdown(p[0] ? p : "Server quit");
		CL_Shutdown();
		VM_Forced_Unload_Done();
		Com_Shutdown ();
		FS_Shutdown(qtrue);
	}
	Sys_Quit ();
}




#define	MAX_CONSOLE_LINES	32
int		com_numConsoleLines;
char	*com_consoleLines[MAX_CONSOLE_LINES];

void Com_ParseCommandLine( char *commandLine ) {
    int inq = 0;
    com_consoleLines[0] = commandLine;
    com_numConsoleLines = 1;

    while ( *commandLine ) {
        if (*commandLine == '"') {
            inq = !inq;
        }
        if ( (*commandLine == '+' && !inq) || *commandLine == '\n'  || *commandLine == '\r' ) {
            if ( com_numConsoleLines == MAX_CONSOLE_LINES ) {
                return;
            }
            com_consoleLines[com_numConsoleLines] = commandLine + 1;
            com_numConsoleLines++;
            *commandLine = 0;
        }
        commandLine++;
    }
}



void Com_StartupVariable( const char *match ) {
	int		i;
	char	*s;

	for (i=0 ; i < com_numConsoleLines ; i++) {
		Cmd_TokenizeString( com_consoleLines[i] );
		if ( strcmp( Cmd_Argv(0), "set" ) ) {
			continue;
		}

		s = Cmd_Argv(1);
		
		if(!match || !strcmp(s, match))
		{
			if(Cvar_Flags(s) == CVAR_NONEXISTENT)
				Cvar_Get(s, Cmd_ArgsFrom(2), CVAR_USER_CREATED);
			else
				Cvar_Set2(s, Cmd_ArgsFrom(2), qfalse);
		}
	}
}


qboolean Com_AddStartupCommands( void ) {
	int		i;
	qboolean	added;

	added = qfalse;
	for (i=0 ; i < com_numConsoleLines ; i++) {
		if ( !com_consoleLines[i] || !com_consoleLines[i][0] ) {
			continue;
		}

		if ( !Q_stricmpn( com_consoleLines[i], "set ", 4 ) ) {
			continue;
		}
		Cbuf_AddText( com_consoleLines[i] );
		Cbuf_AddText( "\n" );
	}

	return added;
}



void Info_Print( const char *s ) {
	char	key[BIG_INFO_KEY];
	char	value[BIG_INFO_VALUE];
	char	*o;
	int		l;

	if (*s == '\\')
		s++;
	while (*s)
	{
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		if (l < 20)
		{
			Com_Memset (o, ' ', 20-l);
			key[20] = 0;
		}
		else
			*o = 0;
		Com_Printf ("%s", key);

		if (!*s)
		{
			Com_Printf ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;
		Com_Printf ("%s\n", value);
	}
}

const char *Com_StringContains(const char *str1, const char *str2, int casesensitive) {
	intptr_t len;
	int i, j;

	len = strlen(str1) - strlen(str2);
	for (i = 0; i <= len; i++, str1++) {
		for (j = 0; str2[j]; j++) {
			if (casesensitive) {
				if (str1[j] != str2[j]) {
					break;
				}
			}
			else {
				if (toupper(str1[j]) != toupper(str2[j])) {
					break;
				}
			}
		}
		if (!str2[j]) {
			return str1;
		}
	}
	return NULL;
}

int Com_Filter(const char *filter, const char *name, int casesensitive)
{
	char buf[MAX_TOKEN_CHARS];
	const char *ptr;
	int i, found;

	while(*filter) {
		if (*filter == '*') {
			filter++;
			for (i = 0; *filter; i++) {
				if (*filter == '*' || *filter == '?' || *filter == '[') break;
				buf[i] = *filter;
				filter++;
			}
			buf[i] = '\0';
			if (strlen(buf)) {
				ptr = Com_StringContains(name, buf, casesensitive);
				if (!ptr) return qfalse;
				name = ptr + strlen(buf);
			}
		}
		else if (*filter == '?') {
			filter++;
			name++;
		}
		else if (*filter == '[' && *(filter+1) == '[') {
			filter++;
		}
		else if (*filter == '[') {
			filter++;
			found = qfalse;
			while(*filter && !found) {
				if (*filter == ']' && *(filter+1) != ']') break;
				if (*(filter+1) == '-' && *(filter+2) && (*(filter+2) != ']' || *(filter+3) == ']')) {
					if (casesensitive) {
						if (*name >= *filter && *name <= *(filter+2)) found = qtrue;
					}
					else {
						if (toupper(*name) >= toupper(*filter) &&
							toupper(*name) <= toupper(*(filter+2))) found = qtrue;
					}
					filter += 3;
				}
				else {
					if (casesensitive) {
						if (*filter == *name) found = qtrue;
					}
					else {
						if (toupper(*filter) == toupper(*name)) found = qtrue;
					}
					filter++;
				}
			}
			if (!found) return qfalse;
			while(*filter) {
				if (*filter == ']' && *(filter+1) != ']') break;
				filter++;
			}
			filter++;
			name++;
		}
		else {
			if (casesensitive) {
				if (*filter != *name) return qfalse;
			}
			else {
				if (toupper(*filter) != toupper(*name)) return qfalse;
			}
			filter++;
			name++;
		}
	}
	return qtrue;
}

int Com_FilterPath(const char *filter, const char *name, int casesensitive)
{
	int i;
	char new_filter[MAX_QPATH];
	char new_name[MAX_QPATH];

	for (i = 0; i < MAX_QPATH-1 && filter[i]; i++) {
		if ( filter[i] == '\\' || filter[i] == ':' ) {
			new_filter[i] = '/';
		}
		else {
			new_filter[i] = filter[i];
		}
	}
	new_filter[i] = '\0';
	for (i = 0; i < MAX_QPATH-1 && name[i]; i++) {
		if ( name[i] == '\\' || name[i] == ':' ) {
			new_name[i] = '/';
		}
		else {
			new_name[i] = name[i];
		}
	}
	new_name[i] = '\0';
	return Com_Filter(new_filter, new_name, casesensitive);
}

int Com_RealTime(qtime_t *qtime) {
	time_t t;
	struct tm *tms;

	t = time(NULL);
	if (!qtime)
		return t;
	tms = localtime(&t);
	if (tms) {
		qtime->tm_sec = tms->tm_sec;
		qtime->tm_min = tms->tm_min;
		qtime->tm_hour = tms->tm_hour;
		qtime->tm_mday = tms->tm_mday;
		qtime->tm_mon = tms->tm_mon;
		qtime->tm_year = tms->tm_year;
		qtime->tm_wday = tms->tm_wday;
		qtime->tm_yday = tms->tm_yday;
		qtime->tm_isdst = tms->tm_isdst;
	}
	return t;
}

char *CopyString( const char *in ) {
	char	*out;

#ifndef ZONE_DEBUG
	if (!in[0]) {
		return ( char * )Z_EmptyStringPointer();
	}
	else if (!in[1]) {
		if (in[0] >= '0' && in[0] <= '9') {
			return ( char * )Z_NumberStringPointer( *in );
		}
	}
#endif

	out = ( char * )Z_Malloc( strlen( in ) + 1 );
	strcpy( out, in );
	return out;
}

void *B_Malloc( size_t size ) {
	return Z_Malloc( size );
}

void B_Free( void *ptr ) {
	return Z_Free( ptr );
}

void CL_ShutdownCGame( void );
void CL_ShutdownUI( void );
void SV_ShutdownGameProgs( void );



#define	MAX_PUSHED_EVENTS	            1024
static int com_pushedEventsHead = 0;
static int com_pushedEventsTail = 0;
static sysEvent_t	com_pushedEvents[MAX_PUSHED_EVENTS];

void Com_InitJournaling( void ) {
	Com_StartupVariable( "journal" );
	com_journal = Cvar_Get ("journal", "0", CVAR_INIT);
	if ( !com_journal->integer ) {
		return;
	}

	if ( com_journal->integer == 1 ) {
		Com_Printf( "Journaling events\n");
		com_journalFile = FS_FOpenFileWrite_HomeState( "journal.dat" );
		com_journalDataFile = FS_FOpenFileWrite_HomeState( "journaldata.dat" );
	} else if ( com_journal->integer == 2 ) {
		Com_Printf( "Replaying journaled events\n");
		FS_FOpenFileRead( "journal.dat", &com_journalFile, qtrue, qtrue );
		FS_FOpenFileRead( "journaldata.dat", &com_journalDataFile, qtrue, qtrue );
	}

	if ( !com_journalFile || !com_journalDataFile ) {
		Cvar_Set( "com_journal", "0" );
		com_journalFile = 0;
		com_journalDataFile = 0;
		Com_Printf( "Couldn't open journal files\n" );
	}
}


#define MAX_QUEUED_EVENTS  256
#define MASK_QUEUED_EVENTS ( MAX_QUEUED_EVENTS - 1 )

static sysEvent_t  eventQueue[ MAX_QUEUED_EVENTS ];
static int         eventHead = 0;
static int         eventTail = 0;

void Com_QueueEvent( int time, sysEventType_t type, int value, int value2, size_t ptrLength, void *ptr )
{
	sysEvent_t  *ev;

	if ( type == SE_MOUSE && eventHead != eventTail )
	{
		ev = &eventQueue[ ( eventHead + MAX_QUEUED_EVENTS - 1 ) & MASK_QUEUED_EVENTS ];

		if ( ev->evType == SE_MOUSE )
		{
			ev->evValue += value;
			ev->evValue2 += value2;
			return;
		}
	}

	ev = &eventQueue[ eventHead & MASK_QUEUED_EVENTS ];

	if ( eventHead - eventTail >= MAX_QUEUED_EVENTS )
	{
		Com_Printf("Com_QueueEvent: overflow\n");
		if ( ev->evPtr )
		{
			Z_Free( ev->evPtr );
		}
		eventTail++;
	}

	eventHead++;

	if ( time == 0 )
	{
		time = Sys_Milliseconds();
	}

	ev->evTime = time;
	ev->evType = type;
	ev->evValue = value;
	ev->evValue2 = value2;
	ev->evPtrLength = ptrLength;
	ev->evPtr = ptr;
}

sysEvent_t Com_GetSystemEvent(void)
{
	sysEvent_t  ev;
	char        *s;

	if ( eventHead > eventTail )
	{
		eventTail++;
		return eventQueue[ ( eventTail - 1 ) & MASK_QUEUED_EVENTS ];
	}

	Sys_SendKeyEvents();

	s = Sys_ConsoleInput();
	if ( s )
	{
		char  *b;
		int   len;

		len = strlen( s ) + 1;
		b = Z_Malloc( len );
		strcpy( b, s );
		Com_QueueEvent( 0, SE_CONSOLE, 0, 0, len, b );
	}

	if ( eventHead > eventTail )
	{
		eventTail++;
		return eventQueue[ ( eventTail - 1 ) & MASK_QUEUED_EVENTS ];
	}

	memset( &ev, 0, sizeof( ev ) );
	ev.evTime = Sys_Milliseconds();

	return ev;
}

sysEvent_t	Com_GetRealEvent( void ) {
	int			r;
	sysEvent_t	ev;

	if ( com_journal->integer == 2 ) {
		r = FS_Read( &ev, sizeof(ev), com_journalFile );
		if ( r != sizeof(ev) ) {
			Com_Error( ERR_FATAL, "Error reading from journal file" );
		}
		if ( ev.evPtrLength ) {
			ev.evPtr = Z_Malloc( ev.evPtrLength );
			r = FS_Read( ev.evPtr, ev.evPtrLength, com_journalFile );
			if ( r != ev.evPtrLength ) {
				Com_Error( ERR_FATAL, "Error reading from journal file" );
			}
		}
	} else {
		ev = Com_GetSystemEvent();

		if ( com_journal->integer == 1 ) {
			r = FS_Write( &ev, sizeof(ev), com_journalFile );
			if ( r != sizeof(ev) ) {
				Com_Error( ERR_FATAL, "Error writing to journal file" );
			}
			if ( ev.evPtrLength ) {
				r = FS_Write( ev.evPtr, ev.evPtrLength, com_journalFile );
				if ( r != ev.evPtrLength ) {
					Com_Error( ERR_FATAL, "Error writing to journal file" );
				}
			}
		}
	}

	return ev;
}


void Com_InitPushEvent( void ) {
  memset( com_pushedEvents, 0, sizeof(com_pushedEvents) );
  com_pushedEventsHead = 0;
  com_pushedEventsTail = 0;
}


void Com_PushEvent( sysEvent_t *event ) {
	sysEvent_t		*ev;
	static int printedWarning = 0;

	ev = &com_pushedEvents[ com_pushedEventsHead & (MAX_PUSHED_EVENTS-1) ];

	if ( com_pushedEventsHead - com_pushedEventsTail >= MAX_PUSHED_EVENTS ) {

		if ( !printedWarning ) {
			printedWarning = qtrue;
			Com_Printf( "WARNING: Com_PushEvent overflow\n" );
		}

		if ( ev->evPtr ) {
			Z_Free( ev->evPtr );
		}
		com_pushedEventsTail++;
	} else {
		printedWarning = qfalse;
	}

	*ev = *event;
	com_pushedEventsHead++;
}

sysEvent_t	Com_GetEvent( void ) {
	if ( com_pushedEventsHead > com_pushedEventsTail ) {
		com_pushedEventsTail++;
		return com_pushedEvents[ (com_pushedEventsTail-1) & (MAX_PUSHED_EVENTS-1) ];
	}
	return Com_GetRealEvent();
}

void Com_RunAndTimeServerPacket( netadr_t *evFrom, msg_t *buf ) {
	int		t1, t2, msec;

	t1 = 0;

	if ( com_speeds->integer ) {
		t1 = Sys_Milliseconds ();
	}

	SV_PacketEvent( *evFrom, buf );

	if ( com_speeds->integer ) {
		t2 = Sys_Milliseconds ();
		msec = t2 - t1;
		if ( com_speeds->integer == 3 ) {
			Com_Printf( "SV_PacketEvent time: %i\n", msec );
		}
	}
}

int Com_EventLoop( void ) {
	sysEvent_t	ev;
	netadr_t	evFrom;
	byte		bufData[MAX_MSGLEN];
	msg_t		buf;

	MSG_Init( &buf, bufData, sizeof( bufData ) );

	while ( 1 ) {
		ev = Com_GetEvent();

		if ( ev.evType == SE_NONE ) {
			while ( NET_GetLoopPacket( NS_CLIENT, &evFrom, &buf ) ) {
				CL_PacketEvent( evFrom, &buf );
			}

			while ( NET_GetLoopPacket( NS_SERVER, &evFrom, &buf ) ) {
				if ( com_sv_running->integer ) {
					Com_RunAndTimeServerPacket( &evFrom, &buf );
				}
			}

			return ev.evTime;
		}


		switch(ev.evType)
		{
			case SE_KEY:
				CL_KeyEvent( ev.evValue, ev.evValue2, ev.evTime );
			break;
			case SE_CHAR:
				CL_CharEvent( ev.evValue );
			break;
			case SE_MOUSE:
				CL_MouseEvent( ev.evValue, ev.evValue2, ev.evTime );
			break;
			case SE_JOYSTICK_AXIS:
				CL_JoystickEvent( ev.evValue, ev.evValue2, ev.evTime );
			break;
			case SE_CONSOLE:
				Cbuf_AddText( (char *)ev.evPtr );
				Cbuf_AddText( "\n" );
			break;
			default:
				Com_Error( ERR_FATAL, "Com_EventLoop: bad event type %i", ev.evType );
			break;
		}

		if ( ev.evPtr ) {
			Z_Free( ev.evPtr );
		}
	}

	return 0;	
}

int Com_Milliseconds (void) {
	sysEvent_t	ev;

	do {

		ev = Com_GetRealEvent();
		if ( ev.evType != SE_NONE ) {
			Com_PushEvent( &ev );
		}
	} while ( ev.evType != SE_NONE );

	return ev.evTime;
}


static void Q_NO_RETURN Com_Error_f (void) {
	if ( Cmd_Argc() > 1 ) {
		Com_Error( ERR_DROP, "Testing drop error" );
	} else {
		Com_Error( ERR_FATAL, "Testing fatal error" );
	}
}


static void Com_Freeze_f (void) {
	float	s;
	int		start, now;

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "freeze <seconds>\n" );
		return;
	}
	s = atof( Cmd_Argv(1) );

	start = Com_Milliseconds();

	while ( 1 ) {
		now = Com_Milliseconds();
		if ( ( now - start ) * 0.001 > s ) {
			break;
		}
	}
}

static void Com_Crash_f( void ) {
	* ( volatile int * ) 0 = 0x12345678;
}

void Com_Setenv_f(void)
{
	int argc = Cmd_Argc();
	char *arg1 = Cmd_Argv(1);

	if(argc > 2)
	{
		char *arg2 = Cmd_ArgsFrom(2);
		
		Sys_SetEnv(arg1, arg2);
	}
	else if(argc == 2)
	{
		char *env = getenv(arg1);
		
		if(env)
			Com_Printf("%s=%s\n", arg1, env);
		else
			Com_Printf("%s undefined\n", arg1);
        }
}

#ifndef STANDALONE

#ifndef DEDICATED
char	cl_cdkey[34] = "                                ";
#else
char	cl_cdkey[34] = "123456789";
#endif

qboolean CL_CDKeyValidate( const char *key, const char *checksum );
void Com_ReadCDKey( const char *filename ) {
	fileHandle_t	f;
	char			buffer[33];
	char			fbuffer[MAX_OSPATH];

	Com_sprintf(fbuffer, sizeof(fbuffer), "%s/q3key", filename);

	FS_BaseDir_FOpenFileRead( fbuffer, &f );
	if ( !f ) {
		Com_Memset( cl_cdkey, '\0', 17 );
		return;
	}

	Com_Memset( buffer, 0, sizeof(buffer) );

	FS_Read( buffer, 16, f );
	FS_FCloseFile( f );

	if (CL_CDKeyValidate(buffer, NULL)) {
		Q_strncpyz( cl_cdkey, buffer, 17 );
	} else {
		Com_Memset( cl_cdkey, '\0', 17 );
	}
}

void Com_AppendCDKey( const char *filename ) {
	fileHandle_t	f;
	char			buffer[33];
	char			fbuffer[MAX_OSPATH];

	Com_sprintf(fbuffer, sizeof(fbuffer), "%s/q3key", filename);

	FS_BaseDir_FOpenFileRead( fbuffer, &f );
	if (!f) {
		Com_Memset( &cl_cdkey[16], '\0', 17 );
		return;
	}

	Com_Memset( buffer, 0, sizeof(buffer) );

	FS_Read( buffer, 16, f );
	FS_FCloseFile( f );

	if (CL_CDKeyValidate(buffer, NULL)) {
		strcat( &cl_cdkey[16], buffer );
	} else {
		Com_Memset( &cl_cdkey[16], '\0', 17 );
	}
}

#ifndef DEDICATED
static Q_UNUSED_VAR void Com_WriteCDKey( const char *filename, const char *ikey ) {
	fileHandle_t	f;
	char			fbuffer[MAX_OSPATH];
	char			key[17];
#ifndef _WIN32
	mode_t			savedumask;
#endif


	Com_sprintf(fbuffer, sizeof(fbuffer), "%s/q3key", filename);


	Q_strncpyz( key, ikey, 17 );

	if(!CL_CDKeyValidate(key, NULL) ) {
		return;
	}

#ifndef _WIN32
	savedumask = umask(0077);
#endif
	f = FS_BaseDir_FOpenFileWrite_HomeState( fbuffer );
	if ( !f ) {
		Com_Printf ("Couldn't write CD key to %s.\n", fbuffer );
		goto out;
	}

	FS_Write( key, 16, f );

	FS_Printf( f, "\n// generated by " PRODUCT_NAME ", do not modify\r\n" );
	FS_Printf( f, "// Do not give this file to ANYONE.\r\n" );
	FS_Printf( f, "// id Software and Activision will NOT ask you to send this file to them.\r\n");

	FS_FCloseFile( f );
out:
#ifndef _WIN32
	umask(savedumask);
#else
	;
#endif
}
#endif

#endif 

static void Com_InitRand(void)
{
	unsigned int seed;

	if(Sys_RandomBytes((byte *) &seed, sizeof(seed)))
		srand(seed);
	else
		srand(time(NULL));
}

#ifdef __ANDROID__
static void Com_AndroidAdoptRetailSaveUser( char *configname, int confignameSize )
{
	char **saveDirs;
	int    nSaveDirs = 0, i;
	char  *adopted = NULL;

	if ( Q_stricmp( configname, "config.cfg" ) ) {
		return;		 
	}

	saveDirs = FS_ListFiles( "save", "/", qfalse, &nSaveDirs );
	for ( i = 0; i < nSaveDirs; i++ ) {
		char	 qpath[MAX_QPATH];
		char   **entries;
		int	 nEntries = 0, j;
		qboolean retail = qfalse;

		Com_sprintf( qpath, sizeof( qpath ), "save/%s", saveDirs[i] );
		entries = FS_ListFiles( qpath, "ssv", qfalse, &nEntries );
		for ( j = 0; j < nEntries; j++ ) {
			if ( !Q_stricmpn( entries[j], "SaveGame", 8 ) ) {
				retail = qtrue;
				break;
			}
		}
		FS_FreeFileList( entries );

		if ( retail ) {
			if ( adopted ) {	// more than one retail profile: do not guess
				adopted = NULL;
				break;
			}
			adopted = saveDirs[i];
		}
	}

	if ( adopted ) {
		Q_strncpyz( configname, adopted, confignameSize );
		Q_strcat( configname, confignameSize, ".cfg" );
		Com_Printf( "[R145:saveid] Android has no OS user; adopted the retail save "
			"profile '%s' so retail's saves resolve\n", adopted );
	}

	FS_FreeFileList( saveDirs );
}

static void Com_MigrateLegacySaveFolder( void )
{
	char   folder[256];
	char **files;
	int    n = 0, i;

	Q_strncpyz( folder, Com_GetArchiveFolder(), sizeof( folder ) );
	if ( !Q_stricmp( folder, "save/config" ) ) {
		return;		
	}

	files = FS_ListFiles( "save/config", NULL, qfalse, &n );
	for ( i = 0; i < n; i++ ) {
		char  from[MAX_QPATH], to[MAX_QPATH];
		void *buf = NULL;
		int   len;

		Com_sprintf( from, sizeof( from ), "save/config/%s", files[i] );
		len = FS_ReadFile( from, &buf );
		if ( len < 0 || !buf ) {
			continue;
		}
		Com_sprintf( to, sizeof( to ), "%s/%s", folder, files[i] );
		FS_WriteFile( to, buf, len );
		FS_FreeFile( buf );

		FS_Remove_HomeData( from );
		Com_Printf( "[R145:saveid] migrated legacy save file '%s' -> '%s'\n", from, to );
	}
	FS_FreeFileList( files );
}
#endif	

qboolean Com_ConfigExists(const char* configname) {
	fileHandle_t handle = (fileHandle_t)0;

    if (FS_FOpenFileRead(va("%s", configname), &handle, qfalse, qfalse) == -1) {
		return qfalse;
    }

	FS_FCloseFile(handle);
	return qtrue;
}


// would be a retail feature this build never shipped.
void ToolServerInit( void ) {}
void ToolServerShutdown( void ) {}
void ToolServerProcessCommands( void ) {}
char *ToolServerGetData( void ) { return NULL; }

#ifdef __cplusplus
extern "C" {
#endif
void CreateLoadSaveGameManager( void );
void DeleteLoadSaveGameManager( void );
#ifdef __cplusplus
}
#endif

void Com_Init( char *commandLine ) {
	int			iStart;
	int			iEnd;
	const char	*s;
	char		configname[ 128 ];
	int			qport;
	qboolean	configExists;

	Com_Printf( "--- Common Initialization ---\n" );

	iStart = Sys_Milliseconds();

	Com_Printf( "%s %s %s %s\n", PRODUCT_NAME, PRODUCT_VERSION_FULL, PLATFORM_STRING, PRODUCT_VERSION_DATE );

	if ( setjmp (abortframe) ) {
		Sys_Error("Error during initialization");
	}

	Com_Memset( &eventQueue[ 0 ], 0, MAX_QUEUED_EVENTS * sizeof( sysEvent_t ) );

	Com_InitRand();

	Com_InitPushEvent();

	Com_ParseCommandLine( commandLine );

	Swap_Init();
	Cbuf_Init();
	Z_InitMemory();

	Cmd_Init();
	Cvar_Init();

	Com_StartupVariable( NULL );

	Com_StartupVariable( "developer" );

	developer = Cvar_Get( "developer", "0", CVAR_ARCHIVE | CVAR_CHEAT );

	CL_InitKeyCommands();

	com_homepath = Cvar_Get("com_homepath", "", CVAR_INIT|CVAR_PROTECTED);

	FS_InitFilesystem ();

	Com_InitJournaling();

	TIKI_Begin();

	Cbuf_Execute( 0 );

	Cbuf_AddText( "exec global/default.cfg\n" );

	Cmd_AddCommand( "setd", Cvar_SetD_f );
	Cbuf_AddText( "exec global/game.cfg\n" );
	Cbuf_Execute( 0 );
	Cmd_RemoveCommand( "setd" );


	cvar_global_force = qtrue;
	Cbuf_Execute( 0 );
	cvar_global_force = qfalse;


	{
		const char *cfguser = getenv( "USERNAME" );
		if ( !cfguser || !cfguser[ 0 ] ) {
			cfguser = getenv( "USER" );
		}
		if ( cfguser && cfguser[ 0 ] ) {
			Q_strncpyz( configname, cfguser, sizeof( configname ) );
			Q_strcat( configname, sizeof( configname ), ".cfg" );
		} else {
			Q_strncpyz( configname, "config", sizeof( configname ) );
		}
	}

	Com_StartupVariable( "config" );

	config = Cvar_Get( "config", "", 0 );

	if( strlen( config->string ) > 1 )
	{
		size_t len = strlen( config->string );

		Q_strncpyz( configname, config->string, len >= sizeof( configname ) ? sizeof( configname ) : len );
		COM_StripExtension( configname, configname, sizeof( configname ) );
		Q_strcat( configname, sizeof( configname ), ".cfg" );
		SaveRegistryInfo( 1, "config", configname, sizeof( configname ) );
	}
	else
	{
		long size = sizeof( configname );
		LoadRegistryInfo(1, "config", configname, &size );
		COM_DefaultExtension( configname, sizeof( configname ), ".cfg" );
	}

	SaveRegistryInfo( 1, "config", configname, sizeof( configname ) );

#ifdef __ANDROID__
	Com_AndroidAdoptRetailSaveUser( configname, sizeof( configname ) );
#endif

	Cvar_Set( "config", configname );

#ifdef __ANDROID__
	Com_MigrateLegacySaveFolder();
#endif

	Q_strncpyz( configname, EF2_CONFIG_FILE, sizeof( configname ) );

	configExists = Com_ConfigExists(configname);

	if ( configExists ) {
		com_firstConfig = !FS_FileExists_HomeConfig(configname);
	} else if ( FS_FileExists_HomeConfig( configname ) ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: %s exists but could not be read; "
			"settings and key bindings will not be loaded\n", configname );
	}

	if ( configExists ) {
		Cbuf_AddText( va( "exec %s\n", configname ) );
	}

	Com_Printf( "Config: %s \n", configname );

	Cbuf_Execute( 0 );

#ifdef __ANDROID__
	// joystick term in CL_CreateCmd @0x16F490 is deliberately not transcribed
	// upgrading install.  Bump again whenever the shipped defaults must win
#define ANDROID_DEFAULTS_VERSION 7
	if ( Cvar_VariableIntegerValue( "in_androidDefaults" ) < ANDROID_DEFAULTS_VERSION )
	{
		Cbuf_AddText( EF2_ANDROID_DEFAULT_COMMANDS );
		Cbuf_Execute( 0 );
		Cvar_Get( "in_androidDefaults", "0", CVAR_ARCHIVE );
		Cvar_Set( "in_androidDefaults", XSTRING( ANDROID_DEFAULTS_VERSION ) );

		Com_WriteConfigToFile( EF2_CONFIG_FILE );
	}
#endif

	{
		fileHandle_t f;
		char         fbuffer[64];
		char         buffer[33];

		Com_sprintf( fbuffer, sizeof( fbuffer ), ":%s:ef2cd.key", "base" );
		FS_FOpenFileRead( fbuffer, &f, qtrue, qfalse );
		if ( !f ) {
			Q_strncpyz( cl_cdkey, "                ", 17 );
		} else {
			Com_Memset( buffer, 0, sizeof( buffer ) );
			FS_Read( buffer, 16, f );
			FS_FCloseFile( f );

			if ( CL_CDKeyValidate( buffer, NULL ) ) {
				Q_strncpyz( cl_cdkey, buffer, 17 );
			} else {
				Q_strncpyz( cl_cdkey, "                ", 17 );
			}
		}
	}

	Com_StartupVariable( NULL );

	cvar_modifiedFlags &= ~CVAR_ARCHIVE;

	if( developer && developer->integer ) {
		Cmd_AddCommand( "error", Com_Error_f );
		Cmd_AddCommand( "crash", Com_Crash_f );
		Cmd_AddCommand( "freeze", Com_Freeze_f );
	}
	Cmd_AddCommand("quit", Com_Quit_f);
	Cmd_AddCommand("changeVectors", MSG_ReportChangeVectors_f );
	Cmd_AddCommand("playerChangeVectors", MSG_PlayerReportChangeVectors_f );
	Cmd_AddCommand("writeconfig", Com_WriteConfig_f );
	Cmd_AddCommand("pause", Com_Pause_f);
	Cmd_AddCommand("unpause", Com_UnPause_f);

	Com_StartupVariable( NULL );

#ifdef DEDICATED
	com_dedicated = Cvar_Get ("dedicated", "1", CVAR_INIT);
	Cvar_CheckRange( com_dedicated, 1, 2, qtrue );
#else
	com_dedicated = Cvar_Get ("dedicated", "0", CVAR_LATCH);
	Cvar_CheckRange( com_dedicated, 0, 2, qtrue );
#endif
	Com_InitHunkMemory();

	cvar_modifiedFlags &= ~CVAR_ARCHIVE;

#ifdef __ANDROID__
	if ( Cvar_VariableIntegerValue( "in_androidDefaults" ) )
		cvar_modifiedFlags |= CVAR_ARCHIVE;
#endif

	com_maxfps = Cvar_Get( "com_maxfps", "85", CVAR_ARCHIVE );
	/* r203-pace: PORT-ONLY, beyond retail's list above.  Safe defaults and NOT
	   archived -- the limiter blocks with a 1 ms spin margin, and any other device
	   or experiment must opt out explicitly rather than inherit a stored value. */
	com_frameWait       = Cvar_Get( "com_frameWait", "1", 0 );
	com_frameWaitMargin = Cvar_Get( "com_frameWaitMargin", "1", 0 );
	deathmatch = Cvar_Get( "deathmatch", "0", 0 );
	paused = Cvar_Get( "paused", "0", CVAR_ROM );
	com_canAutoSaveNow = Cvar_Get( "com_canAutoSaveNow", "0", CVAR_ROM );
	fps = Cvar_Get( "fps", "0", CVAR_ARCHIVE );
	com_logfile = Cvar_Get("logfile", "0", CVAR_TEMP);
	com_timescale = Cvar_Get( "timescale", "1", CVAR_CHEAT | CVAR_SYSTEMINFO );
	com_fixedtime = Cvar_Get( "fixedtime", "0", CVAR_CHEAT );
	com_showtrace = Cvar_Get( "com_showtrace", "0", CVAR_CHEAT );
	com_dropsim = Cvar_Get( "com_dropsim", "0", CVAR_CHEAT );
	com_viewlog = Cvar_Get( "viewlog", "0", CVAR_CHEAT );
	com_speeds = Cvar_Get( "com_speeds", "0", 0 );
	com_timedemo = Cvar_Get( "timedemo", "0", CVAR_CHEAT );
	com_dedicated = Cvar_Get( "dedicated", "0", CVAR_LATCH );
	com_introplayed = Cvar_Get( "com_introplayed", "0", CVAR_ARCHIVE );
	com_sv_running = Cvar_Get( "sv_running", "0", CVAR_ROM );
	com_cl_running = Cvar_Get( "cl_running", "0", CVAR_ROM );
	com_buildScript = Cvar_Get( "com_buildScript", "0", 0 );
	// retail's (pool 0x249ece); the demo pool string documents "The server must have
	com_passthrough = Cvar_Get( "com_passthrough", "1", 0 );
	com_disablewriteconfig = Cvar_Get( "com_disablewriteconfig", "0", 0 );
	com_toolserver = Cvar_Get( "com_toolserver", "0", 0 );
	com_printwarnings = Cvar_Get( "com_printwarnings", "1", 0 );
	loadingstatus = Cvar_Get( "loadingstatus", "", 0 );
	cg_maxclients = Cvar_Get( "cg_maxclients", "1", 0 );

	if( com_dedicated->integer )
	{
		if( !com_viewlog->integer ) {
			Cvar_Set( "viewlog", "1" );
		}
	}

	// r140: the platform token is THIS build's, not the Mac PPC decompile's -- the
	// `version' cvar is CVAR_SERVERINFO, so it must not advertise a PPC Mac on a
	s = va( "%s %s %s %s", "Elite Force II", "1.10", PLATFORM_STRING, "Oct  9 2003");
	com_version = Cvar_Get( "version", s, CVAR_ROM | CVAR_SERVERINFO );
	// Retail registers "protocol" in SV_Init, not here (SV_Init @0x001DF3E0,
	com_shortversion = Cvar_Get( "shortversion", "1.10", CVAR_ROM );


	Sys_Init();

#ifdef NDEBUG
	Sys_InitPIDFile(FS_GetCurrentGameDir());
#endif

	Com_RandomBytes((byte*)&qport, sizeof(int));
	Netchan_Init(qport & 0xffff);
	SV_Init();


	if ( com_dedicated->integer ) {
		ToolServerInit();
	}

	initializeStringResource();

#ifndef DEDICATED
	CreateLoadSaveGameManager();
#endif

	com_dedicated->modified = qfalse;

#ifndef DEDICATED
	if( com_dedicated->integer )
	{
		Sys_CloseMutex();
	}
	else
	{
		CL_Init();
		Sys_ShowConsole( com_viewlog->integer, qfalse );
	}
#else
    Sys_CloseMutex();
#endif

	InitializeCinematicSystem();

	com_frameTime = Com_Milliseconds();

	Com_AddStartupCommands();

	LoadViewModes();

	com_fullyInitialized = qtrue;


	RecoverLostAutodialData();


	iEnd = Sys_Milliseconds();
	Com_Printf( "--- Common Initialization Complete --- %i ms\n", iEnd - iStart );
}



void Com_WriteConfigToFile( const char *filename ) {
	fileHandle_t	f;

	if ( com_disablewriteconfig && com_disablewriteconfig->integer ) {
		return;
	}

	f = FS_FOpenFileWrite_HomeConfig( filename );
	if ( !f ) {
		Com_Printf ("Couldn't write %s.\n", filename );
		return;
	}

	FS_Printf (f, "// generated, do not modify\n");
	FS_Printf( f, "//\n" );
	FS_Printf( f, "// Key Bindings\n" );
	FS_Printf( f, "//\n" );
	Key_WriteBindings( f );
	FS_Printf( f, "//\n" );
	FS_Printf( f, "// Cvars\n" );
	FS_Printf( f, "//\n" );
	Cvar_WriteVariables( f );
	FS_Printf( f, "//\n" );
	FS_Printf( f, "// Aliases\n" );
	FS_Printf( f, "//\n" );
	Cmd_WriteAliases( f );

	FS_FCloseFile( f );
}


void Com_WriteConfiguration( void ) {
	if ( !com_fullyInitialized ) {
		return;
	}

	if ( !(cvar_modifiedFlags & CVAR_ARCHIVE ) ) {
		return;
	}
	cvar_modifiedFlags &= ~CVAR_ARCHIVE;

	// r142-mt P5: the ONE unified file, not config->string (retail's OS-operator
	Com_WriteConfigToFile( EF2_CONFIG_FILE );

#ifndef DEDICATED
	Cvar_Get ("fs_game", "", CVAR_INIT|CVAR_SYSTEMINFO );
#endif
}


void Com_WriteConfig_f( void ) {
	char	filename[MAX_QPATH];

	// given (Com_WriteConfig_f @0x000CA8E0); an explicit argument that is not the
	if ( Cmd_Argc() == 2 ) {
		Q_strncpyz( filename, Cmd_Argv( 1 ), sizeof( filename ) );
		COM_DefaultExtension( filename, sizeof( filename ), ".cfg" );
		if ( Q_stricmp( filename, EF2_CONFIG_FILE ) ) {
			Com_Printf( "writeconfig: %s is ignored; the unified user-state file is %s\n",
				filename, EF2_CONFIG_FILE );
		}
	} else if ( Cmd_Argc() > 2 ) {
		Com_Printf( "Usage: writeconfig [filename]\n" );
		return;
	}

	Com_Printf( "Writing %s.\n", EF2_CONFIG_FILE );
	Com_WriteConfigToFile( EF2_CONFIG_FILE );
}

int Com_ModifyMsec( int msec ) {
	int		clampTime;

	if ( com_fixedtime->integer ) {
		msec = com_fixedtime->integer;
	} else if ( com_timescale->value ) {
		msec *= com_timescale->value;
	}

	if ( msec < 1 ) {
		msec = 1;
	}

	if ( com_dedicated->integer ) {
		if (msec > 500)
			Com_Printf( "Hitch warning: %i msec frame time\n", msec );

		clampTime = 5000;
	} else
	if ( !com_sv_running->integer ) {
		clampTime = 5000;
	} else {
		clampTime = 200;
	}

	if ( msec > clampTime ) {
		msec = clampTime;
	}

	return msec;
}

#define R201_BUDGET_WINDOW_MS		1000	
#define R201_BUDGET_MAX_FRAMES		3600	
#define R201_BUDGET_MAX_ZONES		11		

static int	r201_budgetFrames;
static int	r201_budgetWindowStart;
static int	r201_budgetAudio,  r201_budgetAudioMax;
static int	r201_budgetSleep,  r201_budgetSleepMax;
static int	r201_budgetSwap,   r201_budgetSwapMax;

static qboolean	r201_lastWindowValid;
static int		r201_lastWindowFrames;
static int		r201_lastWindowMean10[R201_BUDGET_MAX_ZONES];

void R201_BudgetAdd( int zone, int msec ) {
	if ( msec <= 0 ) {
		return;
	}

	switch ( zone ) {
	case R201_ZONE_AUDIO:
		r201_budgetAudio += msec;
		if ( msec > r201_budgetAudioMax ) {
			r201_budgetAudioMax = msec;
		}
		break;
	case R201_ZONE_SLEEP:
		r201_budgetSleep += msec;
		if ( msec > r201_budgetSleepMax ) {
			r201_budgetSleepMax = msec;
		}
		break;
	case R201_ZONE_SWAP:
		r201_budgetSwap += msec;
		if ( msec > r201_budgetSwapMax ) {
			r201_budgetSwapMax = msec;
		}
		break;
	default:
		break;
	}
}

qboolean R201_BudgetLastWindow( int *framesOut, int *mean10Out ) {
	int i;

	if ( !r201_lastWindowValid ) {
		return qfalse;
	}
	if ( framesOut ) {
		*framesOut = r201_lastWindowFrames;
	}
	if ( mean10Out ) {
		for ( i = 0; i < R201_BUDGET_MAX_ZONES; i++ ) {
			mean10Out[i] = r201_lastWindowMean10[i];
		}
	}
	return qtrue;
}

#define R201_LOAD_MAX_ROWS		720		

int		r201_loadOn = 0;
int		r201_loadDepth = 0;
int		r201_loadInClient = 0;

static int	r201_loadFrames;
static int	r201_loadSums[R201_LZ_COUNT], r201_loadMaxs[R201_LZ_COUNT];
static int	r201_loadOther,   r201_loadOtherMax;
static int	r201_loadClient,  r201_loadClientMax;
static int	r201_loadUnacctO, r201_loadUnacctOMax;
static int	r201_loadUnacctC, r201_loadUnacctCMax;
static int	r201_loadAttribO, r201_loadAttribC;
static int	r201_loadRows;
static int	r201_loadStopped;

void R201_LoadAdd( int zone, int msec, int depth ) {
	if ( zone < 0 || zone >= R201_LZ_COUNT ) {
		return;
	}
	if ( msec < 0 ) {
		msec = 0;
	}

	r201_loadSums[zone] += msec;
	if ( msec > r201_loadMaxs[zone] ) {
		r201_loadMaxs[zone] = msec;
	}

	if ( depth == 0 ) {
		if ( r201_loadInClient ) {
			r201_loadAttribC += msec;
		} else {
			r201_loadAttribO += msec;
		}
	}
}

static void R201_LoadFrame( int otherMsec, int clientMsec ) {
	int	unacctO, unacctC;

	if ( !r201_loadOn ) {
		r201_loadAttribO = r201_loadAttribC = 0;
		return;
	}

	unacctO = otherMsec  - r201_loadAttribO;
	unacctC = clientMsec - r201_loadAttribC;
	if ( unacctO < 0 ) unacctO = 0;
	if ( unacctC < 0 ) unacctC = 0;

	r201_loadOther += otherMsec;	if ( otherMsec  > r201_loadOtherMax  ) r201_loadOtherMax  = otherMsec;
	r201_loadClient += clientMsec;	if ( clientMsec > r201_loadClientMax ) r201_loadClientMax = clientMsec;
	r201_loadUnacctO += unacctO;	if ( unacctO    > r201_loadUnacctOMax ) r201_loadUnacctOMax = unacctO;
	r201_loadUnacctC += unacctC;	if ( unacctC    > r201_loadUnacctCMax ) r201_loadUnacctCMax = unacctC;

	r201_loadFrames++;
	r201_loadAttribO = r201_loadAttribC = 0;
}

static void R201_LoadReset( void ) {
	int	i;

	for ( i = 0; i < R201_LZ_COUNT; i++ ) {
		r201_loadSums[i] = r201_loadMaxs[i] = 0;
	}
	r201_loadFrames  = 0;
	r201_loadOther   = r201_loadOtherMax   = 0;
	r201_loadClient  = r201_loadClientMax  = 0;
	r201_loadUnacctO = r201_loadUnacctOMax = 0;
	r201_loadUnacctC = r201_loadUnacctCMax = 0;
}

static void R201_LoadPrint( int frames, int window ) {
	static const char *zoneName[R201_LZ_COUNT] = {
		"spawn", "cmap", "hunkclr", "svinit", "spawnent",
		"clmap", "hunkusr", "rendreg", "initcg", "cginit",
		"fs", "sndinit", "scr", "cgame", "ui"
	};
	char	line[1024], part[64];
	int		i, mean10;
	struct { const char *name; int sum; int max; } zone[R201_LZ_COUNT + 4];

	if ( r201_loadStopped ) {
		return;
	}

	if ( frames < 1 ) {
		frames = 1;
	}

	for ( i = 0; i < R201_LZ_COUNT; i++ ) {
		zone[i].name = zoneName[i];
		zone[i].sum  = r201_loadSums[i];
		zone[i].max  = r201_loadMaxs[i];
	}
	zone[R201_LZ_COUNT+0].name = "other";	zone[R201_LZ_COUNT+0].sum = r201_loadOther;	zone[R201_LZ_COUNT+0].max = r201_loadOtherMax;
	zone[R201_LZ_COUNT+1].name = "client";	zone[R201_LZ_COUNT+1].sum = r201_loadClient;	zone[R201_LZ_COUNT+1].max = r201_loadClientMax;
	zone[R201_LZ_COUNT+2].name = "unacctO";	zone[R201_LZ_COUNT+2].sum = r201_loadUnacctO;	zone[R201_LZ_COUNT+2].max = r201_loadUnacctOMax;
	zone[R201_LZ_COUNT+3].name = "unacctC";	zone[R201_LZ_COUNT+3].sum = r201_loadUnacctC;	zone[R201_LZ_COUNT+3].max = r201_loadUnacctCMax;

	line[0] = '\0';
	for ( i = 0; i < R201_LZ_COUNT + 4; i++ ) {
		mean10 = (int)( ( (long long)zone[i].sum * 10 + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d",
					 zone[i].name, mean10 / 10, mean10 % 10, zone[i].max );
		Q_strcat( line, sizeof( line ), part );
	}

	Com_Printf( "[R201:load] frames=%d window=%dms%s\n", frames, window, line );

	r201_loadRows++;
	if ( r201_loadRows >= R201_LOAD_MAX_ROWS ) {
		r201_loadStopped = 1;
		Com_Printf( "[R201:load] capped at %d rows; no further [R201:load] lines\n",
					R201_LOAD_MAX_ROWS );
	}
}

static void R201_BudgetFrame( int serverMsec, int clientMsec, int spinMsec, int totalMsec ) {
	static cvar_t	*r201_frameStats;
	static cvar_t	*r201_loadStats;
	static int		r201_server, r201_serverMax;
	static int		r201_client, r201_clientMax;
	static int		r201_front,  r201_frontMax;
	static int		r201_back,   r201_backMax;
	static int		r201_spin,   r201_spinMax;
	static int		r201_other,  r201_otherMax;
	static int		r201_total,  r201_totalMax;

	int		otherMsec, window, frames, i, mean10;
	char	line[1024], part[64];
	struct { const char *name; int sum; int max; } zone[R201_BUDGET_MAX_ZONES];

	if ( !r201_frameStats ) {
		r201_frameStats = Cvar_Get( "r201_frameStats", "1", 0 );
	}
	if ( !r201_loadStats ) {
		r201_loadStats = Cvar_Get( "r201_loadStats", "1", 0 );
	}

	r201_loadOn = r201_loadStats->integer;

	if ( !r201_frameStats->integer ) {
		r201_budgetFrames = 0;
		r201_budgetWindowStart = 0;
		r201_budgetAudio = r201_budgetAudioMax = 0;
		r201_budgetSleep = r201_budgetSleepMax = 0;
		r201_budgetSwap  = r201_budgetSwapMax  = 0;
		r201_loadOn = 0;
		r201_loadAttribO = r201_loadAttribC = 0;
		R201_LoadReset();
		return;
	}

	if ( serverMsec < 0 ) serverMsec = 0;
	if ( clientMsec < 0 ) clientMsec = 0;
	if ( spinMsec   < 0 ) spinMsec   = 0;
	if ( totalMsec  < 0 ) totalMsec  = 0;

	otherMsec = totalMsec - spinMsec - serverMsec - clientMsec;
	if ( otherMsec < 0 ) {
		otherMsec = 0;
	}

	R201_LoadFrame( otherMsec, clientMsec );

	r201_server += serverMsec;	if ( serverMsec > r201_serverMax ) r201_serverMax = serverMsec;
	r201_client += clientMsec;	if ( clientMsec > r201_clientMax ) r201_clientMax = clientMsec;
	r201_spin   += spinMsec;	if ( spinMsec   > r201_spinMax   ) r201_spinMax   = spinMsec;
	r201_other  += otherMsec;	if ( otherMsec  > r201_otherMax  ) r201_otherMax  = otherMsec;
	r201_total  += totalMsec;	if ( totalMsec  > r201_totalMax  ) r201_totalMax  = totalMsec;

	r201_front += time_frontend;	if ( time_frontend > r201_frontMax ) r201_frontMax = time_frontend;
	r201_back  += time_backend;		if ( time_backend  > r201_backMax  ) r201_backMax  = time_backend;

	r201_budgetFrames++;

	if ( !r201_budgetWindowStart ) {
		r201_budgetWindowStart = Sys_Milliseconds();
	}

	window = Sys_Milliseconds() - r201_budgetWindowStart;
	if ( window < R201_BUDGET_WINDOW_MS && r201_budgetFrames < R201_BUDGET_MAX_FRAMES ) {
		return;
	}

	frames = r201_budgetFrames;
	if ( frames < 1 ) {
		frames = 1;
	}

	zone[0].name  = "server";			zone[0].sum  = r201_server;	zone[0].max  = r201_serverMax;
	zone[1].name  = "client";			zone[1].sum  = r201_client;	zone[1].max  = r201_clientMax;
	zone[2].name  = "renderFront";		zone[2].sum  = r201_front;	zone[2].max  = r201_frontMax;
	zone[3].name  = "renderBack";		zone[3].sum  = r201_back;	zone[3].max  = r201_backMax;
	zone[4].name  = "audio";			zone[4].sum  = r201_budgetAudio;	zone[4].max = r201_budgetAudioMax;
	zone[5].name  = "blocked";			zone[5].sum  = r201_budgetSleep + r201_budgetSwap;
	zone[5].max   = r201_budgetSleepMax + r201_budgetSwapMax;
	zone[6].name  = "sleep";			zone[6].sum  = r201_budgetSleep;	zone[6].max = r201_budgetSleepMax;
	zone[7].name  = "swap";				zone[7].sum  = r201_budgetSwap;		zone[7].max = r201_budgetSwapMax;
	zone[8].name  = "spin";				zone[8].sum  = r201_spin;	zone[8].max  = r201_spinMax;
	zone[9].name  = "other";			zone[9].sum  = r201_other;	zone[9].max  = r201_otherMax;
	zone[10].name = "total";			zone[10].sum = r201_total;	zone[10].max = r201_totalMax;

	line[0] = '\0';
	for ( i = 0; i < R201_BUDGET_MAX_ZONES; i++ ) {
		mean10 = (int)( ( (long long)zone[i].sum * 10 + frames / 2 ) / frames );
		r201_lastWindowMean10[i] = mean10;	
		Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d",
					 zone[i].name, mean10 / 10, mean10 % 10, zone[i].max );
		Q_strcat( line, sizeof( line ), part );
	}

	r201_lastWindowFrames = frames;
	r201_lastWindowValid  = qtrue;

	Com_Printf( "[R201:frame] frames=%d window=%dms%s\n", frames, window, line );

	if ( r201_loadOn ) {
		R201_LoadPrint( r201_loadFrames, window );
		R201_LoadReset();
	}

	r201_budgetFrames = 0;
	r201_budgetWindowStart = Sys_Milliseconds();
	r201_budgetAudio = r201_budgetAudioMax = 0;
	r201_budgetSleep = r201_budgetSleepMax = 0;
	r201_budgetSwap  = r201_budgetSwapMax  = 0;
	r201_server = r201_serverMax = 0;
	r201_client = r201_clientMax = 0;
	r201_front  = r201_frontMax  = 0;
	r201_back   = r201_backMax   = 0;
	r201_spin   = r201_spinMax   = 0;
	r201_other  = r201_otherMax  = 0;
	r201_total  = r201_totalMax  = 0;
}


static int R203_WaitDecision( int minMsec, int elapsed, int margin, int minimized ) {
	int remain;

	if ( minMsec <= 0 ) {
		return 0;
	}
	if ( elapsed < 0 ) {
		elapsed = 0;			
	}
	remain = minMsec - elapsed;
	if ( remain <= 0 ) {
		return 0;				
	}
	if ( margin < 0 ) {
		margin = 0;
	}
	if ( minimized ) {
		return remain;			
	}
	if ( remain <= margin ) {
		return 0;				
	}
	return remain - margin;
}

#define R202_WINDOW_MS		1000
#define R202_MAX_FRAMES		3600
#define R202_MAX_ZONES		8
#define R202_MAX_COUNTERS	6
#define R202_MAX_ROWS		900

int			r202_statsOn;

static int	r202_zoneSum[R202_FAM_COUNT][R202_MAX_ZONES];
static int	r202_zoneMax[R202_FAM_COUNT][R202_MAX_ZONES];
static int	r202_cntSum[R202_FAM_COUNT][R202_MAX_COUNTERS];
static int	r202_cntMax[R202_FAM_COUNT][R202_MAX_COUNTERS];
static int	r202_totalSum[R202_FAM_COUNT];
static int	r202_totalMax[R202_FAM_COUNT];
static int	r202_frames[R202_FAM_COUNT];
static int	r202_windowStart[R202_FAM_COUNT];
static int	r202_rowsEmitted[R202_FAM_COUNT];
static int	r202_frameNamed[R202_FAM_COUNT];

static int	r202_waitSum,   r202_waitMax;
static int	r202_spinSum,   r202_spinMax;
static int	r202_periodSum, r202_periodMax;
static int	r202_minimSum,  r202_minimMax;
static int	r202_paceLastExitUsec;

static const char *r202_familyName[R202_FAM_COUNT] = {
	"ui", "disp", "v3d", "walk", "spin"
};

static const int r202_zoneCount[R202_FAM_COUNT] = {
	R202_UI_ZONES, R202_DISP_ZONES, R202_V3D_ZONES, R202_WALK_ZONES, R202_SPIN_ZONES
};

static const int r202_counterCount[R202_FAM_COUNT] = {
	R202_UI_COUNTERS, R202_DISP_COUNTERS, 0, R202_WALK_COUNTERS, R202_SPIN_COUNTERS
};

static const char *r202_zoneName[R202_FAM_COUNT][R202_MAX_ZONES] = {
	{ "setrt", "filluid", "service", "consoles", "state", "menus", "views", "other" },
	{ "walkup", "disp", "cursor", "printstr", "other", NULL, NULL, NULL },
	{ "scrdraw", "set2d", "draw2d", "cgdraw", "other", NULL, NULL, NULL },
	{ "vis", "hid", "other", NULL, NULL, NULL, NULL, NULL },
	{ "cfg", "vlog", "fnum", "evloop", "other", NULL, NULL, NULL }
};

static int	r202_rootT0[R202_FAM_COUNT];

static const char *r202_counterName[R202_FAM_COUNT][R202_MAX_COUNTERS] = {
	{ "upd", "disp", "draw", "text", NULL, NULL },
	{ "children", "disps", NULL, NULL, NULL, NULL },
	{ NULL, NULL, NULL, NULL, NULL, NULL },
	{ "menus", "visw", "hidw", NULL, NULL, NULL },
	{ "iters", "limited", "min", NULL, NULL, NULL }
};

int R202_Microseconds( void ) {
#ifndef _WIN32
	struct timespec ts;

	clock_gettime( CLOCK_MONOTONIC, &ts );
	return (int)( ( ts.tv_sec % 1000000 ) * 1000000 + ts.tv_nsec / 1000 );
#else
	return Sys_Milliseconds() * 1000;
#endif
}

void R202_On( void ) {
	static cvar_t *v;

	if ( !v ) {
		v = Cvar_Get( "r202_stats", "1", 0 );
	}

	r202_statsOn = ( v && v->integer ) ? 1 : 0;
}

static void R202_Reset( int family );
static void R202_AddTenths( int family, int zone, int tenths );

void R202_RootBegin( int family ) {
	R202_On();

	if ( family < 0 || family >= R202_FAM_COUNT ) {
		return;
	}

	r202_rootT0[family] = r202_statsOn ? R202_Microseconds() : 0;
}

void R202_RootEnd( int family ) {
	if ( family < 0 || family >= R202_FAM_COUNT ) {
		return;
	}

	if ( !r202_statsOn ) {
		R202_Reset( family );
		return;
	}

	R202_Frame( family, R202_Microseconds() - r202_rootT0[family] );
}

static void R202_Reset( int family ) {
	int i;

	r202_frames[family] = 0;
	r202_windowStart[family] = 0;
	r202_frameNamed[family] = 0;
	r202_totalSum[family] = r202_totalMax[family] = 0;

	for ( i = 0; i < R202_MAX_ZONES; i++ ) {
		r202_zoneSum[family][i] = r202_zoneMax[family][i] = 0;
	}
	for ( i = 0; i < R202_MAX_COUNTERS; i++ ) {
		r202_cntSum[family][i] = r202_cntMax[family][i] = 0;
	}

	if ( family == R202_FAM_SPIN ) {
		r202_waitSum = r202_waitMax = 0;
		r202_spinSum = r202_spinMax = 0;
		r202_periodSum = r202_periodMax = 0;
		r202_minimSum = r202_minimMax = 0;
		r202_paceLastExitUsec = 0;
	}
}


static void R202_Print( int family, int window ) {
	int		frames, i, mean10, sum, pmean;
	char	line[1024], part[64];

	frames = r202_frames[family];
	if ( frames < 1 ) {
		frames = 1;
	}

	line[0] = '\0';

	for ( i = 0; i < r202_zoneCount[family]; i++ ) {
		sum = r202_zoneSum[family][i];
		mean10 = (int)( ( (long long)sum + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d.%d",
					 r202_zoneName[family][i], mean10 / 10, mean10 % 10,
					 r202_zoneMax[family][i] / 10, r202_zoneMax[family][i] % 10 );
		Q_strcat( line, sizeof( line ), part );
	}

	for ( i = 0; i < r202_counterCount[family]; i++ ) {
		sum = r202_cntSum[family][i];
		mean10 = (int)( ( (long long)sum * 10 + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d",
					 r202_counterName[family][i], mean10 / 10, mean10 % 10,
					 r202_cntMax[family][i] );
		Q_strcat( line, sizeof( line ), part );
	}

	if ( family == R202_FAM_SPIN ) {
		pmean = (int)( ( (long long)r202_waitSum + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " yield=%d.%d/%d.%d",
					 pmean / 10, pmean % 10, r202_waitMax / 10, r202_waitMax % 10 );
		Q_strcat( line, sizeof( line ), part );

		pmean = (int)( ( (long long)r202_spinSum + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " spun=%d.%d/%d.%d",
					 pmean / 10, pmean % 10, r202_spinMax / 10, r202_spinMax % 10 );
		Q_strcat( line, sizeof( line ), part );

		pmean = (int)( ( (long long)r202_periodSum + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " period=%d.%d/%d.%d",
					 pmean / 10, pmean % 10, r202_periodMax / 10, r202_periodMax % 10 );
		Q_strcat( line, sizeof( line ), part );

		pmean = (int)( ( (long long)r202_minimSum * 10 + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " minim=%d.%d/%d",
					 pmean / 10, pmean % 10, r202_minimMax );
		Q_strcat( line, sizeof( line ), part );
	}

	mean10 = (int)( ( (long long)r202_totalSum[family] + frames / 2 ) / frames );
	Com_sprintf( part, sizeof( part ), " total=%d.%d/%d.%d",
				 mean10 / 10, mean10 % 10,
				 r202_totalMax[family] / 10, r202_totalMax[family] % 10 );
	Q_strcat( line, sizeof( line ), part );

	Com_Printf( "[R202:%s] frames=%d window=%dms%s\n",
				r202_familyName[family], frames, window, line );

	r202_rowsEmitted[family]++;
}

void R202_Frame( int family, int totalUsec ) {
	int window;

	if ( family < 0 || family >= R202_FAM_COUNT ) {
		return;
	}

	if ( !r202_statsOn ) {
		R202_Reset( family );
		return;
	}

	if ( totalUsec < 0 ) {
		totalUsec = 0;
	}

	totalUsec /= 100;			

	{
		int r202_other = totalUsec - r202_frameNamed[family];

		if ( r202_other < 0 ) {
			r202_other = 0;
		}

		R202_AddTenths( family, r202_zoneCount[family] - 1, r202_other );
		r202_frameNamed[family] = 0;
	}

	r202_totalSum[family] += totalUsec;
	if ( totalUsec > r202_totalMax[family] ) {
		r202_totalMax[family] = totalUsec;
	}

	r202_frames[family]++;

	if ( !r202_windowStart[family] ) {
		r202_windowStart[family] = Sys_Milliseconds();
	}

	window = Sys_Milliseconds() - r202_windowStart[family];
	if ( window < R202_WINDOW_MS && r202_frames[family] < R202_MAX_FRAMES ) {
		return;
	}

	if ( r202_rowsEmitted[family] < R202_MAX_ROWS ) {
		R202_Print( family, window );
	} else if ( r202_rowsEmitted[family] == R202_MAX_ROWS ) {
		r202_rowsEmitted[family]++;
		Com_Printf( "[R202:%s] capped at %d rows; no further lines\n",
					r202_familyName[family], R202_MAX_ROWS );
	}

	R202_Reset( family );
	r202_windowStart[family] = Sys_Milliseconds();
}

static void R202_AddTenths( int family, int zone, int tenths ) {
	if ( family < 0 || family >= R202_FAM_COUNT || zone < 0 || zone >= R202_MAX_ZONES ) {
		return;
	}
	if ( tenths < 0 ) {
		tenths = 0;
	}
	if ( tenths == 0 ) {
		return;
	}

	r202_zoneSum[family][zone] += tenths;
	if ( tenths > r202_zoneMax[family][zone] ) {
		r202_zoneMax[family][zone] = tenths;
	}

	if ( zone != r202_zoneCount[family] - 1 ) {
		r202_frameNamed[family] += tenths;
	}
}

void R202_Add( int family, int zone, int usec ) {
	if ( usec <= 0 ) {
		return;
	}

	R202_AddTenths( family, zone, usec / 100 );
}

void R202_Bump( int family, int counter, int n ) {
	if ( family < 0 || family >= R202_FAM_COUNT || counter < 0 || counter >= R202_MAX_COUNTERS ) {
		return;
	}
	if ( n <= 0 ) {
		return;
	}

	r202_cntSum[family][counter] += n;
	if ( n > r202_cntMax[family][counter] ) {
		r202_cntMax[family][counter] = n;
	}
}

static void R202_PaceAdd( int *sum, int *max, int units ) {
	if ( units <= 0 ) {
		return;
	}

	*sum += units;
	if ( units > *max ) {
		*max = units;
	}
}

void Com_Frame( void ) {

	int		msec, minMsec;
	static int	lastTime = 0;

	int		timeBeforeFirstEvents;
	int		timeBeforeServer;
	int		timeBeforeEvents;
	int		timeBeforeClient;
	int		timeAfter;

	int		r201FrameStart, r201SpinStart, r201AfterSpin, r201BeforeServer, r201AfterServer, r201ClientStart, r201FrameEnd;

	int		r202_cfgUsec = 0, r202_vlogUsec = 0, r202_fnumUsec = 0;
	int		r202_evUsec = 0, r202_spinIters = 0;

	int		r202_waitMsec = 0, r202_minimized = 0;
	int		r202_waitUsec = 0, r202_spinUsec = 0, r202_periodUsec = 0;

	if ( setjmp (abortframe) ) {
		return;			
	}

	R202_ROOT_BEGIN( R202_FAM_SPIN );

	r201FrameStart = Sys_Milliseconds();

	// game_export_t::SetFrameNumber, which retail's game_export_t has never had, so it
#ifndef DEDICATED
	if (!com_dedicated || !com_dedicated->integer)
	{
		r202_fnumUsec = ( r202_statsOn ? R202_Microseconds() : 0 );
		CL_SetFrameNumber(com_frameNumber);
		if ( r202_statsOn ) { r202_fnumUsec = R202_Microseconds() - r202_fnumUsec; }
	}
#endif

	timeBeforeFirstEvents =0;
	timeBeforeServer =0;
	timeBeforeEvents =0;
	timeBeforeClient = 0;
	timeAfter = 0;

	r202_cfgUsec = ( r202_statsOn ? R202_Microseconds() : 0 );
	Com_WriteConfiguration();
	if ( r202_statsOn ) { r202_cfgUsec = R202_Microseconds() - r202_cfgUsec; }

#ifndef DEDICATED
    if (com_viewlog->modified) {
        if (!com_dedicated || !com_dedicated->integer) {
            r202_vlogUsec = ( r202_statsOn ? R202_Microseconds() : 0 );
            Sys_ShowConsole(com_viewlog->integer, qfalse);
            if ( r202_statsOn ) { r202_vlogUsec = R202_Microseconds() - r202_vlogUsec; }
        }

        com_viewlog->modified = qfalse;
    }
#endif

	if ( com_speeds->integer ) {
		timeBeforeFirstEvents = Sys_Milliseconds ();
	}

    if ( com_dedicated->integer || com_maxfps->integer <= 0 || com_timedemo->integer ) {
        minMsec = 1;
    } else {
        minMsec = 1000 / com_maxfps->integer;
    }

    if ( minMsec < 1 ) {
        minMsec = 1;
    }

    if ( com_frameWait && com_frameWait->integer && !com_dedicated->integer ) {
        int r203Elapsed;

        if ( !com_minimizedCvar ) {
            com_minimizedCvar = Cvar_Get( "com_minimized", "0", 0 );
        }
        r202_minimized = ( com_minimizedCvar && com_minimizedCvar->integer ) ? 1 : 0;

        r203Elapsed = Sys_Milliseconds() - lastTime;
        r202_waitMsec = R203_WaitDecision( minMsec, r203Elapsed,
                                           com_frameWaitMargin->integer,
                                           r202_minimized );
        if ( r202_waitMsec > 0 ) {
            int r203T0 = ( r202_statsOn ? R202_Microseconds() : 0 );

            Sys_Sleep( r202_waitMsec );

            if ( r202_statsOn ) { r202_waitUsec = R202_Microseconds() - r203T0; }
        }
    }

    r201SpinStart = Sys_Milliseconds();

    {
    int r203SpinT0 = ( r202_statsOn ? R202_Microseconds() : 0 );

    do
    {
        int r202_evT = ( r202_statsOn ? R202_Microseconds() : 0 );

        com_frameTime = Com_EventLoop();

        if ( r202_statsOn ) { r202_evUsec += R202_Microseconds() - r202_evT; }
        r202_spinIters++;

        if ( lastTime > com_frameTime ) {
            lastTime = com_frameTime;
        }

        msec = com_frameTime - lastTime;
    } while ( msec < minMsec );

    if ( r202_statsOn ) { r202_spinUsec = R202_Microseconds() - r203SpinT0; }
    }

    lastTime = com_frameTime;

	r201AfterSpin = Sys_Milliseconds();

	if ( r202_statsOn ) {
		int r203Now = R202_Microseconds();

		if ( r202_paceLastExitUsec != 0 ) {
			r202_periodUsec = r203Now - r202_paceLastExitUsec;
			if ( r202_periodUsec < 0 ) {
				r202_periodUsec = 0;
			}
		}
		r202_paceLastExitUsec = r203Now;
	} else {
		r202_paceLastExitUsec = 0;
	}

	if ( r202_statsOn ) {
		R202_Add( R202_FAM_SPIN, R202_SPZ_cfg,    r202_cfgUsec );
		R202_Add( R202_FAM_SPIN, R202_SPZ_vlog,   r202_vlogUsec );
		R202_Add( R202_FAM_SPIN, R202_SPZ_fnum,   r202_fnumUsec );
		R202_Add( R202_FAM_SPIN, R202_SPZ_evloop, r202_evUsec );

		R202_Bump( R202_FAM_SPIN, R202_SC_iters, r202_spinIters );
		if ( r202_spinIters > 1 ) {
			R202_Bump( R202_FAM_SPIN, R202_SC_limited, 1 );
		}
		R202_Bump( R202_FAM_SPIN, R202_SC_min, minMsec );

		R202_PaceAdd( &r202_waitSum,   &r202_waitMax,   r202_waitUsec / 100 );
		R202_PaceAdd( &r202_spinSum,   &r202_spinMax,   r202_spinUsec / 100 );
		R202_PaceAdd( &r202_periodSum, &r202_periodMax, r202_periodUsec / 100 );
		R202_PaceAdd( &r202_minimSum,  &r202_minimMax,  r202_minimized );

		R202_ROOT_END( R202_FAM_SPIN );
	}

#ifndef DEDICATED
	// port-only CL_FinishedIntro, which retail does not have)
	Cbuf_Execute(0);
#else
	Cbuf_Execute(msec);
#endif

    msec = Com_ModifyMsec(msec);

    if (com_speeds->integer) {
        timeBeforeServer = Sys_Milliseconds();
    }

	if ( com_dedicated->integer ) {
		ToolServerProcessCommands();
	}

	r201BeforeServer = Sys_Milliseconds();

	SV_Frame( msec );

	r201AfterServer = Sys_Milliseconds();

	if ( com_dedicated->modified ) {
		Cvar_Get( "dedicated", "0", 0 );
		com_dedicated->modified = qfalse;
		if ( !com_dedicated->integer ) {
			SV_Shutdown( "dedicated set to 0" );
			CL_FlushMemory();
		} else {
			CL_Shutdown();
			Sys_CloseMutex();
			Sys_ShowConsole(0, 0);
			NET_Init();
		}
	}

#ifndef DEDICATED
	if ( com_speeds->integer ) {
		timeBeforeEvents = Sys_Milliseconds ();
	}
	Com_EventLoop();
	Cbuf_Execute(msec);


	if ( com_speeds->integer ) {
		timeBeforeClient = Sys_Milliseconds ();
	}

	time_frontend = 0;
	time_backend = 0;
	r201ClientStart = Sys_Milliseconds();

	r201_loadInClient = 1;

	CL_Frame( msec );

	r201_loadInClient = 0;

	r201FrameEnd = Sys_Milliseconds();

	if ( com_speeds->integer ) {
		timeAfter = Sys_Milliseconds ();
	}
#else
	if ( com_speeds->integer ) {
		timeAfter = Sys_Milliseconds ();
		timeBeforeEvents = timeAfter;
		timeBeforeClient = timeAfter;
	}
	r201ClientStart = r201AfterServer;
	r201FrameEnd = Sys_Milliseconds();
#endif


	if ( com_speeds->integer ) {
		int			all, sv, ev, cl;

		all = timeAfter - timeBeforeServer;
		sv = timeBeforeEvents - timeBeforeServer;
		ev = timeBeforeServer - timeBeforeFirstEvents + timeBeforeClient - timeBeforeEvents;
		cl = timeAfter - timeBeforeClient;
		sv -= time_game;
		cl -= time_frontend + time_backend;

		Com_Printf ("frame:%i all:%3i sv:%3i ev:%3i cl:%3i gm:%3i rf:%3i bk:%3i\n",
					 com_frameNumber, all, sv, ev, cl, time_game, time_frontend, time_backend );
	}

	if (fps->integer) {
		fps_time_t fpstime;

#ifdef __cplusplus
		fpstime = fps_clock_t::now();
#else
		fpstime = Sys_Milliseconds();
#endif


		fpstotal = (fpstime - fpslasttime) + (fpstotal - fpstimes[fpsindex]);
		fpstimes[fpsindex] = fpstime - fpslasttime;
		fpsindex = (fpsindex + 1) % MAX_FPS_TIMES;
		fpslasttime = fpstime;

#ifdef __cplusplus
		if (fpstotal.count()) {
			currentfps = MAX_FPS_TIMES / std::chrono::duration<long double>(fpstotal).count();
		} else {
			currentfps = 0.0;
		}
#else
		if (fpstotal) {
			currentfps = MAX_FPS_TIMES / (fpstotal / 1000.0);
		} else {
			currentfps = 0.0;
		}
#endif
	}

	if ( com_showtrace->integer ) {

		extern	int c_traces, c_brush_traces, c_patch_traces, c_terrain_patch_traces;
		extern	int	c_pointcontents;

		Com_Printf ("%4i traces  (%ib %ip %itp) %4i points\n", c_traces,
			c_brush_traces, c_patch_traces, c_terrain_patch_traces, c_pointcontents);
		c_traces = 0;
		c_brush_traces = 0;
		c_patch_traces = 0;
		c_terrain_patch_traces = 0;
		c_pointcontents = 0;
	}

	R201_BudgetFrame( r201AfterServer - r201BeforeServer,
					  r201FrameEnd - r201ClientStart,
					  r201AfterSpin - r201SpinStart,
					  r201FrameEnd - r201FrameStart );

	com_frameNumber++;
}

void Com_Shutdown (void) {
	if (logfile) {
		FS_FCloseFile (logfile);
		logfile = 0;
	}

	if ( com_dedicated && com_dedicated->integer ) {
		ToolServerShutdown();
	}

	uninitializeStringResource();

#ifndef DEDICATED
	DeleteLoadSaveGameManager();
#endif

	if ( com_journalFile ) {
		FS_FCloseFile( com_journalFile );
		com_journalFile = 0;
	}
}

qboolean Com_SanitizeName(const char* pszOldName, char* pszNewName, size_t bufferSize)
{
    int i;
    qboolean bBadName = qfalse;
    const char* p = pszOldName;
    size_t maxLength;

    maxLength = (bufferSize / sizeof(char)) - 1;
    bBadName = qfalse;

    for (p = pszOldName; *p && (unsigned char)*p <= ' '; p++) {
        bBadName = qtrue;
    }

    for (i = 0; *p && (unsigned char)*p >= ' ' && i < maxLength; p++, i++)
    {
        if (*p == '~' || *p == '`')
        {
            pszNewName[i] = '*';
            bBadName = qtrue;
        }
        else if (*p == '\"')
        {
            pszNewName[i] = '\'';
            bBadName = qtrue;
        }
        else if (*p == '\\')
        {
            pszNewName[i] = '/';
            bBadName = qtrue;
        }
        else if (*p == ';')
        {
            pszNewName[i] = ':';
            bBadName = qtrue;
        }
        else
        {
            pszNewName[i] = *p;
        }
    }

    for (; i > 0 && (unsigned char)pszNewName[i - 1] <= ' '; i--) {
        bBadName = qtrue;
    }

    pszNewName[i] = 0;

    if (!i)
    {
        const char* pNewNameDynamic = va("*** Blank Name #%04d ***", rand() % 100000);
        Q_strncpyz(pszNewName, pNewNameDynamic, bufferSize);
        bBadName = qtrue;
    }

    if (*p)
        bBadName = qtrue;

    return bBadName;
}

const char *Com_GetArchiveFolder( void )
{
	static char name[ 256 ];
	char        trimmedconfig[ 256 ];
	char       *dot;

	Q_strncpyz( trimmedconfig, config->string, sizeof( trimmedconfig ) );
	dot = strchr( trimmedconfig, '.' );
	if ( dot ) {
		*dot = 0;
	}

	Com_sprintf( name, sizeof( name ), "%s/%s", "save", trimmedconfig );
	return name;
}

const char *Com_GetArchiveFileName( const char *gameName, const char *filename, const char *extension, qboolean tempFile )
{
	static char name[ 256 ];
	char        safename[ 256 ];
	int         i;

	Q_strncpyz( name, Com_GetArchiveFolder(), sizeof( name ) );
	Q_strcat( name, sizeof( name ), "/" );

	if ( !tempFile && gameName && gameName[ 0 ]
		&& ( !extension || Q_stricmp( extension, "ssv" ) ) ) {
		Q_strcat( name, sizeof( name ), gameName );
		Q_strcat( name, sizeof( name ), "/" );
	}

	if ( filename ) {
		COM_StripExtension( filename, safename, sizeof( safename ) );
		for ( i = 0; safename[ i ]; i++ ) {
			if ( safename[ i ] == '/' || safename[ i ] == '\\' ) {
				safename[ i ] = '_';
			}
		}
		Q_strcat( name, sizeof( name ), safename );
	}

	if ( extension ) {
		Q_strcat( name, sizeof( name ), "." );
		Q_strcat( name, sizeof( name ), extension );
	}

	return name;
}

const char *Com_GetArchiveSlotFolder( const char *gameName )
{
	static char name[ 256 ];
	Com_sprintf( name, sizeof( name ), "%s/%s", Com_GetArchiveFolder(), gameName );
	return name;
}

void Com_DeleteSublevelFiles( const char *gameName )
{
	char **list;
	int    numfiles;
	int    i;

	list = FS_ListFiles( Com_GetArchiveFileName( gameName, NULL, NULL, qfalse ), "ssb", qfalse, &numfiles );
	for ( i = 0; i < numfiles; i++ ) {
		FS_Remove_HomeData( Com_GetArchiveFileName( gameName, list[ i ], "ssb", qfalse ) );
		FS_Remove_HomeData( Com_GetArchiveFileName( gameName, list[ i ], "sub", qfalse ) );
		FS_Remove_HomeData( Com_GetArchiveFileName( gameName, list[ i ], "tga", qfalse ) );
	}
	FS_FreeFileList( list );
}

void Com_WipeSavegame( const char *savename, qboolean deep )
{
	Com_DPrintf( "Com_WipeSaveGame(%s)\n", savename );

	if ( deep ) {
		FS_Remove_HomeData( Com_GetArchiveFileName( savename, savename, "spv", qtrue ) );
		return;
	}

	FS_Remove_HomeData( Com_GetArchiveFileName( NULL, savename, "ssv", qfalse ) );
	FS_Remove_HomeData( Com_GetArchiveFileName( savename, savename, "sav", qfalse ) );
	FS_Remove_HomeData( Com_GetArchiveFileName( savename, savename, "spv", qfalse ) );
	FS_Remove_HomeData( Com_GetArchiveFileName( savename, savename, "tga", qfalse ) );

	Com_DeleteSublevelFiles( savename );

	FS_Remove_HomeData( Com_GetArchiveFileName( savename, NULL, NULL, qfalse ) );
}

qboolean Com_ShiftedStrStr(const char* shifted, const char* name, int offset) {
	char string[1024];
	int i;

	for (i = 0; name[i]; i++) {
		string[i] = name[i] - offset;
	}
	string[i] = 0;

	return strstr(string, shifted) != NULL;
}

qboolean COM_IsMapValid(const char* name) {
	return qtrue;

}





void Field_Clear(field_t* edit) {
    memset(edit->buffer, 0, MAX_EDIT_LINE);
    edit->cursor = 0;
    edit->scroll = 0;
}

static const char* completionString;
static char shortestMatch[MAX_TOKEN_CHARS];
static int	matchCount;
static field_t* completionField;

static void FindMatches(const char* s) {
    int		i;

    if (Q_stricmpn(s, completionString, strlen(completionString))) {
        return;
    }
    matchCount++;
    if (matchCount == 1) {
        Q_strncpyz(shortestMatch, s, sizeof(shortestMatch));
        return;
    }

    for (i = 0; shortestMatch[i]; i++) {
        if (i >= strlen(s)) {
            shortestMatch[i] = 0;
            break;
        }

        if (tolower(shortestMatch[i]) != tolower(s[i])) {
            shortestMatch[i] = 0;
        }
    }
}

static void PrintMatches(const char* s) {
    if (!Q_stricmpn(s, shortestMatch, strlen(shortestMatch))) {
        Com_Printf("    %s\n", s);
    }
}

static void PrintCvarMatches(const char* s) {
    char value[TRUNCATE_LENGTH];

    if (!Q_stricmpn(s, shortestMatch, strlen(shortestMatch))) {
        Com_TruncateLongString(value, Cvar_VariableString(s));
        Com_Printf("    %s = \"%s\"\n", s, value);
    }
}

static char* Field_FindFirstSeparator(char* s)
{
    int i;

    for (i = 0; i < strlen(s); i++)
    {
        if (s[i] == ';')
            return &s[i];
    }

    return NULL;
}

static qboolean Field_Complete(void)
{
    int completionOffset;

    if (matchCount == 0)
        return qtrue;

    completionOffset = strlen(completionField->buffer) - strlen(completionString);

    Q_strncpyz(&completionField->buffer[completionOffset], shortestMatch,
        sizeof(completionField->buffer) - completionOffset);

    completionField->cursor = strlen(completionField->buffer);

    if (matchCount == 1)
    {
        Q_strcat(completionField->buffer, sizeof(completionField->buffer), " ");
        completionField->cursor++;
        return qtrue;
    }

    Com_Printf("]%s\n", completionField->buffer);

    return qfalse;
}

#ifndef DEDICATED
void Field_CompleteKeyname(void)
{
    matchCount = 0;
    shortestMatch[0] = 0;

    Key_KeynameCompletion(FindMatches);

    if (!Field_Complete())
        Key_KeynameCompletion(PrintMatches);
}
#endif

void Field_CompleteFilename(const char* dir,
    const char* ext, qboolean stripExt, qboolean allowNonPureFilesOnDisk)
{
    matchCount = 0;
    shortestMatch[0] = 0;

    FS_FilenameCompletion(dir, ext, stripExt, FindMatches, allowNonPureFilesOnDisk);

    if (!Field_Complete())
        FS_FilenameCompletion(dir, ext, stripExt, PrintMatches, allowNonPureFilesOnDisk);
}

void Field_CompleteCommand(char* cmd,
    qboolean doCommands, qboolean doCvars)
{
    int		completionArgument = 0;

    cmd = Com_SkipCharset(cmd, " \"");

    Cmd_TokenizeStringIgnoreQuotes(cmd);
    completionArgument = Cmd_Argc();

    if (*(cmd + strlen(cmd) - 1) == ' ')
    {
        completionString = "";
        completionArgument++;
    }
    else
        completionString = Cmd_Argv(completionArgument - 1);


    if (completionArgument > 1)
    {
        char* p;

        if ((p = Field_FindFirstSeparator(cmd)))
            Field_CompleteCommand(p + 1, qtrue, qtrue); 
    }
    else
    {
        if (completionString[0] == '\\' || completionString[0] == '/')
            completionString++;

        matchCount = 0;
        shortestMatch[0] = 0;

        if (strlen(completionString) == 0)
            return;

        if (doCommands)
            Cmd_CommandCompletion(FindMatches);

        if (doCvars)
            Cvar_CommandCompletion(FindMatches);

        if (!Field_Complete())
        {
            if (doCommands)
                Cmd_CommandCompletion(PrintMatches);

            if (doCvars)
                Cvar_CommandCompletion(PrintCvarMatches);
        }
    }
}

void Field_AutoComplete(field_t* field)
{
    completionField = field;

    Field_CompleteCommand(completionField->buffer, qtrue, qtrue);
}

void Com_RandomBytes( byte *string, int len )
{
	int i;

	if( Sys_RandomBytes( string, len ) )
		return;

	Com_Printf( "Com_RandomBytes: using weak randomization\n" );
	for( i = 0; i < len; i++ )
		string[i] = (unsigned char)( rand() % 256 );
}


qboolean Com_IsVoipTarget(uint8_t *voipTargets, int voipTargetsSize, int clientNum)
{
	int index;
	if(clientNum < 0)
	{
		for(index = 0; index < voipTargetsSize; index++)
		{
			if(voipTargets[index])
				return qtrue;
		}
		
		return qfalse;
	}

	index = clientNum >> 3;
	
	if(index < voipTargetsSize)
		return (voipTargets[index] & (1 << (clientNum & 0x07)));

	return qfalse;
}

static qboolean Field_CompletePlayerNameFinal( qboolean whitespace )
{
	int completionOffset;

	if( matchCount == 0 )
		return qtrue;

	completionOffset = strlen( completionField->buffer ) - strlen( completionString );

	Q_strncpyz( &completionField->buffer[ completionOffset ], shortestMatch,
		sizeof( completionField->buffer ) - completionOffset );

	completionField->cursor = strlen( completionField->buffer );

	if( matchCount == 1 && whitespace )
	{
		Q_strcat( completionField->buffer, sizeof( completionField->buffer ), " " );
		completionField->cursor++;
		return qtrue;
	}

	return qfalse;
}

static void Name_PlayerNameCompletion( const char **names, int nameCount, void(*callback)(const char *s) ) 
{
	int i;

	for( i = 0; i < nameCount; i++ ) {
		callback( names[ i ] );
	}
}

qboolean Com_FieldStringToPlayerName( char *name, int length, const char *rawname )
{
	char		hex[5];
	int			i;
	int			ch;

	if( name == NULL || rawname == NULL )
		return qfalse;

	if( length <= 0 )
		return qtrue;

	for( i = 0; *rawname && i + 1 <= length; rawname++, i++ ) {
		if( *rawname == '\\' ) {
			Q_strncpyz( hex, rawname + 1, sizeof(hex) );
			ch = Com_HexStrToInt( hex );
			if( ch > -1 ) {
				name[i] = ch;
				rawname += 4; 
			} else {
				name[i] = *rawname;
			}
		} else {
			name[i] = *rawname;
		}
	}
	name[i] = '\0';

	return qtrue;
}

qboolean Com_PlayerNameToFieldString( char *str, int length, const char *name )
{
	const char *p;
	int i;
	int x1, x2;

	if( str == NULL || name == NULL )
		return qfalse;

	if( length <= 0 )
		return qtrue;

	*str = '\0';
	p = name;

	for( i = 0; *p != '\0'; i++, p++ )
	{
		if( i + 1 >= length )
			break;

		if( *p <= ' ' )
		{
			if( i + 5 + 1 >= length )
				break;

			x1 = *p >> 4;
			x2 = *p & 15;

			str[i+0] = '\\';
			str[i+1] = '0';
			str[i+2] = 'x';
			str[i+3] = x1 > 9 ? x1 - 10 + 'a' : x1 + '0';
			str[i+4] = x2 > 9 ? x2 - 10 + 'a' : x2 + '0';

			i += 4;
		} else {
			str[i] = *p;
		}		
	}
	str[i] = '\0';

	return qtrue;
}

void Field_CompletePlayerName( const char **names, int nameCount )
{
	qboolean whitespace;

	matchCount = 0;
	shortestMatch[ 0 ] = 0;

	if( nameCount <= 0 )
		return;

	Name_PlayerNameCompletion( names, nameCount, FindMatches );

	if( completionString[0] == '\0' )
	{
		Com_PlayerNameToFieldString( shortestMatch, sizeof( shortestMatch ), names[ 0 ] );
	}

	if( completionString[0] != '\0'
		&& Q_stricmp( shortestMatch, completionString ) == 0 
		&& nameCount > 1 ) 
	{
		int i;

		for( i = 0; i < nameCount; i++ ) {
			if( Q_stricmp( names[ i ], completionString ) == 0 ) 
			{
				i++;
				if( i >= nameCount )
				{
					i = 0;
				}

				Com_PlayerNameToFieldString( shortestMatch, sizeof( shortestMatch ), names[ i ] );
				break;
			}
		}
	}

	if( matchCount > 1 )
	{
		Com_Printf( "]%s\n", completionField->buffer );
		
		Name_PlayerNameCompletion( names, nameCount, PrintMatches );
	}

	whitespace = nameCount == 1? qtrue: qfalse;
	if( !Field_CompletePlayerNameFinal( whitespace ) )
	{

	}
}

int QDECL Com_strCompare( const void *a, const void *b )
{
    const char **pa = (const char **)a;
    const char **pb = (const char **)b;
    return strcmp( *pa, *pb );
}


#ifdef __cplusplus
}
#endif

char *Com_GetGameTypeName(int gametype, char *buf, int bufsize)
{
    if (!buf) {
        return buf;
    }

    if (gametype == 2) {
        convertStringResourceIds("CaptureTheFlag", buf, bufsize);   
    } else if (gametype == 3) {
        convertStringResourceIds("BombDiffusion", buf, bufsize);    
    } else if (gametype == 1) {
        convertStringResourceIds("TeamHolomatch", buf, bufsize);    
    } else if (gametype == 0) {
        strncpy(buf, "Holomatch", bufsize);                         
    } else {
        convertStringResourceIds("Holomatch", buf, bufsize);        
    }

    return buf;
}
