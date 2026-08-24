

#include "../corepp/tiki.h"
#include <assert.h>

int             TIKI_Skel_Bones_Cursor[TIKI_BONE_ARENAS];
int             skeletorMorphCacheIndex;
skelBoneCache_t TIKI_Skel_Bones_Arena[TIKI_BONE_ARENAS][MAX_SKELBONES];
int             skeletorMorphCache[MAX_SKELMORPH];

static_assert(MAX_SKELBONES == TIKI_BONE_CACHE, "MAX_SKELBONES must be retail's TIKI_BONE_CACHE");
static_assert(
    sizeof(TIKI_Skel_Bones_Arena[0]) / sizeof(TIKI_Skel_Bones_Arena[0][0]) == TIKI_BONE_CACHE,
    "each bone arena must hold exactly retail's TIKI_BONE_CACHE rows"
);

void TIKI_Reset_Caches_Arena(int arena)
{
    assert(arena >= 0 && arena < TIKI_BONE_ARENAS);

    TIKI_Skel_Bones_Cursor[arena] = 0;
    skeletorMorphCacheIndex       = 0;
}

void TIKI_Reset_Caches()
{
    TIKI_Reset_Caches_Arena(0);
}
