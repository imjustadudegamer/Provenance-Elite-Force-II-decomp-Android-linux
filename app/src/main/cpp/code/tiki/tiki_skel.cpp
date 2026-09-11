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
#include "../skeletor/skeletor.h"
#include "../client/client.h"
#include "../corepp/tiki.h"
#include "../tiki/tiki_skan.h"

#define TIKI_MAX_MORPH_TARGETS 128

ChannelNameTable skel_BoneNames;
ChannelNameTable skel_ChannelNames;


void LoadBoneFromBuffer2(boneFileData_t *fileData, boneData_t *boneData)
{
    char *newChannelName;
    char *newBoneRefName;
    int   i;

    memset(boneData, 0, sizeof(boneData_t));
    boneData->channel  = skel_BoneNames.RegisterChannel(fileData->name);
    boneData->boneType = fileData->boneType;

    if (boneData->boneType == SKELBONE_HOSEROT) {
        i = LittleLongPtr(fileData->parent[fileData->ofsBaseData + 4]);

        if (i == 1) {
            boneData->boneType = SKELBONE_HOSEROTPARENT;
        } else if (i == 2) {
            boneData->boneType = SKELBONE_HOSEROTBOTH;
        }
    }

    newChannelName        = (char *)fileData + fileData->ofsChannelNames;
    boneData->numChannels = skelBone_Base::GetNumChannels(boneData->boneType);

    for (i = 0; i < boneData->numChannels; i++) {
        boneData->channelIndex[i] = skel_ChannelNames.RegisterChannel(newChannelName);
        if (boneData->channelIndex[i] < 0) {
            SKEL_Warning("Channel named %s not added. (Bone will not work without it)\n", newChannelName);
            boneData->boneType = SKELBONE_ZERO;
        }

        newChannelName += strlen(newChannelName) + 1;
    }

    newBoneRefName    = (char *)fileData + fileData->ofsBoneNames;
    boneData->numRefs = skelBone_Base::GetNumBoneRefs(boneData->boneType);

    for (i = 0; i < boneData->numRefs; i++) {
        boneData->refIndex[i] = skel_BoneNames.RegisterChannel(newBoneRefName);
        newBoneRefName += strlen(newBoneRefName) + 1;
    }

    if (!strcmp(fileData->parent, SKEL_BONENAME_WORLD)) {
        boneData->parent = -1;
    } else {
        boneData->parent = skel_BoneNames.RegisterChannel(fileData->parent);
    }

    switch (boneData->boneType) {
    case SKELBONE_ROTATION:
        {
            float *baseData = (float *)((char *)fileData + fileData->ofsBaseData);
            CopyLittleLong(&boneData->offset[0], &baseData[0]);
            CopyLittleLong(&boneData->offset[1], &baseData[1]);
            CopyLittleLong(&boneData->offset[2], &baseData[2]);
            break;
        }
    case SKELBONE_IKSHOULDER:
        {
            float *baseData = (float *)((char *)fileData + fileData->ofsBaseData);
            CopyLittleLong(&boneData->offset[0], &baseData[4]);
            CopyLittleLong(&boneData->offset[1], &baseData[5]);
            CopyLittleLong(&boneData->offset[2], &baseData[6]);
            break;
        }
    case SKELBONE_IKELBOW:
    case SKELBONE_IKWRIST:
        {
            float *pBaseData = (float *)((char *)fileData + fileData->ofsBaseData);
            vec3_t baseData;
            CopyLittleLong(&baseData[0], &pBaseData[0]);
            CopyLittleLong(&baseData[1], &pBaseData[1]);
            CopyLittleLong(&baseData[2], &pBaseData[2]);
            boneData->length = VectorLength(baseData);
            break;
        }
    case SKELBONE_AVROT:
        {
            float *baseData = (float *)((char *)fileData + fileData->ofsBaseData);
            CopyLittleLong(&boneData->length, &baseData[0]);
            CopyLittleLong(&boneData->offset[0], &baseData[1]);
            CopyLittleLong(&boneData->offset[1], &baseData[2]);
            CopyLittleLong(&boneData->offset[2], &baseData[3]);
            break;
        }
    case SKELBONE_HOSEROT:
    case SKELBONE_HOSEROTBOTH:
    case SKELBONE_HOSEROTPARENT:
        {
            float *baseData = (float *)((char *)fileData + fileData->ofsBaseData);
            CopyLittleLong(&boneData->offset[0], &baseData[3]);
            CopyLittleLong(&boneData->offset[1], &baseData[4]);
            CopyLittleLong(&boneData->offset[2], &baseData[5]);
            CopyLittleLong(&boneData->bendRatio, &baseData[0]);
            CopyLittleLong(&boneData->bendMax, &baseData[1]);
            CopyLittleLong(&boneData->spinRatio, &baseData[2]);
            break;
        }
    default:
        break;
    }
}

/*
 * TIKI_SurfaceGameBytes -- one surface's block in the game-side cache: the
 * header, the triangles, the vertices and the collapse table, in the order
 * the copy pass below lays them out.  Both passes read this one statement, so
 * the allocation cannot disagree with the walk that fills it.  They used to
 * state the block twice -- the size pass padded a running total across
 * surfaces, the copy pass padded each block from its own start -- and the two
 * compositions differ on real data: the copy pass needed up to 8 bytes more
 * than the size pass had reserved on 55 of the 259 shipped .skb, so the last
 * surface's collapse table wrote past the surfaces region into the bone block
 * (erased again by the memset below, which is why it never showed).  Measured
 * by personal/scratch/vk_m3_skb_audit.c, pass 4.
 */
static size_t TIKI_SurfaceGameBytes(skelSurface_t *pSurf, size_t *nVertBytesOut)
{
    skeletorVertex_t *pVert    = (skeletorVertex_t *)((byte *)pSurf + LongNoSwapPtr(&pSurf->ofsVerts));
    const int         numVerts = LongNoSwapPtr(&pSurf->numVerts);
    size_t            nVertBytes;
    size_t            nBytes;
    int               j;

    nVertBytes = 0;
    for (j = 0; j < numVerts; j++) {
        const int numMorphs  = LongNoSwapPtr(&pVert->numMorphs);
        const int numWeights = LongNoSwapPtr(&pVert->numWeights);

        // (retail TIKI_SwapSkel @0x000F4C90:95-104) never bounds them.  Both loaders
        const size_t iOffset = sizeof(skeletorMorph_t) * numMorphs + sizeof(skelWeight_t) * numWeights
                             + sizeof(skeletorVertex_t);

        nVertBytes += iOffset;
        pVert = (skeletorVertex_t *)((byte *)pVert + iOffset);
    }

    nBytes  = sizeof(skelSurfaceGame_t);
    nBytes += LongNoSwapPtr(&pSurf->numTriangles) * sizeof(skelIndex_t) * 3;
    nBytes  = PAD(nBytes, sizeof(void *));
    nBytes += nVertBytes;
    nBytes  = PAD(nBytes, sizeof(void *));
    nBytes += numVerts * sizeof(int);

    if (nVertBytesOut) {
        *nVertBytesOut = nVertBytes;
    }

    return nBytes;
}

