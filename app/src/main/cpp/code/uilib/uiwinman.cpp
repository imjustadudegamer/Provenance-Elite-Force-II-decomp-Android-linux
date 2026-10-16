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

#define PROBE_GET(name, def, flags) ((cvar_t *)0)
#define PROBE_PRINT                 Com_Printf
#define PROBE_ON_OVERRIDE                 (atoi(CvarGetForUI("qa_probe", "0")) != 0)  
#include "../qcommon/qa_probe.h"

#include "../qcommon/qa_ui.h"

/* A held navigation key's own repeat: the pad sends one press per tap and the
   UI never sees an operating-system repeat, so the delay before the first
   repeat and the interval after it are the UI's.  The move itself is clamped in
   the container, which is what stops a held key overshooting a list. */
#define UI_PAD_NAV_REPEAT_DELAY 400
#define UI_PAD_NAV_REPEAT       120

Event W_MouseExited
(
    "mouse_exited",
    EV_DEFAULT,
    "ffi",
    "xpos ypos buttons",
    "Mouse exiting a widget event"
);
Event W_MouseEntered
(
    "mouse_entered",
    EV_DEFAULT,
    "ffi",
    "xpos ypos buttons",
    "Mouse entered a widget event"   
);
Event W_LeftMouseDragged
(
    "left_mousedragged",
    EV_DEFAULT,
    "ffi",
    "xpos ypos buttons",
    "Mouse was moved in a widget with the left button down"
);
Event W_RightMouseDragged
(
    "right_mousedragged",
    EV_DEFAULT,
    "ffi",
    "xpos ypos buttons",
    "Mouse was moved in a widget with the right button down"
);
Event W_CenterMouseDragged
(
    "center_mousedragged",
    EV_DEFAULT,
    "ffi",
    "xpos ypos buttons",
    "Mouse was moved in a widget with the center button down"
);
Event W_MouseMoved
(
    "mouse_moved",
    EV_DEFAULT,
    "ffi",
    "xpos ypos buttons",
    "Mouse was moved in a widget with no buttons down"
);
Event W_LeftMouseDown
(
    "left_mouse_down",
    EV_DEFAULT,
    "ffi",
    "xpos ypos buttons",
    "Left mouse button has been pressed down"
);
Event W_LeftMouseUp
(
    "left_mouse_up",
    EV_DEFAULT,
    "ffi",
    "xpos ypos buttons",
    "Left mouse button has been released"
);
Event W_RightMouseDown
(
    "right_mouse_down",
    EV_DEFAULT,
    "ffi",
    "xpos ypos buttons",
    "Right mouse button has been pressed down"
);
Event W_RightMouseUp
(
    "right_mouse_up",
    EV_DEFAULT,
    "ffi",
    "xpos ypos buttons",
    "Right mouse button has been released"
);
Event W_CenterMouseDown
(
    "center_mouse_down",
    EV_DEFAULT,
    "ffi",
    "xpos ypos buttons",
    "Center mouse button has been pressed down"
);
Event W_CenterMouseUp
(
    "center_mouse_up",
    EV_DEFAULT,
    "ffi",
    "xpos ypos buttons",
    "Center mouse button has been released"
);
Event W_AnyMouseDown
(
    "any_mouse_down",
    EV_DEFAULT,
    "ffi",
    "xpos ypos buttons",
    "Any mouse button has been pressed"
);
Event W_AnyMouseUp
(
    "any_mouse_up",
    EV_DEFAULT,
    "ffi",
    "xpos ypos buttons",
    "Any mouse button has been released"
);

UIWindowManager uWinMan;

CLASS_DECLARATION(UIWidget, UIWindowManager, NULL) {
    {NULL, NULL}
};

UIWindowManager::UIWindowManager()
{
    m_amidead          = false;
    m_lastbuttons      = 0;
    m_oldview          = NULL;
    m_firstResponder   = NULL;
    m_backgroundwidget = NULL;
    m_cursor           = NULL;
    m_showcursor       = true;
    m_font             = NULL;
    m_bindactive       = NULL;
    m_padNavActive     = false;
    m_padRepeatKey     = 0;
    m_padNextRepeat    = 0;
    setBorderStyle(border_none);
}

UIWindowManager::~UIWindowManager()
{
    m_amidead = true;
}

void UIWindowManager::ViewEvent(UIWidget *view, Event& event, UIPoint2D& pos, int buttons)
{
    if (!view) {
        view = m_firstResponder;
        if (!view) {
            return;
        }
    }

    if (!view->isEnabled()) {
        return;
    }

    Event *ev = new Event(event);
    ev->AddFloat(pos.x);
    ev->AddFloat(pos.y);
    ev->AddInteger(buttons);
    view->ProcessEvent(ev);
}

