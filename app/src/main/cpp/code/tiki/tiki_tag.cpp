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
#include "../renderercommon/tr_types.h"
#include "tiki_cache.h"
#include "tiki_skan.h"
#include "tiki_files.h"
#include "tiki_skel.h"
#include "tiki_tag.h"

static qboolean CHAN_TagProbeOn(void)
{
	static cvar_t *chan_qa;

	if (!chan_qa) {
		chan_qa = Cvar_Get("qa_probe", "0", 0);
	}
	return (chan_qa && chan_qa->integer) ? qtrue : qfalse;
}

static int TIKI_Tag_BoneChannelOfTag(const dtiki_t *tiki, int tagnum)
{
    if (!tiki || tagnum < 0 || tagnum >= tiki->num_tags) {
        return -1;
    }

    if (tiki->skelIndex >= 0) {
        return tagnum;
    }

    return tagnum + 1;
}

int TIKI_Tag_NameToNum(dtiki_t *pmdl, const char *name)
{
    // WIDGET-E1: retail has a SECOND arm this port never carried.  After the prologue it
    // which retail never turns into a skeleton -- walks the tiki's FIRST animation
    int         i, count;
    skelAnimDataGameHeader_t *animData;

    if (!pmdl) {
        return -1;
    }

    if (pmdl->skelIndex < 0) {
        animData = NULL;
        if (pmdl->a && pmdl->a->num_anims > 0) {
            animData = SkeletorCacheGetData(pmdl->a->m_aliases[0]);
        }

        if (animData && animData->tanTags) {
            count = animData->numTanTags;
            if (count > pmdl->num_tags) {
                count = pmdl->num_tags;
            }

            for (i = 0; i < count; i++) {
                if (!Q_stricmp(animData->tanTags->Name(i), name)) {
                    return i;
                }
            }
        }

        return -1;
    }

    return pmdl->GetBoneNumFromName(name);
}

const char *TIKI_Tag_NumToName(dtiki_t *pmdl, int iTagNum)
{
    skelAnimDataGameHeader_t *animData;

    if (!pmdl || iTagNum < 0 || iTagNum >= pmdl->num_tags) {
        return NULL;
    }

    if (pmdl->skelIndex < 0) {
        animData = NULL;
        if (pmdl->a && pmdl->a->num_anims > 0) {
            animData = SkeletorCacheGetData(pmdl->a->m_aliases[0]);
        }

        if (animData && animData->tanTags && iTagNum < animData->numTanTags) {
            return animData->tanTags->Name(iTagNum);
        }

        return NULL;
    }

    return pmdl->GetBoneNameFromNum(iTagNum);
}

extern "C" void TIKI_Error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));


// sized to the loader's own invariant instead of to an observation about shipped chains.
#define TIKI_TAG_MAX_CHAIN TIKI_MAX_BONES
static_assert(TIKI_TAG_MAX_CHAIN >= 1, "chain bound");

static void TIKI_Tag_BuildLocalParentTable(dtiki_t *tiki, int numBones, int *parent)
{
    int              i;
    int              mesh;
    skelHeaderGame_t *skelmodel;

    for (i = 0; i < numBones; i++) {
        parent[i] = -1;
    }

    for (mesh = 0; mesh < tiki->numMeshes; mesh++) {
        skelmodel = TIKI_GetSkel(tiki->mesh[mesh]);
        if (!skelmodel) {
            continue;
        }

        for (i = 0; i < skelmodel->numBones; i++) {
            int local  = tiki->m_boneList.GetLocalFromGlobal(skelmodel->pBones[i].channel);
            int global = skelmodel->pBones[i].parent;

            if (local < 0 || local >= numBones) {
                continue;
            }

            parent[local] = (global < 0) ? -1 : tiki->m_boneList.GetLocalFromGlobal(global);
        }
    }
}

static int TIKI_Tag_SkelParent(const skelHeaderGame_t *skel, int bone)
{
    return skel->pBones[bone].skelParent;
}

static void TIKI_Tag_ApplyControllers(
    const int       *parent,
    int              numBones,
    int              tagnum,
    const int       *bone_tag,
    const vec4_t    *bone_quat,
    skelBoneCache_t *bones,
    orientation_t   *out
)
{
    int       chain[TIKI_TAG_MAX_CHAIN];
    int       depth = 0;
    int       b;
    int       i;
    int       n;
    int       ctrlSlot;
    vec4_t    cq;
    SkelMat4  acc;
    SkelMat4  parentInv;
    SkelMat4  cur;
    SkelMat4  factor;
    SkelMat4  prev;
    SkelQuat  q;
    SkelMat3  rot;

    for (b = tagnum; b >= 0 && b < numBones && depth < TIKI_TAG_MAX_CHAIN; b = parent[b]) {
        chain[depth++] = b;
    }

    acc.MakeIdentity();

    for (i = 0; i < depth; i++) {
        cur.MakeIdentity();
        for (n = 0; n < 3; n++) {
            cur.val[n][0] = bones[chain[i]].matrix[n][0];
            cur.val[n][1] = bones[chain[i]].matrix[n][1];
            cur.val[n][2] = bones[chain[i]].matrix[n][2];
        }
        cur.val[3][0] = bones[chain[i]].offset[0];
        cur.val[3][1] = bones[chain[i]].offset[1];
        cur.val[3][2] = bones[chain[i]].offset[2];

        if (i + 1 < depth) {
            parentInv.MakeIdentity();
            for (n = 0; n < 3; n++) {
                parentInv.val[n][0] = bones[chain[i + 1]].matrix[n][0];
                parentInv.val[n][1] = bones[chain[i + 1]].matrix[n][1];
                parentInv.val[n][2] = bones[chain[i + 1]].matrix[n][2];
            }
            parentInv.val[3][0] = bones[chain[i + 1]].offset[0];
            parentInv.val[3][1] = bones[chain[i + 1]].offset[1];
            parentInv.val[3][2] = bones[chain[i + 1]].offset[2];
            parentInv.Inverse();

            factor.Multiply(cur, parentInv);
        } else {
            factor = cur;
        }

        ctrlSlot = -1;
        for (n = NUM_BONE_CONTROLLERS - 1; n >= 0; n--) {
            if (bone_tag[n] != chain[i]) {
                continue;
            }

            cq[0] = bone_quat[n][0];
            cq[1] = bone_quat[n][1];
            cq[2] = bone_quat[n][2];
            cq[3] = bone_quat[n][3];
            q.Set(cq);
            q.GetMat3(rot);
            ctrlSlot = n;
            break;
        }

        prev = acc;
        acc.Multiply(prev, factor);

        if (ctrlSlot >= 0) {
            acc.RotateBy(rot);
        }
    }

    out->origin[0] = acc.val[3][0];
    out->origin[1] = acc.val[3][1];
    out->origin[2] = acc.val[3][2];
    VectorCopy(acc.val[0], out->axis[0]);
    VectorCopy(acc.val[1], out->axis[1]);
    VectorCopy(acc.val[2], out->axis[2]);
}


