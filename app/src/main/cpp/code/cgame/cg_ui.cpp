/*
===========================================================================
Copyright (C) 2023 the OpenMoHAA team

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
#include "../corepp/str.h"
#include "../client/keycodes.h"

void CG_MessageMode_f(void)
{
    if (!cgs.gametype) {
        return;
    }

    cgi.Cmd_Execute(EXEC_NOW, "globalwidgetcommand dm_console activate");
    cgi.Cmd_Execute(EXEC_NOW, "globalwidgetcommand dm_console messagemode all");
}

void CG_TeamMessageMode_f(void)
{
    if (!cgs.gametype) {
        return;
    }

    cgi.Cmd_Execute(EXEC_NOW, "globalwidgetcommand dm_console activate");
    cgi.Cmd_Execute(EXEC_NOW, "globalwidgetcommand dm_console messagemode team");
}

void CG_HudPrint_f(void)
{
    char buf[1024];

    if (cgi.Cvar_Get("cg_gametype", "0", 0)->integer) {
        Com_sprintf(buf, sizeof(buf), "globalwidgetcommand dm_console print \"%s\"\n", cgi.Argv(1));
    } else {
        Com_sprintf(buf, sizeof(buf), "globalwidgetcommand sp_console print \"%s\"\n", cgi.Argv(1));
    }

    cgi.Cmd_Execute(EXEC_NOW, buf);
}
