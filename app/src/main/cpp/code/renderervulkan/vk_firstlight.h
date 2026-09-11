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
#ifndef VK_VK_FIRSTLIGHT_H
#define VK_VK_FIRSTLIGHT_H

#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"

typedef const char *( *vkHomePathFn_t )( void );

void VK_SetHomePathFn( vkHomePathFn_t fn );
const char *VK_HomePath( void );

qboolean VK_BringUp( int windowWidth, int windowHeight,
					 const char *const *surfaceExtensions, int numSurfaceExtensions );
void VK_BringDown( void );

int VK_FirstLight_Ready( void );

#ifdef __cplusplus
}
#endif

#endif
