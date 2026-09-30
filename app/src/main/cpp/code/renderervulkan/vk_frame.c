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

/* ---- the GPU side of the frame ------------------------------------------ */

static long long s_gpuBucketUsec[VK_GPU_BUCKETS];
static long long s_gpuMarks;
static long long s_gpuHitCap;

/* One command buffer per slot, and the slot's results are read only after its
   own fence has signalled, so a mark costs a timestamp and nothing waits on it.
   The slot is reset by the same command buffer that fills it: CmdResetQueryPool
   is legal outside a render pass, which is exactly where RecordBegin is. */
void VK_PerfGpu_FrameBegin( int slot )
{
	vk.gpuMarkCount[slot] = 0;
	vk.gpuFrameOpen = qfalse;

	if ( !vk.gpuPerfReady || !VK_WalkStatOn() || !qvk.CmdResetQueryPool ) {
		return;
	}

	vk.gpuFrameOpen = qtrue;

	qvk.CmdResetQueryPool( vk.commandBuffers[slot], vk.gpuQueryPool,
						   (uint32_t)( slot * VK_GPU_MAX_MARKS ), (uint32_t)VK_GPU_MAX_MARKS );

	VK_PerfGpu_Mark( slot, VK_GPU_CLEAR );
}

void VK_PerfGpu_Mark( int slot, int bucket )
{
	int i;

	if ( !vk.gpuFrameOpen || !vk.gpuPerfReady || !qvk.CmdWriteTimestamp ) {
		return;
	}

	i = vk.gpuMarkCount[slot];
	if ( i >= VK_GPU_MAX_MARKS ) {
		s_gpuHitCap++;
		return;
	}

	qvk.CmdWriteTimestamp( vk.commandBuffers[slot], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
						   vk.gpuQueryPool, (uint32_t)( slot * VK_GPU_MAX_MARKS + i ) );
	vk.gpuMarkBucket[slot][i] = (unsigned char)bucket;
	vk.gpuMarkCount[slot] = i + 1;
}

/* The terminal mark closes the last class's interval; it carries no bucket of
   its own.  Without it the final interval -- the 2D batch's flushed draws -- is
   unattributed. */
void VK_PerfGpu_FrameEnd( int slot )
{
	int i;

	if ( !vk.gpuFrameOpen || !vk.gpuPerfReady || !qvk.CmdWriteTimestamp ) {
		return;
	}

	i = vk.gpuMarkCount[slot];
	if ( i > 0 && i < VK_GPU_MAX_MARKS ) {
		qvk.CmdWriteTimestamp( vk.commandBuffers[slot], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
							   vk.gpuQueryPool, (uint32_t)( slot * VK_GPU_MAX_MARKS + i ) );
		vk.gpuMarkCount[slot] = i + 1;
	}

	vk.gpuFrameOpen = qfalse;
}

void VK_PerfGpu_Collect( int slot )
{
	uint64_t stamps[VK_GPU_MAX_MARKS];
	int count, i;

	if ( !vk.gpuPerfReady ) {
		return;
	}

	count = vk.gpuMarkCount[slot];
	vk.gpuMarkCount[slot] = 0;

	if ( count < 2 || !qvk.GetQueryPoolResults ) {
		return;
	}

	if ( !VK_Guard_Result( VK_GSITE_QUIRK,
						   qvk.GetQueryPoolResults( vk.device, vk.gpuQueryPool,
													(uint32_t)( slot * VK_GPU_MAX_MARKS ), (uint32_t)count,
													sizeof( stamps ), stamps, sizeof( uint64_t ),
													VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT ),
						   "vkGetQueryPoolResults(timestamp)" ) ) {
		return;
	}

	/* The interval between two marks belongs to the class the earlier mark
	   opened, so the bucket is read one index back from the closing stamp. */
	for ( i = 1; i < count; i++ ) {
		int		 bucket = vk.gpuMarkBucket[slot][i - 1];
		uint64_t delta = stamps[i] - stamps[i - 1];

		if ( bucket >= 0 && bucket < VK_GPU_BUCKETS ) {
			s_gpuBucketUsec[bucket] += (long long)( (double)delta * (double)vk.gpuTimestampPeriod * 0.001 );
		}
	}

	s_gpuMarks += count;
}

