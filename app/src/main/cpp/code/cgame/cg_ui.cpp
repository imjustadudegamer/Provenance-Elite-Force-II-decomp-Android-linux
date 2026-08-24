

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
