/*
 * threadroot_oracle.c -- the walk's job dispatch, measured: the offline proof.
 *
 * The device profile puts ~32% of every cycle in lock paths and 40k context
 * switches a second, under RE_RenderScene -> R_RenderView -> JobPool_RunAndWait
 * -> SDL_LockMutex.  This rig drives the SHIPPING pool (q_jobs.c, linked as the
 * engine links it) through the same shape the walk uses -- one
 * JobPool_ParallelRange per surface, surface sizes spanning the walk's vertex
 * counts, the shipping chunk -- and reports wall time, context switches and
 * system time.  The same shapes run through the pool this change replaced
 * (threadroot_oldpool.h, verbatim from git HEAD with the symbols prefixed) are
 * the negative control: they must reproduce the thrash the rig is there to
 * remove.
 *
 *   1. the shipping pool and the control agree byte for byte on every shape,
 *      which is the output-identity claim for the change;
 *   2. an empty body isolates the dispatch cost itself;
 *   3. a frame walk (S surfaces x F frames) reports ctx/s, sys/s and wall for
 *      both pools at each candidate chunk;
 *   4. the chunk at which dispatching stops paying is read off 3;
 *   5. the texcoord grain: the walk's own bodies (the environment texcoord and
 *      the diffuse colour, verbatim in shape from tr_shade_calc.c) at ONE
 *      surface size per run, inline against each candidate chunk, so the
 *      break-even is read as "a surface of N vertices" and compared against
 *      the verts/surf column [PERF:walk] already prints.
 *
 * Build/run: docs/controls/threadroot/run.sh  (never launches the game)
 */

#include "q_jobs.h"
#include "q_jobs_workers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>

/* ---- the control: the pool this change replaced, verbatim ---------------- */

#include "threadroot_oldpool.h"

/* ---- measurement --------------------------------------------------------- */

typedef struct {
	double	wall;
	long	nvcsw;
	long	nivcsw;
	long	sysUsec;
	long	userUsec;
} sample_t;

static double NowSec( void )
{
	struct timespec	ts;

	clock_gettime( CLOCK_MONOTONIC, &ts );
	return ( double )ts.tv_sec + ( double )ts.tv_nsec / 1e9;
}

static void Sample( sample_t *s )
{
	struct rusage	ru;

	getrusage( RUSAGE_SELF, &ru );
	s->wall = NowSec();
	s->nvcsw = ( long )ru.ru_nvcsw;
	s->nivcsw = ( long )ru.ru_nivcsw;
	s->sysUsec = ( long )ru.ru_stime.tv_usec + ( long )ru.ru_stime.tv_sec * 1000000L;
	s->userUsec = ( long )ru.ru_utime.tv_usec + ( long )ru.ru_utime.tv_sec * 1000000L;
}

static void Delta( const sample_t *a, const sample_t *b, sample_t *out )
{
	*out = *b;
	out->wall = b->wall - a->wall;
	out->nvcsw = b->nvcsw - a->nvcsw;
	out->nivcsw = b->nivcsw - a->nivcsw;
	out->sysUsec = b->sysUsec - a->sysUsec;
	out->userUsec = b->userUsec - a->userUsec;
}

static long CtxOf( const sample_t *s )
{
	return s->nvcsw + s->nivcsw;
}

/* ---- the walk's work ----------------------------------------------------- */

/* One surface's per-vertex body, shaped like the walk's pack: read a position,
   write a stride-sized slot.  The rig reports ns/vertex so the number can be
   compared against the device's own. */
typedef struct {
	const float	*xyz;
	float		*out;
	float		 sink;
} walkCtx_t;

static void WalkBody( void *vctx, int begin, int end )
{
	walkCtx_t	*c = ( walkCtx_t * )vctx;
	int			 i;

	for ( i = begin; i < end; i++ ) {
		float	x = c->xyz[ i * 4 + 0 ];
		float	y = c->xyz[ i * 4 + 1 ];
		float	z = c->xyz[ i * 4 + 2 ];
		float	w = c->xyz[ i * 4 + 3 ];
		float	d = x * 0.5f + y * 0.25f + z * 0.125f + w * 0.0625f;

		c->out[ i * 4 + 0 ] = x * 2.0f - d;
		c->out[ i * 4 + 1 ] = y * 2.0f - d;
		c->out[ i * 4 + 2 ] = z * 2.0f - d;
		c->out[ i * 4 + 3 ] = 1.0f;
		c->sink += d;
	}
}

