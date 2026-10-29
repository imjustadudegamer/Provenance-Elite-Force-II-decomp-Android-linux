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
            /* the dlight hunt: a reset lands on the cvar's reset string, which a
               pak `setd' may have rewritten -- name the pair */
            if (PROBE_On()) {
                cvar_t *cv = uii.Cvar_Find(wid->m_cvarname.c_str());

                PROBE_PRINT("[UI:cvarreset] menu='%s' %s -> '%s'\n", m_name.c_str(),
                            wid->m_cvarname.c_str(),
                            cv ? (cv->latchedString ? cv->latchedString : cv->resetString) : "-");
            }

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

/* The two antialiasing checkboxes name cvars the engine no longer registers and
 * nothing ever read, so they change neither picture nor cost; the shipped page
 * still carries the widgets and the rows must stay hidden. */
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
        /* the page leaving through a transition owns the same popcommand retail's
           PopMenu runs; skipping it leaves the shipped ui_hidemouse undelivered */
        head->NotifyPopMenu();
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

/* Push, Pop, Show and Transition all show the head they install and hide the one
   they retire, so once the motion lock has dropped a hidden head is a stack entry
   left behind -- and answering for it holds the catcher, the touch overlay and the
   pointer with nothing on screen. */
Menu *MenuManager::CurrentMenu(void)
{
    Stack<Menu *> *stacks[2] = { &m_showmenustack, &m_menustack };
    Menu          *head;
    int            i;

    for (i = 0; i < 2; i++) {
        while (!m_lock) {
            head = stacks[i]->Head();
            if (!head) {
                break;
            }
            if (!head->GetContainerWidget() || head->isVisible()) {
                return head;
            }
            stacks[i]->Pop();
        }

        head = stacks[i]->Head();
        if (head) {
            return head;
        }
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

/* these rows stay hidden on BOTH targets because the option does not exist in this lane, not because the menu is Android's: the antialiasing pair has no
   reader in either renderer (every pipeline is VK_SAMPLE_COUNT_1_BIT) and its cvars are gone.  The controls row hides retail's bind scrollbar, which this
   port's own bind tables replace. */

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

   DESIGN-MENU-GRAMMAR.md part II carries the kept/deleted lists and the file:line that kills each one.
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

/* both shipped faces (menu_video.urc 86, tri_keypad.urc); one step up from the -10 built rows inherited */
#define SET_FONT_ROW  "serpentinemedium-12"
#define SET_FONT_HEAD "serpentinemedium-14"

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

/* FILL-THE-BAND LAW, this port's own: pitch = the page's band budget over its own bands,
   floored, remainder in the last panel's tail.  Budget = 87..396 less the headers, tails
   and gaps: video (309-3x18-3x4-2x4)/9 -> 26, controls (309-2x18-2x4-1x4)/9 -> 29.
   Row = pitch - 4, so video lands 395 with 22-tall rows, controls 396 with 25. */
#define SET_HEAD_H   18.0f
#define SET_HEAD_GAP 0.0f
#define SET_BOX_PAD  4.0f
#define SET_SECT_GAP 4.0f

/* the content band: the tab row ends at 87, 396 is 8 above the description bar at 403 */
#define SET_PAGE_Y      87.0f
#define SET_PAGE_BOTTOM 396.0f
#define SET_ROW_GAP     4.0f

/* the controls page's own five-slot tab row (menu_controls.urc tabs 46 87, 16 tall) */
#define SET_TAB_X   46.0f
#define SET_TAB_Y   87.0f
#define SET_TAB_W   64.0f
#define SET_TAB_H   16.0f
#define SET_TAB_GAP 1.0f

#define SET_VIDEO_PITCH 26.0f
#define SET_CTL_PITCH   29.0f
#define SET_VIDEO_ROW_H (SET_VIDEO_PITCH - SET_ROW_GAP)
#define SET_CTL_ROW_H   (SET_CTL_PITCH - SET_ROW_GAP)

/* x 34..606 and the 4-unit label pad are the shipped pages' own content band
   (menu_gameoption.urc 34 103 320 104 / 359 103 247 104, labels at 38; menu_advancedvideo
   34/251).  The left column's own control ends 12 before the right column's first label --
   the shipped two-column pages' seam (menu_gameoption.urc value 194..350 against label 362,
   menu_advancedvideo.urc value+arrow 223..379 against label 391) -- so valx+valw pins that
   edge. */
static const settingsGeom_s s_videoCol[2] = {
    {"VidL", 38.0f, 160.0f, 198.0f, 117.0f, 34.0f, 283.0f, SET_VIDEO_PITCH, SET_VIDEO_ROW_H},
    {"VidR", 327.0f, 160.0f, 487.0f, 115.0f, 323.0f, 283.0f, SET_VIDEO_PITCH, SET_VIDEO_ROW_H}
};

/* The controls page's own right column: x 374..606 (menu_controls.urc's right column opens
   at 374, the shipped Player Preview at 423..546), the same label/value seam arithmetic the
   video columns carry, and the column's own band. */
static const settingsGeom_s s_ctlCol = {
    "Ctl", 374.0f, 104.0f, 478.0f, 124.0f, 374.0f, 232.0f, SET_CTL_PITCH, SET_CTL_ROW_H
};

#ifdef __ANDROID__
#define SET_TARGET "android"
#else
#define SET_TARGET "desktop"
#endif

/* the label column is one width everywhere; the longest label in the page's own font
   sets it, so a translated string can never be cut by a width this table guessed.
   The value box opens on the label box's own right edge: the shipped two-column pages
   run the pair edge to edge (menu_advancedvideo 38+185=223 with its value at 223,
   menu_gameoption 38+156=194 with its value at 194; menu_video/menu_audio leave 4). */
#define SET_LABEL_PAD 8.0f
#define SET_LABEL_GAP 0.0f

/* Each target greys the one row it cannot honour: the handheld's surface is the whole
   window, so r_fullscreen has nothing to change, and the desktop's window is already the
   video mode's own shape (r_mode owns it, retail ships no aspect row), so a letterbox
   inside that window only squashes the page.  The row still ships on both targets -- the
   page is one page -- and greying is EF1's own shape of that answer (ui_video.cpp:410,
   QMF_GRAYED). */
static const char *const s_videoDesktopRows[] = {"r_fullscreen", NULL};
static const char *const s_videoAndroidRows[] = {"android_aspect", NULL};

static qboolean SettingsRowInList(const char *cvar, const char *const *list)
{
    int i;

    if (!cvar) {
        return qfalse;
    }

    for (i = 0; list[i]; i++) {
        if (!stricmp(cvar, list[i])) {
            return qtrue;
        }
    }

    return qfalse;
}

static qboolean SettingsRowIsGreyed(const char *cvar)
{
#ifdef __ANDROID__
    return SettingsRowInList(cvar, s_videoDesktopRows);
#else
    return SettingsRowInList(cvar, s_videoAndroidRows);
#endif
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

    AdaptCommand(wid, "font", SET_FONT_HEAD, NULL);
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

    AdaptCommand(wid, "font", SET_FONT_ROW, NULL);
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
    AdaptCommand(wid, "font", SET_FONT_ROW, NULL);
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
    AdaptCommand(wid, "font", SET_FONT_ROW, NULL);
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

    /* the mark is retail's own 16 in the shipped page's own 16-tall row, and it
       starts at the cvar: a row built from a table has no state of its own */
    if (wid->isSubclassOf(UICheckBox)) {
        ((UICheckBox *)wid)->UpdateData();
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
    AdaptCommand(wid, "font", SET_FONT_ROW, NULL);
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
    float     boxh;
    int       i;
    int       built   = 0;
    int       section = 0;
    int       secrow  = 0;
    int       bands   = 0;
    float     rowtop  = 0.0f;

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
                boxh = bands * cols[0].pitch + SET_BOX_PAD;
                SettingsRect(panel, cols[0].boxx, paneltop, cols[1].boxx + cols[1].boxw - cols[0].boxx,
                             boxh);
                y = paneltop + boxh + SET_SECT_GAP;
            }

            SettingsHeader(container, va("%sHead%d", cols[0].prefix, ++section), row->section,
                           cols[0].boxx, y, cols[1].boxx + cols[1].boxw - cols[0].boxx);
            y += SET_HEAD_H + SET_HEAD_GAP;
            paneltop = y;
            panel = SettingsPanel(container, va("%sBox%d", cols[0].prefix, section),
                                  cols[0].boxx, paneltop,
                                  cols[1].boxx + cols[1].boxw - cols[0].boxx, 10.0f);
            secrow = 0;
            bands  = 0;
        }

        if (!row->widget || built >= SET_ROWS_MAX) {
            continue;
        }

        /* one column walks every row down the page; two fill a band left then right */
        col  = (ncols > 1) ? (secrow & 1) : 0;
        band = (ncols > 1) ? (secrow >> 1) : secrow;
        g    = &cols[col];

        /* the row sits the shipped 4 inside its panel and the tail is the same 4 under it */
        rowtop = paneltop + SET_BOX_PAD + band * g->pitch;

        Com_sprintf(name, sizeof(name), "%s_%s_label", g->prefix, row->cvar);
        label = SettingsRowLabel(container, name, row->label, g->labelx, rowtop, g->labelw, g->rowh);
        AdaptTouchTooltip(label, row->tooltip);

        Com_sprintf(name, sizeof(name), "%s_%s", g->prefix, row->cvar);

        if (!strcmp(row->widget, "SwitchBox")) {
            ctl = SettingsSwitchBox(container, row, name, g->valx, rowtop, g->valw, g->rowh);
        } else if (!strcmp(row->widget, "CheckBox")) {
            ctl = SettingsCheckBox(container, row, name, g->valx, rowtop, g->valw, g->rowh);
        } else {
            ctl = SettingsSlider(container, row, name, g->valx, rowtop, g->valw, g->rowh);
        }

        AdaptTouchTooltip(ctl, row->tooltip);

        /* a row this target greys cannot move, so it owes no restart */
        if (row->latched && !SettingsRowIsGreyed(row->cvar)) {
            SettingsBaselineRecord(row->cvar);
        }

        labels[built] = label;
        ctls[built]   = ctl;
        rowy[built]   = rowtop;
        rowcol[built] = col;
        built++;
        secrow++;
        bands = (ncols > 1) ? (secrow + 1) / 2 : secrow;
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
        boxh = bands * cols[0].pitch + SET_BOX_PAD;
        SettingsRect(panel, cols[0].boxx, paneltop, cols[1].boxx + cols[1].boxw - cols[0].boxx,
                     boxh);
        y = paneltop + boxh;
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

/* The kept rows.  DESIGN-MENU-GRAMMAR.md II.1 carries the cvar each one drives and why it stayed.
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

    /* both option lists are the shipped pages' own (menu_advancedvideo.urc: shadows
       447..449, flares 527..529), in the shipped order and at the shipped values. */
    {NULL, "$$Shadows$$", "cg_shadows", "SwitchBox",
     {{"$$None$$", "set cg_shadows 0", "0"},
      {"$$Simple$$", "set cg_shadows 1", "1"},
      {"$$Stencil$$", "set cg_shadows 2", "2"}},
     3, 0, 0, 0, NULL, 0, "The blob under a character"},

    {NULL, "$$Flares$$", "r_flares", "SwitchBox",
     {{"$$None$$", "set r_flares 0", "0"},
      {"$$Simple$$", "set r_flares 1", "1"},
      {"$$Precise$$", "set r_flares 2", "2"}},
     3, 0, 0, 0, NULL, 0, "The light halos"},

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
static void SettingsGreyRow(UIWidget *label, UIWidget *ctl)
{
    AdaptCommand(label, "widgetstate", "disable", NULL);
    AdaptCommand(label, "disabledfgcolor", "0.400", "0.400", "0.450", "1.000", NULL);
    AdaptCommand(ctl, "widgetstate", "disable", NULL);
    AdaptCommand(ctl, "disabledfgcolor", "0.400", "0.400", "0.450", "1.000", NULL);
    AdaptCommand(ctl, "disabledshadercolor", "0.400", "0.400", "0.450", "1.000", NULL);
    ctl->AllowActivate(false);
}

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
                              s_videoCol, 2, SET_PAGE_Y, &bottom);

    for (i = 0; i < (int)(sizeof(s_videoRows) / sizeof(s_videoRows[0])); i++) {
        /* both are read by the walk's own condition before the body writes them, so an
           uninitialised slot is the "widget" the grey pass would hand on */
        UIWidget *lbl = NULL;
        UIWidget *ctl = NULL;
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
            UIVK_PROBE("settings", "op=row cvar=%s state=greyed target=" SET_TARGET,
                       s_videoRows[i].cvar);
        } else {
            UIVK_PROBE("settings", "op=row cvar=%s state=MISSING target=" SET_TARGET,
                       s_videoRows[i].cvar);
        }
    }

    for (i = 0; i < 2; i++) {
        const char *const *list = i ? s_videoAndroidRows : s_videoDesktopRows;
        int                j;

        for (j = 0; list[j]; j++) {
            UIVK_PROBE("settings", "op=row cvar=%s state=%s target=" SET_TARGET, list[j],
                       SettingsRowIsGreyed(list[j]) ? "greyed" : "live");
        }
    }

    UIVK_PROBE("settings", "op=build menu='video' rows=%d bottom=%.0f", built, bottom);
}

