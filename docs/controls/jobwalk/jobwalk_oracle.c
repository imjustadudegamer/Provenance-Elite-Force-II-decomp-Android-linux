/*
 * jobwalk_oracle.c -- the walk's pack, job-ized: the offline proof.
 *
 * RB_VBO_DrawTessArrays reserves its ring slice and then packs every vertex.
 * That loop is now a JobPool_ParallelRange over the shipping
 * renderervulkan/vk_shade.c body (gen_pack.py lifts the four texts verbatim:
 * VK_Stream_Pack, VK_Stream_PackRange, vkPackCtx_t, vkStreamVertex_t).  This
 * oracle drives the SHIPPING body through the SHIPPING pool (q_jobs.c, linked
 * as the engine links it) and asserts what the parallel pack has to be for the
 * pixels to be unchanged:
 *
 *   1. the pool's ranges cover every vertex exactly once and never overlap, so
 *      no vertex is packed twice and none is skipped -- whatever the machine's
 *      worker count, the count being packed, and the shipping chunk;
 *   2. the parallel bytes are the sequential bytes, byte for byte, over many
 *      shapes (round chunk multiples, chunk+-1, small, empty, large), with the
 *      source arrays distinguishable per index so a misplaced write shows;
 *   3. the split actually runs in parallel for a surface the walk sees (the
 *      point of the change), reported with the machine's worker count;
 *   4. the parallel build scales: a CPU-bound body measured against the same
 *      body run serially is faster, best of three.
 *
 * Plus planted faults (argv[1]), so no arm can pass vacuously:
 *
 *   1 overlapping range        2 a range that skips its first element
 *   3 every element lands in slot 0   4 a parallel pack with the wrong stride
 *   5 the parallel pack one element short   6 the pool disabled (no split)
 *
 * Build/run: docs/controls/jobwalk/run.sh  (never launches the game)
 */

#include "q_jobs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* The engine's tess store.  The pack reads input->xyz[v] and nothing else of
   it -- gen_pack.py fails the control if that stops being true -- so the
   oracle stands the real struct in with exactly that member. */
typedef struct {
	float	( *xyz )[4];
} shaderCommands_t;

#include "jobwalk_pack_gen.c"

static int	g_mutant;

static int	g_checks;
static int	g_failures;

static void check( int ok, const char *what )
{
	g_checks++;
	if ( !ok ) {
		g_failures++;
		printf( "  FAIL  %s\n", what );
	}
}

/* ---- 1. the ranges the shipping pool hands out --------------------------- */

typedef struct {
	int	*hits;
	int	 n;
	int	 outOfRange;
} hitCtx_t;

static void HitBody( void *vctx, int begin, int end )
{
	hitCtx_t	*c = ( hitCtx_t * )vctx;
	int			 i;

	if ( g_mutant == 1 ) {
		end++;					/* planted fault: overlap */
	}
	if ( g_mutant == 2 ) {
		begin++;				/* planted fault: skip */
	}
	if ( g_mutant == 5 && end > c->n ) {
		end = c->n;				/* planted fault: one short */
	}

	for ( i = begin; i < end; i++ ) {
		if ( i < 0 || i >= c->n ) {
			c->outOfRange++;
			continue;
		}
		if ( __atomic_add_fetch( &c->hits[i], 1, __ATOMIC_RELAXED ) != 1 ) {
			c->outOfRange++;	/* written twice */
		}
	}
}

static int		g_ranges;
static int		g_rangeMax;

static void CountBody( void *vctx, int begin, int end )
{
	( void )vctx; ( void )begin; ( void )end;
	__atomic_add_fetch( &g_ranges, 1, __ATOMIC_RELAXED );
}

