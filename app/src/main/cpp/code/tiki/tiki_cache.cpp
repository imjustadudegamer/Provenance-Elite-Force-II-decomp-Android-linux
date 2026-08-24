

#include "q_shared.h"
#include "qcommon.h"
#include "../qcommon/alias.h"
#include "../skeletor/skeletor.h"
#include "../corepp/mem_blockalloc.h"
#include "../corepp/con_set.h"
#include "../corepp/str.h"
#include "tiki_files.h"
#include "tiki_skel.h"
#include "../corepp/tiki_main.h"

struct pchar {
    const char *m_Value;

    pchar() { m_Value = NULL; }

    pchar(const char *value) { m_Value = value; }

    friend bool operator==(const pchar& l, const pchar& r) { return !strcmp(l.m_Value, r.m_Value); }
};

con_map<pchar, dtikianim_t *> *tikianimcache;
con_map<pchar, dtiki_t *>     *tikicache;
static skeletor_c             *skel_entity_cache[TIKI_MAX_ENTITY_CACHE];

typedef struct {
    char     name[96];     
    int      size;          
    dtiki_t *tiki;          
} tikidef_t;

#define TIKI_MAX_DEFS 384   

static tikidef_t s_tikis[TIKI_MAX_DEFS];
static int       s_numTiKis;

template<>
int HashCode<pchar>(const pchar& key)
{
    return HashCode<const char *>(key.m_Value);
}

dtiki_t *TIKI_GetTiki(int handle)
{
    if (handle < 0 || handle > s_numTiKis) {
        TIKI_Error("TIKI_GetTiki: handle %d out of range\n", handle);
        return NULL;
    }

    if (!s_tikis[handle].tiki) {
        TIKI_Error("TIKI_GetTiki: NULL pointer for %s\n", s_tikis[handle].name);
        return NULL;
    }

    return s_tikis[handle].tiki;
}

qboolean TIKI_TikiHandleValid(int handle, const char *name)
{
    if (handle < 0 || handle > s_numTiKis) {
        return qfalse;
    }

    return s_tikis[handle].tiki && !Q_stricmp(s_tikis[handle].name, name);
}

int TIKI_HandleForTiki(const dtiki_t *tiki)
{
    int i;

    if (!tiki) {
        return -1;
    }

    for (i = 0; i <= s_numTiKis; i++) {
        if (s_tikis[i].tiki == tiki) {
            return i;
        }
    }

    return -1;
}

static int TIKI_DefCacheAlloc(const char *name)
{
    int i;

    for (i = 0; i < TIKI_MAX_DEFS; i++) {
        if (s_tikis[i].tiki) {
            continue;
        }

        Q_strncpyz(s_tikis[i].name, name, sizeof(s_tikis[i].name));
        s_tikis[i].size = 0;
        if (i > s_numTiKis) {
            s_numTiKis = i;
        }
        return i;
    }

    TIKI_Error("TIKI_RegisterTiki: No free spots open in def cache for %s\n", name);
    return -1;
}

dtikianim_t *TIKI_FindTikiAnim(const char *path)
{
    char filename[1024];

    if (tikianimcache) {
        dtikianim_t **t;

        t = tikianimcache->find(filename);
        if (t) {
            return *t;
        }
    }

    return NULL;
}

dtiki_t *TIKI_FindTiki(const char *path)
{
    char filename[1024];

    if (tikicache) {
        dtiki_t **t;

        Q_strncpyz(filename, path, sizeof(filename));
        FS_CanonicalFilename(filename);

        t = tikicache->find(filename);
        if (t) {
            return *t;
        }
    }

    return NULL;
}

dtikianim_t *TIKI_RegisterTikiAnimFlags(const char *path, qboolean use)
{
    dtikianim_t *tiki;
    char         filename[1024];

    Q_strncpyz(filename, path, sizeof(filename));
    FS_CanonicalFilename(filename);

    if (tikianimcache) {
        dtikianim_t **t;

        t = tikianimcache->find(filename);
        if (t) {
            return *t;
        }
    } else {
        tikianimcache = new con_map<pchar, dtikianim_t *>;
    }

    tiki = TIKI_LoadTikiAnim(filename);
    if (tiki) {
        (*tikianimcache)[tiki->name] = tiki;
    }

    return tiki;
}