#ifdef __ANDROID__
/* ---- controls ---- */

/* The one controls page.  EF1's own answer key carries every row a player touches on ONE
   page -- the binds it rebinds, the look and mouse rows beside them -- and the shipped page
   split the same rows over four tabs, so a rebind cost a tab hop and the touch and stick rows
   had nowhere to live but a fifth.  This page is one page: the bind list retail shipped,
   curated to the rows a handheld or controller player uses, beside the look, mouse, touch and
   stick rows in the shipped label/value grammar.

   The rows this page drops, with the reason each:
     Weapon Group 1..6  (warpinv 1..6)   six rows a stick player never reaches for; the
                                         weapon rows that stay are next/previous.
     Center View        (centerview)     the mouse/pad look path recentres itself; the key
                                         stays bound in the config.
     Drop Rune          (+droprune)      an arcade-lane action; not part of the campaign's
                                         control set.
     Message / Team Message / Taunt 1 / Taunt 2 / Vote Yes / Vote No
                                         chat and vote rows: no remote server and no team
                                         in this port's campaign, so they are rows that
                                         cannot do anything.

   Everything else the four shipped tables carried is here, in the shipped order. */

/* The one bind list.  The columns are the shipped page's own (menu_controls.urc bindcommand
   115 / bindbutton 106 / bindbuttonalt 106 under an 18 header), the scrollbar is the shipped
   page's own template, and the rows are the shipped addrow data minus the drop list above. */
