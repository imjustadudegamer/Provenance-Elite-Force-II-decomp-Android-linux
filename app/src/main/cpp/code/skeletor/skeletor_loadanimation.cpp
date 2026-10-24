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


#include "q_shared.h"
#include "qcommon.h"
#include "skeletor.h"
#include "../tiki/tiki_skan.h"

extern "C" void TIKI_Error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

qboolean Compress(skelAnimGameFrame_t *current, skelAnimGameFrame_t *last, int channelIndex, int numChannels)
{
    float tolerance;
    float difference;
    int   i;

    return false;

    for (i = 0; i < numChannels; i++) {
        tolerance = current->pChannels[channelIndex][i];
        if (tolerance > -0.000001 && tolerance < 0.000001) {
            current->pChannels[channelIndex][i] = 0.0f;
        }
    }

    if (last) {
        for (i = 0; i < numChannels; i++) {
            difference = last->pChannels[channelIndex][i] - current->pChannels[channelIndex][i];
            if (difference < -0.001f || difference >= 0.001f) {
                return false;
            }
        }
    }

    return true;
}

skelAnimDataGameHeader_t *EncodeFrames(
    skelAnimDataGameHeader_t *enAnim,
    skelAnimGameFrame_t      *m_frame,
    qboolean                  bLog,
    skelChannelList_c        *channelList,
    ChannelNameTable         *channelNames
)
{
    int                  endFrameCap;
    skanChannelHdr      *pChannel;
    int                  i, j;
    int                  frameCnt;
    skelAnimGameFrame_t *pCurrFrame;
    skelAnimGameFrame_t *pLastFrame;
    skanGameFrame       *pFrame;
    size_t               frameSize;
    int                  indexLastFrameAdded;
    channelType_t        channelType;

    pChannel    = enAnim->ary_channels;
    endFrameCap = enAnim->numFrames - 2;

    for (i = 0; i < enAnim->nTotalChannels; i++, pChannel++) {
        pLastFrame = NULL;
        pCurrFrame = m_frame;

        channelType = GetBoneChannelType(enAnim->channelList.ChannelName(channelNames, i));
        switch (channelType) {
        case CHANNEL_ROTATION:
            frameSize = (sizeof(skanGameFrame) - sizeof(skanGameFrame::pChannelData) + sizeof(vec4_t));
            frameCnt  = 0;
            for (j = 0; j < enAnim->numFrames; j++) {
                if (!Compress(pCurrFrame, pLastFrame, i, 4) || j >= endFrameCap) {
                    frameCnt++;
                    pLastFrame = pCurrFrame;
                }

                pCurrFrame++;
            }

            pChannel->ary_frames       = (skanGameFrame *)Skel_Alloc(frameSize * frameCnt);
            pChannel->nFramesInChannel = frameCnt;
            enAnim->nBytesUsed += frameSize * frameCnt;

            pLastFrame          = NULL;
            indexLastFrameAdded = 0;

            pCurrFrame = m_frame;
            pFrame     = pChannel->ary_frames;

            for (j = 0; j < enAnim->numFrames; j++) {
                if (!Compress(pCurrFrame, pLastFrame, i, 4) || j >= endFrameCap) {
                    pFrame                  = (skanGameFrame *)((byte *)pChannel->ary_frames + j * frameSize);
                    pFrame->nFrameNum       = j;
                    pFrame->nPrevFrameIndex = indexLastFrameAdded;

                    if (j > 0) {
                        indexLastFrameAdded++;
                    }

                    pFrame->pChannelData[0] = pCurrFrame->pChannels[i][0];
                    pFrame->pChannelData[1] = pCurrFrame->pChannels[i][1];
                    pFrame->pChannelData[2] = pCurrFrame->pChannels[i][2];
                    pFrame->pChannelData[3] = pCurrFrame->pChannels[i][3];

                    pLastFrame = pCurrFrame;
                }

                pCurrFrame++;
            }
            break;
        case CHANNEL_POSITION:
            frameSize = (sizeof(skanGameFrame) - sizeof(skanGameFrame::pChannelData) + sizeof(vec3_t));
            frameCnt  = 0;
            for (j = 0; j < enAnim->numFrames; j++) {
                if (!Compress(pCurrFrame, pLastFrame, i, 3) || j >= endFrameCap) {
                    frameCnt++;
                    pLastFrame = pCurrFrame;
                }

                pCurrFrame++;
            }

            pChannel->ary_frames       = (skanGameFrame *)Skel_Alloc(frameSize * frameCnt);
            pChannel->nFramesInChannel = frameCnt;
            enAnim->nBytesUsed += frameSize * frameCnt;

            pLastFrame          = NULL;
            indexLastFrameAdded = 0;

            pCurrFrame = m_frame;
            pFrame     = pChannel->ary_frames;

            for (j = 0; j < enAnim->numFrames; j++) {
                if (!Compress(pCurrFrame, pLastFrame, i, 1) || j >= endFrameCap) {
                    pFrame                  = (skanGameFrame *)((byte *)pChannel->ary_frames + j * frameSize);
                    pFrame->nFrameNum       = j;
                    pFrame->nPrevFrameIndex = indexLastFrameAdded;

                    if (j > 0) {
                        indexLastFrameAdded++;
                    }

                    pFrame->pChannelData[0] = pCurrFrame->pChannels[i][0];
                    pFrame->pChannelData[1] = pCurrFrame->pChannels[i][1];
                    pFrame->pChannelData[2] = pCurrFrame->pChannels[i][2];

                    pLastFrame = pCurrFrame;
                }

                pCurrFrame++;
            }
            break;
        case CHANNEL_NONE:
            pChannel->ary_frames       = NULL;
            pChannel->nFramesInChannel = 0;
            break;
        case CHANNEL_VALUE:
        default:
            frameSize = (sizeof(skanGameFrame) - sizeof(skanGameFrame::pChannelData) + sizeof(float));
            frameCnt  = 0;
            for (j = 0; j < enAnim->numFrames; j++) {
                if (!Compress(pCurrFrame, pLastFrame, i, 1) || j >= endFrameCap) {
                    frameCnt++;
                    pLastFrame = pCurrFrame;
                }

                pCurrFrame++;
            }

            pChannel->ary_frames       = (skanGameFrame *)Skel_Alloc(frameSize * frameCnt);
            pChannel->nFramesInChannel = frameCnt;
            enAnim->nBytesUsed += frameSize * frameCnt;

            pLastFrame          = NULL;
            indexLastFrameAdded = 0;

            pCurrFrame = m_frame;
            pFrame     = pChannel->ary_frames;

            for (j = 0; j < enAnim->numFrames; j++) {
                if (!Compress(pCurrFrame, pLastFrame, i, 3) || j >= endFrameCap) {
                    pFrame                  = (skanGameFrame *)((byte *)pChannel->ary_frames + j * frameSize);
                    pFrame->nFrameNum       = j;
                    pFrame->nPrevFrameIndex = indexLastFrameAdded;

                    if (j > 0) {
                        indexLastFrameAdded++;
                    }

                    pFrame->pChannelData[0] = pCurrFrame->pChannels[i][0];

                    pLastFrame = pCurrFrame;
                }

                pCurrFrame++;
            }
            break;
        }
    }

    return enAnim;
}

