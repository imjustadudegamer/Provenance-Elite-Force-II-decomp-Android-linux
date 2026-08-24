
#pragma once

class BindCommand
{
public:
    str name;
    str command;
    str animation;

    BindCommand();
    BindCommand(const str& inName, const str& inCommand, const str& inAnimation);
};

inline BindCommand::BindCommand() {}

inline BindCommand::BindCommand(const str& inName, const str& inCommand, const str& inAnimation)
{
    name      = inName;
    command   = inCommand;
    animation = inAnimation;
}

class bind_t
{
public:
    Container<char *>                      bindGroupNames;
    Container<Container<BindCommand *> *>  bindGroups;
    int                                    currentBindGroup;

public:
    bind_t();
    ~bind_t();

    void Clear(void);
};

class BindListener : public Listener
{
protected:
    bind_t *bind;

public:
    CLASS_PROTOTYPE(BindListener);

public:
    BindListener();
    BindListener(bind_t *b);

    bool Load(Script& script);
    void NewItem(Event *ev);
    void BindGroup(Event *ev);
};


class UIBindButton : public UIButton
{
    str               m_bindcommand;
    int               m_bindindex;
    qboolean          m_getkey;
    qboolean          m_alternate;
    str               m_entersound;
    str               m_activesound;
    str               m_last_keyname;
    UIReggedMaterial *m_mat;

public:
    CLASS_PROTOTYPE(UIBindButton);

public:
    UIBindButton();
    UIBindButton(str entersound, str activesound);

    void     Pressed(Event *ev) override;
    void     SetCommand(Event *ev);
    void     SetCommand(str s);
    void     setData(const str& data) override;
    void     MouseEntered(Event *ev) override;
    void     copyProperties(UIWidget& other) override;
    void     Draw(void) override;
    void     DrawPressKey(void);
    void     clearBindCommand(void);  
    qboolean KeyEvent(int key, unsigned int time) override;
    void     SetAlternateKey(Event *ev);  
};
