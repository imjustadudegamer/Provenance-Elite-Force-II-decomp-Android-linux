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

#ifdef __cplusplus
#    include "../skeletor/skeletor_name_lists.h"

extern ChannelNameTable skel_BoneNames;
extern ChannelNameTable skel_ChannelNames;
#endif

#include "../skeletor/skeletor_model_file_format.h"

#define TAF_DELTADRIVEN    0x01 
#define TAF_DEFAULT_ANGLES 0x08
#define TAF_NOTIMECHECK    0x10 

// already-picked marker, which is why those bits could never be made retail-exact.
#define TAI_RANDOM   0x01 
#define TAI_NOREPEAT 0x02 
#define TAI_PICKED   0x04 

#define TAF_HASDELTA 0x40
#define TAF_HASMORPH 0x80
#define TAF_HASUPPER 0x100

// this port, until now) carried END as -4, which is a value retail never produces and
#define TIKI_FRAME_LAST         -5 
#define TIKI_FRAME_END          -5  
#define TIKI_FRAME_ENTRY        -3 
#define TIKI_FRAME_EXIT         -2 
#define TIKI_FRAME_EVERY        -1 
#define TIKI_FRAME_FIRST        0  
#define TIKI_FRAME_MAXFRAMERATE 60

#define TIKI_SKEL_LOD_INDEXES 10

#define MAX_DEFNAME  128
#define MAX_SKELNAME 128

#define TIKI_SKB_HEADER_IDENT   (*(int *)"SKL ")
#define TIKI_SKB_HEADER_VER_3   3
#define TIKI_SKB_HEADER_VERSION 4

#define TIKI_SKD_HEADER_IDENT       (*(int *)"SKMD")
#define TIKI_SKD_HEADER_OLD_VERSION 5
#define TIKI_SKD_HEADER_VERSION     6

#define TIKI_SKC_HEADER_IDENT       (*(int *)"SKAN")
#define TIKI_SKC_HEADER_OLD_VERSION 13
#define TIKI_SKC_HEADER_VERSION     14
#define TIKI_SKC_HEADER_EF2_VERSION 4

#define TIKI_SKB_EF2_HEADER_SIZE    96

#define TIKI_MAX_BONES       100
#define TIKI_MAX_VERTEXES    4096 
#define TIKI_MAX_TRIANGLES   2000
#define TIKI_MAX_SKELCACHE   1024
#define TIKI_CMD_MAX_CMDS    256
#define TIKI_CMD_MAX_ARGS    12
#define TIKI_MAX_COMMANDS    TIKI_CMD_MAX_CMDS

#define TIKI_MAX_ENTITIES              2048
#define TIKI_MAX_ENTITY_CACHE_PER_ENT  2
#define TIKI_MAX_ENTITY_CACHE          (TIKI_MAX_ENTITIES*TIKI_MAX_ENTITY_CACHE_PER_ENT)

#define TIKI_SURF_SKIN1     (1 << 0)
#define TIKI_SURF_SKIN2     (1 << 1)
#define TIKI_SURF_SKIN3     (TIKI_SURF_SKIN1 | TIKI_SURF_SKIN2)
#define TIKI_SURF_NODRAW    (1 << 2)
#define TIKI_SURF_CROSSFADE (1 << 6)
#define TIKI_SURF_NODAMAGE  (1 << 7)
#define TIKI_SURF_NOMIPMAPS (1 << 8)
#define TIKI_SURF_NOPICMIP  (1 << 9)
#define TIKI_SURF_REPLACED  (1 << 10)
// extra registered mesh, and retail never does that -- replacesurface swaps the

#define MAX_SKELMODELS        12
#define MAX_TIKI_SHADER       4
#define MAX_TIKI_ALIASES      1800
#define MAX_GLOBAL_FROM_LOCAL 200

#define MAX_LOD_CURVE_POINTS  5
#define MAX_LOD_CURVE_CONSTS  4

typedef struct AliasList_s     AliasList_t;
typedef struct AliasListNode_s AliasListNode_t;

typedef struct tiki_singlecmd_s {
    int    num_args;
    char  *args[TIKI_CMD_MAX_ARGS];
} tiki_singlecmd_t;

typedef struct tiki_cmd_s {
    int              num_cmds;
    tiki_singlecmd_t cmds[TIKI_CMD_MAX_CMDS];
} tiki_cmd_t;

#ifdef __cplusplus

