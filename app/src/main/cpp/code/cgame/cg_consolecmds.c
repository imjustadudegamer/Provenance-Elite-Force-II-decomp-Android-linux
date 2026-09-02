/*
===========================================================================
Copyright (C) 2008-2024 the OpenMoHAA team

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

void CG_TargetCommand_f(void);

static void CG_SizeUp_f(void)
{
    cgi.Cvar_Set("viewsize", va("%i", (int)(cg_viewsize->integer + 10)));
}

static void CG_SizeDown_f(void)
{
    cgi.Cvar_Set("viewsize", va("%i", (int)(cg_viewsize->integer - 10)));
}

static void CG_Viewpos_f(void)
{
    cgi.Printf(
        "(%i %i %i) : %i\n",
        (int)cg.refdef.vieworg[0],
        (int)cg.refdef.vieworg[1],
        (int)cg.refdef.vieworg[2],
        (int)cg.refdefViewAngles[YAW]
    );
}

static void CG_FlushModels_f(void)
{
    memset(cgs.model_draw, -1, sizeof(cgs.model_draw));

    CG_FlushCommandManager();

    cgi.TIKI_FlushAll();

    // TIKI_FlushAll drops every tiki, so a handle recorded by the walk memo is
    // about to be handed to a different tiki.  Drop the memo with them.
    CG_ResetTikiWalkCache();

    for (int i = 32; i < 512; i++) {
        CG_ProcessConfigString(i);
    }
}

static void CG_JoinArenaUI_f(void)
{
    int i;
    int numarenas;

    cgi.Cmd_Execute(0, "forcemenu joinarena\n");

    numarenas = atoi(CG_ConfigString(1488));

    if (numarenas > 0) {
        cgi.Cmd_Execute(0, "globalwidgetcommand dm_arenalist deleteallitems\n");
    }

    for (i = 1; i <= numarenas; i++) {
        cgi.Cmd_Execute(
            0,
            va("globalwidgetcommand dm_arenalist addconfigstringitem %d \"join_arena %d\"\n", i + 1489, i));
    }
}

static void CG_JoinTeamUI(int teamnum)
{
    char buf[1024];
    int  i, j;
    int  numteams;

    cgi.Cmd_Execute(0, "forcemenu joinarena\n");

    numteams = atoi(CG_ConfigString(1521));

    cgi.Cmd_Execute(0, "globalwidgetcommand dm_teamlist deleteallitems\n");

    // Retail CG_JoinTeamUI @0x00043040 does NOT return here: after the "No Teams In
    if (teamnum != 0 && numteams == 0) {
        cgi.Cmd_Execute(0, "globalwidgetcommand dm_teamlist additem \"No Teams In Arena\"");
    }

    for (i = 1; i <= numteams; i++) {
        int teamnum_cs;
        int teamid;

        cgi.Cmd_TokenizeString(CG_ConfigString(1522 + i));

        teamnum_cs = atoi(cgi.Argv(0));
        teamid     = atoi(cgi.Argv(1));

        buf[0] = 0;

        for (j = 2; j < cgi.Argc(); j++) {
            if (strlen(buf) + strlen(cgi.Argv(j)) > 1023) {
                break;
            }

            Q_strcat(buf, sizeof(buf), cgi.Argv(j));
            Q_strcat(buf, sizeof(buf), " ");
        }

        if (teamnum == teamnum_cs) {
            cgi.Cmd_Execute(
                0,
                va("globalwidgetcommand dm_teamlist additem \"%s\" \"join_team %d; popmenu 0\"\n", buf, teamid));
        }
    }

    if (teamnum != 0) {
        cgi.Cmd_Execute(0, "globalwidgetcommand dm_teamlist additem \"\"\n");
        cgi.Cmd_Execute(
            0, "globalwidgetcommand dm_teamlist additem \"Create New Team\" \"create_team; popmenu 0\"\n");
    }

    cgi.Cmd_Execute(0, va("globalwidgetcommand dm_refresh stuffcommand \"cg_refresharena_ui %d\"\n", teamnum));
}

static void CG_RefreshArenaUI_f(void)
{
    int teamnum = atoi(cgi.Argv(1));

    CG_JoinArenaUI_f();

    CG_JoinTeamUI(teamnum);
}

static void CG_InitArenaUI_f(void)
{
    cgi.Cmd_Execute(0, "ui_addhud arenahud\n");
}

typedef struct {
    char *cmd;
    void (*function)(void);
} consoleCommand_t;

// voting handling -- are absent from retail and are not registered here either.
static consoleCommand_t commands[] = {
    {"viewpos",              &CG_Viewpos_f            },
    {"sizeup",               &CG_SizeUp_f             },
    {"sizedown",             &CG_SizeDown_f           },
    {"cg_eventlist",         &CG_EventList_f          },
    {"cg_eventhelp",         &CG_EventHelp_f          },
    {"cg_dumpevents",        &CG_DumpEventHelp_f      },
    {"cg_pendingevents",     &CG_PendingEvents_f      },
    {"cg_classlist",         &CG_ClassList_f          },
    {"cg_classtree",         &CG_ClassTree_f          },
    {"cg_classevents",       &CG_ClassEvents_f        },
    {"cg_dumpclassevents",   &CG_DumpClassEvents_f    },
    {"cg_dumpallclasses",    &CG_DumpAllClasses_f     },
    {"testemitter",          &CG_TestEmitter_f        },
    {"reloademitter",        &CG_ReloadEmitter_f      },
    {"dumpemitter",          &CG_DumpEmitter_f        },
    {"flushtikis",           &CG_FlushModels_f        },
    {"cg_joinarena_ui",      &CG_JoinArenaUI_f        },
    {"cg_refresharena_ui",   &CG_RefreshArenaUI_f     },
    {"cg_initarena_ui",      &CG_InitArenaUI_f        },
    {"messagemode",          &CG_MessageMode_f        },
    {"teammessagemode",      &CG_TeamMessageMode_f    },
    {"nextemitter",          &CG_NextEmitter_f        },
    {"prevemitter",          &CG_PrevEmitter_f        },
};

qboolean CG_ConsoleCommand(void)
{
    const char *cmd;
    int         i;

    cmd = cgi.Argv(0);

    for (i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if (!Q_stricmp(cmd, commands[i].cmd)) {
            commands[i].function();
            return qtrue;
        }
    }

    return qfalse;
}

void CG_InitConsoleCommands(void)
{
    int i;

    for (i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        cgi.AddCommand(commands[i].cmd);
    }

}

