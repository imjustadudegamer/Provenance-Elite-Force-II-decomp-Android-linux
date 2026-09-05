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

/* ---- the optional multi-draw --------------------------------------------
 * GL_EXT_multi_draw_arrays is not core, so it is resolved at run time and a
 * context without it keeps the composed-store path byte for byte. */
typedef void (*rbMultiDrawElements_t)( GLenum mode, const GLsizei *count, GLenum type,
									   const void *const *indices, GLsizei primcount );
static rbMultiDrawElements_t	s_multiDraw;
static qboolean					s_multiDrawReady;

static void RB_VboMultiDrawInit( void ) {
	QGL_GLES3_GetProcAddress_t	resolve;

	if ( s_multiDrawReady ) {
		return;
	}
	s_multiDrawReady = qtrue;

	resolve = QGL_GLES3_GetGetProcAddress();
	if ( resolve ) {
		s_multiDraw = (rbMultiDrawElements_t)resolve( "glMultiDrawElementsEXT" );
	}
	/* a symbol is not an extension: a driver can hand back a non-NULL address
	 * for a name it does not advertise */
	if ( s_multiDraw && !QGL_GLES3_HasExtension( "GL_EXT_multi_draw_arrays" ) ) {
		s_multiDraw = NULL;
	}
}

static GLuint	s_vboTessRing[RB_TESS_RING];
static GLuint	s_eboTessRing[RB_TESS_RING];
static int		s_tessRingNext;
static GLuint	s_vboStream;
static GLuint	s_eboStream;

/* ---- the per-frame tessellation stream store -----------------------------
 * One back end frame's stream is appended into a whole store discarded once when
 * the frame opens, three stores rotate, and a frame that overflows falls back to
 * the ring byte for byte. */
#define RB_TESS_ARENA_COUNT		3
#define RB_TESS_ARENA_ALIGN		32
/* PAD is power-of-two only and a whole vertex cannot be one: the store's vertex
 * stream needs this general form. */
#define RB_TESS_ARENA_VALIGN	( (GLintptr)( RB_VERTEX_STRIDE * ( RB_TESS_ARENA_ALIGN / 4 ) ) )
typedef char rb_arena_valign_is_vertex_aligned[
	( ( RB_VERTEX_STRIDE * ( RB_TESS_ARENA_ALIGN / 4 ) ) % RB_VERTEX_STRIDE ) == 0 ? 1 : -1 ];

static GLintptr RB_VboAlignUp( GLintptr v, GLintptr align ) {
	return ( ( v + align - 1 ) / align ) * align;
}
#define RB_TESS_ARENA_VBO_BYTES	( (GLsizeiptr)( (size_t)2u * 1024u * 1024u ) )
#define RB_TESS_ARENA_EBO_BYTES	( (GLsizeiptr)( (size_t)1u * 1024u * 1024u ) )

