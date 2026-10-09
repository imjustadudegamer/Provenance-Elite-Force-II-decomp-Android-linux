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
#include "vk_shaders_embedded.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
	VkShaderModule vertex;
	VkShaderModule fragment;
	VkPipelineLayout layout;
	VkPipeline demoPipeline;
} vkPipelineState_t;

static vkPipelineState_t vk_pipeline;

/* The vertex attribute locations a shader stage actually consumes, as a bitmask.
   Read out of the module's own SPIR-V: every Input-storage OpVariable whose id
   carries a Location decoration (the vertex stream's layout is one fixed stride,
   and an attribute the stage does not read may not be declared in the pipeline's
   vertex input state -- "Vertex attribute at location N not consumed by vertex
   shader", the OutputNotConsumed class the layer reported once per pipeline).
   Returns 0xFFFFFFFF when the module cannot be read, so an unreadable module
   keeps today's bind-everything behaviour rather than dropping a real input. */
unsigned int VK_Pipeline_VertexInputMask( const unsigned int *code, unsigned int words )
{
	unsigned int mask = 0;
	unsigned int i, j;

	if ( !code || words < 5 ) {
		return 0xFFFFFFFFu;
	}

	for ( i = 5; i < words; ) {
		unsigned int wc = code[i] >> 16;
		unsigned int op = code[i] & 0xFFFFu;

		if ( wc == 0 || i + wc > words ) {
			break;
		}

		/* OpVariable = 59, storage class Input = 1, result id at operand 2 */
		if ( op == 59 && wc >= 4 && code[i + 3] == 1 ) {
			unsigned int id = code[i + 2];

			for ( j = 5; j < words; ) {
				unsigned int dwc = code[j] >> 16;
				unsigned int dop = code[j] & 0xFFFFu;

				if ( dwc == 0 || j + dwc > words ) {
					break;
				}

				/* OpDecorate = 71, Decoration Location = 30.  The instruction
				   is (wordCount << 16) | opcode, then target id, decoration,
				   literal -- a Location decoration is FOUR words and the value
				   is the fourth.  Reading it as five words with the operands
				   shifted by one matched nothing and returned the
				   bind-everything mask, which is the warning left unfixed. */
				if ( dop == 71 && dwc >= 4 && code[j + 1] == id && code[j + 2] == 30 ) {
					if ( code[j + 3] < 32 ) {
						mask |= ( 1u << code[j + 3] );
					}
				}

				j += dwc;
			}
		}

		i += wc;
	}

	return mask ? mask : 0xFFFFFFFFu;
}

/* Keep only the attribute descriptions the stage consumes; the stride and the
   binding are untouched, because the ring's vertex format is fixed. */
unsigned int VK_Pipeline_TrimAttributes( VkVertexInputAttributeDescription *attribs,
										 unsigned int count, unsigned int mask )
{
	unsigned int in, out;

	for ( in = 0, out = 0; in < count; in++ ) {
		if ( mask & ( 1u << attribs[in].location ) ) {
			attribs[out++] = attribs[in];
		}
	}

	return out;
}

