

#pragma once

#include <cstddef>

class tempBlock_t;

class MEM_TempAlloc
{
public:
    MEM_TempAlloc();

    void *Alloc(size_t len);
    void *Alloc(size_t len, size_t alignment);
    void  FreeAll(void);
    void *CreateBlock(size_t len);

private:
    tempBlock_t *m_CurrentMemoryBlock;
    size_t       m_CurrentMemoryPos;
    size_t       m_BlockSize;
    size_t       m_LastPos;
};
