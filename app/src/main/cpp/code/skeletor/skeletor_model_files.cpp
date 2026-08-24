

#include "q_shared.h"
#include "qcommon.h"
#include "skeletor.h"

int CreateRotationBoneFileData(
    const char *newBoneName, const char *newBoneParentName, SkelVec3 basePos, boneFileData_t *fileData
)
{
    char       *saveChannelName;
    char       *rotChannelName;
    boneData_t *boneData;

    strncpy(fileData->name, newBoneName, sizeof(fileData->name));
    strncpy(fileData->parent, newBoneParentName, sizeof(fileData->parent));
    fileData->boneType        = SKELBONE_ROTATION;
    fileData->ofsBaseData     = sizeof(boneFileData_t);
    boneData                  = (boneData_t *)((char *)fileData + fileData->ofsBaseData);
    boneData->offset[0]       = basePos[0];
    boneData->offset[1]       = basePos[1];
    boneData->offset[2]       = basePos[2];
    boneData->length          = 1.0f;
    boneData->weight          = 1.0f;
    boneData->bendRatio       = 1.0f;
    fileData->ofsChannelNames = fileData->ofsBaseData + 24;
    saveChannelName           = (char *)((char *)fileData + fileData->ofsChannelNames);

    rotChannelName = (char *)Skel_Alloc(strlen(newBoneName) + 5);
    ConvertToRotationName(newBoneName, rotChannelName);
    strcpy(saveChannelName, rotChannelName);
    fileData->ofsBoneNames = fileData->ofsChannelNames + (int)strlen(rotChannelName) + 1;
    fileData->ofsEnd       = fileData->ofsBoneNames;
    return fileData->ofsEnd;
}

int CreatePosRotBoneFileData(char *newBoneName, char *newBoneParentName, boneFileData_t *fileData)
{
    int         channelNamesLength;
    char       *saveChannelName;
    char       *rotChannelName;
    char       *posChannelName;
    boneData_t *boneData;

    strncpy(fileData->name, newBoneName, sizeof(fileData->name));
    strncpy(fileData->parent, newBoneParentName, sizeof(fileData->parent));
    fileData->boneType        = SKELBONE_POSROT;
    fileData->ofsBaseData     = sizeof(boneFileData_t);
    boneData                  = (boneData_t *)((char *)fileData + fileData->ofsBaseData);
    boneData->offset[0]       = 1.0f;
    boneData->offset[1]       = 1.0f;
    boneData->offset[2]       = 1.0f;
    fileData->ofsChannelNames = fileData->ofsBaseData + 12;
    saveChannelName           = (char *)((char *)fileData + fileData->ofsChannelNames);

    rotChannelName = (char *)Skel_Alloc(strlen(newBoneName) + 5);
    ConvertToRotationName(newBoneName, rotChannelName);
    strcpy(saveChannelName, rotChannelName);
    Skel_Free(rotChannelName);

    channelNamesLength = (int)strlen(saveChannelName);
    saveChannelName    = (char *)((char *)fileData + fileData->ofsChannelNames + channelNamesLength);
    posChannelName     = (char *)Skel_Alloc(strlen(newBoneName) + 5);
    ConvertToPositionName(newBoneName, posChannelName);
    strcpy(saveChannelName, posChannelName);
    Skel_Free(posChannelName);

    channelNamesLength += (int)strlen(saveChannelName);

    fileData->ofsBoneNames = fileData->ofsChannelNames + channelNamesLength + 2;
    fileData->ofsEnd       = fileData->ofsBoneNames;
    return fileData->ofsEnd;
}

