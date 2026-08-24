
#include "ui_local.h"

Event EV_Pulldown_ChildKilled
(
    "_ev_pulldown_child_killed",
    EV_DEFAULT,
    NULL,
    NULL,
    "Signaled when the sub menu is killed for one reason or another"
);

Event EV_Pulldown_HighlightBGColor
(
    "highlight_bgcolor",
    EV_DEFAULT,
    "fff",
    "r g b",
    "Set the background color of the highlighted text when a pulldown is used"
);

Event EV_Pulldown_HighlightFGColor
(
    "highlight_fgcolor",
    EV_DEFAULT,
    "fff",
    "r g b",
    "Set the foreground color of the highlighted text when a pulldown is used"
);

CLASS_DECLARATION(UIWidget, UIPulldownMenu, NULL) {
    {&W_LeftMouseDown,              &UIPulldownMenu::MousePressed    },
    {&W_LeftMouseUp,                &UIPulldownMenu::MouseReleased   },
    {&W_LeftMouseDragged,           &UIPulldownMenu::MouseDragged    },
    {&EV_Pulldown_ChildKilled,      &UIPulldownMenu::ChildKilled     },
    {&EV_Pulldown_HighlightBGColor, &UIPulldownMenu::HighlightBGColor},
    {&EV_Pulldown_HighlightFGColor, &UIPulldownMenu::HighlightFGColor},
    {NULL,                          NULL                             }
};

static UIPopupMenu *MenuFromPoint(UIPoint2D& p)
{
    UIWidget *res = uWinMan.FindResponder(p);
    if (res && res->isSubclassOf(UIPopupMenu)) {
        return (UIPopupMenu *)res;
    }

    return NULL;
}

UIPulldownMenu::UIPulldownMenu()
{
    m_listener         = NULL;
    m_submenu          = -1;
    m_highlightBGColor = UBlack;
    m_highlightFGColor = UWhite;

    m_submenuptr = NULL;
    m_pulledDown = true;

    m_canactivate = true;
}

void UIPulldownMenu::removeSubMenu(void)
{
    if (m_submenuptr) {
        m_submenuptr->Disconnect(this, W_Destroyed);
        if (m_submenuptr) {
            delete m_submenuptr;
        }
        m_submenu = -1;
    }
}

UIPulldownMenu::~UIPulldownMenu()
{
    for (int i = m_desc.NumObjects(); i > 0; i--) {
        uipull_describe *updb = m_desc.ObjectAt(i);
        m_desc.RemoveObjectAt(i);
        delete updb;
    }
}

// DIRECTLY, not through the vtable: raw 0x7C87C in Draw (`bl
float UIPulldownMenu::getDescWidth(uipull_describe *desc)
{
    (void)desc;
    return m_frame.size.width;
}

uipull_describe *UIPulldownMenu::getPulldown(str title)
{
    for (int i = 1; i <= m_desc.NumObjects(); i++) {
        uipull_describe *pd = m_desc.ObjectAt(i);
        if (pd->title == title) {
            return pd;
        }
    }

    return NULL;
}

void UIPulldownMenu::HighlightBGColor(Event *ev)
{
    m_highlightBGColor.r = ev->GetFloat(1);
    m_highlightBGColor.g = ev->GetFloat(2);
    m_highlightBGColor.b = ev->GetFloat(3);
    m_highlightBGColor.a = 1.0f;
}

void UIPulldownMenu::HighlightFGColor(Event *ev)
{
    m_highlightFGColor.r = ev->GetFloat(1);
    m_highlightFGColor.g = ev->GetFloat(2);
    m_highlightFGColor.b = ev->GetFloat(3);
    m_highlightFGColor.a = 1.0f;
}

void UIPulldownMenu::Create(UIWidget *parent, Listener *listener, const UIRect2D& rect)
{
    m_listener = listener;
    InitFrame(parent, rect.pos.x, rect.pos.y, rect.size.width, rect.size.height, 0);
}

