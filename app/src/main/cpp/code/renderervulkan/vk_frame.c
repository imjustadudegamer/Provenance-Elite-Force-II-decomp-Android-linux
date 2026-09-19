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
#include "tr_local.h"

#include <stdlib.h>
#include <string.h>

int VK_Frame_Count( void )
{
	int frames = VK_FRAMES_IN_FLIGHT;

	if ( vk.swapchainImageCount > 0 && (uint32_t)frames > vk.swapchainImageCount ) {
		frames = (int)vk.swapchainImageCount;
	}

	return frames;
}

qboolean VK_Frame_Init( void )
{
	VkCommandPoolCreateInfo poolInfo;
	VkCommandBufferAllocateInfo bufferInfo;
	VkFenceCreateInfo fenceInfo;
	VkSemaphoreCreateInfo semaphoreInfo;
	int frames;
	int i;

	frames = VK_Frame_Count();

	memset( &poolInfo, 0, sizeof( poolInfo ) );
	/* The fences come up signalled, so the first Begin of each slot owes a real
	   wait: pending starts true and only a queue-accepted submit re-arms it. */
	memset( vk.fencePending, 0, sizeof( vk.fencePending ) );
	for ( i = 0; i < VK_FRAMES_IN_FLIGHT; i++ ) {
		vk.fencePending[i] = qtrue;
		vk.fenceRetired[i] = qfalse;
	}
	vk.passOpen = qfalse;
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = vk.graphicsFamily;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.CreateCommandPool( vk.device, &poolInfo, NULL, &vk.commandPool ),
						   "vkCreateCommandPool" ) ) {
		return qfalse;
	}

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.CreateCommandPool( vk.device, &poolInfo, NULL, &vk.uploadPool ),
						   "vkCreateCommandPool(upload)" ) ) {
		return qfalse;
	}

	memset( &bufferInfo, 0, sizeof( bufferInfo ) );
	bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	bufferInfo.commandPool = vk.commandPool;
	bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	bufferInfo.commandBufferCount = (uint32_t)frames;

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.AllocateCommandBuffers( vk.device, &bufferInfo, vk.commandBuffers ),
						   "vkAllocateCommandBuffers" ) ) {
		return qfalse;
	}

	memset( &fenceInfo, 0, sizeof( fenceInfo ) );
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	memset( &semaphoreInfo, 0, sizeof( semaphoreInfo ) );
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for ( i = 0; i < frames; i++ ) {
		if ( !VK_Guard_Result( VK_GSITE_FENCE, qvk.CreateFence( vk.device, &fenceInfo, NULL, &vk.fences[i] ),
							   "vkCreateFence" ) ) {
			return qfalse;
		}

		if ( !VK_Guard_Result( VK_GSITE_SEMAPHORE,
							   qvk.CreateSemaphore( vk.device, &semaphoreInfo, NULL, &vk.acquireSemaphores[i] ),
							   "vkCreateSemaphore(acquire)" ) ) {
			return qfalse;
		}

		if ( !VK_Guard_Result( VK_GSITE_SEMAPHORE,
							   qvk.CreateSemaphore( vk.device, &semaphoreInfo, NULL, &vk.renderSemaphores[i] ),
							   "vkCreateSemaphore(render)" ) ) {
			return qfalse;
		}
	}

	vk.frameReady = qtrue;
	vk.acquired = qfalse;
	vk.acquiredImage = 0;
	vk.lastPresentedImage = 0;

	VK_Guard_Row( "VK:frame", "ready slots=%d images=%u", frames, vk.swapchainImageCount );

	return qtrue;
}