void TIKI_CacheFileSkel(skelHeader_t *pHeader, skelcache_t *cache, int length)
{
    skelHeaderGame_t  *pSkel;
    skelSurfaceGame_t *pGameSurf;
    skelSurface_t     *pSurf;
    int                i, j, k;
    size_t             nSurfBytes;
    size_t             nBoneBytes;
    size_t             nBoxBytes;
    size_t             nMorphBytes;
    size_t             nVertBytes;
    size_t             nTriBytes;
    byte              *start_gs_ptr;
    byte              *gs_ptr = NULL;

    pSurf      = (skelSurface_t *)((byte *)pHeader + pHeader->ofsSurfaces);
    nSurfBytes = 0;

    for (i = 0; i < pHeader->numSurfaces; i++) {
        nSurfBytes += TIKI_SurfaceGameBytes(pSurf, NULL);

        pSurf = (skelSurface_t *)((byte *)pSurf + LongNoSwapPtr(&pSurf->ofsEnd));
    }

    nBoneBytes  = pHeader->numBones * sizeof(boneData_t);
    nBoxBytes   = 0;
    nMorphBytes = 0;

    if (pHeader->version > TIKI_SKB_HEADER_VERSION) {
        char    *pMorphTargets;
        intptr_t nLen;

        nBoxBytes     = pHeader->numBoxes * sizeof(skelHitBox_t);
        pMorphTargets = (char *)((byte *)pHeader + pHeader->ofsMorphTargets);

        if (pHeader->ofsMorphTargets > 0 || (pHeader->ofsMorphTargets + pHeader->numMorphTargets) < length) {
            for (i = 0; i < pHeader->numMorphTargets; i++) {
                nLen = strlen(pMorphTargets) + 1;
                nMorphBytes += nLen;
                pMorphTargets += nLen;
            }
        } else {
            nMorphBytes = pHeader->numMorphTargets;
        }
    } else if (pHeader->version == TIKI_SKB_HEADER_VERSION) {
        nBoxBytes = pHeader->numBoxes * sizeof(skelHitBox_t);
    }

    cache->size = sizeof(skelHeaderGame_t);
    cache->size += nSurfBytes;
    cache->size = PAD(cache->size, sizeof(void *));
    cache->size += nBoneBytes;
    cache->size = PAD(cache->size, sizeof(void *));
    cache->size += nBoxBytes;
    cache->size = PAD(cache->size, sizeof(void *));
    cache->size += nMorphBytes;

    cache->skel = pSkel = (skelHeaderGame_t *)TIKI_Alloc(cache->size);
    if (!pSkel) {
        TIKI_Error("TIKI_CacheFileSkel: could not allocate %i bytes for %s\n", (int)cache->size, pHeader->name);
        cache->skel = NULL;
        return;
    }
    byte *start_ptr     = (byte *)pSkel;
    byte *ptr           = start_ptr + sizeof(skelHeaderGame_t);

    pSkel->pMorphData = NULL;

    pSkel->tanNumFrames   = 0;
    pSkel->tanFrameTime   = 0.0f;
    pSkel->pTanFrameXform = NULL;
    pSkel->tanRadiusFrames = 0;
    pSkel->pTanFrames      = NULL;

    pSkel->version     = pHeader->version;

    if (pHeader->version >= TIKI_SKD_HEADER_VERSION) {
        pSkel->scale = pHeader->scale * 0.52;
    } else {
        pSkel->scale = 0.52;
    }

    pSkel->numSurfaces = pHeader->numSurfaces;
    pSkel->numBones    = pHeader->numBones;
    pSkel->pSurfaces   = (skelSurfaceGame_t *)ptr;
    ptr += nSurfBytes;
    ptr           = (byte *)PADP(ptr, sizeof(void *));
    pSkel->pBones = (boneData_t *)ptr;
    ptr += nBoneBytes;
    ptr             = (byte *)PADP(ptr, sizeof(void *));
    pSkel->numBoxes = (nBoxBytes > 0) ? pHeader->numBoxes : 0;
    pSkel->pBoxes   = (skelHitBox_t *)ptr;
    ptr += nBoxBytes;
    ptr                    = (byte *)PADP(ptr, sizeof(void *));
    pSkel->numMorphTargets = (nMorphBytes > 0 && pHeader->numMorphTargets <= TIKI_MAX_MORPH_TARGETS)
                                 ? pHeader->numMorphTargets
                                 : 0;
    pSkel->pMorphTargets   = (char *)ptr;
    ptr += nMorphBytes;
    ptr = (byte *)PADP(ptr, sizeof(void *));
    memcpy(pSkel->name, pHeader->name, sizeof(pSkel->name));

    pSurf     = (skelSurface_t *)((byte *)pHeader + pHeader->ofsSurfaces);
    pGameSurf = pSkel->pSurfaces;

    for (i = 0; i < pHeader->numSurfaces; i++) {
        size_t            nBytesUsed;
        skeletorVertex_t *pVert;
        const int         numTriangles = LongNoSwapPtr(&pSurf->numTriangles);
        const int         numVerts     = LongNoSwapPtr(&pSurf->numVerts);

        nTriBytes  = numTriangles * sizeof(skelIndex_t) * 3;
        nSurfBytes = numVerts * sizeof(int);

        nBytesUsed = TIKI_SurfaceGameBytes(pSurf, &nVertBytes);

        start_gs_ptr = (byte *)pGameSurf;
        gs_ptr       = start_gs_ptr + sizeof(skelSurfaceGame_t);

        pGameSurf->ident         = LongNoSwapPtr(&pSurf->ident);
        pGameSurf->numTriangles  = numTriangles;
        pGameSurf->numVerts      = numVerts;
        pGameSurf->numModelVerts = LongNoSwapPtr(&pSurf->numModelVerts);
        pGameSurf->pStaticXyz    = NULL;
        pGameSurf->pStaticNormal = NULL;
        pGameSurf->pStaticTexCoords = NULL;
        pGameSurf->pTanXyz       = NULL;
        pGameSurf->pTriangles    = (skelIndex_t *)gs_ptr;
        gs_ptr += nTriBytes;
        gs_ptr            = (byte *)PADP(gs_ptr, sizeof(void *));
        pGameSurf->pVerts = (skeletorVertex_t *)gs_ptr;
        gs_ptr += nVertBytes;
        gs_ptr               = (byte *)PADP(gs_ptr, sizeof(void *));
        pGameSurf->pCollapse = (int *)gs_ptr;
        gs_ptr += sizeof(*pGameSurf->pCollapse) * numVerts;

        Q_strncpyz(pGameSurf->name, pSurf->name, sizeof(pSurf->name));

        if (pGameSurf->numTriangles) {
            const int *pTriangles = (const int *)((byte *)pSurf + LongNoSwapPtr(&pSurf->ofsTriangles));
            for (j = 0; j < numTriangles; j++) {
                pGameSurf->pTriangles[j * 3 + 0] = LongNoSwapPtr(&pTriangles[j * 3 + 0]);
                pGameSurf->pTriangles[j * 3 + 1] = LongNoSwapPtr(&pTriangles[j * 3 + 1]);
                pGameSurf->pTriangles[j * 3 + 2] = LongNoSwapPtr(&pTriangles[j * 3 + 2]);
            }
        } else {
            pGameSurf->pTriangles = NULL;
        }

        if (pGameSurf->numVerts) {
            skeletorVertex_t *skelVert = pGameSurf->pVerts;
            pVert                      = (skeletorVertex_t *)((char *)pSurf + LongNoSwapPtr(&pSurf->ofsVerts));

            for (j = 0; j < pGameSurf->numVerts; j++) {
                const int numMorphs  = LongNoSwapPtr(&pVert->numMorphs);
                const int numWeights = LongNoSwapPtr(&pVert->numWeights);

                const size_t iOffset = sizeof(skeletorMorph_t) * numMorphs + sizeof(skelWeight_t) * numWeights
                                     + sizeof(skeletorVertex_t);

                skelVert->normal[0]    = FloatNoSwapPtr(&pVert->normal[0]);
                skelVert->normal[1]    = FloatNoSwapPtr(&pVert->normal[1]);
                skelVert->normal[2]    = FloatNoSwapPtr(&pVert->normal[2]);
                skelVert->texCoords[0] = FloatNoSwapPtr(&pVert->texCoords[0]);
                skelVert->texCoords[1] = FloatNoSwapPtr(&pVert->texCoords[1]);
                skelVert->numWeights   = numWeights;
                skelVert->numMorphs    = numMorphs;

                skeletorMorph_t *pMorph    = (skeletorMorph_t *)((byte *)pVert + sizeof(skeletorVertex_t));
                skeletorMorph_t *skelMorph = (skeletorMorph_t *)((byte *)skelVert + sizeof(skeletorVertex_t));
                for (k = 0; k < numMorphs; k++, pMorph++, skelMorph++) {
                    skelMorph->morphIndex = LongNoSwapPtr(&pMorph->morphIndex);
                    skelMorph->offset[0]  = FloatNoSwapPtr(&pMorph->offset[0]);
                    skelMorph->offset[1]  = FloatNoSwapPtr(&pMorph->offset[1]);
                    skelMorph->offset[2]  = FloatNoSwapPtr(&pMorph->offset[2]);
                }

                skelWeight_t *pWeight    = (skelWeight_t *)((byte *)pMorph);
                skelWeight_t *skelWeight = (skelWeight_t *)((byte *)skelMorph);
                for (k = 0; k < numWeights; k++, skelWeight++, pWeight++) {
                    const int boneIndex = LongNoSwapPtr(&pWeight->boneIndex);

                    // (retail does not range-check it either -- TIKI_SwapSkel
                    skelWeight->boneIndex  = (boneIndex >= 0 && boneIndex < pHeader->numBones) ? boneIndex : 0;
                    skelWeight->boneWeight = FloatNoSwapPtr(&pWeight->boneWeight);
                    skelWeight->offset[0]  = FloatNoSwapPtr(&pWeight->offset[0]);
                    skelWeight->offset[1]  = FloatNoSwapPtr(&pWeight->offset[1]);
                    skelWeight->offset[2]  = FloatNoSwapPtr(&pWeight->offset[2]);
                }

                pVert    = (skeletorVertex_t *)((byte *)pVert + iOffset);
                skelVert = (skeletorVertex_t *)((byte *)skelVert + iOffset);
            }

            assert((byte *)skelVert - (byte *)pGameSurf <= (byte *)pGameSurf->pCollapse - (byte *)pGameSurf);

            const int *pCollapse = (const int *)((byte *)pSurf + LongNoSwapPtr(&pSurf->ofsCollapse));
            for (j = 0; j < numVerts; j++) {
                pGameSurf->pCollapse[j] = LongNoSwapPtr(&pCollapse[j]);
            }
        } else {
            pGameSurf->pVerts    = NULL;
            pGameSurf->pCollapse = NULL;
        }

        if (i != pHeader->numSurfaces - 1) {
            pGameSurf->pNext = (skelSurfaceGame_t *)gs_ptr;
            pGameSurf        = pGameSurf->pNext;
            pSurf            = (skelSurface_t *)((byte *)pSurf + LongNoSwapPtr(&pSurf->ofsEnd));
        }
    }

    pGameSurf->pNext = NULL;

    assert(((byte *)pGameSurf - (byte *)pSkel) <= cache->size);

    if (nBoneBytes) {
        memset(pSkel->pBones, 0, nBoneBytes);

        if (pHeader->version <= TIKI_SKB_HEADER_VERSION) {
            skelBoneName_t *TIKI_bones = (skelBoneName_t *)((byte *)pHeader + LongNoSwapPtr(&pHeader->ofsBones));
            for (i = 0; i < pSkel->numBones; i++) {
                const char *boneName;

                if (TIKI_bones[i].parent == -1) {
                    boneName = SKEL_BONENAME_WORLD;
                } else {
                    boneName = TIKI_bones[TIKI_bones[i].parent].name;
                }

                CreatePosRotBoneData(TIKI_bones[i].name, boneName, &pSkel->pBones[i]);

                pSkel->pBones[i].ef2BodyPart = (short int)TIKI_bones[i].flags;

                pSkel->pBones[i].skelParent = (short int)TIKI_bones[i].parent;

            }
        } else {
            boneFileData_t *boneBuffer = (boneFileData_t *)((byte *)pHeader + LongNoSwapPtr(&pHeader->ofsBones));
            for (i = 0; i < pSkel->numBones; i++) {
                LoadBoneFromBuffer2(boneBuffer, &pSkel->pBones[i]);
                pSkel->pBones[i].ef2BodyPart = SKEL_BODYPART_UNKNOWN;
                boneBuffer = (boneFileData_t *)((byte *)boneBuffer + LongNoSwapPtr(&boneBuffer->ofsEnd));
            }

            for (i = 0; i < pSkel->numBones; i++) {
                pSkel->pBones[i].skelParent = -1;
                if (pSkel->pBones[i].parent < 0) {
                    continue;
                }
                for (int b = 0; b < pSkel->numBones; b++) {
                    if (pSkel->pBones[b].channel == pSkel->pBones[i].parent) {
                        pSkel->pBones[i].skelParent = (short int)b;
                        break;
                    }
                }
            }
        }
    } else {
        pSkel->numBones = 0;
        pSkel->pBones   = NULL;
    }

    if (pHeader->version <= TIKI_SKB_HEADER_VER_3) {
        pSkel->numBoxes        = 0;
        pSkel->pBoxes          = NULL;
        pSkel->numMorphTargets = 0;
        pSkel->pMorphTargets   = NULL;
        return;
    }

    if (nBoxBytes) {
        if (pHeader->ofsBoxes <= 0 || (nBoxBytes + pHeader->ofsBoxes) > (size_t)length) {
            Com_Printf("^~^~^ Box data is corrupted for '%s'\n", cache->path);
            pSkel->numBoxes = 0;
            pSkel->pBoxes   = NULL;
        } else {
            memcpy(pSkel->pBoxes, ((byte *)pHeader + pHeader->ofsBoxes), nBoxBytes);
        }
    } else {
        pSkel->numBoxes = 0;
        pSkel->pBoxes   = NULL;
    }

    if (pHeader->version <= TIKI_SKB_HEADER_VERSION) {
        pSkel->numMorphTargets = 0;
        pSkel->pMorphTargets   = NULL;
        return;
    }

    if (nMorphBytes) {
        if (pHeader->ofsMorphTargets <= 0 || (nMorphBytes + pHeader->ofsMorphTargets) > (size_t)length) {
            Com_Printf("^~^~^ Morph targets data is corrupted for '%s'\n", cache->path);
            pSkel->numMorphTargets = 0;
            pSkel->pMorphTargets   = NULL;
        } else {
            memcpy(pSkel->pMorphTargets, ((byte *)pHeader + pHeader->ofsMorphTargets), nMorphBytes);
        }
    } else {
        pSkel->numMorphTargets = 0;
        pSkel->pMorphTargets   = NULL;
    }
}


