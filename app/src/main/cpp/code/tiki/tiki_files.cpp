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
#include "../skeletor/tokenizer.h"
#include "../client/client.h"
#include "../corepp/tiki.h"
#include "../tiki/tiki_skan.h"
#include "../tiki/tiki_skel.h"
#include "../corepp/mem_tempalloc.h"

qboolean tiki_loading;

typedef struct {
    char                      path[100];
    skelAnimDataGameHeader_t *data;
    int                       numusers;
    int                       lookup;
} skeletorCacheEntry_t;

static int m_numInCache;

class InitSkelCache
{
public:
    static class InitSkelCache init;

    InitSkelCache();
};

static int                  m_cachedDataLookup[MAX_TIKI_ALIASES];
static skeletorCacheEntry_t m_cachedData[MAX_TIKI_ALIASES];
InitSkelCache               InitSkelCache::init;
MEM_TempAlloc               TIKI_allocator;

InitSkelCache::InitSkelCache()
{
    int i;

    for (i = 0; i < MAX_TIKI_ALIASES; i++) {
        m_cachedData[i].lookup = -1;
    }
}

void TIKI_FreeStorage(dloaddef_t *ld)
{
    TIKI_allocator.FreeAll();
    ld->tikiFile.Close();
}

void *TIKI_AllocateLoadData(size_t length)
{
    return TIKI_allocator.Alloc(length);
}

qboolean TIKI_AliasExists(dloaddef_t *ld, const char *name)
{
    int i;

    for (i = 0; i < ld->numanims; i++) {
        if (!TIKI_Anim_Compare(ld->loadanims[i]->alias, name)) {
            return true;
        }
    }

    return false;
}

char *TIKI_CopyString(const char *s)
{
    char *result = (char *)TIKI_AllocateLoadData(strlen(s) + 1);
    strcpy(result, s);
    return result;
}

qboolean loadtikicommands = true;

dtikianim_t *TIKI_LoadTikiAnim(const char *path)
{
    dloaddef_t   loaddef;
    dtikianim_t *tiki = NULL;
    const char  *token;
    float        tempVec[3];
    msg_t        modelBuf;
    char         tempName[257];
    const char  *ext;

    memset((void *)&loaddef, 0, sizeof(dloaddef_t));
    loaddef.modelBuf = &modelBuf;

    TIKI_InitSetup(&loaddef);

    loaddef.path              = path;
    loaddef.numanims          = 0;
    loaddef.numserverinitcmds = 0;
    loaddef.numclientinitcmds = 0;

    if (loaddef.tikiFile.LoadFile(path, qfalse)) {
        token = loaddef.tikiFile.GetToken(true);
        if (strcmp(token, "TIKI")) {
            TIKI_Error(
                "TIKI_LoadTIKIfile: def file %s has wrong header (%s should be TIKI)\n",
                loaddef.tikiFile.Filename(),
                token
            );
            loaddef.tikiFile.Close();
            return NULL;
        }

        while (loaddef.tikiFile.TokenAvailable(true)) {
            token = loaddef.tikiFile.GetToken(true);

            if (!token[0]) {
                continue;
            }

            if (!Q_stricmp(token, "setup")) {
                if (!TIKI_ParseSetup(&loaddef)) {
                    TIKI_FreeStorage(&loaddef);
                    return NULL;
                }
            } else if (!Q_stricmp(token, "init")) {
                TIKI_ParseInit(&loaddef);
            } else if (!Q_stricmp(token, "animations")) {
                TIKI_ParseAnimations(&loaddef);
            } else if (!Q_stricmp(token, "expressions")) {
                TIKI_ParseExpressions(&loaddef, path);
            } else {
                TIKI_Error(
                    "TIKI_LoadTIKIfile: unknown section %s in %s online %d, skipping line.\n",
                    token,
                    loaddef.tikiFile.Filename(),
                    loaddef.tikiFile.GetLineNumber()
                );

                while (loaddef.tikiFile.TokenAvailable(false)) {
                    loaddef.tikiFile.GetToken(false);
                }
            }
        }
    } else {
        loaddef.tikiFile.Close();

        ext = strstr(path, ".");
        if (!ext) {
            return NULL;
        }

        Q_strncpyz(tempName, path, ext - path + 1);
        tempName[ext - path] = 0;
        Q_strcat(tempName, sizeof(tempName), ".skd");
        WriteSkelmodel(&loaddef, tempName);

        loaddef.bSkelModelParsed = true;

        loaddef.hasSkel = true;
    }

    if (loaddef.numanims) {
        Com_sprintf(tempName, sizeof(tempName), "a%s", path);
        UI_LoadResource(tempName);

        tiki = TIKI_FillTIKIStructureSkel(&loaddef);
        if (tiki) {
            Com_sprintf(tempName, sizeof(tempName), "b%s", path);
            UI_LoadResource(tempName);
            Com_sprintf(tempName, sizeof(tempName), "c%s", path);
            UI_LoadResource(tempName);

            VectorSubtract(tiki->maxs, tiki->mins, tempVec);
            if (VectorLength(tempVec) > 100000.0f) {
                VectorSet(tiki->mins, -4.0f, -4.0f, -4.0f);
                VectorSet(tiki->maxs, 4.0f, 4.0f, 4.0f);
            }

            TIKI_FreeStorage(&loaddef);
            Com_sprintf(tempName, sizeof(tempName), "d%s", path);
            UI_LoadResource(tempName);
        } else {
            TIKI_FreeStorage(&loaddef);
        }
    } else {
        TIKI_Error("TIKI_LoadTIKIfile: No valid animations found in %s.\n", loaddef.tikiFile.Filename());
        TIKI_FreeStorage(&loaddef);
    }

    return tiki;
}

static int TIKI_ResolveBoneMappingBone(const skelHeaderGame_t *skelmodel, const char *boneName)
{
    int j;

    if (!skelmodel || !boneName) {
        return -1;
    }

    for (j = 0; j < skelmodel->numBones; j++) {
        const int   channel = skelmodel->pBones[j].channel;
        const char *name    = (channel >= 0) ? skel_BoneNames.FindName(channel) : NULL;

        if (name && !Q_stricmp(name, boneName)) {
            return j;
        }
    }

    return -1;
}

static int TIKI_ResolveBoneMappingSurface(const skelHeaderGame_t *skelmodel, const char *surfaceName)
{
    skelSurfaceGame_t *surf;
    int                j;

    if (!skelmodel || !surfaceName) {
        return -1;
    }

    surf = skelmodel->pSurfaces;
    for (j = 0; j < skelmodel->numSurfaces; j++, surf = surf->pNext) {
        if (!Q_stricmp(surf->name, surfaceName)) {
            return j;
        }
    }

    return -1;
}

