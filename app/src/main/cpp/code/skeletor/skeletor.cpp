

#include "q_shared.h"
#include "qcommon.h"
#include "skeletor.h"
#include "../corepp/tiki.h"
#include "../tiki/tiki_skan.h"

#define EPSILON 0.000000000001f

ChannelNameTable *skeletor_c::ChannelNames()
{
    return &skel_ChannelNames;
}

int skelAnimDataGameHeader_s::GetFrameNums(
    float timeSeconds, float timeTolerance, int *beforeFrame, int *afterFrame, float *beforeWeight, float *afterWeight
)
{
    int frameNum1;
    int frameNum2;

    frameNum1 = (int)(timeSeconds / frameTime);
    if (frameNum1 < 0) {
        frameNum1 = 0;
    }
    frameNum2 = frameNum1 + 1;

    *afterWeight = (timeSeconds / frameTime) - (float)frameNum1;

    if ((1.0 - *afterWeight) * frameTime < timeTolerance || *afterWeight * frameTime < timeTolerance) {
        if (*afterWeight > 0.5) {
            frameNum1++;
        }

        if (frameNum1 >= numFrames) {
            if ((flags & TAF_DELTADRIVEN)) {
                frameNum1 %= numFrames;
            } else {
                frameNum1 = numFrames - 1;
            }
        }

        *beforeFrame  = frameNum1;
        *beforeWeight = 1.0;
        *afterFrame   = 0;
        *afterWeight  = 0.0;

        return 1;
    } else if (frameNum2 >= numFrames) {
        if ((flags & TAF_DELTADRIVEN)) {
            frameNum2 %= numFrames;
        } else {
            frameNum2 = numFrames - 1;
        }

        if (frameNum1 >= numFrames) {
            if ((flags & TAF_DELTADRIVEN)) {
                frameNum1 %= numFrames;
            } else {
                *beforeFrame  = numFrames - 1;
                *beforeWeight = 1.0;
                return 1;
            }
        }
    }

    *beforeFrame  = frameNum1;
    *afterFrame   = frameNum2;
    *beforeWeight = 1.0 - *afterWeight;

    return 2;
}

SkelVec3 skelAnimDataGameHeader_s::GetDeltaOverTime(float time1, float time2)
{
    float    deltaWeight1;
    int      frameNum1;
    float    deltaWeight2;
    int      frameNum2;
    SkelVec3 delta;
    int      currFrame;
    float    s, d;

    deltaWeight1 = time1 / frameTime;
    deltaWeight2 = time2 / frameTime;
    frameNum1    = ceil(deltaWeight1);
    frameNum2    = ceil(deltaWeight2);

    d = frameNum1 - deltaWeight1;
    s = 1.0 - (frameNum2 - deltaWeight2);

    if (frameNum1 < frameNum2) {
        delta = m_frame[frameNum1 % numFrames].delta;
        VectorScale(delta, d, delta);

        for (currFrame = frameNum1 + 1; currFrame < frameNum2; currFrame++) {
            delta += m_frame[currFrame % numFrames].delta;
        }

        delta.x += m_frame[frameNum2 % numFrames].delta.x * s;
        delta.y += m_frame[frameNum2 % numFrames].delta.y * s;
        delta.z += m_frame[frameNum2 % numFrames].delta.z * s;
    } else {
        s       = s - (1.0 - d);
        delta.x = m_frame[frameNum2 % numFrames].delta.x * s;
        delta.y = m_frame[frameNum2 % numFrames].delta.y * s;
        delta.z = m_frame[frameNum2 % numFrames].delta.z * s;
    }

    if (delta.x > -0.001f && delta.x < 0.001f) {
        delta.x = 0;
    }
    if (delta.y > -0.001f && delta.y < 0.001f) {
        delta.y = 0;
    }
    if (delta.z > -0.001f && delta.z < 0.001f) {
        delta.z = 0;
    }

    return delta;
}

float skelAnimDataGameHeader_s::GetAngularDeltaOverTime(float time1, float time2)
{
    float deltaWeight1;
    int   frameNum1;
    float deltaWeight2;
    int   frameNum2;
    float delta;
    int   currFrame;
    float s, d;

    deltaWeight1 = time1 / frameTime;
    deltaWeight2 = time2 / frameTime;
    frameNum1    = ceil(deltaWeight1);
    frameNum2    = ceil(deltaWeight2);

    d     = frameNum1 - deltaWeight1;
    s     = 1.0 - (frameNum2 - deltaWeight2);
    delta = 0;

    if (frameNum1 < frameNum2) {
        delta = m_frame[frameNum1 % numFrames].angleDelta * d;

        for (currFrame = frameNum1 + 1; currFrame < frameNum2; currFrame++) {
            delta += m_frame[currFrame % numFrames].angleDelta;
        }

        delta += m_frame[frameNum2 % numFrames].angleDelta * s;
    } else {
        s     = s - (1.0 - d);
        delta = m_frame[frameNum2 % numFrames].angleDelta;
    }

    if (delta > -0.001f && delta < 0.001f) {
        delta = 0;
    }

    return delta;
}

skelAnimDataGameHeader_t *skelAnimDataGameHeader_s::AllocAnimData(size_t numFrames, size_t numChannels)
{
    skelAnimDataGameHeader_t *data;

    const size_t animSize =
        sizeof(skelAnimDataGameHeader_t) + (16 * (3 * numFrames - 3)) + 16 * numChannels * numFrames;

    data        = (skelAnimDataGameHeader_t *)Skel_Alloc(animSize);
    data->flags = 0;
    data->channelList.InitChannels();
    data->nBytesUsed = animSize;
    data->numFrames  = numFrames;
    data->tanFrames    = NULL;
    data->numTanFrames = 0;
    data->tanTags      = NULL;
    data->numTanTags   = 0;
    return data;
}

skelAnimDataGameHeader_s *skelAnimDataGameHeader_s::AllocRLEChannelData(size_t numChannels)
{
    int                       animSize;
    skelAnimDataGameHeader_t *data;

    animSize             = sizeof(skelAnimDataGameHeader_t) + (numChannels - 1) * sizeof(skanChannelHdr);
    data                 = (skelAnimDataGameHeader_t *)Skel_Alloc(animSize);
    data->flags          = 0;
    data->nTotalChannels = numChannels;
    data->channelList.InitChannels();
    data->nBytesUsed = animSize;
    data->tanFrames    = NULL;
    data->numTanFrames = 0;
    data->tanTags      = NULL;
    data->numTanTags   = 0;
    return data;
}

