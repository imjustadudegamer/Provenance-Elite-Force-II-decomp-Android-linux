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

#pragma once

class BindCommand
{
public:
    str name;
    str command;
    str animation;

    BindCommand();
    BindCommand(const str& inName, const str& inCommand, const str& inAnimation);
};

inline BindCommand::BindCommand() {}

inline BindCommand::BindCommand(const str& inName, const str& inCommand, const str& inAnimation)
{
    name      = inName;
    command   = inCommand;
    animation = inAnimation;
}

class bind_t
{
public:
    Container<char *>                      bindGroupNames;
    Container<Container<BindCommand *> *>  bindGroups;
    int                                    currentBindGroup;

public:
    bind_t();
    ~bind_t();

    void Clear(void);
};

class BindListener : public Listener
{
protected:
    bind_t *bind;

public:
    CLASS_PROTOTYPE(BindListener);

public:
    BindListener();
    BindListener(bind_t *b);

    bool Load(Script& script);
    void NewItem(Event *ev);
    void BindGroup(Event *ev);
};


class UIBindButton : public UIButton
{
    str               m_bindcommand;
    int               m_bindindex;
    qboolean          m_getkey;
    qboolean          m_alternate;
    str               m_entersound;
    str               m_activesound;
    str               m_last_keyname;
    UIReggedMaterial *m_mat;

public:
    CLASS_PROTOTYPE(UIBindButton);

public:
    UIBindButton();
    UIBindButton(str entersound, str activesound);

    void     Pressed(Event *ev) override;
    void     SetCommand(Event *ev);
    void     SetCommand(str s);
    void     setData(const str& data) override;
    void     MouseEntered(Event *ev) override;
    void     copyProperties(UIWidget& other) override;
    void     Draw(void) override;
    void     DrawPressKey(void);
    void     clearBindCommand(void);  
    qboolean KeyEvent(int key, unsigned int time) override;
    void     SetAlternateKey(Event *ev);  
};
