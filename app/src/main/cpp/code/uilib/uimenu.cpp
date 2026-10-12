/*
===========================================================================
Copyright (C) 2015 the OpenMoHAA team

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

#include "ui_local.h"
#include "../qcommon/q_version.h"
#include "../qcommon/stringresource.h"

#define PROBE_GET(probe_n, probe_d, probe_f) ((cvar_t *)0)
#define PROBE_PRINT                       Com_Printf
#define PROBE_ON_OVERRIDE                       (atoi(CvarGetForUI("qa_probe", "0")) != 0)  
#include "../qcommon/qa_probe.h"

#include "../qcommon/qa_ui.h"

MenuManager menuManager;

Event EV_PushMenu
(
    "pushmenu",
    EV_DEFAULT,
    "s",
    "menuname",
    "Pushes the menu on the stack"
);

Event EV_LockMenus
(
    "lock",
    EV_DEFAULT,
    NULL,
    NULL,
    "Lock out the menu from receiving input"
);

Event EV_UnlockMenus
(
    "unlock",
    EV_DEFAULT,
    NULL,
    NULL,
    "Unlock the menu from receiving input"
);

Event EV_ShowMenu
(
    "showmenu",
    EV_DEFAULT,
    "B",
    "activate",
    "Shows the menu."
);

Event EV_HideMenu
(
    "hidemenu",
    EV_DEFAULT,
    NULL,
    NULL,
    "Hides the menu."
);

CLASS_DECLARATION(Listener, Menu, NULL) {
    {&EV_HideMenu, &Menu::HideMenu},
    {&EV_ShowMenu, &Menu::ShowMenu},
    {NULL,         NULL           }
};

Menu::Menu()
{
    menuManager.AddMenu(this);
    m_fullscreen = qfalse;
}

Menu::Menu(str name)
{
    setName(name);
    m_fullscreen = qfalse;
    menuManager.AddMenu(this);
}

void Menu::AddMenuItem(UIWidget *item)
{
    m_itemlist.AddObject(item);

    for (int i = 1; i <= item->m_children.NumObjects(); i++) {
        m_itemlist.AddObject(item->m_children.ObjectAt(i));
    }
}

void Menu::DeleteMenuItem(UIWidget *item)
{
    for (int i = item->m_children.NumObjects(); i > 0; i--) {
        m_itemlist.RemoveObject(item->m_children.ObjectAt(i));
    }

    m_itemlist.RemoveObject(item);
}

void Menu::setName(str name)
{
    m_name = name;
}

void Menu::ShowMenu(Event *ev)
{
    qboolean activate;
    int      i;
    int      n;

    n = m_itemlist.NumObjects();
    for (i = 1; i <= n; i++) {
        UIWidget *wid = m_itemlist.ObjectAt(i);

        if (wid->isEnabled()) {
            wid->setShow(true);
            wid->ResetMotion(MOTION_IN);
            wid->UpdateUIElement();
            wid->UpdateData();
            wid->ExecuteShowCommands();
            wid->CancelEventsOfType(EV_Widget_Hide);
        } else {
            PROBE(
                "inv:showmenu",
                "SKIPPED disabled widget: menu='%s' widget='%s' enabled=0 visible=%d",
                m_name.c_str(),
                wid->getName(),
                wid->getShow() ? 1 : 0
            );
        }
    }

    if (ev && ev->NumArgs() > 0) {
        activate = ev->GetBoolean(1);
    } else {
        activate = qtrue;
    }

    if (PROBE_On() && !strcmp(m_name.c_str(), "dm_scoreboard")) {
        static int uirect_sbmenu;

        if (PROBE_Throttle(&uirect_sbmenu, 6, 20)) {
            UIRECT_PROBE("sb:menuwidgets", "menu='%s' widgets=%d", m_name.c_str(), n);
            for (int uirecti = 1; uirecti <= n; uirecti++) {
                m_itemlist.ObjectAt(uirecti)->PAINTDumpState(uirecti, m_name.c_str());
            }
        }
    }

    if (n && activate) {
        UIWidgetContainer *widcon = (UIWidgetContainer *)m_itemlist.ObjectAt(1);
        uWinMan.ActivateControl(widcon);
        widcon->SetLastActiveWidgetOrderNum();
    }

    uWinMan.setFirstResponder(NULL);
}

void Menu::HideMenu(Event *ev)
{
    int   i, n;
    bool  force = false;
    float maxtime;

    maxtime = GetMaxMotionTime();

    if (ev->NumArgs() > 0) {
        force = ev->GetBoolean(1);
    }

    n = m_itemlist.NumObjects();
    for (i = 1; i <= n; i++) {
        UIWidget *wid = m_itemlist.ObjectAt(i);

        if (wid->getMotionType() != MOTION_OUT) {
            wid->ResetMotion(MOTION_OUT);
            if (!force) {
                Event *event = new Event("hide");
                wid->PostEvent(event, maxtime);
            } else {
                wid->setShow(false);
            }
        }
    }
}

void Menu::ForceShow(void)
{
    int i;
    int n;

    n = m_itemlist.NumObjects();
    for (i = 1; i <= n; i++) {
        UIWidget *wid = m_itemlist.ObjectAt(i);

        if (!wid->getShow()) {
            wid->ResetMotion(MOTION_IN);
        }

        wid->ExecuteShowCommands();

        if (wid->isEnabled()) {
            wid->setShow(true);
        }
    }
}

void Menu::ForceHide(void)
{
    int i;
    int n;

    n = m_itemlist.NumObjects();
    for (i = 1; i <= n; i++) {
        UIWidget *wid = m_itemlist.ObjectAt(i);

        if (wid->getShow()) {
            wid->ResetMotion(MOTION_OUT);
        }

        wid->setShow(false);
    }
}

UIWidget *Menu::GetContainerWidget(void)
{
    if (m_itemlist.NumObjects() > 0) {
        return m_itemlist.ObjectAt(1);
    } else {
        return NULL;
    }
}

int Menu::GetWidgetCount(void)
{
    return m_itemlist.NumObjects();
}

UIWidget *Menu::GetWidget(int index)
{
    return m_itemlist.ObjectAt(index);
}

qboolean Menu::isNoEscape(void)
{
    UIWidget *w = GetContainerWidget();

    if (!w || !w->isSubclassOf(UIWidgetContainer)) {
        return qfalse;
    }

    return ((UIWidgetContainer *)w)->getNoEscape();
}

bool Menu::CheckNoMouse(void)
{
    UIWidget *w = GetContainerWidget();

    if (!w || !w->isSubclassOf(UIWidgetContainer)) {
        return false;
    }

    return ((UIWidgetContainer *)w)->CheckNoMouse();
}

bool Menu::allowGameFocus(void)
{
    UIWidget *w = GetContainerWidget();

    if (!w || !w->isSubclassOf(UIWidgetContainer)) {
        return false;
    }

    return ((UIWidgetContainer *)w)->getAllowGameFocus();
}

UIWidget *Menu::GetNamedWidget(const char *pszName)
{
    int       i;
    UIWidget *pWidget;

    for (i = 1; i <= m_itemlist.NumObjects(); i++) {
        pWidget = m_itemlist.ObjectAt(i);
        if (!stricmp(pszName, pWidget->getName())) {
            return pWidget;
        }
    }

    return NULL;
}

void Menu::Update(void)
{
    int       i;
    int       n;
    UIWidget *wid;
    UIWidget *cont = GetContainerWidget();
    bool      showBefore;
    bool      showAfter;

    if (!cont) {
        return;
    }

    showBefore = cont->IsVisible();

    cont->update();

    showAfter = cont->IsVisible();

    if (showAfter != true || showBefore) {
        if (!showAfter && showBefore) {
            n = m_itemlist.NumObjects();
            for (i = 1; i <= n; i++) {
                wid = m_itemlist.ObjectAt(i);
                if (wid->getShow()) {
                    wid->ResetMotion(MOTION_OUT);
                }
                wid->setShow(false);
            }
        }
    } else {
        n = m_itemlist.NumObjects();
        for (i = 1; i <= n; i++) {
            wid = m_itemlist.ObjectAt(i);
            if (!wid->getShow()) {
                wid->ResetMotion(MOTION_IN);
            }
            wid->UpdateUIElement();
            wid->UpdateData();
            wid->ExecuteShowCommands();
            if (wid->isEnabled()) {
                wid->setShow(true);
            }
        }
    }
}

void Menu::RealignWidgets(void)
{
    int i;
    int n;

    n = m_itemlist.NumObjects();
    for (i = 1; i <= n; i++) {
        UIWidget *wid = m_itemlist.ObjectAt(i);
        wid->Realign();
    }
}

float Menu::GetMaxMotionTime(void)
{
    int   i;
    int   n;
    float maxtime = 0.0;

    n = m_itemlist.NumObjects();
    for (i = 1; i <= n; i++) {
        UIWidget *wid = m_itemlist.ObjectAt(i);
        if (maxtime < wid->getMotionTime()) {
            maxtime = wid->getMotionTime();
        }
    }

    return maxtime;
}

void Menu::ActivateMenu(void)
{
    if (m_itemlist.NumObjects()) {
        UIWidgetContainer *widcon = (UIWidgetContainer *)m_itemlist.ObjectAt(1);
        uWinMan.ActivateControl(widcon);
        widcon->SetLastActiveWidgetOrderNum();
        uWinMan.setFirstResponder(NULL);
        widcon->BringToFrontPropogated();
    }
}

qboolean Menu::isFullscreen(void)
{
    return m_fullscreen;
}

void Menu::setFullscreen(qboolean bFullScreen)
{
    m_fullscreen = bFullScreen;
}

int Menu::getVidMode(void)
{
    return m_vidmode;
}

void Menu::setVidMode(int iMode)
{
    m_vidmode = iMode;
}

void Menu::NotifyPushMenu(void)
{
    UIWidget *widcon = GetContainerWidget();

    if (widcon) {
        widcon->ExecutePushCommands();
    }
}

void Menu::NotifyPopMenu(void)
{
    UIWidget *widcon = GetContainerWidget();

    if (widcon) {
        widcon->ExecutePopCommands();
    }
}

qboolean Menu::isVisible(void)
{
    UIWidget *wid = GetContainerWidget();
    if (wid) {
        return wid->IsVisible();
    }

    return false;
}

void Menu::SaveCVars(void)
{
    int i;
    int n;

    n = m_itemlist.NumObjects();
    for (i = 1; i <= n; i++) {
        UIWidget *wid = m_itemlist.ObjectAt(i);

        if (wid->m_cvarname.length()) {
            const char *ret = UI_GetCvarString(wid->m_cvarname, NULL);
            if (ret && *ret) {
                wid->m_cvarvalue = ret;
            } else {
                wid->m_cvarvalue = "";
            }
            if (PROBE_On()) {
                PROBE_PRINT("[UI:cvarsave] menu='%s' %s='%s'\n", m_name.c_str(),
                            wid->m_cvarname.c_str(), wid->m_cvarvalue.c_str());
            }
        }
    }
}

void Menu::RestoreCVars(void)
{
    int i;
    int n;

    n = m_itemlist.NumObjects();
    for (i = 1; i <= n; i++) {
        UIWidget *wid = m_itemlist.ObjectAt(i);

        if (wid->m_cvarname.length()) {
            // Cvar_Set2("") zeroes the cvar: an empty save must not round-trip.
            if (!wid->m_cvarvalue.length()) {
                if (PROBE_On()) {
                    PROBE_PRINT("[UI:cvarrestore] menu='%s' %s SKIP(empty)\n",
                                m_name.c_str(), wid->m_cvarname.c_str());
                }
                continue;
            }
            if (PROBE_On()) {
                PROBE_PRINT("[UI:cvarrestore] menu='%s' %s='%s'\n", m_name.c_str(),
                            wid->m_cvarname.c_str(), wid->m_cvarvalue.c_str());
            }
            uii.Cvar_Set(wid->m_cvarname, wid->m_cvarvalue);
        }
    }
}

void Menu::ResetCVars(void)
{
    int i;
    int n;

    n = m_itemlist.NumObjects();
    for (i = 1; i <= n; i++) {
        UIWidget *wid = m_itemlist.ObjectAt(i);

        if (wid->m_cvarname.length()) {
            uii.Cvar_Reset(wid->m_cvarname);
        }
    }
}

void Menu::PassEventToWidget(str name, Event *ev)
{
    int i;
    int n;

    n = m_itemlist.NumObjects();
    for (i = 1; i <= n; i++) {
        UIWidget *wid = m_itemlist.ObjectAt(i);
        if (wid->getName() == name && wid->ValidEvent(ev->getName())) {
            wid->ProcessEvent(ev);
            return;
        }
    }

    delete ev;
}

void Menu::PassEventToAllWidgets(Event& ev)
{
    int i;
    int n;

    n = m_itemlist.NumObjects();
    for (i = 1; i <= n; i++) {
        UIWidget *wid = m_itemlist.ObjectAt(i);
        if (wid->ValidEvent(ev.getName())) {
            wid->ProcessEvent(ev);
        }
    }
}

void Menu::CheckRestart(void)
{
    cvar_t  *cvar;
    qboolean do_restart     = false;
    qboolean do_snd_restart = false;
    qboolean do_ter_restart = false;
    int      i;
    int      n;
    const char *latchname = "";
    const char *sndname   = "";
    const char *tername   = "";

    n = m_itemlist.NumObjects();
    for (i = 1; i <= n; i++) {
        UIWidget *wid = m_itemlist.ObjectAt(i);

        if (wid->m_cvarname.length()) {
            cvar = uii.Cvar_Find(wid->m_cvarname);
            if (cvar && cvar->latchedString) {
                if (str::icmp(wid->m_cvarvalue, cvar->latchedString)) {
                    if (cvar->flags & CVAR_LATCH) {
                        do_restart = true;
                        latchname = wid->m_cvarname.c_str();
                    } else if (cvar->flags & CVAR_SOUND_LATCH) {
                        do_snd_restart = true;
                        sndname = wid->m_cvarname.c_str();
                    } else if (cvar->flags & CVAR_TERRAIN_LATCH) {
                        do_ter_restart = true;
                        tername = wid->m_cvarname.c_str();
                    }
                }
            }
        }
    }

    if (PROBE_On()) {
        PROBE_PRINT("[UI:checkrestart] menu='%s' latch=%s snd=%s ter=%s -> %s\n",
                    m_name.c_str(),
                    do_restart ? latchname : "-",
                    do_snd_restart ? sndname : "-",
                    do_ter_restart ? tername : "-",
                    do_restart ? "vid_restart"
                               : (do_snd_restart || do_ter_restart) ? "snd/ter_restart"
                                                                    : "none");
    }

    // Retail: LATCH -> vid_restart (CheckRestart @0x0006DEB0); the old Android arm dropped it.
    if (do_restart) {
        uii.Cmd_Stuff("vid_restart\n");
    }

    if (do_snd_restart) {
        uii.Cmd_Stuff("snd_restart\n");
    }

    if (do_ter_restart) {
        uii.Cmd_Stuff("ter_restart\n");
    }
}

CLASS_DECLARATION(Listener, MenuManager, NULL) {
    {&EV_PushMenu,    &MenuManager::PushMenu},
    {&EV_LockMenus,   &MenuManager::Lock    },
    {&EV_UnlockMenus, &MenuManager::Unlock  },
    {NULL,            NULL                  }
};

MenuManager::MenuManager()
{
    m_lock = false;
}

void MenuManager::RealignMenus(void)
{
    int i;
    int num;

    num = m_menulist.NumObjects();
    for (i = 1; i <= num; i++) {
        m_menulist.ObjectAt(i)->RealignWidgets();
    }
}

void MenuManager::ChangeResolution(void)
{
    int i;
    int j;
    int num;
    int n;

    num = m_menulist.NumObjects();
    for (i = 1; i <= num; i++) {
        Menu     *menu = m_menulist.ObjectAt(i);
        UIWidget *cont = menu->GetContainerWidget();
        float     hudScale = cont ? cont->getHudScale() : 1.0f;
        /* A full-screen HUD overlay keeps retail geometry; the draw scope still grows its text.
           Everything else is laid out in the HUD scope and keeps its declared corner. The scope is
           entered whenever the menu is a HUD menu at all, so a factor that returns to 1.0 restores
           the authored geometry rather than leaving the last factor in place. */
        bool      bHudScale = cont && cont->isHudScaleSet() && !cont->isHudFullScreen();

        /* The scope wraps the whole item list so a child's own ChangeResolution call, which this
           loop issues after the container's, resolves the same space as the container. */
        if (bHudScale) {
            UIWidget::HudScalePush(hudScale);
        }

        n = menu->m_itemlist.NumObjects();
        for (j = 1; j <= n; j++) {
            UIWidget *wid = menu->m_itemlist.ObjectAt(j);

            if (wid) {
                wid->ChangeResolution();
            }
        }

        if (bHudScale) {
            UIWidget::HudScalePop();
        }
    }
}

