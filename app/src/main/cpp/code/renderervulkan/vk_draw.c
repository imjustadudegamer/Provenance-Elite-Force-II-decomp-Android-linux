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
 * vk_draw.c -- the pipeline and descriptor substrate the back end draws with.
 *
 * The Vulkan half of the reference renderer's tr_program.c + VK_ApplyStateBits: a
 * shader stage's state tuple (the state bits, the topology, the cull) becomes
 * a VkPipeline, built against the pass it binds in and cached on the tuple.
 * Textures bind through the compatibility descriptor path (SDK doc 7); the
 * bindless path A is sized and selected at bring-up for the 3D iterators.
 */

#include "tr_local.h"
#include "vk_shaders_embedded.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
	VkShaderModule	vertex;
	VkShaderModule	fragment;
	VkPipelineLayout layout;
	VkDescriptorSetLayout setLayout;
} vkDrawShaders_t;

static vkDrawShaders_t	vk_draw;

/* ---- the pipeline cache: a pipeline per state tuple, kept for the module's
 * life; the disk cache warms the builds */
#define VK_PIPELINE_SLOTS 128

typedef struct {
	vkPipelineKey_t	key;
	VkPipeline		pipeline;
	qboolean		used;
} vkPipelineSlot_t;

static vkPipelineSlot_t	vk_pipelines[VK_PIPELINE_SLOTS];
static int				vk_numPipelines;

/* Both caches below are looked up by value and were searched by walking the
   entries they had filled, so one 2D draw cost O(fill) and the menu's per-frame
   cost grew with the boot.  The indexes store the entry index plus one (0 is
   empty) and insert with the same probe the lookup starts at, so an inserted
   key is always found. */
#define VK_DRAW_PIPELINE_HASH 256
#define VK_DRAW_SET_HASH      ( VK_DRAW_SET_SLOTS * 2 )

static int				s_pipelineIndex[VK_DRAW_PIPELINE_HASH];
static int				s_setIndex[VK_DRAW_SET_HASH];
static long long		s_buildT0;

/* the ring's index buffer, per frame slot: it is bound once per command buffer */
static VkBuffer			s_indexBound[VK_FRAMES_IN_FLIGHT];
static int				s_indexBoundFrame[VK_FRAMES_IN_FLIGHT];
/* the ring's vertex binding, same key: the 2D path repeats (buffer, 0) */
static VkBuffer			s_vertexBound[VK_FRAMES_IN_FLIGHT];
static int				s_vertexBoundFrame[VK_FRAMES_IN_FLIGHT];
static VkDeviceSize		s_vertexBoundOffset[VK_FRAMES_IN_FLIGHT];

/* the pipeline and the set bound at set 0 are command-buffer state, so binding values already in force is a no-op the driver still charges for
   (the menu's frame is 145 to 331 draws and most repeats follow one another).  Every bind goes through these two functions -- the 2D lane binds
   its own layout at set 0, so a shade-only cache would serve a stale pipeline to a 3D draw that followed a 2D one.  Keyed on the frame as well as the slot. */
static VkCommandBuffer	s_boundPipeCmd[VK_FRAMES_IN_FLIGHT];
static VkCommandBuffer	s_boundSetCmd[VK_FRAMES_IN_FLIGHT];
static VkPipeline		s_boundPipeline[VK_FRAMES_IN_FLIGHT];
static int				s_boundPipelineFrame[VK_FRAMES_IN_FLIGHT];
static VkPipelineLayout	s_boundSetLayout[VK_FRAMES_IN_FLIGHT];
static VkDescriptorSet	s_boundSet[VK_FRAMES_IN_FLIGHT];
static uint32_t			s_boundSetDynCount[VK_FRAMES_IN_FLIGHT];
static uint32_t			s_boundSetOffset[VK_FRAMES_IN_FLIGHT];
static int				s_boundSetFrame[VK_FRAMES_IN_FLIGHT];

static int VK_Draw_BindSlot( void )
{
	int slot = RB_FrameSmpFrame();

	return ( slot >= 0 && slot < VK_FRAMES_IN_FLIGHT ) ? slot : 0;
}

void VK_Draw_BindPipeline( VkCommandBuffer cmd, VkPipeline pipeline )
{
	int slot = VK_Draw_BindSlot();
	int frame = RB_FrameCount();

	VK_WalkStat_Req( VK_ELIDE_PIPE );
	/* The command buffer is part of the key: a frame slot maps to one buffer for
	   the walk, but the bring-up paths record into their own, and state does not
	   carry between buffers. */
	if ( s_boundPipelineFrame[slot] == frame && s_boundPipeCmd[slot] == cmd
		 && s_boundPipeline[slot] == pipeline ) {
		VK_WalkStat_Count( VK_ELIDE_PIPE );
		return;
	}
	qvk.CmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
	s_boundPipeCmd[slot] = cmd;
	s_boundPipeline[slot] = pipeline;
	s_boundPipelineFrame[slot] = frame;
}

/* `dynamicCount' must equal the DYNAMIC descriptors the BOUND SET's layout declares (VUID-...-00359): the shade layout declares its uniform block
   UNIFORM_BUFFER_DYNAMIC, the 2D/immediate layout nothing dynamic at all.  Passing 1 for the 2D layout was caught 105,618 times by validation. */
void VK_Draw_BindSet( VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet set,
					  uint32_t dynamicOffset, uint32_t dynamicCount )
{
	int slot = VK_Draw_BindSlot();
	int frame = RB_FrameCount();

	VK_WalkStat_Req( VK_ELIDE_SET );
	if ( s_boundSetFrame[slot] == frame && s_boundSetCmd[slot] == cmd
		 && s_boundSetLayout[slot] == layout
		 && s_boundSet[slot] == set && s_boundSetOffset[slot] == dynamicOffset
		 && s_boundSetDynCount[slot] == dynamicCount ) {
		VK_WalkStat_Count( VK_ELIDE_SET );
		return;
	}
	if ( dynamicCount > 1 ) {
		dynamicCount = 1;		/* one binding, one dynamic descriptor, one offset */
	}
	qvk.CmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set,
							   dynamicCount, &dynamicOffset );
	s_boundSetCmd[slot] = cmd;
	s_boundSetLayout[slot] = layout;
	s_boundSet[slot] = set;
	s_boundSetOffset[slot] = dynamicOffset;
	s_boundSetDynCount[slot] = dynamicCount;
	s_boundSetFrame[slot] = frame;
}

/* ---- what the last 2D state application and bind actually set ------------- */

/* The 2D state the walk applies is dynamic state that a batch replays per span,
   so "the state a draw ran under" is not readable from the batch any other way.
   Each setter records itself, and the census reads the record beside the draw. */
static int	s_2dLastScissor[4];
static qboolean s_2dLastScissorValid;
static int	s_2dAppliedWin[4];
static int	s_2dAppliedScn[4];
static qboolean s_2dAppliedValid;
static int	s_2dLastSetIndex = -1;