#define TIKI_SKAN_QUAT_SCALE   (1.0f / 32767.0f)    
#define TIKI_SKAN_OFFSET_SCALE (1.0f / 63.0f)       

static void TIKI_Tag_QuatMul(const float *q, const float *p, float *r)
{
    r[0] = q[3] * p[0] + q[0] * p[3] + q[1] * p[2] - q[2] * p[1];
    r[1] = q[3] * p[1] + q[1] * p[3] + q[2] * p[0] - q[0] * p[2];
    r[2] = q[3] * p[2] + q[2] * p[3] + q[0] * p[1] - q[1] * p[0];
    r[3] = q[3] * p[3] - q[0] * p[0] - q[1] * p[1] - q[2] * p[2];
}

static void TIKI_Tag_SkelSolveBone(
    skelHeaderGame_t *skel,
    int               bone,
    skelAnimHeader_t *hdr,
    skelFrame_t      *frameRec,
    const int        *mapping,
    const char       *boneName,
    skelAnimHeader_t *cbHdr,
    skelFrame_t      *cbRec,
    const int        *cbMapping,
    float             crossblend_lerp,
    float             out_quat[4],
    float             out_offset[3]
)
{
    const short int       *bindRow;
    const short int       *animRow;
    const short int       *cbRow;
    float                  ratio;
    float                  cbRatio;
    float                  baseLen;
    float                  cbQuat[4];
    float                  cbOffset[3];
    int                    i;

    baseLen = VectorLength(skel->pBones[bone].offset);
    bindRow = skel->pBones[bone].hasBaseRow ? skel->pBones[bone].baseRow : NULL;

    animRow = TIKI_Skan_RecordRow(hdr, frameRec, mapping, bone, baseLen, boneName, bindRow, &ratio);

    if (!animRow) {
        out_quat[0] = 0.0f;
        out_quat[1] = 0.0f;
        out_quat[2] = 0.0f;
        out_quat[3] = 1.0f;
        VectorCopy(skel->pBones[bone].offset, out_offset);
        return;
    }

    for (i = 0; i < 4; i++) {
        out_quat[i] = TIKI_SKAN_QUAT_SCALE * (float)animRow[i];
    }
    for (i = 0; i < 3; i++) {
        out_offset[i] = TIKI_SKAN_OFFSET_SCALE * (float)animRow[4 + i] * ratio;
    }

    if (cbHdr && cbRec) {
        cbRow = TIKI_Skan_RecordRow(cbHdr, cbRec, cbMapping, bone, baseLen, boneName, bindRow, &cbRatio);

        if (!cbRow) {
            return;
        }

        for (i = 0; i < 4; i++) {
            cbQuat[i] = TIKI_SKAN_QUAT_SCALE * (float)cbRow[i];
        }
        for (i = 0; i < 3; i++) {
            cbOffset[i] = TIKI_SKAN_OFFSET_SCALE * (float)cbRow[4 + i] * cbRatio;
        }

        SlerpQuaternion(cbQuat, out_quat, crossblend_lerp, out_quat);

        for (i = 0; i < 3; i++) {
            out_offset[i] =
                cbOffset[i] * (1.0f - crossblend_lerp) + out_offset[i] * crossblend_lerp;
        }
    }
}