skelAnimDataGameHeader_t *
skeletor_c::ConvertSkelFileToGame(skelAnimDataFileHeader_t *pHeader, int iBuffLength, const char *path)
{
    int                       i, j;
    skelAnimFileFrame_t      *pFileFrame;
    skelAnimGameFrame_t      *pGameFrame;
    skelChannelName_t        *pChannelNames;
    skelAnimDataGameHeader_t *enAnim;
    skelAnimGameFrame_t      *oldFrame;
    skelAnimGameFrame_t      *newFrame;
    int                       channelIndex;

    static_assert(sizeof(skelAnimDataFileHeader_t) == 96, "Skeletor anim header size doesn't match!");
    static_assert(sizeof(skelAnimFileFrame_t) == 48, "Skeletor anim file frame size doesn't match!");

    if (pHeader->numFrames <= 0) {
        return NULL;
    }

    pGameFrame = new skelAnimGameFrame_t[pHeader->numFrames];
    pFileFrame = pHeader->frame;
    newFrame   = pGameFrame;

    for (i = 0; i < pHeader->numFrames; i++) {
        vec4_t *pChannels;

        newFrame->bounds[0][0] = LittleFloat(pFileFrame->bounds[0][0]);
        newFrame->bounds[0][1] = LittleFloat(pFileFrame->bounds[0][1]);
        newFrame->bounds[0][2] = LittleFloat(pFileFrame->bounds[0][2]);
        newFrame->bounds[1][0] = LittleFloat(pFileFrame->bounds[1][0]);
        newFrame->bounds[1][1] = LittleFloat(pFileFrame->bounds[1][1]);
        newFrame->bounds[1][2] = LittleFloat(pFileFrame->bounds[1][2]);
        newFrame->radius       = LittleFloat(pFileFrame->radius);
        newFrame->delta[0]     = LittleFloat(pFileFrame->delta[0]);
        newFrame->delta[1]     = LittleFloat(pFileFrame->delta[1]);
        newFrame->delta[2]     = LittleFloat(pFileFrame->delta[2]);
        newFrame->angleDelta   = LittleFloat(pFileFrame->angleDelta);
        newFrame->pChannels = new vec4_t[pHeader->numChannels];
        pChannels =
            (vec4_t *)((byte *)pHeader
                       + (sizeof(skelAnimDataFileHeader_t) + sizeof(skelAnimFileFrame_t) * (pHeader->numFrames - 1)
                          + sizeof(vec4_t) * pHeader->numChannels * i));
        for (j = 0; j < pHeader->numChannels; j++) {
            newFrame->pChannels[j][0] = LittleFloat(pChannels[j][0]);
            newFrame->pChannels[j][1] = LittleFloat(pChannels[j][1]);
            newFrame->pChannels[j][2] = LittleFloat(pChannels[j][2]);
            newFrame->pChannels[j][3] = LittleFloat(pChannels[j][3]);
        }

        AddToBounds(newFrame->bounds, pFileFrame->bounds);

        pFileFrame++;
        newFrame++;
    }

    enAnim                  = skelAnimDataGameHeader_t::AllocRLEChannelData(pHeader->numChannels);
    enAnim->flags           = pHeader->flags;
    enAnim->frameTime       = pHeader->frameTime;
    enAnim->totalDelta      = pHeader->totalDelta;
    enAnim->totalAngleDelta = pHeader->totalAngleDelta;
    enAnim->numFrames       = pHeader->numFrames;
    enAnim->nTotalChannels  = pHeader->numChannels;
    ClearBounds(enAnim->bounds[0], enAnim->bounds[1]);
    enAnim->m_frame = (skelAnimGameFrame_t *)Skel_Alloc(pHeader->numFrames * sizeof(skelAnimGameFrame_t));

    oldFrame = pGameFrame;
    newFrame = enAnim->m_frame;

    for (i = 0; i < pHeader->numFrames; i++) {
        newFrame->bounds[0]  = oldFrame->bounds[0];
        newFrame->bounds[1]  = oldFrame->bounds[1];
        newFrame->radius     = oldFrame->radius;
        newFrame->delta      = oldFrame->delta;
        newFrame->angleDelta = oldFrame->angleDelta;
        newFrame->pChannels  = NULL;
        AddToBounds(enAnim->bounds, oldFrame->bounds);

        oldFrame++;
        newFrame++;
    }

    enAnim->channelList.ZeroChannels();

    pChannelNames = (skelChannelName_t *)((char *)pHeader + pHeader->ofsChannelNames);

    for (i = 0; i < pHeader->numChannels; i++) {
        channelIndex = skel_ChannelNames.RegisterChannel(*pChannelNames);
        enAnim->channelList.AddChannel(channelIndex);
        pChannelNames++;
    }

    enAnim->channelList.PackChannels();
    EncodeFrames(enAnim, pGameFrame, qfalse, &enAnim->channelList, &skel_ChannelNames);

    if (enAnim->channelList.HasChannel(&skel_ChannelNames, "Bip01 pos")
        && enAnim->channelList.HasChannel(&skel_ChannelNames, "Bip01 R Foot pos")
        && enAnim->channelList.HasChannel(&skel_ChannelNames, "Bip01 L Foot pos")) {
        enAnim->bHasDelta = true;
    } else {
        enAnim->bHasDelta = false;
    }

    if (enAnim->channelList.HasChannel(&skel_ChannelNames, "Bip01 Spine rot")
        && enAnim->channelList.HasChannel(&skel_ChannelNames, "Bip01 Spine1 rot")) {
        enAnim->bHasUpper = true;
    } else {
        enAnim->bHasUpper = false;
    }

    if (enAnim->channelList.HasChannel(&skel_ChannelNames, "VISEME_Bump")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "VISME_Cage_")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "VISME_Earth")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "VISME_Fave")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "VISME_If")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "VISME_New")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "VISME_Ox")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "VISME_Roar")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "VISME_Size")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "VISME_Though")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "VISME_Told")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "VISME_Wet")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "BROW_frown")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "BROW_R_lift")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "BROW_lift")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "BROW_worry")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "EYE_blink")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "EYES_Excited__")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "EYES_L_squint")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "EYES_narrow__")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "EYES_down")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "EYES_left")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "EYES_right")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "EYES_smile")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "EYES_up__")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "JAW_open-closed")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "JAW_open-open")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "LIPS_compressed")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "MOUTH_L_smile_closed")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "MOUTH_L_smile_open")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "MOUTH_L_snarl_closed_")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "MOUTH_L_snarl_open")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "MOUTH_grimace")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "MOUTH_smile_closed")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "MOUTH_smile_open")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "MOUTH_Snarl_closed")
        || enAnim->channelList.HasChannel(&skel_ChannelNames, "MOUTH_Snarl_open")) {
        enAnim->bHasMorph = true;
    } else {
        enAnim->bHasMorph = false;
    }

    for (i = 0; i < pHeader->numFrames; i++) {
        if (pGameFrame[i].pChannels) {
            delete[] pGameFrame[i].pChannels;
        }
    }

    delete[] pGameFrame;
    return enAnim;
}