/* The 2D state the command buffer is known to hold, so a span replay can skip
   what matches.  Keyed on the frame; a 3D view overwrites the viewport and
   scissor and invalidates it. */
static float	s_2dIssuedOrtho[VK_FRAMES_IN_FLIGHT][16];
static int		s_2dIssuedWin[VK_FRAMES_IN_FLIGHT][4];
static int		s_2dIssuedScn[VK_FRAMES_IN_FLIGHT][4];
static int		s_2dIssuedFrame[VK_FRAMES_IN_FLIGHT];
static qboolean	s_2dIssuedValid[VK_FRAMES_IN_FLIGHT];
static cvar_t  *s_2dStateElide;

static qboolean VK_Draw_2DElideOn( void )
{
	if ( !s_2dStateElide ) {
		s_2dStateElide = ri.Cvar_Get( "vk_2dStateElide", "1", CVAR_ARCHIVE );
	}
	return ( s_2dStateElide && s_2dStateElide->integer ) ? qtrue : qfalse;
}

static int VK_Draw_IssuedSlot( int *frameOut )
{
	int	slot = RB_FrameSmpFrame();

	if ( slot < 0 || slot >= VK_FRAMES_IN_FLIGHT ) {
		slot = 0;
	}
	*frameOut = RB_FrameCount();

	return slot;
}

static void VK_Draw_IssueRecord( int slot, int frame, const float *ortho, const int *win, const int *scn )
{
	s_2dIssuedFrame[slot] = frame;
	s_2dIssuedValid[slot] = qtrue;
	if ( ortho ) {
		memcpy( s_2dIssuedOrtho[slot], ortho, 16 * sizeof( float ) );
	}
	if ( win ) {
		memcpy( s_2dIssuedWin[slot], win, sizeof( s_2dIssuedWin[slot] ) );
	}
	if ( scn ) {
		memcpy( s_2dIssuedScn[slot], scn, sizeof( s_2dIssuedScn[slot] ) );
	}
}

static void VK_Draw_IssueInvalidate( void )
{
	int	slot, frame;

	slot = VK_Draw_IssuedSlot( &frame );
	if ( s_2dIssuedFrame[slot] == frame ) {
		s_2dIssuedValid[slot] = qfalse;
	}
}

static unsigned VK_Draw_PipelineHash( const vkPipelineKey_t *key )
{
	unsigned h = (unsigned)key->topology * 2654435761u;

	h ^= key->stateBits * 2246822519u;
	h ^= (unsigned)key->cull * 3266489917u;
	h ^= (unsigned)key->depthTest * 668265263u;
	h ^= (unsigned)key->depthWrite * 374761393u;
	h ^= (unsigned)key->textured * 2654435761u;

	return h & ( VK_DRAW_PIPELINE_HASH - 1u );
}

static unsigned VK_Draw_SetHash( VkImageView view, VkSampler sampler )
{
	uint64_t	h = (uint64_t)(uintptr_t)view * 0x9E3779B97F4A7C15ULL;

	h ^= (uint64_t)(uintptr_t)sampler + 0x9E3779B97F4A7C15ULL + ( h << 6 ) + ( h >> 2 );

	return (unsigned)( ( h ^ ( h >> 32 ) ) & ( VK_DRAW_SET_HASH - 1u ) );
}

/* ---- the descriptor sets: one per (view, sampler) pair, cached; the menu
 * rebinds the same art every frame */
#define VK_SET_SLOTS VK_DRAW_SET_SLOTS

typedef struct {
	VkImageView		view;
	VkSampler		sampler;
	VkDescriptorSet	set;
	qboolean		used;
} vkSetSlot_t;

static vkSetSlot_t	vk_sets[VK_SET_SLOTS];
static int			vk_numSets;

typedef struct {
	float	ortho[16];		/* offset 0, the vertex stage */
	float	alphaTest[4];	/* offset 64: x = the bound (0 = off), y = 1 when 'less than' */
} vkPushConstants_t;

static vkPushConstants_t	vk_current;
static qboolean				vk_currentValid;

/* the rectangle the reference clamps and this lane has to; defined below the
   draw state they read, called from the 2D window and the 3D view */
static void VK_Draw_ScissorRect( VkCommandBuffer cmd, int x, int y, int w, int h );
void VK_Draw_ViewportRect( VkCommandBuffer cmd, int x, int y, int w, int h,
						   float minDepth, float maxDepth );
void VK_Draw_Window2D( VkCommandBuffer cmd, int x, int y, int w, int h );
void VK_Draw_View3D( VkCommandBuffer cmd, int x, int y, int w, int h );

static qboolean VK_Draw_Module( const unsigned int *code, unsigned int words,
								VkShaderModule *module, const char *what )
{
	VkShaderModuleCreateInfo createInfo;

	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = (size_t)words * sizeof( unsigned int );
	createInfo.pCode = code;

	return VK_Guard_Result( VK_GSITE_DESCRIPTOR, qvk.CreateShaderModule( vk.device, &createInfo, NULL, module ),
							what );
}

static qboolean VK_Draw_CreateLayout( void )
{
	VkDescriptorSetLayoutBinding binding;
	VkDescriptorSetLayoutCreateInfo setInfo;
	VkPushConstantRange pushRange;
	VkPipelineLayoutCreateInfo layoutInfo;

	memset( &binding, 0, sizeof( binding ) );
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	memset( &setInfo, 0, sizeof( setInfo ) );
	setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setInfo.bindingCount = 1;
	setInfo.pBindings = &binding;

	if ( !VK_Guard_Result( VK_GSITE_DESCRIPTOR,
						   qvk.CreateDescriptorSetLayout( vk.device, &setInfo, NULL, &vk_draw.setLayout ),
						   "vkCreateDescriptorSetLayout(draw)" ) ) {
		return qfalse;
	}

	memset( &pushRange, 0, sizeof( pushRange ) );
	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushRange.offset = 0;
	pushRange.size = sizeof( vkPushConstants_t );

	memset( &layoutInfo, 0, sizeof( layoutInfo ) );
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &vk_draw.setLayout;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushRange;

	if ( !VK_Guard_Result( VK_GSITE_DESCRIPTOR,
						   qvk.CreatePipelineLayout( vk.device, &layoutInfo, NULL, &vk_draw.layout ),
						   "vkCreatePipelineLayout(draw)" ) ) {
		return qfalse;
	}

	return qtrue;
}