#define TIKI_TAN_IDENT (('T') | ('A' << 8) | ('N' << 16) | (' ' << 24)) 

static int TAN_ReadInt(const byte *p)
{
    int v;
    memcpy(&v, p, 4);
    return LittleLong(v);
}

static float TAN_ReadFloat(const byte *p)
{
    float v;
    memcpy(&v, p, 4);
    return LittleFloat(v);
}

qboolean TIKI_LoadTAN(const char *path, skelcache_t *cache)
{
    byte  *buf;
    int    length;
    int    ident, version, numFrames, numSurfaces, ofsFrames, ofsSurfaces;
    int    i, j;
    size_t nSurfBytes, nBoneBytes;

    length = TIKI_ReadFileEx(path, (void **)&buf, qtrue);
    if (length < 0) {
        TIKI_DPrintf("TIKI_LoadTAN: Couldn't load %s\n", path);
        return qfalse;
    }

    ident = TAN_ReadInt(buf + 0x00);
    if (ident != TIKI_TAN_IDENT) {
        TIKI_Error("TIKI_LoadTAN: '%s' has invalid header (not a TAN model)\n", path);
        TIKI_FreeFile(buf);
        return qfalse;
    }
    version = TAN_ReadInt(buf + 0x04);
    if (version != 2) {
        TIKI_Error("TIKI_LoadTAN: '%s' has wrong version (%i should be 2)\n", path, version);
        TIKI_FreeFile(buf);
        return qfalse;
    }

    numFrames   = TAN_ReadInt(buf + 0x48);
    numSurfaces = TAN_ReadInt(buf + 0x50);
    ofsFrames   = TAN_ReadInt(buf + 0x64);
    ofsSurfaces = TAN_ReadInt(buf + 0x68);

    const float totaltime = TAN_ReadFloat(buf + 0x54);

    int numTags = TAN_ReadInt(buf + 0x4C);
    int ofsTags = TAN_ReadInt(buf + 0x6C);
    if (numTags < 0) {
        numTags = 0;
    }

    if (numFrames < 1 || numSurfaces < 0) {
        TIKI_DPrintf("TIKI_LoadTAN: '%s' has no frames\n", path);
        TIKI_FreeFile(buf);
        return qfalse;
    }

    if (numTags > 0 && (ofsTags < 0 || TAN_TAG_NAME_OFS(ofsTags, numFrames, numTags) > (size_t)length)) {
        TIKI_DPrintf("TIKI_LoadTAN: '%s' tag block runs past end of file; ignoring %i tag(s)\n", path, numTags);
        numTags = 0;
    }

    if (numTags > TIKI_MAX_BONES - 1) {
        TIKI_Error("TIKI_LoadTAN: %s has more than %i bones (%i)\n", path, TIKI_MAX_BONES, 1 + numTags);
        TIKI_FreeFile(buf);
        return qfalse;
    }

    if (ofsFrames < 0 || ofsSurfaces < 0 || (size_t)ofsFrames + (size_t)numFrames * 68 > (size_t)length
        || (numSurfaces > 0 && (size_t)ofsSurfaces + 104 > (size_t)length)) {
        TIKI_Error("TIKI_LoadAnimTan: %s has a bad frame/surface offset\n", path);
        TIKI_FreeFile(buf);
        return qfalse;
    }

    if (numSurfaces > 0) {
        const byte *pSurf = buf + ofsSurfaces;

        for (i = 0; i < numSurfaces; i++) {
            const int    numVerts = TAN_ReadInt(pSurf + 0x48);
            const int    numTri   = TAN_ReadInt(pSurf + 0x50);
            const int    ofsTri   = TAN_ReadInt(pSurf + 0x54);
            const int    ofsColl  = TAN_ReadInt(pSurf + 0x58);
            const int    ofsSt    = TAN_ReadInt(pSurf + 0x5C);
            const int    ofsXyz   = TAN_ReadInt(pSurf + 0x60);
            const int    ofsEnd   = TAN_ReadInt(pSurf + 0x64);
            const size_t stBytes  = (size_t)numVerts * 8;
            const size_t xyzBytes = (size_t)numVerts * (size_t)numFrames * 8;
            const size_t triBytes = (size_t)numTri * 3 * sizeof(int);
            const size_t colBytes = (size_t)numVerts * sizeof(int);

            if ((size_t)(pSurf - buf) + 104 > (size_t)length || numVerts < 0 || numTri < 0 || ofsEnd <= 0
                || (size_t)(pSurf - buf) + (size_t)ofsEnd > (size_t)length || ofsTri < 0
                || (size_t)ofsTri + triBytes > (size_t)ofsEnd || ofsColl < 0
                || (size_t)ofsColl + colBytes > (size_t)ofsEnd || ofsSt < 0
                || (size_t)ofsSt + stBytes > (size_t)ofsEnd || ofsXyz < 0
                || (size_t)ofsXyz + xyzBytes > (size_t)ofsEnd) {
                TIKI_Error("TIKI_LoadAnimTan: %s surface %i is out of bounds\n", path, i);
                TIKI_FreeFile(buf);
                return qfalse;
            }

            if (numVerts > 3000) {
                TIKI_Error("TIKI_LoadAnimTan: %s has more than %i verts on a surface (%i)", path, 3000, numVerts);
            }
            if (3 * numTri > 18000) {
                TIKI_Error("TIKI_LoadAnimTan: %s has more than %i triangles on a surface (%i)", path, 6000, numTri);
            }

            // Hard structural cap, SANCTIONED crash-safety on non-retail content: the
            if (numVerts > TIKI_MAX_VERTEXES) {
                TIKI_Error(
                    "TIKI_LoadAnimTan: %s has more than %i verts on a surface (%i)", path, TIKI_MAX_VERTEXES, numVerts
                );
                TIKI_FreeFile(buf);
                return qfalse;
            }

            pSurf += ofsEnd;
        }
    }

    const byte *frame0 = buf + ofsFrames;
    float       scale[3], offset[3];
    for (i = 0; i < 3; i++) {
        scale[i]  = TAN_ReadFloat(frame0 + 0x18 + i * 4);
        offset[i] = TAN_ReadFloat(frame0 + 0x24 + i * 4);
    }

    memset(cache, 0, sizeof(skelcache_t));
    Q_strncpyz(cache->path, path, sizeof(cache->path));

    int tanFrames = (numFrames > 1 && numSurfaces > 0 && ofsFrames > 0) ? numFrames : 0;

    nSurfBytes = 0;
    {
        const byte *pSurf = buf + ofsSurfaces;
        for (i = 0; i < numSurfaces; i++) {
            const int numVerts = TAN_ReadInt(pSurf + 0x48);
            const int numTri   = TAN_ReadInt(pSurf + 0x50);
            const int ofsEnd   = TAN_ReadInt(pSurf + 0x64);

            nSurfBytes += sizeof(skelSurfaceGame_t);
            nSurfBytes += numTri * sizeof(skelIndex_t) * 3;
            nSurfBytes = PAD(nSurfBytes, sizeof(void *));
            nSurfBytes += numVerts * (sizeof(skeletorVertex_t) + sizeof(skelWeight_t));
            nSurfBytes = PAD(nSurfBytes, sizeof(void *));
            nSurfBytes += numVerts * sizeof(int);

            pSurf += ofsEnd;
        }
    }

    nBoneBytes = (1 + numTags) * sizeof(boneData_t); 

    size_t nTanBytes = 0;
    if (tanFrames) {
        const byte *pSurf = buf + ofsSurfaces;

        nTanBytes = (size_t)tanFrames * 6 * sizeof(float);
        for (i = 0; i < numSurfaces; i++) {
            const int numVerts = TAN_ReadInt(pSurf + 0x48);
            const int ofsEnd   = TAN_ReadInt(pSurf + 0x64);

            nTanBytes = PAD(nTanBytes, sizeof(void *));
            nTanBytes += (size_t)numVerts * (size_t)tanFrames * sizeof(tanXyzNormal_t);
            pSurf += ofsEnd;
        }
    }


    const size_t nTanFrameRecBytes = (size_t)numFrames * sizeof(tikiFrame_t);

    cache->size = sizeof(skelHeaderGame_t);
    cache->size += nSurfBytes;
    cache->size = PAD(cache->size, sizeof(void *));
    cache->size += nBoneBytes;
    if (nTanBytes || nTanFrameRecBytes) {
        cache->size = PAD(cache->size, sizeof(void *));
        cache->size += nTanBytes;
        cache->size = PAD(cache->size, sizeof(void *));
        cache->size += nTanFrameRecBytes;
    }

    skelHeaderGame_t *pSkel = (skelHeaderGame_t *)TIKI_Alloc(cache->size);
    if (!pSkel) {
        TIKI_Error("TIKI_LoadTAN: could not allocate %i bytes for %s\n", (int)cache->size, path);
        cache->skel = NULL;
        TIKI_FreeFile(buf);
        return qfalse;
    }
    cache->skel             = pSkel;
    byte *start_ptr         = (byte *)pSkel;
    byte *ptr               = start_ptr + sizeof(skelHeaderGame_t);

    pSkel->version         = TIKI_SKD_HEADER_VERSION;
    pSkel->scale           = 1.0f; 
    pSkel->numSurfaces     = numSurfaces;
    pSkel->numBones        = 1 + numTags;
    pSkel->pSurfaces       = numSurfaces ? (skelSurfaceGame_t *)ptr : NULL;
    ptr += nSurfBytes;
    ptr             = (byte *)PADP(ptr, sizeof(void *));
    pSkel->pBones   = (boneData_t *)ptr;
    ptr += nBoneBytes;
    pSkel->numBoxes        = 0;
    pSkel->pBoxes          = NULL;
    pSkel->numMorphTargets = 0;
    pSkel->pMorphTargets   = NULL;
    pSkel->pMorphData      = NULL; 

    pSkel->tanNumFrames   = 0;
    pSkel->tanFrameTime   = 0.0f;
    pSkel->pTanFrameXform = NULL;
    pSkel->tanRadiusFrames = 0;
    pSkel->pTanFrames      = NULL;

    byte *tan_ptr = NULL;
    if (nTanBytes || nTanFrameRecBytes) {
        ptr      = (byte *)PADP(ptr, sizeof(void *));
        tan_ptr  = ptr;
    }
    if (nTanBytes) {
        pSkel->tanNumFrames   = tanFrames;
        pSkel->tanFrameTime   = (totaltime > 0.0f) ? (totaltime / (float)numFrames) : 0.1f;
        pSkel->pTanFrameXform = (float *)tan_ptr;

        for (i = 0; i < tanFrames; i++) {
            const byte *pf = buf + ofsFrames + (size_t)i * 68;
            for (j = 0; j < 3; j++) {
                pSkel->pTanFrameXform[i * 6 + j]     = TAN_ReadFloat(pf + 0x18 + j * 4); 
                pSkel->pTanFrameXform[i * 6 + 3 + j] = TAN_ReadFloat(pf + 0x24 + j * 4); 
            }
        }

        tan_ptr += (size_t)tanFrames * 6 * sizeof(float);
    }

    Q_strncpyz(pSkel->name, path, sizeof(pSkel->name));

    CreatePosRotBoneData(TAN_BONE_NAME, SKEL_BONENAME_WORLD, &pSkel->pBones[0]);
    pSkel->pBones[0].skelParent = -1;

    for (int t = 0; t < numTags; t++) {
        char        tagName[TAN_TAG_NAME_SIZE];
        const byte *pTag = buf + TAN_TAG_NAME_OFS(ofsTags, numFrames, t);
        Q_strncpyz(tagName, (const char *)pTag, sizeof(tagName));
        CreatePosRotBoneData(tagName, TAN_BONE_NAME, &pSkel->pBones[1 + t]);
        pSkel->pBones[1 + t].skelParent = 0;
    }

    const byte        *pSurf     = buf + ofsSurfaces;
    skelSurfaceGame_t *pGameSurf = pSkel->pSurfaces;
    byte              *gs_ptr    = (byte *)pGameSurf;

    for (i = 0; i < numSurfaces; i++) {
        const int   numVerts  = TAN_ReadInt(pSurf + 0x48);
        const int   numTri    = TAN_ReadInt(pSurf + 0x50);
        const int   ofsTri    = TAN_ReadInt(pSurf + 0x54);
        const int   ofsColl   = TAN_ReadInt(pSurf + 0x58);
        const int   ofsSt     = TAN_ReadInt(pSurf + 0x5C);
        const int   ofsXyz    = TAN_ReadInt(pSurf + 0x60);
        const int   ofsEnd    = TAN_ReadInt(pSurf + 0x64);

        gs_ptr = (byte *)pGameSurf + sizeof(skelSurfaceGame_t);

        pGameSurf->ident            = 0;
        pGameSurf->numTriangles     = numTri;
        pGameSurf->numVerts         = numVerts;
        pGameSurf->numModelVerts    = TAN_ReadInt(pSurf + 0x4C);
        pGameSurf->pStaticXyz       = NULL;
        pGameSurf->pStaticNormal    = NULL;
        pGameSurf->pStaticTexCoords = NULL;
        Q_strncpyz(pGameSurf->name, (const char *)(pSurf + 0x04), 64);

        pGameSurf->pTriangles = (skelIndex_t *)gs_ptr;
        gs_ptr += numTri * sizeof(skelIndex_t) * 3;
        gs_ptr = (byte *)PADP(gs_ptr, sizeof(void *));
        {
            const byte *pt = pSurf + ofsTri;
            for (j = 0; j < numTri * 3; j++) {
                pGameSurf->pTriangles[j] = (skelIndex_t)TAN_ReadInt(pt + j * 4);
            }
        }

        pGameSurf->pVerts = (skeletorVertex_t *)gs_ptr;
        {
            byte       *vptr = gs_ptr;
            const byte *pst  = pSurf + ofsSt;
            const byte *pxyz = pSurf + ofsXyz; 
            for (j = 0; j < numVerts; j++) {
                skeletorVertex_t *sv = (skeletorVertex_t *)vptr;

                const byte    *vx = pxyz + j * 8;
                unsigned int   rx = (unsigned int)vx[0] | ((unsigned int)vx[1] << 8);
                unsigned int   ry = (unsigned int)vx[2] | ((unsigned int)vx[3] << 8);
                unsigned int   rz = (unsigned int)vx[4] | ((unsigned int)vx[5] << 8);
                unsigned int   nrm = (unsigned int)vx[6] | ((unsigned int)vx[7] << 8);
                float px = (float)rx * scale[0] + offset[0];
                float py = (float)ry * scale[1] + offset[1];
                float pz = (float)rz * scale[2] + offset[2];

                int   lat = (nrm >> 8) & 0xFF;
                int   lng = nrm & 0xFF;
                float flat = lat * (2.0f * M_PI / 255.0f);
                float flng = lng * (2.0f * M_PI / 255.0f);
                sv->normal[0] = cos(flat) * sin(flng);
                sv->normal[1] = sin(flat) * sin(flng);
                sv->normal[2] = cos(flng);

                sv->texCoords[0] = TAN_ReadFloat(pst + j * 8 + 0);
                sv->texCoords[1] = TAN_ReadFloat(pst + j * 8 + 4);
                sv->numWeights   = 1;
                sv->numMorphs    = 0;

                skelWeight_t *w = (skelWeight_t *)(vptr + sizeof(skeletorVertex_t));
                w->boneIndex    = 0;
                w->boneWeight   = 1.0f;
                w->offset[0]    = px;
                w->offset[1]    = py;
                w->offset[2]    = pz;

                vptr += sizeof(skeletorVertex_t) + sizeof(skelWeight_t);
            }
            gs_ptr = vptr;
        }
        gs_ptr = (byte *)PADP(gs_ptr, sizeof(void *));

        pGameSurf->pCollapse = (int *)gs_ptr;
        gs_ptr += numVerts * sizeof(int);
        {
            const int *pColl = (const int *)(pSurf + ofsColl);
            for (j = 0; j < numVerts; j++) {
                pGameSurf->pCollapse[j] = TAN_ReadInt((const byte *)&pColl[j]);
            }
        }

        pGameSurf->pTanXyz = NULL;
        if (tanFrames) {
            tan_ptr = (byte *)PADP(tan_ptr, sizeof(void *));

            tanXyzNormal_t *dst   = (tanXyzNormal_t *)tan_ptr;
            const byte     *src   = pSurf + ofsXyz;
            const int       count = numVerts * tanFrames;

            pGameSurf->pTanXyz = dst;
            for (j = 0; j < count; j++) {
                const byte *v = src + (size_t)j * 8;
                dst[j].xyz[0] = (unsigned short)((unsigned int)v[0] | ((unsigned int)v[1] << 8));
                dst[j].xyz[1] = (unsigned short)((unsigned int)v[2] | ((unsigned int)v[3] << 8));
                dst[j].xyz[2] = (unsigned short)((unsigned int)v[4] | ((unsigned int)v[5] << 8));
                dst[j].normal = (short)((unsigned int)v[6] | ((unsigned int)v[7] << 8));
            }

            tan_ptr += (size_t)count * sizeof(tanXyzNormal_t);
        }

        if (i != numSurfaces - 1) {
            pGameSurf->pNext = (skelSurfaceGame_t *)gs_ptr;
            pGameSurf        = pGameSurf->pNext;
        } else {
            pGameSurf->pNext = NULL;
        }

        pSurf += ofsEnd;
    }

    if (nTanFrameRecBytes) {
        tan_ptr = (byte *)PADP(tan_ptr, sizeof(void *));
        pSkel->pTanFrames      = (tikiFrame_t *)tan_ptr;
        pSkel->tanRadiusFrames = numFrames;

        for (i = 0; i < numFrames; i++) {
            const byte *pf = buf + ofsFrames + (size_t)i * 68;

            for (j = 0; j < 3; j++) {
                pSkel->pTanFrames[i].bounds[0][j] = TAN_ReadFloat(pf + 0x00 + j * 4);
                pSkel->pTanFrames[i].bounds[1][j] = TAN_ReadFloat(pf + 0x0C + j * 4);
                pSkel->pTanFrames[i].scale[j]      = TAN_ReadFloat(pf + 0x18 + j * 4);
                pSkel->pTanFrames[i].offset[j]     = TAN_ReadFloat(pf + 0x24 + j * 4);
                pSkel->pTanFrames[i].delta[j]      = TAN_ReadFloat(pf + 0x30 + j * 4);
            }

            pSkel->pTanFrames[i].radius    = TAN_ReadFloat(pf + 0x3C);
            pSkel->pTanFrames[i].frametime = TAN_ReadFloat(pf + 0x40);
        }

        tan_ptr += nTanFrameRecBytes;
    }

    TIKI_FreeFile(buf);
    return qtrue;
}