UIWidget *UIWindowManager::getResponder(UIPoint2D& pos)
{
    UIWidget *subview;
    UIWidget *responder;
    int       i;

    if (!m_firstResponder) {
        m_firstResponder = this;
    }

    responder = m_firstResponder;

    // path: a port-only input-state filter with zero retail decompile hits.
    if (responder != this) {
        return responder;
    }

    for (i = m_children.NumObjects(); i > 0; i--) {
        subview = m_children.ObjectAt(i);
        if (subview != m_backgroundwidget) {
            responder = subview->FindResponder(pos);
            if (responder) {
                return responder;
            }
        }
    }

    if (m_backgroundwidget) {
        return m_backgroundwidget->FindResponder(pos);
    }

    return this;
}

void UIWindowManager::Init(const UIRect2D& frame, const char *fontname)
{
    setFrame(frame);
    m_font = new UIFont(fontname);
}

void UIWindowManager::setBackgroundWidget(UIWidget *widget)
{
    m_backgroundwidget = widget;
    if (widget) {
        widget->setParent(this);
        widget->setFrame(m_frame);
    }
}

typedef enum {
    RAWPTR_WALK_FOUND = 0,
    RAWPTR_WALK_NOT_FOUND,
    RAWPTR_WALK_TRUNCATED
} rawptrWalk_t;

static rawptrWalk_t RAWPTR_InTree(UIWidget *root, UIWidget *want, int depth, int *budget)
{
    int i;

    if (!root || !want || !budget || *budget <= 0 || depth > 64) {
        return RAWPTR_WALK_TRUNCATED;     
    }
    if (root == want) {
        return RAWPTR_WALK_FOUND;
    }

    for (i = 1; i <= root->getChildren().NumObjects(); i++) {
        rawptrWalk_t r;

        if (--(*budget) <= 0) {
            return RAWPTR_WALK_TRUNCATED;
        }
        r = RAWPTR_InTree(root->getChildren().ObjectAt(i), want, depth + 1, budget);
        if (r == RAWPTR_WALK_FOUND) {
            return RAWPTR_WALK_FOUND;
        }
        if (r == RAWPTR_WALK_TRUNCATED) {
            return RAWPTR_WALK_TRUNCATED; 
        }
    }
    return RAWPTR_WALK_NOT_FOUND;
}

static void RAWPTR_CheckRawWidgetPointers(UIWidget **p, const char *name)
{
    static int    s_rows;
    static int    s_truncRows;
    static cvar_t *s_cvar;
    int           budget;
    rawptrWalk_t    r;

    if (!p || !*p) {
        return;
    }
    if (!s_cvar) {
        s_cvar = Cvar_Get("rawptr_focusStats", "0", 0);
    }
    if (s_cvar && !s_cvar->integer) {
        return;
    }

    budget = 4096;
    r = RAWPTR_InTree(&uWinMan, *p, 0, &budget);

    if (r == RAWPTR_WALK_TRUNCATED) {
        if (s_truncRows < 8) {
            Com_Printf("[RAWPTR:focus] WALK-TRUNCATED %s=%p budget|depth -> not cleared\n",
                       name, (void *)*p);
        } else if (s_truncRows == 8) {
            Com_Printf("[RAWPTR:focus] capped at 8 truncation rows; further truncations are silent "
                       "(nothing is ever cleared on a truncated walk)\n");
        }
        s_truncRows++;
        return;
    }

    if (r == RAWPTR_WALK_NOT_FOUND) {
        if (s_rows < 8) {
            Com_Printf("[RAWPTR:focus] STALE %s=%p is not in the widget tree -> cleared\n",
                       name, (void *)*p);
        } else if (s_rows == 8) {
            Com_Printf("[RAWPTR:focus] capped at 8 rows; further stale pointers are cleared silently\n");
        }
        s_rows++;
        *p = NULL;
    }
}