void MenuManager::AddMenu(Menu *m)
{
    m_menulist.AddObject(m);
}

void MenuManager::DeleteMenu(Menu *m)
{
    m_menulist.RemoveObject(m);
}

void MenuManager::DeleteAllMenus(void)
{
    for (int i = m_menulist.NumObjects(); i > 0; i--) {
        Menu *menu = m_menulist.ObjectAt(i);
        delete menu;
    }

    m_menulist.FreeObjectList();

    m_menustack.Clear();
    m_showmenustack.Clear();
}

Menu *MenuManager::FindMenu(str name)
{
    int i;
    int count;

    count = m_menulist.NumObjects();

    for (i = 1; i <= count; i++) {
        Menu *m = m_menulist.ObjectAt(i);

        if (!str::icmp(m->m_name, name)) {
            return m;
        }
    }

    return NULL;
}

#ifdef __ANDROID__
/* Composition contract: this runs AFTER MENU3DVideoMenuResolutionFixup at every
   open, and it must re-clear what that one added -- the r_mode SwitchBox takes
   the aspect rows and android_aspect here, so the resolution rows only exist on
   the desktop.  It is also the only owner of this row's visibility: the
   load-time adapt pass must not disable it (see s_adaptRowsAndroid), because a
   disabled widget is skipped by Menu::ShowMenu and the aspect options would
   never be reachable. */
