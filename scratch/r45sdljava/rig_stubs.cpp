/* link stubs for the rig: the engine's own Container/str error path */
#include <cstdio>
#include <cstdlib>
extern "C" void Com_Error( int level, const char *fmt, ... )
{
    ( void )level; ( void )fmt;
    fprintf( stderr, "rig: Com_Error\n" );
    fflush( NULL );
    abort();
}