static qboolean TIKI_Tag_OrientationSkelBody(
    dtiki_t       *tiki,
    int            tagnum,
    float          scale,
    const int     *bone_tag,
    const vec4_t  *bone_quat,
    int            anim,
    int            frame,
    int            crossblend_anim,
    int            crossblend_frame,
    float          crossblend_lerp,
    qboolean       uselegs,
    qboolean       usetorso,
    int            torso_anim,
    int            torso_frame,
    int            torso_crossblend_anim,
    int            torso_crossblend_frame,
    float          torso_crossblend_lerp,
    orientation_t *out
)
{
    skelHeaderGame_t  *skel;
    skelAnimHeader_t  *hdr = NULL;
    skelFrame_t       *frameRec = NULL;
    int                chain[TIKI_TAG_MAX_CHAIN];
    float              acc[TIKI_TAG_MAX_CHAIN][7];
    float              accQuat[4];
    float              newAcc[4];
    float              origin[3];
    SkelMat3           mat;
    SkelQuat           sq;
    int                depth = 0;
    int                bone;
    int                i;
    int                n;

    skel = TIKI_GetBaseSkel(tiki);
    if (!skel) {
        return qfalse;
    }

    if (!TIKI_ValidateSkelAnimAndFrame(tiki, anim, frame, &hdr, &frameRec)) {
        return qfalse;
    }

    for (bone = tagnum; bone >= 0 && bone < skel->numBones && depth < TIKI_TAG_MAX_CHAIN;
         bone = TIKI_Tag_SkelParent(skel, bone)) {
        chain[depth++] = bone;
    }
    if (!depth) {
        return qfalse;
    }

    for (i = 0; i < depth; i++) {
        skelAnimHeader_t *chHdr = NULL;
        skelFrame_t      *chRec = NULL;
        const int        *chMap = NULL;
        const char       *boneName;
        qboolean          legs;
        int               chAnim;
        int               chFrame;
        float             chLerp;
        int               cbAnim;
        int               cbFrame;
        skelAnimHeader_t *cbHdr = NULL;
        skelFrame_t      *cbRec = NULL;
        const int        *cbMap = NULL;

        bone = chain[i];

        if (uselegs && usetorso) {
            legs = (skel->pBones[bone].ef2BodyPart == 1);
        } else if (uselegs) {
            legs = qtrue;
        } else {
            legs = qfalse;
        }

        if (legs) {
            chAnim = anim;
            chFrame = frame;
            chLerp = crossblend_lerp;
            cbAnim = crossblend_anim;
            cbFrame = crossblend_frame;
        } else {
            chAnim = torso_anim;
            chFrame = torso_frame;
            chLerp = torso_crossblend_lerp;
            cbAnim = torso_crossblend_anim;
            cbFrame = torso_crossblend_frame;
        }

        if (!TIKI_ValidateSkelAnimAndFrame(tiki, chAnim, chFrame, &chHdr, &chRec)) {
            continue;
        }

        if (tiki->boneMappings && chAnim >= 0 && chAnim < tiki->a->num_anims) {
            chMap = TIKI_GetAnimBoneMapping(tiki->boneMappings[chAnim]);
        }

        boneName = skel_BoneNames.FindName(skel->pBones[bone].channel);

        if (chLerp >= 0.0f) {
            TIKI_ValidateSkelAnimAndFrame(tiki, cbAnim, cbFrame, &cbHdr, &cbRec);

            if (tiki->boneMappings && cbAnim >= 0 && cbAnim < tiki->a->num_anims) {
                cbMap = TIKI_GetAnimBoneMapping(tiki->boneMappings[cbAnim]);
            }

            if (!cbRec || cbRec == chRec) {
                cbHdr = NULL;
                cbRec = NULL;
            }
        }

        TIKI_Tag_SkelSolveBone(
            skel, bone, chHdr, chRec, chMap, boneName, cbHdr, cbRec, cbMap, chLerp,
            acc[i], acc[i] + 4);
    }

    bone = chain[depth - 1];
    memcpy(accQuat, acc[depth - 1], sizeof(accQuat));

    for (n = 0; n < NUM_BONE_CONTROLLERS; n++) {
        if (bone_tag[n] != bone) {
            continue;
        }
        float cq[4];
        cq[0] = bone_quat[n][0];
        cq[1] = bone_quat[n][1];
        cq[2] = bone_quat[n][2];
        cq[3] = bone_quat[n][3];
        TIKI_Tag_QuatMul(accQuat, cq, accQuat);
    }

    sq.Set(accQuat);
    sq.GetMat3(mat);

    origin[0] = acc[depth - 1][4] + tiki->load_origin[0];
    origin[1] = acc[depth - 1][5] + tiki->load_origin[1];
    origin[2] = acc[depth - 1][6] + tiki->load_origin[2];

    for (i = depth - 2; i >= 0; i--) {
        vec3_t transformed;

        bone = chain[i];

        TIKI_Tag_QuatMul(acc[i], accQuat, newAcc);

        for (n = 0; n < NUM_BONE_CONTROLLERS; n++) {
            if (bone_tag[n] != bone) {
                continue;
            }
            float cq[4];
            cq[0] = bone_quat[n][0];
            cq[1] = bone_quat[n][1];
            cq[2] = bone_quat[n][2];
            cq[3] = bone_quat[n][3];
            TIKI_Tag_QuatMul(newAcc, cq, newAcc);
        }

        MatrixTransformVector(acc[i] + 4, mat.val, transformed);
        origin[0] += transformed[0];
        origin[1] += transformed[1];
        origin[2] += transformed[2];

        memcpy(accQuat, newAcc, sizeof(accQuat));
        sq.Set(accQuat);
        sq.GetMat3(mat);
    }

    out->origin[0] = origin[0] * (scale * tiki->load_scale);
    out->origin[1] = origin[1] * (scale * tiki->load_scale);
    out->origin[2] = origin[2] * (scale * tiki->load_scale);
    memcpy(out->axis, mat.val, sizeof(out->axis));

    return qtrue;
}

static qboolean TIKI_Tag_OrientationSolveAnim(
    dtiki_t         *tiki,
    int              anim,
    int              frame,
    int              tagnum,
    const int       *bone_tag,
    const vec4_t    *bone_quat,
    orientation_t   *out
)
{
    int             numBones;
    skelBoneCache_t *bones;
    int            *parent;
    int             i;

    memset(out, 0, sizeof(*out));
    AnglesToAxis(vec3_origin, out->axis);

    if (!tiki) {
        return qfalse;
    }

    numBones = tiki->m_boneList.NumChannels();
    if (numBones <= 0 || tagnum < 0 || tagnum >= numBones) {
        return qfalse;
    }

    tagnum = TIKI_Tag_BoneChannelOfTag(tiki, tagnum);
    if (tagnum < 0 || tagnum >= numBones) {
        return qfalse;
    }

    bones = (skelBoneCache_t *)Skel_Alloc(sizeof(skelBoneCache_t) * numBones);
    if (!bones) {
        return qfalse;
    }

    for (i = 0; i < numBones; i++) {
        VectorClear(bones[i].offset);
        bones[i].matrix[0][0] = 1;
        bones[i].matrix[1][1] = 1;
        bones[i].matrix[2][2] = 1;
    }

    TIKI_GetSkelAnimFrame2(tiki, bones, anim, frame, NULL, NULL, NULL);

    out->origin[0] = bones[tagnum].offset[0];
    out->origin[1] = bones[tagnum].offset[1];
    out->origin[2] = bones[tagnum].offset[2];
    VectorCopy(bones[tagnum].matrix[0], out->axis[0]);
    VectorCopy(bones[tagnum].matrix[1], out->axis[1]);
    VectorCopy(bones[tagnum].matrix[2], out->axis[2]);

    if (bone_tag && bone_quat) {
        parent = (int *)Skel_Alloc(sizeof(int) * numBones);
        if (parent) {
            TIKI_Tag_BuildLocalParentTable(tiki, numBones, parent);
            TIKI_Tag_ApplyControllers(parent, numBones, tagnum, bone_tag, bone_quat, bones, out);
            Skel_Free(parent);
        }
    }

    Skel_Free(bones);
    return qtrue;
}

static skelAnimDataGameHeader_t *TIKI_Tag_ResolveAnim(dtiki_t *pmdl, int anim, int frame, qboolean checkLoaded)
{
    skelAnimDataGameHeader_t *animData;

    if (!pmdl->a || anim >= pmdl->a->num_anims) {
        return NULL;
    }

    animData = SkeletorCacheGetData(pmdl->a->m_aliases[anim]);
    if (!animData) {
        if (checkLoaded) {
            Com_WDPrintf(
                "Unable to load animation: %s. Either the animation does not exist or the name is too long\n",
                pmdl->a->animdefs[anim] ? pmdl->a->animdefs[anim]->alias : pmdl->name
            );
        }
        return NULL;
    }

    if (frame >= animData->numFrames || frame < 0) {
        TIKI_Error("TIKI_Tag_Orientation: frame %d out of range for anim %d in %s.\n", frame, anim, pmdl->name);
        return NULL;
    }

    return animData;
}

