/* Offline self-test for the [VK:bigdraw] clip-then-measure math.
 *
 * The clip, the box accumulation and the anomaly predicate are copied from
 * code/renderervulkan/vk_shade.c's VK_Shade_ClipTri and VK_Shade_FootprintRow.
 * If the source changes shape, this file has to be re-synced: it exists so the
 * instrument can be convicted without a boot, which is the only way it has ever
 * been wrong three times in a row.
 *
 * Build:  cc -O2 -o /tmp/probe_selftest tools/probe_math_selftest.c
 * Run:    /tmp/probe_selftest            (green on the shipping math)
 *         -DFAULT_SWAP / -DFAULT_NOWGUARD / -DFAULT_RAWAREA go red
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

typedef int qboolean;
#define qtrue  1
#define qfalse 0

/* ---- the clip, verbatim from VK_Shade_ClipTri ---------------------------- */

#ifdef FAULT_NOWGUARD
#define GUARD_BIAS 0.0f
#else
#define GUARD_BIAS 1e-4f
#endif

static int ClipTri( const float pj[16], const float *const v[3], float ndc[16][2] )
{
	static const float planes[7][4] = {
		{ -1, 0, 0, 1 }, { 1, 0, 0, 1 },
		{ 0, -1, 0, 1 }, { 0, 1, 0, 1 },
		{ 0, 0, -1, 1 }, { 0, 0, 1, 0 },
		{ 0, 0, 0, 1 }
	};
	static const float bias[7] = { 0, 0, 0, 0, 0, 0, GUARD_BIAS };
	float poly[16][4];
	float out[16][4];
	int   n = 3, p, i, j;

	for ( i = 0; i < 3; i++ ) {
		const float *xyz = v[i];

		poly[i][0] = pj[0] * xyz[0] + pj[4] * xyz[1] + pj[8]  * xyz[2] + pj[12];
		poly[i][1] = pj[1] * xyz[0] + pj[5] * xyz[1] + pj[9]  * xyz[2] + pj[13];
		poly[i][2] = pj[2] * xyz[0] + pj[6] * xyz[1] + pj[10] * xyz[2] + pj[14];
		poly[i][3] = pj[3] * xyz[0] + pj[7] * xyz[1] + pj[11] * xyz[2] + pj[15];
	}

	for ( p = 0; p < 7 && n; p++ ) {
		int m = 0;

		for ( i = 0; i < n; i++ ) {
			const float *a = poly[i];
			const float *b = poly[( i + 1 ) % n];
			float		 da = 0.0f, db = 0.0f, t;

			for ( j = 0; j < 4; j++ ) {
				da += planes[p][j] * a[j];
				db += planes[p][j] * b[j];
			}
			da -= bias[p];
			db -= bias[p];

			if ( da >= 0.0f ) {
				out[m][0] = a[0]; out[m][1] = a[1];
				out[m][2] = a[2]; out[m][3] = a[3];
				m++;
			}
			if ( ( da >= 0.0f ) != ( db >= 0.0f ) ) {
				t = da / ( da - db );
				for ( j = 0; j < 4; j++ ) {
					out[m][j] = a[j] + t * ( b[j] - a[j] );
				}
				m++;
			}
		}

		n = m;
		if ( n > 14 ) {
			n = 14;
		}
		memcpy( poly, out, (size_t)n * sizeof( poly[0] ) );
	}

	for ( i = 0; i < n; i++ ) {
		ndc[i][0] = poly[i][0] / poly[i][3];
		ndc[i][1] = poly[i][1] / poly[i][3];
	}

	return n;
}

/* ---- the measurement, verbatim in shape from VK_Shade_FootprintRow -------- */

typedef struct {
	float clip[4];	/* x0 y0 x1 y1, the clipped polygon's box */
	float raw[4];	/* the unclipped corners' box */
	float area;		/* the clipped polygon's area, frame is 4.0 */
	int   tris;
	int   rawInside;	/* every unclipped corner already projects inside +-1 */
} measure_t;

