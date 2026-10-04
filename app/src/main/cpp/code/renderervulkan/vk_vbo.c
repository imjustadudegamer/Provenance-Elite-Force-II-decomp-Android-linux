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
/*
 * vk_vbo.c -- the static device-buffer path.
 *
 * The world BSP lightmapped multitextured surfaces are geometry whose packed
 * vertex stream is a pure function of the map data, yet the walk re-generated
 * and re-uploaded every one of them every frame.  This module is the reference
 * renderer's architecture for that class: each surface is packed once into a
 * device-local vertex arena and its rebased index block into a device-local
 * index arena, and the draw reads those instead of the frame's streaming rings.
 * Device-local runs of at least MIN_IBO_RUN are drawn where they lie; the
 * remaining short runs are grouped into a host-visible index buffer and drawn
 * in a single call, in index order.  That compose is the one buffer here a
 * recorded draw reads by byte offset and a later walk rewrites, so it is per
 * frame in flight like every other shared surface on this path.
 *
 * The residency key is the surface pointer, exactly as the reference's
 * `vboItemIndex' is stamped on the surface: RE_BeginRegistration frees the
 * previous map's surfaces, and RB_VboStaticForget drops the table there.
 */

#include "tr_local.h"
#include "tr_vbo.h"

#include <string.h>

/* The arena is 16 MiB of packed vertexes, so a device keeps both the
   device-local copy and the host staging copy inside a modest budget; the index
   arena is the reference's three indexes a vertex. */
#define VK_STATIC_ARENA_BYTES   ( 16u * 1024u * 1024u )
#define VK_STATIC_ARENA_VERTS   ( VK_STATIC_ARENA_BYTES / (unsigned)VK_STREAM_STRIDE )
#define VK_STATIC_ARENA_INDEXES ( VK_STATIC_ARENA_VERTS * 3u )
#define VK_STATIC_HASH_BITS     14
#define VK_STATIC_HASH_SIZE     ( 1u << VK_STATIC_HASH_BITS )
#define VK_STATIC_HASH_MASK     ( VK_STATIC_HASH_SIZE - 1u )

/* One host-visible compose holds one batch: a batch is bounded by
   SHADER_MAX_INDEXES by construction, so the buffer is too. */
#define VK_STATIC_SOFT_INDEXES  SHADER_MAX_INDEXES

typedef struct {
	const void	*surf;
	int			first;			/* arena vertex offset */
	int			numVerts;
	int			idxFirst;		/* arena index offset */
	int			numIdx;
} vkStaticRegion_t;

typedef struct {
	const void	*surf;
	int			vertStart;		/* position within the batch's tess stream */
	int			numVerts;
	int			idxStart;
	int			numIdx;
	qboolean	resident;		/* its generator did not run this frame */
	int			region;			/* hash slot, -1 while streamed */
} vkStaticRec_t;

/* batch modes */
enum {
	VK_SB_UNKNOWN = 0,	/* nothing decided this batch yet */
	VK_SB_ARENA,		/* every surface so far is resident: skip the generator */
	VK_SB_STREAM		/* at least one surface streams: the whole batch streams */
};

static VkBuffer			s_devVbo, s_devIbo;
static VkDeviceMemory	s_devVboMem, s_devIboMem;
static VkBuffer			s_stageVbo, s_stageIbo;
static VkDeviceMemory	s_stageVboMem, s_stageIboMem;
static byte				*s_stageVboMap;
static vkIndex_t		*s_stageIboMap;

/* The short-run compose, one per frame in flight.  The vertex/index rings
   (vk_vertex.c) and this path's transfer command buffers are per slot for the
   same reason: a slot's command buffer may be read by the GPU until its own
   fence signals, and VK_Frame_Begin only waits the slot it is re-recording, so
   the other slot's frame can still be in flight.  A compose the two slots
   share is rewritten by the next slot's walk while that draw reads it, and the
   draw then reads another batch's index block -- valid indices, wrong
   triangles, and no row anywhere, because nothing about the draw is invalid. */
static VkBuffer			s_softIbo[VK_FRAMES_IN_FLIGHT];
static VkDeviceMemory	s_softIboMem[VK_FRAMES_IN_FLIGHT];
static vkIndex_t		*s_softMap[VK_FRAMES_IN_FLIGHT];

static vkStaticRegion_t	s_hash[VK_STATIC_HASH_SIZE];
static vkStaticRec_t	s_rec[VK_STATIC_BATCH_MAX];
static int				s_recCount;
static qboolean			s_recOverflow;
static int				s_batchMode;