static int TIKI_ResolveBoneParent(const skelHeaderGame_t *skelmodel, int boneIndex)
{
    if (!skelmodel || boneIndex < 0 || boneIndex >= skelmodel->numBones) {
        return -1;
    }

    if (skelmodel->pBones[boneIndex].skelParent < 0) {
        return -1;
    }

    return skelmodel->pBones[boneIndex].skelParent;
}

skelHeaderGame_t *TIKI_GetBaseSkel(dtiki_t *tiki)
{
    if (!tiki) {
        return NULL;
    }

    if (tiki->skelIndex >= 0) {
        return TIKI_GetSkel(tiki->skelIndex);
    }

    if (tiki->numMeshes > 0) {
        return TIKI_GetSkel(tiki->mesh[0]);
    }

    return NULL;
}

dtiki_t *TIKI_LoadTikiModel(dtikianim_t *tikianim, const char *name, size_t *defSizeOut)
{
    dtiki_t *tiki;
    int    i, j, k;
    char  *strptr;
    size_t defsize;

    struct {
        dtiki_t   tiki;
        short int buffer[32];
    } temp;
    dtiki_t* temp_tiki;

    dloadsurface_t     loadsurfaces[MAX_MODEL_SURFACES];
    int                numSurfacesSetUp;
    int                numReplacedSurfaceRows;
    dtikisurface_t    *tikiSurf;
    dloadsurface_t    *loadsurf;
    int                mesh;
    skelHeaderGame_t  *skelmodel;
    skelSurfaceGame_t *surf;
    int                surfOffset;
    qboolean           found;
    byte              *start_ptr, *max_ptr, *ptr;

    temp_tiki = &temp.tiki;

    TIKI_LoadSetup(
        temp_tiki,
        tikianim->name,
        loadsurfaces,
        &numSurfacesSetUp,
        tikianim->modelData,
        tikianim->modelDataSize
    );
    if (!temp_tiki->numMeshes) {
        return NULL;
    }

    if (tikianim->tanMesh && temp_tiki->numMeshes > 0) {
        const int         baseRow  = temp_tiki->mesh[0];
        const char       *basePath = skelcache[baseRow].path;
        const char       *baseDot  = strrchr(basePath, '.');
        skelHeaderGame_t *baseSkel = TIKI_GetSkel(baseRow);

        if (baseSkel && baseDot && !Q_stricmp(baseDot, ".tan")) {
            for (i = 0; i < tikianim->num_anims; i++) {
                const int         row = tikianim->tanMesh[i];
                skelHeaderGame_t *sm;

                sm = (row >= 0) ? TIKI_GetSkel(row) : NULL;
                if (!sm) {
                    TIKI_Error(
                        "TIKI_FillTIKIStructure: tried to load non-TAN animation '%s' for model '%s'.\n",
                        (row >= 0) ? skelcache[row].path : "<non-.tan animation>",
                        name
                    );
                    return NULL;
                }
                if (sm->numSurfaces != baseSkel->numSurfaces) {
                    TIKI_Error(
                        "TIKI_FillTIKIStructure: tikianim %s has different number of surfaces in %s, skipping.\n",
                        skelcache[row].path,
                        name
                    );
                    return NULL;
                }
                if (sm->numBones != baseSkel->numBones) {
                    TIKI_Error(
                        "TIKI_FillTIKIStructure: tikianim %s has different number of tags in %s, skipping.\n",
                        skelcache[row].path,
                        name
                    );
                    return NULL;
                }
            }
        }
    }

    defsize = sizeof(dtiki_t) - sizeof(tiki->mesh);
    defsize += temp_tiki->numMeshes * sizeof(short);
    defsize = PAD(defsize, sizeof(void *));
    defsize += temp_tiki->num_surfaces * sizeof(dtikisurface_t);
    defsize = PAD(defsize, sizeof(void *));
    defsize += tikianim->num_bone_mappings * sizeof(tikiBoneMapping_t);
    defsize = PAD(defsize, sizeof(void *));
    numReplacedSurfaceRows = (numSurfacesSetUp > temp_tiki->num_surfaces) ? numSurfacesSetUp
                                                                          : temp_tiki->num_surfaces;
    defsize += numReplacedSurfaceRows * sizeof(dtikiReplacedSurface_t);
    if (tikianim->num_expressions) {
        int exprIndex;

        defsize = PAD(defsize, sizeof(void *));

        for (exprIndex = 0; exprIndex < tikianim->num_expressions; exprIndex++) {
            defsize += sizeof(dtikiexpression_t);
        }

        for (exprIndex = 0; exprIndex < tikianim->num_expressions; exprIndex++) {
            defsize += tikianim->loadexpressions[exprIndex].num_morph_targets * sizeof(dtikimorphtarget_t);
        }
    }

    tiki = (dtiki_t *)TIKI_Alloc(defsize);
    memset(tiki, 0, defsize);

    start_ptr = (byte *)tiki;
    max_ptr   = start_ptr + defsize;
    ptr       = start_ptr + sizeof(dtiki_t) - sizeof(tiki->mesh) + temp_tiki->numMeshes * sizeof(tiki->mesh[0]);

    tiki->a = tikianim;
    tiki->m_boneList.InitChannels();
    tiki->skeletor     = NULL;
    tiki->load_scale   = temp_tiki->load_scale;
    tiki->lod_scale    = temp_tiki->lod_scale;
    tiki->lod_bias     = temp_tiki->lod_bias;
    tiki->lod_start_distance     = temp_tiki->lod_start_distance;
    tiki->lod_start_percent_verts = temp_tiki->lod_start_percent_verts;
    tiki->lod_stop_distance      = temp_tiki->lod_stop_distance;
    tiki->lod_stop_percent_verts = temp_tiki->lod_stop_percent_verts;
    tiki->num_surfaces = temp_tiki->num_surfaces;
    tiki->numMeshes    = temp_tiki->numMeshes;
    tiki->radius       = temp_tiki->radius;
    Q_strncpyz(tiki->name, name, sizeof(tiki->name));
    // Retail TIKI_InitTiki @0x000F6F50 sets skelIndex to -1 here; it is NOT the
    tiki->skelIndex = -1;
    ptr      = (byte *)PADP(ptr, sizeof(void *));
    tikiSurf = (dtikisurface_t *)ptr;
    tiki->m_boneList.ZeroChannels();

    ptr = (byte *)PADP((byte *)(tikiSurf + temp_tiki->num_surfaces), sizeof(void *));
    if (tikianim->num_bone_mappings) {
        tiki->bone_mappings = (tikiBoneMapping_t *)ptr;
        ptr += tikianim->num_bone_mappings * sizeof(tikiBoneMapping_t);
    }

    ptr                       = (byte *)PADP(ptr, sizeof(void *));
    tiki->replacedSurfaces    = (dtikiReplacedSurface_t *)ptr;
    tiki->numReplacedSurfaces = numReplacedSurfaceRows;
    memset(tiki->replacedSurfaces, -1, numReplacedSurfaceRows * sizeof(dtikiReplacedSurface_t));
    ptr += numReplacedSurfaceRows * sizeof(dtikiReplacedSurface_t);

    tiki->num_expressions = tikianim->num_expressions;
    if (tikianim->num_expressions) {
        dtikiexpression_t *exprRows;

        ptr = (byte *)PADP(ptr, sizeof(void *));
        tiki->ofs_expressions = (int)(ptr - start_ptr);
        exprRows              = (dtikiexpression_t *)ptr;
        ptr += tikianim->num_expressions * sizeof(dtikiexpression_t);

        for (i = 0; i < tikianim->num_expressions; i++) {
            const dloadexpression_t *src = &tikianim->loadexpressions[i];

            strcpy(exprRows[i].alias, src->alias);
            exprRows[i].num_morph_targets = src->num_morph_targets;
            exprRows[i].ofs_morph_targets = (int)(ptr - start_ptr);

            ptr += src->num_morph_targets * sizeof(dtikimorphtarget_t);
        }
    }

    assert(ptr <= max_ptr);
    assert((byte *)(tikiSurf + temp_tiki->num_surfaces) <= max_ptr);
    (void)max_ptr;

    for (i = 0; i < temp_tiki->numMeshes; i++) {
        mesh          = temp_tiki->mesh[i];
        tiki->mesh[i] = mesh;

        skelmodel = skelcache[mesh].skel;
        skelcache[mesh].numuses++;

        if (i == 0) {
            const char *skelExt = strrchr(skelcache[mesh].path, '.');

            if (!skelExt || Q_stricmp(skelExt, ".tan")) {
                tiki->skelIndex = mesh;
                Q_strncpyz(tiki->skelName, skelcache[mesh].path, sizeof(tiki->skelName));
            }

            if (tiki->a && tiki->a->num_anims > 0) {
                tiki->boneMappings = (int *)TIKI_Alloc(tiki->a->num_anims * sizeof(int));
                memset(tiki->boneMappings, -1, tiki->a->num_anims * sizeof(int));

                for (j = 0; j < tiki->a->num_anims; j++) {
                    skelAnimHeader_t *hdr;

                    hdr = TIKI_Skan_FindByAnim(tiki, j);
                    if (hdr) {
                        tiki->boneMappings[j] = TIKI_RegisterAnimBoneMapping(hdr, tiki->skelIndex);
                    }
                }
            }
        }

        for (j = 0; j < skelmodel->numBones; j++) {
            tiki->m_boneList.AddChannel(skelmodel->pBones[j].channel);
        }

        if (i == 0) {
            const char *baseExt = strrchr(skelcache[mesh].path, '.');

            if (baseExt && !Q_stricmp(baseExt, ".tan")) {
                tiki->num_tags = skelmodel->numBones - 1;
            } else {
                tiki->num_tags = skelmodel->numBones;
            }
        }
    }

    tiki->m_boneList.PackChannels();

    // morph-target name, so it runs here -- retail's ordering in
    TIKI_CopyExpressions(tiki, tikianim);

    {
        int rowIndex;

        for (rowIndex = 0; rowIndex < numSurfacesSetUp; rowIndex++) {
            dloadsurface_t        *rls;
            dtikiReplacedSurface_t *row;
            int                     baseSurfIndex, replSurfIndex, replSkelIdx;
            skelHeaderGame_t       *replSkelModel;

            rls = &loadsurfaces[rowIndex];
            if (!rls->replSkel[0]) {
                continue;
            }

            replSkelIdx = TIKI_RegisterSkel(rls->replSkel, tiki);
            if (replSkelIdx < 0) {
                continue;
            }

            replSkelModel = TIKI_GetSkel(replSkelIdx);
            if (!replSkelModel) {
                continue;
            }

            baseSurfIndex = -1;
            {
                skelHeaderGame_t  *baseSkel = TIKI_GetSkel(tiki->skelIndex);
                skelSurfaceGame_t *bs;
                int                si;

                if (baseSkel) {
                    for (bs = baseSkel->pSurfaces, si = 0; si < baseSkel->numSurfaces; si++, bs = bs->pNext) {
                        if (!Q_stricmp(bs->name, rls->name)) {
                            baseSurfIndex = si;
                            break;
                        }
                    }
                }
            }

            replSurfIndex = -1;
            {
                skelSurfaceGame_t *rs;
                int                si;

                for (rs = replSkelModel->pSurfaces, si = 0; si < replSkelModel->numSurfaces;
                     si++, rs = rs->pNext) {
                    if (!Q_stricmp(rs->name, rls->replSurf)) {
                        replSurfIndex = si;
                        break;
                    }
                }
            }

            if (baseSurfIndex < 0 || replSurfIndex < 0 || baseSurfIndex >= numReplacedSurfaceRows) {
                continue;
            }

            row = &tiki->replacedSurfaces[baseSurfIndex];
            if (row->skelIndex != -1) {
                continue;
            }

            row->skelIndex    = replSkelIdx;
            row->surfaceIndex = replSurfIndex;
            Q_strncpyz(row->skelName, rls->replSkel, sizeof(row->skelName));
            Q_strncpyz(row->morphName, rls->replMorph, sizeof(row->morphName));

            skelcache[replSkelIdx].numuses++;

            row->boneMappingIndex = TIKI_RegisterBoneMapping(replSkelIdx, tiki->skelIndex);

            if (rls->replMorph[0]) {
                TIKI_LoadSKBMorph(replSkelModel, rls->replMorph);
                row->morphMappingIndex = TIKI_RegisterMorphMapping(tiki->skelIndex, replSkelIdx);
            } else {
                row->morphMappingIndex = -1;
            }
        }
    }

    VectorCopy(temp_tiki->light_offset, tiki->light_offset);
    VectorCopy(temp_tiki->load_origin, tiki->load_origin);
    tiki->fade_dist_mod = temp_tiki->fade_dist_mod;
    VectorCopy(temp_tiki->boundingVolumeMin, tiki->boundingVolumeMin);
    VectorCopy(temp_tiki->boundingVolumeMax, tiki->boundingVolumeMax);
    VectorCopy(temp_tiki->head_offset, tiki->head_offset);
    tiki->_loadAllAnims = temp_tiki->_loadAllAnims;
    tiki->num_bone_mappings = tikianim->num_bone_mappings;
    tiki->num_anims         = tikianim->num_anims;
    tiki->surfaces          = tikiSurf;

    for (i = 0; i < numSurfacesSetUp; i++) {
        loadsurf = &loadsurfaces[i];
        found    = false;
        strptr   = strchr(loadsurf->name, '*');

        surfOffset = 0;

        if (strptr || !Q_stricmp(loadsurf->name, "all")) {
            for (j = 0; j < temp_tiki->numMeshes; j++) {
                mesh      = temp_tiki->mesh[j];
                skelmodel = TIKI_GetSkel(mesh);
                surf      = skelmodel->pSurfaces;

                for (k = 0; k < skelmodel->numSurfaces; k++) {
                    tikiSurf = &tiki->surfaces[surfOffset + k];

                    if ((strptr && strptr != loadsurf->name
                         && !strnicmp(loadsurf->name, surf->name, strptr - loadsurf->name))
                        || !Q_stricmp(loadsurf->name, "all")) {
                        TIKI_SetupIndividualSurface(tikianim->name, tikiSurf, surf->name, loadsurf);
                        found = true;
                    }

                    surf = surf->pNext;
                }

                surfOffset += skelmodel->numSurfaces;
            }
        } else {
            for (j = 0; j < temp_tiki->numMeshes; j++) {
                mesh      = temp_tiki->mesh[j];
                skelmodel = TIKI_GetSkel(mesh);
                surf      = skelmodel->pSurfaces;

                tikiSurf = &tiki->surfaces[surfOffset];

                for (k = 0; k < skelmodel->numSurfaces; k++) {
                    if (!Q_stricmp(loadsurf->name, surf->name)) {
                        TIKI_SetupIndividualSurface(tikianim->name, tikiSurf, surf->name, loadsurf);
                        if (!tikiSurf->name[0]) {
                            TIKI_Warning(
                                "TIKI_InitTiki: Surface %i in %s(referenced in %s) has no name!  Please investigate "
                                "and fix\n",
                                k,
                                skelmodel->name,
                                name
                            );
                        }
                        found = true;
                    }

                    surf = surf->pNext;
                    tikiSurf++;
                }

                surfOffset += skelmodel->numSurfaces;
            }
        }

        if (!found) {
            TIKI_Error(
                "TIKI_CopySurfacesSkel: could not find surface %s in %s.\n",
                loadsurf->name,
                tikianim->name
            );
        }
    }

    surfOffset = 0;
    for (i = 0; i < temp_tiki->numMeshes; i++, surfOffset += skelmodel->numSurfaces) {
        skelmodel = TIKI_GetSkel(temp_tiki->mesh[i]);

        surf = skelmodel->pSurfaces;

        for (j = 0; j < skelmodel->numSurfaces; j++, surf = surf->pNext) {
            tikiSurf = &tiki->surfaces[surfOffset + j];

            if (tikiSurf->numskins) {
                continue;
            }

            Q_strncpyz(tikiSurf->name, surf->name, sizeof(tikiSurf->name));

            qboolean bMaterialPlaceholder = qfalse;
            if (!Q_stricmpn(surf->name, "material", 8) && surf->name[8]) {
                const char *pDigit = surf->name + 8;
                bMaterialPlaceholder = qtrue;
                while (*pDigit) {
                    if (*pDigit < '0' || *pDigit > '9') {
                        bMaterialPlaceholder = qfalse;
                        break;
                    }
                    pDigit++;
                }
            }

            if (!bMaterialPlaceholder) {
                static const char *fxGizmoSurfaces[] = { "xyz", "green", "blue", "red" };
                size_t iGizmo;

                for (iGizmo = 0; iGizmo < ARRAY_LEN(fxGizmoSurfaces); iGizmo++) {
                    if (!Q_stricmp(surf->name, fxGizmoSurfaces[iGizmo])) {
                        bMaterialPlaceholder = qtrue;
                        break;
                    }
                }
            }

            if (!bMaterialPlaceholder) {
                Q_strncpyz(tikiSurf->shader[0], surf->name, sizeof(tikiSurf->shader[0]));
            }

            tikiSurf->numskins = 1;
        }
    }

    // reproduced exactly.  What is not reproduced is retail's second, misleading error
    if (tikianim->num_bone_mappings && tiki->bone_mappings) {
        const skelHeaderGame_t *baseSkel = TIKI_GetBaseSkel(tiki);

        for (i = 0; i < tikianim->num_bone_mappings; i++) {
            const dloadbonemapping_t *loadmapping = &tikianim->bone_mappings[i];
            tikiBoneMapping_t        *mapping     = &tiki->bone_mappings[i];

            mapping->boneIndex    = TIKI_ResolveBoneMappingBone(baseSkel, loadmapping->bone);
            mapping->surfaceIndex = TIKI_ResolveBoneMappingSurface(baseSkel, loadmapping->surface);
            mapping->radius       = loadmapping->radius;
            mapping->length       = loadmapping->length;

            if (mapping->boneIndex < 0) {
                TIKI_Warning(
                    "TIKI_LoadTikiModel: bonemapping bone '%s' not found in '%s'.\n", loadmapping->bone, name
                );
            } else if (mapping->surfaceIndex < 0) {
                TIKI_Warning(
                    "TIKI_LoadTikiModel: bonemapping surface '%s' not found in '%s'.\n", loadmapping->surface, name
                );
            }

            if (mapping->boneIndex < 0 || mapping->surfaceIndex < 0) {
                mapping->boneIndex    = -1;
                mapping->surfaceIndex = -1;
            }

            mapping->parentIndex = TIKI_ResolveBoneParent(baseSkel, mapping->boneIndex);
        }

        tiki->num_bone_mappings = tikianim->num_bone_mappings;
    }

    if (!tiki->radius) {
        TIKI_CalcRadius(tiki);
    }

    if (defSizeOut) {
        *defSizeOut = defsize;
    }

    return tiki;
}