dtikianim_t *TIKI_RegisterTikiAnim(const char *path)
{
    return TIKI_RegisterTikiAnimFlags(path, qfalse);
}

dtiki_t *TIKI_RegisterTikiFlags(const char *path, qboolean use)
{
    dtiki_t     *tiki     = NULL;
    dtikianim_t *tikianim = NULL;
    const char  *next_path;
    str          key;
    str          value;
    const char  *name;
    char         filename[1024];
    char         full_filename[1024];

    full_filename[0] = 0;

    name = path;
    for (next_path = strstr(name, "|"); next_path; next_path = strstr(name, "|")) {
        key                   = name;
        key[next_path - name] = 0;

        name      = next_path + 1;
        next_path = strstr(name, "|");
        if (!next_path) {
            break;
        }

        value                   = name;
        value[next_path - name] = 0;
        name = next_path + 1;

        strcat(full_filename, key.c_str());
        strcat(full_filename, "|");
        strcat(full_filename, value.c_str());
        strcat(full_filename, "|");
    }

    Q_strncpyz(filename, name, sizeof(filename));
    FS_CanonicalFilename(filename);
    Q_strcat(full_filename, sizeof(full_filename), filename);

    if (!tikicache) {
        tikicache = new con_map<pchar, dtiki_t *>;
    } else {
        dtiki_t **t;

        t = tikicache->find(full_filename);
        if (t) {
            return *t;
        }
    }

    tikianim = TIKI_RegisterTikiAnimFlags(filename, use);
    if (tikianim) {
        size_t defsize = 0;

        tiki = TIKI_LoadTikiModel(tikianim, full_filename, &defsize);
        if (tiki) {
            int row;

            (*tikicache)[tiki->name] = tiki;

            row = TIKI_HandleForTiki(tiki);
            if (row < 0) {
                row = TIKI_DefCacheAlloc(full_filename);
                if (row >= 0) {
                    s_tikis[row].tiki = tiki;
                }
            }

            if (row >= 0) {
                s_tikis[row].size = (int)defsize;
            }
        }
    }

    return tiki;
}

int TIKI_RegisterTiki(const char *path)
{
    dtiki_t *tiki;
    int      i;

    for (i = 0; i < TIKI_MAX_DEFS; i++) {
        if (s_tikis[i].tiki && !Q_stricmp(s_tikis[i].name, path)) {
            if (s_tikis[i].tiki->a) {
                int j;

                for (j = 0; j < s_tikis[i].tiki->a->num_anims; j++) {
                    SkeletorCacheGetData(s_tikis[i].tiki->a->m_aliases[j]);
                }
            }

            return i;
        }
    }

    tiki = TIKI_RegisterTikiFlags(path, qfalse);
    if (!tiki) {
        return -1;
    }

    return TIKI_HandleForTiki(tiki);
}

int TIKI_ReloadTiki(const char *name)
{
    dtiki_t *old;
    dtiki_t *tiki;
    int      handle;

    for (handle = 0; handle < TIKI_MAX_DEFS; handle++) {
        if (s_tikis[handle].tiki && !Q_stricmp(s_tikis[handle].name, name)) {
            break;
        }
    }

    if (handle >= TIKI_MAX_DEFS) {
        return -1;
    }

    old = s_tikis[handle].tiki;

    if (tikicache) {
        tikicache->remove(pchar(old->name));
    }

    if (old->a && tikianimcache) {
        tikianimcache->remove(pchar(old->a->name));
    }

    TIKI_FreeTiki(handle);

    tiki = TIKI_RegisterTikiFlags(name, qfalse);
    if (!tiki) {
        return -1;
    }

    return TIKI_HandleForTiki(tiki);
}

