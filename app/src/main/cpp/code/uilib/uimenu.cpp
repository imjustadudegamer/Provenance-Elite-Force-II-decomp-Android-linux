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

/* What each latched cvar read when the page was built: the auto-apply restarts the
   renderer only for a row that left it, which is retail's own CheckRestart test. */
typedef struct {
    const char *cvar;
    char        value[32];
} settingsBaseline_s;

#define SETTINGS_BASELINE_MAX 16
static settingsBaseline_s s_settingsBaseline[SETTINGS_BASELINE_MAX];
static int                s_settingsBaselineCount;

static void SettingsBaselineClear(void)
{
    s_settingsBaselineCount = 0;
}

static void SettingsBaselineRecord(const char *cvar)
{
    cvar_t *var;
    int     i;

    if (!cvar || !cvar[0] || s_settingsBaselineCount >= SETTINGS_BASELINE_MAX) {
        return;
    }

    for (i = 0; i < s_settingsBaselineCount; i++) {
        if (!stricmp(s_settingsBaseline[i].cvar, cvar)) {
            return;
        }
    }

    var = uii.Cvar_Find(cvar);
    if (!var) {
        return;
    }

    s_settingsBaseline[s_settingsBaselineCount].cvar = cvar;
    Q_strncpyz(s_settingsBaseline[s_settingsBaselineCount].value,
               var->latchedString ? var->latchedString : var->string,
               sizeof(s_settingsBaseline[0].value));
    s_settingsBaselineCount++;
}

/* The write retail left to the Apply button.  A row calls this on every write it
   makes, so the restart it owes waits for the writes to stop: a slider drags at frame
   rate, and the renderer comes down once, SETTLE_MSEC after the last one. */
#define SET_AUTOAPPLY_SETTLE_MSEC 400

static int s_settingsRestartPending;
static int s_settingsRestartAt;

void UI_SettingsAutoApply(void)
{
    int changed = 0;
    int i;

    for (i = 0; i < s_settingsBaselineCount; i++) {
        settingsBaseline_s *base = &s_settingsBaseline[i];
        cvar_t             *var  = uii.Cvar_Find(base->cvar);
        const char         *now  = var ? (var->latchedString ? var->latchedString
                                                              : var->string) : NULL;

        if (!now || !stricmp(now, base->value)) {
            continue;
        }

        if (!s_settingsRestartPending) {
            UIVK_PROBE("settings", "op=autoapply cvar=%s old=%s new=%s", base->cvar,
                       base->value, now);
        }
        changed++;
    }

    if (changed) {
        s_settingsRestartPending = 1;
        s_settingsRestartAt = Sys_Milliseconds();
    }
}

void UI_SettingsAutoApplyFrame(void)
{
    if (!s_settingsRestartPending) {
        return;
    }

    if (Sys_Milliseconds() - s_settingsRestartAt < SET_AUTOAPPLY_SETTLE_MSEC) {
        return;
    }

    s_settingsRestartPending = 0;
    uii.Cmd_Stuff("vid_restart\n");
    UIVK_PROBE("settings", "op=autoapply restart=vid_restart");
}

#ifdef __ANDROID__
/* composition contract: this runs AFTER MENU3DVideoMenuResolutionFixup at every open and must re-clear what that one added -- the r_mode SwitchBox takes the
   aspect rows and android_aspect here, so the resolution rows only exist on the desktop.  It is also the only owner of this row's visibility: a disabled widget is
   skipped by Menu::ShowMenu and the aspect options would never be reachable. */
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


/* the shipped video page carries only the 2003 4:3 rows (menu_video.urc bakes six SwitchBox options and six reslist addpopups, all "set r_mode N"); both rows
   are rebuilt here from this table at menu open, so the shipped data in the pak stays untouched.  The indices are the renderer's r_vidModes (renderervulkan/tr_init.c):
   0-11 are retail's, 12-14 this port's widescreen additions; keep the two in step. */
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

/* the main menu's lower-left version corner: menu_main.urc ships it as a pair of serpentinelight-10 labels and this port's shortversion is its own number, so retail's
   text is replaced in place -- the pair collapses onto the first label, re-titled to the banner string, the second disabled, and menu_main.urc keeps its bytes.  The frame
   must move AND re-justify: widgets draw centered in their own frame, the label clips to it, and the banner is wider than the 25 authored for "EF2", so the draw is left-justified. */
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

            /* the authored `rect' path, and with the FIFTH argument: a 4-arg rect moves the live frame but leaves m_authoredFrame holding the urc's 25-wide
               "EF2" frame, and every menu re-derive runs ChangeResolution, which re-derives the frame FROM m_authoredFrame -- the widened width died on the next
               menu open.  The fifth arg makes LayoutRect drop the latch; 320 holds the longest banner string, and the draw is left-justified. */
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

#ifndef __ANDROID__
static adaptRow_s s_adaptRows[] = {
    {"advancedvideo", "$$FullScreenAntialiasing$$", "r_arb_multisample",           NULL,          NULL              },
    {"advancedvideo", "$$NvidiaAntiAliasing$$",     "r_ext_nv_multisample_filter", NULL,          NULL              },
    {"controls",      NULL,                         NULL,                          NULL,          "menu/mp/bar-fix" },
};
#endif /* !__ANDROID__ */