skelAnimDataGameHeader_t *skeletor_c::BuildTANAnim(void *buffer, int iBuffLength, const char *path)
{
    const byte *buf = (const byte *)buffer;
    int         numFrames, ofsFrames;
    float       totaltime;
    float       totalDelta[3];

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wself-assign"
#endif
	memcpy(&numFrames, buf + 0x48, 4);
    numFrames = LittleLong(numFrames);
    memcpy(&totaltime, buf + 0x54, 4);
    totaltime = LittleFloat(totaltime);
    for (int i = 0; i < 3; i++) {
        float f;
        memcpy(&f, buf + 0x58 + i * 4, 4);
        totalDelta[i] = LittleFloat(f);
    }
    memcpy(&ofsFrames, buf + 0x64, 4);
    ofsFrames = LittleLong(ofsFrames);

    if (numFrames < 1) {
        return NULL;
    }

    // .tan tags (numTags@0x4C, ofsTags@0x6C) -- must mirror TIKI_LoadTAN, which creates
    int numTags, ofsTags;
    memcpy(&numTags, buf + 0x4C, 4);
    numTags = LittleLong(numTags);
    memcpy(&ofsTags, buf + 0x6C, 4);
    ofsTags = LittleLong(ofsTags);
    if (numTags < 0) {
        numTags = 0;
    }
    if (numTags > 0 && (ofsTags < 0 || TAN_TAG_NAME_OFS(ofsTags, numFrames, numTags) > (size_t)iBuffLength)) {
        numTags = 0;
    }

    const int    numChannels = 2 + 2 * numTags;
    const size_t frameSz     = sizeof(skelAnimFileFrame_t);
    const size_t headerSz    = sizeof(skelAnimDataFileHeader_t) + (numFrames - 1) * frameSz;
    const size_t chanDataSz  = (size_t)numFrames * numChannels * sizeof(skelAnimChannel_t);
    const size_t namesSz     = (size_t)numChannels * sizeof(skelChannelName_t);
    const size_t totalSz     = headerSz + chanDataSz + namesSz;

    byte *fbuf = (byte *)Skel_Alloc(totalSz);
    memset(fbuf, 0, totalSz);

    skelAnimDataFileHeader_t *fh = (skelAnimDataFileHeader_t *)fbuf;
    fh->ident           = 0;
    fh->version         = 0;
    fh->flags           = 0;
    fh->nBytesUsed      = (int)totalSz;
    fh->frameTime       = (totaltime > 0.0f) ? (totaltime / numFrames) : 0.1f;
    fh->totalDelta.set(totalDelta[0], totalDelta[1], totalDelta[2]);
    fh->totalAngleDelta = 0.0f;
    fh->numChannels     = numChannels;
    fh->numFrames       = numFrames;
    fh->ofsChannelNames = (int)(headerSz + chanDataSz);

    const byte *pf = buf + ofsFrames;
    for (int f = 0; f < numFrames; f++, pf += 68) {
        skelAnimFileFrame_t *ff = &fh->frame[f];
        for (int k = 0; k < 3; k++) {
            float lo, hi, d;
            memcpy(&lo, pf + 0x00 + k * 4, 4);
            memcpy(&hi, pf + 0x0C + k * 4, 4);
            memcpy(&d, pf + 0x30 + k * 4, 4);
            ff->bounds[0][k] = LittleFloat(lo);
            ff->bounds[1][k] = LittleFloat(hi);
            ff->delta[k]     = LittleFloat(d);
        }
        float radius;
        memcpy(&radius, pf + 0x3C, 4);
        ff->radius       = LittleFloat(radius);
        ff->angleDelta   = 0.0f;
        ff->iOfsChannels = 0;
    }

    skelAnimChannel_t *chan = (skelAnimChannel_t *)(fbuf + headerSz);
    for (int f = 0; f < numFrames; f++) {
        float *rot = chan[f * numChannels + 0];
        float *pos = chan[f * numChannels + 1];
        rot[0] = 0.0f; rot[1] = 0.0f; rot[2] = 0.0f; rot[3] = 1.0f;
        pos[0] = 0.0f; pos[1] = 0.0f; pos[2] = 0.0f; pos[3] = 0.0f;

        for (int t = 0; t < numTags; t++) {
            const byte *pTag = buf + TAN_TAG_POSE_OFS(ofsTags, numFrames, t, f);
            float       origin[3];
            float       axis[3][3];
            for (int k = 0; k < 3; k++) {
                float v;
                memcpy(&v, pTag + k * 4, 4);
                origin[k] = LittleFloat(v);
            }
            for (int r = 0; r < 3; r++) {
                for (int c = 0; c < 3; c++) {
                    float v;
                    memcpy(&v, pTag + 12 + (r * 3 + c) * 4, 4);
                    axis[r][c] = LittleFloat(v);
                }
            }

            float *trot = chan[f * numChannels + 2 + 2 * t];
            float *tpos = chan[f * numChannels + 3 + 2 * t];
            MatToQuat(axis, trot);
            tpos[0] = origin[0]; tpos[1] = origin[1]; tpos[2] = origin[2]; tpos[3] = 0.0f;
        }
    }

    skelChannelName_t *names = (skelChannelName_t *)(fbuf + fh->ofsChannelNames);
    Q_strncpyz(names[0], TAN_BONE_NAME " rot", sizeof(names[0]));
    Q_strncpyz(names[1], TAN_BONE_NAME " pos", sizeof(names[1]));
    for (int t = 0; t < numTags; t++) {
        const byte *pTag = buf + TAN_TAG_NAME_OFS(ofsTags, numFrames, t);
        char        tagName[TAN_TAG_NAME_SIZE];
        Q_strncpyz(tagName, (const char *)pTag, sizeof(tagName));
        Com_sprintf(names[2 + 2 * t], sizeof(names[0]), "%s rot", tagName);
        Com_sprintf(names[3 + 2 * t], sizeof(names[0]), "%s pos", tagName);
    }

    skelAnimDataGameHeader_t *enAnim = skeletor_c::ConvertSkelFileToGame(fh, (int)totalSz, path);
    Skel_Free(fbuf);

    if (enAnim) {
        const byte *rec = buf + ofsFrames;

        enAnim->numTanFrames = numFrames;
        enAnim->tanFrames    = (tikiFrame_t *)Skel_Alloc((size_t)numFrames * sizeof(tikiFrame_t));

        ClearBounds(enAnim->bounds[0].val, enAnim->bounds[1].val);

        for (int f = 0; f < numFrames; f++, rec += 68) {
            tikiFrame_t *tf = &enAnim->tanFrames[f];
            int          k;

            for (k = 0; k < 3; k++) {
                memcpy(&tf->bounds[0][k], rec + 0x00 + k * 4, 4);
                tf->bounds[0][k] = LittleFloat(tf->bounds[0][k]);
                memcpy(&tf->bounds[1][k], rec + 0x0C + k * 4, 4);
                tf->bounds[1][k] = LittleFloat(tf->bounds[1][k]);
                memcpy(&tf->scale[k], rec + 0x18 + k * 4, 4);
                tf->scale[k] = LittleFloat(tf->scale[k]);
                memcpy(&tf->offset[k], rec + 0x24 + k * 4, 4);
                tf->offset[k] = LittleFloat(tf->offset[k]);
                memcpy(&tf->delta[k], rec + 0x30 + k * 4, 4);
                tf->delta[k] = LittleFloat(tf->delta[k]);
            }

            memcpy(&tf->radius, rec + 0x3C, 4);
            tf->radius = LittleFloat(tf->radius);
            memcpy(&tf->frametime, rec + 0x40, 4);
            tf->frametime = LittleFloat(tf->frametime);

            for (k = 0; k < 3; k++) {
                if (tf->bounds[0][k] < enAnim->bounds[0].val[k]) {
                    enAnim->bounds[0].val[k] = tf->bounds[0][k];
                }

                if (tf->bounds[1][k] > enAnim->bounds[1].val[k]) {
                    enAnim->bounds[1].val[k] = tf->bounds[1][k];
                }
            }
        }

        if (numTags > 0) {
            enAnim->numTanTags = numTags;
            enAnim->tanTags    = (tikiTanTagRecords_t *)Skel_Alloc(tikiTanTagRecords_t::BlockSize(numTags, numFrames));

            enAnim->tanTags->numTags   = numTags;
            enAnim->tanTags->numFrames = numFrames;

            for (int t = 0; t < numTags; t++) {
                char        tagName[TAN_TAG_NAME_SIZE];
                const byte *pTag = buf + TAN_TAG_NAME_OFS(ofsTags, numFrames, t);

                Q_strncpyz(tagName, (const char *)pTag, sizeof(tagName));
                Q_strncpyz((char *)enAnim->tanTags->Name(t), tagName, 64);

                for (int f = 0; f < numFrames; f++) {
                    const byte *pPose = buf + TAN_TAG_POSE_OFS(ofsTags, numFrames, t, f);
                    float       v;

                    for (int k = 0; k < 12; k++) {
                        memcpy(&v, pPose + k * 4, 4);
                        enAnim->tanTags->Pose(t, f)[k] = LittleFloat(v);
                    }
                }
            }
        }
    }

    return enAnim;
}

