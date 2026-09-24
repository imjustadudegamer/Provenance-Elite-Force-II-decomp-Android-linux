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
 * vk_texture.c -- the texture upload path: a staging copy per mip, a
 * vkCmdCopyBufferToImage into a device-local image, then a layout barrier for
 * sampling.  Every format the loaders report is mapped onto a Vulkan format
 * the device has confirmed (SDK doc 6/7: probed, never assumed).
 */

#include "tr_local.h"

#include <string.h>

static vkTexture_t	vk_textures[VK_MAX_TEXTURES];
static int			vk_numTextures;
static int			vk_textureBytes;

/* the shared samplers: wrap modes crossed with the image filter pairs */
#define VK_SAMPLER_SLOTS 16

typedef struct {
	VkSampler	sampler;
	qboolean	clamp;
	qboolean	mipmap;
	qboolean	linear;
} vkSamplerSlot_t;

static vkSamplerSlot_t	vk_samplers[VK_SAMPLER_SLOTS];
static int				vk_numSamplers;

static VkFormatFeatureFlags VK_Texture_WantedFeatures( void )
{
	return VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT
		   | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
}

VkFormat VK_Texture_FormatFor( int internalFormat, qboolean *supported )
{
	VkFormat format;
	VkFormatProperties props;

	if ( supported ) {
		*supported = qfalse;
	}

	switch ( internalFormat ) {
	case VK_TF_DXT1:
		format = VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
		break;
	case VK_TF_DXT3:
		format = VK_FORMAT_BC2_UNORM_BLOCK;
		break;
	case VK_TF_DXT5:
		format = VK_FORMAT_BC3_UNORM_BLOCK;
		break;
		/* The module's names are the loaders' legacy values; an unknown one is
		   named, not silently rounded to rgba8. */
	case VK_TF_RGBA8:
	case VK_TF_RGBA:
	case VK_TF_RGB5_A1:
	case VK_TF_RGBA4:
	case VK_TF_RGB8:
	case VK_TF_RGB:
	case VK_TF_RGB5:
	case VK_TF_LUM8:
	case 0x805A:	/* legacy 0x805A, RGBA4 */
	case 0x1903:	/* legacy 0x1903, RED */
	case 0x1908:	/* legacy 0x1908, RGBA */
		format = VK_FORMAT_R8G8B8A8_UNORM;
		break;
	default: {
		static char unknownFormat[48];

		Com_sprintf( unknownFormat, sizeof( unknownFormat ),
					 "unknown internal format 0x%x, mapping to rgba8", (unsigned)internalFormat );
		VK_Guard_Hit( VK_GSITE_QUIRK, unknownFormat, VK_ERROR_FORMAT_NOT_SUPPORTED );
		format = VK_FORMAT_R8G8B8A8_UNORM;
		break;
	}
	}

	/* A NULL here is a call through address zero, so it is named first. */
	if ( qvk.GetPhysicalDeviceFormatProperties == NULL ) {
		VK_Guard_Hit( VK_GSITE_QUIRK, "vkGetPhysicalDeviceFormatProperties unresolved",
					  VK_ERROR_INITIALIZATION_FAILED );
		if ( supported ) {
			*supported = qfalse;
		}
		return VK_FORMAT_R8G8B8A8_UNORM;
	}

	memset( &props, 0, sizeof( props ) );
	qvk.GetPhysicalDeviceFormatProperties( vk.physicalDevice, format, &props );

	if ( ( props.optimalTilingFeatures & VK_Texture_WantedFeatures() ) == VK_Texture_WantedFeatures() ) {
		if ( supported ) {
			*supported = qtrue;
		}
	} else if ( format != VK_FORMAT_R8G8B8A8_UNORM ) {
			VK_Guard_Row( "VK:texture", "format=%d unsupported optimal=0x%x fallback=rgba8",
					  internalFormat, (unsigned int)props.optimalTilingFeatures );
		return VK_FORMAT_R8G8B8A8_UNORM;
	} else {
		VK_Guard_Hit( VK_GSITE_ALLOC, "rgba8 unsupported on this device", VK_ERROR_FORMAT_NOT_SUPPORTED );
	}

	return format;
}