static GLuint	s_vboArena[RB_TESS_ARENA_COUNT];
static GLuint	s_eboArena[RB_TESS_ARENA_COUNT];
static GLuint	s_vaoArena;
static GLuint	s_vaoStaticArena;
static int		s_arenaNext;
static GLuint	s_arenaVbo;
static GLuint	s_arenaEbo;
static GLintptr	s_arenaVOff;
static GLintptr	s_arenaIOff;
static qboolean	s_arenaInit;
static qboolean	s_arenaActive;
static qboolean	s_arenaFull;
static int		s_arenaFrame = -1;
static cvar_t	*s_arenaCvar;
/* Batches the store could not hold, which the ring drew instead. */
static int		s_arenaFall;

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
static int	s_sendWrites;
static int	s_sendLocal;
static int	s_sendStatics;
static int	s_sendMulti;
static int	s_sendSeq;
/* One rejection reason per batch, in the gate's own order. */
static int	s_sendGateWorld;
static int	s_sendGateDlight;
static int	s_sendGateIter;
static int	s_sendGateSig;
static int	s_sendGateRec;
static int	s_sendGateRecs;
static int				s_locVisits;
static int				s_locVtxMiss;
static int				s_locIdxMiss;
static int				s_locHashMiss;
static const void		*s_locFirst;
static int				s_locFirstAt;
static unsigned			s_locWas, s_locNow;
static int	s_sendLocVisits, s_sendLocVtx, s_sendLocIdx, s_sendLocHash;
static int	s_sendLocFirstAt;
static const void *s_sendLocFirst;
static unsigned	s_sendLocWas, s_sendLocNow;

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
	out->fall = s_arenaFall;
	out->writes = s_sendWrites;
	out->local = s_sendLocal;
	out->statics = s_sendStatics;
	out->multi = s_sendMulti;
	out->seq = s_sendSeq;
	out->locVisits = s_sendLocVisits;
	out->locVtx = s_sendLocVtx;
	out->locIdx = s_sendLocIdx;
	out->locHash = s_sendLocHash;
	out->locFirstAt = s_sendLocFirstAt;
	out->locFirst = (unsigned)(size_t)s_sendLocFirst;
	out->locWas = s_sendLocWas;
	out->locNow = s_sendLocNow;
	out->gateWorld = s_sendGateWorld;
	out->gateDlight = s_sendGateDlight;
	out->gateIter = s_sendGateIter;
	out->gateSig = s_sendGateSig;
	out->gateRec = s_sendGateRec;
	out->recs = s_sendGateRecs;

	s_sendBatches = s_sendRuns = s_sendSum = s_sendFirst = 0;
	s_sendMax = s_sendGapMax = 0;
	s_sendVerts = s_sendBig = s_sendBigVerts = 0;
	s_sendWrites = s_sendLocal = s_sendStatics = s_sendMulti = s_sendSeq = 0;
	s_sendGateWorld = s_sendGateDlight = s_sendGateIter = 0;
	s_sendGateSig = s_sendGateRec = s_sendGateRecs = 0;
	s_sendLocVisits = s_locVisits;
	s_sendLocVtx = s_locVtxMiss;
	s_sendLocIdx = s_locIdxMiss;
	s_sendLocHash = s_locHashMiss;
	s_sendLocFirstAt = s_locFirstAt;
	s_sendLocFirst = s_locFirst;
	s_sendLocWas = s_locWas;
	s_sendLocNow = s_locNow;
	s_locVisits = s_locVtxMiss = s_locIdxMiss = s_locHashMiss = 0;
	s_locFirst = NULL;
	s_locFirstAt = 0;
	s_locWas = s_locNow = 0;
	s_arenaFall = 0;

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

/* Point the five attributes of the bound VAO at `base` in the buffer the VAO
 * already names.  The field offsets are the layout RB_VboPackRange writes. */
static void RB_VboAttribsAt( GLintptr base )
{
	qglEnableVertexAttribArray( ATTR_POSITION );
	qglVertexAttribPointer( ATTR_POSITION, 4, GL_FLOAT, GL_FALSE, RB_VERTEX_STRIDE,
							( const void * )(uintptr_t)( base + 0 ) );

	qglEnableVertexAttribArray( ATTR_NORMAL );
	qglVertexAttribPointer( ATTR_NORMAL, 4, GL_FLOAT, GL_FALSE, RB_VERTEX_STRIDE,
							( const void * )(uintptr_t)( base + 16 ) );

	qglEnableVertexAttribArray( ATTR_COLOR );
	qglVertexAttribPointer( ATTR_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, RB_VERTEX_STRIDE,
							( const void * )(uintptr_t)( base + 32 ) );

	qglEnableVertexAttribArray( ATTR_TEXCOORD0 );
	qglVertexAttribPointer( ATTR_TEXCOORD0, 2, GL_FLOAT, GL_FALSE, RB_VERTEX_STRIDE,
							( const void * )(uintptr_t)( base + 36 ) );

	qglEnableVertexAttribArray( ATTR_TEXCOORD1 );
	qglVertexAttribPointer( ATTR_TEXCOORD1, 2, GL_FLOAT, GL_FALSE, RB_VERTEX_STRIDE,
							( const void * )(uintptr_t)( base + 44 ) );
}

