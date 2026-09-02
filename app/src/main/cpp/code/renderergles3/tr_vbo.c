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

/* The tessellation store is a ring of persistent buffer pairs.  The pair is
 * allocated ONCE at the structural maximum and is never re-specified; a send
 * writes the batch's exact bytes at the pair's base with glBufferSubData.  The
 * allocation is the ring, the upload is the batch, and the two are not the same
 * number.  The slot rotates per batch so a send never writes the storage the
 * preceding draws read; eight slots put eight batches between a write and the
 * next write to the same store.  The device capture shows the cost of the old
 * form is per call, not per byte (`[BE:send] sum=723374us batches=3552
 * first=239us mid=723135us`), so removing the per-send re-specification is the
 * fix and padding the send is not required to reach it. */
#define RB_TESS_RING	8

#define RB_TESS_VBO_BYTES	((GLsizeiptr)( (size_t)SHADER_MAX_VERTEXES * RB_VERTEX_STRIDE ))
#define RB_TESS_EBO_BYTES	((GLsizeiptr)( (size_t)SHADER_MAX_INDEXES * sizeof( glIndex_t ) ))

static GLuint	s_vaoTessRing[RB_TESS_RING];
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
static GLuint	s_vboTessRing[RB_TESS_RING];
static GLuint	s_eboTessRing[RB_TESS_RING];
static int		s_tessRingNext;
static GLuint	s_vboStream;
static GLuint	s_eboStream;

static qboolean	s_vboInitialised;

static rbVertex_t	s_tessStage[SHADER_MAX_VERTEXES];

/* The [BE:send] accumulators.  RB_VboSendTake() hands them to the back-end
 * window printer, so the row rolls in the same window as [BE:ds] and [BE:be]
 * instead of opening its own. */
static int	s_sendLast;
static int	s_sendRuns;
static int	s_sendBatches;
static int	s_sendSum;
static int	s_sendFirst;
static int	s_sendMax;
static int	s_sendGapMax;
static int	s_sendVerts;
static int	s_sendBig;
static int	s_sendBigVerts;

qboolean RB_VboSendTake( beSendRow_t *out ) {
	if ( !out ) {
		return qfalse;
	}

	out->batches = s_sendBatches;
	out->runs = s_sendRuns;
	out->sum = s_sendSum;
	out->first = s_sendFirst;
	out->mid = s_sendSum - s_sendFirst;
	out->max = s_sendMax;
	out->gapMax = s_sendGapMax;
	out->verts = s_sendVerts;
	out->big = s_sendBig;
	out->bigVerts = s_sendBigVerts;

	s_sendBatches = s_sendRuns = s_sendSum = s_sendFirst = 0;
	s_sendMax = s_sendGapMax = 0;
	s_sendVerts = s_sendBig = s_sendBigVerts = 0;

	return ( out->batches > 0 ) ? qtrue : qfalse;
}

/* One range of the repack.  Every vertex reads only its own source slots and writes
 * only its own s_tessStage element, so chunks are disjoint and the bytes produced do
 * not depend on the split.  The pool never pays here: the measured split is slower than
 * inline at every size, and at the structural maximum the range is one chunk, so every
 * batch takes the pool's inline branch. */
#define RB_VBO_PACK_CHUNK	SHADER_MAX_VERTEXES

typedef struct {
	const shaderCommands_t	*input;
	const float				*texcoord0;
	const float				*texcoord1;
	const unsigned char		*colors;
	const float				*normals;
	int						tcStrideFloats;
} rbVboPack_t;

