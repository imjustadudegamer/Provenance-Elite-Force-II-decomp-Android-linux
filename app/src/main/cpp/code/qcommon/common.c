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

#include "qa_ui.h"

#ifndef DEDICATED
#  include "../uilib/ui_public.h"
#endif

#define PROBE_GET(probe_n, probe_d, probe_f) Cvar_Get(probe_n, probe_d, probe_f)
#define PROBE_PRINT                          Com_Printf
#include "qa_probe.h"


void InitializeCinematicSystem(void);

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_NUM_ARGVS	50

int		com_argc;
char	*com_argv[MAX_NUM_ARGVS+1];

jmp_buf abortframe;

/* the clean-quit latch: Com_Quit_f longjmps to abortframe after setting it, so the
   frame loop unwinds and main returns to the platform instead of calling exit() */
static qboolean com_quitRequested;

qboolean Com_QuitRequested( void ) { return com_quitRequested; }


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
cvar_t	*com_maxfpsMinimized;	/* EF1's background rungs, common.c:2904-2907 */
cvar_t	*com_maxfpsUnfocused;
cvar_t	*com_timedemo;
static cvar_t	*com_frameWait;
static cvar_t	*com_frameWaitMargin;
static cvar_t	*com_minimizedCvar;
static cvar_t	*com_unfocusedCvar;
cvar_t	*com_sv_running;
cvar_t	*com_cl_running;
cvar_t	*com_viewlog;
cvar_t	*com_logfile;
cvar_t	*com_execAfterConfig;
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

/* --- print-path liveness --------------------------------------------------
 * `Com_RealPrintf' holds a recursion guard across its whole body and every sink it calls sits inside that window, so a longjmp out of it used to leave the
 * guard set and every later Com_Printf return at the guard -- both sinks silent and the error's own text eaten (the 2026-10-19 menu-to-world silence).  The
 * guard is per-thread and `Com_PrintRecover' clears it from `Com_Error' and once per `Com_Frame', and abandons a redirect whose buffer is still installed. */
static _Thread_local int	t_printRecursion = 0;
static _Thread_local int	t_printOpeningLog = 0;
static int					com_printRecovers = 0;
static qboolean				qconsoleEverOpened = qfalse;

static void Com_PrintRecover( void ) {
	if ( t_printRecursion || t_printOpeningLog ) {
		com_printRecovers++;
		/* One loud row, capped, instead of a silent wedge forever. */
		if ( com_printRecovers <= 4 ) {
			Com_Printf( "[CORE:print] recovered a wedged print guard "
						"(recursion=%d opening=%d recoveries=%d)\n",
						t_printRecursion, t_printOpeningLog, com_printRecovers );
		}
		t_printRecursion = 0;
		t_printOpeningLog = 0;
	}

	if ( rd_buffer ) {
		/* Abandon without flushing: the flush target may be the frame that
		 * just unwound. */
		rd_buffer = NULL;
		rd_buffersize = 0;
		rd_flush = NULL;
		if ( com_printRecovers <= 4 ) {
			Com_Printf( "[CORE:print] abandoned an un-closed print redirect\n" );
		}
	}
}

/* Gated heartbeat: one row every `sec' seconds from the frame loop, so a boot
 * that goes quiet PROVES it instead of leaving a hole that looks like a load
 * that never happened.  Silent by default; `qa_probe' is the master switch. */
static void Com_PrintHeartbeat( void ) {
	static cvar_t	*heartbeatSecs;
	static int		lastBeat;
	int				now;
	int				beat;

	if ( !PROBE_On() ) {
		lastBeat = 0;
		return;
	}

	if ( !heartbeatSecs ) {
		heartbeatSecs = Cvar_Get( "qa_printHeartbeat", "5", CVAR_TEMP );
	}

	beat = heartbeatSecs->integer;
	if ( beat < 1 ) {
		beat = 1;
	}

	now = Sys_Milliseconds() / 1000;
	if ( lastBeat && ( now - lastBeat ) < beat ) {
		return;
	}
	lastBeat = now;

	Com_Printf( "[CORE:print] alive frames=%d logfile=%d recoveries=%d "
				"sinks=stderr+qconsole\n",
				com_frameNumber, logfile, com_printRecovers );
}

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
	if (t_printRecursion) {
		return;
	}
	t_printRecursion++;

	if ( com_printwarnings && !com_printwarnings->integer && level ) {
		t_printRecursion--;
		return;
	}

	if ( rd_buffer ) {
		if ((strlen (msg) + strlen(rd_buffer)) > (rd_buffersize - 1)) {
			rd_flush(rd_buffer);
			*rd_buffer = 0;
		}
		Q_strcat(rd_buffer, rd_buffersize, msg);
		t_printRecursion--;
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
		if ( !logfile && FS_Initialized() && !t_printOpeningLog) {
			struct tm *newtime;
			time_t aclock;

      t_printOpeningLog = 1;

			time( &aclock );
			newtime = localtime( &aclock );

			/* The first open of a boot truncates, as retail does.  Any later
			 * open is a heal after a filesystem rebuild tore the handle down,
			 * and that one appends so the rows already in the file survive as
			 * evidence. */
			if ( qconsoleEverOpened ) {
				logfile = FS_FOpenFileAppend_HomeData( "qconsole.log" );
			} else {
				logfile = FS_FOpenFileWrite_HomeData( "qconsole.log" );
			}

			if(logfile)
			{
				qconsoleEverOpened = qtrue;

				Com_Printf( "logfile opened on %s\n", asctime( newtime ) );

				if ( com_logfile->integer > 2 )
				{
					FS_ForceFlush(logfile);
				}
			}

			if (!logfile)
			{
				Com_Printf("Opening qconsole.log failed!\n");
				Cvar_SetValue("logfile", 0);
			}

      t_printOpeningLog = 0;
		}
		if (logfile && FS_Initialized()) {
			size_t msgLen = strlen(msg);

			if ( !FS_WriteHandleLive( logfile ) ) {
				/* A filesystem rebuild (FS_Restart on a map load) tears down
				 * every handle; the stale one used to raise ERR_DROP out of
				 * this very function and wedge the guard above.  Drop it, say
				 * so with the guard released (the same idiom the open block
				 * above uses), and let the next print reopen qconsole.log. */
				logfile = 0;
				t_printRecursion--;
				Com_Printf( "[CORE:print] qconsole.log handle went stale "
							"at frames=%d; reopening\n", com_frameNumber );
				t_printRecursion++;
			} else if (msgLen > 0) {
				FS_Write(msg, msgLen, logfile);
			}
		}
    }

    t_printRecursion--;
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

/* Retail Com_Error @0x000C8460 has no empty-message case: it formats what it was handed, so its
   ERR_DISCONNECT arm's `strlen( com_errorMessage )' test then picks `CL_Disconnect( 1 )' -- the
   silent disconnect -- and writing anything into the buffer here would pick
   `CL_AbnormalDisconnect' instead. Report the code, the last command tokenised and the frames,
   capped, through the console, which the fault sink's captured stderr stream does not reach. */
static void Com_ErrorFrames( const char *text ) {
	Com_Printf( "%s", text );
}

