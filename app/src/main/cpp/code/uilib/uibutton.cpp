
#include "ui_local.h"

Event W_Button_Pressed
(
    "button_pressed",
    EV_DEFAULT,
    NULL,
    NULL,
    "Signal that the button was pressed"
);

Event W_Button_HoverSound
(
    "hoversound",
    EV_DEFAULT,
    "s",
    "soundName",
    "Set the sound to play when this button is hovered over"
);

Event W_Button_HoverCommand
(
    "hovercommand",
    EV_DEFAULT,
    "s",
    "string",
    "Set the command to execute when this button is hovered over"
);

Event W_Button_MouseAwayCommand
(
    "mouseawaycommand",
    EV_DEFAULT,
    "s",
    "string",
    "Set the command to execute when the mouse exits this button"
);

Event W_Button_ServerThreadToRun
(
    "ServerThreadToRun",
    EV_DEFAULT,
    "s",
    "threadName",
    "Adds to the list of server thread to run when button is pressed"
);

Event W_Button_ClearServerThreads
(
    "ClearServerThreads",
    EV_DEFAULT,
    NULL,
    NULL,
    "Clears the list of server threads."
);

Event W_Button_DialogThreadToRun
(
    "DialogThreadToRun",
    EV_DEFAULT,
    "s",
    "threadName",
    "Adds to the list of dialog threads to run when the button is pressed."
);

Event W_Button_ClearDialogThreads
(
    "ClearDialogThread",   
    EV_DEFAULT,
    NULL,
    NULL,
    "Clears all dialog threads."
);

Event W_Button_ServerVarToSet
(
    "ServerVarToSet",
    EV_DEFAULT,
    "s",
    "varName",
    "Adds to the list of server variables to set when button is pressed\n"
    "This name is the name of the cvar and must start with ClientVar_\n"
    "This variable maps to a level variable in the script on the sever side"
);

Event W_Button_ServerThreadProperty
(
    "serverThreadProperty",
    EV_DEFAULT,
    "ss",
    "name value",
    "Sets a property on the server threads this button runs."
);

Event EV_Button_StayPressed
(
    "staypressed",
    EV_DEFAULT,
    NULL,
    NULL,
    "Keeps the button in the armed state."
);

CLASS_DECLARATION(UIWidget, UIButtonBase, NULL) {
    {&W_LeftMouseDown,             &UIButtonBase::Pressed              },
    {&W_LeftMouseUp,               &UIButtonBase::Released             },
    {&W_MouseEntered,              &UIButtonBase::MouseEntered         },
    {&W_MouseExited,               &UIButtonBase::MouseExited          },
    {&W_Button_HoverSound,         &UIButtonBase::SetHoverSound        },
    {&W_Button_HoverCommand,       &UIButtonBase::SetHoverCommand      },
    {&W_Button_MouseAwayCommand,   &UIButtonBase::SetMouseExitedCommand},
    {&W_Button_ServerVarToSet,         &UIButtonBase::AddServerVarToSet         },
    {&W_Button_ServerThreadProperty,   &UIButtonBase::setServerThreadProperty   },
    {&W_Button_ServerThreadToRun,  &UIButtonBase::AddServerThreadToRun },
    {&W_Button_ClearServerThreads, &UIButtonBase::ClearServerThreads   },
    {&W_Button_DialogThreadToRun,  &UIButtonBase::SetDialogThreadToRun },
    {&W_Button_ClearDialogThreads, &UIButtonBase::ClearDialogThreads   },
    {&EV_Button_StayPressed,       &UIButtonBase::stayPressed    },
    {NULL,                         NULL                                }
};

UIButtonBase::UIButtonBase()
{
    m_mouseState  = M_NONE;
    m_stayPressed = false;
    m_hasServerThreadProperty = false;
    AllowActivate(true);
}

void UIButtonBase::AddServerVarToSet(Event *ev)
{
    static const char *const prefix = "ClientVar_";

    str arg = ev->GetString(1);

    if (strnicmp(arg.c_str(), prefix, strlen(prefix))) {
        Com_Printf("%s incorrect format, server var must start with ClientVar_\n", arg.c_str());
        return;
    }

    m_serverVars.AddObject(new str(arg.c_str() + strlen(prefix)));
}

void UIButtonBase::setServerThreadProperty(Event *ev)
{
    m_serverThreadPropertyName  = ev->GetString(1);
    m_serverThreadPropertyValue = ev->GetString(2);
    m_hasServerThreadProperty   = true;
}

