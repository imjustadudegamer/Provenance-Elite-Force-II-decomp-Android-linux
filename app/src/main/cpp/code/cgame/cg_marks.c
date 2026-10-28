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


#include "cg_local.h"

#define PROBE_GET(name, def, flags) cgi.Cvar_Get(name, def, flags)
#define PROBE_PRINT                 cgi.Printf
#include "../qcommon/qa_probe.h"


#define MAX_MARK_FRAGMENTS 128
#define MAX_MARK_POINTS    384

static vec3_t   cg_vEntAngles;
static vec3_t   cg_vEntOrigin;
static vec3_t   cg_fEntAxis[3];
static qboolean cg_bEntAnglesSet;

static int      cg_iLastEntIndex;
static int      cg_iLastEntTime;
static qboolean cg_bLastEntValid;

markPoly_t *cg_freeMarkPolys; 
markObj_t   cg_activeMarkObjs;
markObj_t  *cg_freeMarkObjs;

markPoly_t *cg_markPolys;
int         cg_iNumMarkPolys;
markObj_t  *cg_markObjs;
int         cg_iNumFreeMarkObjs;
int         cg_iMinFreeMarkObjs;
qboolean    cg_bMarksInitialized;

vec3_t vec_upwards = {0, 0, 1};

typedef struct cg_impactmarkinfo_s {
    vec3_t axis[3];
    vec3_t origin;
    float  fSCenter;
    float  fTCenter;
    float  texCoordScaleS;
    float  texCoordScaleT;
    byte   colors[4];
    int    leafnum;
    vec3_t dir;
    float  radius;
    int    temporary;
    qhandle_t shader;
} cg_impactmarkinfo_t;

int CG_GetMarkFragments(
    int             numVerts,
    const vec3_t   *pVerts,
    const vec3_t    vProjection,
    const vec3_t   *pPointBuffer,
    markFragment_t *pFragmentBuffer,
    float           fPushDist
)
{
    //  * Retail CG_ImpactMark @0x33490 allocates ONE record PER FRAGMENT and never writes
    // What the pass DID here was manufacture fragments retail never builds.  Its face
    // jambs, console flanks, near-backfacing panels -- on brush entities retail never
    return cgi.R_MarkFragments(
        numVerts,
        pVerts,
        vProjection,
        MAX_MARK_POINTS,
        (float *)pPointBuffer,
        MAX_MARK_FRAGMENTS,
        pFragmentBuffer,
        fPushDist
    );
}

static qboolean CG_GetMarkInlineModelOrientation(int iIndex)
{
    centity_t *pCEnt;

    if (iIndex == cg_iLastEntIndex && cg_iLastEntTime == cg.time) {
        return cg_bLastEntValid;
    }

    pCEnt = &cg_entities[-iIndex];
    if (pCEnt->currentValid && pCEnt->currentState.modelindex < cgs.numInlineModels) {
        VectorCopy(pCEnt->lerpAngles, cg_vEntAngles);
        VectorCopy(pCEnt->lerpOrigin, cg_vEntOrigin);
        cg_bLastEntValid = qtrue;

        if (cg_vEntAngles[0] || cg_vEntAngles[1] || cg_vEntAngles[2]) {
            AngleVectorsLeft(cg_vEntAngles, cg_fEntAxis[0], cg_fEntAxis[1], cg_fEntAxis[2]);
            cg_bEntAnglesSet = qtrue;
        } else if (cg_bEntAnglesSet) {
            AxisClear(cg_fEntAxis);
            cg_bEntAnglesSet = qfalse;
        }

        cg_iLastEntIndex = iIndex;
        cg_iLastEntTime  = cg.time;
        return cg_bLastEntValid;
    }

    cg_bLastEntValid = qfalse;
    VectorClear(cg_vEntAngles);
    VectorClear(cg_vEntOrigin);

    if (cg_bEntAnglesSet) {
        AxisClear(cg_fEntAxis);
        cg_bEntAnglesSet = qfalse;
    }

    return qfalse;
}