static void MENU3DAndroidVideoMenuFixup(Menu *menu)
{
    UIWidget *menu3dcont;
    UIWidget *menu3dw;

    if (!menu || Q_stricmp(menu->m_name.c_str(), "video")) {
        return;
    }

    menu3dcont = menu->GetContainerWidget();
    if (!menu3dcont) {
        return;
    }

    menu3dw = menu3dcont->getFirstChild();

    while (menu3dw) {
        const char *menu3dcvar  = menu3dw->getCvarName();
        const char *menu3dtitle = menu3dw->getTitle();
        const char *menu3dname  = menu3dw->getName();

        if (menu3dcvar && menu3dcvar[0] && !Q_stricmp(menu3dcvar, "r_mode")
            && (!menu3dtitle || strcmp(menu3dtitle, "$$Hidden$$"))) {
            static const char *menu3dlabels[3] = { "Fill", "16:9", "4:3" };
            static const char *menu3dcmds[3]   = {
                "set android_aspect 0", "set android_aspect 1", "set android_aspect 2"
            };
            Event *menu3dlink;
            int    menu3di;

            menu3dw->ProcessEvent(new Event("clearoptions"));

            menu3dlink = new Event(EV_Layout_LinkCvar);
            menu3dlink->AddString("android_aspect");
            menu3dw->ProcessEvent(menu3dlink);

            for (menu3di = 0; menu3di < 3; menu3di++) {
                Event *menu3dev = new Event("option");

                menu3dev->AddString(menu3dlabels[menu3di]);
                menu3dev->AddString(menu3dcmds[menu3di]);
                menu3dev->AddString(va("%d", menu3di));
                menu3dw->ProcessEvent(menu3dev);
            }
        } else if (menu3dtitle && !strcmp(menu3dtitle, "$$VideoMode$$")) {
            menu3dw->setTitle("Aspect");     
        } else if (menu3dname && !Q_stricmp(menu3dname, "reslist")) {
            menu3dw->ProcessEvent(new Event(EV_Widget_Hide));   
        }

        menu3dw = menu3dw->getNextSibling();
    }
}

/* The two antialiasing checkboxes (r_arb_multisample, r_ext_nv_multisample_filter)
 * are registered and read nowhere, so they change neither picture nor cost. */
