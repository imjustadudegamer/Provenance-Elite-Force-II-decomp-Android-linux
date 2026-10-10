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
 * vk_texture.c -- the texture upload path: a chain's mips are staged into one
 * buffer and copied in one submit (the per-mip path is the fallback), then a
 * layout barrier for sampling.  Every format the loaders report is mapped onto
 * a Vulkan format the device has confirmed (SDK doc 6/7: probed, never
 * assumed).
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

	if ( VK_Upload_Ready() && qvk.DeviceWaitIdle ) {
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

/* The staged-upload lifecycle.  Defined below the upload section; the destroy
   above is its last caller. */
static void VK_Texture_StagingFree( vkTexture_t *tex );
static void VK_Texture_QueueFree( vkTexture_t *tex );

/* the batching instrument: submits and the copies they carried.  per= is mips
   per submit -- 1.0 is the per-mip submit-and-wait this replaces.  fallback=
   counts the mips a failed epoch handed to that path, drop= the queues nothing
   can copy. */
static int	vk_uploadSubmits;
static int	vk_uploadMips;
static int	vk_uploadFallback;
static int	vk_uploadDrops;

void VK_Texture_Destroy( int handle )
{
	vkTexture_t *tex;

	if ( handle < 0 || handle >= VK_MAX_TEXTURES ) {
		return;
	}

	tex = &vk_textures[handle];

	/* a chain queued and never published dies with its image: the copies would
	   name a destroyed image, so the queue is dropped and named.  Every load
	   path publishes what it queues, so this is the failure path, not an exit. */
	if ( tex->pendingCount > 0 ) {
		vk_uploadDrops++;
		VK_Guard_Row( "VK:upload", "staging-drop handle=%d queued=%d bytes=%d at destroy",
					  handle, tex->pendingCount, (int)tex->stagingUsed );
	}

	/* the texture's death is the one place both lifetimes close, and each
	   through its own function */
	VK_Texture_StagingFree( tex );
	VK_Texture_QueueFree( tex );

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

/* ---- the staged upload lifecycle ----------------------------------------- */

/* A chain's mips are staged into one buffer and copied in one submit, instead
   of a buffer, a submit and a wait per mip.  Two lifetimes carry it and neither
   free touches the other:

     queue  the record array: grown by VK_Texture_QueueReserve before anything
            is staged, released only by VK_Texture_QueueFree, whose only caller
            is the texture's own destroy.  A path that appends a record cannot
            run through an array it released itself.
     epoch  the staging buffer and its map: opened by VK_Texture_StagingReserve,
            which returns qtrue only with a map covering the bytes it was asked
            for, closed by VK_Texture_StagingFree after the fence read them.

   A record is queued only after both have succeeded, so at the memcpy and at
   the append the map and the array are both live. */

/* a mip chain is bounded by its largest level, and the region list rides the
   stack below this count */
#define VK_TEXTURE_FLUSH_REGIONS 16
/* the copy's bufferOffset is a multiple of the format's texel block
   (VUID-vkCmdCopyBufferToImage-dstImage-07975); the widest block this module
   maps is a 16-byte BC block, so 16 covers every one of them */
#define VK_UPLOAD_OFFSET_ALIGN 16
#define VK_UPLOAD_QUEUE_MIN    8
/* an epoch never smaller than this, so a 1x1 chain does not allocate per byte */
#define VK_UPLOAD_MIN_EPOCH 1024

/* The staging buffer is one epoch of one chain.  Only this function closes it
   and it never touches the queue.  Every caller has already waited the fence
   that read the buffer (VUID-vkFreeMemory-memory-00677), and a memory object
   mapped at its free is implicitly unmapped (Vulkan 1.4 s11.2); the unmap here
   names the owner, it is not owed. */
static void VK_Texture_StagingFree( vkTexture_t *tex )
{
	if ( tex->stagingMapped != NULL ) {
		qvk.UnmapMemory( vk.device, tex->stagingMemory );
	}
	if ( tex->staging != VK_NULL_HANDLE && qvk.DestroyBuffer ) {
		qvk.DestroyBuffer( vk.device, tex->staging, NULL );
	}
	if ( tex->stagingMemory != VK_NULL_HANDLE && qvk.FreeMemory ) {
		qvk.FreeMemory( vk.device, tex->stagingMemory, NULL );
	}
	tex->staging = VK_NULL_HANDLE;
	tex->stagingMemory = VK_NULL_HANDLE;
	tex->stagingMapped = NULL;
	tex->stagingSize = 0;
	tex->stagingUsed = 0;
	tex->stagingOwner = 0;
}

/* The record array is the texture's own.  Only this function releases it and it
   never touches the staging buffer. */
static void VK_Texture_QueueFree( vkTexture_t *tex )
{
	if ( tex->pending != NULL ) {
		ri.Free( tex->pending );
		tex->pending = NULL;
	}
	tex->pendingCount = 0;
	tex->pendingCap = 0;
	tex->pendingQuiesce = qfalse;
}

/* The block extent and byte size of the formats this module maps, for the epoch
   that is sized once per chain.  An unmapped format fell back to rgba8 at
   create time (VK_Texture_FormatFor), so the default is that format. */
static void VK_Texture_BlockShape( VkFormat format, int *bw, int *bh, int *blockBytes )
{
	switch ( format ) {
	case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:	*bw = 4; *bh = 4; *blockBytes = 8;  break;
	case VK_FORMAT_BC2_UNORM_BLOCK:
	case VK_FORMAT_BC3_UNORM_BLOCK:			*bw = 4; *bh = 4; *blockBytes = 16; break;
	default:								*bw = 1; *bh = 1; *blockBytes = 4;  break;
	}
}

/* Every level of the chain, block-rounded the way the copy's own containment
   rule rounds it (VUID-vkCmdCopyBufferToImage-imageSubresource-07971/07972): the
   upper bound on what one epoch has to hold. */
static VkDeviceSize VK_Texture_ChainBytes( const vkTexture_t *tex )
{
	int bw, bh, blockBytes;
	VkDeviceSize total = 0;
	int w = tex->width;
	int h = tex->height;
	int level;

	VK_Texture_BlockShape( tex->format, &bw, &bh, &blockBytes );

	for ( level = 0; level < tex->mipLevels; level++ ) {
		total += (VkDeviceSize)blockBytes
				 * (VkDeviceSize)( ( w + bw - 1 ) / bw )
				 * (VkDeviceSize)( ( h + bh - 1 ) / bh );

		if ( w > 1 ) {
			w /= 2;
		}
		if ( h > 1 ) {
			h /= 2;
		}
	}

	if ( total < (VkDeviceSize)VK_UPLOAD_MIN_EPOCH ) {
		total = VK_UPLOAD_MIN_EPOCH;
	}

	return total;
}

/* The queue grows before anything is staged, so a failed grow cannot leave
   staged bytes with no record to name them. */
static qboolean VK_Texture_QueueReserve( vkTexture_t *tex )
{
	vkPendingUpload_t *grown;
	vkPendingUpload_t *old;
	int cap;

	if ( tex->pendingCount < tex->pendingCap ) {
		return qtrue;
	}

	cap = tex->pendingCap ? tex->pendingCap * 2 : VK_UPLOAD_QUEUE_MIN;
	grown = (vkPendingUpload_t *)ri.Malloc( (int)( sizeof( *grown ) * (size_t)cap ) );
	if ( grown == NULL ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "upload queue alloc", VK_ERROR_OUT_OF_HOST_MEMORY );
		return qfalse;
	}

	old = tex->pending;
	if ( old != NULL && tex->pendingCount > 0 ) {
		memcpy( grown, old, sizeof( *grown ) * (size_t)tex->pendingCount );
	}
	tex->pending = grown;
	tex->pendingCap = cap;
	if ( old != NULL ) {
		ri.Free( old );
	}

	return qtrue;
}

/* The epoch is open, mapped and large enough before the caller writes a byte.
   It is not grown in place: what it still holds is submitted first, so no queued
   mip is stranded by the reopen.  A failure leaves no epoch, no map and no
   record -- the caller falls back to the per-mip path, never to a null map. */
static qboolean VK_Texture_StagingReserve( int handle, vkTexture_t *tex, size_t bytes,
										   VkDeviceSize *offsetOut )
{
	VkBuffer		buffer = VK_NULL_HANDLE;
	VkDeviceMemory	memory = VK_NULL_HANDLE;
	void			*mapped = NULL;
	VkDeviceSize	offset;
	VkDeviceSize	need;
	VkDeviceSize	chainBytes;
	VkResult		result;

	chainBytes = VK_Texture_ChainBytes( tex );
	offset = ( tex->stagingUsed + ( VK_UPLOAD_OFFSET_ALIGN - 1 ) )
			 & ~(VkDeviceSize)( VK_UPLOAD_OFFSET_ALIGN - 1 );
	need = offset + (VkDeviceSize)bytes;

	if ( tex->stagingMapped != NULL && tex->stagingSize >= need ) {
		*offsetOut = offset;
		return qtrue;
	}

	if ( tex->stagingMapped != NULL ) {
		if ( tex->pendingCount > 0 ) {
			if ( !VK_Texture_Flush( handle ) ) {
				return qfalse;
			}
		} else {
			VK_Texture_StagingFree( tex );
		}
	}

	need = ( (VkDeviceSize)bytes > chainBytes ) ? (VkDeviceSize)bytes : chainBytes;

	if ( !VK_Memory_CreateBuffer( need, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
								  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
								  &buffer, &memory ) ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "upload staging buffer", VK_ERROR_OUT_OF_HOST_MEMORY );
		return qfalse;
	}

	result = qvk.MapMemory( vk.device, memory, 0, need, 0, &mapped );
	if ( !VK_Guard_Result( VK_GSITE_ALLOC, result, "vkMapMemory(staging)" ) ) {
		if ( buffer != VK_NULL_HANDLE && qvk.DestroyBuffer ) {
			qvk.DestroyBuffer( vk.device, buffer, NULL );
		}
		if ( memory != VK_NULL_HANDLE && qvk.FreeMemory ) {
			qvk.FreeMemory( vk.device, memory, NULL );
		}
		return qfalse;
	}

	tex->staging = buffer;
	tex->stagingMemory = memory;
	tex->stagingMapped = (byte *)mapped;
	tex->stagingSize = need;
	tex->stagingUsed = 0;
	tex->stagingOwner = VK_Upload_ThreadID();
	*offsetOut = 0;

	return qtrue;
}

