/* offline micro-benchmark: the per-stage census work the two r38 commits gate
   off in a quiet boot, at the r35 draw census (372 stage draws, 214 surfaces). */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define MAX_LIGHTMAPS 256
#define ITER 372

static const void *lightmaps[MAX_LIGHTMAPS], *baseLightmaps[MAX_LIGHTMAPS];
static const void *filler[24];
static const char *nm_a = "textures/goth/trim3";
static const char *nm_b = "sysimg/2d/mouse_arrow";

static double now_ms(void)
{
	struct timespec ts;
	clock_gettime( CLOCK_MONOTONIC, &ts );
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static int known( const void *image )	/* VK_StageBindKnown's own loop */
{
	int i;
	if ( !image ) return 0;
	for ( i = 0; i < MAX_LIGHTMAPS; i++ ) {
		if ( image == lightmaps[i] || image == baseLightmaps[i] ) return 1;
	}
	return 0;
}

static int owned( const void *cand, const void *image )	/* the strcmp arm */
{
	return cand && image && cand != image && !strcmp( nm_a, nm_b );
}

int main( void )
{
	int i, k, sink = 0;
	double t0, t1;

	for ( i = 0; i < MAX_LIGHTMAPS; i++ ) { lightmaps[i] = filler; baseLightmaps[i] = filler; }
	for ( i = 0; i < 24; i++ ) filler[i] = (const void *)( (uintptr_t)&sink + i );

	t0 = now_ms();
	for ( k = 0; k < ITER * 2140; k++ ) {	/* 2140 reps => one frame's worth in 372-call units */
		int dis;
		const void *img = filler[k & 23];	/* varies, so no call is loop-invariant */

		dis = ( owned( nm_a, nm_b ) || known( img ) )
		   || ( owned( nm_b, nm_a ) || known( img ) )
		   || known( img );
		sink += dis;
	}
	t1 = now_ms();
	printf( "stagebound scan  %.3f us/frame  (3 x 512 compares + 2 strcmps x 372 draws)\n",
			( t1 - t0 ) * 1000.0 / 2140.0 );

	t0 = now_ms();
	for ( k = 0; k * 136 < ITER * 2140; k++ ) {
		int j, hit = 0, second = 5;
		const void *want = &sink;			/* not in the table: the miss arm */
		for ( j = 0; j < 24; j++ ) {
			if ( filler[j] == want && second == 5 ) { hit = 1; break; }
		}
		sink += hit;
	}
	t1 = now_ms();
	printf( "shadertime scan  %.3f us/frame  (24-entry dedup x 136 surfaces)\n",
			( t1 - t0 ) * 1000.0 / 2140.0 );
	return sink > 100000000 ? 1 : 0;
}