static void CG_FragmentPosToWorldPos(const vec3_t vFrom, vec3_t vTo)
{
    if (cg_bEntAnglesSet) {
        VectorMA(cg_vEntOrigin, vFrom[0], cg_fEntAxis[0], vTo);
        VectorMA(vTo, vFrom[1], cg_fEntAxis[1], vTo);
        VectorMA(vTo, vFrom[2], cg_fEntAxis[2], vTo);
    } else {
        vTo[0] = cg_vEntOrigin[0] + *vFrom;
        vTo[1] = cg_vEntOrigin[1] + vFrom[1];
        vTo[2] = cg_vEntOrigin[2] + vFrom[2];
    }
}

qboolean CG_UpdateMarkPosition(markObj_t *pMark)
{
    vec3_t v;
    vec3_t pt;
    int    iIndex;

    iIndex = pMark->markPolys->iIndex;
    if (!CG_GetMarkInlineModelOrientation(iIndex)) {
        return qfalse;
    }

    VectorCopy(pMark->markPolys->verts[0].xyz, v);

    if (cg_bEntAnglesSet) {
        VectorScale(cg_fEntAxis[0], v[0], pt);
        VectorMA(pt, v[1], cg_fEntAxis[1], pt);
        VectorMA(pt, v[2], cg_fEntAxis[2], pt);
        VectorAdd(pt, cg_vEntOrigin, pt);
    } else {
        VectorAdd(v, cg_vEntOrigin, pt);
    }

    pMark->leafnum = cgi.CM_PointLeafnum(pt);

    return qtrue;
}

void CG_AddFragmentToScene(int iIndex, qhandle_t hShader, int iNumVerts, polyVert_t *pVerts)
{
    if (iIndex < 0) {
        if (cg_bEntAnglesSet) {
            int         i;
            vec3_t      vTmp;
            polyVert_t *pCurrVert;

            for (i = 0; i < iNumVerts; i++) {
                pCurrVert = &pVerts[i];

                vTmp[0] = cg_fEntAxis[0][0] * pCurrVert->xyz[0];
                vTmp[1] = cg_fEntAxis[0][1] * pCurrVert->xyz[0];
                vTmp[2] = cg_fEntAxis[0][2] * pCurrVert->xyz[0];
                VectorMA(vTmp, pCurrVert->xyz[1], cg_fEntAxis[1], vTmp);
                VectorMA(vTmp, pCurrVert->xyz[2], cg_fEntAxis[2], vTmp);
                VectorAdd(vTmp, cg_vEntOrigin, pCurrVert->xyz);
            }
        } else {
            int         i;
            polyVert_t *pCurrVert;

            for (i = 0; i < iNumVerts; i++) {
                pCurrVert = &pVerts[i];
                VectorAdd(pCurrVert->xyz, cg_vEntOrigin, pCurrVert->xyz);
            }
        }
    }

    cgi.R_AddPolyToScene(hShader, iNumVerts, pVerts, 0);
}

void CG_InitMarks(void)
{
    int i;
    int iMaxMarks;

    iMaxMarks = 256;

    cg_iMinFreeMarkObjs = iMaxMarks / 32;
    if (cg_iMinFreeMarkObjs < 8) {
        cg_iMinFreeMarkObjs = 8;
    }
    if (cg_iMinFreeMarkObjs > 32) {
        cg_iMinFreeMarkObjs = 32;
    }

    cg_iNumMarkPolys = 7 * iMaxMarks / 4;

    if (cg_markPolys) {
        cgi.Free(cg_markPolys);
    }

    cg_markPolys = (markPoly_t *)cgi.Malloc(sizeof(markPoly_t) * cg_iNumMarkPolys);

    if (!cg_markPolys) {
        cgi.Error(ERR_DROP, "CG_InitMarks: Could not allocate array for mark polys");
    }

    if (cg_markObjs) {
        cgi.Free(cg_markObjs);
    }

    cg_markObjs = (markObj_t *)cgi.Malloc(sizeof(markObj_t) * iMaxMarks);

    if (!cg_markObjs) {
        cgi.Error(ERR_DROP, "CG_InitMarks: Could not allocate array for mark objects");
    }

    memset(cg_markPolys, 0, sizeof(markPoly_t) * cg_iNumMarkPolys);

    cg_freeMarkPolys = cg_markPolys;
    for (i = 0; i < cg_iNumMarkPolys - 1; i++) {
        cg_markPolys[i].nextPoly = &cg_markPolys[i + 1];
    }
    cg_markPolys[i].nextPoly = NULL;

    cg_activeMarkObjs.nextMark = &cg_activeMarkObjs;
    cg_activeMarkObjs.prevMark = &cg_activeMarkObjs;
    memset(cg_markObjs, 0, sizeof(markObj_t) * iMaxMarks);
    cg_freeMarkObjs = cg_markObjs;

    for (i = 0; i < iMaxMarks - 1; i++) {
        cg_markObjs[i].nextMark = &cg_markObjs[i + 1];
    }
    cg_markObjs[i].nextMark = NULL;

    cg_iNumFreeMarkObjs  = iMaxMarks;
    cg_bMarksInitialized = qtrue;
}

