
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define TIKI_BONE_CACHE 8000
#define MAX_SKELBONES   TIKI_BONE_CACHE
#define MAX_SKELMORPH 12800

#ifndef TIKI_BONE_ARENAS
#define TIKI_BONE_ARENAS 1
#endif

    extern int             TIKI_Skel_Bones_Cursor[TIKI_BONE_ARENAS];
    extern int             skeletorMorphCacheIndex;
    extern skelBoneCache_t TIKI_Skel_Bones_Arena[TIKI_BONE_ARENAS][MAX_SKELBONES];
    extern int             skeletorMorphCache[];

#define TIKI_Skel_Bones         ( TIKI_Skel_Bones_Arena[ 0 ] )
#define TIKI_Skel_Bones_Index   ( TIKI_Skel_Bones_Cursor[ 0 ] )

    void TIKI_Reset_Caches(void);

    void TIKI_Reset_Caches_Arena(int arena);

#ifdef __cplusplus
}
#endif
