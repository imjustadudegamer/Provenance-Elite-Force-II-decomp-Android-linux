
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#ifdef __ANDROID__
#include <android/log.h>
#include <sys/system_properties.h>
#ifdef USE_INTERNAL_SDL_HEADERS
#include "SDL_system.h"
#else
#include <SDL_system.h>
#endif
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#ifndef DEDICATED
#ifdef USE_INTERNAL_SDL_HEADERS
#	include "SDL.h"
#	include "SDL_cpuinfo.h"
#else
#	include <SDL.h>
#	include <SDL_cpuinfo.h>
#endif
#endif

#include "sys_local.h"
#include "sys_loadlib.h"

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

static char binaryPath[ MAX_OSPATH ] = { 0 };
static char installPath[ MAX_OSPATH ] = { 0 };

#ifdef __ANDROID__
void Sys_AndroidLog( int priority, const char *msg )
{
	const char *p;

	if( msg == NULL )
		return;

	for( p = msg; *p; )
	{
		char        chunk[ 1024 ];
		const char *nl = strchr( p, '\n' );
		size_t      len = ( nl ? (size_t)( nl - p ) + 1 : strlen( p ) );

		if( len > sizeof( chunk ) - 1 )
			len = sizeof( chunk ) - 1;

		Q_strncpyz( chunk, p, (int)len + 1 );
		__android_log_print( priority, "ProvenanceEF2", "%s", chunk );

		p += len;
	}
}

#ifdef __ANDROID__
static void Sys_AndroidFaultMirror( const char *text )
{
	Sys_AndroidLog( ANDROID_LOG_ERROR, text );
}
#endif

void Sys_AndroidCaptureStdio( void )
{
	const char *files;

	files = SDL_AndroidGetInternalStoragePath();
	if( files == NULL || !*files )
		return;

	{
		char cur[ MAX_OSPATH ], prev[ MAX_OSPATH ];
		int  captured = 0;
		int  rotated = 0;

		Com_sprintf( cur, sizeof( cur ), "%s%c%s", files, PATH_SEP, "stdout.txt" );
		Com_sprintf( prev, sizeof( prev ), "%s%c%s", files, PATH_SEP, "stdout.prev.txt" );
		rotated += Sys_CaptureRotateRule( prev, cur );
		if( freopen( cur, "w", stdout ) != NULL )
			captured++;

		Com_sprintf( cur, sizeof( cur ), "%s%c%s", files, PATH_SEP, "stderr.txt" );
		Com_sprintf( prev, sizeof( prev ), "%s%c%s", files, PATH_SEP, "stderr.prev.txt" );
		rotated += Sys_CaptureRotateRule( prev, cur );
		if( freopen( cur, "w", stderr ) != NULL )
			captured++;

		__android_log_print( ANDROID_LOG_INFO, "ProvenanceEF2",
			"stdio capture: %d of 2 streams -> %s (rotated %d previous capture(s) to *.prev.txt)\n",
			captured, files, rotated );
	}
}
#else
void Sys_AndroidLog( int priority, const char *msg )
{
	(void)priority;
	(void)msg;
}

void Sys_AndroidCaptureStdio( void )
{
}
#endif

void Sys_SetBinaryPath(const char *path)
{
	Q_strncpyz(binaryPath, path, sizeof(binaryPath));
}

char *Sys_BinaryPath(void)
{
	return binaryPath;
}

void Sys_SetDefaultInstallPath(const char *path)
{
	Q_strncpyz(installPath, path, sizeof(installPath));
}

char *Sys_DefaultInstallPath(void)
{
	if (*installPath)
		return installPath;
	else
		return Sys_Cwd();
}

char *Sys_DefaultAppPath(void)
{
	return Sys_BinaryPath();
}

char *Sys_ConsoleInput(void)
{
	return CON_Input( );
}

char *Sys_GetClipboardData(void)
{
    // `38600000 li r3,0` / `4E800020 blr` -- so the Mac engine never returned
    return NULL;
}

#ifdef DEDICATED
#	define PID_FILENAME PRODUCT_NAME "_server.pid"
#else
#	define PID_FILENAME PRODUCT_NAME ".pid"
#endif

static const char *Sys_PIDFileName( const char *gamedir )
{
	const char *homeStatePath = Cvar_VariableString( "fs_homestatepath" );

	if( *homeStatePath != '\0' )
		return va( "%s/%s/%s", homeStatePath, gamedir, PID_FILENAME );

	return NULL;
}

