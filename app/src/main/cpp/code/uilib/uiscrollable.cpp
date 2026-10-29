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
#include "uiwidgets_ef2.h"


Event EV_Scrollable_ScrollbarTemplate
(
    "scrollbartemplate", EV_DEFAULT, "s", "templateName", "Sets the name of the scrollbar template"
);
Event EV_Scrollable_ItemHeight
(
    "itemheight", EV_DEFAULT, "f", "height", "The height of the item in the list."
);
Event EV_Scrollable_ScrollbarWidth
(
    "scrollbar_width", EV_DEFAULT, "f", "width", "Sets the scrollbar width"   
);

CLASS_DECLARATION(UIWidget, UIScrollable, NULL) {
    {&EV_Scrollable_ItemHeight,        &UIScrollable::setItemHeight    },
    {&EV_Scrollable_ScrollbarTemplate, &UIScrollable::scrollBarTemplate},
    {&EV_Scrollable_ScrollbarWidth,    &UIScrollable::setScrollWidth   },
    {NULL,                             NULL                            }
};

UIScrollable::UIScrollable()
{
    // literal pool at 0x00218998 -- 16.0f, NOT 0.0f -- and never calls AllowActivate.
    m_scrollBar            = NULL;
    // (m_bottomItem), the same r4 the item-height store at 0x8A828 does not use.  The
    // inert -- retail never calls it and the ctor default is already false -- and is
    m_topItem              = 1;
    m_bottomItem           = 1;
    m_scrollItemHeight     = 16.0f;
    m_scrollItemHeightBase = 0.0f;
    m_scrollHeaderHeight   = 0.0f;
    m_scrollBarHeaderOffset = 0.0f;
    AllowActivate(false);
}

// UIROWS: retail UIScrollable::Draw @0x0008AD50 (192 B) -- the body the port never had
void UIScrollable::Draw(void)
{
    int visible;
    int bottom;

    UIWidget::Draw();

    // (the port guards the divisor here, which retail's FPU path does not need: PPC
    visible =
        (int)((getFrame().size.height - m_scrollHeaderHeight) / (m_scrollItemHeight > 0.0f ? m_scrollItemHeight : 1.0f));

    bottom = m_topItem + visible;
    if (bottom <= getNumItems()) {
        m_bottomItem = bottom;
    } else {
        m_bottomItem = getNumItems();
    }

    if (m_scrollBar) {
        m_topItem = m_scrollBar->getTopItem();
    }
}

void UIScrollable::ChangeResolution(void)
{
    UIWidget::ChangeResolution();

    m_scrollItemHeight = m_scrollItemHeightBase * uid.scaleRes[1];
}

UIScrollable::~UIScrollable()
{
    m_scrollBar = NULL;
}

void UIScrollable::setItemHeight(Event *ev)
{
    m_scrollItemHeightBase = ev->GetFloat(1);
    m_scrollItemHeight     = m_scrollItemHeightBase;
}

void UIScrollable::scrollBarTemplate(Event *ev)
{
    str name = ev->GetString(1);

    if (m_scrollBar) {
        delete m_scrollBar;
        m_scrollBar = NULL;
    }

    if (!name.length()) {
        return;
    }

    UIWidget *tmpl = uWinMan.FindWidget(name);

    if (!tmpl) {
        UIWidget *root = this;
        while (root->getParent()) {
            root = root->getParent();
        }
        tmpl = root->FindWidget(name);
    }

    if (!tmpl) {
        Com_DPrintf("UI: scrollbartemplate '%s' not found for '%s'\n", name.c_str(), getName());
        return;
    }

    tmpl->Disable();

    ClassDef *tmplClass = getClass(tmpl->getClassname());
    if (!tmplClass) {
        return;
    }

    UIWidget *created = (UIWidget *)tmplClass->newInstance();
    if (!created || !created->isSubclassOf(UIVertScroll)) {
        delete created;
        return;
    }

    m_scrollBar = (UIVertScroll *)created;
    tmpl->copyProperties(*m_scrollBar);

    m_scrollBar->InitFrameAlignRight(this, m_scrollBarHeaderOffset);
    onSizeChanged();
}

void UIScrollable::setScrollWidth(Event *ev)
{
    if (!m_scrollBar) {
        return;
    }

    m_scrollBar->setScrollbarWidth(ev->GetFloat(1));

    onSizeChanged();
}

void UIScrollable::onSizeChanged(void)
{
    if (!m_scrollBar) {
        return;
    }

    m_scrollBar->InitFrameAlignRight(this, m_scrollBarHeaderOffset);

    if (m_scrollItemHeight > 0.0f) {
        int page = (int)((m_frame.size.height - m_scrollHeaderHeight) / m_scrollItemHeight);
        m_scrollBar->setPageHeight(page);
    }
}

void UIScrollable::setScrollBarHeaderOffset(float offset)
{
    if (m_scrollBarHeaderOffset == offset) {
        return;
    }

    m_scrollBarHeaderOffset = offset;
    onSizeChanged();
}

void UIScrollable::setNumItems(int numItems)
{
    if (!m_scrollBar) {
        return;
    }

    m_scrollBar->setNumItems(numItems);

    if (m_scrollBar->getTopItem() > numItems) {
        m_scrollBar->setTopItem(0);
    }
}

float UIScrollable::getScrollBarWidth(void) const
{
    if (!m_scrollBar) {
        return 0.0f;
    }

    return m_scrollBar->getFrame().size.width;
}
