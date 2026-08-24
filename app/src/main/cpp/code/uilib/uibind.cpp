
#include "ui_local.h"

#include "../qcommon/stringresource.h"

Event EV_Bind_NewItem
(
    "binditem",
    EV_DEFAULT,
    "ssS",
    "name command animation",
    "Create a new bind item in the interface"
);

Event EV_Bind_BindGroup
(
    "bindgroup",
    EV_DEFAULT,
    "s",
    "bindgroup",
    "Create a new bind group"
);

CLASS_DECLARATION(Listener, BindListener, NULL) {
    {&EV_Bind_NewItem,  &BindListener::NewItem  },
    {&EV_Bind_BindGroup,&BindListener::BindGroup},
    {NULL,              NULL                    }
};

bind_t::bind_t()
{
    currentBindGroup = 0;
}

bind_t::~bind_t()
{
    Clear();
}

void bind_t::Clear(void)
{
    int i;

    for (i = 1; i <= bindGroupNames.NumObjects(); i++) {
        char *name = bindGroupNames.ObjectAt(i);

        bindGroupNames.RemoveObjectAt(i);
        if (name) {
            delete[] name;
        }
    }

    for (i = 1; i <= bindGroups.NumObjects(); i++) {
        Container<BindCommand *> *group = bindGroups.ObjectAt(i);
        int                      j;

        for (j = 1; j <= group->NumObjects(); j++) {
            BindCommand *cmd = group->ObjectAt(j);

            group->RemoveObjectAt(j);
            if (cmd) {
                delete cmd;
            }
        }

        bindGroups.RemoveObjectAt(i);
        delete group;
    }

    currentBindGroup = 0;
}

BindListener::BindListener() {}

BindListener::BindListener(bind_t *b)
{
    bind = b;
}

void BindListener::NewItem(Event *ev)
{
    str                       name;
    str                       command;
    str                       animation;
    BindCommand              *cmd;
    Container<BindCommand *> *group;

    if (!ev) {
        return;
    }

    if (ev->NumArgs() >= 1) {
        name = ev->GetString(1);
    } else {
        ev->Error("bind: error on line %d: %s", 1, "");
        name = "";
    }

    if (ev->NumArgs() >= 2) {
        command = ev->GetString(2);
    } else {
        ev->Error("bind: error on line %d: %s", 2, "");
        command = "";
    }

    if (ev->NumArgs() > 2) {
        animation = ev->GetString(3);
    } else {
        animation = "0";
    }

    cmd = new BindCommand(name, command, animation);

    group = bind->bindGroups.ObjectAt(bind->currentBindGroup);
    if (group) {
        group->AddObject(cmd);
    }
}

// which is not necessarily the index of the container just appended.  Retail's
void BindListener::BindGroup(Event *ev)
{
    str  groupName;
    str  resource;
    int  i;
    Container<BindCommand *> *group;

    if (!ev) {
        return;
    }

    if (ev->NumArgs() >= 1) {
        groupName = ev->GetString(1);
    } else {
        ev->Error("bind: error on line %d: %s", 1, "");
        groupName = "";
    }

    resource = groupName;
    if (isStringResource(groupName.c_str())) {
        resource = getStringResource(groupName.c_str());
    }

    for (i = 1; i <= bind->bindGroupNames.NumObjects(); i++) {
        if (!stricmp(resource.c_str(), bind->bindGroupNames.ObjectAt(i))) {
            break;
        }
    }

    if (i > bind->bindGroupNames.NumObjects()) {
        i = -1;
    }

    if (i < 0 && bind->bindGroupNames.NumObjects() < 10) {
        char *copy = new char[resource.length() + 1];

        strcpy(copy, resource.c_str());
        bind->bindGroupNames.AddObject(copy);
        i = bind->bindGroupNames.NumObjects();
    }

    group = new Container<BindCommand *>;
    bind->bindGroups.AddObject(group);
    bind->currentBindGroup = i;
}

bool BindListener::Load(Script& script)
{
    str token;
    str errortext;

    while (script.TokenAvailable(true)) {
        token = script.GetToken(true);

        if (!token.length() || !ValidEvent(token)) {
            throw "invalid token";
        }

        Event *event = new Event(token);

        while (script.TokenAvailable(false)) {
            event->AddToken(script.GetToken(false));
        }

        ProcessEvent(event);
    }

    return true;
}