namespace {
typedef struct {
    short shortQuat[4];   
    short shortOffset[3]; 
    short pad;
} ef2SkelBone_t;

typedef struct {
    float length; 
    char  name[32];
} ef2SkelChannel_t;

typedef struct {
    int   ident;           
    int   version;         
    char  name[64];
    int   type;
    int   numFrames;
    int   numChannels;     
    float totaltime;
    float frametime;
    float totaldelta[3];
    int   ofsChannelNames; 
    int   ofsFrames;       
} ef2SkelAnimHeader_t;
} 

skelAnimDataGameHeader_t *skeletor_c::ConvertEF2SkanToGame(void *buffer, int iBuffLength, const char *path)
{
    ef2SkelAnimHeader_t *pEF2 = (ef2SkelAnimHeader_t *)buffer;

    static_assert(sizeof(ef2SkelAnimHeader_t) == 112, "EF2 SKAN header size doesn't match!");
    static_assert(sizeof(ef2SkelBone_t) == 16, "EF2 SKAN bone record size doesn't match!");
    static_assert(sizeof(ef2SkelChannel_t) == 36, "EF2 SKAN channel record size doesn't match!");

    const int numBones        = LittleLong(pEF2->numChannels);
    int       numFrames       = LittleLong(pEF2->numFrames);
    const int ofsChannelNames = LittleLong(pEF2->ofsChannelNames);
    const int ofsFrames       = LittleLong(pEF2->ofsFrames);

    if (numFrames <= 0 || numBones <= 0) {
        return NULL;
    }

    // .ska over-reads on both.  SANCTIONED crash-safety hardening on non-retail
    if (numBones > TIKI_MAX_BONES) {
        TIKI_Error(
            "TIKI_LoadAnimSkel: %s has more than %i bones (%i)\n", path ? path : "?", TIKI_MAX_BONES, numBones
        );
        return NULL;
    }

    const int    numChannels = numBones * 2; 
    const size_t frameStride = 40 + (size_t)numBones * sizeof(ef2SkelBone_t);

    if (ofsFrames < 112 || ofsFrames > iBuffLength || ofsChannelNames < 112
        || (size_t)ofsChannelNames + (size_t)numBones * sizeof(ef2SkelChannel_t) > (size_t)iBuffLength) {
        Com_DPrintf("EF2 SKAN %s: channel/frame table out of bounds; refusing\n", path ? path : "?");
        return NULL;
    }

    {
        const int maxCompleteFrames = (int)(((size_t)iBuffLength - (size_t)ofsFrames) / frameStride);
        if (numFrames > maxCompleteFrames) {
            Com_DPrintf(
                "EF2 SKAN %s: header declares %d frames but file holds only %d complete frames; clamping.\n",
                path ? path : "?", numFrames, maxCompleteFrames);
            numFrames = maxCompleteFrames;
        }
    }
    if (numFrames <= 0) {
        return NULL;
    }

    const size_t headerBytes      = sizeof(skelAnimDataFileHeader_t) + sizeof(skelAnimFileFrame_t) * (numFrames - 1);
    const size_t channelDataBytes = (size_t)numFrames * numChannels * sizeof(vec4_t);
    const size_t channelNameBytes = (size_t)numChannels * sizeof(skelChannelName_t);
    const size_t totalBytes       = headerBytes + channelDataBytes + channelNameBytes;

    byte                     *work = (byte *)Skel_Alloc(totalBytes);
    skelAnimDataFileHeader_t *pOut = (skelAnimDataFileHeader_t *)work;

    pOut->ident          = TIKI_SKC_HEADER_IDENT;
    pOut->version        = TIKI_SKC_HEADER_OLD_VERSION;
    pOut->flags          = 0;
    pOut->nBytesUsed     = (int)totalBytes;
    pOut->frameTime      = LittleFloat(pEF2->frametime);
    pOut->totalDelta[0]  = LittleFloat(pEF2->totaldelta[0]);
    pOut->totalDelta[1]  = LittleFloat(pEF2->totaldelta[1]);
    pOut->totalDelta[2]  = LittleFloat(pEF2->totaldelta[2]);
    pOut->totalAngleDelta = 0.0f;
    pOut->numChannels    = numChannels;
    pOut->numFrames      = numFrames;
    pOut->ofsChannelNames = (int)(headerBytes + channelDataBytes);

    const ef2SkelChannel_t *pChannels  = (const ef2SkelChannel_t *)((const byte *)buffer + ofsChannelNames);
    const byte             *pEF2Frames = (const byte *)buffer + ofsFrames;
    skelAnimFileFrame_t    *pFrames    = pOut->frame;
    vec4_t                 *pChanData  = (vec4_t *)(work + headerBytes);

    for (int f = 0; f < numFrames; f++) {
        const byte  *pf = pEF2Frames + (size_t)f * frameStride;
        const float *fh = (const float *)pf; 
        skelAnimFileFrame_t *df = &pFrames[f];

        df->bounds[0][0] = LittleFloat(fh[0]);
        df->bounds[0][1] = LittleFloat(fh[1]);
        df->bounds[0][2] = LittleFloat(fh[2]);
        df->bounds[1][0] = LittleFloat(fh[3]);
        df->bounds[1][1] = LittleFloat(fh[4]);
        df->bounds[1][2] = LittleFloat(fh[5]);
        df->radius       = LittleFloat(fh[6]);
        df->delta[0]     = LittleFloat(fh[7]);
        df->delta[1]     = LittleFloat(fh[8]);
        df->delta[2]     = LittleFloat(fh[9]);
        df->angleDelta   = 0.0f;
        df->iOfsChannels = 0;

        const ef2SkelBone_t *bones      = (const ef2SkelBone_t *)(pf + 40);
        vec4_t              *frameChans = pChanData + (size_t)f * numChannels;

        static const float EF2_QUAT_SCALE   = 1.0f / 32767.0f; 
        static const float EF2_OFFSET_SCALE = 1.0f / 63.0f;    

        for (int b = 0; b < numBones; b++) {
            const ef2SkelBone_t *bn      = &bones[b];
            float               *rot     = frameChans[b * 2];
            float               *pos     = frameChans[b * 2 + 1];

            rot[0] = (short)LittleShort(bn->shortQuat[0]) * EF2_QUAT_SCALE;
            rot[1] = (short)LittleShort(bn->shortQuat[1]) * EF2_QUAT_SCALE;
            rot[2] = (short)LittleShort(bn->shortQuat[2]) * EF2_QUAT_SCALE;
            rot[3] = (short)LittleShort(bn->shortQuat[3]) * EF2_QUAT_SCALE;

            pos[0] = (short)LittleShort(bn->shortOffset[0]) * EF2_OFFSET_SCALE;
            pos[1] = (short)LittleShort(bn->shortOffset[1]) * EF2_OFFSET_SCALE;
            pos[2] = (short)LittleShort(bn->shortOffset[2]) * EF2_OFFSET_SCALE;
            pos[3] = 0.0f;
        }
    }

    skelChannelName_t *pNames = (skelChannelName_t *)(work + pOut->ofsChannelNames);
    for (int b = 0; b < numBones; b++) {
        char boneName[sizeof(pChannels[b].name) + 1];

        Q_strncpyz(boneName, pChannels[b].name, sizeof(boneName));
        Com_sprintf(pNames[b * 2], sizeof(skelChannelName_t), "%s rot", boneName);
        Com_sprintf(pNames[b * 2 + 1], sizeof(skelChannelName_t), "%s pos", boneName);
    }

    skelAnimDataGameHeader_t *enAnim = ConvertSkelFileToGame(pOut, (int)totalBytes, path);
    Skel_Free(work);

    if (enAnim) {
        enAnim->skan = TIKI_Skan_RetainNative(
            pEF2, (size_t)ofsFrames + (size_t)numFrames * frameStride, path);
        TIKI_Skan_Bind(enAnim, enAnim->skan);
    }

    return enAnim;
}

