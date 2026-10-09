/* Differential check for the Event::FindEvent rework: the old sorted binary
   search and the new hash index, both the verbatim text of their own revision,
   registered with the module's real event names, must answer the same eventnum
   for every query -- hits, case variants, misses, before and after a table
   reset/rebuild.  Not part of the shipped tree. */
#include "../../app/src/main/cpp/code/corepp/str.h"
#include "../../app/src/main/cpp/code/corepp/container.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

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

static void validateIndex( int num );

/* the registration path of Event::Event( const char *, ... ), verbatim
   semantics: look first, add only when absent, mark both indexes dirty */
static int registerEvent( int which, const char *command )
{
    Container<str *> **cl = which ? &EvNew::commandList : &EvOld::commandList;
    Container<int>   **sl = which ? &EvNew::sortedList : &EvOld::sortedList;
    qboolean          *dl = which ? &EvNew::dirtylist : &EvOld::dirtylist;
    int eventnum;

    if ( !*cl )
    {
        *cl = new Container<str *>;
        *sl = new Container<int>;
        *dl = qfalse;
        ( *sl )->AddObject( ( *cl )->AddObject( new str( "NULL" ) ) );
    }

    eventnum = which ? EvNew::FindEvent( command ) : EvOld::FindEvent( command );
    if ( !eventnum )
    {
        eventnum = ( *cl )->AddObject( new str( command ) );
        ( *sl )->AddObject( eventnum );
        *dl = qtrue;
        if ( which )
        {
            /* the real constructor marks both indexes */
            EvNew::hashdirty = qtrue;
            EvNew::BuildEventHash();
            validateIndex( EvNew::commandList->NumObjects() );
        }
    }

    return eventnum;
}

static void resetTable( int which )
{
    Container<str *> **cl = which ? &EvNew::commandList : &EvOld::commandList;
    Container<int>   **sl = which ? &EvNew::sortedList : &EvOld::sortedList;

    if ( *cl )
    {
        for ( int i = ( *cl )->NumObjects(); i > 0; i-- )
        {
            delete ( *cl )->ObjectAt( i );
        }
        delete *cl;
        *cl = NULL;
    }
    delete *sl;
    *sl = NULL;

    /* L_InitEvents resets the memo with the rest of the event system */
    if ( which )
    {
        EvNew::lastevent = 0;
        EvNew::ResetEventHash();
    }
    else
    {
        EvOld::lastevent = 0;
    }
}

static int failures = 0;

/* the index invariant: every chain is strictly increasing eventnums, all
   within the table, no repeats, every entry reachable exactly once */
static void validateIndex( int num )
{
    static unsigned char seen[ 4096 ];
    int i, chain, total = 0;

    for ( i = 0; i < 4096; i++ ) seen[ i ] = 0;
    for ( i = 0; i < 4096; i++ )
    {
        chain = 0;
        for ( int e = eventHashHead[ i ]; e != 0; e = EvNew::hashNext[ e ] )
        {
            if ( e < 1 || e > num ) { printf( "BAD: bucket %d holds eventnum %d (table %d)\n", i, e, num ); failures++; return; }
            if ( e <= 4095 ) { if ( seen[ e ] ) { printf( "BAD: eventnum %d in two buckets\n", e ); failures++; return; } seen[ e ] = 1; }
            if ( ++chain > num ) { printf( "BAD: bucket %d chain longer than the table\n", i ); failures++; return; }
            total++;
        }
    }
    if ( total != num ) { printf( "BAD: index holds %d of %d entries\n", total, num ); failures++; }
}

static void expect( int ok, const char *what )
{
    if ( !ok )
    {
        printf( "FAIL: %s\n", what );
        failures++;
    }
}

/* one equivalence sweep over both tables */
static void compareAll( const std::vector<std::string>& queries, const char *stage )
{
    char buf[ 256 ];
    int  oldcmp0 = EvOld::numcompares;
    int  newcmp0 = EvNew::numcompares;

    for ( size_t i = 0; i < queries.size(); i++ )
    {
        int a = EvOld::FindEvent( queries[ i ].c_str() );
        int b = EvNew::FindEvent( queries[ i ].c_str() );
        if ( a != b )
        {
            snprintf( buf, sizeof( buf ), "%s: '%s' old=%d new=%d", stage,
                      queries[ i ].c_str(), a, b );
            expect( 0, buf );
            return;
        }
    }

    printf( "  %-10s %6zu queries agree (old used %d stricmp, new used %d)\n",
            stage, queries.size(), EvOld::numcompares - oldcmp0, EvNew::numcompares - newcmp0 );
}