void UIButtonBase::updateGameplayProperty(void)
{
    UIWidget::updateGameplayProperty();

    if (!m_hasServerThreadProperty) {
        return;
    }

    char command[128];

    uii.Gameplay_GetStringProperty(
        m_serverThreadPropertyName.c_str(), m_serverThreadPropertyValue.c_str(), command, sizeof(command)
    );

    if (!strlen(command)) {
        return;
    }

    if (!strcmp(command, m_serverThreadCommand.c_str())) {
        return;
    }

    for (int i = 1; i <= m_serverThreads.NumObjects(); i++) {
        str *entry = m_serverThreads.ObjectAt(i);

        if (entry->length() == m_serverThreadCommand.length()
            && !strcmp(entry->c_str(), m_serverThreadCommand.c_str())) {
            m_serverThreads.RemoveObjectAt(i);
            delete entry;
        }
    }

    m_serverThreads.AddObject(new str(command));
    m_serverThreadCommand = command;
}

void UIButtonBase::stayPressed(Event *ev)
{
    m_stayPressed = true;
}

void UIButtonBase::Pressed(Event *ev)
{
    if (m_widgetState == WIDGET_STATE_DISABLED) {
        return;
    }

    m_mouseState = M_DRAGGING;

    setSelectedState();

    ev->SetSourceObject(this);

    if (m_parent) {
        m_parent->Pressed(ev);
    }

    setArmedState();
    uWinMan.setFirstResponder(this);
}

void UIButtonBase::Released(Event *ev)
{
    if (m_mouseState != M_DRAGGING) {
        m_mouseState = M_NONE;
        return;
    }

    if (uWinMan.getFirstResponder() == this) {
        uWinMan.setFirstResponder(NULL);
    }

    m_mouseState = M_NONE;

    if (ev) {
        if (!getClientFrame().contains(MouseEventToClientPoint(ev))) {
            setNormalState();
            return;
        }
    }

    Action();

    if (ev) {
        if (getClientFrame().contains(MouseEventToClientPoint(ev))) {
            SendSignal(W_Button_Pressed);
        }
    }

    if (m_stayPressed) {
        setArmedState();
    } else {
        setHoverState();
    }
}

// PRESERVE, NEVER CREATE: the armed look is only put back if it was already there before
void UIButtonBase::MouseEntered(Event *ev)
{
    WidgetState prev = m_widgetState;

    UIWidget::MouseEntered(ev);

    if (prev == WIDGET_STATE_ARMED && m_stayPressed) {
        setWidgetState(WIDGET_STATE_ARMED);
    }
}

void UIButtonBase::MouseExited(Event *ev)
{
    WidgetState prev = m_widgetState;

    UIWidget::MouseExited(ev);

    if (prev == WIDGET_STATE_ARMED && m_stayPressed) {
        setWidgetState(WIDGET_STATE_ARMED);
    }
}

void UIButtonBase::SetHoverSound(Event *ev)
{
    m_hoverSound = ev->GetString(1);
}

void UIButtonBase::SetHoverCommand(Event *ev)
{
    m_hoverCommand = ev->GetString(1);
}

void UIButtonBase::SetMouseExitedCommand(Event *ev)
{
    m_mouseExitedCommand = ev->GetString(1);
}

void UIButtonBase::AddServerThreadToRun(Event *ev)
{
    m_serverThreads.AddObject(new str(ev->GetString(1)));
}

void UIButtonBase::ClearServerThreads(Event *ev)
{
    int i;

    for (i = m_serverThreads.NumObjects(); i > 0; i--) {
        delete m_serverThreads.ObjectAt(i);
    }

    m_serverThreads.ClearObjectList();
}

void UIButtonBase::SetDialogThreadToRun(Event *ev)
{
    m_dialogThreadToRun = ev->GetString(1);
}

void UIButtonBase::ClearDialogThreads(Event *ev)
{
    m_dialogThreadToRun = "";
}

void UIButtonBase::setData(const str& data)
{
    setTitle(data);
}

const char *UIButtonBase::getData(void)
{
    return m_title;
}

