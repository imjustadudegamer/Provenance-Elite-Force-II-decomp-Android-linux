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
/*
===========================================================================
THE JOB POOL -- the worker/threading primitives for the port.

ONE implementation, ONE private instance per module: the client executable and
each renderer .so compile q_jobs.c for itself (cmake/client.cmake and
cmake/renderer_common.cmake), so no refimport_t slot is added and no cross-.so
symbol ownership appears.  The dedicated server does not compile it.

Because the instance is private to the translation unit, the API carries no
pool handle at all -- every call below addresses the calling module's own
pool, and its workers never escape it.

DECISIONS:

  T1  SDL2 primitives only (SDL_CreateThread/SDL_mutex/SDL_cond), the same
      primitives the GLES3 render-thread layer uses (renderergles3/
      gles3_renderthread.c) and the only portable threading API the vendored
      SDL2 exposes on both targets.
  T2  one implementation, one instance per module.
  T3  the dedicated server never sees it.
  T4  the whole layer is removable at compile time (EF2_MULTITHREAD=0); with
      it off JobPool_Enabled() reports false and every call site runs the
      byte-identical serial loop.
  T5  a worker NEVER allocates, logs, raises an error, reads a file, touches a
      cvar or draws a random number.  A consumer that needs any of those does
      them on the main thread before it kicks the batch.

THE CONTRACT a JobPool_ParallelRange call site must satisfy:

  1. the input range is final before the batch is kicked;
  2. each output element is a pure function of that element's input plus the
     immutable context pointer;
  3. output elements are disjoint across the split (no two chunks write the
     same byte);
  4. the loop body calls nothing banned by T5;
  5. the merged result is identical whichever worker runs which chunk -- the
     per-element expression order is unchanged, so there is no new floating
     point association and no new nondeterminism.

Copyright is the enclosing project's (GPL, COPYING.txt at the tree root).
===========================================================================
*/
#ifndef __QJOBS_H__
#define __QJOBS_H__

#include "../qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

void JobPool_Init( const char *name, int requestedWorkers );

#include "q_jobs_workers.h"

void JobPool_SplitWorkers( int coreCount, int isAndroid, int requested, int critical,
						   int *outWorkers );

void JobPool_InitCritical( const char *name, int requestedWorkers );

void JobPool_Shutdown( void );

qboolean JobPool_Enabled( void );

int JobPool_NumWorkers( void );

const char *JobPool_Name( void );

void JobPool_BeginBatch( void );
void JobPool_Add( void ( *fn )( void *ctx, int begin, int end ), void *ctx, int begin, int end );
/* qfalse when the batch table refused the range, which means the range has NOT
   run; a caller that cares runs it itself.  JobPool_Add is the fire-and-forget
   form and keeps its historical signature. */
qboolean JobPool_AddChecked( void ( *fn )( void *ctx, int begin, int end ), void *ctx,
							 int begin, int end );
void JobPool_RunAndWait( void );

void JobPool_ParallelRange( int count, int minChunk,
							void ( *fn )( void *ctx, int begin, int end ), void *ctx );

#if defined( __GNUC__ ) || defined( __clang__ )
typedef void ( *JobPoolPrint )( const char *fmt, ... ) __attribute__( ( format( printf, 1, 2 ) ) );
#else
typedef void ( *JobPoolPrint )( const char *fmt, ... );
#endif
qboolean JobPool_SelfTest( JobPoolPrint print );

typedef struct jobpoolStats_s {
	int			workers;
	int			batches;
	int			inlineCalls;
	/* ranges the batch table refused; each one is a range the caller's output
	   would have been missing until the caller runs it itself */
	int			droppedRanges;
	long long	parallelElements;
	long long	inlineElements;
} jobpoolStats_t;

void JobPool_GetStats( jobpoolStats_t *out );
void JobPool_ResetStats( void );

#define JOBPOOL_METER_BUCKETS 200	

typedef struct jobpoolMeter_s {
	int		frames;
	int		frontMsec;
	int		backMsec;
	int		handoffMsec;
	int		windowStart;
	int		prevFrameMs;
	int		maxInterval;
	int		histTotal;
	int		hist[JOBPOOL_METER_BUCKETS];
} jobpoolMeter_t;

void JobPool_MeterFrame( jobpoolMeter_t *m, JobPoolPrint print, int nowMs,
						 int frontMsec, int backMsec, int handoffMsec, int smpActive );

#ifdef __cplusplus
}
#endif

#endif 
