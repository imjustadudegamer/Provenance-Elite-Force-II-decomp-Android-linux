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

#define PROBE_GET(probe_n, probe_d, probe_f) ((cvar_t *)0)
#define PROBE_PRINT                       Com_Printf
#define PROBE_ON_OVERRIDE                       (atoi(CvarGetForUI("qa_probe", "0")) != 0)  
#include "../qcommon/qa_probe.h"



void UIRowColumn::hoveredColumnCommand(Event *ev)
{
    hoverColumnCommand(ev);
}

void UIRowColumn::sortColumnHandler(Event *ev)
{
    sortByColumn(ev);
}

void UIRowColumn::selectRowHandler(Event *ev)
{
    selectRow(ev);
}

void UIRowColumn::MouseHoverExit(Event *ev)
{
    MouseExited(ev);
}

void UIRowColumn::addColumn(Event *ev)
{
    str templateName = ev->GetString(1);
    str dataType     = ev->GetString(3);
    int width        = ev->GetInteger(2);

    addColumn(templateName, width, dataType);
}

void UIRowColumn::addColumn(const str& templateName, int width, const str& dataType)
{
    UIColumn *col = new UIColumn;

    col->headerWidget = uWinMan.FindWidget(templateName);

    if (col->headerWidget) {
        col->headerWidget->Disable();
    }

    col->width     = (float)width;
    col->baseWidth = (float)width;
    col->setDataType(dataType);

    m_columns.AddObject(col);
}

UIRow *UIRowColumn::createRow(void)
{
    UIRow *row = new UIRow;

    if (!row) {
        return NULL;
    }

    UIRect2D frame = getFrame();
    // UIROWS: the y argument is retail's second pool float (0x00218988 = 0.0f), NOT the
    row->InitFrame(this, 0.0f, 0.0f, frame.size.width - getScrollBarWidth(), getItemHeight(), -1, "tahoma-8");
    row->setBackgroundColor(UColor(0, 0, 0, 0), WIDGET_STATE_NORMAL, true);
    row->setBorderStyle(border_none);

    m_rows.AddObject(row);
    setNumItems(m_rows.NumObjects());

    return row;
}

UIWidget *UIRowColumn::createColumnWidget(int columnIndex, const str& cellType, const str& name)
{
    if (columnIndex < 1 || columnIndex > m_columns.NumObjects()) {
        return NULL;
    }

    UIColumn *col = m_columns.ObjectAt(columnIndex);

    ClassDef *columnClass = getClass(col->headerWidget->getClassname());
    if (!columnClass) {
        return NULL;
    }

    UIWidget *widget = (UIWidget *)columnClass->newInstance();
    if (!widget) {
        return NULL;
    }

    col->headerWidget->copyProperties(*widget);
    widget->setData(cellType);
    widget->setName(name);

    return widget;
}

void UIRowColumn::addRow(Event *ev)
{
    UIRow *row = createRow();

    if (!row) {
        return;
    }

    for (int i = 1; i <= ev->NumArgs(); i++) {
        if (i > m_columns.NumObjects()) {
            return;
        }

        str cellType = ev->GetString(i);
        str name     = "";

        UIWidget *widget = createColumnWidget(i, cellType, name);
        if (!widget) {
            return;
        }

        if (m_backgroundRowColorRequested == 1) {
            widget->setBackgroundColor(m_backgroundRowColor, WIDGET_STATE_NORMAL, true);
        }
        if (m_foregroundRowColorRequested == 1) {
            widget->setForegroundColor(m_foregroundRowColor, WIDGET_STATE_NORMAL);
        }

        float width = m_columns.ObjectAt(i)->width;
        row->addWidget(widget, width, getItemHeight());
    }

    row->setNoRowHighlight(m_noRowHighlight);
    row->setNoRowSelect(m_noRowSelect);

    if (m_sortColumn != -1) {
        sortByColumn(m_sortColumn);
    }
}

void UIRowColumn::addRowWidgets(Event *ev)
{
    UIRow *row = createRow();

    if (!row) {
        return;
    }

    for (int i = 1; i <= ev->NumArgs(); i++) {
        str       name   = ev->GetString(i);
        UIWidget *widget = uWinMan.FindWidget(name);

        if (widget) {
            float width = m_columns.ObjectAt(i)->width;
            row->addWidget(widget, width, getItemHeight());

            row->setNoRowHighlight(m_noRowHighlight);
            row->setNoRowSelect(m_noRowSelect);
        }
    }
}

