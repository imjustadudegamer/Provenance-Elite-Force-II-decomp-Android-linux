
#pragma once

class Script;

class UILayout : public Listener
{
    Script            *m_script;
    Stack<Script *>    m_scriptstack;
    UIWidgetContainer *m_currentcontainer;
    UIWidget          *m_currentwidget;

public:
    CLASS_PROTOTYPE(UILayout);

private:
    void CreateWidgetContainer(Event *ev);
    void ProcessCommands(void);

public:
    UILayout(const char *filename = NULL);

    void Load(const char *filename);
};
