
#ifndef __QA_R120_H__
#define __QA_R120_H__

#ifndef R120_PROBE_GET
#error "define R120_PROBE_GET(name,def,flags) before including qa_r120.h"
#endif
#ifndef R120_PROBE_PRINT
#error "define R120_PROBE_PRINT before including qa_r120.h"
#endif

#ifdef R120_ON_OVERRIDE
static inline qboolean R120_On(void)
{
	return R120_ON_OVERRIDE ? qtrue : qfalse;
}
#else
static inline qboolean R120_On(void)
{
	static cvar_t *v;

	if (!v) {
		v = R120_PROBE_GET("qa_r120", "1", 0);
	}
	return (v && v->integer) ? qtrue : qfalse;
}
#endif

static inline qboolean R120_Throttle(int *counter, int first, int every)
{
	(*counter)++;
	return ((*counter <= first) || ((*counter % every) == 0)) ? qtrue : qfalse;
}

#define R1XX_PROBE_ROW_CAP 200

#define R1XX_EMIT(prefix, tag, ...) \
	do { \
		static int s_r1xxProbeRows; \
		if (R120_On()) { \
			if (s_r1xxProbeRows < R1XX_PROBE_ROW_CAP) { \
				s_r1xxProbeRows++; \
				R120_PROBE_PRINT(prefix tag "] " __VA_ARGS__); \
			} else if (s_r1xxProbeRows == R1XX_PROBE_ROW_CAP) { \
				s_r1xxProbeRows++; \
				R120_PROBE_PRINT(prefix tag "] capped at %d rows; no further lines\n", \
				                 R1XX_PROBE_ROW_CAP); \
			} \
		} \
	} while (0)

#define R120_PROBE(tag, ...) R1XX_EMIT("[R120:", tag, __VA_ARGS__)

#define R121_PROBE(tag, ...) R1XX_EMIT("[R121:", tag, __VA_ARGS__)

#define R124_PROBE(tag, ...) R1XX_EMIT("[R124:", tag, __VA_ARGS__)

#define R127_PROBE(tag, ...) R1XX_EMIT("[R127:", tag, __VA_ARGS__)

#define R128_PROBE(tag, ...) R1XX_EMIT("[R128:", tag, __VA_ARGS__)

#define R129_PROBE(tag, ...) R1XX_EMIT("[R129:", tag, __VA_ARGS__)

#define R130_PROBE(tag, ...) R1XX_EMIT("[R130:", tag, __VA_ARGS__)

#define R131_PROBE(tag, ...) R1XX_EMIT("[R131:", tag, __VA_ARGS__)

// happened.  These links use Com_Printf, which retail's reset does not touch.
#define R131M1_PROBE(tag, ...) R1XX_EMIT("[R131:M1:", tag, __VA_ARGS__)

#define R223_PROBE(tag, ...) R1XX_EMIT("[R223:", tag, __VA_ARGS__)

#include <stdlib.h>

static inline qboolean R133_EntTrashOn(void)
{
	static int r133_enttrash_cached = -1;

	if (r133_enttrash_cached == -1) {
		const char *r133_enttrash_env = getenv("EF2_ENTTRASH");

		r133_enttrash_cached = (r133_enttrash_env && *r133_enttrash_env) ? 1 : 0;
	}

	return r133_enttrash_cached ? qtrue : qfalse;
}
#define R133_PROBE(tag, ...) R1XX_EMIT("[R133:", tag, __VA_ARGS__)

#endif 
