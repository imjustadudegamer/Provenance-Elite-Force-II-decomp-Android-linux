#include "client.h"

#define PROBE_GET(name, def, flags) Cvar_Get(name, def, flags)
#define PROBE_PRINT                 Com_Printf
#include "../qcommon/qa_probe.h"
#include "cl_ui.h"
#include "cl_cin.h"
#include "../uilib/ui_public.h"


int nextHistoryLine; 
int historyLine;     

qboolean chat_team;

int chat_playerNum;

qboolean key_overstrikeMode;

int    anykeydown;
qkey_t keys[MAX_KEYS];
qkey_t altkeys[MAX_KEYS];
qkey_t ctrlkeys[MAX_KEYS];

qboolean menubound[MAX_KEYS];

qboolean alt_down;
qboolean ctrl_down;

typedef struct {
    const char *name;
    int         keynum;
} keyname_t;

keyname_t keynames[] = {
    {"TAB",                   K_TAB                  },
    {"ENTER",                 K_ENTER                },
    {"ESCAPE",                K_ESCAPE               },
    {"SPACE",                 K_SPACE                },
    {"BACKSPACE",             K_BACKSPACE            },
    {"UPARROW",               K_UPARROW              },
    {"DOWNARROW",             K_DOWNARROW            },
    {"LEFTARROW",             K_LEFTARROW            },
    {"RIGHTARROW",            K_RIGHTARROW           },

    {"ALT",                   K_ALT                  },
    {"CTRL",                  K_CTRL                 },
    {"SHIFT",                 K_SHIFT                },

    {"COMMAND",               K_COMMAND              },

    {"CAPSLOCK",              K_CAPSLOCK             },

    {"F1",                    K_F1                   },
    {"F2",                    K_F2                   },
    {"F3",                    K_F3                   },
    {"F4",                    K_F4                   },
    {"F5",                    K_F5                   },
    {"F6",                    K_F6                   },
    {"F7",                    K_F7                   },
    {"F8",                    K_F8                   },
    {"F9",                    K_F9                   },
    {"F10",                   K_F10                  },
    {"F11",                   K_F11                  },
    {"F12",                   K_F12                  },
    {"F13",                   K_F13                  },
    {"F14",                   K_F14                  },
    {"F15",                   K_F15                  },

    {"INS",                   K_INS                  },
    {"DEL",                   K_DEL                  },
    {"PGDN",                  K_PGDN                 },
    {"PGUP",                  K_PGUP                 },
    {"HOME",                  K_HOME                 },
    {"END",                   K_END                  },

    {"MOUSE1",                K_MOUSE1               },
    {"MOUSE2",                K_MOUSE2               },
    {"MOUSE3",                K_MOUSE3               },
    {"MOUSE4",                K_MOUSE4               },
    {"MOUSE5",                K_MOUSE5               },

    {"MWHEELUP",              K_MWHEELUP             },
    {"MWHEELDOWN",            K_MWHEELDOWN           },

    {"JOY1",                  K_JOY1                 },
    {"JOY2",                  K_JOY2                 },
    {"JOY3",                  K_JOY3                 },
    {"JOY4",                  K_JOY4                 },
    {"JOY5",                  K_JOY5                 },
    {"JOY6",                  K_JOY6                 },
    {"JOY7",                  K_JOY7                 },
    {"JOY8",                  K_JOY8                 },
    {"JOY9",                  K_JOY9                 },
    {"JOY10",                 K_JOY10                },
    {"JOY11",                 K_JOY11                },
    {"JOY12",                 K_JOY12                },
    {"JOY13",                 K_JOY13                },
    {"JOY14",                 K_JOY14                },
    {"JOY15",                 K_JOY15                },
    {"JOY16",                 K_JOY16                },
    {"JOY17",                 K_JOY17                },
    {"JOY18",                 K_JOY18                },
    {"JOY19",                 K_JOY19                },
    {"JOY20",                 K_JOY20                },
    {"JOY21",                 K_JOY21                },
    {"JOY22",                 K_JOY22                },
    {"JOY23",                 K_JOY23                },
    {"JOY24",                 K_JOY24                },
    {"JOY25",                 K_JOY25                },
    {"JOY26",                 K_JOY26                },
    {"JOY27",                 K_JOY27                },
    {"JOY28",                 K_JOY28                },
    {"JOY29",                 K_JOY29                },
    {"JOY30",                 K_JOY30                },
    {"JOY31",                 K_JOY31                },
    {"JOY32",                 K_JOY32                },

    {"AUX1",                  K_AUX1                 },
    {"AUX2",                  K_AUX2                 },
    {"AUX3",                  K_AUX3                 },
    {"AUX4",                  K_AUX4                 },
    {"AUX5",                  K_AUX5                 },
    {"AUX6",                  K_AUX6                 },
    {"AUX7",                  K_AUX7                 },
    {"AUX8",                  K_AUX8                 },
    {"AUX9",                  K_AUX9                 },
    {"AUX10",                 K_AUX10                },
    {"AUX11",                 K_AUX11                },
    {"AUX12",                 K_AUX12                },
    {"AUX13",                 K_AUX13                },
    {"AUX14",                 K_AUX14                },
    {"AUX15",                 K_AUX15                },
    {"AUX16",                 K_AUX16                },

    {"KP_HOME",               K_KP_HOME              },
    {"KP_UPARROW",            K_KP_UPARROW           },
    {"KP_PGUP",               K_KP_PGUP              },
    {"KP_LEFTARROW",          K_KP_LEFTARROW         },
    {"KP_5",                  K_KP_5                 },
    {"KP_RIGHTARROW",         K_KP_RIGHTARROW        },
    {"KP_END",                K_KP_END               },
    {"KP_DOWNARROW",          K_KP_DOWNARROW         },
    {"KP_PGDN",               K_KP_PGDN              },
    {"KP_ENTER",              K_KP_ENTER             },
    {"KP_INS",                K_KP_INS               },
    {"KP_DEL",                K_KP_DEL               },
    {"KP_SLASH",              K_KP_SLASH             },
    {"KP_MINUS",              K_KP_MINUS             },
    {"KP_PLUS",               K_KP_PLUS              },
    {"KP_NUMLOCK",            K_KP_NUMLOCK           },
    {"KP_STAR",               K_KP_STAR              },
    {"KP_EQUALS",             K_KP_EQUALS            },

    {"PAUSE",                 K_PAUSE                },

    {"SEMICOLON",             ';'                    }, 

    {"WORLD_0",               K_WORLD_0              },
    {"WORLD_1",               K_WORLD_1              },
    {"WORLD_2",               K_WORLD_2              },
    {"WORLD_3",               K_WORLD_3              },
    {"WORLD_4",               K_WORLD_4              },
    {"WORLD_5",               K_WORLD_5              },
    {"WORLD_6",               K_WORLD_6              },
    {"WORLD_7",               K_WORLD_7              },
    {"WORLD_8",               K_WORLD_8              },
    {"WORLD_9",               K_WORLD_9              },
    {"WORLD_10",              K_WORLD_10             },
    {"WORLD_11",              K_WORLD_11             },
    {"WORLD_12",              K_WORLD_12             },
    {"WORLD_13",              K_WORLD_13             },
    {"WORLD_14",              K_WORLD_14             },
    {"WORLD_15",              K_WORLD_15             },
    {"WORLD_16",              K_WORLD_16             },
    {"WORLD_17",              K_WORLD_17             },
    {"WORLD_18",              K_WORLD_18             },
    {"WORLD_19",              K_WORLD_19             },
    {"WORLD_20",              K_WORLD_20             },
    {"WORLD_21",              K_WORLD_21             },
    {"WORLD_22",              K_WORLD_22             },
    {"WORLD_23",              K_WORLD_23             },
    {"WORLD_24",              K_WORLD_24             },
    {"WORLD_25",              K_WORLD_25             },
    {"WORLD_26",              K_WORLD_26             },
    {"WORLD_27",              K_WORLD_27             },
    {"WORLD_28",              K_WORLD_28             },
    {"WORLD_29",              K_WORLD_29             },
    {"WORLD_30",              K_WORLD_30             },
    {"WORLD_31",              K_WORLD_31             },
    {"WORLD_32",              K_WORLD_32             },
    {"WORLD_33",              K_WORLD_33             },
    {"WORLD_34",              K_WORLD_34             },
    {"WORLD_35",              K_WORLD_35             },
    {"WORLD_36",              K_WORLD_36             },
    {"WORLD_37",              K_WORLD_37             },
    {"WORLD_38",              K_WORLD_38             },
    {"WORLD_39",              K_WORLD_39             },
    {"WORLD_40",              K_WORLD_40             },
    {"WORLD_41",              K_WORLD_41             },
    {"WORLD_42",              K_WORLD_42             },
    {"WORLD_43",              K_WORLD_43             },
    {"WORLD_44",              K_WORLD_44             },
    {"WORLD_45",              K_WORLD_45             },
    {"WORLD_46",              K_WORLD_46             },
    {"WORLD_47",              K_WORLD_47             },
    {"WORLD_48",              K_WORLD_48             },
    {"WORLD_49",              K_WORLD_49             },
    {"WORLD_50",              K_WORLD_50             },
    {"WORLD_51",              K_WORLD_51             },
    {"WORLD_52",              K_WORLD_52             },
    {"WORLD_53",              K_WORLD_53             },
    {"WORLD_54",              K_WORLD_54             },
    {"WORLD_55",              K_WORLD_55             },
    {"WORLD_56",              K_WORLD_56             },
    {"WORLD_57",              K_WORLD_57             },
    {"WORLD_58",              K_WORLD_58             },
    {"WORLD_59",              K_WORLD_59             },
    {"WORLD_60",              K_WORLD_60             },
    {"WORLD_61",              K_WORLD_61             },
    {"WORLD_62",              K_WORLD_62             },
    {"WORLD_63",              K_WORLD_63             },
    {"WORLD_64",              K_WORLD_64             },
    {"WORLD_65",              K_WORLD_65             },
    {"WORLD_66",              K_WORLD_66             },
    {"WORLD_67",              K_WORLD_67             },
    {"WORLD_68",              K_WORLD_68             },
    {"WORLD_69",              K_WORLD_69             },
    {"WORLD_70",              K_WORLD_70             },
    {"WORLD_71",              K_WORLD_71             },
    {"WORLD_72",              K_WORLD_72             },
    {"WORLD_73",              K_WORLD_73             },
    {"WORLD_74",              K_WORLD_74             },
    {"WORLD_75",              K_WORLD_75             },
    {"WORLD_76",              K_WORLD_76             },
    {"WORLD_77",              K_WORLD_77             },
    {"WORLD_78",              K_WORLD_78             },
    {"WORLD_79",              K_WORLD_79             },
    {"WORLD_80",              K_WORLD_80             },
    {"WORLD_81",              K_WORLD_81             },
    {"WORLD_82",              K_WORLD_82             },
    {"WORLD_83",              K_WORLD_83             },
    {"WORLD_84",              K_WORLD_84             },
    {"WORLD_85",              K_WORLD_85             },
    {"WORLD_86",              K_WORLD_86             },
    {"WORLD_87",              K_WORLD_87             },
    {"WORLD_88",              K_WORLD_88             },
    {"WORLD_89",              K_WORLD_89             },
    {"WORLD_90",              K_WORLD_90             },
    {"WORLD_91",              K_WORLD_91             },
    {"WORLD_92",              K_WORLD_92             },
    {"WORLD_93",              K_WORLD_93             },
    {"WORLD_94",              K_WORLD_94             },
    {"WORLD_95",              K_WORLD_95             },

    {"WINDOWS",               K_SUPER                },
    {"COMPOSE",               K_COMPOSE              },
    {"MODE",                  K_MODE                 },
    {"HELP",                  K_HELP                 },
    {"PRINT",                 K_PRINT                },
    {"SYSREQ",                K_SYSREQ               },
    {"SCROLLOCK",             K_SCROLLOCK            },
    {"BREAK",                 K_BREAK                },
    {"MENU",                  K_MENU                 },
    {"POWER",                 K_POWER                },
    {"EURO",                  K_EURO                 },
    {"UNDO",                  K_UNDO                 },

    {"PAD0_A",                K_PAD0_A               },
    {"PAD0_B",                K_PAD0_B               },
    {"PAD0_X",                K_PAD0_X               },
    {"PAD0_Y",                K_PAD0_Y               },
    {"PAD0_BACK",             K_PAD0_BACK            },
    {"PAD0_GUIDE",            K_PAD0_GUIDE           },
    {"PAD0_START",            K_PAD0_START           },
    {"PAD0_LEFTSTICK_CLICK",  K_PAD0_LEFTSTICK_CLICK },
    {"PAD0_RIGHTSTICK_CLICK", K_PAD0_RIGHTSTICK_CLICK},
    {"PAD0_LEFTSHOULDER",     K_PAD0_LEFTSHOULDER    },
    {"PAD0_RIGHTSHOULDER",    K_PAD0_RIGHTSHOULDER   },
    {"PAD0_DPAD_UP",          K_PAD0_DPAD_UP         },
    {"PAD0_DPAD_DOWN",        K_PAD0_DPAD_DOWN       },
    {"PAD0_DPAD_LEFT",        K_PAD0_DPAD_LEFT       },
    {"PAD0_DPAD_RIGHT",       K_PAD0_DPAD_RIGHT      },

    {"PAD0_LEFTSTICK_LEFT",   K_PAD0_LEFTSTICK_LEFT  },
    {"PAD0_LEFTSTICK_RIGHT",  K_PAD0_LEFTSTICK_RIGHT },
    {"PAD0_LEFTSTICK_UP",     K_PAD0_LEFTSTICK_UP    },
    {"PAD0_LEFTSTICK_DOWN",   K_PAD0_LEFTSTICK_DOWN  },
    {"PAD0_RIGHTSTICK_LEFT",  K_PAD0_RIGHTSTICK_LEFT },
    {"PAD0_RIGHTSTICK_RIGHT", K_PAD0_RIGHTSTICK_RIGHT},
    {"PAD0_RIGHTSTICK_UP",    K_PAD0_RIGHTSTICK_UP   },
    {"PAD0_RIGHTSTICK_DOWN",  K_PAD0_RIGHTSTICK_DOWN },
    {"PAD0_LEFTTRIGGER",      K_PAD0_LEFTTRIGGER     },
    {"PAD0_RIGHTTRIGGER",     K_PAD0_RIGHTTRIGGER    },

    {"PAD0_MISC1",            K_PAD0_MISC1           },
    {"PAD0_PADDLE1",          K_PAD0_PADDLE1         },
    {"PAD0_PADDLE2",          K_PAD0_PADDLE2         },
    {"PAD0_PADDLE3",          K_PAD0_PADDLE3         },
    {"PAD0_PADDLE4",          K_PAD0_PADDLE4         },
    {"PAD0_TOUCHPAD",         K_PAD0_TOUCHPAD        },

    {"LALT",                  K_ALT                  },
    {"LCTRL",                 K_CTRL                 },
    {"LSHIFT",                K_SHIFT                },

    {NULL,                    0                      }
};

