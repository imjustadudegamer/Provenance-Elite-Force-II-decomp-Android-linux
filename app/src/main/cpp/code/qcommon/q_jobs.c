/*
===========================================================================
This file is part of the port of Star Trek: Voyager -- Elite Force II, whose
engine descends from id Tech 3 through OpenMoHAA.

It is free software; you can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU General Public License, COPYING.txt at the repository
root, for more details.
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later
// Modified 2026-10-04 for this port; the changes are listed in PROVENANCE.tsv.

#include "q_jobs.h"

#if EF2_MULTITHREAD

#include <SDL_thread.h>
#include <SDL_cpuinfo.h>
#include <string.h>

#include "q_jobs_workers.h"

#define JOBPOOL_MAX_JOBS    128

typedef struct job_s {
	void ( *fn )( void *ctx, int begin, int end );
	void *ctx;
	int begin;
	int end;
} job_t;

typedef struct jobpool_s {
	const char *name;
	int critical;		

	SDL_Thread *worker[JOBPOOL_MAX_WORKERS];
	int numWorkers;

	SDL_mutex *mutex;
	SDL_cond *work;		
	SDL_cond *done;		

	SDL_mutex *producer;

	job_t jobs[JOBPOOL_MAX_JOBS];
	int numJobs;		
	int nextJob;		
	int pending;		

	qboolean running;	

	int batchSeq;		
	qboolean shutdown;
	qboolean started;
} jobpool_t;

static jobpool_t jobs_pool;

static jobpoolStats_t jobs_stats;

void JobPool_SplitWorkers( int coreCount, int isAndroid, int requested, int critical,
						   int *outWorkers )
{
	JobPool_SplitWorkersRule( coreCount, isAndroid, requested, critical, outWorkers );
}

static int JobPool_ResolveWorkers( int requested ) {
	int n;

	JobPool_SplitWorkers( SDL_GetCPUCount(),
#ifdef __ANDROID__
						  1,
#else
						  0,
#endif
						  requested, jobs_pool.critical, &n );
	return n;
}

static int SDLCALL JobPool_Worker( void *arg ) {
	jobpool_t *pool = ( jobpool_t * ) arg;

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
			job_t job = pool->jobs[ pool->nextJob++ ];
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

void JobPool_InitCritical( const char *name, int requestedWorkers ) {
	jobs_pool.critical = 1;
	JobPool_Init( name, requestedWorkers );
}

void JobPool_Init( const char *name, int requestedWorkers ) {
	int i;

	if ( jobs_pool.started ) {
		return;
	}
	jobs_pool.started = qtrue;
	jobs_pool.name = name ? name : "jobs";

	jobs_pool.numWorkers = JobPool_ResolveWorkers( requestedWorkers );
	if ( jobs_pool.numWorkers == 0 ) {
		return;
	}

	jobs_pool.mutex = SDL_CreateMutex();
	jobs_pool.work  = SDL_CreateCond();
	jobs_pool.done  = SDL_CreateCond();

	jobs_pool.producer = SDL_CreateMutex();

	if ( !jobs_pool.mutex || !jobs_pool.work || !jobs_pool.done || !jobs_pool.producer ) {
		jobs_pool.numWorkers = 0;
		if ( jobs_pool.mutex ) {
			SDL_DestroyMutex( jobs_pool.mutex );
			jobs_pool.mutex = NULL;
		}
		if ( jobs_pool.work ) {
			SDL_DestroyCond( jobs_pool.work );
			jobs_pool.work = NULL;
		}
		if ( jobs_pool.done ) {
			SDL_DestroyCond( jobs_pool.done );
			jobs_pool.done = NULL;
		}
		if ( jobs_pool.producer ) {
			SDL_DestroyMutex( jobs_pool.producer );
			jobs_pool.producer = NULL;
		}
		return;
	}

	for ( i = 0; i < jobs_pool.numWorkers; i++ ) {
		char threadName[32];
		SDL_snprintf( threadName, sizeof( threadName ), "%s%d", jobs_pool.name, i );
		jobs_pool.worker[ i ] = SDL_CreateThread( JobPool_Worker, threadName, &jobs_pool );
		if ( !jobs_pool.worker[ i ] ) {
			jobs_pool.numWorkers = i;	
			break;
		}
	}

	if ( jobs_pool.numWorkers == 0 ) {
		JobPool_Shutdown();
	}
}

void JobPool_Shutdown( void ) {
	int i;

	if ( !jobs_pool.mutex ) {
		jobs_pool.numWorkers = 0;
		jobs_pool.started = qfalse;
		return;
	}

	SDL_LockMutex( jobs_pool.mutex );
	jobs_pool.shutdown = qtrue;
	jobs_pool.running = qfalse;
	jobs_pool.numJobs = 0;
	jobs_pool.nextJob = 0;
	jobs_pool.pending = 0;
	SDL_CondBroadcast( jobs_pool.work );
	SDL_UnlockMutex( jobs_pool.mutex );

	for ( i = 0; i < JOBPOOL_MAX_WORKERS; i++ ) {
		if ( jobs_pool.worker[ i ] ) {
			SDL_WaitThread( jobs_pool.worker[ i ], NULL );
			jobs_pool.worker[ i ] = NULL;
		}
	}

	SDL_DestroyCond( jobs_pool.work );
	SDL_DestroyCond( jobs_pool.done );
	SDL_DestroyMutex( jobs_pool.mutex );
	if ( jobs_pool.producer ) {
		SDL_DestroyMutex( jobs_pool.producer );
	}
	jobs_pool.producer = NULL;
	jobs_pool.work = NULL;
	jobs_pool.done = NULL;
	jobs_pool.mutex = NULL;
	jobs_pool.numWorkers = 0;
	jobs_pool.started = qfalse;
}

qboolean JobPool_Enabled( void ) {
	return ( jobs_pool.numWorkers > 0 ) ? qtrue : qfalse;
}

int JobPool_NumWorkers( void ) {
	return jobs_pool.numWorkers;
}

const char *JobPool_Name( void ) {
	return jobs_pool.name;
}

void JobPool_BeginBatch( void ) {
	if ( jobs_pool.producer ) {
		SDL_LockMutex( jobs_pool.producer );
	}
	jobs_pool.numJobs = 0;
	jobs_pool.nextJob = 0;
	jobs_pool.pending = 0;
}

void JobPool_Add( void ( *fn )( void *ctx, int begin, int end ), void *ctx, int begin, int end ) {
	job_t *job;

	if ( !fn || jobs_pool.numJobs >= JOBPOOL_MAX_JOBS ) {
		return;		
	}
	job = &jobs_pool.jobs[ jobs_pool.numJobs++ ];
	job->fn = fn;
	job->ctx = ctx;
	job->begin = begin;
	job->end = end;
}

void JobPool_RunAndWait( void ) {
	SDL_LockMutex( jobs_pool.mutex );

	jobs_pool.nextJob = 0;
	jobs_pool.pending = jobs_pool.numJobs;
	jobs_pool.batchSeq++;

	if ( jobs_pool.pending > 0 ) {
		jobs_pool.running = qtrue;
		SDL_CondBroadcast( jobs_pool.work );

		while ( jobs_pool.running ) {
			SDL_CondWait( jobs_pool.done, jobs_pool.mutex );
		}
	}

	jobs_pool.numJobs = 0;
	SDL_UnlockMutex( jobs_pool.mutex );

	if ( jobs_pool.producer ) {
		SDL_UnlockMutex( jobs_pool.producer );
	}
}

void JobPool_ParallelRange( int count, int minChunk,
							void ( *fn )( void *ctx, int begin, int end ), void *ctx ) {
	int chunks, per, i;

	if ( !fn || count <= 0 ) {
		return;
	}
	if ( minChunk < 1 ) {
		minChunk = 1;
	}

	if ( !JobPool_Enabled() ) {
		fn( ctx, 0, count );
		return;
	}
	if ( count <= minChunk ) {
		jobs_stats.inlineCalls++;
		jobs_stats.inlineElements += count;
		fn( ctx, 0, count );
		return;
	}

	chunks = ( count + minChunk - 1 ) / minChunk;
	if ( chunks > jobs_pool.numWorkers * 2 ) {
		chunks = jobs_pool.numWorkers * 2;
	}
	if ( chunks < 2 ) {
		jobs_stats.inlineCalls++;
		jobs_stats.inlineElements += count;
		fn( ctx, 0, count );
		return;
	}

	per = ( count + chunks - 1 ) / chunks;

	JobPool_BeginBatch();
	for ( i = 0; i < chunks; i++ ) {
		int begin = i * per;
		int end = begin + per;
		if ( end > count ) {
			end = count;
		}
		if ( begin >= end ) {
			break;
		}
		JobPool_Add( fn, ctx, begin, end );
	}
	if ( jobs_pool.numJobs == 0 ) {
		JobPool_RunAndWait();
		jobs_stats.inlineCalls++;
		jobs_stats.inlineElements += count;
		fn( ctx, 0, count );
		return;
	}

	jobs_stats.batches++;
	jobs_stats.parallelElements += count;

	JobPool_RunAndWait();
}

void JobPool_GetStats( jobpoolStats_t *out ) {
	if ( !out ) {
		return;
	}
	*out = jobs_stats;
	out->workers = jobs_pool.numWorkers;
}

void JobPool_ResetStats( void ) {
	int workers = jobs_pool.numWorkers;

	memset( &jobs_stats, 0, sizeof( jobs_stats ) );
	jobs_stats.workers = workers;
}

typedef struct selftest_s {
	int count;
	int hits[ JOBPOOL_MAX_JOBS * 8 ];
} selftest_t;

static void JobPool_TestBody( void *ctx, int begin, int end ) {
	selftest_t *t = ( selftest_t * ) ctx;
	int i;

	for ( i = begin; i < end; i++ ) {
		t->hits[ i ]++;
	}
}

qboolean JobPool_SelfTest( JobPoolPrint print ) {
	selftest_t t;
	int i, ok = 1, chunks;

	if ( !print ) {
		return qfalse;
	}
	memset( &t, 0, sizeof( t ) );
	t.count = jobs_pool.numWorkers > 0 ? jobs_pool.numWorkers * 4 : 4;
	if ( t.count > ( int )( sizeof( t.hits ) / sizeof( t.hits[ 0 ] ) ) ) {
		t.count = ( int )( sizeof( t.hits ) / sizeof( t.hits[ 0 ] ) );
	}

	chunks = jobs_pool.numWorkers * 2;
	JobPool_BeginBatch();
	for ( i = 0; i < chunks; i++ ) {
		int step = ( t.count + chunks - 1 ) / chunks;
		int b = i * step;
		int e = b + step;
		if ( e > t.count ) {
			e = t.count;
		}
		if ( b < e ) {
			JobPool_Add( JobPool_TestBody, &t, b, e );
		}
	}
	JobPool_RunAndWait();
	for ( i = 0; i < t.count; i++ ) {
		if ( t.hits[ i ] != 1 ) {
			ok = 0;
		}
	}

	memset( t.hits, 0, sizeof( t.hits ) );
	JobPool_BeginBatch();
	JobPool_Add( JobPool_TestBody, &t, 0, t.count );
	JobPool_RunAndWait();
	for ( i = 0; i < t.count; i++ ) {
		if ( t.hits[ i ] != 1 ) {
			ok = 0;
		}
	}

	memset( t.hits, 0, sizeof( t.hits ) );
	JobPool_ParallelRange( t.count, t.count + 1, JobPool_TestBody, &t );
	for ( i = 0; i < t.count; i++ ) {
		if ( t.hits[ i ] != 1 ) {
			ok = 0;
		}
	}

	print( "jobpooltest: %s (%s, %d workers, %d ranges of %d elements)\n",
		   ok ? "PASS" : "FAIL", jobs_pool.name, jobs_pool.numWorkers, chunks, t.count );
	return ok ? qtrue : qfalse;
}

#else	

#include "q_jobs_workers.h"

void JobPool_Init( const char *name, int requestedWorkers ) {
	( void ) name;
	( void ) requestedWorkers;
}

void JobPool_InitCritical( const char *name, int requestedWorkers ) {
	( void ) name;
	( void ) requestedWorkers;
}

void JobPool_SplitWorkers( int coreCount, int isAndroid, int requested, int critical,
						   int *outWorkers ) {
	JobPool_SplitWorkersRule( coreCount, isAndroid, requested, critical, outWorkers );
}

void JobPool_Shutdown( void ) {
}

qboolean JobPool_Enabled( void ) {
	return qfalse;
}

int JobPool_NumWorkers( void ) {
	return 0;
}

const char *JobPool_Name( void ) {
	return "jobs";
}

void JobPool_BeginBatch( void ) {
}

void JobPool_Add( void ( *fn )( void *ctx, int begin, int end ), void *ctx, int begin, int end ) {
	( void ) fn;
	( void ) ctx;
	( void ) begin;
	( void ) end;
}

void JobPool_RunAndWait( void ) {
}

void JobPool_ParallelRange( int count, int minChunk,
							void ( *fn )( void *ctx, int begin, int end ), void *ctx ) {
	( void ) minChunk;
	if ( !fn || count <= 0 ) {
		return;
	}
	fn( ctx, 0, count );
}

qboolean JobPool_SelfTest( JobPoolPrint print ) {
	if ( print ) {
		print( "jobpooltest: SKIPPED (EF2_MULTITHREAD=0 build)\n" );
	}
	return qtrue;
}

void JobPool_GetStats( jobpoolStats_t *out ) {
	if ( !out ) {
		return;
	}
	memset( out, 0, sizeof( *out ) );
}

void JobPool_ResetStats( void ) {
}

#endif	

static int JobPool_MeterPercentile( const jobpoolMeter_t *m, int pct ) {
	int want, i, acc = 0;

	if ( m->histTotal <= 0 ) {
		return 0;
	}
	want = ( m->histTotal * pct + 99 ) / 100;
	for ( i = 0; i < JOBPOOL_METER_BUCKETS; i++ ) {
		acc += m->hist[i];
		if ( acc >= want ) {
			return i;
		}
	}
	return JOBPOOL_METER_BUCKETS - 1;
}

static void JobPool_MeterReset( jobpoolMeter_t *m, int nowMs ) {
	m->frames = 0;
	m->frontMsec = 0;
	m->backMsec = 0;
	m->handoffMsec = 0;
	m->maxInterval = 0;
	m->histTotal = 0;
	memset( m->hist, 0, sizeof( m->hist ) );
	m->windowStart = nowMs;
}

void JobPool_MeterFrame( jobpoolMeter_t *m, JobPoolPrint print, int nowMs,
						 int frontMsec, int backMsec, int handoffMsec, int smpActive ) {
	jobpoolStats_t st;
	int elapsed, interval;
	long long totalElements;
	double parallelPct;

	if ( !m ) {
		return;
	}

	if ( m->windowStart == 0 ) {
		JobPool_MeterReset( m, nowMs );
		m->prevFrameMs = nowMs;
		return;
	}

	interval = nowMs - m->prevFrameMs;
	m->prevFrameMs = nowMs;
	if ( interval < 0 ) {
		interval = 0;
	}
	if ( interval > m->maxInterval ) {
		m->maxInterval = interval;
	}
	m->hist[ interval < JOBPOOL_METER_BUCKETS ? interval : JOBPOOL_METER_BUCKETS - 1 ]++;
	m->histTotal++;
	m->frames++;
	m->frontMsec += frontMsec;
	m->backMsec += backMsec;
	m->handoffMsec += handoffMsec;

	elapsed = nowMs - m->windowStart;
	if ( elapsed < 1000 ) {
		return;
	}

	JobPool_GetStats( &st );
	totalElements = st.parallelElements + st.inlineElements;
	parallelPct = totalElements > 0 ? 100.0 * (double)st.parallelElements / (double)totalElements : 0.0;

	if ( print ) {
		print( "[MT] %i frames/%i ms %.1f fps | interval p50 %i p95 %i p99 %i max %i ms | "
			   "front %.2f ms/f back %.2f ms/f | pool %s %iw %i batches %lld el parallel %.1f%% "
			   "%i inline %lld el | r_smp %i handoff %.2f ms/f\n",
			   m->frames, elapsed, m->frames * 1000.0 / (double)elapsed,
			   JobPool_MeterPercentile( m, 50 ), JobPool_MeterPercentile( m, 95 ),
			   JobPool_MeterPercentile( m, 99 ), m->maxInterval,
			   (double)m->frontMsec / (double)m->frames,
			   (double)m->backMsec / (double)m->frames,
			   JobPool_Name(), st.workers, st.batches, st.parallelElements, parallelPct,
			   st.inlineCalls, st.inlineElements,
			   smpActive ? 1 : 0,
			   (double)m->handoffMsec / (double)m->frames );
	}

	JobPool_MeterReset( m, nowMs );
}
