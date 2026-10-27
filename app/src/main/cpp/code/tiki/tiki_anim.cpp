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


#include "q_shared.h"
#include "qcommon.h"
#include "../skeletor/skeletor.h"
#include "../corepp/mem_blockalloc.h"
#include "../corepp/con_set.h"
#include "tiki_files.h"
#include "tiki_skan.h"

#define PROBE_GET(name, def, flags) Cvar_Get(name, def, flags)
#define PROBE_PRINT                 Com_Printf
#include "../qcommon/qa_probe.h"

static void *context;

static int AnimCompareFunc(const void *a, const void *b)
{
    dloaddef_t *ld = (dloaddef_t *)context;
    return TIKI_Anim_Compare(ld->loadanims[*(int *)a]->alias, ld->loadanims[*(int *)b]->alias);
}

void TIKI_GetAnimOrder(dloaddef_t *ld, int *order)
{
    int i;

    for (i = 0; i < ld->numanims; i++) {
        order[i] = i;
    }
    context = ld;
    qsort(order, ld->numanims, sizeof(int), AnimCompareFunc);
}

static void TIKI_SetupTikiAnim(dtiki_t *pmdl, int alias_index)
{
    dtikianim_t              *panims;
    skelAnimDataGameHeader_t *animData;
    dtikianimdef_t           *panimdef;
    int                       lastframe;
    int                       i, j;

    panims = pmdl->a;
    if (!panims) {
        return;
    }

    animData = SkeletorCacheGetData(alias_index);
    if (!animData) {
        return;
    }

    lastframe = animData->numFrames - 1;

    for (i = 0; i < panims->num_anims; i++) {
        if (panims->m_aliases[i] != alias_index) {
            continue;
        }

        panimdef = panims->animdefs[i];

        for (j = 0; j < panimdef->num_server_cmds; j++) {
            if (panimdef->server_cmds[j].frame_num == TIKI_FRAME_LAST) {
                panimdef->server_cmds[j].frame_num = lastframe;
            }
        }

        for (j = 0; j < panimdef->num_client_cmds; j++) {
            if (panimdef->client_cmds[j].frame_num == TIKI_FRAME_LAST) {
                panimdef->client_cmds[j].frame_num = lastframe;
            }
        }

        panimdef->cmds_fixed = qtrue;

        /* retail TIKI_SetupTikiAnim @0x000F4400: a native SKAN animation registers
           its bone mapping here, the first time its data is resident, and only
           while the slot is still unset.  An animation paged in after the tiki was
           built would otherwise be read by bone index, and a skeleton whose bone
           order is not that animation's channel order then poses a permutation. */
        if (pmdl->boneMappings && pmdl->boneMappings[i] == TIKI_BONEMAP_UNSET) {
            skelAnimHeader_t *skel = TIKI_Skan_FindByAnim(pmdl, i);

            if (skel && skel->ident == TIKI_SKC_HEADER_IDENT) {
                int verdict = TIKI_MAPV_NOSKEL;
                int mapped  = TIKI_RegisterAnimBoneMappingV(skel, pmdl->skelIndex, &verdict);

                if (verdict == TIKI_MAPV_IDENTITY) {
                    pmdl->boneMappings[i] = TIKI_BONEMAP_IDENTITY;
                } else if (mapped >= 0) {
                    pmdl->boneMappings[i] = mapped;
                }

                if (mapped < 0 && verdict != TIKI_MAPV_IDENTITY) {
                    PROBE_CENSUS_EMIT(
                        "[TIKI:",
                        "map",
                        "setup tiki=%s anim=%d name='%s' handle=-1 calc=%s\n",
                        pmdl->name ? pmdl->name : "?",
                        i,
                        panimdef->alias ? panimdef->alias : "?",
                        TIKI_Skan_MapVerdictName(verdict)
                    );
                }
            } else {
                /* this anim's data is resident (we returned early otherwise) and it
                   still has no native SKAN header, so it never will: latch instead of
                   re-probing on every fetch.  Only tablefull stays retried -- that one
                   clears as tikis free their entries. */
                pmdl->boneMappings[i] = TIKI_BONEMAP_NOHEADER;
                if (PROBE_On()) {
                    PROBE_CENSUS_EMIT(
                        "[TIKI:",
                        "map",
                        "setup tiki=%s anim=%d name='%s' handle=-3 calc=%s\n",
                        pmdl->name ? pmdl->name : "?",
                        i,
                        panimdef->alias ? panimdef->alias : "?",
                        skel ? "ident" : "nohdr"
                    );
                }
            }
        }
    }
}

