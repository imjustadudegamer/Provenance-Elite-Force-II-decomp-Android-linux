
#pragma once

typedef enum {
    M_NONE,
    M_DRAGGING
} mouseState_t;

class UIWindowSizer : public UIWidget
{
protected:
    class UIWidget *m_draggingwidget;
    mouseState_t    m_mouseState;
    UIPoint2D       m_screenDragPoint;

public:
    CLASS_PROTOTYPE(UIWindowSizer);

    UIWindowSizer();
    UIWindowSizer(UIWidget *w);

    void      Draw(void) override;
    void      FrameInitialized(void) override;
    void      MouseDown(Event *ev);
    void      MouseUp(Event *ev);
    void      MouseDragged(Event *ev);
    void      setDraggingWidget(UIWidget *w);
    UIWidget *getDraggingWidget(void);
};

typedef enum {
    WND_ALIGN_NONE,
    WND_ALIGN_BOTTOM
} alignment_t;

typedef struct align_s {
public:
    float       dist;
    alignment_t alignment;
} align_t;

class UIStatusBar : public UIWidget
{
protected:
    align_t         m_align;
    class UIWidget *m_sizeenabled;
    bool            m_created;

    UIWindowSizer *m_sizer;

public:
    CLASS_PROTOTYPE(UIStatusBar);

protected:
    void FrameInitialized(void) override;

public:
    UIStatusBar();
    UIStatusBar(alignment_t align, float height);

    void Draw(void) override;
    void AlignBar(alignment_t align, float height);
    void DontAlignBar(void);
    void EnableSizeBox(UIWidget *which);
    void ParentSized(Event *ev);
    void SelfSized(Event *ev);
};
