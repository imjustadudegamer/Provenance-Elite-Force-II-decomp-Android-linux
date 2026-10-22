/*
===========================================================================
Reconstructed from the retail Star Trek: Voyager -- Elite Force II binaries.
The publisher released no source for this file.  This port asserts no copyright
in it and grants no rights to it: it remains the property of its respective
rights holders, and it is covered neither by the GNU GPL nor by the Ritual
Elite Force II SDK agreement.  See LICENSING.md.
===========================================================================
*/
// SPDX-License-Identifier: LicenseRef-EF2-Retail-Reconstruction

#include "cg_local.h"
#include "be_cgprobe.h"

#include <time.h>


#define BE_WINDOW_MS		1000
#define BE_MAX_FRAMES		3600
#define BE_MAX_ROWS		300
#define BE_MAX_ZONES		12
#define BE_MAX_COUNTERS	3

static const char *be_cgZoneName[BE_MAX_ZONES] = {
	"clear", "predict", "calcview", "ents", "audio", "fx", "viewmode", "draw",
	"other", ""
};

static const char *be_cgCounterName[BE_MAX_COUNTERS] = {
	"snapents", "", ""
};

static const int be_cgZoneCount = BE_CG_ZONES;
static const int be_cgCounterCount = BE_CG_COUNTERS;

typedef char be_assert_zones[(BE_CG_ZONES <= BE_MAX_ZONES) ? 1 : -1];
typedef char be_assert_counters[(BE_CG_COUNTERS <= BE_MAX_COUNTERS) ? 1 : -1];

int be_cgOn = 0;

static int	be_frames;
static int	be_windowStart;
static int	be_windowArmed;
static int	be_rowsEmitted;
static int	be_zoneSum[BE_MAX_ZONES];
static int	be_zoneMax[BE_MAX_ZONES];
static int	be_cntSum[BE_MAX_COUNTERS];
static int	be_cntMax[BE_MAX_COUNTERS];
static int	be_totalSum;
static int	be_totalMax;
static int	be_frameZone[BE_MAX_ZONES];
static int	be_frameNamed;
static int	be_frameCnt[BE_MAX_COUNTERS];

int BE_CgRoot( void )
{
	return be_cgOn ? BE_STAMP( BE_CgNow() ) : 0;
}

void BE_CgZone( int zone, int usec )
{
	int tenths;

	if ( !be_cgOn || zone < 0 || zone >= BE_CG_ZONES ) {
		return;
	}

	if ( usec < 0 ) {
		usec = 0;
	}

	tenths = usec / 100;

	be_frameZone[zone] += tenths;
	be_frameNamed += tenths;
}

void BE_CgCount( int counter, int n )
{
	if ( !be_cgOn || counter < 0 || counter >= BE_CG_COUNTERS ) {
		return;
	}

	be_frameCnt[counter] += n;
}

static void be_app( char *dst, int size, const char *src )
{
	int l = 0;
	int i = 0;

	while ( l < size - 1 && dst[l] ) {
		l++;
	}

	while ( l + i < size - 1 && src[i] ) {
		dst[l + i] = src[i];
		i++;
	}

	dst[l + i] = '\0';
}

static void be_resetWindow( void )
{
	int i;

	be_frames = 0;
	be_windowStart = 0;
	be_windowArmed = 0;
	be_totalSum = be_totalMax = 0;
	be_frameNamed = 0;

	for ( i = 0; i < BE_MAX_ZONES; i++ ) {
		be_zoneSum[i] = be_zoneMax[i] = 0;
		be_frameZone[i] = 0;
	}
	for ( i = 0; i < BE_MAX_COUNTERS; i++ ) {
		be_cntSum[i] = be_cntMax[i] = 0;
		be_frameCnt[i] = 0;
	}
}

