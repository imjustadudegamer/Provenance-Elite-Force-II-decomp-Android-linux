
#pragma once

class unnamed_struct
{
public:
    int itemOffset;
    int orgItem;
};

typedef enum {
    VS_NONE,
    VS_UP_ARROW,
    VS_DOWN_ARROW,
    VS_THUMB,
    VS_PAGE_DOWN,
    VS_PAGE_UP
} whatspressed;

class UIVertScroll : public UIWidget
{
protected:
    int            m_numitems;
    int            m_pageheight;
    int            m_topitem;
    UIFont         m_marlett;
    whatspressed   m_pressed;
    UIRect2D       thumbRect;
    unnamed_struct m_dragThumbState;
    bool           m_frameinitted;
    UColor         m_thumbcolor;
    UColor         m_solidbordercolor;
    float          m_scrollbarWidth;
    float          m_scrollbarWidthBase;

public:
    CLASS_PROTOTYPE(UIVertScroll);

protected:
    int  getItemFromHeight(float height);
    bool isEnoughItems(void);

public:
    UIVertScroll();

    void Draw(void) override;
    void DrawArrow(float top, const char *text, bool pressed);
    void DrawThumb(void);
    void MouseDown(Event *ev);
    void MouseUp(Event *ev);
    void MouseDragged(Event *ev);
    void MouseEnter(Event *ev);
    void MouseLeave(Event *ev);
    void Scroll(Event *ev);
    void setScrollbarWidth(Event *ev);
    void thumbColor(Event *ev);
    void solidBorderColor(Event *ev);
    void copyProperties(UIWidget& other) override;
    bool AttemptScrollTo(int to);
    void setNumItems(int i);
    void setPageHeight(int i);
    void setTopItem(int i);
    int  getTopItem(void);
    int  getPageHeight(void);
    int  getNumItems(void) override;
    void setThumbColor(const UColor& thumb);
    void setSolidBorderColor(const UColor& col);
    void InitFrameAlignRight(UIWidget *parent);
    void FrameInitialized() override;
    void setScrollbarWidth(float w)
    {
        m_scrollbarWidth     = w;
        m_scrollbarWidthBase = w;
    }
};
