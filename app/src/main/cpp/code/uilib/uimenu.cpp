
#include "ui_local.h"

#define R120_PROBE_GET(r120_n, r120_d, r120_f) ((cvar_t *)0)
#define R120_PROBE_PRINT                       Com_Printf
#define R120_ON_OVERRIDE                       (atoi(CvarGetForUI("qa_r120", "1")) != 0)  
#include "../qcommon/qa_r120.h"

#include "../qcommon/qa_r202.h"

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
            R120_PROBE(
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

    if (R120_On() && !strcmp(m_name.c_str(), "dm_scoreboard")) {
        static int r130_sbmenu;

        if (R120_Throttle(&r130_sbmenu, 6, 20)) {
            R130_PROBE("sb:menuwidgets", "menu='%s' widgets=%d", m_name.c_str(), n);
            for (int r130i = 1; r130i <= n; r130i++) {
                m_itemlist.ObjectAt(r130i)->R127DumpState(r130i, m_name.c_str());
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

        // Retail does NOT run the hidecommands here -- that is HideMenu's job -- and
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

// R127 TEMP PROBE -- PORT-ONLY, TEMPORARY, NEVER MERGE.  MenuManager::ShowMenu's
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

// directly.  Retail does not bounds-check here; this port does, because the port creates
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
        Menu *menu = m_menulist.ObjectAt(i);

        n = menu->m_itemlist.NumObjects();
        for (j = 1; j <= n; j++) {
            UIWidget *wid = menu->m_itemlist.ObjectAt(j);

            if (wid) {
                wid->ChangeResolution();
            }
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
// never executed it -- the shipped six-resolution SwitchBox plus the `reslist`
static void R134AndroidVideoMenuFixup(Menu *menu)
{
    UIWidget *r134cont;
    UIWidget *r134w;

    if (!menu || Q_stricmp(menu->m_name.c_str(), "video")) {
        return;
    }

    r134cont = menu->GetContainerWidget();
    if (!r134cont) {
        return;
    }

    r134w = r134cont->getFirstChild();

    while (r134w) {
        const char *r134cvar  = r134w->getCvarName();
        const char *r134title = r134w->getTitle();
        const char *r134name  = r134w->getName();

        if (r134cvar && r134cvar[0] && !Q_stricmp(r134cvar, "r_mode")
            && (!r134title || strcmp(r134title, "$$Hidden$$"))) {
            static const char *r134labels[3] = { "Fill", "16:9", "4:3" };
            static const char *r134cmds[3]   = {
                "set android_aspect 0", "set android_aspect 1", "set android_aspect 2"
            };
            Event *r134link;
            int    r134i;

            r134w->ProcessEvent(new Event("clearoptions"));

            r134link = new Event(EV_Layout_LinkCvar);
            r134link->AddString("android_aspect");
            r134w->ProcessEvent(r134link);

            for (r134i = 0; r134i < 3; r134i++) {
                Event *r134ev = new Event("option");

                r134ev->AddString(r134labels[r134i]);
                r134ev->AddString(r134cmds[r134i]);
                r134ev->AddString(va("%d", r134i));
                r134w->ProcessEvent(r134ev);
            }
        } else if (r134title && !strcmp(r134title, "$$VideoMode$$")) {
            r134w->setTitle("Aspect");     
        } else if (r134name && !Q_stricmp(r134name, "reslist")) {
            r134w->ProcessEvent(new Event(EV_Widget_Hide));   
        }

        r134w = r134w->getNextSibling();
    }
}
#endif

static const char *const r152RetailOnlyAudioCvars[] = {
    "s_milesdriver",   
    "s_reverb",        
    NULL
};

static qboolean R152RetailOnlyAudioCvar(const char *name)
{
    int i;

    if (!name || !name[0]) {
        return qfalse;
    }

    for (i = 0; r152RetailOnlyAudioCvars[i]; i++) {
        if (!Q_stricmp(name, r152RetailOnlyAudioCvars[i])) {
            return qtrue;
        }
    }

    return qfalse;
}

static void R152AudioMenuFixup(Menu *menu)
{
    UIWidget *r152cont;
    UIWidget *r152w;

    if (!menu || Q_stricmp(menu->m_name.c_str(), "audio")) {
        return;
    }

    r152cont = menu->GetContainerWidget();
    if (!r152cont) {
        return;
    }

    r152w = r152cont->getFirstChild();

    while (r152w) {
        const char *r152cvar  = r152w->getCvarName();
        const char *r152title = r152w->getTitle();
        const char *r152name  = r152w->getName();

        if (R152RetailOnlyAudioCvar(r152cvar)) {
            r152w->ProcessEvent(new Event(EV_Widget_Hide));
        } else if (r152title && !strcmp(r152title, "$$SoundDriver$$")) {
            r152w->ProcessEvent(new Event(EV_Widget_Hide));
        } else if (r152name && !Q_stricmp(r152name, "sounddriver")) {
            r152w->ProcessEvent(new Event(EV_Widget_Hide));
        }

        r152w = r152w->getNextSibling();
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

    R152AudioMenuFixup(m);

#ifdef __ANDROID__
    R134AndroidVideoMenuFixup(m);
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

    R152AudioMenuFixup(m);

#ifdef __ANDROID__
    R134AndroidVideoMenuFixup(m);
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

    R127_PROBE("menu:show", "name='%s' lock=%d", name.c_str(), m_lock ? 1 : 0);

    if (m_lock) {
        return false;
    }

    m = FindMenu(name);
    if (!m) {
        uii.Sys_Printf("Couldn't find menu %s\n", name.c_str());
        R127_PROBE("menu:show", "MISS name='%s'", name.c_str());
        return false;
    }

    R152AudioMenuFixup(m);

#ifdef __ANDROID__
    R134AndroidVideoMenuFixup(m);
#endif

    m->SaveCVars();
    m->ProcessEvent(new Event("showmenu"));

    {
        int r127i;
        int r127n = m->GetWidgetCount();

        R127_PROBE("menu:widgets", "menu='%s' widgets=%d", name.c_str(), r127n);
        for (r127i = 1; r127i <= r127n; r127i++) {
            m->GetWidget(r127i)->R127DumpState(r127i, name.c_str());
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

    R202_ROOT_BEGIN(R202_FAM_WALK);

    num = m_menulist.NumObjects();
    R202_COUNT(R202_FAM_WALK, R202_WC_menus, num);

    for (i = 1; i <= num; i++) {
        Menu     *menu = m_menulist.ObjectAt(i);
        UIWidget *cont = menu ? menu->GetContainerWidget() : NULL;
        int       r202_vis = ( cont && cont->IsVisible() ) ? 1 : 0;
        int       r202_t   = ( r202_statsOn ? R202_Microseconds() : 0 );

        menu->Update();

        if ( r202_statsOn ) {
            R202_Add( R202_FAM_WALK, r202_vis ? R202_WZ_vis : R202_WZ_hid,
                      R202_Microseconds() - r202_t );
            R202_Bump( R202_FAM_WALK, r202_vis ? R202_WC_visw : R202_WC_hidw, 1 );
        }
    }

    R202_ROOT_END(R202_FAM_WALK);
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
