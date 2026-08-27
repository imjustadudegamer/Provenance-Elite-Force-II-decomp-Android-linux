/*
===========================================================================
Copyright (C) 2007-2009 Robert Beckebans <trebor_7@users.sourceforge.net>

This file is part of XreaL source code.

XreaL source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

XreaL source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with XreaL source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later
// Modified 2026-10-04 for this port; the changes are listed in PROVENANCE.tsv.
#include "tr_local.h"
#include "tr_vbo.h"
#include "tr_imm_decompose.h"

typedef struct {
	float			xyz[4];		
	float			normal[4];	
	unsigned char	color[4];	
	float			st0[2];		
	float			st1[2];		
} rbVertex_t;					

#define RB_VERTEX_STRIDE	((GLsizei)sizeof( rbVertex_t ))

static GLuint	s_vaoTess;
static GLuint	s_vaoStream;

int r_spriteListOpen = 0;

GLuint r_vaoBound = 0;

/*
 * A buffer bind is only worth a driver call when it changes the binding.  The element
 * array binding is VAO state, so a VAO switch drops that half of the cache.
 */
static GLuint	s_arrayBufferBound = (GLuint)~0u;
static GLuint	s_elementBufferBound = (GLuint)~0u;

void RB_BindBuffer( GLenum target, GLuint buffer ) {
	if ( target == GL_ARRAY_BUFFER ) {
		if ( s_arrayBufferBound == buffer ) {
			return;
		}
		s_arrayBufferBound = buffer;
	} else if ( target == GL_ELEMENT_ARRAY_BUFFER ) {
		if ( s_elementBufferBound == buffer ) {
			return;
		}
		s_elementBufferBound = buffer;
	} else {
		qglBindBuffer( target, buffer );
		return;
	}
	qglBindBuffer( target, buffer );
}

void RB_BindVAO( GLuint vao ) {
	if ( r_vaoBound != vao ) {
		qglBindVertexArray( vao );
		r_vaoBound = vao;
		s_elementBufferBound = (GLuint)~0u;
	}
}
static GLuint	s_vboTess;
static GLuint	s_eboTess;
static GLuint	s_vboStream;
static GLuint	s_eboStream;

static qboolean	s_vboInitialised;

static rbVertex_t	s_tessStage[SHADER_MAX_VERTEXES];

#define RB_STREAM_MAX_VERTS	(SHADER_MAX_VERTEXES * 4)
#define RB_STREAM_MAX_RECS	4096

typedef struct {
	GLenum		mode;		
	int			first;		
	int			count;		
	int			idxFirst;	
	int			idxCount;	
} rbStreamRec_t;

static rbVertex_t		s_streamVerts[RB_STREAM_MAX_VERTS];
static rbStreamRec_t	s_streamRecs[RB_STREAM_MAX_RECS];
static unsigned int		s_streamIndices[RB_IMM_INDEX_CAPACITY( RB_STREAM_MAX_VERTS )];

static int		s_streamNumVerts;
static int		s_streamNumRecs;
static int		s_streamOpenRec;	

static float	s_streamColor[4] = { 1, 1, 1, 1 };
static float	s_streamST[2];

static void RB_ImmResetCurrent( void );	

static void RB_VBO_ConfigureAttribs( void )
{
	qglEnableVertexAttribArray( ATTR_POSITION );
	qglVertexAttribPointer( ATTR_POSITION, 4, GL_FLOAT, GL_FALSE, RB_VERTEX_STRIDE, ( const void * )0 );

	qglEnableVertexAttribArray( ATTR_NORMAL );
	qglVertexAttribPointer( ATTR_NORMAL, 4, GL_FLOAT, GL_FALSE, RB_VERTEX_STRIDE, ( const void * )16 );

	qglEnableVertexAttribArray( ATTR_COLOR );
	qglVertexAttribPointer( ATTR_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, RB_VERTEX_STRIDE, ( const void * )32 );

	qglEnableVertexAttribArray( ATTR_TEXCOORD0 );
	qglVertexAttribPointer( ATTR_TEXCOORD0, 2, GL_FLOAT, GL_FALSE, RB_VERTEX_STRIDE, ( const void * )36 );

	qglEnableVertexAttribArray( ATTR_TEXCOORD1 );
	qglVertexAttribPointer( ATTR_TEXCOORD1, 2, GL_FLOAT, GL_FALSE, RB_VERTEX_STRIDE, ( const void * )44 );
}

