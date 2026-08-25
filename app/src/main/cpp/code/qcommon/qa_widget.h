
#ifndef __QA_WIDGET_H__
#define __QA_WIDGET_H__

#ifndef WIDGET_PROBE_GET
#error "define WIDGET_PROBE_GET(name,def,flags) before including qa_widget.h"
#endif
#ifndef WIDGET_PROBE_PRINT
#error "define WIDGET_PROBE_PRINT before including qa_widget.h"
#endif

static inline qboolean WIDGET_On(void)
{
	static cvar_t *v;

	if (!v) {
		v = WIDGET_PROBE_GET("qa_probe", "1", 0);
	}
	return (v && v->integer) ? qtrue : qfalse;
}

static inline qboolean WIDGET_Throttle(int *counter, int first, int every)
{
	(*counter)++;
	return ((*counter <= first) || ((*counter % every) == 0)) ? qtrue : qfalse;
}

#define WIDGET_A1_PROBE(tag, ...) \
	do { \
		if (WIDGET_On()) { \
			WIDGET_PROBE_PRINT("[WIDGET:A1:" tag "] " __VA_ARGS__); \
		} \
	} while (0)

#define WIDGET_K1_PROBE(tag, ...) \
	do { \
		if (WIDGET_On()) { \
			WIDGET_PROBE_PRINT("[WIDGET:K1:" tag "] " __VA_ARGS__); \
		} \
	} while (0)

#endif 
