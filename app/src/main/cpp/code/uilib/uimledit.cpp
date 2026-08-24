
#include "ui_local.h"

Event EV_Time_Dragged
(
    "_umledit_drag_timer",
    EV_DEFAULT,
    NULL,
    NULL,
    "Used to time mouse drag events"
);
Event EV_UIMultiLineEdit_UpdateCvarEvent
(
    "updatecvar",
    EV_DEFAULT,
    NULL,
    NULL,
    "Causes text box to update itself using the assigned cvar"
);
Event EV_UIMultiLineEdit_SetEdit
(
    "edit",
    EV_DEFAULT,
    "b",
    "bool",
    "Set whether or not the text is editable"
);

CLASS_DECLARATION(UIWidget, UIMultiLineEdit, NULL) {
    {&W_LeftMouseDown,                    &UIMultiLineEdit::MouseDown      },
    {&W_LeftMouseUp,                      &UIMultiLineEdit::MouseUp        },
    {&W_LeftMouseDragged,                 &UIMultiLineEdit::MouseDragged   },
    {&EV_Time_Dragged,                    &UIMultiLineEdit::DragTimer      },
    {&W_SizeChanged,                      &UIMultiLineEdit::SizeChanged    },
    {&EV_UIMultiLineEdit_UpdateCvarEvent, &UIMultiLineEdit::UpdateCvarEvent},
    {&EV_UIMultiLineEdit_SetEdit,         &UIMultiLineEdit::SetEdit        },
    {NULL,                                NULL                             }
};

UIMultiLineEdit::UIMultiLineEdit()
{
    m_vertscroll      = NULL;
    m_mouseState      = M_NONE;
    m_shiftForcedDown = 0;
    m_edit            = true;
    m_dragState.lastPos = {};
    Empty();
}

void UIMultiLineEdit::Empty(void)
{
    m_lines.RemoveAllItems();
    m_lines.AddTail(str());

    m_selection.begin = {};
    m_selection.end   = {};

    if (m_vertscroll) {
        m_vertscroll->setNumItems(1);
        m_vertscroll->setTopItem(0);
    }

    m_changed = false;
}

void UIMultiLineEdit::SetEdit(Event *ev)
{
    m_edit = ev->GetBoolean(1);
}

void UIMultiLineEdit::setData(const char *data)
{
    str         toadd;
    char        s[2] {};
    const char *p;

    if (!data) {
        Empty();
        return;
    }

    m_lines.RemoveAllItems();

    for (p = data; *p; p++) {
        if (*p == '\n') {
            m_lines.AddTail(toadd);
            toadd = "";
        } else if (*p == '\r') {
            continue;
        } else {
            s[0] = *p;
            toadd += s;
        }
    }

    m_lines.AddTail(toadd);
    m_selection.begin = {};
    m_selection.end   = {};

    if (m_vertscroll) {
        m_vertscroll->setNumItems(m_lines.getCount());
        m_vertscroll->setTopItem(0);
    }

    m_changed = false;
}

void UIMultiLineEdit::getData(str& data)
{
    int i;

    data = "";

    for (i = 0, m_lines.IterateFromHead(); m_lines.IsCurrentValid(); i++, m_lines.IterateNext()) {
        str text = m_lines.getCurrent();
        data += text;

        if (i != m_lines.getCount() - 1) {
            data += "\n";
        }
    }
}

void UIMultiLineEdit::FrameInitialized(void)
{
    delete m_vertscroll;
    m_vertscroll = new UIVertScroll();
    m_vertscroll->setPageHeight(m_frame.size.height / m_font->getHeight(false));
    m_vertscroll->setTopItem(0);
    m_vertscroll->setNumItems(m_lines.getCount());
    m_vertscroll->InitFrameAlignRight(this);

    Connect(this, W_SizeChanged, W_SizeChanged);
    AllowActivate(true);
}

void UIMultiLineEdit::SizeChanged(Event *ev)
{
    m_vertscroll->setPageHeight(m_frame.size.height / m_font->getHeight(false));
    m_vertscroll->InitFrameAlignRight(this);
}

void UIMultiLineEdit::SortSelection(selectionpoint_t **topsel, selectionpoint_t **botsel)
{
    if (m_selection.begin.line < m_selection.end.line) {
        *topsel = &m_selection.begin;
        *botsel = &m_selection.end;
    } else if (m_selection.begin.line <= m_selection.end.line && m_selection.begin.column <= m_selection.end.column) {
        *topsel = &m_selection.begin;
        *botsel = &m_selection.end;
    } else {
        *topsel = &m_selection.end;
        *botsel = &m_selection.begin;
    }
}