/* these rows stay hidden on BOTH targets because the option does not exist in this lane, not because the menu is Android's: `r_texturebits' is registered and
   never read again, and `r_arb_multisample' / `r_ext_nv_multisample_filter' are not referenced at all (every pipeline is VK_SAMPLE_COUNT_1_BIT).  The controls row
   hides retail's bind scrollbar, which this port's own bind tables replace. */

#ifdef __ANDROID__
/* Options the port DOES service, hidden only because the Android build's menus were trimmed for touch; the Linux build shows retail's rows.  The video page is not in
   this table any more: SettingsAdaptVideo owns that page's whole content, including the rows this table used to hide. */
static adaptRow_s s_adaptRowsAndroid[] = {
    {"audio",         "$$SoundDriver$$",            "s_milesdriver",               "sounddriver", NULL              },
    {"GameOptions",   "$$DataRate$$",               "rate",                        NULL,          NULL              },
    {"GameOptions",   "$$LowBandwidth$$",           "mp_lowBandwidth",             NULL,          NULL              },
};
#endif

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

/* Widget builders the rebuilt pages own.  Gating the builders themselves, rather
   than a cvar presence, means a stray config can never re-inject a section. */
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

/* The description bar each page ships at its lower left; the video page's is
   tooltip_video, the controls page's tooltip_controls. */
static const char *s_settingsTooltipBar = "tooltip_controls";

static void AdaptTouchTooltip(UIWidget *wid, const char *tooltip)
{
    AdaptCommand(wid, "hovercommand", va("globalwidgetcommand %s title \"%s\"", s_settingsTooltipBar, tooltip), NULL);
    AdaptCommand(wid, "hoverexitcommand", va("globalwidgetcommand %s title \"\"", s_settingsTooltipBar), NULL);
}

/* The two settings pages the handheld owns, built from one row table each.  A row is
   label + control, and the control is one of the three widget classes the shipped
   data already styles.  A section opens a header plus the panel box behind its rows.

   DESIGN.md carries the kept/deleted lists and the file:line that kills each one.
   Nothing here introduces a cvar: every row drives one the engine already registers,
   so the per-boot launcher override chain cannot re-pin it out from under itself. */

struct settingsRow_s {
    const char *section;        /* opens a header when set */
    const char *label;
    const char *cvar;           /* NULL for a section-only row */
    const char *widget;         /* SwitchBox | CheckBox | Slider | NULL */
    const char *opt[4][3];      /* SwitchBox: label, command, cvar value */
    int         nopt;
    float       lo, hi, step;   /* Slider */
    const char *type;           /* Slider: "float" | "integer" */
    int         latched;        /* CVAR_LATCH: the write schedules the renderer restart */
    const char *tooltip;
};

/* The shipped pages' own LCARS palette (menu_video.urc / menu_advancedvideo.urc). */
#define SET_COL_NAVY    "0.122", "0.157", "0.251"
#define SET_COL_PANEL   "0.278", "0.365", "0.510"
#define SET_COL_ORANGE  "0.953", "0.667", "0.271"

/* One row is label + value, and the value is the same widget frame whichever of the
   three it is, so the checkbox mark, the pulldown value and the slider thumb all start
   on the same x.  The page is two such columns under a full-width section header. */
struct settingsGeom_s {
    const char *prefix;   /* the row widgets' name space; the widget lookup is global */
    float labelx;
    float labelw;
    float valx;
    float valw;
    float boxx;
    float boxw;
    float pitch;          /* band pitch: one label+value pair per band, two per band */
    float rowh;
};

static const settingsGeom_s s_videoCol[2] = {
    {"VidL", 51.0f, 160.0f, 217.0f, 90.0f, 45.0f, 268.0f, 28.0f, 22.0f},
    {"VidR", 325.0f, 160.0f, 491.0f, 90.0f, 319.0f, 268.0f, 28.0f, 22.0f}
};

static const settingsGeom_s s_touchCol = {
    "Touch", 50.0f, 170.0f, 226.0f, 139.0f, 46.0f, 325.0f, 28.0f, 22.0f
};

#define SET_HEAD_H   14.0f
#define SET_HEAD_GAP 4.0f

/* the label column is one width everywhere; the longest label in the page's own font
   sets it, so a translated string can never be cut by a width this table guessed */
#define SET_LABEL_PAD 8.0f
#define SET_LABEL_GAP 6.0f

/* The one row the handheld cannot honour: the Android surface is the whole window, so
   the choice has nothing to change.  The row still ships on both targets -- the page is
   one page -- and the handheld greys it, which is EF1's own shape of that answer
   (ui_video.cpp:410, QMF_GRAYED). */
static const char *const s_videoDesktopRows[] = {
    "r_fullscreen", NULL
};

static qboolean SettingsRowIsGreyed(const char *cvar)
{
    if (!cvar) {
        return qfalse;
    }

#ifdef __ANDROID__
    int i;

    for (i = 0; s_videoDesktopRows[i]; i++) {
        if (!stricmp(cvar, s_videoDesktopRows[i])) {
            return qtrue;
        }
    }
#else
    /* the desktop honours every row it shows */
#endif

    return qfalse;
}

static void SettingsRect(UIWidget *wid, float x, float y, float w, float h)
{
    Event *ev = new Event(EV_Layout_Rect);

    /* the fifth arg drops the authored-frame latch (see MENUVersionCornerFixup) */
    ev->AddFloat(x);
    ev->AddFloat(y);
    ev->AddFloat(w);
    ev->AddFloat(h);
    ev->AddFloat(1.0f);
    wid->ProcessEvent(ev);
}

