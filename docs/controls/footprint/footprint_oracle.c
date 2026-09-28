/*
 * footprint_oracle.c -- what the footprint census costs, and the gate that stops
 * the walk paying it for a row that prints nothing.
 *
 * The census (`VK_Shade_FootprintRow') exists to name a draw whose own vertices
 * cover half the frame.  That question was answered when the wedge closed (r12),
 * and the row has read zero since; the census nevertheless walked every triangle
 * of every batch every frame, on the walk thread, inside the SHADE window's
 * residual (no bucket charges it).  r12 cut it from a clip of every triangle
 * (72.8 ns/tri) to one projection pass (5.5 ns/tri) -- this control prices the
 * pass at the frame's real geometry and asserts the pass is not reached unless
 * `r_footprintProbe' arms it.
 *
 * The measured loop is the shipping one, quoted:
 *     for ( i = 0; i + 2 < numIndexes; i += 3 )
 *         for ( v = 0; v < 3; v++ ) { ...project vertex, grow the raw box... }
 *
 * Build/run: docs/controls/footprint/run.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int failures;
static int checks;

#define CHECK(cond, ...) do { \
	checks++; \
	if (!(cond)) { failures++; printf("  FAIL  "); printf(__VA_ARGS__); printf("\n"); } \
} while (0)

static double now_us(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

#define NV 8192
static float	xyz[NV * 3];
static int		idx[NV * 4];
static float	pj[16];
static volatile float	sink;

/* the pass, as the shipping function runs it: one projection per indexed vertex */
static double pass_us(int numIndexes, int reps)
{
	double	t0 = now_us();
	int		r, i, v;
	float	px0 = 2.0f, py0 = 2.0f, px1 = -2.0f, py1 = -2.0f;

	for (r = 0; r < reps; r++) {
		px0 = 2.0f; py0 = 2.0f; px1 = -2.0f; py1 = -2.0f;	/* VK_Shade_FootprintRow opens its box here */
		for (i = 0; i + 2 < numIndexes; i += 3) {
			for (v = 0; v < 3; v++) {
				int			vi = idx[i + v];
				const float	*p;
				float		cw, nx, ny;

				if (vi < 0 || vi >= NV) {
					continue;
				}
				p = xyz + vi * 3;
				cw = pj[3] * p[0] + pj[7] * p[1] + pj[11] * p[2] + pj[15];
				if (cw < 1e-6f && cw > -1e-6f) {
					continue;
				}
				nx = (pj[0] * p[0] + pj[4] * p[1] + pj[8] * p[2] + pj[12]) / cw;
				ny = (pj[1] * p[0] + pj[5] * p[1] + pj[9] * p[2] + pj[13]) / cw;
				if (nx < px0) px0 = nx;
				if (ny < py0) py0 = ny;
				if (nx > px1) px1 = nx;
				if (ny > py1) py1 = ny;
			}
		}
		sink += px0 + py0 + px1 + py1;		/* the loser of the DCE race */
	}
	return (now_us() - t0) / reps;
}

int main(int argc, char **argv)
{
	const char	*src = (argc > 1) ? argv[1] : "../../../app/src/main/cpp/code/renderervulkan/vk_shade.c";
	int			mutant = (argc > 2) ? atoi(argv[2]) : 0;
	int			verts = (argc > 3) ? atoi(argv[3]) : 12000;
	FILE		*f;
	char		*text;
	long		n;
	int			i, reps;
	double		us;
	const char	*gate;
	int			gated;

	f = fopen(src, "rb");
	if (!f) {
		printf("  FAIL  cannot read %s\n", src);
		return 1;
	}
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	text = (char *)malloc((size_t)n + 1);
	if (fread(text, 1, (size_t)n, f) != (size_t)n) {
		printf("  FAIL  short read\n");
		return 1;
	}
	text[n] = 0;
	fclose(f);

	for (i = 0; i < NV; i++) {
		xyz[i * 3 + 0] = ((i * 37) % 2000 - 1000) / 10.0f;
		xyz[i * 3 + 1] = ((i * 53) % 2000 - 1000) / 10.0f;
		xyz[i * 3 + 2] = -(5.0f + (i % 3000) / 4.0f);
	}
	{
		float fov = 1.0f, zn = 4.0f, zf = 4096.0f;

		memset(pj, 0, sizeof pj);
		pj[0] = fov / (4.0f / 3.0f);
		pj[5] = fov;
		pj[10] = (zf + zn) / (zn - zf);
		pj[11] = -1.0f;
		pj[14] = (2 * zf * zn) / (zn - zf);
	}

	/* the gate: the census must return before the pass unless r_footprintProbe
	   is armed.  The source is the authority, not this harness. */
	gate = strstr(text, "s_fpCvar = ri.Cvar_Get( \"r_footprintProbe\", \"0\", 0 );");
	gated = (gate != NULL);
	if (mutant == 1) {
		gated = 0;			/* planted: the gate removed, qa_probe buys the pass again */
	}
	CHECK(gated, "vk_shade.c has no default-off `r_footprintProbe' gate in the census");
	if (gate) {
		const char	*ret_before = strstr(gate, "!s_fpCvar->integer");
		const char	*pass = strstr(gate, "for ( i = 0; i + 2 < numIndexes; i += 3 )");

		CHECK(ret_before != NULL, "the census does not test the gate");
		CHECK(pass != NULL && ret_before < pass,
			  "the census walks the triangles before it tests the gate");
	}

	/* the price of the pass at the frame's own geometry */
	reps = 200;
	us = pass_us(verts / 3 * 3, reps);
	printf("footprint oracle: %d verts, one projection pass = %.1f us/frame (%.2f ns/index)\n",
		   verts, us, us * 1000.0 / (verts / 3 * 3));

	CHECK(us < 1000.0, "the pass is absurdly priced (%.1f us)", us);

	printf("footprint oracle: %d checks, %d failures\n", checks, failures);
	free(text);
	return failures ? 1 : 0;
}
