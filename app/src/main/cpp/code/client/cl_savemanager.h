#pragma once

#include "q_shared.h"
#include "../corepp/str.h"
#include "../corepp/container.h"

class LoadSaveGameManager
{
public:
    Container<savegamestruct_t *> m_saveGameList;  
    str                           m_currentSaveGame;  

public:
    LoadSaveGameManager();
    ~LoadSaveGameManager();

    void readSaveFiles(void);
    void clearSaveGameList(void);
    void loadDefaultDatabaseValues(void);

    savegamestruct_t *getSaveGame(const str &name);
    void              deleteSaveGame(const str &name);

    void saveGame(void);
    void saveGameWithName(const str &name);
    void overwriteSaveGame(const str &name);
    void autoSave(const str &levelName);

    void quickSave(void);
    void quickLoad(void);
    void loadLastSaveGame(void);

    bool isDiskSpaceDangerouslyLow(void) const;
    void displayLowSpaceError(void) const;
};

extern LoadSaveGameManager *s_loadSaveGameManager;

extern "C" {
void CreateLoadSaveGameManager(void);
void DeleteLoadSaveGameManager(void);
}

bool         Sys_DiskSpaceAvailable_AtLeastThreshold(void);
void         GetDescriptiveLevelName(const char *mapName, str *out);
const char  *LSM_PoolString(int offset);
