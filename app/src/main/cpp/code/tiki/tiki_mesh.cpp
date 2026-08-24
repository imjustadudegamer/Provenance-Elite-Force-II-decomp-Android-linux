/*
===========================================================================
Copyright (C) 2023 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later


#include "../corepp/tiki.h"
#include <assert.h>

int             TIKI_Skel_Bones_Cursor[TIKI_BONE_ARENAS];
int             skeletorMorphCacheIndex;
skelBoneCache_t TIKI_Skel_Bones_Arena[TIKI_BONE_ARENAS][MAX_SKELBONES];
int             skeletorMorphCache[MAX_SKELMORPH];

static_assert(MAX_SKELBONES == TIKI_BONE_CACHE, "MAX_SKELBONES must be retail's TIKI_BONE_CACHE");
static_assert(
    sizeof(TIKI_Skel_Bones_Arena[0]) / sizeof(TIKI_Skel_Bones_Arena[0][0]) == TIKI_BONE_CACHE,
    "each bone arena must hold exactly retail's TIKI_BONE_CACHE rows"
);

void TIKI_Reset_Caches_Arena(int arena)
{
    assert(arena >= 0 && arena < TIKI_BONE_ARENAS);

    TIKI_Skel_Bones_Cursor[arena] = 0;
    skeletorMorphCacheIndex       = 0;
}

void TIKI_Reset_Caches()
{
    TIKI_Reset_Caches_Arena(0);
}