void TIKI_FreeAll()
{
    dtiki_t     **entry;
    dtikianim_t **entryanim;
    dtiki_t      *tiki;
    dtikianim_t  *tikianim;
    int           i;

    if (tikicache) {
        con_map_enum<pchar, dtiki_t *> en = *tikicache;

        for (entry = en.NextValue(); entry != NULL; entry = en.NextValue()) {
            skeletor_c *skeletor;

            tiki     = *entry;
            skeletor = (skeletor_c *)tiki->skeletor;
            if (skeletor) {
                delete skeletor;
            }

            tiki->m_boneList.CleanUpChannels();

            TIKI_Free(tiki);
        }

        tikicache->clear();
    }

    if (tikianimcache) {
        con_map_enum<pchar, dtikianim_t *> en = *tikianimcache;

        for (entryanim = en.NextValue(); entryanim != NULL; entryanim = en.NextValue()) {
            tikianim = *entryanim;

            TIKI_RemoveTiki(tikianim);

            if (tikianim->m_aliases) {
                TIKI_Free(tikianim->m_aliases);
                tikianim->m_aliases = NULL;
                tikianim->num_anims = 0;
            }
            if (tikianim->tanMesh) {
                int tanAnimIdx;

                for (tanAnimIdx = 0; tanAnimIdx < tikianim->num_anims; tanAnimIdx++) {
                    const int tanRow = tikianim->tanMesh[tanAnimIdx];

                    if (tanRow >= 0 && tanRow <= cache_maxskel && skelcache[tanRow].skel) {
                        if (skelcache[tanRow].numuses > 0) {
                            skelcache[tanRow].numuses--;
                        }
                        if (skelcache[tanRow].numuses <= 0) {
                            TIKI_FreeSkel(tanRow);
                        }
                    }
                }

                TIKI_Free(tikianim->tanMesh);
                tikianim->tanMesh = NULL;
            }

            TIKI_Free(tikianim);
        }

        tikianimcache->clear();
    }

    tiki_loading = true;

    memset(s_tikis, 0, sizeof(s_tikis));
    s_numTiKis = 0;

    for (i = 0; i < cache_maxskel; i++) {
        if (skelcache[i].skel) {
            TIKI_FreeSkel(i);
        }
    }
}

void TIKI_FreeLevelData()
{
    if (!tikicache) {
        return;
    }

    con_map_enum<pchar, dtiki_t *> en = *tikicache;

    for (dtiki_t **entry = en.NextValue(); entry; entry = en.NextValue()) {
        dtiki_t *tiki = *entry;
        if (!tiki) {
            continue;
        }

        if (tiki->alias_list) {
            Alias_ListClearActors((AliasList_t *)tiki->alias_list);
            Alias_ListSort((AliasList_t *)tiki->alias_list);
        }

        if (tiki->a && tiki->a->alias_list) {
            Alias_ListClearActors((AliasList_t *)tiki->a->alias_list);
            Alias_ListSort((AliasList_t *)tiki->a->alias_list);
        }
    }
}

static qboolean tiki_started;

void *TIKI_GetSkeletor(dtiki_t *tiki, int entnum)
{
    skeletor_c *skel;
    int         i;
    int         index;

    if (entnum == ENTITYNUM_NONE) {
        if (!tiki->skeletor) {
            tiki->skeletor = new skeletor_c(tiki);
        }
        skel = (skeletor_c *)tiki->skeletor;
    } else {
        for (i = 0; i < TIKI_MAX_ENTITY_CACHE_PER_ENT; i++) {
            index = ((entnum % TIKI_MAX_ENTITIES) * TIKI_MAX_ENTITY_CACHE_PER_ENT) + i;

            skel = skel_entity_cache[index];
            if (!skel) {
                break;
            }

            if (skel->m_Tiki == tiki) {
                return skel;
            }
        }

        if (i == TIKI_MAX_ENTITY_CACHE_PER_ENT) {
            i = 0;
        }

        index = ((entnum % TIKI_MAX_ENTITIES) * TIKI_MAX_ENTITY_CACHE_PER_ENT) + i;
        skel  = skel_entity_cache[index];

        if (skel) {
            delete skel;
        }

        skel                     = new skeletor_c(tiki);
        skel_entity_cache[index] = skel;
    }

    return skel;
}

