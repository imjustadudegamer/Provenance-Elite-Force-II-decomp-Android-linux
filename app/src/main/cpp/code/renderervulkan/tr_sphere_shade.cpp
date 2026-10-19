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
#include "tr_program.h"

#include "../tiki/tiki_shared.h"

suninfo_t       s_sun;
static vec3_t   ambientlight;
static int      light_reference_count = 0;
static int      s_exclusiveDropped;

#define MAX_SPHERE_FLASH_TRACK 512

vec3_t spheredef[SPHERE_NUM_BASIS] = {
    { 1.0f,  0.0f,  0.0f},
    {-1.0f,  0.0f,  0.0f},
    { 0.0f,  1.0f,  0.0f},
    { 0.0f, -1.0f,  0.0f},
    { 0.0f,  0.0f,  1.0f},
    { 0.0f,  0.0f, -1.0f}
};

static vec3_t sphereleafoffsets[9] = {
    { 0.0f,  0.0f,  0.0f},
    { 0.0f,  0.0f,  1.0f},
    { 1.0f,  0.0f,  0.0f},
    { 0.0f,  1.0f,  0.0f},
    {-1.0f,  0.0f,  0.0f},
    { 0.0f, -1.0f,  0.0f},
    { 1.0f,  1.0f,  0.0f},
    {-1.0f, -1.0f,  0.0f},
    { 0.0f,  0.0f, -1.0f}
};

static void RB_Sphere_DrawDebugLine(const spherel_t *thislight, float fIntensity, const vec3_t vOrigin)
{
    int    i;
    vec3_t newColor;
    vec3_t fakeLine, fakeLine2;

    if (!r_light_lines->integer) {
        return;
    }

    RB_PushModelView();
    RB_LoadModelViewMatrix(backEnd.ori.modelMatrix);
    vkDepthRange(0.0, 0.0);
    VK_BeginWire();

    if (r_light_lines->integer == 2) {
        newColor[0] = thislight->color[0] * fIntensity;
        newColor[1] = thislight->color[1] * fIntensity;
        newColor[2] = thislight->color[2] * fIntensity;

        if (newColor[0] > 1 || newColor[1] > 1 || newColor[2] > 1) {
            NormalizeColor(newColor, newColor);
        }
    } else if (thislight->type == SPHEREL_SPOT) {
        newColor[0] = 1.0;
        newColor[1] = 0.7;
        newColor[2] = 0.7;
    } else {
        newColor[0] = 1.0;
        newColor[1] = 1.0;
        newColor[2] = 1.0;
    }

    RB_ImmColor3f(newColor[0], newColor[1], newColor[2]);

    RB_ImmBegin(VK_LINES);
    RB_ImmVertex3fv(vOrigin);
    RB_ImmVertex3fv(backEnd.currentSphere->origin);

    for (i = 0; i < 5; i += 2) {
        VectorMA(vOrigin, 16.0, spheredef[i], fakeLine);
        VectorMA(vOrigin, -16.0, spheredef[i], fakeLine2);

        RB_ImmVertex3fv(fakeLine);
        RB_ImmVertex3fv(fakeLine2);
    }

    if (thislight->type == SPHEREL_SPOT) {
        RB_ImmColor3f(0.7, 1.0, 0.7);

        MatrixTransformVectorRight(backEnd.currentEntity->e.axis, thislight->targetNormal, fakeLine);
        VectorMA(vOrigin, 32.0, fakeLine, fakeLine);

        RB_ImmVertex3fv(vOrigin);
        RB_ImmVertex3fv(fakeLine);
    }

    RB_ImmEnd();
    vkDepthRange(0.0, 1.0);
    RB_PopModelView();
}

static void R_Sphere_AccumulateBasis(const vec3_t vColor, float fScale, const vec3_t vDirModel)
{
    int    i;
    float  fWeight;
    vec3_t *pBucket = backEnd.currentSphere->pointcolor;

    for (i = 0; i < SPHERE_NUM_BASIS; i++) {
        fWeight = fScale * DotProduct(vDirModel, spheredef[i]);
        if (fWeight > 0.0f) {
            pBucket[i][0] += vColor[0] * fWeight;
            pBucket[i][1] += vColor[1] * fWeight;
            pBucket[i][2] += vColor[2] * fWeight;
        }
    }
}

/* the sky trace's verdict, for the DOCC:slight row */
static int g_sunSkyHit;

static void R_Sphere_Light_Sun(const vec3_t vDir, const vec3_t vColor)
{
    trace_t trace;
    vec3_t  vEnd;
    vec3_t  vDirModel;
    qboolean hitSun;

    VectorMA(backEnd.currentSphere->worldOrigin, 131072.0, vDir, vEnd);
    ri.CM_BoxTrace(
        &trace, backEnd.currentSphere->worldOrigin, vEnd, vec3_origin, vec3_origin, 0, CONTENTS_SOLID, 0
    );

    hitSun = (qboolean)((trace.surfaceFlags >> 2) & 1);
    g_sunSkyHit = hitSun ? 1 : 0;

    if (r_light_sun_line->integer) {
        vec3_t transpos;
        vec3_t temppos;

        VectorSubtract(trace.endpos, backEnd.currentEntity->e.origin, temppos);
        MatrixTransformVectorRight(backEnd.currentEntity->e.axis, temppos, transpos);

        VK_State(VKS_POLYMODE_LINE | VKS_DEPTHMASK_TRUE);
        RB_PushModelView();
        RB_LoadModelViewMatrix(backEnd.ori.modelMatrix);
        vkDepthRange(0.0, 0.0);
        VK_BeginWire();
        RB_ImmBegin(VK_LINES);

        if (hitSun) {
            RB_ImmColor3f(1.0, 1.0, 0.0);
        } else {
            RB_ImmColor3f(0.0, 0.0, 0.0);
        }

        RB_ImmVertex3fv(backEnd.currentSphere->origin);
        RB_ImmColor3f(0.5, 0.5, 0.5);
        RB_ImmVertex3fv(transpos);
        RB_ImmEnd();

        vkDepthRange(0.0, 1.0);
        RB_PopModelView();
    }

    if (!hitSun) {
        return;
    }

    MatrixTransformVectorRight(backEnd.currentEntity->e.axis, vDir, vDirModel);
    VectorNormalizeFast(vDirModel);

    R_Sphere_AccumulateBasis(vColor, 255.0f, vDirModel);
}

