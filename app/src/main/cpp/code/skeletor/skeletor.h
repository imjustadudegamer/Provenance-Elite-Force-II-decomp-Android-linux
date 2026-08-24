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

#define TAN_BONE_NAME "TAN_root"

#include "../tiki/tiki_shared.h"
#include "SkelVec3.h"
#include "SkelVec4.h"
#include "SkelMat3.h"
#include "SkelMat4.h"
#include "SkelQuat.h"
#include "skeletor_animation_file_format.h"
#include "skeletor_model_file_format.h"
#include "skeletor_name_lists.h"
#include "skeletor_internal.h"

#define MAX_SKEL_BLEND_MOVEMENT_FRAMES 32
#define MAX_SKEL_BLEND_ACTION_FRAMES   32
#define MAX_SKEL_BLEND_FRAMES          (MAX_SKEL_BLEND_MOVEMENT_FRAMES + MAX_SKEL_BLEND_ACTION_FRAMES)

typedef struct skelAnimFrame_s {
    float    radius;
    SkelVec3 bounds[2];
    SkelMat4 bones[1];
} skelAnimFrame_t;

#define SKELANIMFRAME_SIZE(n) \
    (sizeof(skelAnimFrame_t) + ((n) > 0 ? (n) - 1 : 0) * sizeof(SkelMat4))

typedef struct {
    float seconds;
} skelAnimTime_t;

typedef struct {
    float                     weight;
    skelAnimDataGameHeader_t *pAnimationData;
    skelAnimGameFrame_t      *frame;
} skelAnimBlendFrame_t;

typedef struct {
    float                     weight;
    skelAnimDataGameHeader_t *pAnimationData;
    int                       frame;
} skanBlendInfo;

// `Container<skanAnimFrame>&'.  That function is not retail -- it has no symbol in the Mac
#ifdef __cplusplus

class skelAnimStoreFrameList_c
{
public:
    short int     numMovementFrames;
    short int     numActionFrames;
    float         actionWeight;
    skanBlendInfo m_blendInfo[MAX_SKEL_BLEND_FRAMES];

    short int m_ef2Split;
    short int m_ef2BodyPart[MAX_SKEL_BLEND_FRAMES];

public:
    skelAnimStoreFrameList_c();

    SkelQuat GetSlerpValue(int globalChannelNum, int bodyPart = SKEL_BODYPART_UNKNOWN) const;
    void     GetLerpValue3(int globalChannelNum, SkelVec3 *outVec, int bodyPart = SKEL_BODYPART_UNKNOWN) const;

    bool EF2SlotDrives(int blendIndex, int bodyPart) const
    {
        return !m_ef2Split || bodyPart == SKEL_BODYPART_UNKNOWN || m_ef2BodyPart[blendIndex] == SKEL_BODYPART_UNKNOWN
            || m_ef2BodyPart[blendIndex] == bodyPart;
    }
};

class skeletor_c
{
public:
    dtiki_t                *m_Tiki;
    static ChannelNameTable &m_boneNames;
    static ChannelNameTable &m_channelNames;
    static skelBone_World   m_worldBone;

private:
    SkelVec3                 m_frameBounds[2];
    float                    m_frameRadius;
    skelAnimStoreFrameList_c m_frameList;
    short int                m_targetLookLeft;
    short int                m_targetLookRight;
    short int                m_targetLookUp;
    short int                m_targetLookDown;
    short int                m_targetBlink;
    short int                m_timeNextBlink;
    short int                m_headBoneIndex;
    vec3_t                   m_eyeTargetPos;
    skelChannelList_c        m_morphTargetList;
    class skelBone_Base    **m_bone;
    int                      m_numExternalMorphs;
    morph_controller_t       m_externalMorphs[NUM_MORPH_CONTROLLERS];

public:
    skeletor_c(dtiki_t *tiki);
    ~skeletor_c();

    static skelAnimDataGameHeader_t *
    ConvertSkelFileToGame(skelAnimDataFileHeader_t *pHeader, int iBuffLength, const char *path);
    static skelAnimDataGameHeader_t *ConvertEF2SkanToGame(void *buffer, int iBuffLength, const char *path);
    static skelAnimDataGameHeader_t *BuildTANAnim(void *buffer, int iBuffLength, const char *path);
    static void
    SaveProcessedAnim(skelAnimDataGameHeader_t *enAnim, const char *path, skelAnimDataFileHeader_t *pHeader);
    static skelAnimDataGameHeader_t *LoadProcessedAnim(const char *path, void *buffer, int len, const char *name);
    static skelAnimDataGameHeader_t *LoadProcessedAnimEx(const char *path, void *buffer, int len, const char *name);
    void                             LoadMorphTargetNames(skelHeaderGame_t *modelHeader);
    void                             GetFrame(skelAnimFrame_t *newFrame);
    int                              GetMorphWeightFrame(int *data);
    SkelMat4&                        GetBoneFrame(int boneIndex) const;
    bool                             IsBoneOnGround(int boneIndex, float threshold);
    int                              GetMorphWeightFrame(int index, float time, int *data);
    qboolean                         LoadSKB(const char *);
    float                            GetRadius();
    float                            GetCentroidRadius(float *centroid);
    void SetPose(
        const frameInfo_t *frameInfo,
        const int         *contIndices,
        const vec4_t      *contValues,
        float              actionWeight,
        const short int   *ef2BodyParts = nullptr
    );
    void SetEyeTargetPos(const float *pEyeTargetPos);
    int  GetBoneParent(int boneIndex);
    void SetExternalMorphControllers(const morph_controller_t *controllers, int count);
    static class ChannelNameTable *ChannelNames();

private:
    SkelMat4 *BoneTransformation(int, int *, float (*)[4]);
};

