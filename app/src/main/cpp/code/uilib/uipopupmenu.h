/*
===========================================================================
Copyright (C) 2024 the OpenMoHAA team

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

#pragma once

typedef enum {
    UIP_NONE,
    UIP_EVENT,
    UIP_EVENT_STRING,
    UIP_SUBMENU,
    UIP_CMD,
    UIP_CVAR,
    UIP_SEPARATOR
} uipopup_type;

typedef enum {
    UIP_WHERE_RIGHT,
    UIP_WHERE_DOWN
} uipopup_where;

class uipopup_describe
{
public:
    str               title;
    str               raw;
    UIReggedMaterial *material;
    uipopup_type      type;
    void             *data;

    uipopup_describe();
    uipopup_describe(str title, uipopup_type type, void *data, UIReggedMaterial *material);
    ~uipopup_describe();
};

inline uipopup_describe::uipopup_describe()
{
    title    = "";
    material = NULL;
    type     = UIP_NONE;
    data     = NULL;
}

inline uipopup_describe::uipopup_describe(
    str newTitle, uipopup_type newType, void *newData, UIReggedMaterial *newMaterial
)
{
    this->title    = newTitle;
    this->type     = newType;
    this->data     = newData;
    this->material = newMaterial;
}

// NOTE: this is not part of the original game!
inline uipopup_describe::~uipopup_describe()
{
    if (this->data == NULL) {
        return;
    }

    if (this->type == UIP_CMD || this->type == UIP_CVAR) {
        free(this->data);
        this->data = NULL;
    }
}

class UIPopupMenu : public UIWidget
{
    friend class UIPulldownMenu; 

protected:
    Container<uipopup_describe *> *m_describe;
    int                            m_selected;
    Listener                      *m_listener;
    class UIPopupMenu             *m_parentMenu;
    class UIPopupMenu             *m_submenuptr;
    int                            m_submenu;
    UIFont                         m_marlett;
    bool                           m_autodismiss;
    UColor                         m_highlightFGColor;
    UColor                         m_highlightBGColor;
    float                          m_width;

public:
    CLASS_PROTOTYPE(UIPopupMenu);

protected:
    uipopup_describe *getDescribeFromPoint(const UIPoint2D& p);
    // caller is setSelection @0x0007B03C: retail never builds a submenu popup, so
    void              MakeSubMenu(void);
    virtual float     getDescribeHeight(uipopup_describe *d);
    virtual float     getDescribeWidth(uipopup_describe *d);
    // ever walked the m_submenuptr chain that retail never builds.  Removed.

public:
    UIPopupMenu();
    ~UIPopupMenu();

    void Create(
        Container<uipopup_describe *> *describe,
        Listener                      *listener,
        const UIRect2D&                createRect,
        uipopup_where                  where,
        float                          width
    );
    void Draw(void) override;
    void MouseReleased(Event *ev);
    void MouseMoved(Event *ev);
    void MouseExited(Event *ev) override;
    void Dismiss(void);
    void setSelection(int sel);
};

uipopup_type UI_PopupTypeStringToInt(str type);