void UIRowColumn::addItem(Event *ev)
{
    if (!m_columns.NumObjects()) {
        return;
    }

    UIRow *row;
    if (m_rows.NumObjects()) {
        row = m_rows.ObjectAt(m_rows.NumObjects());
    } else {
        row = createRow();
    }

    int i = row->getNumCells() + 1;
    if (i > m_columns.NumObjects()) {
        i   = 1;
        row = createRow();
    }

    float width = m_columns.ObjectAt(i)->width;
    str   text  = ev->GetString(1);
    str   name  = ev->GetString(2);    

    UIWidget *widget = createColumnWidget(i, text, name);
    if (!widget) {
        return;
    }

    if (m_backgroundRowColorRequested == 1) {
        widget->setBackgroundColor(m_backgroundRowColor, WIDGET_STATE_NORMAL, true);
    }
    if (m_foregroundRowColorRequested == 1) {
        widget->setForegroundColor(m_foregroundRowColor, WIDGET_STATE_NORMAL);
    }

    row->addWidget(widget, width, getItemHeight());
}

void UIRowColumn::setRow(Event *ev)
{
    int rowIndex = ev->GetInteger(1);

    UIRow *row = m_rows.ObjectAt(rowIndex);
    if (!row) {
        return;
    }

    for (int i = 2, j = 1; i <= ev->NumArgs(); i++, j++) {
        if (j > m_columns.NumObjects()) {
            break;
        }

        UIWidget *widget = row->getWidget(j);
        if (!widget) {
            continue;
        }

        widget->setData(ev->GetString(i));
    }
}

void UIRowColumn::selectRow(Event *ev)
{
    int index = ev->GetInteger(1);

    if (index < 1 || index > m_rows.NumObjects()) {
        return;
    }

    UIRow *row = m_rows.ObjectAt(index);
    if (!row) {
        return;
    }

    setSelectedRow(row);
    executeCommand(WIDGET_STATE_SELECTED, row);
    executeColumnCommand(WIDGET_STATE_SELECTED, row);
}

void UIRowColumn::selectRowWithData(Event *ev)
{
    int  columnIndex = ev->GetInteger(1);
    str  data        = ev->GetString(2);

    if (columnIndex > m_columns.NumObjects()) {
        return;
    }

    for (int i = 1; i <= m_rows.NumObjects(); i++) {
        UIRow *row = m_rows.ObjectAt(i);

        UIWidget *widget = row->getWidget(columnIndex);
        if (!widget) {
            continue;
        }

        if (!str::icmp(widget->getData(), data.c_str())) {
            setSelectedRow(row);
            executeCommand(WIDGET_STATE_SELECTED, row);
            executeColumnCommand(WIDGET_STATE_SELECTED, row);
        }
    }
}

void UIRowColumn::setSelectedRow(UIRow *row)
{
    if (!row) {
        return;
    }

    if (m_selectedRow == row) {
        return;
    }

    if (m_selectedRow) {
        m_selectedRow->setSelected(false);
    }

    m_selectedRow = row;
    row->setSelected(true);
}

void UIRowColumn::MouseEntered(Event *ev)
{
    if (m_widgetState == WIDGET_STATE_DISABLED) {
        return;
    }

    UIRow *row = (UIRow *)ev->GetSourceObject();
    if (row) {
        executeCommand(WIDGET_STATE_HOVER, row);
        executeColumnCommand(WIDGET_STATE_HOVER, row);
    }

    UIWidget::MouseEntered(ev);

    if (!m_noFocus) {
        uWinMan.ActivateControl(NULL);
    }
}

void UIRowColumn::noFocus(Event *ev)
{
    m_noFocus = true;
}

void UIRowColumn::MouseExited(Event *ev)
{
    UIWidget::MouseExited(ev);
}

void UIRowColumn::Pressed(Event *ev)
{
    UIRow *row = (UIRow *)ev->GetSourceObject();

    if (!row) {
        return;
    }

    float x = ev->GetFloat(1);
    float y = ev->GetFloat(2);

    x -= m_screenorigin.x;
    y -= m_screenorigin.y;

    UIRect2D frame = getFrame();

    bool sameSpot = false;
    if (x == m_lastClickX) {
        sameSpot = (y == m_lastClickY);
    }

    int now = uid.time;

    if (sameSpot) {
        if (x >= frame.size.width || now - m_lastClickTime >= 500) {
            executeCommand(WIDGET_STATE_SELECTED, row);
            executeColumnCommand(WIDGET_STATE_SELECTED, row);
        } else {
            executeCommand(WIDGET_STATE_ARMED, row);
            executeColumnCommand(WIDGET_STATE_ARMED, row);
        }
    } else {
        if (m_selectedRow != row) {
            executeCommand(WIDGET_STATE_SELECTED, row);
        }
        executeColumnCommand(WIDGET_STATE_SELECTED, row);
    }

    if (m_selectedRow != row) {
        if (m_selectedRow) {
            m_selectedRow->setSelected(false);
        }
        m_selectedRow = row;
        row->setSelected(true);
    }

    m_lastClickTime = now;
    m_lastClickX    = x;
    m_lastClickY    = y;

    UIWidget::Pressed(ev);
    setHoverState();
}

