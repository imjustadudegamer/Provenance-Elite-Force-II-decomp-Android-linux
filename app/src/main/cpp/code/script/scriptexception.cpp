

#include "scriptexception.h"
#include "q_shared.h"

#include <cstdarg>

int ScriptException::next_abort      = 0;
int ScriptException::next_bIsForAnim = 0;

void ScriptException::CreateException(const char *data)
{
    string = data;

    bAbort          = next_abort;
    next_abort      = 0;
    bIsForAnim      = next_bIsForAnim;
    next_bIsForAnim = 0;
}

ScriptException::ScriptException(str text)
{
    CreateException(text.c_str());
}

ScriptException::ScriptException(const char *format, ...)
{
    va_list va;
    char    data[4100];

    va_start(va, format);
    Q_vsnprintf(data, sizeof(data), format, va);
    va_end(va);

    CreateException(data);
}

ScriptException::ScriptException(char *text)
{
    CreateException(text);
}

void Error(const char *format, ...)
{
    va_list va;
    char    data[4100];

    va_start(va, format);
    Q_vsnprintf(data, sizeof(data), format, va);
    va_end(va);

    throw ScriptException("%s", data);
}