void UIWindowManager::UpdateViews(void)
{
    int i;
    int n;
    int x, y;

    RAWPTR_CheckRawWidgetPointers(&m_activeControl, "m_activeControl");
    RAWPTR_CheckRawWidgetPointers(&m_backgroundwidget, "m_backgroundwidget");
    RAWPTR_CheckRawWidgetPointers(&m_bindactive, "m_bindactive");

    UI_ROOT_BEGIN(UI_FAM_DISP);

    UI_ZONE_BEGIN(UI_DZ_walkup);

    update();

    UI_ZONE_END(UI_FAM_DISP, UI_DZ_walkup);
    UI_ZONE_BEGIN(UI_DZ_disp);

    if (m_backgroundwidget) {
        m_backgroundwidget->Display(m_frame, 1.0);

        n = m_children.NumObjects();
        UI_COUNT(UI_FAM_DISP, UI_DC_children, n);
        for (i = 1; i <= n; i++) {
            if (m_children.ObjectAt(i) != m_backgroundwidget) {
                m_children.ObjectAt(i)->Display(m_frame, 1.0);
            }
        }
    } else {
        UI_COUNT(UI_FAM_DISP, UI_DC_children, m_children.NumObjects());
        Display(m_frame, 1.0);
    }

    UI_ZONE_END(UI_FAM_DISP, UI_DZ_disp);
    UI_ZONE_BEGIN(UI_DZ_cursor);

    /* `uiHasMouse' is the UI's desire for a pointer, and touch sets it too, so
       it cannot decide whether a pointer exists.  `IN_HasRealMouse' is the
       platform answer: on Android it stays false until a real mouse event, and
       on the desktop it is true, so the cursor there is exactly retail's. */
    if (m_cursor && m_showcursor && uid.uiHasMouse && IN_HasRealMouse()) {
        vec4_t col;

        setCursor(m_cursorname);

        set2D();

        {
            VectorSet4(col, 1, 1, 1, 1);
            uii.Rend_SetColor(col);

            int cursorX = uid.mouseX;
            int cursorY = uid.mouseY;

            if (cursorX < 0) {
                cursorX = 0;
            } else if (cursorX > uid.vidWidth) {
                cursorX = uid.vidWidth;
            }
            if (cursorY < 0) {
                cursorY = 0;
            } else if (cursorY > uid.vidHeight) {
                cursorY = uid.vidHeight;
            }

            uii.Rend_DrawPicStretched(cursorX, cursorY, 0, 0, 0, 0, 1, 1, m_cursor->GetMaterial());
        }

        m_font->setColor(UWhite);

        UI_ZONE_BEGIN(UI_DZ_printstr);
        if (UI_GetCvarInt("ui_drawcoords", 0)) {
            x = uid.mouseX + 10;
            y = uid.mouseY - 10;
            m_font->Print(x, y, va("%d:%d", uid.mouseX, uid.mouseY), -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);
        }
        UI_ZONE_END(UI_FAM_DISP, UI_DZ_printstr);
    }

    UI_ZONE_END(UI_FAM_DISP, UI_DZ_cursor);

    UI_ROOT_END(UI_FAM_DISP);
}

void UIWindowManager::ChangeResolution(void)
{
    int i;

    setFrame(UIRect2D(0, 0, (float)uid.vidWidth, (float)uid.vidHeight));

    for (i = 1; i <= m_children.NumObjects(); i++) {
        m_children.ObjectAt(i)->ChangeResolution();
    }
}

