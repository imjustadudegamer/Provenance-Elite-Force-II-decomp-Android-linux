/*
===========================================================================
Reconstructed from the retail Star Trek: Voyager -- Elite Force II binaries.
The publisher released no source for this file.  This port asserts no copyright
in it and grants no rights to it: it remains the property of its respective
rights holders, and it is covered neither by the GNU GPL nor by the Ritual
Elite Force II SDK agreement.  See LICENSING.md.
===========================================================================
*/
// SPDX-License-Identifier: LicenseRef-EF2-Retail-Reconstruction


#include "q_shared.h"
#include "qcommon.h"
#include "tiki_shared.h"
#include "tiki_skan.h"
#include "../skeletor/skeletor.h"
#include "tiki_files.h"

#include <stddef.h>

extern "C" void TIKI_Error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

extern "C" const char *TIKI_Anim_NameForNum(dtiki_t *pmdl, int animnum);

static_assert(sizeof(skelBone_t) == 16, "skelBone_t must be retail's 16-byte bone row");
static_assert(sizeof(skelFrame_t) == 56, "skelFrame_t must be retail's 40-byte header + one bone row");
static_assert(offsetof(skelFrame_t, radius) == 24, "skelFrame_t::radius must be at retail's +24");
static_assert(offsetof(skelFrame_t, delta) == 28, "skelFrame_t::delta must be at retail's +28");
static_assert(offsetof(skelFrame_t, bones) == 40, "skelFrame_t::bones must be at retail's +40");

static const int SKAN_FRAME_HEADER_SIZE = (int)(sizeof(skelFrame_t) - sizeof(skelBone_t));

struct skanRetained_t {
    const skelAnimDataGameHeader_t *owner;
    skelAnimHeaderRec_t            *hdr;
    size_t                          bytes;
};

static skanRetained_t m_skanRetained[MAX_TIKI_ALIASES];
static int            m_skanRetainedCount;

skelAnimHeaderRec_t *TIKI_Skan_RetainNative(const void *image, size_t bytes)
{
    skelAnimHeaderRec_t *copy;
    int                  numFrames;
    int                  numbones;
    int                  ofsChannelNames;
    int                  ofsFrames;
    size_t               need;

    if (!image || bytes < 112) {
        return NULL;
    }

    numFrames       = LittleLong(*(const int *)((const byte *)image + 76));
    numbones        = LittleLong(*(const int *)((const byte *)image + 80));
    ofsChannelNames = LittleLong(*(const int *)((const byte *)image + 104));
    ofsFrames       = LittleLong(*(const int *)((const byte *)image + 108));

    if (numFrames <= 0 || numbones <= 0 || numbones > TIKI_MAX_BONES || ofsFrames <= 0
        || (size_t)ofsFrames >= bytes) {
        return NULL;
    }

    if (ofsChannelNames < 112 || (size_t)ofsChannelNames + (size_t)numbones * 36 > bytes) {
        return NULL;
    }

    {
        const size_t stride = SKAN_FRAME_HEADER_SIZE + (size_t)numbones * sizeof(skelBone_t);

        if ((size_t)numFrames > ((size_t)bytes - (size_t)ofsFrames) / stride) {
            const int maxFull = (int)(((size_t)bytes - (size_t)ofsFrames) / stride);

            if (maxFull <= 0) {
                return NULL;
            }

            numFrames = maxFull;
        }

        need = (size_t)ofsFrames + (size_t)numFrames * stride;
        if (need > bytes || need < (size_t)ofsFrames
            || (size_t)ofsChannelNames + (size_t)numbones * 36 > need) {
            return NULL;
        }
    }

    if (m_skanRetainedCount >= MAX_TIKI_ALIASES) {
        Com_DPrintf("TIKI_Skan_RetainNative: native SKAN image table full (%d)\n", MAX_TIKI_ALIASES);
        return NULL;
    }

    copy = (skelAnimHeaderRec_t *)Skel_Alloc(need);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, image, need);
    TIKI_SwapAnimSkel(copy);
    copy->numFrames = numFrames;

    m_skanRetained[m_skanRetainedCount].owner = NULL;
    m_skanRetained[m_skanRetainedCount].hdr   = copy;
    m_skanRetained[m_skanRetainedCount].bytes = need;
    m_skanRetainedCount++;

    return copy;
}

