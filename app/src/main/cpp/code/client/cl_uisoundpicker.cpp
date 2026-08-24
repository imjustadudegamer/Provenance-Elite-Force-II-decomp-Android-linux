
#include "cl_ui.h"

CLASS_DECLARATION(FilePickerClass, SoundPickerClass, NULL) {
    {NULL, NULL}
};

SoundPickerClass::SoundPickerClass()
{
    str         soundpath;
    str         currentpath;
    const char *varValue;
    int         i;

    soundpath = "sound";

    varValue = UI_GetCvarString("ui_pickedsound", NULL);
    if (varValue && *varValue) {
        currentpath = varValue;

        for (i = currentpath.length(); i > 0; i--) {
            if (currentpath[i] == '/') {
                break;
            }
        }

        currentpath = str(currentpath, 0, i + 1);
    } else {
        currentpath = soundpath;
    }
}

void SoundPickerClass::FileSelected(const str& inCurrentDirectory, const str& partialName, const str& fullname)
{
    uii.Snd_PlaySound(fullname, CHAN_MENU);  
}

void SoundPickerClass::FileChosen(const str& inCurrentDirectory, const str& partialName, const str& fullname)
{
    uii.Cvar_Set("ui_pickedsound", fullname);
    CloseWindow();
}
