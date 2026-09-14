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
/*
 * vk_vertex.c -- the frame's vertex and index rings.
 *
 * The reference renderer streams geometry through an immediate-mode store; this module
 * streams it through persistently mapped host-visible buffers that the render
 * thread's command buffer reads straight out of: the front end fills them while
 * it builds a frame, the back end binds them once per pass and issues
 * vkCmdDrawIndexed off the offsets it reserved.
 */

#include "tr_local.h"

#include <string.h>

typedef struct {
	VkBuffer		buffer;
	VkDeviceMemory	memory;
	vkTessVertex_t	*mapped;

	VkBuffer		indexBuffer;
	VkDeviceMemory	indexMemory;
	vkIndex_t		*indexMapped;

	int				vertexBase;
	int				indexBase;
	int				flushed;
} vkRing_t;

/* the one surface slice the current iterator may reuse across its stages */
typedef struct {
	qboolean		valid;
	int				token;
	int				vertexes;
	int				indexes;
	const void		*tc0;
	const void		*tc1;
	int				tcStride;
	const void		*colors;
	const void		*normals;
	int				base;
	int				indexBase;
} vkStreamSlice_t;

static vkStreamSlice_t	vk_streamSlice;

/* the frame's demand, as the guard and the census row name it */
static int	vk_frameDropCount;
static int	vk_frameSurfaceCount;
static int	vk_frameRefusedSlots;
static int	vk_sliceToken;

static void	VK_VertexRing_FrameDemandFull( int *usedOut, int *vertexesOut, int *indexesOut,
										   int *dropsOut, int *surfacesOut, int *refusedSlotsOut );

/* One ring per frame slot: a submit does not wait, so slot 1 is filling while
   slot 0 executes, and one ring would rewrite geometry the GPU is fetching. */
static vkRing_t	vk_ring[VK_FRAMES_IN_FLIGHT];
static int		vk_ringSlot = -1;

/* the slot the frame being recorded or walked reads and fills */
#define VK_RING_SLOT() ( ( vk_ringSlot >= 0 && vk_ringSlot < VK_FRAMES_IN_FLIGHT ) ? vk_ringSlot : 0 )
#define VK_Ring()      ( &vk_ring[ VK_RING_SLOT() ] )

void VK_VertexRing_SetFrame( int slot )
{
	vk_ringSlot = slot;
}

