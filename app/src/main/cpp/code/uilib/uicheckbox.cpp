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

Event EV_UICheckBox_SetCheckedCommand
(
    "checked_command",
    EV_DEFAULT,
    "s",
    "command",
    "Command to execute when checkbox is checked"
);

Event EV_UICheckBox_SetUncheckedCommand
(
    "unchecked_command",
    EV_DEFAULT,
    "s",
    "command",
    "Command to execute when checkbox is checked" // text not present in original executable  /* retail engine @0x0023EFC6 */
);

Event EV_UICheckBox_SetCheckedShader
(
    "checked_shader",
    EV_DEFAULT,
    "s",
    "shader",
    "Set the shader of the small button when it is checked"
);

Event EV_UICheckBox_SetUncheckedShader
(
    "unchecked_shader",
    EV_DEFAULT,
    "s",
    "shader",
    "Set the shader of the small button when it is unchecked"
);

Event EV_UICheckBox_LinkCvarBit
(
    "linkcvarbit",
    EV_DEFAULT,
    "i",
    "bit",
    "bit to change in the cvar"
);

CLASS_DECLARATION(UIWidget, UICheckBox, NULL) {
    {&W_LeftMouseDown,                   &UICheckBox::Pressed            },
    {&W_LeftMouseUp,                     &UICheckBox::Released           },
    {&W_MouseEntered,                    &UICheckBox::MouseEntered       },
    {&W_MouseExited,                     &UICheckBox::MouseExited        },
    {&EV_UICheckBox_SetCheckedCommand,   &UICheckBox::SetCheckedCommand  },
    {&EV_UICheckBox_SetUncheckedCommand, &UICheckBox::SetUncheckedCommand},
    {&EV_UICheckBox_SetCheckedShader,    &UICheckBox::SetCheckedShader   },
    {&EV_UICheckBox_SetUncheckedShader,  &UICheckBox::SetUncheckedShader },
    {&EV_UICheckBox_LinkCvarBit,         &UICheckBox::LinkCvarBit     },
    {NULL,                               NULL                            }
};

UICheckBox::UICheckBox()
{
    AllowActivate(qtrue);
    m_check_height       = 16.0;
    m_check_width        = 16.0;
    m_check_scale        = 1.0;
    m_indent             = 16.0;
    m_depressed          = qfalse;
    m_checked_material   = NULL;
    m_unchecked_material = NULL;
    m_cvarbit            = -1;
    m_textRect = UIRect2D(0, 0, 0, 0);
    calculateTextBox();
}

void UICheckBox::calculateTextBox(void)
{
    UIRect2D drawRegion = getDrawRegion();
    float    x          = m_check_width + m_indent;

    m_textRect.pos.x       = x;
    m_textRect.pos.y       = drawRegion.pos.y;
    m_textRect.size.width  = drawRegion.size.width - x;
    m_textRect.size.height = drawRegion.size.height;
}

// the text box is recomputed.  Note this arm is NOT behind isResizeable(): retail
void UICheckBox::ChangeResolution(void)
{
    UIWidget::ChangeResolution();

    m_check_width  = 16.0f * m_check_scale * uid.scaleRes[0];
    m_check_height = 16.0f * m_check_scale * uid.scaleRes[1];
    m_indent       = 16.0f * uid.scaleRes[0];

    calculateTextBox();
}

void UICheckBox::setCheckScale(float scale)
{
    m_check_scale = scale;

    m_check_width  = 16.0f * m_check_scale;
    m_check_height = 16.0f * m_check_scale;

    calculateTextBox();
}

