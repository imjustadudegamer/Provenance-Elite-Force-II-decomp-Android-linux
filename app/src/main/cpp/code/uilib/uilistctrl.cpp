
#include "ui_local.h"

static UISize2D s_columnpadding(2.0, 2.0);

bool              UIListCtrl::s_qsortreverse;
int               UIListCtrl::s_qsortcolumn;
class UIListCtrl *UIListCtrl::s_qsortobject;

CLASS_DECLARATION(UIListBase, UIListCtrl, NULL) {
    {&W_SizeChanged,      &UIListCtrl::OnSizeChanged},
    {&W_LeftMouseDown,    &UIListCtrl::MousePressed },
    {&W_LeftMouseUp,      &UIListCtrl::MouseReleased},
    {&W_LeftMouseDragged, &UIListCtrl::MouseDragged },
    {&W_MouseEntered,     &UIListCtrl::MouseEntered },
    {NULL,                NULL                      }
};

UIListCtrl::UIListCtrl()
{
    m_clickState.point.x = m_clickState.point.y = 0;
    m_sizestate.column                          = 0;
    m_iLastSortColumn                           = 0;
    m_bDrawHeader                               = qtrue;
    m_headerfont                                = NULL;

    Connect(this, W_SizeChanged, W_SizeChanged);
}

UIListCtrl::~UIListCtrl()
{
    if (m_headerfont) {
        delete m_headerfont;
        m_headerfont = NULL;
    }
}

int UIListCtrl::StringCompareFunction(const UIListCtrlItem *i1, const UIListCtrlItem *i2, int columnname)
{
    // being sorted into a group order retail does not produce.
    if (i1->getListItemType(columnname) != TYPE_STRING || i2->getListItemType(columnname) != TYPE_STRING) {
        return 0;
    }

    str s1 = i1->getListItemString(columnname);
    str s2 = i2->getListItemString(columnname);

    return Q_stricmp(s1, s2);
}

int UIListCtrl::StringNumberCompareFunction(const UIListCtrlItem *i1, const UIListCtrlItem *i2, int columnname)
{
    int val1, val2;

    val1 = i1->getListItemValue(columnname);
    val2 = i2->getListItemValue(columnname);

    if (val1 > val2) {
        return 1;
    } else if (val1 < val2) {
        return -1;
    } else {
        return 0;
    }
}

int UIListCtrl::QsortCompare(const void *e1, const void *e2)
{
    int val =
        s_qsortobject->m_comparefunction(*(const UIListCtrlItem **)e1, *(const UIListCtrlItem **)e2, s_qsortcolumn);
    return s_qsortreverse ? -val : val;
}

void UIListCtrl::Draw(void)
{
    UIWidget::Draw();

    if (m_bDrawHeader) {
        DrawColumns();
    }

    DrawContent();
}

int UIListCtrl::getHeaderHeight(void)
{
    if (!m_bDrawHeader) {
        return 0;
    }

    if (m_headerfont) {
        return (int)(m_headerfont->getHeight(false) + (s_columnpadding.height + s_columnpadding.height));
    }

    if (m_font) {
        return (int)(m_font->getHeight(false) + (s_columnpadding.height + s_columnpadding.height));
    }

    return (int)((s_columnpadding.height + s_columnpadding.height));
}

void UIListCtrl::MousePressed(Event *ev)
{
    UIPoint2D p;

    p = MouseEventToClientPoint(ev);

    if (m_clicksound.length()) {
        uii.Snd_PlaySound(m_clicksound.c_str(), CHAN_MENU2);  
    }

    if (getHeaderHeight() > p.y) {
        int closesep    = 0;
        int atleft      = 0;
        int closeatleft = 0;
        int insep       = 0;

        for (int i = 1; i <= m_columnlist.NumObjects(); i++) {
            const columndef_t& column = m_columnlist.ObjectAt(i);
            atleft += column.width;

            if (abs(atleft - p.x) > 2) {
                if (atleft - column.width < p.x && atleft > p.x) {
                    insep = i;
                }
            } else {
                closesep    = i;
                closeatleft = atleft;
            }
        }

        if (closesep) {
            m_sizestate.column = closesep;
            m_sizestate.min    = closeatleft - m_columnlist.ObjectAt(closesep).width;
            uWinMan.setFirstResponder(this);
        } else if (insep) {
            SortByColumn(m_columnlist.ObjectAt(insep).name);
        }
    } else {
        if (m_vertscroll) {
            TrySelectItem(
                (m_vertscroll->getTopItem() + 1) + (p.y - getHeaderHeight()) / m_font->getHeight(false)
            );
        } else {
            TrySelectItem(1.0 + (p.y - getHeaderHeight()) / m_font->getHeight(false));
        }

        if (m_clickState.time + 500 > uid.time && m_currentItem == m_clickState.selected
            && fabs(m_clickState.point.x - p.x) <= 2.0 && fabs(m_clickState.point.y - p.y) <= 2.0) {
            Event newev(EV_UIListBase_ItemDoubleClicked);
            newev.AddInteger(m_currentItem);
            SendSignal(newev);
            m_clickState.time = 0;
        } else {
            m_clickState.time     = uid.time;
            m_clickState.selected = m_currentItem;
            m_clickState.point.x  = p.x;
            m_clickState.point.y  = p.y;
        }
    }
}