static void R_Sphere_AddNewPointOrSpotLight(const spherel_t *thislight, float fBrightness)
{
    vec3_t vDelta;
    vec3_t vDir;
    vec3_t vDirModel;
    vec3_t vDebug;
    float  fDistSquared;
    float  fDist;
    float  fIntensity;
    float  fDot;
    float  fFalloff;
    float  fT;

    VectorSubtract(thislight->origin, backEnd.currentSphere->worldOrigin, vDelta);
    fDistSquared = VectorLengthSquared(vDelta);

    if (fDistSquared == 0.0f) {
        VectorSet(vDelta, 0.0f, 0.0f, 1.0f);
        fDistSquared = 1.0f;
    }

    if (fDistSquared > thislight->falloff_end_dist * thislight->falloff_end_dist) {
        return;
    }

    fDist = sqrt(fDistSquared);
    VectorScale(vDelta, 1.0f / fDist, vDir);

    fIntensity = fBrightness;

    if (thislight->type == SPHEREL_SPOT) {
        fDot = DotProduct(vDir, thislight->targetNormal);

        if (fDot <= thislight->dotProduct_hotspot) {
            if (fDot >= thislight->dotProduct_penumbra) {
                fIntensity = fBrightness
                           * ((1.0f - thislight->spherical_ambient)
                                  * ((fDot - thislight->dotProduct_penumbra)
                                     / (thislight->dotProduct_hotspot - thislight->dotProduct_penumbra))
                              + thislight->spherical_ambient);
            } else {
                fIntensity = fBrightness * thislight->spherical_ambient;
            }
        }

        if (fIntensity >= -0.01f && fIntensity <= 0.01f) {
            return;
        }
    }

    if (fDist >= thislight->falloff_start_dist) {
        if (fDist <= thislight->falloff_end_dist) {
            fT = (fDist - thislight->falloff_start_dist)
               / (thislight->falloff_end_dist - thislight->falloff_start_dist);

            if (thislight->curveOffset == 0.0f) {
                fFalloff = 1.0f - fT;
            } else {
                fFalloff = (thislight->curveOffset * ((1.0f - fT) * thislight->curveOffset))
                         / ((fT + thislight->curveOffset) * (fT + thislight->curveOffset));
            }

            fIntensity = fIntensity * fFalloff;
        } else {
            fIntensity = 0.0f;
        }
    }

    if (fIntensity >= -0.01f && fIntensity <= 0.01f) {
        return;
    }

    vDirModel[0] = DotProduct(vDir, backEnd.currentEntity->e.axis[0]);
    vDirModel[1] = DotProduct(vDir, backEnd.currentEntity->e.axis[1]);
    vDirModel[2] = DotProduct(vDir, backEnd.currentEntity->e.axis[2]);

    R_Sphere_AccumulateBasis(thislight->color, 255.0f * fIntensity, vDirModel);

    VectorScale(vDirModel, fDist, vDebug);
    RB_Sphere_DrawDebugLine(thislight, fIntensity, vDebug);
}

/* DOCC:slight -- names the light whose verdict or brightness moved on a still entity */
static void R_Sphere_TrackLight(const spherel_t *thislight, float fBrightness, int iVerdict, int iSrc)
{
    static struct {
        int   frame;
        int   v;
        float br;
    } s_track[MAX_MAP_SPHERE_LIGHTS_EF2];
    static int s_rows;
    int        i;

    if (!R_ProbeMarkOn() || !thislight) {
        return;
    }

    i = (int)(thislight - tr.sLights);
    if (i < 0 || i >= MAX_MAP_SPHERE_LIGHTS_EF2) {
        return;
    }

    if (s_track[i].frame && s_track[i].frame != RB_FrameCount()
        && (s_track[i].v != iVerdict || fabs(fBrightness - s_track[i].br) > 0.01f)
        && (s_rows < 24 || (s_rows % 600) == 0)) {
        s_rows++;
        VK_Guard_Row(
            "DOCC:slight",
            "f=%d ent=%d li=%d src=%d typ=%d tr=%d v=%d->%d br=%.3f->%.3f grp=%d gi=%d "
            "org=(%.1f,%.1f,%.1f) col=(%.3f,%.3f,%.3f) fo=(%.1f,%.1f) wo=(%.2f,%.2f,%.2f) "
            "nl=%d sky=%d",
            RB_FrameCount(), backEnd.currentEntity->e.entityNumber, i, iSrc,
            thislight->type, thislight->needs_trace, s_track[i].v, iVerdict,
            s_track[i].br, fBrightness, thislight->group_num,
            (thislight->group_num != -1 && tr.world)
                ? backEnd.refdef.lightGroupIntensity[thislight->group_num & 0xFF] : -1,
            thislight->origin[0], thislight->origin[1], thislight->origin[2],
            thislight->color[0], thislight->color[1], thislight->color[2],
            thislight->falloff_start_dist, thislight->falloff_end_dist,
            backEnd.currentSphere->worldOrigin[0], backEnd.currentSphere->worldOrigin[1],
            backEnd.currentSphere->worldOrigin[2],
            backEnd.currentSphere->numLights, g_sunSkyHit
        );
    }

    s_track[i].frame = RB_FrameCount();
    s_track[i].br    = fBrightness;
    s_track[i].v     = iVerdict;
}

