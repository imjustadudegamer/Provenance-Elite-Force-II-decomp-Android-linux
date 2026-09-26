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

#define PROBE_GET(probe_n, probe_d, probe_f) ((cvar_t *)0)
#define PROBE_PRINT                       Com_Printf
#define PROBE_ON_OVERRIDE                       (atoi(CvarGetForUI("qa_probe", "0")) != 0)  
#include "../qcommon/qa_probe.h"

Event EV_UILabel_LinkString
(
    "linkstring",
    EV_DEFAULT,
    "is",
    "value string",
    "Creates a link from the specified value to a string.Use this if you want the label to display a string different from the value of the cvar"
);

// The string "linkcvartoshader" does NOT occur anywhere in the retail engine
linkstring::linkstring(const str& newValue, const str& newString)
{
    value = newValue;

    if (isStringResource(newString.c_str())) {
        raw = newString;
        string = getStringResource(newString.c_str());
    } else {
        string = newString;
    }
}

// and prologue crawls on a port-only keyframe machine instead of retail's WidgetMover;


// Label never tells its parent.  Removing the two ResponseDef rows restores retail's

CLASS_DECLARATION(UIWidget, UILabel, NULL) {
    {&EV_UILabel_LinkString, &UILabel::LinkString           },
    {&EV_Layout_Shader,      &UILabel::LabelLayoutShader    },
    {&EV_Layout_TileShader,  &UILabel::LabelLayoutTileShader},
    {NULL,                   NULL                           }
};

UILabel::UILabel()
{
    AllowActivate(false);
}

void UILabel::LinkString(Event *ev)
{
    linkstring *ls;
    str         value;
    str         string;

    value  = ev->GetString(1);
    string = ev->GetString(2);

    ls = new linkstring(value, string);

    m_linkstrings.AddObject(ls);
}

void UILabel::SetLabel(str lab)
{
    if (isStringResource(lab.c_str())) {
        m_rawlabel      = lab;
        const char *res = getStringResource(lab.c_str());
        label = res ? str(res) : str("");
    } else {
        label = lab;
    }
}

int UILabel::FindLinkString(str val)
{
    int i;

    for (i = 1; i <= m_linkstrings.NumObjects(); i++) {
        if (!str::icmp(val, m_linkstrings.ObjectAt(i)->value)) {
            return i;
        }
    }

    return 0;
}

void UILabel::LabelLayoutShader(Event *ev)
{
    m_sCurrentShaderName = ev->GetString(1);
    setMaterial(uWinMan.RegisterShader(m_sCurrentShaderName), WIDGET_STATE_NORMAL);

    m_flags &= ~WF_TILESHADER;
}

void UILabel::LabelLayoutTileShader(Event *ev)
{
    m_sCurrentShaderName = ev->GetString(1);
    if (m_sCurrentShaderName.length() > 1) {
        setMaterial(uWinMan.RegisterShader(m_sCurrentShaderName), WIDGET_STATE_NORMAL);
    } else {
        m_sCurrentShaderName = "";
    }

    m_flags |= WF_TILESHADER;
}

void UILabel::setData(const str& data)
{
    if (isStringResource(data.c_str())) {
        m_rawlabel = data;

        const char *resolved = getStringResource(data.c_str());
        if (resolved) {
            label = resolved;
        } else {
            label = "";
        }
    } else {
        label = data;
    }
}

const char *UILabel::getData(void)
{
    return label;
}

void UILabel::reloadStrings(void)
{
    int i;

    UIWidget::reloadStrings();

    if (m_rawlabel.length()) {
        const char *res = getStringResource(m_rawlabel.c_str());
        if (res) {
            label = res;
        } else {
            label = "";
        }
    }

    for (i = 1; i <= m_linkstrings.NumObjects(); i++) {
        linkstring *ls = m_linkstrings.ObjectAt(i);
        if (ls && ls->raw.length()) {
            const char *res = getStringResource(ls->raw.c_str());
            if (res) {
                ls->string = res;
            } else {
                ls->string = "";
            }
        }
    }
}

void UILabel::Draw(void)
{
    const char *string;
    str         val;

    UIWidget::Draw();

    if (m_labelText.length()) {
        UColor color = getStateForegroundColor();

        color.a *= m_local_alpha;
        m_font->setColor(color);

        m_font->PrintJustified(
            getClientFrame(),
            FONT_JUSTHORZ_LEFT,
            FONT_JUSTVERT_TOP,
            m_labelText.c_str(),
            getStateTextShadowColor(),
            m_bVirtual
        );
        return;
    }

    if (m_cvarname.length()) {
        string = uii.Cvar_GetString(m_cvarname, "");

        if (m_linkstrings.NumObjects()) {
            cvar_t *cvar = UI_FindCvar(m_cvarname);
            if (cvar) {
                int ret;

                if (cvar->latchedString) {
                    val = cvar->latchedString;
                } else {
                    val = cvar->string;
                }

                ret = FindLinkString(val);
                if (ret) {
                    string = m_linkstrings.ObjectAt(ret)->string;
                }
            }
        }
    } else if (label.length()) {
        string = label;
    } else if (m_title.length()) {
        string = m_title;
    } else {
        string = "";
    }

    if (!strcmp(getName(), "prologueText") || !strcmp(getName(), "prologueText2")) {
        static int paint_lbl;

        if (PROBE_On() && PROBE_Throttle(&paint_lbl, 4, 300)) {
            PAINT_PROBE(
                "text:label",
                "label='%s' labelLen=%d titleLen=%d labeltextLen=%d cvarnameLen=%d "
                "srcLen=%d frame=(%.1f,%.1f %.1fx%.1f) virtual=%d keyframes=%d mwcIndex=%d",
                getName(),
                (int)label.length(),
                (int)m_title.length(),
                (int)m_labelText.length(),
                (int)m_cvarname.length(),
                string ? (int)strlen(string) : -1,
                (double)m_frame.pos.x,
                (double)m_frame.pos.y,
                (double)m_frame.size.width,
                (double)m_frame.size.height,
                m_bVirtual ? 1 : 0,
                m_mover ? m_mover->NumCommands() : 0,
                m_mover ? m_mover->CurrentCommand() : 0);
        }
    }

    if (string && *string) {
        char converted[0x400];

        /* The conversion returns its input unchanged unless the text carries a
           `$$id$$' pair, so text without one is drawn as it stands. */
        if (strstr(string, "$$")) {
            uii.SR_ConvertStringResourceIds(string, converted, sizeof(converted));
            string = converted;
        }

        m_font->setColor(getStateForegroundColor());

        m_font->PrintJustified(
            getDrawRegion(),
            m_iFontAlignmentHorizontal,
            m_iFontAlignmentVertical,
            string,
            getStateTextShadowColor(),
            m_bVirtual
        );
    }
}