void WriteEncodedFrames(msg_t *msg, skelAnimDataGameHeader_t *enAnim)
{
    skanChannelHdr *pChannel;
    skanGameFrame  *pFrame;
    int             i, j;

    MSG_WriteLong(msg, enAnim->numFrames);
    MSG_WriteShort(msg, enAnim->nTotalChannels);

    for (i = 0; i < enAnim->nTotalChannels; i++) {
        pChannel = &enAnim->ary_channels[i];
        MSG_WriteShort(msg, pChannel->nFramesInChannel);

        for (j = 0; j < pChannel->nFramesInChannel; j++) {
            vec4_t channelData;

            pFrame = &pChannel->ary_frames[i];
            MSG_WriteShort(msg, pFrame->nFrameNum);
            MSG_WriteShort(msg, pFrame->nPrevFrameIndex);

            channelData[0] = LittleFloat(pFrame->pChannelData[0]);
            channelData[1] = LittleFloat(pFrame->pChannelData[1]);
            channelData[2] = LittleFloat(pFrame->pChannelData[2]);
            channelData[3] = LittleFloat(pFrame->pChannelData[3]);
            MSG_WriteData(msg, channelData, sizeof(vec4_t));
        }
    }

    MSG_WriteLong(msg, enAnim->nBytesUsed);
}

