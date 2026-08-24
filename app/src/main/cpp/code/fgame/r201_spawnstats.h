#ifndef R201_SPAWNSTATS_H
#define R201_SPAWNSTATS_H

#define R201_SPAWN_MAX_ROWS		64		
#define R201_SPAWN_MAX_CLASSES	96		
#define R201_SPAWN_TOP_CLASSES	8		

#define R201_SP_ents		0		
#define R201_SP_init		1
#define R201_SP_pinit		2
#define R201_SP_wparse		3
#define R201_SP_loop		4
#define R201_SP_spawn		5		
#define R201_SP_drain		6
#define R201_SP_start		7
#define R201_SP_precache	8		
#define R201_SP_postload	9
#define R201_SP_player		10
#define R201_SP_model		11		
#define R201_SP_pathload	12
#define R201_SP_pathcalc	13
#define R201_SP_pathopt		14
#define R201_SP_pathsave	15
#define R201_SP_findtgt		16
#define R201_SP_ZONES		17

#define R201_SPC_traces		0		
#define R201_SPC_clear		1		
#define R201_SPC_ladder		2		
#define R201_SPC_memohit	3		
#define R201_SPC_memomiss	4		
#define R201_SPC_skipped	5		
#define R201_SPC_equivok	6		
#define R201_SPC_equivbad	7
#define R201_SPC_COUNT		8

extern	int		r201_spawnOn;

int		R201_SpawnNow( void );
void	R201_SpawnEnter( const char *map, int ents );
void	R201_SpawnLeave( int spawned, int inhibited );
void	R201_SpawnZone( int zone, int msec );
void	R201_SpawnClass( const char *classname, int msec );
void	R201_SpawnCounter( int which, int add );

#define R201_SP_BEGIN(z) \
	int r201_sp_t##z = ( r201_spawnOn ? R201_SpawnNow() : 0 )
#define R201_SP_END(z) \
	do { \
		if ( r201_spawnOn ) { \
			R201_SpawnZone( R201_SP_##z, R201_SpawnNow() - r201_sp_t##z ); \
		} \
	} while ( 0 )

#endif 