void UIWindowManager::ServiceEvents(void)
{
    UIPoint2D         pos;
    unsigned int      buttons;
    SafePtr<UIWidget> view;

    pos.x   = uid.mouseX;
    pos.y   = uid.mouseY;
    buttons = uid.mouseFlags;

    if (m_bindactive) {
        view = m_bindactive;
    } else {
        view = getResponder(pos);
    }

    SafePtr<UIWidget> b = m_oldview;

    /* The mouse takes the highlight back the moment it moves or a button is pressed: the pad never
       blocks it, and with no pad in use the state here is never entered. */
    if (m_padNavActive && (m_oldpos != pos || buttons != m_lastbuttons)) {
        ClearPadFocus();
    }

    /* A held direction key repeats at the UI's own rate.  The key state is the
       engine's, so a D-pad button held down and a stick pushed past its
       deadzone both keep the key down until they are released. */
    if (m_padNavActive && m_padRepeatKey) {
        if (!Key_IsDown(m_padRepeatKey)) {
            m_padRepeatKey = 0;
        } else if ((int)(uid.time - m_padNextRepeat) >= 0) {
            int dir = PadNavDirection(m_padRepeatKey);

            m_padNextRepeat = uid.time + UI_PAD_NAV_REPEAT;

            if (dir != 0 && !PadFocusRepeat(m_padRepeatKey)) {
                MovePadFocus(dir);
            }
        }
    }

    if (b != view) {
        ViewEvent(m_oldview, W_MouseExited, pos, buttons);
        ViewEvent(view, W_MouseEntered, pos, buttons);
    }

    if (m_oldpos != pos) {
        if ((buttons & 1) && (m_lastbuttons & 1)) {
            ViewEvent(view, W_LeftMouseDragged, pos, buttons);
        }
        if ((buttons & 2) && (m_lastbuttons & 2)) {
            ViewEvent(view, W_RightMouseDragged, pos, buttons);
        }
        if ((buttons & 4) && (m_lastbuttons & 4)) {
            ViewEvent(view, W_CenterMouseDragged, pos, buttons);
        }
        if (!buttons) {
            ViewEvent(view, W_MouseMoved, pos, 0);
        }
    }

    if ((buttons & 1) != (m_lastbuttons & 1)) {
        if (buttons & 1) {
            uii.UI_WantsKeyboard();

            static int uivk_press;
            if (PROBE_Throttle(&uivk_press, 8, 64)) {
                UIWidget *uivkv    = view;
                UIWidget *uivktop  = uivkv;
                int       uivkidx  = 0;
                int       uivktotal = GetChildCount();

                while (uivktop && uivktop->getParent() != NULL && uivktop->getParent() != this) {
                    uivktop = uivktop->getParent();
                }

                for (int uivki = 1; uivki <= uivktotal; uivki++) {
                    if (GetChild(uivki) == uivktop) {
                        uivkidx = uivki;
                        break;
                    }
                }

                Menu *uivkmenu = menuManager.CurrentMenu();

                UIVK_PROBE("press",
                           "widget='%s' class='%s' manager=%d top='%s' walkIndex=%d of=%d menu='%s' gamefocus=%d activeControl='%s' guimouse=%d\n",
                           (uivkv && uivkv->getName()) ? uivkv->getName() : "(null)",
                           (uivkv && uivkv->getClassname()) ? uivkv->getClassname() : "(null)",
                           (uivkv == this) ? 1 : 0,
                           (uivktop && uivktop->getName()) ? uivktop->getName() : "(null)",
                           uivkidx,
                           uivktotal,
                           (uivkmenu && uivkmenu->m_name.length()) ? uivkmenu->m_name.c_str() : "(none)",
                           (uivkmenu && uivkmenu->allowGameFocus()) ? 1 : 0,
                           (m_activeControl && m_activeControl->getName()) ? m_activeControl->getName() : "(null)",
                           uid.uiHasMouse ? 1 : 0);
            }

            ActivateControl(view);
            ViewEvent(view, W_LeftMouseDown, pos, buttons);
        } else {
            ViewEvent(view, W_LeftMouseUp, pos, buttons);
        }
    }
    if ((buttons & 2) != (m_lastbuttons & 2)) {
        if (buttons & 2) {
            ViewEvent(view, W_RightMouseDown, pos, buttons);
        } else {
            ViewEvent(view, W_RightMouseUp, pos, buttons);
        }
    }
    if ((buttons & 4) != (m_lastbuttons & 4)) {
        if (buttons & 4) {
            ViewEvent(view, W_CenterMouseDown, pos, buttons);
        } else {
            ViewEvent(view, W_CenterMouseUp, pos, buttons);
        }
    }
    if (buttons != m_lastbuttons) {
        ViewEvent(view, W_AnyMouseDown, pos, buttons);
        ViewEvent(view, W_AnyMouseUp, pos, buttons);
    }

    m_lastbuttons = buttons;
    m_oldpos      = pos;
    m_oldview     = view;

    if (!m_cursor) {
        setCursor("sysimg/2d/mouse_arrow");   
    }
}

void UIWindowManager::setFirstResponder(UIWidget *responder)
{
    if (!responder) {
        m_firstResponder = this;
    } else {
        m_firstResponder = responder;
    }
}

UIWidget *UIWindowManager::getFirstResponder(void)
{
    if (m_firstResponder != this) {
        return m_firstResponder;
    }
    return NULL;
}

UIWidget *UIWindowManager::ActiveControl(void)
{
    return m_activeControl;
}

void UIWindowManager::ActivateControl(UIWidget *control)
{
    UIWidget *wid;

    if (!control) {
        wid = m_activeControl;

        if (wid) {
            wid->ProcessEvent(EV_Widget_Deactivate);
            wid->SendSignal(W_Deactivated);
        }

        m_activeControl = NULL;
        return;
    }

    for (wid = control; wid && wid != this; wid = wid->getParent()) {
        if (wid->CanActivate() && wid->isEnabled()) {
            break;
        }
    }

    if (!wid || wid == this) {
        return;
    }

    if (wid->CanActivate() && wid->isEnabled()) {
        UIWidget *active = ActiveControl();

        if (wid != active) {
            if (active) {
                m_activeControl = NULL;
                active->ProcessEvent(EV_Widget_Deactivate);
                active->SendSignal(W_Deactivated);
            }

            if (!active || !ActiveControl()) {
                m_activeControl = wid;
                wid->ActivateOrder();
                wid->BringToFrontPropogated();
                wid->ProcessEvent(EV_Widget_Activate);
                wid->SendSignal(W_Activated);
            }
        }
    }
}

void UIWindowManager::DeactivateCurrentControl(void)
{
    UIWidget *wid = m_activeControl;

    if (wid) {
        m_activeControl = NULL;
        wid->ProcessEvent(EV_Widget_Deactivate);
        wid->SendSignal(W_Deactivated);
    }
}

