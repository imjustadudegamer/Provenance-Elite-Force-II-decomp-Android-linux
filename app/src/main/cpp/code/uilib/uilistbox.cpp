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
#include "../qcommon/stringresource.h"
#include <string.h>

CLASS_DECLARATION(UIWidget, UIListBase, NULL) {
    {NULL, NULL}
};

UIListBase::UIListBase()
{
    m_currentItem    = 0;
    m_vertscroll     = 0;
    m_bUseVertScroll = true;
    AllowActivate(true);
}

void UIListBase::TrySelectItem(int which)
{
    int numitems;

    numitems = getNumItems();
    if (!numitems) {
        m_currentItem = 0;
        return;
    }

    if (m_currentItem != which) {
        int previous = m_currentItem;

        m_currentItem = which;
        if (which > numitems) {
            m_currentItem = numitems;
        }

        if (m_currentItem <= 0) {
            m_currentItem = 1;
        }

        if (previous != m_currentItem) {
            Event ev(EV_UIListBase_ItemSelected);
            ev.AddInteger(m_currentItem);
            SendSignal(ev);
        }
    }

    if (m_vertscroll) {
        if (m_currentItem < m_vertscroll->getTopItem() + 1) {
            if (m_currentItem - 1 >= 0) {
                m_vertscroll->setTopItem(m_currentItem - 1);
            } else {
                m_vertscroll->setTopItem(0);
            }
        }

        if (m_currentItem >= m_vertscroll->getTopItem() + m_vertscroll->getPageHeight() + 1) {
            if (m_currentItem - m_vertscroll->getPageHeight() >= 0) {
                m_vertscroll->setTopItem(m_currentItem - m_vertscroll->getPageHeight());
            } else {
                m_vertscroll->setTopItem(0);
            }
        }
    }
}

qboolean UIListBase::KeyEvent(int key, unsigned int time)
{
    int      offsetitem;
    qboolean key_rec;
    int      itemindex;

    offsetitem = 0;
    key_rec    = qfalse;

    switch (key) {
    case K_UPARROW:
    case K_LEFTARROW:
        offsetitem = -1;
        key_rec    = qtrue;
        break;
    case K_DOWNARROW:
    case K_RIGHTARROW:
        offsetitem = 1;
        key_rec    = qtrue;
        break;
    case K_PGDN:
        if (GetScrollBar()) {
            itemindex = GetScrollBar()->getTopItem() + GetScrollBar()->getPageHeight();
            if (getCurrentItem() == itemindex) {
                offsetitem = GetScrollBar()->getPageHeight();
            } else {
                TrySelectItem(itemindex);
            }
        }
        key_rec = qtrue;
        break;
    case K_PGUP:
        if (GetScrollBar()) {
            if (getCurrentItem() - 1 == GetScrollBar()->getTopItem()) {
                offsetitem = -GetScrollBar()->getPageHeight();
            } else {
                TrySelectItem(GetScrollBar()->getTopItem() + 1);
            }
        }
        key_rec = qtrue;
        break;
    case K_HOME:
        TrySelectItem(1);
        key_rec = qtrue;
        break;
    case K_END:
        TrySelectItem(getNumItems());
        key_rec = qtrue;
        break;
    case K_MWHEELDOWN:
        if (GetScrollBar()) {
            GetScrollBar()->AttemptScrollTo(GetScrollBar()->getTopItem() + 2);
        }
        key_rec = qtrue;
        break;
    case K_MWHEELUP:
        if (GetScrollBar()) {
            GetScrollBar()->AttemptScrollTo(GetScrollBar()->getTopItem() - 2);
        }
        key_rec = qtrue;
        break;
    default:
        break;
    }

    if (offsetitem) {
        TrySelectItem(getCurrentItem() + offsetitem);
    }

    return key_rec;
}

void UIListBase::FrameInitialized(void)
{
    if (m_vertscroll) {
        delete m_vertscroll;
    }

    m_vertscroll = new UIVertScroll();
    m_vertscroll->InitFrame(this, m_frame.size.width - 16.0, 0.0, 16.0, m_frame.size.height, -1);
    m_vertscroll->setTopItem(0);
}