qboolean TIKI_LoadSKB(const char *path, skelcache_t *cache)
{
    int            i, j, k;
    skelHeader_t  *pheader;
    skelSurface_t *surf;
    int            version;
    unsigned int   header;
    int            length;
    char          *buf;
    int            totalVerts;
    size_t         newLength;
    skelHeader_t  *newHeader;
    skelSurface_t *oldSurf;
    skelSurface_t *newSurf;

    length = TIKI_ReadFileEx(path, (void **)&buf, true);
    if (length < 0) {
        TIKI_DPrintf("Tiki:LoadAnim Couldn't load %s\n", path);
        return qfalse;
    }

    pheader = (skelHeader_t *)TIKI_Alloc(length);
    memcpy(pheader, buf, length);
    TIKI_FreeFile(buf);
    memset(cache, 0, sizeof(skelcache_t));
    strncpy(cache->path, path, sizeof(cache->path));

    header = pheader->ident;
    if (header != (unsigned int)TIKI_SKB_HEADER_IDENT) {
        TIKI_Error("TIKI_LoadSKB: Tried to load '%s' as a skeletal base frame (File has invalid header)\n", path);
        TIKI_Free(pheader);
        return qfalse;
    }

    version = LittleLong(pheader->version);
    if (version != TIKI_SKB_HEADER_VER_3 && version != TIKI_SKB_HEADER_VERSION) {
        TIKI_Error(
            "TIKI_LoadSKB: %s has wrong version (%i should be %i or %i)\n",
            path,
            version,
            TIKI_SKB_HEADER_VER_3,
            TIKI_SKB_HEADER_VERSION
        );
        TIKI_Free(pheader);
        return qfalse;
    }

    int ef2BaseFrame = 0;
    if (version == TIKI_SKB_HEADER_VERSION && LittleLong(pheader->ofsBones) == TIKI_SKB_EF2_HEADER_SIZE) {
        ef2BaseFrame    = LittleLong(pheader->ofsEnd);
        version         = TIKI_SKB_HEADER_VER_3;
        pheader->version = TIKI_SKB_HEADER_VER_3;
    }

    TIKI_SwapSkel(pheader);

    if (pheader->ofsSurfaces < 0 || (size_t)pheader->ofsSurfaces > (size_t)length) {
        TIKI_Error("TIKI_LoadSkel: %s has a bad surface offset (%i)\n", path, pheader->ofsSurfaces);
        TIKI_Free(pheader);
        return qfalse;
    }

    totalVerts = 0;
    surf       = (skelSurface_t *)((char *)pheader + pheader->ofsSurfaces);
    for (i = 0; i < pheader->numSurfaces; i++) {
        int          numSurfVerts, numSurfTriangles;
        int          ofsSurfVerts, ofsSurfTriangles, ofsSurfCollapse, ofsSurfEnd;
        const byte  *pVert;
        const byte  *pSurfEnd;
        size_t       surfUsed;
        const size_t vertHeader = sizeof(skelVertex_t) - sizeof(skelWeight_t); 

        surfUsed = (size_t)((char *)surf - (char *)pheader);
        if (surfUsed + sizeof(skelSurface_t) > (size_t)length) {
            TIKI_Error("TIKI_LoadSkel: %s surface %i runs past the end of the file\n", path, i);
            TIKI_Free(pheader);
            return qfalse;
        }

        numSurfVerts     = LongNoSwapPtr(&surf->numVerts);
        numSurfTriangles = LongNoSwapPtr(&surf->numTriangles);
        ofsSurfTriangles = LongNoSwapPtr(&surf->ofsTriangles);
        ofsSurfVerts     = LongNoSwapPtr(&surf->ofsVerts);
        ofsSurfCollapse  = LongNoSwapPtr(&surf->ofsCollapse);
        ofsSurfEnd       = LongNoSwapPtr(&surf->ofsEnd);

        if (numSurfVerts < 0 || numSurfTriangles < 0 || ofsSurfEnd < (int)sizeof(skelSurface_t)
            || surfUsed + (size_t)ofsSurfEnd > (size_t)length || ofsSurfVerts < (int)sizeof(skelSurface_t)
            || ofsSurfVerts > ofsSurfEnd || ofsSurfTriangles < (int)sizeof(skelSurface_t)
            || ofsSurfTriangles > ofsSurfEnd || ofsSurfCollapse < (int)sizeof(skelSurface_t)
            || ofsSurfCollapse > ofsSurfEnd) {
            TIKI_Error("TIKI_LoadSkel: %s surface %i has a bad offset or count\n", path, i);
            TIKI_Free(pheader);
            return qfalse;
        }

        if ((size_t)numSurfTriangles * 3 * sizeof(unsigned int) > (size_t)(ofsSurfEnd - ofsSurfTriangles)
            || (size_t)numSurfVerts * sizeof(unsigned int) > (size_t)(ofsSurfEnd - ofsSurfCollapse)) {
            TIKI_Error("TIKI_LoadSkel: %s surface %i table overruns its record\n", path, i);
            TIKI_Free(pheader);
            return qfalse;
        }

        if (numSurfVerts > 3000) {
            TIKI_Error("TIKI_LoadSkel: %s has more than %i verts on a surface (%i)", path, 3000, numSurfVerts);
        }
        if (3 * numSurfTriangles > 18000) {
            TIKI_Error("TIKI_LoadSkel: %s has more than %i triangles on a surface (%i)", path, 6000, numSurfTriangles);
        }

        // Hard structural cap, SANCTIONED crash-safety on non-retail content: the
        if (numSurfVerts > TIKI_MAX_VERTEXES) {
            TIKI_Error(
                "TIKI_LoadSkel: %s has more than %i verts on a surface (%i)", path, TIKI_MAX_VERTEXES, numSurfVerts
            );
            TIKI_Free(pheader);
            return qfalse;
        }

        pVert    = (const byte *)surf + ofsSurfVerts;
        pSurfEnd = (const byte *)surf + ofsSurfEnd;
        for (j = 0; j < numSurfVerts; j++) {
            int    numWeights;
            size_t remaining;

            if ((size_t)(pSurfEnd - pVert) < vertHeader) {
                TIKI_Error("TIKI_LoadSkel: %s surface %i vertex %i runs past its record\n", path, i, j);
                TIKI_Free(pheader);
                return qfalse;
            }

            numWeights = LongNoSwapPtr((const int *)(pVert + 20));
            remaining  = (size_t)(pSurfEnd - pVert) - vertHeader;
            if (numWeights < 0 || (size_t)numWeights > remaining / sizeof(skelWeight_t)) {
                TIKI_Error("TIKI_LoadSkel: %s surface %i vertex %i has %i weights\n", path, i, j, numWeights);
                TIKI_Free(pheader);
                return qfalse;
            }

            pVert += vertHeader + (size_t)numWeights * sizeof(skelWeight_t);
        }

        if (numSurfVerts > INT_MAX - totalVerts) {
            TIKI_Error("TIKI_LoadSkel: %s has too many total vertices\n", path);
            TIKI_Free(pheader);
            return qfalse;
        }

        totalVerts += numSurfVerts;
        surf = (skelSurface_t *)((char *)surf + ofsSurfEnd);
    }

    if (pheader->numBones < 0 || pheader->numBones > TIKI_MAX_BONES) {
        TIKI_Error("TIKI_LoadSkel: %s has more than %i bones (%i)\n", path, TIKI_MAX_BONES, pheader->numBones);
        TIKI_Free(pheader);
        return qfalse;
    }

    newLength = (size_t)totalVerts * sizeof(unsigned int);
    if (newLength > (size_t)INT_MAX || (size_t)length + newLength > (size_t)INT_MAX) {
        TIKI_Error("TIKI_LoadSkel: %s repack size overflows\n", path);
        TIKI_Free(pheader);
        return qfalse;
    }

    newHeader = (skelHeader_t *)TIKI_Alloc((size_t)length + newLength);
    if (!newHeader) {
        TIKI_Error("TIKI_LoadSkel: could not allocate %i bytes for %s\n", (int)((size_t)length + newLength), path);
        TIKI_Free(pheader);
        return qfalse;
    }
    memcpy(newHeader, pheader, pheader->ofsSurfaces);

    if (newHeader->ofsBones > newHeader->ofsSurfaces) {
        newHeader->ofsBones += (int)newLength;
    }
    if (newHeader->ofsBoxes > newHeader->ofsSurfaces) {
        newHeader->ofsBoxes += (int)newLength;
    }
    if (newHeader->ofsEnd > newHeader->ofsSurfaces) {
        newHeader->ofsEnd += (int)newLength;
    }

    oldSurf = (skelSurface_t *)((byte *)pheader + pheader->ofsSurfaces);
    newSurf = (skelSurface_t *)((byte *)newHeader + newHeader->ofsSurfaces);

    for (i = 0; i < pheader->numSurfaces; i++) {
        skeletorVertex_t *newVerts;
        skelVertex_t     *oldVerts;
        int               numVerts;

        memcpy(newSurf, oldSurf, oldSurf->ofsVerts);
        if (newSurf->ofsCollapse > LongNoSwapPtr(&newSurf->ofsVerts)) {
            int newOffset =
                LongNoSwapPtr(&newSurf->ofsCollapse) + sizeof(unsigned int) * LongNoSwapPtr(&newSurf->numVerts);
            memcpy(&newSurf->ofsCollapse, &newOffset, sizeof(newSurf->ofsCollapse));
        }
        if (newSurf->ofsTriangles > LongNoSwapPtr(&newSurf->ofsVerts)) {
            int newOffset =
                LongNoSwapPtr(&newSurf->ofsTriangles) + sizeof(unsigned int) * LongNoSwapPtr(&newSurf->numVerts);
            memcpy(&newSurf->ofsTriangles, &newOffset, sizeof(newSurf->ofsTriangles));
        }
        if (newSurf->ofsEnd > LongNoSwapPtr(&newSurf->ofsVerts)) {
            int newOffset = LongNoSwapPtr(&newSurf->ofsEnd) + sizeof(unsigned int) * LongNoSwapPtr(&newSurf->numVerts);
            memcpy(&newSurf->ofsEnd, &newOffset, sizeof(newSurf->ofsEnd));
        }

        oldVerts = (skelVertex_t *)((byte *)oldSurf + oldSurf->ofsVerts);
        newVerts = (skeletorVertex_t *)((byte *)newSurf + newSurf->ofsVerts);

        numVerts = LongNoSwapPtr(&oldSurf->numVerts);
        for (j = 0; j < numVerts; j++) {
            int numMorphs, numWeights;

            memcpy(newVerts->normal, oldVerts->normal, sizeof(newVerts->normal));
            memcpy(newVerts->texCoords, oldVerts->texCoords, sizeof(newVerts->texCoords));

            numMorphs  = 0;
            numWeights = LongNoSwapPtr(&oldVerts->numWeights);
            memcpy(&newVerts->numMorphs, &numMorphs, sizeof(newVerts->numMorphs));
            memcpy(&newVerts->numWeights, &numWeights, sizeof(newVerts->numWeights));

            skelWeight_t *newWeights = (skelWeight_t *)((byte *)newVerts + sizeof(skeletorVertex_t));

            for (k = 0; k < numWeights; k++) {
                memcpy(newWeights, &oldVerts->weights[k], sizeof(skelWeight_t));
                newWeights++;
            }

            oldVerts = (skelVertex_t *)((byte *)oldVerts + sizeof(skelWeight_t) * numWeights
                                        + (sizeof(skelVertex_t) - sizeof(skelWeight_t)));
            newVerts =
                (skeletorVertex_t *)((byte *)newVerts + sizeof(skeletorVertex_t) + sizeof(skelWeight_t) * numWeights);
        }

        memcpy(newVerts, oldVerts, LongNoSwapPtr(&oldSurf->ofsEnd) - ((byte *)oldVerts - (byte *)oldSurf));
        oldSurf = (skelSurface_t *)((byte *)oldSurf + LongNoSwapPtr(&oldSurf->ofsEnd));
        newSurf = (skelSurface_t *)((byte *)newSurf + LongNoSwapPtr(&newSurf->ofsEnd));
    }

    if ((size_t)((char *)oldSurf - (char *)pheader) > (size_t)pheader->ofsEnd) {
        TIKI_Error("TIKI_LoadSkel: %s surface walk overruns the header end offset\n", path);
        TIKI_Free(newHeader);
        TIKI_Free(pheader);
        return qfalse;
    }

    memcpy(newSurf, oldSurf, pheader->ofsEnd - ((char *)oldSurf - (char *)pheader));

    short baseFrameRows[TIKI_MAX_BONES][8];
    int   numSkelBones = 0;
    if (ef2BaseFrame > 0 && pheader->numBones > 0 && pheader->numBones <= TIKI_MAX_BONES
        && (size_t)ef2BaseFrame + 16 * (size_t)pheader->numBones <= (size_t)length) {
        numSkelBones = pheader->numBones;
        for (i = 0; i < numSkelBones; i++) {
            const short *row = (const short *)((byte *)pheader + ef2BaseFrame + 16 * i);
            for (k = 0; k < 8; k++) {
                baseFrameRows[i][k] = LittleShort(row[k]);
            }
        }
    }

    TIKI_Free(pheader);
    TIKI_CacheFileSkel(newHeader, cache, length);
    TIKI_Free(newHeader);

    if (!cache->skel) {
        return qfalse;
    }

    if (numSkelBones > 0 && cache->skel && cache->skel->pBones) {
        for (i = 0; i < cache->skel->numBones && i < numSkelBones; i++) {
            boneData_t *bone = &cache->skel->pBones[i];

            memcpy(bone->baseRow, baseFrameRows[i], sizeof(bone->baseRow));
            bone->hasBaseRow = qtrue;

            bone->offset[0] = (float)bone->baseRow[4] * (1.0f / 63.0f);
            bone->offset[1] = (float)bone->baseRow[5] * (1.0f / 63.0f);
            bone->offset[2] = (float)bone->baseRow[6] * (1.0f / 63.0f);
        }
    }
    cache_numskel++;

    return qtrue;
}

