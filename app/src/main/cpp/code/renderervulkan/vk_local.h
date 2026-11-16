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
	/* the present rung: com_maxfps owns the pace, the present only decides whether the
	   display may cap it (desktop reads r_swapInterval, Android takes the uncapped rung) */
	VkPresentModeKHR presentMode;
	VkExtent2D extent;
	uint32_t imageCount;
	uint32_t minImageCount;
	VkSurfaceTransformFlagBitsKHR preTransform;
	VkCompositeAlphaFlagBitsKHR compositeAlpha;
	VkImageUsageFlags usage;
	/* the surface's own answers, kept so the creation row can prove what the
	   chosen preTransform was picked against */
	VkSurfaceTransformFlagBitsKHR currentTransform;
	VkSurfaceTransformFlagsKHR supportedTransforms;
	VkExtent2D currentExtent;
} vkSwapchainPlan_t;

/* the flare states the occlusion pool must cover; tr_sun_flare.cpp static-asserts it */
#define VK_FLARE_QUERIES 128

typedef struct {
	qboolean initialized;
	VkInstance instance;
	uint32_t instanceApiVersion;
	uint32_t deviceApiVersion;
	VkPhysicalDevice physicalDevice;
	VkPhysicalDeviceProperties deviceProps;
	VkPhysicalDeviceVulkan12Properties props12;
	qboolean has12Props;
	uint32_t driverID;
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
	/* the rung the live swapchain was created on, so the frame-stats row can
	   name the pacing the display is actually applying */
	VkPresentModeKHR presentMode;
	/* The surface's own minimum, and how many images this process is holding
	   between acquire and present.  Together they are VUID-07783's forward
	   progress test, so the acquire timeout can be chosen from the spec's own
	   predicate instead of guessed at. */
	uint32_t swapchainMinImageCount;
	int imagesHeld;
	/* A present that answered SUBOPTIMAL on the desktop: the rebuild waits for
	   the next acquire instead of restarting inside a present that landed. */
	qboolean swapchainRebuildDue;
	VkImage *swapchainImages;
	VkImageView *swapchainViews;
	VkPipelineCache pipelineCache;
	qboolean pipelineCacheLoaded;
	qboolean pipelineCacheForeign;
	size_t pipelineCacheBytes;
	/* a creation call is consulting the loaded cache: a fault inside that
	   window accuses the blob the same way a load fault does */
	qboolean pipelineConsultInFlight;
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
	/* The stencil readback's staging set, armed at target init: the image's
	   usage and the pass's stencil store op are both creation-time. */
	VkFramebuffer *framebuffers;
	uint32_t framebufferCount;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexMemory;
	VkCommandBuffer uploadCmd;
	VkCommandPool commandPool;
	/* the walk's frame buffers come from commandPool; every transient upload, readback and arena bake comes from the CALLING THREAD's own one-shot
	   pool (VK_Upload_Pool), because a pool is host-synchronised for its whole lifetime including recording and the arena bake records across a frame. */
	VkCommandBuffer commandBuffers[VK_FRAMES_IN_FLIGHT];
	VkFence fences[VK_FRAMES_IN_FLIGHT];
	/* A wait is only owed to a submission this engine made and has not retired:
	   a walk that opens a pass and never reaches PRESENT leaves the fence reset,
	   and waiting on that reset fence forever is the sublevel-transition hang. */
	qboolean fencePending[VK_FRAMES_IN_FLIGHT];
	/* Set by the quiesce: the slot owes nothing for a reason the log need not name. */
	qboolean fenceRetired[VK_FRAMES_IN_FLIGHT];
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
	/* The pass opened with a depth clear of its own.  Until something that can
	   write depth is recorded, a view's depth clear re-writes 1.0 over values the
	   pass already set, and the walk drops it. */
	qboolean depthWritten;
	/* The swapchain pass's colour attachment opened with LOAD_OP_DONT_CARE
	   (r_deferredClear), so the clear the pass used to do at RC_VK_BEGIN is owed
	   to the first draw that writes colour: that draw records it with the value
	   the pass would have written, unless the write itself covers the plane
	   opaquely and settles the debt.  Nothing between the pass begin and that
	   write reads the plane, so the picture is the one the pass clear produced. */
	qboolean colorLoadDeferred;
	qboolean colorClearOwed;
	float deferredClearColor[4];
	/* retail's occlusion-query arm: one query per flare state, two slots deep,
	   so a slot's reset never touches another slot's in-flight queries */
	VkQueryPool flareQueryPool;
	int flareSlot;
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
void VK_PCache_ConsultBegin( void );
void VK_PCache_ConsultEnd( void );
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
void VK_Draw_BindPipeline( VkCommandBuffer cmd, VkPipeline pipeline );
void VK_Draw_BindSet( VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet set,
					  uint32_t dynamicOffset, uint32_t dynamicCount );
qboolean VK_Vertex_Init( void );
void VK_Vertex_Shutdown( void );

qboolean VK_Target_Init( void );
void VK_Target_Shutdown( void );
qboolean VK_Target_Recreate( void );
VkRenderPass VK_Target_MakePass( VkFormat colourFormat, VkImageLayout finalLayout, qboolean withDepth,
								 VkAttachmentLoadOp colourLoad );

/* The stencil plane's readback, one throttled row per armed frame: the copy is
   recorded after the pass closes and counted once the frame's fence signals. */

qboolean VK_Frame_Init( void );
void VK_Frame_Shutdown( void );
qboolean VK_Frame_Begin( int slot );
qboolean VK_Frame_Acquire( int slot );
void VK_Frame_RecordClear( int slot, const float rgba[4] );
qboolean VK_Frame_RecordBegin( int slot );
void VK_Frame_RecordEnd( int slot );
void VK_Frame_Abandon( int slot );
void VK_Frame_Retire( int slot );
void VK_Frame_WaitIdle( void );
qboolean VK_Frame_Submit( int slot );
qboolean VK_Frame_Present( int slot );
int VK_Frame_Count( void );

/* the persistent bring-up sentinel (SDK doc 8) */
void VK_SafeModeArm( void );
void VK_SafeModeClear( void );
qboolean VK_SafeModeMarkerPresent( void );

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
qboolean VKimp_RenderThreadAlive( void );
int VKimp_ParkMsec( void );
int VK_Platform_Validation( void );
int RB_FrameSmpFrame( void );
int RB_FrameCount( void );

/* ---- the object lifetime census (master gate) ---------------------------- */
/* every object class the renderer owns is counted made/gone with the registration generation it belongs to, so a handle whose lifetime crosses a map change reads as a live count that never returns to zero */
enum {
	VK_LIFE_IMAGE = 0,
	VK_LIFE_PASS,
	VK_LIFE_FRAMEBUFFER,
	VK_LIFE_VIEW,
	VK_LIFE_PIPELINE,
	VK_LIFE_SET,
	VK_LIFE_BUFFER,
	VK_LIFE_KINDS
};


int RB_FrameBoneArena( void );

extern volatile qboolean	vkRenderThreadActive;

qboolean VKimp_IsRenderThread( void );
unsigned long VKimp_ThreadID( void );
qboolean VKimp_MainOwnsFrame( void );
qboolean VKimp_SpawnRenderThread( void ( *function )( void ) );
void *VKimp_RendererSleep( void );
void VKimp_FrontEndSleep( void );
/* The slot a handed-off frame record refers to, so the handoff can claim it
   under the mutex the front end's own wait reads. */
int RB_FrameSlot( const void *frame );
void RB_CommandListReset( void );
int RB_WalkMsec( void );
long long RB_WalkUsec64( void );
/* the frame-scoped twin, drained by RE_EndFrame for [SMP:frame]'s renderBack= */
long long RB_WalkFrameUsec64( void );
long long BE_Microseconds64( void );
int BE_Microseconds( void );


qboolean VK_Flare_QueryArm( unsigned int index );
void VK_Flare_QueryEnd( unsigned int index );
qboolean VK_Flare_QueryResult( unsigned int index, unsigned int *samples );
void VK_Flare_QueryReset( int slot );
void VK_Flare_QueryCollect( int slot );
void VK_Flare_QueryShutdown( void );

void VKimp_WalkEnd( int slot );
qboolean VKimp_SlotBusy( int slot );
void VKimp_FrontEndWaitSlot( int slot );
void VKimp_FrontEndWaitIdle( void );

/* every blocking driver wait goes through these, so a hang names its wait */
VkResult VK_Sync_WaitForFences( VkFence fence, VkBool32 waitAll, const char *site, const char *why );
VkResult VK_Sync_DeviceWaitIdle( const char *site, const char *why );
void VKimp_WakeRenderer( void *data );
void VKimp_ShutdownRenderThread( void );
int VKimp_HandshakeViolations( void );
/* Every queue touch the port makes, under one lock.  VkQueue is host
   synchronised and r_smp reaches it from two threads.  The queue lock and the
   pool table are both created by VK_Sync_Init, once, on the main thread from
   VK_Frame_Init, before either thread can reach a pool or a queue. */
void VK_Sync_Init( void );
VkResult VK_Fence_ResetResult( VkFence fence );
VkResult VK_Queue_Submit( VkQueue queue, uint32_t submitCount,
						  const VkSubmitInfo *submits, VkFence fence );
VkResult VK_Queue_Present( VkQueue queue, const VkPresentInfoKHR *present );

/* the live swapchain's present mode, for the frame-stats row */
const char *VK_Swapchain_PresentModeName( void );
VkResult VK_Queue_WaitIdle( VkQueue queue );
/* One pool per producer, one lock per pool: the pool is the unit of host
   synchronization (Vulkan 1.4 s6.2), so the lock is looked up from the pool
   handle, never from the queue.  owner 0 marks the walk's frame pool, which
   only ever has one recording thread. */
qboolean VK_Pool_Register( VkCommandPool pool, unsigned long owner );
VkCommandPool VK_Upload_Pool( void );
qboolean VK_Upload_Ready( void );
int VK_Pool_Count( void );
void VK_Pool_ShutdownAll( void );
VkResult VK_Pool_AllocBuffers( VkCommandPool pool, const VkCommandBufferAllocateInfo *info,
							   VkCommandBuffer *buffers );
void VK_Pool_FreeBuffers( VkCommandPool pool, uint32_t count,
						  const VkCommandBuffer *buffers );
/* A one-shot transfer transaction allocates, records, submits, waits and frees
   entirely under its own pool's lock. */
void VK_Upload_Init( void );
void VK_Upload_Begin( void );
void VK_Upload_End( void );
/* Park the walk thread and drain its frame before re-laying out an image it
   may still sample. */
void VK_Upload_Quiesce( void );
/* The calling thread's id, in the pool-ownership sense: 0 is no owner. */
unsigned long VK_Upload_ThreadID( void );


/* The vertex-input builder declares only what the stage consumes: the locations
   its SPIR-V reads, and the trim of the description array to match. */
unsigned int VK_Pipeline_VertexInputMask( const unsigned int *code, unsigned int words );
unsigned int VK_Pipeline_TrimAttributes( VkVertexInputAttributeDescription *attribs,
										 unsigned int count, unsigned int mask );

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
#ifdef __ANDROID__
qboolean VK_Surface_Refresh( void );
qboolean VK_Surface_WindowChanged( void );
void VK_Surface_WindowTrack( void );
#endif

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
