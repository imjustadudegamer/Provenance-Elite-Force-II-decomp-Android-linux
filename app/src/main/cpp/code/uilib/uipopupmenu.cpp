/*
===========================================================================
Copyright (C) 2015-2024 the OpenMoHAA team

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

// retail does NOT bind W_Deactivated and has no DismissEvent -- the port's binding
CLASS_DECLARATION(UIWidget, UIPopupMenu, NULL) {
    {&W_LeftMouseUp, &UIPopupMenu::MouseReleased},
    {&W_MouseMoved,  &UIPopupMenu::MouseMoved   },
    {&W_MouseExited, &UIPopupMenu::MouseExited  },
    {NULL,           NULL                       }
};

uipopup_type UI_PopupTypeStringToInt(str type)
{
    if (type == "event_string") {
        return UIP_EVENT_STRING;
    } else if (type == "command") {
        return UIP_CMD;
    } else if (type == "cvar") {
        return UIP_CVAR;
    } else if (type == "separator") {
        return UIP_SEPARATOR;
    }

    return UIP_NONE;
}

UIPopupMenu::UIPopupMenu()
{
    m_selected         = -1;
    m_listener         = NULL;
    m_parentMenu       = NULL;
    m_submenu          = -1;
    m_marlett          = UIFont("marlett");
    m_autodismiss      = qtrue;
    m_highlightFGColor = UColor(0.0f, 0.0f, 0.0f, 1.0f);
    m_highlightBGColor = UWhite;
}

UIPopupMenu::~UIPopupMenu()
{
    if (m_parentMenu) {
        m_parentMenu->m_submenu = -1;
    }

    if (m_submenu != -1 && m_submenuptr) {
        m_submenuptr->m_parentMenu = NULL;
        delete m_submenuptr;
    }
}

uipopup_describe *UIPopupMenu::getDescribeFromPoint(const UIPoint2D& p)
{
    if (p.y < 4.0f || p.x < 2.0f || m_frame.size.width - 2.0f <= p.x) {
        return NULL;
    }

    float top = 4.0f;
    for (int i = 1; i <= m_describe->NumObjects(); i++) {
        uipopup_describe *desc   = m_describe->ObjectAt(i);
        float             height = getDescribeHeight(desc);
        if (p.y >= top && p.y < top + height) {
            return desc;
        }

        top += height;
    }

    return NULL;
}

void UIPopupMenu::MakeSubMenu(void) {}

float UIPopupMenu::getDescribeHeight(uipopup_describe *d)
{
    if (d->type == UIP_SEPARATOR) {
        return 8.0f;
    }

    return m_font->getHeight(true);
}

float UIPopupMenu::getDescribeWidth(uipopup_describe *d)
{
    // (raw 0x7B2E4 `li r5, -1`, 0x7B2EC `li r6, 1`), not the widget's m_bVirtual.
    return m_font->getWidth(d->title, -1, true);
}


void UIPopupMenu::Create(
    Container<uipopup_describe *> *describe,
    Listener                      *listener,
    const UIRect2D&                createRect,
    uipopup_where                  where,
    float                          width
)
{
    float w;
    float h;
    int   i;

    (void)where;

    m_describe = describe;
    m_listener = listener;
    setFont(uWinMan.getFont()->getName().c_str());

    bool bComputed = (width == 0.0f);
    if (!bComputed) {
        m_width = width;
    }

    w = 0.0f;
    h = 0.0f;
    for (i = 1; i <= m_describe->NumObjects(); i++) {
        uipopup_describe *desc = m_describe->ObjectAt(i);

        h += getDescribeHeight(desc);
        if (desc->type != UIP_SEPARATOR && bComputed) {
            float newWidth = getDescribeWidth(desc);
            if (newWidth > w) {
                w = newWidth;
            }
        }
    }

    if (m_width > 0.0f) {
        w = m_width * uid.scaleRes[0];
    }

    if (w == 0.0f || h == 0.0f) {
        return;
    }

    if (!bComputed) {
        w += 24.0f;
        h += 8.0f;
    }

    InitFrame(m_parent, createRect.pos.x, createRect.pos.y, w, h, border_3D, uWinMan.getFont()->getName().c_str());

    m_hoversound = "";
    m_clicksound = "";
}

void UIPopupMenu::Draw(void)
{
    UIWidget::Draw();

    float top = 4.0f;

    for (int i = 1; i <= m_describe->NumObjects(); i++) {
        uipopup_describe *desc = m_describe->ObjectAt(i);

        if (desc->type == UIP_SEPARATOR) {
            DrawBox(8.0f, top + 4.0f, m_frame.size.width - 16.0f, 1.0f, m_foreground_color, 1.0f);
        } else {
            UColor textColor;

            m_font->setColor(m_foreground_color);
            m_marlett.setColor(m_foreground_color);

            if (m_selected == i || m_submenu == i) {
                DrawBox(
                    2.0f,
                    top,
                    m_frame.size.width - 4.0f,
                    m_font->getHeight(true),
                    m_highlightBGColor,
                    m_local_alpha
                );

                textColor = m_highlightFGColor;
            } else {
                textColor = m_foreground_color;
            }

            m_font->setColor(textColor);
            m_marlett.setColor(textColor);

            m_font->Print(4.0f, top, desc->title, -1, getTextShadowColor(), 1);

            if (desc->type == UIP_SUBMENU) {
                m_marlett.PrintJustified(
                    UIRect2D(0.0f, top, m_frame.size.width - 4.0f, getDescribeHeight(desc)),
                    FONT_JUSTHORZ_RIGHT,
                    FONT_JUSTVERT_CENTER,
                    "4",
                    getTextShadowColor(),
                    1
                );
            }
        }

        top += getDescribeHeight(desc);
    }
}

void UIPopupMenu::MouseReleased(Event *ev)
{
    // Retail MouseReleased__11UIPopupMenuFP5Event @0x0007AB00 (340 B) does NOT call
    if (m_selected == -1 || m_selected > m_describe->NumObjects()) {
        return;
    }

    uipopup_describe *desc = m_describe->ObjectAt(m_selected);
    if (!desc || desc->type == UIP_SEPARATOR || desc->type == UIP_SUBMENU) {
        return;
    }

    Listener *listener = m_listener;

    if (m_autodismiss) {
        Dismiss();
    }

    switch (desc->type) {
    case UIP_EVENT:
        listener->ProcessEvent(*(Event *)desc->data);
        break;
    case UIP_EVENT_STRING:
        listener->ProcessEvent(new Event((const char *)desc->data));
        break;
    case UIP_CMD:
    case UIP_CVAR:
        Cbuf_AddText((const char *)desc->data);
        break;
    default:
        return;
    }
}

void UIPopupMenu::MouseMoved(Event *ev)
{
    UIPoint2D         point    = MouseEventToClientPoint(ev);
    uipopup_describe *describe = getDescribeFromPoint(point);
    if (describe) {
        int index = m_describe->IndexOfObject(describe);
        setSelection(index);
    } else {
        setSelection(-1);
    }
}

void UIPopupMenu::MouseExited(Event *ev)
{
    MouseMoved(ev);

    UIWidget::MouseExited(ev);
}

void UIPopupMenu::Dismiss(void)
{
    if (m_parentMenu) {
        m_parentMenu->Dismiss();
    } else {
        delete this;
    }
}

void UIPopupMenu::setSelection(int sel)
{
    int maxSel = m_describe->NumObjects();
    if (sel > maxSel) {
        m_selected = maxSel;
        return;
    }

    int newSubMenu = -1;
    if (sel != -1) {
        uipopup_describe *desc = m_describe->ObjectAt(sel);
        if (desc && desc->type == UIP_SUBMENU) {
            newSubMenu = sel;
        }
    }

    m_selected = sel;

    if (newSubMenu == m_submenu) {
        return;
    }

    if (newSubMenu == -1) {
        if (m_submenuptr) {
            delete m_submenuptr;
        }

        m_submenu = -1;
    } else {
        if (m_submenu != -1 && m_submenuptr) {
            delete m_submenuptr;
        }

        m_submenu = newSubMenu;
        MakeSubMenu();
    }
}

