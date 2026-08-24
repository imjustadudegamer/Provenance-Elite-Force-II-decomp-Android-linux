
#include "ui_local.h"

CLASS_DECLARATION(UIFloatingWindow, UIDialog, NULL) {
    {NULL, NULL}
};

UIDialog::UIDialog()
{
    m_label  = NULL;
    m_ok     = NULL;
    m_cancel = NULL;
}

UIDialog::~UIDialog()
{
    if (m_label) {
        delete m_label;
    }
    if (m_ok) {
        delete m_ok;
    }
    if (m_cancel) {
        delete m_cancel;
    }
}

void UIDialog::FrameInitialized(void)
{
    UIRect2D rect;

    UIFloatingWindow::FrameInitialized();

    rect = getChildSpace()->getClientFrame();

    m_label = new UILabel();
    m_label->InitFrame(getChildSpace(), rect, 0);

    m_cancel = new UIButton();
    m_cancel->setTitle("Cancel");
    m_cancel->setBackgroundAlpha(0);
    m_cancel->InitFrame(getChildSpace(), rect.size.width - 60 - 48, rect.size.height - 24 - 6, 60, 24, 0);
    m_cancel->Connect(this, W_Button_Pressed, UIFloatingWindow::W_ClosePressed);

    m_ok = new UIButton();
    m_ok->setTitle("Ok");
    m_ok->setBackgroundAlpha(0);
    m_ok->InitFrame(getChildSpace(), 48, rect.size.height - 24 - 6, 60, 24, 0);
    m_ok->Connect(this, W_Button_Pressed, UIFloatingWindow::W_ClosePressed);

    m_minimizeButton->ProcessEvent(new Event("hide"));
}

void UIDialog::LinkCvar(str cvarname)
{
    cvar_t *cvar;
    str     formatted;
    char   *ptr;
    char   *nextptr;
    char    dialog[256];

    if (!m_label) {
        return;
    }

    cvar = UI_FindCvar(cvarname);
    if (!cvar) {
        return;
    }

    Q_strncpyz(dialog, cvar->string, sizeof(dialog));
    ptr = dialog;

    for (ptr = dialog; ptr; ptr = nextptr + 1) {
        nextptr = strchr(ptr, '$');
        if (nextptr) {
            *nextptr = 0;
        }

        if (strlen(ptr)) {
            formatted += ptr;
        }

        formatted += '\n';

        if (!nextptr) {
            break;
        }

        *nextptr = '$';
    }

    m_label->SetLabel(formatted);
}

void UIDialog::SetOKCommand(str cvarname)
{
    if (m_ok) {
        m_ok->LinkCommand(cvarname);
    }
}

void UIDialog::SetLabelMaterial(UIReggedMaterial *mat)
{
    if (!m_label) {
        return;
    }

    m_label->setMaterial(mat, WIDGET_STATE_NORMAL);
    m_label->setTitle(str());
}

void UIDialog::SetOkMaterial(UIReggedMaterial *mat)
{
    if (!m_ok) {
        return;
    }

    m_ok->setMaterial(mat, WIDGET_STATE_NORMAL);
    m_ok->setTitle(str());
}

void UIDialog::SetCancelMaterial(UIReggedMaterial *mat)
{
    if (!m_cancel) {
        return;
    }

    m_cancel->setMaterial(mat, WIDGET_STATE_NORMAL);
    m_cancel->setTitle(str());
}