static void RB_VBO_ConfigureAttribs( void )
{
	RB_VboAttribsAt( 0 );
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
static void RB_VboArenaShutdown( void );

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
	RB_VboArenaShutdown();

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
#define RB_STATIC_ARENA_IDXES	( RB_STATIC_ARENA_VERTS * 3u )
#define RB_STATIC_HASH_BITS		14
#define RB_STATIC_HASH_SIZE		( 1u << RB_STATIC_HASH_BITS )
#define RB_STATIC_BATCH_MAX		4096

typedef struct {
	const void	*surf;		/* NULL = never used */
	GLint		first;		/* first vertex of the region in the arena */
	GLint		idxFirst;	/* first index of the region, same bake order */
	int			numVerts;	/* the counts the region was baked with */
	int			numIdx;
	unsigned	idxHash;	/* FN1a of the baked block, never 0 */
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
static GLuint			s_staticEbo;
static GLuint			s_vaoStaticLocal;
static GLuint			s_staticVaoRing[RB_TESS_RING];
static int				s_staticVertsUsed;
static int				s_staticIdxesUsed;
static qboolean			s_staticFull;

static rbStaticRec_t	s_staticRec[RB_STATIC_BATCH_MAX];
static int				s_staticRecCount;
static qboolean			s_staticRecOverflow;
static glIndex_t		s_staticIdx[SHADER_MAX_INDEXES];
static glIndex_t		s_staticIdxStage[SHADER_MAX_INDEXES];
static GLsizei			s_staticCounts[RB_STATIC_BATCH_MAX];
static const void		*s_staticOffsets[RB_STATIC_BATCH_MAX];

static void RB_VboStaticShutdown( void ) {
	int		i;

	if ( s_staticVbo ) {
		for ( i = 0; i < RB_TESS_RING; i++ ) {
			qglDeleteVertexArrays( 1, &s_staticVaoRing[i] );
		}
		if ( s_vaoStaticLocal ) {
			qglDeleteVertexArrays( 1, &s_vaoStaticLocal );
			s_vaoStaticLocal = 0;
		}
		qglDeleteBuffers( 1, &s_staticVbo );
		s_staticVbo = 0;
	}
	if ( s_staticEbo ) {
		qglDeleteBuffers( 1, &s_staticEbo );
		s_staticEbo = 0;
	}
	Com_Memset( s_staticHash, 0, sizeof( s_staticHash ) );
	s_staticVertsUsed = 0;
	s_staticIdxesUsed = 0;
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

/* One source index of a recorded surface, rebased onto that surface's region of
 * the vertex arena.  This is the only arithmetic that turns a batch index into a
 * drawn index, and it is the same call in the bake and in the fallback compose,
 * so the two index streams are the same bytes by construction. */
static glIndex_t RB_StaticRebase( const rbStaticRec_t *r, const rbStaticRegion_t *e,
								  glIndex_t local ) {
	return (glIndex_t)( e->first + ( local - r->vertStart ) );
}

/* Copy one record's whole rebased index block to `dst`.  Returns the count. */
static int RB_StaticBlock( const rbStaticRec_t *r, glIndex_t *dst, const glIndex_t *indexes ) {
	const rbStaticRegion_t	*e = RB_StaticFind( r->surf );
	int						j;

	for ( j = 0; j < r->numIdx; j++ ) {
		dst[j] = RB_StaticRebase( r, e, indexes[r->idxStart + j] );
	}
	return r->numIdx;
}

static unsigned RB_StaticHashBlock( const rbStaticRec_t *r, const glIndex_t *indexes ) {
	const rbStaticRegion_t	*e = RB_StaticFind( r->surf );
	unsigned				h = 2166136261u;
	int						j;

	for ( j = 0; j < r->numIdx; j++ ) {
		h = ( h ^ (unsigned)RB_StaticRebase( r, e, indexes[r->idxStart + j] ) ) * 16777619u;
	}
	return h ? h : 1u;
}

static void RB_VboStaticInit( void ) {
	int		i;

	if ( s_staticVbo ) {
		return;
	}

	/* The element binding is VAO state and a VAO is live here: allocate with
	 * none bound, bind at the point of use. */
	RB_BindVAO( 0 );

	qglGenBuffers( 1, &s_staticVbo );
	RB_BindBuffer( GL_ARRAY_BUFFER, s_staticVbo );
	qglBufferData( GL_ARRAY_BUFFER,
				   ( GLsizeiptr )( ( size_t )RB_STATIC_ARENA_VERTS * sizeof( rbVertex_t ) ),
				   NULL, GL_STATIC_DRAW );

	qglGenBuffers( 1, &s_staticEbo );
	RB_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_staticEbo );
	qglBufferData( GL_ELEMENT_ARRAY_BUFFER,
				   ( GLsizeiptr )( ( size_t )RB_STATIC_ARENA_IDXES * sizeof( glIndex_t ) ),
				   NULL, GL_STATIC_DRAW );

	for ( i = 0; i < RB_TESS_RING; i++ ) {
		qglGenVertexArrays( 1, &s_staticVaoRing[i] );
		RB_BindVAO( s_staticVaoRing[i] );
		RB_BindBuffer( GL_ARRAY_BUFFER, s_staticVbo );
		RB_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_eboTessRing[i] );
		RB_VBO_ConfigureAttribs();
	}

	/* the same vertices drawn with the frame store's indices */
	qglGenVertexArrays( 1, &s_vaoStaticArena );
	RB_BindVAO( s_vaoStaticArena );
	RB_BindBuffer( GL_ARRAY_BUFFER, s_staticVbo );
	RB_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_arenaEbo );
	RB_VBO_ConfigureAttribs();

	/* the same vertices drawn with the device-local baked index blocks */
	qglGenVertexArrays( 1, &s_vaoStaticLocal );
	RB_BindVAO( s_vaoStaticLocal );
	RB_BindBuffer( GL_ARRAY_BUFFER, s_staticVbo );
	RB_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_staticEbo );
	RB_VBO_ConfigureAttribs();

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