int main( void )
{
    printf( "main entered\n" ); fflush( stdout );
    std::vector<std::string> names;
    std::vector<std::string> queries;
    int i;

    /* --- 1. register the module's real names, in real order, into both ----- */
    for ( i = 0; i < G_REGISTERED_N; i++ )
    {
        if ( ( i % 200 ) == 0 ) { printf( "reg %d\n", i ); fflush( stdout ); }
        names.push_back( g_registered[ i ] );
        registerEvent( 0, g_registered[ i ] );
        registerEvent( 1, g_registered[ i ] );
    }
    printf( "registered %d names -> old table %d entries, new table %d entries\n",
            G_REGISTERED_N, EvOld::commandList->NumObjects(), EvNew::commandList->NumObjects() );
    expect( EvOld::commandList->NumObjects() == EvNew::commandList->NumObjects(),
            "both tables hold the same number of entries" );

    /* the container must be case-insensitively unique: that is what makes the
       two lookups equivalent for every possible query, not just the sampled ones */
    {
        std::vector<std::string> lower;
        for ( i = 1; i <= EvNew::commandList->NumObjects(); i++ )
        {
            std::string s = EvNew::commandList->ObjectAt( i )->c_str();
            for ( size_t j = 0; j < s.size(); j++ ) s[ j ] = ( char )tolower( s[ j ] );
            lower.push_back( s );
        }
        std::sort( lower.begin(), lower.end() );
        int dups = 0;
        for ( i = 1; i < ( int )lower.size(); i++ )
        {
            if ( lower[ i ] == lower[ i - 1 ] ) dups++;
        }
        expect( dups == 0, "the command list is case-insensitively unique" );
        printf( "  distinct event names in the table: %d (duplicates %d)\n",
                ( int )lower.size(), dups );
    }

    /* --- 2. the query set: every name, case variants, misses --------------- */
    for ( i = 0; i < G_REGISTERED_N; i++ )
    {
        queries.push_back( g_registered[ i ] );
    }
    for ( i = 0; i < G_REGISTERED_N; i += 3 )
    {
        std::string s = g_registered[ i ];
        for ( size_t j = 0; j < s.size(); j++ )
        {
            s[ j ] = ( char )( ( j & 1 ) ? toupper( s[ j ] ) : tolower( s[ j ] ) );
        }
        queries.push_back( s );
    }
    for ( i = 0; i < 2000; i++ )
    {
        char buf[ 64 ];
        snprintf( buf, sizeof( buf ), "no_such_event_%d", i );
        queries.push_back( buf );
        snprintf( buf, sizeof( buf ), "%s", i < 500 ? "nul" : "NULLX" );
        queries.push_back( buf );
    }
    compareAll( queries, "pass 1" );

    /* --- 3. what the two algorithms actually spend ------------------------ */
    {
        int oc = EvOld::numcompares, nc = EvNew::numcompares;
        int of = EvOld::numfinds, nf = EvNew::numfinds;
        printf( "  stricmp calls per lookup: old %.2f, new %.2f\n",
                ( double )oc / of, ( double )nc / nf );
        expect( nc < oc, "the index performs fewer string compares" );
    }

    /* --- 4. reset and rebuild (the L_ShutdownEvents / re-init path) -------- */
    resetTable( 0 );
    resetTable( 1 );
    {
        /* with a stale index every query would answer an eventnum that is gone */
        for ( i = 0; i < 200; i++ )
        {
            char buf[ 64 ];
            snprintf( buf, sizeof( buf ), "no_such_event_%d", i );
            expect( EvNew::FindEvent( buf ) == 0, "post-reset miss stays a miss" );
        }
        expect( EvNew::FindEvent( "anim" ) == 0, "post-reset lookup of a former name misses" );
    }

    for ( i = 0; i < G_REGISTERED_N; i++ )
    {
        registerEvent( 0, g_registered[ i ] );
        registerEvent( 1, g_registered[ i ] );
    }
    compareAll( queries, "pass 2" );

    /* --- 5. a shuffled registration order must not change the answers ----- */
    {
        std::vector<std::string> shuffled( names );
        for ( i = ( int )shuffled.size() - 1; i > 0; i-- )
        {
            std::swap( shuffled[ i ], shuffled[ ( i * 7919 + 13 ) % ( i + 1 ) ] );
        }
        resetTable( 0 );
        resetTable( 1 );
        for ( i = 0; i < ( int )shuffled.size(); i++ )
        {
            registerEvent( 0, shuffled[ i ].c_str() );
            registerEvent( 1, shuffled[ i ].c_str() );
        }
        compareAll( queries, "shuffled" );
    }

    if ( failures )
    {
        printf( "FAIL: %d assertion(s)\n", failures );
        return 1;
    }
    printf( "PASS: old and new FindEvent agree on every query, before and after a rebuild\n" );
    return 0;
}
