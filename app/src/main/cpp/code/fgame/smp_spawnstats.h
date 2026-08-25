#ifndef SMP_SPAWNSTATS_H
#define SMP_SPAWNSTATS_H

#define SMP_SPAWN_MAX_ROWS		64		
#define SMP_SPAWN_MAX_CLASSES	96		
#define SMP_SPAWN_TOP_CLASSES	8		

#define SMP_SP_ents		0		
#define SMP_SP_init		1
#define SMP_SP_pinit		2
#define SMP_SP_wparse		3
#define SMP_SP_loop		4
#define SMP_SP_spawn		5		
#define SMP_SP_drain		6
#define SMP_SP_start		7
#define SMP_SP_precache	8		
#define SMP_SP_postload	9
#define SMP_SP_player		10
#define SMP_SP_model		11		
#define SMP_SP_pathload	12
#define SMP_SP_pathcalc	13
#define SMP_SP_pathopt		14
#define SMP_SP_pathsave	15
#define SMP_SP_findtgt		16
#define SMP_SP_ZONES		17

#define SMP_SPC_traces		0		
#define SMP_SPC_clear		1		
#define SMP_SPC_ladder		2		
#define SMP_SPC_memohit	3		
#define SMP_SPC_memomiss	4		
#define SMP_SPC_skipped	5		
#define SMP_SPC_equivok	6		
#define SMP_SPC_equivbad	7
#define SMP_SPC_COUNT		8

extern	int		smp_spawnOn;

int		SMP_SpawnNow( void );
void	SMP_SpawnEnter( const char *map, int ents );
void	SMP_SpawnLeave( int spawned, int inhibited );
void	SMP_SpawnZone( int zone, int msec );
void	SMP_SpawnClass( const char *classname, int msec );
void	SMP_SpawnCounter( int which, int add );

#define SMP_SP_BEGIN(z) \
	int smp_sp_t##z = ( smp_spawnOn ? SMP_SpawnNow() : 0 )
#define SMP_SP_END(z) \
	do { \
		if ( smp_spawnOn ) { \
			SMP_SpawnZone( SMP_SP_##z, SMP_SpawnNow() - smp_sp_t##z ); \
		} \
	} while ( 0 )

#endif 