static void MENU3DAndroidAdvancedVideoMenuFixup(Menu *menu)
{
    UIWidget *advcont;
    UIWidget *advw;

    if (!menu || Q_stricmp(menu->m_name.c_str(), "advancedvideo")) {
        return;
    }

    advcont = menu->GetContainerWidget();
    if (!advcont) {
        return;
    }

    advw = advcont->getFirstChild();

    while (advw) {
        const char *advcvar = advw->getCvarName();

        if (advcvar && advcvar[0]
            && (!Q_stricmp(advcvar, "r_arb_multisample")
                || !Q_stricmp(advcvar, "r_ext_nv_multisample_filter"))) {
            advw->ProcessEvent(new Event(EV_Widget_Hide));
        }

        advw = advw->getNextSibling();
    }
}
#endif

#ifdef __ANDROID__
static const char *const audioRetailOnlyAudioCvars[] = {
    "s_milesdriver",
    "s_reverb",
    NULL
};

static qboolean AUDIORetailOnlyAudioCvar(const char *name)
{
    int i;

    if (!name || !name[0]) {
        return qfalse;
    }

    for (i = 0; audioRetailOnlyAudioCvars[i]; i++) {
        if (!Q_stricmp(name, audioRetailOnlyAudioCvars[i])) {
            return qtrue;
        }
    }

    return qfalse;
}

static void AUDIOAudioMenuFixup(Menu *menu)
{
    UIWidget *audiocont;
    UIWidget *audiow;

    if (!menu || Q_stricmp(menu->m_name.c_str(), "audio")) {
        return;
    }

    audiocont = menu->GetContainerWidget();
    if (!audiocont) {
        return;
    }

    audiow = audiocont->getFirstChild();

    while (audiow) {
        const char *audiocvar  = audiow->getCvarName();
        const char *audiotitle = audiow->getTitle();
        const char *audioname  = audiow->getName();

        if (AUDIORetailOnlyAudioCvar(audiocvar)) {
            audiow->ProcessEvent(new Event(EV_Widget_Hide));
        } else if (audiotitle && !strcmp(audiotitle, "$$SoundDriver$$")) {
            audiow->ProcessEvent(new Event(EV_Widget_Hide));
        } else if (audioname && !Q_stricmp(audioname, "sounddriver")) {
            audiow->ProcessEvent(new Event(EV_Widget_Hide));
        }

        audiow = audiow->getNextSibling();
    }
}
#endif


/* The shipped video page carries only the 2003 4:3 rows: menu_video.urc bakes
   six SwitchBox options and six reslist addpopups, all "set r_mode N".  The
   operator asked for the standard 16:9 rows interleaved by width, and both
   rows are rebuilt here from this table at menu open -- the shipped data in
   the pak stays untouched and every label and command keeps its shape.  The
   indices are the renderer's r_vidModes (renderervulkan/tr_init.c): 0-11 are
   retail's, 12-14 are this port's widescreen additions; keep the two in
   step. */
typedef struct {
    int         mode;
    const char *label;
} vidMenuRow_t;

static const vidMenuRow_t s_videoModeRows[] = {
    { 3,  "640 x 480" },
    { 4,  "800 x 600" },
    { 6,  "1024 x 768" },
    { 7,  "1152 x 864" },
    { 12, "1280 x 720 (wide)" },
    { 8,  "1280 x 1024" },
    { 13, "1600 x 900 (wide)" },
    { 9,  "1600 x 1200" },
    { 14, "1920 x 1080 (wide)" },
    { 10, "2048 x 1536" },
};

static void MENU3DVideoMenuResolutionFixup(Menu *m)
{
    UIWidget *menu3dcont;
    UIWidget *menu3dw;

    if (!m || Q_stricmp(m->m_name.c_str(), "video")) {
        return;
    }

    menu3dcont = m->GetContainerWidget();
    if (!menu3dcont) {
        return;
    }

    menu3dw = menu3dcont->getFirstChild();

    while (menu3dw) {
        const char *menu3dcvar  = menu3dw->getCvarName();
        const char *menu3dtitle = menu3dw->getTitle();
        const char *menu3dname  = menu3dw->getName();
        int         i;

        /* the page also carries a hidden r_mode label; only the display and
           the pulldown take rows */
        if (menu3dcvar && menu3dcvar[0] && !Q_stricmp(menu3dcvar, "r_mode")
            && (!menu3dtitle || strcmp(menu3dtitle, "$$Hidden$$"))) {
            menu3dw->ProcessEvent(new Event("clearoptions"));

            for (i = 0; i < ( int )( sizeof( s_videoModeRows ) / sizeof( s_videoModeRows[0] ) ); i++) {
                Event *option = new Event("option");

                option->AddString(s_videoModeRows[i].label);
                option->AddString(va("set r_mode %d", s_videoModeRows[i].mode));
                option->AddString(va("%d", s_videoModeRows[i].mode));
                menu3dw->ProcessEvent(option);
            }
        } else if (menu3dname && !Q_stricmp(menu3dname, "reslist")) {
            menu3dw->ProcessEvent(new Event("clearpopups"));

            for (i = 0; i < ( int )( sizeof( s_videoModeRows ) / sizeof( s_videoModeRows[0] ) ); i++) {
                Event *popup = new Event("addpopup");

                popup->AddString("MENU");
                popup->AddString(s_videoModeRows[i].label);
                popup->AddString("command");
                popup->AddString(va("set r_mode %d", s_videoModeRows[i].mode));
                menu3dw->ProcessEvent(popup);
            }
        }

        menu3dw = menu3dw->getNextSibling();
    }
}

/* The main menu's lower-left version corner.  menu_main.urc ships the corner as
   a pair of serpentinelight-10 labels -- the literal "EF2" at 20,455 and the
   `shortversion' label beside it at 40,455 -- and this port's shortversion is
   its own number (qcommon/common.c registers it CVAR_ROM from PRODUCT_VERSION),
   so retail's text is replaced in place rather than stacked beside: the pair is
   collapsed onto the first label, which is re-titled to the same string the
   startup banner prints, and the second is disabled.  Nothing is created; the
   widget's own font, colours and border survive.  The frame must move AND
   re-justify: widgets draw centered in their own frame by default
   (uiwidget.cpp), the label clips to its own frame (uiwidget.cpp Rend_Scissor),
   and the banner string is wider than the 25 the file authored for "EF2" -- a
   centered draw in a narrow frame loses both ends at the scissor.  Left
   justification keeps the authored left edge as the anchor and the widened
   frame just carries the string.  menu_main.urc keeps its bytes. */