void UIPulldownMenu::CreateAligned(UIWidget *parent, Listener *listener)
{
    setFont("Tahoma-10");

    UIWidget *p = parent ? parent : m_parent;
    if (!p) {
        p = &uWinMan;
    }

    UIRect2D cf = p->getClientFrame();
    Create(p, listener, UIRect2D(cf.pos.x, cf.pos.y, cf.size.width, (float)(int)cf.size.height));
}

void UIPulldownMenu::MousePressed(Event *ev)
{
    if (m_clicksound.length()) {
        uii.Snd_PlaySound(m_clicksound, CHAN_MENU2);  
    }

    if (m_submenu == -1) {
        if (m_widgetState != WIDGET_STATE_DISABLED) {
            m_pulledDown = true;
            uWinMan.setFirstResponder(this);
            MouseDragged(ev);
        }
    } else {
        removeSubMenu();
        m_pulledDown = false;
    }
}

void UIPulldownMenu::MouseDragged(Event *ev)
{
    int              newSubMenu = -1;
    UIRect2D         subRect;
    uipull_describe *subdesc = NULL;

    UIPoint2D point = MouseEventToClientPoint(ev);
    if (!m_pulledDown || m_widgetState == WIDGET_STATE_DISABLED) {
        return;
    }

    if (getClientFrame().contains(point)) {
        // off_22ABAC[0] is 0.0f (data 0x218950), not atx -- the popup rectangle
        float atx = 2.0f;
        for (int i = 1; i <= m_desc.NumObjects(); i++) {
            uipull_describe *desc  = m_desc.ObjectAt(i);
            float            width = getDescWidth(desc);
            if (atx <= point.x && atx + width > point.x) {
                newSubMenu          = i;
                subdesc             = desc;
                subRect.pos.x       = 0.0f;
                subRect.pos.y       = 0.0f;
                subRect.size.width  = m_frame.size.width;
                subRect.size.height = m_frame.size.height;
                break;
            }
            atx += width;
        }
    }

    if (newSubMenu == m_submenu || newSubMenu == -1) {
        if (m_submenu == -1) {
            return;
        }

        UIPoint2D cursorLocation(point.x + m_screenframe.pos.x, point.y + m_screenframe.pos.y);

        UIPopupMenu *menu = MenuFromPoint(cursorLocation);
        if (!menu) {
            UIPopupMenu *submenu = m_submenuptr;
            while (submenu->m_submenu != -1) {
                submenu = submenu->m_submenuptr;
            }

            submenu->MouseMoved(ev);
        } else {
            while (menu->m_parentMenu) {
                menu = menu->m_parentMenu;
            }
            if (menu == m_submenuptr) {
                menu->MouseMoved(ev);
            }
        }

        return;
    }

    if (m_submenu != -1) {
        removeSubMenu();
    }

    m_submenu    = newSubMenu;
    m_submenuptr = new UIPopupMenu();
    subRect.pos.x += m_screenframe.pos.x;
    subRect.pos.y += m_screenframe.pos.y;
    subRect.pos.x -= (subdesc->popup_width * uid.scaleRes[0] - m_frame.size.width);
    subRect.pos.y += m_frame.size.height;
    m_submenuptr->setParent(m_parent->getParent());
    m_submenuptr->Create(&subdesc->desc, m_listener, subRect, UIP_WHERE_DOWN, subdesc->popup_width);
    m_submenuptr->Connect(this, W_Destroyed, EV_Pulldown_ChildKilled);
}

void UIPulldownMenu::MouseReleased(Event *ev)
{
    if (uWinMan.getFirstResponder() == this) {
        uWinMan.setFirstResponder(NULL);
        UIPoint2D    point(ev->GetFloat(1), ev->GetFloat(2));
        UIPopupMenu *menu = MenuFromPoint(point);
        if (menu) {
            menu->MouseReleased(ev);
        }
    }
    // block the port carried here does NOT exist in retail.  Retail
}

void UIPulldownMenu::ChildKilled(Event *ev)
{
    if (m_submenu != -1) {
        m_submenu = -1;
    }

    if (uWinMan.getFirstResponder() == this) {
        uWinMan.setFirstResponder(NULL);
    }
}

