

#include "q_shared.h"
#include "qcommon.h"
#include "tiki_shared.h"
#include "tiki_utility.h"
#include "../qcommon/alias.h"

extern "C" void TIKI_Error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

const char *TIKI_Name(dtiki_t *pmdl)
{
    if (pmdl) {
        return pmdl->a->name;
    } else {
        return 0;
    }
}

int TIKI_NumAnims(dtiki_t *pmdl)
{
    if (pmdl) {
        return pmdl->a->num_anims;
    } else {
        return 0;
    }
}

int TIKI_NumSurfaces(dtiki_t *pmdl)
{
    if (pmdl) {
        return pmdl->num_surfaces;
    } else {
        return 0;
    }
}

int TIKI_NumTags(dtiki_t *pmdl)
{
    return pmdl->m_boneList.NumChannels();
}

void TIKI_CalculateBounds(dtiki_t *pmdl, float scale, vec3_t mins, vec3_t maxs)
{
    int j;

    for (j = 0; j < 3; j++) {
        mins[j] = pmdl->a->mins[j] * pmdl->load_scale * scale;
        maxs[j] = pmdl->a->maxs[j] * pmdl->load_scale * scale;
    }
}

float TIKI_GlobalRadius(dtiki_t *pmdl)
{
    return pmdl->radius;
}


static AliasList_t *TIKI_AliasList(dtiki_t *pmdl, const char *function)
{
    if (!pmdl) {
        TIKI_Error("%s: no TIKI\n", function);
        return NULL;
    }

    return (AliasList_t *)pmdl->alias_list;
}

qboolean TIKI_Alias_Add(dtiki_t *pmdl, const char *alias, const char *name, const char *parameters)
{
    AliasList_t *list;

    if (!pmdl) {
        TIKI_Error("TIKI_Alias_Add: no TIKI\n");
        return qfalse;
    }

    list = (AliasList_t *)pmdl->alias_list;
    if (!list) {
        list = AliasList_New(pmdl->name);
        pmdl->alias_list = list;
    }

    return Alias_ListAdd(list, alias, name, parameters);
}

const char *TIKI_Alias_FindRandom(dtiki_t *pmdl, const char *alias)
{
    AliasList_t *list = TIKI_AliasList(pmdl, "TIKI_Alias_FindRandom");

    if (!list) {
        return NULL;
    }

    return Alias_ListFindRandom(list, alias);
}

void TIKI_Alias_Dump(dtiki_t *pmdl)
{
    AliasList_t *list = TIKI_AliasList(pmdl, "TIKI_Alias_Dump");

    if (list) {
        Alias_ListDump(list);
    }
}

void TIKI_Alias_Clear(dtiki_t *pmdl)
{
    AliasList_t *list = TIKI_AliasList(pmdl, "TIKI_Alias_Clear");

    if (list) {
        Alias_ListClear(list);
    }
}

const char *TIKI_Alias_FindDialog(dtiki_t *pmdl, const char *alias, int random, int entity_number)
{
    AliasList_t *list = TIKI_AliasList(pmdl, "TIKI_Alias_FindDialog");

    if (!list) {
        return NULL;
    }

    return Alias_ListFindDialog(list, alias, random, entity_number);
}

const char *TIKI_Alias_FindSpecificAnim(dtiki_t *pmdl, const char *name)
{
    AliasList_t *list = TIKI_AliasList(pmdl, "TIKI_Alias_FindSpecificAnim");

    if (!list) {
        return NULL;
    }

    return Alias_ListFindSpecificAnim(list, name);
}

qboolean TIKI_Alias_CheckLoopAnim(dtiki_t *pmdl, const char *name)
{
    AliasList_t *list = TIKI_AliasList(pmdl, "TIKI_Alias_CheckLoopAnim");

    if (!list) {
        return qfalse;
    }

    return Alias_ListCheckLoopAnim(list, name);
}

float TIKI_GetFadeDistMod(dtiki_t *pmdl)
{
    if (!pmdl) {
        return 1.0;
    }

    return pmdl->fade_dist_mod;
}