static int				s_arenaVerts;
static int				s_arenaIdxes;
static qboolean			s_arenaFull;
static qboolean			s_forgetPending;
static qboolean			s_ready;
static cvar_t			*s_cvar;

/* what the last arena draw actually read from: the census reports the submitted
   geometry beside the expected stage set.  0 none, 1 device runs, 2 the compose */
static int				s_lastRunMode;
static int				s_lastRunIdx;
static int				s_composeDraws;
static int				s_devRunDraws;

/* what the row reports */
static int				s_fastSurfaces;
static int				s_bakedSurfaces;
static int				s_arenaBatches;
static int				s_streamBatches;

/* ---- the bake's transfers ------------------------------------------------- */

/* The bake copies the ring's packed bytes into the device-local arena, and
   vkCmdCopyBuffer may not be recorded inside a render pass -- the walk is
   inside one for the whole frame, and the validation layer named it
   (VUID-vkCmdCopyBuffer-renderpass, once per baked surface).  The copies go
   into a second command buffer for the same frame, submitted ahead of the
   frame's own buffer, so queue order still puts them before every draw of the
   frame that reads the arena: the order recording them inline gave, without the
   invalid call site. */
static VkCommandBuffer	s_xferCmd[VK_FRAMES_IN_FLIGHT];
static qboolean			s_xferReady[VK_FRAMES_IN_FLIGHT];
static qboolean			s_xferOpen[VK_FRAMES_IN_FLIGHT];

static VkCommandBuffer VK_StaticXferCmd( void )
{
	VkCommandBufferAllocateInfo	allocInfo;
	VkCommandBufferBeginInfo	beginInfo;
	int							slot = RB_FrameSmpFrame();

	if ( slot < 0 || slot >= VK_FRAMES_IN_FLIGHT ) {
		slot = 0;
	}
	if ( s_xferOpen[slot] ) {
		return s_xferCmd[slot];
	}

	if ( !s_xferReady[slot] ) {
		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		/* This buffer records ACROSS the frame -- the bake copies land in it as
		   the walk bakes and it is ended at submit.  A pool is host-synchronised
		   while its buffers record (Vulkan 1.4 s6.2, vkspec.txt:14741), so it
		   must come from the WALK THREAD's own pool, never from a pool the main
		   thread's uploads share. */
		allocInfo.commandPool = VK_Upload_Pool();
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;

		if ( !VK_Guard_Result( VK_GSITE_SYNC,
							   VK_Pool_AllocBuffers( VK_Upload_Pool(), &allocInfo, &s_xferCmd[slot] ),
							   "vkAllocateCommandBuffers(static transfer)" ) ) {
			return VK_NULL_HANDLE;
		}
		s_xferReady[slot] = qtrue;
	}

	qvk.ResetCommandBuffer( s_xferCmd[slot], 0 );

	memset( &beginInfo, 0, sizeof( beginInfo ) );
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.BeginCommandBuffer( s_xferCmd[slot], &beginInfo ),
						   "vkBeginCommandBuffer(static transfer)" ) ) {
		return VK_NULL_HANDLE;
	}

	s_xferOpen[slot] = qtrue;
	return s_xferCmd[slot];
}

/* The frame's transfer buffer, ended and barriered, or VK_NULL_HANDLE when the
   walk baked nothing this frame.  The barrier makes the copies' writes visible
   to the vertex and index input of the frame buffer that follows in the same
   submission. */
VkCommandBuffer VK_VboStaticXferClose( int slot )
{
	VkMemoryBarrier	barrier;

	if ( slot < 0 || slot >= VK_FRAMES_IN_FLIGHT || !s_xferOpen[slot] ) {
		return VK_NULL_HANDLE;
	}

	memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT;

	qvk.CmdPipelineBarrier( s_xferCmd[slot],
							VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT,
							VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
							0, 1, &barrier, 0, NULL, 0, NULL );

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.EndCommandBuffer( s_xferCmd[slot] ),
						   "vkEndCommandBuffer(static transfer)" ) ) {
		s_xferOpen[slot] = qfalse;
		return VK_NULL_HANDLE;
	}

	s_xferOpen[slot] = qfalse;
	return s_xferCmd[slot];
}


static unsigned VK_StaticHash( const void *p )
{
	size_t	v = (size_t)p >> 4;

	return (unsigned)( ( v * (size_t)2654435761u ) >> ( ( sizeof( size_t ) * 8 ) - VK_STATIC_HASH_BITS ) );
}