static void TIKI_DeleteSkeletor(int entnum)
{
    skeletor_c *skel;
    int         i;

    if (entnum == ENTITYNUM_NONE) {
        return;
    }

    for (i = 0; i < TIKI_MAX_ENTITY_CACHE_PER_ENT; i++) {
        skel = skel_entity_cache[entnum * TIKI_MAX_ENTITY_CACHE_PER_ENT + i];
        if (skel) {
            delete skel;
        }
    }
}

void TIKI_Begin(void)
{
    int i;

    for (i = 0; i < TIKI_MAX_ENTITY_CACHE; i++) {
        skel_entity_cache[i] = 0;
    }

    tiki_started = true;
}

void TIKI_End(void)
{
    int i;

    for (i = 0; i < MAX_GENTITIES; i++) {
        TIKI_DeleteSkeletor(i);
    }

    tiki_started = false;
}

void TIKI_FinishLoad(void)
{
    if (!tiki_loading) {
        return;
    }

    tiki_loading = false;
    SkeletorCacheCleanCache();
}

void TIKI_FreeImages(void)
{
    dtikisurface_t                *dsurf;
    dtiki_t                       *tiki;
    int                            j, k;
    dtiki_t                      **entry;
    con_map_enum<pchar, dtiki_t *> en;

    if (!tikicache) {
        return;
    }

    en = *tikicache;
    for (entry = en.NextValue(); entry != NULL; entry = en.NextValue()) {
        tiki = *entry;
        for (k = 0; k < tiki->num_surfaces; k++) {
            dsurf = &tiki->surfaces[k];

            for (j = 0; j < dsurf->numskins; j++) {
                dsurf->hShader[j] = 0;
            }
        }
    }
}

void TIKI_TikiAnimList_f(void)
{
    con_map_enum<pchar, dtikianim_t *> en;
    dtikianim_t                      **entry;

    Com_Printf("\ntikianimlist:\n");
    if (tikicache) {
        en = *tikianimcache;
        for (entry = en.NextValue(); entry != NULL; entry = en.NextValue()) {
            Com_Printf("%s\n", (*entry)->name);
        }
    }
}

void TIKI_TikiList_f(void)
{
    con_map_enum<pchar, dtiki_t *> en;
    dtiki_t                      **entry;

    Com_Printf("\ntikilist:\n");
    if (tikicache) {
        en = *tikicache;
        for (entry = en.NextValue(); entry != NULL; entry = en.NextValue()) {
            Com_Printf("%s\n", (*entry)->name);
        }
    }
}

void TIKI_FreeTiki(int handle)
{
    dtiki_t *tiki;
    int      i;

    if (handle < 0 || handle > s_numTiKis) {
        TIKI_Error("TIKI_FreeTiki: handle %d out of range\n", handle);
        return;
    }

    tiki = s_tikis[handle].tiki;
    if (!tiki) {
        TIKI_Error("TIKI_FreeTiki: NULL pointer for %s\n", s_tikis[handle].name);
        return;
    }

    if (tiki->alias_list) {
        Alias_ListDelete((AliasList_t *)tiki->alias_list);
        tiki->alias_list = NULL;
    }

    if (tiki->a) {
        TIKI_RemoveTiki(tiki->a);
    }

    for (i = 0; i < tiki->numMeshes; i++) {
        int mesh = tiki->mesh[i];

        if (mesh < 0 || mesh > cache_maxskel) {
            continue;
        }

        if (skelcache[mesh].numuses > 0) {
            skelcache[mesh].numuses--;
        }

        if (skelcache[mesh].numuses <= 0 && skelcache[mesh].skel) {
            TIKI_FreeSkel(mesh);
        }
    }

    {
        skeletor_c *skeletor = (skeletor_c *)tiki->skeletor;

        if (skeletor) {
            delete skeletor;
            tiki->skeletor = NULL;
        }
    }

    // replacement skeleton is deliberately not a mesh (retail's replacesurface never
    if (tiki->replacedSurfaces) {
        for (i = 0; i < tiki->numReplacedSurfaces; i++) {
            dtikiReplacedSurface_t *row = &tiki->replacedSurfaces[i];

            if (row->skelIndex != -1 && row->skelIndex <= cache_maxskel) {
                if (skelcache[row->skelIndex].numuses > 0) {
                    skelcache[row->skelIndex].numuses--;
                }

                if (skelcache[row->skelIndex].numuses <= 0 && skelcache[row->skelIndex].skel) {
                    TIKI_FreeSkel(row->skelIndex);
                }
            }

            if (row->boneMappingIndex != -1) {
                TIKI_ReleaseBoneMapping(row->boneMappingIndex);
            }

            if (row->morphMappingIndex != -1) {
                TIKI_ReleaseMorphMapping(row->morphMappingIndex);
            }
        }
    }

    tiki->m_boneList.CleanUpChannels();

    if (tiki->boneMappings) {
        for (i = 0; i < tiki->a->num_anims; i++) {
            if (tiki->boneMappings[i] >= 0) {
                TIKI_FreeAnimBoneMapping(tiki->boneMappings[i]);
            }
        }

        TIKI_Free(tiki->boneMappings);
        tiki->boneMappings = NULL;
    }

    s_tikis[handle].tiki = NULL;
    s_numTiKis--;
}