static void VK_Draw_BlendForState( unsigned stateBits, VkPipelineColorBlendAttachmentState *blend )
{
	VkBlendFactor src = VK_BLEND_FACTOR_ONE;
	VkBlendFactor dst = VK_BLEND_FACTOR_ZERO;

	memset( blend, 0, sizeof( *blend ) );

	switch ( stateBits & VKS_SRCBLEND_BITS ) {
	case VKS_SRCBLEND_ZERO:						src = VK_BLEND_FACTOR_ZERO; break;
	case VKS_SRCBLEND_ONE:						src = VK_BLEND_FACTOR_ONE; break;
	case VKS_SRCBLEND_DST_COLOR:				src = VK_BLEND_FACTOR_DST_COLOR; break;
	case VKS_SRCBLEND_ONE_MINUS_DST_COLOR:		src = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR; break;
	case VKS_SRCBLEND_SRC_ALPHA:				src = VK_BLEND_FACTOR_SRC_ALPHA; break;
	case VKS_SRCBLEND_ONE_MINUS_SRC_ALPHA:		src = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; break;
	case VKS_SRCBLEND_DST_ALPHA:				src = VK_BLEND_FACTOR_DST_ALPHA; break;
	case VKS_SRCBLEND_ONE_MINUS_DST_ALPHA:		src = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA; break;
	case VKS_SRCBLEND_ALPHA_SATURATE:			src = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE; break;
	default:									src = VK_BLEND_FACTOR_ONE; break;
	}

	switch ( stateBits & VKS_DSTBLEND_BITS ) {
	case VKS_DSTBLEND_ZERO:						dst = VK_BLEND_FACTOR_ZERO; break;
	case VKS_DSTBLEND_ONE:						dst = VK_BLEND_FACTOR_ONE; break;
	case VKS_DSTBLEND_SRC_COLOR:				dst = VK_BLEND_FACTOR_SRC_COLOR; break;
	case VKS_DSTBLEND_ONE_MINUS_SRC_COLOR:		dst = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR; break;
	case VKS_DSTBLEND_SRC_ALPHA:				dst = VK_BLEND_FACTOR_SRC_ALPHA; break;
	case VKS_DSTBLEND_ONE_MINUS_SRC_ALPHA:		dst = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; break;
	case VKS_DSTBLEND_DST_ALPHA:				dst = VK_BLEND_FACTOR_DST_ALPHA; break;
	case VKS_DSTBLEND_ONE_MINUS_DST_ALPHA:		dst = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA; break;
	default:									dst = VK_BLEND_FACTOR_ZERO; break;
	}

	blend->blendEnable = ( src != VK_BLEND_FACTOR_ONE || dst != VK_BLEND_FACTOR_ZERO ) ? VK_TRUE : VK_FALSE;
	blend->srcColorBlendFactor = src;
	blend->dstColorBlendFactor = dst;
	blend->colorBlendOp = VK_BLEND_OP_ADD;
	blend->srcAlphaBlendFactor = src;
	blend->dstAlphaBlendFactor = dst;
	blend->alphaBlendOp = VK_BLEND_OP_ADD;
	blend->colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
							| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
}

static VkPrimitiveTopology VK_Draw_Topology( int topology )
{
	switch ( topology ) {
	case VK_IMM_LINES:				return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	case VK_IMM_LINE_LOOP:			return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
	case VK_IMM_TRIANGLE_STRIP:		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	case VK_IMM_TRIANGLES:
	default:						return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	}
}

static qboolean VK_Draw_Build( const vkPipelineKey_t *key, VkPipeline *out )
{
	VkPipelineShaderStageCreateInfo stages[2];
	VkPipelineVertexInputStateCreateInfo vertexInput;
	VkVertexInputBindingDescription binding;
	VkVertexInputAttributeDescription attributes[3];
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineViewportStateCreateInfo viewportState;
	VkPipelineRasterizationStateCreateInfo rasterization;
	VkPipelineMultisampleStateCreateInfo multisample;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkPipelineColorBlendAttachmentState blendAttachment;
	VkPipelineColorBlendStateCreateInfo colourBlend;
	VkDynamicState dynamicStates[3];
	VkPipelineDynamicStateCreateInfo dynamicState;
	VkGraphicsPipelineCreateInfo pipelineInfo;
	VkResult result;
	uint32_t attribCount;

	memset( &binding, 0, sizeof( binding ) );
	binding.binding = 0;
	binding.stride = VK_TESS_VERTEX_BYTES;
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	memset( &attributes, 0, sizeof( attributes ) );
	attributes[0].location = 0;
	attributes[0].binding = 0;
	attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributes[0].offset = offsetof( vkTessVertex_t, xyz );
	attributes[1].location = 1;
	attributes[1].binding = 0;
	attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
	attributes[1].offset = offsetof( vkTessVertex_t, st );
	attributes[2].location = 2;
	attributes[2].binding = 0;
	attributes[2].format = VK_FORMAT_R8G8B8A8_UNORM;
	attributes[2].offset = offsetof( vkTessVertex_t, color );

	/* Only what the tess vertex stage reads; the stride is the ring's. */
	attribCount = VK_Pipeline_TrimAttributes( attributes, 3,
					VK_Pipeline_VertexInputMask( vk_tess_vert_spv, VK_TESS_VERT_SPV_WORDS ) );

	memset( &vertexInput, 0, sizeof( vertexInput ) );
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.vertexBindingDescriptionCount = 1;
	vertexInput.pVertexBindingDescriptions = &binding;
	vertexInput.vertexAttributeDescriptionCount = attribCount;
	vertexInput.pVertexAttributeDescriptions = attributes;

	memset( &inputAssembly, 0, sizeof( inputAssembly ) );
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_Draw_Topology( key->topology );

	memset( &dynamicStates, 0, sizeof( dynamicStates ) );
	dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

	memset( &dynamicState, 0, sizeof( dynamicState ) );
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	memset( &viewportState, 0, sizeof( viewportState ) );
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	memset( &rasterization, 0, sizeof( rasterization ) );
	rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization.cullMode = ( key->cull == CT_FRONT_SIDED ) ? VK_CULL_MODE_BACK_BIT
						   : ( key->cull == CT_BACK_SIDED ) ? VK_CULL_MODE_FRONT_BIT
						   : VK_CULL_MODE_NONE;
	rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterization.lineWidth = 1.0f;

	memset( &multisample, 0, sizeof( multisample ) );
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	memset( &depthStencil, 0, sizeof( depthStencil ) );
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = key->depthTest ? VK_TRUE : VK_FALSE;
	depthStencil.depthWriteEnable = ( key->depthWrite && key->depthTest ) ? VK_TRUE : VK_FALSE;
	depthStencil.depthCompareOp = ( key->stateBits & VKS_DEPTHFUNC_EQUAL ) ? VK_COMPARE_OP_EQUAL
																: VK_COMPARE_OP_LESS_OR_EQUAL;

	VK_Draw_BlendForState( key->stateBits, &blendAttachment );

	memset( &colourBlend, 0, sizeof( colourBlend ) );
	colourBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colourBlend.attachmentCount = 1;
	colourBlend.pAttachments = &blendAttachment;

	memset( &stages, 0, sizeof( stages ) );
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vk_draw.vertex;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = vk_draw.fragment;
	stages[1].pName = "main";

	memset( &pipelineInfo, 0, sizeof( pipelineInfo ) );
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = stages;
	pipelineInfo.pVertexInputState = &vertexInput;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterization;
	pipelineInfo.pMultisampleState = &multisample;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colourBlend;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = vk_draw.layout;
	pipelineInfo.renderPass = vk.renderPass;
	pipelineInfo.subpass = 0;

	result = qvk.CreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &pipelineInfo, NULL, out );

	return VK_Guard_Result( VK_GSITE_DESCRIPTOR, result, "vkCreateGraphicsPipelines(draw)" );
}

