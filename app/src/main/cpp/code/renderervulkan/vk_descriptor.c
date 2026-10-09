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

#define VK_MAX_BINDLESS_TEXTURES 16536u
#define VK_BINDLESS_HEADROOM 8u
#define VK_COMPAT_TEXTURE_SLOTS 4u

static uint32_t VK_Descriptor_BindlessSize( void )
{
	uint32_t deviceLimit;
	qboolean useUAB;

	if ( !vk.bindless ) {
		return 0;
	}

	useUAB = qfalse;
	if ( vk.updateAfterBind && vk.has12Props
		 && vk.props12.maxPerStageDescriptorUpdateAfterBindSampledImages > 0 ) {
		useUAB = qtrue;
	}

	if ( useUAB ) {
		uint32_t perStage = vk.props12.maxPerStageDescriptorUpdateAfterBindSampledImages;
		uint32_t perSet = vk.props12.maxDescriptorSetUpdateAfterBindSampledImages;

		deviceLimit = ( perStage < perSet ) ? perStage : perSet;
	} else {
		deviceLimit = vk.deviceProps.limits.maxPerStageDescriptorSampledImages;
	}

	if ( deviceLimit == 0 ) {
		return 1;
	}

	if ( deviceLimit > VK_BINDLESS_HEADROOM ) {
		deviceLimit -= VK_BINDLESS_HEADROOM;
	}

	if ( deviceLimit > VK_MAX_BINDLESS_TEXTURES ) {
		deviceLimit = VK_MAX_BINDLESS_TEXTURES;
	}

	return deviceLimit ? deviceLimit : 1;
}