void skeletor_c::SaveProcessedAnim(
    skelAnimDataGameHeader_t *enAnim, const char *path, skelAnimDataFileHeader_t *pHeader
)
{
}

void ReadEncodedFrames(msg_t *msg, skelAnimDataGameHeader_t *enAnim)
{
    skanChannelHdr *pChannel;
    skanGameFrame  *pFrame;
    size_t          frameSize;
    int             frameCnt;
    int             i, j;
    channelType_t   channelType;
    vec4_t          channelData;

    for (i = 0; i < enAnim->nTotalChannels; i++) {
        pChannel = &enAnim->ary_channels[i];

        channelType = GetBoneChannelType(enAnim->channelList.ChannelName(&skel_ChannelNames, i));
        frameCnt    = MSG_ReadShort(msg);

        switch (channelType) {
        case CHANNEL_ROTATION:
            frameSize = (sizeof(skanGameFrame) - sizeof(skanGameFrame::pChannelData) + sizeof(vec4_t));

            pFrame                     = (skanGameFrame *)Skel_Alloc(frameSize * frameCnt);
            pChannel->ary_frames       = pFrame;
            pChannel->nFramesInChannel = frameCnt;

            for (j = 0; j < frameCnt; j++) {
                pFrame                  = (skanGameFrame *)((byte *)pChannel->ary_frames + j * frameSize);
                pFrame->nFrameNum       = MSG_ReadShort(msg);
                pFrame->nPrevFrameIndex = MSG_ReadShort(msg);
                MSG_ReadData(msg, channelData, sizeof(vec4_t));

                pFrame->pChannelData[0] = LittleFloat(channelData[0]);
                pFrame->pChannelData[1] = LittleFloat(channelData[1]);
                pFrame->pChannelData[2] = LittleFloat(channelData[2]);
                pFrame->pChannelData[3] = LittleFloat(channelData[3]);
            }
            break;
        case CHANNEL_POSITION:
            frameSize = (sizeof(skanGameFrame) - sizeof(skanGameFrame::pChannelData) + sizeof(vec3_t));

            pFrame                     = (skanGameFrame *)Skel_Alloc(frameSize * frameCnt);
            pChannel->ary_frames       = pFrame;
            pChannel->nFramesInChannel = frameCnt;

            for (j = 0; j < frameCnt; j++) {
                pFrame                  = (skanGameFrame *)((byte *)pChannel->ary_frames + j * frameSize);
                pFrame->nFrameNum       = MSG_ReadShort(msg);
                pFrame->nPrevFrameIndex = MSG_ReadShort(msg);
                MSG_ReadData(msg, channelData, sizeof(vec4_t));

                pFrame->pChannelData[0] = LittleFloat(channelData[0]);
                pFrame->pChannelData[1] = LittleFloat(channelData[1]);
                pFrame->pChannelData[2] = LittleFloat(channelData[2]);
            }
            break;
        case CHANNEL_NONE:
            pChannel->ary_frames       = NULL;
            pChannel->nFramesInChannel = 0;

            for (j = 0; j < frameCnt; j++) {
                MSG_ReadShort(msg);
                MSG_ReadShort(msg);
                MSG_ReadData(msg, channelData, sizeof(vec4_t));
            }
            break;
        case CHANNEL_VALUE:
        default:
            frameSize = (sizeof(skanGameFrame) - sizeof(skanGameFrame::pChannelData) + sizeof(float));

            pFrame                     = (skanGameFrame *)Skel_Alloc(frameSize * frameCnt);
            pChannel->ary_frames       = pFrame;
            pChannel->nFramesInChannel = frameCnt;

            for (j = 0; j < frameCnt; j++) {
                pFrame                  = (skanGameFrame *)((byte *)pChannel->ary_frames + j * frameSize);
                pFrame->nFrameNum       = MSG_ReadShort(msg);
                pFrame->nPrevFrameIndex = MSG_ReadShort(msg);
                MSG_ReadData(msg, channelData, sizeof(vec4_t));

                pFrame->pChannelData[0] = LittleFloat(channelData[0]);
            }
            break;
        }
    }

    enAnim->nBytesUsed = MSG_ReadLong(msg);
}