void CG_FreeMarkPoly(markPoly_t *le)
{
    if (!cg_bMarksInitialized) {
        return;
    }

    le->nextPoly     = cg_freeMarkPolys;
    cg_freeMarkPolys = le;
}

void CG_FreeMarkObj(markObj_t *pMark)
{
    markPoly_t *pPoly;
    markPoly_t *pNextPoly;

    assert(pMark != &cg_activeMarkObjs);

    for (pPoly = pMark->markPolys; pPoly; pPoly = pNextPoly) {
        pNextPoly = pPoly->nextPoly;
        CG_FreeMarkPoly(pPoly);
    }

    pMark->prevMark->nextMark = pMark->nextMark;
    pMark->nextMark->prevMark = pMark->prevMark;
    pMark->nextMark           = cg_freeMarkObjs;
    cg_freeMarkObjs           = pMark;
    cg_iNumFreeMarkObjs++;
}

void CG_FreeBestMarkObj(qboolean bAllowFade)
{
    markObj_t *pMark;

    for (pMark = cg_activeMarkObjs.prevMark; pMark != &cg_activeMarkObjs; pMark = pMark->prevMark) {
        if (pMark->lastVisTime < cg.time - 250) {
            CG_FreeMarkObj(pMark);
            return;
        }
    }

    if (!cg_iNumFreeMarkObjs || !bAllowFade) {
        CG_FreeMarkObj(cg_activeMarkObjs.prevMark);
        return;
    }

    for (pMark = cg_activeMarkObjs.prevMark; pMark != &cg_activeMarkObjs; pMark = pMark->prevMark) {
        if (!pMark->alphaFade || pMark->time > cg.time - 9000) {
            break;
        }
    }

    pMark->time = cg.time - 9000;
    pMark->alphaFade = qtrue;
}

markObj_t *CG_AllocMark(int iNumPolys)
{
    int         iPolyCount;
    markPoly_t *pPoly;
    markObj_t  *pMark;

    if (!cg_bMarksInitialized) {
        return NULL;
    }

    if (iNumPolys < 1) {
        return NULL;
    }

    if (iNumPolys > cg_iNumMarkPolys) {
        return NULL;
    }

    if (cg_iNumFreeMarkObjs <= cg_iMinFreeMarkObjs) {
        CG_FreeBestMarkObj(1);
    }

    pMark           = cg_freeMarkObjs;
    cg_freeMarkObjs = cg_freeMarkObjs->nextMark;

    memset(pMark, 0, sizeof(markObj_t));
    pMark->lastVisTime = cg.time;

    for (iPolyCount = 0; iPolyCount < iNumPolys; iPolyCount++) {
        while (!cg_freeMarkPolys) {
            CG_FreeBestMarkObj(qfalse);
        }

        pPoly            = cg_freeMarkPolys;
        cg_freeMarkPolys = pPoly->nextPoly;

        memset(pPoly, 0, sizeof(*pPoly));
        pPoly->nextPoly  = pMark->markPolys;
        pMark->markPolys = pPoly;
    }

    pMark->nextMark                      = cg_activeMarkObjs.nextMark;
    pMark->prevMark                      = &cg_activeMarkObjs;
    cg_activeMarkObjs.nextMark->prevMark = pMark;
    cg_activeMarkObjs.nextMark           = pMark;

    cg_iNumFreeMarkObjs--;
    assert(cg_iNumFreeMarkObjs >= 0);

    return pMark;
}