void TIKI_Tag_Orientation(
    orientation_t *out,
    int            handle,
    int            anim,
    int            frame,
    int            tagnum,
    float          scale,
    const int     *bone_tag,
    const vec4_t  *bone_quat
)
{
    dtiki_t *pmdl;

    memset(out, 0, sizeof(*out));

    pmdl = TIKI_GetTiki(handle);
    if (!pmdl) {
        return;
    }

    if (tagnum >= pmdl->num_tags || tagnum < 0) {
        TIKI_Error("TIKI_Tag_Orientation: Tag %d out of range for %s.\n", tagnum, pmdl->name);
        return;
    }

    if (anim >= pmdl->num_anims || anim < 0) {
        TIKI_Error("TIKI_Tag_Orientation: anim %d out of range for %s.\n", anim, pmdl->name);
        return;
    }

    if (!TIKI_Tag_ResolveAnim(pmdl, anim, frame, qtrue)) {
        memset(out, 0, sizeof(*out));
        return;
    }

    if (!TIKI_Tag_OrientationSolveAnim(pmdl, anim, frame, tagnum, bone_tag, bone_quat, out)) {
        memset(out, 0, sizeof(*out));
        return;
    }

    VectorAdd(out->origin, pmdl->load_origin, out->origin);
    VectorScale(out->origin, scale * pmdl->load_scale, out->origin);
}

void TIKI_Tag_OrientationEx(
    orientation_t *out,
    int            handle,
    int            anim,
    int            frame,
    int            tagnum,
    float          scale,
    const int     *bone_tag,
    const vec4_t  *bone_quat,
    int            crossblend_anim,
    int            crossblend_frame,
    float          crossblend_lerp,
    qboolean       uselegs,
    qboolean       usetorso,
    int            torso_anim,
    int            torso_frame,
    int            torso_crossblend_anim,
    int            torso_crossblend_frame,
    float          torso_crossblend_lerp
)
{
    dtiki_t          *tiki;
    skelHeaderGame_t *skelmodel;
    qboolean          legs;
    int               passedAnim;
    int               selected;

    passedAnim = anim;

    (void)crossblend_anim;
    (void)crossblend_frame;
    (void)crossblend_lerp;
    (void)uselegs;
    (void)torso_crossblend_anim;
    (void)torso_crossblend_frame;
    (void)torso_crossblend_lerp;

    memset(out, 0, sizeof(*out));

    tiki = TIKI_GetTiki(handle);
    if (!tiki) {
        return;
    }

    if (tagnum >= tiki->num_tags || tagnum < 0) {
        TIKI_Error("TIKI_Tag_Orientation: Tag %d out of range for %s.\n", tagnum, tiki->name);
        memset(out, 0, sizeof(*out));
        return;
    }

    legs = !usetorso;
    if (!legs) {
        skelmodel = (tiki->skelIndex >= 0) ? TIKI_GetSkel(tiki->skelIndex) : NULL;
        if (skelmodel) {
            const int bone = TIKI_Tag_BoneChannelOfTag(tiki, tagnum);

            if (bone >= 0 && bone < skelmodel->numBones
                && skelmodel->pBones[bone].ef2BodyPart == SKEL_BODYPART_LEGS) {
                legs = qtrue;
            }
        }
    }

    selected = legs ? anim : torso_anim;
    if (selected >= tiki->num_anims || selected < 0) {
        TIKI_Error(
            "TIKI_Tag_Orientation: anim %d out of range for %s.\n",
            legs ? passedAnim : torso_anim,
            tiki->name
        );
        memset(out, 0, sizeof(*out));
        return;
    }

    anim = selected;
    frame = legs ? frame : torso_frame;

    if (!TIKI_Tag_ResolveAnim(tiki, anim, frame, qfalse)) {
        memset(out, 0, sizeof(*out));
        return;
    }

    if (TIKI_Tag_OrientationSkelBody(
            tiki,
            tagnum,
            scale,
            bone_tag,
            bone_quat,
            anim,
            frame,
            crossblend_anim,
            crossblend_frame,
            crossblend_lerp,
            uselegs,
            usetorso,
            torso_anim,
            torso_frame,
            torso_crossblend_anim,
            torso_crossblend_frame,
            torso_crossblend_lerp,
            out
        )) {
        return;
    }

    if (!TIKI_Tag_OrientationSolveAnim(tiki, anim, frame, tagnum, bone_tag, bone_quat, out)) {
        memset(out, 0, sizeof(*out));
        return;
    }

    VectorAdd(out->origin, tiki->load_origin, out->origin);
    VectorScale(out->origin, scale * tiki->load_scale, out->origin);
}

void TIKI_Tag_NamedOrientation(
    orientation_t *out,
    int            handle,
    int            anim,
    int            frame,
    float          scale,
    const char    *tagName,
    const int     *bone_tag,
    const vec4_t  *bone_quat
)
{
    int tagnum;

    tagnum = TIKI_Tag_NameToNum(TIKI_GetTiki(handle), tagName);

    TIKI_Tag_OrientationEx(
        out,
        handle,
        anim,
        frame,
        tagnum,
        scale,
        bone_tag,
        bone_quat,
        0,               
        0,               
        1.0f,            
        qtrue,           
        qfalse,          
        0,               
        0,               
        0,               
        0,               
        1.0f             
    );
}


typedef struct {
    skelAnimHeader_t *hdr;      
    skelFrame_t      *rec;      
    const int        *map;      

    skelAnimHeader_t *oldHdr;   
    skelFrame_t      *oldRec;   
    const int        *oldMap;   

    skelAnimHeader_t *cbHdr;    
    skelFrame_t      *cbRec;    
    const int        *cbMap;    

    int   mode;                  
    float wOld;                 
    float wLive;                
    float wCb;                  
    float wICb;                 
} tikiTagLerpChannel_t;

static const int *TIKI_Tag_LerpSkel_AnimMapping(dtiki_t *tiki, int anim)
{
    int handle;

    if (!tiki->boneMappings || !tiki->a || anim < 0 || anim >= tiki->a->num_anims) {
        return NULL;
    }

    handle = tiki->boneMappings[anim];
    if (handle == -1) {
        return NULL;
    }

    return TIKI_GetAnimBoneMapping(handle);
}

