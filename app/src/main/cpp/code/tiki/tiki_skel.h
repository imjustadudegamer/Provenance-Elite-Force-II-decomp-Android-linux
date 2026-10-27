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

    void              TIKI_CacheFileSkel(skelHeader_t *pHeader, skelcache_t *cache, int length);
    void              TIKI_CalcLodConsts(lodControl_t *LOD);
    qboolean          TIKI_LoadSKB(const char *path, skelcache_t *cache);
    qboolean          TIKI_LoadSKD(const char *path, skelcache_t *cache);
    qboolean          TIKI_LoadTAN(const char *path, skelcache_t *cache);
    skelHeaderGame_t *TIKI_GetSkel(int index);
    int               TIKI_GetSkelCache(skelHeaderGame_t *pSkel);
    void              TIKI_FreeSkel(int index);
    void              TIKI_FreeSkelCache(skelcache_t *cache);
    skelcache_t      *TIKI_FindSkel(const char *path);
    skelcache_t      *TIKI_FindSkelByHeader(skelHeaderGame_t *skelmodel);
    skelcache_t      *TIKI_FindFreeSkel(void);
    int               TIKI_RegisterSkel(const char *path, dtiki_t *tiki);
    skelMorphHeader_t *TIKI_GetMorph(int index);
    void              TIKI_LoadSKBMorph(skelHeaderGame_t *pSkel, const char *morphFileName);
    int               TIKI_Morph_NumForName(dtiki_t *tiki, const char *name);

#ifdef __cplusplus
}
#endif

    int  TIKI_CalcBoneMapping(int *map, skelHeaderGame_t *from, skelHeaderGame_t *to);
    int  TIKI_RegisterBoneMapping(int fromIndex, int toIndex);
    int *TIKI_GetBoneMapping(int handle);
    void TIKI_ReleaseBoneMapping(int handle);

    int  TIKI_CalcMorphMapping(int *map, skelHeaderGame_t *from, skelHeaderGame_t *to);
    int  TIKI_RegisterMorphMapping(int fromIndex, int toIndex);
    int *TIKI_GetMorphMapping(int handle);
    void TIKI_ReleaseMorphMapping(int handle);

    int  TIKI_CalcAnimBoneMapping(int *map, skelAnimHeader_t *anim, skelHeaderGame_t *skel);
    /* retail answers -1 for four different reasons; the verdict names which one. */
#define TIKI_MAPV_HANDLE     0
#define TIKI_MAPV_IDENTITY   1
#define TIKI_MAPV_NOSKEL     2
#define TIKI_MAPV_TOOMANY    3
#define TIKI_MAPV_TABLEFULL  4
    int  TIKI_RegisterAnimBoneMappingV(skelAnimHeader_t *anim, int skelIndex, int *verdict);
    int  TIKI_RegisterAnimBoneMapping(skelAnimHeader_t *anim, int skelIndex);
    struct animBoneMapping_s *TIKI_FindAnimBoneMapping(
        skelAnimHeader_t *anim, int checksum, skelHeaderGame_t *skel);
    int *TIKI_GetAnimBoneMapping(int handle);
    void TIKI_ReleaseAnimBoneMapping(int handle);
    void TIKI_FreeAnimBoneMapping(int handle);