static void Measure( const float pj[16], const float *xyz, const int *indexes,
					 int numIndexes, measure_t *out )
{
	float cminx, cminy, cmaxx, cmaxy, clipArea;
	float bx0, by0, bx1, by1;
	int   i, v, tris;

	cminx = cminy = 2.0f;
	cmaxx = cmaxy = -2.0f;
	bx0 = by0 = 2.0f;
	bx1 = by1 = -2.0f;
	clipArea = 0.0f;
	tris = numIndexes / 3;

	for ( i = 0; i + 2 < numIndexes; i += 3 ) {
		const float	*vert[3];
		float		 ndc[16][2];
		int			 n, k, inside = 0;

		for ( v = 0; v < 3; v++ ) {
			int vi = indexes[i + v];

			if ( vi < 0 || vi >= 4096 ) {
				inside = -1;
				continue;
			}
			vert[v] = xyz + vi * 3;
		}
		if ( inside == -1 ) {
			continue;
		}

		n = ClipTri( pj, vert, ndc );

		for ( v = 0; v < 3; v++ ) {
			const float *p = vert[v];
			float		 cw = pj[3] * p[0] + pj[7] * p[1] + pj[11] * p[2] + pj[15];
			float		 nx, ny;

			if ( cw < 1e-6f && cw > -1e-6f ) {
				continue;
			}
			nx = ( pj[0] * p[0] + pj[4] * p[1] + pj[8]  * p[2] + pj[12] ) / cw;
			ny = ( pj[1] * p[0] + pj[5] * p[1] + pj[9]  * p[2] + pj[13] ) / cw;
			if ( nx < bx0 ) bx0 = nx;
			if ( ny < by0 ) by0 = ny;
			if ( nx > bx1 ) bx1 = nx;
			if ( ny > by1 ) by1 = ny;
		}

		if ( n >= 3 ) {
			float cx0 = 2.0f, cy0 = 2.0f, cx1 = -2.0f, cy1 = -2.0f;
			float area;

			for ( k = 0; k < n; k++ ) {
				if ( ndc[k][0] < cx0 ) cx0 = ndc[k][0];
				if ( ndc[k][1] < cy0 ) cy0 = ndc[k][1];
				if ( ndc[k][0] > cx1 ) cx1 = ndc[k][0];
				if ( ndc[k][1] > cy1 ) cy1 = ndc[k][1];
			}
#ifdef FAULT_RAWAREA
			area = ( bx1 - bx0 ) * ( by1 - by0 );
#else
			area = ( cx1 - cx0 ) * ( cy1 - cy0 );
#endif
			if ( area > clipArea ) {
				clipArea = area;
			}
			if ( cx0 < cminx ) cminx = cx0;
			if ( cy0 < cminy ) cminy = cy0;
			if ( cx1 > cmaxx ) cmaxx = cx1;
			if ( cy1 > cmaxy ) cmaxy = cy1;
		}
	}

#ifdef FAULT_SWAP
	/* the planted fault: one accumulator pair feeding both fields, which is the
	   inversion that produced three boots of misread rows */
	out->clip[0] = bx0; out->clip[1] = by0;
	out->clip[2] = bx1; out->clip[3] = by1;
	out->raw[0] = cminx; out->raw[1] = cminy;
	out->raw[2] = cmaxx; out->raw[3] = cmaxy;
#else
	out->clip[0] = cminx; out->clip[1] = cminy;
	out->clip[2] = cmaxx; out->clip[3] = cmaxy;
	out->raw[0] = bx0; out->raw[1] = by0;
	out->raw[2] = bx1; out->raw[3] = by1;
#endif
	out->area = clipArea;
	out->tris = tris;
	/* the anomaly tell: the corners were already inside the volume, so the clip
	   did no work and the footprint is the data's own, not the frame's cut of a
	   bigger primitive */
	out->rawInside = ( bx0 >= -1.0f && by0 >= -1.0f && bx1 <= 1.0f && by1 <= 1.0f )
					 ? qtrue : qfalse;
}

/* A wedge is one primitive that was never in world space to begin with: its
   corners land inside the volume unclipped and its clipped area is large.  A
   wall the camera stands beside spans far outside and is cut down, so raw is
   not inside and it does not flag. */
static int Anomaly( const measure_t *m )
{
	return ( m->rawInside && m->area >= 0.5f ) ? qtrue : qfalse;
}

/* ---- fixtures ------------------------------------------------------------ */

static int s_fail;
static void Check( const char *what, int got, int want )
{
	printf( "  %-46s got %-4d want %-4d  %s\n", what, got, want,
			got == want ? "ok" : "FAIL" );
	if ( got != want ) {
		s_fail++;
	}
}

static void CheckF( const char *what, float got, float want, float tol )
{
	int ok = fabsf( got - want ) <= tol;

	printf( "  %-46s got %-9.4f want %-9.4f %s\n", what, got, want,
			ok ? "ok" : "FAIL" );
	if ( !ok ) {
		s_fail++;
	}
}