UIWidget *UIWindowManager::FindNextControl(UIWidget *control)
{
    UIWidget *widget;
    bool      back_up;

    if (!control) {
        control = this;
    }

    while (control) {
        widget = control->getFirstChild();
        if (widget) {
            if (widget->isEnabled() && widget->CanActivate()) {
                return widget;
            }

            control = widget;
        } else {
            widget = control->getNextSibling();
            if (widget) {
                if (widget->isEnabled() && widget->CanActivate()) {
                    return widget;
                }

                control = widget;
            } else {
                back_up = true;

                while (control->getParent() && back_up) {
                    control = control->getParent();
                    if (control->getNextSibling()) {
                        control = control->getNextSibling();

                        if (control->isEnabled() && control->CanActivate()) {
                            return control;
                        }

                        back_up = false;
                    }
                }

                if (control == this) {
                    break;
                }
            }
        }
    }

    return NULL;
}

UIWidget *UIWindowManager::FindPrevControl(UIWidget *control)
{
    UIWidget *tmp_control;
    UIWidget *next_control;

    if (control) {
        tmp_control = this;
        do {
            next_control = tmp_control;
            tmp_control  = FindNextControl(tmp_control);

            if (!tmp_control) {
                return NULL;
            }
        } while (tmp_control != control);
    } else {
        tmp_control = this;
        do {
            next_control = tmp_control;
            tmp_control  = FindNextControl(tmp_control);
        } while (tmp_control);
    }

    if (next_control != this) {
        return next_control;
    }

    return NULL;
}

// (refreshCursor / IN_SetCursorFromImage) does not exist in retail -- the cursor is the
void UIWindowManager::setCursor(const char *string)
{
    m_cursor = RegisterShader(string);

    if (m_cursor) {
        m_cursorname = string;
    } else {
        uii.Sys_Printf("Could not register shader '%s'\n", string);
    }
}

void UIWindowManager::showCursor(bool show)
{
    m_showcursor = show;
}

void UIWindowManager::CharEvent(int ch)
{
    if (m_activeControl) {
        m_activeControl->CharEvent(ch);
    }
}

/* `dir` is the child-order step for a navigation key: up/left walk back, down/ right walk on. The
   left stick produces its own key codes, not arrows, so it is a direction here too -- its deadzone
   is the SDL layer's joy threshold and is already applied before the key is emitted. */
int UIWindowManager::PadNavDirection(int key)
{
    switch (key) {
    case K_UPARROW:
    case K_PAD0_LEFTSTICK_UP:
    case K_LEFTARROW:
    case K_PAD0_LEFTSTICK_LEFT:
        return -1;

    case K_DOWNARROW:
    case K_PAD0_LEFTSTICK_DOWN:
    case K_RIGHTARROW:
    case K_PAD0_LEFTSTICK_RIGHT:
        return 1;

    default:
        break;
    }

    return 0;
}

qboolean UIWindowManager::PadHorizontal(int key)
{
    return key == K_LEFTARROW || key == K_RIGHTARROW
        || key == K_PAD0_LEFTSTICK_LEFT || key == K_PAD0_LEFTSTICK_RIGHT;
}

UIWidget *UIWindowManager::PadFocusedWidget(void)
{
    Menu     *menu = menuManager.CurrentMenu();
    UIWidget *cont;

    if (!menu) {
        return NULL;
    }

    cont = menu->GetContainerWidget();

    if (!cont || !cont->isSubclassOf(UIWidgetContainer)) {
        return NULL;
    }

    return ((UIWidgetContainer *)cont)->getFocusWidget();
}

/* Held on a row that owns its horizontal, the row keeps the key: the slider steps,
   a switchbox only holds, because an option runs its command every step. */
bool UIWindowManager::PadFocusRepeat(int key)
{
    UIWidget *focused;

    if (!PadHorizontal(key)) {
        return false;
    }

    focused = PadFocusedWidget();

    if (!focused) {
        return false;
    }

    if (focused->isSubclassOf(UISlider)) {
        focused->KeyEvent(key, 0);
    }

    return true;
}

bool UIWindowManager::MovePadFocus(int dir)
{
    Menu      *menu = menuManager.CurrentMenu();
    UIWidget  *cont;

    if (!menu) {
        return false;
    }

    cont = menu->GetContainerWidget();

    if (!cont || !cont->isSubclassOf(UIWidgetContainer)) {
        return false;
    }

    return ((UIWidgetContainer *)cont)->moveFocus(dir) ? true : false;
}

void UIWindowManager::ClearPadFocus(void)
{
    Menu     *menu = menuManager.CurrentMenu();
    UIWidget *cont;

    m_padNavActive  = false;
    m_padRepeatKey  = 0;

    if (!menu) {
        return;
    }

    cont = menu->GetContainerWidget();

    if (cont && cont->isSubclassOf(UIWidgetContainer)) {
        ((UIWidgetContainer *)cont)->clearFocusWidget();
    }
}