/* Lookup, or the insertion point for a new key.  NULL only when the table is full. */
static vkStaticRegion_t *VK_StaticFind( const void *surf )
{
	unsigned	i;

	if ( !surf ) {
		return NULL;
	}

	for ( i = 0; i < VK_STATIC_HASH_SIZE; i++ ) {
		vkStaticRegion_t	*e = &s_hash[( VK_StaticHash( surf ) + i ) & VK_STATIC_HASH_MASK];

		if ( e->surf == surf ) {
			return e;
		}
		if ( !e->surf ) {
			return e;
		}
	}

	return NULL;
}

static qboolean VK_StaticEnabled( void )
{
	if ( !s_cvar ) {
		s_cvar = ri.Cvar_Get( "r_vboStatic", "1", 0 );
	}
	return s_cvar->integer != 0;
}

/* Everything the class test can know before a generator runs: the batch is the
   world's, the lightmapped multitexture iterator's, unlit, and the packed
   stream's sources are the invariant ones.  Anything else keeps streaming. */
static qboolean VK_StaticClassOK( const shaderCommands_t *in )
{
	if ( !in->shader || in->shader->needsNormal || in->shader->needsLSpherical ) {
		return qfalse;
	}
	if ( in->currentStageIteratorFunc != RB_StageIteratorLightmappedMultitextureUnfogged ) {
		return qfalse;
	}
	if ( backEnd.currentEntity != &tr.worldEntity ) {
		return qfalse;
	}
	if ( in->dlightBits ) {
		return qfalse;
	}
	if ( r_showtris->integer || r_shownormals->integer ) {
		return qfalse;
	}
	return qtrue;
}

/* A surface's own counts, without running its generator.  -1 when the surface
   has a path this arena does not model (the static model LOD arm). */
static int VK_StaticCounts( const void *surf, int type, int *vertsOut, int *idxOut )
{
	if ( type == SF_FACE ) {
		const srfSurfaceFace_t	*f = (const srfSurfaceFace_t *)surf;

		*vertsOut = f->numPoints;
		*idxOut = f->numIndices;
		return 0;
	}
	if ( type == SF_TRIANGLES ) {
		const srfTriangles_t	*t = (const srfTriangles_t *)surf;

		if ( t->verts[2].lodExtra != -1000.0f && t->verts[2].lodExtra != -1001.0f ) {
			return -1;
		}
		*vertsOut = t->numVerts;
		*idxOut = t->numIndexes;
		return 0;
	}
	return -1;
}

/* The resident region for a surface whose counts match what the arena baked. */
static vkStaticRegion_t *VK_StaticResident( const void *surf, int numVerts, int numIdx )
{
	vkStaticRegion_t	*e = VK_StaticFind( surf );

	if ( !e || e->surf != surf ) {
		return NULL;
	}
	if ( e->numVerts != numVerts || e->numIdx != numIdx ) {
		return NULL;
	}
	return e;
}

/* Advance the tess counters exactly as the generator would have, and log the
   arena region this batch draws from.  No tess array is written. */
static void VK_StaticSkip( const void *surf, int type, int vertBase, int idxBase,
						   int numVerts, int numIdx, int region )
{
	int		ndx;

	if ( type == SF_TRIANGLES ) {
		backEnd.pc.c_indexes += numIdx;
		backEnd.pc.c_indexBytes += numIdx * tess.numPasses;
		backEnd.pc.c_vertexes += numVerts;
		tess.entityFade = ( (const srfTriangles_t *)surf )->shaderTime;
	}

	tess.numIndexes += numIdx;
	tess.numVertexes += numVerts;
	tess.vertexColorValid = qtrue;

	/* the per-vertex dlight bits the generator writes: zero, the only value the
	   class admits, but written so the array never carries a stale set */
	for ( ndx = vertBase; ndx < vertBase + numVerts; ndx++ ) {
		tess.vertexDlightBits[ndx] = 0;
	}

	s_rec[s_recCount].surf = surf;
	s_rec[s_recCount].vertStart = vertBase;
	s_rec[s_recCount].numVerts = numVerts;
	s_rec[s_recCount].idxStart = idxBase;
	s_rec[s_recCount].numIdx = numIdx;
	s_rec[s_recCount].resident = qtrue;
	s_rec[s_recCount].region = region;
	s_recCount++;
}

