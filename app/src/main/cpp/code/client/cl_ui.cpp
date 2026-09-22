/*
===========================================================================
Copyright (C) 2024 the OpenMoHAA team

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

#include "client.h"

#include "../server/server.h"
#include "../renderercommon/tr_public.h"
#include "../corepp/tiki.h"
#include "../qcommon/q_version.h"

#include "cl_ui.h"
#include "cl_savemanager.h"

#define PROBE_GET(name, def, flags) Cvar_Get(name, def, flags)
#define PROBE_PRINT                 Com_Printf
#include "../qcommon/qa_probe.h"

#include "../qcommon/qa_ui.h"

#include "../uilib/limit_uiprobe.h"

#ifdef __ANDROID__
#include <SDL.h>
#endif

#include "../corepp/listener.h"
#include "../corepp/script.h"
#include "../qcommon/gameplaymanager.h"
#include "../qcommon/stringresource.h"
#include "../qcommon/gameplaymanager.h"

#include <chrono>

static qboolean             ui_hud = qtrue;
static UIFloatingConsole   *fakk_console;
static Miniconsole          *mini_console;             
static View3D              *view3d;
static Menu                *mainmenu;
static Menu                *hud_health;       
static Menu                *hud_boss;         
static Menu                *hud_levelExit;     
static Menu                *hud_targetName;    
static Menu                *printhud;         
static Menu                *itemTextHud;      
static Menu                *poweruphud;       
static Menu                *radarhud;         
static Menu                *missionfailure;   
static Menu                *missionsuccess;   
static Menu                *headhud;          
static Menu                *headtexthud;      
static Menu                *texthud;          
static Menu                *specialmovetimer; 
static Menu                *pointshud;        
qboolean                    server_loading;
static qboolean             server_loading_waiting;
static int                  server_loading_stage;
typedef struct {
    str *text;
    int  color;
} intermediate_console_text_t;

static Container<intermediate_console_text_t *> s_intermediateconsole;
static qboolean             hudListHidden;
static int                  lastScoreboardTime;
Menu                       *ui_pLoadingMenu;

static Menu                *weaphud[6];
static const char *const    weaphudKeys[6]  = {"Plasma", "Fed", "Idryll", "Phaser", "Disruptor", "Enterprise"};
static const char *const    weaphudMenus[6] = {
    "weaphudPlasma",
    "weaphudFed",
    "weaphudIdryll",
    "weaphudPhaser",
    "weaphudDisruptor",
    "weaphudEnterprise"
};

static_media_t ui_static_materials;
cvar_t        *ui_drawcoords;         
cvar_t        *ui_changeresolution;   
cvar_t        *ui_minicon;            
cvar_t        *ui_consoleposition;    
static cvar_t *ui_inventoryfile;      
cvar_t        *ui_console;            
cvar_t        *ui_newvidmode;         
static cvar_t *ui_dialogdisplaymode;  
static cvar_t *ui_radarviewmode;      
static cvar_t *ui_radarmode;          
static cvar_t *ui_serverrefresh;      
static cvar_t *ui_mainMenuModel;      
static cvar_t *cg_targetedPlayerName; 
static cvar_t *ui_hudScale;
// "$$" (pool +1032); retail never Cvar_Get()s it -- Cvar_Set creates it.

static char          server_mapname[64];
// retail off_229EBC -- the engine's 64-byte Scoreboard object.  Owned by

unsigned char        UIListCtrlItem[8];

[[maybe_unused]] static const float maxWidthRes  = 1920;
[[maybe_unused]] static const float maxHeightRes = 1080;

inventory_t              client_inv;
bind_t                   client_bind;
static str               ui_sCurrentLoadingMenu;
static str               ui_sLevelLoadingScreen;
static Container<Menu *> hudList;

static void UI_SyncHudScale( void );

const UColor UWhiteChatMessageColor(0.75, 0.75, 0.75);
const UColor URedChatMessageColor(1.0, 0.333, 0.333);
const UColor UGreenChatMessageColor(0.0, 1.0, 0.333, 1.0);

class ConsoleHider : public Listener
{
public:
    CLASS_PROTOTYPE(ConsoleHider);

    void HideConsole(Event *ev);
    void DeadConsole(Event *ev);
};

CLASS_DECLARATION(Listener, ConsoleHider, NULL) {
    {&UIFloatingWindow::W_ClosePressed, &ConsoleHider::HideConsole},
    {&W_Destroyed,                      &ConsoleHider::DeadConsole},
    {NULL,                              NULL                      }
};

void ConsoleHider::HideConsole(Event *ev)
{
    UI_CloseConsole();
}

void ConsoleHider::DeadConsole(Event *ev)
{
    if (fakk_console && !fakk_console->IsDying()) {
        delete fakk_console;
        fakk_console = NULL;
    }
}

static ConsoleHider s_consolehider;

static void ConsoleCommandHandler(const char *txt)
{
    Cbuf_AddText(txt);
}

static UIRect2D getDefaultConsoleRectangle(void)
{
    float    f[4];
    int      i;
    UIRect2D rect;

    if (sscanf(ui_consoleposition->string, "%f %f %f %f", &f[0], &f[1], &f[2], &f[3]) == 4) {
        for (i = 0; i < 4; i++) {
            f[i] = floor(f[i]);
        }

        rect.pos.x       = f[0];
        rect.pos.y       = f[1];
        rect.size.width  = f[2];
        rect.size.height = f[3];

        if (rect.pos.x < 0.0f) {
            rect.pos.x = 0.0f;
        }
        if (rect.pos.y < 0.0f) {
            rect.pos.y = 0.0f;
        }
        if (rect.size.width < 50.0f) {
            rect.size.width = 50.0f;
        }
        if (rect.size.height < 50.0f) {
            rect.size.height = 50.0f;
        }
        if (rect.pos.x > (float)uid.vidWidth) {
            rect.pos.x = (float)uid.vidWidth;
        }
        if (rect.pos.y > (float)uid.vidHeight) {
            rect.pos.y = (float)uid.vidHeight;
        }
    } else {
        rect.pos.x       = 25.0;
        rect.pos.y       = 25.0;
        rect.size.width  = (uid.vidWidth - 50);
        rect.size.height = (uid.vidHeight / 2);
    }

    return rect;
}


void LAN_LoadCachedServers(void)
{
    int          size;
    fileHandle_t fileIn;
    cls.numglobalservers = cls.nummplayerservers = cls.numfavoriteservers = 0;
    cls.numGlobalServerAddresses                                          = 0;
    if (FS_BaseDir_FOpenFileRead("servercache.dat", &fileIn)) {
        FS_Read(&cls.numglobalservers, sizeof(int), fileIn);
        FS_Read(&cls.nummplayerservers, sizeof(int), fileIn);
        FS_Read(&cls.numfavoriteservers, sizeof(int), fileIn);
        FS_Read(&size, sizeof(int), fileIn);
        if (size == sizeof(cls.globalServers) + sizeof(cls.favoriteServers) + sizeof(cls.mplayerServers)) {
            FS_Read(&cls.globalServers, sizeof(cls.globalServers), fileIn);
            FS_Read(&cls.mplayerServers, sizeof(cls.mplayerServers), fileIn);
            FS_Read(&cls.favoriteServers, sizeof(cls.favoriteServers), fileIn);
        } else {
            cls.numglobalservers = cls.nummplayerservers = cls.numfavoriteservers = 0;
            cls.numGlobalServerAddresses                                          = 0;
        }
        FS_FCloseFile(fileIn);
    }
}

void LAN_SaveServersToCache(void)
{
    int          size;
    fileHandle_t fileOut = FS_BaseDir_FOpenFileWrite_HomeData("servercache.dat");
    FS_Write(&cls.numglobalservers, sizeof(int), fileOut);
    FS_Write(&cls.nummplayerservers, sizeof(int), fileOut);
    FS_Write(&cls.numfavoriteservers, sizeof(int), fileOut);
    size = sizeof(cls.globalServers) + sizeof(cls.favoriteServers) + sizeof(cls.mplayerServers);
    FS_Write(&size, sizeof(int), fileOut);
    FS_Write(&cls.globalServers, sizeof(cls.globalServers), fileOut);
    FS_Write(&cls.mplayerServers, sizeof(cls.mplayerServers), fileOut);
    FS_Write(&cls.favoriteServers, sizeof(cls.favoriteServers), fileOut);
    FS_FCloseFile(fileOut);
}

qboolean LAN_UpdateVisiblePings(int source)
{
    return CL_UpdateVisiblePings_f(source);
}

int LAN_GetServerStatus(const char *serverAddress, char *serverStatus, int maxLen)
{
    return CL_ServerStatus(serverAddress, serverStatus, maxLen);
}

qboolean UI_usesUniqueCDKey(void)
{
    return qfalse;
}




static void UI_SetupConsoles(void)
{
    bool consoleEnabled;

    consoleEnabled = (ui_console->integer || developer->integer) != 0;

    if (consoleEnabled && !fakk_console) {
        str consoleName;

        consoleName  = "Elite Force II"; 
        consoleName += " Console";       

        fakk_console = new UIFloatingConsole;
        fakk_console->Create(NULL, getDefaultConsoleRectangle(), consoleName, UColor(0.1, 0.1, 0.1, 1.0), UWhite);
        fakk_console->setConsoleHandler(ConsoleCommandHandler);
        fakk_console->setConsoleBackground(UBlack, 0.8f);
        fakk_console->setShow(false);
        fakk_console->Connect(&s_consolehider, UIFloatingWindow::W_ClosePressed, UIFloatingWindow::W_ClosePressed);
        fakk_console->Connect(&s_consolehider, W_Destroyed, W_Destroyed);
        fakk_console->setDepth(0.01f);
    } else if (!consoleEnabled && fakk_console) {
        fakk_console->SendSignal(W_Destroyed);
        fakk_console = NULL;
    }

    if (ui_minicon->integer) {
        if (!mini_console) {
            mini_console = new Miniconsole;
            mini_console->Create(UISize2D(500, 100), UWhite, UColor(0.0, 0.5, 1.0, 1.0), 0.2f);
            mini_console->setBorderStyle(border_none);
            mini_console->setDepth(0.01f);
        }
    } else if (mini_console) {
        delete mini_console;
        mini_console = NULL;
    }
}




void UI_GetHighResolutionScale(vec2_t scale)
{
    if (uid.bHighResScaling) {
        scale[0] = uid.scaleRes[0];
        scale[1] = uid.scaleRes[1];
    } else {
        scale[0] = scale[1] = 1.0;
    }
}

void UI_ReloadStrings(void)
{
    menuManager.reloadStrings();
}

void UI_ShowHudList(void)
{
    for (int i = 1; i <= hudList.NumObjects(); i++) {
        hudList.ObjectAt(i)->ForceShow();
    }
}

void UI_HideHudList(void)
{
    for (int i = 1; i <= hudList.NumObjects(); i++) {
        hudList.ObjectAt(i)->ForceHide();
    }
}

void UI_DisplayHudList(void)
{
    for (int i = 1; i <= hudList.NumObjects(); i++) {
        Menu     *pMenu   = hudList.ObjectAt(i);
        UIWidget *pWidget = pMenu->GetContainerWidget();
        float     hudScale = pWidget ? pWidget->getHudScale() : 1.0f;

        pMenu->ForceShow();

        if (hudScale > 1.0f) {
            UIWidget::HudScalePush(hudScale);
        }

        pWidget->Display(uWinMan.getFrame(), 1.0);

        if (hudScale > 1.0f) {
            UIWidget::HudScalePop();
        }
    }
}

static const UColor *ConsolePrintColor(int color)
{
    switch (color) {
    case MESSAGE_YELLOW:
        return &UHudColor;
    case MESSAGE_CHAT_WHITE:
        return &UWhiteChatMessageColor;
    case MESSAGE_WHITE:
        return &UWhite;
    case MESSAGE_CHAT_RED:
        return &URedChatMessageColor;
    case MESSAGE_CHAT_GREEN:
        return &UGreenChatMessageColor;
    default:
        return NULL;
    }
}

void UI_PrintConsole(const char *msg, int color)
{
    const UColor *pColor = NULL;
    const char   *pszString;
    int           msgColor = color;

    pszString = msg;

    if (*pszString > 0 && (unsigned char)*pszString < MESSAGE_MAX) {
        msgColor = *pszString;

        switch (*pszString) {
        case MESSAGE_YELLOW:
            pColor = &UHudColor;
            break;
        case MESSAGE_CHAT_WHITE:
            pColor = &UWhiteChatMessageColor;
            break;
        case MESSAGE_WHITE:
            pColor = &UWhite;
            break;
        case MESSAGE_CHAT_RED:
            pColor = &URedChatMessageColor;
            break;
        case MESSAGE_CHAT_GREEN:
            pColor = &UGreenChatMessageColor;
            break;
        }

        pszString++;
    }

    if (*pszString == '\a') {
        pszString++;
        if (mini_console) {
            mini_console->Print(pszString, msgColor);
            return;
        }
    }

    if (fakk_console) {
        fakk_console->AddText(pszString, pColor);

        if (mini_console) {
            mini_console->Print(pszString, msgColor);
        }
    } else {
        intermediate_console_text_t *record;

        record       = new intermediate_console_text_t;
        record->text = new str(pszString);
        record->color = color;
        s_intermediateconsole.AddObject(record);
    }
}


qboolean UI_MenuActive(void)
{
    return menuManager.CurrentMenu() != NULL;
}

qboolean UI_IsHudActive(void)
{
    for (int i = 1; i <= hudList.NumObjects(); i++) {
        Menu *menu = hudList.ObjectAt(i);

        if (menu && menu->isNoEscape()) {
            return qtrue;
        }
    }

    return qfalse;
}

int UI_HudCount(void)
{
    return hudList.NumObjects();
}

const char *UI_HudNames(char *buf, int buflen)
{
    int i;

    buf[0] = 0;
    for (i = 1; i <= hudList.NumObjects() && buflen > 1; i++) {
        Menu    *hud = hudList.ObjectAt(i);
        const char *nm = hud ? hud->m_name.c_str() : "(null)";

        Q_strcat(buf, buflen, va("%s'%s'", i > 1 ? " " : "", nm ? nm : "?"));
    }

    if (!buf[0]) {
        return "(empty)";
    }

    return buf;
}

void UI_FocusMenuIfExists(void)
{
    Menu *currentMenu;
    int   r_mode;
    bool  noMenu;

    currentMenu = menuManager.CurrentMenu();
    r_mode      = Cvar_VariableValue("r_mode");

    noMenu = (currentMenu == NULL);
    if (UI_IsHudActive()) {
        noMenu = false;
    }

    if (noMenu) {
        UI_ActivateView3D();
        IN_MouseOff();

        Cvar_SetValue("ui_newvidmode", -1);
        return;
    }

    if (currentMenu && !currentMenu->CheckNoMouse()) {
        IN_MouseOn();
    }

    if (view3d->IsActive()) {
        uWinMan.DeactivateCurrentControl();
    }

    if (currentMenu) {
        // focus pass must perform exactly what retail's ActivateMenu does -- one
        currentMenu->ActivateMenu();

        if (PROBE_On() && view3d) {
            int    painttotal = uWinMan.GetChildCount();
            int    paintv3d = 0;
            int    paintmenu = 0;
            int    painttie = 0;   
            int    painttieNC = 0; 
            int    paintcon_ = 0;  
            UIWidget *paintcon = currentMenu->GetContainerWidget();

            for (int painti = 1; painti <= painttotal; painti++) {
                UIWidget *paintw = uWinMan.GetChild(painti);

                if (paintw == view3d) {
                    paintv3d = painti;
                }
                if (paintcon && paintw == paintcon) {
                    paintmenu = painti;
                }
                if (!paintw->isSubclassOf(UIWidgetContainer) && paintw != view3d) {
                    paintcon_++;
                }
                // the shipped data does not produce.  tieNC splits them by class: retail's
                if (paintw->getDepth() >= 1.0f) {
                    painttie++;
                    if (!paintw->isSubclassOf(UIWidgetContainer)) {
                        painttieNC++;
                    }
                }
            }

            char paintfront[512];
            paintfront[0] = 0;
            for (int paintf = 1; paintf <= 8 && paintf <= painttotal; paintf++) {
                UIWidget *paintw = uWinMan.GetChild(paintf);
                const char *paintnm = paintw ? paintw->getName() : "(null)";

                Q_strcat(paintfront, sizeof(paintfront), va("%d:%s/%.2f ", paintf, paintnm ? paintnm : "?", (double)(paintw ? paintw->getDepth() : -1.0f)));
            }

            char painttieNames[2048];
            painttieNames[0] = 0;
            for (int paintt = 1; paintt <= painttotal && strlen(painttieNames) < 1800; paintt++) {
                UIWidget *paintw = uWinMan.GetChild(paintt);

                if (paintw && paintw->getDepth() >= 1.0f) {
                    const char *paintnm = paintw->getName();

                    Q_strcat(painttieNames, sizeof(painttieNames), va("%s/%.2f ", paintnm ? paintnm : "?", (double)paintw->getDepth()));
                }
            }

            static int uivk_prevv3d;
            static int uivk_prevmenu;
            int        uivk_flip = 0;

            if (uivk_prevmenu && paintmenu && paintv3d) {
                int wasOver = (uivk_prevmenu < uivk_prevv3d);
                int isOver  = (paintmenu < paintv3d);

                uivk_flip = (wasOver != isOver) ? 1 : 0;
            }
            uivk_prevv3d = paintv3d;
            uivk_prevmenu = paintmenu;

            int uivk_gap = (paintmenu && paintv3d) ? (paintv3d - paintmenu) : 0;

            PAINT_PROBE("paint:order",
                       "menu='%s' children=%d containerIndex=%d depth=%.3f visible=%d | "
                       "view3dIndex=%d depth=%.3f visible=%d cls.state=%d tie=%d tieNC=%d nonurc=%d flip=%d prev3d=%d gap=%d | %s| front=%s| tieNames=%s guimouse=%d",
                       currentMenu->m_name.c_str(),
                       painttotal,
                       paintmenu,
                       paintcon ? (double)paintcon->getDepth() : -1.0,
                       (currentMenu->isVisible()) ? 1 : 0,
                       paintv3d,
                       (double)view3d->getDepth(),
                       view3d->getShow() ? 1 : 0,
                       cls.state,
                       painttie,
                       painttieNC,
                       paintcon_,
                       uivk_flip,
                       uivk_prevv3d,
                       uivk_gap,
                       (paintmenu && paintv3d && paintmenu < paintv3d)
                           ? "VIEW3D PAINTS OVER THE MENU"
                           : "menu paints over view3d",
                       paintfront,
                       painttieNames,
                       in_guimouse ? 1 : 0);
        }
    }

    if (ui_newvidmode->integer == -1) {
        // retail passes 2048 (CVAR_SERVER_CREATED), not CVAR_RESETSTRING
        Cvar_Get("ui_newvidmode", va("%d", r_mode), CVAR_SERVER_CREATED);
        Cvar_SetValue("ui_newvidmode", r_mode);
    }
}

void UI_OpenConsole(void)
{
    if (!fakk_console) {
        return;
    }

    fakk_console->setShow(true);
    uWinMan.ActivateControl(fakk_console);
    IN_MouseOn();
}

qboolean UI_ConsoleIsVisible(void)
{
    return fakk_console && fakk_console->IsVisible();
}

qboolean UI_ConsoleIsOpen(void)
{
    return fakk_console && fakk_console->IsVisible() && fakk_console->IsActive();
}

void UI_CloseConsole(void)
{
    if (fakk_console && fakk_console->getShow()) {
        fakk_console->setShow(false);
    }
    UIVK_PROBE("focus", "caller=UI_CloseConsole State=%d", cls.state);
    UI_FocusMenuIfExists();
}

void UI_ToggleConsole(void)
{
    if (!fakk_console) {
        return;
    }

    if (fakk_console->IsThisOrChildActive()) {
        if (fakk_console->getShow()) {
            fakk_console->setShow(false);
        }

        UIVK_PROBE("focus", "caller=UI_ToggleConsole hide State=%d", cls.state);
        UI_FocusMenuIfExists();
    } else {
        fakk_console->setShow(true);
        uWinMan.ActivateControl(fakk_console);
        IN_MouseOn();
    }
}




void UI_KeyEvent(int key, unsigned int time)
{
    uWinMan.KeyEvent(key, time);
}

void UI_CharEvent(int ch)
{
    uWinMan.CharEvent(ch);
}

void UI_ClearBackground(void)
{
    /* g_color_table[0] is opaque black, so this is the full-screen fill the
       operator reads as "the bgcolor fill".  Naming the branch that took it is
       what separates a real background from a wash painted under one. */
    {
        static int rows;
        rows++;
        if (rows <= 16 || (rows % 600) == 0) {
            Menu *m = menuManager.CurrentMenu();
            Com_Printf(
                "[UI:bkgfill] n=%d menu='%s' full=%d state=%d rect=%gx%g\n", rows,
                m ? m->m_name.c_str() : "(none)", (m && m->isFullscreen()) ? 1 : 0,
                (int)cls.state, (float)uid.vidWidth, (float)uid.vidHeight);
        }
    }

    re.Set2DWindow(
        uid.originX,
        uid.surfaceHeight - uid.originY - uid.vidHeight,
        uid.vidWidth,
        uid.vidHeight,
        0, uid.vidWidth, uid.vidHeight, 0,
        -1, 1
    );
    re.Scissor(uid.originX, uid.surfaceHeight - uid.originY - uid.vidHeight, uid.vidWidth, uid.vidHeight);
    re.SetColor(g_color_table[0]);
    re.DrawBox(0, 0, uid.vidWidth, uid.vidHeight);
    re.SetColor(NULL);
}

void UI_ActivateView3D(void)
{
    if (!view3d) {
        return;
    }

    view3d->setShow(true);
    uWinMan.ActivateControl(view3d);
}


qboolean UI_MenuUp()
{
    return menuManager.CurrentMenu() != NULL;
}

qboolean UI_FullscreenMenuUp()
{
    Menu *currentMenu = menuManager.CurrentMenu();

    if (currentMenu) {
        return currentMenu->isFullscreen();
    }

    return qfalse;
}

static Container<str> ui_printStrings;

void UI_PrintString(const char *fontname, float x, float y, const char *s, float alpha)
{
    UIFont    font;
    UIRect2D  frame;
    float     px;
    float     py;
    float     h;

    if (!fontname || !s || !*s || !view3d) {
        return;
    }

    font.setFont(fontname);

    frame = view3d->getFrame();
    h     = font.getHeight(false);

    px = (x == -1.0f) ? 0.0f : x;
    py = (y == -1.0f) ? 0.0f : frame.size.height - (y + h);

    font.setColor(UColor(1.0f, 1.0f, 1.0f, alpha));
    font.PrintJustified(UIRect2D(frame.pos.x + px + 1, frame.pos.y + py + 1, frame.size.width, h),
                        FONT_JUSTHORZ_LEFT,
                        FONT_JUSTVERT_TOP,
                        s,
                        UColor(0.0f, 0.0f, 0.0f, 0.0f),
                        false);
    font.PrintJustified(UIRect2D(frame.pos.x + px, frame.pos.y + py, frame.size.width, h),
                        FONT_JUSTHORZ_LEFT,
                        FONT_JUSTVERT_TOP,
                        s,
                        UColor(0.0f, 0.0f, 0.0f, 0.0f),
                        false);
}

void UI_AddPrintString(const char *text, float *pos, int width, float *color, const char *fontname)
{
    if (view3d) {
        view3d->AddPrintString(text, pos, width, color, fontname);
    }
}

static void getAllWeaponHuds(void)
{
    int i;

    for (i = 0; i < 6; i++) {
        weaphud[i] = menuManager.FindMenu(weaphudMenus[i]);
    }
}

int UI_GetGlobalFontHeight(void)
{
    UIFont *font;

    if (!view3d) {
        return -1;
    }

    font = view3d->getFont();
    return font ? font->getHeight(false) : -1;
}

int UI_GetFontHeight(const char *fontname, qboolean virtualScreen)
{
    if (!fontname) {
        return 0;
    }

    UIFont font(fontname);
    return font.getHeight(virtualScreen != 0);
}

void UI_NewMap(void)
{
    hudList.ClearObjectList();
}

static void deleteAllWeaponHuds(void)
{
    int i;

    for (i = 0; i < 6; i++) {
        if (weaphud[i]) {
            weaphud[i] = NULL;
        }
    }
}

void hideAllWeaponHuds(void)
{
    int i;

    for (i = 0; i < 6; i++) {
        if (weaphud[i]) {
            weaphud[i]->ForceHide();
        }
    }
}

void showWeaponHud(const char *ammoType)
{
    int i;

    if (!ammoType) {
        hideAllWeaponHuds();
        return;
    }

    for (i = 0; i < 6; i++) {
        if (!weaphud[i] || stricmp(ammoType, weaphudKeys[i])) {
            weaphud[i]->ForceHide();
        } else {
            weaphud[i]->ForceShow();
        }
    }
}

void UI_UpdateTacticalInfo(void)
{
    cvar_t     *ui_currentInfoIndex = Cvar_Get("ui_currentInfoIndex", "1", 0);
    cvar_t     *ui_lastInfoChangeTime = Cvar_Get("ui_lastInfoChangeTime", "0.0", 0);
    char        buf[32];
    int         count = MissionInformation_NumObjects();
    int         index = ( int )ui_currentInfoIndex->value;

    if (count < 1) {
        return;
    }

    if (( float )cls.realtime - ui_lastInfoChangeTime->value > 3000.0f) {
        index++;
        Com_sprintf(buf, sizeof(buf), "%d", index);
        Cvar_Set("ui_currentInfoIndex", buf);
        Com_sprintf(buf, sizeof(buf), "%d", cls.realtime);
        Cvar_Set("ui_lastInfoChangeTime", buf);
    }

    if (index <= MissionInformation_NumObjects()) {
        const char *data = MissionInformation_DataAtIndex(index);
        if (data) {
            Cvar_Set("ui_infoText", data);
        }
    } else {
        Cvar_Set("ui_currentInfoIndex", "1");
    }
}

