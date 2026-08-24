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

#include "../corepp/tiki.h"

#ifdef __cplusplus
extern "C" {
#endif

    extern qboolean tiki_loading;

    void         TIKI_FreeStorage(dloaddef_t *ld);
    void        *TIKI_AllocateLoadData(size_t length);
    qboolean     TIKI_AliasExists(dloaddef_t* ld, const char* name);
    char        *TIKI_CopyString(const char *s);
    dtikianim_t *TIKI_LoadTikiAnim(const char *path);

#ifdef __cplusplus
    dtiki_t *TIKI_LoadTikiModel(dtikianim_t *tikianim, const char *name, size_t *defSizeOut = NULL);
#endif

    void         TIKI_CalcRadius(dtiki_t *tiki);
    void                TIKI_CopyExpressions(dtiki_t *tiki, const dtikianim_t *tikianim);
    dtikimorphtarget_t *TIKI_GetExpression(dtiki_t *tiki, const char *expression_name, int *number_of_morph_targets);
    skelAnimDataGameHeader_t *SkeletorCacheFileCallback(const char *path);
    skelAnimDataGameHeader_t *SkeletorCacheGetData(int index);

    int TIKI_RegisterAnim(const char *path, qboolean use);
    void TIKI_CacheAnim(const char *path);

#ifdef __cplusplus
    bool SkeletorCacheFindFilename(const char *path, int *indexPtr);
    bool SkeletorCacheLoadData(const char *path, bool precache, int newIndex);
    void SkeletorCacheUnloadData(int index);
    void SkeletorCacheCleanCache();
    void TikiAddToBounds(dtikianim_t *tiki, SkelVec3 *newBounds);
#endif

    void TIKI_AnimList_f(void);
    dtikianim_t *TIKI_InitTiki(dloaddef_t *ld, size_t defsize);
    void         TIKI_RemoveTiki(dtikianim_t *ptiki);

    skelHeaderGame_t *TIKI_GetBaseSkel(dtiki_t *tiki);

    int         SkeletorCacheNumEntries(void);
    const char *SkeletorCacheEntryPath(int index);
    qboolean    SkeletorCacheEntryLoaded(int index);
    int         SkeletorCacheEntrySize(int index);

#ifdef __cplusplus
}
#endif
