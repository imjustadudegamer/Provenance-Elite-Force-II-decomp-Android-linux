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

/* The device side of the frame.  be= contains the frame-fence wait, but the wait
   is the device draining a single pass that holds the clear, the world, the
   sprites and the 2D batch; a fence number alone cannot say which of them to cut.
   The walk writes a GPU timestamp whenever the work class changes, so the next
   row attributes the pass.  There is one pass per frame, so these buckets are
   that pass's phases; the terminal mark is end-of-pass and carries no bucket. */
enum {
	VK_GPU_CLEAR = 0,  /* pass open: the colour/depth clear and pass setup */
	VK_GPU_WORLD,      /* RC_DRAW_SURFS: world/static surface geometry */
	VK_GPU_SPRITE,     /* RC_SPRITE_SURFS: sprite surfaces */
	VK_GPU_UI,         /* the RC_2D_* cases: the whole 2D batch, flushed by pass end */
	VK_GPU_BUCKETS
};
/* A transition per class change, bounded by the classes plus interleavings; the
   cap is a runaway guard, not a budget -- a frame that hits it stops marking and
   the row's marks= counter names it. */
#define VK_GPU_MAX_MARKS 32

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
	/* The surface's own minimum, and how many images this process is holding
	   between acquire and present.  Together they are VUID-07783's forward
	   progress test, so the acquire timeout can be chosen from the spec's own
	   predicate instead of guessed at. */
	uint32_t swapchainMinImageCount;
	int imagesHeld;
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
	/* The walk's frame buffers come from commandPool.  Every transient upload,
	   readback and arena bake comes from the CALLING THREAD's own one-shot pool
	   (VK_Upload_Pool): a pool is host-synchronised for its whole lifetime
	   including recording (Vulkan 1.4 s6.2, vkspec.txt:14741), and the arena
	   bake records across a frame, so no transaction lock can cover a pool both
	   threads share. */
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
	/* GPU-side pass attribution.  One timestamp query per mark per slot; the
	   pool is reset by the slot's own command buffer, and results are read only
	   after that slot's fence has signalled, so no host wait is added. */
	VkQueryPool gpuQueryPool;
	qboolean gpuPerfReady;
	float gpuTimestampPeriod;
	int gpuMarkCount[VK_FRAMES_IN_FLIGHT];
	unsigned char gpuMarkBucket[VK_FRAMES_IN_FLIGHT][VK_GPU_MAX_MARKS];
	qboolean gpuFrameOpen;
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
void VK_Draw_BindPipeline( VkCommandBuffer cmd, VkPipeline pipeline );
void VK_Draw_BindSet( VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet set,
					  uint32_t dynamicOffset, uint32_t dynamicCount );
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

/* ---- the object lifetime census (master gate) ----------------------------
   Every object class the renderer owns is counted made/gone and reported with
   the registration generation it belongs to, so a handle whose lifetime
   crosses a map change is visible as a live count that never returns to zero.
   The creation sites in the other lanes' files call VK_Life_NoteCreate/Destroy
   with the kind that names them. */
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

extern int vk_lifeGen;

void VK_Life_NoteCreate( int kind );
void VK_Life_NoteDestroy( int kind );
void VK_Life_Census( const char *site );
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

/* The walk's cost split, printed by the r_perfFps row that owns the gate.  Every
   opcode the walk executes lands in one of these, so `rest` is the accounting
   error and not a bucket: a residual that is large is the instrument's own
   defect, not a cost. */
enum {
	VK_WP_GEN = 0, VK_WP_PACK, VK_WP_BIND, VK_WP_REC, VK_WP_FENCE,
	VK_WP_ACQ,   /* vkAcquireNextImageKHR: the device returning a presented image */
	VK_WP_OPEN,  /* pass open, framebuffer open, command buffer record begin */
	VK_WP_UI,    /* the RC_2D_* cases: the whole 2D command path */
	VK_WP_SHADE, /* per-surface shader/entity bookkeeping, minus the binds it flushes */
	VK_WP_OUT,   /* record end, queue submit, queue present */
	VK_WP_PIX,   /* the presented-frame readback: device copy plus the host census */
	VK_WP_PHASES
};
long long BE_Microseconds64( void );
qboolean VK_WalkStatOn( void );
void VK_WalkStat_Add( int phase, long long usec );
/* The nested buckets a caller's window contains, so an outer wrap can subtract
   them and no phase is counted twice. */