qboolean VK_Draw_Init( void )
{
	int i;

	memset( &vk_draw, 0, sizeof( vk_draw ) );
	memset( vk_pipelines, 0, sizeof( vk_pipelines ) );
	vk_numPipelines = 0;
	memset( vk_sets, 0, sizeof( vk_sets ) );
	vk_numSets = 0;
	memset( s_pipelineIndex, 0, sizeof( s_pipelineIndex ) );
	memset( s_setIndex, 0, sizeof( s_setIndex ) );
	memset( s_indexBound, 0, sizeof( s_indexBound ) );
	memset( s_indexBoundFrame, 0, sizeof( s_indexBoundFrame ) );
	memset( s_boundPipeCmd, 0, sizeof( s_boundPipeCmd ) );
	memset( s_boundSetCmd, 0, sizeof( s_boundSetCmd ) );
	memset( s_boundPipeline, 0, sizeof( s_boundPipeline ) );
	memset( s_boundPipelineFrame, 0, sizeof( s_boundPipelineFrame ) );
	memset( s_boundSetLayout, 0, sizeof( s_boundSetLayout ) );
	memset( s_boundSet, 0, sizeof( s_boundSet ) );
	memset( s_boundSetOffset, 0, sizeof( s_boundSetOffset ) );
	memset( s_boundSetFrame, 0, sizeof( s_boundSetFrame ) );
	memset( &vk_current, 0, sizeof( vk_current ) );
	vk_currentValid = qfalse;

	if ( !VK_Draw_Module( vk_tess_vert_spv, VK_TESS_VERT_SPV_WORDS, &vk_draw.vertex,
						  "vkCreateShaderModule(tess vertex)" ) ) {
		return qfalse;
	}

	if ( !VK_Draw_Module( vk_tess_frag_spv, VK_TESS_FRAG_SPV_WORDS, &vk_draw.fragment,
						  "vkCreateShaderModule(tess fragment)" ) ) {
		return qfalse;
	}

	if ( !VK_Draw_CreateLayout() ) {
		return qfalse;
	}

	for ( i = 0; i < VK_SET_SLOTS; i++ ) {
		vk_sets[i].set = VK_NULL_HANDLE;
	}

	if ( R_ProbeStatsOn() ) {
		VK_Guard_Row( "VK:draw", "ready pipelines=%d sets=%d vertex-bytes=%d",
					  VK_PIPELINE_SLOTS, VK_SET_SLOTS, VK_TESS_VERTEX_BYTES );
	}

	return qtrue;
}

void VK_Draw_Shutdown( void )
{
	int i;

	if ( vk.device == VK_NULL_HANDLE ) {
		return;
	}

	for ( i = 0; i < VK_PIPELINE_SLOTS; i++ ) {
		if ( vk_pipelines[i].used && vk_pipelines[i].pipeline != VK_NULL_HANDLE && qvk.DestroyPipeline ) {
			qvk.DestroyPipeline( vk.device, vk_pipelines[i].pipeline, NULL );
		}
		vk_pipelines[i].pipeline = VK_NULL_HANDLE;
		vk_pipelines[i].used = qfalse;
	}
	vk_numPipelines = 0;
	memset( s_pipelineIndex, 0, sizeof( s_pipelineIndex ) );

	if ( vk_draw.layout != VK_NULL_HANDLE && qvk.DestroyPipelineLayout ) {
		qvk.DestroyPipelineLayout( vk.device, vk_draw.layout, NULL );
	}
	vk_draw.layout = VK_NULL_HANDLE;

	if ( vk_draw.setLayout != VK_NULL_HANDLE && qvk.DestroyDescriptorSetLayout ) {
		qvk.DestroyDescriptorSetLayout( vk.device, vk_draw.setLayout, NULL );
	}
	vk_draw.setLayout = VK_NULL_HANDLE;

	if ( vk_draw.fragment != VK_NULL_HANDLE && qvk.DestroyShaderModule ) {
		qvk.DestroyShaderModule( vk.device, vk_draw.fragment, NULL );
	}
	vk_draw.fragment = VK_NULL_HANDLE;

	if ( vk_draw.vertex != VK_NULL_HANDLE && qvk.DestroyShaderModule ) {
		qvk.DestroyShaderModule( vk.device, vk_draw.vertex, NULL );
	}
	vk_draw.vertex = VK_NULL_HANDLE;
}

VkPipeline VK_Draw_Pipeline( const vkPipelineKey_t *key )
{
	unsigned	h;
	int			i, probe;

	if ( !key ) {
		return VK_NULL_HANDLE;
	}
	h = VK_Draw_PipelineHash( key );

	for ( probe = 0; probe < VK_DRAW_PIPELINE_HASH; probe++ ) {
		int at = s_pipelineIndex[( h + probe ) & ( VK_DRAW_PIPELINE_HASH - 1u )];

		if ( !at ) {
			break;
		}
		at--;
		if ( vk_pipelines[at].used
			 && memcmp( &vk_pipelines[at].key, key, sizeof( *key ) ) == 0 ) {
			return vk_pipelines[at].pipeline;
		}
	}

	for ( i = 0; i < VK_PIPELINE_SLOTS; i++ ) {
		if ( !vk_pipelines[i].used ) {
			break;
		}
	}

	if ( i >= VK_PIPELINE_SLOTS ) {
		VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "pipeline table full", VK_ERROR_TOO_MANY_OBJECTS );
		return VK_NULL_HANDLE;
	}

	s_buildT0 = BE_Microseconds64();
	if ( !VK_Draw_Build( key, &vk_pipelines[i].pipeline ) ) {
		return VK_NULL_HANDLE;
	}

	VK_Shade_WarmNoteBuild( tess.shader ? tess.shader->name : "(2d)",
							key->stateBits, BE_Microseconds64() - s_buildT0 );

	vk_pipelines[i].key = *key;
	vk_pipelines[i].used = qtrue;
	if ( i == vk_numPipelines ) {
		vk_numPipelines = i + 1;
	}

	for ( probe = 0; probe < VK_DRAW_PIPELINE_HASH; probe++ ) {
		int *at = &s_pipelineIndex[( h + probe ) & ( VK_DRAW_PIPELINE_HASH - 1u )];

		if ( !*at ) {
			*at = i + 1;
			break;
		}
	}

	return vk_pipelines[i].pipeline;
}