void ReadEncodedFramesEx(msg_t *msg, skelAnimDataGameHeader_t *enAnim)
{
    skanChannelHdr *pChannel;
    skanGameFrame  *pFrame;
    size_t          frameSize;
    int             frameCnt;
    int             i, j;
    const char     *name;
    channelType_t   type;

    for (i = 0; i < enAnim->nTotalChannels; i++) {
        pChannel = &enAnim->ary_channels[i];

        name     = enAnim->channelList.ChannelName(&skel_ChannelNames, i);
        type     = GetBoneChannelType(name);
        frameCnt = MSG_ReadShort(msg);

        switch (type) {
        case CHANNEL_ROTATION:
            frameSize                  = (sizeof(skanGameFrame) - sizeof(skanGameFrame::pChannelData) + sizeof(vec4_t));
            pFrame                     = (skanGameFrame *)Skel_Alloc(frameSize * frameCnt);
            pChannel->ary_frames       = pFrame;
            pChannel->nFramesInChannel = frameCnt;

            for (j = 0; j < pChannel->nFramesInChannel; j++) {
                pFrame                  = (skanGameFrame *)((byte *)pChannel->ary_frames + j * frameSize);
                pFrame->nFrameNum       = MSG_ReadShort(msg);
                pFrame->nPrevFrameIndex = MSG_ReadShort(msg);
                pFrame->pChannelData[0] = MSG_ReadFloat(msg);
                pFrame->pChannelData[1] = MSG_ReadFloat(msg);
                pFrame->pChannelData[2] = MSG_ReadFloat(msg);
                pFrame->pChannelData[3] = MSG_ReadFloat(msg);
            }
            break;
        case CHANNEL_POSITION:
            frameSize                  = (sizeof(skanGameFrame) - sizeof(skanGameFrame::pChannelData) + sizeof(vec3_t));
            pFrame                     = (skanGameFrame *)Skel_Alloc(frameSize * frameCnt);
            pChannel->ary_frames       = pFrame;
            pChannel->nFramesInChannel = frameCnt;

            for (j = 0; j < pChannel->nFramesInChannel; j++) {
                pFrame                  = (skanGameFrame *)((byte *)pChannel->ary_frames + j * frameSize);
                pFrame->nFrameNum       = MSG_ReadShort(msg);
                pFrame->nPrevFrameIndex = MSG_ReadShort(msg);
                pFrame->pChannelData[0] = MSG_ReadFloat(msg);
                pFrame->pChannelData[1] = MSG_ReadFloat(msg);
                pFrame->pChannelData[2] = MSG_ReadFloat(msg);
            }
            break;
        case CHANNEL_VALUE:
            frameSize            = (sizeof(skanGameFrame) - sizeof(skanGameFrame::pChannelData) + sizeof(float) * 1);
            pFrame               = (skanGameFrame *)Skel_Alloc(frameSize * frameCnt);
            pChannel->ary_frames = pFrame;
            pChannel->nFramesInChannel = frameCnt;

            for (j = 0; j < pChannel->nFramesInChannel; j++) {
                pFrame                  = (skanGameFrame *)((byte *)pChannel->ary_frames + j * frameSize);
                pFrame->nFrameNum       = MSG_ReadShort(msg);
                pFrame->nPrevFrameIndex = MSG_ReadShort(msg);
                pFrame->pChannelData[0] = MSG_ReadFloat(msg);
            }
            break;
        case CHANNEL_NONE:
            break;
        }
    }
}