void RB_VBO_Init( void )
{
	if ( s_vboInitialised ) {
		return;
	}

	qglGenBuffers( 1, &s_vboTess );
	qglGenBuffers( 1, &s_eboTess );
	qglGenBuffers( 1, &s_vboStream );
	qglGenBuffers( 1, &s_eboStream );

	qglGenVertexArrays( 1, &s_vaoTess );
	RB_BindVAO( s_vaoTess );
	RB_BindBuffer( GL_ARRAY_BUFFER, s_vboTess );
	RB_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_eboTess );
	RB_VBO_ConfigureAttribs();

	qglGenVertexArrays( 1, &s_vaoStream );
	RB_BindVAO( s_vaoStream );
	RB_BindBuffer( GL_ARRAY_BUFFER, s_vboStream );
	RB_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_eboStream );
	RB_VBO_ConfigureAttribs();


	RB_ImmResetCurrent();

	s_vboInitialised = qtrue;
}

void RB_VBO_Shutdown( void )
{
	if ( !s_vboInitialised ) {
		return;
	}

	qglDeleteBuffers( 1, &s_vboTess );
	qglDeleteBuffers( 1, &s_eboTess );
	qglDeleteBuffers( 1, &s_vboStream );
	qglDeleteBuffers( 1, &s_eboStream );
	qglDeleteVertexArrays( 1, &s_vaoTess );
	qglDeleteVertexArrays( 1, &s_vaoStream );

	s_vboInitialised = qfalse;
}


void RB_VBO_DrawTessArrays( const shaderCommands_t *input,
							const glIndex_t *indexes, int numIndexes,
							const float *texcoord0, const float *texcoord1, int tcStrideFloats,
							const unsigned char *colors,
							const float *normals )
{
	int		i;
	rbVertex_t	*dst;

	if ( !s_vboInitialised ) {
		RB_VBO_Init();
	}

	if ( numIndexes <= 0 || input->numVertexes <= 0 ) {
		return;
	}

	BE_SUB( BE_FAM_DS, BE_DSZ_sub_vbo, {
		dst = s_tessStage;
		for ( i = 0; i < input->numVertexes; i++, dst++ ) {
			const float *src;

			src = input->xyz[i];
			dst->xyz[0] = src[0];
			dst->xyz[1] = src[1];
			dst->xyz[2] = src[2];
			dst->xyz[3] = src[3];

			if ( normals ) {
				src = normals + i * 4;
				dst->normal[0] = src[0];
				dst->normal[1] = src[1];
				dst->normal[2] = src[2];
				dst->normal[3] = src[3];
			} else {
				dst->normal[0] = input->xyz[i][0];
				dst->normal[1] = input->xyz[i][1];
				dst->normal[2] = input->xyz[i][2];
				dst->normal[3] = 1.0f;
			}

			if ( colors ) {
				dst->color[0] = colors[i * 4 + 0];
				dst->color[1] = colors[i * 4 + 1];
				dst->color[2] = colors[i * 4 + 2];
				dst->color[3] = colors[i * 4 + 3];
			} else {
				dst->color[0] = dst->color[1] = dst->color[2] = dst->color[3] = 255;
			}

			if ( texcoord0 ) {
				dst->st0[0] = texcoord0[i * tcStrideFloats + 0];
				dst->st0[1] = texcoord0[i * tcStrideFloats + 1];
			} else {
				dst->st0[0] = dst->st0[1] = 0.0f;
			}
			if ( texcoord1 ) {
				dst->st1[0] = texcoord1[i * tcStrideFloats + 0];
				dst->st1[1] = texcoord1[i * tcStrideFloats + 1];
			} else {
				dst->st1[0] = dst->st1[1] = 0.0f;
			}
		}

		RB_BindVAO( s_vaoTess );
		RB_BindBuffer( GL_ARRAY_BUFFER, s_vboTess );
		qglBufferData( GL_ARRAY_BUFFER,
					   ( GLsizeiptr )( ( size_t )input->numVertexes * RB_VERTEX_STRIDE ),
					   s_tessStage, GL_STREAM_DRAW );

		RB_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_eboTess );
		qglBufferData( GL_ELEMENT_ARRAY_BUFFER,
					   ( GLsizeiptr )( ( size_t )numIndexes * sizeof( glIndex_t ) ),
					   indexes, GL_STREAM_DRAW );

		qglDrawElements( GL_TRIANGLES, numIndexes, GL_UNSIGNED_INT, ( const void * )0 );
		BE_Bump( BE_FAM_BE, BE_BEC_draws, 1 );
		BE_Bump( BE_FAM_DS, BE_DSC_draws, 1 );
		if ( r_spriteListOpen ) {
			BE_Bump( BE_FAM_BE, BE_BEC_sprDraw, 1 );
		}
	} );
}