static skelAnimDataGameHeader_t *TIKI_GetAnimData(dtiki_t *pmdl, int animnum)
{
    dtikianim_t              *panims      = pmdl->a;
    int                       alias_index = panims->m_aliases[animnum];
    skelAnimDataGameHeader_t *animData    = SkeletorCacheGetData(alias_index);

    /* retail TIKI_GetAnim @0x000F4810 re-runs the setup from the live data; the cmds
       half is idempotent (it only rewrites TIKI_FRAME_LAST), so re-entering it also
       re-tries the mapping registration the first pass could not do because the
       animation's data was not resident yet. */
    if (animData
        && (!panims->animdefs[animnum]->cmds_fixed
            || (pmdl->boneMappings && pmdl->boneMappings[animnum] == TIKI_BONEMAP_UNSET))) {
        TIKI_SetupTikiAnim(pmdl, alias_index);
    }

    return animData;
}

const char *TIKI_Anim_NameForNum(dtiki_t *pmdl, int animnum)
{
    dtikianimdef_t *panimdef;

    if (!pmdl || !pmdl->a) {
        return NULL;
    }

    if (animnum < 0 || animnum >= pmdl->a->num_anims) {
        return NULL;
    }

    panimdef = pmdl->a->animdefs[animnum];
    return panimdef->alias;
}

int TIKI_Anim_NumForName(dtiki_t *pmdl, const char *name)
{
    int i;

    if (!pmdl || !pmdl->a) {
        return -1;
    }

    for (i = 0; i < pmdl->a->num_anims; i++) {
        if (!Q_stricmp(pmdl->a->animdefs[i]->alias, name)) {
            return i;
        }
    }

    return -1;
}

int TIKI_Anim_Random(dtiki_t *pmdl, const char *name)
{
    dtikianimdef_t *panimdef;
    int             i;
    float           totalweight;
    float           weights[MAX_FRAMEINFOS];
    int             anim[MAX_FRAMEINFOS];
    int             num;
    size_t          len;
    int             diff;
    float           weight;

    len = strlen(name);
    if (!len || !pmdl) {
        return -1;
    }

    if (!pmdl->a) {
        return -1;
    }

    num         = 0;
    totalweight = 0.0f;
    for (i = 0; i < pmdl->a->num_anims; i++) {
        panimdef = pmdl->a->animdefs[i];
        diff     = strnicmp(panimdef->alias, name, len);
        if (diff || panimdef->alias[len] == '_') {
            if (diff > 0) {
                break;
            }
        } else {
            if (num >= MAX_FRAMEINFOS || !TIKI_GetAnimData(pmdl, i)) {
                break;
            }

            totalweight += panimdef->weight;
            anim[num]    = i;
            weights[num] = panimdef->weight;

            num++;
        }
    }

    if (!num) {
        return -1;
    }

    weight = ((float)(rand() & 0x7fff) / 32767.0f) * totalweight;
    for (i = 0; i < num; i++) {
        weight -= weights[i];
        if (weight <= 0) {
            break;
        }
    }

    return anim[i];
}

int TIKI_Anim_NumFrames(dtiki_t *pmdl, int animnum)
{
    if (!pmdl) {
        return 0;
    }

    if (!pmdl->a || animnum < 0 || animnum >= pmdl->a->num_anims) {
        return 0;
    }

    skelAnimDataGameHeader_t *animData = TIKI_GetAnimData(pmdl, animnum);
    return animData->numFrames;
}