int CG_ImpactMark_GetLeafCallback(markFragment_t *mf, void *pCustom)
{
    return ((cg_impactmarkinfo_t *)pCustom)->leafnum;
}

qboolean CG_ImpactMark_PerPolyCallback(const vec3_t *markPoints, markFragment_t *mf, polyVert_t *verts, void *pCustom)
{
    int                  j;
    polyVert_t          *v;
    cg_impactmarkinfo_t *pInfo;

    pInfo = (cg_impactmarkinfo_t *)pCustom;

    if (mf->iIndex >= 0) {
        for (j = 0; j < mf->numPoints; j++) {
            vec3_t delta;

            v = &verts[j];
            VectorCopy(markPoints[j + mf->firstPoint], v->xyz);
            VectorSubtract(v->xyz, pInfo->origin, delta);
            v->st[0]       = pInfo->fSCenter + DotProduct(delta, pInfo->axis[1]) * pInfo->texCoordScaleS;
            v->st[1]       = pInfo->fTCenter + DotProduct(delta, pInfo->axis[2]) * pInfo->texCoordScaleT;
            v->modulate[0] = pInfo->colors[0];
            v->modulate[1] = pInfo->colors[1];
            v->modulate[2] = pInfo->colors[2];
            v->modulate[3] = pInfo->colors[3];
        }

        {
            static int faultMarkPlaceRows;

            if (PROBE_On() && PROBE_Throttle(&faultMarkPlaceRows, 24, 400)) {
                UIVK_PROBE(
                    "MARK:place",
                    "n=%d sh=%d '%s' iIndex=%d npts=%d temp=%d org=(%.2f,%.2f,%.2f) "
                    "dir=(%.3f,%.3f,%.3f) radius=%.4f "
                    "poly0=(%.2f,%.2f,%.2f) poly1=(%.2f,%.2f,%.2f) polyN=(%.2f,%.2f,%.2f)\n",
                    faultMarkPlaceRows, pInfo->shader, cgi.R_GetShaderName(pInfo->shader),
                    mf->iIndex, mf->numPoints, pInfo->temporary,
                    pInfo->origin[0], pInfo->origin[1], pInfo->origin[2],
                    pInfo->dir[0], pInfo->dir[1], pInfo->dir[2], pInfo->radius,
                    verts[0].xyz[0], verts[0].xyz[1], verts[0].xyz[2],
                    verts[1].xyz[0], verts[1].xyz[1], verts[1].xyz[2],
                    verts[mf->numPoints - 1].xyz[0], verts[mf->numPoints - 1].xyz[1],
                    verts[mf->numPoints - 1].xyz[2]);
            }
        }
    } else {
        if (!CG_GetMarkInlineModelOrientation(mf->iIndex)) {
            return qfalse;
        }

        for (j = 0; j < mf->numPoints; j++) {
            vec3_t vWorldPos;
            vec3_t delta;

            v = &verts[j];
            VectorCopy(markPoints[j + mf->firstPoint], v->xyz);

            CG_FragmentPosToWorldPos(v->xyz, vWorldPos);
            VectorSubtract(vWorldPos, pInfo->origin, delta);
            v->st[0]       = pInfo->fSCenter + DotProduct(delta, pInfo->axis[1]) * pInfo->texCoordScaleS;
            v->st[1]       = pInfo->fTCenter + DotProduct(delta, pInfo->axis[2]) * pInfo->texCoordScaleT;
            v->modulate[0] = pInfo->colors[0];
            v->modulate[1] = pInfo->colors[1];
            v->modulate[2] = pInfo->colors[2];
            v->modulate[3] = pInfo->colors[3];
        }
    }

    return qtrue;
}



