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
#include "tr_local.h"

#include <stdlib.h>
#include <string.h>

/* retail's target is 24-bit depth with an 8-bit stencil (the shadow volumes
   count stencil), so the combined format is asked for first; a depth-only pick
   leaves stencilBits 0 and the whole volume path stands down */
static const VkFormat vk_depthFormats[] = {
	VK_FORMAT_D24_UNORM_S8_UINT,
	VK_FORMAT_D32_SFLOAT,
	VK_FORMAT_D16_UNORM
};

static qboolean VK_Target_HasStencil( VkFormat format )
{
	return ( format == VK_FORMAT_D24_UNORM_S8_UINT
			 || format == VK_FORMAT_D32_SFLOAT_S8_UINT ) ? qtrue : qfalse;
}

static qboolean VK_Target_DepthOk( VkFormat format )
{
	VkFormatProperties props;

	memset( &props, 0, sizeof( props ) );
	qvk.GetPhysicalDeviceFormatProperties( vk.physicalDevice, format, &props );

	return ( props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ) ? qtrue : qfalse;
}

static void VK_Target_PickDepth( void )
{
	size_t i;

	for ( i = 0; i < sizeof( vk_depthFormats ) / sizeof( vk_depthFormats[0] ); i++ ) {
		if ( VK_Target_DepthOk( vk_depthFormats[i] ) ) {
			vk.depthFormat = vk_depthFormats[i];
			vkConfig.stencilBits = VK_Target_HasStencil( vk.depthFormat ) ? 8 : 0;
			return;
		}
	}

	vk.depthFormat = VK_FORMAT_UNDEFINED;
	vkConfig.stencilBits = 0;
	VK_Guard_Hit( VK_GSITE_DEVICE, "no depth-stencil format", VK_ERROR_FORMAT_NOT_SUPPORTED );
}

/* qa_probe arms the plane's readback, and both halves of the arm -- the image's
   TRANSFER_SRC usage and the pass's stencil STORE -- are fixed when the target is
   created, so a boot that arms it later gets no copy rather than a stale one. */
static qboolean VK_Target_StencilArmed( void )
{
#ifdef PROBE_BUILD_SILENT
	return qfalse;
#else
	return R_ProbeStatsOn();
#endif
}

/* one frame in this many is copied and counted: a readback waits that frame's
   fence, so the row cannot ride every frame */
#define VK_STENCIL_PERIOD	180

static void VK_Target_StencilRelease( void );

/* r_deferredClear: the swapchain pass's colour attachment opens with DONT_CARE
   and the clear moves to the first colour write of the frame (VK_DeferredClear_*).
   The pass is built once, so a change of the cvar takes a vid_restart; a pass
   built with the load op it names is never cleared by both paths. */
static qboolean VK_Target_DeferredClear( void )
{
	cvar_t	*cv = ri.Cvar_Get( "r_deferredClear", "1", 0 );

	return ( cv && cv->integer ) ? qtrue : qfalse;
}