/* Shared by both upload paths, so a bad argument is named once wherever it
   lands. */
static qboolean VK_Texture_UploadArgs( int handle, vkTexture_t **texOut, int mipLevel,
									   int width, int height, const void *data, size_t bytes )
{
	vkTexture_t *tex;

	if ( handle < 0 || handle >= VK_MAX_TEXTURES ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "upload handle out of range", VK_ERROR_TOO_MANY_OBJECTS );
		return qfalse;
	}

	tex = &vk_textures[handle];
	if ( !tex->inUse || data == NULL || bytes == 0 ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "upload of an unusable texture", VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	/* A zero extent cannot be contained in the image
	   (VUID-vkCmdCopyBufferToImage-imageSubresource-07971/07972); the caller
	   that computes one has walked its chain past the last level. */
	if ( width < 1 || height < 1 || mipLevel < 0 || mipLevel >= tex->mipLevels ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "upload level outside the image",
					  VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	*texOut = tex;

	return qtrue;
}

/* The proven per-mip path: a buffer, a copy, a submit and a wait per mip.  It is
   the fallback, not the default, and its body is the one that shipped before the
   batching -- only the shared argument check moved out of it. */
static qboolean VK_Texture_UploadPerMip( vkTexture_t *tex, int mipLevel, int width, int height,
										 const void *data, size_t bytes )
{
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
	allocInfo.commandPool = VK_Upload_Pool();
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, VK_Pool_AllocBuffers( VK_Upload_Pool(), &allocInfo, &cmd ),
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
		VK_Pool_FreeBuffers( VK_Upload_Pool(), 1, &cmd );
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

/* Every queued copy of one chain, in one submit.  The bytes are the per-mip
   path's bytes in the per-mip path's regions, one submit later: the pre-submit
   host writes reach this submit through s7.9's own queue-submission dependency,
   and the staging is HOST_COHERENT (s11.2.1), so no flush call is owed. */
qboolean VK_Texture_Flush( int handle )
{
	vkTexture_t *tex;
	VkCommandBufferAllocateInfo allocInfo;
	VkCommandBufferBeginInfo beginInfo;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkBufferImageCopy regions[VK_TEXTURE_FLUSH_REGIONS];
	VkBufferImageCopy *regionList = regions;
	VkFenceCreateInfo fenceInfo;
	VkFence fence = VK_NULL_HANDLE;
	VkSubmitInfo submitInfo;
	VkImageLayout oldLayout;
	VkResult result;
	int i;
	int count;
	qboolean ok = qfalse;

	if ( handle < 0 || handle >= VK_MAX_TEXTURES ) {
		return qfalse;
	}

	tex = &vk_textures[handle];
	count = tex->pendingCount;
	if ( !tex->inUse || count == 0 ) {
		return qtrue;
	}

	/* The epoch belongs to the thread that opened it.  A flush from any other
	   thread would free a map that thread is still writing through, so it is
	   refused whole: the queue stays, and the owner's own flush drains it. */
	if ( tex->stagingOwner != 0 && tex->stagingOwner != VK_Upload_ThreadID() ) {
		VK_Guard_Row( "VK:upload", "staging-owner handle=%d queued=%d thread=%lu owner=%lu",
					  handle, count, VK_Upload_ThreadID(), tex->stagingOwner );
		return qfalse;
	}

	/* Fail closed.  A queued mip with no mapped buffer behind it is a state
	   nothing in this module can build, so it is dropped and named rather than
	   copied through a null pointer. */
	if ( tex->stagingMapped == NULL || tex->staging == VK_NULL_HANDLE || tex->pending == NULL ) {
		vk_uploadDrops++;
		VK_Guard_Row( "VK:upload", "staging-drop handle=%d queued=%d map=%p buf=%p",
					  handle, count, (void *)tex->stagingMapped, (void *)(size_t)tex->staging );
		VK_Texture_StagingFree( tex );
		tex->pendingCount = 0;
		return qfalse;
	}

	/* Quiesce BEFORE the section: the walk thread can reach a lazy texture load
	   inside the frame this parks, and that load needs the section (the same
	   order the per-mip path takes). */
	if ( tex->pendingQuiesce ) {
		tex->pendingQuiesce = qfalse;
		VK_Upload_Quiesce();
	}

	VK_Upload_Begin();

	if ( count > VK_TEXTURE_FLUSH_REGIONS ) {
		regionList = (VkBufferImageCopy *)ri.Malloc( sizeof( *regionList ) * (size_t)count );
		if ( regionList == NULL ) {
			/* the queue is not lost: it is still there for the next flush */
			VK_Upload_End();
			return qfalse;
		}
	}

	memset( regionList, 0, sizeof( *regionList ) * (size_t)count );
	for ( i = 0; i < count; i++ ) {
		/* a record the epoch does not hold is not copied: reading past the
		   buffer is VUID-vkCmdCopyBufferToImage-pRegions-00171, and a level or
		   extent outside the image is 07967/07971/07972.  Nothing in this
		   module queues one, so the queue goes whole and is named. */
		if ( tex->pending[i].level < 0 || tex->pending[i].level >= tex->mipLevels
			 || tex->pending[i].width < 1 || tex->pending[i].height < 1
			 || tex->pending[i].offset + (VkDeviceSize)tex->pending[i].bytes > tex->stagingSize ) {
			vk_uploadDrops++;
			VK_Guard_Row( "VK:upload", "record-drop handle=%d level=%d w=%d h=%d off=%d bytes=%d epoch=%d",
						  handle, tex->pending[i].level, tex->pending[i].width, tex->pending[i].height,
						  (int)tex->pending[i].offset, (int)tex->pending[i].bytes, (int)tex->stagingSize );
			if ( regionList != regions ) {
				ri.Free( regionList );
			}
			VK_Texture_StagingFree( tex );
			tex->pendingCount = 0;
			VK_Upload_End();
			return qfalse;
		}

		regionList[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		regionList[i].imageSubresource.mipLevel = (uint32_t)tex->pending[i].level;
		regionList[i].imageSubresource.layerCount = 1;
		regionList[i].imageExtent.width = (uint32_t)tex->pending[i].width;
		regionList[i].imageExtent.height = (uint32_t)tex->pending[i].height;
		regionList[i].imageExtent.depth = 1;
		regionList[i].bufferOffset = tex->pending[i].offset;
	}

	memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = VK_Upload_Pool();
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, VK_Pool_AllocBuffers( VK_Upload_Pool(), &allocInfo, &cmd ),
						   "vkAllocateCommandBuffers(flush)" ) ) {
		goto done;
	}

	memset( &beginInfo, 0, sizeof( beginInfo ) );
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.BeginCommandBuffer( cmd, &beginInfo ),
						   "vkBeginCommandBuffer(flush)" ) ) {
		goto done;
	}

	/* The copies want TRANSFER_DST, and the image may be in any layout the boot
	   already left it in: a refill hits SHADER_READ_ONLY, a re-publish hits
	   TRANSFER_DST.  One barrier from the TRACKED layout covers both, and its
	   declared oldLayout is the layout the image actually holds. */
	oldLayout = tex->layout;
	if ( oldLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ) {
		VK_Texture_Barrier( cmd, tex->image, oldLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							VK_Texture_LayoutAccess( oldLayout ),
							VK_ACCESS_TRANSFER_WRITE_BIT, tex->mipLevels );
	}

	qvk.CmdCopyBufferToImage( cmd, tex->staging, tex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							  (uint32_t)count, regionList );

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.EndCommandBuffer( cmd ), "vkEndCommandBuffer(flush)" ) ) {
		goto done;
	}

	memset( &fenceInfo, 0, sizeof( fenceInfo ) );
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	if ( !VK_Guard_Result( VK_GSITE_FENCE, qvk.CreateFence( vk.device, &fenceInfo, NULL, &fence ),
						   "vkCreateFence(flush)" ) ) {
		goto done;
	}

	memset( &submitInfo, 0, sizeof( submitInfo ) );
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;

	if ( !VK_Guard_Result( VK_GSITE_SUBMIT, VK_Queue_Submit( vk.graphicsQueue, 1, &submitInfo, fence ),
						   "vkQueueSubmit(flush)" ) ) {
		goto done;
	}

	result = VK_Sync_WaitForFences( fence, VK_TRUE, "texture-flush", "upload" );
	if ( !VK_Guard_Result( VK_GSITE_FENCE, result, "vkWaitForFences(flush)" ) ) {
		goto done;
	}

	/* the transition is known to have run */
	if ( oldLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ) {
		tex->layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	}
	ok = qtrue;

	vk_uploadSubmits++;
	vk_uploadMips += count;
	if ( R_ProbeStatsOn() && ( vk_uploadSubmits & 63 ) == 0 ) {
		VK_Guard_Row( "VK:upload", "submits=%d mips=%d per=%d.%d bytes=%d fallback=%d drop=%d",
					  vk_uploadSubmits, vk_uploadMips,
					  vk_uploadMips / vk_uploadSubmits,
					  ( vk_uploadMips * 10 / vk_uploadSubmits ) % 10,
					  VK_Texture_BytesUsed(), vk_uploadFallback, vk_uploadDrops );
	}

