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
#ifndef VK_VK_LOCAL_H
#define VK_VK_LOCAL_H

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_public.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "vk_procs.h"
#include "vk_guard.h"

#include <vulkan/vulkan.h>

struct SDL_Window;
typedef struct SDL_Window SDL_Window;
extern SDL_Window *SDL_window;

#define VK_TRACE_CVAR "r_vkTrace"
#define VK_GUARD_TRACE_CVAR "r_vkGuardTrace"

#define VK_FRAMES_IN_FLIGHT 2
#define SMP_FRAMES 2

/* The shade descriptor set table, and the pool budget that has to carry it.
   One set is three combined image samplers plus one dynamic uniform buffer, so
   the pool the table allocates from has to declare 3 * VK_SHADE_SETS of the
   first and VK_SHADE_SETS of the second -- vk_descriptor.c reads both here, so
   the two numbers cannot drift apart again.  Sized from the live boot below. */
#define VK_SHADE_SETS 4096
#define VK_SHADE_SET_SAMPLERS_PER_SET 3
#define VK_SHADE_SET_DYNAMIC_UBOS_PER_SET 1

/* The 2D/first-light path's own descriptor set table, and the set total the
   shared pool has to promise.  Two tables allocate from one pool -- vk_draw.c
   one combined image sampler a set, vk_shade.c three plus a dynamic uniform
   buffer -- so the pool's maxSets is their sum, read from here. */
#define VK_DRAW_SET_SLOTS 256
#define VK_SET_SLOTS_TOTAL ( VK_DRAW_SET_SLOTS + VK_SHADE_SETS )

typedef struct vkPhysDevice_s {
	VkPhysicalDevice device;
	VkPhysicalDeviceProperties props;
	VkPhysicalDeviceDriverProperties driverProps;
	VkPhysicalDeviceVulkan12Properties props12;
	VkPhysicalDeviceDescriptorIndexingFeatures diFeatures;
	VkPhysicalDeviceVulkan12Features features12;
	qboolean hasDriverProps;
	qboolean has12Props;
	qboolean hasDescriptorIndexing;
	qboolean hasDescriptorIndexingExt;
	qboolean hasMultiDrawIndirect;
	qboolean hasUpdateAfterBind;
	qboolean hasVariableDescriptorCount;
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
	VkPhysicalDeviceVulkan12Properties props12;
	qboolean has12Props;
	qboolean headless;
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
	uint32_t bindlessTextures;
	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout bindlessLayout;
	VkDescriptorSetLayout compatLayout;
	qboolean bindless;
	qboolean multiDrawIndirect;
	qboolean updateAfterBind;
	qboolean variableDescriptorCount;
	qboolean smpActive;
	int frameSlot;
	int frameCount;
	VkPhysicalDeviceMemoryProperties memProps;
	VkRenderPass renderPass;
	VkRenderPass offscreenPass;
	VkFormat depthFormat;
	VkImage depthImage;
	VkDeviceMemory depthMemory;
	VkImageView depthView;
	VkFramebuffer *framebuffers;
	uint32_t framebufferCount;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexMemory;
	VkCommandBuffer uploadCmd;
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffers[VK_FRAMES_IN_FLIGHT];
	VkFence fences[VK_FRAMES_IN_FLIGHT];
	/* A wait is only owed to a submission this engine made and has not retired:
	   a walk that opens a pass and never reaches PRESENT leaves the fence reset,
	   and waiting on that reset fence forever is the sublevel-transition hang. */
	qboolean fencePending[VK_FRAMES_IN_FLIGHT];
	/* The pass RC_VK_BEGIN opened, so a walk that dies before PRESENT can close it. */
	qboolean passOpen;
	VkSemaphore acquireSemaphores[VK_FRAMES_IN_FLIGHT];
	VkSemaphore renderSemaphores[VK_FRAMES_IN_FLIGHT];
	uint32_t acquiredImage;
	uint32_t lastPresentedImage;
	qboolean acquired;
	qboolean demoDraw;
	qboolean targetReady;
	qboolean frameReady;
	float clearColor[4];
} vkContext_t;

extern vkContext_t vk;
extern refimport_t ri;

qboolean R_GetModeInfo( int *width, int *height, float *windowAspect, int mode );

int VK_Memory_FindType( uint32_t typeBits, VkMemoryPropertyFlags wanted );
qboolean VK_Memory_AllocFor( VkMemoryRequirements requirements, VkMemoryPropertyFlags wanted,
							 VkDeviceMemory *memory );
qboolean VK_Memory_AllocImage( VkImage image, VkMemoryPropertyFlags wanted, VkDeviceMemory *memory );
qboolean VK_Memory_AllocBuffer( VkBuffer buffer, VkMemoryPropertyFlags wanted, VkDeviceMemory *memory );
qboolean VK_Memory_CreateBuffer( VkDeviceSize size, VkBufferUsageFlags usage,
								 VkMemoryPropertyFlags wanted, VkBuffer *buffer, VkDeviceMemory *memory );