VkRenderPass VK_Target_MakePass( VkFormat colourFormat, VkImageLayout finalLayout, qboolean withDepth,
								 VkAttachmentLoadOp colourLoad )
{
	VkAttachmentDescription attachments[2];
	VkAttachmentReference colourRef;
	VkAttachmentReference depthRef;
	VkSubpassDescription subpass;
	VkSubpassDependency dependencies[2];
	VkRenderPassCreateInfo createInfo;
	VkRenderPass pass = VK_NULL_HANDLE;
	uint32_t attachmentCount = 1;
	VkResult result;

	memset( &attachments, 0, sizeof( attachments ) );
	memset( &colourRef, 0, sizeof( colourRef ) );
	memset( &depthRef, 0, sizeof( depthRef ) );
	memset( &subpass, 0, sizeof( subpass ) );
	memset( &dependencies, 0, sizeof( dependencies ) );
	memset( &createInfo, 0, sizeof( createInfo ) );

	attachments[0].format = colourFormat;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = colourLoad;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = finalLayout;

	colourRef.attachment = 0;
	colourRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colourRef;

	if ( withDepth && vk.depthFormat != VK_FORMAT_UNDEFINED ) {
		attachments[1].format = vk.depthFormat;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		/* the volume pass counts stencil from zero, so the pass clears it and
		   keeps it for the draws between: retail's glClear(STENCIL) */
		attachments[1].stencilLoadOp = VK_Target_HasStencil( vk.depthFormat )
									   ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		/* a copy of the plane after the pass is only defined if the pass stored
		   it, so the readback arm sets STORE and the shipped path keeps DONT_CARE */
		attachments[1].stencilStoreOp = VK_Target_StencilArmed()
										? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		depthRef.attachment = 1;
		depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		subpass.pDepthStencilAttachment = &depthRef;
		attachmentCount = 2;
	}

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
								   | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = 0;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
								   | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
									| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
								   | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
									| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
								   | VK_PIPELINE_STAGE_TRANSFER_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT
									| VK_ACCESS_TRANSFER_READ_BIT;

	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = attachmentCount;
	createInfo.pAttachments = attachments;
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subpass;
	createInfo.dependencyCount = 2;
	createInfo.pDependencies = dependencies;

	result = qvk.CreateRenderPass( vk.device, &createInfo, NULL, &pass );
	if ( result != VK_SUCCESS ) {
		VK_Guard_Hit( VK_GSITE_SYNC, "vkCreateRenderPass", result );
		return VK_NULL_HANDLE;
	}

	return pass;
}

static qboolean VK_Target_DepthCreate( void )
{
	VkImageCreateInfo createInfo;
	VkImageViewCreateInfo viewInfo;
	qboolean armed = VK_Target_StencilArmed();

	if ( vk.depthFormat == VK_FORMAT_UNDEFINED ) {
		return qfalse;
	}

	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	createInfo.imageType = VK_IMAGE_TYPE_2D;
	createInfo.format = vk.depthFormat;
	createInfo.extent.width = vk.swapchainExtent.width;
	createInfo.extent.height = vk.swapchainExtent.height;
	createInfo.extent.depth = 1;
	createInfo.mipLevels = 1;
	createInfo.arrayLayers = 1;
	createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	createInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
					   | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
	if ( armed ) {
		/* a transient, lazily-allocated attachment can never be a copy source:
		   the arm trades both for the one read that names whether the plane holds
		   the frame's volumes */
		createInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
						   | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	if ( !VK_Guard_Result( VK_GSITE_ALLOC, qvk.CreateImage( vk.device, &createInfo, NULL, &vk.depthImage ),
						   "vkCreateImage(depth)" ) ) {
		return qfalse;
	}

	if ( !VK_Memory_AllocImage( vk.depthImage,
							   armed ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
									 : ( VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT
										 | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ),
							   &vk.depthMemory ) ) {
		return qfalse;
	}

	memset( &viewInfo, 0, sizeof( viewInfo ) );
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = vk.depthImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = vk.depthFormat;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT
										  | ( VK_Target_HasStencil( vk.depthFormat )
											  ? VK_IMAGE_ASPECT_STENCIL_BIT : 0 );
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.layerCount = 1;

	return VK_Guard_Result( VK_GSITE_ALLOC, qvk.CreateImageView( vk.device, &viewInfo, NULL, &vk.depthView ),
							"vkCreateImageView(depth)" );
}

static void VK_Target_FramebuffersDestroy( void )
{
	uint32_t i;

	if ( vk.framebuffers ) {
		for ( i = 0; i < vk.framebufferCount; i++ ) {
			if ( vk.framebuffers[i] != VK_NULL_HANDLE && qvk.DestroyFramebuffer ) {
				qvk.DestroyFramebuffer( vk.device, vk.framebuffers[i], NULL );
			}
		}
		free( vk.framebuffers );
		vk.framebuffers = NULL;
	}
	vk.framebufferCount = 0;
}

qboolean VK_Target_Init( void )
{
	uint32_t i;

	vk.targetReady = qfalse;

	if ( vk.swapchain == VK_NULL_HANDLE || vk.swapchainImageCount == 0 ) {
		VK_Guard_Hit( VK_GSITE_SYNC, "target init without a swapchain", VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	VK_Target_PickDepth();

	/* read before the pass and the image are created: the pair is the arm */
	vk.stencilReadback = VK_Target_StencilArmed();

	/* the walk's debt of a colour clear rides on the load op the pass is built
	   with, so the two are chosen together and never disagree at run time */
	vk.colorLoadDeferred = VK_Target_DeferredClear();
	vk.renderPass = VK_Target_MakePass( vk.swapchainFormat, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, qtrue,
										vk.colorLoadDeferred ? VK_ATTACHMENT_LOAD_OP_DONT_CARE
															 : VK_ATTACHMENT_LOAD_OP_CLEAR );
	/* an offscreen target's clear is its content: it keeps the load-op clear */
	vk.offscreenPass = VK_Target_MakePass( VK_FORMAT_R8G8B8A8_UNORM,
										   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, qfalse,
										   VK_ATTACHMENT_LOAD_OP_CLEAR );

	if ( vk.renderPass == VK_NULL_HANDLE ) {
		return qfalse;
	}

	if ( !VK_Target_DepthCreate() ) {
		return qfalse;
	}

	vk.framebuffers = (VkFramebuffer *)calloc( vk.swapchainImageCount, sizeof( VkFramebuffer ) );
	if ( !vk.framebuffers ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "framebuffer array", VK_ERROR_OUT_OF_HOST_MEMORY );
		return qfalse;
	}
	vk.framebufferCount = vk.swapchainImageCount;

	for ( i = 0; i < vk.swapchainImageCount; i++ ) {
		VkImageView attachments[2];
		VkFramebufferCreateInfo createInfo;

		attachments[0] = vk.swapchainViews[i];
		attachments[1] = vk.depthView;

		memset( &createInfo, 0, sizeof( createInfo ) );
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.renderPass = vk.renderPass;
		createInfo.attachmentCount = ( vk.depthView != VK_NULL_HANDLE ) ? 2 : 1;
		createInfo.pAttachments = attachments;
		createInfo.width = vk.swapchainExtent.width;
		createInfo.height = vk.swapchainExtent.height;
		createInfo.layers = 1;

		if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.CreateFramebuffer( vk.device, &createInfo, NULL,
																	 &vk.framebuffers[i] ),
							   "vkCreateFramebuffer" ) ) {
			return qfalse;
		}
	}

	vk.targetReady = qtrue;

	if ( R_ProbeStatsOn() ) {
		VK_Guard_Row( "VK:target", "ready depth=%d framebuffers=%u %ux%u",
					  (int)vk.depthFormat, vk.framebufferCount,
					  vk.swapchainExtent.width, vk.swapchainExtent.height );
	}

	return qtrue;
}

