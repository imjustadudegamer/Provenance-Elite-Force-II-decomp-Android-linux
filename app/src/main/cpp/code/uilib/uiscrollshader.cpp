

#include "ui_local.h"
#include "../qcommon/stringresource.h"


class UIScrollShader : public UIWidget
{
protected:
    // linked cvar's value.  Retail's constructor does NOT initialise any of them --
    float m_scrollMin;
    float m_scrollMax;
    float m_scrollPercent;
    float m_cvarvalue;

public:
    CLASS_PROTOTYPE(UIScrollShader);

    UIScrollShader();

    void SetScroll(Event *ev);
    void UpdateUIElement(void) override;
    void Draw(void) override;
};

Event EV_ScrollShader_Scroll
(
    "scroll",
    EV_DEFAULT,
    "fff",
    "min max percent",
    "Minimum texcoord, Max texcoord, Expose Percentage"
);

CLASS_DECLARATION(UIWidget, UIScrollShader, NULL) {
    {&EV_ScrollShader_Scroll, &UIScrollShader::SetScroll},
    {NULL,                    NULL                         }
};

UIScrollShader::UIScrollShader() {}

void UIScrollShader::SetScroll(Event *ev)
{
    if (!ev) {
        return;
    }

    m_scrollMin     = ev->GetFloat(1);
    m_scrollMax     = ev->GetFloat(2);
    m_scrollPercent = ev->GetFloat(3);
}

// Retail's UIWidget::Display never reads +1596..+1608 -- its material draw is a plain
// 0..1 quad -- so the strip does not actually move in the shipped engine; the port's
void UIScrollShader::UpdateUIElement(void)
{
    float value;
    float half;

    if (!strlen(m_cvarname)) {
        return;
    }

    value = UI_GetCvarFloat(m_cvarname, 0.0f);

    m_cvarvalue = value;

    half = m_scrollPercent * 0.5f;

    m_textureCoordinates[0] = value - half;
    m_textureCoordinates[2] = value + half;

    if (m_textureCoordinates[0] < m_scrollMin) {
        m_textureCoordinates[0] = m_scrollMin;
    }
    if (m_textureCoordinates[0] > m_scrollMax) {
        m_textureCoordinates[0] = m_scrollMax;
    }
    if (m_textureCoordinates[2] < m_scrollMin) {
        m_textureCoordinates[2] = m_scrollMin;
    }
    if (m_textureCoordinates[2] > m_scrollMax) {
        m_textureCoordinates[2] = m_scrollMax;
    }
}

// the scrolling this port used to do here does not exist in retail.
void UIScrollShader::Draw(void)
{
    UIWidget::Draw();
}