/* Why the gate refused -- its own conditions, its own order.  Instrument only:
 * runs under BE_On() and decides nothing. */
enum {
	RB_GATE_accepted = 0,
	RB_GATE_world,
	RB_GATE_dlight,
	RB_GATE_iter,
	RB_GATE_sig,
	RB_GATE_records
};

static int RB_StaticGateReason( const shaderCommands_t *input, int numIndexes,
								const float *texcoord0, const float *texcoord1,
								int tcStrideFloats, const unsigned char *colors,
								const float *normals ) {
	int		i, v, n;

	if ( input != &tess ) {
		return RB_GATE_sig;
	}
	if ( backEnd.currentEntity != &tr.worldEntity ) {
		return RB_GATE_world;
	}
	if ( tess.dlightBits ) {
		return RB_GATE_dlight;
	}
	if ( tess.currentStageIteratorFunc != RB_StageIteratorLightmappedMultitextureUnfogged ) {
		return RB_GATE_iter;
	}
	if ( r_showtris->integer || r_shownormals->integer
		 || tcStrideFloats != 4
		 || texcoord0 != (const float *)input->texCoords[0][0]
		 || texcoord1 != (const float *)input->texCoords[0][0] + 2
		 || colors != (const unsigned char *)tess.constantColor255
		 || normals != input->normal[0]
		 || input->numVertexes > SHADER_MAX_VERTEXES || numIndexes > SHADER_MAX_INDEXES ) {
		return RB_GATE_sig;
	}
	if ( s_staticRecCount <= 0 || s_staticRecOverflow ) {
		return RB_GATE_records;
	}

	/* the same tiling test the gate makes */
	v = 0;
	n = 0;
	for ( i = 0; i < s_staticRecCount; i++ ) {
		if ( s_staticRec[i].vertStart != v || s_staticRec[i].idxStart != n ) {
			return RB_GATE_records;
		}
		v += s_staticRec[i].numVerts;
		n += s_staticRec[i].numIdx;
	}
	if ( v != input->numVertexes || n != numIndexes ) {
		return RB_GATE_records;
	}
	return RB_GATE_accepted;
}

/* One reason a batch, under the instrument gate only. */
static void RB_StaticGateAccount( const shaderCommands_t *input, int numIndexes,
								  const float *texcoord0, const float *texcoord1,
								  int tcStrideFloats, const unsigned char *colors,
								  const float *normals ) {
	switch ( RB_StaticGateReason( input, numIndexes, texcoord0, texcoord1,
								  tcStrideFloats, colors, normals ) ) {
	case RB_GATE_world:		s_sendGateWorld++;	break;
	case RB_GATE_dlight:	s_sendGateDlight++;	break;
	case RB_GATE_iter:		s_sendGateIter++;	break;
	case RB_GATE_sig:		s_sendGateSig++;	break;
	case RB_GATE_records:	s_sendGateRec++;	break;
	default:									break;
	}
	s_sendGateRecs += s_staticRecCount;
}

/* Bake every recorded surface that is not in the arena yet.  The bytes come from
 * `s_tessStage`, which the caller packed with the shipping body over the same
 * range, so the arena holds exactly the streaming stream.  A surface that does
 * not fit is left unbaked and the batch falls back for good. */