void TIKI_CalcLodConsts(lodControl_t *LOD)
{
    int i;

    for (i = 0; i < 4; i++) {
        float common =
            (LOD->curve[i + 1].val - LOD->curve[i + 0].val) / (LOD->curve[i + 1].pos - LOD->curve[i + 0].pos);

        LOD->consts[i].base = LOD->curve[i + 0].val
                            + (LOD->minMetric / (LOD->minMetric - LOD->maxMetric) - LOD->curve[i + 0].pos) * common;
        LOD->consts[i].scale  = common / (LOD->maxMetric - LOD->minMetric);
        LOD->consts[i].cutoff = LOD->minMetric + (LOD->maxMetric - LOD->minMetric) * LOD->curve[i + 0].pos;
    }
}

qboolean TIKI_LoadSKD(const char *path, skelcache_t *cache)
{
    int            i;
    skelHeader_t  *pheader;
    skelSurface_t *surf;
    int            version;
    unsigned int   header;
    int            length;

    length = TIKI_ReadFileEx(path, (void **)&pheader, qtrue);
    if (length < 0) {
        TIKI_DPrintf("Tiki:LoadAnim Couldn't load %s\n", path);
        return qfalse;
    }

    if (LittleLong(pheader->numSurfaces) <= 0) {
        TIKI_Error("^~^~^ TIKI_LoadSKD: %s has no surfaces\n", path);
        TIKI_FreeFile(pheader);
        return qfalse;
    }

    memset(cache, 0, sizeof(skelcache_t));
    strncpy(cache->path, path, sizeof(cache->path));

    header = pheader->ident;
    if (header != (unsigned int)TIKI_SKD_HEADER_IDENT) {
        TIKI_Error("TIKI_LoadSKD: Tried to load '%s' as a skeletal base frame (File has invalid header)\n", path);
        TIKI_FreeFile(pheader);
        return qfalse;
    }

    version = LittleLong(pheader->version);
    if (version != TIKI_SKD_HEADER_OLD_VERSION && version != TIKI_SKD_HEADER_VERSION) {
        TIKI_Error("TIKI_LoadSKD: %s has wrong version (%i should be %i)\n", path, version, TIKI_SKD_HEADER_VERSION);
        TIKI_FreeFile(pheader);
        return qfalse;
    }

    TIKI_SwapSkel(pheader);

    surf = (skelSurface_t *)((byte *)pheader + pheader->ofsSurfaces);

    if (pheader->ofsSurfaces < 0 || (size_t)pheader->ofsSurfaces > (size_t)length) {
        TIKI_Error("TIKI_LoadSKD: %s has a bad surface offset (%i)\n", path, pheader->ofsSurfaces);
        TIKI_FreeFile(pheader);
        return qfalse;
    }

    for (i = 0; i < pheader->numSurfaces; i++) {
        const size_t used = (size_t)((byte *)surf - (byte *)pheader);
        int          numVerts, numTriangles, ofsVerts, ofsTriangles, ofsCollapse, ofsEnd;

        if (used + sizeof(skelSurface_t) > (size_t)length) {
            TIKI_Error("TIKI_LoadSKD: %s surface %i runs past the end of the file\n", path, i);
            TIKI_FreeFile(pheader);
            return qfalse;
        }

        numVerts     = surf->numVerts;
        numTriangles = surf->numTriangles;
        ofsTriangles = surf->ofsTriangles;
        ofsVerts     = surf->ofsVerts;
        ofsCollapse  = surf->ofsCollapse;
        ofsEnd       = surf->ofsEnd;

        if (numVerts < 0 || numTriangles < 0 || ofsEnd < (int)sizeof(skelSurface_t)
            || used + (size_t)ofsEnd > (size_t)length || ofsVerts < (int)sizeof(skelSurface_t) || ofsVerts > ofsEnd
            || ofsTriangles < (int)sizeof(skelSurface_t) || ofsTriangles > ofsEnd
            || ofsCollapse < (int)sizeof(skelSurface_t) || ofsCollapse > ofsEnd
            || (size_t)numTriangles * 3 * sizeof(unsigned int) > (size_t)(ofsEnd - ofsTriangles)
            || (size_t)numVerts * sizeof(unsigned int) > (size_t)(ofsEnd - ofsCollapse)) {
            TIKI_Error("TIKI_LoadSKD: %s surface %i has a bad offset or count\n", path, i);
            TIKI_FreeFile(pheader);
            return qfalse;
        }

        if (numVerts > TIKI_MAX_VERTEXES) {
            TIKI_Error("TIKI_LoadSKD: %s has more than %i verts on a surface (%i)", path, TIKI_MAX_VERTEXES, numVerts);
            TIKI_FreeFile(pheader);
            return qfalse;
        }

        {
            const byte *pVert    = (const byte *)surf + ofsVerts;
            const byte *pSurfEnd = (const byte *)surf + ofsEnd;
            int         v;

            for (v = 0; v < numVerts; v++) {
                int    numWeights, numMorphs;
                size_t remaining;

                if ((size_t)(pSurfEnd - pVert) < sizeof(skeletorVertex_t)) {
                    TIKI_Error("TIKI_LoadSKD: %s surface %i vertex %i runs past its record\n", path, i, v);
                    TIKI_FreeFile(pheader);
                    return qfalse;
                }

                numWeights = *(const int *)(pVert + 20);
                numMorphs  = *(const int *)(pVert + 24);
                remaining  = (size_t)(pSurfEnd - pVert) - sizeof(skeletorVertex_t);
                if (numWeights < 0 || numMorphs < 0
                    || (size_t)numWeights * sizeof(skelWeight_t) + (size_t)numMorphs * sizeof(skeletorMorph_t)
                           > remaining) {
                    TIKI_Error("TIKI_LoadSKD: %s surface %i vertex %i has a bad weight/morph count\n", path, i, v);
                    TIKI_FreeFile(pheader);
                    return qfalse;
                }

                pVert += sizeof(skeletorVertex_t) + (size_t)numWeights * sizeof(skelWeight_t)
                       + (size_t)numMorphs * sizeof(skeletorMorph_t);
            }
        }

        surf = (skelSurface_t *)((byte *)surf + ofsEnd);
    }

    if (pheader->numBones < 0 || pheader->numBones > TIKI_MAX_BONES) {
        TIKI_FreeFile(pheader);
        TIKI_Error("TIKI_LoadSKD: %s has more than %i bones (%i)\n", path, TIKI_MAX_BONES, pheader->numBones);
        return qfalse;
    }

    TIKI_CacheFileSkel(pheader, cache, length);
    TIKI_FreeFile(pheader);
    if (!cache->skel) {
        return qfalse;
    }
    cache_numskel++;

    return true;
}