static void be_printRow( int window )
{
	char	line[1024], part[256];
	int	frames = be_frames < 1 ? 1 : be_frames;
	int	i, mean10;

	line[0] = '\0';

	for ( i = 0; i < be_cgZoneCount; i++ ) {
		int sum = be_zoneSum[i];

		mean10 = (int)( ( (long long)sum + frames / 2 ) / frames );
		Com_sprintf( part, sizeof(part), " %s=%d.%d/%d.%d", be_cgZoneName[i],
					 mean10 / 10, mean10 % 10,
					 be_zoneMax[i] / 10, be_zoneMax[i] % 10 );
		be_app( line, (int)sizeof(line), part );
	}

	for ( i = 0; i < be_cgCounterCount; i++ ) {
		int sum = be_cntSum[i];

		mean10 = (int)( ( (long long)sum * 10 + frames / 2 ) / frames );
		Com_sprintf( part, sizeof(part), " %s=%d.%d/%d", be_cgCounterName[i],
					 mean10 / 10, mean10 % 10, be_cntMax[i] );
		be_app( line, (int)sizeof(line), part );
	}

	mean10 = (int)( ( (long long)be_totalSum + frames / 2 ) / frames );
	Com_sprintf( part, sizeof(part), " total=%d.%d/%d.%d", mean10 / 10, mean10 % 10,
				 be_totalMax / 10, be_totalMax % 10 );
	be_app( line, (int)sizeof(line), part );

	Com_Printf( "[UI:cg] frames=%d window=%dms%s\n", frames, window, line );

	be_rowsEmitted++;
}

void BE_CgFrameEnd( int frameStamp )
{
	int i, window, total;

	if ( !be_cgOn || !frameStamp ) {
		be_windowStart = 0;
		be_windowArmed = 0;
		be_frames = 0;
		return;
	}

	total = ( BE_CgNow() - BE_UNSTAMP( frameStamp ) ) / 100;
	if ( total < 0 ) {
		total = 0;
	}

	be_totalSum += total;
	if ( total > be_totalMax ) {
		be_totalMax = total;
	}

	{
		int other = total - be_frameNamed;

		if ( other < 0 ) {
			other = 0;
		}
		be_frameZone[BE_CG_ZONES - 1] += other;
	}

	for ( i = 0; i < BE_CG_ZONES; i++ ) {
		int v = be_frameZone[i];

		be_zoneSum[i] += v;
		if ( v > be_zoneMax[i] ) {
			be_zoneMax[i] = v;
		}
		be_frameZone[i] = 0;
	}

	for ( i = 0; i < BE_CG_COUNTERS; i++ ) {
		int v = be_frameCnt[i];

		be_cntSum[i] += v;
		if ( v > be_cntMax[i] ) {
			be_cntMax[i] = v;
		}
		be_frameCnt[i] = 0;
	}

	be_frameNamed = 0;
	be_frames++;

	if ( !be_windowArmed ) {
		be_windowStart = BE_SysMilliseconds();
		be_windowArmed = 1;
	}

	window = BE_SysMilliseconds() - be_windowStart;

	if ( window < BE_WINDOW_MS && be_frames < BE_MAX_FRAMES ) {
		return;
	}

	if ( be_rowsEmitted < BE_MAX_ROWS ) {
		be_printRow( window );
	} else if ( be_rowsEmitted == BE_MAX_ROWS ) {
		be_rowsEmitted++;
		Com_Printf( "[UI:cg] capped at %d rows; no further lines\n", BE_MAX_ROWS );
	}

	be_resetWindow();
	be_windowStart = BE_SysMilliseconds();
	be_windowArmed = 1;
}


int BE_CgNow( void )
{
	struct timespec ts;

	clock_gettime( CLOCK_MONOTONIC, &ts );
	return (int)( ( ts.tv_sec % 1000000 ) * 1000000 + ts.tv_nsec / 1000 );
}

int BE_SysMilliseconds( void )
{
	return cgi.Milliseconds();
}

int BE_CgFrameOpen( void )
{
	static cvar_t *stats;
	static cvar_t *master;

	if ( !stats ) {
		stats = cgi.Cvar_Get( "ui_stats", "1", 0 );
	}
	if ( !master ) {
		master = cgi.Cvar_Get( "qa_probe", "0", 0 );
	}

	be_cgOn = ( stats && stats->integer && master && master->integer ) ? 1 : 0;

	return BE_CgRoot();
}