static qboolean RB_VboStaticBake( const glIndex_t *indexes ) {
	int		i;

	RB_VboStaticInit();
	/* the local VAO names both arenas, so the two bakes below cannot land in a
	 * store another VAO had bound */
	RB_BindVAO( s_vaoStaticLocal );
	RB_BindBuffer( GL_ARRAY_BUFFER, s_staticVbo );
	RB_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_staticEbo );

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
		if ( s_staticVertsUsed + r->numVerts > (int)RB_STATIC_ARENA_VERTS
			 || s_staticIdxesUsed + r->numIdx > (int)RB_STATIC_ARENA_IDXES ) {
			s_staticFull = qtrue;
			return qfalse;
		}

		e->surf = r->surf;
		e->first = s_staticVertsUsed;
		e->idxFirst = s_staticIdxesUsed;
		e->numVerts = r->numVerts;
		e->numIdx = r->numIdx;
		e->idxHash = RB_StaticHashBlock( r, indexes );
		qglBufferSubData( GL_ARRAY_BUFFER,
						  (GLintptr)( ( size_t )s_staticVertsUsed * sizeof( rbVertex_t ) ),
						  (GLsizeiptr)( ( size_t )r->numVerts * sizeof( rbVertex_t ) ),
						  s_tessStage + r->vertStart );
		s_staticVertsUsed += r->numVerts;

		/* the same rebase the composed send would have made, written once into a
		 * device buffer the draws then read for the rest of the level */
		RB_StaticBlock( r, s_staticIdxStage, indexes );
		qglBufferSubData( GL_ELEMENT_ARRAY_BUFFER,
						  (GLintptr)( ( size_t )e->idxFirst * sizeof( glIndex_t ) ),
						  (GLsizeiptr)( ( size_t )r->numIdx * sizeof( glIndex_t ) ),
						  s_staticIdxStage );
		s_staticIdxesUsed += r->numIdx;
	}
	return qtrue;
}

/* Compose the batch's index list against the arena and send only that.  The
 * indices are the batch's own triangles, rebased onto the region of the surface
 * each came from, so the drawn triangles and their order are unchanged.  It is
 * the fallback for a batch the device-local blocks cannot draw in one call. */
static void RB_VboStaticSend( const glIndex_t *indexes, int numIndexes ) {
	int		slot = s_tessRingNext;
	int		k = 0;
	int		i;

	s_tessRingNext = ( s_tessRingNext + 1 ) % RB_TESS_RING;

	for ( i = 0; i < s_staticRecCount; i++ ) {
		k += RB_StaticBlock( &s_staticRec[i], s_staticIdx + k, indexes );
	}

	RB_BindVAO( s_staticVaoRing[slot] );
	RB_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_eboTessRing[slot] );
	qglBufferSubData( GL_ELEMENT_ARRAY_BUFFER, 0,
					  (GLsizeiptr)( ( size_t )k * sizeof( glIndex_t ) ), s_staticIdx );
}

/* ---- the device-local static draw ----------------------------------------
 * One call a record, one multi-draw where the context has the extension, and no
 * store write on either path. */
static qboolean RB_VboStaticLocalPrepare( const glIndex_t *indexes, int numIndexes ) {
	static cvar_t	*cvar;
	int		i, k;

	if ( s_staticRecCount <= 0 ) {
		return qfalse;
	}
	if ( !cvar ) {
		/* Default off: the device-local draw arm regressed frame time on Mali-class
		 * GPUs relative to the composed send; set r_tessLocal 1 to re-enable. */
		cvar = ri.Cvar_Get( "r_tessLocal", "0", 0 );
	}
	if ( !cvar->integer ) {
		return qfalse;
	}

	RB_VboMultiDrawInit();

	k = 0;
	for ( i = 0; i < s_staticRecCount; i++ ) {
		const rbStaticRec_t		*r = &s_staticRec[i];
		const rbStaticRegion_t	*e = RB_StaticFind( r->surf );

		if ( !e || e->surf != r->surf ) {
			return qfalse;
		}
		s_locVisits++;
		if ( e->numVerts != r->numVerts ) {
			s_locVtxMiss++;
			if ( !s_locFirst ) { s_locFirst = r->surf; s_locFirstAt = s_locVisits; }
			return qfalse;
		}
		if ( e->numIdx != r->numIdx ) {
			s_locIdxMiss++;
			if ( !s_locFirst ) { s_locFirst = r->surf; s_locFirstAt = s_locVisits; }
			return qfalse;
		}
		if ( e->idxHash != RB_StaticHashBlock( r, indexes ) ) {
			s_locHashMiss++;
			if ( !s_locFirst ) {
				s_locFirst = r->surf;
				s_locFirstAt = s_locVisits;
				s_locWas = e->idxHash;
				s_locNow = RB_StaticHashBlock( r, indexes );
			}
			return qfalse;
		}
		s_staticCounts[i] = (GLsizei)r->numIdx;
		s_staticOffsets[i] = (const void *)(uintptr_t)( (size_t)e->idxFirst * sizeof( glIndex_t ) );
		k += r->numIdx;
	}
	if ( k != numIndexes ) {
		return qfalse;
	}

	RB_BindVAO( s_vaoStaticLocal );
	RB_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_staticEbo );
	return qtrue;
}