skeletor_c::skeletor_c(dtiki_t *tiki)
{
    int               numBones;
    int               mesh;
    skelHeaderGame_t *skelmodel;

    m_Tiki = tiki;

    m_morphTargetList.InitChannels();
    m_morphTargetList.ZeroChannels();

    m_frameBounds[0].set(-32, -32, 0);
    m_frameBounds[1].set(32, 32, 28);
    m_frameRadius                 = 64;
    m_frameList.numActionFrames   = 0;
    m_frameList.numMovementFrames = 0;
    m_frameList.m_ef2Split        = 0;
    m_targetLookLeft              = 0;
    m_targetLookRight             = 0;
    m_targetLookUp                = 0;
    m_targetLookDown              = 0;
    m_targetBlink                 = -1;

    VectorClear(m_eyeTargetPos);

    m_timeNextBlink = Sys_Milliseconds();
    numBones        = m_Tiki->m_boneList.NumChannels();
    m_bone          = (skelBone_Base **)Skel_Alloc(numBones * sizeof(skelBone_Base *));
    memset(m_bone, 0, numBones * sizeof(skelBone_Base *));

    for (mesh = 0; mesh < m_Tiki->numMeshes; mesh++) {
        skelmodel = TIKI_GetSkel(m_Tiki->mesh[mesh]);
        SkeletorLoadBonesFromBuffer(&m_Tiki->m_boneList, skelmodel, m_bone);
        LoadMorphTargetNames(skelmodel);
    }

    m_morphTargetList.PackChannels();
    m_headBoneIndex = m_Tiki->GetBoneNumFromName("Bip01 Head");

    m_numExternalMorphs = 0;
}

void skeletor_c::SetExternalMorphControllers(const morph_controller_t *controllers, int count)
{
    int i;

    if (!controllers || count <= 0) {
        m_numExternalMorphs = 0;
        return;
    }

    if (count > NUM_MORPH_CONTROLLERS) {
        count = NUM_MORPH_CONTROLLERS;
    }

    m_numExternalMorphs = count;
    for (i = 0; i < count; i++) {
        m_externalMorphs[i] = controllers[i];
    }
}

skeletor_c::~skeletor_c()
{
    int i;

    for (i = 0; i < m_Tiki->m_boneList.m_numChannels; i++) {
        delete m_bone[i];
    }

    m_morphTargetList.CleanUpChannels();
    Skel_Free(m_bone);
    m_bone = NULL;
}

void skelAnimDataGameHeader_s::DeallocAnimData(skelAnimDataGameHeader_t *data)
{
    skanChannelHdr *pChannel;
    int             i;
    int             channelType;

    if (!data) {
        return;
    }

    for (i = 0; i < data->nTotalChannels; i++) {
        pChannel    = &data->ary_channels[i];
        channelType = GetBoneChannelType(data->channelList.ChannelName(skeletor_c::ChannelNames(), i));

        if (channelType != 2) {
            Skel_Free(pChannel->ary_frames);
        }
    }

    data->channelList.CleanUpChannels();

    TIKI_Skan_Release(data);

    if (data->tanFrames) {
        Skel_Free(data->tanFrames);
    }

    if (data->tanTags) {
        Skel_Free(data->tanTags);
    }

    if (data->m_frame) {
        Skel_Free(data->m_frame);
    }

    Skel_Free(data);
}

void ConvertToRotationName(const char *boneName, char *rotChannelName)
{
    strcpy(rotChannelName, boneName);
    strcat(rotChannelName, " rot");
}

void ConvertToPositionName(const char *boneName, char *posChannelName)
{
    strcpy(posChannelName, boneName);
    strcat(posChannelName, " pos");
}