/* The same shape with roughly four times the per-vertex work, so the rig can
   say where dispatching starts to pay instead of only where it does not. */
static void HeavyBody( void *vctx, int begin, int end )
{
	walkCtx_t	*c = ( walkCtx_t * )vctx;
	int			 i;

	for ( i = begin; i < end; i++ ) {
		float	x = c->xyz[ i * 4 + 0 ];
		float	y = c->xyz[ i * 4 + 1 ];
		float	z = c->xyz[ i * 4 + 2 ];
		float	w = c->xyz[ i * 4 + 3 ];
		float	d = x * 0.5f + y * 0.25f + z * 0.125f + w * 0.0625f;
		float	e = d * d + x * y - z * w;
		float	f = e * 0.5f + d * 0.25f;
		int			k;

		for ( k = 0; k < 3; k++ ) {
			f = f * 1.0009765625f + 0.25f;
		}
		c->out[ i * 4 + 0 ] = x * 2.0f - f;
		c->out[ i * 4 + 1 ] = y * 2.0f - e;
		c->out[ i * 4 + 2 ] = z * 2.0f - d;
		c->out[ i * 4 + 3 ] = 1.0f;
		c->sink += f;
	}
}

static void EmptyBody( void *vctx, int begin, int end )
{
	( void )vctx; ( void )begin; ( void )end;
}

/* ---- 5. the walk's own texcoord bodies ----------------------------------- */

/* tr_shade_calc.c RB_CalcEnvironmentTexCoordsRange, shape verbatim: a view
   vector per vertex, the fast reciprocal square root the engine carries, the
   reflect, two stores.  Q_rsqrt and the vector macros are inlined here so the
   rig keeps its one-include build. */
static float RigQrsqrt( float number )
{
	union { float f; int i; } t;
	float	x2, y;

	x2 = number * 0.5f;
	t.f = number;
	t.i = 0x5f3759df - ( t.i >> 1 );
	y = t.f;
	y = y * ( 1.5f - ( x2 * y * y ) );

	return y;
}

typedef struct {
	float		*st;
	const float	*xyz;
	const float	*normal;
	int			numVertexes;
	float		viewOrigin[3];
	float		sink;
} rigEnvTex_t;

static void EnvTexBody( void *vctx, int begin, int end )
{
	rigEnvTex_t	*c = ( rigEnvTex_t * )vctx;
	int			i;
	const float	*v = c->xyz + 4 * begin;
	const float	*normal = c->normal + 4 * begin;
	float		*st = c->st + 2 * begin;
	float		viewer[3], reflected;
	float		d;

	for ( i = begin; i < end; i++, v += 4, normal += 4, st += 2 ) {
		viewer[0] = c->viewOrigin[0] - v[0];
		viewer[1] = c->viewOrigin[1] - v[1];
		viewer[2] = c->viewOrigin[2] - v[2];
		d = RigQrsqrt( viewer[0] * viewer[0] + viewer[1] * viewer[1] + viewer[2] * viewer[2] );
		viewer[0] *= d; viewer[1] *= d; viewer[2] *= d;

		d = normal[0] * viewer[0] + normal[1] * viewer[1] + normal[2] * viewer[2];

		reflected = normal[0] * 2 * d - viewer[0];
		st[0] = 1.0f + normal[1] * 2 * d - viewer[1];
		st[1] = 1.0f - ( normal[2] * 2 * d - viewer[2] );
		c->sink += reflected;
	}
}

/* tr_shade_calc.c RB_CalcDiffuseColorRange, shape verbatim: the dot against the
   light direction, the three clamped colour channels, the four byte stores. */
typedef struct {
	unsigned char	*colors;
	const float		*xyz;
	const float		*normal;
	int				numVertexes;
	int				ambientLightInt;
	float			ambientLight[3];
	float			directedLight[3];
	float			lightDir[3];
	float			sink;
} rigDiffuse_t;

static int RigFtol( float f )
{
	return ( int )f;
}

