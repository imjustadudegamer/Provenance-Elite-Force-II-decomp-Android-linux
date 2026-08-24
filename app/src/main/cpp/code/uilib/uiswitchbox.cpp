/*
===========================================================================
Reconstructed from the retail Star Trek: Voyager -- Elite Force II binaries.
The publisher released no source for this file.  This port asserts no copyright
in it and grants no rights to it: it remains the property of its respective
rights holders, and it is covered neither by the GNU GPL nor by the Ritual
Elite Force II SDK agreement.  See LICENSING.md.
===========================================================================
*/
// SPDX-License-Identifier: LicenseRef-EF2-Retail-Reconstruction


#include "ui_local.h"
#include "../qcommon/stringresource.h"
#include "uibutton.h"


typedef struct switchoption_s {
    str label;       
    str imagename;   
    str command;     
    str name;        
    int isImage;     
} switchoption_t;   

class UISwitchBox : public UIButton
{
protected:
    Container<switchoption_t *> m_options;       
    int                         m_current;       
    bool                        m_unusedFlag;    
    str                         m_currentValue;  

public:
    CLASS_PROTOTYPE(UISwitchBox);

    UISwitchBox();
    ~UISwitchBox();

    void addOption(Event *ev);
    void addOption(switchoption_t *opt);
    void addImageOption(Event *ev);
    void addImageNameOption(Event *ev);
    void clearOptions(Event *ev);
    void displayOption(int index, bool bStuff);
    void nextOption(Event *ev);
    void Pressed(Event *ev) override;
    void copyProperties(UIWidget& other) override;
    void setData(const str& data) override;
    const char *getData(void) override;
    void        Draw(void) override;
};

Event EV_SwitchBox_Option
(
    "option",
    EV_DEFAULT,
    "sSS",
    "name command cvarvalue",
    "The properties of a switchbox option"
);
Event EV_SwitchBox_ClearOptions
(
    "clearoptions",
    EV_DEFAULT,
    NULL,
    NULL,
    "menu3d D-UI: remove every option from the switchbox"
);
Event EV_SwitchBox_ImageOption
(
    "imageoption",
    EV_DEFAULT,
    "sSS",
    "image command cvarvalue",
    "The properties of a image switch box option"
);
Event EV_SwitchBox_ImageNameOption
(
    "imagenameoption",
    EV_DEFAULT,
    "ssSS",
    "image name command cvarvalue",
    "Like an image option, but will return a name on the getData call, rather than the image path"
);

CLASS_DECLARATION(UIButton, UISwitchBox, NULL) {
    {&EV_SwitchBox_Option,          &UISwitchBox::addOption         },
    {&EV_SwitchBox_ClearOptions,    &UISwitchBox::clearOptions      },
    {&EV_SwitchBox_ImageOption,     &UISwitchBox::addImageOption    },
    {&EV_SwitchBox_ImageNameOption, &UISwitchBox::addImageNameOption},
    {NULL,                          NULL                            }
};

UISwitchBox::UISwitchBox()
{
    m_options.ClearObjectList();
    m_current     = 1;
    m_unusedFlag  = false;
    m_currentValue = "";
    AllowActivate(true);
}

UISwitchBox::~UISwitchBox()
{
    for (int i = 1; i <= m_options.NumObjects(); i++) {
        delete m_options.ObjectAt(i);
    }

    m_options.ClearObjectList();
}

void UISwitchBox::clearOptions(Event *ev)
{
    for (int i = 1; i <= m_options.NumObjects(); i++) {
        delete m_options.ObjectAt(i);
    }

    m_options.ClearObjectList();
    m_current = 1;
    m_currentValue = "";
}

void UISwitchBox::addOption(Event *ev)
{
    switchoption_t *opt = new switchoption_t;

    opt->label     = "Default";
    opt->command   = "";
    opt->name      = "";
    opt->imagename = "";

    if (ev->NumArgs() >= 1) {
        opt->label = ev->GetString(1);
    }
    if (ev->NumArgs() > 1) {
        opt->command = ev->GetString(2);
    }
    if (ev->NumArgs() > 2) {
        opt->name = ev->GetString(3);
    }

    opt->isImage = 0;
    addOption(opt);
}

void UISwitchBox::addImageOption(Event *ev)
{
    switchoption_t *opt = new switchoption_t;

    opt->label     = "Default";
    opt->command   = "";
    opt->name      = "";
    opt->imagename = "";

    if (ev->NumArgs() >= 1) {
        opt->imagename = ev->GetString(1);
    }
    if (ev->NumArgs() > 1) {
        opt->command = ev->GetString(2);
    }
    if (ev->NumArgs() > 2) {
        opt->name = ev->GetString(3);
    }

    opt->isImage = 1;
    addOption(opt);
}