static void Box( const char *name, const float b[4] )
{
	printf( "  %-46s (%.3f,%.3f)-(%.3f,%.3f)\n", name, b[0], b[1], b[2], b[3] );
}

int main( void )
{
	/* identity MVP: clip space is the vertex itself, w = 1 */
	float ident[16];
	/* a perspective like the world's: 90 deg horizontal, near 2, far 2048 */
	float persp[16];
	float xyz[16][3];
	int   idx[6];
	measure_t m;

	memset( ident, 0, sizeof( ident ) );
	ident[0] = ident[5] = ident[10] = ident[15] = 1.0f;

	memset( persp, 0, sizeof( persp ) );
	persp[0] = 1.0f;			/* x scale */
	persp[5] = 4.0f / 3.0f;		/* y scale */
	persp[10] = 1.0f;			/* clip z = z - near */
	persp[11] = 1.0f;			/* Vulkan: w = z */
	persp[14] = -0.01f;			/* near = 0.01 */

	printf( "probe math self-test\n" );

	/* (a) fully inside: the clip does no work, so clip == raw and the area is
	   the triangle's own box */
	{
		float a[3] = { -0.25f, -0.25f, 0.5f };
		float b[3] = {  0.25f, -0.25f, 0.5f };
		float c[3] = {  0.0f,   0.25f, 0.5f };

		memcpy( xyz[0], a, 12 ); memcpy( xyz[1], b, 12 ); memcpy( xyz[2], c, 12 );
		idx[0] = 0; idx[1] = 1; idx[2] = 2;
		Measure( ident, &xyz[0][0], idx, 3, &m );
		printf( "(a) fully inside\n" );
		Box( "raw", m.raw ); Box( "clip", m.clip );
		Check( "clip==raw x0", (int)( m.clip[0] * 1000 ), (int)( m.raw[0] * 1000 ) );
		Check( "clip==raw x1", (int)( m.clip[2] * 1000 ), (int)( m.raw[2] * 1000 ) );
		CheckF( "area == true box area", m.area, 0.5f * 0.5f, 1e-3f );
		Check( "rawInside", m.rawInside, 1 );
		Check( "not flagged (small area)", Anomaly( &m ), 0 );
	}

	/* (b) fully outside: nothing survives, the clip box stays empty */
	{
		float a[3] = { 3.0f, 3.0f, 0.5f };
		float b[3] = { 4.0f, 3.0f, 0.5f };
		float c[3] = { 3.5f, 4.0f, 0.5f };

		memcpy( xyz[0], a, 12 ); memcpy( xyz[1], b, 12 ); memcpy( xyz[2], c, 12 );
		Measure( ident, &xyz[0][0], idx, 3, &m );
		printf( "(b) fully outside\n" );
		Box( "raw", m.raw ); Box( "clip", m.clip );
		Check( "survivors (clip box still empty)", m.clip[0] > m.clip[2], 1 );
		Check( "area == 0", (int)( m.area * 1000 ), 0 );
		Check( "not flagged", Anomaly( &m ), 0 );
	}

	/* (c) one corner crossing: the clipped box is strictly inside raw on the
	   axis that crosses */
	{
		float a[3] = { 0.0f, 0.0f, 0.5f };
		float b[3] = { 0.5f, 0.0f, 0.5f };
		float c[3] = { 2.0f, 0.5f, 0.5f };

		memcpy( xyz[0], a, 12 ); memcpy( xyz[1], b, 12 ); memcpy( xyz[2], c, 12 );
		Measure( ident, &xyz[0][0], idx, 3, &m );
		printf( "(c) one corner crossing\n" );
		Box( "raw", m.raw ); Box( "clip", m.clip );
		Check( "raw reaches the crossing corner", m.raw[2] > 1.0f, 1 );
		Check( "clip is cut at the boundary", fabsf( m.clip[2] - 1.0f ) < 1e-3f, 1 );
		Check( "clip strictly inside raw on x", m.clip[2] < m.raw[2], 1 );
		CheckF( "area is the clipped box's", m.area, 0.25f, 1e-3f );
		Check( "not flagged", Anomaly( &m ), 0 );
	}

	/* (d) a vertex at w ~ 0 takes the guard path and does not divide by it */
	{
		float a[3] = { 0.1f,  0.1f, 5e-5f };	/* w above 0, below the guard */
		float b[3] = { 0.1f,  0.1f, 0.5f };
		float c[3] = { -0.1f, 0.1f, 0.5f };
		float big[16][2];
		int   n;

		memcpy( xyz[0], a, 12 ); memcpy( xyz[1], b, 12 ); memcpy( xyz[2], c, 12 );
		{
			const float *vt[3] = { xyz[0], xyz[1], xyz[2] };
			n = ClipTri( persp, vt, big );
		}
		printf( "(d) vertex at w ~ 0\n" );
		/* The guard is not observable with this projection family: clip z is
		   z - near and w is z, so a vertex the guard would reject has already
		   failed the z >= 0 plane, and one that passes it has w large enough
		   that the x/y planes bound the divide.  It stays as defence against a
		   projection whose w row stops being z.  FAULT_NOWGUARD therefore does
		   not turn this harness red, and that is the finding, not a gap. */
		{
			int i, sane = 1;
			for ( i = 0; i < n; i++ ) {
				/* 1.0001: a survivor sits on the boundary plane and the divide
				   lands within an epsilon of exactly 1 */
				if ( !( big[i][0] >= -1.0001f && big[i][0] <= 1.0001f
					 && big[i][1] >= -1.0001f && big[i][1] <= 1.0001f ) ) {
					sane = 0;
				}
				if ( !( isfinite( big[i][0] ) && isfinite( big[i][1] ) ) ) {
					sane = 0;
				}
			}
			Check( "every survivor inside and finite", sane, 1 );
		}
		Measure( persp, &xyz[0][0], idx, 3, &m );
		Check( "clip box inside the unit cube",
			   m.clip[0] >= -1.0f && m.clip[2] <= 1.0f, 1 );
	}

	/* (e) the camera-adjacent wall: raw spans far outside, the clip fills the
	   frame, and the row must read ordinary -- this is the met_grate10 shape
	   that three boots of rows mistook for pre-projected data */
	{
		float a[3] = { -0.5f, -0.5f, 0.0125f };
		float b[3] = {  0.5f, -0.5f, 0.0125f };
		float c[3] = {  0.5f,  0.5f, 0.0125f };
		float d[3] = { -0.5f,  0.5f, 0.0125f };
		int   q[6] = { 0, 1, 2, 0, 2, 3 };

		memcpy( xyz[0], a, 12 ); memcpy( xyz[1], b, 12 );
		memcpy( xyz[2], c, 12 ); memcpy( xyz[3], d, 12 );
		Measure( persp, &xyz[0][0], q, 6, &m );
		printf( "(e) camera-adjacent wall (the met_grate10 shape)\n" );
		Box( "raw", m.raw ); Box( "clip", m.clip );
		/* the two triangles of the quad clip to bboxes that each cover most of
		   the frame, so the max is near 4 but not exactly it; the ceiling is the
		   assertion that catches raw area being measured instead of clipped */
		Check( "clip covers the frame", m.area >= 3.0f, 1 );
		Check( "area is the clipped area, not raw", m.area <= 4.001f, 1 );
		Check( "raw spans outside the unit cube", m.rawInside == 0, 1 );
		Check( "NOT flagged as an anomaly", Anomaly( &m ), 0 );
	}

	/* (f) the radar floor: raw is the full cube because the data is authored in
	   that space and the projection is the ortho that goes with it.  Geometrically
	   this is the same signature as a wedge -- corners already inside the volume,
	   clipped area the whole frame -- and no measurement of the geometry alone
	   separates them.  That is a finding, not a defect: the instrument reports
	   the shape, and the row's sb / ent / model / sib are what adjudicate it. */
	{
		float a[3] = { -1.0f, -1.0f, 0.5f };
		float b[3] = {  1.0f, -1.0f, 0.5f };
		float c[3] = {  1.0f,  1.0f, 0.5f };
		float d[3] = { -1.0f,  1.0f, 0.5f };
		int   q[6] = { 0, 1, 2, 0, 2, 3 };

		memcpy( xyz[0], a, 12 ); memcpy( xyz[1], b, 12 );
		memcpy( xyz[2], c, 12 ); memcpy( xyz[3], d, 12 );
		Measure( ident, &xyz[0][0], q, 6, &m );
		printf( "(f) radar floor (authored in the unit cube)\n" );
		Box( "raw", m.raw ); Box( "clip", m.clip );
		CheckF( "area == 4.0", m.area, 4.0f, 1e-3f );
		Check( "clip == raw == the cube", m.clip[0] == -1.0f && m.clip[2] == 1.0f, 1 );
		Check( "flagged -- indistinguishable from a wedge", Anomaly( &m ), 1 );
	}

	printf( "%s (%d failure%s)\n", s_fail ? "RED" : "GREEN",
			s_fail, s_fail == 1 ? "" : "s" );
	return s_fail ? 1 : 0;
}
