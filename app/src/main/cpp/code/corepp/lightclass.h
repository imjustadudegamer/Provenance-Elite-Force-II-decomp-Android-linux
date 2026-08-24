

#pragma once

#include "../qcommon/q_shared.h"

class LightClass
{
public:
    void *operator new(size_t s);
    void  operator delete(void *ptr);

    void *operator new(size_t size, void *placement);
    void  operator delete(void *ptr, void *placement);

    template<typename T>
    void *operator new(size_t size, T& placement)
    {
        return operator new(size, (void *)&placement);
    }

    template<typename T>
    void operator delete(void *ptr, T& placement)
    {
        return operator delete(ptr, (void *)&placement);
    }
};

void DisplayMemoryUsage();