Event EV_UIBindButton_Command
(
    "bindcommand",
    EV_DEFAULT,
    "s",
    "cmd",
    "Command to bind"
);

Event EV_UIBindButton_AlternateKey
(
    "alternatekey",
    EV_DEFAULT,
    NULL,
    NULL,
    "Sets button to display alternate key"
);

CLASS_DECLARATION(UIButton, UIBindButton, NULL) {
    {&W_MouseEntered,               &UIBindButton::MouseEntered       },
    {&EV_UIBindButton_Command,      &UIBindButton::SetCommand         },
    {&EV_UIBindButton_AlternateKey, &UIBindButton::SetAlternateKey    },
    {NULL,                          NULL                              }
};

void UIBindButton::SetAlternateKey(Event *ev)
{
    m_alternate = qtrue;
}

void UIBindButton::copyProperties(UIWidget& other)
{
    UIWidget::copyProperties(other);

    ((UIBindButton&)other).m_alternate = m_alternate;
}

void UIBindButton::MouseEntered(Event *ev)
{
    if (!uWinMan.BindActive()) {
        UIButtonBase::MouseEntered(ev);
        uWinMan.ActivateControl(this);
        setWidgetState(WIDGET_STATE_ARMED);
    }
}

UIBindButton::UIBindButton()
    : m_bindindex(0)
    , m_getkey(qfalse)
    , m_alternate(qfalse)
    , m_mat(NULL)
{}

UIBindButton::UIBindButton(str entersound, str activesound)
    : m_bindindex(0)
    , m_getkey(qfalse)
    , m_alternate(qfalse)
    , m_mat(NULL)
{
    m_entersound  = entersound;
    m_activesound = activesound;

    m_mat      = uWinMan.RegisterShader("textures/bind/emptykey");
    m_material = m_mat;
}

void UIBindButton::SetCommand(Event *ev)
{
    SetCommand(ev->GetString(1));
}

void UIBindButton::SetCommand(str s)
{
    m_bindcommand = s;
    m_bindindex   = uWinMan.AddBinding(m_bindcommand);

    str keyname = uWinMan.GetKeyStringForCommand(m_bindcommand, m_bindindex, m_alternate, NULL, NULL);
    setTitle(keyname);
}

void UIBindButton::setData(const str& data)
{
    m_bindcommand = data;
    m_bindindex   = uWinMan.AddBinding(m_bindcommand);

    str keyname = uWinMan.GetKeyStringForCommand(m_bindcommand, m_bindindex, m_alternate, NULL, NULL);
    setTitle(keyname);
}

void UIBindButton::Pressed(Event *ev) {}

qboolean UIBindButton::KeyEvent(int key, unsigned int time)
{
    if (m_getkey) {
        if (key != K_ESCAPE) {
            str keyname;

            uWinMan.BindKeyToCommand(m_bindcommand, key, m_bindindex, m_alternate);
            keyname = uWinMan.GetKeyStringForCommand(m_bindcommand, m_bindindex, m_alternate, NULL, NULL);
            setTitle(keyname);
        }

        m_getkey = false;
        uWinMan.SetBindActive(NULL);
        Cbuf_ExecuteText(0, "globalwidgetcommand PressKey disable\n");

        return qtrue;
    }

    if ((key == K_ENTER || key == 178) && !m_getkey && m_widgetState == WIDGET_STATE_ARMED && !uWinMan.BindActive()) {
        m_getkey = true;
        uWinMan.SetBindActive(this);
        Cbuf_ExecuteText(0, "globalwidgetcommand PressKey enable\n");

        if (m_clicksound.length()) {
            uii.Snd_PlaySound(m_clicksound, CHAN_MENU2);  
        }

        DrawPressKey();

        return qtrue;
    }

    if (key == 127 && !m_getkey) {
        clearBindCommand();
        return qtrue;
    }

    return UIButton::KeyEvent(key, time);
}

void UIBindButton::DrawPressKey(void) {}

// Unbinding is a window-manager side effect that retail never performs from here.
void UIBindButton::clearBindCommand(void)
{
    m_bindcommand = str();
}

void UIBindButton::Draw(void)
{
    str s = uWinMan.GetKeyStringForCommand(m_bindcommand, m_bindindex, m_alternate, NULL, NULL);

    setTitle(s);

    UIButton::Draw();
}