void UI_ShowMissionFailed(bool bFailed)
{
    if (!missionfailure) {
        return;
    }

    if (bFailed) {
        str sIn = CL_ConfigString(0x7B1);
        str sOut;

        sOut = "$$" + sIn + "$$";
        Cvar_Set("ui_failureReason", sOut);

        if (!missionfailure->isVisible()) {
            menuManager.ClearMenus(true);
            menuManager.ShowMenu(missionfailure->m_name);
        }
    } else if (missionfailure->isVisible()) {
        missionfailure->ForceHide();
    }
}

void CL_ServerRefresh(void);

static void UI_Update_Impl(void);

#define UI_UI_STATE_END() \
    do { \
        if ( ui_statsOn && ui_stateT0 >= 0 ) { \
            UI_Add( UI_FAM_UI, UI_UIZ_state, UI_Microseconds() - ui_stateT0 ); \
            ui_stateT0 = -1; \
            LIMIT_StateEnd(); \
        } \
    } while ( 0 )

void UI_Update(void)
{
    /* One update descent per UI update: the stamp is what stops the window
       manager's walk repeating the descent the menu manager's walk already did. */
    UIWidget::FrameStampBump();

    UI_ROOT_BEGIN(UI_FAM_UI);

    UI_Update_Impl();

    UI_ROOT_END(UI_FAM_UI);

    LIMIT_UiFrameEnd();
}

static void UI_Update_Impl(void)
{
    Menu        *currentMenu;
    UIRect2D     frame;
    const char  *weapHud;
    const char  *rightHud;
    int          ammoIdx;
    str          menuName;
    static int   uivk_orderRows;

    int          ui_z0, ui_z1, ui_z2, ui_z3;
    int          ui_stateT0 = -1;

    ui_z0 = ( ui_statsOn ? UI_Microseconds() : 0 );
    re.SetRenderTime(cls.realtime);                       
    if ( ui_statsOn ) { UI_Add( UI_FAM_UI, UI_UIZ_setrt, UI_Microseconds() - ui_z0 ); }

    {
        int dx, dy, dw, dh;

        int limit_uit_aspect = LIMIT_Leg();

        SCR_AndroidDisplayRect(cls.glconfig.vidWidth, cls.glconfig.vidHeight, &dx, &dy, &dw, &dh);
        if (dx != uid.originX || dy != uid.originY || dw != uid.vidWidth || dh != uid.vidHeight) {
            UI_ResolutionChange();
        }

        LIMIT_LegEnd(LIMIT_FAM_UIT, LIMIT_UITZ_aspect, limit_uit_aspect);
    }

    ui_z1 = ( ui_statsOn ? UI_Microseconds() : 0 );
    CL_FillUIDef();
    if ( ui_statsOn ) { UI_Add( UI_FAM_UI, UI_UIZ_filluid, UI_Microseconds() - ui_z1 ); }

    if ( ui_hudScale && ui_hudScale->modified ) {
        ui_hudScale->modified = false;
        UI_SyncHudScale();
        UI_ResolutionChange();
    }

    if (cl.snap.ps.stats[STAT_CINEMATIC] & 1) {
        int limit_uit_cine = LIMIT_Leg();

        Cbuf_AddText("globalwidgetcommand InventoryBar inventoryhide\n"); 
        if (!hudListHidden) {
            UI_HideHudList();
            hudListHidden = qtrue;
        }

        LIMIT_LegEnd(LIMIT_FAM_UIT, LIMIT_UITZ_cine, limit_uit_cine);
    }

    ui_z2 = ( ui_statsOn ? UI_Microseconds() : 0 );
    uWinMan.ServiceEvents();
    if ( ui_statsOn ) { UI_Add( UI_FAM_UI, UI_UIZ_service, UI_Microseconds() - ui_z2 ); }

    ui_z3 = ( ui_statsOn ? UI_Microseconds() : 0 );
    UI_SetupConsoles();
    if ( ui_statsOn ) { UI_Add( UI_FAM_UI, UI_UIZ_consoles, UI_Microseconds() - ui_z3 ); }

    ui_stateT0 = ( ui_statsOn ? UI_Microseconds() : 0 );
    LIMIT_StateBegin();

    if (cls.no_menus && cls.state == CA_ACTIVE) {
        int limit_uis_nomenu = LIMIT_Leg();

        view3d->setShow(true);
        frame = uWinMan.getFrame();
        UI_ZONE(UI_FAM_UI, UI_UIZ_views, view3d->Display(frame, 1.0));

        if (ui_hud && !view3d->LetterboxActive()) {
            weapHud = NULL;
            ammoIdx = cl.snap.ps.activeItems[ITEM_NAME_AMMO_LEFT];
            if (ammoIdx >= 0) {
                weapHud = CL_ConfigString(CS_ITEMS + ammoIdx);
                if (strlen(weapHud)) {
                    ( void )stricmp(weapHud, "none");
                }
            }
            ammoIdx = cl.snap.ps.activeItems[ITEM_NAME_AMMO_RIGHT];
            if (ammoIdx >= 0) {
                rightHud = CL_ConfigString(CS_ITEMS + ammoIdx);
                if (strlen(rightHud)) {
                    ( void )stricmp(rightHud, "none");
                }
            }
            showWeaponHud(weapHud);

            if (hud_health) {
                hud_health->ForceShow();
                frame = uWinMan.getFrame();
                hud_health->GetContainerWidget()->Display(frame, 1.0);
            }
        }

        LIMIT_LegEnd(LIMIT_FAM_UIS, LIMIT_UISZ_nomenu, limit_uis_nomenu);
        UI_UI_STATE_END();
        return;
    }

    int limit_uis_machine = LIMIT_Leg();

    currentMenu = menuManager.CurrentMenu();

    /* The once-per-navigation row in UI_FocusMenuIfExists cannot see an order
       that changes between frames, and a view that draws on some frames and is
       covered on others reads as a flash.  This one is taken every frame the
       menu paints, rate limited, and carries the child order itself: the first
       entry in front= is what paints first. */
    if (currentMenu && PROBE_On() && view3d && PROBE_Throttle(&uivk_orderRows, 16, 60)) {
        int  orderTotal = uWinMan.GetChildCount();
        int  orderV3d   = 0;
        int  orderMenu  = 0;
        char orderFront[1024];

        orderFront[0] = 0;
        for (int orderi = 1; orderi <= orderTotal && strlen(orderFront) < 900; orderi++) {
            UIWidget *orderw = uWinMan.GetChild(orderi);

            if (orderw == view3d) {
                orderV3d = orderi;
            }
            if (orderw == currentMenu->GetContainerWidget()) {
                orderMenu = orderi;
            }
            if (orderi <= 10) {
                Q_strcat(orderFront, sizeof(orderFront),
                         va("%d:%s ", orderi, orderw ? orderw->getName() : "(null)"));
            }
        }

        PAINT_PROBE("paint:order",
                    "perframe menu='%s' state=%d view3d=%d menu=%d "
                    "verdict=%s | front=%s",
                    currentMenu->m_name.c_str(), (int)cls.state, orderV3d, orderMenu,
                    (orderV3d && orderMenu && orderV3d < orderMenu)
                        ? "VIEW3D_OVER_MENU" : "menu_over_view3d",
                    orderFront);
    }

    if (ui_minicon->modified && ui_minicon->integer) {
        ui_minicon->modified = false;

        if (!mini_console) {
            mini_console = new Miniconsole;
            mini_console->Create(UISize2D(500, 100), UWhite, UColor(0.0, 0.5, 1.0, 1.0), 0.2f);
            mini_console->setBorderStyle(border_none);
        }

        mini_console->UpdateFrame();
    }

    if (fakk_console && !server_loading) {
        if (ui_minicon->integer) {
            if (mini_console) {
                mini_console->setRealShow(!fakk_console->getShow());
            }
        } else if (mini_console) {
            mini_console->setRealShow(false);
        }
    }

    if (currentMenu && currentMenu->isFullscreen()) {
        if (!server_loading) {
            lastScoreboardTime = -1;
        }

        view3d->setShow(false);
    } else if (server_loading) {
        if (server_loading_stage) {
            menuName = "sublevelLoading";
        } else if (ui_sLevelLoadingScreen.length()) {
            menuName = ui_sLevelLoadingScreen;
        } else {
            menuName = "loading";
        }


        view3d->setShow(false);                           

        if (cg_gametype->integer) {
            if (!in_scoreboardShown) {
                UI_HideScoreboard_f();
            }
        } else if (missionsuccess && missionsuccess->isVisible()) {
            menuManager.HideMenu(missionsuccess->m_name);
        }

        if (menuManager.getLock()) {
            menuManager.Unlock(NULL);
            menuManager.PushMenu(menuName);
            menuManager.Lock(NULL);
        } else {
            menuManager.PushMenu(menuName);
        }
    } else {
        switch (cls.state) {
        case CA_CONNECTING:
        case CA_CHALLENGING:
        case CA_CONNECTED:
            UI_ClearBackground();                         
            UI_DrawConnect();                             
            break;

        default:
            if (cls.state >= CA_ACTIVE) {
                if (cls.state == CA_ACTIVE) {
                    lastScoreboardTime = -1;              

                    if (!view3d->LetterboxActive()) {     
                        view3d->setShow(true);
                    }
                } else {
                    UI_ClearBackground();                 
                    view3d->setShow(false);
                }
            } else if (cls.state == CA_DISCONNECTED && currentMenu) {
                /* A front-end menu floats over the 3D view, and the 3D view is
                   where the starfield and the ship live.  Retail leaves it
                   behind the menu; hiding the view and filling black put a
                   black rectangle behind every non-fullscreen menu, the controls
                   screen among them.  Loading and connect still fill. */
            } else {
                view3d->setShow(false);
                UI_ClearBackground();

            }
            break;
        }
    }

    LIMIT_LegEnd(LIMIT_FAM_UIS, LIMIT_UISZ_machine, limit_uis_machine);


    if (!ui_hud || cls.state != CA_ACTIVE || view3d->LetterboxActive() || (currentMenu && currentMenu->isFullscreen())) {
        int limit_uis_hudhide = LIMIT_Leg();
        // statics that retail never writes, so the branch is live only while
        if (server_loading) {
            if (headhud && headhud->isVisible()) {
                headhud->ForceHide();                     
            }
            if (texthud && texthud->isVisible()) {
                texthud->ForceHide();                     
            }
            if (headtexthud && headtexthud->isVisible()) {
                headtexthud->ForceHide();                 
            }
        }

        if (!hudListHidden) {
            UI_HideHudList();
            hudListHidden = qtrue;
        }

        hideAllWeaponHuds();                              
        if (hud_health) {
            hud_health->ForceHide();
        }
        if (printhud) {
            printhud->ForceHide();
        }
        if (hud_boss) {
            hud_boss->ForceHide();
        }
        if (itemTextHud) {
            itemTextHud->ForceHide();
        }
        if (pointshud) {
            pointshud->ForceHide();                       
        }
        if (specialmovetimer) {
            specialmovetimer->ForceHide();                
        }
        if (poweruphud) {
            poweruphud->ForceHide();
        }
        if (radarhud) {
            radarhud->ForceHide();
        }
        if (hud_levelExit) {
            hud_levelExit->ForceHide();                   
        }
        if (hud_targetName) {
            hud_targetName->ForceHide();                  
        }
        LIMIT_LegEnd(LIMIT_FAM_UIS, LIMIT_UISZ_hudhide, limit_uis_hudhide);
    } else {
        int limit_uis_hudshow = LIMIT_Leg();
        if (cl.snap.ps.stats[STAT_HEALTH] > 0) {
            lastScoreboardTime = cls.realtime;
        }

        if (!server_loading
            && ((cl.snap.ps.pm_flags & PMF_INTERMISSION)
                || (cl.snap.ps.stats[STAT_HEALTH] <= 0 && cg_gametype->integer
                    && cls.realtime > lastScoreboardTime + 1000))) {
            static int tikanim_sb_arm;
            if (PROBE_Throttle(&tikanim_sb_arm, 1, 20)) {
                TIKIANIM_PROBE("sb:deadarmlive",
                           "health=%d gametype=%d intermission=%d realtime=%d lastScore=%d",
                           cl.snap.ps.stats[STAT_HEALTH], cg_gametype->integer,
                           (cl.snap.ps.pm_flags & PMF_INTERMISSION) ? 1 : 0,
                           cls.realtime, lastScoreboardTime);
            }
            if (!cg_gametype->integer) {
                if (missionsuccess && !missionsuccess->isVisible()) {
                    menuManager.HideAllMenus();
                    menuManager.ShowMenu(missionsuccess->m_name);
                }
            } else {
                UI_ShowScoreboard_f();
                CL_ServerRefresh();                       
            }

            UI_UI_STATE_END();
            UI_ZONE(UI_FAM_UI, UI_UIZ_menus, menuManager.UpdateAllMenus());
            UI_ZONE(UI_FAM_UI, UI_UIZ_views, uWinMan.UpdateViews());
            return;
        }

        if (cg_gametype->integer) {
            if (!in_scoreboardShown) {
                static int tikanim_sb_uhide;
                if (PROBE_Throttle(&tikanim_sb_uhide, 1, 20)) {
                    TIKIANIM_PROBE("sb:uihide", "cg_gametype=%d in_scoreboardShown=0 -> UI_HideScoreboard_f", cg_gametype->integer);
                }
                UI_HideScoreboard_f();
            }
        } else if (missionsuccess && missionsuccess->isVisible()) {
            menuManager.HideMenu(missionsuccess->m_name);
        }

        if (hud_levelExit) {
            if (cl.snap.ps.pm_flags & PMF_LEVELEXIT) {
                hud_levelExit->ForceShow();
            } else {
                hud_levelExit->ForceHide();
            }
        }

        if (hud_targetName) {
            if (strlen(cg_targetedPlayerName->string)) {
                hud_targetName->ForceShow();
            } else {
                hud_targetName->ForceHide();
            }
        }

        if (cl.snap.ps.missionStatus & 1) {
            UI_ShowMissionFailed(true);
            UI_UI_STATE_END();
            UI_ZONE(UI_FAM_UI, UI_UIZ_views, uWinMan.UpdateViews());
            return;
        }
        UI_ShowMissionFailed(false);

        if (in_objectivesHud) {
            UI_UpdateTacticalInfo();
        }

        weapHud = NULL;
        ammoIdx = cl.snap.ps.activeItems[ITEM_NAME_AMMO_LEFT];
        if (ammoIdx >= 0) {
            weapHud = CL_ConfigString(CS_ITEMS + ammoIdx);
            if (strlen(weapHud)) {
                ( void )stricmp(weapHud, "none");
            }
        }
        ammoIdx = cl.snap.ps.activeItems[ITEM_NAME_AMMO_RIGHT];
        if (ammoIdx >= 0) {
            rightHud = CL_ConfigString(CS_ITEMS + ammoIdx);
            if (strlen(rightHud)) {
                ( void )stricmp(rightHud, "none");
            }
        }
        showWeaponHud(weapHud);

        if (poweruphud) {
            if (cl.snap.ps.stats[STAT_POWERUPTIME] <= 0) {
                poweruphud->ForceHide();
            } else {
                poweruphud->ForceShow();
            }
        }

        if (hud_boss) {
            if (cl.snap.ps.stats[STAT_BOSSHEALTH] > 0 && !hud_boss->isVisible()) {
                Event *event = new Event(EV_ShowMenu);
                event->AddInteger(false);
                hud_boss->ProcessEvent(event);
            } else if (hud_boss->isVisible()) {
                hud_boss->ProcessEvent(EV_HideMenu);
            }
        }

        if (itemTextHud) {
            if (cl.snap.ps.stats[STAT_ITEMTEXT] > 0 && !itemTextHud->isVisible()) {
                Event *event = new Event(EV_ShowMenu);
                event->AddInteger(false);
                itemTextHud->ProcessEvent(event);
            } else if (itemTextHud->isVisible()) {
                itemTextHud->ProcessEvent(EV_HideMenu);
            }
        }

        // which does not exist in the shipped .urc set, so retail's arm is
        if (specialmovetimer) {
            if (cl.snap.ps.stats[STAT_SPECIALMOVETIMER] > 0 && !specialmovetimer->isVisible()) {
                Event *event = new Event(EV_ShowMenu);
                event->AddInteger(false);
                specialmovetimer->ProcessEvent(event);
            } else if (specialmovetimer->isVisible()) {
                specialmovetimer->ProcessEvent(EV_HideMenu);
            }
        }

        if (pointshud) {                                  
            pointshud->ForceShow();
        }
        if (hud_health) {                                 
            hud_health->ForceShow();
        }
        if (printhud && !cg_gametype->integer) {          
            printhud->ForceShow();
        }
        if (radarhud && !cg_gametype->integer) {          
            radarhud->ForceShow();
        }
        if (hudListHidden) {                              
            UI_ShowHudList();
            hudListHidden = qfalse;
        }
        LIMIT_LegEnd(LIMIT_FAM_UIS, LIMIT_UISZ_hudshow, limit_uis_hudshow);
    }

    UI_UI_STATE_END();

    {
        int limit_uit_srvref = LIMIT_Leg();

        CL_ServerRefresh();

        LIMIT_LegEnd(LIMIT_FAM_UIT, LIMIT_UITZ_srvref, limit_uit_srvref);
    }

    {
        int limit_uit_print = LIMIT_Leg();

        if (ui_printStrings.NumObjects()) {
            str s = ui_printStrings.ObjectAt(1);
            UI_PrintString("serpentinemedium-16", -1.0f, -1.0f, s.c_str(), 1.0f);
            ui_printStrings.RemoveObjectAt(1);
        }

        LIMIT_LegEnd(LIMIT_FAM_UIT, LIMIT_UITZ_printpop, limit_uit_print);
    }

    UI_ZONE(UI_FAM_UI, UI_UIZ_menus, menuManager.UpdateAllMenus());
    UI_ZONE(UI_FAM_UI, UI_UIZ_views, uWinMan.UpdateViews());
}

void UI_ToggleMenu(str name)
{
    Menu *menu;

    menu = menuManager.CurrentMenu();

    if (menu) {
        UI_ForceMenuOff(false);

        if (menu->m_name != name) {
            UI_PushMenu(name);
        }
    } else {
        UI_PushMenu(name);
    }

    UI_FocusMenuIfExists();
}

void UI_ToggleMenu_f()
{
    if (Cmd_Argc() <= 1) {
        Com_Printf("Usage: togglemenu <menuname>\n");
        return;
    }

    UI_ToggleMenu(Cmd_Argv(1));
}

void UI_PopMenu(qboolean restore_cvars)
{
    if (menuManager.CurrentMenu()) {
        menuManager.PopMenu(restore_cvars);
    }

    UIVK_PROBE("focus", "caller=UI_PopMenu State=%d", cls.state);
    UI_FocusMenuIfExists();
}

void UI_PushMenu(const char *name)
{
    Menu    *menu = menuManager.CurrentMenu();
    // veto the push, which is a gate retail does not have.
    qboolean bDiff = qtrue;

    if (menu) {
        bDiff = strcmp(menu->m_name, name) != 0;
    }

    if (bDiff) {
        menuManager.PushMenu(name);
    }

    UIVK_PROBE("focus",
               "caller=UI_PushMenu name='%s' pushed=%d State=%d",
               name,
               bDiff ? 1 : 0,
               cls.state);
    UI_FocusMenuIfExists();
}

void UI_TransitionMenu(const char *name)
{
    menuManager.TransitionMenu(name);

    UI_FocusMenuIfExists();
}

void UI_ForceMenu(const char *name)
{
    Menu    *menu  = menuManager.CurrentMenu();
    qboolean bDiff = qfalse;

    if (menu) {
        bDiff = strcmp(menu->m_name, name) != 0;
    }

    if (!menu || bDiff) {
        menuManager.ForceMenu(name);
    }

    UI_FocusMenuIfExists();
}

void UI_ShowMenu(const char *name, qboolean bForce)
{
    Menu *pMenu;

    if (bForce) {
        pMenu = menuManager.FindMenu(name);

        if (pMenu) {
            pMenu->ForceShow();
        }

        return;
    }

    //    widgetContainerCompare (@0x000B4450), which NEVER answers 0 -- a tie
    menuManager.ShowMenu(name);

    UIVK_PROBE("focus", "caller=UI_ShowMenu name='%s' State=%d", name, cls.state);
    UI_FocusMenuIfExists();
}

void UI_HideMenu(const char *name, qboolean bForce)
{
    menuManager.HideMenu(name);
}

void UI_PushMenu_f(void)
{
    if (Cmd_Argc() <= 1) {
        Com_Printf("Usage: pushmenu <menuname>\n");
        return;
    }

    UI_PushMenu(Cmd_Argv(1));
}


void UI_TransitionMenu_f(void)
{
    if (Cmd_Argc() <= 1) {
        Com_Printf("Usage: transitionmenu <menuname> [transitiontype]\n");
        return;
    }

    // Retail UI_TransitionMenu_f @0x00194150 pops through MenuManager::PopMenu
    // @0x0006D3A0 and records a transition that UpdateAllMenus @0x0006C870 pushes
    // once menus unlock; the collapse into TransitionMenu lands on the same stacks.
    if (menuManager.HasShownMenu()) {
        menuManager.PopMenu(atoi(Cmd_Argv(2)));
        UI_PushMenu(Cmd_Argv(1));
        return;
    }

    UI_TransitionMenu(Cmd_Argv(1));
}

void UI_ForceMenu_f(void)
{
    if (Cmd_Argc() <= 1) {
        Com_Printf("Usage: forcemenu <menuname>\n");
        return;
    }

    UI_ForceMenu(Cmd_Argv(1));
}

void UI_PopMenu_f(void)
{
    if (Cmd_Argc() <= 1) {
        Com_Printf("Usage: popmenu <restore_cvars - set to 1 if you want all cvars restored to their original values>");
        return;
    }

    UI_PopMenu(atoi(Cmd_Argv(1)));
}

void UI_ShowMenu_f(void)
{
    const char *name;

    if (Cmd_Argc() <= 1) {
        Com_Printf("Usage: showmenu <menuname>\n");
        return;
    }

    name = Cmd_Argv(1);

    UI_ShowMenu(name, qfalse);
}

void UI_HideMenu_f(void)
{
    const char *name = Cmd_Argv(1);

    UI_HideMenu(name, qfalse);
}

void UI_WidgetCommand_f(void)
{
    str name;
    str commandstring;

    if (Cmd_Argc() <= 2) {
        Com_Printf("Usage: widgetcommand <name> <args1...argsN>\n");
        return;
    }

    name          = Cmd_Argv(1);
    commandstring = Cmd_Argv(2);
    Event *event  = new Event(commandstring);

    for (int i = 3; i < Cmd_Argc(); i++) {
        event->AddToken(Cmd_Argv(i));
    }

    menuManager.PassEventToWidget(name, event);
}

void UI_GlobalWidgetCommand_f(void)
{
    str name;
    str commandstring;

    if (Cmd_Argc() <= 2) {
        Com_Printf("Usage: globalwidgetcommand <name> <args1...argsN>\n");
        return;
    }

    name          = Cmd_Argv(1);
    commandstring = Cmd_Argv(2);
    Event *event  = new Event(commandstring);

    PROBE("inv:gwc", "widget='%s' event='%s' argc=%d", name.c_str(), commandstring.c_str(), Cmd_Argc());

    for (int i = 3; i < Cmd_Argc(); i++) {
        event->AddToken(Cmd_Argv(i));
    }

    uWinMan.PassEventToWidget(name, event, qtrue);
}

void UI_ListMenus_f()
{
    menuManager.ListMenus();
}

qboolean UI_BindActive(void)
{
    return uWinMan.BindActive() != NULL;
}

extern "C" qboolean UI_BindActive_C(void)
{
    return UI_BindActive();
}

extern "C" qboolean UI_MenuActive_C(void)
{
    return UI_MenuActive();
}

extern "C" qboolean UI_IsHudActive_C(void)
{
    return UI_IsHudActive();
}

void UI_MenuEscape(const char *name)
{
    if (uWinMan.BindActive()) {
        UIVK_PROBE("key", "key='ESCAPE' MenuEscape arm=BindActive");
        UI_KeyEvent(K_ESCAPE, qfalse);
        return;
    }

    if (server_loading) {
        UIVK_PROBE("key", "key='ESCAPE' MenuEscape arm=server_loading State=%d", cls.state);
        return;
    }

    if (menuManager.CurrentMenu() && menuManager.CurrentMenu() == mainmenu
        && cls.state == CA_DISCONNECTED) {
        // 0x001A19D0), never a strcmp: retail holds the main menu object, and the
        UI_FocusMenuIfExists();
        return;
    }

    // change retail never produces whenever one of the eight shipped huds that author
    for (int i = 1; i <= hudList.NumObjects(); i++) {
        Menu *hud = hudList.ObjectAt(i);

        if (hud && hud->isNoEscape()) {
            UIVK_PROBE("key",
                       "key='ESCAPE' MenuEscape arm=noEscape-hud-scan -> return (hud '%s')",
                       hud->m_name.c_str());
            return;
        }
    }

    if (menuManager.CurrentMenu()) {
        Menu *cur = menuManager.CurrentMenu();

        if (!cur->isNoEscape()) {
            UIVK_PROBE("key",
                       "key='ESCAPE' MenuEscape arm=PopMenu '%s' State=%d",
                       cur->m_name.c_str(),
                       cls.state);
            menuManager.PopMenu(qtrue);
        } else {
            UIVK_PROBE("key",
                       "key='ESCAPE' MenuEscape arm=noEscape '%s' -> focus pass only State=%d",
                       cur->m_name.c_str(),
                       cls.state);
        }

        UI_FocusMenuIfExists();
        return;
    } else if (!Q_stricmp(name, "main") && cls.state == CA_ACTIVE && cg_gametype->integer == 0) {
        UIVK_PROBE("key",
                   "key='ESCAPE' MenuEscape arm=PushMenu ingame_singleplayer State=%d",
                   cls.state);
        UI_PushMenu("ingame_singleplayer");
    } else if (!Q_stricmp(name, "main") && cls.state == CA_ACTIVE && cg_gametype->integer > 0) {
        UI_PushMenu("ingame_multiplayer");
    } else if (!Q_stricmp(name, "main") && cls.state > CA_PRIMED && cg_gametype->integer > 0) {
        UI_PushMenu("dm_main");
    } else {
        UI_PushMenu(name);
    }

    UI_FocusMenuIfExists();
}

