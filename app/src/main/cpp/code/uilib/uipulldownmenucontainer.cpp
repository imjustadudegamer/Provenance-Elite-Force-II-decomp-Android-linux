
#include "ui_local.h"
#include "../qcommon/stringresource.h"

Event EV_UIPulldownMenuContainer_AddPopup
(
    "addpopup",
    EV_DEFAULT,
    "ssss",
    "menuname title type string",
    "Adds a popup to the list box.\nmenuname - the name of the menu to add this to\ntitle - the string to display\ntype - type of item this is (command,cvar,event_name)\nstring - the data that corresponds to the correct type\n"
);

Event EV_UIPulldownMenuContainer_MenuShader
(
    "menushader",
    EV_DEFAULT,
    "ss",
    "menuname shader",
    "Adds a menu to the container and sets the shader.\nmenuname - the name of the menu to add this to\nshader - the string to display"
);

Event EV_UIPulldownMenuContainer_SelectedMenuShader
(
    "selmenushader",
    EV_DEFAULT,
    "ss",
    "menuname shader",
    "Adds a menu to the container and sets the shader when it's selected.\nmenuname - the name of the menu to add this to\nshader - the string to display"
);

Event EV_UIPulldownMenuContainer_PopupHighlightFGColor
(
    "popup_highlight_fgcolor",
    EV_DEFAULT,
    "sffff",
    "menu r g b a",
    "Set the highlight background color of the popup menumenuname - the name of the popup menu to color\n"
);

Event EV_UIPulldownMenuContainer_HighlightBGColor
(
    "highlight_bgcolor",
    EV_DEFAULT,
    "sffff",
    "menu r g b a",
    "Set the highlight foreground color of the pulldown menumenuname - the name of the popup menu to color\n"
);

Event EV_UIPulldownMenuContainer_HighlightFGColor
(
    "highlight_fgcolor",
    EV_DEFAULT,
    "sffff",
    "menu r g b a",
    "Set the highlight background color of the pulldown menumenuname - the name of the popup menu to color\n"
);

Event EV_UIPulldownMenuContainer_PopupHighlightBGColor
(
    "popup_highlight_bgcolor",
    EV_DEFAULT,
    "sffff",
    "menu r g b a",
    "Set the highlight foreground color of the popup menumenuname - the name of the popup menu to color\n"
);

Event EV_UIPulldownMenuContainer_PopupFGColor
(
    "popup_fgcolor",
    EV_DEFAULT,
    "sffff",
    "menu r g b a",
    "Set the background color of the popup menumenuname - the name of the popup menu to color\n"
);

Event EV_UIPulldownMenuContainer_PopupBGColor
(
    "popup_bgcolor",
    EV_DEFAULT,
    "sffff",
    "menu r g b a",
    "Set the foreground color of the popup menumenuname - the name of the popup menu to color\n"
);

Event EV_UIPulldownMenuContainer_PopupWidth
(
    "popup_width",
    EV_DEFAULT,
    "sf",
    "menu width",
    "Sets the width of the menu popup"
);

