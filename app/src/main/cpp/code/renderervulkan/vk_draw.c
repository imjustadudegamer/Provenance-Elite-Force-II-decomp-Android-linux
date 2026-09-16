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

	memset( &vertexInput, 0, sizeof( vertexInput ) );
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.vertexBindingDescriptionCount = 1;
	vertexInput.pVertexBindingDescriptions = &binding;
	vertexInput.vertexAttributeDescriptionCount = 3;
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

	VK_Guard_Row( "VK:draw", "ready pipelines=%d sets=%d vertex-bytes=%d",
				  VK_PIPELINE_SLOTS, VK_SET_SLOTS, VK_TESS_VERTEX_BYTES );

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
	int i;

	for ( i = 0; i < vk_numPipelines; i++ ) {
		if ( vk_pipelines[i].used
			 && memcmp( &vk_pipelines[i].key, key, sizeof( *key ) ) == 0 ) {
			return vk_pipelines[i].pipeline;
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

	if ( !VK_Draw_Build( key, &vk_pipelines[i].pipeline ) ) {
		return VK_NULL_HANDLE;
	}

	vk_pipelines[i].key = *key;
	vk_pipelines[i].used = qtrue;
	if ( i == vk_numPipelines ) {
		vk_numPipelines = i + 1;
	}

	VK_PipelineCache_Save();

	return vk_pipelines[i].pipeline;
}

qboolean VK_Draw_BindTexture( VkCommandBuffer cmd, VkImageView view, VkSampler sampler )
{
	VkDescriptorSetAllocateInfo allocInfo;
	VkDescriptorImageInfo imageInfo;
	VkWriteDescriptorSet write;
	VkDescriptorSet set = VK_NULL_HANDLE;
	int i;

	if ( view == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE ) {
		VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "bind of an empty view or sampler", VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	for ( i = 0; i < VK_SET_SLOTS; i++ ) {
		if ( vk_sets[i].used && vk_sets[i].view == view && vk_sets[i].sampler == sampler ) {
			set = vk_sets[i].set;
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

		set = vk_sets[i].set;
	}

	qvk.CmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_draw.layout, 0, 1, &set,
							   0, NULL );

	return qtrue;
}

/* The 2D window in force, for the draw row.  A quad's rect is local to the
   window the UI sets around it, so the rect alone reads as a pile of quads all
   at the origin; the window is what puts the tile on its own screen rect. */
int	vk_2dWindow[4]   = { 0, 0, 0, 0 };
int	vk_2dScissor[4]  = { 0, 0, 0, 0 };

void VK_Draw_Set2D( VkCommandBuffer cmd, int x, int y, int w, int h,
					float left, float right, float bottom, float top, float n, float f )
{
	float m[16];
	float rl, tb, fn;
	qboolean degenerate;

	vk_2dWindow[0] = x; vk_2dWindow[1] = y; vk_2dWindow[2] = w; vk_2dWindow[3] = h;
	vk_2dScissor[0] = x; vk_2dScissor[1] = y; vk_2dScissor[2] = w; vk_2dScissor[3] = h;

	rl = right - left;
	tb = top - bottom;
	fn = f - n;
	degenerate = ( rl == 0.0f || tb == 0.0f || fn == 0.0f ) ? qtrue : qfalse;

	if ( degenerate ) {
		/* Retail's own UI authors zero-extent windows (menu_controls' bindscroll, its two "table fudge"
		   columns, menu_loadgame's width-0 columns); the dims travel set2D -> Set2DWindow -> this record
		   verbatim and nothing rasterises. A diagnosis row: [VK:guard] stays the error stream. */
		static int s_rows;

		if ( s_rows < 8 || ( s_rows % 500 ) == 0 ) {
			VK_Guard_Row( "VK:2dwin",
						  "site=sync detail=\"degenerate 2D window x=%d y=%d w=%d h=%d "
						  "l=%g r=%g b=%g t=%g n=%g f=%g\" n=%u",
						  x, y, w, h, left, right, bottom, top, n, f, s_rows + 1 );
		}
		s_rows++;
	}

	/* The window is issued either way, as the reference does: the zero-extent
	   scissor rasterises nothing.  Returning left the last window in force. */
	memset( m, 0, sizeof( m ) );
	if ( !degenerate ) {
		m[0]  = 2.0f / rl;
		m[5]  = -2.0f / tb;
		m[10] = -1.0f / fn;
		m[12] = -( right + left ) / rl;
		m[13] = ( top + bottom ) / tb;
		m[14] = 0.5f - ( f + n ) / ( 2.0f * fn );
		m[15] = 1.0f;
	} else {
		m[0] = m[5] = m[10] = m[15] = 1.0f;
	}

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
}

/* The alpha test rides the stage's push constants; the value is the reference
   selector (0 off, 1 GT0, 2 LT80, 4 GE128), which no threshold expresses. */
void VK_Draw_SetAlphaTest( VkCommandBuffer cmd, unsigned stateBits )
{
	float values[4];

	memset( values, 0, sizeof( values ) );

	switch ( stateBits & VKS_ATEST_BITS ) {
	case VKS_ATEST_GT_0:	values[0] = 1.0f;			break;
	case VKS_ATEST_LT_80:	values[0] = 2.0f;			break;
	case VKS_ATEST_GE_80:	values[0] = 4.0f;			break;
	default:				return;		/* no alpha test: leave it off */
	}

	qvk.CmdPushConstants( cmd, vk_draw.layout,
						  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
						  offsetof( vkPushConstants_t, alphaTest ), sizeof( values ), values );
}

void VK_Draw_SetScissor( VkCommandBuffer cmd, int x, int y, int width, int height )
{
	vk_2dScissor[0] = x; vk_2dScissor[1] = y; vk_2dScissor[2] = width; vk_2dScissor[3] = height;

	/* RE_Scissor's box is bottom-left; Vulkan's is the top-left, and the box is
	   cut to the render area the way GL cut it for free. */
	VK_Draw_ScissorRect( cmd, x, VK_Draw_TopY( y, height ), width, height );
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

/* The reference hands the client's window straight to glScissor, and GL clamps
   the box to the framebuffer for free: a tile that hangs off the bottom of the
   screen rasterises the part that lands.  Vulkan does not clamp, and a
   negative scissor offset is undefined rasterization -- 6,743 of them in one
   boot, every one a 2D window taller than the screen it sits on.  This is the
   reference's clamp, written out: the box is cut to the render area, and a box
   that lands entirely outside becomes the empty extent, which rasterises
   nothing exactly as GL's zero-area box does.  The corners are held wide
   because the box's own origin overflows an int. */
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

	qvk.CmdSetScissor( cmd, 0, 1, &scissor );
}

/* The viewport has no framebuffer to be cut to, but it does have a device
   range, and a coordinate outside it is undefined too.  The rect is held inside
   what the device said it takes.  Every CmdSetViewport in this lane goes through
   here, so no site can invent an out-of-range origin. */
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

	offsets[0] = vertexOffset;
	offsets[1] = 0;

	buffers[0] = VK_VertexRing_Buffer();
	buffers[1] = VK_VertexRing_IndexBuffer();

	qvk.CmdBindVertexBuffers( cmd, 0, 1, &buffers[0], &offsets[0] );

	/* glIndex_t is a 32-bit index; the buffer is filled with those. */
	if ( buffers[1] != VK_NULL_HANDLE ) {
		qvk.CmdBindIndexBuffer( cmd, buffers[1], 0, VK_INDEX_TYPE_UINT32 );
	}
}

void VK_Draw_DrawIndexed( VkCommandBuffer cmd, int indexCount, int firstIndex, int vertexBase )
{
	qvk.CmdDrawIndexed( cmd, (uint32_t)indexCount, 1, (uint32_t)firstIndex, (int32_t)vertexBase, 0 );
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