void VK_Target_Shutdown( void )
{
	VK_Target_FramebuffersDestroy();

	if ( vk.depthView != VK_NULL_HANDLE && qvk.DestroyImageView ) {
		qvk.DestroyImageView( vk.device, vk.depthView, NULL );
	}
	vk.depthView = VK_NULL_HANDLE;

	if ( vk.depthImage != VK_NULL_HANDLE && qvk.DestroyImage ) {
		qvk.DestroyImage( vk.device, vk.depthImage, NULL );
	}
	vk.depthImage = VK_NULL_HANDLE;

	if ( vk.depthMemory != VK_NULL_HANDLE && qvk.FreeMemory ) {
		qvk.FreeMemory( vk.device, vk.depthMemory, NULL );
	}
	vk.depthMemory = VK_NULL_HANDLE;

	if ( vk.renderPass != VK_NULL_HANDLE && qvk.DestroyRenderPass ) {
		qvk.DestroyRenderPass( vk.device, vk.renderPass, NULL );
	}
	vk.renderPass = VK_NULL_HANDLE;

	if ( vk.offscreenPass != VK_NULL_HANDLE && qvk.DestroyRenderPass ) {
		qvk.DestroyRenderPass( vk.device, vk.offscreenPass, NULL );
	}
	vk.offscreenPass = VK_NULL_HANDLE;

	VK_Target_StencilRelease();

	vk.targetReady = qfalse;
}