void UIRowColumn::deselectAll(Event *ev)
{
    for (int i = 1; i <= m_rows.NumObjects(); i++) {
        m_rows.ObjectAt(i)->setSelected(false);
    }

    m_selectedRow = NULL;
}

void UIRowColumn::Draw(void)
{
    UIScrollable::Draw();

    UIRect2D frame = getFrame();
    float    y     = (float)(int)m_firstDrawnY;

    for (int i = 1; i <= getNumItems(); i++) {
        UIRow *row = m_rows.ObjectAt(i);

        if (i - 1 >= m_topItem && i - 1 <= m_bottomItem) {
            row->setShow(true);

            UIRect2D rowFrame = row->getFrame();
            rowFrame.pos.x        = 0.0f;
            rowFrame.pos.y        = y;
            rowFrame.size.width   = frame.size.width - getScrollBarWidth();
            rowFrame.size.height  = getItemHeight();
            row->setFrame(rowFrame);

            y += (float)(int)getItemHeight();
        } else {
            row->setShow(false);
        }
    }

    static int uirows_rows;
    static int uirect_sb;
    if (PROBE_On()
        && (!strcmp(getName(), "dmscore") ? PROBE_Throttle(&uirect_sb, 8, 200) : PROBE_Throttle(&uirows_rows, 4, 400))) {
        UIRow    *row1  = m_rows.NumObjects() >= 1 ? m_rows.ObjectAt(1) : NULL;
        UIWidget *cell  = row1 ? row1->getWidget(1) : NULL;
        UIVertScroll *sb = getScrollBar();

        UIRECT_PROBE(
            "sb:rowrender",
            "rc='%s' rows=%d cols=%d itemh=%.1f top=%d bottom=%d sb=%d sbnum=%d sbpage=%d sbtop=%d "
            "fr=(%.0f,%.0f %.0fx%.0f) "
            "row1=%s row1kids=%d row1vis=%d row1fr=(%.0f,%.0f %.0fx%.0f) row1scr=(%.0f,%.0f) "
            "row1clip=(%.0f,%.0f %.0fx%.0f) "
            "cell1=%s cell1vis=%d cell1fr=(%.0f,%.0f %.0fx%.0f) cell1scr=(%.0f,%.0f) "
            "cell1clip=(%.0f,%.0f %.0fx%.0f) cell1font=%d cell1virt=%d cell1data='%s'\n",
            getName(),
            m_rows.NumObjects(),
            m_columns.NumObjects(),
            getItemHeight(),
            m_topItem,
            m_bottomItem,
            sb ? 1 : 0,
            sb ? sb->getNumItems() : -1,
            sb ? sb->getPageHeight() : -1,
            sb ? sb->getTopItem() : -1,
            frame.pos.x,
            frame.pos.y,
            frame.size.width,
            frame.size.height,
            row1 ? row1->getClassname() : "(none)",
            row1 ? row1->getNumCells() : 0,
            (row1 && row1->getShow()) ? 1 : 0,
            row1 ? row1->getFrame().pos.x : 0.0f,
            row1 ? row1->getFrame().pos.y : 0.0f,
            row1 ? row1->getFrame().size.width : 0.0f,
            row1 ? row1->getFrame().size.height : 0.0f,
            row1 ? row1->UIRECTProbeScreenOrigin().x : 0.0f,
            row1 ? row1->UIRECTProbeScreenOrigin().y : 0.0f,
            row1 ? row1->UIRECTProbeClippedFrame().pos.x : 0.0f,
            row1 ? row1->UIRECTProbeClippedFrame().pos.y : 0.0f,
            row1 ? row1->UIRECTProbeClippedFrame().size.width : 0.0f,
            row1 ? row1->UIRECTProbeClippedFrame().size.height : 0.0f,
            cell ? cell->getClassname() : "(none)",
            (cell && cell->getShow()) ? 1 : 0,
            cell ? cell->getFrame().pos.x : 0.0f,
            cell ? cell->getFrame().pos.y : 0.0f,
            cell ? cell->getFrame().size.width : 0.0f,
            cell ? cell->getFrame().size.height : 0.0f,
            cell ? cell->UIRECTProbeScreenOrigin().x : 0.0f,
            cell ? cell->UIRECTProbeScreenOrigin().y : 0.0f,
            cell ? cell->UIRECTProbeClippedFrame().pos.x : 0.0f,
            cell ? cell->UIRECTProbeClippedFrame().pos.y : 0.0f,
            cell ? cell->UIRECTProbeClippedFrame().size.width : 0.0f,
            cell ? cell->UIRECTProbeClippedFrame().size.height : 0.0f,
            (cell && cell->getFont()) ? 1 : 0,
            cell ? (cell->UIRECTProbeVirtual() ? 1 : 0) : 0,
            cell ? cell->getData() : ""
        );
    }

    if (PROBE_On() && !strcmp(getName(), "dmscore") && m_rows.NumObjects()) {
        static int widget_cells;

        if (PROBE_Throttle(&widget_cells, 4, 200)) {
            UIRow *row1 = m_rows.ObjectAt(1);

            WIDGET_PROBE(
                "sb:cells",
                "rows=%d cols=%d itemh=%.1f top=%d bottom=%d\n",
                m_rows.NumObjects(),
                m_columns.NumObjects(),
                getItemHeight(),
                m_topItem,
                m_bottomItem);

            for (int c = 1; c <= m_columns.NumObjects() && c <= 13; c++) {
                UIWidget *cell = row1->getWidget(c);

                if (!cell) {
                    WIDGET_PROBE("sb:cells", "  col%d=(none)\n", c);
                    continue;
                }

                WIDGET_PROBE(
                    "sb:cells",
                    "  col%d class=%s w=%.0f mat='%s' data='%s' vis=%d\n",
                    c,
                    cell->getClassname(),
                    m_columns.ObjectAt(c)->width,
                    cell->WIDGETProbeMaterialName(),
                    cell->getData(),
                    cell->getShow() ? 1 : 0);
            }
        }
    }
}