static void RB_VboPackRange( void *vctx, int begin, int end ) {
	const rbVboPack_t	*c = (const rbVboPack_t *)vctx;
	const shaderCommands_t	*input = c->input;
	rbVertex_t			*dst = s_tessStage + begin;

	for ( ; begin < end; begin++, dst++ ) {
		const float *src;

		src = input->xyz[begin];
		dst->xyz[0] = src[0];
		dst->xyz[1] = src[1];
		dst->xyz[2] = src[2];
		dst->xyz[3] = src[3];

		if ( c->normals ) {
			src = c->normals + begin * 4;
			dst->normal[0] = src[0];
			dst->normal[1] = src[1];
			dst->normal[2] = src[2];
			dst->normal[3] = src[3];
		} else {
			dst->normal[0] = input->xyz[begin][0];
			dst->normal[1] = input->xyz[begin][1];
			dst->normal[2] = input->xyz[begin][2];
			dst->normal[3] = 1.0f;
		}

		if ( c->colors ) {
			dst->color[0] = c->colors[begin * 4 + 0];
			dst->color[1] = c->colors[begin * 4 + 1];
			dst->color[2] = c->colors[begin * 4 + 2];
			dst->color[3] = c->colors[begin * 4 + 3];
		} else {
			dst->color[0] = dst->color[1] = dst->color[2] = dst->color[3] = 255;
		}

		if ( c->texcoord0 ) {
			dst->st0[0] = c->texcoord0[begin * c->tcStrideFloats + 0];
			dst->st0[1] = c->texcoord0[begin * c->tcStrideFloats + 1];
		} else {
			dst->st0[0] = dst->st0[1] = 0.0f;
		}
		if ( c->texcoord1 ) {
			dst->st1[0] = c->texcoord1[begin * c->tcStrideFloats + 0];
			dst->st1[1] = c->texcoord1[begin * c->tcStrideFloats + 1];
		} else {
			dst->st1[0] = dst->st1[1] = 0.0f;
		}
	}
}

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
	int		i;

	if ( s_vboInitialised ) {
		return;
	}

	for ( i = 0; i < RB_TESS_RING; i++ ) {
		qglGenBuffers( 1, &s_vboTessRing[i] );
		qglGenBuffers( 1, &s_eboTessRing[i] );
		qglGenVertexArrays( 1, &s_vaoTessRing[i] );
		RB_BindVAO( s_vaoTessRing[i] );
		RB_BindBuffer( GL_ARRAY_BUFFER, s_vboTessRing[i] );
		qglBufferData( GL_ARRAY_BUFFER, RB_TESS_VBO_BYTES, NULL, GL_STREAM_DRAW );
		RB_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_eboTessRing[i] );
		qglBufferData( GL_ELEMENT_ARRAY_BUFFER, RB_TESS_EBO_BYTES, NULL, GL_STREAM_DRAW );
		RB_VBO_ConfigureAttribs();
	}
	s_tessRingNext = 0;

	qglGenBuffers( 1, &s_vboStream );
	qglGenBuffers( 1, &s_eboStream );

	qglGenVertexArrays( 1, &s_vaoStream );
	RB_BindVAO( s_vaoStream );
	RB_BindBuffer( GL_ARRAY_BUFFER, s_vboStream );
	qglBufferData( GL_ARRAY_BUFFER,
				   ( GLsizeiptr )( ( size_t )RB_STREAM_MAX_VERTS * RB_VERTEX_STRIDE ),
				   NULL, GL_STREAM_DRAW );
	RB_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_eboStream );
	qglBufferData( GL_ELEMENT_ARRAY_BUFFER,
				   ( GLsizeiptr )( ( size_t )RB_IMM_INDEX_CAPACITY( RB_STREAM_MAX_VERTS )
								   * sizeof( unsigned int ) ),
				   NULL, GL_STREAM_DRAW );
	RB_VBO_ConfigureAttribs();


	RB_ImmResetCurrent();

	s_vboInitialised = qtrue;
}

static void RB_VboStaticShutdown( void );

void RB_VBO_Shutdown( void )
{
	int		i;

	if ( !s_vboInitialised ) {
		return;
	}

	for ( i = 0; i < RB_TESS_RING; i++ ) {
		qglDeleteBuffers( 1, &s_vboTessRing[i] );
		qglDeleteBuffers( 1, &s_eboTessRing[i] );
		qglDeleteVertexArrays( 1, &s_vaoTessRing[i] );
	}
	qglDeleteBuffers( 1, &s_vboStream );
	qglDeleteBuffers( 1, &s_eboStream );
	qglDeleteVertexArrays( 1, &s_vaoStream );

	RB_VboStaticShutdown();

	s_vboInitialised = qfalse;
}

