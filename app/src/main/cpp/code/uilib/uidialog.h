
#pragma once

class UIDialog : public UIFloatingWindow
{
public:
    UILabel  *m_label;
    UIButton *m_ok;
    UIButton *m_cancel;

public:
    CLASS_PROTOTYPE(UIDialog);

    UIDialog();
    ~UIDialog();

    void FrameInitialized(void) override;
    void LinkCvar(str cvarname) override;
    void SetOKCommand(str command);
    void SetLabelMaterial(UIReggedMaterial *mat);
    void SetOkMaterial(UIReggedMaterial *mat);
    void SetCancelMaterial(UIReggedMaterial *mat);
};
