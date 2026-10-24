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

/* The load's own crash marker, the same shape as tr_init.c's VK_SAFE_MODE_MARKER:
   armed before the one call that can die inside the driver, cleared when it
   returns.  Present at the next launch means the parse killed the process. */
#define VK_PCACHE_LOAD_MARKER ".vk_pcache_load"
#define VK_PCACHE_DEAD_MARKER ".vk_pcache_dead"

static qboolean VK_PCache_MarkerPresent( const char *name )
{
	return ( ri.FS_FileExists && ri.FS_FileExists( name ) ) ? qtrue : qfalse;
}

static void VK_PCache_MarkerWrite( const char *name )
{
	fileHandle_t f;

	if ( !ri.FS_OpenFileWrite ) {
		return;
	}

	f = ri.FS_OpenFileWrite( name );
	if ( f ) {
		ri.FS_Write( "1", 1, f );
		ri.FS_CloseFile( f );
	}
}

static void VK_PCache_MarkerClear( const char *name )
{
	if ( ri.FS_RemoveHomeData ) {
		ri.FS_RemoveHomeData( name );
	}
}

typedef struct {
	uint32_t magic;
	uint32_t version;
	uint32_t dataSize;
	uint32_t vendorID;
	uint32_t deviceID;
	uint32_t driverVersion;
	uint8_t pipelineCacheUUID[VK_UUID_SIZE];
} vkPcacheHeader_t;

/* fs_homepath is the desktop launcher's var; the port keeps the write root in
   fs_homedatapath, and on Android fs_homepath is empty. */
static const char *VK_PCache_Home( void )
{
	cvar_t *cv;

	cv = ri.Cvar_Get( "fs_homepath", "", 0 );
	if ( cv && cv->string[0] ) {
		return cv->string;
	}
	cv = ri.Cvar_Get( "fs_homedatapath", "", 0 );
	if ( cv && cv->string[0] ) {
		return cv->string;
	}
	cv = ri.Cvar_Get( "fs_homeconfigpath", "", 0 );
	return ( cv && cv->string[0] ) ? cv->string : NULL;
}

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
	cvar_t *loadCvar;

	vk.pipelineCache = VK_NULL_HANDLE;
	vk.pipelineCacheLoaded = qfalse;
	vk.pipelineCacheForeign = qfalse;
	vk.pipelineCacheBytes = 0;

	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

	homeCvar = ri.Cvar_Get( "fs_homepath", "", 0 );
	home = homeCvar ? homeCvar->string : NULL;
	if ( !home || !home[0] ) {
		home = VK_PCache_Home();
	}

	/* Loaded by default on the phone, opt-in on the desktop.  Mesa's ANV 25.2.8
	   segfaults inside vkCreatePipelineCache parsing a blob it saved itself
	   (memcpy from libvulkan_intel, fault at blob+0x51010 on every reopen), so
	   the marker below turns that from a repeat into a one-boot cost; the
	   phone's pipeline compiles are the slow leg of a cold boot. */
#ifdef __ANDROID__
	loadCvar = ri.Cvar_Get( "r_vkPipelineCacheLoad", "1", 0 );
#else
	loadCvar = ri.Cvar_Get( "r_vkPipelineCacheLoad", "0", 0 );
#endif

	if ( home && home[0] && loadCvar->integer ) {
		snprintf( path, sizeof( path ), "%s/%s", home, VK_PCACHE_FILE );
		fh = fopen( path, "rb" );
	} else {
		path[0] = '\0';
		fh = NULL;
	}

	/* A load that killed the last bring-up accuses its blob: drop it and retire
	   the load for this install, so the next boot compiles cold and the save
	   below writes a blob this device will not be asked to parse again. */
	if ( VK_PCache_MarkerPresent( VK_PCACHE_LOAD_MARKER ) ) {
		VK_Guard_Row( "VK:pcache", "status=load-crashed marker=%s blob-removed=%d",
					  VK_PCACHE_LOAD_MARKER, home && home[0] ? 1 : 0 );
		VK_PCache_MarkerClear( VK_PCACHE_LOAD_MARKER );
		if ( home && home[0] ) {
			VK_PCache_MarkerWrite( VK_PCACHE_DEAD_MARKER );
			VK_PCache_MarkerClear( VK_PCACHE_FILE );
		}
		path[0] = '\0';
		fh = NULL;
	} else if ( VK_PCache_MarkerPresent( VK_PCACHE_DEAD_MARKER ) ) {
		if ( R_ProbeStatsOn() ) {
			VK_Guard_Row( "VK:pcache", "status=load-retired marker=%s", VK_PCACHE_DEAD_MARKER );
		}
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

	/* the parse is the only step that has ever died inside a driver, so the
	   marker covers exactly it */
	if ( createInfo.initialDataSize ) {
		VK_PCache_MarkerWrite( VK_PCACHE_LOAD_MARKER );
	}

	result = qvk.CreatePipelineCache( vk.device, &createInfo, NULL, &vk.pipelineCache );

	if ( createInfo.initialDataSize ) {
		VK_PCache_MarkerClear( VK_PCACHE_LOAD_MARKER );
	}

	if ( result != VK_SUCCESS ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "vkCreatePipelineCache", result );
		vk.pipelineCache = VK_NULL_HANDLE;
		return qfalse;
	}

	if ( vk.pipelineCacheForeign ) {
		/* an on-disk cache the device cannot use is a failed load: it stays loud */
		VK_Guard_Row( "VK:pcache", "status=foreign path=%s size=%u device=%04x:%04x",
					  path, header.dataSize, header.vendorID, header.deviceID );
	} else if ( vk.pipelineCacheLoaded ) {
		if ( R_ProbeStatsOn() ) {
			VK_Guard_Row( "VK:pcache", "status=loaded path=%s bytes=%u", path,
						  (unsigned int)vk.pipelineCacheBytes );
		}
	} else if ( R_ProbeStatsOn() ) {
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
		home = VK_PCache_Home();
	}
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

	if ( R_ProbeStatsOn() ) {
		VK_Guard_Row( "VK:pcache", "status=saved path=%s bytes=%u", path, (unsigned int)bytes );
	}
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
