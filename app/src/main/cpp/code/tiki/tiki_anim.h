

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    void        TIKI_GetAnimOrder(dloaddef_t *ld, int *order);

    const char *TIKI_Anim_NameForNum(dtiki_t *pmdl, int animnum);
    int         TIKI_Anim_NumForName(dtiki_t *pmdl, const char *name);
    int         TIKI_Anim_Random(dtiki_t *pmdl, const char *name);
    int         TIKI_Anim_NumFrames(dtiki_t *pmdl, int animnum);
    struct tikiFrame_s *TIKI_Anim_TanFrames(dtiki_t *pmdl, int animnum, int *pNumFrames);
    void        TIKI_Frame_Bounds(dtiki_t *pmdl, int animnum, int framenum, float scale, vec3_t mins, vec3_t maxs);
    void        TIKI_Full_Frame_Bounds(
        dtiki_t *pmdl, int anim, int framenum, int torso_anim, int torso_framenum,
        float scale, vec3_t mins, vec3_t maxs);
    float       TIKI_Anim_Time(dtiki_t *pmdl, int animnum);
    float       TIKI_Anim_Frametime(dtiki_t *pmdl, int animnum);

    void        TIKI_Anim_Delta(dtiki_t *pmdl, int animnum, float *delta);
    void        TIKI_Anim_AbsoluteDelta(dtiki_t *pmdl, int animnum, float *delta);
    void        TIKI_Anim_AngularDelta(dtiki_t *pmdl, int animnum, float *delta);
    qboolean    TIKI_Anim_HasDelta(dtiki_t *pmdl, int animnum);
    void        TIKI_Anim_DeltaOverTime(dtiki_t *pTiki, int iAnimnum, float fTime1, float fTime2, vec3_t vDelta);
    void        TIKI_Anim_AngularDeltaOverTime(dtiki_t *pTiki, int iAnimnum, float fTime1, float fTime2, float *fDelta);

    int         TIKI_Anim_Flags(dtiki_t *pmdl, int animnum);
    int         TIKI_Anim_FlagsSkel(dtiki_t *pmdl, int animnum);

    qboolean    TIKI_Anim_HasServerCommands(dtiki_t *pmdl, int animnum);
    qboolean    TIKI_Anim_HasClientCommands(dtiki_t *pmdl, int animnum);
    int         TIKI_Anim_CrossblendTime(dtiki_t *pmdl, int animnum);
    qboolean    TIKI_AnimLoaded(dtiki_t *pmdl, int animnum);
    void        TIKI_Frame_Delta(dtiki_t *pmdl, int animnum, int framenum, float *delta);
    float       TIKI_Frame_Time(dtiki_t *pmdl, int animnum, int framenum);
    void        TIKI_Frame_Coordinates(
        dtiki_t *pmdl, int animnum, int framenum, float scale, float *coordinates);
    float       TIKI_Frame_Radius(dtiki_t *pmdl, int animnum, int framenum);

    int         TIKI_Anim_Compare(const char *s1, const char *s2);

#ifdef __cplusplus
}
#endif