done:
	if ( fence != VK_NULL_HANDLE && qvk.DestroyFence ) {
		qvk.DestroyFence( vk.device, fence, NULL );
	}
	if ( cmd != VK_NULL_HANDLE && qvk.FreeCommandBuffers ) {
		VK_Pool_FreeBuffers( VK_Upload_Pool(), 1, &cmd );
	}
	if ( regionList != regions ) {
		ri.Free( regionList );
	}

	/* the copies have run, so the epoch is dead either way: the queue array
	   stays with the texture, emptied */
	VK_Texture_StagingFree( tex );
	if ( !ok ) {
		tex->pendingCount = 0;
		vk_uploadDrops++;
	}

	VK_Upload_End();

	return ok;
}

static qboolean VK_Texture_UploadBatched( int handle, vkTexture_t *tex, int mipLevel,
										  int width, int height, const void *data, size_t bytes )
{
	VkDeviceSize offset;

	if ( !VK_Texture_QueueReserve( tex ) ) {
		return qfalse;
	}

	if ( !VK_Texture_StagingReserve( handle, tex, bytes, &offset ) ) {
		/* no epoch, no map: what the queue still holds is drained by its own
		   submit and this mip takes the proven path.  The array stays -- only
		   the queue's owner releases it. */
		vk_uploadFallback++;
		tex->pendingQuiesce = qfalse;
		if ( tex->pendingCount > 0 ) {
			if ( !VK_Texture_Flush( handle ) ) {
				return qfalse;
			}
		}
		VK_Texture_StagingFree( tex );
		return VK_Texture_UploadPerMip( tex, mipLevel, width, height, data, bytes );
	}

	/* the map is live and covers these bytes; the record is appended after the
	   bytes it names, into an array nothing in this call released */
	memcpy( tex->stagingMapped + offset, data, bytes );

	tex->pending[tex->pendingCount].level = mipLevel;
	tex->pending[tex->pendingCount].width = width;
	tex->pending[tex->pendingCount].height = height;
	tex->pending[tex->pendingCount].offset = offset;
	tex->pending[tex->pendingCount].bytes = bytes;
	tex->pendingCount++;
	tex->stagingUsed = offset + (VkDeviceSize)bytes;

	vk_textureBytes += (int)( (size_t)width * (size_t)height );

	return qtrue;
}

