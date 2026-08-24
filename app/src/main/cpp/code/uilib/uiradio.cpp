

#include "ui_local.h"
#include "uiwidgets_ef2.h"

Event EV_UIRadio_RadioButton
(
    "radiobutton",
    EV_DEFAULT,
    "sss",
    "returnstring label stuffcommand",
    "create a new radio button/label"
);
Event EV_UIRadio_ButtonShader
(
    "buttonshader",
    EV_DEFAULT,
    "s",
    "button_material",
    "set the normal button shader"
);
Event EV_UIRadio_SelectShader
(
    "selectshader",
    EV_DEFAULT,
    "s",
    "select_material",
    "set the selected button shader"
);
Event EV_UIRadio_BindCvar
(
    "bindcvar",
    EV_DEFAULT,
    "s",
    "cvar",
    "link radio buttons to a particular cvar"
);
Event EV_UIRadio_RowHeight
(
    "rowheight",
    EV_DEFAULT,
    "f",
    "rowheight",
    "set the height of each row"
);
Event EV_UIRadio_ButtonSize
(
    "buttonsize",
    EV_DEFAULT,
    "ff",
    "buttonsize.x buttonsize.y",
    "set the size of the radio buttons"
);
Event EV_UIRadio_Padding
(
    "padding",
    EV_DEFAULT,
    "ff",
    "pad.x pad.y",
    "set the padding from top-left corner"
);

CLASS_DECLARATION(UIWidget, UIRadioButton, NULL) {
    {&EV_UIRadio_RadioButton, &UIRadioButton::AddRadioButton },
    {&EV_UIRadio_ButtonShader, &UIRadioButton::SetButtonShader},
    {&EV_UIRadio_SelectShader, &UIRadioButton::SetSelectShader},
    {&EV_UIRadio_BindCvar,    &UIRadioButton::BindCvar       },
    {&EV_UIRadio_RowHeight,   &UIRadioButton::SetRowHeight   },
    {&EV_UIRadio_ButtonSize,  &UIRadioButton::SetButtonSize  },
    {&EV_UIRadio_Padding,     &UIRadioButton::SetPadding     },
    {NULL,                    NULL                           }
};

UIRadioButton::UIRadioButton()
{
    m_buttonShader = NULL;
    m_selectShader = NULL;
    m_buttonSize.width  = 16.0f;
    m_buttonSize.height = 16.0f;
    m_padding.width     = 8.0f;
    m_padding.height    = 8.0f;
    m_rowHeight    = 0.0f;
    m_numButtons   = 0;
    m_selected     = 0;
    m_cvar         = "";
}

UIRadioButton::~UIRadioButton() {}

void UIRadioButton::AddRadioButton(Event *ev)
{
    str returnstring = ev->GetString(1);
    str stuffcommand = ev->GetString(3);

    UIWidget *button = new UIWidget;
    button->setBorderStyle(border_none);
    button->setBackgroundColor(UColor(0.0f, 0.0f, 0.0f, 0.0f), WIDGET_STATE_NORMAL, true);
    button->InitFrame(
        this,
        UIRect2D(
            m_padding.width, m_numButtons * m_rowHeight + m_padding.height, m_buttonSize.width, m_buttonSize.height
        ),
        -1,
        "tahoma-10"
    );
    addChild(button);

    UILabel *label = new UILabel;
    label->setLabelText(ev->GetString(2));
    label->setFont(m_font->getName().c_str());
    label->setBorderStyle(border_none);
    label->setBackgroundColor(UColor(0.0f, 0.0f, 0.0f, 0.0f), WIDGET_STATE_NORMAL, true);
    label->setForegroundColor(m_foreground_color, WIDGET_STATE_NORMAL);
    label->InitFrame(
        this,
        UIRect2D(
            m_buttonSize.width + m_padding.width,
            m_numButtons * m_rowHeight + m_padding.height,
            getFrame().size.width - m_buttonSize.width,
            m_rowHeight
        ),
        -1,
        "tahoma-10"
    );
    addChild(label);

    m_stuffCommands.AddObject(stuffcommand);
    m_returnStrings.AddObject(returnstring);
    m_numButtons++;
}

void UIRadioButton::SetButtonShader(Event *ev)
{
    m_buttonShader = uWinMan.RegisterShader(ev->GetString(1));
}

void UIRadioButton::SetSelectShader(Event *ev)
{
    m_selectShader = uWinMan.RegisterShader(ev->GetString(1));
}

void UIRadioButton::BindCvar(Event *ev)
{
    m_cvar = ev->GetString(1);
}

void UIRadioButton::SetRowHeight(Event *ev)
{
    m_rowHeight = ev->GetFloat(1);
    ResetFrames();
}

void UIRadioButton::SetButtonSize(Event *ev)
{
    m_buttonSize.width = ev->GetFloat(1);
    m_buttonSize.height = ev->GetFloat(2);
    ResetFrames();
}

void UIRadioButton::SetPadding(Event *ev)
{
    m_padding.width = ev->GetFloat(1);
    m_padding.height = ev->GetFloat(2);
    ResetFrames();
}

void UIRadioButton::ResetFrames(void)
{
    int i;

    for (i = 1; i <= m_children.NumObjects() / 2; i++) {
        UIWidget *button = m_children.ObjectAt(2 * i - 1);
        UIWidget *label  = m_children.ObjectAt(2 * i);
        UIRect2D  frame  = getFrame();
        float     y      = m_padding.height + (i - 1) * m_rowHeight;

        button->setFrame(UIRect2D(m_padding.width, y, m_buttonSize.width, m_buttonSize.height));
        label->setFrame(UIRect2D(
            m_buttonSize.width + m_padding.width, y, frame.size.width - m_buttonSize.width, m_rowHeight
        ));
    }
}

void UIRadioButton::PressButton(int num)
{
    if (num == m_selected || num > m_numButtons) {
        return;
    }

    m_selected = num;

    if (Q_stricmp(m_cvar.c_str(), "")) {
        uii.Cvar_Set(m_cvar.c_str(), m_returnStrings.ObjectAt(num).c_str());
    }

    str command = m_stuffCommands.ObjectAt(num);
    if (Q_stricmp(command.c_str(), "")) {
        uii.Cmd_Stuff(command.c_str());
    }
}

void UIRadioButton::Pressed(Event *ev)
{
    float x = ev->GetFloat(1);
    float y = ev->GetFloat(2);
    float dx = x - (m_frame.pos.x + m_padding.width);
    float dy = y - (m_frame.pos.y + m_padding.height);

    if (dx < 0.0f || dx > m_buttonSize.width) {
        return;
    }

    int num = 1;
    while (dy > m_rowHeight) {
        dy -= m_rowHeight;
        num++;
    }

    if (dy <= m_buttonSize.height) {
        PressButton(num);
    }
}

void UIRadioButton::Draw(void)
{
    UIWidget::Draw();

    if (Q_stricmp(m_cvar.c_str(), "")) {
        m_selected = 0;
        for (int i = 1; i <= m_numButtons; i++) {
            if (!Q_stricmp(
                    m_returnStrings.ObjectAt(i).c_str(),
                    uii.Cvar_GetString(m_cvar.c_str(), "")
                )) {
                PressButton(i);
            }
        }
    }

    for (int i = 0; i < m_numButtons; i++) {
        UIWidget *button = m_children.ObjectAt(2 * i + 1);

        if (m_selected == i + 1) {
            button->setMaterial(m_selectShader, WIDGET_STATE_NORMAL);
        } else {
            button->setMaterial(m_buttonShader, WIDGET_STATE_NORMAL);
        }
    }
}