void Sys_RemovePIDFile( const char *gamedir )
{
	const char *pidFile = Sys_PIDFileName( gamedir );

	if( pidFile != NULL )
		remove( pidFile );
}

static qboolean Sys_WritePIDFile( const char *gamedir )
{
	const char *pidFile = Sys_PIDFileName( gamedir );
	FILE      *f;
	qboolean  stale = qfalse;

	if( pidFile == NULL )
		return qfalse;

	if( ( f = fopen( pidFile, "r" ) ) != NULL )
	{
		char  pidBuffer[ 64 ] = { 0 };
		int   pid;

		pid = fread( pidBuffer, sizeof( char ), sizeof( pidBuffer ) - 1, f );
		fclose( f );

		if(pid > 0)
		{
			pid = atoi( pidBuffer );
			if( !Sys_PIDIsRunning( pid ) )
				stale = qtrue;
		}
		else
			stale = qtrue;
	}

	if( FS_CreatePath( pidFile ) ) {
		return 0;
	}

	if( ( f = fopen( pidFile, "w" ) ) != NULL )
	{
		fprintf( f, "%d", Sys_PID( ) );
		fclose( f );
	}
	else
		Com_Printf( S_COLOR_YELLOW "Couldn't write %s.\n", pidFile );

	return stale;
}

void Sys_InitPIDFile( const char *gamedir ) {
	if( Sys_WritePIDFile( gamedir ) ) {
#ifndef DEDICATED
		char message[1024];
		char modName[MAX_OSPATH];

		FS_GetModDescription( gamedir, modName, sizeof ( modName ) );
		Q_CleanStr( modName );

		(void)message;
		Com_Printf( "WARNING: %s did not exit cleanly last time (stale PID file). "
			"Continuing normally.\n", modName );
#endif
	}
}

qboolean Sys_OpenFolderInFileManager( const char *path, qboolean create )
{
	if( create )
	{
		if( FS_CreatePath( path ) )
			return qfalse;
	}

	return Sys_OpenFolderInPlatformFileManager( path );
}

static Q_NO_RETURN void Sys_Exit( int exitCode )
{
#ifdef __ANDROID__
	Sys_AndroidLog( exitCode >= 2 ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO,
		va( "ENGINE EXIT code=%d state=%s\n", exitCode,
			com_fullyInitialized ? "fully-initialised" : "DURING INIT" ) );
#endif

    Sys_ShutdownEx();

	CON_Shutdown( );

#ifndef DEDICATED
	SDL_Quit( );
#endif

	if( exitCode < 2 && com_fullyInitialized )
	{
		Sys_RemovePIDFile( FS_GetCurrentGameDir() );
	}

	NET_Shutdown( );

	Sys_PlatformExit( );

	exit( exitCode );
}

void Sys_Quit( void )
{
	Sys_Exit( 0 );
}

cpuFeatures_t Sys_GetProcessorFeatures( void )
{
	cpuFeatures_t features = 0;

#ifndef DEDICATED
	if( SDL_HasRDTSC( ) )      features |= CF_RDTSC;
	if( SDL_Has3DNow( ) )      features |= CF_3DNOW;
	if( SDL_HasMMX( ) )        features |= CF_MMX;
	if( SDL_HasSSE( ) )        features |= CF_SSE;
	if( SDL_HasSSE2( ) )       features |= CF_SSE2;
	if( SDL_HasAltiVec( ) )    features |= CF_ALTIVEC;
#endif

	return features;
}

#define SYS_MODEL_LEN 256

static const char *Sys_FirstField( const char *path, const char *key, char *out, size_t outlen );

const char *Sys_GetCPUModel( void )
{
	static char model[SYS_MODEL_LEN];
	const char *p;

	if( model[0] )
		return model;

	model[0] = 0;
	p = Sys_FirstField( "/proc/cpuinfo", "model name", model, sizeof( model ) );
	if( !p )
		p = Sys_FirstField( "/proc/cpuinfo", "Hardware", model, sizeof( model ) );
	if( !p )
		p = Sys_FirstField( "/proc/cpuinfo", "Processor", model, sizeof( model ) );

#ifdef __ANDROID__
	if( !p && __system_property_get( "ro.soc.model", model ) <= 0 )
		if( __system_property_get( "ro.board.platform", model ) <= 0 )
			__system_property_get( "ro.hardware", model );
#endif

	if( !model[0] )
		Q_strncpyz( model, "Unknown", sizeof( model ) );

	return model;
}