/* M1's first-light triangle, in the demo pipeline's vertex layout */
const vkDemoVertex_t vk_demoVertices[VK_VERTEX_DEMO_COUNT] = {
	{ { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
	{ {  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
	{ {  0.0f,  0.5f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
};

#define VK_RING_VERTEX_BYTES ( VK_RING_MAX_VERTEXES * VK_TESS_VERTEX_BYTES )
#define VK_RING_INDEX_BYTES  ( VK_RING_MAX_INDEXES * (int)sizeof( vkIndex_t ) )

qboolean VK_Vertex_Init( void )
{
	return VK_VertexRing_Init();
}

void VK_Vertex_Shutdown( void )
{
	VK_VertexRing_Shutdown();
}

/* The demo triangle's device-local buffer: M1's first-light proof draws it, and
   it is the one geometry that does not come from the frame's tess store. */
static qboolean VK_DemoBuffer_Init( void )
{
	VkBufferCreateInfo bufferInfo;
	VkBufferCreateInfo stagingInfo;
	VkBuffer staging = VK_NULL_HANDLE;
	VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
	void *mapped = VK_NULL_HANDLE;
	VkCommandBufferAllocateInfo allocInfo;
	VkCommandBufferBeginInfo beginInfo;
	VkBufferCopy copyRegion;
	VkSubmitInfo submitInfo;
	VkFenceCreateInfo fenceInfo;
	VkFence fence = VK_NULL_HANDLE;
	VkResult result;

	memset( &bufferInfo, 0, sizeof( bufferInfo ) );
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = VK_VERTEX_DEMO_BYTES;
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if ( !VK_Guard_Result( VK_GSITE_ALLOC, qvk.CreateBuffer( vk.device, &bufferInfo, NULL, &vk.vertexBuffer ),
						   "vkCreateBuffer(vertex)" ) ) {
		return qfalse;
	}

	if ( !VK_Memory_AllocBuffer( vk.vertexBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vk.vertexMemory ) ) {
		return qfalse;
	}

	memset( &stagingInfo, 0, sizeof( stagingInfo ) );
	stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	stagingInfo.size = VK_VERTEX_DEMO_BYTES;
	stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if ( !VK_Memory_CreateBuffer( VK_VERTEX_DEMO_BYTES, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
								  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
								  &staging, &stagingMemory ) ) {
		return qfalse;
	}

	result = qvk.MapMemory( vk.device, stagingMemory, 0, VK_VERTEX_DEMO_BYTES, 0, &mapped );
	if ( !VK_Guard_Result( VK_GSITE_ALLOC, result, "vkMapMemory(staging)" ) ) {
		return qfalse;
	}

	memcpy( mapped, vk_demoVertices, VK_VERTEX_DEMO_BYTES );
	qvk.UnmapMemory( vk.device, stagingMemory );

	memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = vk.commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.AllocateCommandBuffers( vk.device, &allocInfo, &vk.uploadCmd ),
						   "vkAllocateCommandBuffers(upload)" ) ) {
		return qfalse;
	}

	memset( &beginInfo, 0, sizeof( beginInfo ) );
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.BeginCommandBuffer( vk.uploadCmd, &beginInfo ),
						   "vkBeginCommandBuffer(upload)" ) ) {
		return qfalse;
	}

	memset( &copyRegion, 0, sizeof( copyRegion ) );
	copyRegion.size = VK_VERTEX_DEMO_BYTES;

	qvk.CmdCopyBuffer( vk.uploadCmd, staging, vk.vertexBuffer, 1, &copyRegion );

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.EndCommandBuffer( vk.uploadCmd ),
						   "vkEndCommandBuffer(upload)" ) ) {
		return qfalse;
	}

	memset( &fenceInfo, 0, sizeof( fenceInfo ) );
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	if ( !VK_Guard_Result( VK_GSITE_FENCE, qvk.CreateFence( vk.device, &fenceInfo, NULL, &fence ),
						   "vkCreateFence(upload)" ) ) {
		return qfalse;
	}

	memset( &submitInfo, 0, sizeof( submitInfo ) );
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &vk.uploadCmd;

	if ( !VK_Guard_Result( VK_GSITE_SUBMIT, qvk.QueueSubmit( vk.graphicsQueue, 1, &submitInfo, fence ),
						   "vkQueueSubmit(upload)" ) ) {
		return qfalse;
	}

	result = qvk.WaitForFences( vk.device, 1, &fence, VK_TRUE, UINT64_MAX );
	(void)VK_Guard_Result( VK_GSITE_FENCE, result, "vkWaitForFences(upload)" );

	if ( fence != VK_NULL_HANDLE && qvk.DestroyFence ) {
		qvk.DestroyFence( vk.device, fence, NULL );
	}
	if ( staging != VK_NULL_HANDLE && qvk.DestroyBuffer ) {
		qvk.DestroyBuffer( vk.device, staging, NULL );
	}
	if ( stagingMemory != VK_NULL_HANDLE && qvk.FreeMemory ) {
		qvk.FreeMemory( vk.device, stagingMemory, NULL );
	}
	if ( vk.uploadCmd != VK_NULL_HANDLE && qvk.FreeCommandBuffers ) {
		qvk.FreeCommandBuffers( vk.device, vk.commandPool, 1, &vk.uploadCmd );
	}
	vk.uploadCmd = VK_NULL_HANDLE;

	return qtrue;
}

qboolean VK_VertexRing_Init( void )
{
	int i;

	memset( vk_ring, 0, sizeof( vk_ring ) );

	if ( !VK_DemoBuffer_Init() ) {
		return qfalse;
	}

	for ( i = 0; i < VK_FRAMES_IN_FLIGHT; i++ ) {
		vkRing_t *ring = &vk_ring[i];

		if ( !VK_Memory_CreateBuffer( VK_RING_VERTEX_BYTES,
									  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
									  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
									  &ring->buffer, &ring->memory ) ) {
			return qfalse;
		}
		if ( !VK_Guard_Result( VK_GSITE_ALLOC, qvk.MapMemory( vk.device, ring->memory, 0,
															  VK_WHOLE_SIZE, 0, (void **)&ring->mapped ),
							   "vkMapMemory(vertex ring)" ) ) {
			return qfalse;
		}
		if ( !VK_Memory_CreateBuffer( VK_RING_INDEX_BYTES,
									  VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
									  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
									  &ring->indexBuffer, &ring->indexMemory ) ) {
			return qfalse;
		}
		if ( !VK_Guard_Result( VK_GSITE_ALLOC, qvk.MapMemory( vk.device, ring->indexMemory, 0,
															  VK_WHOLE_SIZE, 0, (void **)&ring->indexMapped ),
							   "vkMapMemory(index ring)" ) ) {
			return qfalse;
		}
		ring->vertexBase = 0;
		ring->indexBase = 0;
		ring->flushed = 0;
	}
	vk_ringSlot = 0;

	VK_Guard_Row( "VK:ring", "ready vertexes=%d bytes=%d indexes=%d slots=%d",
				  VK_RING_MAX_VERTEXES, VK_RING_VERTEX_BYTES, VK_RING_MAX_INDEXES,
				  VK_FRAMES_IN_FLIGHT );

	return qtrue;
}