typedef class skelChannelList_s
{
public:
    short int m_numChannels;
    short int m_numLocalFromGlobal;
    short int *m_chanLocalFromGlobal;
    short int m_chanGlobalFromLocal[MAX_GLOBAL_FROM_LOCAL];

    int         NumChannels(void) const;
    void        ZeroChannels(void);
    void        PackChannels(void);
    void        InitChannels(void);
    void        CleanUpChannels();
    int         GlobalChannel(int localchannel) const;
    int         LocalChannel(int channel) const;
    int         GetLocalFromGlobal(int globalChannel) const;
    void        SetLocalFromGlobal(int channel, int localchannel);
    qboolean    HasChannel(ChannelNameTable *nameTable, const char *channelName) const;
    qboolean    HasChannel(int channel) const;
    int         AddChannel(int newGlobalChannelNum);
    const char *ChannelName(ChannelNameTable *nameTable, int localChannelNum) const;
} skelChannelList_c;

#else

typedef struct skelChannelList_s {
    short int  m_numChannels;
    short int  m_numLocalFromGlobal;
    short int *m_chanLocalFromGlobal;
    short int  m_chanGlobalFromLocal[MAX_GLOBAL_FROM_LOCAL];
} skelChannelList_c;

#endif

typedef struct dloadbonemapping_s {
    char  bone[MAX_NAME_LENGTH];
    char  surface[MAX_NAME_LENGTH];
    float radius;
    float length;
} dloadbonemapping_t;

#ifndef DTIKI_EXPRESSION_TYPES_DEFINED
#define DTIKI_EXPRESSION_TYPES_DEFINED
#define MAX_EXPRESSION_NAME_LENGTH 32

typedef struct {
    int   morph_index;
    float percent;
} dtikimorphtarget_t;

typedef struct {
    char alias[MAX_EXPRESSION_NAME_LENGTH];
    int  num_morph_targets;
    int  ofs_morph_targets;
} dtikiexpression_t;
#endif

typedef struct dloadexpression_s dloadexpression_t;

typedef struct dtikianim_s {
    char                  *name;
    int                    num_anims;
    void                  *alias_list;
    int                    num_client_initcmds;
    struct dtikicmd_s     *client_initcmds;
    int                    num_server_initcmds;
    struct dtikicmd_s     *server_initcmds;
    byte                  *modelData;
    size_t                 modelDataSize;
    vec3_t                 mins;
    vec3_t                 maxs;
    short int             *m_aliases;
    short int             *tanMesh;
    // tokens do not occur anywhere in the retail engine image nor in any of the
    int                    num_bone_mappings;
    dloadbonemapping_t    *bone_mappings;
    int                    num_expressions;
    dloadexpression_t     *loadexpressions;
    struct dtikianimdef_s *animdefs[1];
} dtikianim_t;

typedef struct dtikiReplacedSurface_s {
    int  skelIndex;
    int  surfaceIndex;
    int  boneMappingIndex;
    int  morphMappingIndex;
    char skelName[MAX_SKELNAME];
    char morphName[MAX_SKELNAME];
} dtikiReplacedSurface_t;

typedef struct dtiki_s {
    char                   name[MAX_DEFNAME];
    int                    num_surfaces;
    int                    num_tags;
    int                    num_anims;
    int                    num_bone_mappings;
    int                    ofs_bone_mappings;
    int                    ofsAnimBoneMappings;
    int                    num_expressions;
    int                    ofs_expressions;
    void                  *alias_list;
    int                    num_client_initcmds;
    int                    ofs_client_initcmds;
    int                    num_server_initcmds;
    int                    ofs_server_initcmds;
    struct dtikisurface_s *surfaces;
    int                    ofs_replacedSurfaces;
    float                  load_scale;
    float                  lod_scale;
    float                  lod_bias;
    float                  lod_start_distance;
    float                  lod_start_percent_verts;
    float                  lod_stop_distance;
    float                  lod_stop_percent_verts;
    float                  fade_dist_mod;
    vec3_t                 boundingVolumeMin;
    vec3_t                 boundingVolumeMax;
    vec3_t                 light_offset;
    vec3_t                 head_offset;
    qboolean               _loadAllAnims;
    vec3_t                 load_origin;
    vec3_t                 mins;
    vec3_t                 maxs;
    float                  radius;
    int                    skelIndex;
    char                   skelName[MAX_SKELNAME];
    char                   morphName[MAX_SKELNAME];
    unsigned int           exclusive_light_name_hash;
    tikiBoneMapping_t     *bone_mappings;
    dtikianim_t           *a;
    void                  *skeletor;
    skelChannelList_c      m_boneList;
    int                    numMeshes;
    short int              mesh[1];
    int                   *boneMappings;
    dtikiReplacedSurface_t *replacedSurfaces;
    int                     numReplacedSurfaces;

#ifdef __cplusplus
    int         GetBoneNumFromName(const char *name);
    const char *GetBoneNameFromNum(int num) const;
#endif
} dtiki_t;