void ConvertToFKRotationName(const char *boneName, char *rotChannelName)
{
    strcpy(rotChannelName, boneName);
    strcat(rotChannelName, " rot");
    strcat(rotChannelName, "FK");
}

void ConvertToFKPositionName(const char *boneName, char *rotChannelName)
{
    strcpy(rotChannelName, boneName);
    strcat(rotChannelName, " pos");
    strcat(rotChannelName, "FK");
}

int GetHighestFloat(float *selection)
{
    float currentHighest;
    int   currentHighestIndex;
    int   i;

    currentHighest      = selection[0];
    currentHighestIndex = 0;

    for (i = 0; i < 8; i++) {
        if (selection[i] > currentHighest) {
            currentHighest      = selection[i];
            currentHighestIndex = i;
        }
    }

    return currentHighestIndex;
}

void AddToBounds(SkelVec3 *bounds, SkelVec3 *newBounds)
{
    int i;

    for (i = 0; i < 3; i++) {
        if (bounds[0][i] > newBounds[0][i]) {
            bounds[0][i] = newBounds[0][i];
        }

        if (bounds[1][i] < newBounds[1][i]) {
            bounds[1][i] = newBounds[1][i];
        }
    }
}

static qboolean EF2AnimSplitActive(void)
{
    return qtrue;
}

void skeletor_c::SetPose(
    const frameInfo_t *frameInfo,
    const int         *contIndices,
    const vec4_t      *contValues,
    float              actionWeight,
    const short int   *ef2BodyParts
)
{
    skelAnimDataGameHeader_t *animData;
    short int                 ef2BodyPart;
    int                       nEF2LegsSlots;
    int                       nEF2TorsoSlots;
    int                       nEF2ForeignSlots;
    short int                *aliases;
    int                       boneNum;
    int                       blendNum;
    int                       i;
    int                       blendFrame;
    int                       movementBlendFrame;
    int                       actionBlendFrame;
    int                       realAnimIndex;
    int                       beforeFrame;
    int                       afterFrame;
    float                     beforeWeight;
    float                     afterWeight;
    int                       numFramesAdded;
    float                     cutoff_weight;
    int                       contNum;
    float                     animWeight;
    skanBlendInfo            *frame1, *frame2;

    for (i = 0; i < m_Tiki->m_boneList.NumChannels(); i++) {
        m_bone[i]->m_controller = NULL;
        m_bone[i]->m_isDirty    = true;
    }

    if (contIndices && contValues) {
        for (contNum = 0; contNum < 5; contNum++) {
            boneNum = contIndices[contNum];
            if (boneNum < 0 || boneNum >= m_Tiki->m_boneList.NumChannels()) {
                continue;
            }

            cutoff_weight = (contValues[contNum][3] - 1.0) * (contValues[contNum][3] - 1.0);
            if (cutoff_weight >= EPSILON) {
                m_bone[boneNum]->m_controller = (float *)contValues[contNum];
            }
        }
    }

    for (i = 0; i < 3; i++) {
        m_frameBounds[0][i] = -2.0f;
        m_frameBounds[1][i] = 2.0f;
    }

    m_frameRadius      = 2.0f;
    animWeight         = 0.0f;
    movementBlendFrame = 0;
    actionBlendFrame   = MAX_SKEL_BLEND_MOVEMENT_FRAMES;
    aliases            = m_Tiki->a->m_aliases;
    nEF2LegsSlots      = 0;
    nEF2TorsoSlots     = 0;
    nEF2ForeignSlots   = 0;

    m_frameList.m_ef2Split = 0;

    for (blendNum = 0; blendNum < MAX_FRAMEINFOS; blendNum++) {
        const frameInfo_t& currentFrame = frameInfo[blendNum];

        if (animWeight < currentFrame.weight) {
            animWeight = currentFrame.weight;
        }
    }

    cutoff_weight = animWeight * 0.01f;

    for (blendNum = 0; blendNum < MAX_FRAMEINFOS; blendNum++) {
        const frameInfo_t& currentFrame = frameInfo[blendNum];

        if (currentFrame.weight <= cutoff_weight) {
            continue;
        }

        if (currentFrame.index < 0 || currentFrame.index > m_Tiki->a->num_anims) {
            continue;
        }

        realAnimIndex = aliases[currentFrame.index];
        if (realAnimIndex < 0 || realAnimIndex >= MAX_TIKI_ALIASES) {
            continue;
        }

        animData = SkeletorCacheGetData(realAnimIndex);
        if (!animData) {
            continue;
        }

        if (animData->bHasDelta) {
            blendFrame = movementBlendFrame;
        } else {
            blendFrame = actionBlendFrame;
        }

        beforeWeight = 0.0;
        afterWeight  = 0.0;
        beforeFrame  = 0.0;
        afterFrame   = 0.0;

        numFramesAdded =
            animData->GetFrameNums(currentFrame.time, 0.001f, &beforeFrame, &afterFrame, &beforeWeight, &afterWeight);

        ef2BodyPart = ef2BodyParts ? ef2BodyParts[blendNum] : (short int)SKEL_BODYPART_UNKNOWN;
        m_frameList.m_ef2BodyPart[blendFrame] = ef2BodyPart;
        if (numFramesAdded == 2) {
            m_frameList.m_ef2BodyPart[blendFrame + 1] = ef2BodyPart;
        }
        if (ef2BodyPart == SKEL_BODYPART_LEGS) {
            nEF2LegsSlots++;
        } else if (ef2BodyPart == SKEL_BODYPART_TORSO) {
            nEF2TorsoSlots++;
        } else {
            nEF2ForeignSlots++;
        }

        frame1                 = &m_frameList.m_blendInfo[blendFrame];
        frame1->frame          = beforeFrame;
        frame1->pAnimationData = animData;
        frame1->weight         = beforeWeight * currentFrame.weight;

        AddToBounds(m_frameBounds, animData->bounds);

        if (frame1->pAnimationData->m_frame[frame1->frame].radius > m_frameRadius) {
            m_frameRadius = frame1->pAnimationData->m_frame[frame1->frame].radius;
        }

        if (numFramesAdded == 2) {
            frame2                 = &m_frameList.m_blendInfo[blendFrame + 1];
            frame2->frame          = afterFrame;
            frame2->pAnimationData = animData;
            frame2->weight         = afterWeight * currentFrame.weight;
            AddToBounds(m_frameBounds, animData->bounds);

            if (frame2->pAnimationData->m_frame[frame2->frame].radius > m_frameRadius) {
                m_frameRadius = frame2->pAnimationData->m_frame[frame2->frame].radius;
            }
        }

        if (animData->bHasDelta) {
            movementBlendFrame = blendFrame + numFramesAdded;
        } else {
            actionBlendFrame = blendFrame + numFramesAdded;
        }
    }

    for (i = 0; i < 3; i++) {
        m_frameBounds[0][i] += -7.0f;
        m_frameBounds[1][i] += 7.0f;
    }

    m_frameList.numMovementFrames = movementBlendFrame;
    m_frameList.numActionFrames   = actionBlendFrame - MAX_SKEL_BLEND_MOVEMENT_FRAMES;
    m_frameList.actionWeight      = actionWeight;

    if (nEF2LegsSlots && nEF2TorsoSlots && !nEF2ForeignSlots && EF2AnimSplitActive()) {
        m_frameList.m_ef2Split = 1;
    }

    assert(m_frameList.numMovementFrames < MAX_SKEL_BLEND_MOVEMENT_FRAMES);
    assert(m_frameList.numActionFrames < MAX_SKEL_BLEND_ACTION_FRAMES);
}

