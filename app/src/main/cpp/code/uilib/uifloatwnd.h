
#pragma once

class UIChildSpaceWidget : public UIWidget
{
public:
    CLASS_PROTOTYPE(UIChildSpaceWidget);

public:
    UIChildSpaceWidget();

    qboolean KeyEvent(int key, unsigned int time) override;
};

class UIFloatingWindow : public UIWidget
{
    UIPoint2D           m_clickOffset;
    bool                m_isPressed;
    UColor              m_titleColor;
    UColor              m_textColor;
    UIChildSpaceWidget *m_childspace;
    bool                m_minimized;
    float               m_restoredHeight;
    UIPoint2D           m_clickpoint;
    int                 m_clicktime;

protected:
    UIButton *m_closeButton;
    UIButton *m_minimizeButton;

public:
    CLASS_PROTOTYPE(UIFloatingWindow);

    static Event W_ClosePressed;
    static Event W_MinimizePressed;

protected:
    void FrameInitialized(void) override;
    void FrameInitialized(bool bHasDragBar);

public:
    UIFloatingWindow();
    ~UIFloatingWindow();

    void ClosePressed(Event *ev);
    void MinimizePressed(Event *ev);
    void Pressed(Event *ev) override;
    void Released(Event *ev);
    void Dragged(Event *ev);
    void SizeChanged(Event *ev);
    void OnActivated(Event *ev);
    void OnDeactivated(Event *ev);
    void
    Create(UIWidget *parent, const UIRect2D& rect, const char *title, const UColor& bgColor, const UColor& fgColor);
    void                Draw(void) override;
    UIChildSpaceWidget *getChildSpace(void);
    bool                IsMinimized(void);
};

static UColor UWindowColor(0.15f, 0.195f, 0.278f, 1);