void VK_PerfGpu_Take( long long *usecOut, long long *marksOut, long long *hitCapOut )
{
	int i;

	for ( i = 0; i < VK_GPU_BUCKETS; i++ ) {
		usecOut[i] = s_gpuBucketUsec[i];
		s_gpuBucketUsec[i] = 0;
	}
	if ( marksOut ) {
		*marksOut = s_gpuMarks;
	}
	if ( hitCapOut ) {
		*hitCapOut = s_gpuHitCap;
	}
	s_gpuMarks = 0;
	s_gpuHitCap = 0;
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

	/* The queue and upload-section locks exist before the first pool, buffer or
	   queue touch on either thread. */
	VK_Sync_Init();

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

	if ( !VK_Guard_Result( VK_GSITE_SYNC, VK_Pool_AllocBuffers( vk.device, &bufferInfo, vk.commandBuffers ),
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
	vk.imagesHeld = 0;

	/* The GPU pass split needs one timestamp query per mark per slot.  Its
	   availability is the graphics queue family's own timestampValidBits plus a
	   usable period; where either is missing the row simply reports no device
	   split instead of costing anything. */
	vk.gpuPerfReady = qfalse;
	vk.gpuQueryPool = VK_NULL_HANDLE;
	vk.gpuTimestampPeriod = vk.deviceProps.limits.timestampPeriod;

	if ( vk.gpuTimestampPeriod > 0.0f && qvk.CreateQueryPool && qvk.CmdResetQueryPool
		 && qvk.CmdWriteTimestamp && qvk.GetQueryPoolResults
		 && qvk.GetPhysicalDeviceQueueFamilyProperties ) {
		uint32_t				familyCount = 0;
		uint32_t				validBits = 0;
		VkQueueFamilyProperties	*families;
		VkQueryPoolCreateInfo	queryInfo;

		qvk.GetPhysicalDeviceQueueFamilyProperties( vk.physicalDevice, &familyCount, NULL );
		families = (VkQueueFamilyProperties *)malloc( familyCount * sizeof( *families ) );

		if ( families && familyCount > vk.graphicsFamily ) {
			qvk.GetPhysicalDeviceQueueFamilyProperties( vk.physicalDevice, &familyCount, families );
			validBits = families[vk.graphicsFamily].timestampValidBits;
		}
		free( families );

		memset( &queryInfo, 0, sizeof( queryInfo ) );
		queryInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
		queryInfo.queryCount = VK_FRAMES_IN_FLIGHT * VK_GPU_MAX_MARKS;

		if ( validBits > 0
			 && VK_Guard_Result( VK_GSITE_QUIRK,
								 qvk.CreateQueryPool( vk.device, &queryInfo, NULL, &vk.gpuQueryPool ),
								 "vkCreateQueryPool(timestamp)" ) ) {
			vk.gpuPerfReady = qtrue;
		}
	}

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

	if ( vk.gpuQueryPool != VK_NULL_HANDLE && qvk.DestroyQueryPool ) {
		qvk.DestroyQueryPool( vk.device, vk.gpuQueryPool, NULL );
	}
	vk.gpuQueryPool = VK_NULL_HANDLE;
	vk.gpuPerfReady = qfalse;
	memset( vk.gpuMarkCount, 0, sizeof( vk.gpuMarkCount ) );

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
	vk.imagesHeld = 0;
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

		/* The slot's fence has signalled, so its timestamps are resolvable and
		   no wait is added by reading them. */
		VK_PerfGpu_Collect( slot );
	}

	{
		long long openT0 = VK_WALK_T0();

		if ( !VK_Guard_Result( VK_GSITE_FENCE, qvk.ResetFences( vk.device, 1, &vk.fences[slot] ),
							   "vkResetFences" ) ) {
			VK_WALK_ADD( openT0, VK_WP_OPEN );
			return qfalse;
		}

		memset( &beginInfo, 0, sizeof( beginInfo ) );
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		if ( !VK_Guard_Result( VK_GSITE_SYNC,
							   qvk.BeginCommandBuffer( vk.commandBuffers[slot], &beginInfo ),
							   "vkBeginCommandBuffer" ) ) {
			VK_WALK_ADD( openT0, VK_WP_OPEN );
			return qfalse;
		}
		VK_WALK_ADD( openT0, VK_WP_OPEN );
	}

	vk.acquired = qfalse;
	vk.acquiredImage = 0;

	return qtrue;
}

/* Generous but finite, and only ever used in the state that needs it.  ~6
   display periods: long enough that a momentarily stalled compositor is not
   mistaken for a starved swapchain, short enough that the walk comes back. */
#define VK_ACQUIRE_TIMEOUT_NS	100000000ULL

/* VUID-07783 is conditional, not blanket: the timeout must not be UINT64_MAX
   when forward progress cannot be guaranteed.  Forward progress is exactly
   "every acquired image comes back", and the back end knows whether it does,
   because it counts them.  Below the limit the acquire blocks indefinitely,
   which is the Khronos samples' shape and what Android drivers want -- Adreno
   refuses finite timeouts here and turns them into a per-frame warning with
   ragged pacing.  At or past the limit this boot held all 4 images, so
   blocking indefinitely is the wedge itself, and the finite timeout turns it
   into a skipped frame while retire hands the images back. */
static uint64_t VK_Frame_AcquireTimeout( void )
{
	uint32_t spare = vk.swapchainImageCount;

	if ( vk.swapchainMinImageCount > 0 && vk.swapchainMinImageCount < spare ) {
		spare -= vk.swapchainMinImageCount;
	} else {
		spare = 0;
	}

	if ( vk.imagesHeld > (int)spare ) {
		return VK_ACQUIRE_TIMEOUT_NS;
	}

	return UINT64_MAX;
}

static VkResult VK_Frame_AcquireOnce( int slot )
{
	VkResult result;
	uint64_t timeout = VK_Frame_AcquireTimeout();

	result = qvk.AcquireNextImageKHR( vk.device, vk.swapchain, timeout,
									  vk.acquireSemaphores[slot], VK_NULL_HANDLE, &vk.acquiredImage );

	if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ) {
		VK_Guard_Hit( result == VK_ERROR_OUT_OF_DATE_KHR ? VK_GSITE_SWAPCHAIN_OUT_OF_DATE
														 : VK_GSITE_SWAPCHAIN_SUBOPTIMAL,
					  "acquire", result );

		if ( !VK_Swapchain_Recreate( (int)vk.swapchainExtent.width, (int)vk.swapchainExtent.height ) ) {
			return result;
		}
		if ( !VK_Target_Recreate() ) {
			return result;
		}

		result = qvk.AcquireNextImageKHR( vk.device, vk.swapchain, VK_Frame_AcquireTimeout(),
										  vk.acquireSemaphores[slot], VK_NULL_HANDLE, &vk.acquiredImage );
	}

	return result;
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

	result = VK_Frame_AcquireOnce( slot );

	if ( result == VK_ERROR_SURFACE_LOST_KHR ) {
		VK_Guard_Hit( VK_GSITE_SWAPCHAIN_LOST, "acquire", result );
		return qfalse;
	}

	/* Nothing was acquired: the spec leaves the semaphore and the fence
	   untouched, so this slot is clean for the next walk and no image is owed
	   back.  Skipping the frame is the whole point of the finite timeout -- the
	   next walk retries from a state that has an image to take. */
	if ( result == VK_TIMEOUT || result == VK_NOT_READY ) {
		if ( VK_TraceOn( VK_TRACE_CVAR, 1 ) ) {
			VK_Guard_Row( "VK:frame", "acquire-empty slot=%d %s f=%d", slot,
						  VK_Guard_ResultName( result ), RB_FrameCount() );
		}
		return qfalse;
	}

	if ( !VK_Guard_Result( VK_GSITE_ACQUIRE, result, "vkAcquireNextImageKHR" ) ) {
		return qfalse;
	}

	/* The count the next acquire's timeout decision reads.  Nothing else in the
	   back end may hold an image, so acquire is the only increment and present
	   is the only decrement. */
	vk.imagesHeld++;
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

	if ( vk.acquiredImage >= vk.framebufferCount ) {
		VK_Guard_Hit( VK_GSITE_SYNC, "acquired image out of range", VK_ERROR_TOO_MANY_OBJECTS );
		return qfalse;
	}

	/* passOpen has to mean "a render pass is recording", so it is set here and
	   not on entry.  Set on entry, a failed begin left the walk holding a flag
	   that said a pass was open while the command buffer had none, and the
	   retire then ended a buffer that was never begun. */
	vk.passOpen = qtrue;

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

	/* The pool is reset and the frame's first timestamp written here, before the
	   pass opens: CmdResetQueryPool is legal outside a render pass, and the
	   clear the pass performs is then the first interval. */
	VK_PerfGpu_FrameBegin( slot );

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
		/* The flush is what records the 2D batch's draws, so the terminal
		   timestamp comes after it: the UI interval has to contain them. */
		VK_PerfGpu_FrameEnd( slot );
		qvk.CmdEndRenderPass( vk.commandBuffers[slot] );
		/* Ending the buffer belongs with ending the pass, so the present path and
		   the retire path both leave the command buffer in the same state: ended
		   once, ready to submit. */
		qvk.EndCommandBuffer( vk.commandBuffers[slot] );
	}

	vk.passOpen = qfalse;
	vk_back_end_slot = -1;
}