static void Com_ErrorNoText( int code ) {
	static int	s_rows;
	static const char	*const codeName[] = { "ERR_FATAL", "ERR_DROP", "ERR_DISCONNECT", "ERR_NEED_CD" };

	if ( s_rows >= 4 ) {
		return;
	}
	s_rows++;

	Com_Printf( "Com_Error: code=%d (%s) was raised with NO text\n", code,
		( code >= 0 && code < 4 ) ? codeName[code] : "unknown code" );
	Com_Printf( "  last command: '%s %s'\n", Cmd_Argv( 0 ), Cmd_Args() );
	Sys_PrintBackTraceTo( Com_ErrorFrames );
}

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
		Com_ErrorNoText( code );
	} else {
#ifdef __ANDROID__
		/* ERR_DISCONNECT is a normal gameplay event (leaving a server or the
		   menu); only a real engine error reaches logcat. */
		if ( code != ERR_DISCONNECT ) {
			Sys_AndroidLog( ANDROID_LOG_ERROR,
				va( "ENGINE ERROR code=%d: %s\n", code, com_errorMessage ) );
		}
#endif
	}

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

	/* Every arm below longjmps or exits.  Drop the print-path state first:
	 * an error raised from inside Com_RealPrintf leaves its recursion guard
	 * set, and the ERR_DROP banner above is itself a Com_Printf, so the
	 * banner never reached either sink while the guard was wedged. */
	Com_PrintRecover();

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
#ifdef __ANDROID__
	/* exit() from this thread kills the process under the activity's own UI render
	   thread; unwinding to main lets the UI thread finish its window first. */
	com_quitRequested = qtrue;
	Sys_ShutdownPlatform( 0 );
	longjmp( abortframe, -1 );
#else
	Sys_Quit ();