void TIKI_Skan_Bind(skelAnimDataGameHeader_t *owner, skelAnimHeaderRec_t *hdr)
{
    int i;

    if (!owner || !hdr) {
        return;
    }

    for (i = 0; i < m_skanRetainedCount; i++) {
        if (m_skanRetained[i].hdr == hdr) {
            m_skanRetained[i].owner = owner;
            return;
        }
    }
}

void TIKI_Skan_Release(const skelAnimDataGameHeader_t *owner)
{
    int i;

    if (!owner) {
        return;
    }

    for (i = 0; i < m_skanRetainedCount; i++) {
        if (m_skanRetained[i].owner == owner) {
            Skel_Free(m_skanRetained[i].hdr);
            m_skanRetained[i]       = m_skanRetained[m_skanRetainedCount - 1];
            m_skanRetainedCount--;
            return;
        }
    }
}

skelAnimHeaderRec_t *TIKI_Skan_Find(const skelAnimDataGameHeader_t *owner)
{
    int i;

    if (!owner) {
        return NULL;
    }

    for (i = 0; i < m_skanRetainedCount; i++) {
        if (m_skanRetained[i].owner == owner) {
            return m_skanRetained[i].hdr;
        }
    }

    return NULL;
}

skelAnimHeaderRec_t *TIKI_Skan_FindByAnim(dtiki_t *tiki, int animnum)
{
    if (!tiki || !tiki->a) {
        return NULL;
    }

    if (animnum < 0 || animnum >= tiki->a->num_anims) {
        return NULL;
    }

    return TIKI_Skan_Find(SkeletorCacheGetData(tiki->a->m_aliases[animnum]));
}

void TIKI_SwapAnimSkel(skelAnimHeaderRec_t *hdr)
{
    skelFrame_t *frame;
    float       *f;
    int          i;
    int          j;
    int          k;
    int          ofsFrames;

    hdr->ident           = LittleLong(hdr->ident);
    hdr->version         = LittleLong(hdr->version);
    hdr->numFrames       = LittleLong(hdr->numFrames);
    hdr->numbones        = LittleLong(hdr->numbones);
    hdr->totaltime       = LittleFloat(hdr->totaltime);
    hdr->frametime       = LittleFloat(hdr->frametime);
    hdr->totaldelta[0]   = LittleFloat(hdr->totaldelta[0]);
    hdr->totaldelta[1]   = LittleFloat(hdr->totaldelta[1]);
    hdr->totaldelta[2]   = LittleFloat(hdr->totaldelta[2]);
    hdr->ofsChannelNames = LittleLong(hdr->ofsChannelNames);
    ofsFrames            = LittleLong(hdr->ofsFrames);
    hdr->ofsFrames       = ofsFrames;

    frame = (skelFrame_t *)((byte *)hdr + ofsFrames);
    for (i = 0; i < hdr->numFrames; i++) {
        frame->radius = LittleFloat(frame->radius);

        f = (float *)frame;
        for (j = 0; j < 3; j++) {
            f[0] = LittleFloat(f[0]);
            f[3] = LittleFloat(f[3]);
            f[7] = LittleFloat(f[7]);
            f++;
        }

        frame = (skelFrame_t *)((byte *)frame + sizeof(skelFrame_t));

        f = (float *)frame;
        for (k = 0; k < hdr->numbones; k++) {
            unsigned short *s = (unsigned short *)f;

            for (j = 0; j < 4; j++) {
                s[0] = LittleShort(s[0]);
                if (j < 3) {
                    s[4] = LittleShort(s[4]);
                }
                s++;
            }

            f += 4; 
        }
    }

    f = (float *)((byte *)hdr + hdr->ofsChannelNames);
    for (i = 0; i < hdr->numbones; i++) {
        f[0] = LittleFloat(f[0]);
        f += 9;
    }
}

