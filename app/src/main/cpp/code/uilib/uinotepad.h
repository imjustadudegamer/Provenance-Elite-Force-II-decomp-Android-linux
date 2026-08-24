
#pragma once

class UINotepad;

class UINotepadEdit : public UIMultiLineEdit
{
protected:
    UINotepad *m_notepad;

public:
    CLASS_PROTOTYPE(UINotepadEdit);

public:
    UINotepadEdit();

    void CharEvent(int ch) override;
    void setNotepad(UINotepad *notepad);
    bool GotoLine(int line);
    bool FindText(const char *text, int offsetFromSel);
    void MousePressed(Event *ev);
};

typedef enum {
    STATE_NONE,
    STATE_GOTO_LINE,
    STATE_FIND_TEXT,
    STATE_SAVE_AS,
    STATE_TIMED_MESSAGE,
    STATE_CONFIRMCLOSE
} state_t;

typedef struct textinput_s {
    str text;
} textinput_t;

typedef struct timedmessage_s {
    int die;
    str text;
} timedmessage_t;

typedef struct ctrlevent_s {
    char   ch;
    Event *ev;
} ctrlevent_t;

class UINotepad : public UIFloatingWindow
{
    friend class UINotepadEdit; 

private:
    Container<Container<uipopup_describe *> *> m_popups;

protected:
    state_t               m_state;
    textinput_s           m_textinput;
    timedmessage_s        m_timedmessage;
    str                   m_lastfind;
    str                   m_filename;
    UINotepadEdit        *m_edit;
    UIStatusBar          *m_status;
    class UIPulldownMenu *m_menu;

public:
    CLASS_PROTOTYPE(UINotepad);

protected:
    void TimeMessage(const char *message, int howlong);

public:
    UINotepad();
    ~UINotepad();

    bool OpenFile(const char *filename);
    bool Create(UIWidget *parent, UIRect2D& rect, const char *filename);
    void ChildSizeChanged(Event *ev);
    void SaveAs(Event *ev);
    void Save(Event *ev);
    void Open(Event *ev);
    void OpenFile(Event *ev);
    void ClosePressed(Event *ev);
    void OnFind(Event *ev);
    void OnGoto(Event *ev);
    void OnCopy(Event *ev);
    void OnPaste(Event *ev);
    void OnCut(Event *ev);
    bool ProcessControlEvents(int ch);
    bool ProcessCharEvent(int ch);
    void Draw(void) override;
    void setFileName(const char *filename);
};

bool UI_LoadNotepadFile(const char *filename);
