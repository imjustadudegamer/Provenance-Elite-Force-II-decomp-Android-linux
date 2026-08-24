

#pragma once

struct refEntity_s;
typedef struct refEntity_s refEntity_t;

#ifdef __cplusplus
extern "C" {
#endif

    int           TIKI_Tag_NameToNum(dtiki_t *pmdl, const char *name);
    const char   *TIKI_Tag_NumToName(dtiki_t *pmdl, int iTagNum);
    int           TIKI_Bone_GetParentNum(dtiki_t *pmdl, int iTagNum);
    void TIKI_Tag_Orientation(
        orientation_t *out,
        int            handle,
        int            anim,
        int            frame,
        int            tagnum,
        float          scale,
        const int     *bone_tag,
        const vec4_t  *bone_quat
    );
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
    );
    void TIKI_Tag_NamedOrientation(
        orientation_t *out,
        int            handle,
        int            anim,
        int            frame,
        float          scale,
        const char    *tagName,
        const int     *bone_tag,
        const vec4_t  *bone_quat
    );
    void TIKI_Tag_LerpOrientation(
        orientation_t     *out,
        int                handle,
        const refEntity_t *ent,
        int                tagnum
    );
    void TIKI_Tag_LerpOrientationSkel(
        orientation_t     *out,
        const refEntity_t *ent,
        dtiki_t           *tiki,
        int                tagnum
    );
    SkelMat4     *TIKI_TransformInternal(dtiki_t *tiki, int entnum, int tagnum);
    qboolean      TIKI_IsOnGroundInternal(dtiki_t *tiki, int entnum, int tagnum, float threshold);
    orientation_t TIKI_OrientationInternal(dtiki_t *tiki, int entnum, int tagnum, float scale);
    void          TIKI_SetPoseInternal(
                 void *skeletor, const frameInfo_t *frameInfo, const int *bone_tag, const vec4_t *bone_quat, float actionWeight
             );
    void          TIKI_SetPoseChannelsInternal(
        void            *skeletor,
        const frameInfo_t *frameInfo,
        const short int *bodyParts,
        const int       *bone_tag,
        const vec4_t    *bone_quat,
        float            actionWeight
    );
    float TIKI_GetRadiusInternal(dtiki_t *tiki, int entnum, float scale);
    float TIKI_GetCentroidRadiusInternal(dtiki_t *tiki, int entnum, float scale, float *centroid);
    void  TIKI_GetFrameInternal(dtiki_t *tiki, int entnum, skelAnimFrame_t *newFrame);
    void  TIKI_SetEyeTargetPos(dtiki_t *tiki, int entnum, vec3_t pos);

#ifdef __cplusplus
}
#endif