void TIKI_CopyExpressions(dtiki_t *tiki, const dtikianim_t *tikianim)
{
    skelMorphHeader_t      *morph;
    const char             *names;
    dtikiexpression_t      *row;
    dtikimorphtarget_t     *data;
    const dloadexpression_t *loadexpr;
    union {
        float f;
        int   i;
    } nan_index;
    int exprIndex;
    int targetIndex;

    morph = TIKI_GetMorph(tiki->skelIndex);

    names = morph ? (const char *)morph + morph->ofsTargets : NULL;

    nan_index.f = NAN;

    for (exprIndex = 0; exprIndex < tiki->num_expressions; exprIndex++) {
        row     = (dtikiexpression_t *)((byte *)tiki + tiki->ofs_expressions) + exprIndex;
        data    = (dtikimorphtarget_t *)((byte *)tiki + row->ofs_morph_targets);
        loadexpr = &tikianim->loadexpressions[exprIndex];

        for (targetIndex = 0; targetIndex < row->num_morph_targets; targetIndex++) {
            const dloadmorphtarget_t *loadtarget = loadexpr->targets[targetIndex];
            int                       i;

            data[targetIndex].morph_index = nan_index.i;

            if (morph) {
                for (i = 0; i < morph->numTargets; i++) {
                    if (!Q_stricmp(names + 72 * i, loadtarget->name)) {
                        data[targetIndex].morph_index = i;
                        break;
                    }
                }
            }

            data[targetIndex].percent = loadtarget->percent;
        }
    }
}

