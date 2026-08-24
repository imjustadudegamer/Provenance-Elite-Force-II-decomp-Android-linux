
#include "cl_ui.h"

CLASS_DECLARATION(FilePickerClass, MapRunnerClass, NULL) {
    {NULL, NULL}
};

void MapRunnerClass::FileChosen(const str& inCurrentDirectory, const str& partialName, const str& fullname)
{
    str newName;

    newName = "spmap " + str(fullname, 5, fullname.length() - 4) + "\n";
    Cbuf_AddText(newName);

    CloseWindow();
}

CLASS_DECLARATION(FilePickerClass, UIPickFileClass, NULL) {
    {NULL, NULL}
};

UIPickFileClass::UIPickFileClass()
{
    retobj   = NULL;
    retevent = NULL;
}

UIPickFileClass::~UIPickFileClass()
{
    if (retevent) {
        delete retevent;
    }
}

void UIPickFileClass::FileChosen(const str& inCurrentDirectory, const str& partialName, const str& fullname)
{
    if (!retobj || !retevent) {
        CloseWindow();
        return;
    }

    retevent->AddString(fullname);
    retobj->ProcessEvent(retevent);

    retevent = NULL;
    retobj   = NULL;

    CloseWindow();
}

void PickFile(const char *name, Listener *obj, Event& event)
{
    UIPickFileClass *picker;
    str              currentpath;
    int              i;

    picker           = new UIPickFileClass();
    picker->retevent = new Event(event);
    picker->retobj   = obj;

    if (name && *name && strchr(name, '/')) {
        currentpath = name;

        for (i = currentpath.length() - 1; i > 0; i--) {
            if (currentpath[i] == '/') {
                break;
            }
        }

        currentpath = str(currentpath, 0, i + 1);
    } else {
        currentpath = "";
    }

    picker->Setup("", currentpath, ".*", true);
}

CLASS_DECLARATION(FilePickerClass, ViewSpawnerClass, NULL) {
    {NULL, NULL}
};

void ViewSpawnerClass::FileChosen(const str& inCurrentDirectory, const str& partialName, const str& fullname)
{
    str newName;

    newName = "viewspawn " + fullname + "\n";
    Cbuf_AddText(newName);

    CloseWindow();
}