void RB_VBO_DrawTessIndexed( const shaderCommands_t *input, const glIndex_t *indexes, int numIndexes )
{
	RB_VBO_DrawTessArrays( input, indexes, numIndexes,
						   ( const float * )input->svars.texcoords[0],
						   ( const float * )input->svars.texcoords[1], 2,
						   ( const unsigned char * )input->svars.colors,
						   input->normal[0] );
}

void RB_VBO_DrawTess( const shaderCommands_t *input )
{
	RB_VBO_DrawTessIndexed( input, input->indexes, input->numIndexes );
}

void RB_VBO_DecalAttribProbe( int *enabled, int *size, int *stride, int *type,
							  int *normalized, const void **pointer )
{
	if ( !s_vboInitialised ) {
		RB_VBO_Init();
	}

	RB_BindVAO( s_vaoTess );

	qglGetVertexAttribiv( ATTR_COLOR, GL_VERTEX_ATTRIB_ARRAY_ENABLED,    enabled );
	qglGetVertexAttribiv( ATTR_COLOR, GL_VERTEX_ATTRIB_ARRAY_SIZE,       size );
	qglGetVertexAttribiv( ATTR_COLOR, GL_VERTEX_ATTRIB_ARRAY_TYPE,       type );
	qglGetVertexAttribiv( ATTR_COLOR, GL_VERTEX_ATTRIB_ARRAY_STRIDE,     stride );
	qglGetVertexAttribiv( ATTR_COLOR, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, normalized );
	qglGetVertexAttribPointerv( ATTR_COLOR, GL_VERTEX_ATTRIB_ARRAY_POINTER, (void **)pointer );

	RB_BindVAO( 0 );
}


static void RB_ImmResetCurrent( void )
{
	s_streamColor[0] = s_streamColor[1] = s_streamColor[2] = s_streamColor[3] = 1.0f;
	s_streamST[0] = s_streamST[1] = 0.0f;
}