void UI_ForceMenuOff(bool force)
{
    menuManager.ClearMenus(force);
    UI_FocusMenuIfExists();
}

void UI_UpdateConnectionString(void) {}

void UI_ParseServerInfoMessage(msg_t *msg) {}

void UI_DrawConnectText(void) {}

static void UI_QueueConnectText(const char *text)
{
    char converted[256];

    uii.SR_ConvertStringResourceIds(text, converted, sizeof(converted));
    text = converted;

    if (ui_printStrings.NumObjects() <= 64) {
        if (ui_printStrings.NumObjects()) {
            str *head;

            head = &ui_printStrings.ObjectAt(1);
            *head += text;
        } else {
            ui_printStrings.AddObject(str(text));
        }
    }
}

void UI_AddNotifyString(const char *text)
{
    UI_QueueConnectText(text);
}

void UI_DrawConnect(void)
{
    bool localServer;
    str  s;

    localServer = !stricmp(cls.servername, "localhost");              

    UI_QueueConnectText(va("%s - $$Version$$ %s\n", "Elite Force II", "1.10"));   
    UI_QueueConnectText(va("$$ConnectingTo$$ %s\n", cls.servername));             

    switch (cls.state) {
    case CA_CONNECTING:
        s = va("$$AwaitingConnection$$...%i\n\n\n^3%s^1\n\n$$PressEscToCancel$$",
               clc.connectPacketCount, clc.serverMessage);             
        break;
    case CA_CHALLENGING:
        s = va("$$AwaitingChallenge$$...%i\n\n\n^3%s^1\n\n$$PressEscToCancel$$",
               clc.connectPacketCount, clc.serverMessage);             
        break;
    case CA_CONNECTED:
        s = va("$$AwaitingGamestate$$...\n\n\n%s\n\n\n$$PressEscToCancel$$",
               clc.serverMessage);                                     
        break;
    default:
        return;
    }

    if (!localServer) {
        UI_QueueConnectText(s);
    }
}

void CL_PingServers_f(void)
{
    int         i;
    netadr_t    adr;
    char        name[32];
    const char *adrstring;
    cvar_t     *noudp;
    cvar_t     *noipx;

    Com_Printf("pinging broadcast...\n");

    noudp = Cvar_Get("noudp", "0", CVAR_INIT);
    if (!noudp->integer) {
        adr.type = NA_BROADCAST;
        adr.port = BigShort(12203);
        CL_NET_OutOfBandPrint(adr, "info %i", 8);
    }

    noipx = Cvar_Get("noipx", "0", CVAR_INIT);
    if (!noipx->integer) {
        adr.type = NA_BROADCAST_IPX;
        adr.port = BigShort(12203);
        CL_NET_OutOfBandPrint(adr, "info %i", 8);
    }

    for (i = 0; i < 16; i++) {
        Com_sprintf(name, sizeof(name), "adr%i", i);
        adrstring = Cvar_VariableString(name);

        if (adrstring && *adrstring) {
            Com_Printf("pinging adr%i...\n", i);

            if (NET_StringToAdr(adrstring, &adr)) {
                if (!adr.port) {
                    adr.port = BigShort(12203);
                }

                CL_NET_OutOfBandPrint(adr, "info %i", 8);
            } else {
                Com_Printf("Bad address: adr%i\n", i);
            }
        }
    }
}

void UI_MapList_f(void)
{
    str mappath = "maps";

    if (Cmd_Argc() == 2) {
        mappath += "/";
        mappath += Cmd_Argv(1);
    }

    MapRunnerClass *map = new MapRunnerClass;
    map->Setup("maps", mappath, ".bsp", true);

    CL_SetMousePos(uid.originX + uid.vidWidth / 2, uid.originY + uid.vidHeight / 2);
}

void UI_SoundPicker_f(void)
{
    new SoundPickerClass;
}

void UI_ViewSpawnList_f(void)
{
    str modelpath = "models";

    ViewSpawnerClass *picker = new ViewSpawnerClass;
    picker->Setup("models", modelpath, ".tik", true);
}

void UI_Notepad_f(void)
{
    if (Cmd_Argc() > 1) {
        UI_LoadNotepadFile(Cmd_Argv(1));
    } else {
        UI_LoadNotepadFile(NULL);
    }
}

void UI_EditScript_f(void)
{
    const char *info;
    const char *mname;
    str         mapname;
    str         mappath;

    if (cls.state != CA_ACTIVE) {
        Com_Printf("You need to load a map to edit its script\n");
        return;
    }

    mname   = Info_ValueForKey(&cl.gameState.stringData[cl.gameState.stringOffsets[CS_SERVERINFO]], "mapname");
    mapname = mname;

    if (strchr(mname, '$')) {
        info                  = strchr(mname, '$');
        mapname[info - mname] = 0;
    }

    if (!strcmp(mapname, "")) {
        Com_Printf("No map loaded?\n");
        return;
    }

    mappath = "maps/" + mapname + ".scr";

    if (!UI_LoadNotepadFile(mappath)) {
        Com_Printf("Couldn't load/find script file for %s\n", mappath.c_str());
    }
}

void UI_EditShader_f(void)
{
    str shaderpath;

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: Editshader <file>\n");
        return;
    }

    shaderpath = "docs/scripts/" + str(Cmd_Argv(1)) + ".shader";

    if (!UI_LoadNotepadFile(shaderpath)) {
        Com_Printf("Couldn't open shader named %s\n", shaderpath.c_str());
    }
}

void UI_EditSpecificShader_f(void) {}

void UI_LoadMenu(const char *name)
{
    char buffer[256];

    Com_sprintf(buffer, sizeof(buffer), "ui/%s", name);
    COM_StripExtension(buffer, buffer, sizeof(buffer));
    Q_strcat(buffer, 256, ".urc");

    if (FS_ReadFile(buffer, NULL) < 0) {
        char        locbuffer[256];
        const char *language = CvarGetForUI("local_language", "Eng");

        Com_sprintf(locbuffer, sizeof(locbuffer), "loc/%s/ui/%s", language, name);
        COM_StripExtension(locbuffer, locbuffer, sizeof(locbuffer));
        Q_strcat(locbuffer, 256, ".urc");

        if (FS_ReadFile(locbuffer, NULL) < 0) {
            Com_Printf("UI_LoadMenu: couldn't find menu '%s'\n", name);
            return;
        }

        Q_strncpyz(buffer, locbuffer, sizeof(buffer));
    }

    new UILayout(buffer);
    UI_AdaptSettingsMenus();
    uWinMan.CreateMenus();
    UI_FocusMenuIfExists();
}

void UI_LoadMenu_f(void)
{
    UI_LoadMenu(Cmd_Argv(1));
}

const char *CvarGetForUI(const char *name, const char *defval)
{
    cvar_t *cvar = Cvar_FindVar(name);

    if (cvar) {
        if (cvar->latchedString && *cvar->latchedString) {
            return cvar->latchedString;
        } else {
            return cvar->string;
        }
    } else {
        return defval;
    }
}

void ListFilesForUI(const char *filespec)
{
    int    nfiles;
    char **filenames;

    filenames = FS_ListFiles("", filespec, qfalse, &nfiles);

    for (int i = 0; i < nfiles; i++) {
        uie.AddFileToList(filenames[i]);
    }

    FS_FreeFileList(filenames);
}

int IsKeyDown(int key)
{
    return keys[key].down;
}

void UI_WantsKeyboard()
{
    Key_SetCatcher(Key_GetCatcher() | KEYCATCH_UI);
}

struct widgettrans_s {
    const char *src;
    const char *dst;
};

static widgettrans_s s_widgettrans[2] = {
    {"Label", "FakkLabel"},
    {NULL,    NULL       }
};

const char *TranslateWidgetName(const char *widget)
{
    widgettrans_s *trans = &s_widgettrans[0];

    for (; trans->src != NULL; trans++) {
        if (!strcmp(trans->src, widget)) {
            return trans->dst;
        }
    }

    return NULL;
}

void UI_Cvar_Set(const char *var_name, const char *value)
{
    Cvar_Set2(var_name, value, qfalse);
}

/* One HUD scale factor for the in-game HUD only. The 640x480 virtual space that `uid.scaleRes'
   resolves stays retail, so every 2D menu, dialog, loading screen and scoreboard is
   pixel-identical; a HUD element's subtree is the only place `uid.scaleRes' is multiplied, and
   each HUD menu keeps the edge its `align' declares. `ui_hudScale' 0 means auto: 1.0 on a desktop,
   and on Android the display density over 160 clamped to [1,2] -- a 6.4" 720p panel (density 320)
   and a 6.4" 1440p panel (density 560) both land on the cap and read the same size. The clamp is
   the same on both ends. */
#define HUD_SCALE_MIN	1.0f
#define HUD_SCALE_MAX	2.0f
/* A phone panel is at least an xhdpi one, so when neither density query answers
   this is the floor to size against rather than dropping the feature to 1.0. */
#define HUD_SCALE_PHONE_FLOOR_DPI	320

static float s_uiHudScaleAuto = 1.0f;
static int   s_uiHudScaleDpi = 0;
static const char *s_uiHudScaleDpiSource = "none";

static float UI_ClampHudScale( float scale )
{
    if ( !( scale > HUD_SCALE_MIN ) ) {
        return HUD_SCALE_MIN;
    }
    if ( scale > HUD_SCALE_MAX ) {
        return HUD_SCALE_MAX;
    }
    return scale;
}

static void UI_ComputeAutoHudScale( void )
{
    s_uiHudScaleAuto = 1.0f;
    s_uiHudScaleDpi = 0;
    s_uiHudScaleDpiSource = "desktop";

#ifdef __ANDROID__
    {
        float ddpi = 0.0f;

        if ( SDL_GetDisplayDPI( 0, &ddpi, NULL, NULL ) == 0 && ddpi > 0.0f ) {
            s_uiHudScaleDpi = (int)ddpi;
            s_uiHudScaleDpiSource = "sdl";
        } else {
            /* SDL's own Android DPI query can answer non-zero and hand back
               nothing usable.  The Resources density is the same number the
               window manager reports, so it is read directly. */
            int jdpi = Sys_AndroidDensityDpi();

            if ( jdpi > 0 ) {
                s_uiHudScaleDpi = jdpi;
                s_uiHudScaleDpiSource = "resources";
            }
        }

        if ( s_uiHudScaleDpi <= 0 ) {
            s_uiHudScaleDpi = HUD_SCALE_PHONE_FLOOR_DPI;
            s_uiHudScaleDpiSource = "phone-floor";
        }

        s_uiHudScaleAuto = UI_ClampHudScale( (float)s_uiHudScaleDpi / 160.0f );
    }
#endif
}

static float UI_EffectiveHudScale( void )
{
    if ( ui_hudScale && ui_hudScale->value > 0.0f ) {
        return UI_ClampHudScale( ui_hudScale->value );
    }

    return s_uiHudScaleAuto;
}

/* One startup line, always, with the inputs that produced the scale: the resolved factor, the path
   it came from, the raw density and the pre-clamp ratio. Without it a device round cannot tell a
   working scale from a silent 1.0. */
static void UI_LogHudScale( void )
{
    if ( ui_hudScale && ui_hudScale->value > 0.0f ) {
        Com_Printf( "HUD scale: source=cvar configured=%.3f density_ratio=n/a effective=%.3f\n",
                    ui_hudScale->value, UI_EffectiveHudScale() );
        return;
    }

    if ( s_uiHudScaleDpi > 0 ) {
        Com_Printf( "HUD scale: source=auto dpi=%d via=%s density_ratio=%.3f effective=%.3f\n",
                    s_uiHudScaleDpi, s_uiHudScaleDpiSource,
                    (float)s_uiHudScaleDpi / 160.0f, UI_EffectiveHudScale() );
    } else {
        Com_Printf( "HUD scale: source=auto desktop density_ratio=n/a effective=%.3f\n",
                    UI_EffectiveHudScale() );
    }
}

/* The in-game HUD is two sets that reach the screen by different routes, and both are scaled. The
   first is `hudList': everything `ui_addhud' pushes there -- health, radar, inventory, item text,
   mission objectives, the pause icon, the objective notices, the arena and match-mode status
   boards, and the menus the game adds through `Player::addHud'. `dm_scoreboard' is the exception:
   the multiplayer scoreboard is interface, not HUD, and stays retail-sized. The second is the
   fixed set the client finds by name -- the weapon-ammo readouts (`weaphud*'), the dialogue/comm
   box (`headhud', `headtexthud', `texthud') and the small HUD panels. Menus, dialogs, loading
   screens, the mission-result screens and the scoreboard are in neither set. */
static qboolean UI_HudMenuScaled( const Menu *hud )
{
    return hud && strcmp( hud->m_name.c_str(), "dm_scoreboard" ) != 0;
}

static void UI_ApplyHudScale( Menu *hud )
{
    UIWidget *cont;

    if ( !hud ) {
        return;
    }

    cont = hud->GetContainerWidget();

    if ( cont ) {
        cont->setHudScale( UI_HudMenuScaled( hud ) ? UI_EffectiveHudScale() : 1.0f );
    }
}

/* The client-managed HUD menus, in one place so the factor is applied when they
   are first found and again when the cvar changes. */
static Menu **UI_StaticHudMenus( int *count )
{
    static Menu *list[32];
    int          n = 0;
    int          i;

    for ( i = 0; i < 6; i++ ) {
        list[n++] = weaphud[i];
    }

    list[n++] = hud_health;
    list[n++] = printhud;
    list[n++] = hud_boss;
    list[n++] = itemTextHud;
    list[n++] = poweruphud;
    list[n++] = radarhud;
    list[n++] = headhud;
    list[n++] = headtexthud;
    list[n++] = texthud;
    list[n++] = specialmovetimer;
    list[n++] = pointshud;
    list[n++] = hud_levelExit;
    list[n++] = hud_targetName;

    *count = n;
    return list;
}

/* A HUD menu added or first found after the last resolution change was laid out at retail scale,
   so it is laid out here in its own scope and re-aligned afterwards. A full-screen overlay keeps
   retail geometry and takes the factor at draw time only. */
static void UI_RelayoutHudMenu( Menu *hud )
{
    UIWidget *cont;
    bool      bLayoutScale;
    int       i, n;

    if ( !hud ) {
        return;
    }

    cont = hud->GetContainerWidget();

    if ( !cont ) {
        return;
    }

    bLayoutScale = cont->isHudScaleSet() && !cont->isHudFullScreen();

    if ( bLayoutScale ) {
        UIWidget::HudScalePush( cont->getHudScale() );
    }

    n = hud->GetWidgetCount();
    for ( i = 1; i <= n; i++ ) {
        UIWidget *wid = hud->GetWidget(i);

        if ( wid ) {
            wid->ChangeResolution();
        }
    }

    if ( bLayoutScale ) {
        UIWidget::HudScalePop();
    }

    hud->RealignWidgets();
}

static void UI_ApplyStaticHudScale( void )
{
    int   n, i;
    Menu **list = UI_StaticHudMenus( &n );

    for ( i = 0; i < n; i++ ) {
        UI_ApplyHudScale( list[i] );
    }
}

/* Set the factor on every client-managed HUD menu and lay it out. Used once the client has found
   its HUD menus; a cvar change re-lays them out through `UI_ResolutionChange' instead. */
static void UI_ScaleStaticHudMenus( void )
{
    int   n, i;
    Menu **list = UI_StaticHudMenus( &n );

    for ( i = 0; i < n; i++ ) {
        UI_ApplyHudScale( list[i] );
        UI_RelayoutHudMenu( list[i] );
    }
}

static void UI_SyncHudScale( void )
{
    for ( int i = 1; i <= hudList.NumObjects(); i++ ) {
        UI_ApplyHudScale( hudList.ObjectAt(i) );
    }

    UI_ApplyStaticHudScale();
}

// THE single writer of the whole `uid` block.  Retail CL_FillUIDef @0x00196BE0
void CL_FillUIDef(void)
{
    static const float ef2VirtualWidth  = 640.0f;
    static const float ef2VirtualHeight = 480.0f;
    int                dx, dy, dw, dh;

    CL_GetMouseState(&uid.mouseX, &uid.mouseY, &uid.mouseFlags);
    uid.time = cls.realtime;

    uid.surfaceWidth  = cls.glconfig.vidWidth;
    uid.surfaceHeight = cls.glconfig.vidHeight;

    SCR_AndroidDisplayRect(cls.glconfig.vidWidth, cls.glconfig.vidHeight, &dx, &dy, &dw, &dh);
    uid.originX = dx;
    uid.originY = dy;

    uid.vidWidth   = dw;
    uid.vidHeight  = dh;
    uid.uiHasMouse = in_guimouse != qfalse;

    uid.scaleRes[0] = (float)uid.vidWidth / ef2VirtualWidth;
    uid.scaleRes[1] = (float)uid.vidHeight / ef2VirtualHeight;
    uid.bHighResScaling = qtrue;

    uid.mouseX -= uid.originX;
    uid.mouseY -= uid.originY;
}

sfxHandle_t UI_RegisterSound(const char *sample, qboolean streamed)
{
    return S_RegisterSoundExt(sample, streamed, qfalse);
}

void UI_FadeSound(float fTime)
{
    S_FadeSound(fTime);
}

void UI_StopAll()
{
    S_StopAllSounds2(qtrue);
}

void UI_ShowDisconnectMessage(const char *msg)
{
    str dialogName;
    str title;

    if (!strlen(msg)) {
        return;
    }

    dialogName = "okdialog";                                          
    menuManager.ShowMenu(dialogName);
    UI_FocusMenuIfExists();

    title = "globalwidgetcommand OkDialogTitle title \"";             
    if (msg) {
        title += msg;
    }
    title += "\"\n";                                                  

    Cbuf_AddText(title.c_str());
}

void UI_StartLocalSound(const char *sound_name, int channel)
{
    S_StartLocalSoundChannel(sound_name, qtrue, (soundChannel_t)channel);
}

void UI_StartLocalSoundDialog(const char *sound_name)
{
    S_StartLocalSoundChannel(sound_name, qtrue, CHAN_DIALOG);
}

static void CL_VerifyUIImports(void)
{
    if (uii.Rend_RegisterModel    != re.RegisterModel       ||
        uii.Rend_RegisterMaterial != re.RegisterShaderNoMip ||
        uii.Rend_RefreshMaterial  != re.RefreshShaderNoMip  ||
        uii.Rend_DrawBox          != re.DrawBox             ||
        uii.Rend_DrawPicStretched != re.DrawStretchPic      ||
        uii.Rend_DrawPicTiled     != re.DrawTilePic         ||
        uii.Rend_GetShaderWidth   != re.GetShaderWidth      ||
        uii.Rend_GetShaderHeight  != re.GetShaderHeight     ||
        uii.Rend_DrawString       != re.DrawString          ||
        uii.Rend_LoadFont         != re.LoadFont            ||
        uii.Rend_Scissor          != re.Scissor             ||
        uii.Rend_Set2D            != re.Set2DWindow         ||
        uii.Rend_SetColor         != re.SetColor) {
        Com_Error(ERR_FATAL,
                  "uii.Rend_* does not match the current refexport_t: the UI's cached "
                  "renderer imports are stale (renderer module reloaded without "
                  "CL_FillUIImports?)");
    }
}

void CL_FillUIImports(void)
{
    uii.Rend_DrawBox           = re.DrawBox;
    uii.Rend_DrawPicStretched  = re.DrawStretchPic;
    uii.Rend_DrawPicTiled      = re.DrawTilePic;
    uii.Rend_GetShaderHeight   = re.GetShaderHeight;
    uii.Rend_GetShaderWidth    = re.GetShaderWidth;
    uii.Rend_DrawString        = re.DrawString;
    uii.Rend_LoadFont          = re.LoadFont;
    uii.Rend_RegisterMaterial  = re.RegisterShaderNoMip;
    uii.Rend_RefreshMaterial   = re.RefreshShaderNoMip;
    uii.Rend_RegisterModel     = re.RegisterModel;
    uii.Rend_Scissor           = re.Scissor;
    uii.Rend_Set2D             = re.Set2DWindow;
    uii.Rend_SetColor          = re.SetColor;

    uii.Cmd_Stuff                   = Cbuf_AddText;
    uii.Cvar_GetString              = CvarGetForUI;
    uii.Cvar_Reset                  = Cvar_Reset;
    uii.Cvar_Find                   = Cvar_FindVar;
    uii.Cvar_Set                    = UI_Cvar_Set;
    uii.File_PickFile               = PickFile;
    uii.File_FreeFile               = FS_FreeFile;
    uii.File_ListFiles              = ListFilesForUI;
    uii.File_OpenFile               = FS_ReadFile;
    uii.File_WriteFile              = FS_WriteTextFile;
    uii.Snd_PlaySound               = UI_StartLocalSound;
    uii.Snd_RegisterSound           = UI_RegisterSound;
    uii.Alias_Add                   = Alias_Add;
    uii.Alias_FindRandom            = Alias_FindRandom;
    uii.Sys_Error                   = Com_Error;
    uii.Sys_IsKeyDown               = IsKeyDown;
    uii.Sys_Milliseconds            = Sys_Milliseconds;
    uii.Sys_Printf                  = Com_Printf;
    uii.Sys_DPrintf                 = Com_DPrintf;
    uii.Sys_GetClipboard            = Sys_GetWholeClipboard;
    uii.Sys_SetClipboard            = Sys_SetClipboard;
    uii.Cmd_CompleteCommandByNumber = Cmd_CompleteCommandByNumber;
    uii.Cvar_CompleteCvarByNumber   = Cvar_CompleteVariableByNumber;
    uii.UI_WantsKeyboard            = UI_WantsKeyboard;
    uii.Client_TranslateWidgetName  = TranslateWidgetName;
    uii.Key_GetKeynameForCommand    = Key_GetKeynameForCommand;
    uii.Key_GetCommandForKey        = Key_GetBinding;
    uii.Key_SetBinding              = Key_SetBinding;
    uii.Key_GetKeysForCommand       = Key_GetKeysForCommand;
    // port was binding Key_KeynumToBindString, which does not exist in retail
    uii.Key_KeynumToString          = Key_KeynumToString;

    uii.File_ReadFileEx            = FS_ReadFileEx;                
    uii.File_GetDirectoryNames     = FS_GetDirectoryNames;         
    uii.Sys_WPrintf                = Com_WPrintf;                  
    uii.Sys_WDPrintf               = Com_WDPrintf;                 
    uii.Key_StringToKeynum         = Key_StringToKeynum;           
    uii.ActivateView3D             = UI_ActivateView3D;            
    uii.MissionObjective_GetDescription       = MissionObjective_GetDescription;       
    uii.MissionObjective_SetDescription       = MissionObjective_SetDescription;       
    uii.MissionObjective_GetShowObjective     = MissionObjective_GetShowObjective;     
    uii.MissionObjective_SetShowObjective     = MissionObjective_SetShowObjective;     
    uii.MissionObjective_GetObjectiveComplete = MissionObjective_GetObjectiveComplete; 
    uii.MissionObjective_SetObjectiveComplete = MissionObjective_SetObjectiveComplete; 
    uii.MissionObjective_GetObjectiveFailed   = MissionObjective_GetObjectiveFailed;   
    uii.MissionObjective_SetObjectiveFailed   = MissionObjective_SetObjectiveFailed;   
    uii.MissionObjective_GetNameFromIndex     = MissionObjective_GetNameFromIndex;     
    uii.MissionObjective_GetIndexFromName     = MissionObjective_GetIndexFromName;     
    uii.MissionObjective_NewObjective         = MissionObjective_NewObjective;         
    uii.MissionObjective_ClearObjectiveList   = MissionObjective_ClearObjectiveList;   
    uii.MissionObjectives_ParseObjectiveFile  = MissionObjectives_ParseObjectiveFile;  
    uii.MissionInformation_GetShader          = MissionInformation_GetShader;          
    uii.MissionInformation_SetShader          = MissionInformation_SetShader;          
    uii.MissionInformation_GetInformationData = MissionInformation_GetInformationData; 
    uii.MissionInformation_SetInformationData = MissionInformation_SetInformationData; 
    uii.MissionInformation_GetNameFromIndex   = MissionInformation_GetNameFromIndex;   
    uii.MissionInformation_GetIndexFromName   = MissionInformation_GetIndexFromName;   
    uii.MissionInformation_NewInformation     = MissionInformation_NewInformation;     
    uii.MissionInformation_ClearInformationList = MissionInformation_ClearInformationList; 
    uii.MissionInformation_SetShowInformation = MissionInformation_SetShowInformation; 
    uii.MissionInformation_GetShowInformation = MissionInformation_GetShowInformation; 
    uii.SR_GetStringResource         = getStringResource;            
    uii.SR_IsStringResource          = isStringResource;             
    uii.SR_ConvertStringResourceIds  = convertStringResourceIds;     
    uii.Gameplay_SetStringProperty   = SetGameplayStringProperty;    
    uii.Gameplay_GetStringProperty   = GetGameplayStringProperty;    
    uii.Gameplay_GetFloatProperty    = GetGameplayFloatProperty;     
    uii.Gameplay_SetFloatProperty    = SetGameplayFloatProperty;     
    uii.Gameplay_HasProperty         = HasGameplayProperty;          
    uii.GetPlayerStat                = CL_SafePlayerStat;            

    uii.GetConfigstring   = CL_ConfigString;

    CL_VerifyUIImports();
}

void CL_BeginRegistration(void)
{
    SMP_LOAD_BEGIN( rendreg );
    re.BeginRegistration(&cls.glconfig);
#ifdef __ANDROID__
    {
        extern void CL_TouchRegisterShaders( void );
        CL_TouchRegisterShaders();
    }
#endif
    uWinMan.CleanupShadersFromList();
    // Models have no lazy path: retail re-registers them eagerly, and they MUST be
    uWinMan.RegisterModelsFromList();
    SMP_LOAD_END( rendreg );
}

void CL_EndRegistration(void)
{
    S_EndRegistration();
    re.EndRegistration();
    Com_TouchMemory();
}

