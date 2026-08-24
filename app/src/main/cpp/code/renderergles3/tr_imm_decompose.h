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
#ifndef EF2_RB_IMM_DECOMPOSE_H
#define EF2_RB_IMM_DECOMPOSE_H

#ifndef GL_POINTS
#define GL_POINTS			0x0000
#endif
#ifndef GL_LINES
#define GL_LINES			0x0001
#endif
#ifndef GL_LINE_LOOP
#define GL_LINE_LOOP		0x0002
#endif
#ifndef GL_LINE_STRIP
#define GL_LINE_STRIP		0x0003
#endif
#ifndef GL_TRIANGLES
#define GL_TRIANGLES		0x0004
#endif
#ifndef GL_TRIANGLE_STRIP
#define GL_TRIANGLE_STRIP	0x0005
#endif
#ifndef GL_TRIANGLE_FAN
#define GL_TRIANGLE_FAN		0x0006
#endif
#ifndef GL_QUADS
#define GL_QUADS			0x0007
#endif
#ifndef GL_QUAD_STRIP
#define GL_QUAD_STRIP		0x0008
#endif
#ifndef GL_POLYGON
#define GL_POLYGON			0x0009
#endif

static inline int RB_ImmModeIsDirect( unsigned int mode )
{
	switch ( mode ) {
	case GL_POINTS:
	case GL_LINES:
	case GL_LINE_LOOP:
	case GL_LINE_STRIP:
	case GL_TRIANGLES:
	case GL_TRIANGLE_STRIP:
	case GL_TRIANGLE_FAN:
		return 1;
	default:
		return 0;
	}
}

static inline int RB_ImmBuildIndices( unsigned int mode, unsigned int first,
									  int count, unsigned int *out )
{
	int				n = 0;
	int				i;
	unsigned int	a, b, c, d;

	if ( count <= 0 ) {
		return 0;
	}

	if ( mode == GL_QUADS ) {
		for ( i = 0; i + 4 <= count; i += 4 ) {
			a = first + (unsigned int)i;
			b = a + 1;
			c = a + 2;
			d = a + 3;
			out[n++] = a; out[n++] = b; out[n++] = c;
			out[n++] = a; out[n++] = c; out[n++] = d;
		}
		return n;
	}

	if ( mode == GL_QUAD_STRIP ) {
		for ( i = 0; i + 4 <= count; i += 2 ) {
			a = first + (unsigned int)i;
			b = a + 1;
			c = a + 2;
			d = a + 3;
			out[n++] = a; out[n++] = b; out[n++] = d;
			out[n++] = a; out[n++] = d; out[n++] = c;
		}
		return n;
	}

	if ( mode == GL_POLYGON ) {
		a = first;
		for ( i = 1; i + 1 < count; i++ ) {
			out[n++] = a;
			out[n++] = first + (unsigned int)i;
			out[n++] = first + (unsigned int)i + 1;
		}
		return n;
	}

	return 0;
}

#define RB_IMM_INDEX_CAPACITY(count)	(3 * (count) + 8)

#endif 