void UIButtonBase::Action(void)
{
    if (m_clicksound.length()) {
        uii.Snd_PlaySound(m_clicksound.c_str(), CHAN_MENU2);
    }

    if (m_command.length()) {
        str stuff = m_command + "\n";
        uii.Cmd_Stuff(stuff.c_str());
    }

    // container (AddServerVarToSet @0x00051C80) but never drained it on release, so
    int numVars = m_serverVars.NumObjects();

    for (int i = 1; i <= numVars; i++) {
        const char *name = m_serverVars.ObjectAt(i)->c_str();

        if (!name || !name[0]) {
            continue;
        }

        str cvarName = "ClientVar_";
        cvarName += name;

        const char *value = UI_GetCvarString(cvarName.c_str(), (char *)"");

        str stuff = "clientsetvar ";
        stuff += name;
        stuff += " ";

        if (value) {
            stuff += value;
        }

        stuff += "\n";
        uii.Cmd_Stuff(stuff.c_str());
    }

    int numThreads = m_serverThreads.NumObjects();

    for (int i = 1; i <= numThreads; i++) {
        str thread = *m_serverThreads.ObjectAt(i);

        if (thread.length()) {
            str stuff = "clientrunthread ";
            stuff += thread;
            stuff += "\n";
            uii.Cmd_Stuff(stuff.c_str());
        }
    }

    if (m_dialogThreadToRun.length()) {
        str stuff = "dialogrunthread " + m_dialogThreadToRun + "\n";
        uii.Cmd_Stuff(stuff.c_str());
    }
}

CLASS_DECLARATION(UIButtonBase, UIButton, NULL) {
    {&EV_Layout_PressedShader, &UIButton::pressedShader},
    {NULL,                     NULL                    }
};

UIButton::UIButton()
{
    setBorderStyle(border_none);
}

void UIButton::pressedShader(Event *ev)
{
    LayoutArmedShader(ev);
}

// Retail UIButton::Draw @0x00052800 (244 B) is NOT the MOHAA
void UIButton::Draw(void)
{
    UIWidget::Draw();

    UIRect2D clientFrame = getClientFrame();

    m_font->setColor(getForegroundColor());
    m_font->setAlpha(m_local_alpha);

    clientFrame.pos.x += 1.0f;
    clientFrame.pos.y += 1.0f;

    m_font->PrintJustified(
        clientFrame,
        m_iFontAlignmentHorizontal,
        m_iFontAlignmentVertical,
        m_title.c_str(), getTextShadowColor(), m_bVirtual);
}


qboolean UIButton::KeyEvent(int key, unsigned int time)
{
    return UIWidget::KeyEvent(key, time);
}

CLASS_DECLARATION(USignal, ToggleCVar, NULL) {
    {NULL, NULL}
};

ToggleCVar::ToggleCVar()
{
    m_button = 0;
}

ToggleCVar::ToggleCVar(UIButton *button, const char *cvar)
    : ToggleCVar()
{
    setButton(button);

    if (cvar) {
        setCVar(cvar);
    }
}

void ToggleCVar::Press(Event *ev)
{
    if (m_cvarname.length()) {
        UI_SetCvarInt(m_cvarname.c_str(), UI_GetCvarInt(m_cvarname.c_str(), 0) == 0);
    }
}

void ToggleCVar::setCVar(const char *cvar)
{
    m_cvarname = cvar;
}

void ToggleCVar::setButton(UIButton *button)
{
    if (m_button) {
        m_button->Disconnect(this, W_Button_Pressed);
    }

    m_button = button;
    if (m_button) {
        m_button->Connect(this, W_Button_Pressed, W_Button_Pressed);
    }
}

CLASS_DECLARATION(USignal, ExecCmd, NULL) {
    {NULL, NULL}
};

ExecCmd::ExecCmd()
{
    m_button = NULL;
}

ExecCmd::ExecCmd(UIButton *button, const char *cmd)
    : ExecCmd()
{
    setButton(button);
    setCommand(cmd);
}

void ExecCmd::Press(Event *ev)
{
    if (m_cmd.length()) {
        str stuff = m_cmd + "\n";
        uii.Cmd_Stuff(stuff.c_str());
    }
}

void ExecCmd::setCommand(const char *cmd)
{
    m_cmd = cmd;
}

void ExecCmd::setButton(UIButton *button)
{
    if (m_button) {
        m_button->Disconnect(this, W_Button_Pressed);
    }

    m_button = button;
    if (m_button) {
        m_button->Connect(this, W_Button_Pressed, W_Button_Pressed);
    }
}