void UI_CreateDialog(
    const char *title,
    char       *cvarname,
    const char *command,
    int         width,
    int         height,
    const char *shader,
    const char *okshader,
    const char *cancelshader
)
{
    UIDialog *dlg     = new UIDialog;
    UIRect2D  rect    = UIRect2D((uid.vidWidth - width) / 2, (uid.vidHeight - height) / 2, width, height);
    UColor    bgColor = UColor(0.1f, 0.1f, 0.1f, 1.0f);

    dlg->Create(NULL, rect, title, bgColor, UWhite);
    dlg->LinkCvar(cvarname);
    dlg->SetOKCommand(command);

    if (shader && strlen(shader)) {
        dlg->SetLabelMaterial(uWinMan.RegisterShader(shader));
    }

    if (okshader && strlen(okshader)) {
        dlg->SetOkMaterial(uWinMan.RegisterShader(okshader));
    }

    if (cancelshader && strlen(cancelshader)) {
        dlg->SetCancelMaterial(uWinMan.RegisterShader(cancelshader));
    }

    uWinMan.ActivateControl(dlg);
}

void UI_ResolutionChange(void)
{
    UIRect2D frame;

    CL_FillUIImports();
    CL_FillUIDef();


    if (!uie.ResolutionChange) {
        return;
    }

    if (fakk_console) {
        frame = getDefaultConsoleRectangle();
        fakk_console->setFrame(frame);
    }

    uie.ResolutionChange();
    menuManager.ChangeResolution();
    menuManager.RealignMenus();

    if (view3d) {
        frame = UIRect2D(0, 0, uid.vidWidth, uid.vidHeight);
        view3d->setFrame(frame);
    }

}

void UI_FinishLoadingScreen_f(void)
{
    Com_Unpause();
    UI_ForceMenuOff(qtrue);

    ui_pLoadingMenu        = NULL;
    ui_sCurrentLoadingMenu = "";
    server_loading         = qfalse;
    server_loading_waiting = qfalse;
    menuManager.setLoading(false);

    StopLoadingScreen();

    UI_ActivateView3D();

    // NO PORT-ONLY SORT HERE.  Retail UI_ServerLoaded @0x00197CA0 (228 B, the whole
    // count compensation, and widgetContainerCompare @0x000B4450 never answers 0

    if (com_sv_running->integer) {
        Cbuf_AddText("ui_addhud healthhud\n");
        Cbuf_AddText("ui_addhud radarhud\n");
        Cbuf_AddText("ui_addhud inventoryhud\n");
        Cbuf_AddText("ui_addhud itemTextHud\n");
    }
}


void UI_UpdateMenus(void)
{
    menuManager.UpdateAllMenus();
}

void UI_ServerLoaded(void)
{
    cvar_t *pMaxClients;

    WIDGETM1_PROBE( "svloaded", "state=%d svrun=%d paused=%d serverLoading=%d", cls.state,
        com_sv_running->integer, paused ? paused->integer : -1, UI_IsServerLoading() );

    CL_UpdateSnapFlags();
    // Retail UI_ServerLoaded @0x00197CA0 (228 B) never touches sv.state: its body is

    UI_EndLoad();

    pMaxClients = Cvar_Get("sv_maxclients", "1", 0);

    if (com_sv_running->integer && pMaxClients->integer > 1) {
        SV_Heartbeat_f();
    }

    uWinMan.showCursor(true);

    UI_FinishLoadingScreen_f();

    Cvar_Set("com_canAutoSaveNow", "1");
}

void UI_ClearCenterPrint(void)
{
    if (view3d) {
        view3d->ClearCenterPrint();
    }
}

void UI_UpdateCenterPrint(const char *s, float alpha)
{
    view3d->UpdateCenterPrint(s, alpha);

    if (cg_gametype->integer == 1 || cg_gametype->integer == 2) {
        Cbuf_ExecuteText(EXEC_NOW, va("globalwidgetcommand dm_console print \"%s\n\"\n", s));
    }
}

void UI_UpdateLocationPrint(int x, int y, const char *s, float alpha)
{
    view3d->UpdateLocationPrint(x, y, s, alpha);
}

void UI_CenterPrint_f(void)
{
    const char *s;
    float       alpha;

    if (Cmd_Argc() <= 1) {
        Com_WPrintf("Usage: centerprint string [alpha]\n");           
        return;
    }

    s = Cmd_Argv(1);

    if (Cmd_Argc() > 2) {
        alpha = atof(Cmd_Argv(2));
    } else {
        alpha = 1.0;
    }

    view3d->UpdateCenterPrint(s, alpha);

    if (cg_gametype->integer == 1 || cg_gametype->integer == 2) {
        Cbuf_ExecuteText(EXEC_NOW, va("globalwidgetcommand dm_console print \"%s\n\"\n", s));
    }
}

void UI_LocationPrint_f(void)
{
    int         x, y;
    const char *s;
    float       alpha;

    if (Cmd_Argc() <= 3) {
        Com_WPrintf("Usage: locationprint x y string [alpha]\n");     
        return;
    }

    x = atol(Cmd_Argv(1));
    y = atol(Cmd_Argv(2));

    s = Cmd_Argv(3);

    if (Cmd_Argc() > 4) {
        alpha = atof(Cmd_Argv(4));
    } else {
        alpha = 1.0;
    }

    UI_UpdateLocationPrint(x, y, s, alpha);
}

void UI_LoadInventory_f(void)
{
    CL_LoadInventory(ui_inventoryfile->string, &client_inv);
}

void UI_ClearState(void)
{
    // back to back -- a sequence retail does not have at all (retail reaches
    S_FadeSound(0.0);
}

void UI_CheckRestart(void)
{
    UI_SetupConsoles();

    menuManager.CheckRestart();
}

void UI_ResetCvars(void)
{
    menuManager.ResetCVars();
}

void UI_ShowMouse_f(void)
{
    if (view3d->IsActive()) {
        uWinMan.DeactivateCurrentControl();
    }

    IN_MouseOn();
}

void UI_HideMouse_f(void)
{
    if (!view3d->IsActive()) {
        uWinMan.ActivateControl(view3d);
    }

    IN_MouseOff();
}

static str          loadName;

qboolean UI_IsMenuActive(void)
{
    return menuManager.CurrentMenu() != NULL;
}


void UI_Hud_f(void)
{
    qboolean hide;

    if (Cmd_Argc() != 2) {
        Com_WPrintf("Usage: ui_hud [1|0]");
        return;
    }

    ui_hud = atoi(Cmd_Argv(1));
    hide   = !ui_hud;

    if (hide) {
        if (!hudListHidden) {
            UI_HideHudList();
            hudListHidden = qtrue;
        }
    } else if (hudListHidden) {
        UI_ShowHudList();
        hudListHidden = qfalse;
    }
}

void UI_ClearConsole_f(void)
{
    // (developer) console has no retail counterpart and is not touched here.
    if (fakk_console) {
        fakk_console->Clear();
    }
}

void UI_StartIntro_f(void)
{
    const char *cmd;

    IN_MouseOff();
    cmd = CvarGetForUI("introCommand", "");
    if (cmd && strlen(cmd)) {
        Cbuf_AddText(cmd);
    }
}

static void SaveConsoleRectangle(void)
{
    bool     wasMinimized;
    UIRect2D r;

    if (!fakk_console) {
        return;
    }

    wasMinimized = fakk_console->IsMinimized();

    if (wasMinimized) {
        fakk_console->ProcessEvent(UIFloatingWindow::W_MinimizePressed);
    }

    if (!fakk_console->IsMinimized()) {
        r = fakk_console->getFrame();

        Cvar_Set(
            "ui_consoleposition", va("%d %d %d %d", (int)r.pos.x, (int)r.pos.y, (int)r.size.width, (int)r.size.height)
        );

        if (wasMinimized) {
            fakk_console->ProcessEvent(UIFloatingWindow::W_MinimizePressed);
        }
    }
}

static void LoadConsoleRectangle(void)
{
    bool wasMinimized;

    if (!fakk_console) {
        return;
    }

    wasMinimized = fakk_console->IsMinimized();

    if (wasMinimized) {
        fakk_console->ProcessEvent(UIFloatingWindow::W_MinimizePressed);
    }

    if (!fakk_console->IsMinimized() && wasMinimized) {
        fakk_console->ProcessEvent(UIFloatingWindow::W_MinimizePressed);
    }
}

void UI_AddHud(const char *name, int show)
{
    Menu *hud = menuManager.FindMenu(name);

    if (!hud) {
        Com_WDPrintf("Hud %s not found in menu system.\n", Cmd_Argv(1));
        return;
    }

    if (hudList.ObjectInList(hud)) {
        if (hudListHidden) {
            hud->ForceHide();
        } else {
            hud->ForceShow();
        }
    } else {
        hudList.AddObject(hud);
        if (!hudListHidden || show) {
            hud->ProcessEvent(new Event(EV_ShowMenu));
        } else {
            hud->ForceHide();
        }
    }

    UI_ApplyHudScale(hud);
    UI_RelayoutHudMenu(hud);
}

void UI_RemoveHud(const char *name)
{
    Menu *hud = menuManager.FindMenu(name);

    if (hud && hudList.ObjectInList(hud)) {
        hudList.RemoveObject(hud);
        hud->ProcessEvent(new Event(EV_HideMenu));

        if (hud->GetContainerWidget()) {
            hud->GetContainerWidget()->setHudScale(1.0f);
        }
    }
}

void UI_AddHud_f(void)
{
    if (Cmd_Argc() < 2) {
        UI_AddHud(Cmd_Argv(1), 0);
    } else {
        UI_AddHud(Cmd_Argv(1), atoi(Cmd_Argv(2)));
    }
}

void UI_RemoveHud_f(void)
{
    UI_RemoveHud(Cmd_Argv(1));
}

void UI_StartServer_f(void)
{
    const char *map = Cvar_VariableString("ui_startmap");

    if (com_dedicated->modified) {
        Cvar_Get("dedicated", "0", 0);            
        com_dedicated->modified = qfalse;         
        if (com_dedicated->integer) {             
            CL_Shutdown();                        
            Sys_ShowConsole(1, qtrue);            
        } else {
            Sys_ShowConsole(com_viewlog->integer, qfalse);  
        }
    }

    if (strlen(map)) {
        Cbuf_AddText(va("map %s\n", map));        
    }
}

void UI_SalesScreen_f()
{
    if (random() > 0.5) {
        Cbuf_AddText("pushmenu sales_splash1\n");
    } else {
        Cbuf_AddText("pushmenu sales_splash2\n");
    }
}

void CL_ShutdownUI(void)
{
    if (!cls.uiStarted) {
        return;
    }

    if (s_intermediateconsole.NumObjects()) {
        int i;

        for (i = 1; i <= s_intermediateconsole.NumObjects(); i++) {
            intermediate_console_text_t *record = s_intermediateconsole.ObjectAt(i);

            if (record) {
                if (record->text) {
                    delete record->text;
                }

                delete record;
            }
        }

        s_intermediateconsole.FreeObjectList();
    }

    {
        fileHandle_t f;
        int          numGlobal = cls.numglobalservers;
        int          numFav    = cls.numfavoriteservers;
        int          checksum  = numGlobal + numFav;

        f = FS_FOpenFileWrite_HomeData("serverlist.dat");
        if (f) {
            FS_Write(&numGlobal, 4, f);
            FS_Write(&numFav, 4, f);
            FS_Write(&checksum, 4, f);
            FS_Write(cls.globalServers, sizeof(cls.globalServers), f);
            FS_Write(cls.favoriteServers, sizeof(cls.favoriteServers), f);
            FS_FCloseFile(f);
        }
    }


    Cmd_RemoveCommand("flushui");
    Cmd_RemoveCommand("currentmenu");
    Cmd_RemoveCommand("pushmenu");
    Cmd_RemoveCommand("hidemenu");
    Cmd_RemoveCommand("forcemenu");
    Cmd_RemoveCommand("popmenu");
    Cmd_RemoveCommand("transitionmenu");
    Cmd_RemoveCommand("showmenu");
    Cmd_RemoveCommand("widgetcommand");
    Cmd_RemoveCommand("globalwidgetcommand");
    Cmd_RemoveCommand("listmenus");
    Cmd_RemoveCommand("togglemenu");
    Cmd_RemoveCommand("loadmenu");
    Cmd_RemoveCommand("maplist");
    Cmd_RemoveCommand("availablemaps");
    Cmd_RemoveCommand("viewspawnlist");
    Cmd_RemoveCommand("viewspawnquicklist");
    Cmd_RemoveCommand("viewsetdialog");
    Cmd_RemoveCommand("soundpicker");
    Cmd_RemoveCommand("notepad");
    Cmd_RemoveCommand("editscript");
    Cmd_RemoveCommand("editshader");
    Cmd_RemoveCommand("inv_restart");
    Cmd_RemoveCommand("inventory");
    Cmd_RemoveCommand("warpinv");
    Cmd_RemoveCommand("nextinv");
    Cmd_RemoveCommand("previnv");
    Cmd_RemoveCommand("ui_disableinventory");
    Cmd_RemoveCommand("ui_enableinventory");
    Cmd_RemoveCommand("ui_showmouse");
    Cmd_RemoveCommand("ui_hidemouse");
    Cmd_RemoveCommand("ui_saveconsolepos");
    Cmd_RemoveCommand("ui_loadconsolepos");
    Cmd_RemoveCommand("clear");
    Cmd_RemoveCommand("ui_hud");
    Cmd_RemoveCommand("ui_resetcvars");
    Cmd_RemoveCommand("ui_setrangecvar");
    Cmd_RemoveCommand("ui_verifycvarrange");
    Cmd_RemoveCommand("ui_checkrestart");
    Cmd_RemoveCommand("dointro");
    Cmd_RemoveCommand("_intro");
    Cmd_RemoveCommand("centerprint");
    Cmd_RemoveCommand("locationprint");
    Cmd_RemoveCommand("saveweaps");
    Cmd_RemoveCommand("ui_addhud");
    Cmd_RemoveCommand("ui_removehud");
    Cmd_RemoveCommand("ui_removehuds");
    Cmd_RemoveCommand("startserver");
    Cmd_RemoveCommand("displaybranchdialog");
    Cmd_RemoveCommand("ui_connectspecifiedserver");
    Cmd_RemoveCommand("ui_newserverlist");
    Cmd_RemoveCommand("ui_refreshserverlist");
    Cmd_RemoveCommand("ui_setselectedserver");
    Cmd_RemoveCommand("ui_buildserverlist");
    Cmd_RemoveCommand("ui_getserverstatus");
    Cmd_RemoveCommand("ui_displayserverinformation");
    Cmd_RemoveCommand("ui_addfavoriteserver");
    Cmd_RemoveCommand("ui_addfavoritespecserver");
    Cmd_RemoveCommand("ui_deletefavoriteserver");
    Cmd_RemoveCommand("ui_enablefavoritebutton");
    Cmd_RemoveCommand("ui_setloadsavegamelist");
    Cmd_RemoveCommand("ui_getsavegames");
    Cmd_RemoveCommand("ui_getsavegameinfo");
    Cmd_RemoveCommand("ui_loadgame");
    Cmd_RemoveCommand("ui_promptsavegame");
    Cmd_RemoveCommand("ui_savegame");
    Cmd_RemoveCommand("ui_savegamewithname");
    Cmd_RemoveCommand("ui_autosavegame");
    Cmd_RemoveCommand("ui_deletesavegame");
    Cmd_RemoveCommand("ui_loadlastsave");
    Cmd_RemoveCommand("ui_quicksave");
    Cmd_RemoveCommand("ui_quickload");
    Cmd_RemoveCommand("ui_showjoinbuttons");

    if (arenaController) {
        arenaController->unregisterCommands();
    }

    for (int i = hudList.NumObjects(); i > 0; i--) {
        hudList.RemoveObjectAt(i);
    }

    if (view3d) {
        delete view3d;
        view3d = NULL;
    }
    if (mini_console) {
        delete mini_console;
        mini_console = NULL;
    }
    if (fakk_console) {
        delete fakk_console;
        fakk_console = NULL;
    }

    hud_health = NULL;
    hud_boss = NULL;
    hud_levelExit = NULL;
    hud_targetName = NULL;
    printhud = NULL;
    itemTextHud = NULL;
    poweruphud = NULL;
    radarhud = NULL;
    missionfailure = NULL;
    missionsuccess = NULL;
    headhud = NULL;
    headtexthud = NULL;
    texthud = NULL;
    specialmovetimer = NULL;
    pointshud = NULL;

    deleteAllWeaponHuds();

    UI_DeleteScoreboard();

    client_inv.Clear();

    menuManager.DeleteAllMenus();

    cls.uiStarted = false;
}

#define UI_OLD_API_VERSION 4

static void UI_SetRangeCvar(void)
{
    float       value, minv, maxv;
    char        buf[64];
    const char *text;

    value = atof(Cmd_Argv(2));
    text  = Cmd_Argv(3);
    minv  = *text >= '0' && *text <= '9' ? atof(text) : Cvar_VariableValue(text);
    text  = Cmd_Argv(4);
    maxv  = *text >= '0' && *text <= '9' ? atof(text) : Cvar_VariableValue(text);

    if (value > maxv) {
        value = maxv;
    }
    if (value < minv) {
        value = minv;
    }

    if (Cmd_Argc() > 4 && !Q_stricmp(Cmd_Argv(5), "float")) {
        Com_sprintf(buf, sizeof(buf), "%0.1f", value);
    } else {
        Com_sprintf(buf, sizeof(buf), "%d", (int)value);
    }

    Cvar_Set(Cmd_Argv(1), buf);
}

static void UI_VerifyCvarRange(void)
{
    float       value, minv, maxv;
    char        buf[64];
    const char *text;

    if (Cmd_Argc() < 4) {
        return;
    }

    text = Cmd_Argv(2);
    minv = *text >= '0' && *text <= '9' ? atof(text) : Cvar_VariableValue(text);
    text = Cmd_Argv(3);
    maxv = *text >= '0' && *text <= '9' ? atof(text) : Cvar_VariableValue(text);

    value = Cvar_VariableValue(Cmd_Argv(1));

    if (value > maxv) {
        value = maxv;
    }
    if (value < minv) {
        value = minv;
    }

    if (Cmd_Argc() > 3 && !Q_stricmp(Cmd_Argv(5), "float")) {
        Com_sprintf(buf, sizeof(buf), "%0.1f", value);
    } else {
        Com_sprintf(buf, sizeof(buf), "%d", (int)value);
    }

    Cvar_Set(Cmd_Argv(1), buf);
}

// (retail registers "unpause" from Com_Init, not from CL_InitializeUI; the


static char     ui_saveListWidget[64];
static qboolean ui_saveListIsSave;
static char     ui_selectedSaveName[64];

typedef struct uisaveentry_s {
    char comment[64];
    char saveName[64];
    int  secondsPlayed;
    int  time;
} uisaveentry_t;

static int UI_SaveEntryCompare(const void *a, const void *b)
{
    const uisaveentry_t *sa = (const uisaveentry_t *)a;
    const uisaveentry_t *sb = (const uisaveentry_t *)b;

    if (sb->time > sa->time) {
        return 1;
    }
    if (sb->time < sa->time) {
        return -1;
    }
    return 0;
}

static void UI_SaveSanitize(char *dst, const char *src, size_t dstlen)
{
    size_t i;

    for (i = 0; i + 1 < dstlen && src[i]; i++) {
        dst[i] = (src[i] == '"') ? '\'' : src[i];
    }
    dst[i] = 0;
}

static void UI_SetLoadSaveGameList_f(void)
{
    if (Cmd_Argc() < 3) {
        Com_Printf("usage: ui_setloadsavegamelist <load/save list widget> <0/1>\n");
        return;
    }

    Q_strncpyz(ui_saveListWidget, Cmd_Argv(1), sizeof(ui_saveListWidget));
    ui_saveListIsSave      = (qboolean)(atoi(Cmd_Argv(2)) != 0);
    ui_selectedSaveName[0] = 0;
}

static void UI_GetSavedGames_f(void)
{
    const char    *folder;
    char         **filenames;
    int            numfiles;
    int            i;
    int            count;
    uisaveentry_t *entries;

    if (!ui_saveListWidget[0] || !Q_stricmp(ui_saveListWidget, "EMPTY")) {
        return;
    }

    Cbuf_AddText(va("globalwidgetcommand %s deleteallitems\n", ui_saveListWidget));

    if (ui_saveListIsSave) {
        Cbuf_AddText(va("globalwidgetcommand %s addrow \"[$$Empty$$]\" \"\" \"\" \"empty\"\n", ui_saveListWidget));
    }

    folder    = Com_GetArchiveFolder();
    filenames = FS_ListFiles(folder, "ssv", qfalse, &numfiles);

    entries = new uisaveentry_t[numfiles > 0 ? numfiles : 1];
    count   = 0;

    for (i = 0; i < numfiles; i++) {
        str              work;
        fileHandle_t     f;
        savegamestruct_t save;

        work = folder;
        work += "/";
        work += filenames[i];

        FS_FOpenFileRead(work.c_str(), &f, qfalse, qtrue);
        if (!f) {
            continue;
        }

        FS_Read(&save, sizeof(save), f);
        FS_FCloseFile(f);


        if (ui_saveListIsSave && !Q_stricmp(save.saveName, "quicksave")) {
            continue;
        }

        Q_strncpyz(entries[count].comment, save.comment, sizeof(entries[count].comment));
        Q_strncpyz(entries[count].saveName, save.saveName, sizeof(entries[count].saveName));
        entries[count].secondsPlayed = save.secondsPlayed;
        entries[count].time    = save.time;
        count++;
    }

    FS_FreeFileList(filenames);

    qsort(entries, count, sizeof(uisaveentry_t), UI_SaveEntryCompare);

    for (i = 0; i < count; i++) {
        char   comment[64];
        char   name[64];
        char   elapsed[32];
        char   date[64];
        int    seconds = entries[i].secondsPlayed;
        time_t t       = (time_t)entries[i].time;

        Com_sprintf(elapsed, sizeof(elapsed), "%02d:%02d:%02d", seconds / 3600, (seconds % 3600) / 60, seconds % 60);
        strftime(date, sizeof(date), "%a %b %d %Y %H:%M:%S", localtime(&t));

        UI_SaveSanitize(comment, entries[i].comment, sizeof(comment));
        UI_SaveSanitize(name, entries[i].saveName, sizeof(name));

        Cbuf_AddText(va(
            "globalwidgetcommand %s addrow \"%s\" \"%s\" \"%s\" \"%s\" \"%d\"\n",
            ui_saveListWidget,
            comment,
            elapsed,
            date,
            name,
            entries[i].time
        ));
    }

    delete[] entries;

    Com_DPrintf("ui_getsavegames: %d .ssv files found, %d rows added to '%s'\n", numfiles, count, ui_saveListWidget);

    if (count > 0) {
        Cbuf_AddText(va("globalwidgetcommand %s selectrow %d\n", ui_saveListWidget, ui_saveListIsSave ? 2 : 1));
    }
}

static savegamestruct_t *UI_FindSaveGame(const char *name)
{
    static savegamestruct_t save;
    const char             *folder;
    char                   **filenames;
    int                     numfiles;
    int                     i;

    folder    = Com_GetArchiveFolder();
    filenames = FS_ListFiles(folder, "ssv", qfalse, &numfiles);

    for (i = 0; i < numfiles; i++) {
        str         work;
        fileHandle_t f;

        work = folder;
        work += "/";
        work += filenames[i];

        FS_FOpenFileRead(work, &f, qfalse, qtrue);
        if (!f) {
            continue;
        }

        FS_Read(&save, sizeof(savegamestruct_t), f);
        FS_FCloseFile(f);

        if (!strcmp(name, save.saveName)) {
            return &save;
        }
    }

    return NULL;
}

static void UI_GetSaveGameInfo_f(void)
{
    str name;
    GameplayManager *gpm;

    if (Cmd_Argc() < 1) {
        Com_Printf("usage: selectedsavegame <savegamename>\n");
        return;
    }

    name = Cmd_Argv(1);
    gpm  = GameplayManager::getTheGameplayManager();

    if (stricmp(name, "EMPTY")) {
        savegamestruct_t *save = UI_FindSaveGame(name);
        str              shot;

        if (!save) {
            return;
        }

        shot = Com_GetArchiveFileName(name, name, "tga", qfalse);

        gpm->setStringValue("LoadSaveGame", "ScreenShotShader", shot);
        gpm->setFloatValue("LoadSaveGame", "EnableLoadSaveButton", 1.0f);
        gpm->setFloatValue("LoadSaveGame", "EnableDeleteButton", 1.0f);
        gpm->setStringValue("LoadSaveGame", "SaveGameName", save->saveName);

        Q_strncpyz(ui_selectedSaveName, save->saveName, sizeof(ui_selectedSaveName));
    } else {
        gpm->setStringValue("LoadSaveGame", "ScreenShotShader", "sysimg/2d/default-map-screen.tga");
        gpm->setStringValue("LoadSaveGame", "SaveGameName", "EMPTY");

        // bridge's selection restores retail's semantics: sentinel -> nothing selected.
        ui_selectedSaveName[0] = 0;
    }
}

static void UI_LoadGame_f(void)
{
    if (!ui_selectedSaveName[0]) {
        return;
    }

    Cbuf_AddText(va("loadgame %s\n", ui_selectedSaveName));
}

static void UI_PromptSaveGame_f(void)
{
    if (ui_selectedSaveName[0]) {
        Cbuf_AddText("showmenu overwritesavegame\n");
    } else {
        Cbuf_AddText("showmenu newsavegame\n");
    }
}

static void UI_SaveGame_f(void)
{
    int overwrite = (Cmd_Argc() > 1) ? atoi(Cmd_Argv(1)) : 0;

    if (overwrite) {
        if (!ui_selectedSaveName[0]) {
            return;
        }
        Cbuf_AddText(va("savegame %s\n", ui_selectedSaveName));
        return;
    }

    const char *name = Cvar_Get("savegamename", "", 0)->string;
    if (name && name[0]) {
        Cbuf_AddText(va("savegame %s\n", name));
    } else {
        Cbuf_AddText(va("savegame save%d\n", (int)time(NULL)));
    }
}