static void TIKI_Tag_LerpSkel_SetupChannel(
    dtiki_t              *tiki,
    const refEntity_t    *ent,
    qboolean              legs,
    qboolean              resolveCbAlways,
    tikiTagLerpChannel_t *ch
)
{
    int   anim;
    int   frame;
    int   oldanim;
    int   oldframe;
    int   cbAnim;
    int   cbFrame;
    float framelerp;
    float cbLerp;
    float one = 1.0f;     
    float zero = 0.0f;    

    if (legs) {
        anim = ent->anim;
        frame = ent->frame;
        oldanim = ent->oldanim;
        oldframe = ent->oldframe;
        framelerp = ent->framelerp;
        cbLerp = ent->crossblend_lerp;
        cbAnim = ent->crossblend_anim;
        cbFrame = ent->crossblend_frame;
    } else {
        anim = ent->torso_anim;
        frame = ent->torso_frame;
        oldanim = ent->torso_oldanim;
        oldframe = ent->torso_oldframe;
        framelerp = ent->torso_framelerp;
        cbLerp = ent->torso_crossblend_lerp;
        cbAnim = ent->torso_crossblend_anim;
        cbFrame = ent->torso_crossblend_frame;
    }

    memset(ch, 0, sizeof(*ch));

    if (!TIKI_ValidateSkelAnimAndFrame(tiki, anim, frame, &ch->hdr, &ch->rec)) {
        ch->rec = NULL;
        return;
    }
    ch->map = TIKI_Tag_LerpSkel_AnimMapping(tiki, anim);

    TIKI_ValidateSkelAnimAndFrame(tiki, oldanim, oldframe, &ch->oldHdr, &ch->oldRec);
    ch->oldMap = TIKI_Tag_LerpSkel_AnimMapping(tiki, oldanim);

    if (ch->oldRec && ch->oldRec != ch->rec) {
        ch->wOld = framelerp;
        ch->wLive = one - framelerp;
    } else {
        ch->wOld = zero;
        ch->wLive = one;
    }

    if (resolveCbAlways || cbLerp >= zero) {
        TIKI_ValidateSkelAnimAndFrame(tiki, cbAnim, cbFrame, &ch->cbHdr, &ch->cbRec);
        ch->cbMap = TIKI_Tag_LerpSkel_AnimMapping(tiki, cbAnim);
    }

    if (cbLerp < zero) {
        ch->cbRec = NULL;
        ch->cbHdr = NULL;
        ch->cbMap = NULL;
        ch->wCb = zero;
        ch->wICb = zero;
        ch->mode = (ch->wOld == zero) ? 1 : 2;
        return;
    }

    ch->wCb = cbLerp;
    ch->wICb = one - cbLerp;

    if (ch->wOld == zero) {
        ch->oldRec = ch->cbRec;
        ch->mode = 2;
    } else {
        ch->mode = 3;
    }

    if (!ch->cbRec) {
        ch->mode--;
    }
}

static qboolean TIKI_Tag_LerpSkel_SolveRecord(
    skelHeaderGame_t *skel,
    int               bone,
    skelAnimHeader_t *hdr,
    skelFrame_t      *rec,
    const int        *mapping,
    float             baseLen,
    const char       *boneName,
    const short int  *bindRow,
    float             out_quat[4],
    float             out_offset[3]
)
{
    const short int *row;
    float            ratio = 1.0f;
    int              i;

    row = TIKI_Skan_RecordRow(hdr, rec, mapping, bone, baseLen, boneName, bindRow, &ratio);
    if (!row) {
        return qfalse;
    }

    for (i = 0; i < 4; i++) {
        out_quat[i] = TIKI_SKAN_QUAT_SCALE * (float)row[i];
    }
    for (i = 0; i < 3; i++) {
        out_offset[i] = TIKI_SKAN_OFFSET_SCALE * (float)row[4 + i] * ratio;
    }

    return qtrue;
}

static void TIKI_Tag_LerpSkel_SolveBone(
    skelHeaderGame_t           *skel,
    int                         bone,
    const tikiTagLerpChannel_t *ch,
    float                       baseLen,
    const char                 *boneName,
    const short int            *bindRow,
    float                       out_quat[4],
    float                       out_offset[3]
)
{
    float qA[4];
    float oA[3];
    float qC[4];
    float oC[3];
    int   i;

    if (!TIKI_Tag_LerpSkel_SolveRecord(skel, bone, ch->hdr, ch->rec, ch->map, baseLen, boneName, bindRow, out_quat, out_offset)) {
        out_quat[0] = 0.0f;
        out_quat[1] = 0.0f;
        out_quat[2] = 0.0f;
        out_quat[3] = 1.0f;
        VectorCopy(skel->pBones[bone].offset, out_offset);
        return;
    }

    if (ch->mode >= 2) {
        if (!TIKI_Tag_LerpSkel_SolveRecord(skel, bone, ch->oldHdr, ch->oldRec, ch->oldMap, baseLen, boneName, bindRow, qA, oA)) {
            qA[0] = 0.0f;
            qA[1] = 0.0f;
            qA[2] = 0.0f;
            qA[3] = 1.0f;
            VectorCopy(skel->pBones[bone].offset, oA);
        }

        SlerpQuaternion(qA, out_quat, ch->wLive, out_quat);
        for (i = 0; i < 3; i++) {
            out_offset[i] = oA[i] * ch->wOld + out_offset[i] * ch->wLive;
        }
    }

    if (ch->mode == 3) {
        if (!TIKI_Tag_LerpSkel_SolveRecord(skel, bone, ch->cbHdr, ch->cbRec, ch->cbMap, baseLen, boneName, bindRow, qC, oC)) {
            qC[0] = 0.0f;
            qC[1] = 0.0f;
            qC[2] = 0.0f;
            qC[3] = 1.0f;
            VectorCopy(skel->pBones[bone].offset, oC);
        }

        SlerpQuaternion(qC, out_quat, ch->wCb, out_quat);
        for (i = 0; i < 3; i++) {
            out_offset[i] = oC[i] * ch->wICb + out_offset[i] * ch->wCb;
        }
    }
}

