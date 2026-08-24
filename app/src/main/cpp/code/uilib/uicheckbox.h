
#pragma once

class UICheckBox : public UIWidget
{
    str               m_checked_command;
    str               m_unchecked_command;
    UIReggedMaterial *m_checked_material;
    UIReggedMaterial *m_unchecked_material;
    bool              m_checked;
    bool              m_depressed;
    float             m_check_width;
    float             m_check_height;
    int               m_cvarbit; 
    UIRect2D          m_textRect;

public:
    CLASS_PROTOTYPE(UICheckBox);

private:
    void calculateTextBox(void);
    void Draw(void) override;
    void CharEvent(int ch) override;
    void Pressed(Event *ev) override;
    void Released(Event *ev);
    void UpdateCvar(void);
    void MouseEntered(Event *ev) override;
    void MouseExited(Event *ev) override;
    void SetCheckedCommand(Event *ev);
    void SetUncheckedCommand(Event *ev);
    void SetCheckedShader(Event *ev);
    void SetUncheckedShader(Event *ev);
    void LinkCvarBit(Event *ev);

public:
    UICheckBox(void);

    void UpdateData(void) override;
    void ChangeResolution(void) override;
    bool isChecked(void);
};

extern Event EV_UICheckBox_SetCheckedCommand;
extern Event EV_UICheckBox_SetUncheckedCommand;
extern Event EV_UICheckBox_SetCheckedShader;
extern Event EV_UICheckBox_SetUncheckedShader;
extern Event EV_UICheckBox_LinkCvarBit;