static void RB_VboStaticLocalDraw( void ) {
	int		i;

	if ( s_staticRecCount == 1 ) {
		qglDrawElements( GL_TRIANGLES, s_staticCounts[0], GL_UNSIGNED_INT, s_staticOffsets[0] );
		return;
	}
	if ( s_multiDraw ) {
		s_multiDraw( GL_TRIANGLES, s_staticCounts, GL_UNSIGNED_INT, s_staticOffsets,
					 (GLsizei)s_staticRecCount );
		return;
	}
	/* No multi-draw on this context: one call a record out of the baked blocks is
	 * still ZERO store writes, in record order. */
	for ( i = 0; i < s_staticRecCount; i++ ) {
		qglDrawElements( GL_TRIANGLES, s_staticCounts[i], GL_UNSIGNED_INT, s_staticOffsets[i] );
	}
}

/* ---- the frame store ------------------------------------------------------ */

/* Base-vertex drawing needs the entry point resolved AND a context it is core
 * in: a symbol is not an implementation. */
static qboolean	s_baseVertexDraw;
static qboolean	s_baseVertexTested;

static void RB_VboBaseVertexInit( void ) {
	if ( s_baseVertexTested ) {
		return;
	}
	s_baseVertexTested = qtrue;
	s_baseVertexDraw = qfalse;
	if ( !qglDrawElementsBaseVertex ) {
		return;
	}
	if ( !QGLES_VERSION_ATLEAST( 3, 2 ) ) {
		return;
	}
	s_baseVertexDraw = qtrue;
}

static void RB_VboArenaInit( void ) {
	int		i;

	if ( s_arenaInit ) {
		return;
	}

	qglGenVertexArrays( 1, &s_vaoArena );
	RB_BindVAO( s_vaoArena );

	for ( i = 0; i < RB_TESS_ARENA_COUNT; i++ ) {
		qglGenBuffers( 1, &s_vboArena[i] );
		RB_BindBuffer( GL_ARRAY_BUFFER, s_vboArena[i] );
		qglBufferData( GL_ARRAY_BUFFER, RB_TESS_ARENA_VBO_BYTES, NULL, GL_STREAM_DRAW );

		qglGenBuffers( 1, &s_eboArena[i] );
		RB_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_eboArena[i] );
		qglBufferData( GL_ELEMENT_ARRAY_BUFFER, RB_TESS_ARENA_EBO_BYTES, NULL, GL_STREAM_DRAW );
	}

	RB_VBO_ConfigureAttribs();

	s_arenaNext = RB_TESS_ARENA_COUNT - 1;
	s_arenaInit = qtrue;
}

static void RB_VboArenaShutdown( void ) {
	int		i;

	if ( !s_arenaInit ) {
		return;
	}

	for ( i = 0; i < RB_TESS_ARENA_COUNT; i++ ) {
		qglDeleteBuffers( 1, &s_vboArena[i] );
		qglDeleteBuffers( 1, &s_eboArena[i] );
	}
	qglDeleteVertexArrays( 1, &s_vaoArena );
	if ( s_vaoStaticArena ) {
		qglDeleteVertexArrays( 1, &s_vaoStaticArena );
		s_vaoStaticArena = 0;
	}
	s_arenaInit = qfalse;
}

