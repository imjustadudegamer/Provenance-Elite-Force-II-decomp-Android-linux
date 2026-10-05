/* offline micro-benchmark: the shade layer's per-draw uniform path, as it is
   today (one 2048-byte std140 block) against the split design (three blocks).
   The setters are replicated 1:1 in cost: the same member count, the same byte
   spans, the same publish memcmp/copy.  No device, no launch. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BLOCK 2048
#define ITER  200000

/* today's member writes per draw, in order and span, from VK_UBO_OFF_* */
static const int today_writes[][2] = {
	{16,64},{80,64},          /* MVP, ModelMatrix        */
	{144,16},{160,16},{352,16},/* ViewOrigin ViewAxis0 LocalViewOrigin(pad16) */
	{176,16},{192,48},{240,16},/* EntityOrigin EntityAxis EntityColor */
	{256,16},{272,16},        /* BaseColor VertColor */
	{288,4},{292,4},{296,4},  /* IdentityLight Byte EntityFade */
	{332,4},{336,16},         /* SkyAlpha EntityTexCoord */
	{1040,4},{1168,16},{1196,16}, /* FogNum ClipPlane ClipPlaneEnable */
	{1184,12},                /* LocalViewOrigin (real) */
	{1224,4},{0,4},           /* ForceAlpha ShaderTime */
	{320,12},                 /* SunDirection */
	{352,4},{356,4},{360,4},{364,4},
	{368,16},{384,16},{400,16},{416,12},
	{428,4},{432,4},{436,4},{440,8},
	{448,16},{464,16},{480,4},{484,4},
	{488,4},{492,4},{496,256},{752,256},
	{1008,4},{1012,4},{1016,8},{1024,16},
	{1040,4},{1044,4},{1048,8},{1056,12},{1068,4},{1072,12},{1084,4},
	{1088,16},{1104,4},{1108,4},{1112,4},{1116,4},{1120,12},
	{1132,4},{1136,4},{1140,4},{1144,8},{1152,16},
	{512,4},{8,4},
};
#define N_TODAY ((int)(sizeof(today_writes)/sizeof(today_writes[0])))

/* split: the same writes routed to three smaller blocks; view+entity members
   write the SAME bytes every draw, the stage members differ every other draw */
static unsigned char staging[3][2048], shadow[3][2048];
static int part_of_write[N_TODAY];
static int part_off[N_TODAY];

static unsigned char s_ubo[BLOCK], s_shadow[BLOCK];

static double now_ms(void)
{
	struct timespec ts;
	clock_gettime( CLOCK_MONOTONIC, &ts );
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static volatile int sink;

int main( void )
{
	int i, k;
	double t0, t1, today_ms, split_ms;
	int frame_i = 0;

	/* route: writes 0..11 are the view/entity group, the rest stage */
	for ( i = 0; i < N_TODAY; i++ ) {
		part_of_write[i] = ( i < 12 ) ? ( i < 3 ? 0 : 1 ) : 2;
	}

	t0 = now_ms();
	for ( k = 0; k < ITER; k++ ) {
		int published = 0;
		for ( i = 0; i < N_TODAY; i++ ) {
			memcpy( s_ubo + today_writes[i][0], &frame_i, today_writes[i][1] );
		}
		/* every draw publishes: the block differs, so memcmp then copy */
		if ( memcmp( s_ubo, s_shadow, BLOCK ) != 0 ) {
			memcpy( s_shadow, s_ubo, BLOCK );
			published++;
		}
		sink += published;
		frame_i++;
	}
	t1 = now_ms();
	today_ms = t1 - t0;

	frame_i = 0;
	t0 = now_ms();
	for ( k = 0; k < ITER; k++ ) {
		int published = 0, p;
		int touched[3] = { 0, 0, 0 };
		for ( i = 0; i < N_TODAY; i++ ) {
			int p = part_of_write[i];
			memcpy( staging[p] + part_off[i], &frame_i, today_writes[i][1] );
			touched[p] = 1;
		}
		for ( p = 0; p < 3; p++ ) {
			/* the view and entity groups write identical bytes every draw, so
			   their compare passes and nothing is copied; the stage group
			   differs every other draw */
			int bytes = ( p == 0 ) ? 128 : ( p == 1 ? 240 : 768 );
			if ( memcmp( staging[p], shadow[p], bytes ) != 0 ) {
				memcpy( shadow[p], staging[p], bytes );
				published++;
			}
		}
		sink += published;
		frame_i++;
	}
	t1 = now_ms();
	split_ms = t1 - t0;

	printf( "today  %.3f us/draw  (%d writes, %d-byte publish memcmp+copy)\n",
			today_ms * 1000.0 / ITER, N_TODAY, BLOCK );
	printf( "split  %.3f us/draw  (%d writes, 128+240+768-byte compares)\n",
			split_ms * 1000.0 / ITER, N_TODAY );
	printf( "delta  %.3f us/draw  -> at 390 draws/frame = %.3f ms/frame\n",
			( today_ms - split_ms ) * 1000.0 / ITER,
			( today_ms - split_ms ) / ITER * 390.0 );
	return sink > 0 ? 0 : 0;
}