static void R_Sphere_AddNewLight(const spherel_t *thislight)
{
    trace_t trace;
    float   fBrightness;
    vec3_t  vColor;
    qboolean bVisible;
    qboolean bAllowed;

    if (thislight->needs_trace) {
        if (thislight->type == SPHEREL_SUN) {
            bVisible = qtrue;
        } else {
            qboolean bWorthTracing = qtrue;

            if (thislight->type == SPHEREL_POINT) {
                vec3_t vDelta;

                VectorSubtract(thislight->origin, backEnd.currentSphere->worldOrigin, vDelta);
                bWorthTracing = (qboolean)(VectorLengthSquared(vDelta)
                                           <= thislight->falloff_end_dist * thislight->falloff_end_dist);
            }

            if (bWorthTracing) {
                ri.CM_BoxTrace(
                    &trace,
                    backEnd.currentSphere->worldOrigin,
                    thislight->origin,
                    vec3_origin,
                    vec3_origin,
                    0,
                    CONTENTS_SOLID,
                    0
                );
                bVisible = (qboolean)(trace.fraction >= 1.0f);
            } else {
                bVisible = qfalse;
            }
        }
    } else {
        bVisible = qtrue;
    }

    if (!bVisible) {
        R_Sphere_TrackLight(thislight, 0.0f, 1, 0);
        return;
    }

    if (!backEnd.currentEntity->e.tiki) {
        R_Sphere_TrackLight(thislight, 0.0f, 2, 0);
        return;
    }

    if (thislight->exclusiveLightNameCount) {
        int i;
        const unsigned int uTikiHash = backEnd.currentEntity->e.tiki->exclusive_light_name_hash;

        bAllowed = qfalse;
        for (i = 0; i < thislight->exclusiveLightNameCount; i++) {
            if (thislight->exclusiveLightNameHashList[i] == uTikiHash) {
                bAllowed = qtrue;
                break;
            }
        }

        if (!bAllowed) {
            s_exclusiveDropped++;
            R_Sphere_TrackLight(thislight, 0.0f, 3, 0);
            return;
        }
    }

    fBrightness = thislight->brightness;
    if (thislight->group_num != -1 && tr.world) {
        fBrightness = fBrightness
                    * ((float)backEnd.refdef.lightGroupIntensity[thislight->group_num & 0xFF] / 255.0f);
    }

    if (fBrightness >= -0.01f && fBrightness <= 0.01f) {
        R_Sphere_TrackLight(thislight, fBrightness, 4, 0);
        return;
    }

    switch (thislight->type) {
    case SPHEREL_POINT:
    case SPHEREL_SPOT:
        R_Sphere_AddNewPointOrSpotLight(thislight, fBrightness);
        R_Sphere_TrackLight(thislight, fBrightness, 0, 0);
        break;
    case SPHEREL_SUN:
        VectorScale(thislight->color, fBrightness, vColor);
        R_Sphere_Light_Sun(thislight->targetNormal, vColor);
        R_Sphere_TrackLight(thislight, fBrightness, 0, 1);
        break;
    case SPHEREL_AREA:
        ri.Printf(PRINT_ALL, "R_Sphere_AddNewLight : area lights not yet supported\n");
        break;
    default:
        ri.Printf(
            PRINT_ALL,
            "R_Sphere_AddNewLight : bad light->type #%d for light at (%.0f, %.0f, %.0f)\n",
            thislight->type,
            thislight->origin[0],
            thislight->origin[1],
            thislight->origin[2]
        );
        break;
    }
}

static qboolean R_Sphere_CalculateSphereOrigin(void)
{
    refEntity_t *ent;
    dtiki_t     *tiki;
    skelFrame_t *nf;
    vec3_t       vBase;
    vec3_t       vLocal;
    float        fBound;
    float        fScale;

    ent = &backEnd.currentEntity->e;

    tiki = ent->tiki;
    if (!tiki) {
        return qfalse;
    }

    nf = ent->nf;
    if (!nf) {
        return qfalse;
    }

    fBound = (tiki->skelIndex >= 0) ? nf->radius : ((tikiFrame_t *)nf)->radius;

    backEnd.currentSphere->radius = tiki->load_scale * (fBound * ent->scale) * 0.5f;

    VectorAdd(nf->bounds[0], nf->bounds[1], vLocal);
    VectorScale(vLocal, 0.5f, vLocal);
    VectorAdd(vLocal, tiki->load_origin, vLocal);
    VectorAdd(vLocal, tiki->light_offset, vLocal);

    fScale = tiki->load_scale * ent->scale;
    VectorScale(vLocal, fScale, vLocal);

    VectorCopy(vLocal, backEnd.currentSphere->origin);

    if (ent->renderfx & RF_LIGHTING_ORIGIN) {
        VectorCopy(ent->lightingOrigin, vBase);
    } else {
        VectorCopy(ent->origin, vBase);
    }

    MatrixTransformVector(vLocal, ent->axis, backEnd.currentSphere->worldOrigin);
    VectorAdd(backEnd.currentSphere->worldOrigin, vBase, backEnd.currentSphere->worldOrigin);

    return qtrue;
}

static qboolean R_Sphere_SetupGlobals(void)
{
    int      i;
    int      nLeaves;
    vec3_t   vSample;
    mnode_t *leaf;

    if ((backEnd.refdef.rdflags & RDF_NOWORLDMODEL)
        || (backEnd.currentEntity && (backEnd.currentEntity->e.renderfx & RF_FULLBRIGHT))) {
        for (i = 0; i < SPHERE_NUM_BASIS; i++) {
            VectorSet(
                backEnd.currentSphere->pointcolor[i],
                (float)tr.identityLightByte,
                (float)tr.identityLightByte,
                (float)tr.identityLightByte
            );
        }
        VectorClear(backEnd.currentSphere->ambient);
        return qfalse;
    }

    if (!R_Sphere_CalculateSphereOrigin()) {
        return qfalse;
    }

    light_reference_count++;

    memset(backEnd.currentSphere->leaves, 0, sizeof(backEnd.currentSphere->leaves));
    nLeaves = 0;

    for (i = 0; i < 9; i++) {
        int j;

        VectorMA(backEnd.currentSphere->worldOrigin, backEnd.currentSphere->radius, sphereleafoffsets[i], vSample);
        leaf = R_PointInLeaf(vSample);

        if (leaf->area == -1) {
            continue;
        }

        for (j = 0; j < 4; j++) {
            if (backEnd.currentSphere->leaves[j] == leaf) {
                break;
            }
        }

        if (j == 4 && nLeaves < 4) {
            backEnd.currentSphere->leaves[nLeaves++] = leaf;
        }
    }

    backEnd.currentSphere->numLights   = 0;
    backEnd.currentSphere->TessFunction = &RB_Light_Sphere;
    return qtrue;
}

