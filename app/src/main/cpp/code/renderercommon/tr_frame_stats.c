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

/* Frame pacing at the present, for both renderer backends.
 * r_perfFps: 0 silent, N > 0 one row every N seconds.
 * A late frame is 1.5x the com_maxfps budget, or 1.5x the last median when
 * the budget is off. */

#include "tr_common.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define FRAME_STATS_RING		8192
#define FRAME_STATS_WORST		8

typedef struct {
	long long frame;
	long long interval;
	long long over;
} frameStatsHitch_t;

static cvar_t			*s_period;
static cvar_t			*s_maxfps;
static int				s_ring[FRAME_STATS_RING];
static int				s_ringHead;
static int				s_ringCount;
static long long		s_frame;
static long long		s_lastPresent;
static long long		s_windowStart;
static long long		s_frontUsec;
static long long		s_backUsec;
static int				s_stutters;
static long long		s_missed;
static int				s_lastP50;
static frameStatsHitch_t s_worst[FRAME_STATS_WORST];
static int				s_worstCount;
static const char		*s_backend = "?";
static const char		*s_present = "?";
static int				s_vsync;
static int				s_wasOn;
static fileHandle_t		s_log;
static qboolean			s_logOpen;

static long long FrameStats_Now( void )
{
	struct timespec ts;

	if ( clock_gettime( CLOCK_MONOTONIC, &ts ) == 0 ) {
		return (long long)ts.tv_sec * 1000000LL + (long long)ts.tv_nsec / 1000LL;
	}

	return (long long)ri.Milliseconds() * 1000LL;
}

static int FrameStats_Compare( const void *a, const void *b )
{
	const int x = *(const int *)a;
	const int y = *(const int *)b;

	return ( x > y ) - ( x < y );
}

static void FrameStats_LogOpen( void )
{
	static const char header[] =
		"# backend t_s frame fps frames avg_ms p50 p95 p99 max jitter stutters missed target fe_ms be_ms present vsync\n";

	if ( s_logOpen ) {
		return;
	}
	s_logOpen = qtrue;
	if ( !ri.FS_OpenFileWrite || !ri.FS_Write ) {
		return;
	}
	s_log = ri.FS_OpenFileWrite( "perf_fps.log" );
	if ( s_log ) {
		ri.FS_Write( header, sizeof( header ) - 1, s_log );
	}
}

static void FrameStats_LogLine( const char *line, int len )
{
	if ( s_log && len > 0 ) {
		ri.FS_Write( line, (size_t)len, s_log );
	}
}

void R_FrameStats_Reset( void )
{
	s_ringHead = 0;
	s_ringCount = 0;
	s_windowStart = 0;
	s_frontUsec = 0;
	s_backUsec = 0;
	s_stutters = 0;
	s_missed = 0;
	s_lastP50 = 0;
	s_worstCount = 0;
	memset( s_worst, 0, sizeof( s_worst ) );
	s_lastPresent = 0;
}

static void FrameStats_RecordHitch( long long interval, long long threshold )
{
	int					i;
	int					slot;

	if ( s_worstCount < FRAME_STATS_WORST ) {
		slot = s_worstCount++;
	} else {
		slot = 0;
		for ( i = 1; i < FRAME_STATS_WORST; i++ ) {
			if ( s_worst[i].interval < s_worst[slot].interval ) {
				slot = i;
			}
		}
		if ( interval <= s_worst[slot].interval ) {
			return;
		}
	}

	s_worst[slot].frame = s_frame;
	s_worst[slot].interval = interval;
	s_worst[slot].over = interval - threshold;
}

