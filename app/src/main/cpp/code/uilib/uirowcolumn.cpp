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

Event EV_RowColumn_Column
(
    "column",
    EV_DEFAULT,
    "sfs",
    "templatename width datatype",
    "The column properties."
);
Event EV_RowColumn_AddRowWidgets
(
    "addrowwidgets",
    EV_DEFAULT,
    "sSSSSSSS",
    "s1 s2 s3 s4 s5 s6 s7 s8",
    "Adds specific widgets to the row."
);
Event EV_RowColumn_AddRow
(
    "addrow",
    EV_DEFAULT,
    "sSSSSSSS",
    "s1 s2 s3 s4 s5 s6 s7 s8",
    "The string arguments to set on the row"
);
Event EV_RowColumn_AddRowItem
(
    "addrowitem",
    EV_DEFAULT,
    "sS",
    "item data",
    "Adds an item to a row."
);
Event EV_RowColumn_SetRow
(
    "setrow",
    EV_DEFAULT,
    "isSSSSSSS",
    "rowIndex s1 s2 s3 s4 s5 s6 s7 s8",
    "The string arguments to set on the row"
);
Event EV_RowColumn_SelectRow
(
    "selectrow", EV_DEFAULT, "i", "rowIndex", "Specifies the row to select"
);
Event EV_RowColumn_SelectRowWithData
(
    "selectrowwithdata",
    EV_DEFAULT,
    "is",
    "column data",
    "Selects a row based on the stored data"
);
Event EV_RowColumn_SortColumn
(
    "sortcolumn",
    EV_DEFAULT,
    "i",
    "columnIndex",
    "Sorts the column based on the column index"
);
Event EV_RowColumn_ActivateCommand
(
    "activatecommand",
    EV_DEFAULT,
    "sIIIII",
    "command i1 i2 i3 i4 i5",
    "The command to execute when the row is activated (double clicked). I1-I5 are the column indices"
);
Event EV_RowColumn_SelectedCommand
(
    "selectedcommand",
    EV_DEFAULT,
    "sIIIII",
    "command i1 i2 i3 i4 i5",
    "The command to exectue when the row is selected (single clicked)"
);
Event EV_RowColumn_ActivateColumnCommand
(
    "activatecolumncommand",
    EV_DEFAULT,
    "siIIIII",
    "command column i1 i2 i3 i4 i5",
    "The command to execute when the column is activated."
);
Event EV_RowColumn_SelectedColumnCommand
(
    "selectedcolumncommand",
    EV_DEFAULT,
    "siIIIII",
    "command column i1 i2 i3 i4 i5",
    "The command to execute when the column is selected"
);
Event EV_RowColumn_HoveredColumnCommand
(
    "hoveredcolumncommand",
    EV_DEFAULT,
    "siIIIII",
    "command column i1 i2 i3 i4 i5",
    "The command to execute when the column is hovered."
);
Event EV_RowColumn_Activate
(
    "activate", EV_DEFAULT, NULL, NULL, "Triggers the activate commnand"
);
Event EV_RowColumn_SortType
(
    "sorttype", EV_DEFAULT, "s", "type", "Specifies the sorting method"
);
Event EV_RowColumn_BgRowColor
(
    "bgrowcolor",
    EV_DEFAULT,
    "ffff",
    "red green blue alpha",
    "Specifies the explicit background color to use on the row"
);
Event EV_RowColumn_FgRowColor
(
    "fgrowcolor",
    EV_DEFAULT,
    "ffff",
    "red green blue alpha",
    "Specifies the explicit foreground color to use on the row"
);
Event EV_RowColumn_DeselectAll
(
    "deselectall", EV_DEFAULT, NULL, NULL, "Deselects all items in the list"
);
Event EV_RowColumn_NoFocus
(
    "nofocus", EV_DEFAULT, NULL, NULL, "Prevents the wigdet from gaining focus."
);
Event EV_RowColumn_NoRowSelect
(
    "norowselect", EV_DEFAULT, NULL, NULL, "Prevents the row from selecting"
);
Event EV_RowColumn_NoRowHighlight
(
    "norowhighlight", EV_DEFAULT, NULL, NULL, "Sets the no row highlight"
);
Event EV_RowColumn_DeleteAllItems
(
    "deleteallitems", EV_DEFAULT, NULL, NULL, "Delete all the items from the widget"
);