tikiFrame_t *TIKI_Anim_TanFrames(dtiki_t *pmdl, int animnum, int *pNumFrames)
{
    if (pNumFrames) {
        *pNumFrames = 0;
    }

    if (!pmdl || !pmdl->a) {
        return NULL;
    }

    if (animnum < 0 || animnum >= pmdl->a->num_anims) {
        return NULL;
    }

    skelAnimDataGameHeader_t *animData = TIKI_GetAnimData(pmdl, animnum);
    if (!animData) {
        return NULL;
    }

    if (pNumFrames) {
        *pNumFrames = animData->numTanFrames;
    }

    return animData->tanFrames;
}

void TIKI_Frame_Bounds(dtiki_t *pmdl, int animnum, int framenum, float scale, vec3_t mins, vec3_t maxs)
{
    skelAnimDataGameHeader_t *animData;
    skelAnimGameFrame_t      *frame;
    int                       i;

    VectorClear(mins);
    VectorClear(maxs);

    if (!pmdl || !pmdl->a) {
        return;
    }

    if (animnum < 0 || animnum >= pmdl->a->num_anims) {
        TIKI_Error("TIKI_Frame_Bounds: anim %i out of range for '%s'\n", animnum, pmdl->name);
        return;
    }

    animData = TIKI_GetAnimData(pmdl, animnum);
    if (!animData || !animData->m_frame) {
        return;
    }

    if (framenum < 0 || framenum >= animData->numFrames) {
        TIKI_Error("TIKI_Frame_Bounds: frame %i out of range for '%s'\n", framenum, pmdl->name);
        return;
    }

    frame = &animData->m_frame[framenum];

    for (i = 0; i < 3; i++) {
        mins[i] = scale * (pmdl->load_scale * frame->bounds[0][i]);
        maxs[i] = scale * (pmdl->load_scale * frame->bounds[1][i]);
    }
}

void TIKI_Full_Frame_Bounds(
    dtiki_t *pmdl, int anim, int framenum, int torso_anim, int torso_framenum,
    float scale, vec3_t mins, vec3_t maxs
)
{
    static const int kAnimBlend = (1 << 10);
    static const int kAnimMask  = ~((1 << 9) | (1 << 10) | (1 << 12));
    static const int kFrameMask = ~(1 << 13);

    vec3_t legsMins, legsMaxs;
    vec3_t torsoMins, torsoMaxs;
    int    legsLive, torsoLive;
    int    i;

    VectorClear(mins);
    VectorClear(maxs);

    if (!pmdl) {
        return;
    }

    VectorClear(legsMins);
    VectorClear(legsMaxs);
    VectorClear(torsoMins);
    VectorClear(torsoMaxs);

    legsLive  = (anim & kAnimBlend) != 0;
    torsoLive = (torso_anim & kAnimBlend) != 0;

    if (legsLive) {
        TIKI_Frame_Bounds(pmdl, anim & kAnimMask, framenum & kFrameMask, scale, legsMins, legsMaxs);
    }

    if (torsoLive) {
        TIKI_Frame_Bounds(
            pmdl, torso_anim & kAnimMask, torso_framenum & kFrameMask, scale, torsoMins, torsoMaxs
        );
    }

    if (legsLive && torsoLive) {
        for (i = 0; i < 3; i++) {
            mins[i] = (legsMins[i] + torsoMins[i]) * 0.5f;
            maxs[i] = (legsMaxs[i] + torsoMaxs[i]) * 0.5f;
        }
    } else if (legsLive) {
        VectorCopy(legsMins, mins);
        VectorCopy(legsMaxs, maxs);
    } else if (torsoLive) {
        VectorCopy(torsoMins, mins);
        VectorCopy(torsoMaxs, maxs);
    }
}

float TIKI_Anim_Time(dtiki_t *pmdl, int animnum)
{
    if (!pmdl) {
        return 0.0;
    }

    if (!pmdl->a || animnum < 0 || animnum >= pmdl->a->num_anims) {
        return 0.0;
    }

    skelAnimDataGameHeader_t *animData = TIKI_GetAnimData(pmdl, animnum);

    // retail returns the stored duration field and its PPC `divw r0,r3,r28` does NOT trap
    return animData->numFrames ? (animData->frameTime * animData->numFrames) : 0.0f;
}