qboolean TIKI_ValidateSkelAnimAndFrame(
    dtiki_t              *tiki,
    int                   animnum,
    int                   framenum,
    skelAnimHeaderRec_t **hdrOut,
    skelFrameRec_t      **frameOut
)
{
    skelAnimHeaderRec_t *hdr;
    const char          *animName;

    if (!tiki || !hdrOut || !frameOut) {
        return qfalse;
    }

    *hdrOut   = NULL;
    *frameOut = NULL;

    if (!tiki->a || animnum >= tiki->a->num_anims || animnum < 0) {
        Com_WPrintf("TIKI_ValidateSkelAnimAndFrame: no such anim %d for model '%s'\n", animnum, tiki->name);
        return qfalse;
    }

    hdr = TIKI_Skan_FindByAnim(tiki, animnum);
    if (!hdr) {
        animName = TIKI_Anim_NameForNum(tiki, animnum);
        Com_WPrintf(
            "TIKI_ValidateSkelAnimAndFrame: couldn't get anim %d (alias: %s) in '%s'\n",
            animnum,
            animName ? animName : "",
            tiki->name
        );
        return qfalse;
    }

    if (framenum >= hdr->numFrames || framenum < 0) {
        Com_WPrintf(
            "TIKI_ValidateSkelAnimAndFrame: no such frame %d in anim %d (name: %s) in tiki '%s'\n",
            framenum,
            animnum,
            hdr->name,
            tiki->name
        );
        return qfalse;
    }

    *hdrOut   = hdr;
    *frameOut = (skelFrameRec_t *)((byte *)hdr + hdr->ofsFrames + framenum * (SKAN_FRAME_HEADER_SIZE + (int)sizeof(skelBone_t) * hdr->numbones));
    return qtrue;
}

#define TIKI_SKAN_QUAT_SCALE   (1.0f / 32767.0f)
#define TIKI_SKAN_OFFSET_SCALE (1.0f / 63.0f)

static void TIKI_Skan_DequantRow(const short int *row, float *quat, float *offset)
{
    int i;

    for (i = 0; i < 4; i++) {
        quat[i] = TIKI_SKAN_QUAT_SCALE * (float)row[i];
    }
    for (i = 0; i < 3; i++) {
        offset[i] = TIKI_SKAN_OFFSET_SCALE * (float)row[4 + i];
    }
}

static void TIKI_Skan_QuatMul(const float *q, const float *p, float *r)
{
    r[0] = q[3] * p[0] + q[0] * p[3] + q[1] * p[2] - q[2] * p[1];
    r[1] = q[3] * p[1] + q[1] * p[3] + q[2] * p[0] - q[0] * p[2];
    r[2] = q[3] * p[2] + q[2] * p[3] + q[0] * p[1] - q[1] * p[0];
    r[3] = q[3] * p[3] - q[0] * p[0] - q[1] * p[1] - q[2] * p[2];
}

const short int *TIKI_Skan_RecordRow(
    skelAnimHeader_t *hdr,
    skelFrame_t      *frameRec,
    const int        *mapping,
    int               bone,
    float             baseLen,
    const char       *boneName,
    const short int  *bindRow,
    float            *outRatio
)
{
    const short int *row = NULL;
    float            ratio = 1.0f;
    int              mapped;

    mapped = mapping ? mapping[bone] : bone;

    if (mapped != -1 && hdr) {
        float chanLen = *(const float *)((const byte *)hdr + hdr->ofsChannelNames + 36 * mapped);

        if (chanLen != 0.0f && chanLen != baseLen) {
            ratio = baseLen / chanLen;
        }
    }

    if (boneName && !strnicmp(boneName, "tag", 3)) {
        ratio = 1.0f;
    }

    if (mapped != -1 && frameRec) {
        row = (const short int *)((const byte *)frameRec + 40 + 16 * mapped);
    }
    if (!row) {
        row = bindRow;
    }

    *outRatio = ratio;
    return row;
}