qboolean VK_Draw_BindTexture( VkCommandBuffer cmd, VkImageView view, VkSampler sampler )
{
	VkDescriptorSetAllocateInfo allocInfo;
	VkDescriptorImageInfo imageInfo;
	VkWriteDescriptorSet write;
	VkDescriptorSet set = VK_NULL_HANDLE;
	unsigned	h;
	int i, probe;

	if ( view == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE ) {
		VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "bind of an empty view or sampler", VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	h = VK_Draw_SetHash( view, sampler );

	for ( probe = 0; probe < VK_DRAW_SET_HASH; probe++ ) {
		int at = s_setIndex[( h + probe ) & ( VK_DRAW_SET_HASH - 1u )];

		if ( !at ) {
			break;
		}
		at--;
		if ( vk_sets[at].used && vk_sets[at].view == view && vk_sets[at].sampler == sampler ) {
			set = vk_sets[at].set;
			i = at;		/* the cache-hit path skips the slot scan below: the row reads the real index */
			break;
		}
	}

	if ( set == VK_NULL_HANDLE ) {
		for ( i = 0; i < VK_SET_SLOTS; i++ ) {
			if ( !vk_sets[i].used ) {
				break;
			}
		}

		if ( i >= VK_SET_SLOTS ) {
			VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "descriptor set table full", VK_ERROR_TOO_MANY_OBJECTS );
			return qfalse;
		}

		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = vk.descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &vk_draw.setLayout;

		if ( !VK_Guard_Result( VK_GSITE_DESCRIPTOR,
							   qvk.AllocateDescriptorSets( vk.device, &allocInfo, &vk_sets[i].set ),
							   "vkAllocateDescriptorSets" ) ) {
			return qfalse;
		}

		memset( &imageInfo, 0, sizeof( imageInfo ) );
		imageInfo.sampler = sampler;
		imageInfo.imageView = view;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		memset( &write, 0, sizeof( write ) );
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = vk_sets[i].set;
		write.dstBinding = 0;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.pImageInfo = &imageInfo;

		qvk.UpdateDescriptorSets( vk.device, 1, &write, 0, NULL );

		vk_sets[i].view = view;
		vk_sets[i].sampler = sampler;
		vk_sets[i].used = qtrue;

		if ( i == vk_numSets ) {
			vk_numSets = i + 1;
		}

		for ( probe = 0; probe < VK_DRAW_SET_HASH; probe++ ) {
			int *at = &s_setIndex[( h + probe ) & ( VK_DRAW_SET_HASH - 1u )];

			if ( !*at ) {
				*at = i + 1;
				break;
			}
		}

		set = vk_sets[i].set;
	}

	VK_Draw_BindSet( cmd, vk_draw.layout, set, 0, 0 );

	/* the set this quad samples through; i is vk_sets[]'s own index for the
	   (view,sampler) pair, so an aliased or stale pair is nameable in the row */
	s_2dLastSetIndex = i;

	return qtrue;
}

/* The 2D window in force, for the draw row.  A quad's rect is local to the
   window the UI sets around it, so the rect alone reads as a pile of quads all
   at the origin; the window is what puts the tile on its own screen rect. */
int	vk_2dWindow[4]   = { 0, 0, 0, 0 };
int	vk_2dScissor[4]  = { 0, 0, 0, 0 };

/* the alpha-test selector the last VK_Draw_SetAlphaTest published */
static int s_2dLastAlphaTest;

int VK_Draw_LastAlphaTest( void )
{
	return s_2dLastAlphaTest;
}

/* The widget's placement in the 2D window: the window defines the pass's whole
   space (retail's constant glViewport/glOrtho), so setting one drops the
   placement, and the widget records its own right after. */
float	vk_2dOrigin[2]   = { 0.0f, 0.0f };

/* A span replay re-issues state the run already had; it is not a state change.
   The mirrors above are what a span snapshot reads, so a replay that wrote them
   made the next snapshot read back the previously replayed span's scissor -- a
   widget drawn under another widget's clip, which is a whole LCARS tile gone
   while its neighbours drew. */
static qboolean vk_2dReplaying;

void VK_Draw_Set2DOrigin( float x, float y )
{
	if ( vk_2dReplaying ) {
		return;
	}

	vk_2dOrigin[0] = x;
	vk_2dOrigin[1] = y;
}

void VK_Draw_Set2D( VkCommandBuffer cmd, int x, int y, int w, int h,
					float left, float right, float bottom, float top, float n, float f )
{
	float m[16];
	float rl, tb, fn;

	if ( !vk_2dReplaying ) {
		vk_2dWindow[0] = x; vk_2dWindow[1] = y; vk_2dWindow[2] = w; vk_2dWindow[3] = h;
		vk_2dScissor[0] = x; vk_2dScissor[1] = y; vk_2dScissor[2] = w; vk_2dScissor[3] = h;

		/* a window defines the space; the widget that asked for it places itself */
		vk_2dOrigin[0] = 0.0f;
		vk_2dOrigin[1] = 0.0f;
	}

	rl = right - left;
	tb = top - bottom;
	fn = f - n;

	/* the 2D pass has ONE window for every widget -- the screen -- and a widget clips with the scissor, which is retail's own shape
	   (retail-menu-solomatch-final.trace 10103-10124: constant glViewport and glOrtho(0,640,480,0,-1,1), per-widget glScissor).  The placement is a
	   vertex origin now, so this ortho cannot degenerate; the guards keep a zero range from producing a non-finite matrix. */
	if ( rl == 0.0f ) { rl = 1.0f; }
	if ( tb == 0.0f ) { tb = 1.0f; }
	if ( fn == 0.0f ) { fn = 1.0f; }

	memset( m, 0, sizeof( m ) );
	m[0]  = 2.0f / rl;
	m[5]  = -2.0f / tb;
	m[10] = -1.0f / fn;
	m[12] = -( right + left ) / rl;
	m[13] = ( top + bottom ) / tb;
	m[14] = 0.5f - ( f + n ) / ( 2.0f * fn );
	m[15] = 1.0f;

	{
		vkPushConstants_t push;

		memset( &push, 0, sizeof( push ) );
		memcpy( push.ortho, m, sizeof( m ) );
		push.alphaTest[0] = 0.0f;	/* the alpha test is per stage, set by the back end */
		push.alphaTest[1] = 0.0f;

		qvk.CmdPushConstants( cmd, vk_draw.layout,
							  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
							  0, sizeof( push ), &push );

	/* The 2D window is in force until the frame ends: the surfaces the walk
	   performs inside it take the 2D walk. */
	backEnd.in2D = qtrue;
	}

	/* The reference's RB_2D_SetWindow takes the 2D depth gate here, after the
	   state block that turns depth off for the whole UI pass. */
	VK_DepthGate2DTake();

	memset( vk_current.ortho, 0, sizeof( vk_current.ortho ) );
	memcpy( vk_current.ortho, m, sizeof( m ) );
	vk_currentValid = qtrue;

	/* The client's window is bottom-left; the viewport is Vulkan's top-left.
	   Both go through the reference's clamp, because the window is the client's
	   and may hang off the screen it sits on. */
	VK_Draw_Window2D( cmd, x, y, w, h );

	{
		int	frame;
		int	slot = VK_Draw_IssuedSlot( &frame );
		int	win[4], scn[4];

		win[0] = x; win[1] = y; win[2] = w; win[3] = h;
		memcpy( scn, win, sizeof( scn ) );
		VK_Draw_IssueRecord( slot, frame, m, win, scn );
	}
}

