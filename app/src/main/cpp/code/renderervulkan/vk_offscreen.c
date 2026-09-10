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

typedef struct {
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	VkFramebuffer framebuffer;
	VkBuffer buffer;
	VkDeviceMemory bufferMemory;
	void *mapped;
} vkOffscreen_t;

static void VK_Offscreen_Destroy( vkOffscreen_t *target )
{
	if ( vk.device == VK_NULL_HANDLE ) {
		return;
	}

	if ( target->mapped ) {
		qvk.UnmapMemory( vk.device, target->bufferMemory );
		target->mapped = NULL;
	}

	if ( target->buffer != VK_NULL_HANDLE && qvk.DestroyBuffer ) {
		qvk.DestroyBuffer( vk.device, target->buffer, NULL );
	}
	if ( target->bufferMemory != VK_NULL_HANDLE && qvk.FreeMemory ) {
		qvk.FreeMemory( vk.device, target->bufferMemory, NULL );
	}
	if ( target->framebuffer != VK_NULL_HANDLE && qvk.DestroyFramebuffer ) {
		qvk.DestroyFramebuffer( vk.device, target->framebuffer, NULL );
	}
	if ( target->view != VK_NULL_HANDLE && qvk.DestroyImageView ) {
		qvk.DestroyImageView( vk.device, target->view, NULL );
	}
	if ( target->image != VK_NULL_HANDLE && qvk.DestroyImage ) {
		qvk.DestroyImage( vk.device, target->image, NULL );
	}
	if ( target->memory != VK_NULL_HANDLE && qvk.FreeMemory ) {
		qvk.FreeMemory( vk.device, target->memory, NULL );
	}

	memset( target, 0, sizeof( *target ) );
}

static qboolean VK_Offscreen_Barrier( VkCommandBuffer cmd, VkImage image, VkImageLayout from,
									  VkImageLayout to, VkAccessFlags fromAccess, VkAccessFlags toAccess )
{
	VkImageMemoryBarrier barrier;
	VkImageSubresourceRange range;

	memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = from;
	barrier.newLayout = to;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	range = barrier.subresourceRange;
	range.baseMipLevel = 0;
	range.levelCount = 1;
	range.baseArrayLayer = 0;
	range.layerCount = 1;
	barrier.subresourceRange = range;

	barrier.srcAccessMask = fromAccess;
	barrier.dstAccessMask = toAccess;

	qvk.CmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
							0, 0, NULL, 0, NULL, 1, &barrier );

	return qtrue;
}