void RB_ImmFlush( void )
{
	int		i;
	int		nIndices;

	if ( s_streamNumVerts == 0 ) {
		s_streamNumRecs = 0;
		s_streamOpenRec = -1;
		return;
	}

	if ( !s_vboInitialised ) {
		RB_VBO_Init();
	}

	RB_BindVAO( s_vaoStream );
	RB_BindBuffer( GL_ARRAY_BUFFER, s_vboStream );
	qglBufferData( GL_ARRAY_BUFFER,
				   ( GLsizeiptr )( ( size_t )s_streamNumVerts * RB_VERTEX_STRIDE ),
				   s_streamVerts, GL_STREAM_DRAW );

	nIndices = 0;
	for ( i = 0; i < s_streamNumRecs; i++ ) {
		rbStreamRec_t	*rec = &s_streamRecs[i];
		int				n;

		rec->idxFirst = 0;
		rec->idxCount = 0;

		if ( rec->count <= 0 || RB_ImmModeIsDirect( rec->mode ) ) {
			continue;
		}
		if ( nIndices >= RB_IMM_INDEX_CAPACITY( RB_STREAM_MAX_VERTS ) ) {
			break;
		}

		n = RB_ImmBuildIndices( rec->mode, ( unsigned int )rec->first, rec->count,
								s_streamIndices + nIndices );
		if ( n > 0 ) {
			rec->idxFirst = nIndices;
			rec->idxCount = n;
			nIndices += n;
		}
	}

	if ( nIndices > 0 ) {
		RB_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_eboStream );
		qglBufferData( GL_ELEMENT_ARRAY_BUFFER,
					   ( GLsizeiptr )( ( size_t )nIndices * sizeof( unsigned int ) ),
					   s_streamIndices, GL_STREAM_DRAW );
	}

	for ( i = 0; i < s_streamNumRecs; i++ ) {
		const rbStreamRec_t	*rec = &s_streamRecs[i];

		if ( rec->count <= 0 ) {
			continue;
		}

		if ( rec->idxCount > 0 ) {
			qglDrawElements( GL_TRIANGLES, rec->idxCount, GL_UNSIGNED_INT,
							 ( const void * )( ( size_t )rec->idxFirst * sizeof( unsigned int ) ) );
			continue;
		}

		if ( !RB_ImmModeIsDirect( rec->mode ) ) {
			static qboolean	s_warned[1];
			if ( !s_warned[0] ) {
				s_warned[0] = qtrue;
				ri.Printf( PRINT_ALL, "RB_ImmFlush: dropped a 0x%04x primitive range of %d vertex(es) "
									 "(no ES 3.0 equivalent, malformed for its mode)\n",
									 (unsigned)rec->mode, rec->count );
			}
			continue;
		}

		qglDrawArrays( rec->mode, rec->first, rec->count );
		BE_Bump( BE_FAM_BE, BE_BEC_draws, 1 );
	}


	s_streamNumVerts = 0;
	s_streamNumRecs = 0;
	s_streamOpenRec = -1;
}

void RB_ImmBegin( GLenum mode )
{
	if ( s_streamNumRecs >= RB_STREAM_MAX_RECS ||
		 s_streamNumVerts >= RB_STREAM_MAX_VERTS - 4 ) {
		RB_ImmFlush();
	}

	// NOTE: the current colour/texcoord are NOT reset here.  In the

	s_streamOpenRec = s_streamNumRecs++;
	s_streamRecs[s_streamOpenRec].mode = mode;
	s_streamRecs[s_streamOpenRec].first = s_streamNumVerts;
	s_streamRecs[s_streamOpenRec].count = 0;
}

static rbVertex_t *RB_ImmNewVertex( void )
{
	rbVertex_t	*v;

	if ( s_streamOpenRec < 0 ) {
		return NULL;
	}
	if ( s_streamNumVerts >= RB_STREAM_MAX_VERTS ) {
		return NULL;
	}

	v = &s_streamVerts[s_streamNumVerts++];
	s_streamRecs[s_streamOpenRec].count++;

	v->xyz[3] = 1.0f;
	v->normal[0] = 0.0f;
	v->normal[1] = 0.0f;
	v->normal[2] = 1.0f;
	v->normal[3] = 1.0f;
	v->color[0] = ( unsigned char )( s_streamColor[0] * 255.0f );
	v->color[1] = ( unsigned char )( s_streamColor[1] * 255.0f );
	v->color[2] = ( unsigned char )( s_streamColor[2] * 255.0f );
	v->color[3] = ( unsigned char )( s_streamColor[3] * 255.0f );
	v->st0[0] = s_streamST[0];
	v->st0[1] = s_streamST[1];
	v->st1[0] = s_streamST[0];
	v->st1[1] = s_streamST[1];
	return v;
}

