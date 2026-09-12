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
#ifndef VK_VK_GUARD_H
#define VK_VK_GUARD_H

#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"

typedef enum {
	VK_GSITE_LOADER = 0,
	VK_GSITE_INSTANCE,
	VK_GSITE_DEVICE,
	VK_GSITE_SURFACE,
	VK_GSITE_SWAPCHAIN_CREATE,
	VK_GSITE_SWAPCHAIN_RECREATE,
	VK_GSITE_SWAPCHAIN_LOST,
	VK_GSITE_SWAPCHAIN_OUT_OF_DATE,
	VK_GSITE_SWAPCHAIN_SUBOPTIMAL,
	VK_GSITE_ACQUIRE,
	VK_GSITE_PRESENT,
	VK_GSITE_SUBMIT,
	VK_GSITE_FENCE,
	VK_GSITE_SEMAPHORE,
	VK_GSITE_ALLOC,
	VK_GSITE_DESCRIPTOR,
	VK_GSITE_SYNC,
	VK_GSITE_DEVICE_LOST,
	VK_GSITE_QUIRK,
	VK_GSITE_SENTINEL,
	VK_GSITE_COUNT
} vkGuardSite_t;

void VK_Guard_SetLogger( void ( *fn )( const char *line ) );
void VK_Guard_SetTraceProvider( int ( *fn )( const char *cvarName, int defaultValue ) );

int VK_TraceLevel( const char *cvarName, int defaultValue );
qboolean VK_TraceOn( const char *cvarName, int defaultValue );

void VK_Guard_Hit( vkGuardSite_t site, const char *detail, VkResult result );
qboolean VK_Guard_Result( vkGuardSite_t site, VkResult result, const char *what );
void VK_Guard_Row( const char *tag, const char *fmt, ... ) __attribute__((format(printf, 2, 3)));

const char *VK_Guard_ResultName( VkResult result );

#define VK_SENTINEL_FILE "vk_safe_mode"
#define VK_PCACHE_FILE "vk_pipeline_cache.bin"

void VK_Sentinel_SetIO(
	qboolean ( *existsFn )( const char *path ),
	qboolean ( *writeFn )( const char *path, const void *data, int size ),
	qboolean ( *removeFn )( const char *path ) );

qboolean VK_Sentinel_Present( void );
void VK_Sentinel_Arm( void );
void VK_Sentinel_Clear( void );

#ifdef __cplusplus
}
#endif

#endif