int UIRowColumn::getNumItems(void)
{
    return m_rows.NumObjects();
}

void UIRowColumn::sortByColumn(Event *ev)
{
    int columnIndex = ev->GetInteger(1);

    if (columnIndex > 0 && columnIndex <= m_columns.NumObjects()) {
        sortByColumn(columnIndex);
    }
}

void UIRowColumn::sortByColumn(int columnIndex)
{
    if (columnIndex <= 0 || columnIndex > m_columns.NumObjects()) {
        return;
    }

    m_sortColumn = columnIndex;

    int (*compare)(const str&, const str&);
    switch (m_columns.ObjectAt(columnIndex)->dataType) {
    case 1:
        compare = &UIRowColumn::alphaCompare;
        break;
    case 2:
    case 3:
        compare = &UIRowColumn::floatCompare;
        break;
    case 0:
        compare = &UIRowColumn::integerCompare;
        break;
    default:
        compare = NULL;
        break;
    }

    if (compare) {
        sortRows(compare, columnIndex);
    }
}

void UIRowColumn::sortRows(int (*compare)(const str&, const str&), int columnIndex)
{
    int direction;

    if (m_sortType == 2) {
        direction = m_columns.ObjectAt(columnIndex)->sortDirection;
    } else if (m_sortType) {
        direction = -1;
    } else {
        direction = 1;
    }

    int n = m_rows.NumObjects();
    for (int i = 1; i <= n; i++) {
        for (int j = i + 1; j <= n; j++) {
            UIWidget *wi = m_rows.ObjectAt(i)->getWidget(columnIndex);
            if (!wi) {
                continue;
            }

            UIWidget *wj = m_rows.ObjectAt(j)->getWidget(columnIndex);
            if (!wj) {
                continue;
            }

            if (direction * compare(wi->getData(), wj->getData()) > 0) {
                UIRow *ri = m_rows.ObjectAt(i);
                UIRow *rj = m_rows.ObjectAt(j);
                m_rows.RemoveObjectAt(j);
                m_rows.RemoveObjectAt(i);
                m_rows.InsertObjectAt(i, rj);
                m_rows.InsertObjectAt(j, ri);
            }
        }
    }

    UIColumn *col = m_columns.ObjectAt(columnIndex);
    col->sortDirection = -col->sortDirection;
}

int UIRowColumn::alphaCompare(const str& s1, const str& s2)
{
    return str::icmp(s1.c_str(), s2.c_str());
}

int UIRowColumn::integerCompare(const str& s1, const str& s2)
{
    return atoi(s1.c_str()) - atoi(s2.c_str());
}

int UIRowColumn::floatCompare(const str& s1, const str& s2)
{
    return (int)(atof(s1.c_str()) - atof(s2.c_str()));
}

void UIRowColumn::ChangeResolution(void)
{
    UIScrollable::ChangeResolution();

    for (int i = 1; i <= m_columns.NumObjects(); i++) {
        UIColumn *col = m_columns.ObjectAt(i);
        col->width    = uid.scaleRes[0] * col->baseWidth;
    }
}

void UIRowColumn::DeleteItems(Event *ev)
{
    DeleteItems();
}

void UIRowColumn::DeleteItems(void)
{
    for (int i = 1; i <= m_rows.NumObjects(); i++) {
        UIRow *row = m_rows.ObjectAt(i);
        if (row) {
            row->setShow(false);
            removeChild(row);
            delete row;
        }
    }

    m_rows.ClearObjectList();
    setNumItems(0);
    m_selectedRow = NULL;
}

