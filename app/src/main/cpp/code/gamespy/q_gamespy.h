

#pragma once

#include "../qcommon/q_shared.h"

typedef struct {
    const char *host;
    int queryport;
    int hbport;
} master_entry_t;

void Com_InitGameSpy(void);
qboolean Com_RefreshGameSpyMasters(void);

unsigned int Com_GetNumMasterEntries(void);
void Com_GetMasterEntry(int index, master_entry_t* entry);
const char *Com_GetMasterHost(void);
int Com_GetMasterQueryPort(void);
int Com_GetMasterHeartbeatPort(void);
