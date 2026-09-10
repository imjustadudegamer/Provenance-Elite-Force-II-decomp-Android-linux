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

int VK_Memory_FindType( uint32_t typeBits, VkMemoryPropertyFlags wanted )
{
	uint32_t i;

	for ( i = 0; i < vk.memProps.memoryTypeCount; i++ ) {
		if ( ( typeBits & ( 1u << i ) )
			 && ( vk.memProps.memoryTypes[i].propertyFlags & wanted ) == wanted ) {
			return (int)i;
		}
	}

	return -1;
}

qboolean VK_Memory_AllocFor( VkMemoryRequirements requirements, VkMemoryPropertyFlags wanted,
							 VkDeviceMemory *memory )
{
	static const VkMemoryPropertyFlags fallbacks[] = {
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0
	};
	int typeIndex;
	size_t i;

	typeIndex = VK_Memory_FindType( requirements.memoryTypeBits, wanted );

	for ( i = 0; typeIndex < 0 && i < sizeof( fallbacks ) / sizeof( fallbacks[0] ); i++ ) {
		if ( fallbacks[i] == wanted ) {
			continue;
		}
		typeIndex = VK_Memory_FindType( requirements.memoryTypeBits, fallbacks[i] );
	}

	if ( typeIndex < 0 ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "no memory type", VK_ERROR_OUT_OF_DEVICE_MEMORY );
		return qfalse;
	}

	{
		VkMemoryAllocateInfo allocateInfo;

		memset( &allocateInfo, 0, sizeof( allocateInfo ) );
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = requirements.size;
		allocateInfo.memoryTypeIndex = (uint32_t)typeIndex;

		return VK_Guard_Result( VK_GSITE_ALLOC,
								qvk.AllocateMemory( vk.device, &allocateInfo, NULL, memory ),
								"vkAllocateMemory" );
	}
}

qboolean VK_Memory_AllocImage( VkImage image, VkMemoryPropertyFlags wanted, VkDeviceMemory *memory )
{
	VkMemoryRequirements requirements;

	memset( &requirements, 0, sizeof( requirements ) );
	qvk.GetImageMemoryRequirements( vk.device, image, &requirements );

	if ( !VK_Memory_AllocFor( requirements, wanted, memory ) ) {
		return qfalse;
	}

	return VK_Guard_Result( VK_GSITE_ALLOC,
							qvk.BindImageMemory( vk.device, image, *memory, 0 ),
							"vkBindImageMemory" );
}

qboolean VK_Memory_AllocBuffer( VkBuffer buffer, VkMemoryPropertyFlags wanted, VkDeviceMemory *memory )
{
	VkMemoryRequirements requirements;

	memset( &requirements, 0, sizeof( requirements ) );
	qvk.GetBufferMemoryRequirements( vk.device, buffer, &requirements );

	if ( !VK_Memory_AllocFor( requirements, wanted, memory ) ) {
		return qfalse;
	}

	return VK_Guard_Result( VK_GSITE_ALLOC,
							qvk.BindBufferMemory( vk.device, buffer, *memory, 0 ),
							"vkBindBufferMemory" );
}

qboolean VK_Memory_CreateBuffer( VkDeviceSize size, VkBufferUsageFlags usage,
								 VkMemoryPropertyFlags wanted, VkBuffer *buffer, VkDeviceMemory *memory )
{
	VkBufferCreateInfo createInfo;
	VkMemoryRequirements requirements;

	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.size = size;
	createInfo.usage = usage;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if ( !VK_Guard_Result( VK_GSITE_ALLOC, qvk.CreateBuffer( vk.device, &createInfo, NULL, buffer ),
						   "vkCreateBuffer" ) ) {
		return qfalse;
	}

	memset( &requirements, 0, sizeof( requirements ) );
	qvk.GetBufferMemoryRequirements( vk.device, *buffer, &requirements );

	if ( !VK_Memory_AllocFor( requirements, wanted, memory ) ) {
		qvk.DestroyBuffer( vk.device, *buffer, NULL );
		*buffer = VK_NULL_HANDLE;
		return qfalse;
	}

	if ( !VK_Guard_Result( VK_GSITE_ALLOC, qvk.BindBufferMemory( vk.device, *buffer, *memory, 0 ),
						   "vkBindBufferMemory" ) ) {
		return qfalse;
	}

	return qtrue;
}
