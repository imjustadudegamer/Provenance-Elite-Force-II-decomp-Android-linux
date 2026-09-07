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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define TIKI_BONE_CACHE 8000
#define MAX_SKELBONES   TIKI_BONE_CACHE
#define MAX_SKELMORPH 12800

/* Two arenas, because the pose indexes them by front-end frame parity.  The pose
 * writes TIKI_Skel_Bones_Arena[tr.smpFrame] (tr_model.cpp) and the back end reads
 * TIKI_Skel_Bones_Arena[RB_FrameBoneArena()] (tr_model.cpp:1611), and smpFrame
 * alternates 0/1 (tr_scene.c:49) -- so one arena is the double-buffer bug: every
 * odd frame posed a full arena MAX_SKELBONES*sizeof(skelBoneCache_t) past the
 * array and read its cursor out of the first bone cache's own bytes.  That is the
 * third occurrence of the menu-3d flat-blue family (r134-menu3d R1, ledger
 * \S11.2), this time with the reset present and landing out of bounds. */
#ifndef TIKI_BONE_ARENAS
#define TIKI_BONE_ARENAS 2
#endif

    extern int             TIKI_Skel_Bones_Cursor[TIKI_BONE_ARENAS];
    extern int             skeletorMorphCacheIndex;
    extern skelBoneCache_t TIKI_Skel_Bones_Arena[TIKI_BONE_ARENAS][MAX_SKELBONES];
    extern int             skeletorMorphCache[];

#define TIKI_Skel_Bones         ( TIKI_Skel_Bones_Arena[ 0 ] )
#define TIKI_Skel_Bones_Index   ( TIKI_Skel_Bones_Cursor[ 0 ] )

    void TIKI_Reset_Caches(void);

    void TIKI_Reset_Caches_Arena(int arena);

#ifdef __cplusplus
}
#endif