/* One batch's two sends.  `slot` rotates per batch, and the vertex and index
 * sends of a batch share their slot, so the pair a draw reads is the pair just
 * written and the pair the next send writes is eight batches old.  Both sizes and
 * both payloads are the batch's exact values, no padding: the allocation is the
 * structural maximum and the upload is the batch.  The call set is two
 * glBufferSubData sends plus the same bind calls; no call re-specifies storage,
 * and there is no fence, map or flush. */
static void RB_VboSendTess( int numVertexes, const glIndex_t *indexes, int numIndexes )
{
	int		slot = s_tessRingNext;

	s_tessRingNext = ( s_tessRingNext + 1 ) % RB_TESS_RING;

	RB_BindVAO( s_vaoTessRing[slot] );
	RB_BindBuffer( GL_ARRAY_BUFFER, s_vboTessRing[slot] );
	qglBufferSubData( GL_ARRAY_BUFFER, 0,
					  ( GLsizeiptr )( ( size_t )numVertexes * RB_VERTEX_STRIDE ),
					  s_tessStage );

	RB_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_eboTessRing[slot] );
	qglBufferSubData( GL_ELEMENT_ARRAY_BUFFER, 0,
					  ( GLsizeiptr )( ( size_t )numIndexes * sizeof( glIndex_t ) ),
					  indexes );
}

/* ---- static world vertex arena ------------------------------------------
 * A world surface whose batch the lightmapped multitexture fast path draws
 * takes the same vertex bytes on every frame: the positions and texture
 * coordinates come out of the BSP's own arrays, the colour is the constant
 * white array, and the batch carries no dynamic light.  The arena therefore
 * bakes each such surface once, appends the packed bytes to a device buffer
 * that is written once and never re-specified, and later frames draw the
 * surface from its region.  The bake runs the shipping pack body over the same
 * tess range, so the bytes are the bytes the streaming path would have
 * uploaded; the only thing that moves is where the vertex data comes from.
 * Draw order, batch composition, index order, uniforms and state are untouched.
 *
 * What is proven static for this class, and why it is the whole input set:
 *   - the surface is world BSP geometry, never an entity (the record hook tests
 *     `backEnd.currentEntity == &tr.worldEntity`); BSP surfaces do not move;
 *   - the shader takes RB_StageIteratorLightmappedMultitextureUnfogged, so the
 *     batch is one stage with a constant vertex colour and bundle 0/1 texture
 *     coordinates taken straight from `tess.texCoords` (the BSP drawVert st and
 *     lightmap fields), and no svars generator runs;
 *   - the batch has no dynamic light (`tess.dlightBits == 0`), so no dlight
 *     projection consumes the tess arrays, and `GLP_NORMAL` is not set by this
 *     program for a lightmapped shader, so the normal slot is not an input;
 *   - neither `r_showtris` nor `r_shownormals` is on, so no debug pass reads the
 *     batch back out of `tess`.
 * Anything else -- a moving entity, a dlight, a deformed or waveform shader,
 * the shadow pass, the generic or vertex-lit iterator, 2D -- is left on the
 * streaming path byte for byte as it was. */
#define RB_STATIC_ARENA_VERTS	( ( 24u * 1024u * 1024u ) / (unsigned)sizeof( rbVertex_t ) )
#define RB_STATIC_HASH_BITS		14
#define RB_STATIC_HASH_SIZE		( 1u << RB_STATIC_HASH_BITS )
#define RB_STATIC_BATCH_MAX		4096

typedef struct {
	const void	*surf;		/* NULL = never used */
	GLint		first;		/* first vertex of the region in the arena */
} rbStaticRegion_t;

typedef struct {
	const void	*surf;
	int			vertStart;
	int			numVerts;
	int			idxStart;
	int			numIdx;
} rbStaticRec_t;

