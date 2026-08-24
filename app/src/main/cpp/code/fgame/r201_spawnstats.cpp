#include "g_local.h"
#include "level.h"
#include "r201_spawnstats.h"

int		r201_spawnOn = 0;

static int	r201_ssSums[R201_SP_ZONES];
static int	r201_ssMaxs[R201_SP_ZONES];
static int	r201_ssCounters[R201_SPC_COUNT];
static int	r201_ssSpawned;
static int	r201_ssInhibited;
static int	r201_ssEnts;
static int	r201_ssSeq;
static int	r201_ssStopped;

static struct {
	char	name[40];
	int		count;
	int		total;
	int		peak;
} r201_ssClass[R201_SPAWN_MAX_CLASSES];
static int	r201_ssClassNum;
static int	r201_ssOtherCount;
static int	r201_ssOtherTotal;

int R201_SpawnNow( void ) {
	if ( !r201_spawnOn ) {
		return 0;
	}
	return gi.Milliseconds();
}

void R201_SpawnEnter( const char *map, int ents ) {
	static cvar_t	*r201_spawnStats;
	int				i;

	if ( !r201_spawnStats ) {
		r201_spawnStats = gi.cvar( "r201_spawnStats", "1", 0 );
	}

	(void)map;
	r201_spawnOn = ( r201_spawnStats && r201_spawnStats->integer ) ? 1 : 0;

	for ( i = 0; i < R201_SP_ZONES; i++ ) {
		r201_ssSums[i] = r201_ssMaxs[i] = 0;
	}
	for ( i = 0; i < R201_SPC_COUNT; i++ ) {
		r201_ssCounters[i] = 0;
	}
	for ( i = 0; i < r201_ssClassNum; i++ ) {
		r201_ssClass[i].name[0] = '\0';
		r201_ssClass[i].count = r201_ssClass[i].total = r201_ssClass[i].peak = 0;
	}
	r201_ssClassNum    = 0;
	r201_ssOtherCount  = 0;
	r201_ssOtherTotal  = 0;
	r201_ssSpawned     = 0;
	r201_ssInhibited   = 0;
	r201_ssEnts        = ents;
}

void R201_SpawnZone( int zone, int msec ) {
	if ( !r201_spawnOn || zone < 0 || zone >= R201_SP_ZONES ) {
		return;
	}
	if ( msec < 0 ) {
		msec = 0;
	}

	r201_ssSums[zone] += msec;
	if ( msec > r201_ssMaxs[zone] ) {
		r201_ssMaxs[zone] = msec;
	}
}

void R201_SpawnCounter( int which, int add ) {
	if ( !r201_spawnOn || which < 0 || which >= R201_SPC_COUNT ) {
		return;
	}
	r201_ssCounters[which] += add;
}

void R201_SpawnClass( const char *classname, int msec ) {
	int		i;
	char	name[40];

	if ( !r201_spawnOn ) {
		return;
	}

	R201_SpawnZone( R201_SP_spawn, msec );

	if ( !classname || !classname[0] ) {
		classname = "Unspecified";
	}
	Q_strncpyz( name, classname, sizeof( name ) );

	for ( i = 0; i < r201_ssClassNum; i++ ) {
		if ( !Q_stricmp( r201_ssClass[i].name, name ) ) {
			r201_ssClass[i].count++;
			r201_ssClass[i].total += msec;
			if ( msec > r201_ssClass[i].peak ) {
				r201_ssClass[i].peak = msec;
			}
			return;
		}
	}

	if ( r201_ssClassNum >= R201_SPAWN_MAX_CLASSES ) {
		r201_ssOtherCount++;
		r201_ssOtherTotal += msec;
		return;
	}

	Q_strncpyz( r201_ssClass[r201_ssClassNum].name, name, sizeof( r201_ssClass[0].name ) );
	r201_ssClass[r201_ssClassNum].count  = 1;
	r201_ssClass[r201_ssClassNum].total  = msec;
	r201_ssClass[r201_ssClassNum].peak   = msec;
	r201_ssClassNum++;
}

