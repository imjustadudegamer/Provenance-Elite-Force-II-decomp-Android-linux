/*
===========================================================================
Copyright (C) 2015-2023 the OpenMoHAA team

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

Event EV_UIField_MaxCharacters
(
    "maxcharacters",
    EV_DEFAULT,
    "i",
    "maxcharacter",
    "Sets the maximum characters the field can have"
);

CLASS_DECLARATION(UIWidget, UIField, NULL) {
    {&EV_Widget_Activate,        &UIField::activate         },
    {&EV_Widget_Deactivate,      &UIField::deactivate       },
    {&EV_UIField_MaxCharacters,  &UIField::setMaxCharacters},
    {NULL,                       NULL                       }
};

UIField::UIField()
{
    memset(m_edit.m_buffer, 0, sizeof(m_edit.m_buffer));
    m_edit.m_buffer[0] = 0;
    m_edit.m_cursor    = 0;
    m_maxCharacters    = 0;
    AllowActivate(qtrue);
}

void UIField::setMaxCharacters(Event *ev)
{
    m_maxCharacters = ev->GetInteger(1);
}

void UIField::activate(Event *ev)
{
    m_edit.m_cursor = strlen(m_edit.m_buffer);
    UIWidget::Activate(ev);
}

void UIField::deactivate(Event *ev)
{
    str result;

    if (m_cvarname.length()) {
        uii.Cvar_Set(m_cvarname, m_edit.m_buffer);
    }

    const char *hit = strstr(m_command.c_str(), "$e$");
    if (hit) {
        for (const char *p = m_command.c_str(); p != hit; ++p) {
            result += *p;
        }

        result += m_edit.m_buffer;
        result += hit + strlen("$e$");
    }

    if (result.length()) {
        str stuff = result + "\n";
        uii.Cmd_Stuff(stuff.c_str());
    }
}

void UIField::Draw(void)
{
    // so retail's own data never builds this widget and the OPM-derived body below is

    UIWidget::Draw();

    UpdateData();

    float fVirtualWidth = m_frame.size.width - m_font->getWidth(" ..", -1, true);
    float indentLength  = m_indent * uid.scaleRes[0];
    float cursorSize    = m_font->getCharWidth('_', true);

    float totalDrawLength = m_font->getWidth(m_edit.m_buffer, -1, true) + indentLength;

    int iPreStep = 0;

    int iMaxPreStep;

    float fEndLen = totalDrawLength + cursorSize;
    int   buflen  = strlen(m_edit.m_buffer);

    for (iMaxPreStep = 0; fEndLen > fVirtualWidth && iMaxPreStep < buflen; iMaxPreStep++) {
        float fCharWidth = m_font->getCharWidth(m_edit.m_buffer[iMaxPreStep], true);
        fEndLen -= fCharWidth;
    } 

    int iLastChar;

    fEndLen = indentLength + cursorSize;
    for (iLastChar = iPreStep; fEndLen < fVirtualWidth; iLastChar++) {
        if (char c = m_edit.m_buffer[iLastChar]) {
            float fCharWidth = m_font->getCharWidth(c, true);
            fEndLen += fCharWidth;
        } else {
            break;
        }
    }

    if (IsActive()) {
        if (m_edit.m_cursor >= iPreStep) {
            while (iLastChar <= m_edit.m_cursor && iPreStep < iMaxPreStep
                   && iPreStep < iLastChar) { 
                fEndLen -= m_font->getCharWidth(m_edit.m_buffer[iPreStep], true);
                iPreStep++;

                if (char c = m_edit.m_buffer[iLastChar]) {
                    float fCharWidth = m_font->getCharWidth(c, true);
                    fEndLen += fCharWidth;
                } else {
                    break;
                }

                while (fEndLen < fVirtualWidth) {
                    iLastChar++;

                    if (char c = m_edit.m_buffer[iLastChar]) {
                        float fCharWidth = m_font->getCharWidth(c, true);
                        fEndLen += fCharWidth;
                    } else {
                        break;
                    }
                }
            }
        } else {
            iPreStep = m_edit.m_cursor;
        }

        iPreStep = Q_min(iPreStep, iMaxPreStep);
    }

    char temptext[EDITFIELD_BUFSIZE];
    Q_strncpyz(temptext, &m_edit.m_buffer[iPreStep], iLastChar - iPreStep + 1);

    float cursorPos = indentLength - 1.0f;

    for (int i = iPreStep; i < m_edit.m_cursor && i < EDITFIELD_BUFSIZE; i++) {
        float fCharWidth = m_font->getCharWidth(m_edit.m_buffer[i], true);
        cursorPos += fCharWidth;
    }

    cursorPos = Q_min(cursorPos, fVirtualWidth);

    float y = m_frame.size.height - m_font->getHeight(true);
    if (m_bottomindent * 2 <= y) 
    {
        y -= m_bottomindent;
    } else {
        y /= 2;
    }

    m_font->setColor(m_foreground_color);
    m_font->Print(indentLength, y, temptext, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), true);

    if (IsActive()) {
        const char *cursorChar = (uid.time / 250) & 1 ? "|" : "_";
        m_font->Print(cursorPos, y, cursorChar, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), true);
    }

    Q_strncpyz(temptext, "..", EDITFIELD_BUFSIZE);

    if (iPreStep) {
        m_font->Print(2.0f, y, temptext, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), true);
    }

    if (iLastChar < EDITFIELD_BUFSIZE && m_edit.m_buffer[iLastChar]) {
        fVirtualWidth = m_frame.size.width - m_font->getWidth(temptext, -1, true) - 2.0f;
        m_font->Print(fVirtualWidth, y, temptext, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), true);
    }
}

qboolean UIField::KeyEvent(int key, unsigned int time)
{
    if (key == 'l' && uii.Sys_IsKeyDown(K_CTRL)) {
        return qtrue;
    }

    if (key == K_ENTER || key == K_KP_ENTER) {
        uWinMan.ActivateControl(NULL);
        return qtrue;
    }

    if ((key == K_INS || key == K_KP_INS) && uii.Sys_IsKeyDown(K_SHIFT)) {
        return qtrue;
    }

    int len = strlen(m_edit.m_buffer);
    switch (key) {
    case K_DEL: 
        if (m_edit.m_cursor < len) {
            memmove(&m_edit.m_buffer[m_edit.m_cursor], &m_edit.m_buffer[m_edit.m_cursor + 1], len - m_edit.m_cursor);

            if (m_cvarname.length()) {
                uii.Cvar_Set(m_cvarname, m_edit.m_buffer);
            }
        }
        return qtrue;
    case K_RIGHTARROW:
        if (m_edit.m_cursor < len) {
            m_edit.m_cursor++;
        }
        return qtrue;
    case K_LEFTARROW:
        if (m_edit.m_cursor > 0) {
            m_edit.m_cursor--;
        }
        return qtrue;
    }

    if (key == K_HOME || (tolower(key) == 'a' && uii.Sys_IsKeyDown(K_CTRL))) {
        m_edit.m_cursor = 0;
        return qtrue;
    }

    if (key == K_END || (tolower(key) == 'e' && uii.Sys_IsKeyDown(K_CTRL))) {
        m_edit.m_cursor = len;
        return qtrue;
    }

    return key == K_INS;
}

void UIField::CharEvent(int ch)
{
    if (ch == 3 || ch == 22) 
    {
        return;
    }

    int len = strlen(m_edit.m_buffer);
    switch (ch) {
    case '\b': 
        if (m_edit.m_cursor <= 0) {
            return;
        }

        memmove(&m_edit.m_buffer[m_edit.m_cursor - 1], &m_edit.m_buffer[m_edit.m_cursor], len + 1 - m_edit.m_cursor);

        m_edit.m_cursor--;

        if (m_cvarname.length()) {
            uii.Cvar_Set(m_cvarname, m_edit.m_buffer);
        }

        return;
    case 1: 
        m_edit.m_cursor = 0;
        return;
    case 5: 
        m_edit.m_cursor = len;
        return;
    }

    if (iscntrl(ch)) {
        return;
    }

    if (m_maxCharacters > 0 && (int)strlen(m_edit.m_buffer) >= m_maxCharacters) {
        return;
    }

    len++; 

    if (len >= EDITFIELD_BUFSIZE) 
    {
        return;
    }

    memmove(&m_edit.m_buffer[m_edit.m_cursor + 1], &m_edit.m_buffer[m_edit.m_cursor], len - m_edit.m_cursor);

    m_edit.m_buffer[m_edit.m_cursor] = ch;

    m_edit.m_cursor++;

    if (m_edit.m_cursor == len) {
        m_edit.m_buffer[m_edit.m_cursor] = 0;
    }

    if (m_cvarname.length()) {
        uii.Cvar_Set(m_cvarname, m_edit.m_buffer);
    }
}

void UIField::UpdateData(void)
{
    if (m_cvarname.length()) {
        if (!IsActive()) {
            str::snprintf(m_edit.m_buffer, EDITFIELD_BUFSIZE, "%s", CvarGetForUI(m_cvarname, ""));
            m_edit.m_cursor = Q_min(m_edit.m_cursor, (int)strlen(m_edit.m_buffer));
        }
    }
}


