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
#include "../script/scriptexception.h"

Event EV_Layout_Menu
(
    "menu",
    EV_DEFAULT,
    "sffsF",
    "name width height direction motion_time",
    "Sets up the layout of the menu."
);

CLASS_DECLARATION(Listener, UILayout, NULL) {
    {&EV_Layout_Menu, &UILayout::CreateWidgetContainer},
    {NULL,            NULL                            }
};

UILayout::UILayout(const char *filename)
{
    m_script           = NULL;
    m_currentcontainer = NULL;
    m_currentwidget    = NULL;

    if (filename) {
        Load(filename);
    }
}

void UILayout::CreateWidgetContainer(Event *ev)
{
    str   name;
    str   direction;
    float w, h;

    name = ev->GetString(1);

    w         = ev->GetFloat(2);
    h         = ev->GetFloat(3);
    direction = ev->GetString(4);

    m_currentcontainer = new UIWidgetContainer;
    m_currentcontainer->setName(name);

    if (!stricmp(direction, "from_bottom")) {
        m_currentcontainer->setDirection(D_FROM_BOTTOM);
    } else if (!stricmp(direction, "from_top")) {
        m_currentcontainer->setDirection(D_FROM_TOP);
    } else if (!stricmp(direction, "from_left")) {
        m_currentcontainer->setDirection(D_FROM_LEFT);
    } else if (!stricmp(direction, "from_right")) {
        m_currentcontainer->setDirection(D_FROM_RIGHT);
    } else {
        m_currentcontainer->setDirection(D_NONE);
    }

    if (ev->NumArgs() == 5) {
        m_currentcontainer->setMotionTime(ev->GetFloat(5));
    }

    m_currentcontainer->InitFrame(NULL, 0, 0, w, h);
    m_currentwidget = m_currentcontainer;
}

void UILayout::ProcessCommands(void)
{
    const char *token;
    Event      *ev;
    UIWidget   *widget;

    try {
        while (m_script->TokenAvailable(true)) {
            token = m_script->GetToken(true);

            if (!strcmp(token, "end.")) {
                break;
            } else if (!strcmp(token, "include")) {
                if (m_script->TokenAvailable(false)) {
                    m_scriptstack.Push(m_script);
                    Load(m_script->GetToken(false));
                    m_script = m_scriptstack.Pop();
                }
            } else if (!strcmp(token, "resource")) {
                ClassDef   *widgetClass;
                str         className;
                const char *newname;

                token = m_script->GetToken(true);

                newname = uii.Client_TranslateWidgetName(token);
                if (!newname) {
                    newname = token;
                }

                className = "UI";
                className += newname;
                widgetClass = getClass(className);
                if (!widgetClass) {
                    throw ScriptException("LOAD_BAD_WIDGET");
                }

                widget = (UIWidget *)widgetClass->newInstance();

                token = m_script->GetToken(true);
                if (*token == '{') {
                    widget->InitFrame(m_currentcontainer, 10, 10, 100, 64, -1, "tahoma-8");

                    token = m_script->GetToken(true);
                    while (1) {
                        if (!strcmp(token, "}")) {
                            break;
                        }


                        if (widget->ValidEvent(token)) {
                            ev = new Event(token);

                            while (m_script->TokenAvailable(false)) {
                                token = m_script->GetToken(false);
                                ev->AddToken(token);
                            }

                            widget->ProcessEvent(ev);
                        } else {
                            widget->warning(
                                "ValidEvent",
                                "Event '%s' out of response range for class '%s'.  Event probably invalid or allocated late.\n",
                                token,
                                widget->getClassname());
                        }

                        if (!m_script->TokenAvailable(true)) {
                            break;
                        }

                        token = m_script->GetToken(true);
                    }

                    // LAYOUT FIX) -- which is what fixes the widgets that are NEVER passed
                }
            } else {
                if (ValidEvent(token)) {
                    ev = new Event(token);

                    while (m_script->TokenAvailable(false)) {
                        token = m_script->GetToken(false);
                        ev->AddToken(token);
                    }

                    ProcessEvent(ev);
                } else if (m_currentwidget && m_currentwidget->ValidEvent(token)) {
                    ev = new Event(token);

                    while (m_script->TokenAvailable(false)) {
                        token = m_script->GetToken(false);
                        ev->AddToken(token);
                    }

                    m_currentwidget->ProcessEvent(ev);
                } else if (m_currentwidget) {
                    m_currentwidget->warning(
                        "ValidEvent",
                        "Event '%s' out of response range for class '%s'.  Event probably invalid or allocated late.\n",
                        token,
                        m_currentwidget->getClassname());
                } else {
                    warning(
                        "ValidEvent",
                        "Event '%s' out of response range for class '%s'.  Event probably invalid or allocated late.\n",
                        token,
                        getClassname());

                    uii.Sys_Printf(
                        "UI: Invalid token '%s' on line %d of %s\n",
                        token,
                        m_script->GetLineNumber(),
                        m_script->Filename());
                }
            }
        }
    } catch (ScriptException& exc) {
        uii.Sys_Printf("UI EXCEPTION : %s\n", exc.string.c_str());
    }
}

void UILayout::Load(const char *filename)
{
    if (filename) {
        m_script = new Script;
        m_script->LoadFile(filename);

        ProcessCommands();

        delete m_script;
        m_script = NULL;
    }
}