void UISwitchBox::addImageNameOption(Event *ev)
{
    switchoption_t *opt = new switchoption_t;

    opt->label     = "Default";
    opt->command   = "";
    opt->name      = "";
    opt->imagename = "";

    if (ev->NumArgs() >= 1) {
        opt->imagename = ev->GetString(1);
    }
    if (ev->NumArgs() >= 2) {
        opt->label = ev->GetString(2);
    }
    if (ev->NumArgs() > 2) {
        opt->command = ev->GetString(3);
    }
    if (ev->NumArgs() > 3) {
        opt->name = ev->GetString(4);
    }

    opt->isImage = 1;
    addOption(opt);
}

void UISwitchBox::addOption(switchoption_t *opt)
{
    if (!m_options.NumObjects()) {
        if (opt->isImage) {
            setMaterial(uWinMan.RegisterShader(opt->imagename), WIDGET_STATE_NORMAL);
        } else {
            setTitle(opt->label);
        }
    }

    m_options.AddObject(opt);
}

void UISwitchBox::displayOption(int index, bool bStuff)
{
    if (index < 1 || index > m_options.NumObjects()) {
        return;
    }

    switchoption_t *opt = m_options.ObjectAt(index);

    if (opt->isImage) {
        setMaterial(uWinMan.RegisterShader(opt->imagename), WIDGET_STATE_NORMAL);
    } else {
        setTitle(opt->label);
    }

    if (opt->command.length() && bStuff) {
        str stuff = opt->command + "\n";
        uii.Cmd_Stuff(stuff.c_str());
    }
}

void UISwitchBox::nextOption(Event *ev)
{
    if (m_widgetState == WIDGET_STATE_DISABLED) {
        return;
    }

    if (!m_options.NumObjects()) {
        return;
    }

    m_current++;
    if (m_current > m_options.NumObjects()) {
        m_current = 1;
    }

    displayOption(m_current, true);
}

void UISwitchBox::Pressed(Event *ev)
{
    if (m_widgetState != WIDGET_STATE_DISABLED && m_options.NumObjects()) {
        m_current++;
        if (m_current > m_options.NumObjects()) {
            m_current = 1;
        }

        displayOption(m_current, true);
    }

    UIButtonBase::Pressed(ev);
}

void UISwitchBox::copyProperties(UIWidget& other)
{
    UIWidget::copyProperties(other);

    for (int i = 1; i <= m_options.NumObjects(); i++) {
        switchoption_t *src  = m_options.ObjectAt(i);
        switchoption_t *copy = new switchoption_t;

        copy->label     = src->label;
        copy->imagename = src->imagename;
        copy->command   = src->command;
        copy->name      = src->name;
        copy->isImage   = src->isImage;

        ((UISwitchBox &)other).addOption(copy);
    }
}

void UISwitchBox::setData(const str& data)
{
    for (int i = 1; i <= m_options.NumObjects(); i++) {
        switchoption_t *opt = m_options.ObjectAt(i);

        if (opt->name.length() == data.length() && !strcmp(opt->name.c_str(), data.c_str())) {
            m_current = i;
            displayOption(i, false);
            return;
        }
    }
}

const char *UISwitchBox::getData(void)
{
    return m_options.ObjectAt(m_current)->name;
}

void UISwitchBox::Draw(void)
{
    if (m_cvarname.length()) {
        cvar_t *cvar = uii.Cvar_Find(m_cvarname.c_str());

        if (!cvar) {
            return;
        }

        {
            const char *value = cvar->latchedString ? cvar->latchedString : cvar->string;

            if (value && str::icmp(value, m_currentValue.c_str())) {
                for (int i = 1; i <= m_options.NumObjects(); i++) {
                    switchoption_t *opt = m_options.ObjectAt(i);

                    if (!str::icmp(value, opt->name.c_str())) {
                        if (opt->isImage) {
                            setMaterial(uWinMan.RegisterShader(opt->imagename), WIDGET_STATE_NORMAL);
                        } else {
                            setTitle(opt->label);
                        }

                        m_current = i;
                        break;
                    }
                }

                if (value) {
                    m_currentValue = value;
                } else {
                    m_currentValue = "";
                }
            }
        }
    }

    UIButton::Draw();
}