static void MENUVersionCornerFixup(Menu *m)
{
    UIWidget *maincont;
    UIWidget *mainw;

    if (!m || Q_stricmp(m->m_name.c_str(), "main")) {
        return;
    }

    maincont = m->GetContainerWidget();
    if (!maincont) {
        return;
    }

    mainw = maincont->getFirstChild();

    while (mainw) {
        const char *maincvar  = mainw->getCvarName();
        const char *maintitle = mainw->getTitle();

        if (maincvar && maincvar[0] && !Q_stricmp(maincvar, "shortversion")) {
            /* Its draw reads the cvar ahead of the title (uilabel.cpp tries
               m_labelText, then m_cvarname, then m_title), so a blank title
               cannot silence it; the established hide is the only thing that
               can. */
            mainw->Disable();
        } else if (maintitle && !strcmp(maintitle, "EF2")) {
            Event *rect = new Event(EV_Layout_Rect);

            mainw->setTitle(str("P:EF2 ") + PRODUCT_VERSION_FULL);
            mainw->setFontHorizontalAlignment(FONT_JUSTHORZ_LEFT);

            /* the authored `rect' path, and with the FIFTH argument: a 4-arg
               rect moves the live frame but leaves m_authoredFrame holding the
               urc's 25-wide "EF2" frame, and every menu re-derive runs
               ChangeResolution, which re-derives the frame FROM
               m_authoredFrame (and overwrites m_size/m_startingpos with the
               result) -- the widened width died on the next menu open.  The
               fifth arg makes LayoutRect drop the latch and re-derive, so the
               banner frame becomes the authored frame and survives every later
               re-derive.  320 holds the longest banner string with room to
               spare; the draw is left-justified, so the extra width stays
               empty rather than shifting the text. */
            rect->AddFloat(20.0f);
            rect->AddFloat(455.0f);
            rect->AddFloat(320.0f);
            rect->AddFloat(16.0f);
            rect->AddFloat(1.0f);
            mainw->ProcessEvent(rect);
        }

        mainw = mainw->getNextSibling();
    }
}


bool MenuManager::PushMenu(str name)
{
    Menu *m;
    float maxouttime = 0.0;
    float maxintime;
    Menu *head;

    if (m_lock) {
        return false;
    }

    m = FindMenu(name);
    if (!m) {
        uii.Sys_Printf("Couldn't find menu %s\n", name.c_str());
        return false;
    }

    head = m_menustack.Head();
    if (head == m) {
        return false;
    }

    MENU3DVideoMenuResolutionFixup(m);
    MENUVersionCornerFixup(m);

#ifdef __ANDROID__
    AUDIOAudioMenuFixup(m);
    MENU3DAndroidVideoMenuFixup(m);
    MENU3DAndroidAdvancedVideoMenuFixup(m);
#endif

    m->SaveCVars();

    if (head) {
        maxouttime = head->GetMaxMotionTime();
        head->ProcessEvent(Event("hidemenu"));
    }

    if (maxouttime == 0.0) {
        m->ProcessEvent(Event("showmenu"));
    } else {
        m->PostEvent(Event("showmenu"), maxouttime);
    }

    maxintime = m->GetMaxMotionTime();

    Lock(NULL);
    PostEvent(EV_UnlockMenus, maxintime + maxouttime);

    m->NotifyPushMenu();

    m_menustack.Push(m);

    return true;
}

bool MenuManager::TransitionMenu(str name)
{
    Menu *m;
    Menu *head;
    float maxouttime = 0.0;
    float maxintime;

    if (m_lock) {
        return false;
    }

    m = FindMenu(name);
    if (!m) {
        uii.Sys_Printf("Couldn't find menu %s\n", name.c_str());
        return false;
    }

    head = m_menustack.Head();
    if (head == m) {
        return false;
    }

    if (head) {
        m_menustack.Pop();
        maxouttime = head->GetMaxMotionTime();
        head->ProcessEvent(Event("hidemenu"));
    }

    MENU3DVideoMenuResolutionFixup(m);
    MENUVersionCornerFixup(m);

#ifdef __ANDROID__
    AUDIOAudioMenuFixup(m);
    MENU3DAndroidVideoMenuFixup(m);
    MENU3DAndroidAdvancedVideoMenuFixup(m);
#endif

    m->SaveCVars();

    if (maxouttime == 0.0) {
        m->ProcessEvent(Event("showmenu"));
    } else {
        m->PostEvent(Event("showmenu"), maxouttime);
    }

    maxintime = m->GetMaxMotionTime();

    Lock(NULL);
    PostEvent(EV_UnlockMenus, maxintime + maxouttime);

    m->NotifyPushMenu();

    m_menustack.Push(m);

    return true;
}

bool MenuManager::ShowMenu(str name)
{
    Menu *m;
    float maxintime;

    PAINT_PROBE("menu:show", "name='%s' lock=%d", name.c_str(), m_lock ? 1 : 0);

    if (m_lock) {
        return false;
    }

    m = FindMenu(name);
    if (!m) {
        uii.Sys_Printf("Couldn't find menu %s\n", name.c_str());
        PAINT_PROBE("menu:show", "MISS name='%s'", name.c_str());
        return false;
    }

    MENU3DVideoMenuResolutionFixup(m);
    MENUVersionCornerFixup(m);

#ifdef __ANDROID__
    AUDIOAudioMenuFixup(m);
    MENU3DAndroidVideoMenuFixup(m);
    MENU3DAndroidAdvancedVideoMenuFixup(m);
#endif

    m->SaveCVars();
    m->ProcessEvent(new Event("showmenu"));

    {
        int painti;
        int paintn = m->GetWidgetCount();

        PAINT_PROBE("menu:widgets", "menu='%s' widgets=%d", name.c_str(), paintn);
        for (painti = 1; painti <= paintn; painti++) {
            m->GetWidget(painti)->PAINTDumpState(painti, name.c_str());
        }
    }

    maxintime = m->GetMaxMotionTime();

    Lock(NULL);
    PostEvent(EV_UnlockMenus, maxintime);

    m->NotifyPushMenu();

    m_showmenustack.Push(m);

    return true;
}

bool MenuManager::HideMenu(str name)
{
    Menu *m;

    if (m_lock) {
        return false;
    }

    m = FindMenu(name);
    if (!m) {
        uii.Sys_Printf("Couldn't find menu %s\n", name.c_str());
        return false;
    }

    m->ForceHide();

    return true;
}

void MenuManager::HideAllMenus(void)
{
    int i;

    for (i = 1; i <= m_menulist.NumObjects(); i++) {
        m_menulist.ObjectAt(i)->ForceHide();
    }

    m_showmenustack.Clear();
    m_menustack.Clear();
}

void MenuManager::PushMenu(Event *ev)
{
    if (!ui_pLoadingMenu || CurrentMenu() != ui_pLoadingMenu) {
        PushMenu(ev->GetString(1));
    }
}

void MenuManager::PopMenu(qboolean restore_cvars)
{
    Menu *top;
    Menu *head;
    float maxtime = 0.0;

    if (m_lock) {
        return;
    }

    top = m_showmenustack.Pop();
    if (top) {
        if (restore_cvars) {
            top->RestoreCVars();
        }

        top->NotifyPopMenu();

        top->ProcessEvent(Event("hidemenu"));
    } else {
        top = m_menustack.Pop();
        if (top) {
            if (restore_cvars) {
                top->RestoreCVars();
            }

            maxtime = top->GetMaxMotionTime();

            top->NotifyPopMenu();
            top->ProcessEvent(Event("hidemenu"));

            head = m_menustack.Head();
            if (head) {
                head->PostEvent(Event("showmenu"), maxtime);
            }
        }

        Lock(NULL);
        PostEvent(EV_UnlockMenus, maxtime);
    }
}