void Message_Key(int key)
{
    if (key == K_ESCAPE) {
        Key_SetCatcher(Key_GetCatcher() & ~KEYCATCH_MESSAGE);
        return;
    }

    if (key == K_ENTER || key == K_KP_ENTER) {
        Key_SetCatcher(Key_GetCatcher() & ~KEYCATCH_MESSAGE);
        return;
    }
}


qboolean Key_GetOverstrikeMode(void)
{
    return key_overstrikeMode;
}

void Key_SetOverstrikeMode(qboolean state)
{
    key_overstrikeMode = state;
}

qboolean Key_IsDown(int keynum)
{
    if (keynum < 0 || keynum >= MAX_KEYS) {
        return qfalse;
    }

    return keys[keynum].down;
}

int Key_StringToKeynum(const char *str)
{
    keyname_t *kn;

    if (!str || !str[0]) {
        return -1;
    }
    if (!str[1]) {
        return str[0];
    }

    if (str[0] == '0' && str[1] == 'x' && strlen(str) == 4) {
        int n1, n2;

        n1 = str[2];
        if (n1 >= '0' && n1 <= '9') {
            n1 -= '0';
        } else if (n1 >= 'a' && n1 <= 'f') {
            n1 = n1 - 'a' + 10;
        } else {
            n1 = 0;
        }

        n2 = str[3];
        if (n2 >= '0' && n2 <= '9') {
            n2 -= '0';
        } else if (n2 >= 'a' && n2 <= 'f') {
            n2 = n2 - 'a' + 10;
        } else {
            n2 = 0;
        }

        return n1 * 16 + n2;
    }

    for (kn = keynames; kn->name; kn++) {
        if (!Q_stricmp(str, kn->name)) {
            return kn->keynum;
        }
    }

    return -1;
}

