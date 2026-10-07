/* stubs for the offline cvar bench: exactly the symbols nm -u on cvar.o
   reports as coming from engine layers the bench does not build. */
#include "../app/src/main/cpp/code/qcommon/qcommon.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

cvar_t *developer;
qboolean com_errorEntered;

void *Z_Malloc( int size ) { return malloc( size ); }
void Z_Free( void *p ) { free( p ); }
void Cmd_AddCommand( const char *n, xcommand_t f ) { (void)n; (void)f; }
int  Cmd_Argc( void ) { return 0; }
char *Cmd_Args( void ) { return ""; }
char *Cmd_Argv( int i ) { (void)i; return ""; }
void Com_DPrintf( const char *fmt, ... ) { (void)fmt; }
void Com_Error( int level, const char *fmt, ... ) { (void)level; (void)fmt; exit( 5 ); }
void Com_Printf( const char *fmt, ... ) { (void)fmt; }
char *CopyString( const char *s ) {
    size_t n = strlen( s ) + 1; char *d = (char *)malloc( n ); memcpy( d, s, n ); return d;
}
size_t FS_Write( const void *b, size_t n, fileHandle_t h ) { (void)b; (void)h; return n; }
int Com_Filter( const char *f, const char *s, int c ) { (void)f; (void)s; (void)c; return 0; }
void SV_ClearSvsTimeFixups( void ) {}