void TIKI_Tag_LerpOrientationSkel(orientation_t *out, const refEntity_t *ent, dtiki_t *tiki, int tagnum)
{
    skelHeaderGame_t           *skel;
    int                         chain[TIKI_TAG_MAX_CHAIN];
    float                       acc[TIKI_TAG_MAX_CHAIN][7];
    tikiTagLerpChannel_t        legsCh;
    tikiTagLerpChannel_t        torsoCh;
    const tikiTagLerpChannel_t *ch;
    int                         depth = 0;
    int                         bone;
    int                         i;
    int                         n;
    float                       accQuat[4];
    float                       newAcc[4];
    float                       origin[3];
    vec3_t                      transformed;
    SkelQuat                    sq;
    SkelMat3                    mat;

    memset(out, 0, sizeof(*out));
    AnglesToAxis(vec3_origin, out->axis);

    skel = TIKI_GetSkel(tiki->skelIndex);
    if (!skel) {
        Com_WPrintf("TIKI_Tag_Orientation : NULL skelmodel.\n");
        return;
    }

    for (bone = tagnum; bone >= 0 && bone < skel->numBones && depth < TIKI_TAG_MAX_CHAIN;
         bone = TIKI_Tag_SkelParent(skel, bone)) {
        chain[depth++] = bone;
    }
    if (!depth) {
        return;
    }

    memset(acc, 0, sizeof(acc));

    memset(&legsCh, 0, sizeof(legsCh));
    memset(&torsoCh, 0, sizeof(torsoCh));

    if (ent->legsLive) {
        TIKI_Tag_LerpSkel_SetupChannel(tiki, ent, qtrue, qfalse, &legsCh);
        if (!legsCh.rec) {
            return;
        }
    }

    if (ent->torsoLive) {
        TIKI_Tag_LerpSkel_SetupChannel(tiki, ent, qfalse, qtrue, &torsoCh);
    }

    for (i = 0; i < depth; i++) {
        const short int *bindRow;
        float            baseLen;
        float            localQuat[4];
        float            localOffset[3];
        const char      *boneName;

        bone = chain[i];

        // when the torso channel is not live or the bone is tagged legs (0x104680
        if (ent->legsLive && ent->torsoLive) {
            ch = (skel->pBones[bone].ef2BodyPart == 1) ? &legsCh : &torsoCh;
        } else if (ent->legsLive) {
            ch = &legsCh;
        } else {
            ch = &torsoCh;
        }

        bindRow = skel->pBones[bone].hasBaseRow ? skel->pBones[bone].baseRow : NULL;

        baseLen = VectorLength(skel->pBones[bone].offset);

        boneName = skel_BoneNames.FindName(skel->pBones[bone].channel);

        TIKI_Tag_LerpSkel_SolveBone(skel, bone, ch, baseLen, boneName, bindRow, localQuat, localOffset);

        acc[i][0] = localQuat[0];
        acc[i][1] = localQuat[1];
        acc[i][2] = localQuat[2];
        acc[i][3] = localQuat[3];
        acc[i][4] = localOffset[0];
        acc[i][5] = localOffset[1];
        acc[i][6] = localOffset[2];
    }

    bone = chain[depth - 1];
    accQuat[0] = acc[depth - 1][0];
    accQuat[1] = acc[depth - 1][1];
    accQuat[2] = acc[depth - 1][2];
    accQuat[3] = acc[depth - 1][3];

    for (n = 0; n < NUM_BONE_CONTROLLERS; n++) {
        if (ent->bone_tag[n] != bone) {
            continue;
        }
        float cq[4];
        cq[0] = ent->bone_quat[n][0];
        cq[1] = ent->bone_quat[n][1];
        cq[2] = ent->bone_quat[n][2];
        cq[3] = ent->bone_quat[n][3];
        TIKI_Tag_QuatMul(accQuat, cq, accQuat);
    }

    sq.Set(accQuat);
    sq.GetMat3(mat);

    origin[0] = acc[depth - 1][4] + tiki->load_origin[0];
    origin[1] = acc[depth - 1][5] + tiki->load_origin[1];
    origin[2] = acc[depth - 1][6] + tiki->load_origin[2];

    for (i = depth - 2; i >= 0; i--) {
        bone = chain[i];

        TIKI_Tag_QuatMul(acc[i], accQuat, newAcc);

        for (n = 0; n < NUM_BONE_CONTROLLERS; n++) {
            if (ent->bone_tag[n] != bone) {
                continue;
            }
            float cq[4];
            cq[0] = ent->bone_quat[n][0];
            cq[1] = ent->bone_quat[n][1];
            cq[2] = ent->bone_quat[n][2];
            cq[3] = ent->bone_quat[n][3];
            TIKI_Tag_QuatMul(newAcc, cq, newAcc);
        }

        MatrixTransformVector(acc[i] + 4, mat.val, transformed);
        origin[0] += transformed[0];
        origin[1] += transformed[1];
        origin[2] += transformed[2];

        memcpy(accQuat, newAcc, sizeof(accQuat));
        sq.Set(accQuat);
        sq.GetMat3(mat);
    }

    out->origin[0] = origin[0] * (ent->scale * tiki->load_scale);
    out->origin[1] = origin[1] * (ent->scale * tiki->load_scale);
    out->origin[2] = origin[2] * (ent->scale * tiki->load_scale);
    memcpy(out->axis, mat.val, sizeof(out->axis));

    if (developer->integer || CHAN_TagProbeOn()) {
        static int lerpskel_tag;
        if (++lerpskel_tag % 240 == 1) {
            Com_Printf(
                "[LERPSKEL:tag:lerpskel] tiki=%s tag=%d ent=%d skel=1 modeL=%d modeT=%d "
                "lerp=%.3f crosslerp=%.3f legs=%d torso=%d anim=%d frame=%d oldanim=%d oldframe=%d "
                "off=(%.2f,%.2f,%.2f) axis0=(%.3f,%.3f,%.3f) zero=%d\n",
                tiki->name,
                tagnum,
                ent->entityNumber,
                ent->legsLive ? legsCh.mode : -1,
                ent->torsoLive ? torsoCh.mode : -1,
                ent->framelerp,
                ent->crossblend_lerp,
                ent->legsLive,
                ent->torsoLive,
                ent->anim,
                ent->frame,
                ent->oldanim,
                ent->oldframe,
                out->origin[0],
                out->origin[1],
                out->origin[2],
                out->axis[0][0],
                out->axis[0][1],
                out->axis[0][2],
                (VectorCompare(out->origin, vec3_origin)
                 && out->axis[0][0] == 1.0f && out->axis[1][1] == 1.0f && out->axis[2][2] == 1.0f)
                    ? 1
                    : 0);
        }
    }
}