qboolean RB_VboStaticTrySurface( const void *surf, int type )
{
	int						numVerts = 0, numIdx = 0;
	vkStaticRegion_t		*e;

	if ( !s_ready || !VK_StaticEnabled() || !tess.shader ) {
		return qfalse;
	}

	if ( VK_StaticCounts( surf, type, &numVerts, &numIdx ) < 0 || !VK_StaticClassOK( &tess ) ) {
		e = NULL;
	} else {
		e = VK_StaticResident( surf, numVerts, numIdx );
	}

	/* A batch whose counters are already up but which has not decided is one a
	   generator split from inside: RB_BeginSurface reset the mode and the
	   generator may not have recorded (a surface class this arena does not
	   model).  Its tess arrays are written, so nothing here may skip. */
	if ( s_batchMode == VK_SB_UNKNOWN && ( tess.numVertexes > 0 || tess.numIndexes > 0 ) ) {
		s_batchMode = VK_SB_STREAM;
		s_streamBatches++;
		return qfalse;
	}

	/* An arena batch that meets a surface it cannot draw is closed here, at the
	   same boundary the generator's own overflow split uses, so the tess arrays
	   this batch never wrote can never be read by a fallback. */
	if ( s_batchMode == VK_SB_ARENA && !e ) {
		s_streamBatches++;
		RB_EndSurface();
		RB_BeginSurface( tess.shader, tess.fogNum );
		s_batchMode = VK_SB_STREAM;
		return qfalse;
	}

	if ( s_batchMode == VK_SB_STREAM ) {
		return qfalse;
	}

	if ( !e ) {
		if ( s_batchMode == VK_SB_UNKNOWN ) {
			s_batchMode = VK_SB_STREAM;
			s_streamBatches++;
		}
		return qfalse;
	}

	if ( s_batchMode == VK_SB_UNKNOWN ) {
		s_batchMode = VK_SB_ARENA;
		s_arenaBatches++;
	}
	if ( s_recCount >= VK_STATIC_BATCH_MAX ) {
		s_recOverflow = qtrue;
		RB_EndSurface();
		RB_BeginSurface( tess.shader, tess.fogNum );
		s_batchMode = VK_SB_STREAM;
		s_streamBatches++;
		return qfalse;
	}

	/* the generator's own overflow boundary, evaluated on the counts it would
	   have advanced: the split and therefore the batch composition are unchanged */
	if ( tess.numVertexes + numVerts >= SHADER_MAX_VERTEXES
		 || tess.numIndexes + numIdx >= SHADER_MAX_INDEXES ) {
		RB_EndSurface();
		RB_BeginSurface( tess.shader, tess.fogNum );
		s_batchMode = VK_SB_STREAM;
		s_streamBatches++;
		return qfalse;
	}

	s_fastSurfaces++;
	VK_StaticSkip( surf, type, tess.numVertexes, tess.numIndexes, numVerts, numIdx,
				   (int)( e - s_hash ) );
	return qtrue;
}

void RB_VboStaticReset( void )
{
	s_recCount = 0;
	s_recOverflow = qfalse;
	s_batchMode = VK_SB_UNKNOWN;
	/* the census reads these as THIS batch's geometry; a streamed batch never
	   calls RB_VboStaticDrawRuns, so without the reset it would report the
	   previous arena batch's run and a streamed miss would read as composed */
	s_lastRunMode = 0;
	s_lastRunIdx = 0;
}

void RB_VboStaticRecord( const void *surf, int vertStart, int numVerts, int idxStart, int numIdx )
{
	if ( s_recOverflow || numVerts <= 0 || numIdx <= 0 ) {
		return;
	}
	if ( s_recCount >= VK_STATIC_BATCH_MAX ) {
		s_recOverflow = qtrue;
		return;
	}

	/* a generator ran: whatever this batch was before, it is a streamed batch now */
	s_batchMode = VK_SB_STREAM;

	s_rec[s_recCount].surf = surf;
	s_rec[s_recCount].vertStart = vertStart;
	s_rec[s_recCount].numVerts = numVerts;
	s_rec[s_recCount].idxStart = idxStart;
	s_rec[s_recCount].numIdx = numIdx;
	s_rec[s_recCount].resident = qfalse;
	s_rec[s_recCount].region = -1;
	s_recCount++;
}

void RB_VboStaticForget( void )
{
	/* The new map's surfaces are not the old map's, and the old pointers may be
	   handed straight back.  The walk thread applies it, so a frame in flight
	   through the arena is never rewritten under the GPU. */
	s_forgetPending = qtrue;
}

