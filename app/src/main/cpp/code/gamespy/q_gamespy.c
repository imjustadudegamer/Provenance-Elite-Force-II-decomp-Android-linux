/*
===========================================================================
Copyright (C) 2025 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "q_gamespy.h"
#include "../qcommon/qcommon.h"
#include "cl_gamespy.h"
// (sv_gamespy.c): EF2 retail has no gamespy query responder in the engine.

#define MAX_MASTERS           8
#define MASTER_DEFAULT_MSPORT 28900
#define MASTER_DEFAULT_HBPORT 27900

cvar_t *com_master_host[MAX_MASTERS];
cvar_t *com_master_msport[MAX_MASTERS];
cvar_t *com_master_hbport[MAX_MASTERS];

master_entry_t entries[MAX_MASTERS];
int            num_entries = 0;


static qboolean ShouldRefreshMasters(void)
{
    int i;

    for (i = 0; i < MAX_MASTERS; i++) {
        if (com_master_host[i] && com_master_host[i]->latchedString) {
            return qtrue;
        }
        if (com_master_msport[i] && com_master_msport[i]->latchedString) {
            return qtrue;
        }
        if (com_master_hbport[i] && com_master_hbport[i]->latchedString) {
            return qtrue;
        }
    }

    return qfalse;
}

static void CreateMasterVar(int index, const char *host, int msport, int hbport)
{
    assert(index < MAX_MASTERS);

    com_master_host[index]   = Cvar_Get(va("com_master%d_host", index), host, CVAR_LATCH | CVAR_TEMP);
    com_master_msport[index] = Cvar_Get(va("com_master%d_msport", index), va("%d", msport), CVAR_LATCH | CVAR_TEMP);
    com_master_hbport[index] = Cvar_Get(va("com_master%d_hbport", index), va("%d", hbport), CVAR_LATCH | CVAR_TEMP);

    com_master_host[index]->flags &= ~CVAR_ARCHIVE;
    com_master_msport[index]->flags &= ~CVAR_ARCHIVE;
    com_master_hbport[index]->flags &= ~CVAR_ARCHIVE;
}

qboolean Com_RefreshGameSpyMasters(void)
{
    int      msIndex = 0;
    int      i;
    qboolean shouldRestart;

    shouldRestart = ShouldRefreshMasters();

    CreateMasterVar(msIndex++, "master.333networks.com", MASTER_DEFAULT_MSPORT, MASTER_DEFAULT_HBPORT);
    CreateMasterVar(msIndex++, "master.frag-net.com", MASTER_DEFAULT_MSPORT, MASTER_DEFAULT_HBPORT);
    CreateMasterVar(msIndex++, "master.errorist.eu", MASTER_DEFAULT_MSPORT, MASTER_DEFAULT_HBPORT);
    CreateMasterVar(msIndex++, "master-au.unrealarchive.org", MASTER_DEFAULT_MSPORT, MASTER_DEFAULT_HBPORT);
    CreateMasterVar(msIndex++, "master.noccer.de", MASTER_DEFAULT_MSPORT, MASTER_DEFAULT_HBPORT);

    for (; msIndex < MAX_MASTERS; msIndex++) {
        CreateMasterVar(msIndex, "", MASTER_DEFAULT_MSPORT, MASTER_DEFAULT_HBPORT);
    }

    num_entries = 0;

    for (i = 0; i < MAX_MASTERS; i++) {
        master_entry_t *entry = &entries[num_entries];

        if (com_master_host[i]->string && com_master_host[i]->string[0]) {
            entry->host      = com_master_host[i]->string;
            entry->queryport = com_master_msport[i]->integer;
            entry->hbport    = com_master_hbport[i]->integer;
            num_entries++;
        }
    }

    return shouldRestart;
}

void Com_InitGameSpy(void)
{
    Com_RefreshGameSpyMasters();

    // W31-SWEEP r2 correction.  The earlier note here claimed "Retail EF2 carries no
    // REFUTED by the shipped bytes: the retail Mac engine's string table carries the QR2
    // at PEF file 0x264ef1 (the leading `2` is the encoded-table length byte, so the
    // literal is "master.gamespy.com"), the literal "GameSpy" at 0x2533bc, and a real
    // retail GameSpy C++ body UpdateServerListCallBack__FP25GServerListImplementationiPvPvPv
    // @0x001B51D0 in the 5241-function corpus.  So the SDK subtrees under code/gamespy/**
    // are shipped middleware, not port-only -- and this glue's com_master* 333networks
    // override set IS a port-only delta (its host literals occur in no retail image; the
    // retail server master list is sv_master1..sv_master5, engine string table 0x2679ae,
    // default "ef2master.activision.com" at 0x2679ba, reproduced in sv_init.c:1061).
    // Restoring retail's own GameSpy master defaults needs the retail init site re-derived;
}

unsigned int Com_GetNumMasterEntries(void)
{
    return num_entries;
}

void Com_GetMasterEntry(int index, master_entry_t *entry)
{
    if (index >= num_entries) {
        entry->host      = NULL;
        entry->hbport    = 0;
        entry->queryport = 0;
        return;
    }

    entry->host      = com_master_host[index]->string;
    entry->queryport = com_master_msport[index]->integer;
    entry->hbport    = com_master_hbport[index]->integer;
}

const char *Com_GetMasterHost(void)
{
    return com_master_host[0]->string;
}

int Com_GetMasterQueryPort(void)
{
    return com_master_msport[0]->integer;
}

int Com_GetMasterHeartbeatPort(void)
{
    return com_master_hbport[0]->integer;
}