skelHeaderGame_t *TIKI_GetSkel(int index)
{
    skelHeaderGame_t *skel;

    if (index < 0 || index > cache_maxskel) {
        TIKI_Error("TIKI_GetSkel: handle %d out of range\n", index);
        return NULL;
    }

    skel = skelcache[index].skel;
    if (!skel) {
        TIKI_Error("TIKI_GetSkel: NULL pointer for %s\n", skelcache[index].path);
        return NULL;
    }

    return skel;
}

int TIKI_GetSkelCache(skelHeaderGame_t *pSkel)
{
    for (int index = 0; index < cache_maxskel; index++) {
        if (skelcache[index].skel == pSkel) {
            return index;
        }
    }

    return -1;
}

void TIKI_FreeSkel(int index)
{
    if (index < 0 || index > cache_maxskel) {
        TIKI_Error("TIKI_FreeSkel: handle %d out of range\n", index);
        return;
    }

    if (!skelcache[index].skel) {
        TIKI_Error("TIKI_FreeSkel: NULL pointer for %s\n", skelcache[index].path);
        return;
    }

    TIKI_FreeSkelCache(&skelcache[index]);
    cache_numskel--;
}

void TIKI_FreeSkelCache(skelcache_t *cache)
{
    skelSurfaceGame_t *pSurf;
    int                i;

    if (cache->morph) {
        Skel_Free(cache->morph);
        cache->morph = NULL;
    }

    if (cache->skel == NULL) {
        return;
    }

    pSurf = cache->skel->pSurfaces;
    for (i = 0; i < cache->skel->numSurfaces && pSurf != NULL; i++, pSurf = pSurf->pNext) {
        if (pSurf->pStaticXyz) {
            TIKI_Free(pSurf->pStaticXyz);
        }
    }

    TIKI_Free(cache->skel);
    cache->skel    = NULL;
    cache->size    = 0;
    cache->path[0] = 0;
    cache->numuses = 0;
}

skelcache_t *TIKI_FindSkel(const char *path)
{
    int          i;
    int          num;
    skelcache_t *cache;

    num = cache_numskel;

    for (i = 0; i < TIKI_MAX_SKELCACHE; i++) {
        cache = &skelcache[i];

        if (!cache->skel) {
            continue;
        }

        if (!strcmp(path, cache->path)) {
            return cache;
        }

        if (num == 0) {
            break;
        }
        num--;
    }

    return NULL;
}