qboolean VK_Texture_Init( void )
{
	memset( vk_textures, 0, sizeof( vk_textures ) );
	vk_numTextures = 0;
	vk_textureBytes = 0;

	memset( vk_samplers, 0, sizeof( vk_samplers ) );
	vk_numSamplers = 0;

	/* the upload section lock exists before either thread can reach a pool */
	VK_Upload_Init();

	return qtrue;
}

void VK_Texture_Shutdown( void )
{
	int i;

	if ( vk.device == VK_NULL_HANDLE ) {
		return;
	}

	if ( vk.uploadPool != VK_NULL_HANDLE && qvk.DeviceWaitIdle ) {
		VK_Guard_Result( VK_GSITE_SYNC, VK_Sync_DeviceWaitIdle( "texture-shutdown", "texture-shutdown" ),
						 "vkDeviceWaitIdle(textures)" );
	}

	for ( i = 0; i < VK_MAX_TEXTURES; i++ ) {
		VK_Texture_Destroy( i );
	}

	for ( i = 0; i < VK_SAMPLER_SLOTS; i++ ) {
		if ( vk_samplers[i].sampler != VK_NULL_HANDLE && qvk.DestroySampler ) {
			qvk.DestroySampler( vk.device, vk_samplers[i].sampler, NULL );
		}
		vk_samplers[i].sampler = VK_NULL_HANDLE;
	}
	vk_numSamplers = 0;

	vk_numTextures = 0;
	vk_textureBytes = 0;
}


static void VK_Texture_Barrier( VkCommandBuffer cmd, VkImage image, VkImageLayout from,
								VkImageLayout to, VkAccessFlags fromAccess, VkAccessFlags toAccess,
								int mipLevels )
{
	VkImageMemoryBarrier barrier;

	memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = from;
	barrier.newLayout = to;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = (uint32_t)mipLevels;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = fromAccess;
	barrier.dstAccessMask = toAccess;

	qvk.CmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
							0, 0, NULL, 0, NULL, 1, &barrier );
}

static void VK_Texture_BarrierLevel( VkCommandBuffer cmd, VkImage image, uint32_t level,
									 uint32_t dstLevel, VkAccessFlags fromAccess,
									 VkAccessFlags toAccess )
{
	VkImageMemoryBarrier barrier;

	memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = level;
	barrier.subresourceRange.levelCount = ( dstLevel > level ) ? ( dstLevel - level + 1 ) : 1;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = fromAccess;
	barrier.dstAccessMask = toAccess;

	qvk.CmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
							0, 0, NULL, 0, NULL, 1, &barrier );
}

qboolean VK_Texture_Create( int width, int height, int mipLevels, VkFormat format, int *handleOut )
{
	VkImageCreateInfo imageInfo;
	VkImageViewCreateInfo viewInfo;
	vkTexture_t *tex;
	int i;

	if ( !handleOut ) {
		return qfalse;
	}

	*handleOut = -1;

	for ( i = 0; i < VK_MAX_TEXTURES; i++ ) {
		if ( !vk_textures[i].inUse ) {
			break;
		}
	}

	if ( i >= VK_MAX_TEXTURES ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "texture table full", VK_ERROR_OUT_OF_HOST_MEMORY );
		return qfalse;
	}

	tex = &vk_textures[i];
	memset( tex, 0, sizeof( *tex ) );
	tex->width = width;
	tex->height = height;
	tex->mipLevels = mipLevels;
	tex->format = format;
	tex->layout = VK_IMAGE_LAYOUT_UNDEFINED;

	memset( &imageInfo, 0, sizeof( imageInfo ) );
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = format;
	imageInfo.extent.width = (uint32_t)width;
	imageInfo.extent.height = (uint32_t)height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = (uint32_t)mipLevels;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
					  | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	if ( !VK_Guard_Result( VK_GSITE_ALLOC, qvk.CreateImage( vk.device, &imageInfo, NULL, &tex->image ),
						   "vkCreateImage(texture)" ) ) {
		return qfalse;
	}

	if ( !VK_Memory_AllocImage( tex->image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &tex->memory ) ) {
		qvk.DestroyImage( vk.device, tex->image, NULL );
		tex->image = VK_NULL_HANDLE;
		return qfalse;
	}

	memset( &viewInfo, 0, sizeof( viewInfo ) );
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = tex->image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.levelCount = (uint32_t)mipLevels;
	viewInfo.subresourceRange.layerCount = 1;

	if ( !VK_Guard_Result( VK_GSITE_ALLOC, qvk.CreateImageView( vk.device, &viewInfo, NULL, &tex->view ),
						   "vkCreateImageView(texture)" ) ) {
		qvk.DestroyImage( vk.device, tex->image, NULL );
		tex->image = VK_NULL_HANDLE;
		return qfalse;
	}

	tex->inUse = qtrue;
	vk_numTextures++;

	*handleOut = i;

	return qtrue;
}