static qboolean R_Sphere_ResetPointColors(void)
{
    int      i;
    qboolean bHaveOffset = qfalse;
    vec3_t   vOffset;

    VectorClear(vOffset);

    if (backEnd.currentEntity) {
        if (backEnd.currentEntity->e.renderfx & RF_LIGHTOFFSET) {
            bHaveOffset = qtrue;
            vOffset[0]  = (float)backEnd.currentEntity->e.shaderRGBA[0];
            vOffset[1]  = (float)backEnd.currentEntity->e.shaderRGBA[1];
            vOffset[2]  = (float)backEnd.currentEntity->e.shaderRGBA[2];
        } else if (backEnd.currentEntity->e.renderfx & RF_MINLIGHT) {
            bHaveOffset = qtrue;
            vOffset[0] = vOffset[1] = vOffset[2] = 32.0f * tr.identityLight;
        }
    }

    VectorClear(backEnd.currentSphere->ambient);

    if (tr.numSLights && !r_fullbright->integer && backEnd.currentSphere->leaves[0]) {
        for (i = 0; i < SPHERE_NUM_BASIS; i++) {
            VectorAdd(ambientlight, vOffset, backEnd.currentSphere->pointcolor[i]);
        }
        return qtrue;
    }

    for (i = 0; i < SPHERE_NUM_BASIS; i++) {
        backEnd.currentSphere->pointcolor[i][0] = (float)tr.identityLightByte + (bHaveOffset ? vOffset[0] : 0.0f);
        backEnd.currentSphere->pointcolor[i][1] = (float)tr.identityLightByte + (bHaveOffset ? vOffset[1] : 0.0f);
        backEnd.currentSphere->pointcolor[i][2] = (float)tr.identityLightByte + (bHaveOffset ? vOffset[2] : 0.0f);
    }
    return qfalse;
}

static void R_Sphere_BuildStaticLights(spherel_t **pLightList)
{
    int i, j;

    if (tr.world->vis) {
        for (j = 0; j < 4; j++) {
            mnode_t *leaf = backEnd.currentSphere->leaves[j];
            if (!leaf) {
                break;
            }

            for (i = 0; i < leaf->numlights; i++) {
                spherel_t *pLight = leaf->lights[i];
                byte       mask;

                mask = backEnd.refdef.areamask[pLight->leaf->area >> 3];
                if (mask & (1 << (pLight->leaf->area & 7))) {
                    continue;
                }
                if (pLight->reference_count == light_reference_count) {
                    continue;
                }
                if (pLight->type == SPHEREL_SUN) {
                    continue;
                }

                pLightList[backEnd.currentSphere->numLights++] = pLight;
                pLight->reference_count                        = light_reference_count;
            }
        }

        for (i = 0; i < tr.numSLights; i++) {
            if (tr.sLights[i].type == SPHEREL_SUN) {
                pLightList[backEnd.currentSphere->numLights++] = &tr.sLights[i];
            }
        }
    } else {
        for (i = 0; i < tr.numSLights; i++) {
            pLightList[backEnd.currentSphere->numLights++] = &tr.sLights[i];
        }
    }
}

void R_Sphere_BuildDLights(spherel_t **pLightList)
{
    int i;

    backEnd.numExactDLights = 0;
    backEnd.bExactDLights   = (qboolean)(backEnd.refdef.num_dlights != 0);

    if (backEnd.refdef.num_dlights) {
        backEnd.currentSphere->TessFunction = &RB_Light_Sphere_ExactDLights;

        for (i = 0; i < backEnd.refdef.num_dlights && backEnd.numExactDLights != MAX_DLIGHTS; i++) {
            dlight_t *dl = &backEnd.refdef.dlights[i];
            vec3_t    vDelta;
            vec3_t    vLocal;

            VectorSubtract(dl->origin, backEnd.currentSphere->worldOrigin, vDelta);
            if (VectorLength(vDelta) > backEnd.currentSphere->radius + dl->radius) {
                continue;
            }

            VectorSubtract(dl->origin, backEnd.currentEntity->e.origin, vLocal);
            MatrixTransformVectorRight(
                backEnd.currentEntity->e.axis, vLocal, backEnd.exactDLights[backEnd.numExactDLights].transformed
            );
            backEnd.exactDLights[backEnd.numExactDLights].index = i;
            backEnd.numExactDLights++;
        }
    } else {
        backEnd.currentSphere->TessFunction = &RB_Light_Sphere;

        // dead in retail (num_dlights is 0 here by construction) -- kept for fidelity
        for (i = 0; i < backEnd.refdef.num_dlights; i++) {
            dlight_t  *dl     = &backEnd.refdef.dlights[i];
            spherel_t *pLight = &tr.sLights[tr.numSLights + i];

            VectorCopy(dl->origin, pLight->origin);
            VectorCopy(dl->color, pLight->color);
            pLight->leaf                   = R_PointInLeaf(dl->origin);
            pLight->needs_trace            = 0;
            pLight->type                   = SPHEREL_POINT;
            pLight->falloff_end_dist       = dl->radius;
            pLight->falloff_start_dist     = 0.5f * dl->radius;
            pLight->brightness             = 1.0f;
            pLight->curveOffset            = 0.8f;
            pLight->spherical_ambient      = 0.0f;
            pLight->dotProduct_hotspot     = 0.0f;
            pLight->dotProduct_penumbra    = 0.0f;
            pLight->exclusiveLightNameCount = 0;
            VectorSet(pLight->targetNormal, 0.0f, 0.0f, 1.0f);

            pLightList[backEnd.currentSphere->numLights++] = pLight;
        }
    }
}

static void EmphasizeSphereColors(void)
{
    int i;

    if (backEnd.refdef.rdflags & RDF_FULLBRIGHT) {
        for (i = 0; i < SPHERE_NUM_BASIS; i++) {
            VectorSet(backEnd.currentSphere->pointcolor[i], 255.0f, 255.0f, 255.0f);
        }
        return;
    }

    if (r_light_emphasizePercent->integer) {
        float fScale = 0.01f * r_light_emphasizePercent->value + 1.0f;

        for (i = 0; i < SPHERE_NUM_BASIS; i++) {
            VectorScale(backEnd.currentSphere->pointcolor[i], fScale, backEnd.currentSphere->pointcolor[i]);
        }
    }

    if (r_light_emphasize->integer > 1) {
        for (i = 0; i < SPHERE_NUM_BASIS; i++) {
            backEnd.currentSphere->pointcolor[i][0] += r_light_emphasize->value;
            backEnd.currentSphere->pointcolor[i][1] += r_light_emphasize->value;
            backEnd.currentSphere->pointcolor[i][2] += r_light_emphasize->value;
        }
    }
}

