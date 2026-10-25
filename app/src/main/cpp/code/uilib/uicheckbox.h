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

class UICheckBox : public UIWidget
{
    str               m_checked_command;
    str               m_unchecked_command;
    UIReggedMaterial *m_checked_material;
    UIReggedMaterial *m_unchecked_material;
    bool              m_checked;
    bool              m_depressed;
    float             m_check_width;
    float             m_check_height;
    float             m_check_scale;
    int               m_cvarbit;
    UIRect2D          m_textRect;

public:
    CLASS_PROTOTYPE(UICheckBox);

private:
    void calculateTextBox(void);
    void Draw(void) override;
    void CharEvent(int ch) override;
    void Pressed(Event *ev) override;
    void Released(Event *ev);
    void UpdateCvar(const char *from);
    void MouseEntered(Event *ev) override;
    void MouseExited(Event *ev) override;
    void SetCheckedCommand(Event *ev);
    void SetUncheckedCommand(Event *ev);
    void SetCheckedShader(Event *ev);
    void SetUncheckedShader(Event *ev);
    void LinkCvarBit(Event *ev);

public:
    UICheckBox(void);

    void UpdateData(void) override;
    void ChangeResolution(void) override;
    void setCheckScale(float scale);
    bool isChecked(void);
};

extern Event EV_UICheckBox_SetCheckedCommand;
extern Event EV_UICheckBox_SetUncheckedCommand;
extern Event EV_UICheckBox_SetCheckedShader;
extern Event EV_UICheckBox_SetUncheckedShader;
extern Event EV_UICheckBox_LinkCvarBit;