CLASS_DECLARATION(UIWidget, UIPulldownMenuContainer, NULL) {
    {&EV_UIPulldownMenuContainer_AddPopup,              &UIPulldownMenuContainer::AddPopup                },
    {&EV_UIPulldownMenuContainer_PopupWidth,            &UIPulldownMenuContainer::SetPopupWidth           },
    {&EV_UIPulldownMenuContainer_MenuShader,            &UIPulldownMenuContainer::SetMenuShader           },
    {&EV_UIPulldownMenuContainer_SelectedMenuShader,    &UIPulldownMenuContainer::SetSelectedMenuShader   },
    {&EV_UIPulldownMenuContainer_PopupHighlightFGColor, &UIPulldownMenuContainer::SetPopupHighlightFGColor},
    {&EV_UIPulldownMenuContainer_PopupHighlightBGColor, &UIPulldownMenuContainer::SetPopupHighlightBGColor},
    {&EV_UIPulldownMenuContainer_PopupFGColor,          &UIPulldownMenuContainer::SetPopupFGColor         },
    {&EV_UIPulldownMenuContainer_PopupBGColor,          &UIPulldownMenuContainer::SetPopupBGColor         },
    {&EV_UIPulldownMenuContainer_HighlightFGColor,      &UIPulldownMenuContainer::SetHighlightFGColor     },
    {&EV_UIPulldownMenuContainer_HighlightBGColor,      &UIPulldownMenuContainer::SetHighlightBGColor     },
    {&EV_Layout_Font,                                   &UIPulldownMenuContainer::LayoutFont              },
    {&EV_Layout_ShaderColor,                            &UIPulldownMenuContainer::LayoutShaderColor       },
    {&EV_Layout_HoverShaderColor,                       &UIPulldownMenuContainer::LayoutHoverShaderColor  },
    {&EV_Layout_SelectedShaderColor,                    &UIPulldownMenuContainer::LayoutSelectedShaderColor},
    {&EV_Layout_ArmedShaderColor,                       &UIPulldownMenuContainer::LayoutArmedShaderColor  },
    {&EV_Layout_ClickSound,                             &UIPulldownMenuContainer::LayoutClickSound        },
    {NULL,                                              NULL                                              }
};

UIPulldownMenuContainer::UIPulldownMenuContainer()
{
    m_menu = new UIPulldownMenu();
    m_menu->CreateAligned(this, this);
}

void UIPulldownMenuContainer::SetPopupWidth(Event *ev)
{
    m_menu->setPopupWidth(ev->GetString(1), ev->GetFloat(2));
}


void UIPulldownMenuContainer::LayoutFont(Event *ev)
{
    UIWidget::LayoutFont(ev);

    if (m_menu) {
        m_menu->LayoutFont(ev);
    }
}

void UIPulldownMenuContainer::LayoutShaderColor(Event *ev)
{
    UIWidget::LayoutShaderColor(ev);

    if (m_menu) {
        m_menu->LayoutShaderColor(ev);
    }
}

void UIPulldownMenuContainer::LayoutHoverShaderColor(Event *ev)
{
    UIWidget::LayoutHoverShaderColor(ev);

    if (m_menu) {
        m_menu->LayoutHoverShaderColor(ev);
    }
}

void UIPulldownMenuContainer::LayoutSelectedShaderColor(Event *ev)
{
    UIWidget::LayoutSelectedShaderColor(ev);

    if (m_menu) {
        m_menu->LayoutSelectedShaderColor(ev);
    }
}

void UIPulldownMenuContainer::LayoutArmedShaderColor(Event *ev)
{
    UIWidget::LayoutArmedShaderColor(ev);

    if (m_menu) {
        m_menu->LayoutArmedShaderColor(ev);
    }
}

void UIPulldownMenuContainer::LayoutClickSound(Event *ev)
{
    if (m_menu) {
        m_menu->LayoutClickSound(ev);
    }
}

UIPulldownMenuContainer::~UIPulldownMenuContainer()
{
    for (int i = m_popups.NumObjects(); i > 0; i--) {
        uipopup_describe *uipd = m_popups.ObjectAt(i);
        m_popups.RemoveObjectAt(i);
        delete uipd;
    }

    for (int i = m_dataContainer.NumObjects(); i > 0; i--) {
        char *data = m_dataContainer.ObjectAt(i);
        m_dataContainer.RemoveObjectAt(i);
        delete data;
    }

}

void UIPulldownMenuContainer::FrameInitialized(void)
{
    AllowActivate(qfalse);
    m_menu->setBackgroundColor(m_background_color, WIDGET_STATE_NORMAL, qtrue);
    m_menu->setForegroundColor(m_foreground_color);
}

void UIPulldownMenuContainer::setBackgroundAlpha(float f)
{
    m_alpha = f;
    m_menu->setBackgroundAlpha(f);
}

void UIPulldownMenuContainer::setBackgroundColor(const UColor& color, WidgetState state, bool setbordercolor)
{
    UIWidget::setBackgroundColor(color, state, setbordercolor);
    m_menu->setBackgroundColor(color, state, setbordercolor);
}