void UIRowColumn::activateCommand(Event *ev)
{
    str command = ev->GetString(1);

    m_commands[WIDGET_STATE_ARMED] = command;

    for (int i = 2; i <= ev->NumArgs(); i++) {
        m_commandArgs[WIDGET_STATE_ARMED].AddObject(ev->GetInteger(i));
    }
}

void UIRowColumn::selectedCommand(Event *ev)
{
    str command = ev->GetString(1);

    m_commands[WIDGET_STATE_SELECTED] = command;

    for (int i = 2; i <= ev->NumArgs(); i++) {
        m_commandArgs[WIDGET_STATE_SELECTED].AddObject(ev->GetInteger(i));
    }
}

void UIRowColumn::hoverCommand(Event *ev)
{
    str command = ev->GetString(1);

    m_commands[WIDGET_STATE_HOVER] = command;

    for (int i = 2; i <= ev->NumArgs(); i++) {
        m_commandArgs[WIDGET_STATE_HOVER].AddObject(ev->GetInteger(i));
    }
}

void UIRowColumn::activateColumnCommand(Event *ev)
{
    str        command = ev->GetString(1);
    int        column  = ev->GetInteger(2);
    UIColumn  *col;

    if (column < 1 || column > m_columns.NumObjects()) {
        return;
    }

    col = m_columns.ObjectAt(column);
    col->setCommand(command, WIDGET_STATE_ARMED);

    for (int i = 3; i <= ev->NumArgs(); i++) {
        col->setCommandIndex(ev->GetInteger(i), WIDGET_STATE_ARMED);
    }
}

void UIRowColumn::selectedColumnCommand(Event *ev)
{
    str       command = ev->GetString(1);
    int       column  = ev->GetInteger(2);
    UIColumn *col;

    if (column < 1 || column > m_columns.NumObjects()) {
        return;
    }

    col = m_columns.ObjectAt(column);
    col->setCommand(command, WIDGET_STATE_SELECTED);

    for (int i = 3; i <= ev->NumArgs(); i++) {
        col->setCommandIndex(ev->GetInteger(i), WIDGET_STATE_SELECTED);
    }
}

void UIRowColumn::hoverColumnCommand(Event *ev)
{
    str       command = ev->GetString(1);
    int       column  = ev->GetInteger(2);
    UIColumn *col;

    if (column < 1 || column > m_columns.NumObjects()) {
        return;
    }

    col = m_columns.ObjectAt(column);
    col->setCommand(command, WIDGET_STATE_HOVER);

    for (int i = 3; i <= ev->NumArgs(); i++) {
        col->setCommandIndex(ev->GetInteger(i), WIDGET_STATE_HOVER);
    }
}

void UIRowColumn::activate(Event *ev)
{
    if (m_selectedRow) {
        executeCommand(WIDGET_STATE_ARMED, m_selectedRow);
        executeColumnCommand(WIDGET_STATE_ARMED, m_selectedRow);
    }
}

void UIRowColumn::noRowHighlight(Event *ev)
{
    m_noRowHighlight = true;
}

void UIRowColumn::noRowSelect(Event *ev)
{
    m_noRowSelect = true;
}

void UIRowColumn::sortType(Event *ev)
{
    str type = ev->GetString(1);

    if (!str::icmp(type.c_str(), "descending")) {
        m_sortType = 1;
    } else if (!str::icmp(type.c_str(), "ascending")) {
        m_sortType = 0;
    } else if (!str::icmp(type.c_str(), "alternate")) {
        m_sortType = 2;
    } else {
        m_sortType = 0;
    }
}

void UIRowColumn::setBackgroundRowColor(Event *ev)
{
    float a = ev->GetFloat(4);
    float b = ev->GetFloat(3);
    float g = ev->GetFloat(2);
    float r = ev->GetFloat(1);

    m_backgroundRowColor            = UColor(r, g, b, a);
    m_backgroundRowColorRequested   = true;
}

void UIRowColumn::setForegroundRowColor(Event *ev)
{
    float a = ev->GetFloat(4);
    float b = ev->GetFloat(3);
    float g = ev->GetFloat(2);
    float r = ev->GetFloat(1);

    m_foregroundRowColor            = UColor(r, g, b, a);
    m_foregroundRowColorRequested   = true;
}

void UIRowColumn::executeCommand(int state, UIRow *row)
{
    if (!row) {
        return;
    }

    str line = m_commands[state];

    Container<int> *args = &m_commandArgs[state];
    for (int i = 1; i <= args->NumObjects(); i++) {
        int index = args->ObjectAt(i);
        line += " ";

        UIColumn *col = m_columns.ObjectAt(index);
        UIWidget *widget = row->getWidget(index);

        if (col->dataType) {
            line += widget->getData();
        } else {
            line += "\"";
            if (!widget) {
                Com_WPrintf("Invalid widget index: %d\n", index);
                return;
            }
            line += widget->getData();
            line += "\"";
        }
    }

    line += "\n";
    uii.Cmd_Stuff(line.c_str());
}