void VK_Texture_Destroy( int handle )
{
	vkTexture_t *tex;

	if ( handle < 0 || handle >= VK_MAX_TEXTURES ) {
		return;
	}

	tex = &vk_textures[handle];

	if ( tex->view != VK_NULL_HANDLE && qvk.DestroyImageView ) {
		qvk.DestroyImageView( vk.device, tex->view, NULL );
	}
	if ( tex->image != VK_NULL_HANDLE && qvk.DestroyImage ) {
		qvk.DestroyImage( vk.device, tex->image, NULL );
	}
	if ( tex->memory != VK_NULL_HANDLE && qvk.FreeMemory ) {
		qvk.FreeMemory( vk.device, tex->memory, NULL );
	}

	if ( tex->inUse ) {
		vk_numTextures--;
	}

	memset( tex, 0, sizeof( *tex ) );
}

/* The access stage a layout is left in, for the barrier that leaves it.  A
   barrier's srcAccessMask has to describe the accesses the old layout allowed
   (Vulkan 1.4 s7.7), so it comes from the tracked layout, not from a guess. */
static VkAccessFlags VK_Texture_LayoutAccess( VkImageLayout layout )
{
	switch ( layout ) {
	case VK_IMAGE_LAYOUT_UNDEFINED:
		return 0;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		return VK_ACCESS_TRANSFER_READ_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		return VK_ACCESS_TRANSFER_WRITE_BIT;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		return VK_ACCESS_SHADER_READ_BIT;
	default:
		return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	}
}

