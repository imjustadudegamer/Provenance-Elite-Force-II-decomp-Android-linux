/*
===========================================================================
Copyright (C) 2008-2024 the OpenMoHAA team
Copyright (C) 1999-2005 Id Software, Inc.

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
// SPDX-License-Identifier: GPL-2.0-or-later


#include "cg_local.h"

void CG_ParseServerinfo(void)
{
    const char *info;
    const char *mapname;
    char        map[MAX_QPATH];
    char       *spawnpos;

    info             = CG_ConfigString(CS_SERVERINFO);
    cgs.gametype     = atoi(Info_ValueForKey(info, "g_gametype"));
    cgs.mp_flags     = atoi(Info_ValueForKey(info, "mp_flags"));
    cgs.teamflags    = atoi(Info_ValueForKey(info, "teamflags"));
    cgs.pointlimit   = atoi(Info_ValueForKey(info, "mp_pointlimit"));
    cgs.timelimit    = atoi(Info_ValueForKey(info, "mp_timelimit"));
    cgs.maxclients   = atoi(Info_ValueForKey(info, "sv_maxclients"));

    cgi.Cvar_Set("cg_gametype", Info_ValueForKey(info, "g_gametype"));
    cgi.Cvar_Set("cg_mpgametype", Info_ValueForKey(info, "mp_gametype"));
    cgi.Cvar_Set("cg_pointlimit", Info_ValueForKey(info, "mp_pointlimit"));
    cgi.Cvar_Set("cg_timelimit", Info_ValueForKey(info, "mp_timelimit"));
    cgi.Cvar_Set("cg_maxclients", Info_ValueForKey(info, "sv_maxclients"));
    mapname = Info_ValueForKey(info, "mapname");

    spawnpos = strchr(mapname, '$');
    if (spawnpos) {
        Q_strncpyz(map, mapname, spawnpos - mapname + 1);
    } else {
        Q_strncpyz(map, mapname, sizeof(map));
    }

    Com_sprintf(cgs.mapname, sizeof(cgs.mapname), "maps/%s.bsp", map);
    Com_sprintf(cgs.rawMapName, sizeof(cgs.rawMapName), "%s", map);  

    // "hud_score") do not exist anywhere in the retail cgame binary.  Retail's only
}

static void CG_ConfigStringModified(int num)
{
    cgi.GetGameState(&cgs.gameState);

    CG_ProcessConfigString(num);
}

static void CG_ServerCommand(void)
{
    const char *cmd;

    cmd = cgi.Argv(0);

    if (!cmd[0]) {
        return;
    }

    if (!strcmp(cmd, "cs")) {
        CG_ConfigStringModified(atoi(cgi.Argv(1)));
        return;
    }



    if (!strcmp(cmd, "print") || !strcmp(cmd, "hudprint") || !strcmp(cmd, "hudsay")) {
        cgi.Printf("%s", cgi.Argv(1));
        if (strcmp(cmd, "print")) {
            CG_HudPrint_f();
        }
        return;
    }


    if (!strcmp(cmd, "stufftext")) {
        char *stuffText = cgi.Argv(1);

        cgi.Cmd_Stuff(stuffText);
        cgi.Cmd_Stuff("\n");
        return;
    }

    if (!strcmp(cmd, "map_restart")) {
        if (cg.snap) {
            cgi.Printf("CG_MapRestart\n");
        }

        CG_Shutdown();
        CG_Init(&cgi, cgs.processedSnapshotNum, cgs.serverCommandSequence);
        return;
    }

    if (!Q_stricmp(cmd, "scores")) {
        cgi.UI_ParseScores();
        return;
    }

    if (!strcmp(cmd, "status")) {
        char buff[1024];

        Com_sprintf(buff, sizeof(buff), "globalwidgetcommand dm_status title \"%s\"\n", cgi.Argv(1));
        cgi.Cmd_Execute(0, buff);
        return;
    }


    // Retail @0x00037C24 calls cgi slot [3] 0x00C -- Com_WPrintf, not the plain
    cgi.Com_WPrintf("Unknown client game command: %s\n", cmd);
}

void CG_ExecuteNewServerCommands(int latestSequence)
{
    while (cgs.serverCommandSequence < latestSequence) {
        if (cgi.GetServerCommand(++cgs.serverCommandSequence)) {
            CG_ServerCommand();
        }
    }
}
