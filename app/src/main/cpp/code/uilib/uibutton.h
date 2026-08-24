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

extern Event W_Button_Pressed;
extern Event W_Button_ServerVarToSet;
extern Event W_Button_ServerThreadProperty;
extern Event W_Button_ServerThreadToRun;
extern Event W_Button_ClearServerThreads;
extern Event W_Button_DialogThreadToRun;
extern Event W_Button_ClearDialogThreads;

class UIButtonBase : public UIWidget
{
protected:
    mouseState_t m_mouseState;
    str          m_hoverSound;
    str          m_hoverCommand;
    str          m_mouseExitedCommand;
    Container<str *> m_serverThreads;
    // AddServerVarToSet @0x00051C80 rejects any name that does not start with
    Container<str *> m_serverVars;
    str              m_serverThreadCommand;
    str              m_serverThreadPropertyName;
    str              m_serverThreadPropertyValue;
    bool             m_hasServerThreadProperty;
    str          m_dialogThreadToRun;
    bool         m_stayPressed;         

public:
    CLASS_PROTOTYPE(UIButtonBase);

protected:
    void Pressed(Event *ev) override;
    void Released(Event *ev);
    void MouseEntered(Event *ev) override;
    void MouseExited(Event *ev) override;
    void SetHoverSound(Event *ev);
    void SetHoverCommand(Event *ev);
    void SetMouseExitedCommand(Event *ev);
    void AddServerVarToSet(Event *ev);          
    void setServerThreadProperty(Event *ev);    
    void AddServerThreadToRun(Event *ev);  
    void ClearServerThreads(Event *ev);    
    void SetDialogThreadToRun(Event *ev);  
    void ClearDialogThreads(Event *ev);    
    void stayPressed(Event *ev);           

public:
    UIButtonBase();

    virtual void Action(void);

    virtual void        setData(const str& data) override;
    virtual const char *getData(void) override;

    virtual void        updateGameplayProperty(void) override;

    bool StaysPressed(void) const override { return m_stayPressed; }
};

class UIButton : public UIButtonBase
{
public:
    CLASS_PROTOTYPE(UIButton);

protected:
    // Protected, not private: retail UISwitchBox::Draw @0x00090020 ends with a call
    void Draw(void) override;

public:
    UIButton();

    qboolean KeyEvent(int key, unsigned int time) override;
    void     pressedShader(Event *ev);
};

class ToggleCVar : public USignal
{
protected:
    str       m_cvarname;
    UIButton *m_button;

protected:
    CLASS_PROTOTYPE(ToggleCVar);

    void Press(Event *ev);

public:
    ToggleCVar();
    ToggleCVar(UIButton *button, const char *cvar);

    void setCVar(const char *cvar);
    void setButton(UIButton *button);
};

class ExecCmd : public USignal
{
protected:
    UIButton *m_button;
    str       m_cmd;

public:
    CLASS_PROTOTYPE(ExecCmd);

protected:
    void Press(Event *ev);

public:
    ExecCmd();
    ExecCmd(UIButton *button, const char *cmd);

    void setCommand(const char *cmd);
    void setButton(UIButton *button);
};