const char *Key_KeynumToString(int keynum)
{
    keyname_t  *kn;
    static char tinystr[5];
    int         i, j;

    if (keynum == -1) {
        return "<KEY NOT FOUND>";
    }

    if (keynum < 0 || keynum >= MAX_KEYS) {
        return "<OUT OF RANGE>";
    }

    if (keynum > 32 && keynum < 127 && keynum != '"' && keynum != ';') {
        tinystr[0] = keynum;
        tinystr[1] = 0;
        return tinystr;
    }

    for (kn = keynames; kn->name; kn++) {
        if (keynum == kn->keynum) {
            return kn->name;
        }
    }

    i = keynum >> 4;
    j = keynum & 15;

    tinystr[0] = '0';
    tinystr[1] = 'x';
    tinystr[2] = i > 9 ? i - 10 + 'a' : i + '0';
    tinystr[3] = j > 9 ? j - 10 + 'a' : j + '0';
    tinystr[4] = 0;

    return tinystr;
}

const char *Key_KeynumToBindString(int keynum)
{
    keyname_t  *kn;
    static char tinystr[5];

    if (keynum == -1) {
        return "Not Bound";
    }

    if (keynum < 0 || keynum >= MAX_KEYS) {
        return "Out of Range";
    }

    if (keynum > 32 && keynum < 127 && keynum != '"' && keynum != ';') {
        tinystr[0] = keynum;
        tinystr[1] = 0;
        return tinystr;
    }

    for (kn = keynames; kn->name; kn++) {
        if (keynum == kn->keynum) {
            return kn->name;
        }
    }

    return "Unknown Key";
}

