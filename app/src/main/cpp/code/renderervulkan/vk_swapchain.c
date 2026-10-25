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
#include "vk_local.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static qboolean VK_FormatSupported( VkPhysicalDevice device, VkFormat format )
{
	VkFormatProperties props;

	memset( &props, 0, sizeof( props ) );
	qvk.GetPhysicalDeviceFormatProperties( device, format, &props );

	return ( props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT ) ? qtrue : qfalse;
}

static void VK_PickSurfaceFormat( vkSwapchainPlan_t *plan, VkSurfaceFormatKHR *formats, uint32_t count )
{
	uint32_t i;

	plan->format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	plan->format.format = VK_FORMAT_UNDEFINED;

	for ( i = 0; i < count; i++ ) {
		if ( formats[i].format == VK_FORMAT_B8G8R8A8_UNORM
			 && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
			plan->format = formats[i];
			return;
		}
	}

	for ( i = 0; i < count; i++ ) {
		if ( formats[i].format == VK_FORMAT_R8G8B8A8_UNORM
			 && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
			plan->format = formats[i];
			return;
		}
	}

	if ( count > 0 ) {
		plan->format = formats[0];
	}
}

/* The probe row names the mode: a raw enum makes the boot log useless for the
   question of which side owns the pace. */
static const char *VK_PresentModeName( VkPresentModeKHR mode )
{
	switch ( mode ) {
	case VK_PRESENT_MODE_IMMEDIATE_KHR:		return "IMMEDIATE";
	case VK_PRESENT_MODE_MAILBOX_KHR:		return "MAILBOX";
	case VK_PRESENT_MODE_FIFO_RELAXED_KHR:	return "FIFO_RELAXED";
	default:								return "FIFO";
	}
}

/* The first mode in the preference list the surface actually reports, FIFO last: it is
   the one mode every surface must report, so the walk never falls off the list. */static qboolean VK_PickFirstPresentMode( VkPresentModeKHR *modes, uint32_t count,
										 const VkPresentModeKHR *want, uint32_t wants )
{
	uint32_t i, j;

	for ( i = 0; i < count; i++ ) {
		for ( j = 0; j < wants; j++ ) {
			if ( modes[i] == want[j] ) {
				return qtrue;
			}
		}
	}

	return qfalse;
}

static void VK_PickPresentMode( vkSwapchainPlan_t *plan, VkPresentModeKHR *modes, uint32_t count )
{
	uint32_t i;
	VkPresentModeKHR want[3];
	uint32_t wants = 0;

#ifdef __ANDROID__
	/* EF1's r_swapInterval 0 arm (renderervk/vk.c:773-781): the uncapped rung, and the
	 * com_maxfps budget is the only pace.  r_swapInterval is not read on this target. */
	want[wants++] = VK_PRESENT_MODE_IMMEDIATE_KHR;
	want[wants++] = VK_PRESENT_MODE_MAILBOX_KHR;
	want[wants++] = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
#else
	extern cvar_t *r_swapInterval;	/* vk_platform.c */
	qboolean vsync = ( r_swapInterval == NULL ) || ( r_swapInterval->integer != 0 );

	/* fifo is the only mode every surface must report */
	want[wants++] = vsync ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
#endif

	plan->presentMode = VK_PRESENT_MODE_FIFO_KHR;
	if ( VK_PickFirstPresentMode( modes, count, want, wants ) ) {
		for ( i = 0; i < count; i++ ) {
			uint32_t j;

			for ( j = 0; j < wants; j++ ) {
				if ( modes[i] == want[j] ) {
					plan->presentMode = want[j];
					return;
				}
			}
		}
	}
}

