/*
===========================================================================
Reconstructed from the retail Star Trek: Voyager -- Elite Force II binaries.
The publisher released no source for this file.  This port asserts no copyright
in it and grants no rights to it: it remains the property of its respective
rights holders, and it is covered neither by the GNU GPL nor by the Ritual
Elite Force II SDK agreement.  See LICENSING.md.
===========================================================================
*/
// SPDX-License-Identifier: LicenseRef-EF2-Retail-Reconstruction
#include "client.h"
#include "cl_savemanager.h"
#include "../qcommon/gameplaymanager.h"
#include "../qcommon/stringresource.h"  

#define LSM_SAVEGAMELIST_EXT      "ssv"
#define LSM_DB_OBJECT             "LoadSaveGame"
#define LSM_PROP_SCREENSHOTSHADER "ScreenShotShader"
#define LSM_PROP_ENABLELOADSAVE   "EnableLoadSaveButton"
#define LSM_PROP_ENABLEDELETE     "EnableDeleteButton"
#define LSM_PROP_SELECTEDGAME     "SelectedGame"
#define LSM_PROP_EMPTY            "EMPTY"
#define LSM_CVAR_SAVEMENAME       "savegamename"
#define LSM_CMD_SAVEGAME          "savegame %s"
#define LSM_CMD_SAVEGAME_AUTO     "savegame SaveGame%010d"
#define LSM_NONESAVE_MSG          "loadLastSaveGame: There are no save games to load\n"
#define LSM_CMD_OKDIALOG          "showmenu OkDialog\n"
#define LSM_CMD_OKDIALOGTITLE                                                                     \
    "globalwidgetcommand OkDialogTitle title \"$$NoSaveGamesToLoad$$\"\n"
#define LSM_CVAR_QUICKSAVE_NAME "** Quick Save **"
#define LSM_CMD_QUICKSAVE       "savegame quicksave"
#define LSM_CMD_QUICKLOAD       "loadgame quicksave"
#define LSM_CMD_LOWDISK         "showmenu errorlowdiskspace\n"
#define LSM_DEFAULT_MAPSCREEN   "sysimg/2d/default-map-screen.tga"

LoadSaveGameManager *s_loadSaveGameManager = NULL;

static const char *lsm_pool = NULL;

const char *LSM_PoolString(int offset)
{
    return lsm_pool ? lsm_pool + offset : "";
}

static const char *lsm_ldFiles[] = {
    "global/leveldefs.txt",  "global/leveldefs2.txt", "global/leveldefs3.txt",
    "global/leveldefs4.txt", "global/leveldefs5.txt", "global/leveldefs6.txt",
    "global/leveldefs7.txt", "global/leveldefs8.txt", "global/leveldefs9.txt",
};

static bool lsm_NextToken(const char **p, char *tok, size_t toksize, bool *quoted = NULL)
{
    const char *s = *p;
    size_t      n = 0;

    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
        s++;
    }

    if (!*s) {
        *p = s;
        return false;
    }

    if (*s == '"') {
        if (quoted) {
            *quoted = true;
        }
        s++;
        while (*s && *s != '"') {
            if (n + 1 < toksize) {
                tok[n++] = *s;
            }
            s++;
        }
        if (*s == '"') {
            s++;
        }
    } else {
        if (quoted) {
            *quoted = false;
        }
        while (*s && *s != ' ' && *s != '\t' && *s != '\r' && *s != '\n') {
            if (n + 1 < toksize) {
                tok[n++] = *s;
            }
            s++;
        }
    }

    tok[n] = 0;
    *p = s;
    return true;
}

void GetDescriptiveLevelName(const char *mapName, str *out)
{
    int fi;

    if (!out) {
        return;
    }

    if (!mapName || !mapName[0]) {
        return;
    }

    for (fi = 0; fi < (int)(sizeof(lsm_ldFiles) / sizeof(lsm_ldFiles[0])); fi++) {
        char       *buf = NULL;
        long        len = FS_ReadFile(lsm_ldFiles[fi], (void **)&buf);
        const char *p;
        char        tok[256];
        bool        inEnv = false;
        bool        match = false;

        if (len <= 0 || !buf) {
            continue;
        }

        p = buf;
        while (lsm_NextToken(&p, tok, sizeof(tok))) {
            if (!stricmp(tok, "env")) {
                inEnv = true;
                continue;
            }
            if (!stricmp(tok, "level") || !stricmp(tok, "sublevel")) {
                if (lsm_NextToken(&p, tok, sizeof(tok))) {
                    if (!stricmp(tok, mapName)) {
                        bool hasdesc = false;
                        match = true;
                        /* only a quoted token is a description; a bare line is
                           followed by the next structural keyword, and retail
                           falls back to the map name there */
                        if (lsm_NextToken(&p, tok, sizeof(tok), &hasdesc) && hasdesc) {
                            *out = tok;
                        }
                        break;
                    }
                }
                inEnv = false;
                continue;
            }
            if (inEnv) {
                inEnv = false;
            }
        }

        FS_FreeFile(buf);

        if (match) {
            return;
        }
    }
}