/* The alpha test rides the stage's push constants; the value is the reference
   selector (0 off, 1 GT0, 2 LT80, 4 GE128), which no threshold expresses. */
void VK_Draw_SetAlphaTest( VkCommandBuffer cmd, unsigned stateBits )
{
	float values[4];

	memset( values, 0, sizeof( values ) );

	/* the selector rides a push constant and the fragment stage discards on it, so the value is command-buffer state that outlives the draw that
	   set it: returning early on "no alpha test" left the PREVIOUS stage's selector in force.  Zero is PUBLISHED, not skipped, exactly as the reference's glDisable(GL_ALPHA_TEST) does. */
	switch ( stateBits & VKS_ATEST_BITS ) {
	case VKS_ATEST_GT_0:	values[0] = 1.0f;			break;
	case VKS_ATEST_LT_80:	values[0] = 2.0f;			break;
	case VKS_ATEST_GE_80:	values[0] = 4.0f;			break;
	default:				values[0] = 0.0f;			break;
	}

	/* the selector this push leaves in the buffer: a draw that pushes none of
	   its own runs under it, so the row names what was actually tested */
	s_2dLastAlphaTest = (int)values[0];

	qvk.CmdPushConstants( cmd, vk_draw.layout,
						  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
						  offsetof( vkPushConstants_t, alphaTest ), sizeof( values ), values );
}

void VK_Draw_SetScissor( VkCommandBuffer cmd, int x, int y, int width, int height )
{
	if ( !vk_2dReplaying ) {
		vk_2dScissor[0] = x; vk_2dScissor[1] = y; vk_2dScissor[2] = width; vk_2dScissor[3] = height;
	}

	/* RE_Scissor's box is bottom-left; Vulkan's is the top-left, and the box is
	   cut to the render area the way GL cut it for free. */
	VK_Draw_ScissorRect( cmd, x, VK_Draw_TopY( y, height ), width, height );

	{
		int		frame;
		int		slot = VK_Draw_IssuedSlot( &frame );
		int		scn[4];

		scn[0] = x; scn[1] = y; scn[2] = width; scn[3] = height;
		VK_Draw_IssueRecord( slot, frame, NULL, NULL, scn );
	}
}

void VK_Draw_DisableScissor( VkCommandBuffer cmd )
{
	VkRect2D scissor;

	memset( &scissor, 0, sizeof( scissor ) );
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent = vk.swapchainExtent;

	qvk.CmdSetScissor( cmd, 0, 1, &scissor );
}

/* ---- the 2D state a span carries ----------------------------------------- */

/* the window command sets a viewport AND a scissor and a later RE_Scissor replaces the scissor alone, so the state in force is the window's rect
   and the scissor the last of the two left; the ortho is the push constant VK_Draw_Set2D wrote.  A batch that crosses a change keeps one span per
   state instead of ending the surface, re-applying the same three calls in the same order; qfalse when no window has been set, which cannot be spanned. */
qboolean VK_Draw_2DStateGet( float *ortho, int *win, int *scn )
{
	if ( !vk_currentValid ) {
		return qfalse;
	}

	memcpy( ortho, vk_current.ortho, 16 * sizeof( float ) );
	memcpy( win, vk_2dWindow, 4 * sizeof( int ) );
	memcpy( scn, vk_2dScissor, 4 * sizeof( int ) );
	return qtrue;
}

/* Whether the command buffer already holds scn as its scissor.  The record
   mirror vk_2dScissor is NOT this: a span replay writes the buffer while the
   replay guard holds the mirror still, so a writer that skips on the mirror
   draws under the replayed span's clip. */
qboolean VK_Draw_2DIssuedScissor( const int *scn )
{
	int	frame;
	int	slot = VK_Draw_IssuedSlot( &frame );

	if ( !VK_Draw_2DElideOn() || !s_2dIssuedValid[slot] || s_2dIssuedFrame[slot] != frame ) {
		return qfalse;
	}

	return memcmp( scn, s_2dIssuedScn[slot], sizeof( s_2dIssuedScn[slot] ) ) == 0 ? qtrue : qfalse;
}

void VK_Draw_2DStateApply( VkCommandBuffer cmd, const float *ortho, const int *win, const int *scn )
{
	int			frame;
	int			slot = VK_Draw_IssuedSlot( &frame );
	qboolean	known = VK_Draw_2DElideOn() && s_2dIssuedValid[slot] && s_2dIssuedFrame[slot] == frame;
	qboolean	orthoSame, winSame, scnSame;

	orthoSame = known && !memcmp( ortho, s_2dIssuedOrtho[slot], 16 * sizeof( float ) );
	winSame   = known && !memcmp( win, s_2dIssuedWin[slot], sizeof( s_2dIssuedWin[slot] ) );
	scnSame   = known && !memcmp( scn, s_2dIssuedScn[slot], sizeof( s_2dIssuedScn[slot] ) );

	/* The ortho alone: this runs after the stage pass set the alpha test, and
	   the window's own push zeroes that field.  The push range covers the whole
	   constant block, and ortho is its first member. */
	if ( !orthoSame ) {
		qvk.CmdPushConstants( cmd, vk_draw.layout,
							  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
							  offsetof( vkPushConstants_t, ortho ), 16 * sizeof( float ), ortho );
	}

	/* A replay restores a state the run already had, so it must not become the
	   state the next snapshot reads back: the mirrors stay those of the widget
	   the run is inside. */
	vk_2dReplaying = qtrue;

	if ( !winSame ) {
		VK_Draw_Window2D( cmd, win[0], win[1], win[2], win[3] );
	}

	/* scn == win does NOT mean the scissor is already in force: the buffer's scissor is the last one anybody issued, and a raw RE_Scissor from
	   another pass -- the tricorder's 256x64 screen -- updates the issued-scn mirror while the issued-window mirror stays at the screen window, so
	   the overlay drew under the other pass's clip.  A window that IS re-issued sets the scissor to the window, so a differing scn must be issued. */
	if ( scn[0] != win[0] || scn[1] != win[1] || scn[2] != win[2] || scn[3] != win[3] ) {
		if ( !winSame || !scnSame ) {
			VK_Draw_SetScissor( cmd, scn[0], scn[1], scn[2], scn[3] );
		}
	} else if ( winSame && !scnSame ) {
		VK_Draw_SetScissor( cmd, win[0], win[1], win[2], win[3] );
	}

	vk_2dReplaying = qfalse;

	{
		int	scnIssued[4];

		if ( scn[0] != win[0] || scn[1] != win[1] || scn[2] != win[2] || scn[3] != win[3] ) {
			memcpy( scnIssued, scn, sizeof( scnIssued ) );
		} else {
			memcpy( scnIssued, win, sizeof( scnIssued ) );
		}
		VK_Draw_IssueRecord( slot, frame, ortho, win, scnIssued );
	}

	/* The state this draw will run under, kept for the census. */
	memcpy( s_2dAppliedWin, win, sizeof( s_2dAppliedWin ) );
	memcpy( s_2dAppliedScn, scn, sizeof( s_2dAppliedScn ) );
	s_2dAppliedValid = qtrue;
}

