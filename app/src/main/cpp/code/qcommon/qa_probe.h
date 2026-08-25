
#ifndef __QA_PROBE_H__
#define __QA_PROBE_H__

#ifndef PROBE_GET
#error "define PROBE_GET(name,def,flags) before including qa_probe.h"
#endif
#ifndef PROBE_PRINT
#error "define PROBE_PRINT before including qa_probe.h"
#endif

#ifdef PROBE_ON_OVERRIDE
static inline qboolean PROBE_On(void)
{
	return PROBE_ON_OVERRIDE ? qtrue : qfalse;
}
#else
static inline qboolean PROBE_On(void)
{
	static cvar_t *v;

	if (!v) {
		v = PROBE_GET("qa_probe", "1", 0);
	}
	return (v && v->integer) ? qtrue : qfalse;
}
#endif

static inline qboolean PROBE_Throttle(int *counter, int first, int every)
{
	(*counter)++;
	return ((*counter <= first) || ((*counter % every) == 0)) ? qtrue : qfalse;
}

#define PROBE_ROW_CAP 200

#define PROBE_EMIT(prefix, tag, ...) \
	do { \
		static int s_probeRows; \
		if (PROBE_On()) { \
			if (s_probeRows < PROBE_ROW_CAP) { \
				s_probeRows++; \
				PROBE_PRINT(prefix tag "] " __VA_ARGS__); \
			} else if (s_probeRows == PROBE_ROW_CAP) { \
				s_probeRows++; \
				PROBE_PRINT(prefix tag "] capped at %d rows; no further lines\n", \
				                 PROBE_ROW_CAP); \
			} \
		} \
	} while (0)

#define PROBE(tag, ...) PROBE_EMIT("[PROBE:", tag, __VA_ARGS__)

#define BOT_PROBE(tag, ...) PROBE_EMIT("[BOT:", tag, __VA_ARGS__)

#define TIKIANIM_PROBE(tag, ...) PROBE_EMIT("[TIKIANIM:", tag, __VA_ARGS__)

#define PAINT_PROBE(tag, ...) PROBE_EMIT("[PAINT:", tag, __VA_ARGS__)

#define CHAN_PROBE(tag, ...) PROBE_EMIT("[CHAN:", tag, __VA_ARGS__)

#define UIROWS_PROBE(tag, ...) PROBE_EMIT("[UIROWS:", tag, __VA_ARGS__)

#define UIRECT_PROBE(tag, ...) PROBE_EMIT("[UIRECT:", tag, __VA_ARGS__)

#define WIDGET_PROBE(tag, ...) PROBE_EMIT("[WIDGET:", tag, __VA_ARGS__)

// happened.  These links use Com_Printf, which retail's reset does not touch.
#define WIDGETM1_PROBE(tag, ...) PROBE_EMIT("[WIDGET:M1:", tag, __VA_ARGS__)

#define LOADHANG_PROBE(tag, ...) PROBE_EMIT("[LOADHANG:", tag, __VA_ARGS__)

#include <stdlib.h>

static inline qboolean UIVK_EntTrashOn(void)
{
	static int uivk_enttrash_cached = -1;

	if (uivk_enttrash_cached == -1) {
		const char *uivk_enttrash_env = getenv("EF2_ENTTRASH");

		uivk_enttrash_cached = (uivk_enttrash_env && *uivk_enttrash_env) ? 1 : 0;
	}

	return uivk_enttrash_cached ? qtrue : qfalse;
}
#define UIVK_PROBE(tag, ...) PROBE_EMIT("[UIVK:", tag, __VA_ARGS__)

#endif 