/* getTitleWidth answers in the unit the widget draws its text in: a virtual widget is
   authored in 640x480 and its font metrics come back multiplied by the display scale,
   while every frame here is authored.  Dividing the metric back is what keeps the label
   column a 640x480 width instead of a screen-pixel one. */
static float SettingsTitleWidth(UIWidget *wid)
{
    float w = wid->getTitleWidth();

    if (w > 0.0f && wid->isResizeable()) {
        w /= uid.scaleRes[0];
    }

    return w;
}

static UIWidget *SettingsHeader(
    UIWidget *container, const char *name, const char *title, float x, float y, float w
)
{
    UIWidget *wid = AdaptWidget(container, "Label", name, x, y, w, SET_HEAD_H);

    AdaptCommand(wid, "title", title, NULL);
    AdaptCommand(wid, "fgcolor", "1.000", "1.000", "1.000", "1.000", NULL);
    AdaptCommand(wid, "bgcolor", SET_COL_PANEL, "1.000", NULL);
    AdaptCommand(wid, "borderstyle", "3D_BORDER", NULL);
    return wid;
}

static UIWidget *SettingsPanel(
    UIWidget *container, const char *name, float x, float y, float w, float h
)
{
    UIWidget *wid = AdaptWidget(container, "Widget", name, x, y, w, h);

    AdaptCommand(wid, "fgcolor", "1", "1", "1", "1", NULL);
    AdaptCommand(wid, "bgcolor", "0.000", "0.000", "0.000", "0.750", NULL);
    AdaptCommand(wid, "borderstyle", "OUTLINE_BORDER", NULL);
    AdaptCommand(wid, "borderwidth", "1", NULL);
    AdaptCommand(wid, "bordercolor", SET_COL_PANEL, "1.000", NULL);
    return wid;
}

static UIWidget *SettingsRowLabel(
    UIWidget *container, const char *name, const char *title, float x, float y, float w, float h
)
{
    UIWidget *wid = AdaptWidget(container, "Label", name, x, y, w, h);

    AdaptCommand(wid, "title", title, NULL);
    AdaptCommand(wid, "fgcolor", "1.00", "1.00", "1.00", "1.00", NULL);
    AdaptCommand(wid, "bgcolor", SET_COL_NAVY, "1.000", NULL);
    AdaptCommand(wid, "borderstyle", "3D_BORDER", NULL);
    AdaptCommand(wid, "horizontaljustify", "right", NULL);
    return wid;
}

static UIWidget *SettingsSwitchBox(
    UIWidget *container, const settingsRow_s *row, const char *name, float x, float y, float w, float h
)
{
    UIWidget *wid = AdaptWidget(container, "SwitchBox", name, x, y, w, h);
    int       i;

    AdaptCommand(wid, "linkcvar", row->cvar, NULL);
    AdaptCommand(wid, "fgcolor", "1.00", "1.00", "1.00", "1.00", NULL);
    AdaptCommand(wid, "bgcolor", "0.000", "0.000", "0.000", "0.750", NULL);
    AdaptCommand(wid, "borderstyle", "OUTLINE_BORDER", NULL);
    AdaptCommand(wid, "bordercolor", SET_COL_PANEL, "1.000", NULL);
    AdaptCommand(wid, "horizontaljustify", "left", NULL);
    wid->ProcessEvent(new Event("clearoptions"));

    for (i = 0; i < row->nopt; i++) {
        Event *opt = new Event("option");

        opt->AddString(row->opt[i][0]);
        opt->AddString(row->latched ? va("%s;ui_settingsautoapply", row->opt[i][1])
                                    : row->opt[i][1]);
        opt->AddString(row->opt[i][2]);
        wid->ProcessEvent(opt);
    }

    return wid;
}

static UIWidget *SettingsCheckBox(
    UIWidget *container, const settingsRow_s *row, const char *name, float x, float y, float w, float h
)
{
    UIWidget *wid = AdaptWidget(container, "CheckBox", name, x, y, w, h);

    AdaptCommand(wid, "title", "", NULL);    /* the row label carries the text */
    AdaptCommand(wid, "linkcvar", row->cvar, NULL);
    AdaptCommand(wid, "fgcolor", "1.00", "1.00", "1.00", "1.00", NULL);
    AdaptCommand(wid, "bgcolor", "0.50", "0.50", "0.50", "0.00", NULL);
    AdaptCommand(wid, "shadercolor", SET_COL_ORANGE, "1.000", NULL);
    AdaptCommand(wid, "hovershadercolor", SET_COL_ORANGE, "1.000", NULL);
    AdaptCommand(wid, "armedshadercolor", SET_COL_ORANGE, "1.000", NULL);
    AdaptCommand(wid, "selectedshadercolor", SET_COL_ORANGE, "1.000", NULL);
    AdaptCommand(wid, "borderstyle", "NONE", NULL);
    AdaptCommand(wid, "checked_shader", "textures/menu/checkbox_checked", NULL);
    AdaptCommand(wid, "unchecked_shader", "textures/menu/checkbox_unchecked", NULL);
    AdaptCommand(wid, "hovershader", "menu/generic/button-hover", NULL);
    AdaptCommand(wid, "armedshader", "menu/generic/button-press", NULL);
    AdaptCommand(wid, "horizontaljustify", "left", NULL);
    AdaptCommand(wid, "clicksound", "sound/misc/menu/click.wav", NULL);

    /* the mark fills the row instead of sitting 16px inside a taller one */
    if (wid->isSubclassOf(UICheckBox)) {
        ((UICheckBox *)wid)->setCheckScale(1.3f);
    }

    if (row->latched) {
        /* the cvar write is immediate, so the hook only has to follow it */
        AdaptCommand(wid, "checked_command", "ui_settingsautoapply", NULL);
        AdaptCommand(wid, "unchecked_command", "ui_settingsautoapply", NULL);
    }

    return wid;
}