void Key_SetBinding(int keynum, const char *binding)
{
    if (keynum < 0 || keynum >= MAX_KEYS) {
        return;
    }

    if (keys[keynum].binding) {
        Z_Free(keys[keynum].binding);
    }

    keys[keynum].binding = CopyString(binding);

    cvar_modifiedFlags |= CVAR_ARCHIVE;
}

void Key_SetAltBinding(int keynum, const char *binding)
{
    if (keynum < 0 || keynum >= MAX_KEYS) {
        return;
    }

    if (altkeys[keynum].binding) {
        Z_Free(altkeys[keynum].binding);
    }

    altkeys[keynum].binding = CopyString(binding);

    cvar_modifiedFlags |= CVAR_ARCHIVE;
}

void Key_SetCtrlBinding(int keynum, const char *binding)
{
    if (keynum < 0 || keynum >= MAX_KEYS) {
        return;
    }

    if (ctrlkeys[keynum].binding) {
        Z_Free(ctrlkeys[keynum].binding);
    }

    ctrlkeys[keynum].binding = CopyString(binding);

    cvar_modifiedFlags |= CVAR_ARCHIVE;
}

const char *Key_GetBinding(int keynum)
{
    if (keynum == -1) {
        return "";
    }

    return keys[keynum].binding;
}