#ifdef __cplusplus
static_assert(offsetof(dtiki_t, num_surfaces) == 128, "retail dtiki_s+128");
static_assert(offsetof(dtiki_t, num_tags) == 132, "retail dtiki_s+132");
static_assert(offsetof(dtiki_t, num_anims) == 136, "retail dtiki_s+136");
static_assert(offsetof(dtiki_t, num_bone_mappings) == 140, "retail dtiki_s+140");
static_assert(offsetof(dtiki_t, ofs_bone_mappings) == 144, "retail dtiki_s+144");
static_assert(offsetof(dtiki_t, ofsAnimBoneMappings) == 148, "retail dtiki_s+148");
static_assert(offsetof(dtiki_t, num_expressions) == 152, "retail dtiki_s+152");
static_assert(offsetof(dtiki_t, ofs_expressions) == 156, "retail dtiki_s+156");
static_assert(sizeof(dtikimorphtarget_t) == 8, "retail dtikimorphtarget_t is int+float");
static_assert(offsetof(dtikimorphtarget_t, morph_index) == 0, "retail morph_index +0");
static_assert(offsetof(dtikimorphtarget_t, percent) == 4, "retail percent +4");
static_assert(sizeof(dtikiexpression_t) == 40, "retail dtikiexpression_t is 40 bytes");
static_assert(offsetof(dtikiexpression_t, alias) == 0, "retail alias +0");
static_assert(offsetof(dtikiexpression_t, num_morph_targets) == 32, "retail num_morph_targets +32");
static_assert(offsetof(dtikiexpression_t, ofs_morph_targets) == 36, "retail ofs_morph_targets +36");
static_assert(offsetof(dtiki_t, alias_list) == 160, "retail dtiki_s+160");
static_assert(
    offsetof(dtiki_t, load_scale) == 188 + 2 * (sizeof(void *) - 4),
    "retail dtiki_s+188 (alias_list +surfaces widened)");
static_assert(
    offsetof(dtiki_t, lod_scale) == 192 + 2 * (sizeof(void *) - 4),
    "retail dtiki_s+192 (alias_list +surfaces widened)");
static_assert(
    offsetof(dtiki_t, lod_bias) == 196 + 2 * (sizeof(void *) - 4),
    "retail dtiki_s+196 (alias_list +surfaces widened)");
static_assert(
    offsetof(dtiki_t, lod_start_distance) == 200 + 2 * (sizeof(void *) - 4),
    "retail dtiki_s+200 (alias_list +surfaces widened)");
static_assert(
    offsetof(dtiki_t, lod_start_percent_verts) == 204 + 2 * (sizeof(void *) - 4),
    "retail dtiki_s+204 (alias_list +surfaces widened)");
static_assert(
    offsetof(dtiki_t, lod_stop_distance) == 208 + 2 * (sizeof(void *) - 4),
    "retail dtiki_s+208 (alias_list +surfaces widened)");
static_assert(
    offsetof(dtiki_t, lod_stop_percent_verts) == 212 + 2 * (sizeof(void *) - 4),
    "retail dtiki_s+212 (alias_list +surfaces widened)");
static_assert(
    offsetof(dtiki_t, fade_dist_mod) == 216 + 2 * (sizeof(void *) - 4),
    "retail dtiki_s+216 (alias_list +surfaces widened)");
static_assert(
    offsetof(dtiki_t, light_offset) == 244 + 2 * (sizeof(void *) - 4),
    "retail dtiki_s+244 (alias_list +surfaces widened)");
static_assert(
    offsetof(dtiki_t, head_offset) == 256 + 2 * (sizeof(void *) - 4),
    "retail dtiki_s+256 (alias_list +surfaces widened)");
static_assert(
    offsetof(dtiki_t, load_origin) == 272 + 2 * (sizeof(void *) - 4),
    "retail dtiki_s+272 (alias_list +surfaces widened)");
static_assert(
    offsetof(dtiki_t, radius) == 308 + 2 * (sizeof(void *) - 4),
    "retail dtiki_s+308 (alias_list +surfaces widened)");