static void DiffuseBody( void *vctx, int begin, int end )
{
	rigDiffuse_t	*c = ( rigDiffuse_t * )vctx;
	int				i, j;
	const float		*v = c->xyz + 4 * begin;
	const float		*normal = c->normal + 4 * begin;
	float			incoming;
	unsigned char	*colors = c->colors;

	for ( i = begin; i < end; i++, v += 4, normal += 4 ) {
		incoming = normal[0] * c->lightDir[0] + normal[1] * c->lightDir[1]
				   + normal[2] * c->lightDir[2];
		if ( incoming <= 0 ) {
			*( int * )&colors[i * 4] = c->ambientLightInt;
			continue;
		}
		j = RigFtol( c->ambientLight[0] + incoming * c->directedLight[0] );
		if ( j > 255 ) j = 255;
		colors[i * 4 + 0] = ( unsigned char )j;

		j = RigFtol( c->ambientLight[1] + incoming * c->directedLight[1] );
		if ( j > 255 ) j = 255;
		colors[i * 4 + 1] = ( unsigned char )j;

		j = RigFtol( c->ambientLight[2] + incoming * c->directedLight[2] );
		if ( j > 255 ) j = 255;
		colors[i * 4 + 2] = ( unsigned char )j;

		colors[i * 4 + 3] = 255;
		c->sink += incoming;
	}
}

static unsigned g_seed = 12345u;

static int NextCount( int lo, int hi )
{
	g_seed = g_seed * 1664525u + 1013904223u;

	return lo + ( int )( ( g_seed >> 8 ) % ( unsigned )( hi - lo + 1 ) );
}

/* ---- checks -------------------------------------------------------------- */

static int g_checks;
static int g_failures;
static float g_sinkHold;

static void check( int ok, const char *what )
{
	g_checks++;
	if ( !ok ) {
		g_failures++;
		printf( "  FAIL  %s\n", what );
	}
}

#define MAX_ELEM 65536

static float	g_src[ MAX_ELEM * 4 ];
static float	g_outNew[ MAX_ELEM * 4 ];
static float	g_outOld[ MAX_ELEM * 4 ];

static float RunOne( int isNew, int count, int minChunk, float *out )
{
	walkCtx_t	c;

	c.xyz = g_src;
	c.out = out;
	c.sink = 0.0f;
	if ( isNew ) {
		JobPool_ParallelRange( count, minChunk, WalkBody, &c );
	} else {
		OldPool_ParallelRange( count, minChunk, WalkBody, &c );
	}

	return c.sink;
}

static void ShapeAgreement( int count, int minChunk )
{
	int	i;

	memset( g_outNew, 0xA5, ( size_t )count * sizeof( float ) * 4 );
	memset( g_outOld, 0x5A, ( size_t )count * sizeof( float ) * 4 );
	g_sinkHold += RunOne( 1, count, minChunk, g_outNew );
	g_sinkHold += RunOne( 0, count, minChunk, g_outOld );

	check( memcmp( g_outNew, g_outOld, ( size_t )count * sizeof( float ) * 4 ) == 0,
		   "the shipping pool writes the control's bytes" );
	if ( memcmp( g_outNew, g_outOld, ( size_t )count * sizeof( float ) * 4 ) != 0 ) {
		for ( i = 0; i < count * 4; i++ ) {
			if ( g_outNew[ i ] != g_outOld[ i ] ) {
				printf( "        first differing float index %d of %d\n", i, count * 4 );
				break;
			}
		}
	}
}

/* The texcoord bodies through the pool against the same range run inline: the
   two must write the same bytes, which is the output-identity claim for handing
   these sites to the pool at any chunk. */
static float RunEnvTex( int pooled, int count, int minChunk )
{
	rigEnvTex_t	c;

	c.xyz = g_src;
	c.normal = g_src;
	c.st = ( float * )g_outNew;
	c.numVertexes = count;
	c.viewOrigin[0] = 12.5f; c.viewOrigin[1] = -3.25f; c.viewOrigin[2] = 40.0f;
	c.sink = 0.0f;
	if ( pooled ) {
		JobPool_ParallelRange( count, minChunk, EnvTexBody, &c );
	} else {
		EnvTexBody( &c, 0, count );
	}

	return c.sink;
}