void CreateLoadSaveGameManager(void)
{
    if (s_loadSaveGameManager) {
        return;
    }

    lsm_pool = LSM_DEFAULT_MAPSCREEN - 0; 
    lsm_pool = NULL;

    s_loadSaveGameManager = new LoadSaveGameManager();
    s_loadSaveGameManager->m_currentSaveGame = LSM_DEFAULT_MAPSCREEN;
}

void DeleteLoadSaveGameManager(void)
{
    int i;

    if (!s_loadSaveGameManager) {
        return;
    }

    for (i = 1; i <= s_loadSaveGameManager->m_saveGameList.NumObjects(); i++) {
        savegamestruct_t *rec = s_loadSaveGameManager->m_saveGameList.ObjectAt(i);

        if (rec) {
            delete rec;
        }
    }

    s_loadSaveGameManager->m_saveGameList.ClearObjectList();
    delete s_loadSaveGameManager;
    s_loadSaveGameManager = NULL;
}


LoadSaveGameManager::LoadSaveGameManager() {}

LoadSaveGameManager::~LoadSaveGameManager() {}

bool Sys_DiskSpaceAvailable_AtLeastThreshold(void)
{
    return true;
}

bool LoadSaveGameManager::isDiskSpaceDangerouslyLow(void) const
{
    return false;
}

void LoadSaveGameManager::displayLowSpaceError(void) const
{
    Cbuf_AddText(LSM_CMD_LOWDISK);
}

void LoadSaveGameManager::loadDefaultDatabaseValues(void)
{
    GameplayManager *gameplay = GameplayManager::getTheGameplayManager();
    str              object   = LSM_DB_OBJECT;
    str              prop;

    prop = LSM_PROP_SCREENSHOTSHADER;
    gameplay->setStringValue(object, prop, m_currentSaveGame, false);

    prop = LSM_PROP_ENABLELOADSAVE;
    gameplay->setFloatValue(object, prop, 0.0f, false);

    prop = LSM_PROP_ENABLEDELETE;
    gameplay->setFloatValue(object, prop, 0.0f, false);

    prop = LSM_PROP_SELECTEDGAME;
    gameplay->setStringValue(object, prop, m_currentSaveGame, false);

    prop = LSM_PROP_EMPTY;
    gameplay->setStringValue(object, prop, m_currentSaveGame, false);
}

void LoadSaveGameManager::clearSaveGameList(void)
{
    int i;

    for (i = 1; i <= m_saveGameList.NumObjects(); i++) {
        savegamestruct_t *rec = m_saveGameList.ObjectAt(i);

        if (rec) {
            delete rec;
        }
    }

    m_saveGameList.ClearObjectList();
}

void LoadSaveGameManager::readSaveFiles(void)
{
    const char *folder;
    char      **list;
    int         numfiles;
    int         i;

    clearSaveGameList();
    loadDefaultDatabaseValues();

    folder = Com_GetArchiveFileName(NULL, NULL, NULL, qfalse);
    list   = FS_ListFiles(folder, LSM_SAVEGAMELIST_EXT, qfalse, &numfiles);

    for (i = 0; list && i < numfiles; i++) {
        str              name = Com_GetArchiveFileName(NULL, list[i], LSM_SAVEGAMELIST_EXT, qfalse);
        fileHandle_t     f;
        savegamestruct_t *rec;

        FS_FOpenFileRead(name.c_str(), &f, qfalse, qtrue);
        if (!f) {
            continue;
        }

        rec = new savegamestruct_t;
        FS_Read(rec, sizeof(*rec), f);
        FS_FCloseFile(f);
        m_saveGameList.AddObject(rec);
    }

    if (list) {
        FS_FreeFileList(list);
    }
}

savegamestruct_t *LoadSaveGameManager::getSaveGame(const str &name)
{
    int i;

    for (i = 1; i <= m_saveGameList.NumObjects(); i++) {
        savegamestruct_t *rec = m_saveGameList.ObjectAt(i);

        if (rec && !strcmp(name.c_str(), rec->saveName)) {
            return rec;
        }
    }

    return NULL;
}