qboolean VK_Texture_Upload( int handle, int mipLevel, int width, int height,
							const void *data, size_t bytes )
{
	vkTexture_t *tex;
	VkBufferCreateInfo stagingInfo;
	VkBuffer staging = VK_NULL_HANDLE;
	VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
	void *mapped = NULL;
	VkCommandBufferAllocateInfo allocInfo;
	VkCommandBufferBeginInfo beginInfo;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkBufferImageCopy region;
	VkFenceCreateInfo fenceInfo;
	VkFence fence = VK_NULL_HANDLE;
	VkSubmitInfo submitInfo;
	size_t rowBytes;
	VkResult result;
	qboolean ok = qfalse;

	if ( handle < 0 || handle >= VK_MAX_TEXTURES ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "upload handle out of range", VK_ERROR_TOO_MANY_OBJECTS );
		return qfalse;
	}

	tex = &vk_textures[handle];
	if ( !tex->inUse || !data || bytes == 0 ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "upload of an unusable texture", VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	/* A zero extent cannot be contained in the image (VUID-07970); the caller
	   that computes one has walked its chain past the last level. */
	if ( width < 1 || height < 1 || mipLevel < 0 || mipLevel >= tex->mipLevels ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "upload level outside the image",
					  VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	/* Quiesce BEFORE the upload section is taken.  The walk thread can reach a
	   lazy texture load inside the frame this parks, and that load needs the
	   section: waiting for the walk while holding it would wait for a thread
	   that is waiting for us. */
	if ( tex->layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
		VK_Upload_Quiesce();
	}

	VK_Upload_Begin();

	memset( &stagingInfo, 0, sizeof( stagingInfo ) );
	stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	stagingInfo.size = (VkDeviceSize)bytes;
	stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if ( !VK_Memory_CreateBuffer( stagingInfo.size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
								  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
								  &staging, &stagingMemory ) ) {
		VK_Upload_End();
		return qfalse;
	}

	result = qvk.MapMemory( vk.device, stagingMemory, 0, stagingInfo.size, 0, &mapped );
	if ( !VK_Guard_Result( VK_GSITE_ALLOC, result, "vkMapMemory(staging)" ) ) {
		goto done;
	}

	memcpy( mapped, data, bytes );
	qvk.UnmapMemory( vk.device, stagingMemory );
	mapped = NULL;

	memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = vk.uploadPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, VK_Pool_AllocBuffers( vk.device, &allocInfo, &cmd ),
						   "vkAllocateCommandBuffers(upload)" ) ) {
		goto done;
	}

	memset( &beginInfo, 0, sizeof( beginInfo ) );
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.BeginCommandBuffer( cmd, &beginInfo ),
						   "vkBeginCommandBuffer(upload)" ) ) {
		goto done;
	}

	/* The copy wants TRANSFER_DST, and the image may be in any layout the boot
	   already left it in: a refill hits SHADER_READ_ONLY, a later level of the
	   same chain hits TRANSFER_DST.  One barrier from the TRACKED layout covers
	   both, and its declared oldLayout is the layout the image actually holds
	   -- declaring SHADER_READ_ONLY over an image that is in TRANSFER_DST is
	   exactly the InvalidImageLayout the layer reported. */
	if ( tex->layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ) {
		VK_Texture_Barrier( cmd, tex->image, tex->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							VK_Texture_LayoutAccess( tex->layout ),
							VK_ACCESS_TRANSFER_WRITE_BIT, tex->mipLevels );
		tex->layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	}

	memset( &region, 0, sizeof( region ) );
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = (uint32_t)mipLevel;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.width = (uint32_t)width;
	region.imageExtent.height = (uint32_t)height;
	region.imageExtent.depth = 1;

	qvk.CmdCopyBufferToImage( cmd, staging, tex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.EndCommandBuffer( cmd ), "vkEndCommandBuffer(upload)" ) ) {
		goto done;
	}

	memset( &fenceInfo, 0, sizeof( fenceInfo ) );
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	if ( !VK_Guard_Result( VK_GSITE_FENCE, qvk.CreateFence( vk.device, &fenceInfo, NULL, &fence ),
						   "vkCreateFence(upload)" ) ) {
		goto done;
	}

	memset( &submitInfo, 0, sizeof( submitInfo ) );
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;

	if ( !VK_Guard_Result( VK_GSITE_SUBMIT, VK_Queue_Submit( vk.graphicsQueue, 1, &submitInfo, fence ),
						   "vkQueueSubmit(upload)" ) ) {
		goto done;
	}

	result = VK_Sync_WaitForFences( fence, VK_TRUE, "texture-upload", "upload" );
	if ( !VK_Guard_Result( VK_GSITE_FENCE, result, "vkWaitForFences(upload)" ) ) {
		goto done;
	}

	rowBytes = (size_t)width;
	vk_textureBytes += (int)( rowBytes * (size_t)height );
	ok = qtrue;

done:
	if ( fence != VK_NULL_HANDLE && qvk.DestroyFence ) {
		qvk.DestroyFence( vk.device, fence, NULL );
	}
	if ( cmd != VK_NULL_HANDLE && qvk.FreeCommandBuffers ) {
		VK_Pool_FreeBuffers( vk.device, vk.uploadPool, 1, &cmd );
	}
	if ( staging != VK_NULL_HANDLE && qvk.DestroyBuffer ) {
		qvk.DestroyBuffer( vk.device, staging, NULL );
	}
	if ( stagingMemory != VK_NULL_HANDLE && qvk.FreeMemory ) {
		qvk.FreeMemory( vk.device, stagingMemory, NULL );
	}

	VK_Upload_End();

	return ok;
}

/* Lay the image out for sampling: the barrier makes the copy visible to the
   sample, and its absence is an invalid image layout at submit. */