static void TIKI_Tag_LerpOrientationVertex(
    orientation_t            *out,
    const refEntity_t        *ent,
    dtiki_t                  *tiki,
    skelAnimDataGameHeader_t *animData,
    skelAnimDataGameHeader_t *oldAnimData,
    int                       tagnum
);

void TIKI_Tag_LerpOrientation(orientation_t *out, int handle, const refEntity_t *ent, int tagnum)
{
    dtiki_t                  *tiki;
    int                       anim;
    skelAnimDataGameHeader_t *animData;

    memset(out, 0, sizeof(*out));
    AnglesToAxis(vec3_origin, out->axis);

    tiki = TIKI_GetTiki(handle);
    if (!tiki) {
        memset(out, 0, sizeof(*out));
        AnglesToAxis(vec3_origin, out->axis);
        return;
    }

    // admitted tagnum 2 and the tag API then walked a bone list retail never indexes.
    if (tagnum >= tiki->num_tags || tagnum < 0) {
        TIKI_Error("TIKI_Tag_LerpOrientation: Tag %d out of range for %s.\n", tagnum, tiki->name);
        memset(out, 0, sizeof(*out));
        AnglesToAxis(vec3_origin, out->axis);
        return;
    }

    anim = ent->anim;
    if (anim >= tiki->num_anims || anim < 0) {
        TIKI_Error("TIKI_Tag_LerpOrientation: anim %d out of range for %s.\n", anim, tiki->name);
        memset(out, 0, sizeof(*out));
        AnglesToAxis(vec3_origin, out->axis);
        return;
    }

    animData = TIKI_Tag_ResolveAnim(tiki, anim, ent->frame, qfalse);
    if (!animData) {
        static int tikanim_lerp_null;
        if (++tikanim_lerp_null % 600 == 1) {
            int alias = (tiki->a && anim < tiki->a->num_anims) ? tiki->a->m_aliases[anim] : -0x7fff;

            Com_DPrintf(
                "[TIKIANIM:tag:lerp:noanimdata] tiki=%s anim=%d alias=%d frame=%d tagnum=%d ent=%d -> %s; posing from "
                "the skeleton, NOT zeroing\n",
                tiki->name,
                anim,
                alias,
                ent->frame,
                tagnum,
                ent->entityNumber,
                alias < 0 ? "alias never registered (retail blob+559 arm)"
                          : "cache row has no data (retail blob+609 arm)");
        }
    } else {
        skelAnimHeaderRec_t *nativeHdr = TIKI_Skan_Find(animData);

        if (nativeHdr && nativeHdr->ident == 1312901971  ) {
            TIKI_Tag_LerpOrientationSkel(out, ent, tiki, tagnum);
            return;
        }

        if (animData->tanTags) {
            skelAnimDataGameHeader_t *oldAnimData;

            oldAnimData = NULL;
            if (tiki->a && ent->oldanim >= 0 && ent->oldanim < tiki->a->num_anims) {
                oldAnimData = SkeletorCacheGetData(tiki->a->m_aliases[ent->oldanim]);
            }

            TIKI_Tag_LerpOrientationVertex(out, ent, tiki, animData, oldAnimData, tagnum);
            return;
        }

        {
            static int lerpskel_fall;
            if ((developer->integer || CHAN_TagProbeOn()) && ++lerpskel_fall % 240 == 1) {
                Com_Printf(
                    "[LERPSKEL:tag:lerpfall] tiki=%s anim=%d tagnum=%d ent=%d -> %s; posing from the "
                    "skeleton (UIRECT: no retail arm left to take -- port-only miss)\n",
                    tiki->name,
                    anim,
                    tagnum,
                    ent->entityNumber,
                    nativeHdr ? "anim is not 'SKAN' and carries no .tan tag table"
                              : "no retained native image (port-only: image rejected or table full)");
            }
        }
    }

    *out = TIKI_OrientationInternal(tiki, ent->entityNumber, tagnum, ent->scale);
}

static const float *TIKI_Tag_TanTagRecord(
    const dtiki_t                  *tiki,
    const skelAnimDataGameHeader_t *animData,
    int                             tagnum,
    int                             frame
)
{
    if (!animData || !animData->tanTags || animData->numTanTags <= 0) {
        return NULL;
    }

    if (frame < 0 || frame >= animData->numTanFrames) {
        return NULL;
    }

    // WIDGET-E1: retail indexes the table, it does not look the name up.
    if (tagnum < 0 || tagnum >= animData->numTanTags) {
        return NULL;
    }

    return animData->tanTags->Pose(tagnum, frame);
}

static void TIKI_Tag_LerpOrientationVertex(
    orientation_t            *out,
    const refEntity_t        *ent,
    dtiki_t                  *tiki,
    skelAnimDataGameHeader_t *animData,
    skelAnimDataGameHeader_t *oldAnimData,
    int                       tagnum
)
{
    const float *newRows;
    const float *oldRows;
    float        framelerp;
    float        scale;
    float        weightNew;
    float        weightOld;
    int          r, c;

    memset(out, 0, sizeof(*out));
    AnglesToAxis(vec3_origin, out->axis);

    if (!animData) {
        return;
    }

    if (ent->frame < 0 || ent->frame >= animData->numTanFrames) {
        TIKI_Error(
            "TIKI_Tag_LerpOrientation: frame %d out of range for anim %d in %s.\n",
            ent->frame,
            ent->anim,
            tiki->name
        );
        return;
    }

    if (ent->oldanim >= tiki->num_anims || ent->oldanim < 0) {
        TIKI_Error("TIKI_Tag_LerpOrientation: anim %d out of range for %s.\n", ent->oldanim, tiki->name);
        return;
    }

    if (!oldAnimData || ent->oldframe < 0 || ent->oldframe >= oldAnimData->numFrames) {
        TIKI_Error(
            "TIKI_Tag_LerpOrientation: frame %d out of range for anim %d in %s.\n",
            ent->oldframe,
            ent->oldanim,
            tiki->name
        );
        return;
    }

    newRows = TIKI_Tag_TanTagRecord(tiki, animData, tagnum, ent->frame);
    if (!newRows) {
        return;
    }

    oldRows = TIKI_Tag_TanTagRecord(tiki, oldAnimData, tagnum, ent->oldframe);

    framelerp = ent->framelerp;
    scale     = tiki->load_scale * ent->scale;

    if (oldRows) {
        weightNew = tiki->load_scale * ((1.0f - framelerp) * ent->scale);
        weightOld = tiki->load_scale * (framelerp * ent->scale);
    } else {
        weightNew = scale;
        weightOld = 0.0f;
    }

    for (r = 0; r < 4; r++) {
        for (c = 0; c < 3; c++) {
            float v = newRows[r * 3 + c] * weightNew + (oldRows ? oldRows[r * 3 + c] * weightOld : 0.0f);

            if (r == 0) {
                v += tiki->load_origin[c] * scale;
                out->origin[c] = v;
            } else {
                out->axis[r - 1][c] = v;
            }
        }
    }

    for (r = 0; r < 3; r++) {
        VectorNormalize(out->axis[r]);
    }
}


