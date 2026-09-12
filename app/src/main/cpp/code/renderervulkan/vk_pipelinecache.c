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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VK_PCACHE_MAGIC 0x4B435645u
#define VK_PCACHE_VERSION 1u
#define VK_PCACHE_HEADER_BYTES 40
#define VK_PCACHE_MAX_BYTES (32u * 1024u * 1024u)

typedef struct {
	uint32_t magic;
	uint32_t version;
	uint32_t dataSize;
	uint32_t vendorID;
	uint32_t deviceID;
	uint32_t driverVersion;
	uint8_t pipelineCacheUUID[VK_UUID_SIZE];
} vkPcacheHeader_t;

qboolean VK_PipelineCache_Open( void )
{
	VkPipelineCacheCreateInfo createInfo;
	vkPcacheHeader_t header;
	void *blob;
	FILE *fh;
	char path[1024];
	const char *home;
	VkResult result;
	cvar_t *homeCvar;

	vk.pipelineCache = VK_NULL_HANDLE;
	vk.pipelineCacheLoaded = qfalse;
	vk.pipelineCacheForeign = qfalse;
	vk.pipelineCacheBytes = 0;

	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

	homeCvar = ri.Cvar_Get( "fs_homepath", "", 0 );
	home = homeCvar ? homeCvar->string : NULL;
	if ( home && home[0] ) {
		snprintf( path, sizeof( path ), "%s/%s", home, VK_PCACHE_FILE );
		fh = fopen( path, "rb" );
	} else {
		path[0] = '\0';
		fh = NULL;
	}

	if ( fh ) {
		if ( fread( &header, 1, sizeof( header ), fh ) == sizeof( header )
			 && header.magic == VK_PCACHE_MAGIC
			 && header.version == VK_PCACHE_VERSION
			 && header.dataSize > 0
			 && header.dataSize <= VK_PCACHE_MAX_BYTES
			 && header.vendorID == vk.deviceProps.vendorID
			 && header.deviceID == vk.deviceProps.deviceID
			 && !memcmp( header.pipelineCacheUUID, vk.deviceProps.pipelineCacheUUID, VK_UUID_SIZE ) ) {
			blob = malloc( header.dataSize );
			if ( blob && fread( blob, 1, header.dataSize, fh ) == header.dataSize ) {
				createInfo.initialDataSize = header.dataSize;
				createInfo.pInitialData = blob;
				vk.pipelineCacheLoaded = qtrue;
				vk.pipelineCacheBytes = header.dataSize;
			} else if ( blob ) {
				VK_Guard_Hit( VK_GSITE_ALLOC, "pipeline cache blob short read", VK_INCOMPLETE );
			}
			free( blob );
		} else {
			vk.pipelineCacheForeign = qtrue;
		}
		fclose( fh );
	}

	result = qvk.CreatePipelineCache( vk.device, &createInfo, NULL, &vk.pipelineCache );

	if ( result != VK_SUCCESS ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "vkCreatePipelineCache", result );
		vk.pipelineCache = VK_NULL_HANDLE;
		return qfalse;
	}

	if ( vk.pipelineCacheForeign ) {
		VK_Guard_Row( "VK:pcache", "status=foreign path=%s size=%u device=%04x:%04x",
					  path, header.dataSize, header.vendorID, header.deviceID );
	} else if ( vk.pipelineCacheLoaded ) {
		VK_Guard_Row( "VK:pcache", "status=loaded path=%s bytes=%u", path,
					  (unsigned int)vk.pipelineCacheBytes );
	} else {
		VK_Guard_Row( "VK:pcache", "status=cold path=%s", path );
	}

	return qtrue;
}

void VK_PipelineCache_Save( void )
{
	vkPcacheHeader_t header;
	void *blob = NULL;
	size_t bytes = 0;
	FILE *fh;
	char path[1024];
	char temp[1100];
	const char *home;
	VkResult result;
	cvar_t *homeCvar;

	if ( vk.pipelineCache == VK_NULL_HANDLE || !qvk.GetPipelineCacheData ) {
		return;
	}

	result = qvk.GetPipelineCacheData( vk.device, vk.pipelineCache, &bytes, NULL );
	if ( result != VK_SUCCESS || bytes == 0 || bytes > VK_PCACHE_MAX_BYTES ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "pipeline cache size query", result );
		return;
	}

	blob = malloc( bytes );
	if ( !blob ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "pipeline cache save alloc", VK_ERROR_OUT_OF_HOST_MEMORY );
		return;
	}

	result = qvk.GetPipelineCacheData( vk.device, vk.pipelineCache, &bytes, blob );
	if ( result != VK_SUCCESS ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "pipeline cache fetch", result );
		free( blob );
		return;
	}

	homeCvar = ri.Cvar_Get( "fs_homepath", "", 0 );
	home = homeCvar ? homeCvar->string : NULL;
	if ( !home || !home[0] ) {
		free( blob );
		return;
	}

	snprintf( path, sizeof( path ), "%s/%s", home, VK_PCACHE_FILE );
	snprintf( temp, sizeof( temp ), "%s.tmp", path );

	memset( &header, 0, sizeof( header ) );
	header.magic = VK_PCACHE_MAGIC;
	header.version = VK_PCACHE_VERSION;
	header.dataSize = (uint32_t)bytes;
	header.vendorID = vk.deviceProps.vendorID;
	header.deviceID = vk.deviceProps.deviceID;
	header.driverVersion = vk.deviceProps.driverVersion;
	memcpy( header.pipelineCacheUUID, vk.deviceProps.pipelineCacheUUID, VK_UUID_SIZE );

	fh = fopen( temp, "wb" );
	if ( !fh ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "pipeline cache temp open", VK_ERROR_MEMORY_MAP_FAILED );
		free( blob );
		return;
	}

	if ( fwrite( &header, 1, sizeof( header ), fh ) != sizeof( header )
		 || fwrite( blob, 1, bytes, fh ) != bytes ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "pipeline cache write", VK_ERROR_MEMORY_MAP_FAILED );
		fclose( fh );
		remove( temp );
		free( blob );
		return;
	}

	fclose( fh );

	if ( rename( temp, path ) != 0 ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "pipeline cache rename", VK_ERROR_MEMORY_MAP_FAILED );
		remove( temp );
		free( blob );
		return;
	}

	free( blob );

	VK_Guard_Row( "VK:pcache", "status=saved path=%s bytes=%u", path, (unsigned int)bytes );
}

void VK_PipelineCache_Close( void )
{
	if ( vk.pipelineCache && qvk.DestroyPipelineCache ) {
		qvk.DestroyPipelineCache( vk.device, vk.pipelineCache, NULL );
	}
	vk.pipelineCache = VK_NULL_HANDLE;
	vk.pipelineCacheLoaded = qfalse;
	vk.pipelineCacheBytes = 0;
}
