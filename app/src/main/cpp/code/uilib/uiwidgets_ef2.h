

#pragma once

#include "ui_local.h"

class UIColumn
{
public:
    float          width;
    float          baseWidth;
    UIWidget      *headerWidget;
    int            sortDirection;
    int            dataType;
    str            commands[WIDGET_STATE_COUNT];
    Container<int> commandIndices[WIDGET_STATE_COUNT];

public:
    UIColumn();

    void setCommand(const str& command, WidgetState state);
    str &getCommand(WidgetState state);
    void setCommandIndex(int index, WidgetState state);
    int  getCommandIndex(int index, WidgetState state);
    int  getNumCommandIndices(WidgetState state);
    void setDataType(const str& type);
};

class UIRow : public UIWidget
{
protected:
    Container<UIWidget *> m_widgets;
    bool                  m_selected;
    bool                  m_noRowHighlight;
    bool                  m_noRowSelect;
    UIWidget             *m_selectedWidget;
    UIWidget             *m_hoveredWidget;
    UIWidget             *m_armedWidget;
    float                 m_accumulatedWidth;

public:
    CLASS_PROTOTYPE(UIRow);

    UIRow();
    ~UIRow();

    void      addWidget(UIWidget *widget, float width, float height);
    UIWidget *getWidget(int index);
    void      setSelected(bool selected);
    void      setChildState(WidgetState state);
    void      Draw(void) override;
    void      Pressed(Event *ev) override;
    void      MouseEntered(Event *ev) override;
    void      MouseExited(Event *ev) override;
    void      noRowHighlight(Event *ev);
    bool      doesWidgetFit(UIWidget *widget);
    int       getSelectedWidgetIndex(void);
    int       getArmedWidgetIndex(void);
    int       getHoveredWidgetIndex(void);

    void setNoRowHighlight(bool flag) { m_noRowHighlight = flag; }
    void setNoRowSelect(bool flag) { m_noRowSelect = flag; }

    float getAccumulatedWidth(void) const { return m_accumulatedWidth; }
    int   getNumCells(void) const { return m_widgets.NumObjects(); }
};

class UIScrollable : public UIWidget
{
protected:
    UIVertScroll *m_scrollBar;
    int           m_topItem;                
    int           m_bottomItem;             
    float         m_scrollItemHeight;       
    float         m_scrollItemHeightBase;   
    float         m_scrollHeaderHeight;     

public:
    CLASS_PROTOTYPE(UIScrollable);

    UIScrollable();
    ~UIScrollable();

    void ChangeResolution(void) override;
    void Draw(void) override;

    void  setItemHeight(Event *ev);
    void  scrollBarTemplate(Event *ev);
    void  setScrollWidth(Event *ev);
    void  onSizeChanged(void);
    void  setNumItems(int numItems);
    float getScrollBarWidth(void) const;

    UIVertScroll *getScrollBar(void) const { return m_scrollBar; }
    float         getItemHeight(void) const { return m_scrollItemHeight; }
    int           getTopItem(void) const { return m_topItem; }
    int           getBottomItem(void) const { return m_bottomItem; }
};

class UIRowColumn : public UIScrollable
{
protected:
    float m_firstDrawnY;
    Container<UIColumn *> m_columns;
    Container<UIRow *>    m_rows;
    UIRow *m_selectedRow;
    int    m_sortColumn;
    float  m_lastClickX;
    float  m_lastClickY;
    int    m_lastClickTime;
    bool   m_noRowHighlight;
    bool   m_noRowSelect;
    int    m_sortType;
    bool   m_backgroundRowColorRequested;
    bool   m_foregroundRowColorRequested;
    UColor m_backgroundRowColor;
    UColor m_foregroundRowColor;
    bool   m_noFocus;
    str            m_commands[WIDGET_STATE_COUNT];
    Container<int> m_commandArgs[WIDGET_STATE_COUNT];

public:
    CLASS_PROTOTYPE(UIRowColumn);

    UIRowColumn();
    ~UIRowColumn();

