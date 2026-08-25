#include "g_local.h"
#include "level.h"
#include "smp_spawnstats.h"

int		smp_spawnOn = 0;

static int	smp_ssSums[SMP_SP_ZONES];
static int	smp_ssMaxs[SMP_SP_ZONES];
static int	smp_ssCounters[SMP_SPC_COUNT];
static int	smp_ssSpawned;
static int	smp_ssInhibited;
static int	smp_ssEnts;
static int	smp_ssSeq;
static int	smp_ssStopped;

static struct {
	char	name[40];
	int		count;
	int		total;
	int		peak;
} smp_ssClass[SMP_SPAWN_MAX_CLASSES];
static int	smp_ssClassNum;
static int	smp_ssOtherCount;
static int	smp_ssOtherTotal;

int SMP_SpawnNow( void ) {
	if ( !smp_spawnOn ) {
		return 0;
	}
	return gi.Milliseconds();
}

void SMP_SpawnEnter( const char *map, int ents ) {
	static cvar_t	*smp_spawnStats;
	int				i;

	if ( !smp_spawnStats ) {
		smp_spawnStats = gi.cvar( "smp_spawnStats", "1", 0 );
	}

	(void)map;
	smp_spawnOn = ( smp_spawnStats && smp_spawnStats->integer ) ? 1 : 0;

	for ( i = 0; i < SMP_SP_ZONES; i++ ) {
		smp_ssSums[i] = smp_ssMaxs[i] = 0;
	}
	for ( i = 0; i < SMP_SPC_COUNT; i++ ) {
		smp_ssCounters[i] = 0;
	}
	for ( i = 0; i < smp_ssClassNum; i++ ) {
		smp_ssClass[i].name[0] = '\0';
		smp_ssClass[i].count = smp_ssClass[i].total = smp_ssClass[i].peak = 0;
	}
	smp_ssClassNum    = 0;
	smp_ssOtherCount  = 0;
	smp_ssOtherTotal  = 0;
	smp_ssSpawned     = 0;
	smp_ssInhibited   = 0;
	smp_ssEnts        = ents;
}

void SMP_SpawnZone( int zone, int msec ) {
	if ( !smp_spawnOn || zone < 0 || zone >= SMP_SP_ZONES ) {
		return;
	}
	if ( msec < 0 ) {
		msec = 0;
	}

	smp_ssSums[zone] += msec;
	if ( msec > smp_ssMaxs[zone] ) {
		smp_ssMaxs[zone] = msec;
	}
}

void SMP_SpawnCounter( int which, int add ) {
	if ( !smp_spawnOn || which < 0 || which >= SMP_SPC_COUNT ) {
		return;
	}
	smp_ssCounters[which] += add;
}

void SMP_SpawnClass( const char *classname, int msec ) {
	int		i;
	char	name[40];

	if ( !smp_spawnOn ) {
		return;
	}

	SMP_SpawnZone( SMP_SP_spawn, msec );

	if ( !classname || !classname[0] ) {
		classname = "Unspecified";
	}
	Q_strncpyz( name, classname, sizeof( name ) );

	for ( i = 0; i < smp_ssClassNum; i++ ) {
		if ( !Q_stricmp( smp_ssClass[i].name, name ) ) {
			smp_ssClass[i].count++;
			smp_ssClass[i].total += msec;
			if ( msec > smp_ssClass[i].peak ) {
				smp_ssClass[i].peak = msec;
			}
			return;
		}
	}

	if ( smp_ssClassNum >= SMP_SPAWN_MAX_CLASSES ) {
		smp_ssOtherCount++;
		smp_ssOtherTotal += msec;
		return;
	}

	Q_strncpyz( smp_ssClass[smp_ssClassNum].name, name, sizeof( smp_ssClass[0].name ) );
	smp_ssClass[smp_ssClassNum].count  = 1;
	smp_ssClass[smp_ssClassNum].total  = msec;
	smp_ssClass[smp_ssClassNum].peak   = msec;
	smp_ssClassNum++;
}