void RB_VBO_BeginFrame( void ) {
	if ( !s_arenaCvar ) {
		/* Default off: one store per frame orphans two large buffers a frame, which
		 * regressed frame time on Mali-class GPUs relative to the ring; set
		 * r_tessArena 1 to re-enable. */
		s_arenaCvar = ri.Cvar_Get( "r_tessArena", "0", 0 );
	}
	if ( !s_arenaCvar->integer ) {
		s_arenaActive = qfalse;
		s_arenaFull = qtrue;
		return;
	}

	if ( !s_vboInitialised ) {
		RB_VBO_Init();
	}
	RB_VboArenaInit();
	RB_VboBaseVertexInit();

	/* One store per frame.  A mid-frame flush re-enters here with the commands
	 * that are left, and must keep appending into the store the frame opened. */
	if ( s_arenaActive && RB_FrameCount() == s_arenaFrame ) {
		return;
	}
	s_arenaFrame = RB_FrameCount();

	s_arenaNext = ( s_arenaNext + 1 ) % RB_TESS_ARENA_COUNT;
	s_arenaVbo = s_vboArena[s_arenaNext];
	s_arenaEbo = s_eboArena[s_arenaNext];

	/* Discard the store this frame is about to fill: the driver may then hand
	 * back storage no in-flight command reads instead of waiting for one. */
	RB_BindVAO( s_vaoArena );
	RB_BindBuffer( GL_ARRAY_BUFFER, s_arenaVbo );
	qglBufferData( GL_ARRAY_BUFFER, RB_TESS_ARENA_VBO_BYTES, NULL, GL_STREAM_DRAW );
	RB_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_arenaEbo );
	qglBufferData( GL_ELEMENT_ARRAY_BUFFER, RB_TESS_ARENA_EBO_BYTES, NULL, GL_STREAM_DRAW );

	/* Base-vertex arm: the pointers are captured once per store, at offset 0. */
	if ( s_baseVertexDraw ) {
		RB_VBO_ConfigureAttribs();
	}

	if ( s_vaoStaticArena ) {
		RB_BindVAO( s_vaoStaticArena );
		RB_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_arenaEbo );
	}

	s_arenaVOff = 0;
	s_arenaIOff = 0;
	s_arenaFull = qfalse;
	s_arenaActive = qtrue;
}

/* Which arm the frame ran, and how much of it the store could not take.  Read
 * by the send row so the log is self-describing: without it a capture says what
 * happened but not which of the two was on. */
int RB_VboArenaMode( void ) {
	return s_arenaActive ? 1 : 0;
}

/* Append one batch to the frame store.  `staticVerts` says the vertex bytes are
 * already resident in the static world buffer and only the composed index list is
 * new.  Returns qfalse when the store has no room, which sends the batch to the
 * ring unchanged.  On success `*baseVertex` is the vertex the batch's index list
 * is relative to: zero where the vertices were already absolute (the static
 * arena), the store offset in vertices where they were just uploaded. */