static int TIKI_DefCacheFind(const char *name)
{
    int remaining;
    int i;

    remaining = s_numTiKis;

    for (i = 0; i < TIKI_MAX_DEFS; i++) {
        if (s_tikis[i].tiki) {
            if (!Q_stricmp(s_tikis[i].name, name)) {
                return i;
            }

            if (remaining-- == 0) {
                break;
            }
        }
    }

    return -1;
}

void TIKI_CacheAllInDir(const char *dir)
{
    char **list;
    char   name[64];
    char   path[268];
    int    count;
    int    i;

    Com_DPrintf("Converting %s directory.\n", dir);

    list = FS_ListFiles(dir, ".tik", qfalse, &count);
    if (!list || !count) {
        if (list) {
            FS_FreeFileList(list);
        }

        Com_Printf("WARNING: no tiki files found\n");
        return;
    }

    tiki_loading = true;
    Com_Printf("%i tiki files need to be cached\n", count);

    for (i = 0; i < count; i++) {
        int row;

        Com_sprintf(name, sizeof(name), "%s/%s", dir, list[i]);
        Com_Printf("...cacheing %s %4i of %4i\n", name, i, count);

        row = TIKI_DefCacheFind(name);
        if (row >= 0) {
            int          size = s_tikis[row].size;
            void        *tiki = s_tikis[row].tiki;
            fileHandle_t h;

            COM_StripExtension(name, path, sizeof(path));
            strcat(path, ".cik");

            h = FS_FOpenFileWrite_HomeData(path);
            if (h) {
                int one = 1;

                FS_Write(&one, 4, h);
                FS_Write(tiki, size, h);
                FS_FCloseFile(h);
            }
        } else {
            int handle = TIKI_RegisterTiki(name);

            if (handle >= 0) {
                TIKI_FreeTiki(handle);
            }
        }

        TIKI_UpdateScreen();
    }

    FS_FreeFileList(list);
    tiki_loading = false;
}

int TIKI_CacheAll(void)
{
    TikiScript  ts;
    const char *token;
    char        path[64];

    TIKI_CacheAllInDir("models");

    if (!ts.LoadFile("models/modeldirs.txt", false)) {
        return 0;
    }

    while (ts.TokenAvailable(true)) {
        token = ts.GetToken(true);
        if (!token || !*token) {
            break;
        }

        if (Q_stricmpn("models", token, 6)) {
            strcpy(path, "models/");
            strcat(path, token);
        } else {
            strcpy(path, token);
        }

        TIKI_CacheAllInDir(path);
    }

    ts.Close();
    return 1;
}

