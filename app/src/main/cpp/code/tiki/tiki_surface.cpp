

#include "q_shared.h"
#include "qcommon.h"
#include "../corepp/tiki.h"

extern "C" void TIKI_Error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

int TIKI_Surface_NameToNum(dtiki_t *pmdl, const char *name)
{
    int             i;
    dtikisurface_t *psurface;

    for (i = 0; i < pmdl->num_surfaces; i++) {
        psurface = &pmdl->surfaces[i];

        if (!stricmp(psurface->name, name)) {
            return i;
        }
    }

    return -1;
}

const char *TIKI_Surface_NumToName(dtiki_t *pmdl, int num)
{
    if (num < 0 || num >= pmdl->num_surfaces) {
        TIKI_Error("TIKI_Surface_NumToName: Surface %d out of range for %s.\n", num, pmdl->a->name);
        return NULL;
    }

    assert(pmdl->surfaces[num].name[0]);
    return pmdl->surfaces[num].name;
}

int TIKI_Surface_Flags(dtiki_t *pmdl, int num)
{
    if (num < 0 || num >= pmdl->num_surfaces) {
        TIKI_Error("TIKI_Surface_Flags: Surface %d out of range for %s.\n", num, pmdl->a->name);
        return 0;
    }

    return pmdl->surfaces[num].flags;
}

int TIKI_Surface_NumSkins(dtiki_t *pmdl, int num)
{
    if (num < 0 || num >= pmdl->num_surfaces) {
        TIKI_Error("TIKI_Surface_NumSkins: Surface %d out of range for %s.\n", num, pmdl->a->name);
        return 0;
    }

    return pmdl->surfaces[num].numskins;
}