int Key_GetKey(const char *binding)
{
    int i;

    if (binding) {
        for (i = 0; i < MAX_KEYS; i++) {
            if (keys[i].binding && Q_stricmp(binding, keys[i].binding) == 0) {
                return i;
            }
        }
    }
    return -1;
}

const char *Key_GetKeynameForCommand(const char *command)
{
    int i;

    for (i = 0; i < MAX_KEYS; i++) {
        if (!keys[i].binding) {
            continue;
        }
        // Retail EF2 does NOT test keys[i].down here -- Mac PPC
        if (!stricmp(command, keys[i].binding)) {
            return Key_KeynumToString(i);
        }
    }

    return "Unknown Command";
}

void Key_GetKeysForCommand(const char *command, int *key1, int *key2)
{
    int i;

    *key1 = *key2 = -1;

    for (i = 0; i < MAX_KEYS; i++) {
        if (!keys[i].binding) {
            continue;
        }
        if (!stricmp(command, keys[i].binding)) {
            if (*key1 == -1) {
                *key1 = i;
            } else {
                *key2 = i;
                return;
            }
        }
    }
}

void Key_Unbind_f(void)
{
    int b;

    if (Cmd_Argc() != 2) {
        Com_Printf("unbind <key> : remove commands from a key\n");
        return;
    }

    b = Key_StringToKeynum(Cmd_Argv(1));
    if (b == -1) {
        Com_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
        return;
    }

    Key_SetBinding(b, "");
}

void Key_Unaltbind_f(void)
{
    int b;

    if (Cmd_Argc() != 2) {
        Com_Printf("unaltbind <key> : remove commands from a key\n");
        return;
    }

    b = Key_StringToKeynum(Cmd_Argv(1));
    if (b == -1) {
        Com_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
        return;
    }

    Key_SetAltBinding(b, "");
}

void Key_Unctrlbind_f(void)
{
    int b;

    if (Cmd_Argc() != 2) {
        Com_Printf("unctrlbind <key> : remove commands from a key\n");
        return;
    }

    b = Key_StringToKeynum(Cmd_Argv(1));
    if (b == -1) {
        Com_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
        return;
    }

    Key_SetCtrlBinding(b, "");
}

void Key_Unbindall_f(void)
{
    int i;

    for (i = 0; i < MAX_KEYS; i++) {
        if (keys[i].binding) {
            Key_SetBinding(i, "");
        }
    }
}

void Key_Bind_f(void)
{
    int  i, c, b;
    char cmd[1024];

    c = Cmd_Argc();

    if (c < 2) {
        Com_Printf("bind <key> [command] : attach a command to a key\n");
        return;
    }
    b = Key_StringToKeynum(Cmd_Argv(1));
    if (b == -1) {
        Com_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
        return;
    }

    if (c == 2) {
        if (keys[b].binding) {
            Com_Printf("\"%s\" = \"%s\"\n", Cmd_Argv(1), keys[b].binding);
        } else {
            Com_Printf("\"%s\" is not bound\n", Cmd_Argv(1));
        }
        return;
    }

    cmd[0] = 0; 
    for (i = 2; i < c; i++) {
        strcat(cmd, Cmd_Argv(i));
        if (i != (c - 1)) {
            strcat(cmd, " ");
        }
    }

    Key_SetBinding(b, cmd);
}

void Key_AltBind_f(void)
{
    int  i, c, b;
    char cmd[1024];

    c = Cmd_Argc();

    if (c < 2) {
        Com_Printf("altbind <key> [command] : attach a command to a key\n");
        return;
    }
    b = Key_StringToKeynum(Cmd_Argv(1));
    if (b == -1) {
        Com_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
        return;
    }

    if (c == 2) {
        if (altkeys[b].binding) {
            Com_Printf("\"%s\" = \"%s\"\n", Cmd_Argv(1), altkeys[b].binding);
        } else {
            Com_Printf("\"%s\" is not bound\n", Cmd_Argv(1));
        }
        return;
    }

    cmd[0] = 0; 
    for (i = 2; i < c; i++) {
        strcat(cmd, Cmd_Argv(i));
        if (i != (c - 1)) {
            strcat(cmd, " ");
        }
    }

    Key_SetAltBinding(b, cmd);
}

