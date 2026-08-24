

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    int         TIKI_Surface_NameToNum(dtiki_t *pmdl, const char *name);
    const char *TIKI_Surface_NumToName(dtiki_t *pmdl, int num);
    int         TIKI_Surface_Flags(dtiki_t *pmdl, int num);
    int         TIKI_Surface_NumSkins(dtiki_t *pmdl, int num);

#ifdef __cplusplus
}
#endif
