

#if defined(GAME_DLL)

#    include "../fgame/g_local.h"

void *MEM_Alloc(int size)
{
    return gi.Malloc(size);
}

void MEM_Free(void *ptr)
{
    return gi.Free(ptr);
}

#elif defined(CGAME_DLL)

#    include "../cgame/cg_local.h"

void *MEM_Alloc(int size)
{
    return cgi.Malloc(size);
}

void MEM_Free(void *ptr)
{
    return cgi.Free(ptr);
}

#elif defined(REF_DLL)

#    include "../renderercommon/tr_common.h"

void *MEM_Alloc(int size)
{
    return ri.Malloc(size);
}

void MEM_Free(void *ptr)
{
    return ri.Free(ptr);
}

#else

#    include "../qcommon/qcommon.h"

void *MEM_Alloc(int size)
{
    return Z_Malloc(size);
}

void MEM_Free(void *ptr)
{
    return Z_Free(ptr);
}

#endif