dtikimorphtarget_t *TIKI_GetExpression(dtiki_t *tiki, const char *expression_name, int *number_of_morph_targets)
{
    dtikiexpression_t *rows;
    int                i;

    if (!tiki) {
        *number_of_morph_targets = 0;
        return NULL;
    }

    rows = (dtikiexpression_t *)((byte *)tiki + tiki->ofs_expressions);

    for (i = 0; i < tiki->num_expressions; i++) {
        if (!stricmp(rows[i].alias, expression_name)) {
            *number_of_morph_targets = rows[i].num_morph_targets;
            return (dtikimorphtarget_t *)((byte *)tiki + rows[i].ofs_morph_targets);
        }
    }

    *number_of_morph_targets = 0;
    return NULL;
}

void TIKI_CalcRadius(dtiki_t *tiki)
{
    int    j;
    float  radius;
    float  tmpVec[3];
    float *bounds[2];

    tiki->radius = 0.0f;

    bounds[0] = &tiki->a->mins[0];
    bounds[1] = &tiki->a->maxs[0];

    // 0x000FB1F0:293-307 -- all eight box corners, then radius *= load_scale
    // (dtiki +188); retail has no 0.7 and no lod_scale here.
    for (j = 0; j < 8; j++) {
        tmpVec[0] = bounds[j & 1][0];
        tmpVec[1] = bounds[(j >> 1) & 1][1];
        tmpVec[2] = bounds[(j >> 2) & 1][2];

        radius = VectorLength(tmpVec);
        if (radius > tiki->radius) {
            tiki->radius = radius;
        }
    }

    tiki->radius *= tiki->load_scale;
}