int TIKI_Bone_GetParentNum(dtiki_t *pmdl, int iTagNum)
{
    skelHeaderGame_t *skelmodel;
    int              parent;

    if (!pmdl) {
        return -1;
    }

    if (iTagNum >= pmdl->num_tags || iTagNum < 0) {
        TIKI_Error("TIKI_Bone_GetParentNum: Tag %d out of range for %s.\n", iTagNum, pmdl->name);
        return -1;
    }

    if (pmdl->skelIndex < 0) {
        return -1;
    }

    skelmodel = TIKI_GetSkel(pmdl->skelIndex);
    if (!skelmodel) {
        Com_WPrintf("TIKI_Bone_GetParentNum: NULL skelmodel.\n");
        return -1;
    }

    if (iTagNum >= skelmodel->numBones) {
        return -1;
    }

    parent = skelmodel->pBones[iTagNum].skelParent;
    if (parent < 0) {
        return -1;
    }

    return parent;
}

SkelMat4 *TIKI_TransformInternal(dtiki_t *tiki, int entnum, int tagnum)
{
    skeletor_c *skeletor;

    if (tagnum < 0 || !tiki || tagnum >= tiki->m_boneList.NumChannels()) {
        return NULL;
    }

    skeletor = (skeletor_c *)TIKI_GetSkeletor(tiki, entnum);
    if (!skeletor) {
        return NULL;
    }

    return &skeletor->GetBoneFrame(tagnum);
}

qboolean TIKI_IsOnGroundInternal(dtiki_t *tiki, int entnum, int tagnum, float threshold)
{
    skeletor_c *skeletor;

    if (tagnum < 0 || !tiki || tagnum >= tiki->m_boneList.NumChannels()) {
        return qfalse;
    }

    skeletor = (skeletor_c *)TIKI_GetSkeletor(tiki, entnum);
    return skeletor->IsBoneOnGround(tagnum, threshold);
}

orientation_t TIKI_OrientationInternal(dtiki_t *tiki, int entnum, int tagnum, float scale)
{
    orientation_t fail;
    memset(&fail, 0, sizeof(fail));
    AnglesToAxis(vec3_origin, fail.axis);

    if (tagnum < 0 || !tiki || tagnum >= tiki->num_tags) {
        return fail;
    }

    tagnum = TIKI_Tag_BoneChannelOfTag(tiki, tagnum);
    if (tagnum < 0 || tagnum >= tiki->m_boneList.NumChannels()) {
        return fail;
    }

    const skeletor_c *skeletor = (skeletor_c *)TIKI_GetSkeletor(tiki, entnum);
    if (!skeletor) {
        static int tikanim_noskel;
        if (++tikanim_noskel % 600 == 1) {
            Com_DPrintf("[TIKIANIM:tag:orient:noskel] tiki=%s entnum=%d tagnum=%d -> no skeletor instance\n", tiki ? tiki->name : "(null)", entnum, tagnum);
        }
        return fail;
    }

    const SkelMat4  & pTransform = skeletor->GetBoneFrame(tagnum);

    orientation_t orient;
    orient.origin[0] = (pTransform.val[3][0] + tiki->load_origin[0]) * (scale * tiki->load_scale);
    orient.origin[1] = (pTransform.val[3][1] + tiki->load_origin[1]) * (scale * tiki->load_scale);
    orient.origin[2] = (pTransform.val[3][2] + tiki->load_origin[2]) * (scale * tiki->load_scale);
    memcpy(orient.axis, pTransform.val, sizeof(orient.axis));

    return orient;
}

void TIKI_SetPoseInternal(
    void *skeletor, const frameInfo_t *frameInfo, const int *bone_tag, const vec4_t *bone_quat, float actionWeight
)
{
    skeletor_c *skel = (skeletor_c *)skeletor;
    skel->SetPose(frameInfo, bone_tag, bone_quat, actionWeight);
}

void TIKI_SetPoseChannelsInternal(
    void             *skeletor,
    const frameInfo_t *frameInfo,
    const short int  *bodyParts,
    const int        *bone_tag,
    const vec4_t     *bone_quat,
    float             actionWeight
)
{
    skeletor_c *skel = (skeletor_c *)skeletor;
    skel->SetPose(frameInfo, bone_tag, bone_quat, actionWeight, bodyParts);
}

float TIKI_GetRadiusInternal(dtiki_t *tiki, int entnum, float scale)
{
    if (!tiki) {
        TIKI_Error("TIKI_GetRadiusInternal: NULL tiki.\n");
        return 0.0f;
    }

    return TIKI_GlobalRadius(tiki) * tiki->load_scale * scale;
}

float TIKI_GetCentroidRadiusInternal(dtiki_t *tiki, int entnum, float scale, float *centroid)
{
    skeletor_c *skeletor = (skeletor_c *)TIKI_GetSkeletor(tiki, entnum);
    return skeletor->GetCentroidRadius(centroid) * tiki->load_scale * scale;
}

void TIKI_GetFrameInternal(dtiki_t *tiki, int entnum, skelAnimFrame_t *newFrame)
{
    skeletor_c *skeletor = (skeletor_c *)TIKI_GetSkeletor(tiki, entnum);
    skeletor->GetFrame(newFrame);
}

void TIKI_SetEyeTargetPos(dtiki_t *tiki, int entnum, vec3_t pos)
{
    skeletor_c *skeletor = (skeletor_c *)TIKI_GetSkeletor(tiki, entnum);
    skeletor->SetEyeTargetPos(pos);
}