void RB_ImmVertex2f( float x, float y )
{
	rbVertex_t	*v = RB_ImmNewVertex();

	if ( v ) {
		v->xyz[0] = x;
		v->xyz[1] = y;
		v->xyz[2] = 0.0f;
	}
}

void RB_ImmVertex3f( float x, float y, float z )
{
	rbVertex_t	*v = RB_ImmNewVertex();

	if ( v ) {
		v->xyz[0] = x;
		v->xyz[1] = y;
		v->xyz[2] = z;
	}
}

void RB_ImmVertex3fv( const float *v3 )
{
	RB_ImmVertex3f( v3[0], v3[1], v3[2] );
}

void RB_ImmColor3f( float r, float g, float b )
{
	RB_ImmColor4f( r, g, b, 1.0f );
}

void RB_ImmColor4f( float r, float g, float b, float a )
{
	s_streamColor[0] = r;
	s_streamColor[1] = g;
	s_streamColor[2] = b;
	s_streamColor[3] = a;
}

void RB_ImmTexCoord2f( float s, float t )
{
	s_streamST[0] = s;
	s_streamST[1] = t;
}

void RB_ImmTexCoord2fv( const float *st )
{
	s_streamST[0] = st[0];
	s_streamST[1] = st[1];
}

void RB_ImmEnd( void )
{
	s_streamOpenRec = -1;

	RB_ImmFlush();
}

void RB_ImmEndDeferred( void )
{
	s_streamOpenRec = -1;
}

static qboolean	s_clipPlaneEnabled;
static vec4_t	s_clipPlane;

void RB_ClipPlane_Disable( void )
{
	s_clipPlaneEnabled = qfalse;
}

void RB_ClipPlane_Set( const vec4_t plane )
{
	s_clipPlane[0] = plane[0];
	s_clipPlane[1] = plane[1];
	s_clipPlane[2] = plane[2];
	s_clipPlane[3] = plane[3];
	s_clipPlaneEnabled = qtrue;
}

qboolean RB_ClipPlane_Get( vec4_t out )
{
	if ( !s_clipPlaneEnabled ) {
		return qfalse;
	}
	out[0] = s_clipPlane[0];
	out[1] = s_clipPlane[1];
	out[2] = s_clipPlane[2];
	out[3] = s_clipPlane[3];
	return qtrue;
}

#define RB_MATRIX_STACK_DEPTH	8

static float	s_modelViewMatrix[16];
static float	s_projectionMatrix[16];

static float	s_modelViewStack[RB_MATRIX_STACK_DEPTH][16];
static float	s_projectionStack[RB_MATRIX_STACK_DEPTH][16];
static int		s_modelViewDepth;
static int		s_projectionDepth;

