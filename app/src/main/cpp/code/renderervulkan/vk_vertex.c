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
#include "vk_local.h"

#include <stdlib.h>
#include <string.h>

const vkDemoVertex_t vk_demoVertices[VK_VERTEX_DEMO_COUNT] = {
	{ { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
	{ {  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
	{ {  0.0f,  0.5f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
};

qboolean VK_Vertex_Init( void )
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

	memset( &vk.vertexBuffer, 0, sizeof( vk.vertexBuffer ) );

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

	if ( !VK_Guard_Result( VK_GSITE_ALLOC, qvk.CreateBuffer( vk.device, &stagingInfo, NULL, &staging ),
						   "vkCreateBuffer(staging)" ) ) {
		return qfalse;
	}

	{
		VkMemoryRequirements requirements;

		qvk.GetBufferMemoryRequirements( vk.device, staging, &requirements );

		if ( !VK_Memory_AllocFor( requirements,
								  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
								  &stagingMemory ) ) {
			return qfalse;
		}

		result = qvk.MapMemory( vk.device, stagingMemory, 0, VK_VERTEX_DEMO_BYTES, 0, &mapped );
		if ( !VK_Guard_Result( VK_GSITE_ALLOC, result, "vkMapMemory(staging)" ) ) {
			return qfalse;
		}

		memcpy( mapped, vk_demoVertices, VK_VERTEX_DEMO_BYTES );
		qvk.UnmapMemory( vk.device, stagingMemory );

		if ( !VK_Guard_Result( VK_GSITE_ALLOC,
							   qvk.BindBufferMemory( vk.device, staging, stagingMemory, 0 ),
							   "vkBindBufferMemory(staging)" ) ) {
			return qfalse;
		}
	}

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
		qvk.DestroyFence( vk.device, fence, NULL );
		return qfalse;
	}

	result = qvk.WaitForFences( vk.device, 1, &fence, VK_TRUE, UINT64_MAX );
	VK_Guard_Result( VK_GSITE_FENCE, result, "vkWaitForFences(upload)" );

	qvk.DestroyFence( vk.device, fence, NULL );
	qvk.FreeCommandBuffers( vk.device, vk.commandPool, 1, &vk.uploadCmd );
	qvk.DestroyBuffer( vk.device, staging, NULL );
	qvk.FreeMemory( vk.device, stagingMemory, NULL );

	return ( result == VK_SUCCESS ) ? qtrue : qfalse;
}

void VK_Vertex_Shutdown( void )
{
	if ( vk.device == VK_NULL_HANDLE ) {
		return;
	}

	if ( vk.vertexBuffer != VK_NULL_HANDLE && qvk.DestroyBuffer ) {
		qvk.DestroyBuffer( vk.device, vk.vertexBuffer, NULL );
	}
	vk.vertexBuffer = VK_NULL_HANDLE;

	if ( vk.vertexMemory != VK_NULL_HANDLE && qvk.FreeMemory ) {
		qvk.FreeMemory( vk.device, vk.vertexMemory, NULL );
	}
	vk.vertexMemory = VK_NULL_HANDLE;
}