qboolean VK_Descriptor_Init( void )
{
	VkDescriptorPoolSize poolSizes[3];
	VkDescriptorPoolCreateInfo poolInfo;
	VkDescriptorSetLayoutBinding binding;
	VkDescriptorSetLayoutCreateInfo layoutInfo;
	VkDescriptorBindingFlags bindFlags;
	VkDescriptorSetLayoutBindingFlagsCreateInfo bindInfo;
	uint32_t textures;
	VkResult result;

	textures = VK_Descriptor_BindlessSize();
	vk.bindlessTextures = textures;

	/* a type the pool never declares is a budget of zero: the shade set's
	   dynamic uniform buffer would fail on a strict driver */
	memset( poolSizes, 0, sizeof( poolSizes ) );
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	memset( &poolInfo, 0, sizeof( poolInfo ) );
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = VK_SET_SLOTS_TOTAL;

	if ( textures > 0 ) {
		poolSizes[0].descriptorCount = textures + VK_SET_SLOTS_TOTAL
									   + VK_SHADE_SETS * VK_SHADE_SET_SAMPLERS_PER_SET;
		poolSizes[1].descriptorCount = VK_SHADE_SETS * VK_SHADE_SET_DYNAMIC_UBOS_PER_SET;
		poolSizes[2].descriptorCount = 1024;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		if ( vk.updateAfterBind ) {
			poolInfo.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
		}
		poolInfo.poolSizeCount = 3;
		poolInfo.pPoolSizes = poolSizes;

		result = qvk.CreateDescriptorPool( vk.device, &poolInfo, NULL, &vk.descriptorPool );
		if ( result != VK_SUCCESS ) {
			VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "bindless pool create", result );

			memset( &poolInfo, 0, sizeof( poolInfo ) );
			poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			poolInfo.maxSets = VK_SET_SLOTS_TOTAL;
			poolSizes[0].descriptorCount = 1024 + VK_SET_SLOTS_TOTAL
										   + VK_SHADE_SETS * VK_SHADE_SET_SAMPLERS_PER_SET;
			poolSizes[1].descriptorCount = VK_SHADE_SETS * VK_SHADE_SET_DYNAMIC_UBOS_PER_SET;
			poolSizes[2].descriptorCount = 1024;
			poolInfo.poolSizeCount = 3;
			poolInfo.pPoolSizes = poolSizes;

			result = qvk.CreateDescriptorPool( vk.device, &poolInfo, NULL, &vk.descriptorPool );
			if ( result != VK_SUCCESS ) {
				VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "compat pool create", result );
				return qfalse;
			}
			vk.bindlessTextures = 0;
		}
	} else {
		poolSizes[0].descriptorCount = 1024 + VK_SET_SLOTS_TOTAL
									   + VK_SHADE_SETS * VK_SHADE_SET_SAMPLERS_PER_SET;
		poolSizes[1].descriptorCount = VK_SHADE_SETS * VK_SHADE_SET_DYNAMIC_UBOS_PER_SET;
		poolSizes[2].descriptorCount = 1024;
		poolInfo.poolSizeCount = 3;
		poolInfo.pPoolSizes = poolSizes;

		result = qvk.CreateDescriptorPool( vk.device, &poolInfo, NULL, &vk.descriptorPool );
		if ( result != VK_SUCCESS ) {
			VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "compat pool create", result );
			return qfalse;
		}
	}

	if ( vk.bindlessTextures > 0 ) {
		memset( &binding, 0, sizeof( binding ) );
		binding.binding = 0;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = textures;
		binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		memset( &bindFlags, 0, sizeof( bindFlags ) );
		bindFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
		if ( vk.variableDescriptorCount ) {
			bindFlags |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
		}
		if ( vk.updateAfterBind ) {
			bindFlags |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
		}

		memset( &bindInfo, 0, sizeof( bindInfo ) );
		bindInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
		bindInfo.bindingCount = 1;
		bindInfo.pBindingFlags = &bindFlags;

		memset( &layoutInfo, 0, sizeof( layoutInfo ) );
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		if ( vk.updateAfterBind ) {
			layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
		}
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &binding;
		layoutInfo.pNext = &bindInfo;

		result = qvk.CreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &vk.bindlessLayout );
		if ( result != VK_SUCCESS ) {
			VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "bindless layout create", result );
			vk.bindlessTextures = 0;
		}
	}

	if ( vk.bindlessTextures == 0 ) {
		VkDescriptorSetLayoutBinding compatBindings[VK_COMPAT_TEXTURE_SLOTS];
		uint32_t i;

		memset( compatBindings, 0, sizeof( compatBindings ) );
		for ( i = 0; i < VK_COMPAT_TEXTURE_SLOTS; i++ ) {
			compatBindings[i].binding = i;
			compatBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			compatBindings[i].descriptorCount = 1;
			compatBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		memset( &layoutInfo, 0, sizeof( layoutInfo ) );
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = VK_COMPAT_TEXTURE_SLOTS;
		layoutInfo.pBindings = compatBindings;

		if ( !VK_Guard_Result( VK_GSITE_DESCRIPTOR,
							   qvk.CreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &vk.compatLayout ),
							   "compat layout create" ) ) {
			return qfalse;
		}
	}

	if ( R_ProbeStatsOn() ) {
		VK_Guard_Row( "VK:descriptor",
					  "path=%s textures=%u uab=%s update-after-bind=%s",
					  ( vk.bindlessTextures > 0 ) ? "bindless" : "compat",
					  ( vk.bindlessTextures > 0 ) ? vk.bindlessTextures : VK_COMPAT_TEXTURE_SLOTS,
					  ( vk.bindlessTextures > 0 ) ? "yes" : "no",
					  ( vk.bindlessTextures > 0 ) ? "1" : "0" );
	}

	return qtrue;
}

void VK_Descriptor_Shutdown( void )
{
	if ( vk.device == VK_NULL_HANDLE ) {
		return;
	}

	if ( vk.bindlessLayout != VK_NULL_HANDLE && qvk.DestroyDescriptorSetLayout ) {
		qvk.DestroyDescriptorSetLayout( vk.device, vk.bindlessLayout, NULL );
	}
	vk.bindlessLayout = VK_NULL_HANDLE;

	if ( vk.compatLayout != VK_NULL_HANDLE && qvk.DestroyDescriptorSetLayout ) {
		qvk.DestroyDescriptorSetLayout( vk.device, vk.compatLayout, NULL );
	}
	vk.compatLayout = VK_NULL_HANDLE;

	if ( vk.descriptorPool != VK_NULL_HANDLE && qvk.DestroyDescriptorPool ) {
		qvk.DestroyDescriptorPool( vk.device, vk.descriptorPool, NULL );
	}
	vk.descriptorPool = VK_NULL_HANDLE;

	vk.bindlessTextures = 0;
}