void CreatePosRotBoneData(const char *newBoneName, const char *newBoneParentName, boneData_t *boneData)
{
    char *rotChannelName;
    char *posChannelName;

    boneData->channel = skel_BoneNames.RegisterChannel(newBoneName);

    if (!strcmp(newBoneParentName, "worldbone")) {
        boneData->parent = -1;
    } else {
        boneData->parent = skel_BoneNames.RegisterChannel(newBoneParentName);
        assert(boneData->parent >= 0);
    }

    boneData->boneType = SKELBONE_POSROT;

    rotChannelName = (char *)Skel_Alloc(strlen(newBoneName) + 5);
    ConvertToRotationName(newBoneName, rotChannelName);
    boneData->channelIndex[0] = skel_ChannelNames.RegisterChannel(rotChannelName);
    if (boneData->channelIndex[0] < 0) {
        SKEL_Warning("Channel named %s not added. (Bone will not work without it)\n", rotChannelName);
        boneData->boneType = SKELBONE_ZERO;
    }
    Skel_Free(rotChannelName);

    posChannelName = (char *)Skel_Alloc(strlen(newBoneName) + 5);
    ConvertToPositionName(newBoneName, posChannelName);
    boneData->channelIndex[1] = skel_ChannelNames.RegisterChannel(posChannelName);
    if (boneData->channelIndex[1] < 0) {
        SKEL_Warning("Channel named %s not added. (Bone will not work without it)\n", posChannelName);
        boneData->boneType = SKELBONE_ZERO;
    }
    Skel_Free(posChannelName);

    boneData->numChannels = 2;
    boneData->numRefs     = 0;
}

int CreateIKShoulderBoneFileData(
    const char *newBoneName, const char *newBoneParentName, SkelQuat baseOrient, SkelVec3 basePos, boneData_t *boneData
)
{
    return 0;
}

int CreateIKElbowBoneFileData(
    const char *newBoneName, const char *newBoneParentName, SkelVec3 basePos, boneData_t *boneData
)
{
    return 0;
}

int CreateIKWristBoneFileData(
    const char     *newBoneName,
    const char     *newBoneParentName,
    const char     *shoulderBoneName,
    SkelVec3        basePos,
    boneFileData_t *fileData
)
{
    return 0;
}

int CreateHoseRotBoneFileData(
    char           *newBoneName,
    char           *newBoneParentName,
    char           *targetBoneName,
    float           bendRatio,
    float           bendMax,
    float           spinRatio,
    hoseRotType_t   hoseRotType,
    SkelVec3        basePos,
    boneFileData_t *fileData
)
{
    return 0;
}

int CreateAvRotBoneFileData(
    char           *newBoneName,
    char           *newBoneParentName,
    char           *baseBoneName,
    char           *targetBoneName,
    float           rotRatio,
    SkelVec3        basePos,
    boneFileData_t *fileData
)
{
    return 0;
}

void skeletor_c::LoadMorphTargetNames(skelHeaderGame_t *modelHeader)
{
    int   numTargets;
    char *newTargetName;
    int   i;
    int   newChannel;
    int   morphTargetIndex;

    numTargets    = modelHeader->numMorphTargets;
    newTargetName = modelHeader->pMorphTargets;

    for (i = 0; i < numTargets; i++) {
        newChannel       = skel_ChannelNames.RegisterChannel(newTargetName);
        morphTargetIndex = m_morphTargetList.AddChannel(newChannel);

        if (!strncmp(newTargetName, "EYES_left", 9)) {
            m_targetLookLeft = morphTargetIndex;
        } else if (!strncmp(newTargetName, "EYES_right", 10)) {
            m_targetLookRight = morphTargetIndex;
        } else if (!strncmp(newTargetName, "EYES_up", 7)) {
            m_targetLookUp = morphTargetIndex;
        } else if (!strncmp(newTargetName, "EYES_down", 9)) {
            m_targetLookDown = morphTargetIndex;
        } else if (!strncmp(newTargetName, "EYES_blink", 9)) {
            m_targetBlink = morphTargetIndex;
        }

        newTargetName += strlen(newTargetName) + 1;
    }
}