static void CoverageShape( int n, int minChunk )
{
	hitCtx_t	ctx;
	int			i;
	int			misses = 0;

	ctx.hits = ( int * )calloc( (size_t)( n > 0 ? n : 1 ), sizeof( int ) );
	ctx.n = n;
	ctx.outOfRange = 0;

	JobPool_ParallelRange( ( g_mutant == 5 && n > 0 ) ? n - 1 : n, minChunk,
						   HitBody, &ctx );

	for ( i = 0; i < n; i++ ) {
		if ( ctx.hits[i] != 1 ) {
			misses++;
		}
	}
	check( misses == 0 && ctx.outOfRange == 0,
		   "ranges cover every vertex exactly once" );
	if ( misses || ctx.outOfRange ) {
		printf( "        n=%d minChunk=%d unwritten=%d outOfRange=%d\n",
				n, minChunk, misses, ctx.outOfRange );
	}
	free( ctx.hits );
}

/* ---- 2. the parallel bytes are the sequential bytes ---------------------- */

typedef struct {
	float			( *xyz )[4];
	unsigned char	*colors;
	float			*normals;
	float			*tc0;
	float			*tc1;
	int				 tcStride;
} shape_t;

static void ShapeFill( shape_t *s, int n )
{
	int	i;

	s->xyz = ( float ( * )[4] )malloc( (size_t)( n > 0 ? n : 1 ) * sizeof( float ) * 4 );
	s->colors = ( unsigned char * )malloc( (size_t)( n > 0 ? n : 1 ) * 4 );
	s->normals = ( float * )malloc( (size_t)( n > 0 ? n : 1 ) * 4 * sizeof( float ) );
	s->tcStride = 4;
	s->tc0 = ( float * )malloc( (size_t)( n > 0 ? n : 1 ) * 4 * sizeof( float ) );
	s->tc1 = ( float * )malloc( (size_t)( n > 0 ? n : 1 ) * 4 * sizeof( float ) );

	for ( i = 0; i < n; i++ ) {
		int	k;

		for ( k = 0; k < 4; k++ ) {
			s->xyz[i][k] = ( float )( i * 7 + k );
			s->normals[i * 4 + k] = ( float )( i * 3 - k );
			s->tc0[i * 4 + k] = ( float )( i * 11 + k );
			s->tc1[i * 4 + k] = ( float )( i * 13 - k );
			s->colors[i * 4 + k] = ( unsigned char )( i * 5 + k );
		}
	}
}

static void ShapeFree( shape_t *s )
{
	free( s->xyz ); free( s->colors ); free( s->normals );
	free( s->tc0 ); free( s->tc1 );
}

/* the planted-fault adapters: same pool, same shipping element body, a call
   pattern the checks must reject */
static void BadSlotBody( void *vctx, int begin, int end )
{
	vkPackCtx_t	*c = ( vkPackCtx_t * )vctx;
	int			 i;

	for ( i = begin; i < end; i++ ) {
		VK_Stream_Pack( &c->verts[0], c->input, i, c->tc0, c->tc1,
						c->tcStride, c->colors, c->normals );
	}
}

static void BadStrideBody( void *vctx, int begin, int end )
{
	vkPackCtx_t	*c = ( vkPackCtx_t * )vctx;
	int			 i;

	for ( i = begin; i < end; i++ ) {
		VK_Stream_Pack( &c->verts[i], c->input, i, c->tc0, c->tc1,
						( c->tcStride == 4 ) ? 2 : c->tcStride,
						c->colors, c->normals );
	}
}

