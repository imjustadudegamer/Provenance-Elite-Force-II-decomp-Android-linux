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
 * The GLES3 twin streams geometry through an immediate GL store; this module
 * streams it through two persistently mapped host-visible buffers that the
 * render thread's command buffer reads straight out of.  The front end fills
 * them while it builds a frame, the back end binds them once per pass and
 * issues vkCmdDrawIndexed off the offsets it reserved, and the whole frame's
 * geometry is one allocation -- no per-draw buffer, no orphaning, and no
 * barrier between draws because the CPU writes finish before the submit.
 */

#include "tr_local.h"

#include <string.h>

typedef struct {
	VkBuffer		buffer;
	VkDeviceMemory	memory;
	vkTessVertex_t	*mapped;

	VkBuffer		indexBuffer;
	VkDeviceMemory	indexMemory;
	glIndex_t		*indexMapped;

	int				vertexBase;
	int				indexBase;
	int				flushed;
} vkRing_t;

static vkRing_t	vk_ring;

/* M1's first-light triangle: three vertices, two of them on the NDC diagonal,
   in the demo vertex layout the demo pipeline was built against. */
const vkDemoVertex_t vk_demoVertices[VK_VERTEX_DEMO_COUNT] = {
	{ { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
	{ {  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
	{ {  0.0f,  0.5f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
};

#define VK_RING_VERTEX_BYTES ( VK_RING_MAX_VERTEXES * VK_TESS_VERTEX_BYTES )
#define VK_RING_INDEX_BYTES  ( VK_RING_MAX_INDEXES * (int)sizeof( glIndex_t ) )

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
	memset( &vk_ring, 0, sizeof( vk_ring ) );

	if ( !VK_DemoBuffer_Init() ) {
		return qfalse;
	}

	if ( !VK_Memory_CreateBuffer( VK_RING_VERTEX_BYTES,
								  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
								  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
								  &vk_ring.buffer, &vk_ring.memory ) ) {
		return qfalse;
	}

	if ( !VK_Guard_Result( VK_GSITE_ALLOC, qvk.MapMemory( vk.device, vk_ring.memory, 0,
														  VK_WHOLE_SIZE, 0, (void **)&vk_ring.mapped ),
						   "vkMapMemory(vertex ring)" ) ) {
		return qfalse;
	}

	if ( !VK_Memory_CreateBuffer( VK_RING_INDEX_BYTES,
								  VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
								  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
								  &vk_ring.indexBuffer, &vk_ring.indexMemory ) ) {
		return qfalse;
	}

	if ( !VK_Guard_Result( VK_GSITE_ALLOC, qvk.MapMemory( vk.device, vk_ring.indexMemory, 0,
														  VK_WHOLE_SIZE, 0, (void **)&vk_ring.indexMapped ),
						   "vkMapMemory(index ring)" ) ) {
		return qfalse;
	}

	vk_ring.vertexBase = 0;
	vk_ring.indexBase = 0;
	vk_ring.flushed = 0;

	VK_Guard_Row( "VK:ring", "ready vertexes=%d bytes=%d indexes=%d",
				  VK_RING_MAX_VERTEXES, VK_RING_VERTEX_BYTES, VK_RING_MAX_INDEXES );

	return qtrue;
}

void VK_VertexRing_Shutdown( void )
{
	if ( vk.device == VK_NULL_HANDLE ) {
		return;
	}

	if ( vk_ring.mapped ) {
		qvk.UnmapMemory( vk.device, vk_ring.memory );
		vk_ring.mapped = NULL;
	}

	if ( vk_ring.indexMapped ) {
		qvk.UnmapMemory( vk.device, vk_ring.indexMemory );
		vk_ring.indexMapped = NULL;
	}

	if ( vk_ring.buffer != VK_NULL_HANDLE && qvk.DestroyBuffer ) {
		qvk.DestroyBuffer( vk.device, vk_ring.buffer, NULL );
	}
	vk_ring.buffer = VK_NULL_HANDLE;

	if ( vk_ring.memory != VK_NULL_HANDLE && qvk.FreeMemory ) {
		qvk.FreeMemory( vk.device, vk_ring.memory, NULL );
	}
	vk_ring.memory = VK_NULL_HANDLE;

	if ( vk_ring.indexBuffer != VK_NULL_HANDLE && qvk.DestroyBuffer ) {
		qvk.DestroyBuffer( vk.device, vk_ring.indexBuffer, NULL );
	}
	vk_ring.indexBuffer = VK_NULL_HANDLE;

	if ( vk_ring.indexMemory != VK_NULL_HANDLE && qvk.FreeMemory ) {
		qvk.FreeMemory( vk.device, vk_ring.indexMemory, NULL );
	}
	vk_ring.indexMemory = VK_NULL_HANDLE;

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

	vk_ring.vertexBase = 0;
	vk_ring.indexBase = 0;
	vk_ring.flushed = 0;
}

void VK_VertexRing_ResetFrame( void )
{
	vk_ring.vertexBase = 0;
	vk_ring.indexBase = 0;
	vk_ring.flushed = 0;
}

vkTessVertex_t *VK_VertexRing_Reserve( int vertexes, int *baseOut )
{
	if ( vertexes <= 0 ) {
		if ( baseOut ) {
			*baseOut = vk_ring.vertexBase;
		}
		return NULL;
	}

	if ( vk_ring.vertexBase + vertexes > VK_RING_MAX_VERTEXES ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "vertex ring exhausted", VK_ERROR_OUT_OF_DEVICE_MEMORY );
		if ( baseOut ) {
			*baseOut = vk_ring.vertexBase;
		}
		return NULL;
	}

	if ( baseOut ) {
		*baseOut = vk_ring.vertexBase;
	}

	vk_ring.vertexBase += vertexes;

	return vk_ring.mapped + ( vk_ring.vertexBase - vertexes );
}

glIndex_t *VK_VertexRing_ReserveIndexes( int indexes, int *baseOut )
{
	if ( indexes <= 0 ) {
		if ( baseOut ) {
			*baseOut = vk_ring.indexBase;
		}
		return NULL;
	}

	if ( vk_ring.indexBase + indexes > VK_RING_MAX_INDEXES ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "index ring exhausted", VK_ERROR_OUT_OF_DEVICE_MEMORY );
		if ( baseOut ) {
			*baseOut = vk_ring.indexBase;
		}
		return NULL;
	}

	if ( baseOut ) {
		*baseOut = vk_ring.indexBase;
	}

	vk_ring.indexBase += indexes;

	return vk_ring.indexMapped + ( vk_ring.indexBase - indexes );
}

void VK_VertexRing_RollBack( int vertexes, int indexes )
{
	vk_ring.vertexBase -= vertexes;
	vk_ring.indexBase -= indexes;

	if ( vk_ring.vertexBase < 0 ) {
		vk_ring.vertexBase = 0;
	}
	if ( vk_ring.indexBase < 0 ) {
		vk_ring.indexBase = 0;
	}
}

vkTessVertex_t *VK_VertexRing_Tail( int back )
{
	int index = vk_ring.vertexBase - back;

	if ( back <= 0 || index < 0 ) {
		return NULL;
	}

	return vk_ring.mapped + index;
}

void vk_ring_note( int vertexHigh, int indexHigh )
{
	if ( vk_ring.flushed < vertexHigh ) {
		vk_ring.flushed = vertexHigh;
	}

	( void )indexHigh;
}

VkBuffer VK_VertexRing_Buffer( void )
{
	return vk_ring.buffer;
}

VkBuffer VK_VertexRing_IndexBuffer( void )
{
	return vk_ring.indexBuffer;
}

int VK_VertexRing_Used( void )
{
	return vk_ring.vertexBase;
}

int VK_VertexRing_Flushed( void )
{
	return vk_ring.flushed;
}