typedef struct {
    skelAnimHeader_t *mainHdr;
    skelFrame_t      *mainRec;
    const int        *mainMap;

    skelAnimHeader_t *aHdr;    
    skelFrame_t      *aRec;
    const int        *aMap;

    skelAnimHeader_t *crossHdr;
    skelFrame_t      *crossRec;
    const int        *crossMap;

    int               mode;     
    float             innerT;  
    float             outerT;  
} skelSolveChannel_t;

static qboolean TIKI_Skan_SetupChannel(
    dtiki_t            *tiki,
    int                 anim,
    int                 frame,
    int                 oldanim,
    int                 oldframe,
    float               framelerp,
    int                 cbAnim,
    int                 cbFrame,
    float               cbLerp,
    qboolean            resolveCb,
    skelSolveChannel_t *ch
)
{
    memset(ch, 0, sizeof(*ch));

    if (!TIKI_ValidateSkelAnimAndFrame(tiki, anim, frame, &ch->mainHdr, &ch->mainRec)) {
        return qfalse;
    }
    if (tiki->boneMappings && anim >= 0 && anim < tiki->a->num_anims) {
        ch->mainMap = TIKI_GetAnimBoneMapping(tiki->boneMappings[anim]);
    }

    TIKI_ValidateSkelAnimAndFrame(tiki, oldanim, oldframe, &ch->aHdr, &ch->aRec);
    if (tiki->boneMappings && oldanim >= 0 && oldanim < tiki->a->num_anims) {
        ch->aMap = TIKI_GetAnimBoneMapping(tiki->boneMappings[oldanim]);
    }

    if (!ch->aRec || ch->aRec == ch->mainRec) {
        framelerp = 0.0f;
    }

    if (resolveCb) {
        TIKI_ValidateSkelAnimAndFrame(tiki, cbAnim, cbFrame, &ch->crossHdr, &ch->crossRec);
        if (tiki->boneMappings && cbAnim >= 0 && cbAnim < tiki->a->num_anims) {
            ch->crossMap = TIKI_GetAnimBoneMapping(tiki->boneMappings[cbAnim]);
        }
    }

    if (cbLerp < 0.0f) {
        if (framelerp == 0.0f) {
            ch->mode = 1;
        } else {
            ch->mode   = 2;
            ch->innerT = 1.0f - framelerp;
        }
        return qtrue;
    }

    if (!ch->crossRec) {
        if (framelerp == 0.0f) {
            ch->mode = 1;
        } else {
            ch->mode   = 2;
            ch->innerT = 1.0f - framelerp;
        }
        return qtrue;
    }

    if (framelerp == 0.0f) {
        ch->mode   = 2;
        ch->aHdr   = ch->crossHdr;
        ch->aRec   = ch->crossRec;
        ch->innerT = cbLerp;
    } else {
        ch->mode   = 3;
        ch->innerT = 1.0f - framelerp;
        ch->outerT = cbLerp;
    }

    return qtrue;
}

