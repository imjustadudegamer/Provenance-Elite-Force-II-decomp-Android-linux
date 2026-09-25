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
/*
 * vk_m3_surface.c -- the 3D stage iterators' reserved seats.
 *
 * The shader compiler hands every shader an `optimalStageIteratorFunc`, so the
 * data model names the 3D iterators before they draw anything.  These bodies
 * are the guard, not a silent stub: reaching one prints a [VK:guard] row that
 * names the site.  M3 replaces each body with its real walk; nothing in the
 * M2 boot path reaches them, because RE_RenderScene never builds a 3D scene
 * in this milestone and logs its own [VK:stub] row when called.
 */

#include "tr_local.h"

#include <time.h>

/* Named in the header: the back end reaches it for a shader that carries 3D
   terms the 2D walk does not evaluate. */
void VK_M3_Touched( const char *what )
{
	static int rows;

	if ( rows < 8 || ( rows % 600 ) == 0 ) {
		VK_Guard_Hit( VK_GSITE_QUIRK, what, VK_INCOMPLETE );
	}
	rows++;
}























/* ---- the module globals the data model names ----------------------------- */

qboolean haveClampToEdge = qtrue;
float	displayAspect = 4.0f / 3.0f;
int	smp_texTranscode = 0;
qboolean smp_hasAstcLdr = qfalse;
qboolean smp_hasAstcSliced3d = qfalse;
qboolean smp_hasEtc2Core = qfalse;

qboolean R_ProbeStatsOn( void )
{
	static cvar_t *probe;

	if ( !probe ) {
		probe = ri.Cvar_Get( "qa_probe", "0", 0 );
	}

	return ( probe && probe->integer ) ? qtrue : qfalse;
}

/* The renderer's own clock.  It was `ri.Milliseconds() * 1000`, which is a
   millisecond value scaled: every `usec=` row it feeds (the sync waits, the 2D
   phases, the upload prices) was therefore quantised to a millisecond, and a
   cost under one read as zero.  A stage bind is far below that, so the walk's
   attribution had no resolution where the time actually goes.  CLOCK_MONOTONIC
   is the real microsecond source; the 2^32 us range wraps in ~71 minutes and
   every caller reads a difference, so the truncation is the engine's own
   Milliseconds idiom, not a new trap. */
int BE_Microseconds( void )
{
	struct timespec ts;

	if ( clock_gettime( CLOCK_MONOTONIC, &ts ) != 0 ) {
		return ri.Milliseconds() * 1000;
	}

	return (int)( (unsigned)( (unsigned long long)ts.tv_sec * 1000000ULL
							  + (unsigned long long)ts.tv_nsec / 1000ULL ) );
}

/* The same instant without the int truncation: the 32-bit form goes negative
   after 2^31 us of uptime, which is a legal reading its callers' "-1 = not
   timed" sentinels cannot tell from "not timed". */
long long BE_Microseconds64( void )
{
	struct timespec ts;

	if ( clock_gettime( CLOCK_MONOTONIC, &ts ) != 0 ) {
		return (long long)ri.Milliseconds() * 1000LL;
	}

	return (long long)ts.tv_sec * 1000000LL + (long long)ts.tv_nsec / 1000LL;
}

int BE_Armed( void )
{
	return R_ProbeStatsOn() ? 1 : 0;
}


/* the ghost overlay surface: the image layer names it, M3 draws it */






qboolean VKimp_GLMainOwnsContext( void )
{
	/* No context to own: only the render thread records, and the front end
	   reaches Vulkan only through R_SyncRenderThread, which parks it. */
	return qfalse;
}
