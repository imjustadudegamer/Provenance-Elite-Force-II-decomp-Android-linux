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
#ifndef VK_VK_LOCAL_H
#define VK_VK_LOCAL_H

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_public.h"

#include "vk_procs.h"
#include "vk_guard.h"

#include <vulkan/vulkan.h>

#define VK_TRACE_CVAR "r_vkTrace"
#define VK_GUARD_TRACE_CVAR "r_vkGuardTrace"

#define VK_FRAMES_IN_FLIGHT 2
#define SMP_FRAMES 2
#define RC_END_OF_LIST 0

typedef struct vkPhysDevice_s {
	VkPhysicalDevice device;
	VkPhysicalDeviceProperties props;
	VkPhysicalDeviceDriverProperties driverProps;
	VkPhysicalDeviceDescriptorIndexingFeatures diFeatures;
	VkPhysicalDeviceVulkan12Features features12;
	qboolean hasDriverProps;
	qboolean hasDescriptorIndexing;
	qboolean hasDescriptorIndexingExt;
	qboolean hasMultiDrawIndirect;
	qboolean supportsBindless;
	uint32_t graphicsFamily;
	uint32_t presentFamily;
	qboolean hasPresent;
	VkExtensionProperties *extensions;
	uint32_t extensionCount;
} vkPhysDevice_t;

typedef struct {
	VkSurfaceFormatKHR format;
	VkPresentModeKHR presentMode;
	VkExtent2D extent;
	uint32_t imageCount;
	VkSurfaceTransformFlagBitsKHR preTransform;
	VkCompositeAlphaFlagBitsKHR compositeAlpha;
	VkImageUsageFlags usage;
} vkSwapchainPlan_t;

typedef struct {
	qboolean initialized;
	VkInstance instance;
	uint32_t instanceApiVersion;
	uint32_t deviceApiVersion;
	VkPhysicalDevice physicalDevice;
	VkPhysicalDeviceProperties deviceProps;
	VkDevice device;
	uint32_t graphicsFamily;
	uint32_t presentFamily;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	VkFormat swapchainFormat;
	VkColorSpaceKHR swapchainColorSpace;
	VkExtent2D swapchainExtent;
	uint32_t swapchainImageCount;
	VkImage *swapchainImages;
	VkImageView *swapchainViews;
	VkPipelineCache pipelineCache;
	qboolean pipelineCacheLoaded;
	qboolean pipelineCacheForeign;
	size_t pipelineCacheBytes;
	qboolean descriptorIndexing;
	qboolean descriptorIndexingExt;
	qboolean bindless;
	qboolean multiDrawIndirect;
	qboolean smpActive;
	int frameSlot;
	int frameCount;
	glconfig_t glConfig;
} vkContext_t;

extern vkContext_t vk;
extern refimport_t ri;

qboolean R_GetModeInfo( int *width, int *height, float *windowAspect, int mode );

void R_Init( void );
void RE_Shutdown( qboolean destroyWindow );
qboolean R_SetMode( int mode, glconfig_t *glConfig );
void R_SetFullscreen( qboolean fullScreen, glconfig_t *glConfig );

void R_IssueRenderCommands( void );
void RB_ExecuteRenderCommands( const void *data );
void RB_RenderThread( void );
void R_InitCommandBuffers( void );
void R_ShutdownCommandBuffers( void );
void R_SyncRenderThread( void );
qboolean R_SmpActive( void );
int RB_FrameSmpFrame( void );
int RB_FrameCount( void );
int RB_FrameBoneArena( void );

extern volatile qboolean	vkRenderThreadActive;

qboolean VKimp_IsRenderThread( void );
qboolean VKimp_MainOwnsFrame( void );
qboolean VKimp_SpawnRenderThread( void ( *function )( void ) );
void *VKimp_RendererSleep( void );
void VKimp_FrontEndSleep( void );
void VKimp_WakeRenderer( void *data );
void VKimp_ShutdownRenderThread( void );
int VKimp_HandshakeViolations( void );

void VKimp_AdapterInit( void );
void VKimp_Shutdown( void );

qboolean VK_Instance_Create( const char *const *surfaceExtensions, int numSurfaceExtensions );
void VK_Instance_Destroy( void );

int VK_Device_Select( void );
qboolean VK_Device_Create( void );
void VK_Device_Destroy( void );

qboolean VK_Surface_Create( void );
void VK_Surface_Destroy( void );

qboolean VK_Swapchain_Plan( vkSwapchainPlan_t *plan, int windowWidth, int windowHeight );
qboolean VK_Swapchain_Create( const vkSwapchainPlan_t *plan );
void VK_Swapchain_Destroy( void );
qboolean VK_Swapchain_Recreate( int windowWidth, int windowHeight );

qboolean VK_PipelineCache_Open( void );
void VK_PipelineCache_Close( void );
void VK_PipelineCache_Save( void );


#endif