Event EV_RowColumn_TapActivate
(
    "tapactivate", EV_DEFAULT, NULL, NULL,
    "Makes one row press the list's own activatecommand"
);

Event EV_RowColumn_Autofit
(
    "autofit", EV_DEFAULT, "iif",
    "srcColumn flexColumn maxWidth",
    "Widens srcColumn to its longest cell and takes the width back from flexColumn"
);

Event EV_RowColumn_FitColumns
(
    "columnfit", EV_DEFAULT, NULL, NULL,
    "Runs the autofit pass over the rows the list carries now"
);

CLASS_DECLARATION(UIScrollable, UIRowColumn, NULL) {
    {&W_LeftMouseDown,               &UIRowColumn::Pressed                   },
    {&W_MouseEntered,                &UIRowColumn::MouseEntered              },
    {&W_MouseExited,                 &UIRowColumn::MouseExited               },
    {&EV_RowColumn_Column,           &UIRowColumn::addColumn                 },
    {&EV_RowColumn_AddRowWidgets,    &UIRowColumn::addRowWidgets             },
    {&EV_RowColumn_AddRow,           &UIRowColumn::addRow                    },
    {&EV_RowColumn_AddRowItem,       &UIRowColumn::addItem                   },
    {&EV_RowColumn_SetRow,           &UIRowColumn::setRow                    },
    {&EV_RowColumn_SelectRow,        &UIRowColumn::selectRowHandler          },
    {&EV_RowColumn_SelectRowWithData, &UIRowColumn::selectRowWithData        },
    {&EV_RowColumn_SortColumn,       &UIRowColumn::sortColumnHandler         },
    {&EV_RowColumn_ActivateCommand,  &UIRowColumn::activateCommand           },
    {&EV_RowColumn_SelectedCommand,  &UIRowColumn::selectedCommand           },
    {&EV_RowColumn_ActivateColumnCommand, &UIRowColumn::activateColumnCommand },
    {&EV_RowColumn_SelectedColumnCommand, &UIRowColumn::selectedColumnCommand },
    {&EV_RowColumn_HoveredColumnCommand,  &UIRowColumn::hoveredColumnCommand  },
    {&EV_RowColumn_Activate,         &UIRowColumn::activate                  },
    {&EV_RowColumn_SortType,         &UIRowColumn::sortType                  },
    {&EV_RowColumn_BgRowColor,       &UIRowColumn::setBackgroundRowColor     },
    {&EV_RowColumn_FgRowColor,       &UIRowColumn::setForegroundRowColor     },
    {&EV_RowColumn_DeselectAll,      &UIRowColumn::deselectAll               },
    {&EV_RowColumn_NoFocus,          &UIRowColumn::noFocus                   },
    {&EV_RowColumn_NoRowSelect,      &UIRowColumn::noRowSelect               },
    {&EV_RowColumn_NoRowHighlight,   &UIRowColumn::noRowHighlight            },
    {&EV_RowColumn_DeleteAllItems,   &UIRowColumn::DeleteItems               },
    {&EV_RowColumn_TapActivate,      &UIRowColumn::tapActivate               },
    {&EV_RowColumn_Autofit,          &UIRowColumn::autofit                   },
    {&EV_RowColumn_FitColumns,       &UIRowColumn::fitColumns                },
    {NULL,                           NULL                                    }
};

CLASS_DECLARATION(UIWidget, UIRow, NULL) {
    {&W_LeftMouseDown, &UIRow::Pressed       },
    {&W_MouseEntered,  &UIRow::MouseEntered  },
    {&W_MouseExited,   &UIRow::MouseExited   },
    {NULL,             NULL                  }
};

UIColumn::UIColumn()
{
    width         = 0.0f;
    baseWidth     = 0.0f;
    headerWidget  = NULL;
    headWidget    = NULL;
    sortDirection = 1;
    dataType      = 0;
}

