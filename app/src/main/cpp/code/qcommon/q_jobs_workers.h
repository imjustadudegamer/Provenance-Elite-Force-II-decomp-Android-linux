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
#ifndef __QJOBS_WORKERS_H__
#define __QJOBS_WORKERS_H__

/* The array the pool is declared with, never the size the rule hands out. */
#define JOBPOOL_MAX_WORKERS			64

/* One rule for every machine.  The frame's own threads are reserved first --
   the main/sim thread and the render thread r_smp starts -- and every core
   left is a worker; nothing else is hardcoded, because the compositor and
   driver threads a device also runs are not a fixed count and the OS owns
   them.  The critical (renderer) pool and the non-critical (client) pool are
   separate modules' pools, so they split the headroom instead of each claiming
   it and oversubscribing the machine when a load runs both. */
static inline void JobPool_SplitWorkersRule( int coreCount, int isAndroid,
											 int requested, int critical,
											 int *outWorkers )
{
	int total, crit, share;

	(void)isAndroid;

	if ( !outWorkers ) {
		return;
	}

	total = coreCount - 2;
	if ( total < 0 ) {
		total = 0;
	}
	if ( total > JOBPOOL_MAX_WORKERS ) {
		total = JOBPOOL_MAX_WORKERS;
	}

	/* the renderer's pool takes the larger half so the walk's parallel ranges
	   keep the cores they need; the client's pool takes the rest.  An even
	   headroom halves exactly, so the renderer takes one more there: it is the
	   pool the frame blocks on, and the client's loads run between frames.  On
	   the smallest machines the two stay equal so a load is not starved. */
	crit = total - total / 2;
	if ( total >= 4 && ( total & 1 ) == 0 ) {
		crit++;
	}
	if ( crit > total ) {
		crit = total;
	}

	share = critical ? crit : ( total - crit );

	if ( requested == 0 ) {
		share = 0;
	} else if ( requested > 0 && requested < share ) {
		share = requested;
	}

	if ( share > JOBPOOL_MAX_WORKERS ) {
		share = JOBPOOL_MAX_WORKERS;
	}
	if ( share < 0 ) {
		share = 0;
	}

	*outWorkers = share;
}

#endif 