qboolean VK_Texture_Publish( int handle )
{
	vkTexture_t *tex;
	VkCommandBufferAllocateInfo allocInfo;
	VkCommandBufferBeginInfo beginInfo;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFenceCreateInfo fenceInfo;
	VkFence fence = VK_NULL_HANDLE;
	VkSubmitInfo submitInfo;
	VkImageMemoryBarrier barrier;
	VkResult result;
	qboolean ok = qfalse;

	if ( handle < 0 || handle >= VK_MAX_TEXTURES ) {
		return qfalse;
	}

	tex = &vk_textures[handle];
	if ( !tex->inUse ) {
		return qfalse;
	}

	VK_Upload_Begin();

	memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = vk.uploadPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, VK_Pool_AllocBuffers( vk.device, &allocInfo, &cmd ),
						   "vkAllocateCommandBuffers(publish)" ) ) {
		goto done;
	}

	memset( &beginInfo, 0, sizeof( beginInfo ) );
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.BeginCommandBuffer( cmd, &beginInfo ),
						   "vkBeginCommandBuffer(publish)" ) ) {
		goto done;
	}

	/* The barrier leaves the layout the image is actually in.  An image already
	   in SHADER_READ_ONLY (a publish after a publish) has nothing to transfer
	   and must not declare a TRANSFER_DST it never held. */
	if ( tex->layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
		memset( &barrier, 0, sizeof( barrier ) );
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = tex->layout;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = tex->image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = (uint32_t)tex->mipLevels;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = VK_Texture_LayoutAccess( tex->layout );
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		qvk.CmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
								0, 0, NULL, 0, NULL, 1, &barrier );
	}

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.EndCommandBuffer( cmd ), "vkEndCommandBuffer(publish)" ) ) {
		goto done;
	}

	memset( &fenceInfo, 0, sizeof( fenceInfo ) );
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	if ( !VK_Guard_Result( VK_GSITE_FENCE, qvk.CreateFence( vk.device, &fenceInfo, NULL, &fence ),
						   "vkCreateFence(publish)" ) ) {
		goto done;
	}

	memset( &submitInfo, 0, sizeof( submitInfo ) );
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;

	if ( !VK_Guard_Result( VK_GSITE_SUBMIT, VK_Queue_Submit( vk.graphicsQueue, 1, &submitInfo, fence ),
						   "vkQueueSubmit(publish)" ) ) {
		goto done;
	}

	result = VK_Sync_WaitForFences( fence, VK_TRUE, "texture-publish", "upload" );
	if ( VK_Guard_Result( VK_GSITE_FENCE, result, "vkWaitForFences(publish)" ) ) {
		/* the layout is only recorded once the transition is known to have run:
		   a failed wait must not leave the tracker claiming SHADER_READ_ONLY */
		tex->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		ok = qtrue;
	}

done:
	if ( fence != VK_NULL_HANDLE && qvk.DestroyFence ) {
		qvk.DestroyFence( vk.device, fence, NULL );
	}
	if ( cmd != VK_NULL_HANDLE && qvk.FreeCommandBuffers ) {
		VK_Pool_FreeBuffers( vk.device, vk.uploadPool, 1, &cmd );
	}

	VK_Upload_End();

	return ok;
}

/* A readback of texel (0,0): the offline proof that the copy actually landed. */
qboolean VK_Texture_ReadTexel( int handle, int x, int y, unsigned char out[4] )
{
	vkTexture_t *tex;
	VkCommandBufferAllocateInfo allocInfo;
	VkCommandBufferBeginInfo beginInfo;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkBufferImageCopy region;
	VkFenceCreateInfo fenceInfo;
	VkFence fence = VK_NULL_HANDLE;
	VkSubmitInfo submitInfo;
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	void *mapped = NULL;
	VkResult result;
	qboolean ok = qfalse;

	if ( handle < 0 || handle >= VK_MAX_TEXTURES ) {
		return qfalse;
	}

	tex = &vk_textures[handle];
	if ( !tex->inUse ) {
		return qfalse;
	}

	if ( !VK_Memory_CreateBuffer( 16, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
								  &buffer, &memory ) ) {
		return qfalse;
	}

	VK_Upload_Begin();

	result = qvk.MapMemory( vk.device, memory, 0, 16, 0, &mapped );
	if ( !VK_Guard_Result( VK_GSITE_ALLOC, result, "vkMapMemory(readtexel)" ) ) {
		goto done;
	}

	memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = vk.uploadPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, VK_Pool_AllocBuffers( vk.device, &allocInfo, &cmd ),
						   "vkAllocateCommandBuffers(readtexel)" ) ) {
		goto done;
	}

	memset( &beginInfo, 0, sizeof( beginInfo ) );
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.BeginCommandBuffer( cmd, &beginInfo ),
						   "vkBeginCommandBuffer(readtexel)" ) ) {
		goto done;
	}

	memset( &region, 0, sizeof( region ) );
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageOffset.x = x;
	region.imageOffset.y = y;
	region.imageExtent.width = 1;
	region.imageExtent.height = 1;
	region.imageExtent.depth = 1;

	/* the image is in SHADER_READ_ONLY after Publish; a copy source is a read */
	qvk.CmdCopyImageToBuffer( cmd, tex->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
							  buffer, 1, &region );

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.EndCommandBuffer( cmd ), "vkEndCommandBuffer(readtexel)" ) ) {
		goto done;
	}

	memset( &fenceInfo, 0, sizeof( fenceInfo ) );
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	if ( !VK_Guard_Result( VK_GSITE_FENCE, qvk.CreateFence( vk.device, &fenceInfo, NULL, &fence ),
						   "vkCreateFence(readtexel)" ) ) {
		goto done;
	}

	memset( &submitInfo, 0, sizeof( submitInfo ) );
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;

	if ( !VK_Guard_Result( VK_GSITE_SUBMIT, VK_Queue_Submit( vk.graphicsQueue, 1, &submitInfo, fence ),
						   "vkQueueSubmit(readtexel)" ) ) {
		goto done;
	}

	result = VK_Sync_WaitForFences( fence, VK_TRUE, "texel-read", "upload" );
	(void)VK_Guard_Result( VK_GSITE_FENCE, result, "vkWaitForFences(readtexel)" );

	if ( mapped ) {
		memcpy( out, mapped, 4 );
		ok = qtrue;
	}

