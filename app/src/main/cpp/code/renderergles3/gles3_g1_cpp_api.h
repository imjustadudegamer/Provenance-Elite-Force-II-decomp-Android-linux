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
#ifndef GLES3_GLES3_G1_CPP_API_H
#define GLES3_GLES3_G1_CPP_API_H

#include "tr_vbo.h"

#ifdef __cplusplus
extern "C" {
#endif

void	GLES3_ApplyStateBits( unsigned long stateBits );
void	GLES3_BeginWire( void );

#ifdef __cplusplus
}
#endif

#endif 
