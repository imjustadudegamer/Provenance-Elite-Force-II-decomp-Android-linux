
#pragma once

extern Event EV_Layout_AddListItem;
extern Event EV_UIListBase_ItemDoubleClicked;
extern Event EV_UIListBase_ItemSelected;

class UIListBase : public UIWidget
{
protected:
    int                 m_currentItem;
    class UIVertScroll *m_vertscroll;
    qboolean            m_bUseVertScroll;

public:
    CLASS_PROTOTYPE(UIListBase);

protected:
    virtual void TrySelectItem(int which);
    qboolean     KeyEvent(int key, unsigned int time) override;

public:
    UIListBase();

    void          FrameInitialized(void) override;
    int           getCurrentItem(void);
    int           getNumItems(void) override;
    virtual void  DeleteAllItems(void);
    virtual void  DeleteItem(int which);
    UIVertScroll *GetScrollBar(void);
    void          SetUseScrollBar(qboolean bUse);
};
typedef class UIListBase UIListBase;

class ListItem : public Class
{
public:
    str string;
    str raw;
    str command;
    int index;

public:
    ListItem();
    ListItem(str string, int index, str command);
};

class UIListBox : public UIListBase
{
protected:
    Container<ListItem *> m_itemlist;

    struct {
        int       time;
        int       selected;
        UIPoint2D point;
    } m_clickState;

    qboolean m_bMakeUnique;

public:
    CLASS_PROTOTYPE(UIListBox);

protected:
    void Draw(void) override;
    void MousePressed(Event *ev);
    void MouseReleased(Event *ev);
    void DeleteAllItems(Event *ev);
    void SetListFont(Event *ev);
    void TrySelectItem(int which) override;
    void MakeItemsUnique(Event *ev);

public:
    UIListBox();

    void     AddItem(const char *item, const char *command);
    qboolean CanAddItem(const char *item, const char *command);
    void AddItem(int index, const char *command);
    void FrameInitialized(void) override;
    void LayoutAddListItem(Event *ev);
    void LayoutAddConfigstringListItem(Event *ev);
    str  getItemText(int which);
    int  getNumItems(void) override;
    void DeleteAllItems(void) override;
    void DeleteItem(int which) override;
    void InsertItem(const char *string, int which, const char *command = NULL);
    void setCurrentItem(int which);
    void reloadStrings(void) override;
};