qboolean VK_Target_Recreate( void )
{
	VK_Target_FramebuffersDestroy();

	if ( vk.depthView != VK_NULL_HANDLE && qvk.DestroyImageView ) {
		qvk.DestroyImageView( vk.device, vk.depthView, NULL );
		vk.depthView = VK_NULL_HANDLE;
	}
	if ( vk.depthImage != VK_NULL_HANDLE && qvk.DestroyImage ) {
		qvk.DestroyImage( vk.device, vk.depthImage, NULL );
		vk.depthImage = VK_NULL_HANDLE;
	}
	if ( vk.depthMemory != VK_NULL_HANDLE && qvk.FreeMemory ) {
		qvk.FreeMemory( vk.device, vk.depthMemory, NULL );
		vk.depthMemory = VK_NULL_HANDLE;
	}

	if ( !VK_Target_Init() ) {
		/* the old target is gone and the new one did not land: RecordBegin refuses
		   on the framebuffer range until something rebuilds, so the next acquire
		   owes the pair */
		vk.swapchainRebuildDue = qtrue;
		return qfalse;
	}

	return qtrue;
}

/* ---- the stencil plane's readback --------------------------------------- */

/* The one measurement that separates "the volume never reached the plane" from
   "the plane is armed and the overlay does not paint": the frame's stencil is
   copied out, counted and named beside the volume that frame drew.  The clone of
   vk_swapchain.c's colour readback, at one frame in VK_STENCIL_PERIOD. */

#ifndef PROBE_BUILD_SILENT

static int	s_stencilPending = -1;	/* the frame whose copy is recorded, -1 none */

static void VK_Target_StencilRelease( void )
{
	if ( vk.stencilMapped ) {
		qvk.UnmapMemory( vk.device, vk.stencilMemory );
		vk.stencilMapped = NULL;
	}
	if ( vk.stencilBuffer != VK_NULL_HANDLE && qvk.DestroyBuffer ) {
		qvk.DestroyBuffer( vk.device, vk.stencilBuffer, NULL );
	}
	if ( vk.stencilMemory != VK_NULL_HANDLE && qvk.FreeMemory ) {
		qvk.FreeMemory( vk.device, vk.stencilMemory, NULL );
	}
	vk.stencilBuffer = VK_NULL_HANDLE;
	vk.stencilMemory = VK_NULL_HANDLE;
	vk.stencilBytes = 0;
}

static qboolean VK_Target_StencilEnsure( void )
{
	VkDeviceSize bytes;

	bytes = (VkDeviceSize)vk.swapchainExtent.width * (VkDeviceSize)vk.swapchainExtent.height;
	if ( bytes == 0 ) {
		return qfalse;
	}
	if ( vk.stencilBuffer != VK_NULL_HANDLE && vk.stencilBytes >= bytes ) {
		return qtrue;
	}

	VK_Target_StencilRelease();
	if ( !VK_Memory_CreateBuffer( bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
								  &vk.stencilBuffer, &vk.stencilMemory ) ) {
		return qfalse;
	}
	vk.stencilBytes = bytes;

	return qtrue;
}

/* Recorded after the pass ends: a render-pass attachment may not be a copy
   source inside it, and only a stored plane is defined to copy at all. */
void VK_Target_StencilCopy( VkCommandBuffer cmd )
{
	VkBufferImageCopy copyRegion;
	VkImageMemoryBarrier barrier;
	int frame = RB_FrameCount();

	if ( !vk.stencilReadback || cmd == VK_NULL_HANDLE || vk.depthImage == VK_NULL_HANDLE ) {
		return;
	}
	if ( ( frame % VK_STENCIL_PERIOD ) != 0 ) {
		return;
	}
	if ( !VK_Target_StencilEnsure() ) {
		return;
	}

	memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = vk.depthImage;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	qvk.CmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
							VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier );

	memset( &copyRegion, 0, sizeof( copyRegion ) );
	copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
	copyRegion.imageSubresource.layerCount = 1;
	copyRegion.imageExtent.width = vk.swapchainExtent.width;
	copyRegion.imageExtent.height = vk.swapchainExtent.height;
	copyRegion.imageExtent.depth = 1;

	qvk.CmdCopyImageToBuffer( cmd, vk.depthImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
							  vk.stencilBuffer, 1, &copyRegion );

	/* the next frame's pass loads with UNDEFINED, but leaving the attachment in
	   the layout its own description names keeps the barrier legal anyway */
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	qvk.CmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
							VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, NULL, 0, NULL, 1, &barrier );

	s_stencilPending = frame;
}