const char *Sys_GetDeviceModel( void )
{
	static char model[SYS_MODEL_LEN];

	if( model[0] )
		return model;

	model[0] = 0;

#ifdef __ANDROID__
	__system_property_get( "ro.product.model", model );
#elif defined( __linux__ )
	{
		FILE *fp = fopen( "/sys/devices/virtual/dmi/id/product_name", "r" );
		if( fp ) {
			if( fgets( model, sizeof( model ), fp ) ) {
				size_t n = strlen( model );
				while( n > 0 && ( model[n - 1] == '\n' || model[n - 1] == '\r' ) )
					model[--n] = 0;
			}
			fclose( fp );
		}
	}
#endif

	return model;
}

static const char *Sys_FirstField( const char *path, const char *key, char *out, size_t outlen )
{
	char  line[512];
	FILE *fp = fopen( path, "r" );
	size_t keylen = strlen( key );

	if( !fp )
		return NULL;

	while( fgets( line, sizeof( line ), fp ) ) {
		char *colon;

		if( strncmp( line, key, keylen ) != 0 )
			continue;
		colon = strchr( line, ':' );
		if( !colon )
			continue;
		colon++;
		while( *colon == ' ' || *colon == '\t' )
			colon++;
		Q_strncpyz( out, colon, (int)outlen );
		{
			size_t n = strlen( out );
			while( n > 0 && ( out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' ' || out[n - 1] == '\t' ) )
				out[--n] = 0;
		}
		fclose( fp );
		return out;
	}

	fclose( fp );
	return NULL;
}

void Sys_Init(void)
{
	Cvar_Set( "arch", OS_STRING " " ARCH_STRING );
	Cvar_Set( "username", Sys_GetCurrentUser( ) );
}



void Sys_Print( const char *msg )
{
	CON_LogWrite( msg );
	CON_Print( msg );
}

void Sys_Error( const char *error, ... )
{
	va_list argptr;
	char    string[1024];

	va_start (argptr,error);
	Q_vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);

#ifdef __ANDROID__
	Sys_AndroidLog( ANDROID_LOG_ERROR, va( "ENGINE FATAL: %s\n", string ) );
#endif

	Sys_ErrorDialog( string );

	Sys_Exit( 3 );
}

#if 0
static Q_PRINTF_FUNC(1, 2) void Sys_Warn( char *warning, ... )
{
	va_list argptr;
	char    string[1024];

	va_start (argptr,warning);
	Q_vsnprintf (string, sizeof(string), warning, argptr);
	va_end (argptr);

	CON_Print( va( "Warning: %s", string ) );
}
#endif

int Sys_FileTime( char *path )
{
	struct stat buf;

	if (stat (path,&buf) == -1)
		return -1;

	return buf.st_mtime;
}

void Sys_UnloadDll( void *dllHandle )
{
	if( !dllHandle )
	{
		Com_Printf("Sys_UnloadDll(NULL)\n");
		return;
	}

	Sys_UnloadLibrary(dllHandle);
}

#ifdef __ANDROID__
static const char *Sys_AndroidNativeLibDir( void )
{
	static char libDir[ MAX_OSPATH ];
	FILE *f;
	char line[ 2048 ];
	char *p;
	char *sep;

	if( *libDir )
		return libDir;

	f = fopen( "/proc/self/maps", "r" );
	if( !f )
		return "";

	while( fgets( line, sizeof( line ), f ) )
	{
		p = strstr( line, "/libmain.so" );
		if( !p )
			continue;

		while( p > line && *( p - 1 ) != ' ' && *( p - 1 ) != '\t' )
			p--;

		if( strlen( p ) >= sizeof( libDir ) )
			break;

		Q_strncpyz( libDir, p, sizeof( libDir ) );
		sep = strrchr( libDir, '/' );
		if( !sep )
		{
			*libDir = '\0';
		}
		else if( sep != libDir )
		{
			*sep = '\0';
		}
		break;
	}

	fclose( f );
	return libDir;
}
#endif