static SkelMat4 GetGlobalDefaultPosition(skelBone_Base *bone)
{
    SkelMat4 lLocalPosition;
    SkelMat4 lGlobalPosition;
    SkelMat4 lParentGlobalPosition;

    lLocalPosition = bone->GetTransform(NULL);

    if (bone->Parent()) {
        lGlobalPosition = lLocalPosition;
    } else {
        lGlobalPosition = lLocalPosition;
    }

    return lGlobalPosition;
}

SkelMat4 GlobalToLocal(skelBone_Base *bone, SkelMat4 pGlobalPosition)
{
    SkelMat4 lLocalPosition;
    SkelMat4 lParentGlobalPosition;

    if (bone->Parent()) {
        lParentGlobalPosition = GetGlobalDefaultPosition(bone->Parent());
        lParentGlobalPosition.Inverse();
        lLocalPosition.Multiply(lParentGlobalPosition, pGlobalPosition);
    } else {
        lLocalPosition = pGlobalPosition;
    }

    return lLocalPosition;
}

void SkeletorGetAnimFrame2(
    skelHeaderGame_t         *skelmodel,
    skelChannelList_c        *boneList,
    skelBoneCache_t          *bones,
    skelAnimStoreFrameList_c *frameList,
    float                    *radius,
    vec3_t                   *mins,
    vec3_t                   *maxes
)
{
    int              numBones;
    skelBone_Base  **bone;
    int              i;
    skelAnimFrame_t *newFrame;

    numBones = skelmodel->numBones;

    bone = (skelBone_Base **)Skel_Alloc(sizeof(skelBone_Base *) * numBones);
    memset(bone, 0, sizeof(skelBone_Base *) * numBones);

    SkeletorLoadBonesFromBuffer(boneList, skelmodel, bone);

    for (i = 0; i < numBones; i++) {
        bone[i]->m_controller = NULL;
        bone[i]->m_isDirty    = true;
    }

    newFrame            = (skelAnimFrame_t *)Skel_Alloc(SKELANIMFRAME_SIZE(numBones));
    newFrame->radius    = 0;
    newFrame->bounds[0] = SkelVec3();
    newFrame->bounds[1] = SkelVec3();

    for (i = 0; i < numBones; i++) {
        newFrame->bones[i] = bone[i]->GetTransform(frameList);
    }

    for (i = 0; i < numBones; i++) {
        VectorCopy(newFrame->bones[i][3], bones[i].offset);
        bones[i].matrix[0][0] = newFrame->bones[i][0][0];
        bones[i].matrix[0][1] = newFrame->bones[i][0][1];
        bones[i].matrix[0][2] = newFrame->bones[i][0][2];
        bones[i].matrix[0][3] = 0;
        bones[i].matrix[1][0] = newFrame->bones[i][1][0];
        bones[i].matrix[1][1] = newFrame->bones[i][1][1];
        bones[i].matrix[1][2] = newFrame->bones[i][1][2];
        bones[i].matrix[1][3] = 0;
        bones[i].matrix[2][0] = newFrame->bones[i][2][0];
        bones[i].matrix[2][1] = newFrame->bones[i][2][1];
        bones[i].matrix[2][2] = newFrame->bones[i][2][2];
        bones[i].matrix[2][3] = 0;
    }

    for (i = 0; i < numBones; i++) {
        delete bone[i];
    }

    Skel_Free(bone);

    if (radius) {
        *radius = newFrame->radius;
    }

    if (mins || maxes) {
        for (i = 0; i < 3; i++) {
            if (mins) {
                (*mins)[i] = newFrame->bounds[0][i];
            }
            if (maxes) {
                (*maxes)[i] = newFrame->bounds[1][i];
            }
        }
    }

    Skel_Free(newFrame);
}

