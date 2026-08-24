
#pragma once

#include "../corepp/listener.h"
#include "../corepp/stack.h"

class Menu : public Listener
{
    qboolean m_fullscreen;
    int      m_vidmode;
    // Retail Menu::noEscape @0x0006DB00 (52 B) does NOT cache it on the Menu -- it reads
    // retail would not).  Never per-frame widget work in steady state.

public:
    CLASS_PROTOTYPE(Menu);

public:
    Container<UIWidget *> m_itemlist;
    str                   m_name;

public:
    Menu();
    Menu(str name);

    void      AddMenuItem(UIWidget *item);
    void      DeleteMenuItem(UIWidget *item);
    // R127 TEMP PROBE accessors -- PORT-ONLY, TEMPORARY, NEVER MERGE.  MenuManager::
    int       GetWidgetCount(void);
    UIWidget *GetWidget(int index);
    void      setName(str name);
    void      ShowMenu(Event *ev);
    void      HideMenu(Event *ev);
    void      ForceShow(void);
    void      ForceHide(void);
    UIWidget *GetContainerWidget(void);
    UIWidget *GetNamedWidget(const char *pszName);
    void      Update(void);
    void      RealignWidgets(void);
    float     GetMaxMotionTime(void);
    void      ActivateMenu(void);
    qboolean  isFullscreen(void);
    void      setFullscreen(qboolean bFullScreen);
    int       getVidMode(void);
    void      setVidMode(int iMode);
    void      setPushCommand(const str &cmd);
    void      setPopCommand(const str &cmd);
    qboolean  isNoEscape(void);
    void      NotifyPushMenu(void);
    void      NotifyPopMenu(void);
    qboolean  isVisible(void);
    void      SaveCVars(void);
    void      RestoreCVars(void);
    void      ResetCVars(void);
    void      PassEventToWidget(str name, Event *ev);
    void      PassEventToAllWidgets(Event& ev);
    void      CheckRestart(void);
    bool      CheckNoMouse(void);
    bool      allowGameFocus(void);
};

class MenuManager : public Listener
{
    Container<Menu *> m_menulist;
    Stack<Menu *>     m_showmenustack;
    Stack<Menu *>     m_menustack;
    bool              m_lock;
    bool              m_loading = false;

public:
    CLASS_PROTOTYPE(MenuManager);

public:
    MenuManager();

    void  RealignMenus(void);
    void  ChangeResolution(void);
    void  AddMenu(Menu *m);
    void  DeleteMenu(Menu *m);
    void  DeleteAllMenus(void);
    Menu *FindMenu(str name);
    bool  PushMenu(str name);
    bool  TransitionMenu(str name);
    bool  ShowMenu(str name);
    bool  HideMenu(str name);
    void  HideAllMenus(void);
    void  PushMenu(Event *ev);
    void  PopMenu(qboolean restore_cvars);
    Menu *CurrentMenu(void);
    bool  ClearMenus(bool force);
    void  ListMenus(void);
    void  UpdateAllMenus(void);
    void  reloadStrings(void);
    void  Lock(Event *ev);
    void  Unlock(Event *ev);
    void  PassEventToWidget(str name, Event *ev);
    void  PassEventToAllWidgets(Event& ev);
    void  ResetCVars(void);
    void  CheckRestart(void);
    bool  ForceMenu(str name);
    void  setLoading(bool loading) { m_loading = loading; }
    bool  getLoading(void) const { return m_loading; }
    bool  getLock(void) const { return m_lock; }
};

extern MenuManager menuManager;

extern Event EV_PushMenu;
extern Event EV_LockMenus;
extern Event EV_UnlockMenus;
extern Event EV_ShowMenu;
extern Event EV_HideMenu;