skelAnimDataGameHeader_t *skeletor_c::LoadProcessedAnim(const char *path, void *buffer, int len, const char *name)
{
    skelAnimDataGameHeader_t *enAnim;
    int                       i, j;
    msg_t                     msg;
    msg_t                     msgForAnim;
    int                       numChannels;
    skelAnimGameFrame_t      *newFrame;

    MSG_Init(&msg, (byte *)buffer, len);
    msg.cursize = len;
    MSG_BeginReading(&msg);

    numChannels             = MSG_ReadLong(&msg);
    enAnim                  = skelAnimDataGameHeader_t::AllocRLEChannelData(numChannels);
    enAnim->flags           = MSG_ReadLong(&msg);
    enAnim->frameTime       = MSG_ReadFloat(&msg);
    enAnim->totalDelta[0]   = MSG_ReadFloat(&msg);
    enAnim->totalDelta[1]   = MSG_ReadFloat(&msg);
    enAnim->totalDelta[2]   = MSG_ReadFloat(&msg);
    enAnim->totalAngleDelta = MSG_ReadFloat(&msg);
    enAnim->numFrames       = MSG_ReadLong(&msg);
    enAnim->nTotalChannels  = MSG_ReadShort(&msg);
    enAnim->bHasDelta       = MSG_ReadByte(&msg) != 0;
    enAnim->bHasUpper       = MSG_ReadByte(&msg) != 0;
    enAnim->bHasMorph       = MSG_ReadByte(&msg) != 0;

    newFrame        = (skelAnimGameFrame_t *)Skel_Alloc(enAnim->numFrames * sizeof(skelAnimGameFrame_t));
    enAnim->m_frame = newFrame;

    for (i = 0; i < enAnim->numFrames; i++) {
        newFrame->bounds[0][0] = MSG_ReadFloat(&msg);
        newFrame->bounds[0][1] = MSG_ReadFloat(&msg);
        newFrame->bounds[0][2] = MSG_ReadFloat(&msg);
        newFrame->bounds[1][0] = MSG_ReadFloat(&msg);
        newFrame->bounds[1][1] = MSG_ReadFloat(&msg);
        newFrame->bounds[1][2] = MSG_ReadFloat(&msg);
        newFrame->radius       = MSG_ReadFloat(&msg);
        newFrame->delta[0]     = MSG_ReadFloat(&msg);
        newFrame->delta[1]     = MSG_ReadFloat(&msg);
        newFrame->delta[2]     = MSG_ReadFloat(&msg);
        newFrame->angleDelta   = MSG_ReadFloat(&msg);
        newFrame->pChannels    = NULL;
        newFrame++;
    }

    enAnim->bounds[0][0] = MSG_ReadFloat(&msg);
    enAnim->bounds[0][1] = MSG_ReadFloat(&msg);
    enAnim->bounds[0][2] = MSG_ReadFloat(&msg);
    enAnim->bounds[1][0] = MSG_ReadFloat(&msg);
    enAnim->bounds[1][1] = MSG_ReadFloat(&msg);
    enAnim->bounds[1][2] = MSG_ReadFloat(&msg);

    enAnim->numFrames      = MSG_ReadLong(&msg);
    enAnim->nTotalChannels = MSG_ReadShort(&msg);

    msgForAnim = msg;

    for (i = 0; i < enAnim->nTotalChannels; i++) {
        int    frameCnt = MSG_ReadShort(&msgForAnim);
        vec4_t channelData;

        for (j = 0; j < frameCnt; j++) {
            MSG_ReadShort(&msgForAnim);
            MSG_ReadShort(&msgForAnim);
            MSG_ReadData(&msgForAnim, channelData, sizeof(vec4_t));
        }
    }

    enAnim->nBytesUsed = MSG_ReadLong(&msgForAnim);

    numChannels = MSG_ReadLong(&msgForAnim);
    enAnim->channelList.ZeroChannels();

    for (i = 0; i < numChannels; i++) {
        const char *channelName = MSG_ReadString(&msgForAnim);

        if (enAnim->channelList.AddChannel(skel_ChannelNames.RegisterChannel(channelName)) != i) {
            Com_Printf("^~^~^ Animation '%s' has duplicate channel '%s'\n", path, channelName);
        }
    }

    ReadEncodedFrames(&msg, enAnim);
    return enAnim;
}

