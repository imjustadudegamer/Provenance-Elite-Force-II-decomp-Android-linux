/*
===========================================================================
Copyright (C) 2023 the OpenMoHAA team

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

CLASS_DECLARATION(UIWidget, UIList, NULL) {
    {&W_LeftMouseDown,       &UIList::Pressed          },
    {&W_LeftMouseUp,         &UIList::Released         },
    {&EV_Layout_AddListItem, &UIList::LayoutAddListItem},
    {NULL,                   NULL                      }
};

UIList::UIList()
{
    m_arrow_width          = 15.0;
    m_currentItem          = 0;
    m_next_arrow_region    = 0;
    m_prev_arrow_region    = 0;
    m_indent               = 0.0f;
    m_prev_arrow_depressed = 0;
    m_next_arrow_depressed = 0;

    AllowActivate(true);

    m_prev_arrow = uWinMan.RegisterShader("sysimg/2d/arrow_left.tga");
    if (!m_prev_arrow) {
        uii.Sys_WPrintf("UIList::Ulist : Could not register shader sysimg/2d/arrow_left.tga");
    }

    m_next_arrow = uWinMan.RegisterShader("sysimg/2d/arrow_right.tga");
    if (!m_next_arrow) {
        uii.Sys_WPrintf("UIList::Ulist : Could not register shader sysimg/2d/arrow_right.tga");
    }
}

UIList::~UIList()
{
    for (int i = m_itemlist.NumObjects(); i > 0; i--) {
        UIListItem *item = m_itemlist.ObjectAt(i);
        m_itemlist.RemoveObjectAt(i);
        delete item;
    }

    if (m_prev_arrow_region) {
        delete m_prev_arrow_region;
    }
    if (m_next_arrow_region) {
        delete m_next_arrow_region;
    }
}

void UIList::Draw(void)
{
    UIWidget::Draw();

    UpdateUIElement();

    if (m_currentItem) {
        str         itemText;
        UIListItem *item = m_itemlist.ObjectAt(m_currentItem);
        // Retail UIList::Draw @0x00065C90 tests the two str LENGTHS, not the
        if (item->itemalias.length()) {
            itemText = item->itemalias;
        } else if (item->itemname.length()) {
            itemText = item->itemname;
        }

        float textX = 0.5f * m_frame.size.width - 0.5f * m_font->getWidth(itemText, -1, 0);
        float textY =
            0.5f * (m_frame.size.height - m_font->getHeight(false)) - 1.0f;
        m_font->setColor(m_foreground_color);
        const char *text = itemText;
        m_font->Print(textX, textY, text, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);
    }

    Draw3DBox(0.0f, 0.0f, m_arrow_width, m_frame.size.height, m_prev_arrow_depressed, m_border_color, m_local_alpha);

    float height = m_frame.size.height - 4.0;
    float width  = m_arrow_width - 4.0;
    uii.Rend_DrawPicStretched(2.0f, 2.0f, width, height, 0.0f, 0.0f, 1.0f, 1.0f, m_prev_arrow->GetMaterial());

    float x = m_frame.size.width - m_arrow_width;
    float y = 0.0f;
    Draw3DBox(x, y, m_arrow_width, m_frame.size.height, m_next_arrow_depressed, m_border_color, m_local_alpha);

    uii.Rend_DrawPicStretched(x + 2.0f, y + 2.0f, width, height, 0.0f, 0.0f, 1.0f, 1.0f, m_next_arrow->GetMaterial());
}

qboolean UIList::KeyEvent(int key, unsigned int time)
{
    switch (key) {
    case K_RIGHTARROW:
        ScrollNext();
        UpdateData();
        if (m_commandhandler) {
            m_commandhandler(m_itemlist.ObjectAt(m_currentItem)->itemname.c_str(), NULL);
        }
        return qtrue;
    case K_LEFTARROW:
        ScrollPrev();
        UpdateData();
        if (m_commandhandler) {
            m_commandhandler(m_itemlist.ObjectAt(m_currentItem)->itemname.c_str(), NULL);
        }
        return qtrue;
    default:
        return qfalse;
    }
}

void UIList::CharEvent(int ch) {}

void UIList::Pressed(Event *ev)
{
    m_held      = qtrue;
    m_depressed = qtrue;

    float mouse_xpos = ev->GetFloat(1);
    float mouse_ypos = ev->GetFloat(2);
    ev->GetInteger(3); 

    FrameInitialized();

    if (m_prev_arrow_region->contains(mouse_xpos, mouse_ypos)) {
        m_prev_arrow_depressed = qtrue;
        ScrollPrev();
        UpdateData();
        return;
    }

    if (m_next_arrow_region->contains(mouse_xpos, mouse_ypos)) {
        m_next_arrow_depressed = qtrue;
        ScrollNext();
        UpdateData();
    }
}

void UIList::Released(Event *ev)
{
    m_held                 = qfalse;
    m_depressed            = qfalse;
    m_prev_arrow_depressed = qfalse;
    m_next_arrow_depressed = qfalse;
}

void UIList::ScrollNext(void)
{
    m_currentItem++;
    if (m_currentItem > m_itemlist.NumObjects()) {
        m_currentItem = m_itemlist.NumObjects();
    }
}

void UIList::ScrollPrev(void)
{
    m_currentItem--;
    if (m_currentItem < 1) {
        m_currentItem = 1;
    }
}

void UIList::FrameInitialized(void)
{
    m_prev_arrow_region = new UIRect2D(m_clippedframe.pos.x, m_clippedframe.pos.y, m_arrow_width, m_frame.size.height);
    m_next_arrow_region = new UIRect2D(
        m_clippedframe.size.width - m_arrow_width + m_clippedframe.pos.x,
        m_clippedframe.pos.y,
        m_arrow_width,
        m_frame.size.height
    );
}

void UIList::LayoutAddListItem(Event *ev)
{
    str item = ev->GetString(1);
    str alias;
    if (ev->NumArgs() > 1) {
        alias = ev->GetString(2);
    }

    AddItem(item, alias);
}

void UIList::AddItem(str item, str alias)
{
    UIListItem *listItem = new UIListItem();
    listItem->itemname   = item;
    listItem->itemalias  = alias;

    m_itemlist.AddObject(listItem);
    m_currentItem = 1;
}

void UIList::UpdateUIElement(void)
{
    // Retail @0x00065B80 gates on the cvar-name str's LENGTH (`a1[276]`), not its
    if (!m_cvarname.length()) {
        return;
    }

    const char *cvarstring = UI_GetCvarString(m_cvarname, NULL);
    if (!cvarstring) {
        m_currentItem = 1;
        UpdateData();
        return;
    }

    for (int i = 1; i <= m_itemlist.NumObjects(); i++) {
        UIListItem *obj = m_itemlist.ObjectAt(i);
        if (obj->itemname == cvarstring) {
            m_currentItem = i;
            break;
        }
    }
}

void UIList::UpdateData(void)
{
    if (!m_currentItem) {
        return;
    }

    if (m_cvarname.length()) {
        UIListItem *item = m_itemlist.ObjectAt(m_currentItem);
        if (item->itemname) {
            uii.Cvar_Set(m_cvarname, item->itemname);
        }
    }

    if (m_command.length()) {
        UIListItem *item = m_itemlist.ObjectAt(m_currentItem);
        // itemname + '\n' (Mac UIList::UpdateData 0x00065720) -- and never
        str cmd = m_command + ' ' + item->itemname + '\n';
        Cbuf_AddText(cmd.c_str());
    }
}