    void addColumn(Event *ev);
    void addColumn(const str& templateName, int width, const str& dataType);
    void addRowWidgets(Event *ev);
    void addRow(Event *ev);
    void addItem(Event *ev);
    void setRow(Event *ev);
    void selectRow(Event *ev);
    void selectRowWithData(Event *ev);
    UIRow *createRow(void);
    UIWidget *createColumnWidget(int columnIndex, const str& cellType, const str& name);
    void setSelectedRow(UIRow *row);
    void MouseEntered(Event *ev) override;
    void noFocus(Event *ev);
    void MouseExited(Event *ev) override;
    void Pressed(Event *ev) override;
    void deselectAll(Event *ev);
    void Draw(void) override;
    int  getNumItems(void) override;
    void sortByColumn(Event *ev);
    void sortByColumn(int columnIndex);
    void selectRowHandler(Event *ev);
    void sortColumnHandler(Event *ev);
    void hoveredColumnCommand(Event *ev);
    void MouseHoverExit(Event *ev);
    void sortRows(int (*compare)(const str&, const str&), int columnIndex);
    void ChangeResolution(void) override;
    void DeleteItems(Event *ev);
    void DeleteItems(void);
    void activateCommand(Event *ev);
    void selectedCommand(Event *ev);
    void hoverCommand(Event *ev);
    void activateColumnCommand(Event *ev);
    void selectedColumnCommand(Event *ev);
    void hoverColumnCommand(Event *ev);
    void activate(Event *ev);
    void noRowHighlight(Event *ev);
    void noRowSelect(Event *ev);
    void sortType(Event *ev);
    void setBackgroundRowColor(Event *ev);
    void setForegroundRowColor(Event *ev);
    void executeCommand(int state, UIRow *row);
    void executeColumnCommand(WidgetState state, UIRow *row);

    static int alphaCompare(const str& s1, const str& s2);
    static int integerCompare(const str& s1, const str& s2);
    static int floatCompare(const str& s1, const str& s2);

    void setTopItem(int top);
    int  getTopItem(void) const { return m_topItem; }
    float getRowHeight(void) const { return getItemHeight(); }
    void ScrollTo(int row);
    int  VisibleRowCount(void) const;
    void ClampScroll(void);
    void SyncScrollBar(void);
    qboolean KeyEvent(int key, unsigned int time) override;

    int               getNumColumns(void) const { return m_columns.NumObjects(); }
    const UIColumn   *getColumn(int index) const { return m_columns.ObjectAt(index); }
};

class UITable : public UIWidget
{
protected:
    Container<UIWidget *> m_headerWidgets;
    UIRowColumn *m_pRowColumn;
    int  m_headerHeight;
    int  m_headerHeightBase;
    float m_headerWidth;
    bool m_allowUserSort;

public:
    CLASS_PROTOTYPE(UITable);

    UITable();
    ~UITable();

    void addTableColumn(Event *ev);
    void setHeaderHeight(Event *ev);
    void allowUserSort(Event *ev);

    void addRow(Event *ev);
    void selectedCommand(Event *ev);
    void activateCommand(Event *ev);
    void sortType(Event *ev);
    void selectRowHandler(Event *ev);
    void deselectAll(Event *ev);
    void deleteItems(Event *ev);
    void noRowSelect(Event *ev);
    void noRowHighlight(Event *ev);
    void setScrollbarTemplate(Event *ev);
    void setScrollbarWidth(Event *ev);
    void setItemHeight(Event *ev);
    void sortColumn(Event *ev);
    void hoverCommand(Event *ev);
    void hoverExitCommand(Event *ev);
    void activate(Event *ev);
    void Pressed(Event *ev) override;
    void Draw(void) override;
    void Realign(void) override;
    void ChangeResolution(void) override;

private:
    void LayoutRowColumn(void);
};
class UIRadioButton : public UIWidget
{
protected:
    UIReggedMaterial *m_buttonShader;  
    UIReggedMaterial *m_selectShader;  
    Container<str>    m_stuffCommands; 
    Container<str>    m_returnStrings; 
    UISize2D          m_buttonSize;    
    UISize2D          m_padding;       
    float             m_rowHeight;     
    int               m_numButtons;    
    int               m_selected;      
    str               m_cvar;          

public:
    CLASS_PROTOTYPE(UIRadioButton);

    UIRadioButton();
    ~UIRadioButton();

    void AddRadioButton(Event *ev);
    void SetButtonShader(Event *ev);
    void SetSelectShader(Event *ev);
    void BindCvar(Event *ev);
    void SetRowHeight(Event *ev);
    void SetButtonSize(Event *ev);
    void SetPadding(Event *ev);

    void ResetFrames(void);
    void PressButton(int num);
    void Pressed(Event *ev) override; // retail never binds this to an event
    void Draw(void) override;
};

extern Event EV_RowColumn_AddRow;
extern Event EV_RowColumn_SelectedCommand;
extern Event EV_RowColumn_ActivateCommand;
extern Event EV_RowColumn_SortType;
extern Event EV_RowColumn_SelectRow;
extern Event EV_RowColumn_DeselectAll;
extern Event EV_RowColumn_DeleteAllItems;
extern Event EV_RowColumn_NoRowSelect;
extern Event EV_RowColumn_NoRowHighlight;
extern Event EV_RowColumn_SortColumn;
extern Event EV_RowColumn_Activate;
extern Event EV_Scrollable_ItemHeight;
extern Event EV_Scrollable_ScrollbarTemplate;
extern Event EV_Scrollable_ScrollbarWidth;

extern Event EV_Table_Column;
extern Event EV_Table_HeaderHeight;
extern Event EV_Table_AllowUserSort;