/* A walk that opened the pass and then left without PRESENT used to leave the
   fence reset and the command buffer recording: the next Begin on that slot was
   the wait that never returned.  Worse, it kept the acquired image.  The
   20261014 boot held 2 images from the menu onward and climbed to all 4 by
   shutdown, each one an abandoned walk that never handed its image back, and the
   last acquire then had nothing left to take.

   The pass and the image are the back end's, so closing them is the back end's
   job on every exit.  Retire ends the pass -- finalLayout is PRESENT_SRC, so the
   image is presentable the instant the pass ends -- submits what was recorded,
   which also re-arms the fence so the next Begin on this slot waits on a real
   submission again, and presents, which is the only way the presentation engine
   ever gets an image back.  Acquire and present are therefore paired by
   construction rather than by convention: every RC_VK_BEGIN acquires, and both
   exits from the pass, the PRESENT marker and this retire, hand the image back.

   The contents are whatever the walk managed to record, so a retired frame can
   show a partial image.  That is the correct cost of a walk that died early; the
   alternative was holding the image until the swapchain wedged.  Only a submit
   the queue refused still leaves an image held, and that is a device failure the
   guard names rather than a path this function can repair. */
void VK_Frame_Retire( int slot )
{
	if ( !vk.passOpen ) {
		return;
	}

	if ( vk.commandBuffers[slot] != VK_NULL_HANDLE && qvk.EndCommandBuffer ) {
		VK_Guard_Row( "VK:frame", "retire-pass slot=%d f=%d", slot, RB_FrameCount() );
		RB_2D_Flush();
		VK_PerfGpu_FrameEnd( slot );
		if ( qvk.CmdEndRenderPass ) {
			qvk.CmdEndRenderPass( vk.commandBuffers[slot] );
		}
		vk_back_end_slot = -1;
		qvk.EndCommandBuffer( vk.commandBuffers[slot] );
	}

	vk.passOpen = qfalse;

	if ( VK_Frame_Submit( slot ) ) {
		VK_Frame_Present( slot );
	}
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
		VK_Guard_Result( VK_GSITE_SYNC, VK_Queue_WaitIdle( vk.graphicsQueue ), "vkQueueWaitIdle(quiesce)" );
	}
}