void SMP_SpawnLeave( int spawned, int inhibited ) {
	static const char *zoneName[SMP_SP_ZONES] = {
		"ents", "init", "pinit", "wparse", "loop", "spawn", "drain", "start",
		"precache", "postload", "player", "model",
		"pathload", "pathcalc", "pathopt", "pathsave", "findtgt"
	};
	int		i, j, best, printed, children, drainChildren, drainOther, unacct;
	int		clsIdx[SMP_SPAWN_TOP_CLASSES];
	char	line[1024], part[64];

	if ( !smp_spawnOn || smp_ssStopped ) {
		return;
	}

	smp_ssSpawned   = spawned;
	smp_ssInhibited = inhibited;
	smp_ssSeq++;

	children = smp_ssSums[SMP_SP_init] + smp_ssSums[SMP_SP_pinit]
			 + smp_ssSums[SMP_SP_wparse] + smp_ssSums[SMP_SP_loop]
			 + smp_ssSums[SMP_SP_drain] + smp_ssSums[SMP_SP_start]
			 + smp_ssSums[SMP_SP_postload] + smp_ssSums[SMP_SP_player];
	unacct = smp_ssSums[SMP_SP_ents] - children;
	if ( unacct < 0 ) {
		unacct = 0;		
	}

	drainChildren = smp_ssSums[SMP_SP_pathload] + smp_ssSums[SMP_SP_pathcalc]
				  + smp_ssSums[SMP_SP_pathopt] + smp_ssSums[SMP_SP_pathsave]
				  + smp_ssSums[SMP_SP_findtgt];
	drainOther = smp_ssSums[SMP_SP_drain] - drainChildren;
	if ( drainOther < 0 ) {
		drainOther = 0;
	}

	line[0] = '\0';
	Com_sprintf( part, sizeof( part ), "seq=%d map=%s ents=%d spawned=%d inh=%d total=%dms max=%dms",
				 smp_ssSeq, level.mapname.c_str(), smp_ssEnts, smp_ssSpawned,
				 smp_ssInhibited, smp_ssSums[SMP_SP_ents], smp_ssMaxs[SMP_SP_ents] );
	Q_strcat( line, sizeof( line ), part );
	for ( i = SMP_SP_init; i <= SMP_SP_player; i++ ) {
		Com_sprintf( part, sizeof( part ), " %s=%d/%d", zoneName[i],
					 smp_ssSums[i], smp_ssMaxs[i] );
		Q_strcat( line, sizeof( line ), part );
	}
	Com_sprintf( part, sizeof( part ), " parse=%d spawn=%d model=%d unacct=%d",
				 smp_ssSums[SMP_SP_loop] - smp_ssSums[SMP_SP_spawn],
				 smp_ssSums[SMP_SP_spawn], smp_ssSums[SMP_SP_model], unacct );
	Q_strcat( line, sizeof( line ), part );
	gi.Printf( "[SMP:spawn] %s\n", line );

	line[0] = '\0';
	for ( i = SMP_SP_pathload; i < SMP_SP_ZONES; i++ ) {
		Com_sprintf( part, sizeof( part ), " %s=%d/%d", zoneName[i],
					 smp_ssSums[i], smp_ssMaxs[i] );
		Q_strcat( line, sizeof( line ), part );
	}
	Com_sprintf( part, sizeof( part ),
				 " other=%d traces=%d clear=%d ladder=%d memohit=%d memomiss=%d skipped=%d equivOK=%d equivBAD=%d",
				 drainOther, smp_ssCounters[SMP_SPC_traces],
				 smp_ssCounters[SMP_SPC_clear], smp_ssCounters[SMP_SPC_ladder],
				 smp_ssCounters[SMP_SPC_memohit], smp_ssCounters[SMP_SPC_memomiss],
				 smp_ssCounters[SMP_SPC_skipped], smp_ssCounters[SMP_SPC_equivok],
				 smp_ssCounters[SMP_SPC_equivbad] );
	Q_strcat( line, sizeof( line ), part );
	gi.Printf( "[SMP:spawn] drain:%s\n", line );

	for ( i = 0; i < SMP_SPAWN_TOP_CLASSES; i++ ) {
		clsIdx[i] = -1;
	}
	printed = 0;
	while ( printed < SMP_SPAWN_TOP_CLASSES ) {
		best = -1;
		for ( j = 0; j < smp_ssClassNum; j++ ) {
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
			if ( best < 0 || smp_ssClass[j].total > smp_ssClass[best].total ) {
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
		gi.Printf( "[SMP:spawn] cls[%d] %s n=%d tot=%dms peak=%dms\n",
				   i, smp_ssClass[j].name, smp_ssClass[j].count,
				   smp_ssClass[j].total, smp_ssClass[j].peak );
	}
	if ( smp_ssClassNum > printed || smp_ssOtherCount ) {
		int	restCount = smp_ssOtherCount;
		int	restTotal = smp_ssOtherTotal;

		for ( i = 0; i < smp_ssClassNum; i++ ) {
			int	seen = 0, k;

			for ( k = 0; k < printed; k++ ) {
				if ( clsIdx[k] == i ) {
					seen = 1;
					break;
				}
			}
			if ( !seen ) {
				restCount += smp_ssClass[i].count;
				restTotal += smp_ssClass[i].total;
			}
		}
		gi.Printf( "[SMP:spawn] clsOther n=%d tot=%dms (classes=%d)\n",
				   restCount, restTotal, smp_ssClassNum + ( smp_ssOtherCount ? 1 : 0 ) );
	}

	if ( smp_ssSeq >= SMP_SPAWN_MAX_ROWS ) {
		smp_ssStopped = 1;
		gi.Printf( "[SMP:spawn] capped at %d spawns; no further [SMP:spawn] blocks\n",
				   SMP_SPAWN_MAX_ROWS );
	}
}
