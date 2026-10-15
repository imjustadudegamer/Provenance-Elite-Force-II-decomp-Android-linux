/* The pool this change replaced: q_jobs.c at git HEAD, symbols prefixed, verbatim. */
#include <SDL_thread.h>
#include <SDL_cpuinfo.h>
#include <string.h>

#define OLDP_MAX_WORKERS 64

typedef struct oldpoolStats_s {
	int			workers;
	int			batches;
	int			inlineCalls;
	int			droppedRanges;
	long long	parallelElements;
	long long	inlineElements;
} oldpoolStats_t;

void	OldPool_Init( const char *name, int requestedWorkers );
void	OldPool_Shutdown( void );
qboolean	OldPool_Enabled( void );
int		OldPool_NumWorkers( void );
void	OldPool_BeginBatch( void );
qboolean	OldPool_AddChecked( void ( *fn )( void *ctx, int begin, int end ), void *ctx, int begin, int end );
void	OldPool_RunAndWait( void );
void	OldPool_ParallelRange( int count, int minChunk, void ( *fn )( void *ctx, int begin, int end ), void *ctx );







#define OLDP_MAX_JOBS    128

typedef struct oldjob_s {
	void ( *fn )( void *ctx, int begin, int end );
	void *ctx;
	int begin;
	int end;
} oldjob_t;

typedef struct jobpool_s {
	const char *name;
	int critical;		

	SDL_Thread *worker[OLDP_MAX_WORKERS];
	int numWorkers;

	SDL_mutex *mutex;
	SDL_cond *work;		
	SDL_cond *done;		

	SDL_mutex *producer;

	oldjob_t jobs[OLDP_MAX_JOBS];
	int numJobs;		
	int nextJob;		
	int pending;		

	qboolean running;	

	int batchSeq;		
	qboolean shutdown;
	qboolean started;
} oldpool_t;

static oldpool_t oldp_pool;

static oldpoolStats_t oldp_stats;

void OldPool_SplitWorkers( int coreCount, int isAndroid, int requested, int critical,
						   int *outWorkers )
{
	JobPool_SplitWorkersRule( coreCount, isAndroid, requested, critical, outWorkers );
}

static int OldPool_ResolveWorkers( int requested ) {
	int n;

	OldPool_SplitWorkers( SDL_GetCPUCount(),
#ifdef __ANDROID__
						  1,
#else
						  0,
#endif
						  requested, oldp_pool.critical, &n );
	return n;
}

static int SDLCALL OldPool_Worker( void *arg ) {
	oldpool_t *pool = ( oldpool_t * ) arg;

	for ( ;; ) {
		SDL_LockMutex( pool->mutex );

		while ( !pool->shutdown && !pool->running ) {
			SDL_CondWait( pool->work, pool->mutex );
		}

		if ( pool->shutdown ) {
			SDL_UnlockMutex( pool->mutex );
			return 0;
		}

		if ( pool->nextJob < pool->numJobs ) {
			oldjob_t job = pool->jobs[ pool->nextJob++ ];
			SDL_UnlockMutex( pool->mutex );

			job.fn( job.ctx, job.begin, job.end );

			SDL_LockMutex( pool->mutex );
			pool->pending--;
			if ( pool->pending == 0 ) {
				pool->running = qfalse;
				SDL_CondSignal( pool->done );
			}
			SDL_UnlockMutex( pool->mutex );
		} else {
			SDL_UnlockMutex( pool->mutex );
		}
	}
}

void OldPool_InitCritical( const char *name, int requestedWorkers ) {
	oldp_pool.critical = 1;

	/* One private instance per module (q_jobs.h T2): this oldp_pool is the
	   calling module's own, so a client pool that started earlier never set
	   `started' here and there is no client pool to take over.  A pool that IS
	   already running is adopted, never rebuilt: OldPool_Shutdown zeroes
	   numJobs/nextJob/pending without the producer lock, so a caller sitting
	   inside OldPool_ParallelRange would reach RunAndWait with numJobs 0, run a
	   batch of zero ranges and lose every one of them -- and a caller still in
	   its Add loop would write over the reset cursor, running some ranges twice
	   and not at all.  RE_Shutdown joins the walk and stops the pool first, so
	   a restart that does want a new worker count goes through Shutdown on a
	   quiesced seam and keeps that path. */
	if ( oldp_pool.started ) {
		return;
	}

	OldPool_Init( name, requestedWorkers );
}