int UIListBase::getCurrentItem(void)
{
    return m_currentItem;
}

int UIListBase::getNumItems(void)
{
    return -1;
}

void UIListBase::DeleteAllItems(void) {}

void UIListBase::DeleteItem(int which) {}

UIVertScroll *UIListBase::GetScrollBar(void)
{
    return m_vertscroll;
}

void UIListBase::SetUseScrollBar(qboolean bUse)
{
    m_bUseVertScroll = bUse;

    if (bUse) {
        if (!m_vertscroll) {
            m_vertscroll = new UIVertScroll();
        }

        m_vertscroll->InitFrame(this, m_frame.size.width - 16.0, 0.0, 16.0, m_frame.size.height, -1);
    } else {
        if (m_vertscroll) {
            delete m_vertscroll;
            m_vertscroll = NULL;
        }
    }
}

ListItem::ListItem()
{
    index = -1;
}

ListItem::ListItem(str newString, int newIndex, str newCommand)
{
    this->string  = newString;
    this->index   = newIndex;
    this->command = newCommand;
}

void UIListBox::reloadStrings(void)
{
    int i;

    UIWidget::reloadStrings();

    for (i = 1; i <= m_itemlist.NumObjects(); i++) {
        ListItem *item = m_itemlist.ObjectAt(i);
        if (item && item->raw.length()) {
            const char *res = getStringResource(item->raw.c_str());
            if (res) {
                item->string = res;
            } else {
                item->string = "";
            }
        }
    }
}

Event
    EV_UIListBase_ItemSelected("listbase_item_selected", EV_DEFAULT, "i", "index", "Signaled when an item is selected");
Event EV_UIListBase_ItemDoubleClicked(
    "listbase_item_doubleclicked", EV_DEFAULT, "i", "index", "Signaled when an item is double clicked"

);
Event EV_Layout_AddListItem("additem", EV_DEFAULT, "sS", "itemname command", "Add an item to the list");
Event EV_Layout_AddConfigstringListItem(
    "addconfigstringitem", EV_DEFAULT, "iS", "index command", "Add an item to the list that uses a configstring"

);
Event EV_UIListBox_DeleteAllItems("deleteallitems", EV_DEFAULT, NULL, NULL, "Delete all the items from the widget");
Event EV_UIListBox_UniqueItems("uniqueitems", EV_DEFAULT, NULL, NULL, "Makes sure items in list are unique");

CLASS_DECLARATION(UIListBase, UIListBox, NULL) {
    {&W_LeftMouseDown,                   &UIListBox::MousePressed                 },
    {&W_LeftMouseUp,                     &UIListBox::MouseReleased                },
    {&EV_Layout_AddListItem,             &UIListBox::LayoutAddListItem            },
    {&EV_Layout_AddConfigstringListItem, &UIListBox::LayoutAddConfigstringListItem},
    {&EV_UIListBox_UniqueItems,          &UIListBox::MakeItemsUnique              },
    {&EV_UIListBox_DeleteAllItems,       &UIListBox::DeleteAllItems               },
    {&EV_Layout_Font,                    &UIListBox::SetListFont                  },
    {NULL,                               NULL                                     }
};

UIListBox::UIListBox()
{
    m_clickState.point.x = 0.0;
    m_clickState.point.y = 0.0;
    m_bMakeUnique        = qfalse;
}