void UIRowColumn::executeColumnCommand(WidgetState state, UIRow *row)
{
    if (!row) {
        Com_Printf("Invalid row\n");
        return;
    }

    int index;
    switch (state) {
    case WIDGET_STATE_SELECTED:
        index = row->getSelectedWidgetIndex();
        break;
    case WIDGET_STATE_HOVER:
        index = row->getHoveredWidgetIndex();
        break;
    case WIDGET_STATE_ARMED:
        index = row->getArmedWidgetIndex();
        break;
    default:
        Com_Printf("Unsupported state\n");
        return;
    }

    if (index < 1 || index > m_columns.NumObjects()) {
        return;
    }

    UIColumn *col = m_columns.ObjectAt(index);
    str       line = col->getCommand(state);

    for (int i = 1; i <= col->getNumCommandIndices(state); i++) {
        line += " ";

        if (col->getCommandIndex(i, state) == -1) {
            UIWidget *widget = row->getWidget(index);
            if (widget->getName()) {
                line += widget->getName();
            }
        } else {
            UIWidget *widget = row->getWidget(col->getCommandIndex(i, state));
            line += widget->getData();
        }
    }

    line += "\n";
    uii.Cmd_Stuff(line.c_str());
}


void UIRowColumn::setTopItem(int top)
{
    int total   = m_rows.NumObjects();
    int visible = VisibleRowCount();

    m_topItem = top;

    if (m_topItem + visible > total) {
        m_topItem = total - visible;
    }
    if (m_topItem < 0) {
        m_topItem = 0;
    }

    m_bottomItem = m_topItem + visible - 1;

    UIVertScroll *sb = getScrollBar();
    if (sb) {
        sb->setTopItem(m_topItem);
    }
}

void UIRowColumn::ScrollTo(int row)
{
    int visible = VisibleRowCount();

    if (row < m_topItem) {
        setTopItem(row);
    } else if (row >= m_topItem + visible) {
        setTopItem(row - visible + 1);
    }
}

int UIRowColumn::getSelectedIndex(void) const
{
    return m_selectedRow ? m_rows.IndexOfObject(m_selectedRow) : 0;
}

int UIRowColumn::VisibleRowCount(void) const
{
    float rowHeight = getItemHeight();

    if (rowHeight <= 0.0f && m_font) {
        rowHeight = m_font->getHeight(m_bVirtual);
    }
    if (rowHeight <= 0.0f) {
        return 1;
    }

    int count = (int)(m_frame.size.height / rowHeight);
    return count > 0 ? count : 1;
}

void UIRowColumn::ClampScroll(void)
{
    int total   = m_rows.NumObjects();
    int visible = VisibleRowCount();
    int maxTop  = total - visible;

    if (maxTop < 0) {
        maxTop = 0;
    }

    if (m_topItem > maxTop) {
        m_topItem = maxTop;
    }
    if (m_topItem < 0) {
        m_topItem = 0;
    }

    m_bottomItem = m_topItem + visible - 1;
}

void UIRowColumn::SyncScrollBar(void)
{
    UIVertScroll *sb = getScrollBar();

    if (!sb) {
        return;
    }

    sb->setNumItems(m_rows.NumObjects());
    sb->setPageHeight(VisibleRowCount());
    sb->setTopItem(m_topItem);
}

qboolean UIRowColumn::KeyEvent(int key, unsigned int time)
{
    int total   = m_rows.NumObjects();
    int visible = VisibleRowCount();

    if (!total) {
        return UIScrollable::KeyEvent(key, time);
    }

    /* Pad nav: an unselected list's first arrow takes row 1 rather than skipping
       it, which the populate path's own selectrow usually gives it. */
    if (!m_selectedRow && (key == K_UPARROW || key == K_DOWNARROW)) {
        m_selectedRow = m_rows.ObjectAt(1);
        m_selectedRow->setSelected(true);
        executeCommand(WIDGET_STATE_SELECTED, m_selectedRow);
        executeColumnCommand(WIDGET_STATE_SELECTED, m_selectedRow);
        return qtrue;
    }

    switch (key) {
    case K_MWHEELUP:
        setTopItem(m_topItem - 1);
        return qtrue;
    case K_MWHEELDOWN:
        setTopItem(m_topItem + 1);
        return qtrue;
    case K_PGUP:
        setTopItem(m_topItem - visible);
        return qtrue;
    case K_PGDN:
        setTopItem(m_topItem + visible);
        return qtrue;
    case K_UPARROW:
        if (m_selectedRow && m_rows.IndexOfObject(m_selectedRow) > 1) {
            int index = m_rows.IndexOfObject(m_selectedRow) - 1;
            setSelectedRow(m_rows.ObjectAt(index));
            ScrollTo(index);
            executeCommand(WIDGET_STATE_SELECTED, m_selectedRow);
            executeColumnCommand(WIDGET_STATE_SELECTED, m_selectedRow);
            return qtrue;
        }
        break;
    case K_DOWNARROW:
        if (m_rows.IndexOfObject(m_selectedRow ? m_selectedRow : m_rows.ObjectAt(1)) < total) {
            int index = (m_selectedRow ? m_rows.IndexOfObject(m_selectedRow) : 1) + 1;
            setSelectedRow(m_rows.ObjectAt(index));
            ScrollTo(index);
            executeCommand(WIDGET_STATE_SELECTED, m_selectedRow);
            executeColumnCommand(WIDGET_STATE_SELECTED, m_selectedRow);
            return qtrue;
        }
        break;
    default:
        break;
    }

    return UIScrollable::KeyEvent(key, time);
}