void *Sys_LoadDll(const char *name, qboolean useSystemLib)
{
	void *dllhandle = NULL;

	if(!Sys_DllExtension(name))
	{
		Com_Printf("Refusing to attempt to load library \"%s\": Extension not allowed.\n", name);
		return NULL;
	}

	if(useSystemLib)
	{
		Com_Printf("Trying to load \"%s\"...\n", name);
		dllhandle = Sys_LoadLibrary(name);
	}
	
	if(!dllhandle)
	{
		const char *topDir;
		char libPath[MAX_OSPATH];
		int len;

		topDir = Sys_BinaryPath();

		if(!*topDir)
			topDir = ".";

		len = Com_sprintf(libPath, sizeof(libPath), "%s%c%s", topDir, PATH_SEP, name);
		if(len < sizeof(libPath))
		{
			Com_Printf("Trying to load \"%s\" from \"%s\"...\n", name, topDir);
			dllhandle = Sys_LoadLibrary(libPath);
		}
		else
		{
			Com_Printf("Skipping trying to load \"%s\" from \"%s\", file name is too long.\n", name, topDir);
		}

		if(!dllhandle)
		{
			const char *basePath = Cvar_VariableString("fs_basepath");
			
			if(!basePath || !*basePath)
				basePath = ".";
			
			if(FS_FilenameCompare(topDir, basePath))
			{
				len = Com_sprintf(libPath, sizeof(libPath), "%s%c%s", basePath, PATH_SEP, name);
				if(len < sizeof(libPath))
				{
					Com_Printf("Trying to load \"%s\" from \"%s\"...\n", name, basePath);
					dllhandle = Sys_LoadLibrary(libPath);
				}
				else
				{
					Com_Printf("Skipping trying to load \"%s\" from \"%s\", file name is too long.\n", name, basePath);
				}
			}
			
			if(!dllhandle)
				Com_Printf("Loading \"%s\" failed\n", name);
		}
	}

#ifdef __ANDROID__
	if(!dllhandle)
	{
		if( Q_strncmp( name, "lib", 3 ) )
		{
			char androidName[ MAX_OSPATH ];
			int  len = Com_sprintf( androidName, sizeof( androidName ), "lib%s", name );

			if( len < sizeof( androidName ) )
			{
				const char *libDir = Sys_AndroidNativeLibDir();
				if( *libDir )
				{
					Com_Printf( "Trying to load \"%s\" from \"%s\"...\n", androidName, libDir );
					dllhandle = Sys_LoadLibrary( va( "%s%c%s", libDir, PATH_SEP, androidName ) );
				}

				if( !dllhandle )
				{
					Com_Printf( "Trying to load \"%s\" from the system library path...\n", androidName );
					dllhandle = Sys_LoadLibrary( androidName );
				}
			}
		}
	}
#endif

	return dllhandle;
}

void *Sys_LoadGameDll(const char *name,
	vmMainProc *entryPoint,
	intptr_t (*systemcalls)(intptr_t, ...))
{
	void *libHandle;
	void (*dllEntry)(intptr_t (*syscallptr)(intptr_t, ...));

	assert(name);

	if(!Sys_DllExtension(name))
	{
		Com_Printf("Refusing to attempt to load library \"%s\": Extension not allowed.\n", name);
		return NULL;
	}

	Com_Printf( "Loading DLL file: %s\n", name);
	libHandle = Sys_LoadLibrary(name);

	if(!libHandle)
	{
		Com_Printf("Sys_LoadGameDll(%s) failed:\n\"%s\"\n", name, Sys_LibraryError());
		return NULL;
	}

	dllEntry = Sys_LoadFunction( libHandle, "dllEntry" );
	*entryPoint = Sys_LoadFunction( libHandle, "vmMain" );

	if ( !*entryPoint || !dllEntry )
	{
		Com_Printf ( "Sys_LoadGameDll(%s) failed to find vmMain function:\n\"%s\" !\n", name, Sys_LibraryError( ) );
		Sys_UnloadLibrary(libHandle);

		return NULL;
	}

	Com_Printf ( "Sys_LoadGameDll(%s) found vmMain function at %p\n", name, *entryPoint );
	dllEntry( systemcalls );

	return libHandle;
}