static UIWidget *SettingsSlider(
    UIWidget *container, const settingsRow_s *row, const char *name, float x, float y, float w, float h
)
{
    char     lo[32];
    char     hi[32];
    char     step[32];
    UIWidget *wid;

    Com_sprintf(lo, sizeof(lo), "%g", row->lo);
    Com_sprintf(hi, sizeof(hi), "%g", row->hi);
    Com_sprintf(step, sizeof(step), "%g", row->step);

    wid = AdaptWidget(container, "Slider", name, x, y, w, h);

    AdaptCommand(wid, "linkcvar", row->cvar, NULL);
    AdaptCommand(wid, "fgcolor", "1.00", "1.00", "1.00", "1.00", NULL);
    AdaptCommand(wid, "bgcolor", "0.000", "0.000", "0.000", "0.750", NULL);
    AdaptCommand(wid, "borderstyle", "OUTLINE_BORDER", NULL);
    AdaptCommand(wid, "bordercolor", SET_COL_PANEL, "1.000", NULL);
    AdaptCommand(wid, "setrange", lo, hi, NULL);
    AdaptCommand(wid, "stepsize", step, NULL);
    AdaptCommand(wid, "slidertype", row->type, NULL);
    AdaptCommand(wid, "clicksound", "sound/misc/menu/click.wav", NULL);

    if (row->latched) {
        /* the slider's own command runs on every write; the settle window in
           UI_SettingsAutoApplyFrame is what makes it one restart, not one a frame */
        AdaptCommand(wid, "command", "ui_settingsautoapply", NULL);
    }

    return wid;
}

/* Builds one page: a full-width section header, then its rows as label+value pairs
   flowing down two aligned columns.  `g` is the left column, `g+1` the right; a page
   with one column passes the same pointer twice and gets a single column.
   Every widget name carries the page prefix, because the named lookup walks every
   menu in the tree and the first hit wins. */
#define SET_ROWS_MAX 32

static int SettingsBuildRows(
    UIWidget             *container,
    const settingsRow_s  *rows,
    int                   count,
    const settingsGeom_s *cols,
    int                   ncols,
    float                 y,
    float                *bottom
)
{
    UIWidget *panel    = NULL;
    UIWidget *labels[SET_ROWS_MAX];
    UIWidget *ctls[SET_ROWS_MAX];
    float     rowy[SET_ROWS_MAX];
    int       rowcol[SET_ROWS_MAX];
    float     labelw   = 0.0f;
    float     paneltop = 0.0f;
    int       i;
    int       built   = 0;
    int       section = 0;
    int       secrow  = 0;

    for (i = 0; i < count; i++) {
        const settingsRow_s  *row = &rows[i];
        const settingsGeom_s *g;
        char                  name[64];
        int                   band;
        int                   col;
        UIWidget             *ctl;
        UIWidget             *label;

        if (row->section) {
            if (panel) {
                SettingsRect(panel, cols[0].boxx, paneltop, cols[1].boxx + cols[1].boxw - cols[0].boxx,
                             y - paneltop + 4.0f);
            }

            SettingsHeader(container, va("%sHead%d", cols[0].prefix, ++section), row->section,
                           cols[0].boxx, y, cols[1].boxx + cols[1].boxw - cols[0].boxx);
            y += SET_HEAD_H + SET_HEAD_GAP;
            paneltop = y;
            panel = SettingsPanel(container, va("%sBox%d", cols[0].prefix, section),
                                  cols[0].boxx, paneltop,
                                  cols[1].boxx + cols[1].boxw - cols[0].boxx, 10.0f);
            secrow = 0;
        }

        if (!row->widget || built >= SET_ROWS_MAX) {
            continue;
        }

        /* one column walks every row down the page; two fill a band left then right */
        col  = (ncols > 1) ? (secrow & 1) : 0;
        band = (ncols > 1) ? (secrow >> 1) : secrow;
        g    = &cols[col];

        Com_sprintf(name, sizeof(name), "%s_%s_label", g->prefix, row->cvar);
        label = SettingsRowLabel(container, name, row->label, g->labelx,
                                 paneltop + band * g->pitch, g->labelw, g->rowh);
        AdaptTouchTooltip(label, row->tooltip);

        Com_sprintf(name, sizeof(name), "%s_%s", g->prefix, row->cvar);

        if (!strcmp(row->widget, "SwitchBox")) {
            ctl = SettingsSwitchBox(container, row, name, g->valx,
                                    paneltop + band * g->pitch, g->valw, g->rowh);
        } else if (!strcmp(row->widget, "CheckBox")) {
            ctl = SettingsCheckBox(container, row, name, g->valx,
                                   paneltop + band * g->pitch, g->valw, g->rowh);
        } else {
            ctl = SettingsSlider(container, row, name, g->valx,
                                 paneltop + band * g->pitch, g->valw, g->rowh);
        }

        AdaptTouchTooltip(ctl, row->tooltip);

        /* a row this target greys cannot move, so it owes no restart */
        if (row->latched && !SettingsRowIsGreyed(row->cvar)) {
            SettingsBaselineRecord(row->cvar);
        }

        labels[built] = label;
        ctls[built]   = ctl;
        rowy[built]   = paneltop + band * g->pitch;
        rowcol[built] = col;
        built++;
        secrow++;
        y = paneltop + (band + 1) * g->pitch;
    }

    /* One width for the label column, taken from the longest label the page carries.
       The row keeps the slot the build pass gave it -- the slot is read back out of
       rowy/rowcol, never recounted, so a section boundary cannot move a row. */
    for (i = 0; i < built; i++) {
        float w = SettingsTitleWidth(labels[i]);

        if (w > labelw) {
            labelw = w;
        }
    }
    labelw += SET_LABEL_PAD;

    if (labelw > cols[0].labelw) {
        UIVK_PROBE("settings", "op=labelw authored=%.0f measured=%.0f",
                   cols[0].labelw, labelw);
    }

    for (i = 0; i < built; i++) {
        const settingsGeom_s *g = &cols[rowcol[i]];
        float                 valx = g->labelx + labelw + SET_LABEL_GAP;

        SettingsRect(labels[i], g->labelx, rowy[i], labelw, g->rowh);
        SettingsRect(ctls[i], valx, rowy[i], g->valx + g->valw - valx, g->rowh);
    }

    if (panel) {
        SettingsRect(panel, cols[0].boxx, paneltop, cols[1].boxx + cols[1].boxw - cols[0].boxx,
                     y - paneltop + 4.0f);
    }

    *bottom = y;
    return built;
}