static void R_Sphere_NormalizeColors(void)
{
    int   i;
    float fMult;

    if (backEnd.bExactDLights) {
        return;
    }

    fMult = tr.overbrightMult;

    for (i = 0; i < SPHERE_NUM_BASIS; i++) {
        float *c = backEnd.currentSphere->pointcolor[i];

        if (fMult != 0.0f) {
            VectorScale(c, fMult, c);
        }

        if (c[0] > 255.0f || c[1] > 255.0f || c[2] > 255.0f) {
            NormalizeColor(c, c);
            VectorScale(c, 255.0f, c);
        }
    }
}

void R_Sphere_SetupEntity()
{
    int        i;
    float      fDist;
    vec3_t     vDelta;
    vec3_t     vLightDir;
    spherel_t *lightlist[MAX_MAP_SPHERE_LIGHTS_EF2];

    backEnd.currentSphere->TessFunction = &RB_Light_Sphere;

    if ((backEnd.currentEntity->e.renderfx & RF_FULLBRIGHT) || r_light_nolight->integer) {
        backEnd.currentSphere->TessFunction = &RB_Light_FullbrightSphere;
        return;
    }

    if (!R_Sphere_SetupGlobals()) {
        return;
    }
    s_exclusiveDropped = 0;
    if (!R_Sphere_ResetPointColors()) {
        return;
    }

    if (s_sun.exists) {
        R_Sphere_Light_Sun(s_sun.direction, s_sun.color);
    }

    R_Sphere_BuildStaticLights(lightlist);
    R_Sphere_BuildDLights(lightlist);

    backEnd.pc.c_characterlights += backEnd.currentSphere->numLights;
    for (i = 0; i < backEnd.currentSphere->numLights; i++) {
        R_Sphere_AddNewLight(lightlist[i]);
    }

    VectorClear(vLightDir);
    for (i = 0; i < SPHERE_NUM_BASIS; i++) {
        VectorMA(vLightDir, VectorLength(backEnd.currentSphere->pointcolor[i]), spheredef[i], vLightDir);
    }
    VectorNormalize(vLightDir);
    VectorCopy(vLightDir, backEnd.currentEntity->lightDir);

    EmphasizeSphereColors();
    R_Sphere_NormalizeColors();

    VectorSubtract(backEnd.currentEntity->e.origin, backEnd.refdef.vieworg, vDelta);
    fDist = VectorLength(vDelta);

    if ((backEnd.currentEntity->e.renderfx & RF_EXTRALIGHT) && !r_light_simple->integer
        && fDist <= r_light_simple_dist->value) {
        float fFudge = r_light_ambientFudge->value;

        VectorClear(backEnd.currentSphere->ambient);
        for (i = 0; i < SPHERE_NUM_BASIS; i++) {
            VectorAdd(backEnd.currentSphere->ambient, backEnd.currentSphere->pointcolor[i], backEnd.currentSphere->ambient);
            VectorScale(backEnd.currentSphere->pointcolor[i], 1.0f - fFudge, backEnd.currentSphere->pointcolor[i]);
        }

        VectorScale(backEnd.currentSphere->ambient, fFudge * 0.1666666716337204f, backEnd.currentSphere->ambient);
        VectorScale(
            backEnd.currentSphere->ambient, r_light_ambientEmphasize->value, backEnd.currentSphere->ambient
        );
    } else {
        vec3_t vAmbient;

        backEnd.currentSphere->TessFunction = &RB_CalcColorFromEntity;

        VectorClear(vAmbient);
        for (i = 0; i < SPHERE_NUM_BASIS; i++) {
            VectorAdd(vAmbient, backEnd.currentSphere->pointcolor[i], vAmbient);
        }

        VectorScale(vAmbient, 0.28333333134651184f, vAmbient);

        vAmbient[0] = Q_min(vAmbient[0], 255.0f);
        vAmbient[1] = Q_min(vAmbient[1], 255.0f);
        vAmbient[2] = Q_min(vAmbient[2], 255.0f);

        VectorCopy(vAmbient, backEnd.currentEntity->ambientLight);

        ((byte *)&backEnd.currentEntity->ambientLightInt)[0] = (byte)vAmbient[0];
        ((byte *)&backEnd.currentEntity->ambientLightInt)[1] = (byte)vAmbient[1];
        ((byte *)&backEnd.currentEntity->ambientLightInt)[2] = (byte)vAmbient[2];
        ((byte *)&backEnd.currentEntity->ambientLightInt)[3] = 0xFF;

        backEnd.currentEntity->e.shaderRGBA[0] = (byte)vAmbient[0];
        backEnd.currentEntity->e.shaderRGBA[1] = (byte)vAmbient[1];
        backEnd.currentEntity->e.shaderRGBA[2] = (byte)vAmbient[2];

        VectorClear(backEnd.currentEntity->directedLight);
    }

    /* an entity that has not moved whose resolved ambient changed names itself with the light set that produced it */
    if (R_ProbeMarkOn()) {
        static struct {
            int   frame;
            int   dl;
            float origin[3];
            float ambient[3];
            float dlor[3];
            float dlcol[3];
        } s_prev[MAX_SPHERE_FLASH_TRACK];
        int          iSlot = backEnd.currentEntity->e.entityNumber % MAX_SPHERE_FLASH_TRACK;
        unsigned int iLeaf, iLeafKey = 0, iLeafFilled = 0;

        for (iLeaf = 0; iLeaf < 4; iLeaf++) {
            iLeafKey = (iLeafKey * 33u)
                       + (unsigned int)(uintptr_t)backEnd.currentSphere->leaves[iLeaf];
            iLeafFilled += backEnd.currentSphere->leaves[iLeaf] != 0;
        }

        if (s_prev[iSlot].frame && s_prev[iSlot].frame != RB_FrameCount()) {
            float fMove = fabs(backEnd.currentEntity->e.origin[0] - s_prev[iSlot].origin[0])
                          + fabs(backEnd.currentEntity->e.origin[1] - s_prev[iSlot].origin[1])
                          + fabs(backEnd.currentEntity->e.origin[2] - s_prev[iSlot].origin[2]);
            float fDelta = fabs(backEnd.currentEntity->ambientLight[0] - s_prev[iSlot].ambient[0])
                           + fabs(backEnd.currentEntity->ambientLight[1] - s_prev[iSlot].ambient[1])
                           + fabs(backEnd.currentEntity->ambientLight[2] - s_prev[iSlot].ambient[2]);

            if (fMove < 4.0f && fDelta > 8.0f) {
                int iCur = backEnd.refdef.num_dlights - 1;

                VK_Guard_Row(
                    "DOCC:sphere",
                    "f=%d ent=%d move=%.2f ambient=(%.1f,%.1f,%.1f)->(%.1f,%.1f,%.1f) "
                    "d=%.1f nlights=%d sun=%d leaves=%08x/%u dl=%d->%d ex=%d ob=%.3f fb=%d exl=%d "
                    "ws=(%.0f,%.0f,%.0f) dlorg=(%.0f,%.0f,%.0f)->(%.0f,%.0f,%.0f) dlcol=(%.2f,%.2f,%.2f)",
                    RB_FrameCount(), backEnd.currentEntity->e.entityNumber, fMove,
                    s_prev[iSlot].ambient[0], s_prev[iSlot].ambient[1], s_prev[iSlot].ambient[2],
                    backEnd.currentEntity->ambientLight[0], backEnd.currentEntity->ambientLight[1],
                    backEnd.currentEntity->ambientLight[2], fDelta,
                    backEnd.currentSphere->numLights, tr.numSLights, iLeafKey, iLeafFilled,
                    s_prev[iSlot].dl, backEnd.refdef.num_dlights, backEnd.bExactDLights,
                    tr.overbrightMult, (backEnd.refdef.rdflags & RDF_FULLBRIGHT) ? 1 : 0,
                    s_exclusiveDropped,
                    ambientlight[0], ambientlight[1], ambientlight[2],
                    s_prev[iSlot].dlor[0], s_prev[iSlot].dlor[1], s_prev[iSlot].dlor[2],
                    ( iCur >= 0 ) ? backEnd.refdef.dlights[iCur].origin[0] : 0.0f,
                    ( iCur >= 0 ) ? backEnd.refdef.dlights[iCur].origin[1] : 0.0f,
                    ( iCur >= 0 ) ? backEnd.refdef.dlights[iCur].origin[2] : 0.0f,
                    ( iCur >= 0 ) ? backEnd.refdef.dlights[iCur].color[0] : 0.0f,
                    ( iCur >= 0 ) ? backEnd.refdef.dlights[iCur].color[1] : 0.0f,
                    ( iCur >= 0 ) ? backEnd.refdef.dlights[iCur].color[2] : 0.0f
                );
            }
        }

        s_prev[iSlot].frame = RB_FrameCount();
        s_prev[iSlot].dl    = backEnd.refdef.num_dlights;
        if ( backEnd.refdef.num_dlights > 0 ) {
            dlight_t *pDl = &backEnd.refdef.dlights[backEnd.refdef.num_dlights - 1];

            VectorCopy(pDl->origin, s_prev[iSlot].dlor);
            VectorCopy(pDl->color, s_prev[iSlot].dlcol);
        } else {
            VectorClear(s_prev[iSlot].dlor);
            VectorClear(s_prev[iSlot].dlcol);
        }
        VectorCopy(backEnd.currentEntity->e.origin, s_prev[iSlot].origin);
        VectorCopy(backEnd.currentEntity->ambientLight, s_prev[iSlot].ambient);
    }
}

