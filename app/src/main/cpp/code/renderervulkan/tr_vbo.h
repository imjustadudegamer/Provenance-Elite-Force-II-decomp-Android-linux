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

/* the five attribute slots, the order the vertex lays out and the locations
 * the SPIR-V was compiled with */
enum {
	ATTR_POSITION  = 0,
	ATTR_NORMAL    = 1,
	ATTR_COLOR     = 2,
	ATTR_TEXCOORD0 = 3,
	ATTR_TEXCOORD1 = 4
};

/* the vertex the SPIR-V reads and the stream stores: position, normal, colour,
   two texcoord pairs.  It is published here because the static arena holds the
   same bytes the streaming pack writes, so both sides must agree on the layout. */
typedef struct {
	float	xyz[4];
	float	normal[4];
	byte	color[4];
	float	st[2][2];
} vkStreamVertex_t;

#define VK_STREAM_STRIDE ( (int)sizeof( vkStreamVertex_t ) )

/* One static surface as the draw sees it: where its vertexes sit in the
   device-local arena, where the batch placed them in the tess stream, and where
   its rebased index block lies in the device-local index arena. */
typedef struct {
	int	vertStart;
	int	numVerts;
	int	first;
	int	idxFirst;
	int	numIdx;
} vkStaticItemView_t;

/* The reference's MIN_IBO_RUN and the per-batch record bound, shared with the
   draw arm that has to size its item view. */
#define VK_STATIC_MIN_IBO_RUN	320
#define VK_STATIC_BATCH_MAX		4096

#ifdef __cplusplus
extern "C" {
#endif

qboolean RB_VBO_Init( void );
void RB_VBO_Shutdown( void );
/* Opens the frame's tessellation stream store, once per back end frame. */
void RB_VBO_BeginFrame( void );

void RB_VBO_DrawTessIndexed( const shaderCommands_t *input, const vkIndex_t *indexes, int numIndexes );
void RB_VBO_DrawTessIndexed_Stage( const shaderCommands_t *input, const vkIndex_t *indexes, int numIndexes,
								   const void *stage );
/* stage is the calling stage's identity: the surface's packed ring slice is
   reused by that surface's remaining stages and by nothing else */
void RB_VBO_DrawTessArrays( const shaderCommands_t *input,
							const vkIndex_t *indexes, int numIndexes,
							const float *texcoord0, const float *texcoord1, int tcStrideFloats,
							const unsigned char *colors,
							const float *normals,
							const void *stage );
void RB_VBO_DrawTess( const shaderCommands_t *input );

/* The per-batch surface log and the static arena behind it.  Record appends a
   surface whose generator ran this frame (its bytes are then in the streaming
   pack); TrySurface is the fast arm that skips the generator outright for a
   surface already resident in the arena. */
void RB_VboStaticReset( void );
void RB_VboStaticRecord( const void *surf, int vertStart, int numVerts, int idxStart, int numIdx );
void RB_VboStaticForget( void );

/* Fast arm: qtrue when the surface was taken straight out of the arena (its
   generator must not run).  Called where the draw-surf walk calls the surface
   generator, so it may close and reopen the batch on a class change. */
qboolean RB_VboStaticTrySurface( const void *surf, int type );

/* Validation for the draw: -1 when the batch cannot be drawn from the arena,
   else the item count, with the item views written to out (max entries). */
int RB_VboStaticBatchItems( vkStaticItemView_t *out, int max );

/* The batch the fast arm built.  True means its tess arrays were never written,
   so the draw must come from the arena and never from the streaming pack. */
qboolean RB_VboStaticArenaBatch( void );

/* Bake every recorded surface not resident yet from the batch's packed ring
   bytes at ringByteBase; the staging and device copies are recorded into cmd. */
void RB_VboStaticBakeFrame( int ringByteBase );

/* Draws the batch's arena items in index order: device-local runs of at least
   MIN_IBO_RUN in the device index buffer, every remaining short run grouped into
   one host-visible pass. */
void RB_VboStaticDrawRuns( VkCommandBuffer cmd, int numIndexes,
						   const vkStaticItemView_t *items, int numItems );

/* The arena's vertex bytes, so the probe census reads positions where the draw
   reads them instead of the tess arrays the fast arm never wrote. */
const float *RB_VboStaticStageFloats( void );

/* Items the arena has baked, arena bytes in use, and the refusals that made a
   surface stream instead: one row a second on the boundary that ends it. */
void RB_VboStaticRow( void );

#include "tr_vbo_imm.h"

const float *RB_GetModelViewMatrix( void );
const float *RB_GetProjectionMatrix( void );

extern int r_spriteListOpen;

#ifdef __cplusplus
}
#endif

#endif