skelAnimDataGameHeader_t *SkeletorCacheFileCallback(const char *path)
{
    skelAnimDataFileHeader_t *pHeader;
    int                       iBuffLength;
    char                      tempName[100];
    char                      extension[100];
    skelAnimDataGameHeader_t *finishedHeader;
    char                     *buffer;
    char                      npath[256];
    int                       t0;
    int                       r0;

    Skel_ExtractFileExtension(path, extension);

    if (strcmp(extension, "skc") && strcmp(extension, "ska") && strcmp(extension, "tan")) {
        Com_DPrintf("Skeletor CacheAnimSkel: %s: File extension unknown.  Attempting to open as skc file\n", path);
    }

    t0 = TIKI_SkelRollStart();

    Q_strncpyz(npath, "newanim/", sizeof(npath));
    Q_strcat(npath, sizeof(npath), path);

    r0 = Com_Milliseconds();
    iBuffLength = TIKI_ReadFileEx(npath, (void **)&buffer, qtrue);
    TIKI_SkelRollRead(iBuffLength > 0 ? iBuffLength : 0, Com_Milliseconds() - r0, npath);
    if (iBuffLength > 0) {
        finishedHeader = skeletor_c::LoadProcessedAnim(npath, buffer, iBuffLength, path);
        TIKI_FreeFile(buffer);
    } else {
        r0 = Com_Milliseconds();
        iBuffLength = TIKI_ReadFileEx(path, (void **)&pHeader, qtrue);
        TIKI_SkelRollRead(iBuffLength > 0 ? iBuffLength : 0, Com_Milliseconds() - r0, path);
        if (iBuffLength <= 0) {
            Com_DPrintf("Skeletor CacheAnimSkel: Could not open binary file %s\n", path);
            return NULL;
        }

        int ident   = LittleLong(pHeader->ident);
        int version = LittleLong(pHeader->version);
        if (ident == (('T') | ('A' << 8) | ('N' << 16) | (' ' << 24))) {
            finishedHeader = skeletor_c::BuildTANAnim(pHeader, iBuffLength, path);
        } else if (ident == TIKI_SKC_HEADER_IDENT && version == TIKI_SKC_HEADER_EF2_VERSION) {
            finishedHeader = skeletor_c::ConvertEF2SkanToGame(pHeader, iBuffLength, path);
        } else if (ident != TIKI_SKC_HEADER_IDENT
            || (version != TIKI_SKC_HEADER_OLD_VERSION && version != TIKI_SKC_HEADER_VERSION)) {
            Com_DPrintf(
                "Skeletor CacheAnimSkel: anim %s has wrong header ([ident,version] = [%i,%i] should be [%i,%i])\n",
                path,
                ident,
                version,
                TIKI_SKC_HEADER_IDENT,
                TIKI_SKC_HEADER_VERSION
            );
            TIKI_FreeFile(pHeader);
            return NULL;
        } else if (version == TIKI_SKC_HEADER_OLD_VERSION) {
            Com_DPrintf("WARNING- DOWNGRADING TO OLD ANIMATION FORMAT FOR FILE: %s\n", path);

            pHeader->flags = LittleLong(pHeader->flags);
            pHeader->nBytesUsed = LittleLong(pHeader->nBytesUsed);
            pHeader->frameTime = LittleFloat(pHeader->frameTime);
            pHeader->totalDelta.x = LittleFloat(pHeader->totalDelta.x);
            pHeader->totalDelta.y = LittleFloat(pHeader->totalDelta.y);
            pHeader->totalDelta.z = LittleFloat(pHeader->totalDelta.z);
            pHeader->totalAngleDelta = LittleFloat(pHeader->totalAngleDelta);
            pHeader->numChannels = LittleLong(pHeader->numChannels);
            pHeader->ofsChannelNames = LittleLong(pHeader->ofsChannelNames);
            pHeader->numFrames = LittleLong(pHeader->numFrames);

            finishedHeader = skeletor_c::ConvertSkelFileToGame(pHeader, iBuffLength, path);
        } else {

            buffer = (char *)pHeader + sizeof(int) + sizeof(int);
            iBuffLength -= sizeof(int) + sizeof(int);

            finishedHeader = skeletor_c::LoadProcessedAnimEx(path, buffer, iBuffLength, path);
        }

        TIKI_FreeFile(pHeader);
    }

    TIKI_SkelRollEnd(t0);

    Com_sprintf(tempName, sizeof(tempName), "g%s", path);
    UI_LoadResource(tempName);

    return finishedHeader;
}