typedef struct {
    const char *label;
    const char *command;
} ctlBindRow_s;

static const ctlBindRow_s s_bindRows[] = {
    {"$$MoveForward$$",      "+forward"},
    {"$$MoveBackward$$",     "+back"},
    {"$$TurnLeft$$",         "+left"},
    {"$$TurnRight$$",        "+right"},
    {"$$RunWalk$$",          "+speed"},
    {"$$StrafeLeft$$",       "+moveleft"},
    {"$$StrafeRight$$",      "+moveright"},
    {"$$Jump$$",             "+moveup"},
    {"$$LeanLeft$$",         "+leanleft"},
    {"$$LeanRight$$",        "+leanright"},
    {"$$Crouch$$",           "+movedown"},
    {"$$UseWeaponAim$$",     "+aim"},
    {"$$LookUp$$",           "+lookup"},
    {"$$LookDown$$",         "+lookdown"},
    {"$$Attack$$",           "+attackleft"},
    {"$$AlternateAttack$$",  "+attackright"},
    {"$$Use$$",              "+use"},
    {"$$Holster$$",          "+holster"},
    {"$$Reload$$",           "+reload"},
    {"$$NextWeapon$$",       "nextinv"},
    {"$$PreviousWeapon$$",   "previnv"},
    {"$$Objectives$$",       "+objectives_score"},
    {"$$NightVision$$",      "nightvision"},
    {"$$PauseGame$$",        "pause"},
    {"$$QuickSave$$",        "ui_quicksave"},
    {"$$QuickLoad$$",        "ui_quickload"},
};