qboolean VK_Texture_Upload( int handle, int mipLevel, int width, int height,
							const void *data, size_t bytes )
{
	vkTexture_t *tex;

	if ( !VK_Texture_UploadArgs( handle, &tex, mipLevel, width, height, data, bytes ) ) {
		return qfalse;
	}

	/* A refilled chain is sampled by the walk until its copies run, so the
	   flush parks the walk first (VK_Texture_Flush). */
	if ( tex->layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
		tex->pendingQuiesce = qtrue;
	}

	return VK_Texture_UploadBatched( handle, tex, mipLevel, width, height, data, bytes );
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

	/* the chain's queued copies run before the layout that publishes them: a
	   barrier out of TRANSFER_DST with no copy behind it would publish mips the
	   device never received */
	if ( !VK_Texture_Flush( handle ) ) {
		return qfalse;
	}

	VK_Upload_Begin();

	memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = VK_Upload_Pool();
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, VK_Pool_AllocBuffers( VK_Upload_Pool(), &allocInfo, &cmd ),
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
		VK_Pool_FreeBuffers( VK_Upload_Pool(), 1, &cmd );
	}

	VK_Upload_End();

	return ok;
}

/* A readback of one texel: the offline proof that the copy actually landed, and
   the only way a run names which art a slot really holds.  `blocked' reports
   whether the image is block-compressed, in which case the bytes returned are
   the block's own and not an RGBA texel. */