void RB_Static_SetupModel()
{
    /* frame-carry: walk side reads the record's refdef */
    RB_SetupStaticModelLighting(
        &backEnd.refdef, backEnd.currentStaticModel, backEnd.currentStaticModel->origin
    );
}

void RB_Light_FullbrightSphere(unsigned char *colors)
{
    int i;

    for (i = 0; i < tess.numVertexes; i++) {
        colors[i * 4]     = 0xFF;
        colors[i * 4 + 1] = 0xFF;
        colors[i * 4 + 2] = 0xFF;
        colors[i * 4 + 3] = 0xFF;
    }
}

void RB_Light_Sphere(unsigned char *colors)
{
    int    i;
    float *normal = tess.normal[0];
    vec3_t *pBucket = backEnd.currentSphere->pointcolor;
    float  *pAmbient = backEnd.currentSphere->ambient;

    for (i = 0; i < tess.numVertexes; i++, normal += 4, colors += 4) {
        const float *bx = pBucket[normal[0] < 0.0f];
        const float *by = pBucket[2 + (normal[1] < 0.0f)];
        const float *bz = pBucket[4 + (normal[2] < 0.0f)];
        float        ax = fabs(normal[0]);
        float        ay = fabs(normal[1]);
        float        az = fabs(normal[2]);
        float        r  = pAmbient[0] + ax * bx[0] + ay * by[0] + az * bz[0];
        float        g  = pAmbient[1] + ax * bx[1] + ay * by[1] + az * bz[1];
        float        b  = pAmbient[2] + ax * bx[2] + ay * by[2] + az * bz[2];

        colors[0] = (unsigned char)(int)Q_min(r, 255.0f);
        colors[1] = (unsigned char)(int)Q_min(g, 255.0f);
        colors[2] = (unsigned char)(int)Q_min(b, 255.0f);
        colors[3] = 0xFF;
    }
}