static void UI_SaveGameWithName_f(void)
{
    const char *name = (Cmd_Argc() > 1) ? Cmd_Argv(1) : Cvar_Get("savegamename", "", 0)->string;

    if (name && name[0]) {
        Cbuf_AddText(va("savegame %s\n", name));
    }
}

static void UI_DeleteSaveGame_f(void)
{
    if (!ui_selectedSaveName[0]) {
        return;
    }

    Com_WipeSavegame(ui_selectedSaveName, qfalse);
    ui_selectedSaveName[0] = 0;

    if (ui_saveListWidget[0]) {
        Cbuf_AddText(va("globalwidgetcommand %s deleteallitems\n", ui_saveListWidget));
        Cbuf_AddText("ui_getsavegames\n");
    }
}

static void UI_LoadLastSave_f(void)
{
    const char *folder;
    char      **filenames;
    int         numfiles;
    int         i;
    int         bestTime = -1;
    char        bestName[64];

    bestName[0] = 0;
    folder      = Com_GetArchiveFolder();
    filenames   = FS_ListFiles(folder, "ssv", qfalse, &numfiles);

    for (i = 0; i < numfiles; i++) {
        str              work;
        fileHandle_t     f;
        savegamestruct_t save;

        work = folder;
        work += "/";
        work += filenames[i];

        FS_FOpenFileRead(work.c_str(), &f, qfalse, qtrue);
        if (!f) {
            continue;
        }

        FS_Read(&save, sizeof(save), f);
        FS_FCloseFile(f);

        if (save.time > bestTime) {
            bestTime = save.time;
            Q_strncpyz(bestName, save.saveName, sizeof(bestName));
        }
    }

    FS_FreeFileList(filenames);

    if (bestName[0]) {
        Cbuf_ExecuteText(EXEC_NOW, va("loadgame %s", bestName));
    } else {
        Com_Printf("loadLastSaveGame: There are no save games to load\n");
        Cbuf_ExecuteText(EXEC_NOW, "showmenu OkDialog\n");
        Cbuf_ExecuteText(EXEC_NOW,
                         "globalwidgetcommand OkDialogTitle title \"$$NoSaveGamesToLoad$$\"\n");
    }
}

static void UI_QuickSave_f(void)
{
    if (!s_loadSaveGameManager) {
        CreateLoadSaveGameManager();
    }
    s_loadSaveGameManager->quickSave();
}

static void UI_QuickLoad_f(void)
{
    if (!s_loadSaveGameManager) {
        CreateLoadSaveGameManager();
    }
    s_loadSaveGameManager->quickLoad();
}

static void UI_AutoSaveGame_f(void)
{
    const char *map = Cvar_Get("mapname", "", 0)->string;

    if (!s_loadSaveGameManager) {
        CreateLoadSaveGameManager();
    }
    s_loadSaveGameManager->autoSave(map ? map : "");
}


static cvar_t *ac_mp_modifier_InstantKill;              
static cvar_t *ac_mp_modifier_ActionHero;               
static cvar_t *ac_mp_modifier_AutoHandicap;             
static cvar_t *ac_mp_modifier_PointsPerWeapon;          
static cvar_t *ac_mp_modifier_ControlPoints;            
static cvar_t *ac_mp_modifier_Destruction;              
static cvar_t *ac_mp_modifier_OneFlag;                  
static cvar_t *ac_mp_modifier_Elimination;              
static cvar_t *ac_mp_modifier_Specialties;              
static cvar_t *ac_temp_sv_hostname;                     
static cvar_t *ac_temp_sv_maxspeed;                     
static cvar_t *ac_temp_sv_maxclients;                   
static cvar_t *ac_temp_mp_gametype;                     
static cvar_t *ac_temp_mp_gametypename;                 
static cvar_t *ac_temp_password;                        
static cvar_t *ac_temp_mp_botskilllevel;                
static cvar_t *ac_temp_mp_pointlimit;                   
static cvar_t *ac_temp_mp_timelimit;                    
static cvar_t *ac_temp_mp_flags;                        
static cvar_t *ac_temp_mp_itemRespawnMultiplier;        
static cvar_t *ac_temp_mp_weaponRespawnMultiplier;      
static cvar_t *ac_temp_mp_powerupRespawnMultiplier;     
static cvar_t *ac_temp_mp_knockbackMultiplier;          
static cvar_t *ac_temp_mp_damageMultiplier;             
static cvar_t *ac_temp_mp_respawnInvincibilityTime;     
static cvar_t *ac_temp_mp_warmUpTime;                   
static cvar_t *ac_temp_mp_minPlayers;                   
static cvar_t *ac_temp_dedicated;                       
static cvar_t *ac_temp_sv_pure;                         
static cvar_t *ac_temp_strafeJumpingAllowed;            
static cvar_t *ac_temp_sv_maxrate;                      
static cvar_t *ac_temp_mp_modifier_InstantKill;         
static cvar_t *ac_temp_mp_modifier_ActionHero;          
static cvar_t *ac_temp_mp_modifier_AutoHandicap;        
static cvar_t *ac_temp_mp_modifier_PointsPerWeapon;     
static cvar_t *ac_temp_mp_modifier_ControlPoints;       
static cvar_t *ac_temp_mp_modifier_Destruction;         
static cvar_t *ac_temp_mp_modifier_OneFlag;             
static cvar_t *ac_temp_mp_modifier_Elimination;         
static cvar_t *ac_temp_mp_modifier_Specialties;         

static int CheckForMdsRegistry(void)
{
    return 0;
}

ArenaController *arenaController;


BotAssignment::BotAssignment(void)
{
    m_name      = "";
    m_team      = "";
    m_specialty = "";
    m_open      = true;
}

bool BotAssignment::parseProperties(Script& script)
{
    str token;

    token = script.GetToken(true);
    if (token[0] != '{') {
        return false;
    }

    while (1) {
        if (!script.TokenAvailable(true)) {
            return false;
        }

        token = script.GetToken(false);

        if (!token.icmp("}")) {
            break;
        } else if (!token.icmp("name")) {
            m_name = script.GetToken(false);
            m_open = false;
        } else if (!token.icmp("team")) {
            m_team = script.GetToken(false);
        } else if (!token.icmp("specialty")) {
            m_specialty = script.GetToken(false);
        } else {
            Com_WPrintf("Unknown bot assignment property: %s\n", token.c_str());
        }
    }

    return true;
}

void BotAssignment::clear(void)
{
    m_name      = "";
    m_team      = "Red";
    m_specialty = "Infiltrator";
    m_open      = true;
}

BotAssignment& BotAssignment::operator=(BotAssignment& assignment)
{
    m_name      = assignment.m_name;
    m_team      = assignment.m_team;
    m_specialty = assignment.m_specialty;
    m_open      = assignment.m_open;

    return *this;
}


BotProperties::BotProperties(void)
{
    m_name             = "";
    m_screenshot       = "";
    m_unknown56        = 1;
    m_defaultSpecialty = "None";
    m_unknown88        = "";
    m_playerModel      = "";
}

BotProperties::~BotProperties(void)
{
}

bool BotProperties::parseBotSection(Script& script)
{
    str token;

    while (1) {
        if (!script.TokenAvailable(true)) {
            return false;
        }

        token = script.GetToken(false);

        if (token[0] == '}') {
            break;
        } else if (!token.icmp("name")) {
            m_name = script.GetToken(false);
        } else if (!token.icmp("screenshot")) {
            m_screenshot = script.GetToken(false);
        } else if (!token.icmp("mp_playermodel")) {
            m_playerModel = script.GetToken(false);
        }
    }

    return true;
}


LevelProperties::LevelProperties(void)
{
    m_mapName            = "";
    m_longMapName        = "";
    m_screenshot         = "";
    m_recommendedPlayers = va("%d", 0);
    m_fraglimit          = 0;
    m_specialties        = false;
    m_bots               = true;
}

LevelProperties::~LevelProperties(void)
{
    int i;

    for (i = 1; i <= m_botAssignments.NumObjects(); i++) {
        delete m_botAssignments.ObjectAt(i);
    }

    m_botAssignments.ClearObjectList();
    m_gameTypes.ClearObjectList();
}

bool LevelProperties::readFile(const str& name)
{
    Script script;
    str    token;

    script.LoadFile(name.c_str());

    if (script.isValid()) {
        do {
            if (!script.TokenAvailable(true)) {
                return true;    
            }

            token = script.GetToken(false);
        } while (token.icmp("LevelProperties") || _parseProperties(script));

        return false;           
    }

    return false;               
}

bool LevelProperties::hasGameType(const str& gameType)
{
    int i;

    if (!gameType.length()) {
        return false;
    }

    for (i = 1; i <= m_gameTypes.NumObjects(); i++) {
        if (!m_gameTypes.ObjectAt(i).icmp(gameType)) {
            return true;
        }
    }

    return false;
}

bool LevelProperties::hasMatchType(const str& matchType)
{
    int i;

    if (!matchType.length()) {
        return false;
    }

    if (m_matchTypes.NumObjects() < 1) {
        return true;
    }

    for (i = 1; i <= m_matchTypes.NumObjects(); i++) {
        if (!m_matchTypes.ObjectAt(i).icmp(matchType)) {
            return true;
        }
    }

    return false;
}

void LevelProperties::setSupportedModifiers(void)
{
    str name;
    int i;

    for (i = 1; i <= m_modifiers.NumObjects(); i++) {
        name = "mp_map_";
        name += m_modifiers.ObjectAt(i).c_str();
        Cvar_Set(name.c_str(), "1");
    }

    if (m_specialties == 1) {
        Cvar_Set("mp_map_specialties", "1");
    }
}

bool LevelProperties::_parseProperties(Script& script)
{
    str            token;
    str            value;
    BotAssignment *assignment;

    token = script.GetToken(true);
    if (token[0] != '{') {
        return false;
    }

    while (1) {
        if (!script.TokenAvailable(true)) {
            return false;
        }

        token = script.GetToken(false);

        if (!token.icmp("}")) {
            break;
        } else if (!token.icmp("mapname")) {
            m_mapName = script.GetToken(false);
        } else if (!token.icmp("longmapname")) {
            m_longMapName = script.GetToken(false);
        } else if (!token.icmp("screenshot")) {
            m_screenshot = script.GetToken(false);
        } else if (!token.icmp("botassignment")) {
            assignment = new BotAssignment();
            if (assignment->parseProperties(script)) {
                m_botAssignments.AddObject(assignment);
            } else {
                delete assignment;
            }
        } else if (!token.icmp("gametype")) {
            value = script.GetToken(false);
            m_gameTypes.AddObject(value);
        } else if (!token.icmp("matchtype")) {
            value = script.GetToken(false);
            m_matchTypes.AddObject(value);
        } else if (!token.icmp("modifier")) {
            value = script.GetToken(false);
            m_modifiers.AddObject(value);
        } else if (!token.icmp("specialties")) {
            m_specialties = true;
        } else if (!token.icmp("recommendedPlayers")) {
            m_recommendedPlayers = script.GetToken(false);
        } else if (!token.icmp("fraglimit")) {
            m_fraglimit = atoi(script.GetToken(false));
        } else if (!token.icmp("bots")) {
            m_bots = atoi(script.GetToken(false)) != 0;
        } else {
            Com_WPrintf("Unknown level property: %s\n", token.c_str());
        }
    }

    return true;
}


static void UI_ReadDMModels_f(void)
{
    str directory;
    int i;

    if (Cmd_Argc() < 1) {
        Com_Printf(" usage: ui_readdmmodels <directory1> <directory2> ...\n");
        return;
    }

    arenaController->clearDMModels();

    for (i = 1; i < Cmd_Argc(); i++) {
        directory = Cmd_Argv(i);
        arenaController->readDMModels(directory);
    }
}

static void UI_GetDMModels_f(void)
{
    str widget;
    int i;

    if (Cmd_Argc() < 1) {
        Com_Printf("usage: ui_getdmmodels <widgetname>\n");
        return;
    }

    widget = Cmd_Argv(1);

    if (!widget.length()) {
        return;
    }

    for (i = 1; i <= arenaController->m_dmModels.NumObjects(); i++) {
        Cbuf_ExecuteText(
            EXEC_NOW,
            va("globalwidgetcommand %s addrowitem sysimg/headshots/%s.tga models/char/%s.tik\n",
               widget.c_str(),
               arenaController->m_dmModels.ObjectAt(i).c_str(),
               arenaController->m_dmModels.ObjectAt(i).c_str()));
    }
}

static void UI_SetLevelList_f(void)
{
    str widget;

    if (Cmd_Argc() < 1) {
        Com_Printf("usage: ui_setmaplist <maplistwidget>\n");
        return;
    }

    widget                           = Cmd_Argv(1);
    arenaController->m_mapListWidget = widget;
}

static void UI_GetLevels_f(void)
{
    str matchType;
    str gameType;

    matchType = "2";

    if (Cmd_Argc() > 1) {
        matchType = Cmd_Argv(1);
    }

    gameType = Cvar_VariableString("temp_mp_gametypename");
    arenaController->getLevels(gameType, matchType);
}

static void UI_ReadtLevelProperties_f(void)
{
    str directory;
    int i;

    if (Cmd_Argc() < 1) {
        Com_Printf("usage: ui_readlevelproperties <directory1> <directory2> ...\n");
        return;
    }

    arenaController->clearLevelProperties();

    for (i = 1; i < Cmd_Argc(); i++) {
        directory = Cmd_Argv(i);
        arenaController->readLevelProperties(directory);
    }
}

static void UI_SetSelectedLevel_f(void)
{
    str levelName;

    if (Cmd_Argc() < 1) {
        Com_Printf("usage: ui_setselectedlevel <showbotassignments> <mapname>\n");
        return;
    }

    levelName = Cmd_Argv(2);
    arenaController->setSelectedLevel(levelName, atoi(Cmd_Argv(1)));
}

static void UI_SetBotList_f(void)
{
    str     widget;
    bool    rowBased;

    if (Cmd_Argc() < 1) {
        Com_Printf("usage: ui_setbotlist <bot list widget> <rowbased>\n");
        return;
    }

    rowBased = false;
    if (Cmd_Argc() > 1) {
        rowBased = atoi(Cmd_Argv(2)) != 0;
    }

    widget                             = Cmd_Argv(1);
    arenaController->m_botListWidget   = widget;
    arenaController->m_botListRowBased = rowBased;
}

static void UI_SetBotSelectList_f(void)
{
    str widget;

    if (Cmd_Argc() < 1) {
        Com_Printf("usage: ui_setbotselectlist <bot list widget>\n");
        return;
    }

    widget                                 = Cmd_Argv(1);
    arenaController->m_botSelectListWidget = widget;
}

static void UI_StartLevelPrompt_f(void)
{
    if (cls.state < CA_CONNECTED) {
        arenaController->startLevel();
    } else {
        Cbuf_ExecuteText(EXEC_NOW, "showmenu startserverprompt\n");
    }
}

static void UI_StartLevel_f(void)
{
    arenaController->startLevel();
}

static void UI_ConnectServerPrompt_f(void)
{
    const char *address;

    address = Cmd_Argv(1);
    if (address) {
        arenaController->m_serverAddress = address;
    } else {
        arenaController->m_serverAddress = "";
    }

    if (cls.state < CA_CONNECTED) {
        arenaController->connectToServer();
    } else {
        Cbuf_ExecuteText(EXEC_NOW, "showmenu connectserverprompt\n");
    }
}

static void UI_ConnectServer_f(void)
{
    arenaController->connectToServer();
}

static void UI_ResetServerScreen_f(void)
{
    str objectName;
    str propertyName;

    objectName   = "Multiplayer";
    propertyName = "MapScreenShot";
    GameplayManager::getTheGameplayManager()->setStringValue(
        objectName,
        propertyName,
        arenaController->m_defaultMapScreen,
        false);
}

static void UI_GetAvailableBots_f(void)
{
    arenaController->getAvailableBots();
}

static void UI_SetSelectedBot_f(void)
{
    str            name;
    str            model;
    BotProperties *bot;
    int            i;

    if (Cmd_Argc() < 1) {
        Com_Printf("usage: ui_setSelectedBot <name>\n");
        return;
    }

    name                           = Cmd_Argv(1);
    arenaController->m_selectedBot = name;

    model = Cmd_Argv(1);

    bot = NULL;
    for (i = 1; i <= arenaController->m_botProperties.NumObjects(); i++) {
        if (arenaController->m_botProperties.ObjectAt(i)->m_name == model) {
            bot = arenaController->m_botProperties.ObjectAt(i);
            break;
        }
    }

    if (bot) {
        Cbuf_ExecuteText(
            EXEC_NOW,
            va("globalwidgetcommand BotView rendermodel %s\n", bot->m_playerModel.c_str()));
    }
}

static void UI_SetSelectedBotAssignment_f(void)
{
    if (Cmd_Argc() < 1) {
        Com_Printf("ui_setselectedbotassignment <index>\n");
        return;
    }

    arenaController->m_selectedBotAssignment = atoi(Cmd_Argv(1)) - 1;
}

static void UI_GetBotAssignments_f(void)
{
    arenaController->getBotAssignments();
}

static void UI_UpdateBotAssignments_f(void)
{
    arenaController->getLevelBotAssignments(atoi(Cmd_Argv(1)));
}

static void UI_AssignBotSelections_f(void)
{
    arenaController->assignBotSelections();
}

static void UI_ToggleSlot_f(void)
{
    unsigned int slot;

    if (Cmd_Argc() < 1) {
        Com_Printf("ui_toggleslot <index>\n");
        return;
    }

    slot = atoi(Cmd_Argv(1)) - 1;

    if (slot > 6) {
        Com_Printf("Index out of range: %d\n", (int)slot);
        return;
    }

    if (arenaController->m_botAssignments[slot].m_open) {
        arenaController->m_botAssignments[slot].m_open = false;
    } else {
        arenaController->m_botAssignments[slot].m_open = true;
    }

    arenaController->setBotRow(arenaController->m_botAssignments[slot], slot + 1);
}

static void UI_SetBotTeam_f(void)
{
    str          team;
    unsigned int slot;

    if (Cmd_Argc() < 2) {
        Com_Printf("usage: ui_setbotteam <index> <team>\n");
        return;
    }

    slot = atoi(Cmd_Argv(1)) - 1;
    team = Cmd_Argv(2);

    if (slot <= 6) {
        arenaController->m_botAssignments[slot].m_team = team;
    }
}

static void UI_SetBotSpecialty_f(void)
{
    str          specialty;
    unsigned int slot;

    if (Cmd_Argc() < 2) {
        Com_Printf("usage: ui_setbotspecialty <index> <team>\n");
        return;
    }

    slot      = atoi(Cmd_Argv(1)) - 1;
    specialty = Cmd_Argv(2);

    if (slot <= 6) {
        arenaController->m_botAssignments[slot].m_specialty = specialty;
    }
}

static void UI_SetVoteOptionWidget_f(void)
{
    str widget;

    if (Cmd_Argc() < 1) {
        Com_Printf("usage: ui_setvoteoptionwidget <vote option widget>\n");
        return;
    }

    widget                              = Cmd_Argv(1);
    arenaController->m_voteOptionWidget = widget;
}

static void UI_SetVoteType_f(void)
{
    str voteType;

    if (Cmd_Argc() < 1) {
        Com_Printf("usage: ui_setvotetype <votetype>\n");
        return;
    }

    voteType = Cmd_Argv(1);
    arenaController->setVoteType(voteType);
}

static void UI_SetVoteOption_f(void)
{
    str voteOption;

    if (Cmd_Argc() < 1) {
        Com_Printf("usage: ui_setvoteoption <voteoption>\n");
        return;
    }

    voteOption                    = Cmd_Argv(1);
    arenaController->m_voteOption = voteOption;
}

static void UI_CallVote_f(void)
{
    if (arenaController->m_voteType.length() && arenaController->m_voteOption.length()) {
        Cbuf_ExecuteText(
            EXEC_NOW,
            va("callvote %s %s\n",
               arenaController->m_voteType.c_str(),
               arenaController->m_voteOption.c_str()));

        arenaController->m_voteType   = "";
        arenaController->m_voteOption = "";
    }
}


ArenaController::ArenaController(void)
{
    m_botListWidget       = "";   
    m_botSelectListWidget = "";   
    m_botListRowBased     = false;
    m_defaultMapScreen    = "sysimg/2d/default-map-screen.tga";   
    m_selectedBot         = "";
    m_voteOptionWidget    = "";
    m_voteType            = "";
    m_voteOption          = "";
    m_selectedLevel       = NULL;
    m_serverAddress       = "";
    m_tempBotskillLevel   = Cvar_Get("temp_mp_botskilllevel", "3", 0);   
    m_tempMaxClients      = Cvar_Get("temp_sv_maxclients", "8", 0);      

    readLevelProperties("maps");    
    readBotFiles();
    m_mdsRegistry = CheckForMdsRegistry();
    readDMModels("models/char");    
    registerCvars();

    m_awardImages[0] = "EMPTY";                                       
    m_awardImages[1] = "sysimg/icons/levelawards/level_award1.tga";   
    m_awardImages[2] = "sysimg/icons/levelawards/level_award2.tga";   
    m_awardImages[3] = "sysimg/icons/levelawards/level_award3.tga";   
    m_awardImages[4] = "sysimg/icons/levelawards/level_award4.tga";   
    m_awardImages[5] = "sysimg/icons/levelawards/level_award5.tga";   

    m_serverAddress = "";   
    m_voteType      = "";   
    m_voteOption    = "";   
}

ArenaController::~ArenaController(void)
{
    clearDMModels();
    clearLevelProperties();
    clearBotProperties();
    clearBotAssignments();
}

void ArenaController::registerCommands(void)
{
    Cmd_AddCommand("ui_readdmmodels", UI_ReadDMModels_f);              
    Cmd_AddCommand("ui_getdmmodels", UI_GetDMModels_f);                
    Cmd_AddCommand("ui_setlevellist", UI_SetLevelList_f);              
    Cmd_AddCommand("ui_getlevels", UI_GetLevels_f);                    
    Cmd_AddCommand("ui_readlevelproperties", UI_ReadtLevelProperties_f); 
    Cmd_AddCommand("ui_setselectedlevel", UI_SetSelectedLevel_f);      
    Cmd_AddCommand("ui_setbotlist", UI_SetBotList_f);                  
    Cmd_AddCommand("ui_setbotselectlist", UI_SetBotSelectList_f);      
    Cmd_AddCommand("ui_startlevelprompt", UI_StartLevelPrompt_f);      
    Cmd_AddCommand("ui_startlevel", UI_StartLevel_f);                  
    Cmd_AddCommand("ui_connectserverprompt", UI_ConnectServerPrompt_f); 
    Cmd_AddCommand("ui_connectserver", UI_ConnectServer_f);            
    Cmd_AddCommand("ui_resetserverscreen", UI_ResetServerScreen_f);    
    Cmd_AddCommand("ui_assignbotselection", UI_AssignBotSelections_f); 
    Cmd_AddCommand("ui_getbotassignments", UI_GetBotAssignments_f);    
    Cmd_AddCommand("ui_updatebotassignments", UI_UpdateBotAssignments_f); 
    Cmd_AddCommand("ui_toggleslot", UI_ToggleSlot_f);                  
    Cmd_AddCommand("ui_setselectedbot", UI_SetSelectedBot_f);          
    Cmd_AddCommand("ui_setbotteam", UI_SetBotTeam_f);                  
    Cmd_AddCommand("ui_setbotspecialty", UI_SetBotSpecialty_f);        
    Cmd_AddCommand("ui_getavailablebots", UI_GetAvailableBots_f);      
    Cmd_AddCommand("ui_setselectedbotassignment", UI_SetSelectedBotAssignment_f); 
    Cmd_AddCommand("ui_setvoteoptionwidget", UI_SetVoteOptionWidget_f); 
    Cmd_AddCommand("ui_setvotetype", UI_SetVoteType_f);                
    Cmd_AddCommand("ui_setvoteoption", UI_SetVoteOption_f);            
    Cmd_AddCommand("ui_callvote", UI_CallVote_f);                      
}

void ArenaController::unregisterCommands(void)
{
    Cmd_RemoveCommand("ui_readdmmodels");
    Cmd_RemoveCommand("ui_getdmmodels");
    Cmd_RemoveCommand("ui_setlevellist");
    Cmd_RemoveCommand("ui_getlevels");
    Cmd_RemoveCommand("ui_readlevelproperties");
    Cmd_RemoveCommand("ui_setselectedlevel");
    Cmd_RemoveCommand("ui_setbotlist");
    Cmd_RemoveCommand("ui_startlevelprompt");
    Cmd_RemoveCommand("ui_startlevel");
    Cmd_RemoveCommand("ui_connectserverprompt");
    Cmd_RemoveCommand("ui_connectserver");
    Cmd_RemoveCommand("ui_resetserverscreen");
    Cmd_RemoveCommand("ui_assignbotselection");
    Cmd_RemoveCommand("ui_getbotassignments");
    Cmd_RemoveCommand("ui_updatebotassignments");
    Cmd_RemoveCommand("ui_toggleslot");
    Cmd_RemoveCommand("ui_setselectedbot");
    Cmd_RemoveCommand("ui_setbotteam");
    Cmd_RemoveCommand("ui_setbotspecialty");
    Cmd_RemoveCommand("ui_getavailablebots");
    Cmd_RemoveCommand("ui_setselectedbotassignment");
    Cmd_RemoveCommand("ui_setvoteoptionwidget");
    Cmd_RemoveCommand("ui_setvotetype");
    Cmd_RemoveCommand("ui_setvoteoption");
    Cmd_RemoveCommand("ui_callvote");
}