void SkeletorGetAnimFrame(
    skelHeaderGame_t         *skelmodel,
    skelAnimDataGameHeader_t *animData,
    skelChannelList_c        *boneList,
    skelBoneCache_t          *bones,
    int                       frame,
    float                    *radius,
    vec3_t                   *mins,
    vec3_t                   *maxes
)
{
    int                      numBones;
    skelBone_Base          **bone;
    int                      i;
    skelAnimStoreFrameList_c frameList;
    skelAnimFrame_t         *newFrame;

    frameList.actionWeight = animData ? 1.0 : 0;

    if (animData) {
        if (!animData->bHasDelta) {
            frameList.numMovementFrames = 0;
            frameList.numActionFrames   = 1;

            frameList.m_blendInfo[MAX_SKEL_BLEND_MOVEMENT_FRAMES].weight         = 1.0;
            frameList.m_blendInfo[MAX_SKEL_BLEND_MOVEMENT_FRAMES].pAnimationData = animData;
            frameList.m_blendInfo[MAX_SKEL_BLEND_MOVEMENT_FRAMES].frame          = frame;
        } else {
            frameList.numMovementFrames             = 1;
            frameList.numActionFrames               = 0;
            frameList.m_blendInfo[0].weight         = 1.0;
            frameList.m_blendInfo[0].pAnimationData = animData;
            frameList.m_blendInfo[0].frame          = frame;
        }
    }
    numBones = skelmodel->numBones;

    bone = (skelBone_Base **)Skel_Alloc(sizeof(skelBone_Base *) * numBones);
    memset(bone, 0, sizeof(skelBone_Base *) * numBones);

    SkeletorLoadBonesFromBuffer(boneList, skelmodel, bone);

    for (i = 0; i < numBones; i++) {
        bone[i]->m_controller = NULL;
        bone[i]->m_isDirty    = true;
    }

    newFrame = (skelAnimFrame_t *)Skel_Alloc(SKELANIMFRAME_SIZE(numBones));

    if (animData) {
        if (animData->m_frame) {
            newFrame->radius = animData->m_frame->radius;
        } else {
            newFrame->radius = 0;
        }

        newFrame->bounds[0] = animData->bounds[0];
        newFrame->bounds[1] = animData->bounds[1];
    } else {
        newFrame->radius    = 0;
        newFrame->bounds[0] = SkelVec3();
        newFrame->bounds[1] = SkelVec3();
    }

    if (animData) {
        for (i = 0; i < numBones; i++) {
            newFrame->bones[i] = bone[i]->GetTransform(&frameList);
        }
    } else {
        for (i = 0; i < numBones; i++) {
            newFrame->bones[i] = SkelMat4();
        }
    }

    for (i = 0; i < numBones; i++) {
        VectorCopy(newFrame->bones[i][3], bones[i].offset);
        bones[i].matrix[0][0] = newFrame->bones[i][0][0];
        bones[i].matrix[0][1] = newFrame->bones[i][0][1];
        bones[i].matrix[0][2] = newFrame->bones[i][0][2];
        bones[i].matrix[0][3] = 0;
        bones[i].matrix[1][0] = newFrame->bones[i][1][0];
        bones[i].matrix[1][1] = newFrame->bones[i][1][1];
        bones[i].matrix[1][2] = newFrame->bones[i][1][2];
        bones[i].matrix[1][3] = 0;
        bones[i].matrix[2][0] = newFrame->bones[i][2][0];
        bones[i].matrix[2][1] = newFrame->bones[i][2][1];
        bones[i].matrix[2][2] = newFrame->bones[i][2][2];
        bones[i].matrix[2][3] = 0;
    }

    for (i = 0; i < numBones; i++) {
        delete bone[i];
    }

    Skel_Free(bone);

    if (radius) {
        *radius = newFrame->radius;
    }

    if (mins || maxes) {
        for (i = 0; i < 3; i++) {
            if (mins) {
                (*mins)[i] = newFrame->bounds[0][i];
            }
            if (maxes) {
                (*maxes)[i] = newFrame->bounds[1][i];
            }
        }
    }

    Skel_Free(newFrame);
}

void TIKI_GetSkelAnimFrameInternal2(
    dtiki_t                  *tiki,
    skelBoneCache_t          *bones,
    skelAnimStoreFrameList_c *frameList,
    float                    *radius,
    vec3_t                   *mins,
    vec3_t                   *maxes
)
{
    int              numBones;
    skelBone_Base  **bone;
    int              i;
    skelAnimFrame_t *newFrame;
    skelHeaderGame_t *skelmodel;

    numBones = tiki->m_boneList.NumChannels();

    bone = (skelBone_Base **)Skel_Alloc(sizeof(skelBone_Base *) * numBones);
    memset(bone, 0, sizeof(skelBone_Base *) * numBones);

    for (i = 0; i < tiki->numMeshes; i++) {
        skelmodel = TIKI_GetSkel(tiki->mesh[i]);
        SkeletorLoadBonesFromBuffer(&tiki->m_boneList, skelmodel, bone);
    }

    for (i = 0; i < numBones; i++) {
        bone[i]->m_controller = NULL;
        bone[i]->m_isDirty    = true;
    }

    newFrame = (skelAnimFrame_t *)Skel_Alloc(SKELANIMFRAME_SIZE(numBones));

    {
        newFrame->radius    = 0;
        newFrame->bounds[0] = SkelVec3();
        newFrame->bounds[1] = SkelVec3();
    }

    for (i = 0; i < numBones; i++) {
        newFrame->bones[i] = bone[i]->GetTransform(frameList);
    }

    for (i = 0; i < numBones; i++) {
        delete bone[i];
    }

    Skel_Free(bone);

    for (i = 0; i < numBones; i++) {
        VectorCopy(newFrame->bones[i][3], bones[i].offset);
        bones[i].matrix[0][0] = newFrame->bones[i][0][0];
        bones[i].matrix[0][1] = newFrame->bones[i][0][1];
        bones[i].matrix[0][2] = newFrame->bones[i][0][2];
        bones[i].matrix[0][3] = 0;
        bones[i].matrix[1][0] = newFrame->bones[i][1][0];
        bones[i].matrix[1][1] = newFrame->bones[i][1][1];
        bones[i].matrix[1][2] = newFrame->bones[i][1][2];
        bones[i].matrix[1][3] = 0;
        bones[i].matrix[2][0] = newFrame->bones[i][2][0];
        bones[i].matrix[2][1] = newFrame->bones[i][2][1];
        bones[i].matrix[2][2] = newFrame->bones[i][2][2];
        bones[i].matrix[2][3] = 0;
    }

    if (radius) {
        *radius = newFrame->radius;
    }

    if (mins && maxes) {
        for (i = 0; i < 3; i++) {
            (*mins)[i]  = newFrame->bounds[0][i];
            (*maxes)[i] = newFrame->bounds[1][i];
        }
    }

    Skel_Free(newFrame);
}

