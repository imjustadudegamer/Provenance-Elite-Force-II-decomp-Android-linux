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
#ifndef TR_VBO_IMM_H
#define TR_VBO_IMM_H

#include "tr_local.h"

#ifdef __cplusplus
extern "C" {
#endif

void RB_ClipPlane_Disable( void );
void RB_ClipPlane_Set( const vec4_t plane );
qboolean RB_ClipPlane_Get( vec4_t out );

void RB_LoadProjectionMatrix( const float *matrix );	
void RB_LoadModelViewMatrix( const float *matrix );
void RB_LoadIdentityModelView( void );
void RB_LoadIdentityProjection( void );
void RB_PushModelView( void );
void RB_PopModelView( void );
void RB_PushProjection( void );
void RB_PopProjection( void );
int  RB_ModelViewDepth( void );
int  RB_ProjectionDepth( void );
void RB_PopModelViewTo( int depth );
void RB_PopProjectionTo( int depth );
void RB_TranslateModelView( float x, float y, float z );
void RB_OrthoProjection( float left, float right, float bottom, float top, float zNear, float zFar );

void RB_ImmBegin( GLenum mode );
void RB_ImmVertex2f( float x, float y );
void RB_ImmVertex3f( float x, float y, float z );
void RB_ImmVertex3fv( const float *v );
void RB_ImmColor3f( float r, float g, float b );
void RB_ImmColor4f( float r, float g, float b, float a );
void RB_ImmTexCoord2f( float s, float t );
void RB_ImmTexCoord2fv( const float *st );
void RB_ImmEnd( void );
void RB_ImmFlush( void );

void RB_ImmEndDeferred( void );

#ifdef __cplusplus
}
#endif

#endif 