typedef struct {
	float position[2];
	float colour[4];
} vkDemoVertex_t;

#define VK_VERTEX_DEMO_COUNT 3
#define VK_VERTEX_DEMO_BYTES ( VK_VERTEX_DEMO_COUNT * sizeof( vkDemoVertex_t ) )

extern const vkDemoVertex_t vk_demoVertices[VK_VERTEX_DEMO_COUNT];

qboolean VK_Descriptor_Init( void );
qboolean VK_Texture_Init( void );
void VK_Texture_Shutdown( void );
qboolean VK_Draw_Init( void );
void VK_Draw_Shutdown( void );
qboolean VK_Shade_Init( void );
void VK_Shade_Shutdown( void );
qboolean VK_Shade_Descriptors_Init( void );
void VK_Shade_Descriptors_Shutdown( void );
void VK_Descriptor_Shutdown( void );

qboolean VK_Pipeline_Init( void );
void VK_Pipeline_Shutdown( void );
qboolean VK_Pipeline_DrawDemo( VkCommandBuffer cmd );

void VK_Draw_BindRing( VkCommandBuffer cmd );
qboolean VK_Vertex_Init( void );
void VK_Vertex_Shutdown( void );

qboolean VK_Target_Init( void );
void VK_Target_Shutdown( void );
qboolean VK_Target_Recreate( void );
VkRenderPass VK_Target_MakePass( VkFormat colourFormat, VkImageLayout finalLayout, qboolean withDepth );

qboolean VK_Frame_Init( void );
void VK_Frame_Shutdown( void );
qboolean VK_Frame_Begin( int slot );
qboolean VK_Frame_Acquire( int slot );
void VK_Frame_RecordClear( int slot, const float rgba[4] );
qboolean VK_Frame_RecordBegin( int slot );
void VK_Frame_RecordEnd( int slot );
void VK_Frame_Abandon( int slot );
void VK_Frame_WaitIdle( void );
qboolean VK_Frame_Submit( int slot );
qboolean VK_Frame_Present( int slot );
int VK_Frame_Count( void );

qboolean VK_Offscreen_ClearAndRead( int width, int height, const float rgba[4],
									unsigned char *out, size_t outBytes );

void R_Init( void );
void RE_Shutdown( qboolean destroyWindow );
qboolean R_SetMode( int mode, glconfig_t *vkConfig );
qboolean VK_EnsureBroughtUp( const char *where );
void R_SetFullscreen( qboolean fullScreen, glconfig_t *vkConfig );

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
unsigned long VKimp_ThreadID( void );
qboolean VKimp_MainOwnsFrame( void );
qboolean VKimp_SpawnRenderThread( void ( *function )( void ) );
void *VKimp_RendererSleep( void );
void VKimp_FrontEndSleep( void );

/* every blocking driver wait goes through these, so a hang names its wait */
VkResult VK_Sync_WaitForFences( VkFence fence, VkBool32 waitAll, const char *site, const char *why );
VkResult VK_Sync_DeviceWaitIdle( const char *site, const char *why );
void VKimp_WakeRenderer( void *data );
void VKimp_ShutdownRenderThread( void );
int VKimp_HandshakeViolations( void );

void VKimp_AdapterInit( void );
void VKimp_Shutdown( void );
qboolean VKimp_WindowCreate( int width, int height, int fullscreen, qboolean hidden );
unsigned int VKimp_InstanceExtensions( const char **out, unsigned int max );
void VKimp_FillConfig( glconfig_t *config, int width, int height, float aspect, qboolean fullscreen );

qboolean VK_Instance_Create( const char *const *surfaceExtensions, int numSurfaceExtensions );
void VK_Instance_SetLayers( const char *const *layers, int numLayers );
qboolean VK_Instance_LayerAvailable( const char *name );
void VK_Instance_Destroy( void );
void VK_Debug_Init( void );
void VK_Debug_Shutdown( void );

int VK_Device_Select( void );
qboolean VK_Device_Create( void );
void VK_Device_Destroy( void );

qboolean VK_Surface_Create( void );
void VK_Surface_Destroy( void );

qboolean VK_Swapchain_CreateViews( void );
qboolean VK_Swapchain_ReadPixels( uint32_t imageIndex, unsigned char *out, size_t outBytes );
qboolean VK_Swapchain_Plan( vkSwapchainPlan_t *plan, int windowWidth, int windowHeight );
qboolean VK_Swapchain_Create( const vkSwapchainPlan_t *plan );
void VK_Swapchain_Destroy( void );
qboolean VK_Swapchain_Recreate( int windowWidth, int windowHeight );

qboolean VK_PipelineCache_Open( void );
void VK_PipelineCache_Close( void );
void VK_PipelineCache_Save( void );


#ifdef __cplusplus
}
#endif

#endif