static rbStaticRegion_t	s_staticHash[RB_STATIC_HASH_SIZE];
static GLuint			s_staticVbo;
static GLuint			s_staticVaoRing[RB_TESS_RING];
static int				s_staticVertsUsed;
static qboolean			s_staticFull;

static rbStaticRec_t	s_staticRec[RB_STATIC_BATCH_MAX];
static int				s_staticRecCount;
static qboolean			s_staticRecOverflow;
static glIndex_t		s_staticIdx[SHADER_MAX_INDEXES];

static void RB_VboStaticShutdown( void ) {
	int		i;

	if ( s_staticVbo ) {
		for ( i = 0; i < RB_TESS_RING; i++ ) {
			qglDeleteVertexArrays( 1, &s_staticVaoRing[i] );
		}
		qglDeleteBuffers( 1, &s_staticVbo );
		s_staticVbo = 0;
	}
	Com_Memset( s_staticHash, 0, sizeof( s_staticHash ) );
	s_staticVertsUsed = 0;
	s_staticFull = qfalse;
	RB_VboStaticReset();
}

void RB_VboStaticReset( void ) {
	s_staticRecCount = 0;
	s_staticRecOverflow = qfalse;
}

void RB_VboStaticRecord( const void *surf, int vertStart, int numVerts, int idxStart, int numIdx ) {
	if ( s_staticRecOverflow || numVerts <= 0 || numIdx <= 0 ) {
		return;
	}
	if ( s_staticRecCount >= RB_STATIC_BATCH_MAX ) {
		s_staticRecOverflow = qtrue;
		return;
	}
	s_staticRec[s_staticRecCount].surf = surf;
	s_staticRec[s_staticRecCount].vertStart = vertStart;
	s_staticRec[s_staticRecCount].numVerts = numVerts;
	s_staticRec[s_staticRecCount].idxStart = idxStart;
	s_staticRec[s_staticRecCount].numIdx = numIdx;
	s_staticRecCount++;
}

static unsigned RB_StaticHash( const void *p ) {
	size_t	v = ( size_t )p >> 4;

	return (unsigned)( ( v * (size_t)2654435761u ) >> ( ( sizeof( size_t ) * 8 ) - RB_STATIC_HASH_BITS ) );
}

/* Lookup, or the insertion point for a new key.  NULL only when the table is full. */
static rbStaticRegion_t *RB_StaticFind( const void *surf ) {
	unsigned			h = RB_StaticHash( surf );
	unsigned			i;

	for ( i = 0; i < RB_STATIC_HASH_SIZE; i++ ) {
		rbStaticRegion_t	*e = &s_staticHash[( h + i ) & ( RB_STATIC_HASH_SIZE - 1 )];

		if ( e->surf == surf ) {
			return e;
		}
		if ( !e->surf ) {
			return e;
		}
	}
	return NULL;
}

static void RB_VboStaticInit( void ) {
	int		i;

	if ( s_staticVbo ) {
		return;
	}

	qglGenBuffers( 1, &s_staticVbo );
	RB_BindBuffer( GL_ARRAY_BUFFER, s_staticVbo );
	qglBufferData( GL_ARRAY_BUFFER,
				   ( GLsizeiptr )( ( size_t )RB_STATIC_ARENA_VERTS * sizeof( rbVertex_t ) ),
				   NULL, GL_STATIC_DRAW );

	for ( i = 0; i < RB_TESS_RING; i++ ) {
		qglGenVertexArrays( 1, &s_staticVaoRing[i] );
		RB_BindVAO( s_staticVaoRing[i] );
		RB_BindBuffer( GL_ARRAY_BUFFER, s_staticVbo );
		RB_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_eboTessRing[i] );
		RB_VBO_ConfigureAttribs();
	}
	RB_BindVAO( 0 );
}

/* Is this draw the lightmapped multitexture world path over a batch that the
 * surface hooks recorded exactly?  The call signature is checked because it is
 * the only caller that passes bundle 0/1 as one four-float stride with the
 * constant white colour, and the arena layout is that signature's layout. */
