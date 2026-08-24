
#pragma once

class UIField : public UIWidget
{
    EditField m_edit;
    int m_maxCharacters;

public:
    CLASS_PROTOTYPE(UIField);

private:
    void     Draw(void) override;
    qboolean KeyEvent(int key, unsigned int time) override;
    void     CharEvent(int ch) override;

public:
    UIField();

    void UpdateData(void) override;
    void activate(Event *ev);
    void deactivate(Event *ev);
    void setMaxCharacters(Event *ev);
};

extern Event EV_UIField_MaxCharacters;