void VK_Frame_Shutdown( void )
{
	int frames = VK_Frame_Count();
	int i;

	if ( vk.device == VK_NULL_HANDLE ) {
		return;
	}

	if ( vk.uploadPool != VK_NULL_HANDLE && qvk.DestroyCommandPool ) {
		qvk.DestroyCommandPool( vk.device, vk.uploadPool, NULL );
	}
	vk.uploadPool = VK_NULL_HANDLE;

	if ( vk.commandPool != VK_NULL_HANDLE && qvk.DestroyCommandPool ) {
		qvk.DestroyCommandPool( vk.device, vk.commandPool, NULL );
	}
	vk.commandPool = VK_NULL_HANDLE;

	for ( i = 0; i < frames; i++ ) {
		if ( vk.fences[i] != VK_NULL_HANDLE && qvk.DestroyFence ) {
			qvk.DestroyFence( vk.device, vk.fences[i], NULL );
		}
		vk.fences[i] = VK_NULL_HANDLE;

		if ( vk.acquireSemaphores[i] != VK_NULL_HANDLE && qvk.DestroySemaphore ) {
			qvk.DestroySemaphore( vk.device, vk.acquireSemaphores[i], NULL );
		}
		vk.acquireSemaphores[i] = VK_NULL_HANDLE;

		if ( vk.renderSemaphores[i] != VK_NULL_HANDLE && qvk.DestroySemaphore ) {
			qvk.DestroySemaphore( vk.device, vk.renderSemaphores[i], NULL );
		}
		vk.renderSemaphores[i] = VK_NULL_HANDLE;
	}

	vk.frameReady = qfalse;
	vk.acquired = qfalse;
	memset( vk.fencePending, 0, sizeof( vk.fencePending ) );
	memset( vk.fenceRetired, 0, sizeof( vk.fenceRetired ) );
	vk.passOpen = qfalse;
}

qboolean VK_Frame_Begin( int slot )
{
	VkCommandBufferBeginInfo beginInfo;
	VkResult result;

	if ( !vk.frameReady ) {
		return qfalse;
	}

	/* UINT64_MAX is only survivable when a submission is actually owed.  An
	   abandoned pass leaves the fence reset with nothing in flight, and the
	   next Begin on that slot is the wait that never returns.  A slot the
	   quiesce just retired also owes nothing, and that is the expected case --
	   two rows a registration is noise, so only an unexplained skip is named. */
	if ( !vk.fencePending[slot] ) {
		if ( vk.fenceRetired[slot] ) {
			vk.fenceRetired[slot] = qfalse;
		} else if ( VK_TraceOn( VK_TRACE_CVAR, 1 ) ) {
			VK_Guard_Row( "VK:frame", "begin-without-submit slot=%d f=%d", slot, RB_FrameCount() );
		}
	} else {
		result = VK_Sync_WaitForFences( vk.fences[slot], VK_FALSE, "frame-fence", "frame" );
		if ( !VK_Guard_Result( VK_GSITE_FENCE, result, "vkWaitForFences" ) ) {
			return qfalse;
		}
		vk.fencePending[slot] = qfalse;
	}

	if ( !VK_Guard_Result( VK_GSITE_FENCE, qvk.ResetFences( vk.device, 1, &vk.fences[slot] ),
						   "vkResetFences" ) ) {
		return qfalse;
	}

	memset( &beginInfo, 0, sizeof( beginInfo ) );
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if ( !VK_Guard_Result( VK_GSITE_SYNC,
						   qvk.BeginCommandBuffer( vk.commandBuffers[slot], &beginInfo ),
						   "vkBeginCommandBuffer" ) ) {
		return qfalse;
	}

	vk.acquired = qfalse;
	vk.acquiredImage = 0;

	return qtrue;
}

qboolean VK_Frame_Acquire( int slot )
{
	VkResult result;

	if ( !vk.frameReady || vk.swapchain == VK_NULL_HANDLE ) {
		return qfalse;
	}

	if ( vk.acquired ) {
		return qtrue;
	}

	result = qvk.AcquireNextImageKHR( vk.device, vk.swapchain, UINT64_MAX,
									  vk.acquireSemaphores[slot], VK_NULL_HANDLE, &vk.acquiredImage );

	if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ) {
		VK_Guard_Hit( result == VK_ERROR_OUT_OF_DATE_KHR ? VK_GSITE_SWAPCHAIN_OUT_OF_DATE
														 : VK_GSITE_SWAPCHAIN_SUBOPTIMAL,
					  "acquire", result );

		if ( !VK_Swapchain_Recreate( (int)vk.swapchainExtent.width, (int)vk.swapchainExtent.height ) ) {
			return qfalse;
		}
		if ( !VK_Target_Recreate() ) {
			return qfalse;
		}

		result = qvk.AcquireNextImageKHR( vk.device, vk.swapchain, UINT64_MAX,
										  vk.acquireSemaphores[slot], VK_NULL_HANDLE, &vk.acquiredImage );
	}

	if ( result == VK_ERROR_SURFACE_LOST_KHR ) {
		VK_Guard_Hit( VK_GSITE_SWAPCHAIN_LOST, "acquire", result );
		return qfalse;
	}

	if ( !VK_Guard_Result( VK_GSITE_ACQUIRE, result, "vkAcquireNextImageKHR" ) ) {
		return qfalse;
	}

	vk.acquired = qtrue;

	return qtrue;
}

