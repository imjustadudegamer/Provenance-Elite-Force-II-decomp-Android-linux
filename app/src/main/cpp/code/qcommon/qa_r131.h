
#ifndef __QA_R131_H__
#define __QA_R131_H__

#ifndef R131_PROBE_GET
#error "define R131_PROBE_GET(name,def,flags) before including qa_r131.h"
#endif
#ifndef R131_PROBE_PRINT
#error "define R131_PROBE_PRINT before including qa_r131.h"
#endif

static inline qboolean R131_On(void)
{
	static cvar_t *v;

	if (!v) {
		v = R131_PROBE_GET("qa_r120", "1", 0);
	}
	return (v && v->integer) ? qtrue : qfalse;
}

static inline qboolean R131_Throttle(int *counter, int first, int every)
{
	(*counter)++;
	return ((*counter <= first) || ((*counter % every) == 0)) ? qtrue : qfalse;
}

#define R131_A1_PROBE(tag, ...) \
	do { \
		if (R131_On()) { \
			R131_PROBE_PRINT("[R131:A1:" tag "] " __VA_ARGS__); \
		} \
	} while (0)

#define R131_K1_PROBE(tag, ...) \
	do { \
		if (R131_On()) { \
			R131_PROBE_PRINT("[R131:K1:" tag "] " __VA_ARGS__); \
		} \
	} while (0)

#endif 