skmMorphData_t *TIKI_LoadMorphFile(const char *path)
{
    void          *buffer = NULL;
    int            length;
    byte          *base;
    int            version;
    int            numMorphVertices, numTargets, ofsMorphTargets, ofsMorphIndices;
    skmMorphData_t *data;
    int            i, j;

    length = Skel_ReadFileEx(path, &buffer, qtrue);
    if (length <= 0 || !buffer) {
        return NULL;
    }

    base = (byte *)buffer;

    if (length < 0x58 || base[0] != 'M' || base[1] != 'R' || base[2] != 'P' || base[3] != 'H') {
        Skel_FreeFile(buffer);
        return NULL;
    }

    version = LittleLong(*(int *)(base + 4));
    if (version != 2) {
        Skel_FreeFile(buffer);
        return NULL;
    }

    numMorphVertices = LittleLong(*(int *)(base + 0x48));
    numTargets       = LittleLong(*(int *)(base + 0x4C));
    ofsMorphIndices  = LittleLong(*(int *)(base + 0x50));
    ofsMorphTargets  = LittleLong(*(int *)(base + 0x54));

    if (numTargets <= 0 || ofsMorphTargets <= 0 || (size_t)ofsMorphTargets + (size_t)numTargets * 72 > (size_t)length) {
        Skel_FreeFile(buffer);
        return NULL;
    }

    // TIKI_LoadMorph @0x000F52A0 has no size check at all.  SANCTIONED crash-safety
    // hardening on non-retail content: the value must at least fit the file it came
    if (numMorphVertices < 0 || (size_t)numMorphVertices * sizeof(int) > (size_t)length) {
        Skel_FreeFile(buffer);
        return NULL;
    }

    data = (skmMorphData_t *)Skel_Alloc(sizeof(skmMorphData_t));
    if (!data) {
        Skel_FreeFile(buffer);
        return NULL;
    }
    data->numMorphVertices = numMorphVertices;
    data->numTargets       = numTargets;
    data->targets          = (skmMorphTarget_t *)Skel_Alloc(sizeof(skmMorphTarget_t) * numTargets);
    data->pIndices         = NULL;
    if (!data->targets) {
        TIKI_FreeMorphData(data);
        Skel_FreeFile(buffer);
        return NULL;
    }

    if (numMorphVertices > 0 && ofsMorphIndices > 0
        && (size_t)ofsMorphIndices + (size_t)numMorphVertices * 4 <= (size_t)length) {
        data->pIndices = (int *)Skel_Alloc(sizeof(int) * numMorphVertices);
        if (!data->pIndices) {
            Skel_Free(data->targets);
            Skel_Free(data);
            Skel_FreeFile(buffer);
            return NULL;
        }
        for (i = 0; i < numMorphVertices; i++) {
            data->pIndices[i] = LittleLong(*(int *)(base + ofsMorphIndices + i * 4));
        }
    } else {
        data->numMorphVertices = 0;
    }

    for (i = 0; i < numTargets; i++) {
        byte             *t        = base + ofsMorphTargets + i * 72;
        int               numVerts = LittleLong(*(int *)(t + 0x40));
        int               ofsVerts = LittleLong(*(int *)(t + 0x44));
        skmMorphTarget_t *out      = &data->targets[i];

        Q_strncpyz(out->name, (const char *)t, sizeof(out->name));
        out->numVerts = numVerts;
        out->verts    = NULL;

        if (numVerts <= 0 || ofsVerts <= 0 || (size_t)ofsVerts + (size_t)numVerts * 16 > (size_t)length) {
            out->numVerts = 0;
            continue;
        }

        out->verts = (skmMorphDelta_t *)Skel_Alloc(sizeof(skmMorphDelta_t) * numVerts);
        for (j = 0; j < numVerts; j++) {
            byte *v = base + ofsVerts + j * 16;

            out->verts[j].index    = LittleLong(*(int *)(v + 0));
            out->verts[j].delta[0] = LittleFloat(*(float *)(v + 4));
            out->verts[j].delta[1] = LittleFloat(*(float *)(v + 8));
            out->verts[j].delta[2] = LittleFloat(*(float *)(v + 12));
        }
    }

    Skel_FreeFile(buffer);
    return data;
}

void TIKI_FreeMorphData(skmMorphData_t *data)
{
    int i;

    if (!data) {
        return;
    }

    if (data->targets) {
        for (i = 0; i < data->numTargets; i++) {
            if (data->targets[i].verts) {
                Skel_Free(data->targets[i].verts);
            }
        }
        Skel_Free(data->targets);
    }

    if (data->pIndices) {
        Skel_Free(data->pIndices);
    }

    Skel_Free(data);
}