static_assert(
    offsetof(dtiki_t, skelIndex) == 312 + 2 * (sizeof(void *) - 4),
    "retail dtiki_s+312 (alias_list +surfaces widened)");
#endif

typedef struct {
    int   ident;
    int   version;
    char  name[64];
    int   numSurfaces;
    int   numBones;
    int   ofsBones;
    int   ofsSurfaces;
    int   ofsEnd;
    int   lodIndex[TIKI_SKEL_LOD_INDEXES];
    int   numBoxes;
    int   ofsBoxes;
    int   numMorphTargets;
    int   ofsMorphTargets;
    float scale;
} skelHeader_t;

typedef struct {
    short int parent;
    short int boxIndex;
    int       flags;
    char      name[64];
} skelBoneName_t;

typedef struct {
    int  ident;
    char name[64];
    int  numTriangles;
    int  numVerts;
    int  numModelVerts;
    int  ofsTriangles;
    int  ofsVerts;
    int  ofsCollapse;
    int  ofsEnd;
} skelSurface_t;

typedef short int skelIndex_t;

typedef struct tanXyzNormal_s {
    unsigned short xyz[3];
    short          normal;
} tanXyzNormal_t;

typedef struct skelSurfaceGame_s {
    int     ident;
    char    name[MAX_QPATH];
    int     numTriangles;
    int     numVerts;
    int     numModelVerts;
    vec4_t *pStaticXyz;
    vec4_t *pStaticNormal;
    vec2_t (*pStaticTexCoords)[2];
    skelIndex_t              *pTriangles;
    struct skeletorVertex_s  *pVerts;
    int                      *pCollapse;
    struct skelSurfaceGame_s *pNext;
    struct tanXyzNormal_s *pTanXyz;

#ifdef __cplusplus
    skelSurfaceGame_s();
#endif
} skelSurfaceGame_t;

struct skmMorphData_s; 

typedef struct skelHeaderGame_s {
    int                  version;
    char                 name[64];
    int                  numSurfaces;
    int                  numBones;
    struct boneData_s   *pBones;
    skelSurfaceGame_t   *pSurfaces;
    int                  numBoxes;
    struct skelHitBox_s *pBoxes;
    int                  numMorphTargets;
    char                *pMorphTargets;
    float                scale;
    struct skmMorphData_s *pMorphData;
    int    tanNumFrames;
    float  tanFrameTime;
    float *pTanFrameXform;
    int    tanRadiusFrames;
    struct tikiFrame_s *pTanFrames;
} skelHeaderGame_t;

typedef struct boneData_s {
    short int  channel;
    boneType_t boneType;
    short int  parent;
    short int  numChannels;
    short int  numRefs;
    short int  channelIndex[2];
    short int  refIndex[2];
    vec3_t     offset;
    float      length;
    float      weight;
    float      bendRatio;
    float      bendMax;
    float      spinRatio;

    short int  ef2BodyPart;

    short int  baseRow[8];
    qboolean   hasBaseRow;

    short int  skelParent;
} boneData_t;

typedef struct tikiSurface_s {
    int  ident;
    char name[64];
    int  numFrames;
    int  numVerts;
    int  numModelVerts;
    int  numTriangles;
    int  ofsTriangles;
    int  ofsCollapseMap;
    int  ofsSt;
    int  ofsXyzNormals;
    int  ofsEnd;
} tikiSurface_t;

#define TAN_TAG_NAME_SIZE 64                       
#define TAN_TAG_POSE_SIZE 48                       

#define TAN_TAG_STRIDE(numFrames) ((size_t)TAN_TAG_NAME_SIZE + (size_t)(numFrames) * (size_t)TAN_TAG_POSE_SIZE)

#define TAN_TAG_NAME_OFS(ofsTags, numFrames, t) ((size_t)(ofsTags) + (size_t)(t) * TAN_TAG_STRIDE(numFrames))

#define TAN_TAG_POSE_OFS(ofsTags, numFrames, t, f) \
    (TAN_TAG_NAME_OFS(ofsTags, numFrames, t) + (size_t)TAN_TAG_NAME_SIZE + (size_t)(f) * (size_t)TAN_TAG_POSE_SIZE)

#define TAN_TAG_BLOCK_SIZE(numTags, numFrames) ((size_t)(numTags) * TAN_TAG_STRIDE(numFrames))

typedef struct dtikisurface_s {
    char  name[MAX_QPATH];
    char  shader[MAX_TIKI_SHADER][MAX_QPATH];
    int   hShader[MAX_TIKI_SHADER];
    int   numskins;
    int   flags;
    float damage_multiplier;
} dtikisurface_t;