static qboolean RB_VboArenaSend( int numVertexes, const glIndex_t *indexes, int numIndexes,
								 qboolean staticVerts, GLintptr *indexOffset, int *baseVertex ) {
	GLintptr	vOff = 0;
	GLsizeiptr	vBytes = 0;
	GLintptr	iOff;
	GLsizeiptr	iBytes;
	const glIndex_t	*src;
	int			k = 0;
	int			i;

	if ( !s_arenaActive ) {
		return qfalse;
	}
	if ( s_arenaFull ) {
		s_arenaFall++;
		return qfalse;
	}

	iOff = (GLintptr)PAD( s_arenaIOff, RB_TESS_ARENA_ALIGN );

	if ( staticVerts ) {
		for ( i = 0; i < s_staticRecCount; i++ ) {
			const rbStaticRec_t		*r = &s_staticRec[i];
			const rbStaticRegion_t	*e = RB_StaticFind( r->surf );
			int						j;

			for ( j = 0; j < r->numIdx; j++ ) {
				s_staticIdx[k++] = (glIndex_t)( e->first + ( indexes[r->idxStart + j] - r->vertStart ) );
			}
		}
		src = s_staticIdx;
		iBytes = (GLsizeiptr)( ( size_t )k * sizeof( glIndex_t ) );
	} else {
		vOff = RB_VboAlignUp( s_arenaVOff, RB_TESS_ARENA_VALIGN );
		vBytes = (GLsizeiptr)( ( size_t )numVertexes * RB_VERTEX_STRIDE );
		src = indexes;
		iBytes = (GLsizeiptr)( ( size_t )numIndexes * sizeof( glIndex_t ) );
	}

	if ( vOff + vBytes > RB_TESS_ARENA_VBO_BYTES
		 || iOff + iBytes > RB_TESS_ARENA_EBO_BYTES ) {
		s_arenaFull = qtrue;
		s_arenaFall++;
		return qfalse;
	}

	RB_BindVAO( staticVerts ? s_vaoStaticArena : s_vaoArena );

	RB_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_arenaEbo );	/* at the point of use */

	if ( !staticVerts ) {
		RB_BindBuffer( GL_ARRAY_BUFFER, s_arenaVbo );
		qglBufferSubData( GL_ARRAY_BUFFER, vOff, vBytes, s_tessStage );
		if ( s_baseVertexDraw ) {
			/* the store VAO holds the pointers; the offset goes with the draw */
			*baseVertex = (int)( vOff / RB_VERTEX_STRIDE );
		} else {
			RB_VboAttribsAt( vOff );
			*baseVertex = 0;
		}
		s_arenaVOff = (GLintptr)( vOff + vBytes );
	} else {
		*baseVertex = 0;
	}

	qglBufferSubData( GL_ELEMENT_ARRAY_BUFFER, iOff, iBytes, src );
	s_arenaIOff = (GLintptr)( iOff + iBytes );

	*indexOffset = iOff;
	return qtrue;
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
		qboolean	localDraw = qfalse;
		qboolean	stored;
		int			writes;
		int			storedBase = 0;
		GLintptr	storedIndex = 0;

		pack.input = input;
		pack.texcoord0 = texcoord0;
		pack.texcoord1 = texcoord1;
		pack.colors = colors;
		pack.normals = normals;
		pack.tcStrideFloats = tcStrideFloats;

		subT0 = BE_On() ? BE_Microseconds() : 0;
		JobPool_ParallelRange( input->numVertexes, RB_VBO_PACK_CHUNK, RB_VboPackRange, &pack );
		BE_Sub( BE_FAM_DS, BE_DSZ_sub_pack, BE_Microseconds() - subT0 );

		/* A static world batch is baked from that pack.  Where the bake left every
		 * record in the device-local arenas the batch is drawn straight out of
		 * them and the frame makes no store write at all; otherwise it takes the
		 * composed send, which is the same index bytes. */
		staticDraw = RB_StaticBatch( input, numIndexes, texcoord0, texcoord1,
									 tcStrideFloats, colors, normals )
					 && RB_VboStaticBake( indexes );

		if ( BE_On() ) {
			/* which condition refused the batch; the acceptances are the row's own
			 * `st`, so reasons plus st is the batch count */
			RB_StaticGateAccount( input, numIndexes, texcoord0, texcoord1,
								  tcStrideFloats, colors, normals );
		}

		subT0 = BE_On() ? BE_Microseconds() : 0;
		writes = 0;
		stored = qfalse;
		if ( staticDraw ) {
			localDraw = RB_VboStaticLocalPrepare( indexes, numIndexes );
		}
		if ( !localDraw ) {
			stored = RB_VboArenaSend( input->numVertexes, indexes, numIndexes, staticDraw,
									  &storedIndex, &storedBase );
			if ( stored ) {
				writes = staticDraw ? 1 : 2;
			} else if ( staticDraw ) {
				RB_VboStaticSend( indexes, numIndexes );
				writes = 1;
			} else {
				RB_VboSendTess( input->numVertexes, indexes, numIndexes );
				writes = 2;
			}
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
			s_sendWrites += writes;
			s_sendVerts += input->numVertexes;
			if ( staticDraw ) {
				s_sendStatics++;
			}
			if ( localDraw ) {
				s_sendLocal++;
				if ( s_staticRecCount > 1 ) {
					if ( s_multiDraw ) {
						s_sendMulti++;
					} else {
						s_sendSeq++;
					}
				}
			}
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
		if ( localDraw ) {
			RB_VboStaticLocalDraw();
		} else if ( stored ) {
			if ( storedBase ) {
				qglDrawElementsBaseVertex( GL_TRIANGLES, numIndexes, GL_UNSIGNED_INT,
										   ( const void * )(uintptr_t)storedIndex, storedBase );
			} else {
				qglDrawElements( GL_TRIANGLES, numIndexes, GL_UNSIGNED_INT,
								 ( const void * )(uintptr_t)storedIndex );
			}
		} else {
			qglDrawElements( GL_TRIANGLES, numIndexes, GL_UNSIGNED_INT, ( const void * )0 );
		}
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
