
#pragma once

class Binding
{
public:
    str binding;
    int key1;
    int key2;
};

class UIWindowManager : public UIWidget
{
    unsigned int                  m_lastbuttons;
    UIPoint2D                     m_oldpos;
    SafePtr<UIWidget>             m_oldview;
    SafePtr<UIWidget>             m_firstResponder;
    class UIWidget               *m_activeControl;
    class UIWidget               *m_backgroundwidget;
    class UIReggedMaterial       *m_cursor;
    str                           m_cursorname;
    bool                          m_showcursor;
    Container<UIReggedMaterial *> m_materiallist;
    Container<UIReggedModel *>    m_modellist;
    bool                          m_amidead;
    class UIWidget               *m_bindactive;
    Container<Binding *>          bindings;

public:
    void  ChangeResolution(void) override;

    CLASS_PROTOTYPE(UIWindowManager);

private:
    void      ViewEvent(UIWidget *view, Event& event, UIPoint2D& pos, int buttons);
    UIWidget *getResponder(UIPoint2D& pos);

public:
    UIWindowManager();
    ~UIWindowManager();

    void              Init(const UIRect2D& frame, const char *fontname);
    void              setBackgroundWidget(UIWidget *widget);
    void              UpdateViews(void);
    void              ServiceEvents(void);
    void              setFirstResponder(UIWidget *responder);
    UIWidget         *getFirstResponder(void);
    UIWidget         *ActiveControl(void);
    void              ActivateControl(UIWidget *control);
    void              DeactivateCurrentControl(void);
    UIWidget         *FindNextControl(UIWidget *control);
    UIWidget         *FindPrevControl(UIWidget *control);
    void              setCursor(const char *string);
    void              showCursor(bool show);
    virtual void      CharEvent(int ch) override;
    virtual qboolean  KeyEvent(int key, unsigned int time) override;
    void              CreateMenus(void);
    void              sortWidgetContainers(void);
    // PAINT TEMP PROBE accessors -- PORT-ONLY, TEMPORARY, NEVER MERGE.  The paint-order
    int               GetChildCount(void);
    UIWidget         *GetChild(int index);
    UIReggedMaterial *RegisterShader(const str& name);
    UIReggedMaterial *RefreshShader(const str& name);
    void              CleanupShadersFromList(void);
    void              RefreshShadersFromList(void);
    UIReggedModel    *RegisterModel(const str& name);
    void              RegisterModelsFromList(void);
    void              DeactivateCurrentSmart(void);
    bool              IsDead(void);
    int               AddBinding(const str& binding);  
    str               GetKeyStringForCommand(str command, int index, qboolean alternate, int *key1, int *key2);
    void              BindKeyToCommand(str command, int key, int index, qboolean alternate);
    UIWidget         *BindActive(void);
    void              SetBindActive(UIWidget *w);
    virtual void      Shutdown(void) override;
    bool              DialogExists(void);
    void              RemoveAllDialogBoxes(void);
};

extern Event W_MouseExited;
extern Event W_MouseEntered;
extern Event W_LeftMouseDragged;
extern Event W_RightMouseDragged;
extern Event W_CenterMouseDragged;
extern Event W_MouseMoved;
extern Event W_LeftMouseDown;
extern Event W_LeftMouseUp;
extern Event W_RightMouseDown;
extern Event W_RightMouseUp;
extern Event W_CenterMouseDown;
extern Event W_CenterMouseUp;
extern Event W_AnyMouseDown;
extern Event W_AnyMouseUp;

extern UIWindowManager uWinMan;