float TIKI_Anim_Frametime(dtiki_t *pmdl, int animnum)
{
    if (!pmdl) {
        return 0.0;
    }

    if (!pmdl->a || animnum < 0 || animnum >= pmdl->a->num_anims) {
        return 0.0;
    }

    skelAnimDataGameHeader_t *animData = TIKI_GetAnimData(pmdl, animnum);
    return animData->frameTime;
}

void TIKI_Anim_Delta(dtiki_t *pmdl, int animnum, float *delta)
{
    if (!pmdl) {
        VectorClear(delta);
        return;
    }

    if (!pmdl->a || animnum < 0 || animnum >= pmdl->a->num_anims) {
        VectorClear(delta);
        return;
    }

    skelAnimDataGameHeader_t *animData = TIKI_GetAnimData(pmdl, animnum);
    VectorScale(animData->totalDelta, pmdl->load_scale, delta);
}

void TIKI_Anim_AbsoluteDelta(dtiki_t *pmdl, int animnum, float *delta)
{
    skelAnimDataGameHeader_t *animData;
    int                      i;

    if (!pmdl) {
        VectorClear(delta);
        return;
    }

    if (!pmdl->a || animnum < 0 || animnum >= pmdl->a->num_anims) {
        VectorClear(delta);
        return;
    }

    animData = TIKI_GetAnimData(pmdl, animnum);
    if (!animData) {
        VectorClear(delta);
        return;
    }

    delta[0] = 0;
    delta[1] = 0;
    delta[2] = 0;
    for (i = 0; i < animData->numFrames; i++) {
        delta[0] += fabs(animData->m_frame[i].delta[0]);
        delta[1] += fabs(animData->m_frame[i].delta[1]);
        delta[2] += fabs(animData->m_frame[i].delta[2]);
    }

    VectorScale(delta, pmdl->load_scale, delta);
}

void TIKI_Anim_AngularDelta(dtiki_t *pmdl, int animnum, float *delta)
{
    if (!pmdl) {
        *delta = 0;
        return;
    }

    if (!pmdl->a || animnum < 0 || animnum >= pmdl->a->num_anims) {
        *delta = 0;
        return;
    }

    skelAnimDataGameHeader_t *animData = TIKI_GetAnimData(pmdl, animnum);
    *delta                             = animData->totalAngleDelta;
}

qboolean TIKI_Anim_HasDelta(dtiki_t *pmdl, int animnum)
{
    if (!pmdl) {
        return qfalse;
    }

    if (!pmdl->a || animnum < 0 || animnum >= pmdl->a->num_anims) {
        return qfalse;
    }

    skelAnimDataGameHeader_t *animData = TIKI_GetAnimData(pmdl, animnum);
    return animData->bHasDelta;
}

void TIKI_Anim_DeltaOverTime(dtiki_t *pTiki, int iAnimnum, float fTime1, float fTime2, vec3_t vDelta)
{
    int                       realAnimIndex;
    skelAnimDataGameHeader_t *animData;
    SkelVec3                  absDelta;

    if (!pTiki || !pTiki->a) {
        return;
    }

    if (iAnimnum >= pTiki->a->num_anims || iAnimnum < 0) {
        return;
    }

    if (fTime2 < fTime1) {
        return;
    }

    realAnimIndex = pTiki->a->m_aliases[iAnimnum];
    if (realAnimIndex != -1) {
        animData = TIKI_GetAnimData(pTiki, iAnimnum);
        absDelta = animData->GetDeltaOverTime(fTime1, fTime2);
        VectorScale(absDelta, pTiki->load_scale, vDelta);
    } else {
        TIKI_Error("Skeletor GetDeltaOverTime: Couldn't find animation with index %i\n", iAnimnum);
        VectorClear(vDelta);
    }
}

