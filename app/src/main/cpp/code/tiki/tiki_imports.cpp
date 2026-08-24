

#include "q_shared.h"
#include "qcommon.h"
#include "tiki_imports.h"

#ifndef DEDICATED
#include "../client/client.h"
#endif

void TIKI_DPrintf(const char *fmt, ...)
{
    char    msg[4096];
    va_list va;

    va_start(va, fmt);
    Q_vsnprintf(msg, sizeof(msg), fmt, va);
    va_end(va);

    Com_DPrintf("%s", msg);
}

void TIKI_Free(void *ptr)
{
    Z_Free(ptr);
}

void *TIKI_Alloc(size_t size)
{
    return Z_TagMalloc(size, TAG_TIKI);
}

void TIKI_FreeFile(void *buffer)
{
    FS_FreeFile(buffer);
}

int TIKI_ReadFileEx(const char *qpath, void **buffer, qboolean quiet)
{
    return FS_ReadFileEx(qpath, buffer, quiet);
}

void TIKI_UpdateScreen(void)
{
#ifndef DEDICATED
    SCR_UpdateScreen();
#endif
}