qboolean VK_Offscreen_ClearAndRead( int width, int height, const float rgba[4],
									unsigned char *out, size_t outBytes )
{
	vkOffscreen_t target;
	VkImageCreateInfo imageInfo;
	VkImageViewCreateInfo viewInfo;
	VkFramebufferCreateInfo framebufferInfo;
	VkBufferCreateInfo bufferInfo;
	VkCommandBufferAllocateInfo allocInfo;
	VkCommandBufferBeginInfo beginInfo;
	VkRenderPassBeginInfo passInfo;
	VkClearValue clearValue;
	VkBufferImageCopy copyRegion;
	VkSubmitInfo submitInfo;
	VkFenceCreateInfo fenceInfo;
	VkFence fence = VK_NULL_HANDLE;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkDeviceSize imageSize = (VkDeviceSize)width * (VkDeviceSize)height * 4;
	VkResult result;
	qboolean ok = qfalse;

	memset( &target, 0, sizeof( target ) );

	if ( vk.offscreenPass == VK_NULL_HANDLE || vk.commandPool == VK_NULL_HANDLE ) {
		VK_Guard_Hit( VK_GSITE_SYNC, "offscreen probe without a pass or pool", VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	memset( &imageInfo, 0, sizeof( imageInfo ) );
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageInfo.extent.width = (uint32_t)width;
	imageInfo.extent.height = (uint32_t)height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	if ( !VK_Guard_Result( VK_GSITE_ALLOC, qvk.CreateImage( vk.device, &imageInfo, NULL, &target.image ),
						   "offscreen vkCreateImage" ) ) {
		goto done;
	}

	if ( !VK_Memory_AllocImage( target.image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &target.memory ) ) {
		goto done;
	}

	memset( &viewInfo, 0, sizeof( viewInfo ) );
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = target.image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.layerCount = 1;

	if ( !VK_Guard_Result( VK_GSITE_ALLOC, qvk.CreateImageView( vk.device, &viewInfo, NULL, &target.view ),
						   "offscreen vkCreateImageView" ) ) {
		goto done;
	}

	memset( &framebufferInfo, 0, sizeof( framebufferInfo ) );
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = vk.offscreenPass;
	framebufferInfo.attachmentCount = 1;
	framebufferInfo.pAttachments = &target.view;
	framebufferInfo.width = (uint32_t)width;
	framebufferInfo.height = (uint32_t)height;
	framebufferInfo.layers = 1;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.CreateFramebuffer( vk.device, &framebufferInfo, NULL,
																 &target.framebuffer ),
						   "offscreen vkCreateFramebuffer" ) ) {
		goto done;
	}

	memset( &bufferInfo, 0, sizeof( bufferInfo ) );
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = imageSize;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if ( !VK_Memory_CreateBuffer( imageSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
								 &target.buffer, &target.bufferMemory ) ) {
		goto done;
	}

	result = qvk.MapMemory( vk.device, target.bufferMemory, 0, imageSize, 0, &target.mapped );
	if ( !VK_Guard_Result( VK_GSITE_ALLOC, result, "offscreen vkMapMemory" ) ) {
		goto done;
	}

	memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = vk.commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.AllocateCommandBuffers( vk.device, &allocInfo, &cmd ),
						   "offscreen vkAllocateCommandBuffers" ) ) {
		goto done;
	}

	memset( &beginInfo, 0, sizeof( beginInfo ) );
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.BeginCommandBuffer( cmd, &beginInfo ),
						   "offscreen vkBeginCommandBuffer" ) ) {
		goto done;
	}

	VK_Offscreen_Barrier( cmd, target.image, VK_IMAGE_LAYOUT_UNDEFINED,
						  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
						  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT );

	memset( &clearValue, 0, sizeof( clearValue ) );
	clearValue.color.float32[0] = rgba[0];
	clearValue.color.float32[1] = rgba[1];
	clearValue.color.float32[2] = rgba[2];
	clearValue.color.float32[3] = rgba[3];

	memset( &passInfo, 0, sizeof( passInfo ) );
	passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	passInfo.renderPass = vk.offscreenPass;
	passInfo.framebuffer = target.framebuffer;
	passInfo.renderArea.extent.width = (uint32_t)width;
	passInfo.renderArea.extent.height = (uint32_t)height;
	passInfo.clearValueCount = 1;
	passInfo.pClearValues = &clearValue;

	qvk.CmdBeginRenderPass( cmd, &passInfo, VK_SUBPASS_CONTENTS_INLINE );
	qvk.CmdEndRenderPass( cmd );

	VK_Offscreen_Barrier( cmd, target.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT );

	memset( &copyRegion, 0, sizeof( copyRegion ) );
	copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.imageSubresource.layerCount = 1;
	copyRegion.imageExtent.width = (uint32_t)width;
	copyRegion.imageExtent.height = (uint32_t)height;
	copyRegion.imageExtent.depth = 1;

	qvk.CmdCopyImageToBuffer( cmd, target.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
							  target.buffer, 1, &copyRegion );

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.EndCommandBuffer( cmd ), "offscreen vkEndCommandBuffer" ) ) {
		goto done;
	}

	memset( &fenceInfo, 0, sizeof( fenceInfo ) );
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	if ( !VK_Guard_Result( VK_GSITE_FENCE, qvk.CreateFence( vk.device, &fenceInfo, NULL, &fence ),
						   "offscreen vkCreateFence" ) ) {
		goto done;
	}

	memset( &submitInfo, 0, sizeof( submitInfo ) );
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;

	if ( !VK_Guard_Result( VK_GSITE_SUBMIT, qvk.QueueSubmit( vk.graphicsQueue, 1, &submitInfo, fence ),
						   "offscreen vkQueueSubmit" ) ) {
		goto done;
	}

	result = qvk.WaitForFences( vk.device, 1, &fence, VK_TRUE, UINT64_MAX );
	if ( !VK_Guard_Result( VK_GSITE_FENCE, result, "offscreen vkWaitForFences" ) ) {
		goto done;
	}

	if ( target.mapped && outBytes >= (size_t)imageSize ) {
		memcpy( out, target.mapped, (size_t)imageSize );
		ok = qtrue;
	}

done:
	if ( fence != VK_NULL_HANDLE && qvk.DestroyFence ) {
		qvk.DestroyFence( vk.device, fence, NULL );
	}
	if ( cmd != VK_NULL_HANDLE && qvk.FreeCommandBuffers ) {
		qvk.FreeCommandBuffers( vk.device, vk.commandPool, 1, &cmd );
	}

	VK_Offscreen_Destroy( &target );

	return ok;
}