void UIColumn::setCommand(const str& command, WidgetState state)
{
    commands[state] = command;
}

str &UIColumn::getCommand(WidgetState state)
{
    return commands[state];
}

void UIColumn::setCommandIndex(int index, WidgetState state)
{
    commandIndices[state].AddObject(index);
}

int UIColumn::getCommandIndex(int index, WidgetState state)
{
    if (index >= 1 && index <= commandIndices[state].NumObjects()) {
        return commandIndices[state].ObjectAt(index);
    }

    return 0;
}

int UIColumn::getNumCommandIndices(WidgetState state)
{
    return commandIndices[state].NumObjects();
}

void UIColumn::setDataType(const str& type)
{
    if (!str::icmp(type.c_str(), "alpha")) {
        dataType = 0;
    } else if (!str::icmp(type.c_str(), "int")) {
        dataType = 2;
    } else if (!str::icmp(type.c_str(), "float")) {
        dataType = 3;
    } else if (!str::icmp(type.c_str(), "alternate")) {
        dataType = 1;
    } else {
        dataType = 4;
    }
}

UIRow::UIRow()
{
    m_selected         = false;
    m_noRowHighlight   = false;
    m_noRowSelect      = false;
    m_selectedWidget   = NULL;
    m_hoveredWidget    = NULL;
    m_armedWidget      = NULL;
    m_accumulatedWidth = 0.0f;
}

UIRow::~UIRow()
{
    m_widgets.ClearObjectList();
}

void UIRow::addWidget(UIWidget *widget, float width, float height)
{
    if (!widget) {
        return;
    }

    widget->InitFrame(
        this, m_accumulatedWidth, 0.0f, width, height, -1, "tahoma-8");

    m_accumulatedWidth += width;
    m_widgets.AddObject(widget);
}

UIWidget *UIRow::getWidget(int index)
{
    if (index > 0 && index <= m_widgets.NumObjects()) {
        return m_widgets.ObjectAt(index);
    }

    return NULL;
}

void UIRow::setSelected(bool selected)
{
    m_selected = selected;

    if (selected) {
        setChildState(WIDGET_STATE_SELECTED);
        setSelectedState();
    } else {
        setChildState(WIDGET_STATE_NORMAL);
        setNormalState();
        m_selectedWidget = NULL;
    }
}

// (retail +1036), not the UIRow-owned +1728 list.  A row flagged
void UIRow::setChildState(WidgetState state)
{
    if (m_noRowHighlight && (state == WIDGET_STATE_HOVER || state == WIDGET_STATE_SELECTED)) {
        return;
    }

    for (int i = 1; i <= m_children.NumObjects(); i++) {
        UIWidget *child = m_children.ObjectAt(i);

        switch (state) {
        case WIDGET_STATE_HOVER:
            child->setHoverState();
            break;
        case WIDGET_STATE_SELECTED:
        case WIDGET_STATE_ARMED:
            child->setSelectedState();
            break;
        case WIDGET_STATE_NORMAL:
            child->setNormalState();
            break;
        default:
            break;
        }
    }
}

void UIRow::Draw(void)
{
    UIWidget::Draw();
}

void UIRow::Pressed(Event *ev)
{
    m_selected = true;

    if (m_selectedWidget) {
        m_selectedWidget->setNormalState();
    }

    UIWidget *pressed = (UIWidget *)ev->GetSourceObject();
    if (pressed) {
        m_selectedWidget = pressed;
        m_armedWidget    = pressed;
    }

    UIWidget::Pressed(ev);
    setChildState(WIDGET_STATE_SELECTED);

    if (m_noRowSelect) {
        m_selected = false;
        setWidgetState(WIDGET_STATE_HOVER);
        setChildState(WIDGET_STATE_HOVER);
    }

    if (m_noRowHighlight && m_selectedWidget) {
        m_selectedWidget->setSelectedState();
    }

    ev->SetSourceObject(this);

    if (m_command.length()) {
        uii.Cmd_Stuff((m_command + "\n").c_str());
    }
}