static void VK_StaticApplyForget( void )
{
	if ( !s_forgetPending ) {
		return;
	}
	s_forgetPending = qfalse;
	memset( s_hash, 0, sizeof( s_hash ) );
	s_arenaVerts = 0;
	s_arenaIdxes = 0;
	s_arenaFull = qfalse;
	RB_VboStaticReset();
}

/* ---- the bake ------------------------------------------------------------- */

static qboolean VK_StaticBake( const vkStaticRec_t *r, const vkIndex_t *indexes, int ringByteBase )
{
	VkCommandBuffer		cmd;
	VkBufferCopy		copyRegion;
	vkStaticRegion_t	*e;
	int					j;

	e = VK_StaticFind( r->surf );
	if ( !e ) {
		s_arenaFull = qtrue;
		return qfalse;
	}
	if ( e->surf == r->surf ) {
		return qtrue;			/* already resident */
	}
	if ( s_arenaFull
		 || s_arenaVerts + r->numVerts > (int)VK_STATIC_ARENA_VERTS
		 || s_arenaIdxes + r->numIdx > (int)VK_STATIC_ARENA_INDEXES ) {
		s_arenaFull = qtrue;
		return qfalse;
	}

	e->surf = r->surf;
	e->first = s_arenaVerts;
	e->numVerts = r->numVerts;
	e->idxFirst = s_arenaIdxes;
	e->numIdx = r->numIdx;

	/* the vertexes the pack just wrote into the ring, byte for byte */
	memcpy( s_stageVboMap + (size_t)e->first * VK_STREAM_STRIDE,
			(const byte *)VK_VertexRing_Mapped() + ringByteBase
				+ (size_t)r->vertStart * VK_STREAM_STRIDE,
			(size_t)r->numVerts * VK_STREAM_STRIDE );

	/* the rebase the streaming pack would have made, written once */
	for ( j = 0; j < r->numIdx; j++ ) {
		s_stageIboMap[e->idxFirst + j] =
			(vkIndex_t)( e->first + ( indexes[r->idxStart + j] - r->vertStart ) );
	}

	/* not VK_Shade_CommandBuffer(): the frame buffer is inside its render pass
	   for the whole walk, and a buffer copy may not be recorded there */
	cmd = VK_StaticXferCmd();
	if ( cmd == VK_NULL_HANDLE ) {
		return qfalse;
	}

	memset( &copyRegion, 0, sizeof( copyRegion ) );
	copyRegion.srcOffset = (VkDeviceSize)( (size_t)e->first * VK_STREAM_STRIDE );
	copyRegion.dstOffset = copyRegion.srcOffset;
	copyRegion.size = (VkDeviceSize)( (size_t)r->numVerts * VK_STREAM_STRIDE );
	qvk.CmdCopyBuffer( cmd, s_stageVbo, s_devVbo, 1, &copyRegion );

	memset( &copyRegion, 0, sizeof( copyRegion ) );
	copyRegion.srcOffset = (VkDeviceSize)( (size_t)e->idxFirst * sizeof( vkIndex_t ) );
	copyRegion.dstOffset = copyRegion.srcOffset;
	copyRegion.size = (VkDeviceSize)( (size_t)r->numIdx * sizeof( vkIndex_t ) );
	qvk.CmdCopyBuffer( cmd, s_stageIbo, s_devIbo, 1, &copyRegion );

	s_arenaVerts += r->numVerts;
	s_arenaIdxes += r->numIdx;
	s_bakedSurfaces++;

	return qtrue;
}

void RB_VboStaticBakeFrame( int ringByteBase )
{
	int		i;

	if ( !s_ready || !VK_StaticEnabled() || s_recCount <= 0 || s_recOverflow ) {
		return;
	}

	for ( i = 0; i < s_recCount; i++ ) {
		if ( s_rec[i].resident ) {
			continue;
		}
		if ( !VK_StaticBake( &s_rec[i], tess.indexes, ringByteBase ) ) {
			break;			/* the arena is full: the rest keep streaming */
		}
		s_rec[i].region = (int)( VK_StaticFind( s_rec[i].surf ) - s_hash );
	}
}

/* ---- the draw ------------------------------------------------------------- */