static void ByteShape( int n )
{
	shape_t				s;
	shaderCommands_t	input;
	vkPackCtx_t			ctx;
	vkStreamVertex_t	*seq;
	vkStreamVertex_t	*par;
	void				( *fn )( void *, int, int ) = VK_Stream_PackRange;
	int					 count = n;
	int					 i;

	ShapeFill( &s, n );
	memset( &input, 0, sizeof( input ) );
	input.xyz = s.xyz;

	seq = ( vkStreamVertex_t * )calloc( (size_t)( n > 0 ? n : 1 ), sizeof( vkStreamVertex_t ) );
	par = ( vkStreamVertex_t * )calloc( (size_t)( n > 0 ? n : 1 ), sizeof( vkStreamVertex_t ) );
	memset( seq, 0xA5, (size_t)n * sizeof( vkStreamVertex_t ) );
	memset( par, 0x5A, (size_t)n * sizeof( vkStreamVertex_t ) );

	ctx.verts = par;
	ctx.input = &input;
	ctx.tc0 = s.tc0;
	ctx.tc1 = s.tc1;
	ctx.tcStride = s.tcStride;
	ctx.colors = s.colors;
	ctx.normals = s.normals;

	if ( g_mutant == 3 ) {
		fn = BadSlotBody;		/* planted fault */
	} else if ( g_mutant == 4 ) {
		fn = BadStrideBody;		/* planted fault */
	}
	if ( g_mutant == 5 ) {
		count = ( n > 0 ) ? n - 1 : 0;	/* planted fault */
	}

	for ( i = 0; i < n; i++ ) {
		VK_Stream_Pack( &seq[i], &input, i, s.tc0, s.tc1, s.tcStride,
						s.colors, s.normals );
	}
	JobPool_ParallelRange( count, JOBWALK_PACK_CHUNK, fn, &ctx );

	check( n == 0 || memcmp( seq, par, (size_t)n * sizeof( vkStreamVertex_t ) ) == 0,
		   "the parallel pack is the sequential pack, byte for byte" );
	if ( n > 0 && memcmp( seq, par, (size_t)n * sizeof( vkStreamVertex_t ) ) != 0 ) {
		int	first = -1;

		for ( i = 0; i < n; i++ ) {
			if ( memcmp( &seq[i], &par[i], sizeof( vkStreamVertex_t ) ) != 0 ) {
				first = i;
				break;
			}
		}
		printf( "        n=%d first differing vertex=%d chunk=%d\n",
				n, first, JOBWALK_PACK_CHUNK );
	}

	free( seq ); free( par );
	ShapeFree( &s );
}

/* ---- 4. the parallel build scales --------------------------------------- */

static double NowMs( void )
{
	struct timespec	ts;

	clock_gettime( CLOCK_MONOTONIC, &ts );
	return ( double )ts.tv_sec * 1000.0 + ( double )ts.tv_nsec / 1e6;
}

#define SCALE_N		240000
#define SCALE_WORK	220

static volatile float	g_scratch[SCALE_N];
static volatile float	g_sink;

static void WorkRange( void *vctx, int begin, int end )
{
	int		i, w;

	( void )vctx;
	for ( i = begin; i < end; i++ ) {
		float	v = ( float )i * 0.5f;

		for ( w = 0; w < SCALE_WORK; w++ ) {
			v = v * 1.0009765625f + 0.25f;
		}
		g_scratch[i] = v;
		g_sink += v;
	}
}

static void SerialWork( void )
{
	int	i, w;

	for ( i = 0; i < SCALE_N; i++ ) {
		float	v = ( float )i * 0.5f;

		for ( w = 0; w < SCALE_WORK; w++ ) {
			v = v * 1.0009765625f + 0.25f;
		}
		g_scratch[i] = v;
		g_sink += v;
	}
}

