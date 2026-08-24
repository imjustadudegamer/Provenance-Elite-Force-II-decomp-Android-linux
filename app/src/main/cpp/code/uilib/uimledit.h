
#pragma once

typedef struct selectionpoint_s {
    int line;
    int column;
} selectionpoint_t;

typedef struct selection_s {
    selectionpoint_s begin;
    selectionpoint_s end;
} selection_t;

typedef struct dragState_s {
    UIPoint2D lastPos;
} dragState_t;

class UIMultiLineEdit : public UIWidget
{
protected:
    selection_s   m_selection;
    dragState_s   m_dragState;
    UList<str>    m_lines;
    UIVertScroll *m_vertscroll;
    mouseState_t  m_mouseState;
    bool          m_shiftForcedDown;
    bool          m_edit;
    bool          m_changed;

public:
    CLASS_PROTOTYPE(UIMultiLineEdit);

protected:
    void FrameInitialized(void) override;
    void PointToSelectionPoint(const UIPoint2D& p, selectionpoint_t& sel);
    str& LineFromLineNumber(int num, bool resetpos);
    void EnsureSelectionPointVisible(const selectionpoint_t& point);
    int  BoundSelectionPoint(selectionpoint_t& point);
    void SortSelection(selectionpoint_t **topsel, selectionpoint_t **botsel);
    void UpdateCvarEvent(Event *ev);
    void SetEdit(Event *ev);
    bool IsSelectionEmpty(void);

public:
    UIMultiLineEdit();

    void      Draw(void) override;
    qboolean  KeyEvent(int key, unsigned int time) override;
    void      CharEvent(int ch) override;
    UIPoint2D getEndSelPoint(void);
    void      MouseDown(Event *ev);
    void      MouseUp(Event *ev);
    void      MouseDragged(Event *ev);
    void      DragTimer(Event *ev);
    void      SizeChanged(Event *ev);
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#endif
    void      setData(const char *data);
    void      getData(str& data);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    void      Empty(void);
    void      CopySelection(void);
    void      PasteSelection(void);
    void      DeleteSelection(void);
    void      setChanged(bool b);
    bool      IsChanged(void);
};
