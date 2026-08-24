

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    void              TIKI_CacheFileSkel(skelHeader_t *pHeader, skelcache_t *cache, int length);
    void              TIKI_CalcLodConsts(lodControl_t *LOD);
    qboolean          TIKI_LoadSKB(const char *path, skelcache_t *cache);
    qboolean          TIKI_LoadSKD(const char *path, skelcache_t *cache);
    qboolean          TIKI_LoadTAN(const char *path, skelcache_t *cache);
    skelHeaderGame_t *TIKI_GetSkel(int index);
    int               TIKI_GetSkelCache(skelHeaderGame_t *pSkel);
    void              TIKI_FreeSkel(int index);
    void              TIKI_FreeSkelCache(skelcache_t *cache);
    skelcache_t      *TIKI_FindSkel(const char *path);
    skelcache_t      *TIKI_FindSkelByHeader(skelHeaderGame_t *skelmodel);
    skelcache_t      *TIKI_FindFreeSkel(void);
    int               TIKI_RegisterSkel(const char *path, dtiki_t *tiki);
    skelMorphHeader_t *TIKI_GetMorph(int index);
    void              TIKI_LoadSKBMorph(skelHeaderGame_t *pSkel, const char *morphFileName);
    int               TIKI_Morph_NumForName(dtiki_t *tiki, const char *name);

#ifdef __cplusplus
}
#endif

    int  TIKI_CalcBoneMapping(int *map, skelHeaderGame_t *from, skelHeaderGame_t *to);
    int  TIKI_RegisterBoneMapping(int fromIndex, int toIndex);
    int *TIKI_GetBoneMapping(int handle);
    void TIKI_ReleaseBoneMapping(int handle);

    int  TIKI_CalcMorphMapping(int *map, skelHeaderGame_t *from, skelHeaderGame_t *to);
    int  TIKI_RegisterMorphMapping(int fromIndex, int toIndex);
    int *TIKI_GetMorphMapping(int handle);
    void TIKI_ReleaseMorphMapping(int handle);

    int  TIKI_CalcAnimBoneMapping(int *map, skelAnimHeader_t *anim, skelHeaderGame_t *skel);
    int  TIKI_RegisterAnimBoneMapping(skelAnimHeader_t *anim, int skelIndex);
    struct animBoneMapping_s *TIKI_FindAnimBoneMapping(
        skelAnimHeader_t *anim, int checksum, skelHeaderGame_t *skel);
    int *TIKI_GetAnimBoneMapping(int handle);
    void TIKI_FreeAnimBoneMapping(int handle);
