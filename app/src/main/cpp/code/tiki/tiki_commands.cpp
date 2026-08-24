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
Foundation, Inc., 51 Franklin St, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later


#include "q_shared.h"
#include "qcommon.h"
#include "../skeletor/skeletor.h"
#include "tiki_cache.h"
#include "tiki_files.h"
#include "tiki_skan.h"
#include "tiki_skel.h"
#include "tiki_commands.h"

void TIKI_ModelInfo(dtiki_t *tiki)
{
    skelHeaderGame_t *skelmodel;
    int               i, n;
    int               totalanimsSize;
    int               totalTriangles;
    int               totalFrames;

    if (!tiki) {
        Com_WPrintf("ModelInfo: model not found.\n");
        return;
    }

    Com_Printf("MODEL: %s\n", tiki->name);

    if (tiki->numMeshes <= 0) {
        Com_WPrintf("ModelInfo: NULL skelmodel.\n");
        return;
    }

    skelmodel = TIKI_GetSkel(tiki->mesh[0]);
    if (!skelmodel) {
        Com_WPrintf("ModelInfo: NULL skelmodel.\n");
        return;
    }

    totalanimsSize = 0;
    totalFrames    = 0;

    Com_Printf("Total Anims: %i\n", tiki->num_anims);
    for (i = 0; i < tiki->num_anims; i++) {
        skelAnimDataGameHeader_t *animData;
        skelAnimHeaderRec_t      *skanHdr;

        if (!tiki->a || i >= tiki->a->num_anims || !tiki->a->animdefs[i]) {
            continue;
        }

        animData = SkeletorCacheGetData(tiki->a->m_aliases[i]);
        if (!animData) {
            Com_WPrintf("ModelInfo: couldn't find anim %d\n", i);
            continue;
        }

        skanHdr = TIKI_Skan_FindByAnim(tiki, i);

        Com_Printf(
            "   %s -> %s : Frames %d Time %.2f Size %i Delta %.2f %.2f %.2f\n",
            tiki->a->animdefs[i]->alias,
            skanHdr ? skanHdr->name : tiki->a->animdefs[i]->alias,
            skanHdr ? skanHdr->numFrames : animData->numFrames,
            skanHdr ? skanHdr->totaltime : animData->frameTime,
            (int)animData->nBytesUsed,
            animData->totalDelta[0],
            animData->totalDelta[1],
            animData->totalDelta[2]
        );

        totalanimsSize += (int)animData->nBytesUsed;
        totalFrames += animData->numFrames;
    }

    Com_Printf("Surfaces:  %i\n", tiki->num_surfaces);
    totalTriangles = 0;
    for (i = 0; i < tiki->num_surfaces; i++) {
        dtikisurface_t *surf = &tiki->surfaces[i];

        Com_Printf("  Surface %s Triangles %i skins", surf->name, skelmodel->pSurfaces[i].numTriangles);
        for (n = 0; n < surf->numskins; n++) {
            Com_Printf(" %s", surf->shader[n]);
        }
        Com_Printf("\n");

        totalTriangles += skelmodel->pSurfaces[i].numTriangles;
    }

    Com_Printf("Bones:  %i\n", skelmodel->numBones);
    for (i = 0; i < skelmodel->numBones; i++) {
        // boneData_t carries the parent channel but not the name -- retail reads
        Com_Printf("  %d : %s -> %d\n", i, TIKI_Tag_NumToName(tiki, i), (int)skelmodel->pBones[i].parent);
    }

    Com_Printf("\n");
    Com_Printf("Total Triangles:  %i\n", totalTriangles);
    Com_Printf("\n");
    Com_Printf("Total Anims Size: %i\n", totalanimsSize);
    Com_Printf("\n");
    Com_Printf("Total Number of Frames: %i\n", totalFrames);
}