static void TIKI_Skan_SolveChannelBone(
    skelHeaderGame_t         *skel,
    int                       bone,
    float                     baseLen,
    const char               *boneName,
    const skelSolveChannel_t *ch,
    float                    *out_quat,
    float                    *out_offset
)
{
    const short int *bindRow = skel->pBones[bone].hasBaseRow ? skel->pBones[bone].baseRow : NULL;
    const short int *row;
    float            qA[4];
    float            oA[3];
    float            qB[4];
    float            oB[3];
    float            qC[4];
    float            oC[3];
    float            ratio;
    int              i;

    row = TIKI_Skan_RecordRow(
        ch->mainHdr, ch->mainRec, ch->mainMap, bone, baseLen, boneName, bindRow, &ratio
    );

    if (!row) {
        out_quat[0] = 0.0f;
        out_quat[1] = 0.0f;
        out_quat[2] = 0.0f;
        out_quat[3] = 1.0f;
        VectorCopy(skel->pBones[bone].offset, out_offset);
        return;
    }

    TIKI_Skan_DequantRow(row, qB, oB);
    for (i = 0; i < 3; i++) {
        oB[i] *= ratio;
    }

    if (ch->mode != 1) {
        row = TIKI_Skan_RecordRow(
            ch->aHdr, ch->aRec, ch->aMap, bone, baseLen, boneName, bindRow, &ratio
        );

        if (!row) {
            qA[0] = 0.0f;
            qA[1] = 0.0f;
            qA[2] = 0.0f;
            qA[3] = 1.0f;
            VectorCopy(skel->pBones[bone].offset, oA);
        } else {
            TIKI_Skan_DequantRow(row, qA, oA);
            for (i = 0; i < 3; i++) {
                oA[i] *= ratio;
            }
        }

        SlerpQuaternion(qA, qB, ch->innerT, qB);

        for (i = 0; i < 3; i++) {
            oB[i] = oA[i] * (1.0f - ch->innerT) + oB[i] * ch->innerT;
        }
    }

    if (ch->mode == 3) {
        row = TIKI_Skan_RecordRow(
            ch->crossHdr, ch->crossRec, ch->crossMap, bone, baseLen, boneName, bindRow, &ratio
        );

        if (!row) {
            qC[0] = 0.0f;
            qC[1] = 0.0f;
            qC[2] = 0.0f;
            qC[3] = 1.0f;
            VectorCopy(skel->pBones[bone].offset, oC);
        } else {
            TIKI_Skan_DequantRow(row, qC, oC);
            for (i = 0; i < 3; i++) {
                oC[i] *= ratio;
            }
        }

        SlerpQuaternion(qC, qB, ch->outerT, out_quat);

        for (i = 0; i < 3; i++) {
            out_offset[i] = oC[i] * (1.0f - ch->outerT) + oB[i] * ch->outerT;
        }
    } else {
        memcpy(out_quat, qB, sizeof(qB));
        VectorCopy(oB, out_offset);
    }
}