void TIKI_Anim_AngularDeltaOverTime(dtiki_t *pTiki, int iAnimnum, float fTime1, float fTime2, float *fDelta)
{
    int                       realAnimIndex;
    skelAnimDataGameHeader_t *animData;

    *fDelta = 0;

    if (!pTiki || !pTiki->a) {
        return;
    }

    if (iAnimnum >= pTiki->a->num_anims || iAnimnum < 0) {
        return;
    }

    if (fTime2 < fTime1) {
        return;
    }

    realAnimIndex = pTiki->a->m_aliases[iAnimnum];
    if (realAnimIndex != -1) {
        animData = TIKI_GetAnimData(pTiki, iAnimnum);
        *fDelta  = animData->GetAngularDeltaOverTime(fTime1, fTime2);
    } else {
        TIKI_Error("Skeletor GetAngularDeltaOverTime: Couldn't find animation with index %i\n", iAnimnum);
        *fDelta = 0;
    }
}

int TIKI_Anim_Flags(dtiki_t *pmdl, int animnum)
{
    if (!pmdl) {
        return 0;
    }

    if (!pmdl->a || animnum < 0 || animnum >= pmdl->a->num_anims) {
        return 0;
    }

    dtikianimdef_t *panimdef = pmdl->a->animdefs[animnum];
    return panimdef->flags;
}

int TIKI_Anim_FlagsSkel(dtiki_t *pmdl, int animnum)
{
    if (!pmdl) {
        return 0;
    }

    if (!pmdl->a || animnum < 0 || animnum >= pmdl->a->num_anims) {
        return 0;
    }

    skelAnimDataGameHeader_t *animData;
    int                       flags;

    animData = TIKI_GetAnimData(pmdl, animnum);
    flags    = animData->flags;

    if (animData->bHasDelta) {
        flags |= TAF_HASDELTA;
    }

    if (animData->bHasMorph) {
        flags |= TAF_HASMORPH;
    }

    if (animData->bHasUpper) {
        flags |= TAF_HASUPPER;
    }

    return flags;
}

qboolean TIKI_Anim_HasServerCommands(dtiki_t *pmdl, int animnum)
{
    if (!pmdl) {
        return qfalse;
    }

    if (!pmdl->a || animnum < 0 || animnum >= pmdl->a->num_anims) {
        return qfalse;
    }

    dtikianimdef_t *panimdef = pmdl->a->animdefs[animnum];
    return panimdef->num_server_cmds > 0;
}

qboolean TIKI_Anim_HasClientCommands(dtiki_t *pmdl, int animnum)
{
    if (!pmdl) {
        return qfalse;
    }

    if (!pmdl->a || animnum < 0 || animnum >= pmdl->a->num_anims) {
        return qfalse;
    }

    dtikianimdef_t *panimdef = pmdl->a->animdefs[animnum];
    return panimdef->num_client_cmds > 0;
}

int TIKI_Anim_CrossblendTime(dtiki_t *pmdl, int animnum)
{
    if (!pmdl) {
        return 0;
    }

    if (!pmdl->a || animnum < 0 || animnum >= pmdl->a->num_anims) {
        TIKI_Error("TIKI_Anim_CrossblendTime: Anim %d out of range for %s.\n", animnum, pmdl->name);
        return 0;
    }

    dtikianimdef_t *panimdef = pmdl->a->animdefs[animnum];
    return panimdef->blendtime;
}

qboolean TIKI_AnimLoaded(dtiki_t *pmdl, int animnum)
{
    if (!pmdl || !pmdl->a) {
        return qfalse;
    }

    if (animnum < 0 || animnum >= pmdl->a->num_anims) {
        return qfalse;
    }

    return SkeletorCacheEntryLoaded(pmdl->a->m_aliases[animnum]);
}