void UICheckBox::Draw()
{
    UIWidget::Draw();

    UpdateData();

    if (m_widgetState == WIDGET_STATE_DISABLED) {
        UpdateCvar();
    }

    if (!m_depressed) {
        Draw3DBox(0.0f, 0.0f, m_frame.size.width, m_frame.size.height, 0, getBorderColor(), m_local_alpha);
    }

    SetColor(getStateShaderColor(), m_local_alpha);

    UColor            color    = m_checked ? UBlue : UBlack;
    UIReggedMaterial *material = m_checked ? m_checked_material : m_unchecked_material;

    float width  = m_check_width;
    float height = m_check_height;
    float x      = 0.0f;
    float y      = (m_frame.size.height - height) * 0.5f;
    if (material) {
        uii.Rend_DrawPicStretched(x, y, width, height, 0.0f, 0.0f, 1.0f, 1.0f, material->GetMaterial());
    } else {
        DrawBox(x, y, width, height, color, m_local_alpha);
    }

    m_font->setColor(getForegroundColor());
    m_font->PrintJustified(
        m_textRect,
        m_iFontAlignmentHorizontal,
        m_iFontAlignmentVertical,
        m_title.c_str(),
        getTextShadowColor(),
        m_bVirtual
    );
}

void UICheckBox::CharEvent(int ch)
{
    if (ch == ' ') {
        m_checked = !m_checked;
        UpdateCvar();
    }
}

void UICheckBox::Pressed(Event *ev)
{
    m_depressed = qtrue;

    UIWidget::Pressed(ev);
}

void UICheckBox::Released(Event *ev)
{
    if (m_widgetState == WIDGET_STATE_DISABLED) {
        return;
    }

    m_checked = !m_checked;

    str command = m_checked ? m_checked_command : m_unchecked_command;
    Cbuf_AddText(command.c_str());

    m_depressed = qfalse;
    UpdateCvar();

    setHoverState();
}

void UICheckBox::UpdateCvar()
{
    if (m_cvarname.length()) {
        if (m_cvarbit >= 0) {
            int value = UI_GetCvarInt(m_cvarname.c_str(), 0);
            if (m_checked) {
                value |= (1 << m_cvarbit);
            } else {
                value &= ~(1 << m_cvarbit);
            }
            if (m_widgetState == WIDGET_STATE_DISABLED) {
                value     = 0;
                m_checked = 0;
            }
            UI_SetCvarInt(m_cvarname, value);
        } else {
            if (m_widgetState == WIDGET_STATE_DISABLED) {
                m_checked = 0;
            }
            UI_SetCvarInt(m_cvarname, m_checked);
        }
    }

    if (m_command.length()) 
    {
        // UICheckBox::UpdateCvar 0x00054770) and never appends to m_command.
        str cmd = m_command + (m_checked ? " 1\n" : " 0\n");
        Cbuf_AddText(cmd.c_str());
    }

    if (m_commandhandler) 
    {
        const char *checkedInt = va("%d", m_checked);
        m_commandhandler(checkedInt, NULL);
    }
}

void UICheckBox::MouseEntered(Event *ev)
{
    auto num = ev->GetInteger(3);
    uWinMan.ActivateControl(this);
    if (num & 1) {
        m_depressed = qtrue;
    }

    UIWidget::MouseEntered(ev);
}

void UICheckBox::MouseExited(Event *ev)
{
    m_depressed = qfalse;

    UIWidget::MouseExited(ev);
}

void UICheckBox::SetCheckedCommand(Event *ev)
{
    m_checked_command = ev->GetString(1);
}

void UICheckBox::SetUncheckedCommand(Event *ev)
{
    m_unchecked_command = ev->GetString(1);
}

void UICheckBox::SetCheckedShader(Event *ev)
{
    str shader         = ev->GetString(1);
    m_checked_material = uWinMan.RegisterShader(shader);
}

void UICheckBox::SetUncheckedShader(Event *ev)
{
    str shader           = ev->GetString(1);
    m_unchecked_material = uWinMan.RegisterShader(shader);
}

void UICheckBox::LinkCvarBit(Event *ev)
{
    m_cvarbit = ev->GetInteger(1);
}

void UICheckBox::UpdateData(void)
{
    if (m_cvarname.length()) {
        if (m_cvarbit >= 0) {
            m_checked = (UI_GetCvarInt(m_cvarname.c_str(), 0) >> m_cvarbit) & 1;
        } else {
            m_checked = UI_GetCvarInt(m_cvarname.c_str(), 0);
        }
    }
}

bool UICheckBox::isChecked()
{
    return m_checked;
}
