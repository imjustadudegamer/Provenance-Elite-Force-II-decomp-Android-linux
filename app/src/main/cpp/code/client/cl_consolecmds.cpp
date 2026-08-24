

#include "client.h"
#include "../corepp/listener.h"

void CL_ReplaceShader_f(void);

void CL_EventList_f(void)
{
    const char *mask = NULL;

    if (Cmd_Argc() > 1) {
        mask = Cmd_Argv(1);
    }

    Event::ListCommands(mask);
}

void CL_EventHelp_f(void)
{
    const char *mask = NULL;

    if (Cmd_Argc() > 1) {
        mask = Cmd_Argv(1);
    }

    Event::ListDocumentation(mask, qfalse);
}

void CL_DumpEventHelp_f(void)
{
    const char *mask = NULL;

    if (Cmd_Argc() > 1) {
        mask = Cmd_Argv(1);
    }

    Event::ListDocumentation(mask, qtrue);
}

void CL_PendingEvents_f(void)
{
    const char *mask = NULL;

    if (Cmd_Argc() > 1) {
        mask = Cmd_Argv(1);
    }

    Event::PendingEvents(mask);
}

void CL_ClassList_f(void)
{
    listAllClasses();
}

void CL_ClassTree_f(void)
{
    if (Cmd_Argc() < 2) {
        Com_Printf("Syntax: cl_classtree [classname].\n");
        return;
    }

    listInheritanceOrder(Cmd_Argv(1));
}

void CL_ClassEvents_f(void)
{
    if (Cmd_Argc() < 2) {
        Com_Printf("Syntax: cl_classevents [classname].\n");
        return;
    }

    ClassEvents(Cmd_Argv(1), qfalse);
}

void CL_DumpClassEvents_f(void)
{
    if (Cmd_Argc() < 2) {
        Com_Printf("Syntax: cl_dumpclassevents [classname].\n");
        return;
    }

    ClassEvents(Cmd_Argv(1), qtrue);
}

void CL_DumpAllClasses_f(void)
{
    DumpAllClasses();
}

void CL_InitConsoleCommands(void)
{
    Cmd_AddCommand("cl_eventlist", CL_EventList_f);
    Cmd_AddCommand("cl_eventhelp", CL_EventHelp_f);
    Cmd_AddCommand("cl_dumpevents", CL_DumpEventHelp_f);
    Cmd_AddCommand("cl_pendingevents", CL_PendingEvents_f);
    Cmd_AddCommand("cl_classlist", CL_ClassList_f);
    Cmd_AddCommand("cl_classtree", CL_ClassTree_f);
    Cmd_AddCommand("cl_classevents", CL_ClassEvents_f);
    Cmd_AddCommand("cl_dumpclassevents", CL_DumpClassEvents_f);
    Cmd_AddCommand("cl_dumpallclasses", CL_DumpAllClasses_f);

    Cmd_AddCommand("cl_reloadshader", CL_ReplaceShader_f);
}
