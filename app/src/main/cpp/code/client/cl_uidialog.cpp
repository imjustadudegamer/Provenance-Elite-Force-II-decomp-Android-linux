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


#include "cl_ui.h"
#include "uilib/uimenu.h"
#include "uilib/uiwinman.h"

static const char *DIALOG_HEAD_CONSOLE_NAME = "DialogHeadConsole";
static const char *DIALOG_CONSOLE1_NAME     = "DialogConsole";
static const char *DIALOG_CONSOLE2_NAME     = "DialogConsole2";

void Dialog_ResetConsole(const char *consoleName)
{
    Event *ev = new Event(EV_Widget_ResetDialog);

    if (consoleName) {
        ev->AddString(consoleName);
    }

    uWinMan.PassEventToWidget(consoleName, ev, qtrue);
}

static void Dialog_DisplayEntity(int entnum)
{
    Event *ev = new Event(EV_Widget_DialogHeadEntity);

    ev->AddInteger(entnum);

    uWinMan.PassEventToWidget(DIALOG_HEAD_CONSOLE_NAME, ev, qtrue);
}

void UI_DisplayDialogConsole(const char *dialogName, const char *dialogTextSoundName, int entnum)
{
    static Menu *headMenu = menuManager.FindMenu("headhud");
    static Menu *headText = menuManager.FindMenu("headtexthud");
    static Menu *textMenu = menuManager.FindMenu("texthud");
    cvar_t      *mode     = Cvar_Get("ui_dialogdisplaymode", "1", 0);

    if (!headMenu || !textMenu || !headText) {
        return;
    }

    int displayMode = mode ? mode->integer : 1;

    if (displayMode == 1) {
        if (!strlen(dialogName)) {
            if (headText->isVisible()) {
                headText->ForceHide();
            }
            if (headMenu->isVisible()) {
                headMenu->ForceHide();
            }
            Dialog_ResetConsole(DIALOG_CONSOLE1_NAME);
        }

        if (entnum == 1023 || entnum == 1022 || !entnum) {
            if (headText->isVisible()) {
                headText->ForceHide();
            }
            if (headMenu->isVisible()) {
                headMenu->ForceHide();
            }
        } else {
            Dialog_PrintDialog(dialogName, DIALOG_CONSOLE1_NAME);
            Dialog_DisplayEntity(entnum);
            if (!headText->isVisible()) {
                headText->ForceShow();
            }
            if (!headMenu->isVisible()) {
                headMenu->ForceShow();
            }
        }
    } else if (displayMode == 2) {
        if (!strlen(dialogName)) {
            if (headText->isVisible()) {
                headText->ForceHide();
            }
            if (headMenu->isVisible()) {
                headMenu->ForceHide();
            }
            Dialog_ResetConsole(DIALOG_CONSOLE1_NAME);
        }

        if (strlen(dialogName)) {
            if (entnum != 1023 && entnum != 1022) {
                Dialog_DisplayEntity(entnum);
                if (Dialog_PrintDialog(dialogName, DIALOG_CONSOLE1_NAME)) {
                    if (!headText->isVisible()) {
                        headText->ForceShow();
                    }
                    if (!headMenu->isVisible()) {
                        headMenu->ForceShow();
                    }
                }
            }
        }

        if (strlen(dialogTextSoundName) && Dialog_PrintDialog(dialogTextSoundName, DIALOG_CONSOLE2_NAME)) {
            if (!textMenu->isVisible()) {
                textMenu->ForceShow();
            }
        }
    } else {
        if (headText->isVisible()) {
            headText->ForceHide();
        }
        if (headMenu->isVisible()) {
            headMenu->ForceHide();
        }
        if (textMenu->isVisible()) {
            textMenu->ForceHide();
        }
    }
}