void VK_VertexRing_Shutdown( void )
{
	int i;

	if ( vk.device == VK_NULL_HANDLE ) {
		return;
	}

	for ( i = 0; i < VK_FRAMES_IN_FLIGHT; i++ ) {
		vkRing_t *ring = &vk_ring[i];

		if ( ring->mapped ) {
			qvk.UnmapMemory( vk.device, ring->memory );
			ring->mapped = NULL;
		}
		if ( ring->indexMapped ) {
			qvk.UnmapMemory( vk.device, ring->indexMemory );
			ring->indexMapped = NULL;
		}
		if ( ring->buffer != VK_NULL_HANDLE && qvk.DestroyBuffer ) {
			qvk.DestroyBuffer( vk.device, ring->buffer, NULL );
		}
		ring->buffer = VK_NULL_HANDLE;
		if ( ring->memory != VK_NULL_HANDLE && qvk.FreeMemory ) {
			qvk.FreeMemory( vk.device, ring->memory, NULL );
		}
		ring->memory = VK_NULL_HANDLE;
		if ( ring->indexBuffer != VK_NULL_HANDLE && qvk.DestroyBuffer ) {
			qvk.DestroyBuffer( vk.device, ring->indexBuffer, NULL );
		}
		ring->indexBuffer = VK_NULL_HANDLE;
		if ( ring->indexMemory != VK_NULL_HANDLE && qvk.FreeMemory ) {
			qvk.FreeMemory( vk.device, ring->indexMemory, NULL );
		}
		ring->indexMemory = VK_NULL_HANDLE;
		ring->vertexBase = 0;
		ring->indexBase = 0;
		ring->flushed = 0;
	}

	/* The demo triangle's buffer: M1's first-light proof, still owned here. */
	if ( vk.uploadCmd != VK_NULL_HANDLE && qvk.FreeCommandBuffers ) {
		qvk.FreeCommandBuffers( vk.device, vk.commandPool, 1, &vk.uploadCmd );
	}
	vk.uploadCmd = VK_NULL_HANDLE;

	if ( vk.vertexBuffer != VK_NULL_HANDLE && qvk.DestroyBuffer ) {
		qvk.DestroyBuffer( vk.device, vk.vertexBuffer, NULL );
	}
	vk.vertexBuffer = VK_NULL_HANDLE;

	if ( vk.vertexMemory != VK_NULL_HANDLE && qvk.FreeMemory ) {
		qvk.FreeMemory( vk.device, vk.vertexMemory, NULL );
	}
	vk.vertexMemory = VK_NULL_HANDLE;
}

/* The frame's demand, named on the boundary that ends it.  Retail's trace answers
   "what did a legal frame stream" (the menu frame: 3,047 vertexes, 10,056 index
   entries, five locked surfaces); a refusal reads as drops against capacity. */
static void VK_VertexRing_FrameRow( void )
{
	static int	rows;
	static int	lastUsed = -1;
	static int	lastDrops = -1;
	int used, bytes, indexes, drops, surfaces, refused;

	VK_VertexRing_FrameDemandFull( &used, &bytes, &indexes, &drops, &surfaces, &refused );

	if ( !used && !drops ) {
		return;
	}

	rows++;
	if ( rows <= 8 || used != lastUsed || drops != lastDrops || ( rows % 600 ) == 0 ) {
		VK_Guard_Row( "VK:ring", "frame slots=%d of %d vertex-bytes=%d indexes=%d surfaces=%d drops=%d refused-slots=%d",
					  used, VK_RING_MAX_VERTEXES, bytes, indexes, surfaces, drops, refused );
	}
	lastUsed = used;
	lastDrops = drops;
}