void Key_CtrlBind_f(void)
{
    int  i, c, b;
    char cmd[1024];

    c = Cmd_Argc();

    if (c < 2) {
        Com_Printf("ctrlbind <key> [command] : attach a command to a key\n");
        return;
    }
    b = Key_StringToKeynum(Cmd_Argv(1));
    if (b == -1) {
        Com_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
        return;
    }

    if (c == 2) {
        if (ctrlkeys[b].binding) {
            Com_Printf("\"%s\" = \"%s\"\n", Cmd_Argv(1), ctrlkeys[b].binding);
        } else {
            Com_Printf("\"%s\" is not bound\n", Cmd_Argv(1));
        }
        return;
    }

    cmd[0] = 0; 
    for (i = 2; i < c; i++) {
        strcat(cmd, Cmd_Argv(i));
        if (i != (c - 1)) {
            strcat(cmd, " ");
        }
    }

    Key_SetCtrlBinding(b, cmd);
}

void Key_WriteBindings(fileHandle_t f)
{
    int i;

    FS_Printf(f, "unbindall\n");

    for (i = 0; i < MAX_KEYS; i++) {	
        if (keys[i].binding && keys[i].binding[0]) {
            FS_Printf(f, "bind %s \"%s\"\n", Key_KeynumToString(i), keys[i].binding);
        }

        if (altkeys[i].binding && altkeys[i].binding[0]) {
            FS_Printf(f, "altbind %s \"%s\"\n", Key_KeynumToString(i), altkeys[i].binding);
        }

        if (ctrlkeys[i].binding && ctrlkeys[i].binding[0]) {
            FS_Printf(f, "ctrlbind %s \"%s\"\n", Key_KeynumToString(i), ctrlkeys[i].binding);
        }
    }
}

void Key_Bindlist_f(void)
{
    int i;

    for (i = 0; i < MAX_KEYS; i++) {
        if (keys[i].binding && keys[i].binding[0]) {
            Com_Printf("%s \"%s\"\n", Key_KeynumToString(i), keys[i].binding);
        }
    }
}

void Key_AltBindlist_f(void)
{
    int i;

    for (i = 0; i < MAX_KEYS; i++) {
        if (altkeys[i].binding && altkeys[i].binding[0]) {
            Com_Printf("%s \"%s\"\n", Key_KeynumToString(i), altkeys[i].binding);
        }
    }
}

void Key_CtrlBindlist_f(void)
{
    int i;

    for (i = 0; i < MAX_KEYS; i++) {
        if (ctrlkeys[i].binding && ctrlkeys[i].binding[0]) {
            Com_Printf("%s \"%s\"\n", Key_KeynumToString(i), ctrlkeys[i].binding);
        }
    }
}

void Key_KeynameCompletion(void (*callback)(const char *s))
{
    int i;

    for (i = 0; keynames[i].name != NULL; i++) {
        callback(keynames[i].name);
    }
}

void CL_InitKeyCommands(void)
{
    int i;

    Cmd_AddCommand("bind", Key_Bind_f);
    Cmd_AddCommand("unbind", Key_Unbind_f);
    Cmd_AddCommand("altbind", Key_AltBind_f);
    Cmd_AddCommand("unaltbind", Key_Unaltbind_f);
    Cmd_AddCommand("ctrlbind", Key_CtrlBind_f);
    Cmd_AddCommand("unctrlbind", Key_Unctrlbind_f);
    Cmd_AddCommand("unbindall", Key_Unbindall_f);
    Cmd_AddCommand("bindlist", Key_Bindlist_f);
    Cmd_AddCommand("altbindlist", Key_AltBindlist_f);
    Cmd_AddCommand("ctrlbindlist", Key_AltBindlist_f);

    memset(menubound, 0, sizeof(menubound));

    for (i = K_F1; i <= K_F12; i++) {
        menubound[i] = qtrue;
    }
}

void CL_AddKeyUpCommands(int key, char *kb, unsigned time)
{
    int      i;
    char     button[1024], *buttonPtr;
    char     cmd[1024];
    qboolean keyevent;

    if (!kb) {
        return;
    }
    keyevent  = qfalse;
    buttonPtr = button;
    for (i = 0;; i++) {
        if (kb[i] == ';' || !kb[i]) {
            *buttonPtr = '\0';
            if (button[0] == '+') {
                Com_sprintf(cmd, sizeof(cmd), "-%s %i %i\n", button + 1, key, time);
                PROBE("input:bindexec", "key=%d executed(-arm)='-%s'", key, button + 1);
                Cbuf_AddText(cmd);
                keyevent = qtrue;
            } else {
                if (keyevent) {
                    Cbuf_AddText(button);
                    Cbuf_AddText("\n");
                }
            }
            buttonPtr = button;
            while ((kb[i] <= ' ' || kb[i] == ';') && kb[i] != 0) {
                i++;
            }
        }
        *buttonPtr++ = kb[i];
        if (!kb[i]) {
            break;
        }
    }
}