void OldPool_Init( const char *name, int requestedWorkers ) {
	int i;

	if ( oldp_pool.started ) {
		return;
	}
	oldp_pool.started = qtrue;
	oldp_pool.name = name ? name : "jobs";

	oldp_pool.numWorkers = OldPool_ResolveWorkers( requestedWorkers );
	if ( oldp_pool.numWorkers == 0 ) {
		return;
	}

	oldp_pool.mutex = SDL_CreateMutex();
	oldp_pool.work  = SDL_CreateCond();
	oldp_pool.done  = SDL_CreateCond();

	oldp_pool.producer = SDL_CreateMutex();

	if ( !oldp_pool.mutex || !oldp_pool.work || !oldp_pool.done || !oldp_pool.producer ) {
		oldp_pool.numWorkers = 0;
		if ( oldp_pool.mutex ) {
			SDL_DestroyMutex( oldp_pool.mutex );
			oldp_pool.mutex = NULL;
		}
		if ( oldp_pool.work ) {
			SDL_DestroyCond( oldp_pool.work );
			oldp_pool.work = NULL;
		}
		if ( oldp_pool.done ) {
			SDL_DestroyCond( oldp_pool.done );
			oldp_pool.done = NULL;
		}
		if ( oldp_pool.producer ) {
			SDL_DestroyMutex( oldp_pool.producer );
			oldp_pool.producer = NULL;
		}
		return;
	}

	for ( i = 0; i < oldp_pool.numWorkers; i++ ) {
		char threadName[32];
		SDL_snprintf( threadName, sizeof( threadName ), "%s%d", oldp_pool.name, i );
		oldp_pool.worker[ i ] = SDL_CreateThread( OldPool_Worker, threadName, &oldp_pool );
		if ( !oldp_pool.worker[ i ] ) {
			oldp_pool.numWorkers = i;	
			break;
		}
	}

	if ( oldp_pool.numWorkers == 0 ) {
		OldPool_Shutdown();
	}
}

void OldPool_Shutdown( void ) {
	int i;

	if ( !oldp_pool.mutex ) {
		oldp_pool.numWorkers = 0;
		oldp_pool.started = qfalse;
		return;
	}

	SDL_LockMutex( oldp_pool.mutex );
	oldp_pool.shutdown = qtrue;
	oldp_pool.running = qfalse;
	oldp_pool.numJobs = 0;
	oldp_pool.nextJob = 0;
	oldp_pool.pending = 0;
	SDL_CondBroadcast( oldp_pool.work );
	SDL_UnlockMutex( oldp_pool.mutex );

	for ( i = 0; i < OLDP_MAX_WORKERS; i++ ) {
		if ( oldp_pool.worker[ i ] ) {
			SDL_WaitThread( oldp_pool.worker[ i ], NULL );
			oldp_pool.worker[ i ] = NULL;
		}
	}

	SDL_DestroyCond( oldp_pool.work );
	SDL_DestroyCond( oldp_pool.done );
	SDL_DestroyMutex( oldp_pool.mutex );
	if ( oldp_pool.producer ) {
		SDL_DestroyMutex( oldp_pool.producer );
	}
	oldp_pool.producer = NULL;
	oldp_pool.work = NULL;
	oldp_pool.done = NULL;
	oldp_pool.mutex = NULL;
	oldp_pool.numWorkers = 0;
	oldp_pool.started = qfalse;
}

qboolean OldPool_Enabled( void ) {
	return ( oldp_pool.numWorkers > 0 ) ? qtrue : qfalse;
}

int OldPool_NumWorkers( void ) {
	return oldp_pool.numWorkers;
}

const char *OldPool_Name( void ) {
	return oldp_pool.name;
}

void OldPool_BeginBatch( void ) {
	if ( oldp_pool.producer ) {
		SDL_LockMutex( oldp_pool.producer );
	}
	oldp_pool.numJobs = 0;
	oldp_pool.nextJob = 0;
	oldp_pool.pending = 0;
}