static qboolean RB_StaticBatch( const shaderCommands_t *input, int numIndexes,
								const float *texcoord0, const float *texcoord1, int tcStrideFloats,
								const unsigned char *colors, const float *normals ) {
	int		i, v, n;

	if ( input != &tess || s_staticRecCount <= 0 || s_staticRecOverflow ) {
		return qfalse;
	}
	if ( r_showtris->integer || r_shownormals->integer ) {
		return qfalse;
	}
	if ( tess.dlightBits ) {
		return qfalse;
	}
	if ( tcStrideFloats != 4
		 || texcoord0 != (const float *)input->texCoords[0][0]
		 || texcoord1 != (const float *)input->texCoords[0][0] + 2
		 || colors != (const unsigned char *)tess.constantColor255
		 || normals != input->normal[0] ) {
		return qfalse;
	}
	if ( input->numVertexes > SHADER_MAX_VERTEXES || numIndexes > SHADER_MAX_INDEXES ) {
		return qfalse;
	}

	/* the records must tile the batch exactly, in order, with no gap */
	v = 0;
	n = 0;
	for ( i = 0; i < s_staticRecCount; i++ ) {
		if ( s_staticRec[i].vertStart != v || s_staticRec[i].idxStart != n ) {
			return qfalse;
		}
		v += s_staticRec[i].numVerts;
		n += s_staticRec[i].numIdx;
	}
	if ( v != input->numVertexes || n != numIndexes ) {
		return qfalse;
	}
	return qtrue;
}

/* Bake every recorded surface that is not in the arena yet.  The bytes come from
 * `s_tessStage`, which the caller packed with the shipping body over the same
 * range, so the arena holds exactly the streaming stream.  A surface that does
 * not fit is left unbaked and the batch falls back for good. */
static qboolean RB_VboStaticBake( void ) {
	int		i;

	RB_VboStaticInit();
	RB_BindBuffer( GL_ARRAY_BUFFER, s_staticVbo );

	for ( i = 0; i < s_staticRecCount; i++ ) {
		const rbStaticRec_t	*r = &s_staticRec[i];
		rbStaticRegion_t	*e = RB_StaticFind( r->surf );

		if ( !e ) {
			s_staticFull = qtrue;
			return qfalse;
		}
		if ( e->surf == r->surf ) {
			continue;			/* already resident */
		}
		if ( s_staticVertsUsed + r->numVerts > (int)RB_STATIC_ARENA_VERTS ) {
			s_staticFull = qtrue;
			return qfalse;
		}

		e->surf = r->surf;
		e->first = s_staticVertsUsed;
		qglBufferSubData( GL_ARRAY_BUFFER,
						  (GLintptr)( ( size_t )s_staticVertsUsed * sizeof( rbVertex_t ) ),
						  (GLsizeiptr)( ( size_t )r->numVerts * sizeof( rbVertex_t ) ),
						  s_tessStage + r->vertStart );
		s_staticVertsUsed += r->numVerts;
	}
	return qtrue;
}

/* Compose the batch's index list against the arena and send only that.  The
 * indices are the batch's own triangles, rebased onto the region of the surface
 * each came from, so the drawn triangles and their order are unchanged. */
static void RB_VboStaticSend( const glIndex_t *indexes, int numIndexes ) {
	int		slot = s_tessRingNext;
	int		k = 0;
	int		i;

	s_tessRingNext = ( s_tessRingNext + 1 ) % RB_TESS_RING;

	for ( i = 0; i < s_staticRecCount; i++ ) {
		const rbStaticRec_t	*r = &s_staticRec[i];
		const rbStaticRegion_t	*e = RB_StaticFind( r->surf );
		int					j;

		for ( j = 0; j < r->numIdx; j++ ) {
			s_staticIdx[k++] = (glIndex_t)( e->first + ( indexes[r->idxStart + j] - r->vertStart ) );
		}
	}

	RB_BindVAO( s_staticVaoRing[slot] );
	RB_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_eboTessRing[slot] );
	qglBufferSubData( GL_ELEMENT_ARRAY_BUFFER, 0,
					  (GLsizeiptr)( ( size_t )k * sizeof( glIndex_t ) ), s_staticIdx );
}