long long VK_WalkStat_Nested( void );
/* The nested amount a window added, robust to a drain landing inside it: the
   counters are cumulative since the last drain and the front end drains them, so
   `Nested() - n0` goes negative on a mid-window drain and the window's own
   subtraction then ADDS the whole stale count instead of removing a part of it. */
long long VK_WalkStat_NestedSince( long long n0 );
void VK_WalkStat_Frame( int verts, int surfs );
/* Batching elisions the frame made, so a change that removes driver calls says
   how many it removed instead of only how the wall time moved.  VK_WP_HITS is
   the number of the frame's own counters the row prints. */
enum { VK_ELIDE_INDEX = 0, VK_ELIDE_UBO, VK_ELIDE_PIPE, VK_ELIDE_SET, VK_ELIDE_HITS };
void VK_WalkStat_Count( int what );
void VK_WalkStat_TakeElisions( long long *out );
void VK_WalkStat_Take( long long *usecOut, long long *vertsOut, long long *surfsOut,
					   long long *framesOut );
/* -1 is "not timed", so a real reading of 0 is still a reading.  The reading
   has to be 64-bit: BE_Microseconds truncates to int, so it goes negative once
   the machine has been up 2^31 us -- 35.8 minutes -- and a sentinel compared
   against a negative clock value discards every reading.  That is exactly what
   happened: the walk's buckets read 0.00 and the whole cost landed in the
   accounting error on the first boot after the uptime crossed 35.8 minutes,
   while the fence bucket (which subtracts two readings and never tests a sign)
   kept working.  The differences stay int-sized, so the engine's own
   `BE_Microseconds() - t0` idiom is untouched; only the sentinel moves to a
   clock that cannot collide with it. */
#define VK_WALK_T0()        ( VK_WalkStatOn() ? BE_Microseconds64() : -1LL )
#define VK_WALK_ADD(t0, ph) do { if ( (t0) >= 0 ) { VK_WalkStat_Add( (ph), BE_Microseconds64() - (t0) ); } } while ( 0 )

/* An outer wrap that contains an inner bucket charges only what the inner
   buckets did not: GEN and UI reach the stage iterator, whose bind/record/pack
   are charged at their own sites.  A leaf wrap keeps the plain form. */
#define VK_WALK_NEST0() ( VK_WalkStatOn() ? VK_WalkStat_Nested() : 0LL )
#define VK_WALK_ADD_NET(t0, n0, ph) do { if ( (t0) >= 0 ) { \
	long long vk_net = VK_WalkStat_NestedSince( n0 ); \
	long long vk_usec = BE_Microseconds64() - (t0) - vk_net; \
	VK_WalkStat_Add( (ph), vk_usec > 0 ? vk_usec : 0 ); } } while ( 0 )

/* The shade window's parts; the residual is OTHER, so they sum to it. */
enum {
	VK_WS_DECOMPOSE = 0, VK_WS_SURFACE, VK_WS_ROTATE, VK_WS_MODELVIEW,
	VK_WS_STATICDLIGHTS, VK_WS_SPHERE, VK_WS_OTHER, VK_WS_PARTS
};
void VK_WalkStat_ShadeAdd( int part, long long usec );
void VK_WalkStat_TakeShade( long long *usecOut );

void VK_PerfGpu_FrameBegin( int slot );
void VK_PerfGpu_Mark( int slot, int bucket );
void VK_PerfGpu_FrameEnd( int slot );
void VK_PerfGpu_Collect( int slot );
void VK_PerfGpu_Take( long long *usecOut, long long *marksOut, long long *hitCapOut );

void VKimp_WalkBegin( int slot );
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
VkResult VK_Queue_Submit( VkQueue queue, uint32_t submitCount,
						  const VkSubmitInfo *submits, VkFence fence );
VkResult VK_Queue_Present( VkQueue queue, const VkPresentInfoKHR *present );
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

/* The master instrument gate (qa_probe, default 0): every row this lane emits
   beyond the startup banner answers to it.  Declared here because the pool
   ownership rows live in vk_renderthread.c, which does not pull tr_local.h. */
qboolean R_ProbeStatsOn( void );

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