done:
	if ( fence != VK_NULL_HANDLE && qvk.DestroyFence ) {
		qvk.DestroyFence( vk.device, fence, NULL );
	}
	if ( cmd != VK_NULL_HANDLE && qvk.FreeCommandBuffers ) {
		VK_Pool_FreeBuffers( vk.device, vk.uploadPool, 1, &cmd );
	}
	if ( mapped ) {
		qvk.UnmapMemory( vk.device, memory );
	}
	if ( buffer != VK_NULL_HANDLE && qvk.DestroyBuffer ) {
		qvk.DestroyBuffer( vk.device, buffer, NULL );
	}
	if ( memory != VK_NULL_HANDLE && qvk.FreeMemory ) {
		qvk.FreeMemory( vk.device, memory, NULL );
	}

	VK_Upload_End();

	return ok;
}

void VK_Texture_MakeMips( int handle )
{
	vkTexture_t *tex;
	VkCommandBufferAllocateInfo allocInfo;
	VkCommandBufferBeginInfo beginInfo;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkImageBlit blit;
	VkFenceCreateInfo fenceInfo;
	VkFence fence = VK_NULL_HANDLE;
	VkSubmitInfo submitInfo;
	int32_t mipWidth, mipHeight;
	uint32_t level;
	VkResult result;

	if ( handle < 0 || handle >= VK_MAX_TEXTURES ) {
		return;
	}

	tex = &vk_textures[handle];
	if ( !tex->inUse || tex->mipLevels <= 1 ) {
		return;
	}

	VK_Upload_Begin();

	memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = vk.uploadPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, VK_Pool_AllocBuffers( vk.device, &allocInfo, &cmd ),
						   "vkAllocateCommandBuffers(mips)" ) ) {
		VK_Upload_End();
		return;
	}

	memset( &beginInfo, 0, sizeof( beginInfo ) );
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.BeginCommandBuffer( cmd, &beginInfo ),
						   "vkBeginCommandBuffer(mips)" ) ) {
		goto done;
	}

	mipWidth = (int32_t)tex->width;
	mipHeight = (int32_t)tex->height;

	VK_Texture_Barrier( cmd, tex->image, tex->layout,
						VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_Texture_LayoutAccess( tex->layout ),
						VK_ACCESS_TRANSFER_READ_BIT, 1 );

	for ( level = 1; level < (uint32_t)tex->mipLevels; level++ ) {
		int32_t nextWidth = mipWidth > 1 ? mipWidth / 2 : 1;
		int32_t nextHeight = mipHeight > 1 ? mipHeight / 2 : 1;

		/* Fill this mip from the one above it; a same-image blit runs in
		   GENERAL because one level is read while another is written. */
		VK_Texture_BarrierLevel( cmd, tex->image, level - 1, level,
								 VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
								 VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT );

		memset( &blit, 0, sizeof( blit ) );
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = level - 1;
		blit.srcSubresource.layerCount = 1;
		blit.srcOffsets[1].x = mipWidth;
		blit.srcOffsets[1].y = mipHeight;
		blit.srcOffsets[1].z = 1;
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = level;
		blit.dstSubresource.layerCount = 1;
		blit.dstOffsets[1].x = nextWidth;
		blit.dstOffsets[1].y = nextHeight;
		blit.dstOffsets[1].z = 1;

		qvk.CmdBlitImage( cmd, tex->image, VK_IMAGE_LAYOUT_GENERAL,
						  tex->image, VK_IMAGE_LAYOUT_GENERAL, 1, &blit,
						  VK_FILTER_LINEAR );

		mipWidth = nextWidth;
		mipHeight = nextHeight;
	}

	/* the blit ran with both sides declared GENERAL */
	tex->layout = VK_IMAGE_LAYOUT_GENERAL;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.EndCommandBuffer( cmd ), "vkEndCommandBuffer(mips)" ) ) {
		goto done;
	}

	memset( &fenceInfo, 0, sizeof( fenceInfo ) );
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	if ( !VK_Guard_Result( VK_GSITE_FENCE, qvk.CreateFence( vk.device, &fenceInfo, NULL, &fence ),
						   "vkCreateFence(mips)" ) ) {
		goto done;
	}

	memset( &submitInfo, 0, sizeof( submitInfo ) );
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;

	if ( !VK_Guard_Result( VK_GSITE_SUBMIT, VK_Queue_Submit( vk.graphicsQueue, 1, &submitInfo, fence ),
						   "vkQueueSubmit(mips)" ) ) {
		goto done;
	}

	result = VK_Sync_WaitForFences( fence, VK_TRUE, "mip-build", "upload" );
	(void)VK_Guard_Result( VK_GSITE_FENCE, result, "vkWaitForFences(mips)" );