/* The pass opens at RC_VK_BEGIN and closes at RC_VK_PRESENT, so the UI
   records into it across the paint. */
static VkRenderPassBeginInfo s_passInfo;
static VkClearValue s_clearValues[2];

qboolean VK_Frame_RecordBegin( int slot )
{
	VkClearValue clearValues[2];
	VkRenderPassBeginInfo beginInfo;
	VkRect2D renderArea;

	vk.passOpen = qtrue;

	if ( vk.acquiredImage >= vk.framebufferCount ) {
		VK_Guard_Hit( VK_GSITE_SYNC, "acquired image out of range", VK_ERROR_TOO_MANY_OBJECTS );
		return qfalse;
	}

	memset( clearValues, 0, sizeof( clearValues ) );
	clearValues[0].color.float32[0] = vk.clearColor[0];
	clearValues[0].color.float32[1] = vk.clearColor[1];
	clearValues[0].color.float32[2] = vk.clearColor[2];
	clearValues[0].color.float32[3] = vk.clearColor[3];
	clearValues[1].depthStencil.depth = 1.0f;
	clearValues[1].depthStencil.stencil = 0;

	memset( &renderArea, 0, sizeof( renderArea ) );
	renderArea.offset.x = 0;
	renderArea.offset.y = 0;
	renderArea.extent = vk.swapchainExtent;

	memset( &beginInfo, 0, sizeof( beginInfo ) );
	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.renderPass = vk.renderPass;
	beginInfo.framebuffer = vk.framebuffers[vk.acquiredImage];
	beginInfo.renderArea = renderArea;
	beginInfo.clearValueCount = 2;
	beginInfo.pClearValues = clearValues;

	qvk.CmdBeginRenderPass( vk.commandBuffers[slot], &beginInfo, VK_SUBPASS_CONTENTS_INLINE );

	/* From here to RecordEnd the back end has a command buffer; the tess
	   surfaces the walk performs land in it. */
	vk_back_end_slot = slot;
	rb_set_slot( slot );

	{
		VkViewport viewport;
		VkRect2D scissor;

		memset( &viewport, 0, sizeof( viewport ) );
		viewport.width = (float)vk.swapchainExtent.width;
		viewport.height = (float)vk.swapchainExtent.height;
		viewport.maxDepth = 1.0f;

		memset( &scissor, 0, sizeof( scissor ) );
		scissor.extent = vk.swapchainExtent;

		qvk.CmdSetViewport( vk.commandBuffers[slot], 0, 1, &viewport );
		qvk.CmdSetScissor( vk.commandBuffers[slot], 0, 1, &scissor );
	}

	if ( vk.demoDraw ) {
		/* The demo triangle binds its own buffer, before the ring bind, so the
		   pass's binding is the one the surfaces draw through. */
		VK_Pipeline_DrawDemo( vk.commandBuffers[slot] );
	}

	/* The vertex ring is bound once per pass; every draw offsets into it. */
	VK_Draw_BindRing( vk.commandBuffers[slot] );

	s_passInfo = beginInfo;
	s_clearValues[0] = clearValues[0];
	s_clearValues[1] = clearValues[1];

	(void)s_passInfo;
	(void)s_clearValues;

	return qtrue;
}

void VK_Frame_RecordEnd( int slot )
{
	if ( vk.commandBuffers[slot] != VK_NULL_HANDLE ) {
		RB_2D_Flush();
		qvk.CmdEndRenderPass( vk.commandBuffers[slot] );
	}

	vk.passOpen = qfalse;
	vk_back_end_slot = -1;
}

/* A walk that opened the pass and then left without PRESENT used to leave the
   fence reset and the command buffer recording: the next Begin on that slot was
   the wait that never returned.  The pool is created with RESET_COMMAND_BUFFER,
   so ending here makes the buffer re-recordable and the fence stays retired. */