Event EV_Table_Column
(
    "tablecolumn",
    EV_DEFAULT,
    "sisss",
    "templateName, width, datatype, headerTemplate, title",
    "Adds a column to the table"
);
Event EV_Table_HeaderHeight
(
    "headerheight", EV_DEFAULT, "i", "height", "Height of the column header widget"
);
Event EV_Table_AllowUserSort
(
    "allowusersort",
    EV_DEFAULT,
    "b",
    "canSort",
    "Specifies whether we allow the operator to sort the list."
);

CLASS_DECLARATION(UIWidget, UITable, NULL) {
    {&EV_Table_Column,                 &UITable::addTableColumn      },
    {&EV_RowColumn_AddRow,             &UITable::addRow              },
    {&EV_Scrollable_ScrollbarTemplate, &UITable::setScrollbarTemplate},
    {&EV_Scrollable_ScrollbarWidth,    &UITable::setScrollbarWidth   },
    {&EV_Table_HeaderHeight,           &UITable::setHeaderHeight     },
    {&EV_Scrollable_ItemHeight,        &UITable::setItemHeight       },
    {&EV_RowColumn_SortType,           &UITable::sortType            },
    {&EV_RowColumn_SortColumn,         &UITable::sortColumn          },
    {&EV_Table_AllowUserSort,          &UITable::allowUserSort       },
    {&EV_RowColumn_DeleteAllItems,     &UITable::deleteItems         },
    {&EV_RowColumn_ActivateCommand,    &UITable::activateCommand     },
    {&EV_RowColumn_SelectedCommand,    &UITable::selectedCommand     },
    {&EV_Layout_HoverCommand,          &UITable::hoverCommand        },
    {&EV_Layout_HoverExitCommand,      &UITable::hoverExitCommand    },
    {&EV_RowColumn_Activate,           &UITable::activate            },
    {&EV_RowColumn_SelectRow,          &UITable::selectRowHandler    },
    {&EV_RowColumn_NoRowHighlight,     &UITable::noRowHighlight      },
    {&EV_RowColumn_DeselectAll,        &UITable::deselectAll         },
    {&EV_RowColumn_NoRowSelect,        &UITable::noRowSelect         },
    {NULL,                             NULL                          }
};

UITable::UITable()
{
    m_headerHeight     = 16;
    m_headerHeightBase = 16;
    m_headerWidth  = 0.0f;
    m_allowUserSort = true;
    m_pRowColumn    = new UIRowColumn;
    m_pRowColumn->setParent(this);
    m_pRowColumn->setBackgroundColor(UColor(0, 0, 0, 0), WIDGET_STATE_NORMAL, true);
    m_pRowColumn->Enable();
}

UITable::~UITable()
{
    delete m_pRowColumn;
    m_pRowColumn = NULL;
}

void UITable::addTableColumn(Event *ev)
{
    str templateName   = ev->GetString(1);
    int width          = ev->GetInteger(2);
    str datatype       = ev->GetString(3);
    str headerTemplate = ev->GetString(4);
    str title          = ev->GetString(5);

    UIWidget *header = uWinMan.FindWidget(headerTemplate);

    UIWidget *widget = NULL;
    if (header) {
        ClassDef *headerClass = getClass(header->getClassname());
        if (headerClass) {
            widget = (UIWidget *)headerClass->newInstance();
            if (widget) {
                header->copyProperties(*widget);

                header->Disable();
            }
        }
    }

    if (widget) {
        widget->InitFrame(this, m_headerWidth, 0.0f, (float)width, (float)m_headerHeight, -1, "tahoma-8");
        widget->setTitle(title);
        m_headerWidgets.AddObject(widget);
        m_headerWidth += (float)width;
    }

    if (m_pRowColumn) {
        m_pRowColumn->addColumn(templateName, width, datatype);
    }
}