void Sys_ParseArgs( int argc, char **argv )
{
	if( argc == 2 )
	{
		if( !strcmp( argv[1], "--version" ) ||
				!strcmp( argv[1], "-v" ) )
		{
			const char* date = PRODUCT_DATE;
#ifdef DEDICATED
			fprintf( stdout, Q3_VERSION " dedicated server (%s)\n", date );
#else
			fprintf( stdout, Q3_VERSION " client (%s)\n", date );
#endif
			Sys_Exit( 0 );
		}
	}
}

#ifdef PROTOCOL_HANDLER
char *Sys_ParseProtocolUri( const char *uri )
{
	if ( Q_strncmp( uri, PROTOCOL_HANDLER ":", strlen( PROTOCOL_HANDLER ":" ) ) )
	{
		Com_Printf( "Sys_ParseProtocolUri: unsupported protocol.\n" );
		return NULL;
	}
	uri += strlen( PROTOCOL_HANDLER ":" );
	if ( !Q_strncmp( uri, "//", strlen( "//" ) ) )
	{
		uri += strlen( "//" );
	}
	Com_Printf( "Sys_ParseProtocolUri: %s\n", uri );

	if ( !Q_strncmp( uri, "connect/", strlen( "connect/" ) ) )
	{
		int i, bufsize;
		char *out;

		uri += strlen( "connect/" );
		if ( *uri == '\0' || *uri == '?' )
		{
			Com_Printf( "Sys_ParseProtocolUri: missing argument.\n" );
			return NULL;
		}

		for ( i=0; uri[i] != '\0'; i++ )
		{
			if ( uri[i] == '?' )
			{
				break;
			}

			if ( isalpha( uri[i] ) == 0 && isdigit( uri[i] ) == 0
				&& uri[i] != '.' && uri[i] != ':' && uri[i] != '-'
				&& uri[i] != '[' && uri[i] != ']' )
			{
				Com_Printf( "Sys_ParseProtocolUri: hostname contains unsupported character.\n" );
				return NULL;
			}
		}

		bufsize = strlen( "connect " ) + i + 1;
		out = malloc( bufsize );
		strcpy( out, "connect " );
		strncat( out, uri, i );
		return out;
	}
	else
	{
		Com_Printf( "Sys_ParseProtocolUri: unsupported command.\n" );
		return NULL;
	}
}
#endif

#ifndef DEFAULT_BASEDIR
#	if defined(__ANDROID__)
#		define DEFAULT_BASEDIR ANDROID_DATA_ROOT
#	elif defined(DEFAULT_RELATIVE_BASEDIR)
#		define DEFAULT_BASEDIR Sys_BinaryPathRelative(DEFAULT_RELATIVE_BASEDIR)
#	else
#		define DEFAULT_BASEDIR Sys_BinaryPath()
#	endif
#endif

void Sys_SigHandler( int signum )
{
	Sys_SigHandlerEx( signum, NULL, NULL );
}

void Sys_SigHandlerEx( int signum, void *siginfo, void *context )
{
	static qboolean signalcaught = qfalse;

	if( signalcaught )
	{
		fprintf( stderr, "DOUBLE SIGNAL FAULT: Received signal %d, exiting...\n",
			signum );
	}
	else
	{
		signalcaught = qtrue;

#ifdef __ANDROID__
		Sys_AndroidLog( ANDROID_LOG_ERROR,
			va( "ENGINE SIGNAL %d%s\n", signum,
				( signum != SIGTERM && signum != SIGINT ) ? " (fault, aborting)" : "" ) );
		Sys_FaultOutSetMirror( Sys_AndroidFaultMirror );
#endif

		Sys_FaultOutf( "----\n" );
		Sys_PrintFaultContext(signum, siginfo, context);
		Sys_FaultOutf( "Backtrace:\n" );
		Sys_PrintBackTrace();
		Sys_FaultOutf( "----\n" );

		fflush(NULL);

		if (signum != SIGTERM && signum != SIGINT) {
			signal(SIGABRT, SIG_DFL);
			abort();
		}

		VM_Forced_Unload_Start();
#ifndef DEDICATED
		CL_Shutdown();
#endif
		SV_Shutdown(va("Received signal %d", signum) );
		VM_Forced_Unload_Done();
	}

	if( signum == SIGTERM || signum == SIGINT )
		Sys_Exit( 1 );
	else
		Sys_Exit( 2 );
}

