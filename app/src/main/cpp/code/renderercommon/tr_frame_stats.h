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

#ifndef TR_FRAME_STATS_H
#define TR_FRAME_STATS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Sample one presented frame.  Reporting is armed by r_perfFps: 0 is silent,
 * N > 0 prints a summary row every N seconds. */
void R_FrameStats_Present( const char *backend, const char *present,
						   int vsync, int frontEndMsec, int backEndMsec );

/* Drop the window and the last present stamp. */
void R_FrameStats_Reset( void );

#ifdef __cplusplus
}
#endif

#endif
