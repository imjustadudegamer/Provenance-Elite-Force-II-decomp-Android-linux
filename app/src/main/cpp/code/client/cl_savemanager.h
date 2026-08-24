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