done:
	if ( fence != VK_NULL_HANDLE && qvk.DestroyFence ) {
		qvk.DestroyFence( vk.device, fence, NULL );
	}
	if ( cmd != VK_NULL_HANDLE && qvk.FreeCommandBuffers ) {
		VK_Pool_FreeBuffers( vk.device, vk.uploadPool, 1, &cmd );
	}

	VK_Upload_End();
}

VkImageView VK_Texture_View( int handle )
{
	if ( handle < 0 || handle >= VK_MAX_TEXTURES ) {
		return VK_NULL_HANDLE;
	}

	return vk_textures[handle].view;
}

qboolean VK_Texture_Sampler( qboolean clamp, qboolean mipmap, qboolean linear, VkSampler *out )
{
	VkSamplerCreateInfo info;
	int i;

	if ( !out ) {
		return qfalse;
	}

	*out = VK_NULL_HANDLE;

	for ( i = 0; i < vk_numSamplers; i++ ) {
		if ( vk_samplers[i].clamp == clamp && vk_samplers[i].mipmap == mipmap
			 && vk_samplers[i].linear == linear ) {
			*out = vk_samplers[i].sampler;
			return qtrue;
		}
	}

	if ( vk_numSamplers >= VK_SAMPLER_SLOTS ) {
		VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "sampler slots exhausted", VK_ERROR_TOO_MANY_OBJECTS );
		return qfalse;
	}

	memset( &info, 0, sizeof( info ) );
	info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	info.magFilter = linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	info.minFilter = linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	info.mipmapMode = mipmap ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
	info.addressModeU = clamp ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
	info.addressModeV = clamp ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
	info.addressModeW = clamp ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
	info.maxLod = mipmap ? VK_LOD_CLAMP_NONE : 0.0f;
	info.anisotropyEnable = VK_FALSE;

	if ( !VK_Guard_Result( VK_GSITE_DESCRIPTOR,
						   qvk.CreateSampler( vk.device, &info, NULL, &vk_samplers[vk_numSamplers].sampler ),
						   "vkCreateSampler" ) ) {
		return qfalse;
	}

	vk_samplers[vk_numSamplers].clamp = clamp;
	vk_samplers[vk_numSamplers].mipmap = mipmap;
	vk_samplers[vk_numSamplers].linear = linear;

	*out = vk_samplers[vk_numSamplers].sampler;
	vk_numSamplers++;

	return qtrue;
}

int VK_Texture_BytesUsed( void )
{
	return vk_textureBytes;
}

int VK_Texture_Count( void )
{
	return vk_numTextures;
}