static float RunDiffuse( int pooled, int count, int minChunk )
{
	rigDiffuse_t	c;

	c.colors = ( unsigned char * )g_outNew;
	c.xyz = g_src;
	c.normal = g_src;
	c.numVertexes = count;
	c.ambientLightInt = 0x00804020;
	c.ambientLight[0] = 0.35f; c.ambientLight[1] = 0.30f; c.ambientLight[2] = 0.25f;
	c.directedLight[0] = 0.9f; c.directedLight[1] = 0.8f; c.directedLight[2] = 0.7f;
	c.lightDir[0] = 0.4f; c.lightDir[1] = -0.5f; c.lightDir[2] = 0.76f;
	c.sink = 0.0f;
	if ( pooled ) {
		JobPool_ParallelRange( count, minChunk, DiffuseBody, &c );
	} else {
		DiffuseBody( &c, 0, count );
	}

	return c.sink;
}

static void BodyAgreement( const char *name, float ( *run )( int pooled, int count, int minChunk ),
						   size_t perVertex, int count, int minChunk )
{
	size_t	bytes = ( size_t )count * perVertex;

	memset( g_outNew, 0x5A, bytes );
	g_sinkHold += run( 0, count, minChunk );
	memcpy( g_outOld, g_outNew, bytes );

	memset( g_outNew, 0xA5, bytes );
	g_sinkHold += run( 1, count, minChunk );

	check( memcmp( g_outNew, g_outOld, bytes ) == 0, name );
	if ( memcmp( g_outNew, g_outOld, bytes ) != 0 ) {
		size_t	i;

		for ( i = 0; i < bytes; i++ ) {
			if ( ( ( unsigned char * )g_outNew )[i] != ( ( unsigned char * )g_outOld )[i] ) {
				printf( "        %s first differing byte %d of %d\n", name, ( int )i, ( int )bytes );
				break;
			}
		}
	}
}

/* ---- 2. the dispatch cost itself ---------------------------------------- */

static void DispatchCost( int chunks, int isNew, sample_t *out )
{
	sample_t	a, b;
	int			i;
	const int	batches = 20000;

	Sample( &a );
	for ( i = 0; i < batches; i++ ) {
		if ( isNew ) {
			JobPool_ParallelRange( chunks, 1, EmptyBody, NULL );
		} else {
			OldPool_ParallelRange( chunks, 1, EmptyBody, NULL );
		}
	}
	Sample( &b );
	Delta( &a, &b, out );
}

static void Row( const char *tag, const char *name, int batches, const sample_t *s )
{
	printf( "  %-9s %-12s %7d batches  wall %8.1f ms  ctx %9ld (%8.0f /s)  "
			"sys %7.1f ms  user %8.1f ms\n",
			tag, name, batches, s->wall * 1000.0, CtxOf( s ),
			( double )CtxOf( s ) / s->wall,
			( double )s->sysUsec / 1000.0, ( double )s->userUsec / 1000.0 );
}

/* ---- 3. the frame walk -------------------------------------------------- */

static void Walk( int isNew, int heavy, int minChunk, int frames, int surfPerFrame, sample_t *out );

static void Walk( int isNew, int heavy, int minChunk, int frames, int surfPerFrame, sample_t *out )
{
	static walkCtx_t	c;
	sample_t			a, b;
	int					f, s;

	c.xyz = g_src;
	c.out = g_outNew;
	c.sink = 0.0f;

	Sample( &a );
	for ( f = 0; f < frames; f++ ) {
		for ( s = 0; s < surfPerFrame; s++ ) {
			int	count = NextCount( 64, 4096 );

			if ( isNew ) {
				JobPool_ParallelRange( count, minChunk, heavy ? HeavyBody : WalkBody, &c );
			} else {
				OldPool_ParallelRange( count, minChunk, heavy ? HeavyBody : WalkBody, &c );
			}
		}
	}
	Sample( &b );
	Delta( &a, &b, out );
	g_sinkHold += c.sink;
	( void )heavy;
}

/* ---- 5. the texcoord grain: one surface size per run --------------------- */

/* S surfaces of exactly N vertices per frame, one body.  minChunk == N is the
   inline row (the pool's own count<=minChunk arm), so the table reads the
   break-even as "a surface of N vertices" and [PERF:walk]'s verts/surf column
   says which row the boot is on. */