void RB_VBO_DrawTessArrays( const shaderCommands_t *input,
							const glIndex_t *indexes, int numIndexes,
							const float *texcoord0, const float *texcoord1, int tcStrideFloats,
							const unsigned char *colors,
							const float *normals )
{
	if ( !s_vboInitialised ) {
		RB_VBO_Init();
	}

	if ( numIndexes <= 0 || input->numVertexes <= 0 ) {
		return;
	}

	BE_SUB( BE_FAM_DS, BE_DSZ_sub_vbo, {
		rbVboPack_t	pack;
		int			subT0;
		int			subT1;
		qboolean	staticDraw;

		pack.input = input;
		pack.texcoord0 = texcoord0;
		pack.texcoord1 = texcoord1;
		pack.colors = colors;
		pack.normals = normals;
		pack.tcStrideFloats = tcStrideFloats;

		subT0 = BE_On() ? BE_Microseconds() : 0;
		JobPool_ParallelRange( input->numVertexes, RB_VBO_PACK_CHUNK, RB_VboPackRange, &pack );
		BE_Sub( BE_FAM_DS, BE_DSZ_sub_pack, BE_Microseconds() - subT0 );

		/* A static world batch is baked from that pack and drawn out of the
		 * arena; everything else takes the streaming send unchanged. */
		staticDraw = RB_StaticBatch( input, numIndexes, texcoord0, texcoord1,
									 tcStrideFloats, colors, normals )
					 && RB_VboStaticBake();

		subT0 = BE_On() ? BE_Microseconds() : 0;
		if ( staticDraw ) {
			RB_VboStaticSend( indexes, numIndexes );
		} else {
			RB_VboSendTess( input->numVertexes, indexes, numIndexes );
		}
		subT1 = BE_On() ? BE_Microseconds() : 0;
		BE_Sub( BE_FAM_DS, BE_DSZ_sub_upload, subT1 - subT0 );

		/* [BE:send] probe: the upload phase is the one place inside the walk where the
		 * CPU can block on the driver.  A phase that is long only when it follows a long
		 * gap is a drain of the previous frame; a phase that is long inside a run, where
		 * the gap is short, is the send itself blocking.  `big` counts the phases whose
		 * batch exceeded the old 512-vertex chunk.  Every read is behind BE_On(), so a
		 * release run reads no clock.  Values are microseconds.  The row is printed by
		 * BE_MaybeRoll() so it shares the [BE:ds]/[BE:be] window. */
		if ( BE_On() ) {
			int			phase;
			int			gap;

			phase = subT1 - subT0;
			gap = s_sendLast ? subT0 - s_sendLast : 0;
			s_sendBatches++;
			s_sendSum += phase;
			s_sendVerts += input->numVertexes;
			if ( input->numVertexes > 512 ) {
				s_sendBig++;
				s_sendBigVerts += input->numVertexes;
			}
			if ( !s_sendLast || gap > 2000 ) {
				s_sendRuns++;
				s_sendFirst += phase;
			}
			if ( gap > s_sendGapMax ) {
				s_sendGapMax = gap;
			}
			if ( phase > s_sendMax ) {
				s_sendMax = phase;
			}
			s_sendLast = subT1;
		}

		subT0 = BE_On() ? BE_Microseconds() : 0;
		qglDrawElements( GL_TRIANGLES, numIndexes, GL_UNSIGNED_INT, ( const void * )0 );
		BE_Sub( BE_FAM_DS, BE_DSZ_sub_draw, BE_Microseconds() - subT0 );

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

	RB_BindVAO( s_vaoTessRing[0] );
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
	qglBufferSubData( GL_ARRAY_BUFFER, 0,
					  ( GLsizeiptr )( ( size_t )s_streamNumVerts * RB_VERTEX_STRIDE ),
					  s_streamVerts );

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
		qglBufferSubData( GL_ELEMENT_ARRAY_BUFFER, 0,
						  ( GLsizeiptr )( ( size_t )nIndices * sizeof( unsigned int ) ),
						  s_streamIndices );
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