qboolean VK_Swapchain_Plan( vkSwapchainPlan_t *plan, int windowWidth, int windowHeight )
{
	VkSurfaceCapabilitiesKHR caps;
	VkSurfaceFormatKHR *formats;
	VkPresentModeKHR *modes;
	uint32_t numFormats = 0;
	uint32_t numModes = 0;
	VkResult result;

	memset( plan, 0, sizeof( *plan ) );

	if ( vk.surface == VK_NULL_HANDLE || vk.physicalDevice == VK_NULL_HANDLE ) {
		VK_Guard_Hit( VK_GSITE_SWAPCHAIN_CREATE, "no surface", VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	result = qvk.GetPhysicalDeviceSurfaceCapabilitiesKHR( vk.physicalDevice, vk.surface, &caps );
	if ( result != VK_SUCCESS ) {
		VK_Guard_Hit( VK_GSITE_SWAPCHAIN_CREATE, "surface capabilities", result );
		return qfalse;
	}

	result = qvk.GetPhysicalDeviceSurfaceFormatsKHR( vk.physicalDevice, vk.surface, &numFormats, NULL );
	if ( result != VK_SUCCESS || numFormats == 0 ) {
		VK_Guard_Hit( VK_GSITE_SWAPCHAIN_CREATE, "surface formats", result );
		return qfalse;
	}

	formats = (VkSurfaceFormatKHR *)malloc( sizeof( VkSurfaceFormatKHR ) * numFormats );
	if ( !formats ) {
		VK_Guard_Hit( VK_GSITE_SWAPCHAIN_CREATE, "format alloc", VK_ERROR_OUT_OF_HOST_MEMORY );
		return qfalse;
	}
	qvk.GetPhysicalDeviceSurfaceFormatsKHR( vk.physicalDevice, vk.surface, &numFormats, formats );
	VK_PickSurfaceFormat( plan, formats, numFormats );
	free( formats );

	if ( plan->format.format == VK_FORMAT_UNDEFINED ) {
		VK_Guard_Hit( VK_GSITE_SWAPCHAIN_CREATE, "no surface format", VK_ERROR_FORMAT_NOT_SUPPORTED );
		return qfalse;
	}

	result = qvk.GetPhysicalDeviceSurfacePresentModesKHR( vk.physicalDevice, vk.surface, &numModes, NULL );
	if ( result == VK_SUCCESS && numModes > 0 ) {
		modes = (VkPresentModeKHR *)malloc( sizeof( VkPresentModeKHR ) * numModes );
		if ( modes ) {
			qvk.GetPhysicalDeviceSurfacePresentModesKHR( vk.physicalDevice, vk.surface, &numModes, modes );
			VK_PickPresentMode( plan, modes, numModes );
			free( modes );
		}
	}

	plan->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if ( caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT ) {
		plan->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}

	if ( caps.currentExtent.width != 0xFFFFFFFFu && caps.currentExtent.height != 0xFFFFFFFFu ) {
		plan->extent = caps.currentExtent;
	} else {
		plan->extent.width = (uint32_t)windowWidth;
		plan->extent.height = (uint32_t)windowHeight;
		if ( plan->extent.width < caps.minImageExtent.width ) {
			plan->extent.width = caps.minImageExtent.width;
		}
		if ( plan->extent.height < caps.minImageExtent.height ) {
			plan->extent.height = caps.minImageExtent.height;
		}
		if ( caps.maxImageExtent.width && plan->extent.width > caps.maxImageExtent.width ) {
			plan->extent.width = caps.maxImageExtent.width;
		}
		if ( caps.maxImageExtent.height && plan->extent.height > caps.maxImageExtent.height ) {
			plan->extent.height = caps.maxImageExtent.height;
		}
	}

	plan->imageCount = caps.minImageCount + 1;
	plan->minImageCount = caps.minImageCount;
	if ( caps.maxImageCount > 0 && plan->imageCount > caps.maxImageCount ) {
		plan->imageCount = caps.maxImageCount;
	}
	if ( plan->imageCount < VK_FRAMES_IN_FLIGHT ) {
		plan->imageCount = VK_FRAMES_IN_FLIGHT;
	}

	plan->currentTransform = caps.currentTransform;
	plan->supportedTransforms = caps.supportedTransforms;
	plan->currentExtent = caps.currentExtent;

	/* the presentation engine transforms the image by (currentTransform relative to preTransform), so on Android the pre-rotation must be IDENTITY:
	   claiming currentTransform tells the compositor the frame is already rotated and it presents a portrait image.  Android lists every transform as supported, so IDENTITY is the safe claim. */
#if defined(__ANDROID__)
	if ( caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ) {
		plan->preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	} else {
		plan->preTransform = caps.currentTransform;
	}
#else
	plan->preTransform = caps.currentTransform;
#endif
	plan->compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	if ( !VK_FormatSupported( vk.physicalDevice, plan->format.format ) ) {
		VK_Guard_Hit( VK_GSITE_SWAPCHAIN_CREATE, "format lacks color-attachment support",
					  VK_ERROR_FORMAT_NOT_SUPPORTED );
		return qfalse;
	}

	{
		VkFormatProperties props;
		memset( &props, 0, sizeof( props ) );
		qvk.GetPhysicalDeviceFormatProperties( vk.physicalDevice, plan->format.format, &props );
		if ( props.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT ) {
			plan->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}
	}

	return qtrue;
}

qboolean VK_Swapchain_CreateViews( void )
{
	uint32_t i;

	if ( !vk.swapchainImages || !vk.swapchainViews || vk.swapchainImageCount == 0 ) {
		return qfalse;
	}

	for ( i = 0; i < vk.swapchainImageCount; i++ ) {
		VkImageViewCreateInfo viewInfo;

		memset( &viewInfo, 0, sizeof( viewInfo ) );
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = vk.swapchainImages[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = vk.swapchainFormat;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.layerCount = 1;

		if ( !VK_Guard_Result( VK_GSITE_SWAPCHAIN_CREATE,
							   qvk.CreateImageView( vk.device, &viewInfo, NULL, &vk.swapchainViews[i] ),
							   "vkCreateImageView(swapchain)" ) ) {
			return qfalse;
		}
		VK_Life_NoteCreate( VK_LIFE_VIEW );
	}

	return qtrue;
}

qboolean VK_Swapchain_Create( const vkSwapchainPlan_t *plan )
{
	VkSwapchainCreateInfoKHR createInfo;
	VkResult result;

	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = vk.surface;
	createInfo.minImageCount = plan->imageCount;
	createInfo.imageFormat = plan->format.format;
	createInfo.imageColorSpace = plan->format.colorSpace;
	createInfo.imageExtent = plan->extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = plan->usage;
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.preTransform = plan->preTransform;
	createInfo.compositeAlpha = plan->compositeAlpha;
	createInfo.presentMode = plan->presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	result = qvk.CreateSwapchainKHR( vk.device, &createInfo, NULL, &vk.swapchain );
	if ( result != VK_SUCCESS ) {
		VK_Guard_Hit( VK_GSITE_SWAPCHAIN_CREATE, "vkCreateSwapchainKHR", result );
		return qfalse;
	}

	result = qvk.GetSwapchainImagesKHR( vk.device, vk.swapchain, &vk.swapchainImageCount, NULL );
	if ( result != VK_SUCCESS || vk.swapchainImageCount == 0 ) {
		VK_Guard_Hit( VK_GSITE_SWAPCHAIN_CREATE, "swapchain image count", result );
		qvk.DestroySwapchainKHR( vk.device, vk.swapchain, NULL );
		vk.swapchain = VK_NULL_HANDLE;
		return qfalse;
	}

	/* A new swapchain owns no held images and carries the surface's own minimum,
	   which is the other half of the acquire's forward-progress test. */
	vk.swapchainMinImageCount = plan->minImageCount;
	vk.imagesHeld = 0;

	vk.swapchainImages = (VkImage *)malloc( sizeof( VkImage ) * vk.swapchainImageCount );
	vk.swapchainViews = (VkImageView *)calloc( vk.swapchainImageCount, sizeof( VkImageView ) );
	if ( !vk.swapchainImages || !vk.swapchainViews ) {
		VK_Guard_Hit( VK_GSITE_SWAPCHAIN_CREATE, "image array alloc", VK_ERROR_OUT_OF_HOST_MEMORY );
		return qfalse;
	}

	qvk.GetSwapchainImagesKHR( vk.device, vk.swapchain, &vk.swapchainImageCount, vk.swapchainImages );

	vk.swapchainFormat = plan->format.format;
	vk.swapchainColorSpace = plan->format.colorSpace;
	vk.swapchainExtent = plan->extent;

	if ( !VK_Swapchain_CreateViews() ) {
		return qfalse;
	}

	if ( R_ProbeStatsOn() ) {
		VK_Guard_Row( "VK:swapchain",
					  "created images=%u %ux%u format=%d present=%s currentExtent=%ux%u currentTransform=0x%x supportedTransforms=0x%x preTransform=0x%x",
					  vk.swapchainImageCount,
					  plan->extent.width, plan->extent.height,
					  (int)plan->format.format, VK_PresentModeName( plan->presentMode ),
					  plan->currentExtent.width, plan->currentExtent.height,
					  (unsigned)plan->currentTransform, (unsigned)plan->supportedTransforms,
					  (unsigned)plan->preTransform );
	}

	return qtrue;
}

static void VK_Swapchain_ReadbackRelease( void );

void VK_Swapchain_Destroy( void )
{
	uint32_t i;

	/* the staging block is sized from the extent, so a swapchain that goes away
	   retires it rather than leaving a block the new extent cannot use */
	VK_Swapchain_ReadbackRelease();

	if ( vk.swapchainViews && qvk.DestroyImageView ) {
		for ( i = 0; i < vk.swapchainImageCount; i++ ) {
			if ( vk.swapchainViews[i] != VK_NULL_HANDLE ) {
				qvk.DestroyImageView( vk.device, vk.swapchainViews[i], NULL );
				VK_Life_NoteDestroy( VK_LIFE_VIEW );
			}
		}
	}

	if ( vk.swapchain && qvk.DestroySwapchainKHR ) {
		qvk.DestroySwapchainKHR( vk.device, vk.swapchain, NULL );
	}
	vk.swapchain = VK_NULL_HANDLE;

	if ( vk.swapchainImages ) {
		free( vk.swapchainImages );
		vk.swapchainImages = NULL;
	}
	if ( vk.swapchainViews ) {
		free( vk.swapchainViews );
		vk.swapchainViews = NULL;
	}
	vk.swapchainImageCount = 0;
}

/* The readback's staging objects, kept across reads.  Every caller is the walk
   thread (the RC_VK_END census and the screenshot flush behind it), so one set
   needs no lock.  The command buffer stays per-call: VK_Upload_Pool is the
   calling thread's own pool. */
static VkBuffer		s_rbBuffer;
static VkDeviceMemory	s_rbMemory;
static void			*s_rbMapped;
static VkDeviceSize	s_rbBytes;
static VkFence		s_rbFence;

static void VK_Swapchain_ReadbackRelease( void )
{
	if ( s_rbMapped ) {
		qvk.UnmapMemory( vk.device, s_rbMemory );
		s_rbMapped = NULL;
	}
	if ( s_rbFence != VK_NULL_HANDLE && qvk.DestroyFence ) {
		qvk.DestroyFence( vk.device, s_rbFence, NULL );
		s_rbFence = VK_NULL_HANDLE;
	}
	if ( s_rbBuffer != VK_NULL_HANDLE && qvk.DestroyBuffer ) {
		qvk.DestroyBuffer( vk.device, s_rbBuffer, NULL );
		s_rbBuffer = VK_NULL_HANDLE;
	}
	if ( s_rbMemory != VK_NULL_HANDLE && qvk.FreeMemory ) {
		qvk.FreeMemory( vk.device, s_rbMemory, NULL );
		s_rbMemory = VK_NULL_HANDLE;
	}
	s_rbBytes = 0;
}

qboolean VK_Swapchain_ReadPixels( uint32_t imageIndex, unsigned char *out, size_t outBytes )
{
	VkBufferImageCopy copyRegion;
	VkCommandBufferAllocateInfo allocInfo;
	VkCommandBufferBeginInfo beginInfo;
	VkSubmitInfo submitInfo;
	VkImageMemoryBarrier barrier;
	VkFence fence;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkBuffer buffer;
	VkDeviceMemory memory;
	void *mapped;
	VkDeviceSize bytes;
	VkResult result;
	qboolean ok = qfalse;

	if ( vk.swapchain == VK_NULL_HANDLE || !vk.swapchainImages || imageIndex >= vk.swapchainImageCount ) {
		VK_Guard_Hit( VK_GSITE_SWAPCHAIN_CREATE, "readback image out of range", VK_ERROR_TOO_MANY_OBJECTS );
		return qfalse;
	}

	bytes = (VkDeviceSize)vk.swapchainExtent.width * (VkDeviceSize)vk.swapchainExtent.height * 4;

	if ( outBytes < (size_t)bytes ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "readback buffer too small", VK_ERROR_TOO_MANY_OBJECTS );
		return qfalse;
	}

	if ( s_rbBuffer == VK_NULL_HANDLE || s_rbBytes < bytes ) {
		VK_Swapchain_ReadbackRelease();
		if ( !VK_Memory_CreateBuffer( bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
									 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
									 &s_rbBuffer, &s_rbMemory ) ) {
			return qfalse;
		}
		s_rbBytes = bytes;
	}
	buffer = s_rbBuffer;
	memory = s_rbMemory;
	fence = s_rbFence;

	VK_Upload_Begin();

	if ( !s_rbMapped ) {
		result = qvk.MapMemory( vk.device, memory, 0, bytes, 0, &s_rbMapped );
		if ( !VK_Guard_Result( VK_GSITE_ALLOC, result, "readback vkMapMemory" ) ) {
			s_rbMapped = NULL;
			goto done;
		}
	}
	mapped = s_rbMapped;

	if ( fence == VK_NULL_HANDLE ) {
		VkFenceCreateInfo fenceInfo;

		memset( &fenceInfo, 0, sizeof( fenceInfo ) );
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		if ( !VK_Guard_Result( VK_GSITE_FENCE, qvk.CreateFence( vk.device, &fenceInfo, NULL, &s_rbFence ),
							   "readback vkCreateFence" ) ) {
			goto done;
		}
		fence = s_rbFence;
	} else {
		/* a waited fence stays signalled, and a submit into a signalled fence
		   completes instantly and reads back the previous frame */
		VK_Fence_ResetResult( fence );
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
	barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = vk.swapchainImages[imageIndex];
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	qvk.CmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
							0, 0, NULL, 0, NULL, 1, &barrier );

	memset( &copyRegion, 0, sizeof( copyRegion ) );
	copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.imageSubresource.layerCount = 1;
	copyRegion.imageExtent.width = vk.swapchainExtent.width;
	copyRegion.imageExtent.height = vk.swapchainExtent.height;
	copyRegion.imageExtent.depth = 1;

	qvk.CmdCopyImageToBuffer( cmd, vk.swapchainImages[imageIndex],
							  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &copyRegion );

	/* The presented image must come back in the layout the next acquire
	   expects; TRANSFER_SRC made every readback submit a layout violation. */
	memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = vk.swapchainImages[imageIndex];
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.dstAccessMask = 0;

	qvk.CmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
							0, 0, NULL, 0, NULL, 1, &barrier );

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.EndCommandBuffer( cmd ), "readback vkEndCommandBuffer" ) ) {
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

	if ( !VK_Guard_Result( VK_GSITE_FENCE, VK_Sync_WaitForFences( fence, VK_TRUE, "swap-readback", "present" ),
						   "readback vkWaitForFences" ) ) {
		goto done;
	}

	memcpy( out, mapped, (size_t)bytes );
	ok = qtrue;

done:
	/* the staging set is the next read's business; only the per-call command
	   buffer goes back, and a failed fence wait leaves it to be reset above */
	if ( cmd != VK_NULL_HANDLE && qvk.FreeCommandBuffers ) {
		VK_Pool_FreeBuffers( VK_Upload_Pool(), 1, &cmd );
	}

	VK_Upload_End();

	return ok;
}

qboolean VK_Swapchain_Recreate( int windowWidth, int windowHeight )
{
	vkSwapchainPlan_t plan;

	if ( vk.swapchain != VK_NULL_HANDLE ) {
		VK_Sync_DeviceWaitIdle( "swapchain-recreate", "present" );
		VK_Swapchain_Destroy();
	}

#if defined( __ANDROID__ )
	/* The old surface must die after the swapchain built on it and before the
	   next one: Android has already replaced the ANativeWindow it was created
	   from, so a plan against it fails and strands the walk. */
	if ( !VK_Surface_Refresh() ) {
		return qfalse;
	}
#endif

	if ( !VK_Swapchain_Plan( &plan, windowWidth, windowHeight ) ) {
		return qfalse;
	}

	if ( !VK_Swapchain_Create( &plan ) ) {
		return qfalse;
	}

	return qtrue;
}