void VK_Frame_Abandon( int slot )
{
	if ( !vk.passOpen ) {
		return;
	}

	if ( vk.commandBuffers[slot] != VK_NULL_HANDLE && qvk.EndCommandBuffer ) {
		VK_Guard_Row( "VK:frame", "abandon-pass slot=%d f=%d", slot, RB_FrameCount() );
		RB_2D_Flush();
		qvk.EndCommandBuffer( vk.commandBuffers[slot] );
	}

	vk.passOpen = qfalse;
	vk.acquired = qfalse;
	vk_back_end_slot = -1;
}

/* The load tears down every image, shader and descriptor the last frames drew
   through.  GL defers those frees behind the driver's own reference counts; a
   Vulkan destroy is immediate, so the in-flight submissions have to be retired
   first or the device wedges inside the next fence wait.  One call per map
   load, never per frame. */
void VK_Frame_WaitIdle( void )
{
	int i;

	if ( !vk.frameReady ) {
		return;
	}

	for ( i = 0; i < VK_Frame_Count(); i++ ) {
		if ( !vk.fencePending[i] ) {
			continue;
		}
		if ( !VK_Guard_Result( VK_GSITE_FENCE,
							   VK_Sync_WaitForFences( vk.fences[i], VK_FALSE, "frame-fence", "quiesce" ),
							   "vkWaitForFences(quiesce)" ) ) {
			return;
		}
		vk.fencePending[i] = qfalse;
		vk.fenceRetired[i] = qtrue;
	}

	if ( qvk.QueueWaitIdle ) {
		VK_Guard_Result( VK_GSITE_SYNC, qvk.QueueWaitIdle( vk.graphicsQueue ), "vkQueueWaitIdle(quiesce)" );
	}
}

qboolean VK_Frame_Submit( int slot )
{
	VkSubmitInfo submitInfo;
	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkResult result;

	if ( !vk.frameReady ) {
		return qfalse;
	}

	if ( !VK_Guard_Result( VK_GSITE_SYNC, qvk.EndCommandBuffer( vk.commandBuffers[slot] ),
						   "vkEndCommandBuffer" ) ) {
		return qfalse;
	}

	memset( &submitInfo, 0, sizeof( submitInfo ) );
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &vk.acquireSemaphores[slot];
	submitInfo.pWaitDstStageMask = &waitStage;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &vk.commandBuffers[slot];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &vk.renderSemaphores[slot];

	result = qvk.QueueSubmit( vk.graphicsQueue, 1, &submitInfo, vk.fences[slot] );

	if ( !VK_Guard_Result( VK_GSITE_SUBMIT, result, "vkQueueSubmit" ) ) {
		return qfalse;
	}

	/* Only a submission the queue accepted makes the fence worth waiting on. */
	vk.fencePending[slot] = qtrue;

	return qtrue;
}

qboolean VK_Frame_Present( int slot )
{
	VkPresentInfoKHR presentInfo;
	VkResult result;

	if ( !vk.acquired ) {
		return qfalse;
	}

	memset( &presentInfo, 0, sizeof( presentInfo ) );
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &vk.renderSemaphores[slot];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &vk.swapchain;
	presentInfo.pImageIndices = &vk.acquiredImage;

	result = qvk.QueuePresentKHR( vk.presentQueue, &presentInfo );

	if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ) {
		VK_Guard_Hit( result == VK_ERROR_OUT_OF_DATE_KHR ? VK_GSITE_SWAPCHAIN_OUT_OF_DATE
														 : VK_GSITE_SWAPCHAIN_SUBOPTIMAL,
					  "present", result );

		if ( VK_Swapchain_Recreate( (int)vk.swapchainExtent.width, (int)vk.swapchainExtent.height ) ) {
			VK_Target_Recreate();
		}
		vk.acquired = qfalse;

		return ( result == VK_SUBOPTIMAL_KHR ) ? qtrue : qfalse;
	}

	if ( result == VK_ERROR_SURFACE_LOST_KHR ) {
		VK_Guard_Hit( VK_GSITE_SWAPCHAIN_LOST, "present", result );
		vk.acquired = qfalse;
		return qfalse;
	}

	if ( !VK_Guard_Result( VK_GSITE_PRESENT, result, "vkQueuePresentKHR" ) ) {
		vk.acquired = qfalse;
		return qfalse;
	}

	/* The image the frame went to: the readback samples this one, not a fresh
	   acquire, which has never been rendered to. */
	vk.lastPresentedImage = vk.acquiredImage;
	vk.acquired = qfalse;

	return qtrue;
}
