

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    extern int         cache_numskel;
    extern int         cache_maxskel;
    extern skelcache_t skelcache[];

    void TIKI_AddPointToBounds(float *v, float *mins, float *maxs);
    void TIKI_Message(const char *fmt, ...) Q_PRINTF_FUNC(1, 2);
    void TIKI_Warning(const char *fmt, ...) Q_PRINTF_FUNC(1, 2);
    void TIKI_Error(const char *fmt, ...) Q_PRINTF_FUNC(1, 2);
    void
    TIKI_SetupIndividualSurface(const char *filename, dtikisurface_t *surf, const char *name, dloadsurface_t *loadsurf);
    size_t       TIKI_CalcAnimDefSize(dloaddef_t *ld);
    dtikianim_t *TIKI_FillTIKIStructureSkel(dloaddef_t *ld);
    void         TIKI_SwapSkel(skelHeader_t *pheader);

#ifdef __cplusplus
}
#endif