/* The video page is rebuilt, so everything stays hidden except the page switcher,
   the description bar, the page's own LCARS chrome and the Main Menu pair.  getTitle
   hands back the translated string, not the $$token$$ the page data carries, so the
   keep list is compared both ways. */
static const char *const s_videoKeepTitles[] = {
    "$$Controls$$", "$$Video$$", "$$Audio$$", "$$GameOptions$$", "$$MainMenu$$", NULL
};

/* background.inc's panels and logo: Widget resources with no title, so the title test
   dropped them and this page alone lost its LCARS art. */
static const char *const s_videoKeepNames[] = {
    "main_bkg_01", "main_bkg_02", "main_bkg_03",
    "main_bkg_04", "main_bkg_05", "main_bkg_06",
    "logo", "tooltip_video", NULL
};

static qboolean SettingsNameKeeps(const char *name)
{
    int j;

    if (!name || !name[0]) {
        return qfalse;
    }

    for (j = 0; s_videoKeepNames[j]; j++) {
        if (!strcmp(name, s_videoKeepNames[j])) {
            return qtrue;
        }
    }

    return qfalse;
}

/* A row keeps its page slot only when its title is one of the five above.  The
   comparison is against the translated string getTitle hands back AND against the
   token the page data carries, because a token that resolves to nothing leaves the
   title empty and an empty title must never keep a row. */
static qboolean SettingsVideoKeeps(UIWidget *wid)
{
    const char *title;
    int         j;

    if (SettingsNameKeeps(wid->getName())) {
        return qtrue;
    }

    title = wid->getTitle();
    if (!title || !title[0]) {
        return qfalse;
    }

    for (j = 0; s_videoKeepTitles[j]; j++) {
        const char *want = s_videoKeepTitles[j];

        if (!stricmp(title, want)) {
            return qtrue;
        }

        if (isStringResource(want)) {
            const char *resolved = getStringResource(want);

            if (resolved && !stricmp(title, resolved)) {
                return qtrue;
            }
        }
    }

    return qfalse;
}

static void SettingsHideVideoRetail(UIWidget *container)
{
    Container<UIWidget *>& children = container->getChildren();
    int                    i;
    int                    hidden = 0;
    int                    art    = 0;

    for (i = 1; i <= children.NumObjects(); i++) {
        UIWidget   *wid  = children.ObjectAt(i);
        const char *name = wid->getName();

        if (SettingsNameKeeps(name)) {
            /* the chrome a rebuild has to leave standing: the row names it, so a boot
               proves the art is still there instead of assuming it */
            if (strcmp(name, "tooltip_video")) {
                art++;
                UIVK_PROBE("settings", "op=keep art='%s' shader='%s' show=%d",
                           name, wid->getMaterialName() ? wid->getMaterialName() : "-",
                           wid->getShow() ? 1 : 0);
            }
            continue;
        }

        if (!SettingsVideoKeeps(wid)) {
            wid->Disable();
            hidden++;
        }
    }

    UIVK_PROBE("settings", "op=hide menu='video' hidden=%d art=%d", hidden, art);
}

/* The kept rows.  DESIGN.md 2.1 carries the cvar each one drives and why it stayed.
   Three sections of six: two columns a band, three bands a section, one band shape
   everywhere -- so no band ever has an empty cell to drift against. */
