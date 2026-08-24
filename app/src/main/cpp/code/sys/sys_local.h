
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DEDICATED
#ifdef USE_INTERNAL_SDL_HEADERS
#	include "SDL_version.h"
#else
#	include <SDL_version.h>
#endif

#define MINSDL_MAJOR 2
#define MINSDL_MINOR 0
#if SDL_VERSION_ATLEAST( 2, 0, 5 )
#define MINSDL_PATCH 5
#else
#define MINSDL_PATCH 0
#endif
#endif

void CON_Shutdown( void );
void CON_Init( void );
char *CON_Input( void );
void CON_Print( const char *message );

unsigned int CON_LogSize( void );
unsigned int CON_LogWrite( const char *in );
unsigned int CON_LogRead( char *out, unsigned int outSize );

char *Sys_BinaryPath( void );
char *Sys_BinaryPathRelative( const char *relative );


void Sys_GLimpSafeInit( void );
void Sys_GLimpInit( void );
void Sys_PlatformInit( void );
void Sys_PlatformExit( void );
void Sys_SigHandler( int signal ) Q_NO_RETURN;
void Sys_SigHandlerEx( int signal, void *siginfo, void *context ) Q_NO_RETURN;
void Sys_ErrorDialog( const char *error );

int Sys_PID( void );
qboolean Sys_PIDIsRunning( int pid );

qboolean Sys_OpenFolderInPlatformFileManager( const char *path );

qboolean Sys_SetMaxFileLimit( void );

#ifdef PROTOCOL_HANDLER
char *Sys_ParseProtocolUri( const char *uri );
#endif

void Sys_AndroidLog( int priority, const char *msg );

void Sys_AndroidCaptureStdio( void );

#ifdef USE_AUTOUPDATER
void Sys_LaunchAutoupdater(int argc, char **argv);
#endif

#include "new/sys_local_new.h"

#ifdef __cplusplus
}
#endif