void UIListBox::Draw(void)
{
    UIWidget::Draw();

    float       aty;
    int         i;
    const char *str;
    UColor selectedBG(0.0f, 1.0f, 1.0f, 1.0f);
    UColor selectedColor(0.9, 0.8, 0.6);
    UColor selectedBorder(0.315, 0.27, 0.0225);

    aty = 0;

    if (m_vertscroll) {
        i = m_vertscroll->getTopItem() + 1;
    } else {
        i = 1;
    }

    while (1) {
        ListItem *li;
        float     rowHeight = (float)m_font->getHeight(false);

        if (aty >= m_frame.size.height || i > m_itemlist.NumObjects()) {
            break;
        }

        if (i == m_currentItem) {
            DrawBox(0, aty, m_frame.size.width - 16.0f, rowHeight, selectedBG, 1.f);
            m_font->setColor(selectedColor);
        } else {
            m_font->setColor(getForegroundColor());
        }

        li = m_itemlist.ObjectAt(i);
        if (li->index > 0) {
            str = uii.GetConfigstring(li->index);
        } else {
            str = li->string;
        }
        m_font->Print(m_indent, aty, str, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);

        if (i == m_currentItem) {
            DrawBoxWithSolidBorder(UIRect2D(0, aty, m_frame.size.width - 16.0f, rowHeight), selectedBorder, selectedBorder, 1, 2, 0.5f);
        }

        aty += rowHeight;
        i++;
    }
}

void UIListBox::MousePressed(Event *ev)
{
    UIPoint2D p;

    p.x = ev->GetFloat(1) - m_screenframe.pos.x;
    p.y = ev->GetFloat(2) - m_screenframe.pos.y;

    if (m_vertscroll) {
        TrySelectItem((m_vertscroll->getTopItem() + 1) + p.y / m_font->getHeight(false));
    } else {
        TrySelectItem(p.y / m_font->getHeight(false) + 1);
    }

    if (m_clickState.time + 500 > uid.time && m_currentItem == m_clickState.selected) {
        UIPoint2D p2;

        p2.x = m_clickState.point.x - p.x;
        if (fabs(p2.x) <= 2) {
            p2.y = m_clickState.point.y - p.y;
            if (fabs(p2.y) <= 2.f) {
                Event event(EV_UIListBase_ItemDoubleClicked);
                event.AddInteger(m_currentItem);
                SendSignal(event);

                ListItem *li = m_itemlist.ObjectAt(m_currentItem);

                if (!m_currentItem || !li) {
                    return;
                }

                if (m_commandhandler) {
                    (*m_commandhandler)(li->string, NULL);
                }

                if (li->command.length()) {
                    uii.Cmd_Stuff(va("%s \"%s\"\n", li->command.c_str(), li->string.c_str()));
                } else {
                    uii.Cmd_Stuff(va("%s \"%s\"\n", m_command.c_str(), li->string.c_str()));
                }
                m_clickState.time = 0;
                return;
            }
        }
    }

    m_clickState.time     = uid.time;
    m_clickState.selected = m_currentItem;
    m_clickState.point    = p;
}

void UIListBox::MouseReleased(Event *ev) {}

void UIListBox::DeleteAllItems(Event *ev)
{
    m_itemlist.ClearObjectList();
    m_currentItem = 0;

    if (m_vertscroll) {
        m_vertscroll->setNumItems(0);
        m_vertscroll->setTopItem(0);
    }
}

void UIListBox::SetListFont(Event *ev)
{
    LayoutFont(ev);

    if (m_vertscroll) {
        m_vertscroll->setPageHeight(m_frame.size.height / m_font->getHeight(false));
    }

    FrameInitialized();
}

void UIListBox::TrySelectItem(int which)
{
    UIListBase::TrySelectItem(which);

    if (!getNumItems()) {
        return;
    }

    if (m_cvarname.length()) {
        uii.Cvar_Set(m_cvarname.c_str(), m_itemlist.ObjectAt(m_currentItem)->string.c_str());
    }
}

qboolean UIListBox::CanAddItem(const char *item, const char *command)
{
    int i;

    if (!m_bMakeUnique) {
        return qtrue;
    }

    for (i = 1; i <= m_itemlist.NumObjects(); i++) {
        ListItem *li = m_itemlist.ObjectAt(i);

        if (command) {
            if (item && !strcmp(li->string.c_str(), item) && !strcmp(li->command.c_str(), command)) {
                return qfalse;
            }
        } else {
            if (item && !strcmp(li->string.c_str(), item) && !li->command.length()) {
                return qfalse;
            }
        }
    }

    return qtrue;
}