/* ---- the rectangle the reference clamps and this lane has to ------------- */

/* The client's box is bottom-left and Vulkan's is top-left, and the client's
   origin hangs off the screen by more than an int names (the radar's box
   arrives at INT32_MIN), so the translation runs wide. */
int VK_Draw_TopY( int y, int h )
{
	long long top = (long long)vkConfig.vidHeight - (long long)y - (long long)h;

	if ( top > 2147483647LL ) {
		return 2147483647;
	}
	if ( top < -2147483648LL ) {
		return -2147483648;
	}
	return (int)top;
}

/* the reference hands the client's window straight to glScissor and GL clamps the box to the framebuffer for free; Vulkan does not clamp and a
   negative scissor offset is undefined rasterization (6,743 of them in one boot, every one a 2D window taller than the screen).  This is the
   reference's clamp written out: cut to the render area, an entirely-outside box becomes the empty extent, and the corners are held wide because the box's own origin overflows an int. */
static void VK_Draw_ScissorRect( VkCommandBuffer cmd, int x, int y, int w, int h )
{
	VkRect2D	 scissor;
	long long	fbw, fbh, bx, by, bw, bh;
	long long	ax0, ay0, ax1, ay1;

	fbw = vk.swapchainExtent.width ? (long long)vk.swapchainExtent.width : vkConfig.vidWidth;
	fbh = vk.swapchainExtent.height ? (long long)vk.swapchainExtent.height : vkConfig.vidHeight;
	bx = x; by = y; bw = w; bh = h;

	ax0 = bx > 0 ? bx : 0;
	ay0 = by > 0 ? by : 0;
	ax1 = bx + bw < fbw ? bx + bw : fbw;
	ay1 = by + bh < fbh ? by + bh : fbh;

	memset( &scissor, 0, sizeof( scissor ) );
	if ( ax1 > ax0 && ay1 > ay0 ) {
		scissor.offset.x = (int)ax0;
		scissor.offset.y = (int)ay0;
		scissor.extent.width = (uint32_t)( ax1 - ax0 );
		scissor.extent.height = (uint32_t)( ay1 - ay0 );
	}

	/* What Vulkan is actually told, kept for the coverage census: the census may
	   report the rect in force, and a rect it recomputed itself could disagree
	   with the one the draw ran under.  VkRect2D's offset must be >= 0
	   (VUID-vkCmdSetScissor-x-00595, vkspec.txt:202864) and offset+extent must
	   not overflow (00596/00597, :202868), which is why the clamp exists. */
	s_2dLastScissor[0] = scissor.offset.x;
	s_2dLastScissor[1] = scissor.offset.y;
	s_2dLastScissor[2] = (int)scissor.extent.width;
	s_2dLastScissor[3] = (int)scissor.extent.height;
	s_2dLastScissorValid = qtrue;

	qvk.CmdSetScissor( cmd, 0, 1, &scissor );
}

void VK_Draw_LastScissorRect( int out[4] )
{
	if ( !out ) {
		return;
	}
	memcpy( out, s_2dLastScissor, sizeof( s_2dLastScissor ) );
}

qboolean VK_Draw_LastScissorValid( void )
{
	return s_2dLastScissorValid;
}

/* The window and scissor the last span re-application restored.  The census
   compares this against the span table's own copy: a draw whose applied rect is
   not the rect it was authored under is a draw under a foreign clip, which is
   the defect class this instrument exists to name. */
qboolean VK_Draw_2DAppliedGet( int *win, int *scn )
{
	if ( !s_2dAppliedValid ) {
		return qfalse;
	}
	if ( win ) {
		memcpy( win, s_2dAppliedWin, sizeof( s_2dAppliedWin ) );
	}
	if ( scn ) {
		memcpy( scn, s_2dAppliedScn, sizeof( s_2dAppliedScn ) );
	}
	return qtrue;
}

/* The descriptor set the last texture bind selected: the census names the set a
   quad sampled through, so a stale or aliased set is visible in the row. */
int VK_Draw_LastSetIndex( void )
{
	return s_2dLastSetIndex;
}

/* the viewport has no framebuffer to be cut to but it does have a device range, and a coordinate outside it is undefined too; every
   CmdSetViewport in this lane goes through here, so no site can invent an out-of-range origin. */
/* the range the lane last issued through VK_Draw_ViewportRect: the decal row reads it, so a draw under the viewmodel's 0.3 names itself */
static float	s_depthRangeLast[2] = { 0.0f, 1.0f };

void VK_Draw_DepthRangeLast( float *n, float *f )
{
	*n = s_depthRangeLast[0];
	*f = s_depthRangeLast[1];
}

void VK_Draw_ViewportRect( VkCommandBuffer cmd, int x, int y, int w, int h,
						   float minDepth, float maxDepth )
{
	VkViewport viewport;
	float		lo, hi;

	memset( &viewport, 0, sizeof( viewport ) );
	viewport.x = (float)x;
	viewport.y = (float)y;
	viewport.width = (float)( w > 0 ? w : 1 );
	viewport.height = (float)( h > 0 ? h : 1 );
	viewport.minDepth = minDepth;
	viewport.maxDepth = maxDepth;

	/* the single CmdSetViewport choke point, so the last issued range is the device's own */
	s_depthRangeLast[0] = minDepth;
	s_depthRangeLast[1] = maxDepth;

	lo = vk.deviceProps.limits.viewportBoundsRange[0];
	hi = vk.deviceProps.limits.viewportBoundsRange[1];

	if ( viewport.x < lo ) viewport.x = lo;
	if ( viewport.y < lo ) viewport.y = lo;
	if ( viewport.x > hi ) viewport.x = hi;
	if ( viewport.y > hi ) viewport.y = hi;
	if ( viewport.x + viewport.width  > hi ) viewport.width  = hi - viewport.x;
	if ( viewport.y + viewport.height > hi ) viewport.height = hi - viewport.y;
	if ( viewport.width  < 0.0f ) viewport.width  = 0.0f;
	if ( viewport.height < 0.0f ) viewport.height = 0.0f;

	qvk.CmdSetViewport( cmd, 0, 1, &viewport );
}

