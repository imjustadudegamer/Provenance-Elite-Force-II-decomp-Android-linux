/* micro-benchmark for Cvar_FindVar at the old and new table sizes.
   Registers N realistic cvar names through the engine's own Cvar_Get, then
   times hit and miss lookup sweeps.  Not part of the shipped tree. */
#include "../app/src/main/cpp/code/qcommon/qcommon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

extern int cvar_numIndexes;


static double now( void ) {
    struct timespec t;
    clock_gettime( CLOCK_MONOTONIC, &t );
    return (double)t.tv_sec + 1e-9 * (double)t.tv_nsec;
}

int main( int argc, char **argv ) {
    static char names[2048][32];
    int n = ( argc > 1 ) ? atoi( argv[1] ) : 1200;
    volatile cvar_t *sink = NULL;
    double t0, t1;
    int reps = 3000000;

    {
        const char *pre[] = { "r_", "cg_", "ui_", "s_", "cl_", "sv_", "com_", "in_", "m_", "vid_" };
        for ( int i = 0; i < n; i++ ) {
            snprintf( names[i], sizeof( names[i] ), "%s%s%d", pre[i % 10],
                      ( i % 3 == 0 ) ? "vertex" : ( i % 3 == 1 ) ? "lightmap" : "gamma", i );
            Cvar_Get( names[i], "1", 0 );
        }
    }

    t0 = now();
    for ( int r = 0; r < reps; r++ ) sink = Cvar_FindVar( names[r % n] );
    t1 = now();
    printf( "N=%4d registered=%4d  hit:  %6.1f ns/lookup\n", n, cvar_numIndexes,
            ( t1 - t0 ) * 1e9 / reps );

    t0 = now();
    for ( int r = 0; r < reps; r++ ) {
        char miss[32];
        snprintf( miss, sizeof( miss ), "nope_%d", r % 97 );
        sink = Cvar_FindVar( miss );
    }
    t1 = now();
    printf( "N=%4d registered=%4d  miss: %6.1f ns/lookup\n", n, cvar_numIndexes,
            ( t1 - t0 ) * 1e9 / reps );
    return 0;
}