typedef struct tikiFrame_s {
    vec3_t bounds[2];
    vec3_t scale;
    vec3_t offset;
    vec3_t delta;
    float  radius;
    float  frametime;
} tikiFrame_t;

#ifdef __cplusplus
static_assert(sizeof(tikiFrame_t) == 68, "retail tikiFrame_t (SDK qfiles.h) is 68 bytes");
static_assert(offsetof(tikiFrame_t, bounds) == 0, "retail tikiFrame_t bounds +0");
static_assert(offsetof(tikiFrame_t, scale) == 24, "retail tikiFrame_t scale +0x18");
static_assert(offsetof(tikiFrame_t, offset) == 36, "retail tikiFrame_t offset +0x24");
static_assert(offsetof(tikiFrame_t, delta) == 48, "retail tikiFrame_t delta +0x30");
static_assert(offsetof(tikiFrame_t, radius) == 60, "retail tikiFrame_t radius +0x3C");
static_assert(offsetof(tikiFrame_t, frametime) == 64, "retail tikiFrame_t frametime +0x40");
#endif

typedef struct tiki_tess_data_s {
    int            ident;
    tikiSurface_t *newsurface;
    tikiSurface_t *oldsurface;
} tiki_tess_data_t;

typedef struct skelWeight_s {
    int    boneIndex;
    float  boneWeight;
    vec3_t offset;
} skelWeight_t;

typedef struct skelVertex_s {
    vec3_t       normal;
    vec2_t       texCoords;
    int          numWeights;
    skelWeight_t weights[1];
} skelVertex_t;

typedef struct skeletorVertex_s {
    vec3_t normal;
    vec2_t texCoords;
    int    numWeights;
    int    numMorphs;
} skeletorVertex_t;

typedef struct skeletorVertexGame_s {
    vec3_t normal;
    vec2_t texCoords;
    int    numWeights;
    int    numMorphs;
} skeletorVertexGame_t;

typedef struct staticSurface_s {
    int                ident;
    int                ofsStaticData;
    skelSurfaceGame_t *surface;
    int                meshNum;
} staticSurface_t;

typedef struct {
    int    morphIndex;
    vec3_t offset;
} skeletorMorph_t;

typedef struct dtikianimdef_s {
    char               alias[48];
    float              weight;
    int                blendtime;
    int                flags;          
    int                internalFlags; 
    int                num_client_cmds;
    struct dtikicmd_s *client_cmds;
    int                num_server_cmds;
    struct dtikicmd_s *server_cmds;
    // hold TIKI_FRAME_LAST (-5)" latch that TIKI_GetAnim @0x000F4810 tests before calling
    qboolean           cmds_fixed;
} dtikianimdef_t;

typedef struct skelBone_s {
    short int shortQuat[4];
    short int shortOffset[3];
    short int pad;
} skelBone_t;

typedef struct skelFrame_s {
    vec3_t     bounds[2];
    float      radius;
    vec3_t     delta;
    skelBone_t bones[1];
} skelFrame_t;

typedef struct skelAnimHeader_s {
    int    ident;
    int    version;
    char   name[64];
    int    type;
    int    numFrames;
    int    numbones;
    float  totaltime;
    float  frametime;
    vec3_t totaldelta;
    int    ofsChannelNames;
    int    ofsFrames;
} skelAnimHeader_t;

typedef struct {
    float offset[4];
    float matrix[3][4];
} skelBoneCache_t;

typedef struct {
    float pos;
    float val;
} lodCurvePoint_t;

typedef struct {
    float base;
    float scale;
    float cutoff;
} lodCurvePointConstants_t;

typedef struct lodControl_s {
    float                    minMetric;
    float                    maxMetric;
    lodCurvePoint_t          curve[MAX_LOD_CURVE_POINTS];
    lodCurvePointConstants_t consts[MAX_LOD_CURVE_CONSTS];
} lodControl_t;

typedef struct skelMorphTargetRecord_s {
    char name[64];
    int  numVerts;
    int  ofsIndices;
} skelMorphTargetRecord_t;

typedef struct skelMorphHeader_s {
    int   numMorphVertices;   
    int   numTargets;         
    int   ofsVertexRemap;     
    int   ofsTargets;         
} skelMorphHeader_t;


typedef struct {
    char              path[96];
    skelHeaderGame_t *skel;
    size_t            size;
    int               numuses;
    skelMorphHeader_t *morph;
} skelcache_t;

typedef int skeletorMorphCache_t;