/* The pad's activation key runs the mouse's own click path at the widget's centre: the container
   focus has already put it in the mouse-over state, so a synthesized press and release reaches
   exactly the code a click reaches -- UIButtonBase::Released -> Action(), a checkbox's toggle, and
   so on. Nothing new is added to the widget layer for activation. */
bool UIWindowManager::ActivateFocusedWidget(void)
{
    Menu              *menu = menuManager.CurrentMenu();
    UIWidgetContainer *cont;
    UIWidget          *focused;
    UIPoint2D          pos;
    Event              down(W_LeftMouseDown);
    Event              up(W_LeftMouseUp);

    if (!menu) {
        return false;
    }

    UIWidget *containerWidget = menu->GetContainerWidget();

    if (!containerWidget || !containerWidget->isSubclassOf(UIWidgetContainer)) {
        return false;
    }

    cont    = (UIWidgetContainer *)containerWidget;
    focused = cont->getFocusWidget();

    if (!focused || !focused->isEnabled() || !focused->getShow()) {
        return false;
    }

    ActivateControl(focused);

    /* A slider has nothing to click and a synthesized press would drop the thumb at
       the centre of its track; its value is the left and right keys. */
    if (focused->isSubclassOf(UISlider)) {
        return true;
    }

    pos = focused->getGlobalPoint(UIPoint2D(focused->getSize().width * 0.5f, focused->getSize().height * 0.5f));

    ViewEvent(focused, down, pos, 1);
    ViewEvent(focused, up, pos, 0);

    return true;
}

qboolean UIWindowManager::KeyEvent(int key, unsigned int time)
{
    int dir;

    if (m_bindactive) {
        m_bindactive->KeyEvent(key, time);
        return true;
    }

    dir = PadNavDirection(key);

    /* menus walk by screen order; TAB joins the walk only where a menu owns the key */
    if ((dir != 0 || key == K_TAB) && menuManager.CurrentMenu()) {
        if (!dir) {
            dir = 1;
        }

        m_padNavActive = true;

        /* The widgets that read arrows themselves -- listbox, table, console,
           text field, slider -- get the key first and keep it. */
        if (m_activeControl && m_activeControl->KeyEvent(key, time)) {
            return true;
        }

        /* A horizontal key steps the focused row instead of walking off it: a slider's
           value and a switchbox's option are the only horizontal move that row has. */
        if (PadHorizontal(key)) {
            UIWidget *focused = PadFocusedWidget();

            if (focused && focused->KeyEvent(key, time)) {
                m_padRepeatKey  = key;
                m_padNextRepeat = uid.time + UI_PAD_NAV_REPEAT_DELAY;
                return true;
            }
        }

        if (MovePadFocus(dir)) {
            m_padRepeatKey  = key;
            m_padNextRepeat = uid.time + UI_PAD_NAV_REPEAT_DELAY;
        }

        return true;
    }

    if (key == K_ENTER && m_padNavActive) {
        if (ActivateFocusedWidget()) {
            return true;
        }
    }

    if (m_activeControl && m_activeControl->KeyEvent(key, time)) {
        return true;
    }

    return (m_activeControl || menuManager.CurrentMenu() || UI_IsHudActive())
           && (key == K_MOUSE1 || key == K_MOUSE2);
}

// Retail's comparator is deliberately NON-TOTAL: widgetContainerCompare @0x000B4450
void UIWindowManager::sortWidgetContainers(void)
{
    // this sort uses never answers 0 on a tie (widgetContainerCompare @0x000B4450: fcmpu /
    static int gltrace_sortPass;
    PROBE("sortpass", "pass=%d children=%d", ++gltrace_sortPass, m_children.NumObjects());

    {
        int n = m_children.NumObjects();

        for (int i = 2; i <= n; i++) {
            UIWidget *cur = m_children.ObjectAt(i);
            float     d   = cur->getDepth();
            int       j   = i - 1;

            while (j >= 1 && m_children.ObjectAt(j)->getDepth() < d) {
                m_children.SetObjectAt(j + 1, m_children.ObjectAt(j));
                j--;
            }

            m_children.SetObjectAt(j + 1, cur);
        }
    }
}

// PAINT TEMP PROBE -- PORT-ONLY, TEMPORARY, NEVER MERGE.  Read side of the sorted
int UIWindowManager::GetChildCount(void)
{
    return m_children.NumObjects();
}

UIWidget *UIWindowManager::GetChild(int index)
{
    return m_children.ObjectAt(index);
}