static void RB_MatrixIdentity( float *m )
{
	int		i;

	for ( i = 0; i < 16; i++ ) {
		m[i] = 0.0f;
	}
	m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void RB_LoadProjectionMatrix( const float *matrix )
{
	Com_Memcpy( s_projectionMatrix, matrix, sizeof( s_projectionMatrix ) );
}

void RB_LoadModelViewMatrix( const float *matrix )
{
	Com_Memcpy( s_modelViewMatrix, matrix, sizeof( s_modelViewMatrix ) );
}

void RB_LoadIdentityModelView( void )
{
	RB_MatrixIdentity( s_modelViewMatrix );
}

void RB_LoadIdentityProjection( void )
{
	RB_MatrixIdentity( s_projectionMatrix );
}

/*
 * The saved-matrix copies are fortify-checked and the bound the compiler derives
 * for the destination is the whole-array remainder, sizeof(stack) - 16*depth,
 * which is zero for every depth outside [0, RB_MATRIX_STACK_DEPTH).  A one-ended
 * test does not bound that copy: a negative depth satisfies a signed upper test,
 * the remainder collapses to zero, and the abort lands in __memcpy_chk.  Each
 * accessor therefore tests the index as unsigned.
 */
void RB_PushModelView( void )
{
	unsigned int	depth = (unsigned int)s_modelViewDepth;

	if ( depth >= (unsigned int)RB_MATRIX_STACK_DEPTH ) {
		return;
	}
	Com_Memcpy( s_modelViewStack[depth], s_modelViewMatrix, sizeof( s_modelViewMatrix ) );
	s_modelViewDepth = (int)( depth + 1u );
}

void RB_PopModelView( void )
{
	unsigned int	depth = (unsigned int)s_modelViewDepth;

	if ( depth == 0u || depth > (unsigned int)RB_MATRIX_STACK_DEPTH ) {
		return;
	}
	depth--;
	s_modelViewDepth = (int)depth;
	Com_Memcpy( s_modelViewMatrix, s_modelViewStack[depth], sizeof( s_modelViewMatrix ) );
}

void RB_PushProjection( void )
{
	unsigned int	depth = (unsigned int)s_projectionDepth;

	if ( depth >= (unsigned int)RB_MATRIX_STACK_DEPTH ) {
		return;
	}
	Com_Memcpy( s_projectionStack[depth], s_projectionMatrix, sizeof( s_projectionMatrix ) );
	s_projectionDepth = (int)( depth + 1u );
}

void RB_PopProjection( void )
{
	unsigned int	depth = (unsigned int)s_projectionDepth;

	if ( depth == 0u || depth > (unsigned int)RB_MATRIX_STACK_DEPTH ) {
		return;
	}
	depth--;
	s_projectionDepth = (int)depth;
	Com_Memcpy( s_projectionMatrix, s_projectionStack[depth], sizeof( s_projectionMatrix ) );
}

/*
 * A caller that saves both matrices restores to the depth it measured before
 * saving, so a push that was refused inside the scope cannot be matched by a
 * pop, and the depth and the saved stack cannot drift apart.
 */
int RB_ModelViewDepth( void )
{
	return s_modelViewDepth;
}

int RB_ProjectionDepth( void )
{
	return s_projectionDepth;
}

void RB_PopModelViewTo( int depth )
{
	if ( depth < 0 || depth > s_modelViewDepth ) {
		return;
	}
	while ( s_modelViewDepth > depth ) {
		RB_PopModelView();
	}
}

void RB_PopProjectionTo( int depth )
{
	if ( depth < 0 || depth > s_projectionDepth ) {
		return;
	}
	while ( s_projectionDepth > depth ) {
		RB_PopProjection();
	}
}

void RB_TranslateModelView( float x, float y, float z )
{
	s_modelViewMatrix[12] += s_modelViewMatrix[0] * x + s_modelViewMatrix[4] * y + s_modelViewMatrix[8]  * z;
	s_modelViewMatrix[13] += s_modelViewMatrix[1] * x + s_modelViewMatrix[5] * y + s_modelViewMatrix[9]  * z;
	s_modelViewMatrix[14] += s_modelViewMatrix[2] * x + s_modelViewMatrix[6] * y + s_modelViewMatrix[10] * z;
	s_modelViewMatrix[15] += s_modelViewMatrix[3] * x + s_modelViewMatrix[7] * y + s_modelViewMatrix[11] * z;
}

void RB_OrthoProjection( float left, float right, float bottom, float top, float zNear, float zFar )
{
	float	m[16];
	float	rl = right - left;
	float	tb = top - bottom;
	float	fn = zFar - zNear;

	RB_MatrixIdentity( m );
	m[0]  = 2.0f / rl;
	m[5]  = 2.0f / tb;
	m[10] = -2.0f / fn;
	m[12] = -( right + left ) / rl;
	m[13] = -( top + bottom ) / tb;
	m[14] = -( zFar + zNear ) / fn;
	RB_LoadProjectionMatrix( m );
}

const float *RB_GetModelViewMatrix( void )
{
	return s_modelViewMatrix;
}

const float *RB_GetProjectionMatrix( void )
{
	return s_projectionMatrix;
}
