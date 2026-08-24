
#pragma once

class UIListItem
{
public:
    str itemname;
    str itemalias;
};

class UIList : public UIWidget
{
protected:
    Container<UIListItem *> m_itemlist;
    int                     m_currentItem;
    float                   m_arrow_width;
    UIRect2D               *m_next_arrow_region;
    UIRect2D               *m_prev_arrow_region;
    bool                    m_depressed;
    bool                    m_held;
    UIReggedMaterial       *m_prev_arrow;
    UIReggedMaterial       *m_next_arrow;
    bool                    m_prev_arrow_depressed;
    bool                    m_next_arrow_depressed;

public:
    CLASS_PROTOTYPE(UIList);

protected:
    void     Draw(void) override;
    qboolean KeyEvent(int key, unsigned int time) override;
    void     CharEvent(int ch) override;
    void     Pressed(Event *ev) override;
    void     Released(Event *ev);
    void     ScrollNext(void);
    void     ScrollPrev(void);
    void     FrameInitialized(void) override;
    void     LayoutAddListItem(Event *ev);
    void     AddItem(str item, str alias);

public:
    UIList();
    ~UIList();

    void UpdateUIElement(void) override;
    void UpdateData(void) override;
};
