/* Micro-benchmark for Event::FindEvent, old sorted search vs new hash index,
   over the module's real event population at the real registration order.
   Not part of the shipped tree. */
#include "../../app/src/main/cpp/code/corepp/str.h"
#include "../../app/src/main/cpp/code/corepp/container.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>

#define stricmp strcasecmp
typedef int qboolean;
#define qtrue  1
#define qfalse 0

struct EvOld {
    static Container<str *> *commandList;
    static Container<int>   *sortedList;
    static qboolean          dirtylist;
    static int               numfinds, numfirstsearch, numcompares, lastevent;
    static int               FindEvent( const char *name );
    static int               compareEvents( const void *arg1, const void *arg2 );
    static void              SortEventList( void );
};

struct EvNew {
    static Container<str *> *commandList;
    static Container<int>   *sortedList;
    static qboolean          dirtylist;
    static int              *hashNext;
    static qboolean          hashdirty;
    static int               numfinds, numfirstsearch, numcompares, lastevent;
    static int               FindEvent( const char *name );
    static int               compareEvents( const void *arg1, const void *arg2 );
    static void              SortEventList( void );
    static long              generateHashValue( const char *name );
    static void              ResetEventHash( void );
    static void              BuildEventHash( void );
};

#include "find_old.inc"
#include "find_new.inc"
#include "event_names.inc"

Container<str *> *EvOld::commandList = NULL;
Container<int>   *EvOld::sortedList = NULL;
qboolean          EvOld::dirtylist = qfalse;
int               EvOld::numfinds, EvOld::numfirstsearch, EvOld::numcompares, EvOld::lastevent;

Container<str *> *EvNew::commandList = NULL;
Container<int>   *EvNew::sortedList = NULL;
qboolean          EvNew::dirtylist = qfalse;
int              *EvNew::hashNext = NULL;
qboolean          EvNew::hashdirty = qtrue;
int               EvNew::numfinds, EvNew::numfirstsearch, EvNew::numcompares, EvNew::lastevent;

static void registerAll( int which )
{
    Container<str *> **cl = which ? &EvNew::commandList : &EvOld::commandList;
    Container<int>   **sl = which ? &EvNew::sortedList : &EvOld::sortedList;

    if ( !*cl )
    {
        *cl = new Container<str *>;
        *sl = new Container<int>;
        ( *sl )->AddObject( ( *cl )->AddObject( new str( "NULL" ) ) );
    }

    for ( int i = 0; i < G_REGISTERED_N; i++ )
    {
        int eventnum = which ? EvNew::FindEvent( g_registered[ i ] )
                             : EvOld::FindEvent( g_registered[ i ] );
        if ( !eventnum )
        {
            eventnum = ( *cl )->AddObject( new str( g_registered[ i ] ) );
            ( *sl )->AddObject( eventnum );
            if ( which ) { EvNew::dirtylist = qtrue; EvNew::hashdirty = qtrue; }
            else         { EvOld::dirtylist = qtrue; }
        }
    }
}

static double now( void )
{
    struct timespec t;
    clock_gettime( CLOCK_MONOTONIC, &t );
    return ( double )t.tv_sec + 1e-9 * ( double )t.tv_nsec;
}

int main( int argc, char **argv )
{
    int reps = ( argc > 1 ) ? atoi( argv[ 1 ] ) : 2000000;
    volatile int sink = 0;
    double t0, t1;
    int i;
    char missname[ 64 ];

    registerAll( 0 );
    registerAll( 1 );
    printf( "table: %d entries (old) / %d entries (new)\n",
            EvOld::commandList->NumObjects(), EvNew::commandList->NumObjects() );

    /* hits, walking every registered name in registration order: the worst
       case for the one-entry memo, because consecutive names differ */
    t0 = now();
    for ( i = 0; i < reps; i++ ) sink = EvOld::FindEvent( g_registered[ i % G_REGISTERED_N ] );
    t1 = now();
    printf( "hit  old: %7.1f ns/lookup (%d stricmp for %d lookups)\n",
            ( t1 - t0 ) * 1e9 / reps, EvOld::numcompares, reps );

    t0 = now();
    for ( i = 0; i < reps; i++ ) sink = EvNew::FindEvent( g_registered[ i % G_REGISTERED_N ] );
    t1 = now();
    printf( "hit  new: %7.1f ns/lookup (%d stricmp for %d lookups)\n",
            ( t1 - t0 ) * 1e9 / reps, EvNew::numcompares, reps );

    /* misses: a name that is not in the table, cycling through 97 of them */
    EvOld::numcompares = EvNew::numcompares = 0;
    snprintf( missname, sizeof( missname ), "no_such_event" );
    t0 = now();
    for ( i = 0; i < reps; i++ )
    {
        missname[ 13 ] = ( char )( 'a' + ( i % 26 ) );
        missname[ 14 ] = ( char )( 'a' + ( ( i / 26 ) % 26 ) );
        missname[ 15 ] = '\0';
        sink = EvOld::FindEvent( missname );
    }
    t1 = now();
    printf( "miss old: %7.1f ns/lookup (%d stricmp)\n", ( t1 - t0 ) * 1e9 / reps, EvOld::numcompares );

    t0 = now();
    for ( i = 0; i < reps; i++ )
    {
        missname[ 13 ] = ( char )( 'a' + ( i % 26 ) );
        missname[ 14 ] = ( char )( 'a' + ( ( i / 26 ) % 26 ) );
        missname[ 15 ] = '\0';
        sink = EvNew::FindEvent( missname );
    }
    t1 = now();
    printf( "miss new: %7.1f ns/lookup (%d stricmp)\n", ( t1 - t0 ) * 1e9 / reps, EvNew::numcompares );

    /* the memo case: the same name over and over, which is what a repeated
       per-frame lookup looks like */
    EvOld::numcompares = EvNew::numcompares = 0;
    t0 = now();
    for ( i = 0; i < reps; i++ ) sink = EvOld::FindEvent( g_registered[ 0 ] );
    t1 = now();
    printf( "repeat old: %7.1f ns/lookup (%d stricmp)\n", ( t1 - t0 ) * 1e9 / reps, EvOld::numcompares );

    t0 = now();
    for ( i = 0; i < reps; i++ ) sink = EvNew::FindEvent( g_registered[ 0 ] );
    t1 = now();
    printf( "repeat new: %7.1f ns/lookup (%d stricmp)\n", ( t1 - t0 ) * 1e9 / reps, EvNew::numcompares );

    return 0;
}
