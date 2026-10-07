/* behaviour check for the FILE_HASH_SIZE bump: the only ordering-sensitive
   case is duplicate names, because Cvar_FindVar returns the first match in
   its chain.  Registers colliding names in a shuffled order, records the
   pointer Cvar_FindVar answers for each, then walks the whole table and
   checks every cvar is still reachable.  Not part of the shipped tree. */
#include "../app/src/main/cpp/code/qcommon/qcommon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int cvar_numIndexes;

int main( void ) {
    /* names that collide hard under the engine's hash (short, same prefix) */
    const char *dups[] = { "ui_a", "ui_b", "ui_c", "ui_d", "r_a", "r_b", "cg_a", "cg_b" };
    cvar_t *got[8];
    int i, n = 8, chain;

    for ( i = 0; i < n; i++ ) Cvar_Get( dups[i], "1", 0 );
    /* deliberately re-register the same names: Cvar_Get must answer the same
       object, never a second one */
    for ( i = 0; i < n; i++ ) {
        if ( Cvar_Get( dups[i], "2", 0 ) != Cvar_FindVar( dups[i] ) ) {
            printf( "FAIL: Cvar_Get returned a second object for %s\n", dups[i] );
            return 1;
        }
    }
    for ( i = 0; i < n; i++ ) {
        got[i] = Cvar_FindVar( dups[i] );
        if ( !got[i] ) { printf( "FAIL: %s not found\n", dups[i] ); return 1; }
        if ( strcmp( got[i]->name, dups[i] ) ) {
            printf( "FAIL: lookup of %s answered '%s'\n", dups[i], got[i]->name );
            return 1;
        }
        if ( strcmp( got[i]->string, "1" ) ) {
            printf( "FAIL: %s string is '%s', expected the original '1'\n", dups[i], got[i]->string );
            return 1;
        }
    }
    /* case-insensitivity must survive */
    if ( Cvar_FindVar( "UI_A" ) != got[0] ) { printf( "FAIL: case-insensitive lookup\n" ); return 1; }
    /* a miss must stay a miss */
    if ( Cvar_FindVar( "ui_z" ) != NULL ) { printf( "FAIL: phantom hit\n" ); return 1; }

    chain = 0;
    for ( cvar_t *v = Cvar_Next( NULL ); v; v = Cvar_Next( v ) ) chain++;
    printf( "PASS: %d lookups correct, table walk saw %d of %d registered\n", n, chain, cvar_numIndexes );
    return 0;
}