skelAnimDataGameHeader_t *SkeletorCacheGetData(int index)
{
    if (index < 0) {
        return NULL;
    }

    skelAnimDataGameHeader_t *data = m_cachedData[index].data;
    if (!data) {
        data                     = SkeletorCacheFileCallback(m_cachedData[index].path);
        m_cachedData[index].data = data;
    }

    return data;
}

int SkeletorCacheNumEntries(void)
{
    return m_numInCache;
}

const char *SkeletorCacheEntryPath(int index)
{
    if (index < 0 || index >= m_numInCache) {
        return "";
    }

    return m_cachedData[index].path;
}

qboolean SkeletorCacheEntryLoaded(int index)
{
    if (index < 0 || index >= m_numInCache) {
        return qfalse;
    }

    return m_cachedData[index].data != NULL;
}

int SkeletorCacheEntryUsers(int index)
{
    if (index < 0 || index >= m_numInCache) {
        return 0;
    }

    return m_cachedData[index].numusers;
}

int SkeletorCacheEntrySize(int index)
{
    if (index < 0 || index >= m_numInCache || !m_cachedData[index].data) {
        return 0;
    }

    return (int)m_cachedData[index].data->nBytesUsed;
}

bool SkeletorCacheFindFilename(const char *path, int *indexPtr)
{
    int sortValue;
    int lowerBound;
    int upperBound;
    int index;

    lowerBound = 0;
    upperBound = m_numInCache - 1;
    while (lowerBound <= upperBound) {
        index     = (lowerBound + upperBound) / 2;
        sortValue = Q_stricmp(path, m_cachedData[m_cachedDataLookup[index]].path);
        if (!sortValue) {
            if (indexPtr) {
                *indexPtr = index;
            }
            return true;
        }
        if (sortValue < 0) {
            upperBound = index - 1;
        } else {
            lowerBound = index + 1;
        }
    }

    if (indexPtr) {
        *indexPtr = lowerBound;
    }
    return false;
}

bool SkeletorCacheLoadData(const char *path, bool precache, int newIndex)
{
    int                       i;
    skelAnimDataGameHeader_t *data;
    int                       lookup;

    if (strlen(path) >= 100) {
        Com_Printf("^~^~^ SkeletorCache: File name over %i characters will be ignored.\n(%s)\n", 99, path);
        return false;
    }

    if (precache) {
        data = SkeletorCacheFileCallback(path);
        if (!data) {
            return false;
        }
    } else {
        data = 0;
    }

    for (lookup = 0; lookup < MAX_TIKI_ALIASES; lookup++) {
        if (m_cachedData[lookup].lookup == -1) {
            break;
        }
    }

    for (i = m_numInCache - 1; i >= newIndex; i--) {
        m_cachedData[m_cachedDataLookup[i]].lookup = i + 1;
        m_cachedDataLookup[i + 1]                  = m_cachedDataLookup[i];
    }

    m_cachedDataLookup[newIndex] = lookup;
    m_cachedData[lookup].lookup  = newIndex;
    m_cachedData[lookup].data    = data;
    Q_strncpyz(m_cachedData[lookup].path, path, sizeof(m_cachedData[lookup].path));
    m_cachedData[lookup].numusers = 0;
    m_numInCache++;

    return true;
}

void SkeletorCacheUnloadData(int index)
{
    int i;

    m_numInCache--;

    if (m_cachedData[m_cachedDataLookup[index]].data) {
        skelAnimDataGameHeader_s::DeallocAnimData(m_cachedData[m_cachedDataLookup[index]].data);
        m_cachedData[m_cachedDataLookup[index]].data = NULL;
    }

    m_cachedData[m_cachedDataLookup[index]].lookup = -1;

    for (i = index; i < m_numInCache; i++) {
        m_cachedDataLookup[i] = m_cachedDataLookup[i + 1];
        m_cachedData[m_cachedDataLookup[i]].lookup = i;
    }
}

void SkeletorCacheCleanCache()
{
    int i;

    for (i = m_numInCache; i > 0; i--) {
        if (!m_cachedData[m_cachedDataLookup[i - 1]].numusers) {
            SkeletorCacheUnloadData(i - 1);
        }
    }
}

int TIKI_RegisterAnim(const char *path, qboolean use)
{
    int index;

    if (SkeletorCacheFindFilename(path, &index)) {
        if (use && !m_cachedData[m_cachedDataLookup[index]].data) {
            // retail: entry known but data not resident -> page it in now.
            SkeletorCacheGetData(m_cachedDataLookup[index]);
        }
    } else {
        if (!use && !FS_Exists(path)) {
            TIKI_Error("TIKI_RegisterAnim: %s doesn't exist\n", path);
            return -1;
        }

        if (m_numInCache >= MAX_TIKI_ALIASES) {
            TIKI_Error("TIKI_RegisterAnim: No free spots open in anim cache for %s\n", path);
            return -1;
        }

        if (!SkeletorCacheLoadData(path, use, index)) {
            return -1;
        }
    }

    if (use) {
        m_cachedData[m_cachedDataLookup[index]].numusers++;
    }

    return m_cachedDataLookup[index];
}

void TIKI_CacheAnim(const char *path)
{
    TIKI_RegisterAnim(path, qtrue);
}

/* Retail TIKI_RegisterTiki @0x000FC900:76-83 re-registers a warm alias whose
   entry has no data or no users -- numusers is what carries the entry through
   SkeletorCacheCleanCache, so skipping it hands the level's own animations to
   the next unload.  Only a RESIDENT entry is re-registered: a resident slot is
   still owned by its own path, a dropped slot may name another path, and an
   unloaded alias keeps its paging-by-index shape. */
int TIKI_Anim_RetainAlias(int alias)
{
    if (alias < 0 || alias >= MAX_TIKI_ALIASES) {
        return alias;
    }

    if (SkeletorCacheEntryLoaded(alias)) {
        if (!SkeletorCacheEntryUsers(alias)) {
            return TIKI_RegisterAnim(SkeletorCacheEntryPath(alias), qtrue);
        }

        return alias;
    }

    SkeletorCacheGetData(alias);
    return alias;
}

