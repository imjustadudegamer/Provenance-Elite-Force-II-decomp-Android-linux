
#pragma once

typedef void (*consoleHandler_t)(const char *text);

class item
{
public:
    str           string;
    int           lines;
    int           begins[10];
    int           breaks[10];
    const UColor *pColor;

    item();
};

inline item::item()
{
    lines = 0;

    for (int i = 0; i < 10; i++) {
        begins[i] = 0;
        breaks[i] = 0;
    }

    pColor = NULL;
}

// The line-history fallback.  Retail does NOT use a compile-time array: its
#define MAX_CONSOLE_ITEMS 300

class UIConsole : public UIWidget
{
protected:
    UList<str>       m_history;
    void            *m_historyposition;
    int              m_buffersize;    
    item            *m_items;         
    str              m_currentline;
    UIVertScroll    *m_scroll;
    int              m_firstitem;
    int              m_numitems;
    size_t           m_caret;
    str              m_completionbuffer;
    bool             m_refreshcompletionbuffer;
    int              m_cntcmdnumber;
    int              m_cntcvarnumber;
    consoleHandler_t m_consolehandler;
    bool             m_notInteractive; 

public:
    CLASS_PROTOTYPE(UIConsole);

    UIConsole();
    // NOT delete m_scroll (+1812) -- retail leaks the UIVertScroll -- so neither
    ~UIConsole();

protected:
    int  getFirstItem(void);
    int  getNextItem(int prev);
    int  getLastItem(void);
    // (0x23F2D0+0xCC -> AddLine__13UIChatConsoleFv @0x0005B760).  It must be,
    virtual int AddLine(void);
    void DrawBottomLine(void);
    void AddHistory(void);

    virtual void Print(Event *ev);
    virtual void KeyEnter(void);
    virtual void KeyEscape(void);
    const char *TryToCompleteMapname(const str& partial);

public:
    virtual str  getPrompt(void);
    virtual bool shouldDrawBottomLine(void);

    void     setConsoleHandler(consoleHandler_t handler);
    bool     getItemData(int index, str& string, int& color);
    int      getItemCount(void) const;
    void     AddText(const char *text, const UColor *pColor);
    void     CalcLineBreaks(item& theItem);
    void     Clear(void);
    void     Clear(Event *ev);
    void     FrameInitialized(void) override;
    void     Draw(void) override;
    void     CharEvent(int ch) override;
    qboolean KeyEvent(int key, unsigned int time) override;
    void     OnSizeChanged(Event *ev);
    void     NotInteractive(Event *ev);   
    void     onlyDrawText(Event *ev);     
};

class UIChatConsole : public UIConsole
{
protected:
    int   m_messageMode;   
    int   m_activeTime;    
    float m_activeAlpha;   

public:
    CLASS_PROTOTYPE(UIChatConsole);

    UIChatConsole();

    void setShow(bool show) override;
    void KeyEnter(void) override;
    void KeyEscape(void) override;
    void CharEvent(int ch) override;
    void Draw(void) override;
    void Print(Event *ev) override;
    void FrameInitialized(void) override;

    str  getPrompt(void) override;
    bool shouldDrawBottomLine(void) override;
    int  AddLine(void) override;
    void messageMode(Event *ev);
    void removeLastLine(Event *ev);
};

typedef struct {
    str *text;
    int  color;
} mini_console_text_t;

class Miniconsole : public UIWidget
{
public:
    typedef enum {
        box_moving_out,
        box_moving_in,
        box_out,
        box_in
    } boxstate_t;

protected:
    Container<mini_console_text_t> m_text;         
    int                            m_maxlines;     
    bool                           m_reallyshown;  
    UISize2D                       m_size;         
    boxstate_t                     m_boxstate;     
    int                            m_boxtime;      
    int                            m_movespeed;    

public:
    CLASS_PROTOTYPE(Miniconsole);

protected:
    void VerifyBoxOut(void);
    void ChangeBoxState(boxstate_t state);
    void HandleBoxMoving(void);
    void PostMoveinEvent(void);
    void setShowState(void);

public:
    Miniconsole();
    ~Miniconsole();

    void OnSizeChanged(Event *ev);
    void Print(const char *text, int color);
    void Create(const UISize2D& size, const UColor& fore, const UColor& back, float alpha);
    void MoveInEvent(Event *ev);
    void UpdateFrame(void);
    void Draw(void) override;
    void setRealShow(bool b);
};

class UIFloatingConsole : public UIFloatingWindow
{
protected:
    UIStatusBar       *m_status;
    SafePtr<UIConsole> m_console;
    consoleHandler_t   m_handler;
    UColor             m_consoleColor;
    UColor             m_consoleBackground;
    float              m_consoleAlpha;

public:
    CLASS_PROTOTYPE(UIFloatingConsole);

    UIFloatingConsole();
    ~UIFloatingConsole();

    void FrameInitialized(void) override;
    void ChangeResolution(void) override;
    void OnChildSizeChanged(Event *ev);
    void AddText(const char *text, const UColor *pColor);
    void setConsoleHandler(consoleHandler_t handler);
    void Clear(void);
    void OnClosePressed(Event *ev);
    void setConsoleBackground(const UColor& color, float alpha);
    void setConsoleColor(const UColor& color);
    UIConsole *getConsole(void) const { return m_console; }
};