void CG_ImpactMark(
    qhandle_t    markShader,
    const vec3_t origin,
    const vec3_t dir,
    float        orientation,
    float        red,
    float        green,
    float        blue,
    float        alpha,
    int          flag,
    float        radius,
    qboolean     temporary,
    int          lighting,
    qboolean     fadeColour
)
{
    int                 i;
    int                 numFragments;
    float               fSScale2, fTScale2;
    float               fSScale3, fTScale3;
    float               texCoordScale;
    vec3_t              originalPoints[4];
    vec3_t              markPoints[MAX_MARK_POINTS];
    vec3_t              projection;
    markFragment_t      markFragments[MAX_MARK_FRAGMENTS], *mf;
    vec3_t              v;
    cg_impactmarkinfo_t info;

    if (!cg_bMarksInitialized) {
        return;
    }

    if (!cg_marks->integer) {
        return;
    }

    /* retail's own guard: its PC cgame ships the string and the Mac body tests
       radius <= 0 before the axes, so a zero radius names itself and bails */
    if (radius <= 0.0f) {
        cgi.Printf("CG_ImpactMark called with <= 0 radius\n");
        return;
    }

    texCoordScale = 0.5f / radius;

    fSScale2 = radius;
    fTScale2 = radius;
    fSScale3 = radius;
    fTScale3 = radius;

    info.fSCenter = 0.5f;
    info.fTCenter = 0.5f;
    info.texCoordScaleS = texCoordScale;
    info.texCoordScaleT = texCoordScale;


    if (orientation) {
        VectorNormalize2(dir, info.axis[0]);
        PerpendicularVector(info.axis[2], info.axis[0]);
        RotatePointAroundVector(v, info.axis[0], info.axis[2], orientation);
        VectorCopy(v, info.axis[2]);
        CrossProduct(info.axis[2], info.axis[0], info.axis[1]);
    } else {
        vec3_t angles;

        vectoangles(dir, angles);
        AnglesToAxis(angles, info.axis);
    }

    for (i = 0; i < 3; i++) {
        originalPoints[0][i] = origin[i] - fSScale2 * info.axis[1][i] + fTScale2 * info.axis[2][i];
        originalPoints[1][i] = origin[i] + fSScale3 * info.axis[1][i] + fTScale2 * info.axis[2][i];
        originalPoints[2][i] = origin[i] + fSScale3 * info.axis[1][i] - fTScale3 * info.axis[2][i];
        originalPoints[3][i] = origin[i] - fSScale2 * info.axis[1][i] - fTScale3 * info.axis[2][i];
    }

    VectorScale(dir, -32, projection);

    if (PROBE_On()) {
        static int s_iMarkQuadCount = 0;

        if (PROBE_Throttle(&s_iMarkQuadCount, 24, 400)) {
            UIVK_PROBE(
                "MARK:quad",
                "n=%d sh=%d '%s' org=(%.2f,%.2f,%.2f) r=%.4f proj=(%.2f,%.2f,%.2f)"
                " a1=(%.3f,%.3f,%.3f) a2=(%.3f,%.3f,%.3f)"
                " P0=(%.2f,%.2f,%.2f) P1=(%.2f,%.2f,%.2f) P2=(%.2f,%.2f,%.2f) P3=(%.2f,%.2f,%.2f)\n",
                s_iMarkQuadCount, markShader, cgi.R_GetShaderName(markShader),
                origin[0], origin[1], origin[2], radius,
                projection[0], projection[1], projection[2],
                info.axis[1][0], info.axis[1][1], info.axis[1][2],
                info.axis[2][0], info.axis[2][1], info.axis[2][2],
                originalPoints[0][0], originalPoints[0][1], originalPoints[0][2],
                originalPoints[1][0], originalPoints[1][1], originalPoints[1][2],
                originalPoints[2][0], originalPoints[2][1], originalPoints[2][2],
                originalPoints[3][0], originalPoints[3][1], originalPoints[3][2]
            );
        }
    }

    numFragments = CG_GetMarkFragments(4, originalPoints, projection, markPoints, markFragments, 20.0f);

    if (PROBE_On()) {
        static int s_iDoccNewRows = 0;
        static int s_iDoccShader;
        int        iPts = 0;

        for (i = 0, mf = markFragments; i < numFragments; i++, mf++) {
            iPts += mf->numPoints;
        }

        /* A shader the boot has not marked with yet always names itself: the
           shared cadence is spent on the per-frame shadow mark, and a decal that
           creates once would otherwise never print the fragment count that
           splits a dead creation path from a dead draw path. */
        if (markShader != s_iDoccShader || PROBE_Throttle(&s_iDoccNewRows, 24, 400)) {
            s_iDoccShader = markShader;
            UIVK_PROBE(
                "DOCC:new",
                "sh=%d '%s' org=(%.1f,%.1f,%.1f) dir=(%.3f,%.3f,%.3f) radius=%.4f nfrag=%d npts=%d temp=%d\n",
                markShader,
                cgi.R_GetShaderName(markShader),
                origin[0], origin[1], origin[2],
                dir[0], dir[1], dir[2],
                radius,
                numFragments,
                iPts,
                temporary ? 1 : 0
            );
        }
    }

    if (lighting) {
        vec3_t vLight;
        cgi.R_GetLightingForDecal(vLight, dir, origin);

        info.colors[0] = (int)(red * vLight[0]);
        info.colors[1] = (int)(green * vLight[1]);
        info.colors[2] = (int)(blue * vLight[2]);
    } else {
        info.colors[0] = (int)(red * 255.0f);
        info.colors[1] = (int)(green * 255.0f);
        info.colors[2] = (int)(blue * 255.0f);
    }

    if (fadeColour) {
        info.colors[0] = 0;
        info.colors[1] = 0;
        info.colors[2] = 0;
        info.colors[3] = 0;
    } else {
        info.colors[0] = (int)(red * 255.0f);
        info.colors[1] = (int)(green * 255.0f);
        info.colors[2] = (int)(blue * 255.0f);
        info.colors[3] = (int)(alpha * 255.0f);
    }
    VectorCopy(origin, info.origin);
    info.shader = markShader;
    VectorAdd(origin, dir, v);
    info.leafnum = cgi.CM_PointLeafnum(v);
    VectorCopy(dir, info.dir);
    info.radius = radius;
    info.temporary = temporary ? 1 : 0;

    if (temporary) {
        polyVert_t verts[MAX_VERTS_ON_POLY];

        for (i = 0, mf = markFragments; i < numFragments; i++, mf++) {
            if (mf->numPoints > MAX_VERTS_ON_POLY) {
                mf->numPoints = MAX_VERTS_ON_POLY;
            }

            if (CG_ImpactMark_PerPolyCallback(markPoints, mf, verts, (void *)&info)) {
                CG_AddFragmentToScene(mf->iIndex, markShader, mf->numPoints, verts);
            }
        }
    } else {
        CG_AssembleFinalMarks(
            markPoints,
            markFragments,
            numFragments,
            &CG_ImpactMark_PerPolyCallback,
            &CG_ImpactMark_GetLeafCallback,
            (void *)&info,
            info.origin,
            radius,          
            markShader,
            fadeColour,
            flag
        );
    }
}