void UIListBox::MakeItemsUnique(Event *ev)
{
    m_bMakeUnique = qtrue;
}

void UIListBox::AddItem(const char *item, const char *command)
{
    ListItem *li;

    if (!CanAddItem(item, command)) {
        return;
    }

    li = new ListItem();
    if (!li) {
        uii.Sys_Error(ERR_DROP, "Couldn't create list item\n");
    }

    if (isStringResource(item)) {
        li->raw = item;
        li->string = getStringResource(item);
    } else {
        li->string = item;
    }

    if (command) {
        li->command = command;
    }

    m_itemlist.AddObject(li);

    if (!m_currentItem) {
        m_currentItem = 1;
    }

    if (m_vertscroll) {
        m_vertscroll->setNumItems(m_itemlist.NumObjects());
    }
}

void UIListBox::AddItem(int index, const char *command)
{
    ListItem *li;

    li = new ListItem();
    if (!li) {
        uii.Sys_Error(ERR_DROP, "Couldn't create list item\n");
    }

    li->index = index;
    if (command) {
        li->command = command;
    }

    m_itemlist.AddObject(li);

    if (!m_currentItem) {
        m_currentItem = 1;
    }

    if (m_vertscroll) {
        m_vertscroll->setNumItems(m_itemlist.NumObjects());
    }
}

void UIListBox::FrameInitialized(void)
{
    UIListBase::FrameInitialized();

    if (m_vertscroll) {
        m_vertscroll->setPageHeight(m_frame.size.height / m_font->getHeight(false));
        m_vertscroll->setNumItems(m_itemlist.NumObjects());
    }
}

void UIListBox::LayoutAddListItem(Event *ev)
{
    if (ev->NumArgs() != 2) {
        AddItem(ev->GetString(1), NULL);
        return;
    }

    AddItem(ev->GetString(1), ev->GetString(2));
}

void UIListBox::LayoutAddConfigstringListItem(Event *ev)
{
    if (ev->NumArgs() != 2) {
        AddItem(ev->GetInteger(1), NULL);
        return;
    }

    AddItem(ev->GetInteger(1), ev->GetString(2));
}

str UIListBox::getItemText(int which)
{
    return m_itemlist.ObjectAt(which)->string;
}

int UIListBox::getNumItems(void)
{
    return m_itemlist.NumObjects();
}

void UIListBox::DeleteAllItems(void)
{
    m_itemlist.ClearObjectList();
    m_currentItem = 0;

    if (m_vertscroll) {
        m_vertscroll->setNumItems(0);
        m_vertscroll->setTopItem(0);
    }
}

void UIListBox::DeleteItem(int which)
{
    m_itemlist.RemoveObjectAt(which);

    if (m_vertscroll) {
        m_vertscroll->setNumItems(m_itemlist.NumObjects());
    }

    if (m_currentItem > getNumItems()) {
        TrySelectItem(getNumItems());
    }
}

void UIListBox::InsertItem(const char *string, int which, const char *command)
{
    ListItem *item;
    int       i;

    item = new ListItem();
    if (!item) {
        Com_Error(ERR_DROP, "Couldn't create list item\n");
    }

    item->string = string;
    if (command) {
        item->command = command;
    }

    if (which >= 1 && which <= m_itemlist.NumObjects()) {
        m_itemlist.AddObject({});
        for (i = m_itemlist.NumObjects(); i > which; i--) {
            m_itemlist.SetObjectAt(i, m_itemlist.ObjectAt(i - 1));
        }

        m_itemlist.SetObjectAt(which, item);
    } else {
        m_itemlist.AddObject(item);
    }

    if (m_vertscroll) {
        m_vertscroll->setNumItems(m_itemlist.NumObjects());
    }
}

void UIListBox::setCurrentItem(int which)
{
    if (which < 1) {
        m_currentItem = 0;
    } else if (which > m_itemlist.NumObjects()) {
        m_currentItem = m_itemlist.NumObjects();
    } else {
        m_currentItem = which;
    }
}