void TIKI_GetSkelAnimFrameInternal(
    dtiki_t                  *tiki,
    skelBoneCache_t          *bones,
    skelAnimDataGameHeader_t *animData,
    int                       frame,
    float                    *radius,
    vec3_t                   *mins,
    vec3_t                   *maxes
)
{
    int                      numBones;
    skelBone_Base          **bone;
    int                      i;
    skelAnimStoreFrameList_c frameList;
    skelAnimFrame_t         *newFrame;
    skelHeaderGame_t *skelmodel;

    frameList.actionWeight = animData ? 1.0 : 0;
    if (!animData || !animData->bHasDelta) {
        frameList.numMovementFrames = 0;
        frameList.numActionFrames   = 1;

        frameList.m_blendInfo[MAX_SKEL_BLEND_MOVEMENT_FRAMES].weight         = 1.0;
        frameList.m_blendInfo[MAX_SKEL_BLEND_MOVEMENT_FRAMES].pAnimationData = animData;
        frameList.m_blendInfo[MAX_SKEL_BLEND_MOVEMENT_FRAMES].frame          = frame;
    } else {
        frameList.numMovementFrames             = 1;
        frameList.numActionFrames               = 0;
        frameList.m_blendInfo[0].weight         = 1.0;
        frameList.m_blendInfo[0].pAnimationData = animData;
        frameList.m_blendInfo[0].frame          = frame;
    }
    numBones = tiki->m_boneList.NumChannels();

    bone = (skelBone_Base **)Skel_Alloc(sizeof(skelBone_Base *) * numBones);
    memset(bone, 0, sizeof(skelBone_Base *) * numBones);

    for (i = 0; i < tiki->numMeshes; i++) {
        skelmodel = TIKI_GetSkel(tiki->mesh[i]);
        SkeletorLoadBonesFromBuffer(&tiki->m_boneList, skelmodel, bone);
    }

    for (i = 0; i < numBones; i++) {
        bone[i]->m_controller = NULL;
        bone[i]->m_isDirty    = true;
    }

    newFrame = (skelAnimFrame_t *)Skel_Alloc(SKELANIMFRAME_SIZE(numBones));

    if (animData) {
        if (animData->m_frame) {
            newFrame->radius = animData->m_frame->radius;
        } else {
            newFrame->radius = 0;
        }

        newFrame->bounds[0] = animData->bounds[0];
        newFrame->bounds[1] = animData->bounds[1];
    } else {
        newFrame->radius    = 0;
        newFrame->bounds[0] = SkelVec3();
        newFrame->bounds[1] = SkelVec3();
    }

    for (i = 0; i < numBones; i++) {
        newFrame->bones[i] = bone[i]->GetTransform(&frameList);
    }

    for (i = 0; i < numBones; i++) {
        delete bone[i];
    }

    Skel_Free(bone);

    for (i = 0; i < numBones; i++) {
        VectorCopy(newFrame->bones[i][3], bones[i].offset);
        bones[i].matrix[0][0] = newFrame->bones[i][0][0];
        bones[i].matrix[0][1] = newFrame->bones[i][0][1];
        bones[i].matrix[0][2] = newFrame->bones[i][0][2];
        bones[i].matrix[0][3] = 0;
        bones[i].matrix[1][0] = newFrame->bones[i][1][0];
        bones[i].matrix[1][1] = newFrame->bones[i][1][1];
        bones[i].matrix[1][2] = newFrame->bones[i][1][2];
        bones[i].matrix[1][3] = 0;
        bones[i].matrix[2][0] = newFrame->bones[i][2][0];
        bones[i].matrix[2][1] = newFrame->bones[i][2][1];
        bones[i].matrix[2][2] = newFrame->bones[i][2][2];
        bones[i].matrix[2][3] = 0;
    }

    if (radius) {
        *radius = newFrame->radius;
    }

    if (mins && maxes) {
        for (i = 0; i < 3; i++) {
            (*mins)[i]  = newFrame->bounds[0][i];
            (*maxes)[i] = newFrame->bounds[1][i];
        }
    }

    Skel_Free(newFrame);
}

void TIKI_GetSkelAnimFrame2(
    dtiki_t *tiki, skelBoneCache_t *bones, int anim, int frame, float *radius, vec3_t *mins, vec3_t *maxes
)
{
    short                    *aliases;
    skelAnimDataGameHeader_t *animData;

    aliases = tiki->a->m_aliases;
    if (*aliases == -1) {
        SKEL_Warning(
            "TIKI_GetSkelAnimFrame: Bad anim in static model %s, couldn't generate pose properly.\n", tiki->name
        );
        return;
    }

    animData = SkeletorCacheGetData(aliases[anim]);

    TIKI_GetSkelAnimFrameInternal(tiki, bones, animData, frame, radius, mins, maxes);
}

void TIKI_GetSkelAnimFrame(dtiki_t *tiki, skelBoneCache_t *bones, float *radius, vec3_t *mins, vec3_t *maxes)
{
    TIKI_GetSkelAnimFrame2(tiki, bones, 0, 0, radius, mins, maxes);
}

void skeletor_c::GetFrame(skelAnimFrame_t *newFrame)
{
    int boneNum;
    int numBones;

    numBones = m_Tiki->m_boneList.NumChannels();

    for (boneNum = 0; boneNum < numBones; boneNum++) {
        newFrame->bones[boneNum] = GetBoneFrame(boneNum);
    }

    newFrame->bounds[0] = m_frameBounds[0];
    newFrame->bounds[1] = m_frameBounds[1];
    newFrame->radius    = m_frameRadius;
}

SkelMat4& skeletor_c::GetBoneFrame(int boneIndex) const
{
    return m_bone[boneIndex]->GetTransform(&m_frameList);
}

bool skeletor_c::IsBoneOnGround(int boneIndex, float threshold)
{
    return GetBoneFrame(boneIndex).val[3][2] < threshold;
}

float skeletor_c::GetRadius()
{
    return m_frameRadius;
}

float skeletor_c::GetCentroidRadius(float *centroid)
{
    centroid[0] = (m_frameBounds[0][0] + m_frameBounds[1][0]) * 0.5f;
    centroid[1] = (m_frameBounds[0][1] + m_frameBounds[1][1]) * 0.5f;
    centroid[2] = (m_frameBounds[0][2] + m_frameBounds[1][2]) * 0.5f;
    return m_frameRadius;
}

int skeletor_c::GetMorphWeightFrame(int index, float time, int *data)
{
    return GetMorphWeightFrame(data);
}

float DecodeFrameValue(skanChannelHdr *channelFrames, int desiredFrameNum)
{
    skanGameFrame *foundFrame;
    size_t         frameSize;
    int            i;

    frameSize = (sizeof(skanGameFrame) - sizeof(skanGameFrame::pChannelData) + sizeof(float));

    foundFrame = channelFrames->ary_frames;

    for (i = 0; i < channelFrames->nFramesInChannel; i++) {
        if (foundFrame->nFrameNum >= desiredFrameNum) {
            break;
        }

        foundFrame = (skanGameFrame *)((byte *)foundFrame + frameSize);
    }

    if (foundFrame->nFrameNum > desiredFrameNum) {
        foundFrame = (skanGameFrame *)((byte *)channelFrames->ary_frames + foundFrame->nPrevFrameIndex * frameSize);
    }

    return foundFrame->pChannelData[0];
}