/* A surface's walk opens here and nowhere else: every RB_BeginSurface gets a new token, so two bat
   ches that happen to agree on vertex count, index count and source pointers -- the 2D quad case,
   where every batch is four vertices and six indexes -- can never draw each other's geometry. */
void VK_VertexRing_SurfaceBegin( void )
{
	vk_sliceToken++;
	VK_VertexRing_StreamForget();
}

void VK_VertexRing_ResetFrame( void )
{
	VK_VertexRing_FrameRow();

	VK_Ring()->vertexBase = 0;
	VK_Ring()->indexBase = 0;
	VK_Ring()->flushed = 0;
	VK_VertexRing_StreamForget();
	vk_frameDropCount = 0;
	vk_frameSurfaceCount = 0;
	vk_frameRefusedSlots = 0;
}

/* A slice is the surface's own packed geometry, so only the stage that packed it
   and the exact source tuple it packed from may draw it again. */
static qboolean VK_StreamSlice_Key( int token, int vertexes, int indexes,
									const void *tc0, const void *tc1, int tcStride,
									const void *colors, const void *normals )
{
	vkStreamSlice_t *s = &vk_streamSlice;

	if ( !s->valid || s->token != token ) {
		return qfalse;
	}
	if ( s->vertexes != vertexes || s->indexes != indexes ) {
		return qfalse;
	}
	if ( s->tc0 != tc0 || s->tc1 != tc1 || s->tcStride != tcStride ) {
		return qfalse;
	}
	if ( s->colors != colors || s->normals != normals ) {
		return qfalse;
	}
	return qtrue;
}

qboolean VK_VertexRing_StreamMatch( int vertexes, int indexes,
									const void *tc0, const void *tc1, int tcStride,
									const void *colors, const void *normals,
									int *baseOut, int *indexBaseOut )
{
	if ( !VK_StreamSlice_Key( vk_sliceToken, vertexes, indexes, tc0, tc1, tcStride,
							  colors, normals ) ) {
		return qfalse;
	}

	if ( baseOut ) {
		*baseOut = vk_streamSlice.base;
	}
	if ( indexBaseOut ) {
		*indexBaseOut = vk_streamSlice.indexBase;
	}
	return qtrue;
}

void VK_VertexRing_StreamStore( int vertexes, int indexes,
								const void *tc0, const void *tc1, int tcStride,
								const void *colors, const void *normals,
								int base, int indexBase )
{
	vk_streamSlice.valid		= qtrue;
	vk_streamSlice.token		= vk_sliceToken;
	vk_streamSlice.vertexes		= vertexes;
	vk_streamSlice.indexes		= indexes;
	vk_streamSlice.tc0			= tc0;
	vk_streamSlice.tc1			= tc1;
	vk_streamSlice.tcStride		= tcStride;
	vk_streamSlice.colors		= colors;
	vk_streamSlice.normals		= normals;
	vk_streamSlice.base			= base;
	vk_streamSlice.indexBase	= indexBase;
	vk_frameSurfaceCount++;
}

void VK_VertexRing_StreamForget( void )
{
	memset( &vk_streamSlice, 0, sizeof( vk_streamSlice ) );
	vk_streamSlice.tcStride = -1;
}

void VK_VertexRing_FrameDemand( int *usedOut, int *vertexesOut, int *indexesOut,
								int *dropsOut, int *surfacesOut )
{
	VK_VertexRing_FrameDemandFull( usedOut, vertexesOut, indexesOut, dropsOut,
								   surfacesOut, NULL );
}

static void VK_VertexRing_FrameDemandFull( int *usedOut, int *vertexesOut, int *indexesOut,
										   int *dropsOut, int *surfacesOut,
										   int *refusedSlotsOut )
{
	if ( usedOut ) {
		*usedOut = VK_Ring()->vertexBase;
	}
	if ( vertexesOut ) {
		*vertexesOut = VK_Ring()->vertexBase * VK_TESS_VERTEX_BYTES;
	}
	if ( indexesOut ) {
		*indexesOut = VK_Ring()->indexBase;
	}
	if ( dropsOut ) {
		*dropsOut = vk_frameDropCount;
	}
	if ( refusedSlotsOut ) {
		*refusedSlotsOut = vk_frameRefusedSlots;
	}
	if ( surfacesOut ) {
		*surfacesOut = vk_frameSurfaceCount;
	}
}