void UIPulldownMenu::AddUIPopupDescribe(const char *title, uipopup_describe *d)
{
    if (!title) {
        return;
    }

    uipull_describe *pd = getPulldown(title);
    if (!pd) {
        pd = new uipull_describe(title, NULL, NULL);
        m_desc.AddObject(pd);
    }

    pd->desc.AddObject(d);
}

void UIPulldownMenu::setHighlightFGColor(UColor c)
{
    m_highlightFGColor = c;
}

void UIPulldownMenu::setHighlightBGColor(UColor c)
{
    m_highlightBGColor = c;
}

void UIPulldownMenu::setPopupHighlightFGColor(str menu, UColor c)
{
    uipull_describe *up = getPulldown(menu);
    if (up) {
        up->highlightFGColor = c;
    }
}

void UIPulldownMenu::setPopupHighlightBGColor(str menu, UColor c)
{
    uipull_describe *up = getPulldown(menu);
    if (up) {
        up->highlightBGColor = c;
    }
}

void UIPulldownMenu::setPopupFGColor(str menu, UColor c)
{
    uipull_describe *up = getPulldown(menu);
    if (up) {
        up->FGColor = c;
    }
}

void UIPulldownMenu::setPopupBGColor(str menu, UColor c)
{
    uipull_describe *up = getPulldown(menu);
    if (up) {
        up->BGColor = c;
    }
}

void UIPulldownMenu::setPulldownShader(const char *title, UIReggedMaterial *mat)
{
    if (!title) {
        return;
    }

    uipull_describe *pd = getPulldown(title);
    if (!pd) {
        pd = new uipull_describe(title, NULL, NULL);
        m_desc.AddObject(pd);
    }

    pd->material = mat;
    Realign();
}

void UIPulldownMenu::setSelectedPulldownShader(const char *title, UIReggedMaterial *mat)
{
    if (!title) {
        return;
    }

    uipull_describe *pd = getPulldown(title);
    if (!pd) {
        pd = new uipull_describe(title, NULL, NULL);
        m_desc.AddObject(pd);
    }

    pd->selected_material = mat;
    Realign();
}

void UIPulldownMenu::Realign(void)
{
    UIWidget *p = m_parent ? m_parent : &uWinMan;
    UIRect2D  cf = p->getClientFrame();
    setFrame(UIRect2D(cf.pos.x, cf.pos.y, cf.size.width, (float)(int)cf.size.height));
}

// 0x7CA84 `li r9, 0`), not the widget's m_bVirtual.
void UIPulldownMenu::Draw(void)
{
    UIWidget::Draw();

    for (int i = 1; i <= m_desc.NumObjects(); i++) {
        uipull_describe *desc  = m_desc.ObjectAt(i);
        float            width = getDescWidth(desc);

        if (m_submenu == i) {
            m_font->setColor(m_highlightFGColor);

            if (desc->selected_material) {
                SetColor(getMaterialColor(), m_local_alpha);
                uii.Rend_DrawPicStretched(
                    0.0f,
                    0.0f,
                    m_frame.size.width,
                    m_frame.size.height,
                    0.0f,
                    0.0f,
                    1.0f,
                    1.0f,
                    desc->selected_material->GetMaterial()
                );
            } else {
                DrawBox(0.0f, 0.0f, width, m_frame.size.height, m_highlightBGColor, 1.0f);
            }
        } else {
            m_font->setColor(m_foreground_color);
        }

        if (desc->material) {
            SetColor(getMaterialColor(), m_local_alpha);
            if (m_submenu != i) {
                uii.Rend_DrawPicStretched(
                    0.0f,
                    0.0f,
                    m_frame.size.width,
                    m_frame.size.height,
                    0.0f,
                    0.0f,
                    1.0f,
                    1.0f,
                    desc->material->GetMaterial()
                );
            }
        } else {
            m_font->Print(4.0f, 2.0f, desc->title, -1, UClear, 0);
        }
    }
}

void UIPulldownMenu::setPopupWidth(const str& menu, float width)
{
    uipull_describe *pd = getPulldown(menu);
    if (pd) {
        pd->popup_width = width;
    }
}
