
#include "lightclass.h"

#if defined(GAME_DLL)

#    include "../fgame/g_local.h"

#    define LIGHTCLASS_Printf  gi.Printf
#    define LIGHTCLASS_DPrintf gi.DPrintf
#    define LIGHTCLASS_Error   gi.Error

#elif defined(CGAME_DLL)

#    include "../cgame/cg_local.h"

#    define LIGHTCLASS_Printf  cgi.Printf
#    define LIGHTCLASS_DPrintf cgi.DPrintf
#    define LIGHTCLASS_Error   cgi.Error

#else

#    include "../qcommon/qcommon.h"

#    define LIGHTCLASS_Printf  Com_Printf
#    define LIGHTCLASS_DPrintf Com_DPrintf
#    define LIGHTCLASS_Error   Com_Error

#endif

size_t       totalmemallocated   = 0;
unsigned int numclassesallocated = 0;

void DisplayMemoryUsage()
{
    LIGHTCLASS_Printf("Classes %-5d Class memory used: %zu\n", numclassesallocated, totalmemallocated);
}

#ifndef _DEBUG_MEM
void *LightClass::operator new(size_t s)
{
    size_t *p;

    if (s == 0) {
        static void *empty_memory = nullptr;
        return &empty_memory;
    }

    s += sizeof(size_t);

#    ifdef GAME_DLL
    p = (size_t *)gi.Malloc(s);
#    elif defined(CGAME_DLL)
    p = (size_t *)cgi.Malloc(s);
#    else
    p = (size_t *)Z_Malloc(s);
#    endif

    totalmemallocated += s;
    numclassesallocated++;

    p++;

    return p;
}

void LightClass::operator delete(void *ptr)
{
    size_t *p = ((size_t *)ptr) - 1;

    totalmemallocated -= *p;
    numclassesallocated--;

#    ifdef GAME_DLL
    gi.Free(p);
#    elif defined(CGAME_DLL)
    cgi.Free(p);
#    else
    Z_Free(p);
#    endif
}
#else

void *LightClass::operator new(size_t s)
{
    return ::operator new(s);
}

void LightClass::operator delete(void *ptr)
{
    ::operator delete(ptr);
}

#endif

void *LightClass::operator new(size_t size, void *placement)
{
    return placement;
}

void LightClass::operator delete(void *ptr, void *placement) {}
