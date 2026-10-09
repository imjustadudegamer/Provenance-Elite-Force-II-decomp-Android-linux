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

static const VkFormat vk_depthFormats[] = {
	VK_FORMAT_D32_SFLOAT,
	VK_FORMAT_D24_UNORM_S8_UINT,
	VK_FORMAT_D16_UNORM
};

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
			/* the stencil-bit count the game's own guards read: D32 and D16 carry
			   no stencil, so the shadow-finish overlay that arms GL_NOTEQUAL has
			   to stand down rather than paint the frame unclipped */
			vkConfig.stencilBits = ( vk.depthFormat == VK_FORMAT_D24_UNORM_S8_UINT ) ? 8 : 0;
			return;
		}
	}

	vk.depthFormat = VK_FORMAT_UNDEFINED;
	vkConfig.stencilBits = 0;
	VK_Guard_Hit( VK_GSITE_DEVICE, "no depth-stencil format", VK_ERROR_FORMAT_NOT_SUPPORTED );
}

VkRenderPass VK_Target_MakePass( VkFormat colourFormat, VkImageLayout finalLayout, qboolean withDepth )
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
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
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
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
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
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	if ( !VK_Guard_Result( VK_GSITE_ALLOC, qvk.CreateImage( vk.device, &createInfo, NULL, &vk.depthImage ),
						   "vkCreateImage(depth)" ) ) {
		return qfalse;
	}

	if ( !VK_Memory_AllocImage( vk.depthImage,
							   VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							   &vk.depthMemory ) ) {
		return qfalse;
	}

	memset( &viewInfo, 0, sizeof( viewInfo ) );
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = vk.depthImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = vk.depthFormat;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
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

	vk.renderPass = VK_Target_MakePass( vk.swapchainFormat, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, qtrue );
	vk.offscreenPass = VK_Target_MakePass( VK_FORMAT_R8G8B8A8_UNORM,
										   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, qfalse );

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

	return VK_Target_Init();
}