void UIListCtrl::MouseDragged(Event *ev)
{
    UIPoint2D p;
    int       newwidth;

    p = MouseEventToClientPoint(ev);
    if (m_sizestate.column) {
        columndef_t& column = m_columnlist.ObjectAt(m_sizestate.column);

        newwidth = p.x - m_sizestate.min;
        if (newwidth > 600) {
            newwidth = 600;
        } else if (newwidth < 0) {
            newwidth = 0;
        }

        column.width = newwidth;
    }
}

void UIListCtrl::MouseReleased(Event *ev)
{
    m_sizestate.column = 0;

    if (uWinMan.getFirstResponder() == this) {
        uWinMan.setFirstResponder(NULL);
    }
}

void UIListCtrl::MouseEntered(Event *ev)
{
    uWinMan.ActivateControl(this);
    ExecuteHoverCommand();
}

void UIListCtrl::OnSizeChanged(Event *ev)
{
    if (!m_vertscroll) {
        return;
    }

    m_vertscroll->InitFrame(this, m_frame.size.width - 16.0, 0.0, 16.0, m_frame.size.height, -1);
    m_vertscroll->setPageHeight((m_frame.size.height - getHeaderHeight()) / m_font->getHeight(false));
    m_vertscroll->setNumItems(m_itemlist.NumObjects());
}

void UIListCtrl::DrawColumns(void)
{
    int     atleft;
    int     i;
    int     height;
    UColor  columnColor, textColor;

    atleft      = 0;
    columnColor = UColor(0.3f, 0.3f, 0.3f, m_local_alpha);
    textColor   = UWhite;
    textColor.a = m_local_alpha;

    height = (int)(2.0f * s_columnpadding.height + m_font->getHeight(false));
    m_font->setColor(columnColor);

    for (i = 1; i <= m_columnlist.NumObjects(); i++) {
        const columndef_t& column = m_columnlist.ObjectAt(i);

        DrawBoxWithSolidBorder(
            UIRect2D((float)atleft, 0.0f, (float)column.width + 1.0f, (float)height),
            columnColor,
            textColor,
            1,
            3,
            m_local_alpha
        );

        m_font->Print(
            (float)atleft + s_columnpadding.width,
            s_columnpadding.height,
            column.title.c_str(),
            -1, UClear, false);

        atleft += column.width;
    }

    if (m_frame.size.width > atleft) {
        DrawBoxWithSolidBorder(
            UIRect2D((float)atleft, 0.0f, m_frame.size.width - atleft + 1.0f, (float)height), columnColor, textColor, 1, 3, m_local_alpha
        );
    }
}

void UIListCtrl::DrawContent(void)
{
    int     item;
    int     height, iItemHeight;
    UIFont *pFont;
    int     top;
    UColor  selColor, selText;
    UColor  backColor, textColor;

    height    = m_font->getHeight(false);
    selColor  = UColor(0.21f, 0.18f, 0.015f, 1.0f);
    selText   = UColor(0.9f, 0.8f, 0.6f, 1.0f);
    backColor = m_background_color;
    textColor = m_foreground_color;

    selColor.a  = m_local_alpha;
    selText.a   = m_local_alpha;
    backColor.a = m_local_alpha;
    textColor.a = m_local_alpha;
    top         = getHeaderHeight();
    if (m_vertscroll) {
        item = m_vertscroll->getTopItem() + 1;
    } else {
        item = 1;
    }

    for (; item <= m_itemlist.NumObjects(); item++) {
        UIListCtrlItem *theitem;
        int             col;
        int             atleft;
        UColor         *itemBg;
        UColor         *itemText;

        theitem = m_itemlist.ObjectAt(item);
        atleft  = 0;

        if (item == m_currentItem) {
            itemBg   = &selColor;
            itemText = &selText;
        } else {
            itemBg   = &backColor;
            itemText = &textColor;
        }

        pFont       = m_font;
        iItemHeight = height;

        pFont->setColor(*itemText);
        for (col = 1; col <= m_columnlist.NumObjects(); col++) {
            columndef_t&   column = m_columnlist.ObjectAt(col);
            UIRect2D       drawRect(atleft, top, column.width, iItemHeight);
            griditemtype_t itemtype = theitem->getListItemType(column.name);

            switch (itemtype) {
            case TYPE_STRING:
                DrawBox(drawRect, *itemBg, m_local_alpha);
                pFont->Print(
                    drawRect.pos.x + 1.0,
                    drawRect.pos.y,
                    theitem->getListItemString(column.name),
                    -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);
                break;
            case TYPE_OWNERDRAW:
                theitem->DrawListItem(column.name, drawRect, item == m_currentItem);
                break;
            }

            atleft += column.width;
        }

        if (m_frame.size.width > atleft) {
            DrawBox(atleft, top, m_frame.size.width - atleft, height, *itemBg, m_local_alpha);
        }

        top += iItemHeight;
        if (iItemHeight + top > m_frame.size.height) {
            break;
        }
    }
}

