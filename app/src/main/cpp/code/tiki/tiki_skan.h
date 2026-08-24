

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct skelAnimDataGameHeader_s;
typedef struct skelAnimDataGameHeader_s skelAnimDataGameHeader_t;
struct dtiki_s;
typedef struct dtiki_s dtiki_t;

struct skelAnimHeader_s;
struct skelFrame_s;

typedef struct skelAnimHeader_s skelAnimHeaderRec_t;
typedef struct skelFrame_s     skelFrameRec_t;

// hand back the same bytes as a host-order skelAnimHeader_t.  Retail never
skelAnimHeaderRec_t *TIKI_Skan_RetainNative(const void *image, size_t bytes);

void TIKI_Skan_Bind(skelAnimDataGameHeader_t *owner, skelAnimHeaderRec_t *hdr);

void TIKI_Skan_Release(const skelAnimDataGameHeader_t *owner);

skelAnimHeaderRec_t *TIKI_Skan_Find(const skelAnimDataGameHeader_t *owner);

skelAnimHeaderRec_t *TIKI_Skan_FindByAnim(dtiki_t *tiki, int animnum);

void TIKI_SwapAnimSkel(skelAnimHeaderRec_t *hdr);

const short int *TIKI_Skan_RecordRow(
    skelAnimHeader_t *hdr,
    skelFrame_t      *frameRec,
    const int        *mapping,
    int               bone,
    float             baseLen,
    const char       *boneName,
    const short int  *bindRow,
    float            *outRatio
);
int TIKI_SkelBonesFromRecords(
    dtiki_t              *tiki,
    int                   anim,
    int                   frame,
    int                   oldanim,
    int                   oldframe,
    float                 framelerp,
    int                   crossblend_anim,
    int                   crossblend_frame,
    float                 crossblend_lerp,
    int                   torso_anim,
    int                   torso_frame,
    int                   torso_oldanim,
    int                   torso_oldframe,
    float                 torso_framelerp,
    int                   torso_crossblend_anim,
    int                   torso_crossblend_frame,
    float                 torso_crossblend_lerp,
    qboolean              uselegs,
    qboolean              usetorso,
    const int            *bone_tag,
    const vec4_t         *bone_quat,
    const vec3_t          localOrigin,
    skelBoneCache_t      *bones,
    int                   maxBones,
    float                *radius,
    vec3_t               *mins,
    vec3_t               *maxs
);

skelMorphHeader_t *TIKI_BuildMorphHeader(const void *morphData, size_t *outSize);

qboolean TIKI_ValidateSkelAnimAndFrame(
    dtiki_t            *tiki,
    int                 animnum,
    int                 framenum,
    skelAnimHeaderRec_t **hdrOut,
    skelFrameRec_t     **frameOut
);

typedef struct {
    short int shortQuat[4];
    short int shortOffset[3];
    short int pad;
} tikiSkanBoneRec_t;

#ifdef __cplusplus
}
#endif