void R201_SpawnLeave( int spawned, int inhibited ) {
	static const char *zoneName[R201_SP_ZONES] = {
		"ents", "init", "pinit", "wparse", "loop", "spawn", "drain", "start",
		"precache", "postload", "player", "model",
		"pathload", "pathcalc", "pathopt", "pathsave", "findtgt"
	};
	int		i, j, best, printed, children, drainChildren, drainOther, unacct;
	int		clsIdx[R201_SPAWN_TOP_CLASSES];
	char	line[1024], part[64];

	if ( !r201_spawnOn || r201_ssStopped ) {
		return;
	}

	r201_ssSpawned   = spawned;
	r201_ssInhibited = inhibited;
	r201_ssSeq++;

	children = r201_ssSums[R201_SP_init] + r201_ssSums[R201_SP_pinit]
			 + r201_ssSums[R201_SP_wparse] + r201_ssSums[R201_SP_loop]
			 + r201_ssSums[R201_SP_drain] + r201_ssSums[R201_SP_start]
			 + r201_ssSums[R201_SP_postload] + r201_ssSums[R201_SP_player];
	unacct = r201_ssSums[R201_SP_ents] - children;
	if ( unacct < 0 ) {
		unacct = 0;		
	}

	drainChildren = r201_ssSums[R201_SP_pathload] + r201_ssSums[R201_SP_pathcalc]
				  + r201_ssSums[R201_SP_pathopt] + r201_ssSums[R201_SP_pathsave]
				  + r201_ssSums[R201_SP_findtgt];
	drainOther = r201_ssSums[R201_SP_drain] - drainChildren;
	if ( drainOther < 0 ) {
		drainOther = 0;
	}

	line[0] = '\0';
	Com_sprintf( part, sizeof( part ), "seq=%d map=%s ents=%d spawned=%d inh=%d total=%dms max=%dms",
				 r201_ssSeq, level.mapname.c_str(), r201_ssEnts, r201_ssSpawned,
				 r201_ssInhibited, r201_ssSums[R201_SP_ents], r201_ssMaxs[R201_SP_ents] );
	Q_strcat( line, sizeof( line ), part );
	for ( i = R201_SP_init; i <= R201_SP_player; i++ ) {
		Com_sprintf( part, sizeof( part ), " %s=%d/%d", zoneName[i],
					 r201_ssSums[i], r201_ssMaxs[i] );
		Q_strcat( line, sizeof( line ), part );
	}
	Com_sprintf( part, sizeof( part ), " parse=%d spawn=%d model=%d unacct=%d",
				 r201_ssSums[R201_SP_loop] - r201_ssSums[R201_SP_spawn],
				 r201_ssSums[R201_SP_spawn], r201_ssSums[R201_SP_model], unacct );
	Q_strcat( line, sizeof( line ), part );
	gi.Printf( "[R201:spawn] %s\n", line );

	line[0] = '\0';
	for ( i = R201_SP_pathload; i < R201_SP_ZONES; i++ ) {
		Com_sprintf( part, sizeof( part ), " %s=%d/%d", zoneName[i],
					 r201_ssSums[i], r201_ssMaxs[i] );
		Q_strcat( line, sizeof( line ), part );
	}
	Com_sprintf( part, sizeof( part ),
				 " other=%d traces=%d clear=%d ladder=%d memohit=%d memomiss=%d skipped=%d equivOK=%d equivBAD=%d",
				 drainOther, r201_ssCounters[R201_SPC_traces],
				 r201_ssCounters[R201_SPC_clear], r201_ssCounters[R201_SPC_ladder],
				 r201_ssCounters[R201_SPC_memohit], r201_ssCounters[R201_SPC_memomiss],
				 r201_ssCounters[R201_SPC_skipped], r201_ssCounters[R201_SPC_equivok],
				 r201_ssCounters[R201_SPC_equivbad] );
	Q_strcat( line, sizeof( line ), part );
	gi.Printf( "[R201:spawn] drain:%s\n", line );

	for ( i = 0; i < R201_SPAWN_TOP_CLASSES; i++ ) {
		clsIdx[i] = -1;
	}
	printed = 0;
	while ( printed < R201_SPAWN_TOP_CLASSES ) {
		best = -1;
		for ( j = 0; j < r201_ssClassNum; j++ ) {
			int	seen = 0, k;

			for ( k = 0; k < printed; k++ ) {
				if ( clsIdx[k] == j ) {
					seen = 1;
					break;
				}
			}
			if ( seen ) {
				continue;
			}
			if ( best < 0 || r201_ssClass[j].total > r201_ssClass[best].total ) {
				best = j;
			}
		}
		if ( best < 0 ) {
			break;
		}
		clsIdx[printed] = best;
		printed++;
	}

	for ( i = 0; i < printed; i++ ) {
		j = clsIdx[i];
		gi.Printf( "[R201:spawn] cls[%d] %s n=%d tot=%dms peak=%dms\n",
				   i, r201_ssClass[j].name, r201_ssClass[j].count,
				   r201_ssClass[j].total, r201_ssClass[j].peak );
	}
	if ( r201_ssClassNum > printed || r201_ssOtherCount ) {
		int	restCount = r201_ssOtherCount;
		int	restTotal = r201_ssOtherTotal;

		for ( i = 0; i < r201_ssClassNum; i++ ) {
			int	seen = 0, k;

			for ( k = 0; k < printed; k++ ) {
				if ( clsIdx[k] == i ) {
					seen = 1;
					break;
				}
			}
			if ( !seen ) {
				restCount += r201_ssClass[i].count;
				restTotal += r201_ssClass[i].total;
			}
		}
		gi.Printf( "[R201:spawn] clsOther n=%d tot=%dms (classes=%d)\n",
				   restCount, restTotal, r201_ssClassNum + ( r201_ssOtherCount ? 1 : 0 ) );
	}

	if ( r201_ssSeq >= R201_SPAWN_MAX_ROWS ) {
		r201_ssStopped = 1;
		gi.Printf( "[R201:spawn] capped at %d spawns; no further [R201:spawn] blocks\n",
				   R201_SPAWN_MAX_ROWS );
	}
}