void UIListCtrl::FrameInitialized(void)
{
    UIListBase::FrameInitialized();
    OnSizeChanged(NULL);
}

void UIListCtrl::SetDrawHeader(qboolean bDrawHeader)
{
    m_bDrawHeader = bDrawHeader;
}

void UIListCtrl::AddItem(UIListCtrlItem *item)
{
    m_itemlist.AddObject(item);
    if (m_vertscroll) {
        m_vertscroll->setNumItems(m_itemlist.NumObjects());
    }
}

void UIListCtrl::InsertItem(UIListCtrlItem *item, int where)
{
    if (where > 0 && where <= m_itemlist.NumObjects()) {
        int i;

        m_itemlist.AddObject(NULL);

        for (i = m_itemlist.NumObjects(); i > where; i++) {
            m_itemlist.SetObjectAt(i, m_itemlist.ObjectAt(i - 1));
        }

        m_itemlist.SetObjectAt(where, item);
    } else {
        m_itemlist.AddObject(item);
    }

    if (m_vertscroll) {
        m_vertscroll->setNumItems(m_itemlist.NumObjects());
    }
}

int UIListCtrl::FindItem(UIListCtrlItem *item)
{
    return m_itemlist.IndexOfObject(item);
}

UIListCtrlItem *UIListCtrl::GetItem(int item)
{
    return m_itemlist.ObjectAt(item);
}

void UIListCtrl::AddColumn(str title, int name, int width, bool numeric, bool reverse_sort)
{
    columndef_t column;

    column.title        = title;
    column.name         = name;
    column.width        = width;
    column.numeric      = numeric;
    column.reverse_sort = reverse_sort;

    m_columnlist.AddObject(column);
}

void UIListCtrl::RemoveAllColumns(void)
{
    m_columnlist.ClearObjectList();
}

int UIListCtrl::getNumItems(void)
{
    return m_itemlist.NumObjects();
}

void UIListCtrl::DeleteAllItems(void)
{
    m_itemlist.ClearObjectList();
    m_currentItem = 0;

    if (m_vertscroll) {
        m_vertscroll->setNumItems(0);
        m_vertscroll->setTopItem(0);
    }
}

void UIListCtrl::DeleteItem(int which)
{
    m_itemlist.RemoveObjectAt(which);

    if (m_vertscroll) {
        m_vertscroll->setNumItems(m_itemlist.NumObjects());
    }

    if (m_currentItem > getNumItems()) {
        TrySelectItem(getNumItems());
    }
}

void UIListCtrl::SortByColumn(int column)
{
    int             i;
    bool            exists;
    bool            numeric;
    bool            reverse;
    UIListCtrlItem *selected;
    bool            selvisible;

    exists     = 0;
    numeric    = 0;
    reverse    = 0;
    selected   = NULL;
    selvisible = 0;

    for (i = 1; i <= m_columnlist.NumObjects(); i++) {
        columndef_t& def = m_columnlist.ObjectAt(i);

        if (def.name == column) {
            numeric = def.numeric;
            reverse = def.reverse_sort;
            exists  = true;
            break;
        }
    }

    if (exists) {
        m_iLastSortColumn = column;
        s_qsortcolumn     = column;
        s_qsortobject     = this;
        s_qsortreverse    = reverse;

        if (m_currentItem) {
            selected = m_itemlist.ObjectAt(m_currentItem);
            if (m_vertscroll) {
                selvisible = false;
                if (m_currentItem >= m_vertscroll->getTopItem() + 1) {
                    selvisible = m_currentItem <= m_vertscroll->getPageHeight() + m_vertscroll->getTopItem();
                }
            } else {
                selvisible = false;
            }
        }

        if (numeric) {
            m_comparefunction = &UIListCtrl::StringNumberCompareFunction;
        } else {
            m_comparefunction = &UIListCtrl::StringCompareFunction;
        }

        if (m_itemlist.NumObjects()) {
            m_itemlist.Sort(&UIListCtrl::QsortCompare);
        }

        if (selected) {
            if (selvisible) {
                TrySelectItem(FindItem(selected));
            } else {
                m_currentItem = FindItem(selected);
            }
        }
    } else {
        m_iLastSortColumn = 0;
    }
}

void UIListCtrl::SortByLastSortColumn(void)
{
    SortByColumn(m_iLastSortColumn);
}

void UIListCtrl::setCompareFunction(int (*func)(const UIListCtrlItem *i1, const UIListCtrlItem *i2, int columnname))
{
    m_comparefunction = func;
}

void UIListCtrl::setHeaderFont(const char *name)
{
    if (m_headerfont) {
        delete m_headerfont;
    }

    if (name) {
        m_headerfont = new UIFont(name);
    } else {
        m_headerfont = NULL;
    }
}
