
#pragma once

#include "../client/cl_ui.h"

typedef struct uiGlobals_s {
    UList<str> fileList;
    UIFont    *globalFont;
    str        clientData;
} uiGlobals_t;

extern uiGlobals_t ui;