void UIMultiLineEdit::UpdateCvarEvent(Event *ev)
{
    if (m_cvarname.length()) {
        const char *s = uii.Cvar_GetString(m_cvarname, "");
        if (s) {
            setData(s);
        }
    }
}

void UIMultiLineEdit::Draw(void)
{
    float             aty;
    int               i;
    UColor            selectionColor(UWhite);
    UColor            selectionBG(0, 0, 0.5f, 1);
    selectionpoint_t *topsel = NULL;
    selectionpoint_t *botsel = NULL;

    UIWidget::Draw();

    SortSelection(&topsel, &botsel);

    m_lines.IterateFromHead();
    for (i = 0; i < m_vertscroll->getTopItem(); i++) {
        m_lines.IterateNext();
    }

    for (i = m_vertscroll->getTopItem(), aty = 0; m_lines.IsCurrentValid() && aty < m_frame.size.height;
         m_lines.IterateNext(), i++) {
        str& cur   = m_lines.getCurrent();
        int  caret = 0;

        if (i < topsel->line || i > botsel->line) {
            m_font->setColor(m_foreground_color);
            m_font->Print(0, aty, cur, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), false);
        } else {
            float linewidth = m_font->getWidth(cur, -1, false);

            if (i > topsel->line && i < botsel->line) {
                DrawBox(
                    0.0f, aty, linewidth, m_font->getHeight(false), selectionBG, 1.f
                );
                m_font->setColor(selectionColor);
                m_font->Print(0, aty, cur, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), false);
            } else if (i != topsel->line) {
                if (i == botsel->line) {                                
                    int toplen = m_font->getWidth(cur, botsel->column, false); 
                    if (toplen) {
                        m_font->setColor(selectionColor);
                        DrawBox(
                            0,
                            aty,
                            toplen,
                            m_font->getHeight(false),
                            selectionBG,
                            1.f
                        );
                        m_font->Print(0, aty, cur, botsel->column, UColor(0.0f, 0.0f, 0.0f, 0.0f), false);
                    }

                    if (toplen < linewidth) { 
                        m_font->setColor(m_foreground_color);
                        m_font->Print(toplen, aty, &cur[botsel->column], -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), false);
                    }

                    if (botsel == &m_selection.end) {
                        caret = toplen;
                    }
                }
            } else if (i != botsel->line) {
                int toplen = m_font->getWidth(cur, topsel->column, false); 
                if (topsel->column) { 
                    m_font->setColor(m_foreground_color);
                    m_font->Print(0, aty, cur, topsel->column, UColor(0.0f, 0.0f, 0.0f, 0.0f), false);
                }

                if (toplen < linewidth) { 
                    m_font->setColor(selectionColor);
                    DrawBox(
                        toplen,
                        aty,
                        (linewidth - toplen),
                        m_font->getHeight(false),
                        selectionBG,
                        1.f
                    );
                    m_font->Print(toplen, aty, &cur[topsel->column], -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), false);
                }

                if (topsel == &m_selection.end) {
                    caret = toplen;
                }
            } else {
                if (topsel->column == botsel->column) {
                    caret = m_font->getWidth(cur, topsel->column, false);
                    m_font->setColor(m_foreground_color);
                    m_font->Print(0, aty, cur, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), false);
                } else {
                    int toplen = m_font->getWidth(cur, topsel->column, false); 
                    int botlen = toplen
                               + m_font->getWidth(
                                   &cur[topsel->column], botsel->column - topsel->column, false
                               ); 

                    if (toplen) { 
                        m_font->setColor(m_foreground_color);
                        m_font->Print(0, aty, cur, topsel->column, UColor(0.0f, 0.0f, 0.0f, 0.0f), false);
                    }

                    if (botlen != toplen) { 
                        DrawBox(
                            toplen,
                            aty,
                            (botlen - toplen),
                            m_font->getHeight(false),
                            selectionBG,
                            1.f
                        );
                        m_font->setColor(selectionColor);
                        m_font->Print(
                            toplen,
                            aty,
                            &cur[topsel->column],
                            botsel->column - topsel->column, UColor(0.0f, 0.0f, 0.0f, 0.0f), false);
                    }

                    if (cur.length() != (size_t)botsel->column) { 
                        m_font->setColor(m_foreground_color);

                        m_font->Print(botlen, aty, &cur[botsel->column], -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), false);
                    }

                    caret = botlen;
                    if (topsel == &m_selection.end) {
                        caret = toplen;
                    }
                }

                m_font->getWidth(cur, botsel->column, false);
                m_font->getWidth(cur, topsel->column, false);
            }
        }

        if (m_selection.end.line == i && (uid.time % 750) >= 375 && IsActive()) {
            DrawBox(caret, aty, 2.f, m_font->getHeight(false), UBlack, 1.f);
        }

        aty += m_font->getHeight(false);
    }
}