skelcache_t *TIKI_FindSkelByHeader(skelHeaderGame_t *skelmodel)
{
    int          i;
    int          num;
    skelcache_t *cache;

    num = cache_numskel;

    for (i = 0; i < TIKI_MAX_SKELCACHE; i++) {
        cache = &skelcache[i];

        if (!cache->skel) {
            continue;
        }

        if (cache->skel == skelmodel) {
            return cache;
        }

        if (num == 0) {
            break;
        }
        num--;
    }

    return NULL;
}

skelcache_t *TIKI_FindFreeSkel(void)
{
    int          i;
    skelcache_t *cache;

    for (i = 0; i < TIKI_MAX_SKELCACHE; i++) {
        cache = &skelcache[i];

        if (!cache->skel) {
            return cache;
        }
    }

    return NULL;
}

int TIKI_RegisterSkel(const char *path, dtiki_t *tiki)
{
    char         tempName[257];
    skelcache_t *cache;
    const char  *extension;

    cache = TIKI_FindSkel(path);
    if (cache) {
        return cache - skelcache;
    }

    cache = TIKI_FindFreeSkel();
    if (!cache) {
        TIKI_Error("TIKI_RegisterSkel: No free spots open in skel cache for %s\n", path);
        return -1;
    }
    
    cache->numuses = 0;

    if (cache - skelcache + 1 > cache_maxskel) {
        cache_maxskel = cache - skelcache + 1;
    }

    extension = TIKI_FileExtension(path);
    if (!strcmp(extension, "skb")) {
        if (!TIKI_LoadSKB(path, cache)) {
            return -1;
        }
    } else if (!strcmp(extension, "skd")) {
        if (!TIKI_LoadSKD(path, cache)) {
            return -1;
        }
    } else if (!strcmp(extension, "tan")) {
        if (!TIKI_LoadTAN(path, cache)) {
            return -1;
        }
    } else {
        TIKI_Error("TIKI_RegisterSkel: Unknown extension %s\n", extension);
        return -1;
    }

    Com_sprintf(tempName, sizeof(tempName), "l%s", path);
    UI_LoadResource(tempName);

    return cache - skelcache;
}

skelMorphHeader_t *TIKI_GetMorph(int index)
{
    if (index < 0 || index > cache_maxskel) {
        TIKI_Error("TIKI_GetMorph: handle %d out of range\n", index);
        return NULL;
    }

    if (!skelcache[index].skel) {
        TIKI_Error("TIKI_GetMorph: NULL pointer for %s\n", skelcache[index].path);
        return NULL;
    }

    return skelcache[index].morph;
}

void TIKI_LoadSKBMorph(skelHeaderGame_t *pSkel, const char *morphFileName)
{
    skmMorphData_t *morphData;
    char           *pNames;
    char           *p;
    size_t          nNameBytes;
    int             i;

    if (!pSkel || !morphFileName || !morphFileName[0]) {
        return;
    }
    if (pSkel->pMorphData) {
        return;
    }

    morphData = TIKI_LoadMorphFile(morphFileName);
    if (!morphData || morphData->numTargets <= 0) {
        return;
    }

    if (morphData->numTargets > TIKI_MAX_MORPH_TARGETS) {
        TIKI_Error(
            "TIKI_LoadSKBMorph: %s declares %i morph targets (max %i), ignoring\n",
            morphFileName,
            morphData->numTargets,
            TIKI_MAX_MORPH_TARGETS
        );
        TIKI_FreeMorphData(morphData);
        return;
    }

    nNameBytes = 0;
    for (i = 0; i < morphData->numTargets; i++) {
        nNameBytes += strlen(morphData->targets[i].name) + 1;
    }

    pNames = (char *)TIKI_Alloc(nNameBytes);
    if (!pNames) {
        TIKI_Error("TIKI_LoadSKBMorph: could not allocate %i name bytes for %s\n", (int)nNameBytes, morphFileName);
        TIKI_FreeMorphData(morphData);
        return;
    }
    p      = pNames;
    for (i = 0; i < morphData->numTargets; i++) {
        size_t n = strlen(morphData->targets[i].name) + 1;
        memcpy(p, morphData->targets[i].name, n);
        p += n;
    }

    pSkel->numMorphTargets = morphData->numTargets;
    pSkel->pMorphTargets   = pNames;
    pSkel->pMorphData      = morphData;

    {
        skelcache_t       *cache = TIKI_FindSkelByHeader(pSkel);
        skelMorphHeader_t *hdr;
        size_t             hdrSize = 0;

        if (cache) {
            hdr = TIKI_BuildMorphHeader(morphData, &hdrSize);
            if (hdr) {
                cache->morph = hdr;
                cache->size += hdrSize;
            }
        }
    }
}

int TIKI_Morph_NumForName(dtiki_t *tiki, const char *name)
{
    int base = 0;
    int m;

    if (!tiki || !name || !name[0]) {
        return -1;
    }

    for (m = 0; m < tiki->numMeshes; m++) {
        skelHeaderGame_t *skel = TIKI_GetSkel(tiki->mesh[m]);
        const char       *p;
        int               i;

        if (!skel || !skel->pMorphTargets) {
            continue;
        }

        p = skel->pMorphTargets;
        for (i = 0; i < skel->numMorphTargets; i++) {
            if (!Q_stricmp(p, name)) {
                return base + i;
            }
            p += strlen(p) + 1;
        }
        base += skel->numMorphTargets;
    }

    return -1;
}


#define TIKI_MAX_BONE_MAPPINGS  128
#define TIKI_MAX_MORPH_MAPPINGS 128

typedef struct boneMapping_s {
    skelHeaderGame_t *from;
    skelHeaderGame_t *to;
    int               numuses;
    int              *map;
} boneMapping_t;

typedef struct morphMapping_s {
    skelHeaderGame_t *from;
    skelHeaderGame_t *to;
    int               numuses;
    int              *map;
} morphMapping_t;

typedef struct animBoneMapping_s {
    char             *names;    
    int               numBones;
    int               checksum;
    skelHeaderGame_t *skel;     // retail keys the cache on the SKELETON, not the anim
    int               numuses;
    int              *map;
} animBoneMapping_t;

static boneMapping_t     s_boneMappings[TIKI_MAX_BONE_MAPPINGS];
static int               s_numBoneMappings;
static int               s_maxBoneMappings;

static morphMapping_t    s_morphMappings[TIKI_MAX_MORPH_MAPPINGS];
static int               s_numMorphMappings;
static int               s_maxMorphMappings;

static animBoneMapping_t s_animBoneMappings[TIKI_MAX_BONE_MAPPINGS];
static int               s_numAnimBoneMappings;
static int               s_maxAnimBoneMappings;

static const char *TIKI_SkelBoneName(skelHeaderGame_t *skel, int index)
{
    return skel_BoneNames.FindName(skel->pBones[index].channel);
}

int TIKI_CalcBoneMapping(int *map, skelHeaderGame_t *from, skelHeaderGame_t *to)
{
    int result;
    int i;
    int j;

    result = 0;
    if (from == to) {
        return 0;
    }

    for (i = 0; i < from->numBones; i++) {
        const char *fromName = TIKI_SkelBoneName(from, i);

        map[i] = -1;
        for (j = 0; j < to->numBones; j++) {
            if (!stricmp(fromName, TIKI_SkelBoneName(to, j))) {
                map[i] = j;
                break;
            }
        }

        if (map[i] != i) {
            result = 1;
        }
    }

    return result;
}

int *TIKI_GetBoneMapping(int handle)
{
    if (handle == -1) {
        return NULL;
    }

    if (handle < 0 || handle > s_maxBoneMappings) {
        TIKI_Error("TIKI_GetBoneMapping: handle %d out of range\n", handle);
        return NULL;
    }

    if (!s_boneMappings[handle].map) {
        TIKI_Error("TIKI_GetBoneMapping: NULL pointer\n");
        return NULL;
    }

    return s_boneMappings[handle].map;
}

int TIKI_RegisterBoneMapping(int fromIndex, int toIndex)
{
    skelHeaderGame_t *from;
    skelHeaderGame_t *to;
    boneMapping_t    *map;
    int               mapBuf[TIKI_MAX_BONES];
    int               i;
    int               remaining;

    if (fromIndex < 0 || fromIndex > cache_maxskel || toIndex < 0 || toIndex > cache_maxskel) {
        return -1;
    }

    from = skelcache[fromIndex].skel;
    to   = skelcache[toIndex].skel;
    if (!from || !to) {
        return -1;
    }

    if (from->numBones > TIKI_MAX_BONES || to->numBones > TIKI_MAX_BONES) {
        TIKI_Error(
            "TIKI_RegisterBoneMapping: skeleton has more than %i bones (%i -> %i)\n",
            TIKI_MAX_BONES,
            from->numBones,
            to->numBones
        );
        return -1;
    }

    if (!TIKI_CalcBoneMapping(mapBuf, from, to)) {
        return -1;
    }

    map = NULL;
    remaining = s_numBoneMappings;
    for (i = 0; i < TIKI_MAX_BONE_MAPPINGS; i++) {
        if (!s_boneMappings[i].map) {
            continue;
        }

        if (s_boneMappings[i].from == from && s_boneMappings[i].to == to) {
            map = &s_boneMappings[i];
            break;
        }

        if (remaining-- == 0) {
            break;
        }
    }

    if (!map) {
        for (i = 0; i < TIKI_MAX_BONE_MAPPINGS; i++) {
            if (!s_boneMappings[i].map) {
                map = &s_boneMappings[i];
                break;
            }
        }

        if (!map) {
            TIKI_Error("TIKI_RegisterBoneMapping: No free spots open in bone mapping cache\n");
            return -1;
        }

        map->numuses = 0;
        i            = map - s_boneMappings;
        if (i > s_maxBoneMappings) {
            s_maxBoneMappings = i;
        }

        map->map = (int *)Z_TagMalloc(from->numBones * sizeof(int), TAG_TIKI);
        memcpy(map->map, mapBuf, from->numBones * sizeof(int));
        map->from = from;
        map->to   = to;
        s_numBoneMappings++;
    }

    map->numuses++;
    return map - s_boneMappings;
}

int TIKI_CalcMorphMapping(int *map, skelHeaderGame_t *from, skelHeaderGame_t *to)
{
    const char *fromNames;
    int         result;
    int         i;
    int         j;

    result = 0;
    if (from == to) {
        return 0;
    }

    fromNames = from->pMorphTargets;

    for (i = 0; i < from->numMorphTargets; i++) {
        const char *p = to->pMorphTargets;

        map[i] = -1;
        for (j = 0; j < to->numMorphTargets; j++) {
            if (!stricmp(fromNames, p)) {
                map[i] = j;
                break;
            }

            p += strlen(p) + 1;
        }

        if (map[i] != i) {
            result = 1;
        }

        fromNames += strlen(fromNames) + 1;
    }

    return result;
}