static settingsRow_s s_videoRows[] = {
    {"Display & Quality", "Aspect", "android_aspect", "SwitchBox",
     {{"Fill", "set android_aspect 0", "0"},
      {"16:9", "set android_aspect 1", "1"},
      {"4:3", "set android_aspect 2", "2"}},
     3, 0, 0, 0, NULL, 0, "The shape of the picture"},

    /* retail's own row (menu_video.urc rect 162 188 120 16, linkcvar r_fullscreen):
       CVAR_LATCH, so a change lands when the auto-apply restarts the renderer. */
    {NULL, "$$FullScreen$$", "r_fullscreen", "SwitchBox",
     {{"Full Screen", "set r_fullscreen 1", "1"},
      {"Windowed", "set r_fullscreen 0", "0"}},
     2, 0, 0, 0, NULL, 1, "$$SetFullScreen$$"},

    {NULL, "$$Brightness$$", "r_gamma", "Slider",
     {{0}}, 0, 0.5f, 3.0f, 0.1f, "float", 1,
     "How bright everything is.  Changing it re-bakes the textures, so it restarts."},

    {NULL, "FPS Counter", "cg_drawFPS", "CheckBox",
     {{0}}, 0, 0, 0, 0, NULL, 0, "The frames-per-second readout in the corner"},

    {NULL, "Frame Rate Limit", "com_maxfps", "SwitchBox",
     {{"$$Off$$", "set com_maxfps 0", "0"},
      {"60", "set com_maxfps 60", "60"},
      {"120", "set com_maxfps 120", "120"}},
     3, 0, 0, 0, NULL, 0, "The most frames a second the engine will draw.  Off is uncapped."},

    {NULL, "$$TextureQuality$$", "r_picmip", "SwitchBox",
     {{"$$Low$$", "set r_picmip 2", "2"},
      {"$$Medium$$", "set r_picmip 1", "1"},
      {"$$High$$", "set r_picmip 0", "0"}},
     3, 0, 0, 0, NULL, 1, "How far textures are shrunk.  Changing it restarts the renderer."},

    {"Lighting", "Simple Lighting", "r_light_simple", "CheckBox",
     {{0}}, 0, 0, 0, 0, NULL, 0, "Flat light instead of the lit path"},

    {NULL, "Simple Lighting Reach", "r_light_simple_dist", "SwitchBox",
     {{"$$Close$$", "set r_light_simple_dist 100", "100"},
      {"$$Near$$", "set r_light_simple_dist 2500", "2500"},
      {"$$Far$$", "set r_light_simple_dist 5000", "5000"}},
     3, 0, 0, 0, NULL, 0, "How far the simple light reaches"},

    {NULL, "Vertex Lighting", "r_vertexLight", "CheckBox",
     {{0}}, 0, 0, 0, 0, NULL, 1, "Per-vertex light instead of lightmaps.  Changing it restarts."},

    {NULL, "$$DynamicLights$$", "r_dynamiclight", "CheckBox",
     {{0}}, 0, 0, 0, 0, NULL, 0, "The weapon and muzzle lights"},

    {NULL, "$$Shadows$$", "cg_shadows", "SwitchBox",
     {{"$$None$$", "set cg_shadows 0", "0"},
      {"$$Simple$$", "set cg_shadows 1", "1"}},
     2, 0, 0, 0, NULL, 0, "The blob under a character"},

    {NULL, "$$Flares$$", "r_flares", "SwitchBox",
     {{"$$None$$", "set r_flares 0", "0"},
      {"$$On$$", "set r_flares 1", "1"}},
     2, 0, 0, 0, NULL, 0, "The light halos"},

    {"Effects & Detail", "Geometry Detail", "r_subdivisions", "SwitchBox",
     {{"$$Low$$", "set r_subdivisions 8", "8"},
      {"$$Medium$$", "set r_subdivisions 4", "4"},
      {"$$High$$", "set r_subdivisions 2", "2"}},
     3, 0, 0, 0, NULL, 1, "How finely curves are cut.  Changing it restarts the renderer."},

    {NULL, "Effect Detail", "cg_effectdetail", "SwitchBox",
     {{"$$Low$$", "set cg_effectdetail 0.5", "0.5"},
      {"$$Medium$$", "set cg_effectdetail 0.8", "0.8"},
      {"$$High$$", "set cg_effectdetail 1.0", "1.0"}},
     3, 0, 0, 0, NULL, 0, "How many effect sprites spawn"},

    /* Curve Detail is cut, both targets: r_lodCurveError is CVAR_CHEAT
       (tr_init.c) and Cvar_SetCheatState resets it on every server connect. */

    {NULL, "$$DeformGeometry$$", "r_allowGeometryDeforms", "CheckBox",
     {{0}}, 0, 0, 0, 0, NULL, 0, "Surfaces that wave and bulge"},

    {NULL, "$$DetailSurfaces$$", "r_detailSurfaces", "CheckBox",
     {{0}}, 0, 0, 0, 0, NULL, 0, "The extra detail pass on some surfaces"},

    {NULL, "$$DetailTextures$$", "r_detailtextures", "CheckBox",
     {{0}}, 0, 0, 0, 0, NULL, 0, "The close-up grain on walls and floors"},

    {NULL, "$$WallDecals$$", "cg_marks", "CheckBox",
     {{0}}, 0, 0, 0, 0, NULL, 0, "Bullet holes and scorch marks"},
};

/* The one row the handheld cannot honour is greyed, not hidden: shown, inert, out of the
   pad walk.  `widgetstate disable' is this uilib's greyed state -- the switch and the
   check both refuse it and the disabled palette greys it -- and AllowActivate takes it
   out of moveFocus, which stops on CanActivate widgets only. */