void CG_AssembleFinalMarks(
    vec3_t         *markPoints,
    markFragment_t *markFragments,
    int             numFragments,
    qboolean (*PerPolyCallback)(const vec3_t *markPoints, markFragment_t *mf, polyVert_t *verts, void *pCustom),
    int (*GetLeafCallback)(markFragment_t *mf, void *pCustom),
    void     *pCustom,
    vec3_t    pos,
    float     radius,
    qhandle_t markShader,
    qboolean  fadeColour,
    int       flag
)
{
    markObj_t      *pMark;
    markPoly_t     *pPoly;
    int             iGroup, iFirstNewGroup;
    int             numFragsForMark;
    int             i;
    markFragment_t *mf;

    // but the read is UB and retail never performs it, so bail where retail's loop does.
    if (numFragments <= 0) {
        return;
    }

    iFirstNewGroup = 0;
    do {
        i               = iFirstNewGroup;
        mf              = &markFragments[iFirstNewGroup];
        iGroup          = mf->iIndex;
        numFragsForMark = 0;

        for (; i < numFragments; i++, mf++) {
            if (iGroup < 0 && mf->iIndex == iGroup) {
                numFragsForMark++;
            } else if (iGroup >= 0 && mf->iIndex >= 0) {
                numFragsForMark++;
            }
        }

        pMark = CG_AllocMark(numFragsForMark);
        if (!pMark) {
            break;
        }

        pMark->time       = cg.time;
        pMark->alphaFade  = fadeColour;
        pMark->markShader = markShader;
        pMark->fadein     = flag;
        VectorCopy(pos, pMark->pos);
        pMark->radius = radius;

        i              = iFirstNewGroup;
        mf             = &markFragments[iFirstNewGroup];
        iFirstNewGroup = 0;

        if (mf->iIndex < 0) {
            centity_t *cent;
            vec3_t vLocalPos;

            cent = &cg_entities[-mf->iIndex];

            VectorSubtract(pMark->pos, cent->lerpOrigin, vLocalPos);

            if (cent->lerpAngles[0] || cent->lerpAngles[1] || cent->lerpAngles[2]) {
                vec3_t axis[3];

                AngleVectorsLeft(cent->lerpAngles, axis[0], axis[1], axis[2]);
                MatrixTransformVectorRight(axis, vLocalPos, pMark->pos);
            } else {
                VectorCopy(vLocalPos, pMark->pos);
            }
        }

        pPoly = pMark->markPolys;
        for (; i < numFragments; i++, mf++) {
            if (iGroup < 0 && mf->iIndex != iGroup) {
                if (!iFirstNewGroup) {
                    iFirstNewGroup = i;
                }
            } else if (iGroup >= 0 && mf->iIndex < 0) {
                if (!iFirstNewGroup) {
                    iFirstNewGroup = i;
                }
            } else {
                if (mf->numPoints > MAX_VERTS_ON_POLY) {
                    mf->numPoints = MAX_VERTS_ON_POLY;
                }

                if (PerPolyCallback(markPoints, mf, pPoly->verts, pCustom)) {
                    pPoly->numVerts = mf->numPoints;
                    pPoly->iIndex   = mf->iIndex;
                    pPoly           = pPoly->nextPoly;
                }
            }
        }

        pMark->leafnum = GetLeafCallback(mf, pCustom);
    } while (iFirstNewGroup);
}