void UIPulldownMenuContainer::setForegroundColor(const UColor& color, WidgetState state)
{
    UIWidget::setForegroundColor(color, state);
    m_menu->setForegroundColor(color, state);
}

void UIPulldownMenuContainer::SetPopupHighlightFGColor(Event *ev)
{
    str   menu = ev->GetString(1);
    float r    = ev->GetFloat(2);
    float g    = ev->GetFloat(3);
    float b    = ev->GetFloat(4);
    float a    = ev->GetFloat(5);

    m_menu->setPopupHighlightFGColor(menu, UColor(r, g, b, a));
}

void UIPulldownMenuContainer::SetPopupHighlightBGColor(Event *ev)
{
    str   menu = ev->GetString(1);
    float r    = ev->GetFloat(2);
    float g    = ev->GetFloat(3);
    float b    = ev->GetFloat(4);
    float a    = ev->GetFloat(5);

    m_menu->setPopupHighlightBGColor(menu, UColor(r, g, b, a));
}

void UIPulldownMenuContainer::SetPopupFGColor(Event *ev)
{
    str   menu = ev->GetString(1);
    float r    = ev->GetFloat(2);
    float g    = ev->GetFloat(3);
    float b    = ev->GetFloat(4);
    float a    = ev->GetFloat(5);

    m_menu->setPopupFGColor(menu, UColor(r, g, b, a));
}

void UIPulldownMenuContainer::SetPopupBGColor(Event *ev)
{
    str   menu = ev->GetString(1);
    float r    = ev->GetFloat(2);
    float g    = ev->GetFloat(3);
    float b    = ev->GetFloat(4);
    float a    = ev->GetFloat(5);

    m_menu->setPopupBGColor(menu, UColor(r, g, b, a));
}

void UIPulldownMenuContainer::SetHighlightFGColor(Event *ev)
{
    float r = ev->GetFloat(1);
    float g = ev->GetFloat(2);
    float b = ev->GetFloat(3);
    float a = ev->GetFloat(4);

    m_menu->setHighlightFGColor(UColor(r, g, b, a));
}

void UIPulldownMenuContainer::SetHighlightBGColor(Event *ev)
{
    float r = ev->GetFloat(1);
    float g = ev->GetFloat(2);
    float b = ev->GetFloat(3);
    float a = ev->GetFloat(4);

    m_menu->setHighlightBGColor(UColor(r, g, b, a));
}

void UIPulldownMenuContainer::SetMenuShader(Event *ev)
{
    str               menu   = ev->GetString(1);
    str               shader = ev->GetString(2);
    UIReggedMaterial *mat    = uWinMan.RegisterShader(shader);
    m_menu->setPulldownShader(menu, mat);
}

void UIPulldownMenuContainer::SetSelectedMenuShader(Event *ev)
{
    str               menu   = ev->GetString(1);
    str               shader = ev->GetString(2);
    UIReggedMaterial *mat    = uWinMan.RegisterShader(shader);
    m_menu->setSelectedPulldownShader(menu, mat);
}

void UIPulldownMenuContainer::Realign(void)
{
    UIWidget::Realign();
    m_menu->Realign();
}


void UIPulldownMenuContainer::AddPopup(Event *ev)
{
    str menu  = ev->GetString(1);
    str title = ev->GetString(2);
    str t     = ev->GetString(3);
    str d     = ev->GetString(4);

    uipopup_type type = UI_PopupTypeStringToInt(t);

    void *data = strdup(d);

    uipopup_describe *uipd = new uipopup_describe(title, type, data, NULL);

    if (isStringResource(title.c_str())) {
        uipd->raw = title;
        uipd->title = getStringResource(title.c_str());
    }

    m_popups.AddObject(uipd);
    m_menu->AddUIPopupDescribe(menu, uipd);
}

void UIPulldownMenuContainer::reloadStrings(void)
{
    int i;

    for (i = 1; i <= m_popups.NumObjects(); i++) {
        uipopup_describe *d = m_popups.ObjectAt(i);
        if (d && d->raw.length()) {
            const char *res = getStringResource(d->raw.c_str());
            if (res) {
                d->title = res;
            } else {
                d->title = "";
            }
        }
    }
}