#ifdef __ANDROID__
static void SettingsGreyRow(UIWidget *label, UIWidget *ctl)
{
    AdaptCommand(label, "widgetstate", "disable", NULL);
    AdaptCommand(label, "disabledfgcolor", "0.400", "0.400", "0.450", "1.000", NULL);
    AdaptCommand(ctl, "widgetstate", "disable", NULL);
    AdaptCommand(ctl, "disabledfgcolor", "0.400", "0.400", "0.450", "1.000", NULL);
    AdaptCommand(ctl, "disabledshadercolor", "0.400", "0.400", "0.450", "1.000", NULL);
    ctl->AllowActivate(false);
}
#endif

static void SettingsAdaptVideo(UIWidget *container)
{
    float bottom = 0.0f;
    int   built;
    int   i;

    s_settingsTooltipBar = "tooltip_video";

    /* the first section header: a real widget, so the guard actually holds */
    if (container->FindWidget(va("%sHead1", s_videoCol[0].prefix))) {
        return;    // built once; a reload re-runs the pass
    }

    SettingsBaselineClear();
    SettingsHideVideoRetail(container);

    built = SettingsBuildRows(container, s_videoRows,
                              (int)(sizeof(s_videoRows) / sizeof(s_videoRows[0])),
                              s_videoCol, 2, 92.0f, &bottom);

#ifdef __ANDROID__
    for (i = 0; i < (int)(sizeof(s_videoRows) / sizeof(s_videoRows[0])); i++) {
        UIWidget *lbl;
        UIWidget *ctl;
        int       c;

        if (!s_videoRows[i].cvar || !SettingsRowIsGreyed(s_videoRows[i].cvar)) {
            continue;
        }

        /* the column the row landed in, not the one the table guessed */
        for (c = 0; c < 2 && !lbl; c++) {
            lbl = container->FindWidget(va("%s_%s_label", s_videoCol[c].prefix, s_videoRows[i].cvar));
            ctl = container->FindWidget(va("%s_%s", s_videoCol[c].prefix, s_videoRows[i].cvar));
        }

        if (lbl && ctl) {
            SettingsGreyRow(lbl, ctl);
            UIVK_PROBE("settings", "op=row cvar=%s state=greyed target=android",
                       s_videoRows[i].cvar);
        } else {
            UIVK_PROBE("settings", "op=row cvar=%s state=MISSING target=android",
                       s_videoRows[i].cvar);
        }
    }
#else
    for (i = 0; i < (int)(sizeof(s_videoDesktopRows) / sizeof(s_videoDesktopRows[0])) - 1; i++) {
        UIVK_PROBE("settings", "op=row cvar=%s state=live target=desktop",
                   s_videoDesktopRows[i]);
    }
#endif

    UIVK_PROBE("settings", "op=build menu='video' rows=%d bottom=%.0f", built, bottom);
}

#ifdef __ANDROID__
/* ---- controls ---- */

/* The touch panel's rows.  The four bind tables keep retail's own rows and come back;
   this panel is the fifth tab. */
static settingsRow_s s_touchRows[] = {
    {"Touch & Stick", "Touch Look Sensitivity", "cl_touchLook", "Slider",
     {{0}}, 0, 0.5f, 6.0f, 0.1f, "float", 0, "Look sensitivity on the touch pad"},

    {NULL, "Joystick Deadzone", "joy_threshold", "Slider",
     {{0}}, 0, 0.05f, 0.35f, 0.01f, "float", 0, "How far the stick must move before it responds"},

    {NULL, "Turn Speed", "cl_yawspeed", "Slider",
     {{0}}, 0, 60.0f, 360.0f, 15.0f, "integer", 0, "How fast the stick turns you"},

    {NULL, "Look Speed", "cl_pitchspeed", "Slider",
     {{0}}, 0, 45.0f, 270.0f, 15.0f, "integer", 0, "How fast the stick looks up and down"},

    {NULL, "Crouch Toggle", "cl_crouchToggle", "CheckBox",
     {{0}}, 0, 0, 0, 0, NULL, 0, "Tap crouch to hold it down"},

    {NULL, "Easy Jump", "cl_easyJump", "CheckBox",
     {{0}}, 0, 0, 0, 0, NULL, 0, "Jump is plain and immediate"},
};

/* Five panels.  A tab's command enables its own and disables the rest; the shipped
   tab buttons already carry the four bind tables' strings, which are rewritten here
   so the touch panel joins the same dance. */
static const char *const s_controlPanels[] = {
    "WeaponBindList", "AttackLookBindList", "MovementBindList", "GameplayBindList", NULL
};

static const char *const s_controlTabs[] = {
    "$$Weapons$$", "$$AttackLook$$", "$$Movement$$", "$$GamePlay$$", "Touch"
};

static const char *const s_controlTabNames[] = {
    "WeaponBind", "AttackLookBind", "MovementBind", "GameplayBind", "TouchBind"
};

#define SET_TAB_X   46.0f
#define SET_TAB_Y   87.0f
#define SET_TAB_W   64.0f
#define SET_TAB_H   16.0f
#define SET_TAB_GAP 1.0f

static void SettingsTabCommand(int index)
{
    str cmd;
    int i;

    for (i = 0; i < 4; i++) {
        cmd += str("globalwidgetcommand ") + s_controlPanels[i]
             + (i == index ? " enable;" : " disable;");
    }

    /* the touch panel is its header, its box and the twelve widgets between them;
       every name here carries this page's prefix, because the named lookup walks
       every menu in the tree and the first hit wins */
    cmd += str(";globalwidgetcommand TouchHead1 ") + ((index == 4) ? "enable" : "disable");
    cmd += str(";globalwidgetcommand TouchBox1 ") + ((index == 4) ? "enable" : "disable");

    for (i = 0; i < ( int )( sizeof( s_touchRows ) / sizeof( s_touchRows[0] ) ); i++) {
        cmd += str(";globalwidgetcommand Touch_") + s_touchRows[i].cvar + "_label "
             + ((index == 4) ? "enable" : "disable");
        cmd += str(";globalwidgetcommand Touch_") + s_touchRows[i].cvar + " "
             + ((index == 4) ? "enable" : "disable");
    }

    uWinMan.FindWidget(s_controlTabNames[index])->LinkCommand(cmd);
}