int main( int argc, char **argv )
{
	static const int	shapes[] = { 0, 1, 2, 255, 256, 257, 511, 512, 513,
									 1000, 1024, 4096, 20000 };
	static const int	chunks[] = { 1, 7, 64, 256, 1000 };
	int					 i;
	double				 serialBest = 1e30, parallelBest = 1e30;
	jobpoolStats_t		 stats;

	g_mutant = ( argc > 1 ) ? atoi( argv[1] ) : 0;

	JobPool_Init( "jobwalk", 4 );
	if ( g_mutant == 6 ) {
		JobPool_Shutdown();		/* planted fault: no split at all */
	}

	printf( "jobwalk: chunk=%d workers=%d enabled=%d\n",
			JOBWALK_PACK_CHUNK, JobPool_NumWorkers(),
			( int )JobPool_Enabled() );

	/* the pool must be live: everything below is vacuous without it */
	check( JobPool_Enabled() && JobPool_NumWorkers() >= 2,
		   "the pool is live with at least two workers" );
	check( JOBWALK_PACK_CHUNK >= 1, "the shipping chunk is a positive count" );

	/* 1. ranges */
	for ( i = 0; i < ( int )( sizeof( chunks ) / sizeof( chunks[0] ) ); i++ ) {
		CoverageShape( 0, chunks[i] );
		CoverageShape( 1, chunks[i] );
		CoverageShape( JOBWALK_PACK_CHUNK - 1, chunks[i] );
		CoverageShape( JOBWALK_PACK_CHUNK, chunks[i] );
		CoverageShape( JOBWALK_PACK_CHUNK + 1, chunks[i] );
		CoverageShape( 3 * JOBWALK_PACK_CHUNK + 7, chunks[i] );
		CoverageShape( 20000, chunks[i] );
	}

	/* the shipping chunk must actually split a surface the walk sees */
	g_ranges = 0;
	g_rangeMax = JobPool_NumWorkers() * 2;
	JobPool_ParallelRange( 20000, JOBWALK_PACK_CHUNK, CountBody, NULL );
	check( g_ranges >= 2 && g_ranges <= g_rangeMax,
		   "a 20000-vertex surface is split into parallel ranges" );
	printf( "        ranges(20000, chunk=%d) = %d (max %d)\n",
			JOBWALK_PACK_CHUNK, g_ranges, g_rangeMax );

	/* 2. bytes */
	for ( i = 0; i < ( int )( sizeof( shapes ) / sizeof( shapes[0] ) ); i++ ) {
		ByteShape( shapes[i] );
	}

	/* 3. it is really parallel: the pool counts the elements it spread */
	JobPool_ResetStats();
	{
		shape_t				s;
		shaderCommands_t	input;
		vkPackCtx_t			ctx;
		vkStreamVertex_t	*par;

		ShapeFill( &s, 20000 );
		memset( &input, 0, sizeof( input ) );
		input.xyz = s.xyz;
		par = ( vkStreamVertex_t * )calloc( 20000, sizeof( vkStreamVertex_t ) );
		ctx.verts = par; ctx.input = &input;
		ctx.tc0 = s.tc0; ctx.tc1 = s.tc1; ctx.tcStride = s.tcStride;
		ctx.colors = s.colors; ctx.normals = s.normals;
		JobPool_ParallelRange( 20000, JOBWALK_PACK_CHUNK, VK_Stream_PackRange, &ctx );
		JobPool_GetStats( &stats );
		check( stats.parallelElements == 20000,
			   "the pack is handed to the workers, not run inline" );
		printf( "        stats: batches=%d parallelElements=%lld inlineElements=%lld\n",
				stats.batches, stats.parallelElements, stats.inlineElements );
		free( par );
		ShapeFree( &s );
	}

	/* 4. scaling: best of three, so a busy moment cannot decide it */
	for ( i = 0; i < 3; i++ ) {
		double	t0 = NowMs();

		SerialWork();
		{
			double	dt = NowMs() - t0;

			if ( dt < serialBest ) {
				serialBest = dt;
			}
		}
		t0 = NowMs();
		JobPool_ParallelRange( SCALE_N, JOBWALK_PACK_CHUNK, WorkRange, NULL );
		{
			double	dt = NowMs() - t0;

			if ( dt < parallelBest ) {
				parallelBest = dt;
			}
		}
	}
	printf( "        scale: serial=%.1fms parallel=%.1fms speedup=%.2fx sink=%.0f\n",
			serialBest, parallelBest,
			( parallelBest > 0.0 ) ? serialBest / parallelBest : 0.0,
			( double )g_sink );
	check( parallelBest < serialBest * 0.85,
		   "the parallel build is measurably faster than the serial one" );

	JobPool_Shutdown();

	printf( "jobwalk: %s (%d checks, %d failed, mutant=%d)\n",
			g_failures ? "FAIL" : "PASS", g_checks, g_failures, g_mutant );
	return g_failures ? 1 : 0;
}