void LoadSaveGameManager::deleteSaveGame(const str &name)
{
    int i;

    for (i = 1; i <= m_saveGameList.NumObjects(); i++) {
        savegamestruct_t *rec = m_saveGameList.ObjectAt(i);

        if (rec && !stricmp(name.c_str(), rec->saveName)) {
            delete rec;
            m_saveGameList.RemoveObjectAt(i);
            Com_WipeSavegame((char *)name.c_str(), qfalse);
        }
    }
}

void LoadSaveGameManager::saveGame(void)
{
    time_t now;
    char   buf[64];

    if (isDiskSpaceDangerouslyLow()) {
        displayLowSpaceError();
        return;
    }

    time(&now);
    Com_sprintf(buf, sizeof(buf), LSM_CMD_SAVEGAME_AUTO, (int)now);
    Cbuf_ExecuteText(EXEC_NOW, buf);
}

void LoadSaveGameManager::saveGameWithName(const str &name)
{
    time_t now;
    char   buf[64];
    char   convertedName[256];

    if (isDiskSpaceDangerouslyLow()) {
        displayLowSpaceError();
        return;
    }

    convertStringResourceIds(name.c_str(), convertedName, sizeof(convertedName));
    Cvar_Set(LSM_CVAR_SAVEMENAME, convertedName);

    if (isDiskSpaceDangerouslyLow()) {
        displayLowSpaceError();
        return;
    }

    time(&now);
    Com_sprintf(buf, sizeof(buf), LSM_CMD_SAVEGAME_AUTO, (int)now);
    Cbuf_ExecuteText(EXEC_NOW, buf);
}

void LoadSaveGameManager::overwriteSaveGame(const str &name)
{
    savegamestruct_t *rec;
    char              buf[64];

    if (isDiskSpaceDangerouslyLow()) {
        displayLowSpaceError();
        return;
    }

    rec = getSaveGame(name);
    if (!rec) {
        return;
    }

    Cvar_Set(LSM_CVAR_SAVEMENAME, rec->comment);
    Com_sprintf(buf, sizeof(buf), LSM_CMD_SAVEGAME, name.c_str());
    Cbuf_ExecuteText(EXEC_NOW, buf);
}

// DEPENDENCY NOT REPRODUCED: the level description retail passes to
void LoadSaveGameManager::autoSave(const str &levelName)
{
    str  description;
    time_t now;
    char   buf[64];

    if (isDiskSpaceDangerouslyLow()) {
        displayLowSpaceError();
        return;
    }

    GetDescriptiveLevelName(levelName.c_str(), &description);
    Cvar_Set(LSM_CVAR_SAVEMENAME, description.c_str());

    if (isDiskSpaceDangerouslyLow()) {
        displayLowSpaceError();
        return;
    }

    time(&now);
    Com_sprintf(buf, sizeof(buf), LSM_CMD_SAVEGAME_AUTO, (int)now);
    Cbuf_ExecuteText(EXEC_NOW, buf);
}

void LoadSaveGameManager::quickSave(void)
{
    if (isDiskSpaceDangerouslyLow()) {
        displayLowSpaceError();
        return;
    }

    Cvar_Set(LSM_CVAR_SAVEMENAME, LSM_CVAR_QUICKSAVE_NAME);
    Cbuf_ExecuteText(EXEC_NOW, LSM_CMD_QUICKSAVE);
}

void LoadSaveGameManager::quickLoad(void)
{
    Cbuf_ExecuteText(EXEC_NOW, LSM_CMD_QUICKLOAD);
}

void LoadSaveGameManager::loadLastSaveGame(void)
{
    int               i;
    savegamestruct_t *best = NULL;
    char              buf[64];

    readSaveFiles();

    for (i = 1; i <= m_saveGameList.NumObjects(); i++) {
        savegamestruct_t *rec = m_saveGameList.ObjectAt(i);

        if (!best || rec->time > best->time) {
            best = rec;
        }
    }

    if (best) {
        Com_sprintf(buf, sizeof(buf), LSM_CMD_SAVEGAME, best->saveName);
        Cbuf_ExecuteText(EXEC_NOW, buf);
    } else {
        Com_Printf(LSM_NONESAVE_MSG);
        Cbuf_ExecuteText(EXEC_NOW, LSM_CMD_OKDIALOG);
        Cbuf_ExecuteText(EXEC_NOW, LSM_CMD_OKDIALOGTITLE);
    }
}

/* The server's save writer labels a slot from here and is compiled as C, so the
   lookup crosses as a plain buffer fill. */
extern "C" void LSM_DescriptiveLevelName(const char *mapName, char *out, int size)
{
    str description;

    if (!out || size <= 0) {
        return;
    }

    out[0] = 0;
    GetDescriptiveLevelName(mapName, &description);
    Q_strncpyz(out, description.c_str(), size);
}