int main( int argc, char **argv )
{
	int   i;
	char  commandLine[ MAX_STRING_CHARS ] = { 0 };
#ifdef PROTOCOL_HANDLER
	char *protocolCommand = NULL;
#endif

#ifdef USE_AUTOUPDATER
	Sys_LaunchAutoupdater(argc, argv);
#endif

#ifndef DEDICATED

#	if !SDL_VERSION_ATLEAST(MINSDL_MAJOR,MINSDL_MINOR,MINSDL_PATCH)
#		error A more recent version of SDL is required
#	endif

	SDL_version ver;
	SDL_GetVersion( &ver );

#define MINSDL_VERSION \
	XSTRING(MINSDL_MAJOR) "." \
	XSTRING(MINSDL_MINOR) "." \
	XSTRING(MINSDL_PATCH)

	if( SDL_VERSIONNUM( ver.major, ver.minor, ver.patch ) <
			SDL_VERSIONNUM( MINSDL_MAJOR, MINSDL_MINOR, MINSDL_PATCH ) )
	{
		Sys_Dialog( DT_ERROR, va( "SDL version " MINSDL_VERSION " or greater is required, "
			"but only version %d.%d.%d was found. You may be able to obtain a more recent copy "
			"from https://www.libsdl.org/.", ver.major, ver.minor, ver.patch ), "SDL Library Too Old" );

		Sys_Exit( 1 );
	}
#endif

	Sys_PlatformInit( );
	Sys_SetMaxFileLimit( );

#ifdef __ANDROID__
	Sys_AndroidLog( ANDROID_LOG_INFO, "ProvenanceEF2 native main entered\n" );
	Sys_AndroidCaptureStdio( );

#ifndef DEDICATED
	SDL_SetHint( SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "1" );
	SDL_SetHint( SDL_HINT_ANDROID_BLOCK_ON_PAUSE_PAUSEAUDIO, "1" );
#endif
#endif

	Sys_Milliseconds( );


	Sys_ParseArgs( argc, argv );
	Sys_SetBinaryPath( Sys_Dirname( argv[ 0 ] ) );
	Sys_SetDefaultInstallPath( DEFAULT_BASEDIR );

	for( i = 1; i < argc; i++ )
	{
		qboolean containsSpaces;

		if ( !strcmp( argv[i], "--uri" ) )
		{
#ifdef PROTOCOL_HANDLER
			if ( argc > i+1 )
			{
				protocolCommand = Sys_ParseProtocolUri( argv[i+1] );
			}
#endif
			break;
		}

		containsSpaces = strchr(argv[i], ' ') != NULL;
		if (containsSpaces)
			Q_strcat( commandLine, sizeof( commandLine ), "\"" );

		Q_strcat( commandLine, sizeof( commandLine ), argv[ i ] );

		if (containsSpaces)
			Q_strcat( commandLine, sizeof( commandLine ), "\"" );

		Q_strcat( commandLine, sizeof( commandLine ), " " );
	}

#ifdef PROTOCOL_HANDLER
	if ( protocolCommand != NULL )
	{
		Q_strcat( commandLine, sizeof( commandLine ), "+" );
		Q_strcat( commandLine, sizeof( commandLine ), protocolCommand );
		free( protocolCommand );
	}
#endif

	CON_Init( );
	Com_Init( commandLine );
	Sys_InitEx(); 
	NET_Init( );

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
	{
		struct sigaction sa;

		memset( &sa, 0, sizeof( sa ) );
		sigemptyset( &sa.sa_mask );
		sa.sa_flags     = SA_SIGINFO;
		sa.sa_sigaction = (void (*)( int, siginfo_t *, void * ))Sys_SigHandlerEx;

		sigaction( SIGILL, &sa, NULL );
		sigaction( SIGFPE, &sa, NULL );
		sigaction( SIGSEGV, &sa, NULL );
		sigaction( SIGBUS, &sa, NULL );
		sigaction( SIGABRT, &sa, NULL );
	}
#else
	signal( SIGILL, Sys_SigHandler );
	signal( SIGFPE, Sys_SigHandler );
	signal( SIGSEGV, Sys_SigHandler );
#endif
	signal( SIGTERM, Sys_SigHandler );
	signal( SIGINT, Sys_SigHandler );

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop( Com_Frame, 0, 1 );
#else
	while( 1 )
	{
		Com_Frame( );
	}
#endif

	return 0;
}
