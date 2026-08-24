

#pragma once

#include "../qcommon/q_shared.h"
#include "../corepp/str.h"

class ScriptException
{
public:
    str string;
    int bAbort;
    int bIsForAnim;

private:
    void CreateException(const char *data);

public:
    ScriptException(str text);
    ScriptException(const char *format, ...) Q_PRINTF_FUNC(2, 3);
    ScriptException(char *text);

    static int next_abort;
    static int next_bIsForAnim;
};

void Error(const char *format, ...) Q_PRINTF_FUNC(1, 2);

#define ScriptDeprecated(function) throw ScriptException(function ": DEPRECATED. DON'T USE IT ANYMORE")
#define ScriptDeprecatedAltMethod(alternative_name) \
    gi.DPrintf("WARNING: %s: is deprecated and has been superseded by the " alternative_name " method\n", __FUNCTION__)
#define ScriptDeprecatedAltVariable(alternative_name)                                                             \
    gi.DPrintf(                                                                                                   \
        "WARNING: %s: is deprecated and has been superseded by the " alternative_name " variable\n", __FUNCTION__ \
    )
#define ScriptError throw ScriptException