static qboolean VK_Pipeline_Module( const unsigned int *code, unsigned int words,
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

qboolean VK_Pipeline_Init( void )
{
	VkPipelineLayoutCreateInfo layoutInfo;
	VkPipelineVertexInputStateCreateInfo vertexInput;
	VkVertexInputBindingDescription binding;
	VkVertexInputAttributeDescription attributes[2];
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineViewportStateCreateInfo viewportState;
	VkPipelineRasterizationStateCreateInfo rasterization;
	VkPipelineMultisampleStateCreateInfo multisample;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkPipelineColorBlendAttachmentState blendAttachment;
	VkPipelineColorBlendStateCreateInfo colourBlend;
	VkDynamicState dynamicStates[2];
	VkPipelineDynamicStateCreateInfo dynamicState;
	VkGraphicsPipelineCreateInfo pipelineInfo;
	VkResult result;

	memset( &vk_pipeline, 0, sizeof( vk_pipeline ) );

	if ( !VK_Pipeline_Module( vk_pos_vert_spv, VK_POS_VERT_SPV_WORDS, &vk_pipeline.vertex,
							  "vkCreateShaderModule(vertex)" ) ) {
		return qfalse;
	}

	if ( !VK_Pipeline_Module( vk_flat_frag_spv, VK_FLAT_FRAG_SPV_WORDS, &vk_pipeline.fragment,
							  "vkCreateShaderModule(fragment)" ) ) {
		return qfalse;
	}

	memset( &layoutInfo, 0, sizeof( layoutInfo ) );
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	if ( !VK_Guard_Result( VK_GSITE_DESCRIPTOR, qvk.CreatePipelineLayout( vk.device, &layoutInfo, NULL,
																		 &vk_pipeline.layout ),
						   "vkCreatePipelineLayout" ) ) {
		return qfalse;
	}

	memset( &binding, 0, sizeof( binding ) );
	binding.binding = 0;
	binding.stride = sizeof( vkDemoVertex_t );
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	memset( &attributes, 0, sizeof( attributes ) );
	attributes[0].location = 0;
	attributes[0].binding = 0;
	attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
	attributes[0].offset = offsetof( vkDemoVertex_t, position );
	attributes[1].location = 1;
	attributes[1].binding = 0;
	attributes[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attributes[1].offset = offsetof( vkDemoVertex_t, colour );


	memset( &vertexInput, 0, sizeof( vertexInput ) );
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.vertexBindingDescriptionCount = 1;
	vertexInput.pVertexBindingDescriptions = &binding;
	vertexInput.vertexAttributeDescriptionCount = VK_Pipeline_TrimAttributes(
		attributes, 2, VK_Pipeline_VertexInputMask( vk_pos_vert_spv, VK_POS_VERT_SPV_WORDS ) );
	vertexInput.pVertexAttributeDescriptions = attributes;

	memset( &inputAssembly, 0, sizeof( inputAssembly ) );
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

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
	rasterization.cullMode = VK_CULL_MODE_NONE;
	rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterization.lineWidth = 1.0f;

	memset( &multisample, 0, sizeof( multisample ) );
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	memset( &depthStencil, 0, sizeof( depthStencil ) );
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	memset( &blendAttachment, 0, sizeof( blendAttachment ) );
	blendAttachment.blendEnable = VK_FALSE;
	blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
									 | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	memset( &colourBlend, 0, sizeof( colourBlend ) );
	colourBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colourBlend.attachmentCount = 1;
	colourBlend.pAttachments = &blendAttachment;

	memset( &pipelineInfo, 0, sizeof( pipelineInfo ) );
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;

	{
		VkPipelineShaderStageCreateInfo stages[2];

		memset( &stages, 0, sizeof( stages ) );
		stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vk_pipeline.vertex;
		stages[0].pName = "main";
		stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = vk_pipeline.fragment;
		stages[1].pName = "main";

		pipelineInfo.pStages = stages;
		pipelineInfo.pVertexInputState = &vertexInput;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterization;
		pipelineInfo.pMultisampleState = &multisample;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pColorBlendState = &colourBlend;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = vk_pipeline.layout;
		pipelineInfo.renderPass = vk.renderPass;
		pipelineInfo.subpass = 0;

		result = qvk.CreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &pipelineInfo, NULL,
											  &vk_pipeline.demoPipeline );
	}

	if ( result != VK_SUCCESS ) {
		VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "vkCreateGraphicsPipelines(demo)", result );
		return qfalse;
	}

	if ( R_ProbeStatsOn() ) {
		VK_Guard_Row( "VK:pipeline", "demo ready topology=tri-list depth=leq blend=off pass=swapchain handle=%p",
					  (void *)(size_t)vk_pipeline.demoPipeline );
	}

	return qtrue;
}

void VK_Pipeline_Shutdown( void )
{
	if ( vk.device == VK_NULL_HANDLE ) {
		return;
	}

	if ( vk_pipeline.demoPipeline != VK_NULL_HANDLE && qvk.DestroyPipeline ) {
		qvk.DestroyPipeline( vk.device, vk_pipeline.demoPipeline, NULL );
	}
	vk_pipeline.demoPipeline = VK_NULL_HANDLE;

	if ( vk_pipeline.layout != VK_NULL_HANDLE && qvk.DestroyPipelineLayout ) {
		qvk.DestroyPipelineLayout( vk.device, vk_pipeline.layout, NULL );
	}
	vk_pipeline.layout = VK_NULL_HANDLE;

	if ( vk_pipeline.fragment != VK_NULL_HANDLE && qvk.DestroyShaderModule ) {
		qvk.DestroyShaderModule( vk.device, vk_pipeline.fragment, NULL );
	}
	vk_pipeline.fragment = VK_NULL_HANDLE;

	if ( vk_pipeline.vertex != VK_NULL_HANDLE && qvk.DestroyShaderModule ) {
		qvk.DestroyShaderModule( vk.device, vk_pipeline.vertex, NULL );
	}
	vk_pipeline.vertex = VK_NULL_HANDLE;
}

qboolean VK_Pipeline_DrawDemo( VkCommandBuffer cmd )
{
	VkDeviceSize offset = 0;
	VkViewport viewport;
	VkRect2D scissor;

	if ( vk_pipeline.demoPipeline == VK_NULL_HANDLE ) {
		return qfalse;
	}

	memset( &viewport, 0, sizeof( viewport ) );
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)vk.swapchainExtent.width;
	viewport.height = (float)vk.swapchainExtent.height;
	viewport.maxDepth = 1.0f;

	memset( &scissor, 0, sizeof( scissor ) );
	scissor.extent = vk.swapchainExtent;

	qvk.CmdSetViewport( cmd, 0, 1, &viewport );
	qvk.CmdSetScissor( cmd, 0, 1, &scissor );

	VK_Draw_BindPipeline( cmd, vk_pipeline.demoPipeline );
	qvk.CmdBindVertexBuffers( cmd, 0, 1, &vk.vertexBuffer, &offset );
	qvk.CmdDraw( cmd, 3, 1, 0, 0 );

	return qtrue;
}
