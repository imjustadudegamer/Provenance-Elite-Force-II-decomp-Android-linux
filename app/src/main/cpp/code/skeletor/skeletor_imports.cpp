

#include "q_shared.h"
#include "qcommon.h"
#include "skeletor.h"

void Skel_DPrintf(const char *fmt, ...)
{
    char    msg[4096];
    va_list va;

    va_start(va, fmt);
    Q_vsnprintf(msg, sizeof(msg), fmt, va);
    va_end(va);

    Com_DPrintf("%s", msg);
}

#ifndef _DEBUG_MEM

void Skel_Free(void *ptr)
{
    Z_Free(ptr);
}

void *Skel_Alloc(size_t size)
{
    return Z_TagMalloc(size, TAG_TIKI);
}

#endif

void Skel_FreeFile(void *buffer)
{
    FS_FreeFile(buffer);
}

int Skel_ReadFileEx(const char *qpath, void **buffer, qboolean quiet)
{
    return FS_ReadFileEx(qpath, buffer, quiet);
}
