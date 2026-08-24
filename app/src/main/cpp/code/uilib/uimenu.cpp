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

#define PROBE_GET(probe_n, probe_d, probe_f) ((cvar_t *)0)
#define PROBE_PRINT                       Com_Printf
#define PROBE_ON_OVERRIDE                       (atoi(CvarGetForUI("qa_probe", "1")) != 0)  
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

// PAINT TEMP PROBE -- PORT-ONLY, TEMPORARY, NEVER MERGE.  MenuManager::ShowMenu's
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

// STALE one and re-run an edge retail would not run.  The two reads are now
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
            if (ret) {
                wid->m_cvarvalue = ret;
            } else {
                wid->m_cvarvalue = "";
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

    n = m_itemlist.NumObjects();
    for (i = 1; i <= n; i++) {
        UIWidget *wid = m_itemlist.ObjectAt(i);

        if (wid->m_cvarname.length()) {
            cvar = uii.Cvar_Find(wid->m_cvarname);
            if (cvar && cvar->latchedString) {
                if (str::icmp(wid->m_cvarvalue, cvar->latchedString)) {
                    if (cvar->flags & CVAR_LATCH) {
                        do_restart = true;
                    } else if (cvar->flags & CVAR_SOUND_LATCH) {
                        do_snd_restart = true;
                    } else if (cvar->flags & CVAR_TERRAIN_LATCH) {
                        do_ter_restart = true;
                    }
                }
            }
        }
    }

    if (do_restart) {
#ifdef __ANDROID__
        if (do_snd_restart) {
            uii.Cmd_Stuff("snd_restart\n");
        }

        if (do_ter_restart) {
            uii.Cmd_Stuff("ter_restart\n");
        }
#else
        uii.Cmd_Stuff("vid_restart\n");
#endif
    } else {
        if (do_snd_restart) {
            uii.Cmd_Stuff("snd_restart\n");
        }

        if (do_ter_restart) {
            uii.Cmd_Stuff("ter_restart\n");
        }
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
        bool      bHudScale = hudScale > 1.0f;

        /* Only a HUD menu is laid out with the multiplied virtual space.  The
           scope wraps the whole item list so a child's own ChangeResolution
           call, which this loop issues after the container's, resolves the
           same space as the container. */
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
#endif

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

    AUDIOAudioMenuFixup(m);

#ifdef __ANDROID__
    MENU3DAndroidVideoMenuFixup(m);
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

    AUDIOAudioMenuFixup(m);

#ifdef __ANDROID__
    MENU3DAndroidVideoMenuFixup(m);
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

    AUDIOAudioMenuFixup(m);

#ifdef __ANDROID__
    MENU3DAndroidVideoMenuFixup(m);
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