static void FrameStats_Row( long long now )
{
	int			sorted[FRAME_STATS_RING];
	long long	elapsed;
	long long	sum = 0;
	long long	front;
	long long	back;
	int			i;
	int			p50, p95, p99, worst;
	double		avg;
	double		jitter = 0.0;
	double		fps;
	int			target = s_maxfps ? s_maxfps->integer : 0;

	if ( s_ringCount == 0 ) {
		s_windowStart = now;
		return;
	}

	for ( i = 0; i < s_ringCount; i++ ) {
		sorted[i] = s_ring[( s_ringHead - s_ringCount + i + FRAME_STATS_RING ) % FRAME_STATS_RING];
	}
	qsort( sorted, s_ringCount, sizeof( int ), FrameStats_Compare );

	for ( i = 0; i < s_ringCount; i++ ) {
		sum += sorted[i];
	}
	avg = (double)sum / (double)s_ringCount;
	for ( i = 0; i < s_ringCount; i++ ) {
		double d = (double)sorted[i] - avg;

		jitter += d * d;
	}
	jitter = sqrt( jitter / (double)s_ringCount );

	p50 = sorted[s_ringCount / 2];
	p95 = sorted[( s_ringCount * 95 ) / 100];
	p99 = sorted[( s_ringCount * 99 ) / 100];
	worst = sorted[s_ringCount - 1];
	s_lastP50 = p50;

	front = s_frontUsec / s_ringCount;
	back = s_backUsec / s_ringCount;
	elapsed = sum > 1000 ? sum : 1000;
	fps = ( s_ringCount * 1000000.0 ) / (double)elapsed;

	FrameStats_LogOpen();

	{
		char		logLine[512];
		int			logLen;

		logLen = snprintf( logLine, sizeof( logLine ),
						   "%s %.1f %lld %.1f %d %.2f %.2f %.2f %.2f %.2f %.2f %d %lld %d %.2f %.2f %s %d\n",
						   s_backend, (double)elapsed / 1000000.0, s_frame, fps, s_ringCount,
						   avg / 1000.0, p50 / 1000.0, p95 / 1000.0, p99 / 1000.0,
						   worst / 1000.0, jitter / 1000.0, s_stutters, s_missed, target,
						   (double)front / 1000.0, (double)back / 1000.0,
						   s_present, s_vsync );
		if ( logLen > (int)sizeof( logLine ) - 1 ) {
			logLen = (int)sizeof( logLine ) - 1;
		}
		FrameStats_LogLine( logLine, logLen );
	}

	ri.Printf( PRINT_ALL,
			   "[PERF:fps] backend=%s t=%.1fs fps=%.1f frames=%d avg=%.2fms "
			   "p50=%.2fms p95=%.2fms p99=%.2fms max=%.2fms jitter=%.2fms "
			   "stutters=%d missed=%lld target=%d fe=%.2fms be=%.2fms "
			   "present=%s vsync=%d\n",
			   s_backend, (double)elapsed / 1000000.0, fps, s_ringCount,
			   avg / 1000.0, p50 / 1000.0, p95 / 1000.0, p99 / 1000.0,
			   worst / 1000.0, jitter / 1000.0,
			   s_stutters, s_missed, target,
			   (double)front / 1000.0, (double)back / 1000.0,
			   s_present, s_vsync );

	for ( i = 0; i < s_worstCount; i++ ) {
		{
			char	logLine[160];
			int		logLen = snprintf( logLine, sizeof( logLine ),
									   "hitch %s %lld %.2f %.2f %d\n",
									   s_backend, s_worst[i].frame,
									   s_worst[i].interval / 1000.0,
									   s_worst[i].over / 1000.0, target );
			if ( logLen > (int)sizeof( logLine ) - 1 ) {
				logLen = (int)sizeof( logLine ) - 1;
			}
			FrameStats_LogLine( logLine, logLen );
		}
		ri.Printf( PRINT_ALL,
				   "[PERF:stutter] backend=%s frame=%lld interval=%.2fms over=%.2fms target=%d\n",
				   s_backend, s_worst[i].frame, s_worst[i].interval / 1000.0,
				   s_worst[i].over / 1000.0, target );
	}

	/* one row is one window: percentiles must not accumulate across the session */
	s_ringHead = 0;
	s_ringCount = 0;

	s_windowStart = now;
	s_stutters = 0;
	s_missed = 0;
	s_worstCount = 0;
	memset( s_worst, 0, sizeof( s_worst ) );
	s_frontUsec = 0;
	s_backUsec = 0;
}

void R_FrameStats_Present( const char *backend, const char *present,
						   int vsync, int frontEndMsec, int backEndMsec )
{
	long long	now;
	long long	interval;
	long long	targetUsec;
	long long	threshold;
	int			period;

	s_backend = ( backend && *backend ) ? backend : "?";
	s_present = ( present && *present ) ? present : "?";
	s_vsync = vsync ? 1 : 0;

	if ( !s_period ) {
		s_period = ri.Cvar_Get( "r_perfFps", "0", 0 );
	}
	if ( !s_maxfps ) {
		s_maxfps = ri.Cvar_Get( "com_maxfps", "60", 0 );
	}
	period = ( s_period && s_period->integer > 0 ) ? s_period->integer : 0;

	if ( !period ) {
		/* disarmed: no clock read at all, just drop a window left behind */
		if ( s_wasOn ) {
			R_FrameStats_Reset();
			s_wasOn = 0;
		}
		return;
	}
	if ( !s_wasOn ) {
		s_wasOn = 1;
		s_lastPresent = 0;
	}

	now = FrameStats_Now();
	if ( !s_lastPresent ) {
		s_lastPresent = now;
		s_windowStart = now;
		return;
	}

	interval = now - s_lastPresent;
	s_lastPresent = now;
	if ( interval < 0 ) {
		interval = 0;
	}
	if ( interval > 1000000 ) {
		interval = 1000000;
	}

	s_frame++;

	targetUsec = ( s_maxfps && s_maxfps->integer > 0 )
				 ? 1000000LL / s_maxfps->integer
				 : 0;
	if ( targetUsec > 0 ) {
		threshold = ( targetUsec * 3 ) / 2;
		if ( interval >= targetUsec ) {
			long long periods = ( interval + targetUsec / 2 ) / targetUsec;

			if ( periods > 1 ) {
				s_missed += periods - 1;
			}
		}
	} else {
		threshold = s_lastP50 > 0 ? ( s_lastP50 * 3 ) / 2 : 20000;
	}

	s_ring[s_ringHead] = (int)( interval > 0x7fffffff ? 0x7fffffff : interval );
	s_ringHead = ( s_ringHead + 1 ) % FRAME_STATS_RING;
	if ( s_ringCount < FRAME_STATS_RING ) {
		s_ringCount++;
	}

	s_frontUsec += frontEndMsec > 0 ? (long long)frontEndMsec * 1000 : 0;
	s_backUsec += backEndMsec > 0 ? (long long)backEndMsec * 1000 : 0;

	if ( interval >= threshold ) {
		s_stutters++;
		FrameStats_RecordHitch( interval, threshold );
	}

	if ( now - s_windowStart >= (long long)period * 1000000LL ) {
		FrameStats_Row( now );
	}
}