void RB_Light_Sphere_ExactDLights(unsigned char *colors)
{
    int    i, j;
    float *normal   = tess.normal[0];
    float *xyz      = tess.xyz[0];
    vec3_t *pBucket = backEnd.currentSphere->pointcolor;
    float  *pAmbient = backEnd.currentSphere->ambient;
    float   fOverbright = tr.overbrightMult;

    for (i = 0; i < tess.numVertexes; i++, normal += 4, xyz += 4, colors += 4) {
        const float *bx = pBucket[normal[0] < 0.0f];
        const float *by = pBucket[2 + (normal[1] < 0.0f)];
        const float *bz = pBucket[4 + (normal[2] < 0.0f)];
        float        ax = fabs(normal[0]);
        float        ay = fabs(normal[1]);
        float        az = fabs(normal[2]);
        int          r, g, b;
        float        fr, fg, fb;

        fr = Q_min(pAmbient[0] + ax * bx[0] + ay * by[0] + az * bz[0], 255.0f);
        fg = Q_min(pAmbient[1] + ax * bx[1] + ay * by[1] + az * bz[1], 255.0f);
        fb = Q_min(pAmbient[2] + ax * bx[2] + ay * by[2] + az * bz[2], 255.0f);

        r = (int)fr;
        g = (int)fg;
        b = (int)fb;

        if (fOverbright != 0.0f) {
            r = (int)((float)r * fOverbright);
            g = (int)((float)g * fOverbright);
            b = (int)((float)b * fOverbright);
        }

        for (j = 0; j < backEnd.numExactDLights; j++) {
            const dlight_t *dl = &backEnd.refdef.dlights[backEnd.exactDLights[j].index];
            vec3_t          vDelta;
            float           fDot;
            float           fScale;

            VectorSubtract(backEnd.exactDLights[j].transformed, xyz, vDelta);
            fDot = DotProduct(vDelta, normal);
            if (fDot < 0.0f) {
                continue;
            }

            fScale = 7500.0f * dl->radius;
            {
                float d2 = DotProduct(vDelta, vDelta);
                float inv;

                if (d2 <= 0.0f) {
                    continue;
                }
                inv = 1.0f / sqrt(d2);
                fScale = fDot * (inv * (inv * (fScale * inv)));
            }

            r = (int)(fScale * dl->color[0] + (float)r);
            g = (int)(fScale * dl->color[1] + (float)g);
            b = (int)(fScale * dl->color[2] + (float)b);
        }

        if ((r | g | b) & ~0xFF) {
            int   iMax   = r > g ? r : g;
            float fScale;

            if (b > iMax) {
                iMax = b;
            }

            fScale = 255.0f / (float)iMax;
            r      = (int)((float)r * fScale);
            g      = (int)((float)g * fScale);
            b      = (int)((float)b * fScale);
        }

        colors[0] = (unsigned char)r;
        colors[1] = (unsigned char)g;
        colors[2] = (unsigned char)b;
        colors[3] = 0xFF;
    }
}

void R_Sphere_InitLights()
{
    const char *ents;
    const char *ret;
    qboolean    bFlareDirSet;

    ents         = ri.CM_EntityString();
    bFlareDirSet = qfalse;

    s_sun.szFlareName[0] = 0;
    s_sun.exists         = qfalse;

    VectorClear(ambientlight);
    backEnd.sphere.TessFunction = &RB_Light_Sphere;
    backEnd.currentSphere       = &backEnd.sphere;
    backEnd.bExactDLights       = qfalse;
    backEnd.numExactDLights     = 0;

    R_InitLightGroups();

    while (ents) {
        ret = COM_Parse((char **)&ents);
        if (!ret || !*ret) {
            break;
        }
        if (*ret == '{' || *ret == '}') {
            continue;
        }

        if (!strcmp(ret, "suncolor") || !strcmp(ret, "sunlight")) {
            sscanf(COM_Parse((char **)&ents), "%f %f %f", &s_sun.color[0], &s_sun.color[1], &s_sun.color[2]);
            VectorScale(s_sun.color, tr.overbrightMult, s_sun.color);
            s_sun.exists = qtrue;
        } else if (!strcmp(ret, "sundirection")) {
            vec3_t dir;

            sscanf(COM_Parse((char **)&ents), "%f %f %f", &dir[0], &dir[1], &dir[2]);
            AngleVectors(dir, s_sun.direction, 0, 0);

            s_sun.exists = qtrue;
            if (!bFlareDirSet) {
                VectorCopy(s_sun.direction, s_sun.flaredirection);
            }
        } else if (!strcmp(ret, "sunflaredirection")) {
            vec3_t dir;

            sscanf(COM_Parse((char **)&ents), "%f %f %f", &dir[0], &dir[1], &dir[2]);
            AngleVectors(dir, s_sun.flaredirection, 0, 0);
            bFlareDirSet = qtrue;
        } else if (!strcmp(ret, "sunflarename")) {
            Q_strncpyz(s_sun.szFlareName, COM_Parse((char **)&ents), sizeof(s_sun.szFlareName));
        } else if (!strcmp(ret, "ambientlight")) {
            sscanf(COM_Parse((char **)&ents), "%f %f %f", &ambientlight[0], &ambientlight[1], &ambientlight[2]);
        } else {
            COM_Parse((char **)&ents);
        }
    }

    if (s_sun.exists && !s_sun.szFlareName[0]) {
        Q_strncpyz(s_sun.szFlareName, "sun", sizeof(s_sun.szFlareName));
    }
}

static float R_SphereLightIntensityAt(const spherel_t *pLight, const vec3_t vPos, const byte *groupIntensity);

static void R_Sphere_AccumulateAtPoint(const vec3_t vPos, vec3_t vLight, const byte *groupIntensity)
{
    int          i;
    int          numlights;
    spherel_t  **lights;
    mnode_t     *leaf;

    VectorCopy(ambientlight, vLight);

    if (!tr.world || !tr.numSLights) {
        vLight[0] = vLight[1] = vLight[2] = (float)tr.identityLightByte;
        return;
    }

    leaf = NULL;
    if (tr.world->vis) {
        leaf = R_PointInLeaf(vPos);
        if (leaf->area == -1) {
            leaf = NULL;
        }
    }

    if (leaf) {
        numlights = leaf->numlights;
        lights    = leaf->lights;
    } else {
        numlights = 0;
        lights    = NULL;
    }

    for (i = 0; i < numlights; i++) {
        spherel_t *pLight = lights[i];
        float      fIntensity;

        if (pLight->type == SPHEREL_SUN) {
            VectorMA(vLight, 255.0f * pLight->brightness, pLight->color, vLight);
            continue;
        }

        fIntensity = R_SphereLightIntensityAt(pLight, vPos, groupIntensity);
        if (fIntensity > 0.0f) {
            VectorMA(vLight, 255.0f * fIntensity, pLight->color, vLight);
        }
    }
}

void R_GetLightingAtPoint(const vec3_t vPos, vec3_t vLight, const byte *groupIntensity)
{
    R_Sphere_AccumulateAtPoint(vPos, vLight, groupIntensity);
}