void ArenaController::registerCvars(void)
{
    ac_mp_modifier_InstantKill          = Cvar_Get("mp_modifier_InstantKill", "0", 4);
    ac_mp_modifier_ActionHero           = Cvar_Get("mp_modifier_ActionHero", "0", 4);
    ac_mp_modifier_AutoHandicap         = Cvar_Get("mp_modifier_AutoHandicap", "0", 4);
    ac_mp_modifier_PointsPerWeapon      = Cvar_Get("mp_modifier_PointsPerWeapon", "0", 4);
    ac_mp_modifier_ControlPoints        = Cvar_Get("mp_modifier_ControlPoints", "0", 4);
    ac_mp_modifier_Destruction          = Cvar_Get("mp_modifier_Destruction", "0", 4);
    ac_mp_modifier_OneFlag              = Cvar_Get("mp_modifier_OneFlag", "0", 4);
    ac_mp_modifier_Elimination          = Cvar_Get("mp_modifier_Elimination", "0", 4);
    ac_mp_modifier_Specialties          = Cvar_Get("mp_modifier_Specialties", "0", 4);
    ac_temp_sv_hostname                 = Cvar_Get("temp_sv_hostname", "EF2 Server", 1);
    ac_temp_sv_maxspeed                 = Cvar_Get("temp_sv_maxspeed", "400", 0);
    ac_temp_sv_maxclients               = Cvar_Get("temp_sv_maxclients", "8", 0);
    ac_temp_mp_gametype                 = Cvar_Get("temp_mp_gametype", "0", 0);
    ac_temp_mp_gametypename             = Cvar_Get("temp_mp_gametypename", "Holomatch", 0);
    ac_temp_password                    = Cvar_Get("temp_password", "", 256);
    ac_temp_mp_botskilllevel            = Cvar_Get("temp_mp_botskilllevel", "3", 0);
    ac_temp_mp_pointlimit               = Cvar_Get("temp_mp_pointlimit", "0", 0);
    ac_temp_mp_timelimit                = Cvar_Get("temp_mp_timelimit", "20", 0);
    ac_temp_mp_flags                    = Cvar_Get("temp_mp_flags", "0", 0);
    ac_temp_mp_itemRespawnMultiplier    = Cvar_Get("temp_mp_itemRespawnMultiplier", "1.0", 0);
    ac_temp_mp_weaponRespawnMultiplier  = Cvar_Get("temp_mp_weaponRespawnMultiplier", "1.0", 0);
    ac_temp_mp_powerupRespawnMultiplier = Cvar_Get("temp_mp_powerupRespawnMultiplier", "1.0", 0);
    ac_temp_mp_knockbackMultiplier      = Cvar_Get("temp_mp_knockbackMultiplier", "1.0", 0);
    ac_temp_mp_damageMultiplier         = Cvar_Get("temp_mp_damageMultiplier", "1.0", 0);
    ac_temp_mp_respawnInvincibilityTime = Cvar_Get("temp_mp_respawnInvincibilityTime", "4", 0);
    ac_temp_mp_warmUpTime               = Cvar_Get("temp_mp_warmUpTime", "10", 0);
    ac_temp_mp_minPlayers               = Cvar_Get("temp_mp_minPlayers", "0", 0);
    ac_temp_dedicated                   = Cvar_Get("temp_dedicated", "0", 0);
    ac_temp_sv_pure                     = Cvar_Get("temp_sv_pure", "0", 0);
    ac_temp_strafeJumpingAllowed        = Cvar_Get("temp_strafeJumpingAllowed", "1", 0);
    ac_temp_sv_maxrate                  = Cvar_Get("temp_sv_maxrate", "0", 1);
    ac_temp_mp_modifier_InstantKill     = Cvar_Get("temp_mp_modifier_InstantKill", "0", 0);
    ac_temp_mp_modifier_ActionHero      = Cvar_Get("temp_mp_modifier_ActionHero", "0", 0);
    ac_temp_mp_modifier_AutoHandicap    = Cvar_Get("temp_mp_modifier_AutoHandicap", "0", 0);
    ac_temp_mp_modifier_PointsPerWeapon = Cvar_Get("temp_mp_modifier_PointsPerWeapon", "0", 0);
    ac_temp_mp_modifier_ControlPoints   = Cvar_Get("temp_mp_modifier_ControlPoints", "0", 0);
    ac_temp_mp_modifier_Destruction     = Cvar_Get("temp_mp_modifier_Destruction", "0", 0);
    ac_temp_mp_modifier_OneFlag         = Cvar_Get("temp_mp_modifier_OneFlag", "0", 0);
    ac_temp_mp_modifier_Elimination     = Cvar_Get("temp_mp_modifier_Elimination", "0", 0);
    ac_temp_mp_modifier_Specialties     = Cvar_Get("temp_mp_modifier_Specialties", "0", 0);
}

void ArenaController::readDMModels(const str& directory)
{
    int    numfiles;
    char **files;
    str    name;
    char  *extension;
    int    i;

    files = FS_ListFiles(directory.c_str(), ".tik", qfalse, &numfiles);

    for (i = 0; i < numfiles; i++) {
        if (!strncmp(files[i], "dm_", 3) || (!strncmp(files[i], "eb_", 3) && m_mdsRegistry)) {
            extension = strstr(files[i], ".tik");
            if (extension) {
                *extension = 0;
                name = files[i];
                m_dmModels.AddObject(name);
            }
        }
    }
}

void ArenaController::clearDMModels(void)
{
    m_dmModels.ClearObjectList();
}

void ArenaController::readLevelProperties(const str& directory)
{
    int              numfiles;
    char           **files;
    str              path;
    LevelProperties *properties;
    int              i;

    files = FS_ListFiles(directory.c_str(), ".mni", qfalse, &numfiles);

    for (i = 0; i < numfiles; i++) {
        properties = new LevelProperties();

        path = directory;
        path += "/";
        if (files[i]) {
            path += files[i];
        }

        if (properties->readFile(path)) {
            m_levelProperties.AddObject(properties);
        } else {
            delete properties;
        }
    }
}

void ArenaController::clearLevelProperties(void)
{
    int i;

    for (i = 1; i <= m_levelProperties.NumObjects(); i++) {
        delete m_levelProperties.ObjectAt(i);
    }

    m_levelProperties.ClearObjectList();
}

void ArenaController::getLevels(const str& gameType, const str& matchType)
{
    LevelProperties *properties;
    int              i;

    for (i = 1; i <= m_levelProperties.NumObjects(); i++) {
        properties = m_levelProperties.ObjectAt(i);
        if (properties && properties->hasMatchType(matchType) && properties->hasGameType(gameType)) {
            Cbuf_ExecuteText(
                EXEC_NOW,
                va("globalwidgetcommand %s addrow \"%s\" \"%s\"",
                   m_mapListWidget.c_str(),
                   properties->m_longMapName.c_str(),
                   properties->m_mapName.c_str()));
        }
    }

    Cbuf_ExecuteText(
        EXEC_NOW,
        va("globalwidgetcommand %s selectrow 1", m_mapListWidget.c_str()));
}

LevelProperties *ArenaController::getLevelProperties(const str& name)
{
    LevelProperties *properties;
    int              i;

    for (i = 1; i <= m_levelProperties.NumObjects(); i++) {
        properties = m_levelProperties.ObjectAt(i);
        if (!properties->m_mapName.icmp(name)) {
            return properties;
        }
    }

    return NULL;
}

void ArenaController::setLevelAward(const str& levelName)
{
    str              awardCvar;
    cvar_t          *award;
    GameplayManager *gameplay;
    str              objectName;
    str              propertyName;

    awardCvar = levelName;
    awardCvar += "_award";                                          
    award = Cvar_Get(awardCvar.c_str(), "0", 1);                    

    gameplay = GameplayManager::getTheGameplayManager();

    objectName   = "Multiplayer";                                   
    propertyName = "MapAward";                                      
    gameplay->setStringValue(objectName, propertyName, m_awardImages[award->integer], false);
}

void ArenaController::setSelectedLevel(const str& levelName, int showBotAssignments)
{
    LevelProperties *properties;
    GameplayManager *gameplay;
    str              objectName;
    str              propertyName;
    int              i;

    Cvar_Set("ui_startmap", levelName.c_str());   

    properties = NULL;
    for (i = 1; i <= m_levelProperties.NumObjects(); i++) {
        if (!m_levelProperties.ObjectAt(i)->m_mapName.icmp(levelName)) {
            properties = m_levelProperties.ObjectAt(i);
            break;
        }
    }

    m_selectedLevel = properties;
    if (!properties) {
        return;
    }

    clearBotAssignments();

    Cbuf_ExecuteText(
        EXEC_NOW,
        va("globalwidgetcommand %s deleteallitems", m_botListWidget.c_str()));   

    gameplay = GameplayManager::getTheGameplayManager();

    objectName   = "Multiplayer";   
    propertyName = "MapScreenShot"; 
    if (properties->m_screenshot.length()) {
        gameplay->setStringValue(objectName, propertyName, properties->m_screenshot, false);
    } else {
        gameplay->setStringValue(objectName, propertyName, m_defaultMapScreen, false);
    }

    propertyName = "MapName";       
    gameplay->setStringValue(objectName, propertyName, properties->m_mapName, false);

    if (properties->m_bots) {
        Cbuf_ExecuteText(
            EXEC_NOW,
            va("globalwidgetcommand %s widgetstate normal\n", m_botListWidget.c_str()));   
        getLevelBotAssignments(showBotAssignments);
    } else {
        Cbuf_ExecuteText(
            EXEC_NOW,
            va("globalwidgetcommand %s widgetstate disable\n", m_botListWidget.c_str()));  
    }

    setLevelAward(levelName);

    Cvar_Set("mp_map_instantkill", "0");     
    Cvar_Set("mp_map_actionhero", "0");      
    Cvar_Set("mp_map_handicap", "0");        
    Cvar_Set("mp_map_weaponscore", "0");     
    Cvar_Set("mp_map_controlpoints", "0");   
    Cvar_Set("mp_map_powerstruggle", "0");   
    Cvar_Set("mp_map_oneflag", "0");         
    Cvar_Set("mp_map_elimination", "0");     
    Cvar_Set("mp_map_specialties", "0");     

    properties->setSupportedModifiers();
}

void ArenaController::readBotFiles(void)
{
    int    numfiles;
    char **files;
    str    path;
    int    i;

    readBotProperties("botfiles/bots.txt");   

    files = FS_ListFiles("scripts", ".bot", qfalse, &numfiles);   

    for (i = 0; i < numfiles; i++) {
        path = "docs/scripts/";   
        if (files[i]) {
            path += files[i];
        }

        readBotProperties(path.c_str());
    }
}

void ArenaController::readBotProperties(const char *filename)
{
    Script         script;
    str            token;
    BotProperties *bot;

    script.LoadFile(filename);

    if (!script.isValid()) {
        Com_Printf("Error loading botfile: %s. File does not exist.\n", filename);
        script.Close();
        return;
    }

    while (1) {
        do {
            if (!script.TokenAvailable(true)) {
                script.Close();
                return;
            }

            token = script.GetToken(false);
        } while (token[0] != '{');

        bot = new BotProperties();
        if (!bot->parseBotSection(script)) {
            Com_Printf("Error loading botfile: %s.\n", filename);
            delete bot;
            break;
        }

        m_botProperties.AddObject(bot);
    }

    script.Close();
}

void ArenaController::clearBotProperties(void)
{
    int i;

    for (i = 1; i <= m_botProperties.NumObjects(); i++) {
        delete m_botProperties.ObjectAt(i);
    }

    m_botProperties.ClearObjectList();
}

void ArenaController::clearBotAssignments(void)
{
    int i;

    for (i = 0; i < 7; i++) {
        m_botAssignments[i].clear();
    }
}

void ArenaController::copyTempCvars(void)
{
    str password;
    int i;

    Cvar_Set("botenable", "1");                                              
    Cvar_Set("sv_hostname", ac_temp_sv_hostname->string);                    
    Cvar_Set("sv_maxspeed", ac_temp_sv_maxspeed->string);                    
    Cvar_Set2("sv_maxclients", ac_temp_sv_maxclients->string, qfalse);       
    Cvar_Set("mp_gametype", ac_temp_mp_gametype->string);                    
    Cvar_Set("mp_gametypename", ac_temp_mp_gametypename->string);            
    Cvar_Set("mp_botskilllevel", ac_temp_mp_botskilllevel->string);          
    Cvar_Set("mp_pointlimit", ac_temp_mp_pointlimit->string);                
    Cvar_Set("mp_timelimit", ac_temp_mp_timelimit->string);                  
    Cvar_Set("mp_flags", ac_temp_mp_flags->string);                          
    Cvar_Set("mp_itemRespawnMultiplier", ac_temp_mp_itemRespawnMultiplier->string);    
    Cvar_Set("mp_weaponRespawnMultiplier", ac_temp_mp_weaponRespawnMultiplier->string); 
    Cvar_Set("mp_powerupRespawnMultiplier", ac_temp_mp_powerupRespawnMultiplier->string); 
    Cvar_Set("mp_knockbackMultiplier", ac_temp_mp_knockbackMultiplier->string);        
    Cvar_Set("mp_damageMultiplier", ac_temp_mp_damageMultiplier->string);              
    Cvar_Set("mp_respawnInvincibilityTime", ac_temp_mp_respawnInvincibilityTime->string); 
    Cvar_Set("mp_warmUpTime", ac_temp_mp_warmUpTime->string);                
    Cvar_Set("mp_minPlayers", ac_temp_mp_minPlayers->string);                
    Cvar_Set("dedicated", ac_temp_dedicated->string);                        
    Cvar_Set("sv_pure", ac_temp_sv_pure->string);                            
    Cvar_Set("sv_maxrate", ac_temp_sv_maxrate->string);                      
    Cvar_Set("sv_strafeJumpingAllowed", ac_temp_strafeJumpingAllowed->string); 

    Cvar_Set("mp_modifier_InstantKill", ac_temp_mp_modifier_InstantKill->string);   
    Cvar_Set("mp_modifier_ActionHero", ac_temp_mp_modifier_ActionHero->string);     
    Cvar_Set("mp_modifier_AutoHandicap", ac_temp_mp_modifier_AutoHandicap->string); 
    Cvar_Set("mp_modifier_PointsPerWeapon", ac_temp_mp_modifier_PointsPerWeapon->string);   
    Cvar_Set("mp_modifier_ControlPoints", ac_temp_mp_modifier_ControlPoints->string);       
    Cvar_Set("mp_modifier_Destruction", ac_temp_mp_modifier_Destruction->string);           
    Cvar_Set("mp_modifier_OneFlag", ac_temp_mp_modifier_OneFlag->string);           
    Cvar_Set("mp_modifier_Elimination", ac_temp_mp_modifier_Elimination->string);   
    Cvar_Set("mp_modifier_Specialties", ac_temp_mp_modifier_Specialties->string);   

    password = ac_temp_password->string;
    for (i = 0; i < (int)password.length(); i++) {
        if (password[i] == '%' || password[i] == '"') {
            password[i] = '.';
        }
    }

    Cvar_Set("password", password.c_str());   
}

void ArenaController::startLevel(void)
{
    str            commands;
    int            maxClients;
    int            count;
    int            i;
    BotAssignment *assignment;

    copyTempCvars();

    commands = "";   

    maxClients = m_tempMaxClients->integer;
    if (maxClients <= 1) {
        maxClients = 8;
    }

    count = maxClients - 1;
    if (count <= 0 || count > 7) {
        count = 7;
    }

    for (i = 0; i < count; i++) {
        assignment = &m_botAssignments[i];

        if (!assignment->m_open && assignment->m_name.length()) {
            commands += ";addbot ";                       
            commands += assignment->m_name;
            commands += " ";                              
            commands += m_tempBotskillLevel->integer;
            commands += " ";                              

            if (m_teamGame == 1) {                        
                commands += assignment->m_team;
                commands += "  ";                         
            }

            if (m_specialties == 1) {                     
                commands += "\"";                         
                commands += assignment->m_specialty;
                commands += "\"";                         
            }
        }
    }

    commands += "\n";                                     
    Cvar_Set("mp_botcommands", commands.c_str());         

    Cbuf_ExecuteText(EXEC_NOW, "startserver");            
}

void ArenaController::connectToServer(void)
{
    str password;
    int i;

    if (!m_serverAddress.length()) {
        Com_Printf("Failed to connect. No server ip address specified.\n");
        return;
    }

    password = ac_temp_password->string;
    for (i = 0; i < (int)password.length(); i++) {
        if (password[i] == '%' || password[i] == '"') {
            password[i] = '.';
        }
    }

    Cvar_Set("password", password.c_str());   

    Cbuf_ExecuteText(
        EXEC_NOW,
        va("connect %s\n", m_serverAddress.c_str()));   
}

void ArenaController::getAvailableBots(void)
{
    str            command;
    BotProperties *bot;
    int            i;

    if (!m_botSelectListWidget.length()) {
        return;
    }

    for (i = 1; i <= m_botProperties.NumObjects(); i++) {
        command = "globalwidgetcommand ";                        
        command += m_botSelectListWidget;
        command += " ";                                          
        command += "addrowitem ";                                

        bot = m_botProperties.ObjectAt(i);
        if (bot) {
            command += bot->m_screenshot;
            command += " ";                                      
            command += bot->m_name;

            Cbuf_ExecuteText(EXEC_NOW, va("%s", command.c_str()));
        }
    }
}

void ArenaController::getLevelBotAssignments(int load)
{
    str              gameTypeName;
    BotAssignment   *assignment;
    int              i;

    if (!m_selectedLevel || !m_selectedLevel->m_bots) {
        return;
    }

    gameTypeName = Cvar_VariableString("temp_mp_gametypename");   
    if (gameTypeName.icmp("Holomatch")) {                         
        m_teamGame = true;
    } else {
        m_teamGame = false;
    }

    m_specialties = m_selectedLevel->m_specialties == 1
                    && Cvar_VariableIntegerValue("temp_mp_modifier_Specialties")
                    && m_teamGame == 1;

    clearBotAssignments();

    if (load) {
        for (i = 1; i <= m_selectedLevel->m_botAssignments.NumObjects() && i < 7; i++) {
            assignment = m_selectedLevel->m_botAssignments.ObjectAt(i);
            m_botAssignments[i - 1] = *assignment;
        }
    }

    getBotAssignments();
}

void ArenaController::getBotAssignments(void)
{
    BotProperties *bot;
    int            count;
    int            i;
    int            j;

    if (!m_selectedLevel || !m_selectedLevel->m_bots) {
        return;
    }

    count = m_tempMaxClients->integer - 1;
    if (count <= 0 || count > 7) {
        count = 7;
    }

    for (i = 0; i < count; i++) {
        if (m_botListRowBased == 1) {
            addBotRow(m_botAssignments[i], i);
        } else {
            bot = NULL;
            for (j = 1; j <= m_botProperties.NumObjects(); j++) {
                if (m_botProperties.ObjectAt(j)->m_name == m_botAssignments[i].m_name) {
                    bot = m_botProperties.ObjectAt(j);
                    break;
                }
            }

            if (bot) {
                addBotItem(bot);
            }
        }
    }
}

void ArenaController::getBotCommand(BotAssignment& assignment, str& command)
{
    if (assignment.m_open == 1) {
        command += "\" \"";          
        command += " Open";          

        if (m_teamGame == 1) {       
            command += " Red";       

            if (m_specialties == 1) {    
                command += " Infiltrator";   
            }
        }
    } else {
        if (!assignment.m_name.length()) {
            assignment.m_name = getRandomBotName()->m_name;
        }

        command += assignment.m_name;
        command += " ";              
        command += " Bot";           

        if (assignment.m_team.length() && m_teamGame == 1) {
            command += " ";          
            command += assignment.m_team;
        }

        if (assignment.m_specialty.length() && m_specialties == 1) {
            command += " \"";        
            command += assignment.m_specialty;
            command += "\" ";        
        }
    }
}

BotProperties *ArenaController::getRandomBotName(void)
{
    // retail @0x0015D7B0 -- ten tries to find a bot that is not assigned yet
    bool           assigned;
    BotProperties *bot;
    int            index;
    int            attempt;
    int            i;

    for (attempt = 0; attempt < 10; attempt++) {
        index = rand() % m_botProperties.NumObjects();
        if (!index) {
            index = 1;
        }

        assigned = false;
        for (i = 0; i < 7; i++) {
            bot = m_botProperties.ObjectAt(index);
            if (bot->m_name == m_botAssignments[i].m_name) {
                assigned = true;
                break;
            }
        }

        if (!assigned) {
            break;
        }
    }

    return m_botProperties.ObjectAt(index);
}

void ArenaController::addBotItem(BotProperties *bot)
{
    str command;

    command = "globalwidgetcommand ";   
    command += m_botListWidget;
    command += " ";                     
    command += "addrowitem ";           
    command += bot->m_screenshot;
    command += " ";                     
    command += bot->m_name;

    Cbuf_ExecuteText(EXEC_NOW, va("%s", command.c_str()));
}

void ArenaController::addBotRow(BotAssignment& assignment, int row)
{
    str command;

    command = "globalwidgetcommand ";   
    command += m_botListWidget;
    command += " ";                     
    command += "addrow ";               
    command += row + 1;
    command += " ";                     

    getBotCommand(assignment, command);

    command += "\n";                    

    Cbuf_ExecuteText(EXEC_NOW, va("%s", command.c_str()));
}

void ArenaController::setBotRow(BotAssignment& assignment, int row)
{
    str command;

    command = "globalwidgetcommand ";   
    command += m_botListWidget;
    command += " ";                     
    command += "setrow ";               
    command += row;
    command += " ";                     
    command += row;
    command += " ";                     

    getBotCommand(assignment, command);

    command += "\n";                    

    Cbuf_ExecuteText(EXEC_NOW, va("%s", command.c_str()));
}

void ArenaController::assignBotSelections(void)
{
    BotProperties *bot;
    int            i;

    if ((unsigned)m_selectedBotAssignment > 6 || !m_selectedBot.length()) {
        return;
    }

    bot = NULL;
    for (i = 1; i <= m_botProperties.NumObjects(); i++) {
        if (m_botProperties.ObjectAt(i)->m_name == m_selectedBot) {
            bot = m_botProperties.ObjectAt(i);
            break;
        }
    }

    if (!bot) {
        return;
    }

    m_botAssignments[m_selectedBotAssignment].m_name = bot->m_name;
    m_botAssignments[m_selectedBotAssignment].m_open = false;

    if (!m_botAssignments[m_selectedBotAssignment].m_team.length()) {
        m_botAssignments[m_selectedBotAssignment].m_team = "Red";              
    }

    if (!m_botAssignments[m_selectedBotAssignment].m_specialty.length()) {
        m_botAssignments[m_selectedBotAssignment].m_specialty = "Infiltrator"; 
    }

    setBotRow(m_botAssignments[m_selectedBotAssignment], m_selectedBotAssignment + 1);
}

void ArenaController::setVoteType(const str& voteType)
{
    m_voteType = voteType;

    Cbuf_ExecuteText(
        EXEC_NOW,
        va("globalwidgetcommand %s deleteallitems\n", m_voteOptionWidget.c_str()));   

    if (!strcmp(m_voteType.c_str(), "kick")) {           
        handlePlayerListVote();
    } else if (!strcmp(m_voteType.c_str(), "map")) {     
        handleMapListVote();
    } else if (!strcmp(m_voteType.c_str(), "timelimit")) {   
        handleTimeLimitVote();
    }
}

void ArenaController::handlePlayerListVote(void)
{
    str  name;
    int  i;

    for (i = 0; i <= 128; i++) {
        name = Info_ValueForKey(CL_ConfigString(1216 + i), "name");   

        if (name.length()) {
            Cbuf_ExecuteText(
                EXEC_NOW,
                va("globalwidgetcommand %s addrow \"%s\" \"%s\"\n",   
                   m_voteOptionWidget.c_str(),
                   name.c_str(),
                   name.c_str()));
        }
    }
}

void ArenaController::handleMapListVote(void)
{
    str gameType;
    str matchType;

    clearLevelProperties();
    readLevelProperties("maps");   

    m_mapListWidget = m_voteOptionWidget;

    gameType  = Cvar_VariableString("mp_gametypename");   
    matchType = Cvar_VariableString("g_gametype");       

    getLevels(gameType, matchType);
}

void ArenaController::handleTimeLimitVote(void)
{
    Cbuf_ExecuteText(EXEC_NOW, va("globalwidgetcommand %s addrow \"5\"  \"5\"\n", m_voteOptionWidget.c_str()));
    Cbuf_ExecuteText(EXEC_NOW, va("globalwidgetcommand %s addrow \"10\" \"10\"\n", m_voteOptionWidget.c_str()));
    Cbuf_ExecuteText(EXEC_NOW, va("globalwidgetcommand %s addrow \"15\" \"15\"\n", m_voteOptionWidget.c_str()));
    Cbuf_ExecuteText(EXEC_NOW, va("globalwidgetcommand %s addrow \"20\" \"20\"\n", m_voteOptionWidget.c_str()));
    Cbuf_ExecuteText(EXEC_NOW, va("globalwidgetcommand %s addrow \"25\" \"25\"\n", m_voteOptionWidget.c_str()));
    Cbuf_ExecuteText(EXEC_NOW, va("globalwidgetcommand %s addrow \"30\" \"30\"\n", m_voteOptionWidget.c_str()));
    Cbuf_ExecuteText(EXEC_NOW, va("globalwidgetcommand %s addrow \"35\" \"35\"\n", m_voteOptionWidget.c_str()));
    Cbuf_ExecuteText(EXEC_NOW, va("globalwidgetcommand %s addrow \"40\" \"40\"\n", m_voteOptionWidget.c_str()));
    Cbuf_ExecuteText(EXEC_NOW, va("globalwidgetcommand %s addrow \"45\" \"45\"\n", m_voteOptionWidget.c_str()));
    Cbuf_ExecuteText(EXEC_NOW, va("globalwidgetcommand %s addrow \"50\" \"50\"\n", m_voteOptionWidget.c_str()));
    Cbuf_ExecuteText(EXEC_NOW, va("globalwidgetcommand %s addrow \"55\" \"55\"\n", m_voteOptionWidget.c_str()));
    Cbuf_ExecuteText(EXEC_NOW, va("globalwidgetcommand %s addrow \"60\" \"60\"\n", m_voteOptionWidget.c_str()));
}

void CreateArenaController(void)
{
    if (!arenaController) {
        arenaController = new ArenaController();
    }
}

void DeleteArenaController(void)
{
    if (arenaController) {
        delete arenaController;
        arenaController = NULL;
    }
}

static str UI_LoadMenuNameForMap(const char *pszMapName);

