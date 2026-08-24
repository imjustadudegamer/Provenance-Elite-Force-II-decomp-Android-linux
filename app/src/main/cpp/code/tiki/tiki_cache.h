

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    void       *TIKI_Alias_GetList(int handle);
    const char *TIKI_Alias_Find(int handle, const char *alias);
    qboolean    TIKI_Alias_UpdateDialog(
        int handle, const char *alias, int number_of_times_played, byte been_played_this_loop, int last_time_played
    );
    qboolean    TIKI_Alias_AddActorDialog(
        int handle, const char *alias, int actor_number, int number_of_times_played, byte been_played_this_loop,
        int last_time_played
    );
    int         TIKI_NumMorphs(int handle);
    void        TIKI_ModelList(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

    dtikianim_t *TIKI_FindTikiAnim(const char *path);
    dtiki_t     *TIKI_FindTiki(const char *path);
    dtikianim_t *TIKI_RegisterTikiAnimFlags(const char *path, qboolean use);
    dtikianim_t *TIKI_RegisterTikiAnim(const char *path);
    dtiki_t     *TIKI_RegisterTikiFlags(const char *path, qboolean use);

    // engine-internal), so it also needs the inverse map; retail never does
    int      TIKI_RegisterTiki(const char *path);
    int      TIKI_ReloadTiki(const char *name);
    dtiki_t *TIKI_GetTiki(int handle);
    qboolean TIKI_TikiHandleValid(int handle, const char *name);
    int      TIKI_HandleForTiki(const dtiki_t *tiki);
    void     TIKI_FreeTiki(int handle);

    int  TIKI_CacheAll(void);
    void TIKI_CacheAllInDir(const char *dir);
    void         TIKI_FreeAll(void);
    void         TIKI_FreeLevelData(void);
    void        *TIKI_GetSkeletor(dtiki_t *tiki, int entnum);
    void         TIKI_Begin(void);
    void         TIKI_End(void);
    void         TIKI_FinishLoad(void);
    void         TIKI_FreeImages(void);
    void         TIKI_TikiAnimList_f(void);
    void         TIKI_TikiList_f(void);

#ifdef __cplusplus
}
#endif