Menu *MenuManager::CurrentMenu(void)
{
    Menu *head;

    head = m_showmenustack.Head();
    if (head) {
        return head;
    }

    head = m_menustack.Head();
    if (head) {
        return head;
    }

    return NULL;
}

bool MenuManager::ClearMenus(bool force)
{
    Menu *top;
    Menu *menu;
    float maxtime;
    float newmaxtime;

    maxtime = 0.0;
    if (m_lock) {
        if (!force) {
            return false;
        }
        m_lock = false;
    }

    // ⚠️ Two lesser divergences deliberately left alone here, for the UI row: retail calls
    if (m_showmenustack.Head()) {
        top = m_showmenustack.Head();
        while (!m_showmenustack.Empty()) {
            menu       = m_showmenustack.Pop();
            newmaxtime = top->GetMaxMotionTime();
            if (maxtime < newmaxtime) {
                maxtime = newmaxtime;
            }

            if (force) {
                menu->ForceHide();
                continue;
            }

            menu->ProcessEvent(new Event("hidemenu"));
        }
    }

    if (!m_menustack.Head()) {
        return true;
    }

    top = m_menustack.Head();
    while (!m_menustack.Empty()) {
        menu       = m_menustack.Pop();
        newmaxtime = top->GetMaxMotionTime();
        if (maxtime < newmaxtime) {
            maxtime = newmaxtime;
        }

        if (force) {
            menu->ForceHide();
            continue;
        }

        menu->ProcessEvent(new Event("hidemenu"));
    }


    if (!force) {
        Lock(NULL);
        PostEvent(EV_UnlockMenus, maxtime);
    }

    return true;
}

void MenuManager::ListMenus(void)
{
    int i;
    int num;

    num = m_menulist.NumObjects();
    for (i = 1; i <= num; i++) {
        Menu *menu = m_menulist.ObjectAt(i);
        uii.Sys_Printf("%s\n", menu->m_name.c_str());
    }
}

void MenuManager::reloadStrings(void)
{
    int i, j;

    for (i = 1; i <= m_menulist.NumObjects(); i++) {
        Menu *menu = m_menulist.ObjectAt(i);
        if (!menu) {
            continue;
        }

        for (j = 1; j <= menu->m_itemlist.NumObjects(); j++) {
            UIWidget *widget = menu->m_itemlist.ObjectAt(j);
            if (widget) {
                widget->reloadStrings();
            }
        }
    }
}

void MenuManager::UpdateAllMenus(void)
{
    int i;
    int num;

    UI_ROOT_BEGIN(UI_FAM_WALK);

    num = m_menulist.NumObjects();
    UI_COUNT(UI_FAM_WALK, UI_WC_menus, num);

    for (i = 1; i <= num; i++) {
        Menu     *menu = m_menulist.ObjectAt(i);
        UIWidget *cont = menu ? menu->GetContainerWidget() : NULL;
        int       ui_vis = ( cont && cont->IsVisible() ) ? 1 : 0;
        int       ui_t   = ( ui_statsOn ? UI_Microseconds() : 0 );

        menu->Update();

        if ( ui_statsOn ) {
            UI_Add( UI_FAM_WALK, ui_vis ? UI_WZ_vis : UI_WZ_hid,
                      UI_Microseconds() - ui_t );
            UI_Bump( UI_FAM_WALK, ui_vis ? UI_WC_visw : UI_WC_hidw, 1 );
        }
    }

    UI_ROOT_END(UI_FAM_WALK);
}

void MenuManager::Lock(Event *ev)
{
    m_lock = true;
}

void MenuManager::Unlock(Event *ev)
{
    m_lock = false;
}

void MenuManager::PassEventToWidget(str name, Event *ev)
{
    Menu *head = CurrentMenu();

    if (head) {
        head->PassEventToWidget(name, ev);
    } else {
        delete ev;
    }
}

void MenuManager::PassEventToAllWidgets(Event& ev)
{
    int i;
    int iMenuCount;

    iMenuCount = m_menulist.NumObjects();
    for (i = 1; i <= iMenuCount; i++) {
        Menu *pMenu = m_menulist.ObjectAt(i);
        pMenu->PassEventToAllWidgets(ev);
    }
}

void MenuManager::ResetCVars(void)
{
    Menu *head = CurrentMenu();

    if (head) {
        head->ResetCVars();
    }
}

void MenuManager::CheckRestart(void)
{
    Menu *head = CurrentMenu();

    if (head) {
        head->CheckRestart();
    }
}

bool MenuManager::ForceMenu(str name)
{
    Menu *m;
    Menu *head;

    m = FindMenu(name);
    if (!m) {
        uii.Sys_Printf("Couldn't find menu %s\n", name.c_str());
        return false;
    }

    head = m_menustack.Head();
    if (head == m) {
        return false;
    }

    m->SaveCVars();

    if (head) {
        head->ForceHide();
    }

    m->ProcessEvent(Event("showmenu"));
    m_menustack.Push(m);

    return true;
}

// The handheld adaptation of the shipped settings pages: a row leaves its page
// through the same `disable' the shipped data issues itself (menu_controls.urc
// showcommands), and is named by what its own file already carries.

struct adaptRow_s {
    const char *menu;    // the shipped `menu' name
    const char *token;   // the row label's string token
    const char *cvar;    // the row control's linked cvar
    const char *name;    // the row's pulldown, where the file names it
    const char *shader;  // a widget the file leaves unnamed carries
};

static adaptRow_s s_adaptRows[] = {
    {"advancedvideo", "$$FullScreenAntialiasing$$", "r_arb_multisample",           NULL,          NULL              },
    {"advancedvideo", "$$NvidiaAntiAliasing$$",     "r_ext_nv_multisample_filter", NULL,          NULL              },
    {"video",         "$$ColorDepth$$",             "r_texturebits",               "colorlist",   NULL              },
    {"controls",      NULL,                         NULL,                          NULL,          "menu/mp/bar-fix" },
};

/* These rows stay hidden on BOTH targets because the option does not exist in
   this lane, not because the menu is Android's: `r_texturebits` is registered
   (renderervulkan/tr_init.c:173) and never read again, and
   `r_arb_multisample` / `r_ext_nv_multisample_filter` are not referenced there
   at all -- every pipeline it builds is VK_SAMPLE_COUNT_1_BIT (vk_draw.c:293,
   vk_pipeline.c:228, vk_shade.c:2369).  The controls row hides retail's bind
   scrollbar, which this port's own bind tables replace.  A shown-but-dead
   option is worse than a hidden one. */

#ifdef __ANDROID__
/* Options the port DOES service, hidden only because the Android build's menus
   were trimmed for touch.  The Linux build shows retail's rows for them.

   The video page's r_mode row is NOT in this table: MENU3DAndroidVideoMenuFixup
   owns that row and turns it into the android_aspect selector, so disabling it
   here (UIWidget::Disable hides) hides the very widget the fixup repurposes --
   Menu::ShowMenu then skips the disabled widget on every open and the aspect
   options never appear.  Only the resolution pulldown is named here, because
   ShowMenu re-shows an enabled widget and would cancel the fixup's own per-open
   hide of it. */