void UI_ServerLoading(const char *mapname, int stage)
{
    if (cge) {
        // CG_DrawActiveFrame here, which retail's slot +0x08 does not serve from this
        cge->CG_UpdateClient(cl.serverTime * 2);

        SCR_UpdateScreen();
        SCR_UpdateScreen();
        SCR_UpdateScreen();
        SCR_UpdateScreen();
    }

    server_loading_stage = stage;
    server_loading       = qtrue;  

    if (mapname) {
        if (strnicmp(mapname, "maps/", 5)) {
            server_mapname[0] = 0;
            Q_strncpyz(server_mapname, mapname, sizeof(server_mapname));
        } else {
            Q_strncpyz(server_mapname, mapname + 5, sizeof(server_mapname));
        }

        size_t len = strlen(server_mapname);

        if (len > 4 && !stricmp(server_mapname + len - 4, ".bsp")) {
            server_mapname[len - 4] = 0;
        }

        ui_sLevelLoadingScreen = UI_LoadMenuNameForMap(server_mapname);
    }

    UI_ClearState();

    Menu *current = menuManager.CurrentMenu();

    if (current && current->m_name == "loading") {
        menuManager.Unlock(NULL);
        menuManager.PopMenu(0);
    }

    menuManager.setLoading(true);
}

qboolean UI_IsServerLoading(void)
{
    return server_loading;
}

void UI_StopLoading(void)
{
    if (paused && paused->integer) {
        Com_Pause_f();
    }

    server_loading = qfalse;
    menuManager.setLoading(false);
    menuManager.Unlock(NULL);
    menuManager.ClearMenus(true);
    UIVK_PROBE("focus", "caller=UI_StopLoading State=%d", cls.state);
    UI_FocusMenuIfExists();
    StopLoadingScreen();
}

void UI_Disconnected(void)
{
    if (!cls.uiStarted) {
        return;
    }

    if (!hudListHidden) {
        UI_HideHudList();
        hudListHidden = qtrue;
    }

    hudList.ClearObjectList();

    if (headhud && headhud->isVisible()) {
        headhud->ForceHide();
    }
    if (texthud && texthud->isVisible()) {
        texthud->ForceHide();
    }
    if (headtexthud && headtexthud->isVisible()) {
        headtexthud->ForceHide();
    }

    SetGameplayFloatProperty("Background", "ShowShip", 1.0f, qfalse);
}

qboolean UI_IsView3DActive(void)
{
    return view3d && view3d->IsActive();
}

const char *CL_GetCurrentWeapon(void)
{
    return CL_ConfigString(CS_ITEMS + cl.snap.ps.activeItems[ITEM_NAME_WEAPON_DUAL]);
}

const char *CL_GetCurrentWeaponsStringCommand(void)
{
    static char buf[1024];  

    buf[0] = 0;

    int dual = cl.snap.ps.activeItems[ITEM_NAME_WEAPON_DUAL];
    if (dual >= 0) {
        Com_sprintf(buf, sizeof(buf), "use \"%s\" dual", CL_ConfigString(CS_ITEMS + dual));
        return buf;
    }

    const char *leftName  = "";
    const char *rightName = "";

    int left = cl.snap.ps.activeItems[ITEM_NAME_WEAPON_LEFT];
    if (left >= 0) {
        leftName = CL_ConfigString(CS_ITEMS + left);
    }

    int right = cl.snap.ps.activeItems[ITEM_NAME_WEAPON_RIGHT];
    if (right >= 0) {
        rightName = CL_ConfigString(CS_ITEMS + right);
    }

    if (strlen(leftName) && strlen(rightName)) {
        Com_sprintf(buf, sizeof(buf), "dualwield \"%s\" \"%s\"\n", leftName, rightName);
    } else if (strlen(leftName)) {
        Com_sprintf(buf, sizeof(buf), "use \"%s\" left\n", leftName);
    } else if (strlen(rightName)) {
        Com_sprintf(buf, sizeof(buf), "use \"%s\" right\n", rightName);
    } else {
        return "holster\n";
    }

    return buf;
}

static void UI_Flush_f(void)
{
    Container<intermediate_console_text_t *> saved;
    str                                      line;
    UIConsole                               *console;
    int                                      color = 0;
    int                                      i;

    UI_ForceMenuOff(true);

    console = fakk_console ? fakk_console->getConsole() : NULL;
    if (console) {
        for (i = 0; i < console->getItemCount(); i++) {
            intermediate_console_text_t *record;

            if (!console->getItemData(i, line, color)) {
                continue;
            }

            record        = new intermediate_console_text_t;
            record->text  = new str(line + '\n');
            record->color = color;
            saved.AddObject(record);
        }
    }

    CL_ShutdownUI();
    L_ClearEventList();

    for (i = 1; i <= saved.NumObjects(); i++) {
        s_intermediateconsole.AddObject(saved.ObjectAt(i));
    }

    saved.ClearObjectList();
}

static void UI_CurrentMenu_f(void)
{
    Menu *current = menuManager.CurrentMenu();

    if (!current) {
        Com_Printf("No current menu\n");
        return;
    }

    Com_Printf("%s\n", current->m_name.c_str());
}

static void UI_ShowInventory_f(void)
{
    Cbuf_AddText("globalwidgetcommand InventoryBar inventoryshow\n");
}

static void UI_DisableInventory_f(void)
{
    Cbuf_AddText("globalwidgetcommand InventoryBar inventorybardisable\n");
}

static void UI_EnableInventory_f(void)
{
    Cbuf_AddText("globalwidgetcommand InventoryBar inventorybarenable\n");
}

static void UI_DoIntro_f(void)
{
    char        def[] = "";
    const char *cmd   = UI_GetCvarString("introCommand", def);

    if (strlen(cmd)) {
        Cbuf_AddText(cmd);
    }
}

static void UI_SaveWeapons_f(void)
{
    if (!Cmd_Argc()) {
        Com_WPrintf("Usage: saveweaps <groupnum>\n");
    }

    const char *cmd = CL_GetCurrentWeaponsStringCommand();

    if (cmd && strlen(cmd)) {
        str alias = "weapgroup";
        alias += Cmd_Argv(1);
        Cmd_Alias(alias.c_str(), cmd);
    }

    Cmd_ExecuteString(va("centerprint \"Saved Weapon Group %d\"\n", atoi(Cmd_Argv(1))));
}

static void UI_RemoveHuds_f(void)
{
    const char *name = Cmd_Argv(1);

    if (!name) {
        return;
    }

    qboolean all = !stricmp(name, "all");

    for (int i = hudList.NumObjects(); i > 0; --i) {
        Menu *hud = hudList.ObjectAt(i);

        if (!all && strnicmp(hud->m_name.c_str(), name, strlen(name))) {
            continue;
        }

        hudList.RemoveObjectAt(i);
        hud->ForceHide();
    }
}

static void UI_AvailableMaps_f(void)
{
    if (Cmd_Argc() < 2) {
        return;
    }

    str prefix = Cmd_Argv(1);
    str widget;

    if (Cmd_Argc() > 2) {
        widget = Cmd_Argv(2);
    }

    int   nfiles;
    char **filenames = FS_ListFiles("maps/", ".bsp", qfalse, &nfiles);

    if (!filenames) {
        return;
    }

    for (int i = 0; i < nfiles; i++) {
        if (strncmp(filenames[i], prefix.c_str(), prefix.length())) {
            continue;
        }

        str map = filenames[i];
        map.CapLength(map.length() - 4);  

        if (widget.length()) {
            Cbuf_ExecuteText(EXEC_NOW, va("globalwidgetcommand %s additem %s\n", widget.c_str(), map.c_str()));
        } else {
            Com_Printf("%s\n", map.c_str());
        }
    }

    FS_FreeFileList(filenames);
}

static void UI_ViewSpawnQuickList_f(void)
{
    str modelpath = "models";

    ViewSpawnerClass *picker = new ViewSpawnerClass;
    picker->Setup("models", modelpath, ".tik", true);
}

static void UI_ViewSetDialog_f(void)
{
    str modelpath = "models";

    ViewSpawnerClass *picker = new ViewSpawnerClass;
    picker->Setup("models", modelpath, ".lst", false);
}

static void UI_ShowJoinTeamButtons_f(void)
{
    if (Cvar_Get("cg_mpgametype", "0", 0)->integer) {
        Cbuf_ExecuteText(EXEC_NOW, "globalwidgetcommand JoinRedBackground enable\n");
        Cbuf_ExecuteText(EXEC_NOW, "globalwidgetcommand JoinRedButton enable\n");
        Cbuf_ExecuteText(EXEC_NOW, "globalwidgetcommand JoinBlueBackground enable\n");
        Cbuf_ExecuteText(EXEC_NOW, "globalwidgetcommand JoinBlueButton enable\n");
        Cbuf_ExecuteText(EXEC_NOW, "globalwidgetcommand JoinGameBackground disable\n");
        Cbuf_ExecuteText(EXEC_NOW, "globalwidgetcommand JoinGameButton disable\n");
    } else {
        Cbuf_ExecuteText(EXEC_NOW, "globalwidgetcommand JoinRedBackground disable\n");
        Cbuf_ExecuteText(EXEC_NOW, "globalwidgetcommand JoinRedButton disable\n");
        Cbuf_ExecuteText(EXEC_NOW, "globalwidgetcommand JoinBlueBackground disable\n");
        Cbuf_ExecuteText(EXEC_NOW, "globalwidgetcommand JoinBlueButton disable\n");
        Cbuf_ExecuteText(EXEC_NOW, "globalwidgetcommand JoinGameBackground enable\n");
        Cbuf_ExecuteText(EXEC_NOW, "globalwidgetcommand JoinGameButton enable\n");
    }
}


extern cvar_t *cl_updatesource;
extern cvar_t *cl_showemptyservers;
extern cvar_t *cl_showfullservers;
int            CL_GetServerCount(void);
serverInfo_t  *CL_GetServers(void);
void           CL_RefreshServerList_f(void);

static int s_selectedServer = -1;

static int s_selectedSource = 0;

static float s_addFavoritesEnabled    = 1.0f;
static float s_deleteFavoritesEnabled = 1.0f;
static float s_cachedSortState        = 0.0f;

static void getGameTypeString(int gameType, str& out)
{
    switch (gameType) {
    case 1:
        out = "$$TM$$"; 
        break;
    case 2:
        out = "$$CTF$$"; 
        break;
    case 3:
        out = "$$BD$$"; 
        break;
    default:
        out = "$$DM$$"; 
        break;
    }
}

void UI_UpdateServerList(void)
{
    serverInfo_t *servers;
    int           serverCount;
    cvar_t       *maxPing;
    int           maxPingValue;
    int           numDone;
    int           numTimedOut;
    int           numFiltered;
    str           sName;
    str           sPlayers;
    str           sGameType;
    int           i;

    serverCount = CL_GetServerCount();
    servers     = CL_GetServers();

    maxPing = Cvar_FindVar("cl_maxping"); 
    if (maxPing) {
        maxPingValue = maxPing->integer;
    } else {
        maxPingValue = 800;
    }

    numDone      = 0;
    numTimedOut  = 0;
    numFiltered  = 0;

    for (i = 0; i < serverCount; i++, servers++) {
        // retail +112 / +108 / ping == -1: already handled this pass, or never
        if (servers->visible || servers->netType || servers->ping == -1) {
            if (servers->netType) {
                numDone++;
            }
            if (servers->visible) {
                numFiltered++;
            }
            continue;
        }

        if ((cl_showemptyservers->value != s_addFavoritesEnabled || servers->clients)
            && (cl_showfullservers->value != s_addFavoritesEnabled
                || servers->clients != servers->maxClients)
            && !NET_IsIPAddressLocal(servers->adr)) {
            if (servers->ping >= maxPingValue && s_cachedSortState != cl_updatesource->value) {
                numTimedOut++;
                continue;
            }

            if (strlen(servers->hostName)) {
                sName = servers->hostName;
            } else {
                sName = NET_AdrToString(servers->adr);
            }

            sPlayers = va("%d", servers->clients);
            sPlayers += "/";
            sPlayers += servers->maxClients;

            getGameTypeString(servers->gameType, sGameType);

            Cbuf_ExecuteText(
                EXEC_NOW,
                va("globalwidgetcommand dm_serverlist addrow \"%s\" \"%s\" \"%s\" \"%s\" \"%d\" \"%s\" \"%d\"",
                   sName.c_str(),
                   servers->mapName,
                   sPlayers.c_str(),
                   sGameType.c_str(),
                   servers->ping,
                   NET_AdrToString(servers->adr),
                   i));

            numDone++;
            servers->netType = 1;
        } else {
            numFiltered++;
            servers->visible = 1;
        }
    }

    if (serverCount == -1) {
        return;
    }

    Cbuf_ExecuteText(
        EXEC_NOW,
        va("globalwidgetcommand dm_refreshstatus title \"$$Status$$: %d / %d ( %d $$ServersTimedOut$$, %d "
           "$$ServersFiltered$$ )\"",
           numDone,
           serverCount,
           numTimedOut,
           numFiltered));
}

static void UI_BuildServerList(void)
{
    serverInfo_t *servers;
    int           serverCount;
    int           numServers;
    str           sName;
    str           sPlayers;
    str           sGameType;
    int           i;

    serverCount = CL_GetServerCount();
    servers     = CL_GetServers();
    numServers  = 0;

    for (i = 0; i < serverCount; i++, servers++) {
        if (strlen(servers->hostName)) {
            sName = servers->hostName;
        } else {
            sName = NET_AdrToString(servers->adr);
        }

        if ((cl_showemptyservers->value != s_addFavoritesEnabled || servers->clients)
            && (cl_showfullservers->value != s_addFavoritesEnabled
                || servers->clients != servers->maxClients)
            && !NET_IsIPAddressLocal(servers->adr)) {
            sPlayers = va("%d", servers->clients);
            sPlayers += "/";
            sPlayers += servers->maxClients;

            getGameTypeString(servers->gameType, sGameType);

            Cbuf_ExecuteText(
                EXEC_NOW,
                va("globalwidgetcommand dm_serverlist addrow \"%s\" \"%s\" \"%s\" \"%s\" \"%d\" \"%s\" \"%d\"",
                   sName.c_str(),
                   servers->mapName,
                   sPlayers.c_str(),
                   sGameType.c_str(),
                   servers->ping,
                   NET_AdrToString(servers->adr),
                   i));

            numServers++;
        }
    }

    (void)numServers;

    GameplayManager::getTheGameplayManager()->setStringValue(
        "Multiplayer", "MapScreenShot", "sysimg/2d/default-map-screen.tga", false);
    GameplayManager::getTheGameplayManager()->setFloatValue(
        "Multiplayer", "EnableDelFavoritesButton", s_addFavoritesEnabled, false);
    GameplayManager::getTheGameplayManager()->setFloatValue(
        "Multiplayer", "EnableAddFavoritesButton", s_addFavoritesEnabled, false);

    Cbuf_ExecuteText(EXEC_NOW, "globalwidgetcommand dm_serverlist selectrow 1");
}

static void UI_AddFavoriteServer(const str& name, const str& address)
{
    netadr_t     adr;
    serverInfo_t *record;
    int          i;

    if (!NET_StringToAdr(address.c_str(), &adr)) {
        Com_Printf("Invalid ip address. Please specify an ip in numerical format\n"); 
        menuManager.ShowMenu(str("OkDialog"));                                       
        UI_FocusMenuIfExists();
        Cbuf_AddText("globalwidgetcommand OkDialogTitle title \"$$InvalidFavoriteEntered$$\"\n");
        return;
    }

    for (i = 0; i < cls.numfavoriteservers; i++) {
        if (NET_CompareAdr(cls.favoriteServers[i].adr, adr)) {
            Com_Printf("Server already added to favorites\n"); 
            menuManager.ShowMenu(str("OkDialog"));
            UI_FocusMenuIfExists();
            Cbuf_AddText("globalwidgetcommand OkDialogTitle title \"$$FavoriteServerAlreadyExists$$\"\n");
            return;
        }
    }

    record = &cls.favoriteServers[cls.numfavoriteservers];
    Com_Memset(record, 0, sizeof(*record));
    record->adr = adr;
    Q_strncpyz(record->hostName, name.c_str(), sizeof(record->hostName));
    cls.numfavoriteservers++;

    Com_Printf("Server successfully added to favorites\n"); 
    menuManager.ShowMenu(str("OkDialog"));
    UI_FocusMenuIfExists();
    Cbuf_AddText("globalwidgetcommand OkDialogTitle title \"$$ServerAddedToFavorites$$\"\n");
}

static void UI_AddFavoriteServer_f(void)
{
    str           sHostName;
    str           sAddress;
    serverInfo_t *server;

    if (cls.numfavoriteservers >= MAX_OTHER_SERVERS) {
        Com_Printf("Too many favorite servers\n"); 
        return;
    }

    if (s_selectedServer < 0) {
        Com_Printf("There is no server selected\n"); 
        return;
    }

    switch (cl_updatesource->integer) {
    case 1:
        server = &cls.globalServers[s_selectedServer];
        break;
    case 2:
        return;
    default:
        server = &cls.localServers[s_selectedServer];
        break;
    }

    sHostName = server->hostName;
    sAddress  = NET_AdrToString(server->adr);

    UI_AddFavoriteServer(sHostName, sAddress);
}

static void UI_AddFavoriteSpecServer(void)
{
    str sAddress;

    sAddress = Cvar_VariableString("ui_specifiedserverip"); 
    sAddress += ":";                                        
    sAddress += Cvar_VariableString("ui_specifiedserverport"); 

    UI_AddFavoriteServer(sAddress, sAddress);
}

static void UI_DeleteFavoriteServer_f(void)
{
    int i;

    if (s_selectedServer < 0) {
        return;
    }

    if (s_selectedServer > cls.numfavoriteservers) {
        return;
    }

    Com_Memset(&cls.favoriteServers[s_selectedServer].adr, 0, sizeof(netadr_t));
    Com_Printf("Server deleted\n"); 

    for (i = s_selectedServer; i + 1 < cls.numfavoriteservers; i++) {
        cls.favoriteServers[i] = cls.favoriteServers[i + 1];
    }

    cls.numfavoriteservers--;
    s_selectedServer = -1;
}

static void UI_EnableFavoriteButton_f(void)
{
    if (cl_updatesource->integer == 2) {
        Cbuf_ExecuteText(EXEC_NOW, "globalwidgetcommand dm_addfavorites disable");
        Cbuf_ExecuteText(EXEC_NOW, "globalwidgetcommand dm_deletefavorites enable");
    } else {
        Cbuf_ExecuteText(EXEC_NOW, "globalwidgetcommand dm_addfavorites enable");
        Cbuf_ExecuteText(EXEC_NOW, "globalwidgetcommand dm_deletefavorites disable");
    }
}

static void UI_GetServerStatus_f(void)
{
    serverInfo_t *servers = NULL;

    switch (s_selectedSource) {
    case 1:
        if (s_selectedServer > cls.numglobalservers) {
            return;
        }
        servers = &cls.globalServers[s_selectedServer];
        break;
    case 2:
        if (s_selectedServer > cls.numfavoriteservers) {
            return;
        }
        servers = &cls.favoriteServers[s_selectedServer];
        break;
    default:
        if (s_selectedServer > cls.numlocalservers) {
            return;
        }
        servers = &cls.localServers[s_selectedServer];
        break;
    }

    Cbuf_ExecuteText(EXEC_NOW, va("serverstatus %s\n", NET_AdrToString(servers->adr)));
}

static void UI_DisplayServerInformation_f(void)
{
    if (s_selectedServer == -1) {
        Com_Printf("No server selected\n"); 
        return;
    }

    menuManager.PushMenu(str("serverinfo"));
}

static void UI_ConnectSpecifiedServer_f(void)
{
    str      sAddress;
    str      sCommand;
    netadr_t adr;

    sAddress = Cvar_VariableString("ui_specifiedserverip"); 
    sAddress += ":";                                        
    sAddress += Cvar_VariableString("ui_specifiedserverport"); 

    if (NET_StringToAdr(sAddress.c_str(), &adr)) {
        sCommand = "ui_connectserverprompt "; 
        sCommand += sAddress;
        sCommand += "\n"; 

        Cbuf_ExecuteText(EXEC_NOW, sCommand.c_str());
    } else {
        menuManager.ShowMenu(str("OkDialog")); 
        UI_FocusMenuIfExists();
        Cbuf_AddText("globalwidgetcommand OkDialogTitle title \"$$InvalidFavoriteEntered$$\"\n");
    }
}

static void UI_GetNewServerList_f(void)
{
    str sKeywords;
    str sCommand;

    GameplayManager::getTheGameplayManager()->setStringValue(
        "Multiplayer", "MapScreenShot", "sysimg/2d/default-map-screen.tga", false);

    switch (cl_updatesource->integer) {
    case 1:
        Cbuf_ExecuteText(EXEC_NOW, "globalwidgetcommand dm_refreshstatus title \"Retrieving list of servers...\"");

        if (cl_showemptyservers->integer == 1) {
            sKeywords += " full"; 
        }
        if (cl_showfullservers->integer == 1) {
            sKeywords += " empty"; 
        }

        sCommand = "globalservers "; 
        sCommand += "0 ";            
        sCommand += 66;
        sCommand += " "; 
        sCommand += sKeywords;
        break;
    case 2:
        sCommand = "refreshserverlist"; 
        break;
    default:
        sCommand = "localservers"; 
        break;
    }

    s_selectedServer = -1;
    Cbuf_ExecuteText(EXEC_NOW, sCommand.c_str());
}

static void UI_RefreshServerList_f(void)
{
    GameplayManager::getTheGameplayManager()->setStringValue(
        "Multiplayer", "MapScreenShot", "sysimg/2d/default-map-screen.tga", false);

    if (!CL_GetServerCount()) {
        UI_GetNewServerList_f();
    }

    CL_RefreshServerList_f();

    s_selectedServer = -1;
}

static void UI_SetSelectedServer_f(void)
{
    LevelProperties  *level;
    GameplayManager  *gameplay;
    str               sMapName;

    if (Cmd_Argc() < 2) {
        Com_Printf("usage: selectedserver <index> <map name>\n"); 
        return;
    }

    s_selectedServer = atoi(Cmd_Argv(1));
    s_selectedSource = cl_updatesource->integer;

    gameplay = GameplayManager::getTheGameplayManager();
    sMapName = Cmd_Argv(2);

    level = arenaController->getLevelProperties(sMapName);
    if (level) {
        gameplay->setStringValue("Multiplayer", "MapScreenShot", level->m_screenshot, false);
    } else {
        gameplay->setStringValue("Multiplayer", "MapScreenShot", "sysimg/2d/default-map-screen.tga", false);
    }

    if (cl_updatesource->integer == 2) {
        gameplay->setFloatValue("Multiplayer", "EnableDelFavoritesButton", s_deleteFavoritesEnabled, false);
        gameplay->setFloatValue("Multiplayer", "EnableAddFavoritesButton", s_addFavoritesEnabled, false);
    } else {
        gameplay->setFloatValue("Multiplayer", "EnableAddFavoritesButton", s_addFavoritesEnabled, false);
        gameplay->setFloatValue("Multiplayer", "EnableDelFavoritesButton", s_deleteFavoritesEnabled, false);
    }
}