void CG_ImpactMarkSimple(
    qhandle_t    markShader,
    const vec3_t origin,
    const vec3_t dir,
    float        orientation,
    float        fRadius,
    float        red,
    float        green,
    float        blue,
    float        alpha,
    qboolean     alphaFade,
    qboolean     temporary,
    qboolean     dolighting,
    qboolean     fadein
)
{
    if (cg_bMarksInitialized) {
        CG_ImpactMark(
            markShader,
            origin,
            dir,
            orientation,
            red,
            green,
            blue,
            alpha,
            alphaFade,     
            fRadius,       
            temporary,
            dolighting,
            fadein
        );
    }
}

#define MARK_FADE_TIME 1000

void CG_AddMarks(void)
{
    int         j;
    int         t;
    int         fade;
    markObj_t  *pMark;
    markObj_t  *pNext;
    markPoly_t *pPoly;
    polyVert_t *pVert;
    polyVert_t  tmpVerts[MAX_VERTS_ON_POLY];    
    int         viewleafnum;
    int         iTotalTime;

    if (!cg_bMarksInitialized) {
        return;
    }

    if (!cg_marks->integer) {
        return;
    }

    iTotalTime = 1000 * cg_markTime->integer;

    viewleafnum = cgi.CM_PointLeafnum(cg.refdef.vieworg);
    for (pMark = cg_activeMarkObjs.nextMark; pMark != &cg_activeMarkObjs; pMark = pNext) {
        pNext = pMark->nextMark;

        if (pMark->alphaFade && cg.time > pMark->time + iTotalTime) {
            CG_FreeMarkObj(pMark);
            continue;
        }

        if (pMark->markPolys->iIndex < 0 && !CG_UpdateMarkPosition(pMark)) {
            if (pMark->lastVisTime < cg.time - 3000) {
                CG_FreeMarkObj(pMark);
            }
            continue;
        }

        if (!cgi.CM_LeafInPVS(viewleafnum, pMark->leafnum)) {
            continue;
        }

        if (pMark->markPolys->iIndex < 0) {
            vec3_t vWorldPos;
            CG_FragmentPosToWorldPos(pMark->pos, vWorldPos);
            if (CG_FrustumCullSphere(vWorldPos, pMark->radius)) {
                continue;
            }
        } else {
            if (CG_FrustumCullSphere(pMark->pos, pMark->radius)) {
                continue;
            }
        }

        pMark->lastVisTime = cg.time;
        if (pMark->fadein) {
            fade = 255 * (cg.time - pMark->time) / MARK_FADE_TIME;
            if (fade > 255) {
                fade          = 255;
                pMark->fadein = 0;
            }

            for (pPoly = pMark->markPolys; pPoly; pPoly = pPoly->nextPoly) {
                for (j = 0; j < pPoly->numVerts; j++) {
                    pPoly->verts[j].modulate[3] = fade;
                }
            }
        }

        if (pMark->alphaFade) {
            t = pMark->time + iTotalTime - cg.time;
            if (t < MARK_FADE_TIME) {
                fade = 255 * t / MARK_FADE_TIME;
                for (pPoly = pMark->markPolys; pPoly; pPoly = pPoly->nextPoly) {
                    for (j = 0; j < pPoly->numVerts; j++) {
                        pPoly->verts[j].modulate[3] = fade;
                    }
                }
            }
        }

        for (pPoly = pMark->markPolys; pPoly; pPoly = pPoly->nextPoly) {
            if (pPoly->iIndex < 0) {
                memcpy(tmpVerts, pPoly->verts, 24 * pPoly->numVerts);
                pVert = tmpVerts;
            } else {
                pVert = pPoly->verts;
            }

            if (PROBE_On() && pPoly->numVerts > 0 && cg.time - pMark->lastProbeTime > 1000) {
                vec3_t  vMarkPos;
                vec3_t  vToView;
                trace_t trace;
                float   fDist;

                pMark->lastProbeTime = cg.time;

                if (pPoly->iIndex < 0) {
                    CG_GetMarkInlineModelOrientation(pPoly->iIndex);
                    CG_FragmentPosToWorldPos(pPoly->verts[0].xyz, vMarkPos);
                } else {
                    VectorCopy(pPoly->verts[0].xyz, vMarkPos);
                }

                VectorSubtract(cg.refdef.vieworg, vMarkPos, vToView);
                fDist = VectorNormalize(vToView);
                cgi.CM_BoxTrace(&trace, vMarkPos, cg.refdef.vieworg, vec3_origin, vec3_origin, 0, CONTENTS_SOLID, qfalse);

                UIVK_PROBE(
                    "DOCC:mark",
                    "polys=%d iIndex=%d pos=(%.1f,%.1f,%.1f) viewdist=%.1f frac=%.3f startsolid=%d leaf=%d radius=%.1f\n",
                    pPoly->numVerts,
                    pPoly->iIndex,
                    vMarkPos[0], vMarkPos[1], vMarkPos[2],
                    fDist,
                    trace.fraction,
                    trace.startsolid ? 1 : 0,
                    pMark->leafnum,
                    pMark->radius
                );
            }

            CG_AddFragmentToScene(pPoly->iIndex, pMark->markShader, pPoly->numVerts, pVert);
        }
    }
}

qboolean CG_CheckMakeMarkOnEntity(int iEntIndex)
{
    if (iEntIndex == ENTITYNUM_WORLD) {
        return qtrue;
    }

    if (iEntIndex == ENTITYNUM_NONE) {
        return qfalse;
    }

    if (cg_entities[iEntIndex].currentState.solid != SOLID_BMODEL) {
        return qfalse;
    }

    if (cg_entities[iEntIndex].currentState.modelindex < 0
        || cg_entities[iEntIndex].currentState.modelindex > cgi.CM_NumInlineModels()) {
        return qfalse;
    }

    return qtrue;
}