int RB_VboStaticBatchItems( vkStaticItemView_t *out, int max )
{
	int		i, v = 0, n = 0;

	if ( !s_ready || !VK_StaticEnabled() || s_batchMode != VK_SB_ARENA ) {
		return -1;
	}
	if ( s_recCount <= 0 || s_recOverflow || s_recCount > max ) {
		return -1;
	}

	for ( i = 0; i < s_recCount; i++ ) {
		const vkStaticRec_t		*r = &s_rec[i];
		const vkStaticRegion_t	*e;

		if ( !r->resident || r->vertStart != v || r->idxStart != n ) {
			return -1;
		}
		e = ( r->region >= 0 ) ? &s_hash[r->region] : NULL;
		if ( !e || e->surf != r->surf || e->numVerts != r->numVerts || e->numIdx != r->numIdx ) {
			return -1;
		}
		out[i].vertStart = r->vertStart;
		out[i].numVerts = r->numVerts;
		out[i].first = e->first;
		out[i].idxFirst = e->idxFirst;
		out[i].numIdx = e->numIdx;
		v += r->numVerts;
		n += r->numIdx;
	}

	if ( v != tess.numVertexes || n != tess.numIndexes ) {
		return -1;
	}

	return s_recCount;
}

const float *RB_VboStaticStageFloats( void )
{
	return (const float *)s_stageVboMap;
}

qboolean RB_VboStaticArenaBatch( void )
{
	return ( s_batchMode == VK_SB_ARENA ) ? qtrue : qfalse;
}

/* The compose a draw this slot records into.  The slot is the one the walk's
   command buffer comes from, so the compose is owned by exactly the frame that
   reads it. */
static int VK_SoftSlot( void )
{
	int slot = RB_FrameSmpFrame();

	return ( slot >= 0 && slot < VK_FRAMES_IN_FLIGHT ) ? slot : 0;
}

/* The invariant the per-slot compose rests on, asserted rather than assumed:
   two frames in flight must not resolve to one buffer.  A single compose is
   what this path had, and the row names it if an arm reintroduces the sharing,
   instead of the sharing showing up as another batch's triangles.  One pointer
   compare per slot, on the census cadence, behind the master gate. */
static void VK_SoftAliasCheck( void )
{
	static int	s_rows;
	int			a, b;

	if ( !R_ProbeStatsOn() ) {
		return;
	}

	s_rows++;
	if ( s_rows > 8 && ( s_rows % 600 ) != 0 ) {
		return;
	}

	for ( a = 0; a < VK_FRAMES_IN_FLIGHT; a++ ) {
		if ( s_softIbo[a] == VK_NULL_HANDLE ) {
			continue;
		}
		for ( b = a + 1; b < VK_FRAMES_IN_FLIGHT; b++ ) {
			if ( s_softIbo[a] == s_softIbo[b] || s_softMap[a] == s_softMap[b] ) {
				VK_Guard_Hit( VK_GSITE_SYNC,
							  "the soft index compose is shared by two frames in flight",
							  VK_INCOMPLETE );
				return;
			}
		}
	}
}

void RB_VboStaticDrawRuns( VkCommandBuffer cmd, int numIndexes,
						   const vkStaticItemView_t *items, int numItems )
{
	int		i = 0;
	int		slot = VK_SoftSlot();
	int		softFirst = 0;
	int		softPending = 0;
	int		runTotal = 0;

	if ( !s_ready || numItems <= 0 ) {
		return;
	}

	( void )numIndexes;

	VK_SoftAliasCheck();

	s_lastRunMode = 0;
	s_lastRunIdx = 0;

	VK_Draw_BindVertexBuffer( cmd, s_devVbo, 0 );

	while ( i < numItems ) {
		int		j = i;

		/* the longest run of items whose index blocks are adjacent in the arena */
		runTotal = items[i].numIdx;
		while ( j + 1 < numItems
				&& items[j + 1].idxFirst == items[j].idxFirst + items[j].numIdx ) {
			j++;
			runTotal += items[j].numIdx;
		}

		if ( runTotal >= VK_STATIC_MIN_IBO_RUN ) {
			if ( softPending > 0 ) {
				VK_Draw_BindIndexBuffer( cmd, s_softIbo[slot] );
				VK_Draw_DrawIndexed( cmd, softPending, softFirst, 0 );
				softFirst += softPending;
				softPending = 0;
			}
			VK_Draw_BindIndexBuffer( cmd, s_devIbo );
			VK_Draw_DrawIndexed( cmd, runTotal, items[i].idxFirst, 0 );
			s_lastRunMode = 1;
			s_lastRunIdx += runTotal;
			s_devRunDraws++;
		} else {
			int	k;

			for ( k = i; k <= j; k++ ) {
				if ( softPending + items[k].numIdx > VK_STATIC_SOFT_INDEXES ) {
					VK_Draw_BindIndexBuffer( cmd, s_softIbo[slot] );
					VK_Draw_DrawIndexed( cmd, softPending, softFirst, 0 );
					softFirst += softPending;
					softPending = 0;
				}
				memcpy( s_softMap[slot] + softPending, s_stageIboMap + items[k].idxFirst,
						(size_t)items[k].numIdx * sizeof( vkIndex_t ) );
				softPending += items[k].numIdx;
				s_lastRunMode = 2;
				s_lastRunIdx += items[k].numIdx;
				s_composeDraws++;
			}
		}

		i = j + 1;
	}

	if ( softPending > 0 ) {
		VK_Draw_BindIndexBuffer( cmd, s_softIbo[slot] );
		VK_Draw_DrawIndexed( cmd, softPending, softFirst, 0 );
	}
}