skelAnimDataGameHeader_t *skeletor_c::LoadProcessedAnimEx(const char *path, void *buffer, int len, const char *name)
{
    skelAnimDataGameHeader_t *enAnim;
    int                       i;
    msg_t                     msg;
    int                       numChannels;
    skelAnimGameFrame_t      *newFrame;

    MSG_Init(&msg, (byte *)buffer, len);
    msg.cursize = len;
    MSG_BeginReading(&msg);

    numChannels = MSG_ReadShort(&msg);
    enAnim      = skelAnimDataGameHeader_t::AllocRLEChannelData(numChannels);
    enAnim->channelList.ZeroChannels();
    enAnim->flags           = MSG_ReadLong(&msg);
    enAnim->frameTime       = MSG_ReadFloat(&msg);
    enAnim->totalDelta[0]   = MSG_ReadFloat(&msg);
    enAnim->totalDelta[1]   = MSG_ReadFloat(&msg);
    enAnim->totalDelta[2]   = MSG_ReadFloat(&msg);
    enAnim->totalAngleDelta = MSG_ReadFloat(&msg);
    enAnim->numFrames       = MSG_ReadLong(&msg);
    enAnim->bHasDelta       = MSG_ReadByte(&msg) != 0;
    enAnim->bHasUpper       = MSG_ReadByte(&msg) != 0;
    enAnim->bHasMorph       = MSG_ReadByte(&msg) != 0;

    for (i = 0; i < enAnim->nTotalChannels; i++) {
        enAnim->channelList.AddChannel(skel_ChannelNames.RegisterChannel(MSG_ReadString(&msg)));
    }

    newFrame        = (skelAnimGameFrame_t *)Skel_Alloc(enAnim->numFrames * sizeof(skelAnimGameFrame_t));
    enAnim->m_frame = newFrame;

    for (i = 0; i < enAnim->numFrames; i++) {
        newFrame->bounds[0][0] = MSG_ReadFloat(&msg);
        newFrame->bounds[0][1] = MSG_ReadFloat(&msg);
        newFrame->bounds[0][2] = MSG_ReadFloat(&msg);
        newFrame->bounds[1][0] = MSG_ReadFloat(&msg);
        newFrame->bounds[1][1] = MSG_ReadFloat(&msg);
        newFrame->bounds[1][2] = MSG_ReadFloat(&msg);
        newFrame->radius       = MSG_ReadFloat(&msg);
        newFrame->delta[0]     = MSG_ReadFloat(&msg);
        newFrame->delta[1]     = MSG_ReadFloat(&msg);
        newFrame->delta[2]     = MSG_ReadFloat(&msg);
        newFrame->angleDelta   = MSG_ReadFloat(&msg);
        newFrame->pChannels    = NULL;
        newFrame++;
    }

    enAnim->bounds[0][0] = MSG_ReadFloat(&msg);
    enAnim->bounds[0][1] = MSG_ReadFloat(&msg);
    enAnim->bounds[0][2] = MSG_ReadFloat(&msg);
    enAnim->bounds[1][0] = MSG_ReadFloat(&msg);
    enAnim->bounds[1][1] = MSG_ReadFloat(&msg);
    enAnim->bounds[1][2] = MSG_ReadFloat(&msg);
    ReadEncodedFramesEx(&msg, enAnim);

    return enAnim;
}
