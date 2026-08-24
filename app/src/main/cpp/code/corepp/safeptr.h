/*
===========================================================================
Copyright (C) 2025 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later


#pragma once

#include "Linklist.h"

class SafePtrBase
{
private:
    void AddReference(Class *newptr);
    void RemoveReference(Class *oldptr);

protected:
    SafePtrBase *prev;
    SafePtrBase *next;
    Class       *ptr;

public:
    SafePtrBase();
    virtual ~SafePtrBase();
    void   InitSafePtr(Class *newptr);
    Class *Pointer(void);
    void   Clear(void);

public:
    static int WIDGETP1_HeadPrevNull(Class *c);

    typedef struct WIDGETF3_HeadInfo_st {
        void *head;   
        void *prev;   
        void *next;   
        void *target; 
        void *vtword;  
    } WIDGETF3_HeadInfo;

    static void WIDGETF3_FillHead(Class *c, WIDGETF3_HeadInfo *out);

    static void WIDGETF3_FillNode(SafePtrBase *n, void **prev, void **next, void **target);
};

inline void SafePtrBase::AddReference(Class *newptr)
{
    if (!newptr->SafePtrList) {
        newptr->SafePtrList = this;
        LL_Reset(this, next, prev);
    } else {
        LL_Add(newptr->SafePtrList, this, next, prev);
    }
}

inline void SafePtrBase::RemoveReference(Class *oldptr)
{
    if (oldptr->SafePtrList == this) {
        if (oldptr->SafePtrList->next == this) {
            oldptr->SafePtrList = nullptr;
        } else {
            oldptr->SafePtrList = next;
            LL_Remove(this, next, prev);
        }
    } else {
        LL_Remove(this, next, prev);
    }
}

inline void SafePtrBase::Clear(void)
{
    if (ptr) {
        RemoveReference(ptr);
        ptr = nullptr;
    }
}

inline SafePtrBase::SafePtrBase()
{
    prev = nullptr;
    next = nullptr;
    ptr  = nullptr;
}

inline SafePtrBase::~SafePtrBase()
{
    Clear();
}

inline Class *SafePtrBase::Pointer(void)
{
    return ptr;
}

inline int SafePtrBase::WIDGETP1_HeadPrevNull(Class *c)
{
    return c->SafePtrList && c->SafePtrList->prev == NULL;
}

inline void SafePtrBase::WIDGETF3_FillHead(Class *c, WIDGETF3_HeadInfo *out)
{
    if (!c || !c->SafePtrList) {
        out->head   = NULL;
        out->prev   = NULL;
        out->next   = NULL;
        out->target = NULL;
        out->vtword = NULL;
        return;
    }

    SafePtrBase *h = c->SafePtrList;
    out->head   = (void *)h;
    out->prev   = (void *)h->prev;
    out->next   = (void *)h->next;
    out->target = (void *)h->ptr;
    out->vtword = *(void **)h;
}

inline void SafePtrBase::WIDGETF3_FillNode(SafePtrBase *n, void **prev, void **next, void **target)
{
    if (!n) {
        *prev   = NULL;
        *next   = NULL;
        *target = NULL;
        return;
    }

    *prev   = (void *)n->prev;
    *next   = (void *)n->next;
    *target = (void *)n->ptr;
}

inline void SafePtrBase::InitSafePtr(Class *newptr)
{
    if (ptr != newptr) {
        if (ptr) {
            RemoveReference(ptr);
        }

        ptr = newptr;
        if (ptr == nullptr) {
            return;
        }

        AddReference(ptr);
    }
}

template<class T>
class SafePtr : public SafePtrBase
{
public:
    SafePtr(T *objptr = 0);
    SafePtr(const SafePtr& obj);

    SafePtr& operator=(const SafePtr& obj);
    SafePtr& operator=(T *const obj);

#ifdef LINUX
    friend bool operator== <>(SafePtr<T> a, T *b);
    friend bool operator!= <>(SafePtr<T> a, T *b);
    friend bool operator== <>(T *a, SafePtr<T> b);
    friend bool operator!= <>(T *a, SafePtr<T> b);
    friend bool operator== <>(SafePtr<T> a, SafePtr<T> b);
    friend bool operator!= <>(SafePtr<T> a, SafePtr<T> b);
#else
    template<class U>
    friend bool operator==(SafePtr<U> a, U *b);
    template<class U>
    friend bool operator!=(SafePtr<U> a, U *b);
    template<class U>
    friend bool operator==(U *a, SafePtr<U> b);
    template<class U>
    friend bool operator!=(U *a, SafePtr<U> b);
    template<class U>
    friend bool operator==(SafePtr<U> a, SafePtr<U> b);
    template<class U>
    friend bool operator!=(SafePtr<U> a, SafePtr<U> b);
#endif

    bool operator!() const;
    operator T *() const;
    T *operator->() const;
    T& operator*() const;
};

template<class T>
inline SafePtr<T>::SafePtr(T *objptr)
{
    InitSafePtr((Class *)objptr);
}

template<class T>
inline SafePtr<T>::SafePtr(const SafePtr& obj)
{
    InitSafePtr(obj.ptr);
}

template<class T>
inline SafePtr<T>& SafePtr<T>::operator=(const SafePtr& obj)
{
    InitSafePtr(obj.ptr);
    return *this;
}

template<class T>
inline SafePtr<T>& SafePtr<T>::operator=(T *const obj)
{
    InitSafePtr(obj);
    return *this;
}

template<class T>
inline bool operator==(SafePtr<T> a, T *b)
{
    return a.ptr == b;
}

template<class T>
inline bool operator!=(SafePtr<T> a, T *b)
{
    return a.ptr != b;
}

template<class T>
inline bool operator==(T *a, SafePtr<T> b)
{
    return a == b.ptr;
}

template<class T>
inline bool operator!=(T *a, SafePtr<T> b)
{
    return a != b.ptr;
}

template<class T>
inline bool operator==(SafePtr<T> a, SafePtr<T> b)
{
    return a.ptr == b.ptr;
}

template<class T>
inline bool operator!=(SafePtr<T> a, SafePtr<T> b)
{
    return a.ptr != b.ptr;
}

template<class T>
inline bool SafePtr<T>::operator!() const
{
    return ptr == nullptr;
}

template<class T>
inline SafePtr<T>::operator T *() const
{
    return (T *)ptr;
}

template<class T>
inline T *SafePtr<T>::operator->() const
{
    return (T *)ptr;
}

template<class T>
inline T& SafePtr<T>::operator*() const
{
    return *(T *)ptr;
}
