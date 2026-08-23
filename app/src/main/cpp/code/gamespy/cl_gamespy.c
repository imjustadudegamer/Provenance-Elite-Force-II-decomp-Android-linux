
#include "cl_gamespy.h"
#include "q_gamespy.h"

unsigned int ServerListGetNumMasters(void)
{
    return Com_GetNumMasterEntries();
}

const char *ServerListGetHost(int index)
{
    master_entry_t entry;
    Com_GetMasterEntry(index, &entry);

    return entry.host;
}

int ServerListGetMsPort(int index)
{
    master_entry_t entry;
    Com_GetMasterEntry(index, &entry);

    return entry.queryport;
}

void CL_RestartGamespy_f(void) {}