/* After the frame's submit: wait its fence, count the texels and name them
   beside the volumes that frame drew, in words a reader can act on. */
void VK_Target_StencilReadback( int slot )
{
	const unsigned char	*texels;
	int				width, height;
	int				x, y, hist[4];
	int				nonzero = 0, minX = 0, minY = 0, maxX = -1, maxY = -1;
	int				volumes = 0, quads = 0, overlay = 0;
	const char		*verdict;

	if ( s_stencilPending < 0 ) {
		return;
	}

	/* a failed submit never signals the fence, so the arm is dropped rather than
	   waited on */
	if ( !vk.fencePending[slot]
		 || !VK_Guard_Result( VK_GSITE_FENCE,
							  VK_Sync_WaitForFences( vk.fences[slot], VK_TRUE, "shadow-stencil", "readback" ),
							  "vkWaitForFences(stencil)" ) ) {
		s_stencilPending = -1;
		return;
	}

	if ( !vk.stencilMapped ) {
		if ( !VK_Guard_Result( VK_GSITE_ALLOC,
							   qvk.MapMemory( vk.device, vk.stencilMemory, 0, vk.stencilBytes, 0,
											  &vk.stencilMapped ),
							   "stencil vkMapMemory" ) ) {
			vk.stencilMapped = NULL;
			s_stencilPending = -1;
			return;
		}
	}

	texels = (const unsigned char *)vk.stencilMapped;
	width = (int)vk.swapchainExtent.width;
	height = (int)vk.swapchainExtent.height;

	hist[0] = hist[1] = hist[2] = hist[3] = 0;
	for ( y = 0; y < height; y++ ) {
		const unsigned char *row = texels + (size_t)y * (size_t)width;

		for ( x = 0; x < width; x++ ) {
			unsigned v = row[x];

			if ( !v ) {
				continue;
			}
			nonzero++;
			if ( v <= 4 ) {
				hist[v - 1]++;
			}
			if ( maxX < 0 || x < minX ) {
				minX = x;
			}
			if ( x > maxX ) {
				maxX = x;
			}
			if ( maxY < 0 || y < minY ) {
				minY = y;
			}
			if ( y > maxY ) {
				maxY = y;
			}
		}
	}

	VK_Shadow_FrameCounters( &volumes, &quads, &overlay );

	/* the row's own conclusion: an empty plane with the volume drawn is the
	   volume landing elsewhere, an armed plane is the overlay's arm to read */
	if ( !nonzero ) {
		verdict = ( quads > 0 ) ? "empty-plane-volume-drew-elsewhere" : "empty-plane-no-volume";
	} else if ( maxX >= 0 && ( maxX - minX ) * ( maxY - minY ) < ( width * height ) / 4096 ) {
		verdict = "plane-armed-sliver";
	} else {
		verdict = "plane-armed-region";
	}

	if ( R_ProbeMarkOn() ) {
		/* the plane is one object per frame, so the row names every view that
		   drew a volume into it: only the last one's walk can still be in the
		   plane at this copy, and the rest are the views it had to be cleared of */
		VK_Guard_Row( "VK:stencil",
					  "f=%d nonzero=%d hist=%d,%d,%d,%d>4=%d bbox=%d,%d..%d,%d of %dx%d "
					  "volumes=%d quads=%d overlay=%d views=%s last=%d verdict=%s store=%d",
					  s_stencilPending, nonzero, hist[0], hist[1], hist[2], hist[3],
					  nonzero - ( hist[0] + hist[1] + hist[2] + hist[3] ),
					  minX, minY, maxX, maxY, width, height,
					  volumes, quads, overlay, VK_Shadow_ViewCensusText(), VK_Shadow_LastView(),
					  verdict, vk.stencilReadback ? 1 : 0 );
	}

	s_stencilPending = -1;
}

#else	/* PROBE_BUILD_SILENT: the shipped build carries no plane readback */

static void VK_Target_StencilRelease( void )
{
}

void VK_Target_StencilCopy( VkCommandBuffer cmd )
{
	( void )cmd;
}

void VK_Target_StencilReadback( int slot )
{
	( void )slot;
}

#endif
