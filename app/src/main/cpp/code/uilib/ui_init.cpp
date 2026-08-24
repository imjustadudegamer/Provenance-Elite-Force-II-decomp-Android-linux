
#include "ui_local.h"

uidef_t     uid;
uiexport_t  uie;
uiimport_t  uii;
uiGlobals_t ui;

void UIE_AddFileToList(const char *name)
{
    ui.fileList.AddTail(name);
}

void UIE_ResolutionChanged(void)
{
    UIRect2D frame(0, 0, uid.vidWidth, uid.vidHeight);
    uWinMan.setFrame(frame);
    uWinMan.RefreshShadersFromList();
}

void UI_GetMouseState(int *mouseX, int *mouseY, int *flags)
{
    if (mouseX) {
        *mouseX = uid.mouseX;
    }

    if (mouseY) {
        *mouseY = uid.mouseY;
    }

    if (flags) {
        *flags = uid.mouseFlags;
    }
}

int UI_GetCvarInt(const char *name, int def)
{
    cvar_t *cvar = uii.Cvar_Find(name);

    if (!cvar) {
        return def;
    }

    if (cvar->latchedString) {
        return atoi(cvar->latchedString);
    }

    return cvar->integer;
}

float UI_GetCvarFloat(const char *name, float def)
{
    cvar_t *cvar = uii.Cvar_Find(name);

    if (!cvar) {
        return def;
    }

    // atoi, not atof: `bl atoi` @0x0004EAA4 followed by the CodeWarrior
    if (cvar->latchedString) {
        return (float)atoi(cvar->latchedString);
    }

    return cvar->value;
}

const char *UI_GetCvarString(const char *cvar, char *def)
{
    return uii.Cvar_GetString(cvar, def);
}

cvar_t *UI_FindCvar(const char *cvar)
{
    return uii.Cvar_Find(cvar);
}

void UI_SetCvarInt(const char *cvar, int value)
{
    char s[16];

    Com_sprintf(s, sizeof(s), "%d", value);
    uii.Cvar_Set(cvar, s);
}

void UI_SetCvarFloat(const char *cvar, float value)
{
    char s[16];

    Com_sprintf(s, sizeof(s), "%f", value);
    uii.Cvar_Set(cvar, s);
}

void UI_ListFiles(const char *filespec)
{
    ui.fileList.RemoveAllItems();
    uii.File_ListFiles(filespec);
}

const char *UI_ConfigString(int index)
{
    return uii.GetConfigstring(index);
}

void UI_Init(void)
{
    UIRect2D frame(0, 0, uid.vidWidth, uid.vidHeight);

    ui.globalFont = new UIFont;
    uWinMan.Init(frame, "tahoma-8");

    UColor color(0, 0, 0, 0);
    uWinMan.setBackgroundColor(color, WIDGET_STATE_NORMAL, true);
}

void UI_Shutdown(void)
{
    if (ui.globalFont) {
        delete ui.globalFont;
        ui.globalFont = NULL;
    }

    uWinMan.Shutdown();
}

void UI_InitExports()
{
    uie.AddFileToList    = UIE_AddFileToList;
    uie.ResolutionChange = UIE_ResolutionChanged;
    uie.Init             = UI_Init;
    uie.Shutdown         = UI_Shutdown;

}