int TIKI_SkelBonesFromRecords(
    dtiki_t         *tiki,
    int              anim,
    int              frame,
    int              oldanim,
    int              oldframe,
    float            framelerp,
    int              crossblend_anim,
    int              crossblend_frame,
    float            crossblend_lerp,
    int              torso_anim,
    int              torso_frame,
    int              torso_oldanim,
    int              torso_oldframe,
    float            torso_framelerp,
    int              torso_crossblend_anim,
    int              torso_crossblend_frame,
    float            torso_crossblend_lerp,
    qboolean         uselegs,
    qboolean         usetorso,
    const int       *bone_tag,
    const vec4_t    *bone_quat,
    const vec3_t     localOrigin,
    skelBoneCache_t *bones,
    int              maxBones,
    float           *radius,
    vec3_t          *mins,
    vec3_t          *maxs
)
{
    skelHeaderGame_t   *skel;
    skelSolveChannel_t  legsCh;
    skelSolveChannel_t  torsoCh;
    skelSolveChannel_t *ch;
    const char         *boneName;
    float              *world;
    float               localQuat[4];
    float               localOffset[3];
    float               parentQuat[4];
    float               parentOffset[3];
    float               cq[4];
    float               pw;
    qboolean            legs;
    int                 parent;
    int                 numBones;
    int                 b;
    int                 i;
    int                 n;

    skel = TIKI_GetSkel(tiki->skelIndex);
    if (!skel) {
        return 0;
    }

    if (!tiki->a) {
        return 0;
    }

    memset(&legsCh, 0, sizeof(legsCh));
    memset(&torsoCh, 0, sizeof(torsoCh));

    if (uselegs) {
        if (!TIKI_Skan_SetupChannel(
                tiki,
                anim,
                frame,
                oldanim,
                oldframe,
                framelerp,
                crossblend_anim,
                crossblend_frame,
                crossblend_lerp,
                (crossblend_lerp >= 0.0f) ? qtrue : qfalse,
                &legsCh
            )) {
            return 0;
        }
    }

    if (usetorso) {
        if (!TIKI_Skan_SetupChannel(
                tiki,
                torso_anim,
                torso_frame,
                torso_oldanim,
                torso_oldframe,
                torso_framelerp,
                torso_crossblend_anim,
                torso_crossblend_frame,
                torso_crossblend_lerp,
                qtrue,
                &torsoCh
            )) {
            return 0;
        }
    }

    if (!uselegs && !usetorso) {
        return 0;
    }

    ch = uselegs ? &legsCh : &torsoCh;

    if (radius && ch->mainRec) {
        *radius = ch->mainRec->radius;
    }
    if (mins && maxs && ch->mainRec) {
        VectorCopy(ch->mainRec->bounds[0], *mins);
        VectorCopy(ch->mainRec->bounds[1], *maxs);
    }

    numBones = skel->numBones;
    if (numBones > maxBones) {
        TIKI_Error("R_AddSkelSurfaces: too many skeleton models visible on '%s'\n", tiki->a->name);
        return 0;
    }

    world = (float *)Skel_Alloc(numBones * 7 * sizeof(float));
    if (!world) {
        return 0;
    }
    memset(world, 0, numBones * 7 * sizeof(float));

    for (b = 0; b < numBones; b++) {
        float *w = &world[b * 7];

        boneName = skeletor_c::m_boneNames.FindName(skel->pBones[b].channel);

        if (uselegs && usetorso) {
            legs = (skel->pBones[b].ef2BodyPart == 1);
        } else if (uselegs) {
            legs = qtrue;
        } else {
            legs = qfalse;
        }

        TIKI_Skan_SolveChannelBone(
            skel,
            b,
            VectorLength(skel->pBones[b].offset),
            boneName,
            legs ? &legsCh : &torsoCh,
            localQuat,
            localOffset
        );

        parent = skel->pBones[b].skelParent;

        if (parent < 0 || parent >= numBones || parent == b) {
            memcpy(w, localQuat, 4 * sizeof(float));
            w[4] = localOffset[0];
            w[5] = localOffset[1];
            w[6] = localOffset[2];
        } else {
            pw = localQuat[3];
            for (i = 0; i < 4; i++) {
                parentQuat[i] = world[parent * 7 + i];
            }
            parentOffset[0] = world[parent * 7 + 4];
            parentOffset[1] = world[parent * 7 + 5];
            parentOffset[2] = world[parent * 7 + 6];

            TIKI_Skan_QuatMul(localQuat, parentQuat, w);

            {
                float px = localOffset[0];
                float py = localOffset[1];
                float pz = localOffset[2];
                SkelQuat  q;
                SkelMat3  m;
                q.Set(parentQuat);
                q.GetMat3(m);
                w[4] = px * m.val[0][0] + py * m.val[1][0] + pz * m.val[2][0] + parentOffset[0];
                w[5] = px * m.val[0][1] + py * m.val[1][1] + pz * m.val[2][1] + parentOffset[1];
                w[6] = px * m.val[0][2] + py * m.val[1][2] + pz * m.val[2][2] + parentOffset[2];
            }
        }

        for (n = NUM_BONE_CONTROLLERS - 1; n >= 0; n--) {
            if (bone_tag[n] != b) {
                continue;
            }
            cq[0] = bone_quat[n][0];
            cq[1] = bone_quat[n][1];
            cq[2] = bone_quat[n][2];
            cq[3] = bone_quat[n][3];
            TIKI_Skan_QuatMul(w, cq, w);
            break;
        }

        if (parent < 0 || parent >= numBones || parent == b) {
            for (i = 0; i < 3; i++) {
                w[4 + i] += localOrigin[i];
            }
        }
    }

    for (b = 0; b < numBones; b++) {
        float       *w = &world[b * 7];
        SkelQuat     q;
        SkelMat3     m;
        skelBoneCache_t *out = &bones[b];
        int          r;
        int          c;

        q.Set(w);
        q.GetMat3(m);

        out->offset[0] = w[4];
        out->offset[1] = w[5];
        out->offset[2] = w[6];
        out->offset[3] = 0.0f;

        for (r = 0; r < 3; r++) {
            for (c = 0; c < 3; c++) {
                out->matrix[r][c] = m.val[r][c];
            }
            out->matrix[r][3] = 0.0f;
        }
    }

    Skel_Free(world);
    (void)pw;
    return numBones;
}

