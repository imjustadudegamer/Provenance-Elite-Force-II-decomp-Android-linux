
// That struct is not game_import_t (whose +0x24C is MissionObjective_GetDescription,

#include "cl_ui.h"
#include "cl_uistd.h"
#include "uilib/uimenu.h"
#include "uilib/uiwinman.h"
#include "../qcommon/qcommon.h"

class BranchOption
{
public:
    str _text;   
    str _thread; 
};

class BranchDialogEntry
{
public:
    str                       _name;     
    str                       _question; 
    Container<BranchOption *> _options;  

    ~BranchDialogEntry();

    void addOption(BranchOption *option);
};

class DialogEntry;  

class DialogTextManager
{
    DialogEntry                   *_dialogEntries; 
    Container<BranchDialogEntry *> _branchEntries; 

public:
    ~DialogTextManager();

    void ClearEntries(void);
    void LoadEntries(const str& name);
    void ParseEntryFile(const char *filename, const char *buffer);
    DialogEntry *GetDialogEntry(const char *name);
    void parseBranchDialogFile(const char *buffer);
    bool parseDialogBranchEntry(BranchDialogEntry *entry, char **buffer);
    void displayBranchDialog(const str& name);
    void setBranchDialogData(BranchDialogEntry *entry);
};

static DialogTextManager dialogTextManager;

BranchDialogEntry::~BranchDialogEntry()
{
    int i;

    for (i = 1; i <= _options.NumObjects(); i++) {
        delete _options.ObjectAt(i);
    }

    _options.ClearObjectList();
}

//  2. The reject branch does NOT free the option.  Retail's else-arm builds a temp
//     (`__dla__FPv' at 0x1A90D8) frees that temp str's spilled buffer -- never
//     `option` (r4, spilled to 0x8C(r1) at 0x1A903C, is not passed to it).
void BranchDialogEntry::addOption(BranchOption *option)
{
    if (!option) {
        return;
    }

    if (_options.NumObjects() <= 3) {
        _options.AddObject(option);
    } else {
        Com_WDPrintf("Too many branch options for %s", _name.c_str());
    }
}

DialogTextManager::~DialogTextManager()
{
    ClearEntries();
}

void DialogTextManager::ClearEntries(void)
{
    int i;

    DialogEntry *entry = _dialogEntries;
    while (entry) {
        DialogEntry *next = entry->_next;
        delete entry;
        entry = next;
    }
    _dialogEntries = NULL;

    for (i = 1; i <= _branchEntries.NumObjects(); i++) {
        delete _branchEntries.ObjectAt(i);
    }

    _branchEntries.ClearObjectList();
}

void DialogTextManager::ParseEntryFile(const char *filename, const char *buffer)
{
    char localized[128];
    char text[4096];
    const char *token;

    token = COM_GetToken((char **)&buffer, qtrue);
    if (strcmp(token, "DIALOG")) {
        Com_Error(ERR_FATAL, "ParseEntryFile: File %s is not a DIALOG file", filename);
    }

    while (buffer && *buffer) {
        unsigned char flags;
        DialogEntry  *entry;
        bool          found;

        token = COM_GetToken((char **)&buffer, qtrue);
        if (!token[0]) {
            break;
        }

        if (token[0] == '/' && token[1] == '/') {
            continue;
        }

        if (isdigit(token[0])) {
            int priority = atoi(token);

            if (priority > 100) {
                const char *nextToken = COM_GetToken((char **)&buffer, qfalse);

                Com_Error(
                    ERR_FATAL, "ParseEntryFile: entry (%s) in (%s) has invalid priority %i\n", nextToken, filename,
                    priority
                );
            }

            flags = (unsigned char)priority;

            token = COM_GetToken((char **)&buffer, qtrue);
            if (!token[0]) {
                Com_DPrintf("Parsed Dialog text file %s\n", filename);
                return;
            }
        } else {
            flags = 50;
        }

        Com_LocalizeFilePath(token, localized);

        entry = GetDialogEntry(localized);
        found = entry != NULL;
        if (!entry) {
            entry = new DialogEntry;
            entry->_flags = 0;
        }

        entry->_name = localized;

        token = COM_GetToken((char **)&buffer, qfalse);
        if (!token[0]) {
            Com_Error(ERR_FATAL, "ParseEntryFile: text entry named (%s) has no text entry", entry->_name.c_str());
        }

        Q_strncpyz(text, token, sizeof(text));
        do {
            Q_strcat(text, sizeof(text), " ");
            token = COM_GetToken((char **)&buffer, qfalse);
            if (strlen(text) + strlen(token) >= sizeof(text)) {
                Com_WDPrintf("Dialog text too long for entry %s\n", entry->_name.c_str());
                return;
            }
            Q_strcat(text, sizeof(text), token);
        } while (token[0]);

        if (text[strlen(text) - 1] == ' ') {
            text[strlen(text) - 1] = '\0';
        }

        entry->_flags = flags;
        entry->_text  = text;

        if (!found) {
            entry->_next = _dialogEntries;
            _dialogEntries = entry;
        }
    }

    Com_DPrintf("Parsed Dialog text file %s\n", filename);
}