qboolean VK_Frame_Submit( int slot )
{
	VkSubmitInfo submitInfo;
	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkCommandBuffer buffers[2];
	VkCommandBuffer xfer;
	uint32_t count = 0;
	VkResult result;

	/* The command buffer was ended by RecordEnd or Retire, whichever closed the
	   pass; this only hands it to the queue. */
	if ( !vk.frameReady ) {
		return qfalse;
	}

	/* The arena bake's copies, when the walk made any, are a command buffer of
	   their own (a copy may not be recorded inside a render pass) and they come
	   FIRST: every draw of this frame that reads the arena is in the buffer that
	   follows, so queue order is the order the copies were recorded in. */
	xfer = VK_VboStaticXferClose( slot );
	if ( xfer != VK_NULL_HANDLE ) {
		buffers[count++] = xfer;
	}
	buffers[count++] = vk.commandBuffers[slot];

	memset( &submitInfo, 0, sizeof( submitInfo ) );
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &vk.acquireSemaphores[slot];
	submitInfo.pWaitDstStageMask = &waitStage;
	submitInfo.commandBufferCount = count;
	submitInfo.pCommandBuffers = buffers;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &vk.renderSemaphores[slot];

	result = VK_Queue_Submit( vk.graphicsQueue, 1, &submitInfo, vk.fences[slot] );

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

	result = VK_Queue_Present( vk.presentQueue, &presentInfo );

	/* SUBOPTIMAL means the image went to the presentation engine, so it is back
	   in the pool and the held count drops.  OUT_OF_DATE means it may not have,
	   but the swapchain is destroyed by the recreate below, which retires every
	   image it owned and resets the count to zero. */
	if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ) {
		VK_Guard_Hit( result == VK_ERROR_OUT_OF_DATE_KHR ? VK_GSITE_SWAPCHAIN_OUT_OF_DATE
														 : VK_GSITE_SWAPCHAIN_SUBOPTIMAL,
					  "present", result );

		if ( result == VK_SUBOPTIMAL_KHR && vk.imagesHeld > 0 ) {
			vk.imagesHeld--;
		}

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

	/* Handed back, so the presentation engine owns it again. */
	if ( vk.imagesHeld > 0 ) {
		vk.imagesHeld--;
	}

	return qtrue;
}
