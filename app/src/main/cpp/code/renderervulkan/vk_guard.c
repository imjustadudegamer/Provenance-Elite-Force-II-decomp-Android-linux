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
#include "vk_guard.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void ( *vk_guardLogger )( const char *line ) = NULL;
static int ( *vk_guardTrace )( const char *cvarName, int defaultValue ) = NULL;

static const char *vk_guardSiteNames[VK_GSITE_COUNT] = {
	"loader",
	"instance",
	"device",
	"surface",
	"swapchain-create",
	"swapchain-recreate",
	"swapchain-lost",
	"swapchain-out-of-date",
	"swapchain-suboptimal",
	"acquire",
	"present",
	"submit",
	"fence",
	"semaphore",
	"alloc",
	"descriptor",
	"sync",
	"device-lost",
	"quirk",
	"sentinel"
};

static unsigned int vk_guardSiteHits[VK_GSITE_COUNT];

void VK_Guard_SetLogger( void ( *fn )( const char *line ) )
{
	vk_guardLogger = fn;
}

void VK_Guard_SetTraceProvider( int ( *fn )( const char *cvarName, int defaultValue ) )
{
	vk_guardTrace = fn;
}

int VK_TraceLevel( const char *cvarName, int defaultValue )
{
	if ( vk_guardTrace ) {
		return vk_guardTrace( cvarName, defaultValue );
	}

	return defaultValue;
}

qboolean VK_TraceOn( const char *cvarName, int defaultValue )
{
	return ( VK_TraceLevel( cvarName, defaultValue ) != 0 ) ? qtrue : qfalse;
}

static void VK_Guard_Emit( const char *line )
{
	if ( vk_guardLogger ) {
		vk_guardLogger( line );
	} else {
		fprintf( stderr, "%s", line );
	}
}

void VK_Guard_Row( const char *tag, const char *fmt, ... )
{
	char line[1024];
	char body[896];
	va_list ap;

	va_start( ap, fmt );
	vsnprintf( body, sizeof( body ), fmt, ap );
	va_end( ap );
	body[sizeof( body ) - 1] = '\0';

	snprintf( line, sizeof( line ), "[%s] %s\n", tag, body );
	line[sizeof( line ) - 1] = '\0';

	VK_Guard_Emit( line );
}

const char *VK_Guard_ResultName( VkResult result )
{
	switch ( result ) {
	case VK_SUCCESS: return "success";
	case VK_NOT_READY: return "not-ready";
	case VK_TIMEOUT: return "timeout";
	case VK_EVENT_SET: return "event-set";
	case VK_EVENT_RESET: return "event-reset";
	case VK_INCOMPLETE: return "incomplete";
	case VK_ERROR_OUT_OF_HOST_MEMORY: return "out-of-host-memory";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "out-of-device-memory";
	case VK_ERROR_INITIALIZATION_FAILED: return "initialization-failed";
	case VK_ERROR_DEVICE_LOST: return "device-lost";
	case VK_ERROR_MEMORY_MAP_FAILED: return "memory-map-failed";
	case VK_ERROR_LAYER_NOT_PRESENT: return "layer-not-present";
	case VK_ERROR_EXTENSION_NOT_PRESENT: return "extension-not-present";
	case VK_ERROR_FEATURE_NOT_PRESENT: return "feature-not-present";
	case VK_ERROR_INCOMPATIBLE_DRIVER: return "incompatible-driver";
	case VK_ERROR_TOO_MANY_OBJECTS: return "too-many-objects";
	case VK_ERROR_FORMAT_NOT_SUPPORTED: return "format-not-supported";
	case VK_ERROR_FRAGMENTED_POOL: return "fragmented-pool";
	case VK_ERROR_SURFACE_LOST_KHR: return "surface-lost";
	case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "native-window-in-use";
	case VK_SUBOPTIMAL_KHR: return "suboptimal";
	case VK_ERROR_OUT_OF_DATE_KHR: return "out-of-date";
	case VK_ERROR_VALIDATION_FAILED_EXT: return "validation-failed";
	default: return "other";
	}
}

void VK_Guard_Hit( vkGuardSite_t site, const char *detail, VkResult result )
{
	unsigned int n;

	if ( (unsigned int)site >= (unsigned int)VK_GSITE_COUNT ) {
		return;
	}

	n = ++vk_guardSiteHits[site];

	/* A row with nothing after detail= cannot be triaged, so an unnamed one
	   says so instead of printing an empty field. */
	VK_Guard_Row( "VK:guard",
				 "site=%s detail=\"%s\" result=%s(%d) n=%u",
				 vk_guardSiteNames[site],
				 ( detail && detail[0] ) ? detail : "(no detail given)",
				 VK_Guard_ResultName( result ),
				 (int)result,
				 n );
}

qboolean VK_Guard_Result( vkGuardSite_t site, VkResult result, const char *what )
{
	if ( result == VK_SUCCESS ) {
		return qtrue;
	}

	VK_Guard_Hit( site, what, result );
	return qfalse;
}

static qboolean ( *vk_sentinelExists )( const char *path ) = NULL;
static qboolean ( *vk_sentinelWrite )( const char *path, const void *data, int size ) = NULL;
static qboolean ( *vk_sentinelRemove )( const char *path ) = NULL;

void VK_Sentinel_SetIO(
	qboolean ( *existsFn )( const char *path ),
	qboolean ( *writeFn )( const char *path, const void *data, int size ),
	qboolean ( *removeFn )( const char *path ) )
{
	vk_sentinelExists = existsFn;
	vk_sentinelWrite = writeFn;
	vk_sentinelRemove = removeFn;
}

qboolean VK_Sentinel_Present( void )
{
	if ( !vk_sentinelExists ) {
		return qfalse;
	}

	return vk_sentinelExists( VK_SENTINEL_FILE );
}

void VK_Sentinel_Arm( void )
{
	static const char marker[] = "1";

	if ( !vk_sentinelWrite ) {
		return;
	}

	if ( vk_sentinelWrite( VK_SENTINEL_FILE, marker, (int)sizeof( marker ) - 1 ) ) {
		VK_Guard_Row( "VK:sentinel", "phase=arm path=%s", VK_SENTINEL_FILE );
	} else {
		VK_Guard_Hit( VK_GSITE_SENTINEL, "arm write failed", VK_ERROR_INITIALIZATION_FAILED );
	}
}

void VK_Sentinel_Clear( void )
{
	if ( !vk_sentinelRemove ) {
		return;
	}

	if ( vk_sentinelRemove( VK_SENTINEL_FILE ) ) {
		VK_Guard_Row( "VK:sentinel", "phase=clear path=%s", VK_SENTINEL_FILE );
	} else {
		VK_Guard_Row( "VK:sentinel", "phase=clear-absent path=%s", VK_SENTINEL_FILE );
	}
}
