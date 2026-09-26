/*
===========================================================================
This file is part of the port of Star Trek: Voyager -- Elite Force II, whose
engine descends from id Tech 3 through OpenMoHAA.

It is free software; you can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU General Public License, COPYING.txt at the repository
root, for more details.
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later
// Modified 2026-10-04 for this port; the changes are listed in PROVENANCE.tsv.

#ifndef __QA_PROBE_H__
#define __QA_PROBE_H__

#ifndef PROBE_GET
#error "define PROBE_GET(name,def,flags) before including qa_probe.h"
#endif
#ifndef PROBE_PRINT
#error "define PROBE_PRINT before including qa_probe.h"
#endif

/* `qa_probe' is the MASTER instrument switch: 0 silences every probe family,
 * 1 arms them all.  The family cvars (smp_frameStats, smp_loadStats, be_stats,
 * ui_stats, smp_serverStats, mem_churnStats, smp_vpProbe, smp_shaderProbe) are
 * fine-grained overrides that only matter while the master is on; every family
 * also answers to the master at its gate.  smp_texCache stays
 * 1 -- a feature. */
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
		v = PROBE_GET("qa_probe", "0", 0);
	}
	return (v && v->integer) ? qtrue : qfalse;
}
#endif

static inline qboolean PROBE_Throttle(int *counter, int first, int every)
{
	(*counter)++;
	return ((*counter <= first) || ((*counter % every) == 0)) ? qtrue : qfalse;
}

/* Each expansion site carries its own counter.  A streaming family (one row per
 * frame, per draw, per entity) wants the small window -- its tail is noise.  A
 * census wants the opposite: the class or the screen it was armed for is
 * exactly what the window hides.  PROBE_CENSUS_EMIT gives those sites the wide
 * window; both windows answer to the master qa_probe gate. */
#define PROBE_ROW_CAP 200
#define PROBE_ROW_CAP_WIDE 4096

#define PROBE_EMIT_N(prefix, tag, cap, ...) \
	do { \
		static int s_probeRows; \
		if (PROBE_On()) { \
			if (s_probeRows < (cap)) { \
				s_probeRows++; \
				PROBE_PRINT(prefix tag "] " __VA_ARGS__); \
			} else if (s_probeRows == (cap)) { \
				s_probeRows++; \
				PROBE_PRINT(prefix tag "] capped at %d rows; no further lines\n", \
				                 (cap)); \
			} \
		} \
	} while (0)

#define PROBE_EMIT(prefix, tag, ...) PROBE_EMIT_N(prefix, tag, PROBE_ROW_CAP, __VA_ARGS__)

#define PROBE_CENSUS_EMIT(prefix, tag, ...) PROBE_EMIT_N(prefix, tag, PROBE_ROW_CAP_WIDE, __VA_ARGS__)

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

#define LOADSTALL_PROBE(tag, ...) PROBE_EMIT("[LOADSTALL:", tag, __VA_ARGS__)

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

/* Layout/class/screen censuses: every screen a boot loads has to fit. */
#define UIVK_CENSUS_PROBE(tag, ...) PROBE_CENSUS_EMIT("[UIVK:", tag, __VA_ARGS__)

#define PAINT_CENSUS_PROBE(tag, ...) PROBE_CENSUS_EMIT("[PAINT:", tag, __VA_ARGS__)

#define WIDGET_CENSUS_PROBE(tag, ...) PROBE_CENSUS_EMIT("[WIDGET:", tag, __VA_ARGS__)

#endif 
