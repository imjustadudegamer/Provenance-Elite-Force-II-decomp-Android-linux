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
/* tr_vbo.h -- the tessellation stream's interface, this module's shape of the
 * reference renderer's header.  Same function names, same call sites: the difference
 * is what the bodies do, and that the legacy-only surface (vertex-array binds, buffer
 * targets) has no counterpart here and is not declared. */
#ifndef TR_VBO_H
#define TR_VBO_H

#include "tr_local.h"

/* the five attribute slots, the order the 48-byte vertex lays out and the
 * locations the SPIR-V was compiled with */
enum {
	ATTR_POSITION  = 0,
	ATTR_NORMAL    = 1,
	ATTR_COLOR     = 2,
	ATTR_TEXCOORD0 = 3,
	ATTR_TEXCOORD1 = 4
};

#ifdef __cplusplus
extern "C" {
#endif

void RB_VBO_Init( void );
void RB_VBO_Shutdown( void );

/* Opens the frame's tessellation stream store, once per back end frame. */
void RB_VBO_BeginFrame( void );

void RB_VBO_DrawTessIndexed( const shaderCommands_t *input, const vkIndex_t *indexes, int numIndexes );
void RB_VBO_DrawTessArrays( const shaderCommands_t *input,
							const vkIndex_t *indexes, int numIndexes,
							const float *texcoord0, const float *texcoord1, int tcStrideFloats,
							const unsigned char *colors,
							const float *normals );
void RB_VBO_DrawTess( const shaderCommands_t *input );

/* The per-batch surface log; no static arena behind it yet, so a recorded
 * batch is the record that a batch was seen. */
void RB_VboStaticReset( void );
void RB_VboStaticRecord( const void *surf, int vertStart, int numVerts, int idxStart, int numIdx );
void RB_VboStaticForget( void );

#include "tr_vbo_imm.h"

const float *RB_GetModelViewMatrix( void );
const float *RB_GetProjectionMatrix( void );

extern int r_spriteListOpen;

#ifdef __cplusplus
}
#endif

#endif