void CL_InitializeUI(void)
{
    int    nfiles;
    char **filenames;
    int    i;

    ui_drawcoords         = Cvar_Get("ui_drawcoords", "", 0);              
    // not the "" this block had.  Resolved off the same cl_ui pool (retail offsets in
    ui_changeresolution   = Cvar_Get("ui_changeresolution", "1", 1);        
    ui_minicon            = Cvar_Get("ui_minicon", "0", 1);                 
    ui_consoleposition    = Cvar_Get("ui_consoleposition", "", 1);         
    ui_inventoryfile      = Cvar_Get("ui_inventoryfile", "global/inventory.scr", 0); 
    ui_console            = Cvar_Get("ui_console", "0", 1);                
    ui_newvidmode         = Cvar_Get("ui_newvidmode", "-1", 0);            
    ui_dialogdisplaymode  = Cvar_Get("ui_dialogdisplaymode", "1", 1);      
    ui_radarviewmode      = Cvar_Get("ui_radarviewmode", "0", 1);          
    ui_radarmode          = Cvar_Get("ui_radarmode", "0", 0);              
    ui_serverrefresh      = Cvar_Get("ui_serverrefresh", "0", 0);          
    ui_mainMenuModel      = Cvar_Get("ui_mainMenuModel", "", 1);           
    cg_targetedPlayerName = Cvar_Get("cg_targetedPlayerName", "", 0);      
    ui_hudScale           = Cvar_Get("ui_hudScale", "0", CVAR_ARCHIVE);     
    UI_ComputeAutoHudScale();
    UI_LogHudScale();
    Cvar_Get("ui_consoleLineHistory", "300", 1);                           
    Cvar_Get("ui_startmap", "", 1);                                        
    Cvar_Get("dlg_badsave", "This save game is invalid", 0);               

    CL_FillUIDef();
    CL_FillUIImports();
    UI_InitExports();
    uie.Init();

    UI_LoadInventory_f();
    UI_CreateScoreboard();

    Cmd_AddCommand("flushui", UI_Flush_f);
    Cmd_AddCommand("currentmenu", UI_CurrentMenu_f);
    Cmd_AddCommand("pushmenu", UI_PushMenu_f);
    Cmd_AddCommand("hidemenu", UI_HideMenu_f);
    Cmd_AddCommand("forcemenu", UI_ForceMenu_f);
    Cmd_AddCommand("popmenu", UI_PopMenu_f);
    Cmd_AddCommand("transitionmenu", UI_TransitionMenu_f);
    Cmd_AddCommand("showmenu", UI_ShowMenu_f);
    Cmd_AddCommand("widgetcommand", UI_WidgetCommand_f);
    Cmd_AddCommand("globalwidgetcommand", UI_GlobalWidgetCommand_f);
    Cmd_AddCommand("listmenus", UI_ListMenus_f);
    Cmd_AddCommand("togglemenu", UI_ToggleMenu_f);
    Cmd_AddCommand("loadmenu", UI_LoadMenu_f);
    Cmd_AddCommand("maplist", UI_MapList_f);
    Cmd_AddCommand("availablemaps", UI_AvailableMaps_f);
    Cmd_AddCommand("viewspawnlist", UI_ViewSpawnList_f);
    Cmd_AddCommand("viewspawnquicklist", UI_ViewSpawnQuickList_f);
    Cmd_AddCommand("viewsetdialog", UI_ViewSetDialog_f);
    Cmd_AddCommand("soundpicker", UI_SoundPicker_f);
    Cmd_AddCommand("notepad", UI_Notepad_f);
    Cmd_AddCommand("editscript", UI_EditScript_f);
    Cmd_AddCommand("editshader", UI_EditShader_f);
    Cmd_AddCommand("inv_restart", UI_LoadInventory_f);
    Cmd_AddCommand("inventory", UI_ShowInventory_f);
    Cmd_AddCommand("warpinv", UI_WarpInventory_f);
    Cmd_AddCommand("nextinv", UI_NextInventory_f);
    Cmd_AddCommand("previnv", UI_PrevInventory_f);
    Cmd_AddCommand("ui_disableinventory", UI_DisableInventory_f);
    Cmd_AddCommand("ui_enableinventory", UI_EnableInventory_f);
    Cmd_AddCommand("ui_showmouse", UI_ShowMouse_f);
    Cmd_AddCommand("ui_hidemouse", UI_HideMouse_f);
    Cmd_AddCommand("ui_saveconsolepos", SaveConsoleRectangle);
    Cmd_AddCommand("ui_loadconsolepos", LoadConsoleRectangle);
    Cmd_AddCommand("clear", UI_ClearConsole_f);
    Cmd_AddCommand("ui_hud", UI_Hud_f);
    Cmd_AddCommand("ui_resetcvars", UI_ResetCvars);
    Cmd_AddCommand("ui_setrangecvar", UI_SetRangeCvar);
    Cmd_AddCommand("ui_verifycvarrange", UI_VerifyCvarRange);
    Cmd_AddCommand("ui_checkrestart", UI_CheckRestart);
    Cmd_AddCommand("dointro", UI_StartIntro_f);
    Cmd_AddCommand("_intro", UI_DoIntro_f);
    Cmd_AddCommand("centerprint", UI_CenterPrint_f);
    Cmd_AddCommand("locationprint", UI_LocationPrint_f);
    Cmd_AddCommand("saveweaps", UI_SaveWeapons_f);
    Cmd_AddCommand("ui_addhud", UI_AddHud_f);
    Cmd_AddCommand("ui_removehud", UI_RemoveHud_f);
    Cmd_AddCommand("ui_removehuds", UI_RemoveHuds_f);
    Cmd_AddCommand("startserver", UI_StartServer_f);
    Cmd_AddCommand("displaybranchdialog", UI_DisplayBranchDialog_f);
    Cmd_AddCommand("ui_connectspecifiedserver", UI_ConnectSpecifiedServer_f);
    Cmd_AddCommand("ui_newserverlist", UI_GetNewServerList_f);
    Cmd_AddCommand("ui_refreshserverlist", UI_RefreshServerList_f);
    Cmd_AddCommand("ui_setselectedserver", UI_SetSelectedServer_f);
    Cmd_AddCommand("ui_buildserverlist", UI_BuildServerList);
    Cmd_AddCommand("ui_getserverstatus", UI_GetServerStatus_f);
    Cmd_AddCommand("ui_displayserverinformation", UI_DisplayServerInformation_f);
    Cmd_AddCommand("ui_addfavoriteserver", UI_AddFavoriteServer_f);
    Cmd_AddCommand("ui_addfavoritespecserver", UI_AddFavoriteSpecServer);
    Cmd_AddCommand("ui_deletefavoriteserver", UI_DeleteFavoriteServer_f);
    Cmd_AddCommand("ui_enablefavoritebutton", UI_EnableFavoriteButton_f);
    Cmd_AddCommand("ui_setloadsavegamelist", UI_SetLoadSaveGameList_f);
    Cmd_AddCommand("ui_getsavegames", UI_GetSavedGames_f);
    Cmd_AddCommand("ui_getsavegameinfo", UI_GetSaveGameInfo_f);
    Cmd_AddCommand("ui_loadgame", UI_LoadGame_f);
    Cmd_AddCommand("ui_promptsavegame", UI_PromptSaveGame_f);
    Cmd_AddCommand("ui_savegame", UI_SaveGame_f);
    Cmd_AddCommand("ui_savegamewithname", UI_SaveGameWithName_f);
    Cmd_AddCommand("ui_autosavegame", UI_AutoSaveGame_f);
    Cmd_AddCommand("ui_deletesavegame", UI_DeleteSaveGame_f);
    Cmd_AddCommand("ui_loadlastsave", UI_LoadLastSave_f);
    Cmd_AddCommand("ui_quicksave", UI_QuickSave_f);
    Cmd_AddCommand("ui_quickload", UI_QuickLoad_f);
    Cmd_AddCommand("ui_showjoinbuttons", UI_ShowJoinTeamButtons_f);

    arenaController->registerCommands();

    view3d = new View3D;
    view3d->InitFrame(NULL, 0, 0, 640.0f, 480.0f, -1, "tahoma-10");
    view3d->setName("view3d");

    ui_static_materials.loading = uWinMan.RegisterShader("textures/menu/loading");

    UI_SetupConsoles();

    if (s_intermediateconsole.NumObjects() && fakk_console) {
        UIConsole *console = fakk_console->getConsole();

        for (i = 1; i <= s_intermediateconsole.NumObjects(); i++) {
            intermediate_console_text_t *record = s_intermediateconsole.ObjectAt(i);

            if (console && record->text) {
                console->AddText(record->text->c_str(), ConsolePrintColor(record->color));
            }

            if (record->text) {
                delete record->text;
            }

            delete record;
        }

        s_intermediateconsole.FreeObjectList();
    }

    filenames = FS_ListFiles("ui/", "urc", qfalse, &nfiles);

    for (i = 0; i < nfiles; i++) {
        char szFilename[MAX_QPATH];

        Com_sprintf(szFilename, sizeof(szFilename), "ui/%s", filenames[i]);
        new UILayout(szFilename);
    }

    FS_FreeFileList(filenames);

    // "ui_language" before; that string does not exist in the retail engine.
    {
        const char *language = CvarGetForUI("local_language", "Eng");
        char        locdir[MAX_QPATH];

        Com_sprintf(locdir, sizeof(locdir), "loc/%s/ui/", language);
        filenames = FS_ListFiles(locdir, "urc", qfalse, &nfiles);

        for (i = 0; i < nfiles; i++) {
            char szFilename[MAX_QPATH];

            Com_sprintf(szFilename, sizeof(szFilename), "%s%s", locdir, filenames[i]);
            new UILayout(szFilename);
        }

        FS_FreeFileList(filenames);
    }

    UI_AdaptSettingsMenus();

    // retail's widgetContainerCompare @0x000B4450 never answers 0.  R4's replay over the
    static char uivk_creation[24576];
    uivk_creation[0] = 0;
    {
        int uivkn = uWinMan.GetChildCount();

        UIVK_PROBE("creation", "children=%d", uivkn);

        for (int uivki = 1; uivki <= uivkn && strlen(uivk_creation) < 24000; uivki++) {
            UIWidget *uivkw = uWinMan.GetChild(uivki);

            Q_strcat(uivk_creation, sizeof(uivk_creation),
                     va("%s/%.2f ", uivkw ? uivkw->getName() : "?", (double)(uivkw ? uivkw->getDepth() : -1.0f)));
        }
    }

    uWinMan.CreateMenus();
    uWinMan.sortWidgetContainers();

    if (PROBE_On()) {
        static const char *paint_ids[3] = {"$$Prologue$$", "$$Prologue2$$", "$$SkipPrologue$$"};
        int                 painti;

        PAINT_PROBE("loc:resource", "local_language='%s'", CvarGetForUI("local_language", "Eng"));

        for (painti = 0; painti < 3; painti++) {
            const char *paint_id = paint_ids[painti];
            const char *paint_val = isStringResource(paint_id) ? getStringResource(paint_id) : NULL;

            PAINT_PROBE("loc:resource",
                       "id='%s' isResource=%d valueLen=%d%s",
                       paint_id,
                       isStringResource(paint_id) ? 1 : 0,
                       paint_val ? (int)strlen(paint_val) : -1,
                       (paint_val && !strlen(paint_val)) ? "  <-- EMPTY LOOKUP: the crawl text is blank"
                                                       : "");
        }

        PAINT_PROBE("loc:resource",
                   "menu 'prologue' %s; ui_addhud list=%d creation=%s",
                   menuManager.FindMenu(str("prologue")) ? "FOUND" : "MISSING",
                   (int)hudList.NumObjects(),
                   uivk_creation);
    }

    getAllWeaponHuds();
    hud_health = menuManager.FindMenu("healthhud");          
    printhud = menuManager.FindMenu("printhud");             
    hud_boss = menuManager.FindMenu("bosshud");              
    itemTextHud = menuManager.FindMenu("itemTextHud");       
    poweruphud = menuManager.FindMenu("poweruphud");         
    radarhud = menuManager.FindMenu("radarhud");             
    missionfailure = menuManager.FindMenu("missionfailure"); 
    missionsuccess = menuManager.FindMenu("missionsuccess"); 
    headhud = menuManager.FindMenu("headhud");               
    headtexthud = menuManager.FindMenu("headtexthud");       
    texthud = menuManager.FindMenu("texthud");               
    specialmovetimer = menuManager.FindMenu("specialmovetimer"); 
    pointshud = menuManager.FindMenu("pointshud");           
    hud_levelExit = menuManager.FindMenu("levelExit");       
    hud_targetName = menuManager.FindMenu("targetNameHud");  

    uWinMan.ChangeResolution();

    /* `uWinMan.ChangeResolution' laid every menu out with the retail virtual
       space, and that is what latches each container's authored frame.  The
       client-managed HUD menus are laid out again here in their own scope. */
    UI_ScaleStaticHudMenus();

    mainmenu = menuManager.FindMenu("main");

    IN_MouseOn();

    Cbuf_AddText("dointro\n");

    menuManager.RealignMenus();

    // then both full record arrays -- retail reads the whole array, not count
    {
        fileHandle_t f;
        int          checksum = 0;

        f = 0;
        FS_FOpenFileRead("serverlist.dat", &f, qfalse, qfalse);
        if (f) {
            FS_Read(&cls.numglobalservers, 4, f);
            FS_Read(&cls.numfavoriteservers, 4, f);
            FS_Read(&checksum, 4, f);
            if (checksum == cls.numglobalservers + cls.numfavoriteservers) {
                FS_Read(cls.globalServers, sizeof(cls.globalServers), f);
                FS_Read(cls.favoriteServers, sizeof(cls.favoriteServers), f);
                FS_FCloseFile(f);
            } else {
                // retail resets both counts here and (a retail bug, kept) never
                cls.numglobalservers   = 0;
                cls.numfavoriteservers = 0;
            }
        }
    }

    cls.uiStarted = qtrue;
}


static constexpr unsigned int MAP_INFO_VERSION = 3;

#if 0
static unsigned int  startCountLow;
static unsigned int  startCountHigh;
static unsigned int  loadCountLow;
static unsigned int  loadCountHigh;
#endif
std::chrono::time_point<std::chrono::steady_clock> startCount;

static unsigned int  loadCount;
static unsigned int  lastTime   = 0;
static unsigned int  updateTime = 0;
static unsigned int  loadNumber;
static unsigned int  totalLoadTime;
static unsigned int  currentLoadTime;

static char        **loadStrings;
static unsigned int *loadTimes;
static char         *loadStringsBuffer;

class LoadResourceInfo
{
public:
    class LoadResourceInfo *next;
    unsigned int            loadCount;
    char                    name[1];

public:
    LoadResourceInfo();
};

static LoadResourceInfo *loadHead;

void UI_BeginLoadResource(void)
{
#if 0
    clock_t time = clock();

    startCountHigh = time >> 32;
    startCountLow  = time;
#endif

    startCount = std::chrono::steady_clock::now();
}

void UI_EndLoadResource(void)
{
#if 0
    clock_t time;

    time =
        clock() - (((clock_t)startCountHigh << 32) | startCountLow) + (((clock_t)loadCountHigh << 32) | loadCountLow);

    loadCountHigh = time >> 32;
    loadCountLow  = time;
    loadCount     = time >> 25;
#endif

    std::chrono::nanoseconds deltaTime;

    deltaTime = std::chrono::steady_clock::now() - startCount;

    loadCount = std::chrono::duration_cast<std::chrono::milliseconds>(deltaTime).count();
}

void UI_EndLoadResource(const char *name)
{
    LoadResourceInfo *newLoadHead;

    if (!loadCount) {
        return;
    }

    newLoadHead            = (LoadResourceInfo *)Z_Malloc(sizeof(LoadResourceInfo) + strlen(name));
    newLoadHead->next      = loadHead;
    newLoadHead->loadCount = loadCount;
    strcpy(newLoadHead->name, name);
    loadHead = newLoadHead;

    loadNumber++;

#if 0
    loadCountLow  = 0;
    loadCountHigh = 0;
#endif
}

qboolean UI_IsResourceLoaded(const char *name)
{
    switch (*name) {
    case 97:
    case 98:
    case 99:
    case 100:
    case 101:
    case 104:
        return TIKI_FindTikiAnim(name + 1) != NULL;
    case 103:
        return SkeletorCacheFindFilename(name + 1, NULL);
    case 107:
        return S_IsSoundRegistered(name + 1);
    case 110:
        return re.ImageExists(name + 1);
    default:
        return qfalse;
    }
}

void UI_RegisterLoadResource(const char *name)
{
    int i, j;
    int low, high;

    low  = 0;
    high = loadNumber - 1;

    while (low <= high) {
        i = (low + high) / 2;
        j = strcmp(name, loadStrings[i]);

        if (j < 0) {
            high = i - 1;
        } else if (j > 0) {
            low = i + 1;
        } else {
            currentLoadTime += loadTimes[i];
            loadTimes[i] = 0;
            break;
        }
    }
}

void UI_TestUpdateScreen(unsigned int timeout)
{
    unsigned int newTime = Sys_Milliseconds();
    unsigned int startRenderTime, endRenderTime;

    if (timeout > 0 && (newTime - lastTime) < (timeout + updateTime)) {
        return;
    }

    startRenderTime = Sys_Milliseconds();
    Sys_PumpMessageLoop();
    SCR_UpdateScreen();
    endRenderTime = Sys_Milliseconds();

    updateTime = Q_min(endRenderTime - startRenderTime, 1000);
    lastTime   = endRenderTime;
}

void UI_ClearResource(void)
{
    if (cls.loading == LOAD_PROGRESS_FALSE || cls.loading != LOAD_PROGRESS_SAVE) {
        return;
    }

    UI_BeginLoadResource();
    UI_TestUpdateScreen(0);
}

void UI_LoadResource(const char *name)
{
    if (cls.loading == LOAD_PROGRESS_FALSE) {
        return;
    }

    if (cls.loading == LOAD_PROGRESS_SAVE) {
        UI_EndLoadResource();
        UI_EndLoadResource(name);
        UI_BeginLoadResource();
    } else if (cls.loading == LOAD_PROGRESS_LOAD) {
        UI_RegisterLoadResource(name);
        Cvar_SetValue("loadingbar", (float)currentLoadTime / (float)totalLoadTime);
    }

    UI_TestUpdateScreen(33);
}

void UI_FreeLoadStrings()
{
    if (loadStrings) {
        Z_Free(loadStrings);
        loadStrings = NULL;
    }
}

void UI_DeleteLoadInfo()
{
    LoadResourceInfo *ptr;
    LoadResourceInfo *nextPtr;

    for (ptr = loadHead; ptr != NULL; ptr = nextPtr) {
        nextPtr = ptr->next;
        Z_Free(ptr);
    }
}

bool UI_ParseLoadMapinfo(Script *scr)
{
    int         size, index;
    int         allocLen;
    char       *pos;
    const char *token;
    char       *newPos;
    int         time;

    loadNumber = scr->GetInteger(false);
    size       = scr->GetInteger(false);
    scr->SkipWhiteSpace(true);

    allocLen          = size + sizeof(loadStrings[0]) * loadNumber + sizeof(loadTimes[0]) * loadNumber;
    loadStrings       = (char **)Z_Malloc(allocLen);
    loadTimes         = (unsigned int *)(loadStrings + loadNumber);
    loadStringsBuffer = (char *)(loadTimes + loadNumber);
    pos               = loadStringsBuffer;

    totalLoadTime = 0;
    index         = 0;

    do {
        token  = scr->GetToken(true);
        newPos = &pos[strlen(token) + 1];
        strcpy(pos, token);
        loadStrings[index] = pos;

        time = scr->GetInteger(true);

        if (UI_IsResourceLoaded(pos)) {
            loadTimes[index] = 0;
        } else {
            loadTimes[index] = time;
            totalLoadTime += time;
        }

        pos = newPos;
        index++;
    } while ((unsigned int)index != loadNumber);

    assert(pos <= (char *)loadStrings + allocLen);

    if (newPos != loadStringsBuffer + size) {
        UI_FreeLoadStrings();
        return false;
    }

    return true;
}

bool UI_ArchiveLoadMapinfo(const char *mapname)
{
    Script scr;
    int    version;
    byte  *tempbuf;
    int    length;
    bool   success;

    length = FS_ReadFile(loadName, (void **)&tempbuf);

    if (length <= 0) {
        return false;
    }

    scr.LoadFile(loadName, length, (const char *)tempbuf);
    FS_FreeFile(tempbuf);

    version = scr.GetInteger(false);
    scr.SkipWhiteSpace(true);

    if (version == MAP_INFO_VERSION) {
        success = UI_ParseLoadMapinfo(&scr);
    } else {
        Com_Printf("Expecting version %d map info file.  Map info is version %d.\n", MAP_INFO_VERSION, version);
        success = false;
    }

    scr.Close();
    return success;
}

static qboolean UI_LD_NextToken(const char **pp, char *out, int outSize)
{
    const char *p = *pp;
    int         n = 0;

    while (*p && (unsigned char)*p <= ' ') {
        p++;
    }
    if (!*p) {
        *pp = p;
        return qfalse;
    }

    if (*p == '"') {
        p++;
        while (*p && *p != '"') {
            if (n < outSize - 1) {
                out[n++] = *p;
            }
            p++;
        }
        if (*p == '"') {
            p++;
        }
    } else {
        while (*p && (unsigned char)*p > ' ') {
            if (n < outSize - 1) {
                out[n++] = *p;
            }
            p++;
        }
    }
    out[n] = '\0';
    *pp    = p;
    return qtrue;
}

static str UI_LoadMenuNameForMap(const char *pszMapName)
{
    static const char *ldFiles[] = {
        "global/leveldefs.txt",  "global/leveldefs2.txt", "global/leveldefs3.txt",
        "global/leveldefs4.txt", "global/leveldefs5.txt", "global/leveldefs6.txt",
        "global/leveldefs7.txt", "global/leveldefs8.txt", "global/leveldefs9.txt",
    };
    int fi;

    for (fi = 0; fi < (int)(sizeof(ldFiles) / sizeof(ldFiles[0])); fi++) {
        char       *buf = NULL;
        long        len = FS_ReadFile(ldFiles[fi], (void **)&buf);
        const char *p;
        str         curMenu = "";
        str         found   = "";
        char        tok[256];

        if (len <= 0 || !buf) {
            continue;
        }

        p = buf;
        while (UI_LD_NextToken(&p, tok, sizeof(tok))) {
            if (!str::icmp(tok, "env")) {
                UI_LD_NextToken(&p, tok, sizeof(tok)); 
                if (UI_LD_NextToken(&p, tok, sizeof(tok))) {
                    curMenu = tok; 
                }
                UI_LD_NextToken(&p, tok, sizeof(tok)); 
            } else if (!str::icmp(tok, "level") || !str::icmp(tok, "sublevel")) {
                if (UI_LD_NextToken(&p, tok, sizeof(tok)) && !str::icmp(tok, pszMapName)) {
                    found = curMenu;
                    break;
                }
            }
        }

        FS_FreeFile(buf);
        if (found.length()) {
            return found;
        }
    }

    return "";
}

void UI_UpdateLoadingStatus(int stage)
{
    str  base;
    int  i;

    if (server_loading_stage) {
        base = "sublevelLoading";
    } else if (ui_sLevelLoadingScreen.length()) {
        base = ui_sLevelLoadingScreen;
    } else {
        base = "loading";
    }

    for (i = 1; i <= 5; i++) {
        str name = base + "Status" + i;

        menuManager.PassEventToWidget(
            name, new Event(i == stage ? EV_Widget_Enable : EV_Widget_Disable));
    }
}

void UI_BeginLoad(const char *pszMapName)
{
    str mapfile;

    if (cls.loading != LOAD_PROGRESS_FALSE) {
        if (cls.loading == LOAD_PROGRESS_SAVE) {
            UI_DeleteLoadInfo();
        }

        UI_FreeLoadStrings();
        cls.loading = LOAD_PROGRESS_FALSE;
    }

    server_loading         = qtrue;
    server_loading_waiting = qfalse;
    menuManager.setLoading(true);
    strcpy(server_mapname, pszMapName);

    {
        str loadMenuName = UI_LoadMenuNameForMap(server_mapname);
        const char *wanted = loadMenuName.length() ? loadMenuName.c_str() : server_mapname;

        ui_sLevelLoadingScreen = loadMenuName;

        if (str::icmp(ui_sCurrentLoadingMenu, wanted)) {
            ui_sCurrentLoadingMenu = wanted;
            ui_pLoadingMenu        = menuManager.FindMenu(wanted);
        }
    }

    if (!ui_pLoadingMenu) {
        ui_sCurrentLoadingMenu = "loading";
        ui_pLoadingMenu        = menuManager.FindMenu(ui_sCurrentLoadingMenu);
    }

    UI_ForceMenuOff(true);

    if (ui_pLoadingMenu) {
        if (!developer->integer && UI_ConsoleIsVisible()) {
            UI_CloseConsole();
        }

        ui_pLoadingMenu->PassEventToWidget("continuebutton", new Event(EV_Widget_Disable));

        loadName = "maps/";
        loadName += pszMapName;
        mapfile = loadName;
        loadName += ".min";

        mapfile += ".bsp";

        if (UI_ArchiveLoadMapinfo(mapfile)) {
            cls.loading = LOAD_PROGRESS_LOAD;
        } else {
            cls.loading = LOAD_PROGRESS_SAVE;
        }

        if (cls.loading == LOAD_PROGRESS_LOAD) {
            ui_pLoadingMenu->PassEventToWidget("loadingflasher", new Event(EV_Widget_Disable));
            ui_pLoadingMenu->PassEventToWidget("loadingbar", new Event(EV_Widget_Enable));
            ui_pLoadingMenu->PassEventToWidget("loadingbar_border", new Event(EV_Widget_Enable));

            currentLoadTime = 0;
        } else {
            ui_pLoadingMenu->PassEventToWidget("loadingflasher", new Event(EV_Widget_Enable));
            ui_pLoadingMenu->PassEventToWidget("loadingbar", new Event(EV_Widget_Disable));
            ui_pLoadingMenu->PassEventToWidget("loadingbar_border", new Event(EV_Widget_Disable));

            UI_BeginLoadResource();
#if 0
            loadCountLow  = 0;
            loadCountHigh = 0;
#endif
            loadHead   = NULL;
            loadNumber = 0;
        }

        Cvar_SetValue("loadingbar", 0);

        L_ProcessPendingEvents();

        lastTime = Sys_Milliseconds();
    } else {
        cls.loading = LOAD_PROGRESS_DEFAULT;

        L_ProcessPendingEvents();

        lastTime = Sys_Milliseconds();
    }

    if (com_sv_running->integer) {
        SCR_UpdateScreen();
    }
}

int UI_CompareLoadResources(const void *elem1, const void *elem2)
{
    LoadResourceInfo *e1 = *(LoadResourceInfo **)elem1;
    LoadResourceInfo *e2 = *(LoadResourceInfo **)elem2;

    return strcmp(e1->name, e2->name);
}

void UI_EndLoad(void)
{
    LoadResourceInfo  *ptr;
    LoadResourceInfo **base;
    int                i;
    fileHandle_t       file;
    int                size;
    char               buf[1024];

    if (cls.loading == LOAD_PROGRESS_FALSE) {
        return;
    }

    UI_LoadResource("*end");

    switch(cls.loading)
    {
    case LOAD_PROGRESS_SAVE:
        if (loadNumber) {
            base = (LoadResourceInfo **)Z_Malloc(loadNumber * sizeof(LoadResourceInfo *));

            i = 0;
            for (ptr = loadHead; ptr != NULL; ptr = ptr->next) {
                base[i] = ptr;
                i++;
            }

            qsort(base, loadNumber, sizeof(LoadResourceInfo *), UI_CompareLoadResources);

            size = 0;
            for (i = 0; (unsigned int)i < loadNumber; i++) {
                ptr = base[i];
                size += strlen(ptr->name) + 1;
            }

            file = FS_FOpenFileWrite_HomeData(loadName);
            Com_sprintf(buf, sizeof(buf), "%d\n%d %d\n", MAP_INFO_VERSION, loadNumber, size);
            FS_Write(buf, strlen(buf), file);

            for (i = 0; (unsigned int)i < loadNumber; i++) {
                Com_sprintf(buf, sizeof(buf), "%s\n%d\n", base[i]->name, base[i]->loadCount);
                FS_Write(buf, strlen(buf), file);
            }

            FS_FCloseFile(file);
            Z_Free(base);
        }
        UI_DeleteLoadInfo();
        break;
    case LOAD_PROGRESS_LOAD:
        for (i = 0; (unsigned int)i < loadNumber; i++) {
            if (loadTimes[i]) {
                loadTimes[i] = 0;
            }
        }

        currentLoadTime = totalLoadTime;
        break;
    default:
        break;
    }

    UI_FreeLoadStrings();

    cls.loading = LOAD_PROGRESS_FALSE;
}

void UI_AbortLoad(void)
{
    if (cls.loading != LOAD_PROGRESS_FALSE) {
        if (cls.loading == LOAD_PROGRESS_SAVE) {
            UI_DeleteLoadInfo();
        }
        cls.loading = LOAD_PROGRESS_FALSE;
    }
}