str& UIMultiLineEdit::LineFromLineNumber(int num, bool resetpos)
{
    static str emptyLine;
    void      *pos;
    int        i;

    pos = m_lines.getPosition();

    for (i = 0, m_lines.IterateFromHead(); i < num && m_lines.IsCurrentValid(); i++) {
        m_lines.IterateNext();
    }

    if (m_lines.IsCurrentValid()) {
        str& ret = m_lines.getCurrent();
        if (resetpos) {
            m_lines.setPosition(pos);
        }

        return ret;
    }

    if (resetpos) {
        m_lines.setPosition(pos);
    }

    return emptyLine;
}

void UIMultiLineEdit::PointToSelectionPoint(const UIPoint2D& p, selectionpoint_t& sel)
{
    int   clickedLine;
    int   i;
    float totalWidth = 0;
    float lastWidth  = 0;

    clickedLine = m_vertscroll->getTopItem() + p.y / m_font->getHeight(false);
    clickedLine = Q_min(clickedLine, m_lines.getCount() - 1);

    if (clickedLine < 0) {
        sel.line   = 0;
        sel.column = 0;
        return;
    }

    const char *line = LineFromLineNumber(clickedLine, true).c_str();
    for (i = 0; line[i] && totalWidth < p.x; i++) {
        lastWidth = m_font->getCharWidth(line[i], false);
        totalWidth += lastWidth;
    }

    if (line[i] && i) {
        lastWidth *= 0.5f;
        if (totalWidth - lastWidth >= p.x) {
            i--;
        }
    }

    sel.line   = clickedLine;
    sel.column = i;
}

void UIMultiLineEdit::MouseDown(Event *ev)
{
    PointToSelectionPoint(MouseEventToClientPoint(ev), m_selection.begin);
    m_selection.end = m_selection.begin;
    EnsureSelectionPointVisible(m_selection.end);
    m_mouseState = M_DRAGGING;
    uWinMan.setFirstResponder(this);
}

void UIMultiLineEdit::MouseUp(Event *ev)
{
    CancelEventsOfType(EV_Time_Dragged);

    if (uWinMan.getFirstResponder() == this) {
        uWinMan.setFirstResponder(NULL);
    }

    if (m_mouseState == M_DRAGGING) {
        EnsureSelectionPointVisible(m_selection.end);
    }

    m_mouseState = M_NONE;
}

void UIMultiLineEdit::MouseDragged(Event *ev)
{
    m_dragState.lastPos = MouseEventToClientPoint(ev);
    PointToSelectionPoint(m_dragState.lastPos, m_selection.end);

    if (!EventPending(EV_Time_Dragged)) {
        PostEvent(new Event(EV_Time_Dragged), 0);
    }
}

void UIMultiLineEdit::DragTimer(Event *ev)
{
    int oldtop;

    PointToSelectionPoint(m_dragState.lastPos, m_selection.end);

    oldtop = m_vertscroll->getTopItem();
    EnsureSelectionPointVisible(m_selection.end);

    if (m_vertscroll->getTopItem() != oldtop) {
        PostEvent(new Event(EV_Time_Dragged), 0.25f);
    }
}

void UIMultiLineEdit::EnsureSelectionPointVisible(const selectionpoint_t& point)
{
    int newtop;

    if (point.line >= m_vertscroll->getTopItem()
        && point.line < m_vertscroll->getPageHeight() + m_vertscroll->getTopItem()) {
        return;
    }

    if (m_vertscroll->getTopItem() > point.line) {
        newtop = point.line;
    } else {
        newtop = point.line - m_vertscroll->getPageHeight() + 1;
    }

    if (newtop < 0) {
        newtop = 0;
    }

    m_vertscroll->setTopItem(newtop);
}

int UIMultiLineEdit::BoundSelectionPoint(selectionpoint_t& point)
{
    int count = m_lines.getCount();

    if (point.line >= count) {
        point.line = count - 1;
    }

    if (point.line < 0) {
        point.line = 0;
    }

    str& line = LineFromLineNumber(point.line, true);

    if (point.column > (int)line.length()) {
        point.column = line.length();
    }

    if (point.column < 0) {
        point.column = 0;
    }

    return line.length();
}