void UIWindowManager::CreateMenus(void)
{
    int i;
    int n;

    menuManager.DeleteAllMenus();

    n = uWinMan.m_children.NumObjects();
    for (i = 1; i <= n; i++) {
        UIWidget *w = uWinMan.m_children.ObjectAt(i);
        if (w->isSubclassOf(UIWidgetContainer)) {
            UIWidgetContainer *wid = (UIWidgetContainer *)w;
            Menu              *m   = new Menu(w->getName());

            m->setFullscreen(wid->isFullscreen());
            m->setVidMode(wid->getVidMode());
            m->AddMenuItem(w);
            m->ForceHide();
        }
    }

    // list twice, and with retail's tie-breaking predicate (never answers 0) a second
}

UIReggedMaterial *UIWindowManager::RegisterShader(const str& name)
{
    int               i;
    UIReggedMaterial *regged;

    for (i = 1; i <= m_materiallist.NumObjects(); i++) {
        regged = m_materiallist.ObjectAt(i);
        if (regged->GetName() == name) {
            return regged;
        }
    }

    regged = new UIReggedMaterial;
    regged->SetMaterial(name);

    m_materiallist.AddObject(regged);
    return regged;
}

UIReggedMaterial *UIWindowManager::RefreshShader(const str& name)
{
    int               i;
    UIReggedMaterial *regged;

    for (i = 1; i <= m_materiallist.NumObjects(); i++) {
        regged = m_materiallist.ObjectAt(i);
        if (regged->GetName() == name) {
            regged->RefreshMaterial();
            return regged;
        }
    }

    regged = new UIReggedMaterial;
    regged->SetMaterial(name);

    m_materiallist.AddObject(regged);
    return regged;
}

void UIWindowManager::CleanupShadersFromList(void)
{
    int i;
    int num;

    num = m_materiallist.NumObjects();
    for (i = 1; i <= num; i++) {
        UIReggedMaterial *regged = m_materiallist.ObjectAt(i);
        regged->CleanupMaterial();
    }
}

void UIWindowManager::RefreshShadersFromList(void)
{
    int i;
    int num;

    num = m_materiallist.NumObjects();
    for (i = 1; i <= num; i++) {
        UIReggedMaterial *regged = m_materiallist.ObjectAt(i);
        regged->ReregisterMaterial();
    }
}

UIReggedModel *UIWindowManager::RegisterModel(const str& name)
{
    int            i;
    UIReggedModel *regged;

    for (i = 1; i <= m_modellist.NumObjects(); i++) {
        regged = m_modellist.ObjectAt(i);
        if (regged->GetName() == name) {
            return regged;
        }
    }

    regged = new UIReggedModel;
    regged->SetModel(name);

    m_modellist.AddObject(regged);
    return regged;
}

void UIWindowManager::RegisterModelsFromList(void)
{
    int i;
    int num;

    num = m_modellist.NumObjects();
    for (i = 1; i <= num; i++) {
        m_modellist.ObjectAt(i)->ReregisterModel();
    }
}

void UIWindowManager::DeactivateCurrentSmart(void)
{
    UIWidget *toset;
    UIWidget *checking;
    UIWidget *sibling;

    toset = NULL;

    if (!m_activeControl) {
        return;
    }

    for (checking = m_activeControl; checking; checking = checking->getParent()) {
        if (!checking->getParent() || (checking->getParent() && checking->getParent()->IsDying())) {
            continue;
        }

        for (sibling = checking->getPrevSibling(); sibling && !toset; sibling = sibling->getPrevSibling()) {
            if (sibling->CanActivate()) {
                toset = sibling;
            }
        }

        if (toset) {
            break;
        }

        for (sibling = checking->getLastSibling(); sibling && sibling != checking && !toset;
             sibling = sibling->getPrevSibling()) {
            if (sibling->CanActivate()) {
                toset = sibling;
            }
        }

        if (toset) {
            break;
        }
    }

    if (!toset) {
        uWinMan.DeactivateCurrentControl();
        return;
    }

    if (toset == &uWinMan) {
        Menu     *menu;
        UIWidget *newWid = NULL;

        menu = menuManager.CurrentMenu();

        if (menu) {
            newWid = menu->GetContainerWidget();
        }

        if (newWid) {
            toset = newWid;
        }
    }

    uWinMan.ActivateControl(toset);
}

bool UIWindowManager::IsDead(void)
{
    return m_amidead;
}

int UIWindowManager::AddBinding(const str& binding)
{
    int      i;
    Binding *bind;

    for (i = 1; i <= bindings.NumObjects(); i++) {
        Binding *b = bindings.ObjectAt(i);

        if (b->binding == binding) {
            return i;
        }
    }

    bind          = new Binding;
    bind->binding = binding;
    uii.Key_GetKeysForCommand(binding, &bind->key1, &bind->key2);
    bindings.AddObject(bind);

    return bindings.IndexOfObject(bind);
}