DialogEntry *DialogTextManager::GetDialogEntry(const char *name)
{
    DialogEntry *entry = _dialogEntries;

    while (entry) {
        if (!Q_stricmp(entry->_name.c_str(), name)) {
            return entry;
        }
        entry = entry->_next;
    }

    return NULL;
}

static float get_lip_length(const char *soundName)
{
    return LIP_GetLength(soundName);
}

DialogEntry *GetDialogEntry(const char *name)
{
    return dialogTextManager.GetDialogEntry(name);
}

bool Dialog_PrintDialog(const char *soundName, const char *consoleName)
{
    DialogEntry *entry;
    float        lipLength;
    const char  *text;
    Event       *ev;

    if (!soundName) {
        return false;
    }

    entry = dialogTextManager.GetDialogEntry(soundName);


    if (!entry) {
        return false;
    }

    if (strlen(soundName) > 4) {
        lipLength = get_lip_length(soundName);
    } else {
        lipLength = 0.0f;
    }

    if (entry->_text.length() && lipLength > 0.0f) {
        text = entry->_text.c_str();
    } else {
        text  = "";
        lipLength = 0.0f;
    }

    ev = new Event(EV_Widget_PrintDialog);
    ev->AddString(soundName);
    ev->AddString(text);
    ev->AddFloat(lipLength);

    uWinMan.PassEventToWidget(consoleName, ev, qtrue);
    return true;
}

// ONE DELIBERATE DEVIATION, and it is the only one in this file: retail's loop
bool DialogTextManager::parseDialogBranchEntry(BranchDialogEntry *entry, char **buffer)
{
    const char *token;

    for (token = COM_GetToken(buffer, qtrue); *buffer && *token != '}'; token = COM_GetToken(buffer, qtrue)) {
        if (!Q_stricmp(token, "question")) {
            token = COM_GetToken(buffer, qtrue);

            if (!token) {
                return false;
            }

            entry->_question = token;
        } else if (!Q_stricmp(token, "option")) {
            BranchOption *option = new BranchOption;

            // COM_GetToken(a3, 0) in the decompiled body): the option text must sit
            token = COM_GetToken(buffer, qfalse);

            if (!token) {
                delete option;
                return false;
            }

            option->_text = token;

            token = COM_GetToken(buffer, qtrue);

            if (!token) {
                delete option;
                return false;
            }

            option->_thread = token;

            entry->addOption(option);
        } else {
        // channel (@0x000C8260, print level 1), not Com_DPrintf.
        Com_WDPrintf("parseBranchDialogFile: Unknown keyword: %s aborting parsing\n", token);
            return false;
        }
    }

    return true;
}

void DialogTextManager::parseBranchDialogFile(const char *buffer)
{
    char       *data = (char *)buffer;
    const char *token;

    token = COM_GetToken(&data, qtrue);

    if (Q_stricmp(token, "BRANCHDIALOG")) {
        return;
    }

    while (data && *data) {
        token = COM_GetToken(&data, qtrue);

        // strcmp, not stricmp -- 0x001A75E4.  Case matters for this one keyword.
        if (!strcmp(token, "branchdialog")) {
            BranchDialogEntry *entry = new BranchDialogEntry;

            entry->_name = COM_GetToken(&data, qtrue);

            token = COM_GetToken(&data, qtrue);

            if (strcmp(token, "{")) {
                Com_WDPrintf("parseBranchDialogFile: Invalid branch dialog format for %s", entry->_name.c_str());
            }

            if (!parseDialogBranchEntry(entry, &data)) {
                Com_WDPrintf(
                    "parseBranchDialogFile: Unable to process branch dialog format for %s", entry->_name.c_str()
                );
                delete entry;
                // Retail abandons the whole file here, not just this entry.
                return;
            }

            _branchEntries.AddObject(entry);
        }
    }
}