qboolean UIMultiLineEdit::KeyEvent(int key, unsigned int time)
{
    bool     caret_moved = false;
    qboolean key_rec     = false;

    switch (key) {
    case K_UPARROW:
        m_selection.end.line--;
        caret_moved = true;
        key_rec     = true;
        break;
    case K_DOWNARROW:
        m_selection.end.line++;
        caret_moved = true;
        key_rec     = true;
        break;
    case K_LEFTARROW:
        m_selection.end.column--;
        if (m_selection.end.column < 0) {
            m_selection.end.column = 999999;
            m_selection.end.line--;
        }
        if (m_selection.end.line < 0) {
            m_selection.end.column = 0;
        }

        caret_moved = true;
        key_rec     = true;
        break;
    case K_RIGHTARROW:
        {
            int oldcol = m_selection.end.column;

            m_selection.end.column++;
            BoundSelectionPoint(m_selection.end);
            if (m_selection.end.column == oldcol) {
                m_selection.end.column = 0;
                m_selection.end.line++;
                if (m_selection.end.line >= m_lines.getCount()) {
                    m_selection.end.line--;
                    m_selection.end.column = 999999;
                }
            }

            caret_moved = true;
            key_rec     = true;
            break;
        }
    case K_INS:
        if (!IsSelectionEmpty()) {
            if (uii.Sys_IsKeyDown(K_CTRL)) {
                CopySelection();
            } else if (uii.Sys_IsKeyDown(K_SHIFT)) {
                PasteSelection();
            }
        }
        key_rec = true;
        break;
    case K_DEL:
        if (IsSelectionEmpty()) {
            m_shiftForcedDown = true;
            KeyEvent(K_RIGHTARROW, 0);
            m_shiftForcedDown = false;
        } else {
            if (uii.Sys_IsKeyDown(K_SHIFT)) {
                CopySelection();
            }

            DeleteSelection();
        }
        key_rec = true;
        break;
    case K_PGDN:
        if (m_selection.end.line == (m_vertscroll->getTopItem() + m_vertscroll->getPageHeight() - 1)) {
            m_selection.end.line = m_vertscroll->getPageHeight() + m_selection.end.line;
        } else {
            m_selection.end.line = m_vertscroll->getTopItem() + m_vertscroll->getPageHeight() - 1;
        }

        caret_moved = true;
        key_rec     = true;
        break;
    case K_PGUP:
        if (m_selection.end.line == m_vertscroll->getTopItem()) {
            m_selection.end.line -= m_vertscroll->getPageHeight();
        } else {
            m_selection.end.line = m_vertscroll->getTopItem();
        }

        caret_moved = true;
        key_rec     = true;
        break;
    case K_HOME:
        if (uii.Sys_IsKeyDown(K_CTRL)) {
            m_selection.end.column = 0;
            m_selection.end.line   = 0;
        } else {
            m_selection.end.column = 0;
        }

        caret_moved = true;
        key_rec     = true;
        break;
    case K_END:
        if (uii.Sys_IsKeyDown(K_CTRL)) {
            m_selection.end.column = 999999;
            m_selection.end.line   = m_lines.getCount();
        } else {
            m_selection.end.column = 999999;
        }

        caret_moved = true;
        key_rec     = true;
        break;
    case K_MWHEELDOWN:
        m_vertscroll->AttemptScrollTo(m_vertscroll->getTopItem() + 2);
        key_rec = true;
        break;
    case K_MWHEELUP:
        m_vertscroll->AttemptScrollTo(m_vertscroll->getTopItem() - 2);
        key_rec = true;
        break;
    }

    if (caret_moved) {
        BoundSelectionPoint(m_selection.end);
        if (!uii.Sys_IsKeyDown(K_SHIFT) && !m_shiftForcedDown) {
            m_selection.begin.line   = m_selection.end.line;
            m_selection.begin.column = m_selection.end.column;
        }
        EnsureSelectionPointVisible(m_selection.end);
    }

    return key_rec;
}