static adaptRow_s s_adaptRowsAndroid[] = {
    {"video",         NULL,                         NULL,                          "reslist",     NULL              },
    {"video",         "$$FullScreen$$",             "r_fullscreen",                NULL,          NULL              },
    {"audio",         "$$SoundDriver$$",            "s_milesdriver",               "sounddriver", NULL              },
    {"GameOptions",   "$$DataRate$$",               "rate",                        NULL,          NULL              },
    {"GameOptions",   "$$LowBandwidth$$",           "mp_lowBandwidth",             NULL,          NULL              },
};
#endif

/* The controls page's keyboard apparatus: the touch build replaces it with the
   touch section, and Linux carries retail's page unmodified. */
#ifdef __ANDROID__
static const char *s_adaptBindWidgets[] = {
    "WeaponBindList", "AttackLookBindList", "MovementBindList", "GameplayBindList",
    "WeaponBind",     "AttackLookBind",     "MovementBind",     "GameplayBind",
    "PressKey",
    /* the bind table's own apparatus: header bar, command column, the two bind
       buttons and the scrollbar -- templates the shipped Table names by string */
    "bindHeader",     "bindcommand",        "bindbutton",       "bindbuttonalt",
    "bindscroll",
};
#endif /* __ANDROID__ */

static void AdaptHideRow(UIWidget *container, const adaptRow_s *row) {
    Container<UIWidget *>& children = container->getChildren();
    str                    title;
    int                    i;

    if (row->token && isStringResource(row->token)) {
        title = getStringResource(row->token);
    }

    for (i = 1; i <= children.NumObjects(); i++) {
        UIWidget   *wid  = children.ObjectAt(i);
        const char *cvar = wid->getCvarName();
        const char *mat  = wid->getMaterialName();

        if ((row->name && !strcmp(wid->getName(), row->name))
            || (row->cvar && *cvar && !stricmp(cvar, row->cvar))
            || (row->shader && mat && !stricmp(mat, row->shader))
            || (title.length() && !strcmp(wid->getTitle(), title.c_str()))) {
            wid->Disable();   // only enabled widgets are ever re-shown
        }
    }
}

#ifdef __ANDROID__
static void AdaptHideNamed(UIWidget *container, const char *const *names, int count)
{
    Container<UIWidget *>& children = container->getChildren();
    int                    i;
    int                    j;

    for (i = 1; i <= children.NumObjects(); i++) {
        UIWidget *wid = children.ObjectAt(i);

        for (j = 0; j < count; j++) {
            if (!strcmp(wid->getName(), names[j])) {
                wid->Disable();
                break;
            }
        }
    }
}
#endif /* __ANDROID__ */

/* Widget builders the touch section owns.  Its gate used to be the presence of
   `cl_touchLook', and that cvar reached Linux because the launcher staged the
   retail install's loose base/global/default.cfg, which shadowed the port's own
   copy inside zpak-android.pk3, so the Linux settings menus grew a whole Touch
   Controls section.  Neither the loose copy nor the boot cfg exec exists any
   more (the defaults ship compiled in ef2_android_defaults.h), and gating the
   section and its helpers themselves means a stray cvar or config can never
   re-inject it, which the cvar test could not promise. */
#ifdef __ANDROID__
static UIWidget *AdaptWidget(
    UIWidget *parent, const char *resource, const char *name, float x, float y, float w, float h
)
{
    const char *translated = uii.Client_TranslateWidgetName(resource);
    str         classname  = "UI";
    UIWidget   *wid;

    classname += translated ? translated : resource;

    ClassDef *widgetclass = getClass(classname.c_str());
    if (!widgetclass) {
        return NULL;
    }

    wid = (UIWidget *)widgetclass->newInstance();
    if (!wid) {
        return NULL;
    }

    wid->InitFrame(parent, x, y, w, h, -1, "serpentinemedium-10");
    wid->setName(str(name));

    return wid;
}

static void AdaptCommand(UIWidget *wid, const char *command, ...)
{
    Event      *ev;
    va_list     args;
    const char *arg;

    if (!wid || !wid->ValidEvent(command)) {
        return;
    }

    ev = new Event(command);

    va_start(args, command);
    while ((arg = va_arg(args, const char *)) != NULL) {
        ev->AddToken(arg);
    }
    va_end(args);

    wid->ProcessEvent(ev);
}

/* The touch section lives only in the Android build.  Gating the section and
   its helpers themselves, rather than the cvar presence, means a stray config
   can never re-inject it. */
static void AdaptTouchTooltip(UIWidget *wid, const char *tooltip)
{
    AdaptCommand(wid, "hovercommand", va("globalwidgetcommand tooltip_controls title \"%s\"", tooltip), NULL);
    AdaptCommand(wid, "hoverexitcommand", "globalwidgetcommand tooltip_controls title \"\"", NULL);
}

static void AdaptTouchSlider(
    UIWidget *parent, const char *name, float y, const char *label, const char *cvar,
    const char *lo, const char *hi, const char *step, const char *tooltip
)
{
    UIWidget *text = AdaptWidget(parent, "Label", va("%sLabel", name), 50, y, 150, 20);
    UIWidget *bar  = AdaptWidget(parent, "Slider", name, 204, y, 150, 20);

    AdaptCommand(text, "title", label, NULL);
    AdaptCommand(text, "fgcolor", "1.00", "1.00", "1.00", "1.00", NULL);
    AdaptCommand(text, "bgcolor", "0.122", "0.157", "0.251", "1.000", NULL);
    AdaptCommand(text, "borderstyle", "3D_BORDER", NULL);
    AdaptCommand(text, "horizontaljustify", "right", NULL);
    AdaptTouchTooltip(text, tooltip);

    AdaptCommand(bar, "fgcolor", "0.953", "0.667", "0.271", "1.000", NULL);
    AdaptCommand(bar, "bgcolor", "0.000", "0.000", "0.000", "0.750", NULL);
    AdaptCommand(bar, "borderstyle", "OUTLINE_BORDER", NULL);
    AdaptCommand(bar, "bordercolor", "0.122", "0.157", "0.251", "1.000", NULL);
    AdaptCommand(bar, "linkcvar", cvar, NULL);
    AdaptCommand(bar, "setrange", lo, hi, NULL);
    AdaptCommand(bar, "stepsize", step, NULL);
    AdaptCommand(bar, "slidertype", "float", NULL);
    AdaptTouchTooltip(bar, tooltip);
    AdaptCommand(bar, "clicksound", "sound/misc/menu/click.wav", NULL);
}