qboolean VK_Texture_ReadTexel( int handle, int x, int y, unsigned char out[4], int *blocked )
{
	return VK_Texture_ReadBlock( handle, x, y, out, NULL, blocked );
}

/* Same read, and the whole block when the format is block-compressed: the first
   four bytes of a BC2/BC3 block are its alpha plane, so a caller that wants the
   colour needs all sixteen. */
qboolean VK_Texture_ReadBlock( int handle, int x, int y, unsigned char out[4],
							   unsigned char blockOut[16], int *blocked )
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
	VkImageLayout origLayout;
	size_t texelOffset = 0;
	int bx = 0, by = 0, extentW = 1, extentH = 1;
	qboolean ok = qfalse;

	if ( blocked ) {
		*blocked = 0;
	}
	if ( handle < 0 || handle >= VK_MAX_TEXTURES ) {
		return qfalse;
	}

	tex = &vk_textures[handle];
	if ( !tex->inUse ) {
		return qfalse;
	}

	/* a readback proves what the device holds, so a chain still queued goes
	   first: reading past it would prove the staging buffer instead */
	if ( !VK_Texture_Flush( handle ) ) {
		return qfalse;
	}

	/* a compressed block is at most 16 bytes, and an uncompressed texel is 4;
	   the copy writes one row of one block-aligned region */
	if ( !VK_Memory_CreateBuffer( 16, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
								  &buffer, &memory ) ) {
		return qfalse;
	}

	origLayout = tex->layout;

	VK_Upload_Begin();

	result = qvk.MapMemory( vk.device, memory, 0, 16, 0, &mapped );
	if ( !VK_Guard_Result( VK_GSITE_ALLOC, result, "vkMapMemory(readtexel)" ) ) {
		goto done;
	}

	memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = VK_Upload_Pool();
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, VK_Pool_AllocBuffers( VK_Upload_Pool(), &allocInfo, &cmd ),
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

	/* A copy source must be in a transfer layout, and a texel of a
	   block-compressed image lives inside a block: a 1x1 region at (x,y)
	   violates the format's block extent, the queue family's transfer
	   granularity, and the layout rule -- three VUIDs, one per baked surface.
	   The image is copied where it is (the layout it is actually in, which this
	   module tracks per texture) after a barrier into TRANSFER_SRC_OPTIMAL, the
	   region is the block that holds the texel, aligned down and clipped to the
	   subresource edge (the spec lets an extent that is not a whole number of
	   blocks end there), and the bytes are picked apart on the CPU. */
	{
		VkImageMemoryBarrier	barrier;
		int						bw = 1, bh = 1;
		int						bytesPerTexel = 4;

		switch ( tex->format ) {
		case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:	bw = 4; bh = 4; bytesPerTexel = 8;  break;
		case VK_FORMAT_BC2_UNORM_BLOCK:
		case VK_FORMAT_BC3_UNORM_BLOCK:			bw = 4; bh = 4; bytesPerTexel = 16; break;
		default:								break;
		}
		if ( bw > 1 ) {
			*blocked = 1;
		}

		memset( &barrier, 0, sizeof( barrier ) );
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = tex->layout;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = tex->image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		qvk.CmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
								VK_PIPELINE_STAGE_TRANSFER_BIT,
								0, 0, NULL, 0, NULL, 1, &barrier );

		bx = ( x / bw ) * bw;
		by = ( y / bh ) * bh;
		extentW = ( ( bx + bw ) < tex->width ) ? bw : tex->width - bx;
		extentH = ( ( by + bh ) < tex->height ) ? bh : tex->height - by;
		if ( extentW < 1 ) extentW = 1;
		if ( extentH < 1 ) extentH = 1;

		memset( &region, 0, sizeof( region ) );
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.layerCount = 1;
		region.imageOffset.x = bx;
		region.imageOffset.y = by;
		region.imageExtent.width = (uint32_t)extentW;
		region.imageExtent.height = (uint32_t)extentH;
		region.imageExtent.depth = 1;

		qvk.CmdCopyImageToBuffer( cmd, tex->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
								  buffer, 1, &region );

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = origLayout;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		qvk.CmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
								VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
								0, 0, NULL, 0, NULL, 1, &barrier );

		/* the texel's own bytes for an uncompressed image: the copied row starts
		   at the block's left edge, so its column is the remainder */
		texelOffset = ( ( y - by ) * extentW + ( x - bx ) ) * ( bw > 1 ? 0 : 4 );
		(void)bytesPerTexel;
	}

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
		if ( blocked && *blocked ) {
			if ( blockOut ) {
				memcpy( blockOut, mapped, 16 );
			}
			memcpy( out, mapped, 4 );		/* the block's first four bytes */
		} else {
			memcpy( out, (const byte *)mapped + texelOffset, 4 );
		}
		ok = qtrue;
	}