#endif

#ifdef __cplusplus
extern "C" {
#endif


    void ConvertToRotationName(const char *boneName, char *rotChannelName);
    void ConvertToPositionName(const char *boneName, char *posChannelName);
    void ConvertToFKRotationName(const char *boneName, char *rotChannelName);
    void ConvertToFKPositionName(const char *boneName, char *rotChannelName);
    void AddToBounds(SkelVec3 *bounds, SkelVec3 *newBounds);
#ifdef __cplusplus
    void TIKI_GetSkelAnimFrameInternal2(
        dtiki_t                  *tiki,
        skelBoneCache_t          *bones,
        skelAnimStoreFrameList_c *frameList,
        float                    *radius,
        vec3_t                   *mins,
        vec3_t                   *maxes
    );
    void SkeletorGetAnimFrame2(
        skelHeaderGame_t         *skelmodel,
        skelChannelList_c        *boneList,
        skelBoneCache_t          *bones,
        skelAnimStoreFrameList_c *frameList,
        float                    *radius,
        vec3_t                   *mins,
        vec3_t                   *maxes
    );
#endif
    void SkeletorGetAnimFrame(
        skelHeaderGame_t         *skelmodel,
        skelAnimDataGameHeader_t *animData,
        skelChannelList_c        *boneList,
        skelBoneCache_t          *bones,
        int                       frame,
        float                    *radius,
        vec3_t                   *mins,
        vec3_t                   *maxes
    );
    void TIKI_GetSkelAnimFrame(dtiki_t *tiki, skelBoneCache_t *bones, float *radius, vec3_t *mins, vec3_t *maxes);
    void TIKI_GetSkelAnimFrame2(
        dtiki_t *tiki, skelBoneCache_t *bones, int anim, int frame, float *radius, vec3_t *mins, vec3_t *maxes
    );
    void TIKI_GetSkelAnimFrameInternal(
        dtiki_t                  *tiki,
        skelBoneCache_t          *bones,
        skelAnimDataGameHeader_t *animData,
        int                       frame,
        float                    *radius,
        vec3_t                   *mins,
        vec3_t                   *maxes
    );


    void Skel_DPrintf(const char *fmt, ...) Q_PRINTF_FUNC(1, 2);

#ifndef _DEBUG_MEM
    void  Skel_Free(void *ptr);
    void *Skel_Alloc(size_t size);
#else
#    define Skel_Free(ptr)   free(ptr)
#    define Skel_Alloc(size) malloc(size)
#endif

    void Skel_FreeFile(void *buffer);
    int  Skel_ReadFileEx(const char *qpath, void **buffer, qboolean quiet);


    void SKEL_Message(const char *fmt, ...) Q_PRINTF_FUNC(1, 2);
    void SKEL_Warning(const char *fmt, ...) Q_PRINTF_FUNC(1, 2);
    void SKEL_Error(const char *fmt, ...) Q_PRINTF_FUNC(1, 2);
    void Skel_ExtractFilePath(const char *path, char *dest);
    void Skel_ExtractFileBase(const char *path, char *dest);
    void Skel_ExtractFileExtension(const char *path, char *dest);
    void Skel_ExtractFileName(const char *path, char *dest);
    int  FileLength(FILE *pFile);


    void SkeletorLoadBoneFromBuffer(skelChannelList_c *boneList, boneData_t *boneData, skelBone_Base **bone);
    void SkeletorLoadBonesFromBuffer(skelChannelList_c *boneList, skelHeaderGame_t *buffer, skelBone_Base **bone);


    typedef struct skmMorphDelta_s {
        int    index;    
        vec3_t delta;    
    } skmMorphDelta_t;

    typedef struct skmMorphTarget_s {
        char             name[64]; 
        int              numVerts;
        skmMorphDelta_t *verts;
    } skmMorphTarget_t;

    typedef struct skmMorphData_s {
        int               numMorphVertices; 
        int               numTargets;
        skmMorphTarget_t *targets;
        int              *pIndices;
    } skmMorphData_t;

    skmMorphData_t *TIKI_LoadMorphFile(const char *path);
    void            TIKI_FreeMorphData(skmMorphData_t *data);

#ifdef __cplusplus
}
#endif