void UIMultiLineEdit::DeleteSelection(void)
{
    selectionpoint_t *topsel;
    selectionpoint_t *botsel;
    int               i;

    if (IsSelectionEmpty()) {
        return;
    }

    m_changed = true;
    SortSelection(&topsel, &botsel);

    if (topsel->line == botsel->line) {
        str&     line   = LineFromLineNumber(topsel->line, true);
        intptr_t newlen = line.length() - (botsel->column - topsel->column);

        for (i = topsel->column; i < newlen; i++) {
            line[i] = line[i + botsel->column - topsel->column];
        }

        line.CapLength(newlen);
        *botsel = *topsel;
        EnsureSelectionPointVisible(*topsel);
        return;
    } else if (botsel->line - topsel->line > 1) {
        for (i = 0, m_lines.IterateFromHead(); m_lines.IsCurrentValid() && i < botsel->line;
             i++, m_lines.IterateNext()) {
            if (i > topsel->line) {
                m_lines.RemoveCurrentSetPrev();
                i--;
                botsel->line--;
            }
        }
    }

    for (i = 0, m_lines.IterateFromHead(); m_lines.IsCurrentValid() && i < topsel->line; i++, m_lines.IterateNext()) {}

    if (i != topsel->line) {
        return;
    }

    str& topline = m_lines.getCurrent();
    if (topline.length() > (size_t)topsel->column) {
        topline.CapLength(topsel->column);
    }
    m_lines.IterateNext();
    if (!m_lines.IsCurrentValid()) {
        return;
    }
    str line = m_lines.getCurrent();

    if (line.length() > (size_t)botsel->column) {
        topline += &line[botsel->column];
    }

    m_lines.RemoveCurrentSetPrev();
    *botsel = *topsel;
    m_vertscroll->setNumItems(m_lines.getCount());
    EnsureSelectionPointVisible(*topsel);
}

void UIMultiLineEdit::CharEvent(int ch)
{
    intptr_t i;

    if (!m_edit) {
        return;
    }

    if ((ch >= ' ' && ch < 127) || ch == '\t') {
        DeleteSelection();
        m_changed = true;

        str& line = LineFromLineNumber(m_selection.begin.line, true);
        line += " ";

        for (i = line.length() - 1; i > m_selection.begin.column; i--) {
            line[i] = line[i - 1];
        }
        line[m_selection.begin.column] = ch;
        m_selection.begin.column++;
        m_selection.end.column++;
    } else if (ch == '\b') {
        if (IsSelectionEmpty()) {
            m_shiftForcedDown = true;
            KeyEvent(K_LEFTARROW, 0);
            m_shiftForcedDown = false;
        }
        DeleteSelection();
    } else if (ch == '\r') {
        DeleteSelection();
        m_changed = true;

        str& line      = LineFromLineNumber(m_selection.begin.line, false);
        str  otherline = "";
        otherline = &line[m_selection.begin.column];
        line.CapLength(m_selection.begin.column);

        if (m_lines.IsCurrentValid()) {
            m_lines.InsertAfterCurrent(otherline);
        } else {
            m_lines.AddTail(otherline);
        }

        m_selection.begin.column = 0;
        m_selection.begin.line++;
        m_selection.end.column = m_selection.begin.column;
        m_selection.end.line   = m_selection.begin.line;
        m_vertscroll->setNumItems(m_lines.getCount());

        EnsureSelectionPointVisible(m_selection.end);
    }
}

bool UIMultiLineEdit::IsSelectionEmpty(void)
{
    return m_selection.begin.column == m_selection.end.column && m_selection.begin.line == m_selection.end.line;
}

void UIMultiLineEdit::CopySelection(void)
{
    selectionpoint_t *topsel;
    selectionpoint_t *botsel;
    str               line;
    str               clipText;

    if (IsSelectionEmpty()) {
        return;
    }

    SortSelection(&topsel, &botsel);

    line = LineFromLineNumber(topsel->line, true);

    clipText += &line[topsel->column];

    if (topsel->line == botsel->line) {
        clipText.CapLength(botsel->column - topsel->column);
    } else {
        for (int i = topsel->line + 1; i < botsel->line; ++i) {
            clipText += "\n" + LineFromLineNumber(i, 1);
        }

        line = LineFromLineNumber(botsel->line, true);
        if (line.length() > (size_t)botsel->column) {
            line.CapLength(botsel->column);
        }
        clipText += "\n" + line;
    }

    uii.Sys_SetClipboard(clipText);
}

void UIMultiLineEdit::PasteSelection(void)
{
    str sel;
    int i;

    const char *clipboardData = uii.Sys_GetClipboard();

    if (clipboardData) {
        sel = clipboardData;
    }

    DeleteSelection();

    for (i = 0; (size_t)i < sel.length(); i++) {
        if (sel[i] == '\n') {
            CharEvent('\r');
        } else {
            CharEvent(sel[i]); 
        }
    }
}

UIPoint2D UIMultiLineEdit::getEndSelPoint(void)
{
    return UIPoint2D(m_selection.end.column, m_selection.end.line);
}

void UIMultiLineEdit::setChanged(bool b)
{
    m_changed = b;
}

bool UIMultiLineEdit::IsChanged(void)
{
    return m_changed;
}
