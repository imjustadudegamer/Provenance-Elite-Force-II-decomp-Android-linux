
#pragma once

typedef enum {
    TYPE_STRING,
    TYPE_OWNERDRAW
} griditemtype_t;

class UIListCtrlItem
{
public:
    virtual griditemtype_t getListItemType(int which) const                                                   = 0;
    virtual str            getListItemString(int which) const                                                 = 0;
    virtual int            getListItemValue(int which) const                                                  = 0;
    virtual void           DrawListItem(int iColumn, const UIRect2D& drawRect, bool bSelected)                = 0;
};

typedef struct m_clickState_s {
    int       time;
    int       selected;
    UIPoint2D point;
} m_clickState_t;

class UIListCtrl : public UIListBase
{
public:
    struct columndef_t {
        str  title;
        int  name;
        int  width;
        bool numeric;
        bool reverse_sort;
    };

protected:
    static bool              s_qsortreverse;
    static int               s_qsortcolumn;
    static class UIListCtrl *s_qsortobject;

    int                                m_iLastSortColumn;
    class UIFont                      *m_headerfont;
    Container<UIListCtrlItem *>        m_itemlist;
    Container<UIListCtrl::columndef_t> m_columnlist;

    qboolean m_bDrawHeader;

    struct {
        int column;
        int min;
    } m_sizestate;

    m_clickState_s m_clickState;

    int (*m_comparefunction)(const UIListCtrlItem *i1, const UIListCtrlItem *i2, int columnname);

public:
    CLASS_PROTOTYPE(UIListCtrl);

protected:
    static int StringCompareFunction(const UIListCtrlItem *i1, const UIListCtrlItem *i2, int columnname);
    static int StringNumberCompareFunction(const UIListCtrlItem *i1, const UIListCtrlItem *i2, int columnname);
    static int QsortCompare(const void *e1, const void *e2);
    void       Draw(void) override;
    int        getHeaderHeight(void);
    void       MousePressed(Event *ev);
    void       MouseDragged(Event *ev);
    void       MouseReleased(Event *ev);
    void       MouseEntered(Event *ev) override;
    void       OnSizeChanged(Event *ev);
    void       DrawColumns(void);
    void       DrawContent(void);

public:
    UIListCtrl();
    ~UIListCtrl();

    void            FrameInitialized(void) override;
    void            SetDrawHeader(qboolean bDrawHeader);
    void            AddItem(UIListCtrlItem *item);
    void            InsertItem(UIListCtrlItem *item, int where);
    int             FindItem(UIListCtrlItem *item);
    UIListCtrlItem *GetItem(int item);
    void            AddColumn(str title, int name, int width, bool numeric, bool reverse_sort);
    void            RemoveAllColumns(void);
    int             getNumItems(void) override;
    void            DeleteAllItems(void) override;
    void            DeleteItem(int which) override;
    virtual void    SortByColumn(int column);
    void            SortByLastSortColumn(void);
    void            setCompareFunction(int (*func)(const UIListCtrlItem *i1, const UIListCtrlItem *i2, int columnname));
    void            setHeaderFont(const char *name);
};