typedef struct {
	const char	*name;
	void		( *body )( void *ctx, int begin, int end );
	void		*ctx;
} rigBody_t;

static void SurfaceSweep( const rigBody_t *b, int sizeVerts, int minChunk,
						  int frames, int surfPerFrame, sample_t *out )
{
	sample_t	a, b2;
	int			f, s;

	Sample( &a );
	for ( f = 0; f < frames; f++ ) {
		for ( s = 0; s < surfPerFrame; s++ ) {
			JobPool_ParallelRange( sizeVerts, minChunk, b->body, b->ctx );
		}
	}
	Sample( &b2 );
	Delta( &a, &b2, out );
}

int main( int argc, char **argv )
{
	static const int	agreeShapes[] = { 0, 1, 2, 255, 256, 257, 511, 512, 513,
										  1024, 1025, 4096, 20000, 65535 };
	static const int	agreeChunks[] = { 1, 64, 256, 512, 2048 };
	static const int	walkChunks[] = { 256, 512, 1024, 2048, 4096 };
	const int			frames = 60;
	const int			surfPerFrame = 380;
	int					i;
	int					walkers = 4;
	int					h;
	sample_t			so, sn;
	double				baseWall = 0.0;

	if ( argc > 1 ) {
		walkers = atoi( argv[1] );
	}

	for ( i = 0; i < MAX_ELEM * 4; i++ ) {
		g_src[ i ] = ( float )( i % 251 ) * 0.01f;
	}

	printf( "threadroot: cores=%d workers=%d (old pool and shipping pool)\n",
			SDL_GetCPUCount(), walkers );

	JobPool_Init( "trnew", walkers );
	OldPool_Init( "trold", walkers );

	check( JobPool_Enabled() && OldPool_Enabled(), "both pools are live" );
	check( JobPool_NumWorkers() == OldPool_NumWorkers(), "both pools run the same width" );

	/* 1. output identity, across chunk and shape */
	for ( i = 0; i < ( int )( sizeof( agreeChunks ) / sizeof( agreeChunks[0] ) ); i++ ) {
		int	ci;

		for ( ci = 0; ci < ( int )( sizeof( agreeShapes ) / sizeof( agreeShapes[0] ) ); ci++ ) {
			ShapeAgreement( agreeShapes[ ci ], agreeChunks[ i ] );
		}
	}

	/* 2. the dispatch cost with an empty body */
	printf( "threadroot: dispatch cost, empty body\n" );
	{
		static const int	chunks[] = { 2, 4, 8 };

		for ( i = 0; i < ( int )( sizeof( chunks ) / sizeof( chunks[0] ) ); i++ ) {
			DispatchCost( chunks[ i ], 0, &so );
			DispatchCost( chunks[ i ], 1, &sn );
			printf( "  %d chunks: control %6.2f us/batch ctx %6ld | "
					"shipping %6.2f us/batch ctx %6ld\n",
					chunks[ i ],
					so.wall * 1e6 / 20000.0, CtxOf( &so ),
					sn.wall * 1e6 / 20000.0, CtxOf( &sn ) );
		}
	}

	/* 3. the frame walk, per candidate chunk */
	for ( h = 0; h < 2; h++ ) {
		printf( "threadroot: frame walk (%s body), %d frames x %d surfaces, vertex counts 64..4096\n",
				h ? "heavy" : "light", frames, surfPerFrame );
		for ( i = 0; i < ( int )( sizeof( walkChunks ) / sizeof( walkChunks[0] ) ); i++ ) {
			char	name[64];

			snprintf( name, sizeof( name ), "chunk%d", walkChunks[ i ] );

			g_seed = 987654321u;
			Walk( 0, h, walkChunks[ i ], frames, surfPerFrame, &so );
			g_seed = 987654321u;
			Walk( 1, h, walkChunks[ i ], frames, surfPerFrame, &sn );

			Row( "control", name, frames * surfPerFrame, &so );
			Row( "shipping", name, frames * surfPerFrame, &sn );
			printf( "           wall %.2fx  ctx %.2fx  sys %.2fx\n",
					( sn.wall > 0.0 ) ? so.wall / sn.wall : 0.0,
					( CtxOf( &sn ) > 0 ) ? ( double )CtxOf( &so ) / ( double )CtxOf( &sn ) : 0.0,
					( sn.sysUsec > 0 ) ? ( double )so.sysUsec / ( double )sn.sysUsec : 0.0 );
			if ( h == 0 && i == 0 ) {
				baseWall = so.wall;
			}
		}
		( void )baseWall;
	}

	/* 5. the texcoord grain: the walk's own bodies at one surface size per run */
	{
		static rigEnvTex_t	env;
		static rigDiffuse_t	dif;
		static const int	sizes[] = { 64, 128, 256, 512, 1024, 2048, 4096 };
		static const int	chunks[] = { 64, 128, 256, 512, 1024 };
		const int			surfFrames = 120;
		const int			surfs = 96;
		rigBody_t			bodies[2];
		int					si, bi;

		env.xyz = g_src;
		env.normal = g_src;
		env.st = g_outNew;
		env.numVertexes = MAX_ELEM;
		env.viewOrigin[0] = 12.5f; env.viewOrigin[1] = -3.25f; env.viewOrigin[2] = 40.0f;
		env.sink = 0.0f;
		dif.colors = ( unsigned char * )g_outNew;
		dif.xyz = g_src;
		dif.normal = g_src;
		dif.numVertexes = MAX_ELEM;
		dif.ambientLightInt = 0x00804020;
		dif.ambientLight[0] = 0.35f; dif.ambientLight[1] = 0.30f; dif.ambientLight[2] = 0.25f;
		dif.directedLight[0] = 0.9f; dif.directedLight[1] = 0.8f; dif.directedLight[2] = 0.7f;
		dif.lightDir[0] = 0.4f; dif.lightDir[1] = -0.5f; dif.lightDir[2] = 0.76f;
		dif.sink = 0.0f;

		bodies[0].name = "envtex";
		bodies[0].body = EnvTexBody;
		bodies[0].ctx = &env;
		bodies[1].name = "diffuse";
		bodies[1].body = DiffuseBody;
		bodies[1].ctx = &dif;

		BodyAgreement( "envtex", RunEnvTex, sizeof( float ) * 2, 1024, 256 );
		BodyAgreement( "diffuse", RunDiffuse, 4, 1024, 256 );

		for ( bi = 0; bi < 2; bi++ ) {
			double	inlineWall[7] = { 0 };
			int		k;

			printf( "threadroot: texcoord grain (%s), %d frames x %d surfaces of one size\n",
					bodies[ bi ].name, surfFrames, surfs );
			printf( "  %-8s %10s %12s %11s %10s\n", "surface", "inline", "chunk", "wall", "ns/vert" );

			for ( si = 0; si < ( int )( sizeof( sizes ) / sizeof( sizes[0] ) ); si++ ) {
				int	n = sizes[ si ];

				SurfaceSweep( &bodies[ bi ], n, n, surfFrames, surfs, &so );
				inlineWall[ si ] = so.wall;

				printf( "  %-8d %10.2fms %12s %11s %10.2f\n", n, so.wall * 1000.0,
						"-", "inline",
						( double )so.wall * 1e9 / ( ( double )surfFrames * surfs * n ) );
				for ( k = 0; k < ( int )( sizeof( chunks ) / sizeof( chunks[0] ) ); k++ ) {
					int	c = chunks[ k ];

					if ( c >= n ) {
						continue;			/* the pool would run it inline anyway */
					}
					SurfaceSweep( &bodies[ bi ], n, c, surfFrames, surfs, &sn );
					printf( "  %-8d %10.2fms %12d %11.2fms %10.2f  %s\n", n,
							inlineWall[ si ] * 1000.0, c, sn.wall * 1000.0,
							( double )sn.wall * 1e9 / ( ( double )surfFrames * surfs * n ),
							( sn.wall < inlineWall[ si ] ) ? "faster" : "slower" );
				}
			}
		}
	}

	OldPool_Shutdown();
	JobPool_Shutdown();

	printf( "threadroot: %s (%d checks, %d failed, sink %.0f)\n",
			g_failures ? "FAIL" : "PASS", g_checks, g_failures,
			( double )g_sinkHold );

	return g_failures ? 1 : 0;
}