typedef struct gatheredLight_s {
    spherel_t *pLight;
    float      fIntensity;
} gatheredLight_t;

#define MAX_GATHERED_LIGHTS 8

static float R_SphereLightIntensityAt(const spherel_t *pLight, const vec3_t vPos, const byte *groupIntensity)
{
    vec3_t vDelta;
    float  fDist, fT, fIntensity, fFalloff;

    if (pLight->type == SPHEREL_AREA || pLight->type == SPHEREL_SUN) {
        return 0.0f;
    }

    VectorSubtract(pLight->origin, vPos, vDelta);
    fDist = VectorLength(vDelta);
    if (fDist > pLight->falloff_end_dist) {
        return 0.0f;
    }

    fIntensity = pLight->brightness;
    if (tr.world && pLight->group_num != -1) {
        fIntensity *= (float)groupIntensity[pLight->group_num & 0xFF] / 255.0f;
    }

    if (pLight->type == SPHEREL_SPOT && fDist > 0.0f) {
        float fDot = DotProduct(vDelta, pLight->targetNormal) / fDist;

        if (fDot <= pLight->dotProduct_hotspot) {
            if (fDot >= pLight->dotProduct_penumbra) {
                fIntensity *= (1.0f - pLight->spherical_ambient)
                                  * ((fDot - pLight->dotProduct_penumbra)
                                     / (pLight->dotProduct_hotspot - pLight->dotProduct_penumbra))
                              + pLight->spherical_ambient;
            } else {
                fIntensity *= pLight->spherical_ambient;
            }
        }
    }

    if (fDist >= pLight->falloff_start_dist) {
        fT = (fDist - pLight->falloff_start_dist) / (pLight->falloff_end_dist - pLight->falloff_start_dist);

        if (pLight->curveOffset == 0.0f) {
            fFalloff = 1.0f - fT;
        } else {
            fFalloff = (pLight->curveOffset * ((1.0f - fT) * pLight->curveOffset))
                     / ((fT + pLight->curveOffset) * (fT + pLight->curveOffset));
        }
        fIntensity *= fFalloff;
    }

    if (fIntensity <= 0.01f) {
        return 0.0f;
    }

    return fIntensity;
}

static void R_InsertLightIntoList(spherel_t *pLight, float fIntensity, gatheredLight_t *pList, int *pCount)
{
    int i, j;

    for (i = 0; i < *pCount; i++) {
        if (fIntensity > pList[i].fIntensity) {
            break;
        }
    }

    if (i >= MAX_GATHERED_LIGHTS) {
        return;
    }

    for (j = Q_min(*pCount, MAX_GATHERED_LIGHTS - 1); j > i; j--) {
        pList[j] = pList[j - 1];
    }

    pList[i].pLight     = pLight;
    pList[i].fIntensity = fIntensity;

    if (*pCount < MAX_GATHERED_LIGHTS) {
        (*pCount)++;
    }
}

int R_GatherLightSources(const vec3_t vPos, vec3_t *pvLightPos, vec3_t *pvLightIntensity, int iMaxLights)
{
    int             i;
    int             iLightCount;
    int             iFound;
    gatheredLight_t lights[MAX_GATHERED_LIGHTS];
    mnode_t        *leaf;

    if (iMaxLights <= 0 || r_fullbright->integer || !tr.world) {
        return 0;
    }

    memset(lights, 0, sizeof(lights));
    iFound = 0;

    leaf = NULL;
    if (tr.world->vis) {
        leaf = R_PointInLeaf(vPos);
        if (leaf->area == -1 || !leaf->numlights) {
            leaf = NULL;
        }
    }

    if (leaf) {
        light_reference_count++;

        for (i = 0; i < leaf->numlights; i++) {
            spherel_t *pCurrLight = leaf->lights[i];
            float      fIntensity;

            if (!pCurrLight->leaf) {
                continue;
            }
            if (backEnd.refdef.areamask[pCurrLight->leaf->area >> 3] & (1 << (pCurrLight->leaf->area & 7))) {
                continue;
            }
            if (pCurrLight->reference_count == light_reference_count) {
                continue;
            }
            pCurrLight->reference_count = light_reference_count;

            fIntensity = R_SphereLightIntensityAt(pCurrLight, vPos, tr.refdef.lightGroupIntensity);
            if (fIntensity > 0.0f) {
                R_InsertLightIntoList(pCurrLight, fIntensity, lights, &iFound);
            }
        }
    } else {
        for (i = 0; i < tr.numSLights; i++) {
            float fIntensity = R_SphereLightIntensityAt(&tr.sLights[i], vPos, tr.refdef.lightGroupIntensity);

            if (fIntensity > 0.0f) {
                R_InsertLightIntoList(&tr.sLights[i], fIntensity, lights, &iFound);
            }
        }
    }

    iLightCount = 0;

    if (s_sun.exists) {
        trace_t trace;
        vec3_t  vEnd;

        VectorMA(vPos, 131072.0, s_sun.direction, vEnd);
        ri.CM_BoxTrace(&trace, vPos, vEnd, vec3_origin, vec3_origin, 0, CONTENTS_SOLID, 0);

        if (trace.surfaceFlags & SURF_SKY) {
            VectorMA(vPos, 131072.0, s_sun.direction, pvLightPos[0]);
            pvLightIntensity[0][0] = s_sun.color[0] * (1.0 / 510.0);
            pvLightIntensity[0][1] = s_sun.color[1] * (1.0 / 510.0);
            pvLightIntensity[0][2] = s_sun.color[2] * (1.0 / 510.0);
            iLightCount            = 1;
        }
    }

    for (i = 0; iLightCount < iMaxLights && i < iFound; i++, iLightCount++) {
        vec3_t vColor;

        VectorCopy(lights[i].pLight->origin, pvLightPos[iLightCount]);
        VectorScale(lights[i].pLight->color, lights[i].fIntensity, vColor);

        if (vColor[0] > 1.0 || vColor[1] > 1.0 || vColor[2] > 1.0) {
            NormalizeColor(vColor, vColor);
        }

        VectorCopy(vColor, pvLightIntensity[iLightCount]);
    }

    return iLightCount;
}