skelMorphHeader_t *TIKI_BuildMorphHeader(const void *morphDataVoid, size_t *outSize)
{
    const skmMorphData_t *morphData = (const skmMorphData_t *)morphDataVoid;

    skelMorphHeader_t       *hdr;
    skelMorphTargetRecord_t *targets;
    int                     *remap;
    size_t                   size;
    size_t                   off;
    int                      i;
    int                      k;

    if (!morphData || morphData->numTargets <= 0 || !morphData->targets) {
        return NULL;
    }

    size = sizeof(skelMorphHeader_t);
    off  = size;

    {
        size_t remapBytes = (size_t)morphData->numMorphVertices * sizeof(int);
        size += remapBytes;
        (void)remapBytes;
    }
    size += (size_t)morphData->numTargets * sizeof(skelMorphTargetRecord_t);
    for (i = 0; i < morphData->numTargets; i++) {
        size += (size_t)morphData->targets[i].numVerts * sizeof(int);
    }

    if (size > (size_t)INT_MAX) {
        return NULL;
    }

    hdr = (skelMorphHeader_t *)Skel_Alloc(size);
    if (!hdr) {
        return NULL;
    }
    memset(hdr, 0, size);

    hdr->numMorphVertices = morphData->numMorphVertices;
    hdr->numTargets       = morphData->numTargets;

    off = sizeof(skelMorphHeader_t);
    hdr->ofsVertexRemap = (int)off;
    off += (size_t)morphData->numMorphVertices * sizeof(int);
    hdr->ofsTargets = (int)off;

    remap = (int *)((byte *)hdr + hdr->ofsVertexRemap);
    for (i = 0; i < morphData->numMorphVertices; i++) {
        remap[i] = -1;
    }

    targets = (skelMorphTargetRecord_t *)((byte *)hdr + hdr->ofsTargets);
    for (i = 0; i < morphData->numTargets; i++) {
        const skmMorphTarget_t *src = &morphData->targets[i];
        int                    *dst;
        int                     listOff;

        Q_strncpyz(targets[i].name, src->name, sizeof(targets[i].name));
        targets[i].numVerts = src->numVerts;

        listOff = (int)off;
        off += (size_t)src->numVerts * sizeof(int);
        targets[i].ofsIndices = listOff;

        dst = (int *)((byte *)hdr + listOff);
        for (k = 0; k < src->numVerts; k++) {
            dst[k] = src->verts[k].index;
            if (dst[k] >= 0 && dst[k] < morphData->numMorphVertices) {
                remap[dst[k]] = k;
            }
        }
    }

    if (outSize) {
        *outSize = size;
    }
    return hdr;
}