void CL_KeyEvent(int key, qboolean down, unsigned time)
{
    char *kb;
    char  cmd[1024];

    keys[key].down = down;

    if (down) {
        keys[key].repeats++;
        if (keys[key].repeats == 1) {
            anykeydown++;
        }
    } else {
        keys[key].repeats = 0;
        anykeydown--;
        if (anykeydown < 0) {
            anykeydown = 0;
        }
    }

    if (key == K_ALT && cl_altbindings->integer) {
        alt_down = down;
    }

    if (key == K_CTRL && cl_ctrlbindings->integer) {
        ctrl_down = down;
    }

    if (key == K_CONSOLE) {
        if (!down) {
            return;
        }

        if (Cvar_VariableIntegerValue("ui_console")) {
            UIVK_PROBE("key", "key='`' consumer=UI_ToggleConsole (ui_console=1)");
            UI_ToggleConsole();
        } else {
            UIVK_PROBE("key", "key='`' consumer=none (ui_console=0)");
        }
        return;
    }

    // CTRL not held.  Latent today only because nothing in shipped content or the
    if (down) {
        if (alt_down && key != K_ALT) {
            altkeys[key].down = down;
        }
        if (ctrl_down && key != K_CTRL) {
            ctrlkeys[key].down = down;
        }
    }

    if (key == K_ENTER) {
        if (down) {
            if (keys[K_ALT].down) {
                Key_ClearStates();
                Cvar_SetValue("r_fullscreen", !Cvar_VariableIntegerValue("r_fullscreen"));
                return;
            }
        }
    }

    if (key >= K_MOUSE1 && key <= K_MOUSE5) {
        if (down) {
            cl.mouseButtons |= (1 << (key - K_MOUSE1));
        } else {
            cl.mouseButtons &= ~(1 << (key - K_MOUSE1));
        }

        if (in_guimouse) {
            return;
        }
    }

    if (down && (key <= K_BACKSPACE || key == K_MOUSE1) && (clc.demoplaying || cls.state == CA_CINEMATIC)
        && Key_GetCatcher() == 0) {
        Cvar_Set("nextdemo", "");
        key = K_ESCAPE;
    }

    if (key == K_ESCAPE) {
        if (down) {
            qboolean wasup = UI_MenuUp();
            char     uivkhudbuf[256];

            UIVK_PROBE("key",
                       "key='ESCAPE' down=1 State=%d catcher=0x%x menuup=%d hudnoescape=%d cin=%d "
                       "hudcount=%d huds='%s' svloading=%d cinflag=%d",
                       cls.state,
                       Key_GetCatcher(),
                       wasup ? 1 : 0,
                       UI_IsHudActive() ? 1 : 0,
                       cinematicRunning() ? 1 : 0,
                       UI_HudCount(),
                       UI_HudNames(uivkhudbuf, sizeof(uivkhudbuf)),
                       UI_IsServerLoading() ? 1 : 0,
                       (cl.snap.ps.stats[STAT_CINEMATIC] & 1) ? 1 : 0);
            // every UIFloatingWindow/UIPopupMenu on ESC, a state change retail never makes.

            if (cinematicRunning()) {
                UIVK_PROBE("key", "key='ESCAPE' consumer=StopCinematic (cinematic up)");
                StopCinematic();
                return;
            }

            UIWidget *uivkdmcon = uWinMan.ActiveControl();
            if (uivkdmcon && uivkdmcon->getName() && !Q_stricmp(uivkdmcon->getName(), "dm_console")) {
                UIVK_PROBE("key", "key='ESCAPE' consumer=UI_ConsoleEscape (dm_console)");
                uivkdmcon->KeyEvent(K_ESCAPE, time);
                return;
            }

            if (wasup) {
                UIVK_PROBE("key",
                           "key='ESCAPE' consumer=UI_MenuEscape(current menu up, State=%d)",
                           cls.state);
                UI_MenuEscape("main");
                return;
            }

            // to the main menu and NEVER skipped a cinematic. Removed that dead check so
            if (cl.snap.ps.stats[STAT_CINEMATIC] & 1 && cls.state == CA_ACTIVE) {
                UIVK_PROBE("key", "key='ESCAPE' consumer=skipcinematic (STAT_CINEMATIC)");
                Cbuf_AddText("skipcinematic\n");
                return;
            }

            if ((unsigned int)(cls.state - CA_CONNECTING) <= 1 || cls.state == CA_CONNECTED) {
                UIVK_PROBE("key",
                           "key='ESCAPE' consumer=CL_Disconnect+UI_MenuEscape (State=%d)",
                           cls.state);
                CL_Disconnect(1);
                UI_MenuEscape("main");
                return;
            }

            if (cls.state <= CA_DISCONNECTED || cls.state == CA_ACTIVE) {
                UIVK_PROBE("key",
                           "key='ESCAPE' consumer=UI_MenuEscape(no menu up, State=%d)",
                           cls.state);
                UI_MenuEscape("main");
            } else {
                UIVK_PROBE("key",
                           "key='ESCAPE' consumer=none (State=%d, no retail arm)",
                           cls.state);
            }
            return;
        }
    } else if (down) {
        // The chat-input gate below is the KEY CATCHER, not cls.loading.  Retail,
        if ((Key_GetCatcher() & KEYCATCH_UI && !menubound[key]) || UI_BindActive()) {
            // instead of the binding executor.  Digits are NOT menubound in retail's
            PROBE(
                "input:swallowed",
                "key=%d catcher=0x%x menubound=%d bindActive=%d bind='%s'",
                key,
                Key_GetCatcher(),
                menubound[key] ? 1 : 0,
                UI_BindActive() ? 1 : 0,
                keys[key].binding ? keys[key].binding : "(none)"
            );
            UI_KeyEvent(key, time);
        } else if (Key_GetCatcher() & KEYCATCH_MESSAGE) {
            Message_Key(key);
        } else if (cls.state != CA_DISCONNECTED || menubound[key]) {
            // Retail's wart is reproduced deliberately: the gate is applied to a
            kb = altkeys[key].binding;
            if (kb && paused->integer && Q_stricmp(kb, "pause")) {
                return;
            }
            if (!kb || !altkeys[key].down) {
                kb = ctrlkeys[key].binding;
                if (kb && paused->integer && Q_stricmp(kb, "pause")) {
                    return;
                }
                if (!kb || !ctrlkeys[key].down) {
                    kb = keys[key].binding;
                    if (kb && paused->integer && Q_stricmp(kb, "pause")) {
                        return;
                    }
                }
            }
            if (!kb) {
                if (key >= 200) {
                    Com_Printf("%s is unbound, use controls menu to set.\n", Key_KeynumToString(key));
                }
            } else if (kb[0] == '+') {
                int  i;
                char button[1024], *buttonPtr;
                buttonPtr = button;
                for (i = 0;; i++) {
                    if (kb[i] == ';' || !kb[i]) {
                        *buttonPtr = '\0';
                        if (button[0] == '+') {
                            Com_sprintf(cmd, sizeof(cmd), "%s %i %i\n", button, key, time);
                            PROBE("input:bindexec", "key=%d executed(+arm)='%s'", key, button);
                            Cbuf_AddText(cmd);
                        } else {
                            Cbuf_AddText(button);
                            Cbuf_AddText("\n");
                        }
                        buttonPtr = button;
                        while ((kb[i] <= ' ' || kb[i] == ';') && kb[i] != 0) {
                            i++;
                        }
                    }
                    *buttonPtr++ = kb[i];
                    if (!kb[i]) {
                        break;
                    }
                }
            } else {
                PROBE("input:bindexec", "key=%d executed='%s'", key, kb);
                Cbuf_AddText(kb);
                Cbuf_AddText("\n");
            }
        }
        return;
    }

    if (altkeys[key].down) {
        kb                = altkeys[key].binding;
        altkeys[key].down = false;
        CL_AddKeyUpCommands(key, kb, time);
    }
    if (ctrlkeys[key].down) {
        kb                 = ctrlkeys[key].binding;
        ctrlkeys[key].down = false;
        CL_AddKeyUpCommands(key, kb, time);
    }

    CL_AddKeyUpCommands(key, keys[key].binding, time);
}

