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

#define JOBPOOL_MAX_WORKERS			7
#define JOBPOOL_MAX_TOTAL			7
#define JOBPOOL_CRITICAL_ANDROID	3
#define JOBPOOL_CRITICAL_OTHER		4

static inline void JobPool_SplitWorkersRule( int coreCount, int isAndroid,
											 int requested, int critical,
											 int *outWorkers )
{
	int total, share, crit;

	if ( !outWorkers ) {
		return;
	}

	total = coreCount - 1;
	if ( total > JOBPOOL_MAX_TOTAL ) {
		total = JOBPOOL_MAX_TOTAL;
	}
	if ( total < 0 ) {
		total = 0;
	}

	crit = ( isAndroid ? JOBPOOL_CRITICAL_ANDROID : JOBPOOL_CRITICAL_OTHER );
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