void TikiAddToBounds(dtikianim_t *tiki, SkelVec3 *newBounds)
{
    int i;

    for (i = 0; i < 3; i++) {
        if (newBounds[0].val[i] < tiki->mins[i]) {
            tiki->mins[i] = newBounds[0].val[i];
        }

        if (newBounds[1].val[i] > tiki->maxs[i]) {
            tiki->maxs[i] = newBounds[1].val[i];
        }
    }
}

void TIKI_AnimList_f()
{
    skeletorCacheEntry_t *entry;
    int                   i;

    Com_Printf("\nanimlist:\n");
    for (i = 0; i < m_numInCache; i++) {
        entry = &m_cachedData[m_cachedDataLookup[i]];
        if (!entry) {
            Com_Printf("*** NOT CACHED: ");
        }

        if (m_cachedData[i].path[0]) {
            Com_Printf("%s\n", m_cachedData[i].path);
        } else {
            Com_Printf("*** EMPTY PATH ERROR\n");
        }
    }

    for (; i < MAX_TIKI_ALIASES; i++) {
        if (m_cachedData[m_cachedDataLookup[i]].path[0]) {
            Com_Printf("*** CORRUPTED ENTRY\n");
        }
    }
}


dtikianim_t *TIKI_InitTiki(dloaddef_t *ld, size_t defsize)
{
    byte       *ptr;
    byte       *start_ptr;
    byte       *max_ptr;
    dtikicmd_t *pcmds;
    int         i, k;
    int         numLoadedAnims;
    size_t      j;
    size_t      size;
    int                       alias_index;
    dtikianim_t              *panim;
    dtikianimdef_t           *panimdef;
    dloadanim_t              *anim;
    skelAnimDataGameHeader_t *data;
    qboolean                  bModelBoundsSet = false;
    bool                      bPrecache;
    int                       index;
    char                      tempName[257];
    int                       order[MAX_TIKI_ALIASES];
    short                     temp_aliases[MAX_TIKI_ALIASES];
    short                     temp_tanmesh[MAX_TIKI_ALIASES];

    panim = (dtikianim_t *)TIKI_Alloc(defsize);

    start_ptr = (byte *)panim;
    max_ptr   = start_ptr + defsize;
    ptr       = start_ptr;

    memset(panim, 0, defsize);
    ClearBounds(panim->mins, panim->maxs);
    panim->num_client_initcmds = ld->numclientinitcmds;
    panim->num_server_initcmds = ld->numserverinitcmds;

    ptr += sizeof(dtikianim_t) - sizeof(dtikianimdef_t *) + sizeof(dtikianimdef_t *) * ld->numanims;
    panim->name = (char *)ptr;
    strcpy(panim->name, ld->path);

    ptr += strlen(ld->path) + 1;
    ptr                    = (byte *)PADP(ptr, sizeof(void *));
    panim->server_initcmds = (dtikicmd_t *)ptr;

    ptr += sizeof(*panim->server_initcmds) * panim->num_server_initcmds;

    for (i = 0; i < ld->numserverinitcmds; i++) {
        pcmds           = &panim->server_initcmds[i];
        pcmds->num_args = ld->loadserverinitcmds[i]->num_args;
        pcmds->args     = (char **)ptr;

        ptr += sizeof(*pcmds->args) * pcmds->num_args;

        for (j = 0; j < (size_t)ld->loadserverinitcmds[i]->num_args; j++) {
            pcmds->args[j] = (char *)ptr;
            size           = strlen(ld->loadserverinitcmds[i]->args[j]) + 1;
            memcpy(pcmds->args[j], ld->loadserverinitcmds[i]->args[j], size);

            ptr += size;
        }

        ptr = (byte *)PADP(ptr, sizeof(void *));
    }

    panim->client_initcmds = (dtikicmd_t *)ptr;

    ptr += sizeof(*panim->client_initcmds) * ld->numclientinitcmds;

    for (i = 0; i < ld->numclientinitcmds; i++) {
        pcmds           = &panim->client_initcmds[i];
        pcmds->num_args = ld->loadclientinitcmds[i]->num_args;
        pcmds->args     = (char **)ptr;

        ptr += sizeof(*pcmds->args) * pcmds->num_args;

        for (j = 0; j < (size_t)ld->loadclientinitcmds[i]->num_args; j++) {
            pcmds->args[j] = (char *)ptr;
            size           = strlen(ld->loadclientinitcmds[i]->args[j]) + 1;
            memcpy(pcmds->args[j], ld->loadclientinitcmds[i]->args[j], size);

            ptr += size;
        }

        ptr = (byte *)PADP(ptr, sizeof(void *));
    }

    panim->num_bone_mappings = ld->numbonemappings;
    if (ld->numbonemappings) {
        panim->bone_mappings = (dloadbonemapping_t *)ptr;
        memcpy(panim->bone_mappings, ld->loadbonemappings, sizeof(dloadbonemapping_t) * ld->numbonemappings);

        ptr += sizeof(dloadbonemapping_t) * ld->numbonemappings;
        ptr = (byte *)PADP(ptr, sizeof(void *));
    }

    panim->num_expressions = ld->numexpressions;
    if (ld->numexpressions) {
        panim->loadexpressions = (dloadexpression_t *)ptr;
        ptr += sizeof(dloadexpression_t) * ld->numexpressions;

        for (i = 0; i < ld->numexpressions; i++) {
            const dloadexpression_t *src = ld->loadexpressions[i];
            dloadexpression_t       *dst = &panim->loadexpressions[i];
            size_t                   len = strlen(src->alias) + 1;
            int                      t;

            dst->alias = (char *)ptr;
            memcpy(ptr, src->alias, len);
            ptr += len;
            ptr = (byte *)PADP(ptr, 4);

            dst->num_morph_targets = src->num_morph_targets;

            for (t = 0; t < src->num_morph_targets; t++) {
                dloadmorphtarget_t *dtarget = (dloadmorphtarget_t *)ptr;

                memcpy(dtarget->name, src->targets[t]->name, sizeof(dtarget->name));
                dtarget->percent  = src->targets[t]->percent;
                dst->targets[t]   = dtarget;
                ptr += sizeof(dloadmorphtarget_t);
            }
        }

        ptr = (byte *)PADP(ptr, sizeof(void *));
    }

    TIKI_GetAnimOrder(ld, order);
    Com_sprintf(tempName, sizeof(tempName), "e%s", ld->path);
    UI_LoadResource(tempName);

    panim->m_aliases = temp_aliases;
    assert(ptr <= max_ptr);
    (void)max_ptr;

    numLoadedAnims = 0;
    for (i = 0; i < ld->numanims; i++) {
        anim = ld->loadanims[order[i]];
        if (!SkeletorCacheFindFilename(anim->name, &index)) {
            bPrecache = false;

            bPrecache = true;

            if (!SkeletorCacheLoadData(anim->name, bPrecache, index)) {
                if (ld->hasSkel) {
                    TIKI_DPrintf("^~^~^ TIKI_InitTiki: Couldn't load %s\n", ld->path);
                    TIKI_Free(panim);
                    return NULL;
                }

                TIKI_Error("TIKI_InitTiki: Failed to load animation '%s' at %s\n", anim->name, anim->location);
                continue;
            }
        }

        alias_index = m_cachedDataLookup[index];
        m_cachedData[alias_index].numusers++;


        panimdef = (dtikianimdef_t *)ptr;
        ptr += sizeof(dtikianimdef_t);
        panim->animdefs[numLoadedAnims]  = panimdef;
        panim->m_aliases[numLoadedAnims] = alias_index;
        temp_tanmesh[numLoadedAnims] = -1;
        {
            const char *dot = strrchr(anim->name, '.');

            if (dot && !Q_stricmp(dot, ".tan")) {
                const int tanRow = TIKI_RegisterSkel(anim->name, NULL);

                if (tanRow >= 0) {
                    skelcache[tanRow].numuses++;
                    temp_tanmesh[numLoadedAnims] = (short)tanRow;
                }
            }
        }
        Q_strncpyz(panimdef->alias, anim->alias, sizeof(panimdef->alias));
        panimdef->weight        = anim->weight;
        panimdef->flags         = anim->flags;
        panimdef->internalFlags = anim->internalFlags;
        panimdef->cmds_fixed    = qfalse;

        if (!TIKI_Anim_Compare(panimdef->alias, "idle")) {
            data = SkeletorCacheGetData(alias_index);
            if (data) {
                VectorCopy(data->bounds[0].val, panim->mins);
                VectorCopy(data->bounds[1].val, panim->maxs);
                bModelBoundsSet = true;
            }
        }

        if (anim->internalFlags & TAI_RANDOM) {
            j = strlen(panimdef->alias);
            if (j > 0 && isdigit((unsigned char)panimdef->alias[j - 1])) {
                do {
                    j--;
                } while (j > 0 && isdigit((unsigned char)panimdef->alias[j - 1]));

                panimdef->alias[j] = 0;
            } else {
                TIKI_DPrintf("TIKI_InitTiki: Random animation name '%s' should end with a number\n", panimdef->alias);
            }
        }

        panimdef->blendtime = (int)anim->blendtime;
        if (loadtikicommands) {
            panimdef->num_server_cmds = anim->num_server_cmds;
            panimdef->num_client_cmds = anim->num_client_cmds;
        } else {
            panimdef->num_server_cmds = 0;
            panimdef->num_client_cmds = 0;
        }

        panimdef->server_cmds = (dtikicmd_t *)ptr;
        ptr                   = (byte *)(panimdef->server_cmds + anim->num_server_cmds);

        for (j = 0; j < (size_t)anim->num_server_cmds; j++) {
            pcmds            = &panimdef->server_cmds[j];
            pcmds->num_args  = anim->loadservercmds[j]->num_args;
            pcmds->frame_num = anim->loadservercmds[j]->frame_num;
            pcmds->args      = (char **)ptr;

            ptr += pcmds->num_args * sizeof(*pcmds->args);

            for (k = 0; k < anim->loadservercmds[j]->num_args; k++) {
                pcmds->args[k] = (char *)ptr;
                size           = strlen(anim->loadservercmds[j]->args[k]) + 1;
                memcpy(pcmds->args[k], anim->loadservercmds[j]->args[k], size);

                ptr += size;
            }

            ptr = (byte *)PADP(ptr, sizeof(void *));
        }

        panimdef->client_cmds = (dtikicmd_t *)ptr;
        ptr += anim->num_client_cmds * sizeof(dtikicmd_t);

        for (j = 0; j < (size_t)anim->num_client_cmds; j++) {
            pcmds            = &panimdef->client_cmds[j];
            pcmds->num_args  = anim->loadclientcmds[j]->num_args;
            pcmds->frame_num = anim->loadclientcmds[j]->frame_num;
            pcmds->args      = (char **)ptr;

            ptr += pcmds->num_args * sizeof(*pcmds->args);

            for (k = 0; k < anim->loadclientcmds[j]->num_args; k++) {
                pcmds->args[k] = (char *)ptr;
                size           = strlen(anim->loadclientcmds[j]->args[k]) + 1;
                memcpy(pcmds->args[k], anim->loadclientcmds[j]->args[k], size);

                ptr += size;
            }

            ptr = (byte *)PADP(ptr, sizeof(void *));
        }

        assert(ptr <= max_ptr);

        numLoadedAnims++;
    }

    panim->m_aliases = NULL;
    if (numLoadedAnims) {
        if (!bModelBoundsSet) {
            TIKI_DPrintf("TIKI_InitTiki: no 'idle' animation found, model bounds not set for %s\n", ld->path);
        }

        panim->num_anims = numLoadedAnims;
        panim->m_aliases = (short *)TIKI_Alloc(panim->num_anims * sizeof(short));
        memcpy(panim->m_aliases, temp_aliases, panim->num_anims * sizeof(short));
        panim->tanMesh = (short *)TIKI_Alloc(panim->num_anims * sizeof(short));
        memcpy(panim->tanMesh, temp_tanmesh, panim->num_anims * sizeof(short));



        panim->modelData     = ptr;
        panim->modelDataSize = ld->modelBuf->cursize;
        memcpy(panim->modelData, ld->modelData, panim->modelDataSize);
        ptr += panim->modelDataSize;

        Com_sprintf(tempName, sizeof(tempName), "h%s", ld->path);
        UI_LoadResource(tempName);
    } else {
        TIKI_Error("TIKI_InitTiki: No valid animations found in %s.\n", ld->path);
        TIKI_Free(panim);
        panim = NULL;
    }

    assert(ptr <= max_ptr);
    return panim;
}

void TIKI_RemoveTiki(dtikianim_t *ptiki)
{
    int i;
    int alias_index;

    for (i = 0; i < ptiki->num_anims; i++) {
        alias_index = ptiki->m_aliases[i];
        m_cachedData[alias_index].numusers--;
    }
}