void DialogTextManager::LoadEntries(const str& name)
{
    cvar_t *lang;
    str     path;
    size_t  i;
    int     j;

    lang = Cvar_Get("local_language", "Eng", CVAR_ARCHIVE);

    path = "loc/";

    if (lang) {
        if (strlen(lang->string)) {
            path += lang->string;
        } else {
            path += lang->resetString;
        }
    }

    path += "/dialog/";

    if (Q_stricmpn(name.c_str(), "maps/", 4)) {
        path += name;
    } else {
        path += str(name, 5, name.length());
    }

    for (i = 0; i < path.length(); i++) {
        if (path[i] == '$' || path[i] == '#') {
            path.CapLength(i);
            break;
        }
    }

    if (path.length() > 4 && !Q_stricmp(path.c_str() + path.length() - 4, ".bsp")) {
        path.CapLength(path.length() - 4);
    }

    for (j = 0; j < 10; j++) {
        str   filename;
        char *buffer = NULL;
        long  len;

        filename = j ? (path + j) : path;

        filename += ".dlg";
        len = FS_ReadFile(filename.c_str(), (void **)&buffer);
        if (len != -1 && buffer) {
            ParseEntryFile(filename.c_str(), buffer);
            FS_FreeFile(buffer);
        }

        filename = j ? (path + j) : path;
        filename += ".blg";
        len = FS_ReadFile(filename.c_str(), (void **)&buffer);
        if (len != -1 && buffer) {
            parseBranchDialogFile(buffer);
            FS_FreeFile(buffer);
        }
    }
}

void DialogTextManager::displayBranchDialog(const str& name)
{
    int i;

    for (i = 1; i <= _branchEntries.NumObjects(); i++) {
        BranchDialogEntry *entry = _branchEntries.ObjectAt(i);

        if (!Q_stricmp(entry->_name.c_str(), name.c_str())) {
            setBranchDialogData(entry);
            return;
        }
    }

    // Retail says nothing when the name does not resolve.
}

// Note what retail does NOT do: it never clears the title of a disabled option.
void DialogTextManager::setBranchDialogData(BranchDialogEntry *entry)
{
    str cmdString;
    int i;

    cmdString = "globalwidgetcommand BranchQuestion title \"";
    cmdString += entry->_question;
    cmdString += "\"\n";
    Cbuf_ExecuteText(EXEC_NOW, cmdString.c_str());

    for (i = 1; i <= 3; i++) {
        if (i <= entry->_options.NumObjects()) {
            BranchOption *option = entry->_options.ObjectAt(i);

            cmdString = "globalwidgetcommand BranchOption";
            cmdString += i;
            cmdString += " title \"";
            cmdString += option->_text;
            cmdString += "\"\n";
            Cbuf_ExecuteText(EXEC_NOW, cmdString.c_str());

            cmdString = "globalwidgetcommand BranchOption";
            cmdString += i;
            cmdString += " DialogThreadToRun \"";
            cmdString += option->_thread;
            cmdString += "\"\n";
            Cbuf_ExecuteText(EXEC_NOW, cmdString.c_str());

            cmdString = "globalwidgetcommand BranchOption";
            cmdString += i;
            cmdString += " enable\n";
            Cbuf_ExecuteText(EXEC_NOW, cmdString.c_str());
        } else {
            cmdString = "globalwidgetcommand BranchOption";
            cmdString += i;
            cmdString += " disable\n";
            Cbuf_ExecuteText(EXEC_NOW, cmdString.c_str());
        }
    }
}

void Dialog_LoadDialog(const char *mapname)
{
    dialogTextManager.ClearEntries();
    dialogTextManager.LoadEntries("standard");
    dialogTextManager.LoadEntries(mapname ? mapname : "");
}

void UI_DisplayBranchDialog_f(void)
{
    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: displaybranchdialog <branchname>\n");
        return;
    }

    dialogTextManager.displayBranchDialog(Cmd_Argv(1));
}