void UIRow::MouseEntered(Event *ev)
{
    UIWidget *entered = (UIWidget *)ev->GetSourceObject();

    if (m_widgetState == WIDGET_STATE_SELECTED) {
        setChildState(WIDGET_STATE_SELECTED);
        m_hoveredWidget = NULL;
    } else {
        setChildState(WIDGET_STATE_HOVER);
        setHoverState();
        m_hoveredWidget = entered;
    }

    ev->SetSourceObject(this);

    UIWidget::MouseEntered(ev);
}

void UIRow::MouseExited(Event *ev)
{
    if (m_selected) {
        if (m_noRowHighlight && m_selectedWidget) {
            m_selectedWidget->setSelectedState();
        } else {
            setChildState(WIDGET_STATE_SELECTED);
        }
    } else {
        setChildState(WIDGET_STATE_NORMAL);
        setNormalState();
        m_hoveredWidget = NULL;
        UIWidget::MouseExited(ev);
    }
}

void UIRow::noRowHighlight(Event *ev)
{
    m_noRowHighlight = true;
}

bool UIRow::doesWidgetFit(UIWidget *widget)
{
    if (!widget) {
        return false;
    }

    UIRect2D mine    = getFrame();
    UIRect2D theirs  = widget->getFrame();

    if (theirs.size.height > mine.size.height) {
        return false;
    }

    float used = 0.0f;
    for (int i = 1; i <= m_children.NumObjects(); i++) {
        used += m_children.ObjectAt(i)->getFrame().size.width;
    }

    UIRect2D candidate = widget->getFrame();
    return candidate.size.width <= mine.size.width - used;
}

int UIRow::getSelectedWidgetIndex(void)
{
    if (!m_selectedWidget) {
        return 0;
    }

    for (int i = 1; i <= m_widgets.NumObjects(); i++) {
        if (m_selectedWidget == m_widgets.ObjectAt(i)) {
            return i;
        }
    }

    return 0;
}

int UIRow::getArmedWidgetIndex(void)
{
    if (!m_armedWidget) {
        return 0;
    }

    for (int i = 1; i <= m_widgets.NumObjects(); i++) {
        if (m_armedWidget == m_widgets.ObjectAt(i)) {
            return i;
        }
    }

    return 0;
}

int UIRow::getHoveredWidgetIndex(void)
{
    if (!m_hoveredWidget) {
        return 0;
    }

    for (int i = 1; i <= m_widgets.NumObjects(); i++) {
        if (m_hoveredWidget == m_widgets.ObjectAt(i)) {
            return i;
        }
    }

    return 0;
}

// UColor(0,0,0,1), m_noFocus (+1852) = 0.  It never calls AllowActivate, so a retail
// AllowActivate(true) here made it take focus retail would not give it.
UIRowColumn::UIRowColumn()
{
    // UIROWS: retail's own ctor (@0x00084840, 252 B) does NOT touch +1728/+1732/+1736/
    m_firstDrawnY                  = 0.0f;
    m_selectedRow                  = NULL;
    m_sortColumn                   = -1;
    m_lastClickX                   = -1.0f;
    m_lastClickY                   = -1.0f;
    m_lastClickTime                = 0;
    m_noRowSelect                  = false;
    m_noRowHighlight               = false;
    m_tapActivate                  = false;
    m_autofitSrc                   = 0;
    m_autofitFlex                  = 0;
    m_autofitMax                   = 0.0f;
    m_sortType                     = 2;
    m_backgroundRowColorRequested  = false;
    m_foregroundRowColorRequested  = false;
    m_backgroundRowColor           = UColor(0.0f, 0.0f, 0.0f, 1.0f);
    m_foregroundRowColor           = UColor(0.0f, 0.0f, 0.0f, 1.0f);
    m_noFocus                      = false;
}

UIRowColumn::~UIRowColumn()
{
    m_rows.ClearObjectList();

    for (int i = m_columns.NumObjects(); i >= 1; i--) {
        delete m_columns.ObjectAt(i);
    }
    m_columns.ClearObjectList();
}