/* the 3D shade path packs a wider vertex: it reserves bytes and binds the ring at that offset */
int VK_VertexRing_ReserveBytes( int bytes, int *byteBaseOut )
{
	int	tessSlots;

	if ( bytes <= 0 ) {
		return qfalse;
	}

	tessSlots = ( bytes + VK_TESS_VERTEX_BYTES - 1 ) / VK_TESS_VERTEX_BYTES;

	if ( VK_Ring()->vertexBase + tessSlots > VK_RING_MAX_VERTEXES ) {
		vk_frameDropCount++;
		vk_frameRefusedSlots += tessSlots;
		VK_Guard_Hit( VK_GSITE_ALLOC, "vertex ring exhausted", VK_ERROR_OUT_OF_DEVICE_MEMORY );
		return qfalse;
	}

	if ( byteBaseOut ) {
		*byteBaseOut = VK_Ring()->vertexBase * VK_TESS_VERTEX_BYTES;
	}

	VK_Ring()->vertexBase += tessSlots;

	return qtrue;
}

vkTessVertex_t *VK_VertexRing_Reserve( int vertexes, int *baseOut )
{
	if ( vertexes <= 0 ) {
		if ( baseOut ) {
			*baseOut = VK_Ring()->vertexBase;
		}
		return NULL;
	}

	if ( VK_Ring()->vertexBase + vertexes > VK_RING_MAX_VERTEXES ) {
		vk_frameDropCount++;
		vk_frameRefusedSlots += vertexes;
		VK_Guard_Hit( VK_GSITE_ALLOC, "vertex ring exhausted", VK_ERROR_OUT_OF_DEVICE_MEMORY );
		if ( baseOut ) {
			*baseOut = VK_Ring()->vertexBase;
		}
		return NULL;
	}

	if ( baseOut ) {
		*baseOut = VK_Ring()->vertexBase;
	}

	VK_Ring()->vertexBase += vertexes;

	return VK_Ring()->mapped + ( VK_Ring()->vertexBase - vertexes );
}

vkIndex_t *VK_VertexRing_ReserveIndexes( int indexes, int *baseOut )
{
	if ( indexes <= 0 ) {
		if ( baseOut ) {
			*baseOut = VK_Ring()->indexBase;
		}
		return NULL;
	}

	if ( VK_Ring()->indexBase + indexes > VK_RING_MAX_INDEXES ) {
		vk_frameDropCount++;
		vk_frameRefusedSlots += indexes;
		VK_Guard_Hit( VK_GSITE_ALLOC, "index ring exhausted", VK_ERROR_OUT_OF_DEVICE_MEMORY );
		if ( baseOut ) {
			*baseOut = VK_Ring()->indexBase;
		}
		return NULL;
	}

	if ( baseOut ) {
		*baseOut = VK_Ring()->indexBase;
	}

	VK_Ring()->indexBase += indexes;

	return VK_Ring()->indexMapped + ( VK_Ring()->indexBase - indexes );
}

void VK_VertexRing_RollBack( int vertexes, int indexes )
{
	VK_Ring()->vertexBase -= vertexes;
	VK_Ring()->indexBase -= indexes;

	if ( VK_Ring()->vertexBase < 0 ) {
		VK_Ring()->vertexBase = 0;
	}
	if ( VK_Ring()->indexBase < 0 ) {
		VK_Ring()->indexBase = 0;
	}
}

vkTessVertex_t *VK_VertexRing_Tail( int back )
{
	int index = VK_Ring()->vertexBase - back;

	if ( back <= 0 || index < 0 ) {
		return NULL;
	}

	return VK_Ring()->mapped + index;
}

void vk_ring_note( int vertexHigh, int indexHigh )
{
	if ( VK_Ring()->flushed < vertexHigh ) {
		VK_Ring()->flushed = vertexHigh;
	}

	( void )indexHigh;
}

unsigned char *VK_VertexRing_Mapped( void )
{
	return ( unsigned char * )VK_Ring()->mapped;
}

VkBuffer VK_VertexRing_Buffer( void )
{
	return VK_Ring()->buffer;
}

VkBuffer VK_VertexRing_IndexBuffer( void )
{
	return VK_Ring()->indexBuffer;
}

int VK_VertexRing_Used( void )
{
	return VK_Ring()->vertexBase;
}

int VK_VertexRing_Flushed( void )
{
	return VK_Ring()->flushed;
}