/* the geometry the last arena batch actually submitted, for the census */
void RB_VboStaticLastRun( int *modeOut, int *numIdxOut )
{
	if ( modeOut ) {
		*modeOut = s_lastRunMode;
	}
	if ( numIdxOut ) {
		*numIdxOut = s_lastRunIdx;
	}
}

void RB_VboStaticRow( void )
{
	static int	rows;
	static int	lastFast = -1;
	int			fast = s_fastSurfaces;
	int			baked = s_bakedSurfaces;

	rows++;
	if ( rows == 1 || ( rows % 600 ) == 0 || fast != lastFast ) {
		VK_Guard_Row( "VK:vbo",
					  "fast=%d baked=%d arena=%d/%d verts (%d) idxes (%d) full=%d "
					  "batches arena=%d stream=%d compose=%d devrun=%d f=%d",
					  fast, baked, s_arenaVerts, (int)VK_STATIC_ARENA_VERTS,
					  s_arenaIdxes, (int)VK_STATIC_ARENA_INDEXES,
					  s_arenaFull ? 1 : 0, s_arenaBatches, s_streamBatches,
					  s_composeDraws, s_devRunDraws, RB_FrameCount() );
	}
	lastFast = fast;
}

/* ---- lifetime ------------------------------------------------------------- */

void RB_VBO_BeginFrame( void )
{
	VK_StaticApplyForget();
	RB_VboStaticReset();
	RB_VboStaticRow();
}

qboolean RB_VBO_Init( void )
{
	int i;

	if ( s_ready ) {
		return qtrue;
	}

	memset( s_hash, 0, sizeof( s_hash ) );
	memset( s_rec, 0, sizeof( s_rec ) );
	memset( s_softIbo, 0, sizeof( s_softIbo ) );
	memset( s_softIboMem, 0, sizeof( s_softIboMem ) );
	memset( s_softMap, 0, sizeof( s_softMap ) );
	s_recCount = 0;
	s_recOverflow = qfalse;
	s_batchMode = VK_SB_UNKNOWN;
	s_arenaVerts = 0;
	s_arenaIdxes = 0;
	s_arenaFull = qfalse;
	s_forgetPending = qfalse;

	if ( !VK_Memory_CreateBuffer( (VkDeviceSize)VK_STATIC_ARENA_VERTS * VK_STREAM_STRIDE,
								  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &s_devVbo, &s_devVboMem ) ) {
		return qfalse;
	}
	if ( !VK_Memory_CreateBuffer( (VkDeviceSize)VK_STATIC_ARENA_INDEXES * sizeof( vkIndex_t ),
								  VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &s_devIbo, &s_devIboMem ) ) {
		return qfalse;
	}
	if ( !VK_Memory_CreateBuffer( (VkDeviceSize)VK_STATIC_ARENA_VERTS * VK_STREAM_STRIDE,
								  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
								  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
								  &s_stageVbo, &s_stageVboMem ) ) {
		return qfalse;
	}
	if ( !VK_Guard_Result( VK_GSITE_ALLOC,
						   qvk.MapMemory( vk.device, s_stageVboMem, 0, VK_WHOLE_SIZE, 0,
										  (void **)&s_stageVboMap ),
						   "vkMapMemory(static vertex arena)" ) ) {
		return qfalse;
	}
	if ( !VK_Memory_CreateBuffer( (VkDeviceSize)VK_STATIC_ARENA_INDEXES * sizeof( vkIndex_t ),
								  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
								  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
								  &s_stageIbo, &s_stageIboMem ) ) {
		return qfalse;
	}
	if ( !VK_Guard_Result( VK_GSITE_ALLOC,
						   qvk.MapMemory( vk.device, s_stageIboMem, 0, VK_WHOLE_SIZE, 0,
										  (void **)&s_stageIboMap ),
						   "vkMapMemory(static index arena)" ) ) {
		return qfalse;
	}
	/* One compose per frame in flight: the draw a slot records reads it until
	   that slot's fence signals, and the other slot's frame may still be in
	   flight when the next walk writes. */
	for ( i = 0; i < VK_FRAMES_IN_FLIGHT; i++ ) {
		if ( !VK_Memory_CreateBuffer( (VkDeviceSize)VK_STATIC_SOFT_INDEXES * sizeof( vkIndex_t ),
									  VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
									  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
									  &s_softIbo[i], &s_softIboMem[i] ) ) {
			return qfalse;
		}
		if ( !VK_Guard_Result( VK_GSITE_ALLOC,
							   qvk.MapMemory( vk.device, s_softIboMem[i], 0, VK_WHOLE_SIZE, 0,
											  (void **)&s_softMap[i] ),
							   "vkMapMemory(static soft indexes)" ) ) {
			return qfalse;
		}
	}

	s_ready = qtrue;

	VK_Guard_Row( "VK:vbo", "arena vertexes=%u (%.1f MiB) indexes=%u (%.1f MiB) soft=%d x %d run=%d",
				  VK_STATIC_ARENA_VERTS,
				  (double)VK_STATIC_ARENA_VERTS * VK_STREAM_STRIDE / ( 1024.0 * 1024.0 ),
				  VK_STATIC_ARENA_INDEXES,
				  (double)VK_STATIC_ARENA_INDEXES * sizeof( vkIndex_t ) / ( 1024.0 * 1024.0 ),
				  VK_STATIC_SOFT_INDEXES, VK_FRAMES_IN_FLIGHT, VK_STATIC_MIN_IBO_RUN );

	VK_SoftAliasCheck();

	return qtrue;
}

