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
#ifndef TR_VBO_H
#define TR_VBO_H

#include "tr_local.h"

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

void RB_VBO_DrawTessIndexed( const shaderCommands_t *input, const glIndex_t *indexes, int numIndexes );
void RB_VBO_DrawTessArrays( const shaderCommands_t *input,
							const glIndex_t *indexes, int numIndexes,
							const float *texcoord0, const float *texcoord1, int tcStrideFloats,
							const unsigned char *colors,
							const float *normals );
void RB_VBO_DrawTess( const shaderCommands_t *input );

/* The static world arena's per-batch surface log.  Reset at RB_BeginSurface and
 * appended by the world surface functions; the draw path reads it. */
void RB_VboStaticReset( void );
void RB_VboStaticRecord( const void *surf, int vertStart, int numVerts, int idxStart, int numIdx );

void RB_VBO_DecalAttribProbe( int *enabled, int *size, int *stride, int *type,
							  int *normalized, const void **pointer );

#include "tr_vbo_imm.h"

const float *RB_GetModelViewMatrix( void );
const float *RB_GetProjectionMatrix( void );

extern GLuint r_vaoBound;
extern int r_spriteListOpen;
void RB_BindVAO( GLuint vao );
void RB_BindBuffer( GLenum target, GLuint buffer );

#ifdef __cplusplus
}
#endif

#endif 