void CL_Key_ForceCommand(int key, unsigned int time)
{
    const char *kb;
    char        cmd[1024];

    if (cls.state == CA_DISCONNECTED) {
        return;
    }

    kb = keys[key].binding;
    if (kb) {
        if (*kb == '+') {
            Com_sprintf(cmd, sizeof(cmd), "%s %i %i\n", kb, key, time);
        } else {
            Cbuf_AddText(keys[key].binding);
            Cbuf_AddText("\n");
        }
    } else if (key > K_JOY12) {
        Com_Printf("%s is unbound, use controls menu to set.\n", Key_KeynumToString(key));
    }
}

void CL_CharEvent(int key)
{
    if (key == '`' || key == '~') {
        return;
    }

    if (key == 127) {
        return;
    }

    if (Key_GetCatcher() & KEYCATCH_CONSOLE) {
        return;
    }

    if (Key_GetCatcher() & KEYCATCH_UI) {
        UI_CharEvent(key);
    }
}

void Key_ClearStates(void)
{
    int i;

    anykeydown = 0;

    for (i = 0; i < MAX_KEYS; i++) {
        if (keys[i].down) {
            CL_KeyEvent(i, qfalse, 0);
        }
        keys[i].down    = 0;
        keys[i].repeats = 0;
    }
}

int Key_GetCatcher(void)
{
    return cls.keyCatchers;
}

void Key_SetCatcher(int catcher)
{
    cls.keyCatchers = catcher;
}