void TIKI_Frame_Delta(dtiki_t *pmdl, int animnum, int framenum, float *delta)
{
    skelAnimDataGameHeader_t *animData;
    skelAnimGameFrame_t      *frame;
    int                       i;

    delta[0] = delta[1] = delta[2] = 0;

    if (!pmdl || !pmdl->a) {
        return;
    }

    if (animnum < 0 || animnum >= pmdl->a->num_anims) {
        TIKI_Error("TIKI_Frame_Delta: Anim %d out of range for %s.\n", animnum, pmdl->name);
        return;
    }

    animData = TIKI_GetAnimData(pmdl, animnum);
    if (!animData || !animData->m_frame) {
        return;
    }

    if (framenum < 0 || framenum >= animData->numFrames) {
        TIKI_Error("TIKI_Frame_Delta: Frame %d out of range for %s.\n", framenum, pmdl->name);
        return;
    }

    frame = &animData->m_frame[framenum];

    for (i = 0; i < 3; i++) {
        delta[i] = pmdl->load_scale * frame->delta[i];
    }
}

float TIKI_Frame_Time(dtiki_t *pmdl, int animnum, int framenum)
{
    skelAnimDataGameHeader_t *animData;

    if (!pmdl || !pmdl->a) {
        return 0.0;
    }

    if (animnum < 0 || animnum >= pmdl->a->num_anims) {
        TIKI_Error("TIKI_Frame_Time: Anim %d out of range for %s.\n", animnum, pmdl->name);
        return 0.0;
    }

    animData = TIKI_GetAnimData(pmdl, animnum);
    if (!animData) {
        return 0.0;
    }

    if (framenum < 0 || framenum >= animData->numFrames) {
        TIKI_Error("TIKI_Frame_Time: Frame %d out of range %s.\n", framenum, pmdl->name);
        return 0.0;
    }

    return animData->frameTime;
}

float TIKI_Frame_Radius(dtiki_t *pmdl, int animnum, int framenum)
{
    skelAnimDataGameHeader_t *animData;
    skelAnimGameFrame_t      *frame;

    if (!pmdl || !pmdl->a) {
        return 0.0;
    }

    if (animnum < 0 || animnum >= pmdl->a->num_anims) {
        TIKI_Error("TIKI_Frame_Radius: Anim %d out of range for %s.\n", animnum, pmdl->name);
        return 0.0;
    }

    animData = TIKI_GetAnimData(pmdl, animnum);
    if (!animData || !animData->m_frame) {
        return 0.0;
    }

    if (framenum < 0 || framenum >= animData->numFrames) {
        TIKI_Error("TIKI_Frame_Radius: Frame %d out of range for %s.\n", framenum, pmdl->name);
        return 0.0;
    }

    frame = &animData->m_frame[framenum];
    return pmdl->load_scale * frame->radius;
}

void TIKI_Frame_Coordinates(dtiki_t *pmdl, int animnum, int framenum, float scale, float *coordinates)
{
    vec3_t mins;
    vec3_t maxs;

    TIKI_Frame_Bounds(pmdl, animnum, framenum, scale, mins, maxs);

    coordinates[0]  = maxs[0]; coordinates[1]  = mins[1]; coordinates[2]  = maxs[2];
    coordinates[3]  = mins[0]; coordinates[4]  = mins[1]; coordinates[5]  = maxs[2];
    coordinates[6]  = mins[0]; coordinates[7]  = maxs[1]; coordinates[8]  = maxs[2];
    coordinates[9]  = maxs[0]; coordinates[10] = maxs[1]; coordinates[11] = maxs[2];
    coordinates[12] = maxs[0]; coordinates[13] = mins[1]; coordinates[14] = mins[2];
    coordinates[15] = mins[0]; coordinates[16] = mins[1]; coordinates[17] = mins[2];
    coordinates[18] = mins[0]; coordinates[19] = maxs[1]; coordinates[20] = mins[2];
    coordinates[21] = maxs[0]; coordinates[22] = maxs[1]; coordinates[23] = mins[2];
}

int TIKI_Anim_Compare(const char *s1, const char *s2)
{
    int c1, c2;

    do {
        c1 = *s1++;
        c2 = *s2++;

        if (c1 >= 'A' && c1 <= 'Z') {
            c1 += ('a' - 'A');
        }
        if (c2 >= 'A' && c2 <= 'Z') {
            c2 += ('a' - 'A');
        }
    } while (c1 && c2 && c1 == c2);

    return c1 - c2;
}