int *TIKI_GetMorphMapping(int handle)
{
    if (handle == -1) {
        return NULL;
    }

    if (handle < 0 || handle > s_maxMorphMappings) {
        TIKI_Error("TIKI_GetMorphMapping: handle %d out of range\n", handle);
        return NULL;
    }

    if (!s_morphMappings[handle].map) {
        TIKI_Error("TIKI_GetMorphMapping: NULL pointer\n");
        return NULL;
    }

    return s_morphMappings[handle].map;
}

int TIKI_RegisterMorphMapping(int fromIndex, int toIndex)
{
    skelHeaderGame_t *from;
    skelHeaderGame_t *to;
    morphMapping_t   *map;
    int               mapBuf[TIKI_MAX_MORPH_TARGETS];
    int               i;
    int               remaining;

    if (fromIndex < 0 || fromIndex > cache_maxskel || toIndex < 0 || toIndex > cache_maxskel) {
        return -1;
    }

    from = skelcache[fromIndex].skel;
    to   = skelcache[toIndex].skel;
    if (!from || !to || !from->pMorphTargets || !to->pMorphTargets) {
        return -1;
    }

    if (!TIKI_CalcMorphMapping(mapBuf, from, to)) {
        return -1;
    }

    map = NULL;
    remaining = s_numMorphMappings;
    for (i = 0; i < TIKI_MAX_MORPH_MAPPINGS; i++) {
        if (!s_morphMappings[i].map) {
            continue;
        }

        if (s_morphMappings[i].from == from && s_morphMappings[i].to == to) {
            map = &s_morphMappings[i];
            break;
        }

        if (remaining-- == 0) {
            break;
        }
    }

    if (!map) {
        for (i = 0; i < TIKI_MAX_MORPH_MAPPINGS; i++) {
            if (!s_morphMappings[i].map) {
                map = &s_morphMappings[i];
                break;
            }
        }

        if (!map) {
            TIKI_Error("TIKI_RegisterMorphMapping: No free spots open in morph maaping cache\n");
            return -1;
        }

        map->numuses = 0;
        i            = map - s_morphMappings;
        if (i > s_maxMorphMappings) {
            s_maxMorphMappings = i;
        }

        map->map = (int *)Z_TagMalloc(from->numMorphTargets * sizeof(int), TAG_TIKI);
        memcpy(map->map, mapBuf, from->numMorphTargets * sizeof(int));
        map->from = from;
        map->to   = to;
        s_numMorphMappings++;
    }

    map->numuses++;
    return map - s_morphMappings;
}

#define SKAN_CHANNEL_ROW_SIZE 36
#define SKAN_CHANNEL_NAME_OFS 4

int TIKI_CalcAnimBoneMapping(int *map, skelAnimHeader_t *anim, skelHeaderGame_t *skel)
{
    const char *animNames;
    int         result;
    int         i;
    int         j;

    result    = 0;
    animNames = (const char *)anim + anim->ofsChannelNames;

    for (i = 0; i < skel->numBones; i++) {
        const char *boneName = TIKI_SkelBoneName(skel, i);

        map[i] = -1;
        for (j = 0; j < anim->numbones; j++) {
            if (!stricmp(boneName, animNames + j * SKAN_CHANNEL_ROW_SIZE + SKAN_CHANNEL_NAME_OFS)) {
                map[i] = j;
                break;
            }
        }

        if (map[i] != i) {
            result = 1;
        }
    }

    return result;
}

static int TIKI_AnimBoneChecksum(skelAnimHeader_t *anim)
{
    const char *names = (const char *)anim + anim->ofsChannelNames;
    int         checksum = 0;
    int         i;
    int         j;

    for (i = 0; i < anim->numbones; i++) {
        const char *name    = names + i * SKAN_CHANNEL_ROW_SIZE + SKAN_CHANNEL_NAME_OFS;
        int         len     = (int)strlen(name);
        int         byteSum = 0;

        for (j = 0; j < len; j++) {
            byteSum += (unsigned char)name[j];
        }

        checksum += byteSum * i;
    }

    return checksum;
}

animBoneMapping_t *TIKI_FindAnimBoneMapping(skelAnimHeader_t *anim, int checksum, skelHeaderGame_t *skel)
{
    int remaining;
    int i;
    int j;

    remaining = s_numAnimBoneMappings;

    for (i = 0; i < TIKI_MAX_BONE_MAPPINGS; i++) {
        if (!s_animBoneMappings[i].names) {
            continue;
        }

        if (s_animBoneMappings[i].skel == skel
            && s_animBoneMappings[i].numBones == anim->numbones
            && s_animBoneMappings[i].checksum == checksum) {
            const char *animNames = (const char *)anim + anim->ofsChannelNames;
            qboolean    same      = qtrue;

            for (j = 0; j < anim->numbones; j++) {
                if (stricmp(
                        animNames + j * SKAN_CHANNEL_ROW_SIZE + SKAN_CHANNEL_NAME_OFS,
                        s_animBoneMappings[i].names + j * MAX_CHANNEL_NAME
                    )) {
                    same = qfalse;
                    break;
                }
            }

            if (same) {
                return &s_animBoneMappings[i];
            }
        }

        if (remaining-- == 0) {
            break;
        }
    }

    return NULL;
}

int *TIKI_GetAnimBoneMapping(int handle)
{
    if (handle == -1) {
        return NULL;
    }

    if (handle < 0 || handle > s_maxAnimBoneMappings) {
        TIKI_Error("TIKI_GetAnimBoneMapping: handle %d out of range\n", handle);
        return NULL;
    }

    if (!s_animBoneMappings[handle].map) {
        TIKI_Error("TIKI_GetAnimBoneMapping: NULL pointer\n");
        return NULL;
    }

    return s_animBoneMappings[handle].map;
}

void TIKI_FreeAnimBoneMapping(int handle)
{
    if (handle < 0 || handle > s_maxAnimBoneMappings) {
        TIKI_Error("TIKI_FreeAnimBoneMapping: handle %d out of range\n", handle);
        return;
    }

    if (!s_animBoneMappings[handle].map) {
        TIKI_Error("TIKI_FreeAnimBoneMapping: NULL pointer\n");
        return;
    }

    Z_Free(s_animBoneMappings[handle].map);
    s_animBoneMappings[handle].map = NULL;
    Z_Free(s_animBoneMappings[handle].names);
    s_animBoneMappings[handle].names = NULL;
    s_numAnimBoneMappings--;

    if (handle == s_maxAnimBoneMappings) {
        s_maxAnimBoneMappings--;
    }
}

void TIKI_ReleaseBoneMapping(int handle)
{
    if (handle < 0 || handle > s_maxBoneMappings) {
        TIKI_Error("TIKI_GetBoneMapping: handle %d out of range\n", handle);
        return;
    }

    if (!s_boneMappings[handle].map) {
        TIKI_Error("TIKI_GetBoneMapping: NULL pointer\n");
        return;
    }

    s_boneMappings[handle].numuses--;
    if (s_boneMappings[handle].numuses <= 0) {
        Z_Free(s_boneMappings[handle].map);
        s_boneMappings[handle].map = NULL;
        s_numBoneMappings--;
        if (handle == s_maxBoneMappings) {
            s_maxBoneMappings--;
        }
    }
}

void TIKI_ReleaseMorphMapping(int handle)
{
    if (handle < 0 || handle > s_maxMorphMappings) {
        TIKI_Error("TIKI_GetMorphMapping: handle %d out of range\n", handle);
        return;
    }

    if (!s_morphMappings[handle].map) {
        TIKI_Error("TIKI_GetMorphMapping: NULL pointer\n");
        return;
    }

    s_morphMappings[handle].numuses--;
    if (s_morphMappings[handle].numuses <= 0) {
        Z_Free(s_morphMappings[handle].map);
        s_morphMappings[handle].map = NULL;
        s_numMorphMappings--;
        if (handle == s_maxMorphMappings) {
            s_maxMorphMappings--;
        }
    }
}

int TIKI_RegisterAnimBoneMapping(skelAnimHeader_t *anim, int skelIndex)
{
    skelHeaderGame_t  *skel;
    animBoneMapping_t *map;
    int                mapBuf[TIKI_MAX_BONES];
    const char        *animNames;
    int                checksum;
    int                i;
    int                j;

    if (skelIndex < 0 || skelIndex > cache_maxskel) {
        return -1;
    }

    skel = skelcache[skelIndex].skel;
    if (!skel) {
        return -1;
    }

    checksum  = TIKI_AnimBoneChecksum(anim);
    animNames = (const char *)anim + anim->ofsChannelNames;

    if (skel->numBones > TIKI_MAX_BONES) {
        TIKI_Error("TIKI_RegisterAnimBoneMapping: skeleton has more than %i bones (%i)\n", TIKI_MAX_BONES, skel->numBones);
        return -1;
    }

    if (!TIKI_CalcAnimBoneMapping(mapBuf, anim, skel)) {
        return -1;
    }

    map = TIKI_FindAnimBoneMapping(anim, checksum, skel);
    if (!map) {
        for (i = 0; i < TIKI_MAX_BONE_MAPPINGS; i++) {
            if (!s_animBoneMappings[i].names) {
                map = &s_animBoneMappings[i];
                break;
            }
        }

        if (!map) {
            TIKI_Error("TIKI_RegisterAnimBoneMapping: No free spots open in anim bone mapping cache\n");
            return -1;
        }

        map->numuses = 0;
        i            = map - s_animBoneMappings;
        if (i > s_maxAnimBoneMappings) {
            s_maxAnimBoneMappings = i;
        }

        map->names = (char *)Z_TagMalloc(anim->numbones * MAX_CHANNEL_NAME, TAG_TIKI);
        for (j = 0; j < anim->numbones; j++) {
            Q_strncpyz(
                map->names + j * MAX_CHANNEL_NAME,
                animNames + j * SKAN_CHANNEL_ROW_SIZE + SKAN_CHANNEL_NAME_OFS,
                MAX_CHANNEL_NAME
            );
        }
        map->map = (int *)Z_TagMalloc(skel->numBones * sizeof(int), TAG_TIKI);
        memcpy(map->map, mapBuf, skel->numBones * sizeof(int));
        map->checksum = checksum;
        map->numBones = anim->numbones;
        map->skel     = skel;
        s_numAnimBoneMappings++;
    }

    map->numuses++;
    return map - s_animBoneMappings;
}
