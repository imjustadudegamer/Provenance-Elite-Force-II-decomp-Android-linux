
#include "cg_local.h"
#include "r204_cgprobe.h"

#include <time.h>


#define R204_WINDOW_MS		1000
#define R204_MAX_FRAMES		3600
#define R204_MAX_ROWS		300
#define R204_MAX_ZONES		12
#define R204_MAX_COUNTERS	3

static const char *r204_cgZoneName[R204_MAX_ZONES] = {
	"clear", "predict", "calcview", "ents", "audio", "fx", "viewmode", "draw",
	"other", ""
};

static const char *r204_cgCounterName[R204_MAX_COUNTERS] = {
	"snapents", "", ""
};

static const int r204_cgZoneCount = R204_CG_ZONES;
static const int r204_cgCounterCount = R204_CG_COUNTERS;

typedef char r204_assert_zones[(R204_CG_ZONES <= R204_MAX_ZONES) ? 1 : -1];
typedef char r204_assert_counters[(R204_CG_COUNTERS <= R204_MAX_COUNTERS) ? 1 : -1];

int r204_cgOn = 0;

static int	r204_frames;
static int	r204_windowStart;
static int	r204_windowArmed;
static int	r204_rowsEmitted;
static int	r204_zoneSum[R204_MAX_ZONES];
static int	r204_zoneMax[R204_MAX_ZONES];
static int	r204_cntSum[R204_MAX_COUNTERS];
static int	r204_cntMax[R204_MAX_COUNTERS];
static int	r204_totalSum;
static int	r204_totalMax;
static int	r204_frameZone[R204_MAX_ZONES];
static int	r204_frameNamed;
static int	r204_frameCnt[R204_MAX_COUNTERS];

int R204_CgRoot( void )
{
	return r204_cgOn ? R204_STAMP( R204_CgNow() ) : 0;
}

void R204_CgZone( int zone, int usec )
{
	int tenths;

	if ( !r204_cgOn || zone < 0 || zone >= R204_CG_ZONES ) {
		return;
	}

	if ( usec < 0 ) {
		usec = 0;
	}

	tenths = usec / 100;

	r204_frameZone[zone] += tenths;
	r204_frameNamed += tenths;
}

void R204_CgCount( int counter, int n )
{
	if ( !r204_cgOn || counter < 0 || counter >= R204_CG_COUNTERS ) {
		return;
	}

	r204_frameCnt[counter] += n;
}

static void r204_app( char *dst, int size, const char *src )
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

static void r204_resetWindow( void )
{
	int i;

	r204_frames = 0;
	r204_windowStart = 0;
	r204_windowArmed = 0;
	r204_totalSum = r204_totalMax = 0;
	r204_frameNamed = 0;

	for ( i = 0; i < R204_MAX_ZONES; i++ ) {
		r204_zoneSum[i] = r204_zoneMax[i] = 0;
		r204_frameZone[i] = 0;
	}
	for ( i = 0; i < R204_MAX_COUNTERS; i++ ) {
		r204_cntSum[i] = r204_cntMax[i] = 0;
		r204_frameCnt[i] = 0;
	}
}

static void r204_printRow( int window )
{
	char	line[1024], part[256];
	int	frames = r204_frames < 1 ? 1 : r204_frames;
	int	i, mean10;

	line[0] = '\0';

	for ( i = 0; i < r204_cgZoneCount; i++ ) {
		int sum = r204_zoneSum[i];

		mean10 = (int)( ( (long long)sum + frames / 2 ) / frames );
		Com_sprintf( part, sizeof(part), " %s=%d.%d/%d.%d", r204_cgZoneName[i],
					 mean10 / 10, mean10 % 10,
					 r204_zoneMax[i] / 10, r204_zoneMax[i] % 10 );
		r204_app( line, (int)sizeof(line), part );
	}

	for ( i = 0; i < r204_cgCounterCount; i++ ) {
		int sum = r204_cntSum[i];

		mean10 = (int)( ( (long long)sum * 10 + frames / 2 ) / frames );
		Com_sprintf( part, sizeof(part), " %s=%d.%d/%d", r204_cgCounterName[i],
					 mean10 / 10, mean10 % 10, r204_cntMax[i] );
		r204_app( line, (int)sizeof(line), part );
	}

	mean10 = (int)( ( (long long)r204_totalSum + frames / 2 ) / frames );
	Com_sprintf( part, sizeof(part), " total=%d.%d/%d.%d", mean10 / 10, mean10 % 10,
				 r204_totalMax / 10, r204_totalMax % 10 );
	r204_app( line, (int)sizeof(line), part );

	Com_Printf( "[R202:cg] frames=%d window=%dms%s\n", frames, window, line );

	r204_rowsEmitted++;
}

void R204_CgFrameEnd( int frameStamp )
{
	int i, window, total;

	if ( !r204_cgOn || !frameStamp ) {
		r204_windowStart = 0;
		r204_windowArmed = 0;
		r204_frames = 0;
		return;
	}

	total = ( R204_CgNow() - R204_UNSTAMP( frameStamp ) ) / 100;
	if ( total < 0 ) {
		total = 0;
	}

	r204_totalSum += total;
	if ( total > r204_totalMax ) {
		r204_totalMax = total;
	}

	{
		int other = total - r204_frameNamed;

		if ( other < 0 ) {
			other = 0;
		}
		r204_frameZone[R204_CG_ZONES - 1] += other;
	}

	for ( i = 0; i < R204_CG_ZONES; i++ ) {
		int v = r204_frameZone[i];

		r204_zoneSum[i] += v;
		if ( v > r204_zoneMax[i] ) {
			r204_zoneMax[i] = v;
		}
		r204_frameZone[i] = 0;
	}

	for ( i = 0; i < R204_CG_COUNTERS; i++ ) {
		int v = r204_frameCnt[i];

		r204_cntSum[i] += v;
		if ( v > r204_cntMax[i] ) {
			r204_cntMax[i] = v;
		}
		r204_frameCnt[i] = 0;
	}

	r204_frameNamed = 0;
	r204_frames++;

	if ( !r204_windowArmed ) {
		r204_windowStart = R204_SysMilliseconds();
		r204_windowArmed = 1;
	}

	window = R204_SysMilliseconds() - r204_windowStart;

	if ( window < R204_WINDOW_MS && r204_frames < R204_MAX_FRAMES ) {
		return;
	}

	if ( r204_rowsEmitted < R204_MAX_ROWS ) {
		r204_printRow( window );
	} else if ( r204_rowsEmitted == R204_MAX_ROWS ) {
		r204_rowsEmitted++;
		Com_Printf( "[R202:cg] capped at %d rows; no further lines\n", R204_MAX_ROWS );
	}

	r204_resetWindow();
	r204_windowStart = R204_SysMilliseconds();
	r204_windowArmed = 1;
}


int R204_CgNow( void )
{
	struct timespec ts;

	clock_gettime( CLOCK_MONOTONIC, &ts );
	return (int)( ( ts.tv_sec % 1000000 ) * 1000000 + ts.tv_nsec / 1000 );
}

int R204_SysMilliseconds( void )
{
	return cgi.Milliseconds();
}

int R204_CgFrameOpen( void )
{
	static cvar_t *stats;

	if ( !stats ) {
		stats = cgi.Cvar_Get( "r202_stats", "1", 0 );
	}

	r204_cgOn = ( stats && stats->integer ) ? 1 : 0;

	return R204_CgRoot();
}