void RB_VBO_Shutdown( void )
{
	int i;

	if ( vk.device == VK_NULL_HANDLE ) {
		return;
	}

	if ( s_stageVboMap ) { qvk.UnmapMemory( vk.device, s_stageVboMem ); s_stageVboMap = NULL; }
	if ( s_stageIboMap ) { qvk.UnmapMemory( vk.device, s_stageIboMem ); s_stageIboMap = NULL; }

	if ( s_devVbo != VK_NULL_HANDLE ) { qvk.DestroyBuffer( vk.device, s_devVbo, NULL ); s_devVbo = VK_NULL_HANDLE; }
	if ( s_devVboMem != VK_NULL_HANDLE ) { qvk.FreeMemory( vk.device, s_devVboMem, NULL ); s_devVboMem = VK_NULL_HANDLE; }
	if ( s_devIbo != VK_NULL_HANDLE ) { qvk.DestroyBuffer( vk.device, s_devIbo, NULL ); s_devIbo = VK_NULL_HANDLE; }
	if ( s_devIboMem != VK_NULL_HANDLE ) { qvk.FreeMemory( vk.device, s_devIboMem, NULL ); s_devIboMem = VK_NULL_HANDLE; }
	if ( s_stageVbo != VK_NULL_HANDLE ) { qvk.DestroyBuffer( vk.device, s_stageVbo, NULL ); s_stageVbo = VK_NULL_HANDLE; }
	if ( s_stageVboMem != VK_NULL_HANDLE ) { qvk.FreeMemory( vk.device, s_stageVboMem, NULL ); s_stageVboMem = VK_NULL_HANDLE; }
	if ( s_stageIbo != VK_NULL_HANDLE ) { qvk.DestroyBuffer( vk.device, s_stageIbo, NULL ); s_stageIbo = VK_NULL_HANDLE; }
	if ( s_stageIboMem != VK_NULL_HANDLE ) { qvk.FreeMemory( vk.device, s_stageIboMem, NULL ); s_stageIboMem = VK_NULL_HANDLE; }
	for ( i = 0; i < VK_FRAMES_IN_FLIGHT; i++ ) {
		if ( s_softMap[i] ) { qvk.UnmapMemory( vk.device, s_softIboMem[i] ); s_softMap[i] = NULL; }
		if ( s_softIbo[i] != VK_NULL_HANDLE ) { qvk.DestroyBuffer( vk.device, s_softIbo[i], NULL ); s_softIbo[i] = VK_NULL_HANDLE; }
		if ( s_softIboMem[i] != VK_NULL_HANDLE ) { qvk.FreeMemory( vk.device, s_softIboMem[i], NULL ); s_softIboMem[i] = VK_NULL_HANDLE; }
	}

	s_ready = qfalse;
}