str UIWindowManager::GetKeyStringForCommand(str command, int index, qboolean alternate, int *key1, int *key2)
{
    str      s;
    str      cmd;
    Binding *bind;

    if (index > bindings.NumObjects()) {
        return "Invalid Command";
    }

    bind = bindings.ObjectAt(index);

    if (command == bind->binding) {
        if (bind->key1 < 0 || bind->key2 < 0) {
            uii.Key_GetKeysForCommand(command, &bind->key1, &bind->key2);
        }

        if (bind->key2 >= 0) {
            cmd = uii.Key_GetCommandForKey(bind->key2);
            if (cmd != bind->binding) {
                bind->key2 = -1;
            }
        }
        if (bind->key1 >= 0) {
            cmd = uii.Key_GetCommandForKey(bind->key1);
            if (cmd != bind->binding) {
                if (bind->key2 < 0) {
                    bind->key1 = -1;
                } else {
                    bind->key1 = bind->key2;
                    bind->key2 = -1;
                }
            }
        }

        if (alternate) {
            if (bind->key2 >= 0) {
                s = uii.Key_KeynumToString(bind->key2);
            } else {
                s = "$$None$$";
            }
        } else {
            if (bind->key1 >= 0) {
                s = uii.Key_KeynumToString(bind->key1);
            } else {
                s = "$$None$$";
            }
        }

        if (key1) {
            *key1 = bind->key1;
        }
        if (key2) {
            *key2 = bind->key2;
        }
    } else {
        s = "Invalid Command";
    }

    return s;
}

void UIWindowManager::BindKeyToCommand(str command, int key, int index, qboolean alternate)
{
    Binding *bind = bindings.ObjectAt(index);

    if (bind->binding == command || !bind->binding.length()) {
        if (!command.length()) {
            uii.Key_SetBinding(key, "");
            return;
        }

        if (!alternate) {
            if (bind->key1 == key) {
                return;
            }

            if (bind->key2 == key) {
                uii.Key_SetBinding(bind->key1, "");
                uii.Key_SetBinding(bind->key2, "");
                bind->key1 = key;
                bind->key2 = -1;
            } else {
                uii.Key_SetBinding(bind->key1, "");
            }

            if (!command.length()) {
                uii.Key_SetBinding(key, "");
            } else {
                uii.Key_SetBinding(key, command.c_str());
            }
        } else {
            if (bind->key2 == key) {
                return;
            }

            if (bind->key1 == key) {
                uii.Key_SetBinding(bind->key2, "");
            } else {
                uii.Key_SetBinding(bind->key2, "");
                bind->key2 = key;

                if (!command.length()) {
                    uii.Key_SetBinding(key, "");
                } else {
                    uii.Key_SetBinding(key, command.c_str());
                }
            }
        }
    } else {
        warning(
            "UIWindowManager::BindKeyToCommand",
            "Invalid bind command '%s' for widget command '%s'\n",
            command.c_str(),
            bind->binding.c_str()
        );
    }
}

UIWidget *UIWindowManager::BindActive(void)
{
    return m_bindactive;
}

void UIWindowManager::SetBindActive(UIWidget *w)
{
    m_bindactive = w;
}

// retail does NOT free m_modellist (the list RegisterModelsFromList re-walks) and does
void UIWindowManager::Shutdown(void)
{
    int i;

    for (i = m_materiallist.NumObjects(); i > 0; i--) {
        UIReggedMaterial *mat = m_materiallist.ObjectAt(i);
        m_materiallist.RemoveObjectAt(i);

        delete mat;
    }

    for (i = bindings.NumObjects(); i > 0; i--) {
        Binding *b = bindings.ObjectAt(i);
        bindings.RemoveObjectAt(i);

        delete b;
    }

    // Platform-layer plumbing, not retail: the port holds an OS cursor handle for the
    // to free.  m_modellist is NOT freed -- retail leaves it alone and
    m_cursor = NULL;
    IN_FreeCursor();

    UIWidget::Shutdown();
}



bool UIWindowManager::DialogExists(void)
{
    int       i;
    UIWidget *pWidg;

    for (i = 1; i <= m_children.NumObjects(); i++) {
        pWidg = m_children.ObjectAt(i);
        if (pWidg->isSubclassOf(UIDialog)) {
            return true;
        }
    }

    return false;
}

void UIWindowManager::RemoveAllDialogBoxes(void)
{
    int       i;
    UIWidget *pWidg;

    for (i = m_children.NumObjects(); i > 0; i--) {
        pWidg = m_children.ObjectAt(i);
        if (pWidg->isSubclassOf(UIDialog)) {
            UIDialog *pDlg = (UIDialog *)pWidg;

            pDlg->m_cancel->m_command += "\n";
            uii.Cmd_Stuff(pDlg->m_cancel->m_command);
        }
    }
}