/* Both take Vulkan's own space: x and y from the top-left, w and h down and
   right.  The client's bottom-left window is translated by the caller, once,
   so the viewport and the scissor cannot disagree about where the rect is. */
void VK_Draw_Window2D( VkCommandBuffer cmd, int x, int y, int w, int h )
{
	int top = VK_Draw_TopY( y, h );

	VK_Draw_ViewportRect( cmd, x, top, w, h, 0.0f, 1.0f );
	VK_Draw_ScissorRect( cmd, x, top, w, h );
}

void VK_Draw_View3D( VkCommandBuffer cmd, int x, int y, int w, int h )
{
	int top = VK_Draw_TopY( y, h );

	VK_Draw_ViewportRect( cmd, x, top, w, h, 0.0f, 1.0f );
	VK_Draw_ScissorRect( cmd, x, top, w, h );

	/* the 3D viewport and scissor overwrite whatever 2D rect the buffer held */
	VK_Draw_IssueInvalidate();
}

void VK_Draw_BindRing( VkCommandBuffer cmd )
{
	VK_Draw_BindRingOffset( cmd, 0 );
}

/* Bind the ring at a byte offset (the 3D path's wider vertex). */
void VK_Draw_BindRingOffset( VkCommandBuffer cmd, VkDeviceSize vertexOffset )
{
	VkBuffer buffers[2];
	VkDeviceSize offsets[2];
	int			slot = RB_FrameSmpFrame();
	int			frame = RB_FrameCount();

	offsets[0] = vertexOffset;
	offsets[1] = 0;

	VK_WalkStat_Req( VK_ELIDE_INDEX );	/* one request per call, two binds may elide */

	buffers[0] = VK_VertexRing_Buffer();
	buffers[1] = VK_VertexRing_IndexBuffer();

	/* The vertex bind repeats for every surface that packs at the same offset --
	   the 2D path always binds offset 0.  The same frame's fresh command buffer
	   holds no binding, so the cache is keyed on the frame like the index's. */
	{
		int bslot = ( slot >= 0 && slot < VK_FRAMES_IN_FLIGHT ) ? slot : 0;

		if ( buffers[0] != VK_NULL_HANDLE
			 && ( s_vertexBound[bslot] != buffers[0] || s_vertexBoundFrame[bslot] != frame
				  || s_vertexBoundOffset[bslot] != vertexOffset ) ) {
			qvk.CmdBindVertexBuffers( cmd, 0, 1, &buffers[0], &offsets[0] );
			s_vertexBound[bslot]       = buffers[0];
			s_vertexBoundFrame[bslot]  = frame;
			s_vertexBoundOffset[bslot] = vertexOffset;
		} else if ( buffers[0] != VK_NULL_HANDLE ) {
			VK_WalkStat_Count( VK_ELIDE_INDEX );
		}
	}

	/* the index buffer never moves: it is the ring's own, always at offset 0, and this is the only CmdBindIndexBuffer in the renderer.  Binding it
	   once per command buffer instead of once per draw halves the driver calls, and the cache is keyed on the frame as well as the slot. */
	if ( slot < 0 || slot >= VK_FRAMES_IN_FLIGHT ) {
		slot = 0;
	}
	if ( buffers[1] != VK_NULL_HANDLE
		 && ( s_indexBound[slot] != buffers[1] || s_indexBoundFrame[slot] != frame ) ) {
		qvk.CmdBindIndexBuffer( cmd, buffers[1], 0, VK_INDEX_TYPE_UINT32 );
		s_indexBound[slot] = buffers[1];
		s_indexBoundFrame[slot] = frame;
	} else if ( buffers[1] != VK_NULL_HANDLE ) {
		VK_WalkStat_Count( VK_ELIDE_INDEX );
	}
}

/* every indexed draw this walk recorded, and the indexes they carried: the
   frame's own count against the front end's draw list, so a coverage hole can
   be split into "the draw never existed" and "the draw existed and covered
   nothing" against the capture's event count */
static int s_frameDrawIndexed;
static int s_frameDrawIndexedIdx;

void VK_Draw_RecordedDraws( int *draws, int *indexes )
{
	*draws   = s_frameDrawIndexed;
	*indexes = s_frameDrawIndexedIdx;
}

void VK_Draw_ResetRecordedDraws( void )
{
	s_frameDrawIndexed   = 0;
	s_frameDrawIndexedIdx = 0;
}

void VK_Draw_DrawIndexed( VkCommandBuffer cmd, int indexCount, int firstIndex, int vertexBase )
{
	s_frameDrawIndexed++;
	s_frameDrawIndexedIdx += indexCount;

	qvk.CmdDrawIndexed( cmd, (uint32_t)indexCount, 1, (uint32_t)firstIndex, (int32_t)vertexBase, 0 );
}

/* The static arena's own binds.  Both keep the index elision cache honest: a
   buffer bound here is what the next ring bind must compare against. */
void VK_Draw_BindVertexBuffer( VkCommandBuffer cmd, VkBuffer buffer, VkDeviceSize offset )
{
	VkDeviceSize	offsets[1];

	offsets[0] = offset;
	qvk.CmdBindVertexBuffers( cmd, 0, 1, &buffer, offsets );
}

void VK_Draw_BindIndexBuffer( VkCommandBuffer cmd, VkBuffer buffer )
{
	int		slot = RB_FrameSmpFrame();

	if ( slot < 0 || slot >= VK_FRAMES_IN_FLIGHT ) {
		slot = 0;
	}

	qvk.CmdBindIndexBuffer( cmd, buffer, 0, VK_INDEX_TYPE_UINT32 );
	s_indexBound[slot] = buffer;
	s_indexBoundFrame[slot] = RB_FrameCount();
}

void VK_Draw_Draw( VkCommandBuffer cmd, int vertexCount, int firstVertex )
{
	qvk.CmdDraw( cmd, (uint32_t)vertexCount, 1, (uint32_t)firstVertex, 0 );
}

qboolean VK_Draw_CurrentValid( void )
{
	return vk_currentValid;
}

/* The 2D window's ortho, for the census that has to run the same transform the
   draw's push constants carry.  NULL while no window is in force. */
const float *VK_Draw_CurrentOrtho( void )
{
	return vk_currentValid ? vk_current.ortho : NULL;
}

VkPipelineLayout VK_Draw_Layout( void )
{
	return vk_draw.layout;
}