qboolean OldPool_AddChecked( void ( *fn )( void *ctx, int begin, int end ), void *ctx,
							 int begin, int end ) {
	oldjob_t *job;

	if ( !fn ) {
		return qfalse;
	}
	if ( oldp_pool.numJobs >= OLDP_MAX_JOBS ) {
		/* A refused range has not run and would never run: count it, so the
		   caller that checks can run the range itself instead of silently
		   losing the output it covers. */
		oldp_stats.droppedRanges++;
		return qfalse;
	}
	job = &oldp_pool.jobs[ oldp_pool.numJobs++ ];
	job->fn = fn;
	job->ctx = ctx;
	job->begin = begin;
	job->end = end;
	return qtrue;
}

void OldPool_Add( void ( *fn )( void *ctx, int begin, int end ), void *ctx, int begin, int end ) {
	OldPool_AddChecked( fn, ctx, begin, end );
}

void OldPool_RunAndWait( void ) {
	SDL_LockMutex( oldp_pool.mutex );

	oldp_pool.nextJob = 0;
	oldp_pool.pending = oldp_pool.numJobs;
	oldp_pool.batchSeq++;

	if ( oldp_pool.pending > 0 ) {
		oldp_pool.running = qtrue;
		SDL_CondBroadcast( oldp_pool.work );

		while ( oldp_pool.running ) {
			SDL_CondWait( oldp_pool.done, oldp_pool.mutex );
		}
	}

	oldp_pool.numJobs = 0;
	SDL_UnlockMutex( oldp_pool.mutex );

	if ( oldp_pool.producer ) {
		SDL_UnlockMutex( oldp_pool.producer );
	}
}

void OldPool_ParallelRange( int count, int minChunk,
							void ( *fn )( void *ctx, int begin, int end ), void *ctx ) {
	int chunks, per, i, ranInline = 0;

	if ( !fn || count <= 0 ) {
		return;
	}
	if ( minChunk < 1 ) {
		minChunk = 1;
	}

	if ( !OldPool_Enabled() ) {
		fn( ctx, 0, count );
		return;
	}
	if ( count <= minChunk ) {
		oldp_stats.inlineCalls++;
		oldp_stats.inlineElements += count;
		fn( ctx, 0, count );
		return;
	}

	chunks = ( count + minChunk - 1 ) / minChunk;
	if ( chunks > oldp_pool.numWorkers * 2 ) {
		chunks = oldp_pool.numWorkers * 2;
	}
	if ( chunks < 2 ) {
		oldp_stats.inlineCalls++;
		oldp_stats.inlineElements += count;
		fn( ctx, 0, count );
		return;
	}

	per = ( count + chunks - 1 ) / chunks;

	OldPool_BeginBatch();
	for ( i = 0; i < chunks; i++ ) {
		int begin = i * per;
		int end = begin + per;
		if ( end > count ) {
			end = count;
		}
		if ( begin >= end ) {
			break;
		}
		if ( !OldPool_AddChecked( fn, ctx, begin, end ) ) {
			/* The table is full, so this range is not queued: run it here.  The
			   contract makes the chunks disjoint, so the producer thread running
			   one of them beside the queued ones is the same set of writes, and
			   it still finishes before RunAndWait joins the batch.  Dropping it
			   would leave exactly the elements the range covers unupdated. */
			oldp_stats.inlineCalls++;
			oldp_stats.inlineElements += end - begin;
			ranInline = 1;
			fn( ctx, begin, end );
		}
	}
	/* Nothing queued and something already run: the batch is done, so the
	   whole-range fallback below would run those chunks a second time. */
	if ( oldp_pool.numJobs == 0 && !ranInline ) {
		OldPool_RunAndWait();
		oldp_stats.inlineCalls++;
		oldp_stats.inlineElements += count;
		fn( ctx, 0, count );
		return;
	}

	oldp_stats.batches++;
	oldp_stats.parallelElements += count;

	OldPool_RunAndWait();
}

void OldPool_GetStats( oldpoolStats_t *out ) {
	if ( !out ) {
		return;
	}
	*out = oldp_stats;
	out->workers = oldp_pool.numWorkers;
}

void OldPool_ResetStats( void ) {
	int workers = oldp_pool.numWorkers;

	memset( &oldp_stats, 0, sizeof( oldp_stats ) );
	oldp_stats.workers = workers;
}

