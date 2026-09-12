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
Copyright (C) 2024 the OpenMoHAA team

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


#include "tr_local.h"
#include "../corepp/tiki.h"

#define MAX_STATIC_MODELS_SURFS    8192
#define MAX_DISTINCT_STATIC_MODELS 1000

int             g_nStaticSurfaces;
staticSurface_t g_staticSurfaces[MAX_STATIC_MODELS_SURFS];
qboolean        g_bInfostaticmodels = qfalse;

void R_InitStaticModels(void)
{
    cStaticModelUnpacked_t *pSM;
    char                    szTemp[1024];
    skelBoneCache_t         bones[128];
    float                   radius;
    int                     i, j, k, l;

    g_bInfostaticmodels = qfalse;

    if (tr.overbrightShift) {
        for (i = 0; i < tr.world->numStaticModelData; i++) {
            int r, g, b;

            r = (int)((float)tr.world->staticModelData[i * 4] * tr.overbrightMult);
            g = (int)((float)tr.world->staticModelData[i * 4 + 1] * tr.overbrightMult);
            b = (int)((float)tr.world->staticModelData[i * 4 + 2] * tr.overbrightMult);

            if (r > 0xFF || g > 0xFF || b > 0xFF) {
                float t;

                t = 255.0 / (float)Q_max(r, Q_max(g, b));

                r = (int)((float)r * t);
                g = (int)((float)g * t);
                b = (int)((float)b * t);
            }

            tr.world->staticModelData[i * 4]     = r;
            tr.world->staticModelData[i * 4 + 1] = g;
            tr.world->staticModelData[i * 4 + 2] = b;
        }
    }

    for (i = 0; i < tr.world->numStaticModels; i++) {
        vec3_t mins, maxs;

        pSM = &tr.world->staticModels[i];

        pSM->bRendered = qfalse;
        AngleVectorsLeft(pSM->angles, pSM->axis[0], pSM->axis[1], pSM->axis[2]);

        if (!strnicmp(pSM->model, "models", 6)) {
            Q_strncpyz(szTemp, pSM->model, sizeof(szTemp));
        } else {
            Com_sprintf(szTemp, sizeof(szTemp), "models/%s", pSM->model);
        }

        ri.FS_CanonicalFilename(szTemp);
        pSM->tiki = ri.TIKI_RegisterTikiFlags(szTemp, qfalse);

        if (!pSM->tiki) {
            ri.Printf(PRINT_WARNING, "^~^~^: Warning: Cannot Load Static Model %s\n", szTemp);
            continue;
        }

        pSM->radius = ri.TIKI_GlobalRadius(pSM->tiki);

        for (j = 0; j < pSM->tiki->num_surfaces; j++) {
            dtikisurface_t *surf = &pSM->tiki->surfaces[j];

            for (k = 0; k < surf->numskins; k++) {
                if (surf->shader[k][0]) {
                    shader_t *sh = R_FindShader(
                        surf->shader[k], -1, !(surf->flags & TIKI_SURF_NOMIPMAPS), r_picmip->integer, qtrue, qtrue
                    );
                    surf->hShader[k] = sh->index;
                } else {
                    surf->hShader[k] = 0;
                }
            }
        }

        if (!ri.TIKI_SkelBonesFromRecords(
                pSM->tiki,
                0,
                0,
                0,
                0,
                0.0f,
                -1,
                -1,
                -1.0f,
                0,
                0,
                0,
                0,
                0.0f,
                -1,
                -1,
                -1.0f,
                qtrue,
                qfalse,
                NULL,
                NULL,
                vec3_origin,
                bones,
                MAX_SKELBONES,
                &radius,
                &mins,
                &maxs
            )) {
            ri.Printf(PRINT_WARNING, "^~^~^: Warning: Cannot pose static model %s\n", szTemp);
            continue;
        }
        pSM->cull_radius = radius * pSM->tiki->load_scale * pSM->scale;



        for (j = 0; j < pSM->tiki->numMeshes; j++) {
            skelHeaderGame_t  *skelmodel = ri.TIKI_GetSkel(pSM->tiki->mesh[j]);
            skelSurfaceGame_t *surf;

            if (!skelmodel) {
                ri.Printf(PRINT_WARNING, "^~^~^: Warning: Missing mesh in Static Model %s\n", skelmodel->name);
                continue;
            }

            surf = skelmodel->pSurfaces;

            for (k = 0; k < skelmodel->numSurfaces; k++, surf = surf->pNext) {
                byte             *buf;
                byte             *p;
                skelWeight_t     *weight;
                skeletorVertex_t *vert;

                if (surf->pStaticXyz) {
                    continue;
                }

                p = buf =
                    (byte *)ri.TIKI_Alloc((sizeof(vec4_t) + sizeof(vec4_t) + sizeof(vec2_t) * 2) * surf->numVerts);
                surf->pStaticXyz = (vec4_t *)p;
                p += sizeof(vec4_t) * surf->numVerts;
                surf->pStaticNormal = (vec4_t *)p;
                p += sizeof(vec4_t) * surf->numVerts;
                surf->pStaticTexCoords = (vec2_t(*)[2])p;

                vert = surf->pVerts;

                for (l = 0; l < surf->numVerts; l++) {
                    int              channel;
                    skelBoneCache_t *bone;

                    weight = (skelWeight_t *)((byte *)vert + sizeof(skeletorVertex_t)
                                              + vert->numMorphs * sizeof(skeletorMorph_t));

                    if (j > 0) {
                        channel = ri.TIKI_GetLocalChannel(pSM->tiki, skelmodel->pBones[weight->boneIndex].channel);
                    } else {
                        channel = weight->boneIndex;
                    }

                    bone = &bones[channel];

                    surf->pStaticXyz[l][0] =
                        ((weight->offset[0] * bone->matrix[0][0] + weight->offset[1] * bone->matrix[1][0]
                          + weight->offset[2] * bone->matrix[2][0])
                         + bone->offset[0])
                        * weight->boneWeight;
                    surf->pStaticXyz[l][1] =
                        ((weight->offset[0] * bone->matrix[0][1] + weight->offset[1] * bone->matrix[1][1]
                          + weight->offset[2] * bone->matrix[2][1])
                         + bone->offset[1])
                        * weight->boneWeight;
                    surf->pStaticXyz[l][2] =
                        ((weight->offset[0] * bone->matrix[0][2] + weight->offset[1] * bone->matrix[1][2]
                          + weight->offset[2] * bone->matrix[2][2])
                         + bone->offset[2])
                        * weight->boneWeight;
                    surf->pStaticXyz[l][3] = 0.f;

                    surf->pStaticNormal[l][0] = vert->normal[0] * bone->matrix[0][0]
                                              + vert->normal[1] * bone->matrix[1][0]
                                              + vert->normal[2] * bone->matrix[2][0];
                    surf->pStaticNormal[l][1] = vert->normal[0] * bone->matrix[0][1]
                                              + vert->normal[1] * bone->matrix[1][1]
                                              + vert->normal[2] * bone->matrix[2][1];
                    surf->pStaticNormal[l][2] = vert->normal[0] * bone->matrix[0][2]
                                              + vert->normal[1] * bone->matrix[1][2]
                                              + vert->normal[2] * bone->matrix[2][2];
                    surf->pStaticNormal[l][3] = 0.f;

                    surf->pStaticTexCoords[l][0][0] = vert->texCoords[0];
                    surf->pStaticTexCoords[l][0][1] = vert->texCoords[1];
                    surf->pStaticTexCoords[l][1][0] = vert->texCoords[0];
                    surf->pStaticTexCoords[l][1][1] = vert->texCoords[1];

                    vert = (skeletorVertex_t *)((byte *)vert + sizeof(skeletorVertex_t)
                                                + sizeof(skeletorMorph_t) * vert->numMorphs
                                                + sizeof(skelWeight_t) * vert->numWeights);
                }
            }
        }
    }

    tr.refdef.numStaticModels    = tr.world->numStaticModels;
    tr.refdef.staticModels       = tr.world->staticModels;
    tr.refdef.numStaticModelData = tr.world->numStaticModelData;
    tr.refdef.staticModelData    = tr.world->staticModelData;
}

void RB_Static_BuildDLights()
{
    int i;

    backEnd.currentStaticModel->useSpecialLighting = backEnd.refdef.num_dlights > 0;
    backEnd.currentStaticModel->numdlights         = 0;

    if (!backEnd.currentStaticModel->useSpecialLighting) {
        return;
    }

    for (i = 0; i < backEnd.refdef.num_dlights && backEnd.currentStaticModel->numdlights != 32; i++) {
        vec3_t lightorigin, delta;

        VectorCopy(backEnd.refdef.dlights[i].origin, lightorigin);
        VectorSubtract(lightorigin, backEnd.currentStaticModel->origin, delta);
        if (backEnd.refdef.dlights[i].radius * 2.0 >= VectorLength(delta)) {
            MatrixTransformVectorRight(
                backEnd.currentStaticModel->axis,
                delta,
                backEnd.currentStaticModel->dlights[backEnd.currentStaticModel->numdlights].transformed
            );

            backEnd.currentStaticModel->dlights[backEnd.currentStaticModel->numdlights++].index = i;
        }
    }

    if (!backEnd.currentStaticModel->numdlights) {
        backEnd.currentStaticModel->useSpecialLighting = qfalse;
    }
}