int skeletor_c::GetMorphWeightFrame(int *data)
{
    int   numTargets;
    int   animChannelNum;
    int   blendNum;
    float weight;
    int   modelChannelNum;
    float channelData;

    numTargets = m_morphTargetList.NumChannels();

    if (!numTargets) {
        return 0;
    }

    memset(data, 0, sizeof(*data) * numTargets);

    for (blendNum = 0; blendNum < m_frameList.numMovementFrames; blendNum++) {
        const skanBlendInfo& blendInfo = m_frameList.m_blendInfo[blendNum];
        weight                         = blendInfo.weight;

        if (weight > 0.001) {
            for (modelChannelNum = 0; modelChannelNum < numTargets; modelChannelNum++) {
                animChannelNum = m_morphTargetList.GlobalChannel(modelChannelNum);
                animChannelNum = blendInfo.pAnimationData->channelList.LocalChannel(animChannelNum);

                if (animChannelNum >= 0) {
                    channelData =
                        DecodeFrameValue(&blendInfo.pAnimationData->ary_channels[animChannelNum], blendInfo.frame);
                    data[modelChannelNum] += (int)(channelData * weight);
                }
            }
        }
    }

    for (blendNum = 0; blendNum < m_frameList.numActionFrames; blendNum++) {
        const skanBlendInfo& blendInfo = m_frameList.m_blendInfo[blendNum + MAX_SKEL_BLEND_MOVEMENT_FRAMES];
        weight                         = blendInfo.weight;

        if (weight > 0.001) {
            for (modelChannelNum = 0; modelChannelNum < numTargets; modelChannelNum++) {
                animChannelNum = m_morphTargetList.GlobalChannel(modelChannelNum);
                animChannelNum = blendInfo.pAnimationData->channelList.LocalChannel(animChannelNum);

                if (animChannelNum >= 0) {
                    channelData =
                        DecodeFrameValue(&blendInfo.pAnimationData->ary_channels[animChannelNum], blendInfo.frame);
                    data[modelChannelNum] += (int)(channelData * weight);
                }
            }
        }
    }

    if (m_numExternalMorphs > 0) {
        int ci;

        for (ci = 0; ci < m_numExternalMorphs; ci++) {
            int   morphIndex = m_externalMorphs[ci].index;
            float percent    = m_externalMorphs[ci].percent;

            if (morphIndex >= 0 && morphIndex < numTargets) {
                data[morphIndex] += (int)(percent * 100.0f);
            }
        }
    }

    if (m_headBoneIndex >= 0 && !VectorCompareEpsilon(m_eyeTargetPos, vec3_origin, EPSILON)) {
        SkelVec3        lookPos    = m_eyeTargetPos;
        const SkelMat4& headOrient = GetBoneFrame(m_headBoneIndex);
        SkelMat4        invHeadOrient;
        invHeadOrient.TransposeRotOf(headOrient);

        SkelVec3 temp = lookPos;
        lookPos[0]    = temp[0] * invHeadOrient[0][0] * temp[1] * invHeadOrient[1][0] + temp[2] * invHeadOrient[2][0];
        lookPos[1]    = temp[0] * invHeadOrient[0][1] * temp[1] * invHeadOrient[1][1] + temp[2] * invHeadOrient[2][1];
        lookPos[2]    = temp[0] * invHeadOrient[0][2] * temp[1] * invHeadOrient[1][2] + temp[2] * invHeadOrient[2][2];

        float lookLeftAmount = lookPos[2] * 100 + data[m_targetLookLeft] - data[m_targetLookRight];
        float lookUpAmount   = lookPos[0] * 100 + data[m_targetLookUp] - data[m_targetLookDown];

        const float s = VectorLengthSquared(lookPos);

        if (s == 0.0) {
            lookPos[0] = 1.0;
        } else if (s != 1.0) {
            float l = 1.0 / sqrt(s);
            VectorScale(lookPos, l, lookPos);
        }

        if (m_targetLookLeft >= 0 && m_targetLookRight >= 0 && m_targetLookUp >= 0 && m_targetLookDown >= 0) {
            if (lookLeftAmount > 0.0) {
                if (lookLeftAmount > 100.0) {
                    lookLeftAmount = 100.0;
                }

                data[m_targetLookLeft]  = lookLeftAmount;
                data[m_targetLookRight] = 0;
            } else {
                if (lookLeftAmount < -100.0) {
                    lookLeftAmount = -100.0;
                }

                data[m_targetLookLeft]  = 0;
                data[m_targetLookRight] = -lookLeftAmount;
            }

            if (m_targetLookUp > 0.0) {
                if (lookUpAmount > 100.0) {
                    lookUpAmount = 100.0;
                }

                data[m_targetLookUp]   = lookUpAmount;
                data[m_targetLookDown] = 0;
            } else {
                if (lookUpAmount < -133.0) {
                    lookUpAmount = -133.0;
                }

                data[m_targetLookUp]   = 0.0;
                data[m_targetLookDown] = (-lookUpAmount * 0.75);
            }
        }
    }

    if (m_targetBlink >= 0) {
        int sysMilliseconds;
        int blinkAmount;

        sysMilliseconds = Sys_Milliseconds();

        if (sysMilliseconds > m_timeNextBlink) {
            if (sysMilliseconds <= m_timeNextBlink + 250) {
                blinkAmount = sysMilliseconds - m_timeNextBlink;

                if (blinkAmount > 100) {
                    blinkAmount = 250 - blinkAmount;

                    if (blinkAmount > 100) {
                        blinkAmount = 100;
                    }
                }

                if (data[m_targetBlink] < blinkAmount) {
                    data[m_targetBlink] = blinkAmount;
                }
            } else {
                m_timeNextBlink = rand() / 5 + sysMilliseconds - 1000;
            }
        }
    }

    return numTargets;
}

void skeletor_c::SetEyeTargetPos(const float *pEyeTargetPos)
{
    VectorCopy(pEyeTargetPos, m_eyeTargetPos);
}

int skeletor_c::GetBoneParent(int boneIndex)
{
    int            iBoneNum;
    skelBone_Base *pBoneParent = m_bone[boneIndex]->Parent();

    for (iBoneNum = 0; iBoneNum < m_Tiki->m_boneList.NumChannels(); iBoneNum++) {
        if (m_bone[iBoneNum] == pBoneParent) {
            return iBoneNum;
        }
    }

    return -1;
}

const char *dtiki_s::GetBoneNameFromNum(int num) const
{
    return m_boneList.ChannelName(&skel_BoneNames, num);
}

int dtiki_s::GetBoneNumFromName(const char *boneName)
{
    int iGlobalChannel;

    iGlobalChannel = skel_BoneNames.FindNameLookup(boneName);

    if (iGlobalChannel < 0) {
        return -1;
    }

    return m_boneList.LocalChannel(iGlobalChannel);
}
