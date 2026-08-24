

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    const char *TIKI_Name(dtiki_t *pmdl);
    int         TIKI_NumAnims(dtiki_t *pmdl);
    int         TIKI_NumSurfaces(dtiki_t *pmdl);
    int         TIKI_NumTags(dtiki_t *pmdl);
    void        TIKI_CalculateBounds(dtiki_t *pmdl, float scale, vec3_t mins, vec3_t maxs);
    float       TIKI_GetFadeDistMod(dtiki_t *pmdl);
    qboolean    TIKI_Alias_Add(dtiki_t *pmdl, const char *alias, const char *name, const char *parameters);
    const char *TIKI_Alias_FindRandom(dtiki_t *pmdl, const char *alias);
    void        TIKI_Alias_Dump(dtiki_t *pmdl);
    void        TIKI_Alias_Clear(dtiki_t *pmdl);
    const char *TIKI_Alias_FindDialog(dtiki_t *pmdl, const char *alias, int random, int entity_number);
    const char *TIKI_Alias_FindSpecificAnim(dtiki_t *pmdl, const char *name);
    qboolean    TIKI_Alias_CheckLoopAnim(dtiki_t *pmdl, const char *name);
    float       TIKI_GlobalRadius(dtiki_t *pmdl);

#ifdef __cplusplus
}
#endif