void UITable::setHeaderHeight(Event *ev)
{
    m_headerHeight     = ev->GetInteger(1);
    m_headerHeightBase = m_headerHeight;

    if (m_pRowColumn) {
        m_pRowColumn->onSizeChanged();
        LayoutRowColumn();
    }
}

void UITable::allowUserSort(Event *ev)
{
    m_allowUserSort = (ev->GetInteger(1) != 0);
}


void UITable::addRow(Event *ev)
{
    if (m_pRowColumn) {
        m_pRowColumn->addRow(ev);
    }
}

void UITable::selectedCommand(Event *ev)
{
    if (m_pRowColumn) {
        m_pRowColumn->selectedCommand(ev);
    }
}

void UITable::activateCommand(Event *ev)
{
    if (m_pRowColumn) {
        m_pRowColumn->activateCommand(ev);
    }
}

void UITable::sortType(Event *ev)
{
    if (m_pRowColumn) {
        m_pRowColumn->sortType(ev);
    }
}

void UITable::selectRowHandler(Event *ev)
{
    if (m_pRowColumn) {
        m_pRowColumn->selectRowHandler(ev);
    }
}

void UITable::setScrollbarTemplate(Event *ev)
{
    if (m_pRowColumn) {
        m_pRowColumn->scrollBarTemplate(ev);
    }
}

void UITable::setScrollbarWidth(Event *ev)
{
    if (m_pRowColumn) {
        m_pRowColumn->setScrollWidth(ev);
    }
}

void UITable::setItemHeight(Event *ev)
{
    if (m_pRowColumn) {
        m_pRowColumn->setItemHeight(ev);
    }
}

void UITable::sortColumn(Event *ev)
{
    if (m_pRowColumn) {
        m_pRowColumn->sortColumnHandler(ev);
    }
}

void UITable::hoverCommand(Event *ev)
{
    if (m_pRowColumn) {
        m_pRowColumn->hoverCommand(ev);
    }
}

void UITable::hoverExitCommand(Event *ev)
{
    if (m_pRowColumn) {
        m_pRowColumn->MouseHoverExit(ev);
    }
}

void UITable::activate(Event *ev)
{
    if (m_pRowColumn) {
        m_pRowColumn->activate(ev);
    }
}

void UITable::deselectAll(Event *ev)
{
    if (m_pRowColumn) {
        m_pRowColumn->deselectAll(ev);
    }
}

void UITable::deleteItems(Event *ev)
{
    if (m_pRowColumn) {
        m_pRowColumn->DeleteItems(ev);
    }
}

void UITable::noRowSelect(Event *ev)
{
    if (m_pRowColumn) {
        m_pRowColumn->noRowSelect(ev);
    }
}

void UITable::noRowHighlight(Event *ev)
{
    if (m_pRowColumn) {
        m_pRowColumn->noRowHighlight(ev);
    }
}


void UITable::Draw(void)
{
    UIWidget::Draw();
}

/* The pad's arrows are the list's own selection while it can still move; at the
   top or bottom row it returns the key and the container's walk moves on. */
qboolean UITable::KeyEvent(int key, unsigned int time)
{
    if (!m_pRowColumn) {
        return qfalse;
    }

    return m_pRowColumn->KeyEvent(key, time);
}

void UITable::LayoutRowColumn(void)
{
    if (!m_pRowColumn) {
        return;
    }

    m_pRowColumn->setFrame(UIRect2D(
        0.0f,
        (float)m_headerHeight,
        m_frame.size.width,
        m_frame.size.height - (float)m_headerHeight));
}

void UITable::Realign(void)
{
    UIWidget::Realign();

    if (!m_pRowColumn->getFont()) {
        m_pRowColumn->setFont(m_font ? m_font->getName().c_str() : "tahoma-8");
    }

    m_pRowColumn->Realign();
}

void UITable::ChangeResolution(void)
{
    UIWidget::ChangeResolution();
    m_headerHeight = (int)((float)m_headerHeightBase * uid.scaleRes[1]);
}

void UITable::Pressed(Event *ev)
{
    if (!m_allowUserSort || !m_pRowColumn) {
        return;
    }

    UIPoint2D p = MouseEventToClientPoint(ev);

    float atx = 0.0f;
    for (int c = 1; c <= m_pRowColumn->getNumColumns(); c++) {
        // @0x00093130 reads no scale at all (it is not one of the fourteen
        float w = m_pRowColumn->getColumn(c)->width;
        if (w > 0.0f && p.x >= atx && p.x < atx + w) {
            m_pRowColumn->sortByColumn(c);
            return;
        }
        atx += w;
    }
}