static void AdaptTouchCheckbox(
    UIWidget *parent, const char *name, float y, const char *label, const char *cvar, const char *tooltip
)
{
    UIWidget *box = AdaptWidget(parent, "CheckBox", name, 50, y, 250, 20);

    AdaptCommand(box, "title", label, NULL);
    AdaptCommand(box, "fgcolor", "1.00", "1.00", "1.00", "1.00", NULL);
    AdaptCommand(box, "bgcolor", "0.50", "0.50", "0.50", "0.00", NULL);
    AdaptCommand(box, "shadercolor", "0.953", "0.667", "0.271", "1.000", NULL);
    AdaptCommand(box, "hovershadercolor", "0.953", "0.667", "0.271", "1.000", NULL);
    AdaptCommand(box, "armedshadercolor", "0.953", "0.667", "0.271", "1.000", NULL);
    AdaptCommand(box, "selectedshadercolor", "0.953", "0.667", "0.271", "1.000", NULL);
    AdaptCommand(box, "borderstyle", "NONE", NULL);
    AdaptCommand(box, "linkcvar", cvar, NULL);
    AdaptCommand(box, "checked_shader", "textures/menu/checkbox_checked", NULL);
    AdaptCommand(box, "unchecked_shader", "textures/menu/checkbox_unchecked", NULL);
    AdaptCommand(box, "hovershader", "menu/generic/button-hover", NULL);
    AdaptCommand(box, "armedshader", "menu/generic/button-press", NULL);
    AdaptCommand(box, "horizontaljustify", "left", NULL);
    AdaptTouchTooltip(box, tooltip);
    AdaptCommand(box, "clicksound", "sound/misc/menu/click.wav", NULL);
}

static void AdaptTouchSection(UIWidget *container)
{
    UIWidget *header;
    UIWidget *box;

    if (container->FindWidget("TouchLook")    // built once; a reload re-runs the pass
        || !UI_FindCvar("cl_touchLook")) {    // the touch layer owns these cvars
        return;
    }

    header = AdaptWidget(container, "Label", "TouchSectionTitle", 46, 87, 325, 16);
    AdaptCommand(header, "title", "Touch Controls", NULL);
    AdaptCommand(header, "fgcolor", "1.000", "1.000", "1.000", "1.000", NULL);
    AdaptCommand(header, "bgcolor", "0.278", "0.365", "0.510", "1.000", NULL);
    AdaptCommand(header, "borderstyle", "3D_BORDER", NULL);

    box = AdaptWidget(container, "Widget", "TouchSectionBox", 46, 107, 325, 84);
    AdaptCommand(box, "fgcolor", "1", "1", "1", "1", NULL);
    AdaptCommand(box, "bgcolor", "0.000", "0.000", "0.000", "0.750", NULL);
    AdaptCommand(box, "borderstyle", "OUTLINE_BORDER", NULL);
    AdaptCommand(box, "borderwidth", "1", NULL);
    AdaptCommand(box, "bordercolor", "0.278", "0.365", "0.510", "1.000", NULL);

    AdaptTouchSlider(container, "TouchLook", 115, "Touch Look Sensitivity", "cl_touchLook",
                     "0.5", "6.0", "0.1", "Look sensitivity on the touch pad");
    /* the deadzone, not the overlay: cl_touchScale and cl_touchOpacity only size
       and fade the onscreen buttons (sdl_input.c IN_TouchScale, cl_scrn.cpp's
       draw scale/opacity), so their rows went.  joy_threshold is the axis
       deadzone itself (sdl_input.c zeroes the axis below it) and its compiled
       default 0.15 is the EF1 answer key's own value; the range starts at EF1's
       own 0.05 floor (ui_controls2.c) and puts that default a third of the way
       up. */
    AdaptTouchSlider(container, "JoystickDeadzone", 139, "Joystick Deadzone", "joy_threshold",
                     "0.05", "0.35", "0.01", "How far the stick must move before it responds");
    AdaptTouchCheckbox(container, "TouchCrouchToggle", 163, "Crouch Toggle", "cl_crouchToggle",
                       "Tap crouch to hold it down");
}
#endif /* __ANDROID__ */

/* Both of these serve rows and prompts the Android menus trim; the Linux build
   keeps retail's own rows and its shipped promptadvancedvideo prompt menu, so
   they are built only where they are used. */
#ifdef __ANDROID__
static void AdaptHideAt(UIWidget *container, float x, float y, float w, float h)
{
    Container<UIWidget *>& children = container->getChildren();
    int                    i;

    for (i = 1; i <= children.NumObjects(); i++) {
        UIWidget *wid   = children.ObjectAt(i);
        UIRect2D  frame = wid->getFrame();

        if (!wid->isSubclassOf(UIPulldownMenuContainer)) {
            continue;
        }

        if (fabsf(frame.pos.x - x) < 0.5f && fabsf(frame.pos.y - y) < 0.5f
            && fabsf(frame.size.width - w) < 0.5f && fabsf(frame.size.height - h) < 0.5f) {
            wid->Disable();   // frames are still authored here, before ChangeResolution
        }
    }
}

static void AdaptRepointCommand(UIWidget *container, const char *from, const char *to)
{
    Container<UIWidget *>& children = container->getChildren();
    int                    i;

    for (i = 1; i <= children.NumObjects(); i++) {
        UIWidget *wid = children.ObjectAt(i);
        str       cmd = wid->getCommand();

        if (cmd.length() && strstr(cmd.c_str(), from)) {
            wid->LinkCommand(str(to));   // the press stuffs it with the newline
        }
    }
}
#endif /* __ANDROID__ */

void UI_AdaptSettingsMenus(void)
{
    const int rowCount = (int)(sizeof(s_adaptRows) / sizeof(s_adaptRows[0]));
    int       i;

    for (i = 1; i <= uWinMan.GetChildCount(); i++) {
        UIWidget *container = uWinMan.GetChild(i);
        int       j;

        if (!container || !container->isSubclassOf(UIWidgetContainer)) {
            continue;
        }

        for (j = 0; j < rowCount; j++) {
            if (!stricmp(container->getName(), s_adaptRows[j].menu)) {
                AdaptHideRow(container, &s_adaptRows[j]);
            }
        }

#ifdef __ANDROID__
        for (j = 0; j < (int)(sizeof(s_adaptRowsAndroid) / sizeof(s_adaptRowsAndroid[0])); j++) {
            if (!stricmp(container->getName(), s_adaptRowsAndroid[j].menu)) {
                AdaptHideRow(container, &s_adaptRowsAndroid[j]);
            }
        }

        if (!stricmp(container->getName(), "video")) {
            AdaptRepointCommand(container, "promptadvancedvideo", "transitionmenu advancedvideo 0");
        }

        if (!stricmp(container->getName(), "GameOptions")) {
            AdaptHideAt(container, 586, 231, 16, 16);   // the data-rate pulldown: no name, no cvar, no title
        }
#endif

#ifdef __ANDROID__
        if (!stricmp(container->getName(), "controls")) {
            /* The shipped page carries its own menu-level showcommand, which
               re-enables WeaponBindList every time the page opens
               (menu_controls.urc) -- exactly what defeats the hide below.  The
               touch build owns this page, so its bind-tab commands do not run. */
            container->ClearShowCommands();

            AdaptHideNamed(container, s_adaptBindWidgets,
                           (int)(sizeof(s_adaptBindWidgets) / sizeof(s_adaptBindWidgets[0])));
            AdaptTouchSection(container);
        }
#endif
    }
}