done:
	if ( fence != VK_NULL_HANDLE && qvk.DestroyFence ) {
		qvk.DestroyFence( vk.device, fence, NULL );
	}
	if ( cmd != VK_NULL_HANDLE && qvk.FreeCommandBuffers ) {
		VK_Pool_FreeBuffers( VK_Upload_Pool(), 1, &cmd );
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

	/* the blit reads level 0, so the chain's queued copies run first */
	if ( !VK_Texture_Flush( handle ) ) {
		return;
	}

	VK_Upload_Begin();

	memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = VK_Upload_Pool();
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, VK_Pool_AllocBuffers( VK_Upload_Pool(), &allocInfo, &cmd ),
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
		VK_Pool_FreeBuffers( VK_Upload_Pool(), 1, &cmd );
	}

	VK_Upload_End();
}

/* The created image's own level count and layout: the source chain the loaders
   report (`image->numMipmaps`) and the levels the image actually has are two
   different numbers, and the sampler can only reach the second.  The census
   reads both beside each other. */
int VK_Texture_MipLevels( int handle )
{
	if ( handle < 0 || handle >= VK_MAX_TEXTURES || !vk_textures[handle].inUse ) {
		return 0;
	}

	return vk_textures[handle].mipLevels;
}

const char *VK_Texture_LayoutName( int handle )
{
	if ( handle < 0 || handle >= VK_MAX_TEXTURES || !vk_textures[handle].inUse ) {
		return "none";
	}

	switch ( vk_textures[handle].layout ) {
	case VK_IMAGE_LAYOUT_UNDEFINED:					return "undefined";
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:		return "transfer-dst";
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:	return "shader-read";
	case VK_IMAGE_LAYOUT_GENERAL:					return "general";
	default:										return "other";
	}
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

	if ( !VK_Texture_Flush( handle ) ) {
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
	allocInfo.commandPool = VK_Upload_Pool();
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, VK_Pool_AllocBuffers( VK_Upload_Pool(), &allocInfo, &cmd ),
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
		VK_Pool_FreeBuffers( VK_Upload_Pool(), 1, &cmd );
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