/* The bind list's own frame: the shipped tables' rect is 46 124 325 255, and with the tab
   row gone the one list takes the whole content band the shipped pages open at 87. */
#define CTL_LIST_X 46.0f
#define CTL_LIST_Y 87.0f
#define CTL_LIST_W 325.0f
#define CTL_LIST_H 308.0f

/* The look, mouse, touch and stick rows, in two sections under their own headers, in the
   page's right column.  Ranges are EF1's own where EF1 carries the row (sensitivity 2..30,
   ui_controls2.cpp); the rest are the landed touch panel's. */
static settingsRow_s s_ctlRows[] = {
    {"Look & Mouse", "Sensitivity", "sensitivity", "Slider",
     {{0}}, 0, 2.0f, 30.0f, 1.0f, "float", 0, "How far a mouse movement turns you"},

    {NULL, "Smooth Mouse", "m_filter", "CheckBox",
     {{0}}, 0, 0, 0, 0, NULL, 0, "Averages the mouse over two frames"},

    {NULL, "Invert Look", "m_pitch", "SwitchBox",
     {{"$$Yes$$", "set m_pitch -0.022", "-0.022"},
      {"$$No$$", "set m_pitch 0.022", "0.022"}},
     2, 0, 0, 0, NULL, 0, "Pushing the mouse forward looks down, like a flight stick"},

    {"Touch & Stick", "Touch Look", "cl_touchLook", "Slider",
     {{0}}, 0, 0.5f, 6.0f, 0.1f, "float", 0, "Look sensitivity on the touch pad"},

    {NULL, "Deadzone", "joy_threshold", "Slider",
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

/* Furniture this one page retires, by name.  The last five are the page's own column and
   scrollbar templates: pure copy sources (copyProperties carries neither m_enabled nor
   m_visible), and they ship with no rect, so they live at UILayout's placeholder 10 10
   100 64 -- bindHeader paints an opaque box there and the buttons take a click. */
static const char *const s_ctlRetired[] = {
    "WeaponBind", "AttackLookBind", "MovementBind", "GameplayBind",
    "WeaponBindList", "AttackLookBindList", "MovementBindList", "GameplayBindList",
    "ModelView",
    "bindHeader", "bindcommand", "bindbutton", "bindbuttonalt", "bindscroll",
    /* the grab hint: the page's own showcommand used to hide it on open, and that
       command is gone, so the build sets the state the bind row expects */
    "PressKey", NULL
};

static void SettingsRetireControls(UIWidget *container)
{
    Container<UIWidget *>& children = container->getChildren();
    int                    i;
    int                    n = 0;

    /* The page's own showcommand re-arms the Weapons table every time it opens, so the
       split it serves goes with the tables: nothing the page shows needs to re-arm. */
    container->ClearShowCommands();

    for (i = 1; i <= children.NumObjects(); i++) {
        UIWidget   *wid  = children.ObjectAt(i);
        const char *name = wid->getName();
        int         j;

        if (!name || !name[0]) {
            continue;
        }

        for (j = 0; s_ctlRetired[j]; j++) {
            if (!strcmp(name, s_ctlRetired[j])) {
                wid->Disable();
                n++;
                break;
            }
        }
    }

    /* the preview's own two labels carry titles and no name the page can be addressed by */
    for (i = 1; i <= children.NumObjects(); i++) {
        UIWidget   *wid = children.ObjectAt(i);
        const char *title;

        if (!wid->isSubclassOf(UILabel) || !wid->isEnabled()) {
            continue;
        }

        title = wid->getTitle();

        if (!title || !title[0]) {
            continue;
        }

        if ((isStringResource("$$PlayerPreview$$") && !stricmp(title, getStringResource("$$PlayerPreview$$")))
            || (isStringResource("$$Controls$$") && !stricmp(title, getStringResource("$$Controls$$")))) {
            UIRect2D f = wid->getFrame();

            if (f.pos.x > 400.0f && f.pos.y < 130.0f) {
                wid->Disable();
                n++;
            }
        }
    }

    UIVK_PROBE("settings", "op=hide menu='controls' retired=%d", n);
}

/* The one bind list is built from the shipped page's own templates: the same column events
   the .urc issues, so the cells come out styled exactly as retail's did.  The list carries
   no norowselect, because a row the pad cannot stop on is a row the pad cannot rebind. */
static void SettingsBuildBindList(UIWidget *container)
{
    UIWidget *list = AdaptWidget(container, "Table", "CtlBindList",
                                 CTL_LIST_X, CTL_LIST_Y, CTL_LIST_W, CTL_LIST_H);
    int       i;

    if (!list) {
        return;
    }

    AdaptCommand(list, "fgcolor", "1.00", "1.00", "1.00", "1.00", NULL);
    AdaptCommand(list, "bgcolor", "0.000", "0.000", "0.000", "0.800", NULL);
    AdaptCommand(list, "borderstyle", "OUTLINE_BORDER", NULL);
    AdaptCommand(list, "bordercolor", "0.125", "0.157", "0.251", "1.000", NULL);
    AdaptCommand(list, "headerheight", "18", NULL);
    AdaptCommand(list, "itemheight", "16", NULL);
    AdaptCommand(list, "allowusersort", "0", NULL);
    AdaptCommand(list, "scrollbartemplate", "bindscroll", NULL);
    /* the shipped template ships scrollbar_width 0, so the bar was born zero-width;
       12 is the shipped save/load lists' own bar (menu_savegame.urc:165) */
    AdaptCommand(list, "scrollbar_width", "12", NULL);

    AdaptCommand(list, "tablecolumn", "bindcommand", "115", "alpha", "bindHeader", "$$Command$$", NULL);
    AdaptCommand(list, "tablecolumn", "bindbutton", "106", "alpha", "bindHeader", "$$Primary$$", NULL);
    AdaptCommand(list, "tablecolumn", "bindbuttonalt", "106", "alpha", "bindHeader", "$$Alternate$$", NULL);

    AdaptCommand(list, "hovercommand", "globalwidgetcommand tooltip_controls title \"$$PressEnterOrClickToChange$$\"", NULL);
    AdaptCommand(list, "hoverexitcommand", "globalwidgetcommand tooltip_controls title \"\"", NULL);

    for (i = 0; i < ( int )( sizeof( s_bindRows ) / sizeof( s_bindRows[0] ) ); i++) {
        AdaptCommand(list, "addrow", s_bindRows[i].label, s_bindRows[i].command, s_bindRows[i].command, NULL);
    }

    UIVK_PROBE("settings", "op=bindlist rows=%d frame=%.0f,%.0f %.0fx%.0f",
               ( int )( sizeof( s_bindRows ) / sizeof( s_bindRows[0] ) ),
               ( double )CTL_LIST_X, ( double )CTL_LIST_Y, ( double )CTL_LIST_W, ( double )CTL_LIST_H);
}

static void SettingsAdaptControls(UIWidget *container)
{
    const settingsGeom_s ctlCols[2] = {s_ctlCol, s_ctlCol};
    float                bottom = 0.0f;
    int                  built;

    s_settingsTooltipBar = "tooltip_controls";

    if (container->FindWidget("CtlBindList")) {
        return;    // built once
    }

    SettingsRetireControls(container);
    SettingsBuildBindList(container);

    /* every width the helper lays out comes from cols[1]; a one-column page hands the same
       column twice, and the column opens on the shipped page's own right edge */
    built = SettingsBuildRows(container, s_ctlRows,
                              (int)(sizeof(s_ctlRows) / sizeof(s_ctlRows[0])),
                              ctlCols, 1, SET_PAGE_Y, &bottom);

    UIVK_PROBE("settings", "op=build menu='controls' rows=%d bottom=%.0f", built, bottom);
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
        /* the controls page is the handheld's; my order scoped the unified
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