#endif
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
			/* retail @0x000C87A0 sets through Cvar_Set, i.e. forced: the command
			   line must beat ROM/INIT/CHEAT/LATCH.  CVAR_USER_CREATED stays on
			   the create path only -- stamping an existing cvar would let
			   Cvar_Restart unset it out of the table. */
			if ( Cvar_Flags( s ) == CVAR_NONEXISTENT ) {
				Cvar_Get( s, Cmd_ArgsFrom(2), CVAR_USER_CREATED );
			} else {
				Cvar_Set2( s, Cmd_ArgsFrom(2), qtrue );
			}
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

	if ( Q_stricmp( configname, "config.cfg" ) && Q_stricmp( configname, "Android.cfg" ) ) {
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
		if ( PROBE_On() ) {
			Com_Printf( "[EFFECTS:saveid] Android has no OS user; adopted the retail save "
				"profile '%s' so retail's saves resolve\n", adopted );
		}
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
		if ( PROBE_On() ) {
			Com_Printf( "[EFFECTS:saveid] migrated legacy save file '%s' -> '%s'\n", from, to );
		}
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

	developer = Cvar_Get( "developer", "0", CVAR_CHEAT );

	CL_InitKeyCommands();

	com_homepath = Cvar_Get("com_homepath", "", CVAR_INIT|CVAR_PROTECTED);

	FS_InitFilesystem ();

	Com_InitJournaling();

	TIKI_Begin();

	Cbuf_Execute( 0 );

	/* ONE boot cfg is exec'd, and it is not a defaults file: `global/game.cfg' ships inside the retail pak and carries the game-side `setd' values the engine has nowhere
	   else -- above all `setd introCommand ...', the ONLY thing that starts the intro.  Applied here, before config.cfg, exactly where retail applies it, so the archived
	   config wins; removing this exec turned the intro into a black screen until ESC.  `exec global/default.cfg' stays gone: that IS the defaults file, folded in below. */
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
#ifdef __ANDROID__
			/* no OS user to name a profile after; retail's own fallback is the
			   literal "config", which names no profile anyone can share */
			Q_strncpyz( configname, "Android", sizeof( configname ) );
#else
			Q_strncpyz( configname, "config", sizeof( configname ) );
#endif
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

	cvar_inConfig = qtrue;
	Cbuf_Execute( 0 );
	cvar_inConfig = qfalse;

	/* the port's one defaults source, folded in once on EVERY platform: the compiled EF2_ANDROID_DEFAULT_COMMANDS (the union of the shipped defaultcontrols.cfg
	   and defaultoptions.cfg, platform-neutral where those are not).  The gate is `port_defaultsSeed', a cvar only this code writes, so a seed bump is the only way
	   new rows reach an install that already archived an older one -- this block runs AFTER the config exec and therefore beats the archived value.  The pin is
	   registered every boot, not only when the seed runs: it is the one engine-internal row the config carries, and an unregistered pin would read 0 here and
	   re-seed every boot. */
#define PORT_DEFAULTS_SEED 9
	Cvar_Get( "port_defaultsSeed", "0", CVAR_ARCHIVE );
	if ( Cvar_VariableIntegerValue( "port_defaultsSeed" ) < PORT_DEFAULTS_SEED )
	{
		Cbuf_AddText( EF2_ANDROID_DEFAULT_COMMANDS );
		Cbuf_Execute( 0 );
		Cvar_Set( "port_defaultsSeed", XSTRING( PORT_DEFAULTS_SEED ) );

		Com_WriteConfigToFile( EF2_CONFIG_FILE );
	}

	/* No config row may leave the engine bindless: every gameplay button and every
	   pad button is a lookup in the bind tables, and a config that carried its cvars
	   and no binds boots with the mouse look alive and nothing else.  A config with
	   any bind row at all is left exactly as it is; an empty table replays the same
	   default rows the seed block carries, after the config has had its say and
	   before the per-boot override slot, so an explicit unbind is still a state the
	   next boot repairs and a launcher override still wins.

	   The repair is deliberately not written back here.  The renderer's archived
	   cvars are not registered yet, so a write now would replace the file with one
	   that lost them; the rows live in the tables for this session and reach the
	   file with the next archived write, and a boot without one replays them again. */
#ifndef DEDICATED
	if ( !Key_BindCount() )
	{
		Cbuf_AddText( EF2_DEFAULT_BIND_COMMANDS );
		Cbuf_Execute( 0 );

		if ( PROBE_On() ) {
			Com_Printf( "[PROBE:input:binds] config carried no binds; replayed %d default rows\n",
				EF2_ANDROID_DEFAULT_BIND_COUNT );
		}
	}
#endif

	/* The one exec slot after config.cfg and before the command line is
	   re-applied: a file named here holds per-boot overrides that beat the
	   config and lose only to an explicit +set.  LATCHed names bind here
	   because no subsystem has registered them yet. */
	{
		const char *overrideCfg = Cvar_VariableString( "com_execAfterConfig" );

		if ( overrideCfg && overrideCfg[ 0 ] && FS_FileExists_HomeData( overrideCfg ) ) {
			Cbuf_AddText( va( "exec %s\n", overrideCfg ) );
			Cbuf_Execute( 0 );
			Com_Printf( "Exec: %s (launcher overrides)\n", overrideCfg );
		}
	}

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

	/* the seed above ran on this boot (or an earlier one), so its values must
	   reach the archive on either platform, not only Android.  Keyed on the
	   seed's own cvar, so a stray or zero value cannot switch archiving off. */
	if ( Cvar_VariableIntegerValue( "port_defaultsSeed" ) )
		cvar_modifiedFlags |= CVAR_ARCHIVE;

	/* the standing 60, not the shipped 85 */
	com_maxfps = Cvar_Get( "com_maxfps", "60", CVAR_ARCHIVE );
	com_maxfpsMinimized = Cvar_Get( "com_maxfpsMinimized", "0", CVAR_ARCHIVE );
	com_maxfpsUnfocused = Cvar_Get( "com_maxfpsUnfocused", "0", CVAR_ARCHIVE );
	/* limit-pace: PORT-ONLY, beyond retail's list above.  Safe defaults and NOT
	   or experiment must opt out explicitly rather than inherit a stored value. */
	com_frameWait       = Cvar_Get( "com_frameWait", "1", 0 );
	com_frameWaitMargin = Cvar_Get( "com_frameWaitMargin", "1", 0 );
	deathmatch = Cvar_Get( "deathmatch", "0", 0 );
	paused = Cvar_Get( "paused", "0", CVAR_ROM );
	com_canAutoSaveNow = Cvar_Get( "com_canAutoSaveNow", "0", CVAR_ROM );
	fps = Cvar_Get( "fps", "0", 0 );
	/* A build that arms the probes writes its own log file; the shipped build
	   writes nothing.  3 is the all-rows level and forces the flush, so
	   qconsole.log in the homepath outlives the logcat ring. */
#ifdef PROBE_BUILD_SILENT
	com_logfile = Cvar_Get("logfile", "0", CVAR_TEMP);
#else
	com_logfile = Cvar_Get("logfile", "3", CVAR_TEMP);
#endif
	/* the per-boot override file; CVAR_TEMP keeps it out of the archive */
	com_execAfterConfig = Cvar_Get( "com_execAfterConfig", "", CVAR_TEMP );
	com_timescale = Cvar_Get( "timescale", "1", CVAR_CHEAT | CVAR_SYSTEMINFO );
	com_fixedtime = Cvar_Get( "fixedtime", "0", CVAR_CHEAT );
	com_showtrace = Cvar_Get( "com_showtrace", "0", CVAR_CHEAT );
	com_dropsim = Cvar_Get( "com_dropsim", "0", CVAR_CHEAT );
	com_viewlog = Cvar_Get( "viewlog", "0", CVAR_CHEAT );
	com_speeds = Cvar_Get( "com_speeds", "0", 0 );
	com_timedemo = Cvar_Get( "timedemo", "0", CVAR_CHEAT );
	/* instrument, not a setting: it gates one probe row, and qa_probe is the master gate */
	Cvar_Get( "mem_churnStats", "1", 0 );
#ifdef PROBE_BUILD_SILENT
	/* the shipped build arms nothing: ROM keeps a config row or a set from
	   moving the one switch the ~40 direct readers consult */
	Cvar_Get( "qa_probe", "0", CVAR_ROM );
#elif defined(__ANDROID__)
	/* the property arms qa_probe itself, so the UI's own override reads it too */
	if ( Sys_AndroidDiagOn() ) {
		Cvar_Set( "qa_probe", "1" );
	}
#endif
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

	s = va( "%s %s %s %s", "Elite Force II", "1.10", PLATFORM_STRING, "Oct  9 2003");
	com_version = Cvar_Get( "version", s, CVAR_ROM | CVAR_SERVERINFO );
	// Retail registers "protocol" in SV_Init, not here (SV_Init @0x001DF3E0,
	/* Retail's corner label links `shortversion'; this port's builds answer with
	   their own number (the same PRODUCT_VERSION_FULL the banner prints), not
	   retail's 1.10. */
	com_shortversion = Cvar_Get( "shortversion", PRODUCT_VERSION, CVAR_ROM );


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

	// cfg-mt P5: the ONE unified file, not config->string.
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

#define SMP_BUDGET_WINDOW_MS		1000	
#define SMP_BUDGET_MAX_FRAMES		3600	
#define SMP_BUDGET_MAX_ZONES		11		

static int	smp_budgetFrames;
static int	smp_budgetWindowStart;
static int	smp_budgetAudio,  smp_budgetAudioMax;
static int	smp_budgetSleep,  smp_budgetSleepMax;
static int	smp_budgetSwap,   smp_budgetSwapMax;

static qboolean	smp_lastWindowValid;
static int		smp_lastWindowFrames;
static int		smp_lastWindowMean10[SMP_BUDGET_MAX_ZONES];

void SMP_BudgetAdd( int zone, int msec ) {
	if ( msec <= 0 ) {
		return;
	}

	switch ( zone ) {
	case SMP_ZONE_AUDIO:
		smp_budgetAudio += msec;
		if ( msec > smp_budgetAudioMax ) {
			smp_budgetAudioMax = msec;
		}
		break;
	case SMP_ZONE_SLEEP:
		smp_budgetSleep += msec;
		if ( msec > smp_budgetSleepMax ) {
			smp_budgetSleepMax = msec;
		}
		break;
	case SMP_ZONE_SWAP:
		smp_budgetSwap += msec;
		if ( msec > smp_budgetSwapMax ) {
			smp_budgetSwapMax = msec;
		}
		break;
	default:
		break;
	}
}

qboolean SMP_BudgetLastWindow( int *framesOut, int *mean10Out ) {
	int i;

	if ( !smp_lastWindowValid ) {
		return qfalse;
	}
	if ( framesOut ) {
		*framesOut = smp_lastWindowFrames;
	}
	if ( mean10Out ) {
		for ( i = 0; i < SMP_BUDGET_MAX_ZONES; i++ ) {
			mean10Out[i] = smp_lastWindowMean10[i];
		}
	}
	return qtrue;
}

#define SMP_LOAD_MAX_ROWS		720		

int		smp_loadOn = 0;
int		smp_loadDepth = 0;
int		smp_loadInClient = 0;

static int	smp_loadFrames;
static int	smp_loadSums[SMP_LZ_COUNT], smp_loadMaxs[SMP_LZ_COUNT];
/* samples per zone: never fired is not the same fact as fired and cost nothing */
static int	smp_loadSamples[SMP_LZ_COUNT];
static int	smp_loadOther,   smp_loadOtherMax;
static int	smp_loadClient,  smp_loadClientMax;
static int	smp_loadUnacctO, smp_loadUnacctOMax;
static int	smp_loadUnacctC, smp_loadUnacctCMax;
static int	smp_loadAttribO, smp_loadAttribC;
static int	smp_loadRows;
static int	smp_loadStopped;

void SMP_LoadAdd( int zone, int msec, int depth ) {
	if ( zone < 0 || zone >= SMP_LZ_COUNT ) {
		return;
	}
	if ( msec < 0 ) {
		msec = 0;
	}

	smp_loadSums[zone] += msec;
	smp_loadSamples[zone]++;
	if ( msec > smp_loadMaxs[zone] ) {
		smp_loadMaxs[zone] = msec;
	}

	if ( depth == 0 ) {
		if ( smp_loadInClient ) {
			smp_loadAttribC += msec;
		} else {
			smp_loadAttribO += msec;
		}
	}
}

static void SMP_LoadFrame( int otherMsec, int clientMsec ) {
	int	unacctO, unacctC;

	if ( !smp_loadOn ) {
		smp_loadAttribO = smp_loadAttribC = 0;
		return;
	}

	unacctO = otherMsec  - smp_loadAttribO;
	unacctC = clientMsec - smp_loadAttribC;
	if ( unacctO < 0 ) unacctO = 0;
	if ( unacctC < 0 ) unacctC = 0;

	smp_loadOther += otherMsec;	if ( otherMsec  > smp_loadOtherMax  ) smp_loadOtherMax  = otherMsec;
	smp_loadClient += clientMsec;	if ( clientMsec > smp_loadClientMax ) smp_loadClientMax = clientMsec;
	smp_loadUnacctO += unacctO;	if ( unacctO    > smp_loadUnacctOMax ) smp_loadUnacctOMax = unacctO;
	smp_loadUnacctC += unacctC;	if ( unacctC    > smp_loadUnacctCMax ) smp_loadUnacctCMax = unacctC;

	smp_loadFrames++;
	smp_loadAttribO = smp_loadAttribC = 0;
}

static void SMP_LoadReset( void ) {
	int	i;

	for ( i = 0; i < SMP_LZ_COUNT; i++ ) {
		smp_loadSums[i] = smp_loadMaxs[i] = 0;
		smp_loadSamples[i] = 0;
	}
	smp_loadFrames  = 0;
	smp_loadOther   = smp_loadOtherMax   = 0;
	smp_loadClient  = smp_loadClientMax  = 0;
	smp_loadUnacctO = smp_loadUnacctOMax = 0;
	smp_loadUnacctC = smp_loadUnacctCMax = 0;
}

static void SMP_LoadPrint( int frames, int window ) {
	static const char *zoneName[SMP_LZ_COUNT] = {
		"spawn", "cmap", "hunkclr", "svinit", "spawnent",
		"clmap", "hunkusr", "rendreg", "initcg", "cginit",
		"fs", "sndinit", "scr", "cgame", "ui"
	};
	char	line[1024], part[64];
	int		i, mean10;
	struct { const char *name; int sum; int max; } zone[SMP_LZ_COUNT + 4];

	if ( smp_loadStopped ) {
		return;
	}

	if ( frames < 1 ) {
		frames = 1;
	}

	for ( i = 0; i < SMP_LZ_COUNT; i++ ) {
		zone[i].name = zoneName[i];
		zone[i].sum  = smp_loadSums[i];
		zone[i].max  = smp_loadMaxs[i];
	}
	zone[SMP_LZ_COUNT+0].name = "other";	zone[SMP_LZ_COUNT+0].sum = smp_loadOther;	zone[SMP_LZ_COUNT+0].max = smp_loadOtherMax;
	zone[SMP_LZ_COUNT+1].name = "client";	zone[SMP_LZ_COUNT+1].sum = smp_loadClient;	zone[SMP_LZ_COUNT+1].max = smp_loadClientMax;
	zone[SMP_LZ_COUNT+2].name = "unacctO";	zone[SMP_LZ_COUNT+2].sum = smp_loadUnacctO;	zone[SMP_LZ_COUNT+2].max = smp_loadUnacctOMax;
	zone[SMP_LZ_COUNT+3].name = "unacctC";	zone[SMP_LZ_COUNT+3].sum = smp_loadUnacctC;	zone[SMP_LZ_COUNT+3].max = smp_loadUnacctCMax;

	line[0] = '\0';
	for ( i = 0; i < SMP_LZ_COUNT + 4; i++ ) {
		if ( i < SMP_LZ_COUNT && !smp_loadSamples[i] ) {
			/* not exercised in this window: a dash, not a reading */
			Com_sprintf( part, sizeof( part ), " %s=-", zone[i].name );
			Q_strcat( line, sizeof( line ), part );
			continue;
		}
		mean10 = (int)( ( (long long)zone[i].sum * 10 + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d",
					 zone[i].name, mean10 / 10, mean10 % 10, zone[i].max );
		Q_strcat( line, sizeof( line ), part );
	}

	Com_Printf( "[SMP:load] frames=%d window=%dms%s\n", frames, window, line );

	smp_loadRows++;
	if ( smp_loadRows >= SMP_LOAD_MAX_ROWS ) {
		smp_loadStopped = 1;
		Com_Printf( "[SMP:load] capped at %d rows; no further [SMP:load] lines\n",
					SMP_LOAD_MAX_ROWS );
	}
}

/* The frame cost on a phone can drift with SoC frequency and heat rather than with anything the
   engine did. Nothing else in the tree reads either, so a capture cannot tell a code regression
   from a throttled part without this row. Sampling is one window roll (a few sysfs reads per
   second) and only under the probe gate. / /* GPU clock sources, in the order tried; `hz' marks a
   node reporting Hz. */
typedef struct {
	const char	*path;
	const char	*name;
	int			hz;
} gpuClkSource_t;

#define SMP_GPUCLK_SOURCES	5

static const gpuClkSource_t s_gpuClkSource[SMP_GPUCLK_SOURCES] = {
	{ "/sys/class/devfreq/b00000.qcom,kgsl-3d0/cur_freq", "devfreq-kgsl", 1 },
	{ "/sys/class/kgsl/kgsl-3d0/gpuclk",                  "kgsl-gpuclk",  0 },
	{ "/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq",        "kgsl-devfreq", 0 },
	{ "/sys/kernel/gpu/gpu_clock",                        "kernel-gpu",   0 },
	{ "/sys/class/devfreq/gpufreq/cur_freq",              "devfreq-gpu",  0 },
};

/* First source that opens and holds a number, in kHz; `*srcUsed` is -1 if none. */
static int SMP_GpuClockKhz( const gpuClkSource_t *src, int count, int *srcUsed ) {
	FILE	*fp;
	char	buf[64];
	int		g, khz = -1;

	if ( srcUsed ) {
		*srcUsed = -1;
	}
	if ( !src || count <= 0 ) {
		return -1;
	}

	for ( g = 0; g < count && khz < 0; g++ ) {
		if ( !src[g].path ) {
			continue;
		}
		fp = fopen( src[g].path, "r" );
		if ( !fp ) {
			continue;
		}
		if ( fgets( buf, sizeof( buf ), fp ) ) {
			khz = atoi( buf );
			if ( khz > 0 ) {
				if ( src[g].hz ) {
					/* devfreq reports Hz; the row is kHz */
					khz /= 1000;
				}
				if ( srcUsed ) {
					*srcUsed = g;
				}
			} else {
				khz = -1;
			}
		}
		fclose( fp );
	}
	return khz;
}

static void SMP_ThermalProbe( int window ) {
	static int	rows;
	static int	first;
	int			core, cores, mhz[8];
	char		path[96], buf[64];
	char		line[256], part[32];
	FILE		*fp;

	if ( !PROBE_On() ) {
		return;
	}
	if ( rows > 200 ) {
		return;
	}
	rows++;

	if ( !first ) {
		first = Sys_Milliseconds();
	}

	/* The readable scaling_cur_freq files are the core count here: the dedicated
	 * server links no SDL, so there is no SDL_GetCPUCount to ask. */
	cores = 0;
	for ( core = 0; core < 8; core++ ) {
		mhz[core] = -1;
		Com_sprintf( path, sizeof( path ),
					 "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", core );
		fp = fopen( path, "r" );
		if ( !fp ) {
			continue;
		}
		if ( fgets( buf, sizeof( buf ), fp ) ) {
			mhz[core] = atoi( buf ) / 1000;
			cores = core + 1;
		}
		fclose( fp );
	}

	line[0] = '\0';
	for ( core = 0; core < cores; core++ ) {
		Com_sprintf( part, sizeof( part ), "%s%d", core ? "," : " mhz=", mhz[core] );
		Q_strcat( line, sizeof( line ), part );
	}

	{
		int	zone, zones = 0;
		int	temp[4];

		for ( zone = 0; zone < 4; zone++ ) {
			temp[zone] = -1;
			Com_sprintf( path, sizeof( path ),
						 "/sys/class/thermal/thermal_zone%d/temp", zone );
			fp = fopen( path, "r" );
			if ( !fp ) {
				continue;
			}
			if ( fgets( buf, sizeof( buf ), fp ) ) {
				temp[zone] = atoi( buf );
				zones++;
			}
			fclose( fp );
		}
		if ( zones ) {
			for ( zone = 0; zone < 4; zone++ ) {
				Com_sprintf( part, sizeof( part ), "%s%d", zone ? "," : " milliC=", temp[zone] );
				Q_strcat( line, sizeof( line ), part );
			}
		}
	}

	Com_Printf( "[THERM:cpu] row=%d window=%dms elapsed=%dms cores=%d%s\n",
				rows, window, Sys_Milliseconds() - first, cores, line );

	/* The row names the source that answered; a box with none says so once */
	{
		int		gpuKhz = -1;
		int		src = -1;
		static qboolean reported;

		gpuKhz = SMP_GpuClockKhz( s_gpuClkSource, SMP_GPUCLK_SOURCES, &src );
		if ( gpuKhz < 0 ) {
			if ( !reported ) {
				Com_Printf( "[THERM:gpu] row=%d window=%dms src=unavailable"
							" (no gpu clock node on this machine)\n", rows, window );
				reported = qtrue;
			}
			return;
		}
		Com_Printf( "[THERM:gpu] row=%d window=%dms gpuclk_khz=%d src=%s\n", rows, window,
					gpuKhz, ( src >= 0 && src < SMP_GPUCLK_SOURCES ) ? s_gpuClkSource[src].name
																	 : "none" );
	}
}

static void SMP_BudgetFrame( int serverMsec, int clientMsec, int spinMsec, int totalMsec ) {
	static cvar_t	*smp_frameStats;
	static cvar_t	*smp_loadStats;
	static int		smp_server, smp_serverMax;
	static int		smp_client, smp_clientMax;
	static int		smp_front,  smp_frontMax;
	static int		smp_back,   smp_backMax;
	static int		smp_spin,   smp_spinMax;
	static int		smp_other,  smp_otherMax;
	static int		smp_total,  smp_totalMax;

	int		otherMsec, window, frames, i, mean10;
	char	line[1024], part[64];
	struct { const char *name; int sum; int max; } zone[SMP_BUDGET_MAX_ZONES];

	if ( !smp_frameStats ) {
		smp_frameStats = Cvar_Get( "smp_frameStats", "1", 0 );
	}
	if ( !smp_loadStats ) {
		smp_loadStats = Cvar_Get( "smp_loadStats", "1", 0 );
	}

	smp_loadOn = ( smp_loadStats->integer && PROBE_On() ) ? 1 : 0;

	if ( !smp_frameStats->integer || !PROBE_On() ) {
		smp_budgetFrames = 0;
		smp_budgetWindowStart = 0;
		smp_budgetAudio = smp_budgetAudioMax = 0;
		smp_budgetSleep = smp_budgetSleepMax = 0;
		smp_budgetSwap  = smp_budgetSwapMax  = 0;
		smp_loadOn = 0;
		smp_loadAttribO = smp_loadAttribC = 0;
		SMP_LoadReset();
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

	SMP_LoadFrame( otherMsec, clientMsec );

	smp_server += serverMsec;	if ( serverMsec > smp_serverMax ) smp_serverMax = serverMsec;
	smp_client += clientMsec;	if ( clientMsec > smp_clientMax ) smp_clientMax = clientMsec;
	smp_spin   += spinMsec;	if ( spinMsec   > smp_spinMax   ) smp_spinMax   = spinMsec;
	smp_other  += otherMsec;	if ( otherMsec  > smp_otherMax  ) smp_otherMax  = otherMsec;
	smp_total  += totalMsec;	if ( totalMsec  > smp_totalMax  ) smp_totalMax  = totalMsec;

	smp_front += time_frontend;	if ( time_frontend > smp_frontMax ) smp_frontMax = time_frontend;
	smp_back  += time_backend;		if ( time_backend  > smp_backMax  ) smp_backMax  = time_backend;

	smp_budgetFrames++;

	if ( !smp_budgetWindowStart ) {
		smp_budgetWindowStart = Sys_Milliseconds();
	}

	window = Sys_Milliseconds() - smp_budgetWindowStart;
	if ( window < SMP_BUDGET_WINDOW_MS && smp_budgetFrames < SMP_BUDGET_MAX_FRAMES ) {
		return;
	}

	frames = smp_budgetFrames;
	if ( frames < 1 ) {
		frames = 1;
	}

	zone[0].name  = "server";			zone[0].sum  = smp_server;	zone[0].max  = smp_serverMax;
	zone[1].name  = "client";			zone[1].sum  = smp_client;	zone[1].max  = smp_clientMax;
	zone[2].name  = "renderFront";		zone[2].sum  = smp_front;	zone[2].max  = smp_frontMax;
	zone[3].name  = "renderBack";		zone[3].sum  = smp_back;	zone[3].max  = smp_backMax;
	zone[4].name  = "audio";			zone[4].sum  = smp_budgetAudio;	zone[4].max = smp_budgetAudioMax;
	zone[5].name  = "blocked";			zone[5].sum  = smp_budgetSleep + smp_budgetSwap;
	zone[5].max   = smp_budgetSleepMax + smp_budgetSwapMax;
	zone[6].name  = "sleep";			zone[6].sum  = smp_budgetSleep;	zone[6].max = smp_budgetSleepMax;
	zone[7].name  = "swap";				zone[7].sum  = smp_budgetSwap;		zone[7].max = smp_budgetSwapMax;
	zone[8].name  = "spin";				zone[8].sum  = smp_spin;	zone[8].max  = smp_spinMax;
	zone[9].name  = "other";			zone[9].sum  = smp_other;	zone[9].max  = smp_otherMax;
	zone[10].name = "total";			zone[10].sum = smp_total;	zone[10].max = smp_totalMax;

	line[0] = '\0';
	for ( i = 0; i < SMP_BUDGET_MAX_ZONES; i++ ) {
		mean10 = (int)( ( (long long)zone[i].sum * 10 + frames / 2 ) / frames );
		smp_lastWindowMean10[i] = mean10;	
		Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d",
					 zone[i].name, mean10 / 10, mean10 % 10, zone[i].max );
		Q_strcat( line, sizeof( line ), part );
	}

	smp_lastWindowFrames = frames;
	smp_lastWindowValid  = qtrue;

	Com_Printf( "[SMP:frame] frames=%d window=%dms%s\n", frames, window, line );

	SMP_ThermalProbe( window );

	if ( smp_loadOn ) {
		SMP_LoadPrint( smp_loadFrames, window );
		SMP_LoadReset();
	}

	smp_budgetFrames = 0;
	smp_budgetWindowStart = Sys_Milliseconds();
	smp_budgetAudio = smp_budgetAudioMax = 0;
	smp_budgetSleep = smp_budgetSleepMax = 0;
	smp_budgetSwap  = smp_budgetSwapMax  = 0;
	smp_server = smp_serverMax = 0;
	smp_client = smp_clientMax = 0;
	smp_front  = smp_frontMax  = 0;
	smp_back   = smp_backMax   = 0;
	smp_spin   = smp_spinMax   = 0;
	smp_other  = smp_otherMax  = 0;
	smp_total  = smp_totalMax  = 0;
}


/* The pace clock: microseconds of monotonic time.  Sys_Milliseconds stays the game's
   clock; the frame budget only ever reads deltas off this one. */
static int64_t Com_PaceUsec( void ) {
#ifdef _WIN32
	return (int64_t)Sys_Milliseconds() * 1000;
#else
	struct timespec ts;

	clock_gettime( CLOCK_MONOTONIC, &ts );
	return (int64_t)ts.tv_sec * 1000000ll + (int64_t)ts.tv_nsec / 1000ll;
#endif
}

/* The one frame-budget wait: the whole milliseconds go to the OS and the spin loop
   takes the sub-millisecond tail, so a 60 budget is 16667us and not a 16ms floor. */
static int LIMIT_WaitDecision( int minUsec, int elapsed, int marginUsec, int minimized ) {
	int remain;

	if ( minUsec <= 0 ) {
		return 0;
	}
	if ( elapsed < 0 ) {
		elapsed = 0;
	}
	remain = minUsec - elapsed;
	if ( remain <= 0 ) {
		return 0;
	}
	if ( marginUsec < 0 ) {
		marginUsec = 0;
	}
	if ( minimized ) {
		return remain;
	}
	if ( remain <= marginUsec ) {
		return 0;
	}
	return remain - marginUsec;
}

#define UI_WINDOW_MS		1000
#define UI_MAX_FRAMES		3600
#define UI_MAX_ZONES		8
#define UI_MAX_COUNTERS	6
#define UI_MAX_ROWS		900

int			ui_statsOn;

static int	ui_zoneSum[UI_FAM_COUNT][UI_MAX_ZONES];
static int	ui_zoneMax[UI_FAM_COUNT][UI_MAX_ZONES];
static int	ui_cntSum[UI_FAM_COUNT][UI_MAX_COUNTERS];
static int	ui_cntMax[UI_FAM_COUNT][UI_MAX_COUNTERS];
static int	ui_totalSum[UI_FAM_COUNT];
static int	ui_totalMax[UI_FAM_COUNT];
static int	ui_frames[UI_FAM_COUNT];
static int	ui_windowStart[UI_FAM_COUNT];
static int	ui_rowsEmitted[UI_FAM_COUNT];
static int	ui_frameNamed[UI_FAM_COUNT];

static int	ui_waitSum,   ui_waitMax;
static int	ui_spinSum,   ui_spinMax;
static int	ui_periodSum, ui_periodMax;
static int	ui_minimSum,  ui_minimMax;
static int	ui_paceLastExitUsec;

static const char *ui_familyName[UI_FAM_COUNT] = {
	"ui", "disp", "v3d", "walk", "spin"
};

static const int ui_zoneCount[UI_FAM_COUNT] = {
	UI_UI_ZONES, UI_DISP_ZONES, UI_V3D_ZONES, UI_WALK_ZONES, UI_SPIN_ZONES
};

static const int ui_counterCount[UI_FAM_COUNT] = {
	UI_UI_COUNTERS, UI_DISP_COUNTERS, 0, UI_WALK_COUNTERS, UI_SPIN_COUNTERS
};

static const char *ui_zoneName[UI_FAM_COUNT][UI_MAX_ZONES] = {
	{ "setrt", "filluid", "service", "consoles", "state", "menus", "views", "other" },
	{ "walkup", "disp", "cursor", "printstr", "other", NULL, NULL, NULL },
	{ "scrdraw", "set2d", "draw2d", "cgdraw", "other", NULL, NULL, NULL },
	{ "vis", "hid", "other", NULL, NULL, NULL, NULL, NULL },
	{ "cfg", "vlog", "fnum", "evloop", "other", NULL, NULL, NULL }
};

static int	ui_rootT0[UI_FAM_COUNT];

static const char *ui_counterName[UI_FAM_COUNT][UI_MAX_COUNTERS] = {
	{ "upd", "disp", "draw", "text", NULL, NULL },
	{ "children", "disps", NULL, NULL, NULL, NULL },
	{ NULL, NULL, NULL, NULL, NULL, NULL },
	{ "menus", "visw", "hidw", NULL, NULL, NULL },
	{ "iters", "limited", "min", NULL, NULL, NULL }
};

int UI_Microseconds( void ) {
#ifndef _WIN32
	struct timespec ts;

	clock_gettime( CLOCK_MONOTONIC, &ts );
	return (int)( ( ts.tv_sec % 1000000 ) * 1000000 + ts.tv_nsec / 1000 );
#else
	return Sys_Milliseconds() * 1000;
#endif
}

void UI_On( void ) {
	static cvar_t *v;

	if ( !v ) {
		v = Cvar_Get( "ui_stats", "1", 0 );
	}

	ui_statsOn = ( v && v->integer && PROBE_On() ) ? 1 : 0;
}

static void UI_Reset( int family );
static void UI_AddTenths( int family, int zone, int tenths );

void UI_RootBegin( int family ) {
	UI_On();

	if ( family < 0 || family >= UI_FAM_COUNT ) {
		return;
	}

	ui_rootT0[family] = ui_statsOn ? UI_Microseconds() : 0;
}

void UI_RootEnd( int family ) {
	if ( family < 0 || family >= UI_FAM_COUNT ) {
		return;
	}

	if ( !ui_statsOn ) {
		UI_Reset( family );
		return;
	}

	UI_Frame( family, UI_Microseconds() - ui_rootT0[family] );
}

static void UI_Reset( int family ) {
	int i;

	ui_frames[family] = 0;
	ui_windowStart[family] = 0;
	ui_frameNamed[family] = 0;
	ui_totalSum[family] = ui_totalMax[family] = 0;

	for ( i = 0; i < UI_MAX_ZONES; i++ ) {
		ui_zoneSum[family][i] = ui_zoneMax[family][i] = 0;
	}
	for ( i = 0; i < UI_MAX_COUNTERS; i++ ) {
		ui_cntSum[family][i] = ui_cntMax[family][i] = 0;
	}

	if ( family == UI_FAM_SPIN ) {
		ui_waitSum = ui_waitMax = 0;
		ui_spinSum = ui_spinMax = 0;
		ui_periodSum = ui_periodMax = 0;
		ui_minimSum = ui_minimMax = 0;
		ui_paceLastExitUsec = 0;
	}
}


static void UI_Print( int family, int window ) {
	int		frames, i, mean10, sum, pmean;
	char	line[1024], part[64];

	frames = ui_frames[family];
	if ( frames < 1 ) {
		frames = 1;
	}

	line[0] = '\0';

	for ( i = 0; i < ui_zoneCount[family]; i++ ) {
		sum = ui_zoneSum[family][i];
		mean10 = (int)( ( (long long)sum + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d.%d",
					 ui_zoneName[family][i], mean10 / 10, mean10 % 10,
					 ui_zoneMax[family][i] / 10, ui_zoneMax[family][i] % 10 );
		Q_strcat( line, sizeof( line ), part );
	}

	for ( i = 0; i < ui_counterCount[family]; i++ ) {
		sum = ui_cntSum[family][i];
		mean10 = (int)( ( (long long)sum * 10 + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d",
					 ui_counterName[family][i], mean10 / 10, mean10 % 10,
					 ui_cntMax[family][i] );
		Q_strcat( line, sizeof( line ), part );
	}

	if ( family == UI_FAM_SPIN ) {
		pmean = (int)( ( (long long)ui_waitSum + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " yield=%d.%d/%d.%d",
					 pmean / 10, pmean % 10, ui_waitMax / 10, ui_waitMax % 10 );
		Q_strcat( line, sizeof( line ), part );

		pmean = (int)( ( (long long)ui_spinSum + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " spun=%d.%d/%d.%d",
					 pmean / 10, pmean % 10, ui_spinMax / 10, ui_spinMax % 10 );
		Q_strcat( line, sizeof( line ), part );

		pmean = (int)( ( (long long)ui_periodSum + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " period=%d.%d/%d.%d",
					 pmean / 10, pmean % 10, ui_periodMax / 10, ui_periodMax % 10 );
		Q_strcat( line, sizeof( line ), part );

		pmean = (int)( ( (long long)ui_minimSum * 10 + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " minim=%d.%d/%d",
					 pmean / 10, pmean % 10, ui_minimMax );
		Q_strcat( line, sizeof( line ), part );
	}

	mean10 = (int)( ( (long long)ui_totalSum[family] + frames / 2 ) / frames );
	Com_sprintf( part, sizeof( part ), " total=%d.%d/%d.%d",
				 mean10 / 10, mean10 % 10,
				 ui_totalMax[family] / 10, ui_totalMax[family] % 10 );
	Q_strcat( line, sizeof( line ), part );

	Com_Printf( "[UI:%s] frames=%d window=%dms%s\n",
				ui_familyName[family], frames, window, line );

	ui_rowsEmitted[family]++;
}

void UI_Frame( int family, int totalUsec ) {
	int window;

	if ( family < 0 || family >= UI_FAM_COUNT ) {
		return;
	}

	if ( !ui_statsOn ) {
		UI_Reset( family );
		return;
	}

	if ( totalUsec < 0 ) {
		totalUsec = 0;
	}

	totalUsec /= 100;			

	{
		int ui_other = totalUsec - ui_frameNamed[family];

		if ( ui_other < 0 ) {
			ui_other = 0;
		}

		UI_AddTenths( family, ui_zoneCount[family] - 1, ui_other );
		ui_frameNamed[family] = 0;
	}

	ui_totalSum[family] += totalUsec;
	if ( totalUsec > ui_totalMax[family] ) {
		ui_totalMax[family] = totalUsec;
	}

	ui_frames[family]++;

	if ( !ui_windowStart[family] ) {
		ui_windowStart[family] = Sys_Milliseconds();
	}

	window = Sys_Milliseconds() - ui_windowStart[family];
	if ( window < UI_WINDOW_MS && ui_frames[family] < UI_MAX_FRAMES ) {
		return;
	}

	if ( ui_rowsEmitted[family] < UI_MAX_ROWS ) {
		UI_Print( family, window );
	} else if ( ui_rowsEmitted[family] == UI_MAX_ROWS ) {
		ui_rowsEmitted[family]++;
		Com_Printf( "[UI:%s] capped at %d rows; no further lines\n",
					ui_familyName[family], UI_MAX_ROWS );
	}

	UI_Reset( family );
	ui_windowStart[family] = Sys_Milliseconds();
}

static void UI_AddTenths( int family, int zone, int tenths ) {
	if ( family < 0 || family >= UI_FAM_COUNT || zone < 0 || zone >= UI_MAX_ZONES ) {
		return;
	}
	if ( tenths < 0 ) {
		tenths = 0;
	}
	if ( tenths == 0 ) {
		return;
	}

	ui_zoneSum[family][zone] += tenths;
	if ( tenths > ui_zoneMax[family][zone] ) {
		ui_zoneMax[family][zone] = tenths;
	}

	if ( zone != ui_zoneCount[family] - 1 ) {
		ui_frameNamed[family] += tenths;
	}
}

void UI_Add( int family, int zone, int usec ) {
	if ( usec <= 0 ) {
		return;
	}

	UI_AddTenths( family, zone, usec / 100 );
}

void UI_Bump( int family, int counter, int n ) {
	if ( family < 0 || family >= UI_FAM_COUNT || counter < 0 || counter >= UI_MAX_COUNTERS ) {
		return;
	}
	if ( n <= 0 ) {
		return;
	}

	ui_cntSum[family][counter] += n;
	if ( n > ui_cntMax[family][counter] ) {
		ui_cntMax[family][counter] = n;
	}
}

static void UI_PaceAdd( int *sum, int *max, int units ) {
	if ( units <= 0 ) {
		return;
	}

	*sum += units;
	if ( units > *max ) {
		*max = units;
	}
}

void Com_Frame( void ) {

	int		msec;
	int64_t		minUsec;
	int64_t		paceSpent;
	static int64_t	lastPace = 0;	/* the pace deadline: the end of the last spin */
	static int	s_pacePeriod = 0;	/* last full period, what the budget governs */
	static int	limitBias = 0;	/* drift accumulator, one budget each way */
	static int	lastTime = 0;
	int		limitRemain = 0;
	int		ui_unfocused = 0;

	int		timeBeforeFirstEvents;
	int		timeBeforeServer;
	int		timeBeforeEvents;
	int		timeBeforeClient;
	int		timeAfter;

	int		smpFrameStart, smpSpinStart, smpAfterSpin, smpBeforeServer, smpAfterServer, smpClientStart, smpFrameEnd;
	/* the main-thread frame split: server against everything after it */
	static int s_splitT0, s_splitFrames, s_splitSv, s_splitRest, s_splitEv, s_splitCl;
	int        s_splitRestT0, s_splitNow, s_splitEvT0 = 0;

	int		ui_cfgUsec = 0, ui_vlogUsec = 0, ui_fnumUsec = 0;
	int		ui_evUsec = 0, ui_spinIters = 0;

	int		ui_minimized = 0;
	int		ui_waitUsec = 0, ui_spinUsec = 0, ui_periodUsec = 0;

	if ( setjmp (abortframe) ) {
		/* A longjmp landed here.  Whatever the frame was holding, including
		 * the print recursion guard, is stale; clear it before the next row
		 * goes out so a wedge cannot outlive the frame that raised it. */
		Com_PrintRecover();
		return;
	}

	Com_PrintRecover();
	Com_PrintHeartbeat();

	UI_ROOT_BEGIN( UI_FAM_SPIN );

	smpFrameStart = Sys_Milliseconds();

	// game_export_t::SetFrameNumber, which retail's game_export_t has never had, so it
#ifndef DEDICATED
	if (!com_dedicated || !com_dedicated->integer)
	{
		ui_fnumUsec = ( ui_statsOn ? UI_Microseconds() : 0 );
		CL_SetFrameNumber(com_frameNumber);
		if ( ui_statsOn ) { ui_fnumUsec = UI_Microseconds() - ui_fnumUsec; }
	}
#endif

	timeBeforeFirstEvents =0;
	timeBeforeServer =0;
	timeBeforeEvents =0;
	timeBeforeClient = 0;
	timeAfter = 0;

	ui_cfgUsec = ( ui_statsOn ? UI_Microseconds() : 0 );
	Com_WriteConfiguration();
	if ( ui_statsOn ) { ui_cfgUsec = UI_Microseconds() - ui_cfgUsec; }

#ifndef DEDICATED
    if (com_viewlog->modified) {
        if (!com_dedicated || !com_dedicated->integer) {
            ui_vlogUsec = ( ui_statsOn ? UI_Microseconds() : 0 );
            Sys_ShowConsole(com_viewlog->integer, qfalse);
            if ( ui_statsOn ) { ui_vlogUsec = UI_Microseconds() - ui_vlogUsec; }
        }

        com_viewlog->modified = qfalse;
    }
#endif

	if ( com_speeds->integer ) {
		timeBeforeFirstEvents = Sys_Milliseconds ();
	}

    /* EF1's frame budget (VoyagerNX common.c:3248-3266) in usec; the base is
     * recomputed every frame and the bias reads the last full period, never the
     * work window a budget does not govern. */
    if ( !com_minimizedCvar ) {
        com_minimizedCvar = Cvar_Get( "com_minimized", "0", 0 );
    }
    if ( !com_unfocusedCvar ) {
        com_unfocusedCvar = Cvar_Get( "com_unfocused", "0", 0 );
    }
    ui_minimized  = ( com_minimizedCvar && com_minimizedCvar->integer ) ? 1 : 0;
    ui_unfocused  = ( com_unfocusedCvar && com_unfocusedCvar->integer ) ? 1 : 0;

    if ( com_dedicated->integer || com_timedemo->integer ) {
        minUsec = 0;
    } else if ( ui_minimized && com_maxfpsMinimized->integer > 0 ) {
        minUsec = 1000000ll / com_maxfpsMinimized->integer;
    } else if ( ui_unfocused && com_maxfpsUnfocused->integer > 0 ) {
        minUsec = 1000000ll / com_maxfpsUnfocused->integer;
    } else if ( com_maxfps->integer > 0 ) {
        minUsec = 1000000ll / com_maxfps->integer;
    } else {
        minUsec = 0;
    }

    if ( minUsec > 0 && s_pacePeriod > 0 ) {
        limitBias += s_pacePeriod - (int)minUsec;
        if ( limitBias > (int)minUsec ) {
            limitBias = (int)minUsec;    /* EF1's clamp: one frame of catch-up, no more */
        }
        if ( limitBias < -(int)minUsec ) {
            limitBias = -(int)minUsec;   /* and one frame of slack the other way */
        }

        minUsec -= limitBias;
        if ( minUsec < 1000 ) {
            minUsec = 1000;    /* a spent budget is a fast frame, never a hot loop */
        }
    }

    paceSpent = Com_PaceUsec() - lastPace;    /* work already spent of this window */
    if ( paceSpent < 0 ) {
        paceSpent = 0;
    }

    if ( minUsec > 0 && com_frameWait && com_frameWait->integer && !com_dedicated->integer ) {
        limitRemain = LIMIT_WaitDecision( (int)minUsec, (int)paceSpent,
                                          com_frameWaitMargin->integer * 1000,
                                          ui_minimized );
        if ( limitRemain > 0 ) {
            int limitT0 = ( ui_statsOn ? UI_Microseconds() : 0 );

            /* whole milliseconds to the OS, the spin loop takes the tail */
            Sys_Sleep( limitRemain / 1000 );

            if ( ui_statsOn ) { ui_waitUsec = UI_Microseconds() - limitT0; }
        }
    }

    smpSpinStart = Sys_Milliseconds();

    {
    int limitSpinT0 = ( ui_statsOn ? UI_Microseconds() : 0 );

    do
    {
        int ui_evT = ( ui_statsOn ? UI_Microseconds() : 0 );

        com_frameTime = Com_EventLoop();

        if ( ui_statsOn ) { ui_evUsec += UI_Microseconds() - ui_evT; }
        ui_spinIters++;

        if ( lastTime > com_frameTime ) {
            lastTime = com_frameTime;
        }

        msec = com_frameTime - lastTime;
    } while ( minUsec > 0 && Com_PaceUsec() - lastPace < minUsec );

    if ( ui_statsOn ) { ui_spinUsec = UI_Microseconds() - limitSpinT0; }
    }

    {
    int64_t paceEnd = Com_PaceUsec();

    /* the period the budget governs: deadline to deadline, EF1's com_frameTime - lastTime */
    s_pacePeriod = ( lastPace > 0 && paceEnd > lastPace ) ? (int)( paceEnd - lastPace ) : 0;
    if ( s_pacePeriod > 1000000 ) {
        s_pacePeriod = 1000000;    /* a stall is not a frame */
    }
    lastPace = paceEnd;
    }
    lastTime = com_frameTime;

	if ( PROBE_On() ) {
		static int s_limitRows, s_limitOffRows;

		if ( minUsec > 0 ) {
			if ( PROBE_Throttle( &s_limitRows, 12, 60 ) ) {
				PROBE( "limit",
					   "maxfps=%d budget=%dusec measured=%dusec bias=%dusec spin=%d governor=%s\n",
					   com_maxfps->integer, (int)minUsec, s_pacePeriod, limitBias, ui_spinIters,
					   limitRemain > 0 ? "sleep" : ( ui_spinIters > 1 ? "spin" : "work" ) );
			}
		} else if ( PROBE_Throttle( &s_limitOffRows, 0, 300 ) ) {
			PROBE( "limit", "maxfps=0 budget=0usec measured=%dusec governor=off\n", s_pacePeriod );
		}
	}

	smpAfterSpin = Sys_Milliseconds();

	if ( ui_statsOn ) {
		int limitNow = UI_Microseconds();

		if ( ui_paceLastExitUsec != 0 ) {
			ui_periodUsec = limitNow - ui_paceLastExitUsec;
			if ( ui_periodUsec < 0 ) {
				ui_periodUsec = 0;
			}
		}
		ui_paceLastExitUsec = limitNow;
	} else {
		ui_paceLastExitUsec = 0;
	}

	if ( ui_statsOn ) {
		UI_Add( UI_FAM_SPIN, UI_SPZ_cfg,    ui_cfgUsec );
		UI_Add( UI_FAM_SPIN, UI_SPZ_vlog,   ui_vlogUsec );
		UI_Add( UI_FAM_SPIN, UI_SPZ_fnum,   ui_fnumUsec );
		UI_Add( UI_FAM_SPIN, UI_SPZ_evloop, ui_evUsec );

		UI_Bump( UI_FAM_SPIN, UI_SC_iters, ui_spinIters );
		if ( ui_spinIters > 1 ) {
			UI_Bump( UI_FAM_SPIN, UI_SC_limited, 1 );
		}
		UI_Bump( UI_FAM_SPIN, UI_SC_min, (int)( minUsec / 1000 ) );

		UI_PaceAdd( &ui_waitSum,   &ui_waitMax,   ui_waitUsec / 100 );
		UI_PaceAdd( &ui_spinSum,   &ui_spinMax,   ui_spinUsec / 100 );
		UI_PaceAdd( &ui_periodSum, &ui_periodMax, ui_periodUsec / 100 );
		UI_PaceAdd( &ui_minimSum,  &ui_minimMax,  ui_minimized );

		UI_ROOT_END( UI_FAM_SPIN );
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

	smpBeforeServer = Sys_Milliseconds();

	SV_Frame( msec );

	smpAfterServer = Sys_Milliseconds();

	s_splitFrames++;
	s_splitSv += smpAfterServer - smpBeforeServer;

	s_splitRestT0 = Sys_Milliseconds();
	s_splitEvT0 = s_splitRestT0;

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

	/* ev= is the event loop plus the command buffer, cl= is CL_Frame; rest=
	   keeps naming the whole after-server time. */
	s_splitEvT0 = Sys_Milliseconds();
	s_splitEv += s_splitEvT0 - s_splitRestT0;

	if ( com_speeds->integer ) {
		timeBeforeClient = Sys_Milliseconds ();
	}

	time_frontend = 0;
	time_backend = 0;
	smpClientStart = Sys_Milliseconds();

	smp_loadInClient = 1;

	CL_Frame( msec );

	smp_loadInClient = 0;

	smpFrameEnd = Sys_Milliseconds();

	if ( com_speeds->integer ) {
		timeAfter = Sys_Milliseconds ();
	}
#else
	if ( com_speeds->integer ) {
		timeAfter = Sys_Milliseconds ();
		timeBeforeEvents = timeAfter;
		timeBeforeClient = timeAfter;
	}
	smpClientStart = smpAfterServer;
	smpFrameEnd = Sys_Milliseconds();
#endif

	/* The fps row names the renderer's two sides and never parks the front end,
	   which left most of the frame unaccounted for.  This is the rest of it: the
	   server against everything the main thread does after it -- events, command
	   buffer, client -- so the cost centre the 1000 fps target has to attack is
	   named instead of inferred. */
	s_splitNow = Sys_Milliseconds();
	s_splitRest += s_splitNow - s_splitRestT0;
	s_splitCl += smpFrameEnd - s_splitEvT0;
	if ( !s_splitT0 ) {
		s_splitT0 = s_splitNow;
	} else if ( s_splitNow - s_splitT0 >= 1000 ) {
		if ( PROBE_On() ) {
			Com_Printf(
				"[PERF:frame] win=%dms frames=%d sv=%dms ev=%dms cl=%dms rest=%dms\n",
				s_splitNow - s_splitT0, s_splitFrames, s_splitSv, s_splitEv, s_splitCl,
				s_splitRest );
		}
		s_splitSv = 0;
		s_splitRest = 0;
		s_splitEv = 0;
		s_splitCl = 0;
		s_splitFrames = 0;
		s_splitT0 = s_splitNow;
	}


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

	SMP_BudgetFrame( smpAfterServer - smpBeforeServer,
					  smpFrameEnd - smpClientStart,
					  smpAfterSpin - smpSpinStart,
					  smpFrameEnd - smpFrameStart );

	/* One [MEM:churn] row a second under `mem_churnStats 1`; off, and free, by default. */
	Z_ChurnMeterFrame();

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
	char **list;
	int    numfiles;
	int    i;

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

	/* retail closes with FS_DeleteDir; the ssb walk misses anything else, so
	   sweep the rest before taking the directory itself. */
	list = FS_ListFiles( Com_GetArchiveFileName( savename, NULL, NULL, qfalse ), NULL, qfalse, &numfiles );
	for ( i = 0; list && i < numfiles; i++ ) {
		SAVE_PROBE( "wipe", "slot %s leftover %s\n", savename, list[ i ] );
		FS_Remove_HomeData( Com_GetArchiveFileName( savename, list[ i ], NULL, qfalse ) );
	}

	if ( list ) {
		FS_FreeFileList( list );
	}

	FS_RemoveDir_HomeData( Com_GetArchiveFileName( savename, NULL, NULL, qfalse ) );

	if ( FS_ReadFileEx( Com_GetArchiveFileName( savename, savename, "sav", qfalse ), NULL, qtrue ) != -1 ) {
		SAVE_PROBE( "wipe", "slot %s still has its .sav after the wipe\n", savename );
	}

	if ( FS_ReadFileEx( Com_GetArchiveFileName( NULL, savename, "ssv", qfalse ), NULL, qtrue ) != -1 ) {
		SAVE_PROBE( "wipe", "slot %s still has its .ssv after the wipe\n", savename );
	}
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
