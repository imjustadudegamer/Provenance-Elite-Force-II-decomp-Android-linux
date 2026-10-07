/* link stubs for the offline cmtrace_probe: symbols the collision sources
   reference that live in engine layers the probe deliberately does not build
   (filesystem, cvar, console output).  The probe drives CM_TraceThroughTree /
   CM_TraceToLeaf / CM_TraceThroughBrush directly and never reaches these. */
#include "q_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

void UI_LoadResource( const char *name ) { (void)name; }
void Com_Printf( const char *fmt, ... ) { (void)fmt; }
cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
    (void)name; (void)value; (void)flags; return NULL;
}
void *Hunk_AllocateTempMemory( int size ) { return malloc( size ); }
void Hunk_FreeTempMemory( void *p ) { free( p ); }
qboolean FS_Seek( int h, long o, int origin ) { (void)h; (void)o; (void)origin; return qfalse; }
int FS_Read( void *b, int n, int h ) { (void)b; (void)n; (void)h; return 0; }
int FS_ReadFile( const char *path, void **buffer ) { (void)path; *buffer = NULL; return -1; }
void FS_FreeFile( void *buffer ) { (void)buffer; }
void CL_ProcessLoadingScreen( const char *mapname ) { (void)mapname; }
int developer_dummy;
void *Z_Malloc( int size ) { return malloc( size ); }
void Z_Free( void *p ) { free( p ); }
int FS_Write( const void *b, int n, int h ) { (void)b; (void)n; (void)h; return 0; }

void Alias_Clear( void ) {}