static void SettingsAdaptControls(UIWidget *container)
{
    UIWidget *tab;
    float     bottom = 0.0f;
    int       i;
    int       built;

    s_settingsTooltipBar = "tooltip_controls";

    if (container->FindWidget("TouchBind")) {
        return;    // built once
    }

    for (i = 0; i < 5; i++) {
        if (i < 4) {
            tab = uWinMan.FindWidget(s_controlTabNames[i]);

            if (!tab) {
                continue;
            }

            SettingsRect(tab, SET_TAB_X + i * (SET_TAB_W + SET_TAB_GAP), SET_TAB_Y,
                         SET_TAB_W, SET_TAB_H);
        } else {
            tab = AdaptWidget(container, "Button", s_controlTabNames[i],
                              SET_TAB_X + i * (SET_TAB_W + SET_TAB_GAP), SET_TAB_Y,
                              SET_TAB_W, SET_TAB_H);
            AdaptCommand(tab, "fgcolor", "0.680", "0.630", "0.340", "1.000", NULL);
            AdaptCommand(tab, "bgcolor", "0.120", "0.150", "0.250", "1.000", NULL);
            AdaptCommand(tab, "hoverfgcolor", SET_COL_ORANGE, "1.000", NULL);
            AdaptCommand(tab, "hoverbgcolor", "0.199", "0.258", "0.380", "1.000", NULL);
            AdaptCommand(tab, "armedfgcolor", "1.000", "1.000", "1.000", "1.000", NULL);
            AdaptCommand(tab, "armedbgcolor", SET_COL_PANEL, "1.000", NULL);
            AdaptCommand(tab, "borderstyle", "3D_BORDER", NULL);
            AdaptCommand(tab, "staypressed", NULL);
            AdaptCommand(tab, "clicksound", "sound/misc/menu/click.wav", NULL);
        }

        tab->setTitle(s_controlTabs[i]);
        SettingsTabCommand(i);
    }

    built = SettingsBuildRows(container, s_touchRows,
                              (int)(sizeof(s_touchRows) / sizeof(s_touchRows[0])),
                              &s_touchCol, 1, 126.0f, &bottom);

    /* hidden until the touch tab takes them: the shipped page showcommand only knows
       the four bind tables, so the default tab stays Weapons */
    for (i = 0; i < ( int )( sizeof( s_touchRows ) / sizeof( s_touchRows[0] ) ); i++) {
        UIWidget *lbl = container->FindWidget(va("Touch_%s_label", s_touchRows[i].cvar));
        UIWidget *ctl = container->FindWidget(va("Touch_%s", s_touchRows[i].cvar));

        if (lbl) {
            lbl->Disable();
        }
        if (ctl) {
            ctl->Disable();
        }
    }

    UIVK_PROBE("settings", "op=build menu='controls' tabs=5 rows=%d bottom=%.0f", built, bottom);
}
#endif /* __ANDROID__ */


/* Serves the one row the Android menus trim by frame alone: a pulldown the shipped
   data leaves with no name, no cvar and no title. */
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
#endif /* __ANDROID__ */

void UI_AdaptSettingsMenus(void)
{
#ifndef __ANDROID__
    const int rowCount = (int)(sizeof(s_adaptRows) / sizeof(s_adaptRows[0]));
#endif
    int       i;

    for (i = 1; i <= uWinMan.GetChildCount(); i++) {
        UIWidget *container = uWinMan.GetChild(i);
        int       j;

        if (!container || !container->isSubclassOf(UIWidgetContainer)) {
            continue;
        }

#ifndef __ANDROID__
        /* the Linux build keeps retail's rows and only hides the ones no renderer reads */
        for (j = 0; j < rowCount; j++) {
            if (!stricmp(container->getName(), s_adaptRows[j].menu)) {
                AdaptHideRow(container, &s_adaptRows[j]);
            }
        }
#endif

#ifdef __ANDROID__
        for (j = 0; j < (int)(sizeof(s_adaptRowsAndroid) / sizeof(s_adaptRowsAndroid[0])); j++) {
            if (!stricmp(container->getName(), s_adaptRowsAndroid[j].menu)) {
                AdaptHideRow(container, &s_adaptRowsAndroid[j]);
            }
        }

        if (!stricmp(container->getName(), "GameOptions")) {
            AdaptHideAt(container, 586, 231, 16, 16);   // the data-rate pulldown: no name, no cvar, no title
        }
#endif

#ifdef __ANDROID__
        /* the controls page is the handheld's; the operator scoped the unified
           graphics page to graphics */
#else
        /* the Linux build hides only the rows no renderer reads; the video page is
           rebuilt below and owns its own content */
#endif
        if (!stricmp(container->getName(), "video")) {
            SettingsAdaptVideo(container);
        }
#ifdef __ANDROID__
        else if (!stricmp(container->getName(), "controls")) {
            SettingsAdaptControls(container);
        }
#endif
    }
}