/* glReadPixels for an uploaded image; callable between frames. */
qboolean VK_Texture_ReadPixels( int handle, unsigned char *out, size_t outBytes )
{
	VkBufferImageCopy copyRegion;
	VkCommandBufferAllocateInfo allocInfo;
	VkCommandBufferBeginInfo beginInfo;
	VkSubmitInfo submitInfo;
	VkFenceCreateInfo fenceInfo;
	VkImageMemoryBarrier barrier;
	vkTexture_t *tex;
	VkFence fence = VK_NULL_HANDLE;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	void *mapped = NULL;
	VkDeviceSize bytes;
	VkResult result;
	qboolean ok = qfalse;

	tex = &vk_textures[handle];
	if ( !tex || tex->image == VK_NULL_HANDLE ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "readback texture out of range", VK_ERROR_TOO_MANY_OBJECTS );
		return qfalse;
	}

	bytes = (VkDeviceSize)tex->width * (VkDeviceSize)tex->height * 4;
	if ( outBytes < (size_t)bytes ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "readback buffer too small", VK_ERROR_TOO_MANY_OBJECTS );
		return qfalse;
	}

	if ( !VK_Memory_CreateBuffer( bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
								  &buffer, &memory ) ) {
		return qfalse;
	}

	VK_Upload_Begin();

	result = qvk.MapMemory( vk.device, memory, 0, bytes, 0, &mapped );
	if ( !VK_Guard_Result( VK_GSITE_ALLOC, result, "readback vkMapMemory" ) ) {
		goto done;
	}

	memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = vk.uploadPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, VK_Pool_AllocBuffers( vk.device, &allocInfo, &cmd ),
						   "readback vkAllocateCommandBuffers" ) ) {
		goto done;
	}

	memset( &beginInfo, 0, sizeof( beginInfo ) );
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.BeginCommandBuffer( cmd, &beginInfo ),
						   "readback vkBeginCommandBuffer" ) ) {
		goto done;
	}

	memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = tex->image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	qvk.CmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
							0, 0, NULL, 0, NULL, 1, &barrier );

	memset( &copyRegion, 0, sizeof( copyRegion ) );
	copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.imageSubresource.layerCount = 1;
	copyRegion.imageExtent.width = tex->width;
	copyRegion.imageExtent.height = tex->height;
	copyRegion.imageExtent.depth = 1;

	qvk.CmdCopyImageToBuffer( cmd, tex->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
							  buffer, 1, &copyRegion );

	memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = tex->image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	qvk.CmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
							0, 0, NULL, 0, NULL, 1, &barrier );

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.EndCommandBuffer( cmd ), "readback vkEndCommandBuffer" ) ) {
		goto done;
	}

	memset( &fenceInfo, 0, sizeof( fenceInfo ) );
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	if ( !VK_Guard_Result( VK_GSITE_FENCE, qvk.CreateFence( vk.device, &fenceInfo, NULL, &fence ),
						   "readback vkCreateFence" ) ) {
		goto done;
	}

	memset( &submitInfo, 0, sizeof( submitInfo ) );
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;

	if ( !VK_Guard_Result( VK_GSITE_SUBMIT, VK_Queue_Submit( vk.graphicsQueue, 1, &submitInfo, fence ),
						   "readback vkQueueSubmit" ) ) {
		goto done;
	}

	if ( !VK_Guard_Result( VK_GSITE_FENCE, VK_Sync_WaitForFences( fence, VK_TRUE, "texel-readback", "upload" ),
						   "readback vkWaitForFences" ) ) {
		goto done;
	}

	memcpy( out, mapped, (size_t)bytes );
	ok = qtrue;

done:
	if ( mapped ) {
		qvk.UnmapMemory( vk.device, memory );
	}
	if ( fence != VK_NULL_HANDLE && qvk.DestroyFence ) {
		qvk.DestroyFence( vk.device, fence, NULL );
	}
	if ( cmd != VK_NULL_HANDLE && qvk.FreeCommandBuffers ) {
		VK_Pool_FreeBuffers( vk.device, vk.uploadPool, 1, &cmd );
	}
	if ( buffer != VK_NULL_HANDLE && qvk.DestroyBuffer ) {
		qvk.DestroyBuffer( vk.device, buffer, NULL );
	}
	if ( memory != VK_NULL_HANDLE && qvk.FreeMemory ) {
		qvk.FreeMemory( vk.device, memory, NULL );
	}

	VK_Upload_End();

	return ok;
}