// TIKI_GetTiki reports, which is why retail's own accessors do not repeat them
static dtiki_t *TIKI_DefCacheGetTiki(int handle)
{
    if (handle < 0 || handle > s_numTiKis) {
        TIKI_Error("TIKI_GetTiki: handle %d out of range\n", handle);
        return NULL;
    }

    if (!s_tikis[handle].tiki) {
        TIKI_Error("TIKI_GetTiki: NULL pointer for %s\n", s_tikis[handle].name);
        return NULL;
    }

    return s_tikis[handle].tiki;
}

void *TIKI_Alias_GetList(int handle)
{
    dtiki_t *tiki = TIKI_DefCacheGetTiki(handle);

    if (!tiki) {
        return NULL;
    }

    return tiki->alias_list;
}

const char *TIKI_Alias_Find(int handle, const char *alias)
{
    dtiki_t *tiki = TIKI_DefCacheGetTiki(handle);

    if (!tiki) {
        return NULL;
    }

    if (!tiki->alias_list) {
        return NULL;
    }

    return Alias_ListFind((AliasList_t *)tiki->alias_list, alias);
}

qboolean TIKI_Alias_UpdateDialog(
    int handle, const char *alias, int number_of_times_played, byte been_played_this_loop, int last_time_played
)
{
    dtiki_t *tiki = TIKI_DefCacheGetTiki(handle);

    if (!tiki) {
        return qfalse;
    }

    if (!tiki->alias_list) {
        return qfalse;
    }

    Alias_ListUpdateDialog((AliasList_t *)tiki->alias_list, alias, number_of_times_played, been_played_this_loop, last_time_played);
    return qtrue;
}

qboolean TIKI_Alias_AddActorDialog(
    int handle, const char *alias, int actor_number, int number_of_times_played, byte been_played_this_loop,
    int last_time_played
)
{
    dtiki_t *tiki = TIKI_DefCacheGetTiki(handle);

    if (!tiki) {
        return qfalse;
    }

    if (!tiki->alias_list) {
        return qfalse;
    }

    Alias_ListAddActorDialog(
        (AliasList_t *)tiki->alias_list, alias, actor_number, number_of_times_played, been_played_this_loop,
        last_time_played
    );
    return qtrue;
}

int TIKI_NumMorphs(int handle)
{
    dtiki_t          *tiki;
    skelHeaderGame_t *skel;

    tiki = TIKI_DefCacheGetTiki(handle);
    if (!tiki) {
        return 0;
    }

    skel = TIKI_GetBaseSkel(tiki);
    if (!skel) {
        const int row = (tiki->skelIndex >= 0) ? tiki->skelIndex : (tiki->numMeshes > 0 ? tiki->mesh[0] : 0);

        TIKI_Error("TIKI_GetSkel: NULL pointer for %s\n", skelcache[row].path);
        return 0;
    }

    return skel->numMorphTargets;
}

void TIKI_ModelList(void)
{
    int totalAnimSize;
    int totalTikiSize;
    int i;

    Com_Printf("==============\n");
    Com_Printf("= Animations =\n");
    Com_Printf("==============\n");

    totalAnimSize = 0;
    for (i = 0; i < SkeletorCacheNumEntries(); i++) {
        if (!SkeletorCacheEntryLoaded(i)) {
            continue;
        }

        Com_Printf("A %6d : %s\n", SkeletorCacheEntrySize(i), SkeletorCacheEntryPath(i));
        totalAnimSize += SkeletorCacheEntrySize(i);
    }

    Com_Printf("Total Anim: %i\n\n", totalAnimSize);

    Com_Printf("=========\n");
    Com_Printf("= Tikis =\n");
    Com_Printf("=========\n");

    totalTikiSize = 0;
    for (i = 0; i < TIKI_MAX_DEFS; i++) {
        if (!s_tikis[i].tiki) {
            continue;
        }

        Com_Printf("T %6d : %s\n", s_tikis[i].size, s_tikis[i].name);
        totalTikiSize += s_tikis[i].size;
    }

    Com_Printf("Total Tikis: %i\n\n", totalTikiSize);
    Com_Printf("Total Resident: %i\n", totalTikiSize + totalAnimSize);
    Com_Printf("Num Anims: %i\n", SkeletorCacheNumEntries());
    Com_Printf("Num Tikis: %i\n", s_numTiKis);
}
